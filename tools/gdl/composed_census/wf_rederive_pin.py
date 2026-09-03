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
config/GUNE5D/webfrank.json with a surgical KEY-anchored swap (no json.dump
round-trip — AGENTS.md trap 6) and ONLY when every body hash is unchanged; if
a body hash moved it refuses with a non-zero exit so an orchestrator
(`probe --rederive-pin`) aborts instead of configuring a stale rule. After
--apply, run configure.py and rebuild the object.

The swap is anchored on the KEY, not on the raw hash string (run-35 item 5).
A window whose permutation moves no RELOCATED instruction hashes its
relocation set to the same value before and after, so the rule legitimately
carries one hash in two slots — and the old single-occurrence string swap
counted two and refused an ordinary re-derivation outright
(claim.law.PC_wf-rederive-pin-apply-cannot-paste-a-twinned-relocation-hash).
The two slots differ by key. When a hash ALSO repeats under the same key
across windows — an empty relocation set hashes to a constant every such
window shares — the window's own region before_sha256 anchors the enclosing
JSON object and confines the swap to it. Genuinely unnarrowable ambiguity
still refuses; --transient's restore is anchored the same way.

--transient is --apply for a THROWAWAY A/B.  Run-34 criticism (GW): even with
--apply, ~2 of 15 probe cycles were pure pin plumbing, because an A/B on a
pinned TU is an edit, a re-derive, a measurement, a REVERT — and the revert
restores the SOURCE while leaving the re-derived hashes in webfrank.json, so
the pin has to be walked back by hand as well.  --transient banks the rule's
CURRENT hash slots to build/GUNE5D/gate/wfpin_<unit>.json before pasting, and
`probe --revert` restores them and drops the bank.  The first bank per
function WINS (like probe's session baseline): re-deriving three times during
one A/B still returns to the pre-probe pin, not to the second derivation.

This tool BUILDS the raw body object it derives from (the WEBFRANK stage
will abort on the stale hash -- that is the guard working, and the raw body
is all this needs).  It used to read whatever object sat on disk and print
the docstring instruction "run after building the body object" instead; an
instruction is not a guard.  Measured 2026-09-02 on
game/ui/screensaver::end_inventory_panel: with the source at HEAD but the
object left from a since-reverted probe edit, it derived
before_sha256=1d409357... , reported the pin CHANGED, and --apply would have
pasted that reverted probe's hash into webfrank.json
(claim.law.PC_wf-rederive-pin-derives-from-whatever-object-is-on-disk).
--no-build opts out and says so; a body object older than its source is
refused outright.
"""
import json
import os
import re
import struct
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
import webfrank as wf  # noqa: E402


def unit_source(unit):
    """Absolute path to `unit`'s source file, or None."""
    for suffix in (".c", ".cpp"):
        path = os.path.join(ROOT, "src", *unit.split("/")) + suffix
        if os.path.exists(path):
            return path
    return None


def stale_body_refusal(source_mtime, body_mtime):
    """Refusal when the raw body object predates its source, else None.

    Pure over two mtimes so the decision is tested without a build tree.
    `None` for either side means the path is missing and the caller has
    already reported that.
    """
    if source_mtime is None or body_mtime is None:
        return None
    if body_mtime >= source_mtime:
        return None
    return ("REFUSED: the raw body object is OLDER than its source, so every"
            " hash below would describe bytes the current source does not"
            " produce. Build the body object first"
            " (this tool does it for you unless --no-build is passed).")


def build_body_object(body, unit):
    """Build the raw body object this derivation reads.

    The whole point of the tool is to paste hashes into webfrank.json, and
    it used to derive them from WHATEVER object happened to sit on disk.
    Measured 2026-09-02 on game/ui/screensaver::end_inventory_panel: with
    the source at HEAD but the object left over from a since-reverted
    probe edit, the tool reported before_sha256 as CHANGED and would have
    pasted the reverted probe's hash into the config (PC's run-36 law).
    The docstring said "run after building the body object" — an
    instruction is not a guard.
    """
    relative = os.path.relpath(body, ROOT).replace(os.sep, "/")
    result = subprocess.run(["ninja", relative], cwd=ROOT,
                            capture_output=True, text=True)
    if result.returncode != 0:
        raise SystemExit(
            f"cannot build the raw body object for {unit}:\n"
            + (result.stdout + result.stderr).strip()[-1500:])
    return relative


def _string_aware_mask(text):
    """[bool] — True where the character is JSON STRUCTURE, not string body.

    Brace matching over raw text is wrong here: webfrank rules carry
    `mechanism` prose, and a `{` in a note would derail the scan. One pass,
    escape-aware.
    """
    mask = bytearray(len(text))
    in_string = False
    escaped = False
    for index, char in enumerate(text):
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
        elif char == '"':
            in_string = True
        else:
            mask[index] = 1
    return mask


def enclosing_object_span(text, index, mask=None):
    """(start, end) of the JSON object literal containing `index`, or None."""
    if index < 0 or index >= len(text):
        return None
    mask = mask if mask is not None else _string_aware_mask(text)
    depth = 0
    start = None
    for position in range(index, -1, -1):
        if not mask[position]:
            continue
        char = text[position]
        if char == "}":
            depth += 1
        elif char == "{":
            if depth == 0:
                start = position
                break
            depth -= 1
    if start is None:
        return None
    depth = 0
    for position in range(start, len(text)):
        if not mask[position]:
            continue
        char = text[position]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return start, position + 1
    return None


def _key_value_matches(text, key, old, span=None):
    """Every `"key": "old"` occurrence, optionally restricted to a span.

    Group 1 is the key and separator VERBATIM and group 2 the closing quote,
    so a rewrite preserves the file's own spacing — this file is edited by
    every lane and a normalized `": "` would show up as a diff hunk nobody
    made.
    """
    pattern = re.compile(
        r'("' + re.escape(key) + r'"\s*:\s*")' + re.escape(old) + r'(")')
    lo, hi = span if span else (0, len(text))
    return [match for match in pattern.finditer(text)
            if lo <= match.start() < hi]


def apply_relocation_updates(config_text, pairs):
    """Surgically replace hash strings in webfrank.json.

    AGENTS.md trap 6: webfrank.json is edited with surgical text inserts only,
    never a json.dump round-trip (which reformats every other lane's rules).

    Accepts two pair shapes:
      (old, new)                 — legacy: swap a globally unique hash string
      (anchor, key, old, new)    — keyed: swap the value of `key`, using
                                   `anchor` (the window's region before_sha256)
                                   to disambiguate when the key/value pair
                                   repeats

    WHY THE KEYED FORM EXISTS (run-35 item 5,
    claim.law.PC_wf-rederive-pin-apply-cannot-paste-a-twinned-relocation-hash).
    The legacy form replaces a raw hash STRING and refuses when it occurs more
    than once. But a window whose permutation moves no relocated instruction
    hashes its relocation set to the SAME value before and after, so the rule
    legitimately carries that hash TWICE — once under
    before_relocations_sha256 and once under after_relocations_sha256 — and a
    perfectly ordinary re-derivation hit the ambiguity guard and stopped.
    The two slots are distinguished by their KEY, which is exactly the
    information the raw-string swap threw away. PC prototyped this for one
    window by hand; this is the general form.

    Second tier: a hash can also repeat under the SAME key across windows —
    an empty relocation set hashes to a constant, so every relocation-free
    window shares it. Then the window's own region before_sha256 (a hash of
    distinct instruction bytes) anchors the enclosing JSON object and the
    swap is confined to it.

    Returns (new_text, [(old, new) actually applied]).
    """
    applied = []
    mask = None
    for pair in pairs:
        if len(pair) == 4:
            anchor, key, old, new = pair
        else:
            anchor, key, (old, new) = None, None, pair
        if old == new or not old:
            continue
        if key is None:
            count = config_text.count(old)
            if count == 0:
                continue
            if count > 1:
                raise ValueError(
                    f"hash {old} appears {count} times in webfrank.json"
                    " — refusing an ambiguous paste; hand-edit the window")
            config_text = config_text.replace(old, new)
            applied.append((old, new))
            mask = None  # the text moved; any cached mask is stale
            continue
        matches = _key_value_matches(config_text, key, old)
        if not matches:
            continue
        if len(matches) > 1 and anchor:
            # Tier two: confine the swap to the window the anchor names.
            mask = mask if mask is not None else _string_aware_mask(
                config_text)
            anchor_at = config_text.find(anchor)
            if anchor_at >= 0 and config_text.find(anchor, anchor_at + 1) < 0:
                span = enclosing_object_span(config_text, anchor_at, mask)
                if span:
                    matches = _key_value_matches(config_text, key, old, span)
        if len(matches) > 1:
            raise ValueError(
                f'"{key}": "{old}" appears {len(matches)} times in'
                " webfrank.json and the window anchor could not narrow it"
                " — refusing an ambiguous paste; hand-edit the window")
        match = matches[0]
        config_text = (config_text[:match.start()]
                       + match.group(1) + new + match.group(2)
                       + config_text[match.end():])
        applied.append((old, new))
        mask = None  # the text moved; any cached mask is stale
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


def rule_hash_descriptors(rule):
    """(anchor, key) for each slot rule_hash_slots() returns, in that order.

    The bank file stores VALUES positionally and predates this, so the two
    must stay index-parallel; keeping them as separate functions is what
    lets restore anchor its swaps without changing the bank format.
    """
    descriptors = [(None, "before_sha256"), (None, "after_sha256")]
    permutation = rule.get("instruction_permutation")
    windows = (permutation if isinstance(permutation, list)
               else [permutation] if permutation else [])
    for window in windows:
        if not isinstance(window, dict):
            continue
        anchor = window.get("before_sha256")
        for key in ("before_sha256", "after_sha256",
                    "before_relocations_sha256", "after_relocations_sha256"):
            # The window's OWN before_sha256 is the anchor, so it cannot
            # also anchor itself: swapping it first would leave later pairs
            # in this window hunting for a string no longer in the file.
            descriptors.append((None if key == "before_sha256" else anchor,
                                key))
    return descriptors


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
    with open(config_path, "r", encoding="utf-8", newline="") as handle:
        text = handle.read()
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
        # KEYED pairs, for the same reason --apply uses them: twinned
        # before/after relocation hashes are one string in two slots, and
        # the raw-string swap refused the whole restore.
        descriptors = rule_hash_descriptors(rule)
        keyed = [(anchor, key, old, new)
                 for (anchor, key), old, new
                 in zip(descriptors, current, banked)]
        # Order any window's OWN before_sha256 swap last, so it is still in
        # the file while the other slots of that window use it as anchor.
        keyed.sort(key=lambda pair: pair[1] == "before_sha256")
        try:
            text, applied = apply_relocation_updates(text, keyed)
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
    # A body-hash-only rule cannot be invalidated by a pool renumbering, so
    # there are no relocation hashes to re-derive for it -- but it still has
    # DATUM BINDINGS, and those a pool renumbering can absolutely change.
    # The governance case CR filed is camera_mode_target, a
    # copy_register_fields + post_recolor_permutation rule that sat pinned at
    # real 0 with two wrong constants: refusing it at the door here is what
    # put the screen out of reach of the one rule shape that needed it.  Such
    # a rule now runs the screen and stops before the paste.
    has_permutation = "instruction_permutation" in rule

    # Derive from an object this tool BUILT, never from whatever is on disk.
    if "--no-build" in sys.argv:
        print("[--no-build: deriving from the object already on disk — its"
              " hashes describe THAT object, not necessarily your source]")
    else:
        print(f"[building {build_body_object(body, unit)}]")

    source = unit_source(unit)
    refusal = stale_body_refusal(
        os.path.getmtime(source) if source else None,
        os.path.getmtime(body) if os.path.exists(body) else None)
    if refusal:
        raise SystemExit(refusal)

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

    # Full relocations (with addends) at their RAW positions; the window loop
    # below moves the ones a permutation carries, and the datum screen after
    # it compares the result against the target's.
    text_relocs_full = wf._function_text_relocations_full(
        data, sections, symbol.section_index,
        symbol.value, symbol.value + symbol.size)
    final_relocs = dict(text_relocs_full)

    windows, ranges = (
        wf.permutation_windows(rule["instruction_permutation"], symbol.size)
        if has_permutation else ([], [])
    )
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
        # Mirror the move onto the full (addend-carrying) map the datum
        # screen below reads, so it sees each relocation where the rule will
        # actually leave it.
        for offset, entry in list(text_relocs_full.items()):
            if not relative_start <= offset < relative_end:
                continue
            inside = offset - relative_start
            final_relocs.pop(offset, None)
            final_relocs[relative_start
                         + destination[inside // 4] * 4 + inside % 4] = entry
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
        # KEYED pairs (run-35 item 5). A window whose permutation moves no
        # relocated instruction has before == after here, so the rule carries
        # one hash string in two slots and the raw-string swap refused the
        # whole paste. The key tells the two apart; the region before_sha256
        # anchors the window when a hash repeats across windows (an empty
        # relocation set hashes to a constant every such window shares).
        anchor = window.get("before_sha256")
        pairs.append((anchor, "before_relocations_sha256",
                      window.get("before_relocations_sha256"), before))
        pairs.append((anchor, "after_relocations_sha256",
                      window.get("after_relocations_sha256"), after))

    # THE DATUM SCREEN.  A pool RENUMBER is exactly the event this tool
    # exists for, and it is also exactly the event that can change WHICH
    # datum a word binds: our anonymous pool labels are renamed wholesale, so
    # a rule whose hashes are re-derived and pasted back can be blessed while
    # binding the wrong constant.  Nothing else in the re-derivation path
    # looks at what a relocation POINTS AT -- the hashes bind names and
    # positions only.  The governance case is camera_mode_target, which sat
    # pinned at real 0 with two wrong constants.
    print("DATUM BINDINGS -- what each relocated word will bind after the "
          "link:")
    binding_ok = True
    image_path = os.path.join(ROOT, "orig", "GUNE5D", "sys", "main.dol")
    symbols_path = os.path.join(ROOT, "config", "GUNE5D", "symbols.txt")
    if not os.path.exists(image_path):
        binding_ok = False
        print(f"  REFUSED: the retail image {image_path} is missing, so the "
              f"bindings cannot be proved; run provision_worktree.py")
    else:
        target_relocs_full = wf._function_text_relocations_full(
            target_data, target_sections, target_symbol.section_index,
            target_symbol.value, target_symbol.value + target_symbol.size)
        try:
            levels = wf.verify_datum_binding(
                final_relocs, target_relocs_full,
                [wf._u32(target_body, offset)
                 for offset in range(0, len(target_body), 4)],
                our_data=data, our_sections=sections,
                our_symbols=wf._symbol_index(data, sections),
                symbol_addresses=(
                    wf.load_symbol_addresses(symbols_path)
                    if os.path.exists(symbols_path) else None),
                image=wf.RetailImage(image_path), function=function)
        except ValueError as failure:
            binding_ok = False
            print(f"  {failure}")
        else:
            print(f"  proved: {levels['L1']} by name, {levels['L2']} by "
                  f"address, {levels['L3']} by datum, {levels['L4']} on the "
                  f"pool correspondence alone")

    print()
    if ok and not binding_ok:
        # Reported after the body-hash verdict, because a moved body hash
        # explains a binding failure and is the more fundamental refusal.
        print("A DATUM BINDING IS WRONG: the pin's text may be byte-correct "
              "and still load the wrong constant, which no score in this "
              "project can see.  Do NOT paste anything -- withdraw or repair "
              "the rule (claim.law.CQ_copy-register-fields-can-rotate-"
              "constant-load-homes-without-their-relocations.20260903.v1).")
        raise SystemExit(2 if apply else 1)
    if not has_permutation:
        if not binding_ok:
            raise SystemExit(
                f"{function}: a datum binding is WRONG (above).  There are no "
                f"relocation hashes to re-derive for a body-hash-only rule, "
                f"but the rule must be withdrawn or repaired.")
        print(f"{function} has no instruction_permutation, so a pool "
              f"renumbering cannot invalidate its hashes and there is "
              f"nothing to paste.  Its datum bindings are proved above.")
        return
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
    with open(config_path, "r", encoding="utf-8", newline="") as handle:
        config_text = handle.read()
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
