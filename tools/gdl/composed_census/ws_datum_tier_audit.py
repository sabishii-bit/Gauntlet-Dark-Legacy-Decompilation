"""Re-verify the EQUIVALENT tier's linked-displacement soundness in one call.

For every rule in config/GUNE5D/webfrank.json, decide what each relocated word
of the postprocessed object will BIND after the link, against what the target
binds.  This is the corpus form of the screen webfrank now runs per rule, and
it exists so the tier can be re-audited without a build: a shipped rule that
starts binding the wrong datum is invisible to fndiff `real`, to the opcode
multiset, to objdiff fuzzy and to every other webfrank guard.

It calls webfrank's own shipped surfaces -- `verify_datum_binding`,
`_function_text_relocations_full`, `_symbol_index`, `RetailImage` -- rather
than reimplementing them, so the audit and the build gate cannot drift apart.

Verdicts:
  OK              every relocated word decided at level 1-3 (name, address or
                  datum bytes).
  CORRESPONDENCE  at least one word decided only at level 4: uninitialised
                  data, where a byte comparison is vacuous and the proof is a
                  one-to-one name correspondence with a consistent addend
                  delta.  Sound, but weaker -- reported separately, never
                  totalled with OK.
  MISMATCH        a word binds something the target does not.  The rule's
                  credit is unsound; withdraw or repair it.
  NOOBJECT/NOSYMBOL  nothing to audit (build first).

usage:
  python tools/gdl/composed_census/ws_datum_tier_audit.py
  python tools/gdl/composed_census/ws_datum_tier_audit.py --unit game/enemy/enemy
  python tools/gdl/composed_census/ws_datum_tier_audit.py --verbose --out PATH

Exit status is 1 when any row is a MISMATCH.

claim.law.CQ_copy-register-fields-can-rotate-constant-load-homes-without-their-
relocations.20260903.v1 is the defect this audits for; move_logic00 was its one
instance and its rule was withdrawn in run 43.
"""
import argparse
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools" / "gdl"))

import webfrank as wf  # noqa: E402

CONFIG = ROOT / "config/GUNE5D/webfrank.json"
SYMBOLS = ROOT / "config/GUNE5D/symbols.txt"
RETAIL = ROOT / "orig/GUNE5D/sys/main.dol"

MODE_KEYS = (
    "copy_register_fields", "recolors", "register_fields",
    "instruction_permutation", "equivalent_copy_form", "equivalent_mask_form",
    "equivalent_zero_form", "value_equality_recolor",
    "post_recolor_permutation",
)


class Object:
    def __init__(self, path):
        self.data = bytearray(Path(path).read_bytes())
        self.sections = wf._sections(self.data)
        self.symbols = wf._symbol_index(self.data, self.sections)

    def function(self, name):
        symbol = wf._find_symbol(self.data, self.sections, name)
        text = self.sections[symbol.section_index]
        start = text.offset + symbol.value
        body = bytes(self.data[start:start + symbol.size])
        relocations = wf._function_text_relocations_full(
            self.data, self.sections, symbol.section_index,
            symbol.value, symbol.value + symbol.size)
        return body, relocations


def audit_unit(unit, patches, addresses, image, rows):
    ours_path = ROOT / f"build/GUNE5D/src/{unit}.o"
    target_path = ROOT / f"build/GUNE5D/obj/{unit}.o"
    have = ours_path.exists() and target_path.exists()
    ours = Object(ours_path) if have else None
    target = Object(target_path) if have else None
    for patch in patches:
        function = patch.get("function")
        row = {
            "unit": unit, "function": function,
            "modes": sorted(key for key in MODE_KEYS if patch.get(key)),
            "unproven_audit": bool(patch.get("unproven_recolor_audit")),
            "levels": {},
        }
        if not have:
            row.update(verdict="NOOBJECT", detail="build the unit first")
            rows.append(row)
            continue
        try:
            body, our_relocations = ours.function(function)
            target_body, target_relocations = target.function(function)
        except (KeyError, IndexError) as error:
            row.update(verdict="NOSYMBOL", detail=str(error))
            rows.append(row)
            continue
        row["text_equal"] = body == target_body
        words = [struct.unpack_from(">I", body, offset)[0]
                 for offset in range(0, len(body), 4)]
        correspondence = []
        try:
            levels = wf.verify_datum_binding(
                our_relocations, target_relocations, words,
                our_data=ours.data, our_sections=ours.sections,
                our_symbols=ours.symbols, symbol_addresses=addresses,
                image=image, function=function,
                correspondence_out=correspondence)
        except ValueError as failure:
            row.update(verdict="MISMATCH", detail=str(failure))
            rows.append(row)
            continue
        row["levels"] = levels
        row["correspondence"] = correspondence
        row["verdict"] = "CORRESPONDENCE" if levels["L4"] else "OK"
        row["detail"] = "; ".join(correspondence)
        rows.append(row)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--unit", action="append")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument(
        "--out", type=Path,
        default=ROOT / "build/GUNE5D/ws_datum_tier_audit.json")
    args = parser.parse_args()

    if not RETAIL.exists():
        raise SystemExit(
            f"the retail image {RETAIL} is missing, so target bindings cannot "
            f"be read; run python tools/gdl/provision_worktree.py")
    units = json.loads(CONFIG.read_text(encoding="utf-8"))["units"]
    addresses = wf.load_symbol_addresses(SYMBOLS) if SYMBOLS.exists() else None
    image = wf.RetailImage(RETAIL)

    rows = []
    for unit, patches in sorted(units.items()):
        if args.unit and unit not in args.unit:
            continue
        audit_unit(unit, patches, addresses, image, rows)

    counts, totals = {}, {"L1": 0, "L2": 0, "L3": 0, "L4": 0}
    for row in rows:
        counts[row["verdict"]] = counts.get(row["verdict"], 0) + 1
        for key, value in row["levels"].items():
            totals[key] += value
    print(f"webfrank-served functions audited: {len(rows)}")
    for verdict in sorted(counts):
        print(f"  {verdict:<15} {counts[verdict]}")
    print(f"relocated words decided: name={totals['L1']} "
          f"address={totals['L2']} datum={totals['L3']} "
          f"correspondence={totals['L4']}")
    print()
    for row in rows:
        if row["verdict"] != "OK" or args.verbose:
            print(f"{row['verdict']:<15} {row['unit']}::{row['function']} "
                  f"[{','.join(row['modes'])}] "
                  f"{'AUDITNOTE ' if row['unproven_audit'] else ''}"
                  f"{row['detail'][:400]}")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(rows, indent=2), encoding="utf-8")
    print(f"\nwrote {args.out}")
    return 1 if counts.get("MISMATCH") else 0


if __name__ == "__main__":
    sys.exit(main())
