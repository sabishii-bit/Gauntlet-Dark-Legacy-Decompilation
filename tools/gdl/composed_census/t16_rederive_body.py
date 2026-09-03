"""T16 lane (run 46): re-derive a webfrank pin whose BODY hash moved.

    python tools/gdl/composed_census/t16_rederive_body.py <unit> <function>
    python tools/gdl/composed_census/t16_rederive_body.py <unit> <function> --apply
    python tools/gdl/composed_census/t16_rederive_body.py <unit> <function> \
        --config <scratch-copy-of-webfrank.json>

THE GAP THIS CLOSES.  `wf_rederive_pin.py` re-derives the two RELOCATION
hashes of an instruction_permutation, and it refuses -- correctly -- the
moment a BODY hash moves, with "re-derive the rule from scratch".  There was
no tool for that, so the recovery path was reading the new hash out of
webfrank's abort message

    <fn>: input hash <new> != expected <old>

and pasting it into config/GUNE5D/webfrank.json by hand (twice, run 45).  That
paste is not a re-derivation: `before_sha256` is a hash of OUR object's body,
so replacing it with whatever the compiler now emits re-blesses the new
codegen unconditionally.  The rule's guards never re-run, and a rule that no
longer closes the function is indistinguishable from one that does.

WHAT MAKES A PASTE SOUND.  Only one thing: after the paste, the rule must
still take our bytes to the TARGET's bytes.  So this tool derives every hash
slot fresh, builds the candidate rule, runs it through the real
`webfrank.apply_patch` with its guards and the retail image (the same
full-strength datum screen `wr_try_rule.py` established), and pastes ONLY on
BYTE-EQUAL.  APPLIED-NOT-EQUAL or any guard refusal means the source change
outran the rule -- typically a permutation `order` that no longer describes
this codegen -- and the answer is `rule_derive.py`, not a hash.

REFUSALS, each for its own reason:
  * `after_sha256` moved.  That hash is the TARGET's body, which does not
    change when our source does.  A move means the rule is now pointing at a
    different symbol (a rename, a size change, a reordered object) and no
    hash paste can fix that.
  * the retail image is missing.  L3 of the datum screen reads it; without it
    every word it would decide falls through to the weaker pool
    correspondence (claim.law.CQ_copy-register-fields-can-rotate-constant-
    load-homes-without-their-relocations.20260903.v1).
  * the raw body object is older than its source, or could not be built.

--config points the read AND the paste at another file, so an end-to-end
positive can be measured against a scratch copy without touching the shipped
config.  Pastes are the surgical key-anchored swap wf_rederive_pin already
ships (AGENTS.md trap 6: never a json.dump round-trip).

WG owns webfrank.py/webfrank.json; this tool edits neither's CODE and writes
the config only under --apply, exactly as wf_rederive_pin does.

IMPORTABLE CORE: derive_slots, classify_move, candidate_rule -- pure over
parsed objects and a rule dict; they never build and never write.
"""
import argparse
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                              # noqa: E402
import wf_rederive_pin as wrp                      # noqa: E402


def unit_paths(unit):
    parts = unit.split("/")
    body = os.path.join(ROOT, "build", "GUNE5D", "src", *parts[:-1],
                        ".postprocess", "body", parts[-1] + ".o")
    target = os.path.join(ROOT, "build", "GUNE5D", "obj", *parts) + ".o"
    return body, target


def _symbol_body(data, function):
    sections = wf._sections(data)
    symbol = wf._find_symbol(data, sections, function)
    start = sections[symbol.section_index].offset + symbol.value
    return sections, symbol, start, bytes(data[start:start + symbol.size])


def derive_slots(rule, our_data, target_data, function):
    """Every hash the rule carries, derived from the CURRENT objects.

    Returns {slot_path: derived_hash} where slot_path is
    "before_sha256" / "after_sha256" for the rule, and
    ("window", i, key) for each permutation window. Pure: it reads the two
    object images it is handed and nothing else.
    """
    sections, symbol, start, body = _symbol_body(our_data, function)
    _tsec, _tsym, _tstart, target_body = _symbol_body(target_data, function)
    out = {"before_sha256": wf._sha256(body),
           "after_sha256": wf._sha256(target_body)}

    permutation = rule.get("instruction_permutation")
    if permutation is None:
        return out
    windows, ranges = wf.permutation_windows(permutation, symbol.size)

    relocation_sections = [
        section for section in sections
        if section.section_type == wf.SHT_RELA
        and section.info == symbol.section_index
    ]
    records = []
    if len(relocation_sections) == 1:
        section = relocation_sections[0]
        size = section.entry_size or 12
        records = [struct.unpack_from(">IIi", our_data, offset)
                   for offset in range(section.offset,
                                       section.offset + section.size, size)]
    text_relocs = wf._function_text_relocations(
        our_data, sections, symbol.section_index,
        symbol.value, symbol.value + symbol.size)

    for index, (window, (lo, hi)) in enumerate(zip(windows, ranges)):
        region = bytes(our_data[start + lo:start + hi])
        order = [wf._parse_int(i) for i in window["order"]]
        permuted = b"".join(region[j * 4:j * 4 + 4] for j in order)
        out[("window", index, "before_sha256")] = wf._sha256(region)
        out[("window", index, "after_sha256")] = wf._sha256(permuted)

        region_start, region_end = symbol.value + lo, symbol.value + hi
        region_records = [(offset - region_start, info, addend)
                          for offset, info, addend in records
                          if region_start <= offset < region_end]
        destination = {source: slot for slot, source in enumerate(order)}
        window_syms = {off - lo: name
                       for off, (_t, name) in text_relocs.items()
                       if lo <= off < hi}
        moved = sorted(((destination[off // 4] * 4 + off % 4, info, addend)
                        for off, info, addend in region_records),
                       key=lambda record: record[0])
        moved_syms = {destination[off // 4] * 4 + off % 4: window_syms[off]
                      for off, _i, _a in region_records}
        out[("window", index, "before_relocations_sha256")] = \
            wf._relocation_sha256(region_records, window_syms)
        out[("window", index, "after_relocations_sha256")] = \
            wf._relocation_sha256(moved, moved_syms)
    return out


def rule_slot(rule, path):
    if isinstance(path, str):
        return rule.get(path)
    _tag, index, key = path
    permutation = rule.get("instruction_permutation")
    windows = (permutation if isinstance(permutation, list)
               else [permutation] if permutation else [])
    if index >= len(windows) or not isinstance(windows[index], dict):
        return None
    return windows[index].get(key)


def classify_move(rule, derived):
    """(verdict, moved_slots). Pure over two dicts, no I/O.

    UNCHANGED   nothing to do here (relocation-only drift is
                wf_rederive_pin's job and is reported separately)
    TARGET-MOVED  after_sha256 differs: the rule points at different target
                bytes and no paste is legitimate
    BODY-MOVED  our body changed; the paste is CONDITIONAL on the rule still
                landing byte-equal
    """
    moved = [path for path, value in derived.items()
             if rule_slot(rule, path) != value]
    if not moved:
        return "UNCHANGED", []
    if "after_sha256" in moved:
        return "TARGET-MOVED", moved
    if "before_sha256" in moved:
        return "BODY-MOVED", moved
    return "WINDOW-MOVED", moved


def candidate_rule(rule, derived):
    """A deep copy of `rule` with every derived hash written in. Pure."""
    new = json.loads(json.dumps(rule))
    for path, value in derived.items():
        if isinstance(path, str):
            new[path] = value
            continue
        _tag, index, key = path
        permutation = new.get("instruction_permutation")
        if isinstance(permutation, list):
            permutation[index][key] = value
        elif isinstance(permutation, dict) and index == 0:
            permutation[key] = value
    return new


def paste_pairs(rule, derived, moved):
    """(anchor, key, old, new) swaps for wf_rederive_pin.apply_relocation_updates."""
    pairs = []
    for path in moved:
        old, new = rule_slot(rule, path), derived[path]
        if isinstance(path, str):
            pairs.append((None, path, old, new))
            continue
        _tag, index, key = path
        anchor = rule_slot(rule, ("window", index, "before_sha256"))
        pairs.append((None if key == "before_sha256" else anchor,
                      key, old, new))
    # A window's own before_sha256 anchors its siblings, so swap it last.
    pairs.sort(key=lambda pair: pair[1] == "before_sha256")
    return pairs


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("unit")
    parser.add_argument("function")
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--no-build", action="store_true")
    parser.add_argument("--config", default=None,
                        help="webfrank.json to read AND paste into"
                             " (default: config/GUNE5D/webfrank.json)")
    parser.add_argument("--image", default=None)
    arguments = parser.parse_args()
    unit, function = arguments.unit.replace("\\", "/"), arguments.function

    config_path = arguments.config or os.path.join(
        ROOT, "config", "GUNE5D", "webfrank.json")
    with open(config_path, "r", encoding="utf-8") as handle:
        config = json.load(handle)
    rule = next((r for r in config.get("units", {}).get(unit, [])
                 if r.get("function") == function), None)
    if rule is None:
        print(f"no webfrank rule for {unit}::{function}")
        return 1

    image_path = arguments.image or os.path.join(
        ROOT, "orig", "GUNE5D", "sys", "main.dol")
    if not os.path.exists(image_path):
        print(f"REFUSED: retail image {image_path} is missing, so the datum"
              " screen would silently drop from L3 to the weaker pool"
              " correspondence. Run provision_worktree.py, or pass --image.")
        return 1

    body_path, target_path = unit_paths(unit)
    if arguments.no_build:
        print("[--no-build: deriving from the object already on disk]")
    else:
        print(f"[building {wrp.build_body_object(body_path, unit)}]")
    source = wrp.unit_source(unit)
    refusal = wrp.stale_body_refusal(
        os.path.getmtime(source) if source else None,
        os.path.getmtime(body_path) if os.path.exists(body_path) else None)
    if refusal:
        print(refusal)
        return 1

    with open(body_path, "rb") as handle:
        our_data = bytearray(handle.read())
    with open(target_path, "rb") as handle:
        target_data = bytearray(handle.read())

    derived = derive_slots(rule, our_data, target_data, function)
    verdict, moved = classify_move(rule, derived)
    print(f"{unit}::{function}: {verdict}")
    for path in sorted(moved, key=str):
        label = path if isinstance(path, str) else \
            f"window[{path[1]}].{path[2]}"
        print(f"  {label:34} rule {rule_slot(rule, path)}")
        print(f"  {'':34} now  {derived[path]}")

    if verdict == "UNCHANGED":
        print("Every hash this rule carries already describes the current"
              " objects — nothing to paste. (Relocation-hash drift from a"
              " pool renumbering is wf_rederive_pin.py's job.)")
        return 0
    if verdict == "TARGET-MOVED":
        print("REFUSED: after_sha256 is a hash of the TARGET's body, which a"
              " source change cannot move. The rule is bound to a different"
              " symbol now (rename, size change, reordered object) — repair"
              " the rule's identity; no hash paste is legitimate here.")
        return 2

    # THE ONLY THING THAT MAKES A PASTE SOUND: the re-hashed rule must still
    # take our bytes to the target's bytes, under the real guards.
    new_rule = candidate_rule(rule, derived)
    _sec, symbol, start, _body = _symbol_body(our_data, function)
    _tsec, _tsym, _tstart, target_body = _symbol_body(target_data, function)
    probe = bytearray(our_data)
    try:
        _before, _after, changed = wf.apply_patch(
            probe, json.loads(json.dumps(new_rule)), bytes(target_data),
            wf.load_symbol_addresses(
                os.path.join(ROOT, "config", "GUNE5D", "symbols.txt")),
            wf.RetailImage(image_path))
    except ValueError as failure:
        print(f"REFUSED by the rule's own guards: {failure}")
        print("The source change outran the rule. Re-derive it"
              " (tools/gdl/rule_derive.py) or withdraw it; do NOT paste a"
              " hash to make the abort go away.")
        return 2
    final = bytes(probe[start:start + symbol.size])
    if final != target_body:
        print(f"APPLIED-NOT-EQUAL ({changed} word(s) changed): the re-hashed"
              " rule no longer closes this function — a permutation `order`"
              " that described the old codegen usually does not describe the"
              " new one. Re-derive with tools/gdl/rule_derive.py.")
        return 2
    print(f"BYTE-EQUAL after re-hashing ({changed} word(s) changed): the rule"
          " still takes our bytes to the target's, so pasting the derived"
          " hashes is sound.")
    if not arguments.apply:
        print("(re-run with --apply to paste; then `python configure.py` and"
              " rebuild, and confirm the WEBFRANK line for this unit"
              " reapplies)")
        return 0

    with open(config_path, "r", encoding="utf-8", newline="") as handle:
        text = handle.read()
    try:
        new_text, applied = wrp.apply_relocation_updates(
            text, paste_pairs(rule, derived, moved))
    except ValueError as failure:
        print(f"REFUSED: {failure}")
        return 2
    if not applied:
        print("--apply: nothing to paste (the file already matches).")
        return 0
    with open(config_path, "w", encoding="utf-8", newline="") as handle:
        handle.write(new_text)
    print(f"--apply: pasted {len(applied)} hash(es) into {config_path}"
          " (surgical key-anchored swap, no reformat). Run configure.py, then"
          " rebuild and confirm the WEBFRANK line reapplies.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
