"""WF lane: re-derive a pinned instruction_permutation's four hashes against
the CURRENT objects, after a source change that renumbered the symbol table.

claim.law.WF_a-pool-fix-renumbers-relocation-hashes-not-body-hashes.20260901
.v1 is the reason this exists.  Deleting or adding a compiler literal-pool
entry renumbers the object's symbol table and therefore the `info` word of
every relocation record, but changes NO function's compiled bytes.  So such a
change invalidates ONLY an instruction_permutation's
before_relocations_sha256 / after_relocations_sha256 and leaves every BODY
hash byte-identical.

That invariant is also the audit: this script prints the body hashes next to
the ones the rule carries, and they MUST come back unchanged.  If a body hash
moved, the source change altered codegen and you are not looking at a
renumbering -- do not paste the new relocation hashes in, re-derive the rule.

usage:
    python tools/gdl/composed_census/wf_rederive_pin.py <unit> <function>
    python tools/gdl/composed_census/wf_rederive_pin.py <unit> <function> --apply
    python tools/gdl/composed_census/wf_rederive_pin.py <unit> <function> --transient

--apply pastes the two derived relocation hashes back into
config/GUNE5D/webfrank.json with a surgical single-occurrence string swap (no
json.dump round-trip — AGENTS.md trap 6) and ONLY when every body hash is
unchanged; if a body hash moved it refuses with a non-zero exit so an
orchestrator (`probe --rederive-pin`) aborts instead of configuring a stale
rule. After --apply, run configure.py and rebuild the object.

--transient is --apply for a THROWAWAY A/B.  Run-34 criticism (GW): even with
--apply, ~2 of 15 probe cycles were pure pin plumbing, because an A/B on a
pinned TU is an edit, a re-derive, a measurement, a REVERT — and the revert
restores the SOURCE while leaving the re-derived hashes in webfrank.json, so
the pin has to be walked back by hand as well.  --transient banks the rule's
CURRENT hash slots to build/GUNE5D/gate/wfpin_<unit>.json before pasting, and
`probe --revert` restores them and drops the bank.  The first bank per
function WINS (like probe's session baseline): re-deriving three times during
one A/B still returns to the pre-probe pin, not to the second derivation.

Run after building ONLY the raw body object, e.g.
    ninja build/GUNE5D/src/game/game/.postprocess/body/combat.o
(the WEBFRANK stage will abort on the stale hash -- that is the guard working,
and the raw body is all this needs).
"""
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
import webfrank as wf  # noqa: E402


def apply_relocation_updates(config_text, pairs):
    """Surgically replace (old_hash -> new_hash) relocation-hash strings.

    AGENTS.md trap 6: webfrank.json is edited with surgical text inserts only,
    never a json.dump round-trip (which reformats every other lane's rules). A
    SHA-256 hex string is globally unique, so each old hash is replaced by an
    exact, single-occurrence string swap. Unchanged pairs are skipped; an old
    hash absent from the text (already updated) is a no-op; an old hash that
    appears more than once refuses rather than clobber. Returns
    (new_text, [changed hash pairs actually applied]).
    """
    applied = []
    for old, new in pairs:
        if old == new or not old:
            continue
        count = config_text.count(old)
        if count == 0:
            continue
        if count > 1:
            raise ValueError(
                f"relocation hash {old} appears {count} times in webfrank.json"
                " — refusing an ambiguous paste; hand-edit the window")
        config_text = config_text.replace(old, new)
        applied.append((old, new))
    return config_text, applied


def bank_path(unit, root=ROOT):
    """The transient pin bank for one TU, under build/ (AGENTS.md 17c).

    One file per UNIT, not per function: `probe --revert` restores the whole
    TU's source state, so every pin transiently re-derived during that A/B
    has to come back with it.
    """
    slug = re.sub(r"[^A-Za-z0-9_.-]+", "_", unit)
    return os.path.join(root, "build", "GUNE5D", "gate",
                        f"wfpin_{slug}.json")


def rule_hash_slots(rule):
    """The ordered hash strings a webfrank rule carries.

    Order is the schema's, not the file's, so two reads of the same rule
    always pair slot-for-slot. A rule whose SHAPE changed (a window added or
    removed) yields a different length, which is how restore detects that
    pairing them would be meaningless.
    """
    slots = [rule.get("before_sha256"), rule.get("after_sha256")]
    permutation = rule.get("instruction_permutation")
    windows = (permutation if isinstance(permutation, list)
               else [permutation] if permutation else [])
    for window in windows:
        if not isinstance(window, dict):
            continue
        for key in ("before_sha256", "after_sha256",
                    "before_relocations_sha256", "after_relocations_sha256"):
            slots.append(window.get(key))
    return slots


def _find_rule(config, unit, function):
    for rule in config.get("units", {}).get(unit, []):
        if rule.get("function") == function:
            return rule
    return None


def bank_transient(unit, function, config_path, path):
    """Record this pin's PRE-probe hash slots; first bank per function wins.

    Returns True when a bank was written (or already existed for this
    function), False when the rule could not be found.
    """
    with open(config_path, "r", encoding="utf-8") as handle:
        config = json.load(handle)
    rule = _find_rule(config, unit, function)
    if rule is None:
        return False
    bank = {"unit": unit, "pins": {}}
    if os.path.exists(path):
        try:
            with open(path, "r", encoding="utf-8") as handle:
                loaded = json.load(handle)
            if isinstance(loaded, dict) and isinstance(loaded.get("pins"),
                                                       dict):
                bank = loaded
        except (OSError, ValueError):
            pass
    # FIRST BANK WINS. Re-deriving three times during one A/B must still
    # return to the pre-probe pin, not to the second derivation — the same
    # rule probe's session baseline follows, for the same reason.
    if function not in bank["pins"]:
        bank["pins"][function] = rule_hash_slots(rule)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(bank, handle, indent=1, sort_keys=True)
    return True


def restore_transient(unit, config_path, path):
    """Put every transiently re-derived pin in this TU back, then drop the
    bank.

    Returns (restored_functions, notes). Restoring is the same surgical
    single-occurrence string swap --apply uses, run in the other direction:
    a SHA-256 hex string is globally unique, so pairing current->banked slot
    for slot touches only this rule's hashes and reformats nothing.
    """
    notes = []
    if not os.path.exists(path):
        return [], notes
    try:
        with open(path, "r", encoding="utf-8") as handle:
            bank = json.load(handle)
    except (OSError, ValueError):
        return [], ["transient pin bank is unreadable; left in place"]
    pins = bank.get("pins") if isinstance(bank, dict) else None
    if not isinstance(pins, dict):
        return [], ["transient pin bank has no pins; left in place"]
    with open(config_path, "r", encoding="utf-8") as handle:
        config = json.load(handle)
    text = open(config_path, "r", encoding="utf-8", newline="").read()
    restored = []
    for function, banked in sorted(pins.items()):
        rule = _find_rule(config, unit, function)
        if rule is None:
            notes.append(f"{function}: no rule in webfrank.json any more —"
                         " nothing restored for it")
            continue
        current = rule_hash_slots(rule)
        if len(current) != len(banked):
            notes.append(
                f"{function}: the rule's SHAPE changed since banking"
                f" ({len(banked)} hash slots then, {len(current)} now) —"
                " refusing to pair them; restore this pin by hand")
            continue
        try:
            text, applied = apply_relocation_updates(
                text, list(zip(current, banked)))
        except ValueError as error:
            notes.append(f"{function}: {error}")
            continue
        if applied:
            restored.append(function)
    if restored:
        with open(config_path, "w", encoding="utf-8", newline="") as handle:
            handle.write(text)
    # The bank is CONSUMED by a revert whether or not a swap was needed: it
    # describes one A/B, and carrying it into the next one would restore a
    # pin state that no longer has anything to do with the tree.
    if not notes:
        os.remove(path)
    else:
        notes.append("bank NOT dropped — resolve the note(s) above, then"
                     f" delete {path}")
    return restored, notes


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    transient = "--transient" in sys.argv
    apply = "--apply" in sys.argv or transient
    if len(args) != 2:
        raise SystemExit(__doc__)
    unit, function = args[0], args[1]
    parts = unit.split("/")

    body = os.path.join(ROOT, "build", "GUNE5D", "src", *parts[:-1],
                        ".postprocess", "body", parts[-1] + ".o")
    target_path = os.path.join(ROOT, "build", "GUNE5D", "obj",
                               *parts) + ".o"
    config_path = os.path.join(ROOT, "config", "GUNE5D", "webfrank.json")

    with open(config_path, "r", encoding="utf-8") as handle:
        config = json.load(handle)
    rules = config.get("units", {}).get(unit, [])
    rule = next((r for r in rules if r.get("function") == function), None)
    if rule is None:
        raise SystemExit(f"no webfrank rule for {unit}::{function}")
    if "instruction_permutation" not in rule:
        raise SystemExit(
            f"{function} has no instruction_permutation; a pool renumbering "
            f"cannot invalidate a body-hash-only rule")

    with open(body, "rb") as handle:
        data = bytearray(handle.read())
    sections = wf._sections(data)
    symbol = wf._find_symbol(data, sections, function)
    text = sections[symbol.section_index]
    start = text.offset + symbol.value

    with open(target_path, "rb") as handle:
        target_data = handle.read()
    target_sections = wf._sections(target_data)
    target_symbol = wf._find_symbol(target_data, target_sections, function)
    target_text = target_sections[target_symbol.section_index]
    target_start = target_text.offset + target_symbol.value
    target_body = target_data[
        target_start:target_start + target_symbol.size]

    ok = True

    def compare(label, derived, banked):
        nonlocal ok
        same = derived == banked
        ok = ok and same
        print(f"  {label:28} {derived}  "
              f"{'(unchanged)' if same else 'CHANGED  rule ' + banked}")

    print(f"{unit}::{function}")
    print("BODY HASHES -- these MUST all read (unchanged):")
    compare("function before_sha256",
            wf._sha256(data[start:start + symbol.size]),
            rule["before_sha256"])
    compare("function after_sha256", wf._sha256(target_body),
            rule["after_sha256"])

    relocation_sections = [
        section for section in sections
        if section.section_type == wf.SHT_RELA
        and section.info == symbol.section_index
    ]
    if len(relocation_sections) != 1:
        raise SystemExit("expected exactly one relocation section for .text")
    relocation_section = relocation_sections[0]
    entry_size = relocation_section.entry_size or 12
    records = [
        struct.unpack_from(">IIi", data, offset)
        for offset in range(
            relocation_section.offset,
            relocation_section.offset + relocation_section.size,
            entry_size)
    ]
    # Function-relative {offset: (reloc_type, symbol_name)} — the same
    # source apply_patch builds its window_symbols from. Required since
    # the run-28 name-bound hash migration.
    text_relocs = wf._function_text_relocations(
        data, sections, symbol.section_index,
        symbol.value, symbol.value + symbol.size)

    windows, ranges = wf.permutation_windows(
        rule["instruction_permutation"], symbol.size)
    updates = []
    pairs = []
    for window, (relative_start, relative_end) in zip(windows, ranges):
        region = bytes(data[start + relative_start:start + relative_end])
        print(f"\nwindow +0x{relative_start:x}..+0x{relative_end:x} "
              f"order={window['order']}")
        compare("region before_sha256", wf._sha256(region),
                window["before_sha256"])
        order = [wf._parse_int(index) for index in window["order"]]
        permuted = b"".join(region[j * 4:j * 4 + 4] for j in order)
        compare("region after_sha256", wf._sha256(permuted),
                window["after_sha256"])

        region_start = symbol.value + relative_start
        region_end = symbol.value + relative_end
        region_records = [
            (offset - region_start, info, addend)
            for offset, info, addend in records
            if region_start <= offset < region_end
        ]
        destination = {source: index for index, source in enumerate(order)}
        moved = sorted(
            ((destination[offset // 4] * 4 + offset % 4, info, addend)
             for offset, info, addend in region_records),
            key=lambda record: record[0])

        print("  RELOCATION HASHES -- these are the ones to paste back:")
        # Name-bound hashing (run-28 migration): the symbols dicts map
        # window-relative reloc offset -> symbol NAME, so the hash never
        # sees an r_info index.
        window_syms = {
            off - relative_start: name
            for off, (_rt, name) in text_relocs.items()
            if relative_start <= off < relative_end
        }
        moved_syms = {
            destination[off // 4] * 4 + off % 4: window_syms[off]
            for off, _info, _addend in region_records
        }
        before = wf._relocation_sha256(region_records, window_syms)
        after = wf._relocation_sha256(moved, moved_syms)
        print(f'    "before_relocations_sha256": "{before}"'
              f'{"" if before == window["before_relocations_sha256"] else "   <-- CHANGED"}')
        print(f'    "after_relocations_sha256":  "{after}"'
              f'{"" if after == window["after_relocations_sha256"] else "   <-- CHANGED"}')
        # Name-bound: report the SYMBOL NAME the hash actually binds, not the
        # r_info index the run-28 migration stopped hashing (the old symidx
        # lookup referenced an undefined `names` and crashed here, killing the
        # verdict line below before it could print).
        for offset, info, addend in region_records:
            print(f"      +0x{offset:02x} type={info & 0xFF} "
                  f"addend={addend} {window_syms.get(offset, '?')}")
        updates.append((before, after))
        pairs.append((window.get("before_relocations_sha256"), before))
        pairs.append((window.get("after_relocations_sha256"), after))

    print()
    if not ok:
        print("A BODY HASH MOVED: the source change altered codegen, not just "
              "the symbol table.  Do NOT paste the relocation hashes in -- "
              "re-derive the rule from scratch.")
        # A refused paste must fail LOUDLY so `probe --rederive-pin` aborts
        # instead of running configure over an unpasted, still-stale rule.
        # exit 1 preserves the historical body-hash-moved code for the plain
        # diagnostic run; exit 2 distinguishes an --apply refusal.
        raise SystemExit(2 if apply else 1)

    print("BODY HASHES ALL UNCHANGED: this is a pure symbol-table "
          "renumbering, and pasting the two relocation hashes above into "
          "config/GUNE5D/webfrank.json is sound.")
    if not apply:
        print("Then run configure.py and confirm the WEBFRANK line for this "
              "unit reapplies.  (Re-run with --apply to paste + write here.)")
        return
    if transient:
        # Bank the PRE-paste state before touching anything, so a revert has
        # somewhere to go back to even if the paste below fails part-way.
        bank = bank_path(unit)
        bank_transient(unit, function, config_path, bank)
        print(f"--transient: pre-probe pin hashes banked to {bank} —"
              " `probe.py <unit> <fn> --revert` restores them and drops the"
              " bank.")
    # newline="" on BOTH sides: no \r\n<->\n translation, so the only bytes
    # that change are the hash strings themselves (AGENTS.md trap 6).
    config_text = open(config_path, "r", encoding="utf-8", newline="").read()
    new_text, applied = apply_relocation_updates(config_text, pairs)
    if not applied:
        print("--apply: every relocation hash already matches — nothing to "
              "paste (the rule was already current).")
        return
    with open(config_path, "w", encoding="utf-8", newline="") as handle:
        handle.write(new_text)
    print(f"--apply: pasted {len(applied)} relocation hash(es) into "
          "config/GUNE5D/webfrank.json (surgical string swap, no reformat). "
          "Run configure.py, then rebuild the object to confirm the WEBFRANK "
          "stage reapplies.")


if __name__ == "__main__":
    raise SystemExit(main())
