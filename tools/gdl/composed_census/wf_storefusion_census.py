#!/usr/bin/env python3
"""WF lane: image-wide DEMAND CENSUS for the STORE-WITH-UPDATE FUSION class.

Commissioned by work_claim.stfsu-fusion.20260902.v1.  AGENTS.md "Mandatory
result policy" requires a demand census BEFORE a postprocessor capability is
built: a sound capability with zero customers is a guard surface maintained
forever in exchange for nothing (WF, run 34).  This counts the customers.

THE SHAPE.  MWCC may emit a store-with-update

    stfsu fS,d(rB)          # rB := rB + d ; MEM[rB] := fS

where our build emits the split pair

    addi rA,rB,d            # rA := rB + d
    stfs fS,d(rB)           # MEM[rB + d] := fS

The two streams agree on the stored value, on the effective address and on
the post-state of the updated base register, but they are DIFFERENT WORDS and
the correspondence is n-to-m (one fused word against two split words), which
no shipped webfrank proof mode models.

WHAT IS COUNTED.  For every function whose target and our-object sizes agree,
the census takes the OPCODE-HISTOGRAM DELTA (target minus ours) and reports
any function where an update-form store appears more often in one stream than
the other.  The update/plain store pairs are the five D-form families:

    stwu/stw  stbu/stb  sthu/sth  stfsu/stfs  stfdu/stfd

Load-side update forms (lwzu/lfsu/...) are counted in the same pass and
reported separately: the corpus's existing update-form laws are all
load-side, so the load column says whether a load-side sibling class would
have a larger population than the store-side one this lane was commissioned
for.

DIRECTION MATTERS.  `target_fused` (the target has the update form, we emit
the split pair) is the direction swbos needs.  `ours_fused` is the inverse
and would need its own proof; it is reported so the table is not silently
one-sided.

LIMITS, stated because the number does not announce them:
  * Functions of unequal size are skipped entirely.  A count-asymmetric
    residual is provably outside every postprocessor class (AGENTS.md
    proposal gate 2), so this is the right screen, but it means the census
    measures POSTPROCESSOR-ELIGIBLE demand, not every stfsu in the image.
  * A histogram delta is not a proof that the split pair is present; it says
    the update form is unbalanced.  Confirm every hit with `fndiff --ops`
    before believing it (the same limit every offset-free census carries).
  * Our side is read from the RAW `.postprocess/body` object where one
    exists, so already-shipped webfrank rules stay visible instead of being
    laundered into "no residual" (canary discipline, cn_census.py).

Run from the repository root after a completed `ninja`:

    python tools/gdl/composed_census/wf_storefusion_census.py [--json PATH]
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools" / "gdl"))
import webfrank as wf  # noqa: E402

BUILD = ROOT / "build" / "GUNE5D"
OBJ = BUILD / "obj"
SRC = BUILD / "src"

# D-form store families: update opcode -> plain opcode.
STORE_PAIRS = {37: 36, 39: 38, 45: 44, 53: 52, 55: 54}
# D-form load families: update opcode -> plain opcode.
LOAD_PAIRS = {33: 32, 35: 34, 41: 40, 43: 42, 49: 48, 51: 50}

MNEMONIC = {
    32: "lwz", 33: "lwzu", 34: "lbz", 35: "lbzu",
    36: "stw", 37: "stwu", 38: "stb", 39: "stbu",
    40: "lhz", 41: "lhzu", 42: "lha", 43: "lhau",
    44: "sth", 45: "sthu", 48: "lfs", 49: "lfsu",
    50: "lfd", 51: "lfdu", 52: "stfs", 53: "stfsu",
    54: "stfd", 55: "stfdu",
}


def units() -> list[str]:
    out = []
    for dirpath, _dirs, files in os.walk(OBJ):
        for name in files:
            if name.endswith(".o"):
                path = Path(dirpath) / name
                out.append(str(path.relative_to(OBJ))[:-2].replace("\\", "/"))
    return sorted(out)


def our_path(unit: str):
    """(path, is_raw_body) for our object, preferring the pre-webfrank body."""
    if "/" not in unit:
        plain = SRC / (unit + ".o")
        return (plain, False) if plain.exists() else (None, False)
    directory, base = unit.rsplit("/", 1)
    body = SRC / directory / ".postprocess" / "body" / (base + ".o")
    if body.exists():
        return body, True
    plain = SRC / (unit + ".o")
    return (plain, False) if plain.exists() else (None, False)


def text_functions(data, sections):
    out = []
    for symbol in wf._symbols(data, sections):
        if not symbol.size or symbol.size % 4:
            continue
        if symbol.section_index >= len(sections):
            continue
        if not sections[symbol.section_index].name.startswith(".text"):
            continue
        out.append(symbol)
    return out


def opcode_histogram(body: bytes) -> Counter:
    return Counter(
        wf._u32(body, offset) >> 26 for offset in range(0, len(body), 4)
    )


def differing_words(ours: bytes, target: bytes) -> int:
    return sum(
        1 for offset in range(0, len(ours), 4)
        if wf._u32(ours, offset) != wf._u32(target, offset)
    )


def words(body: bytes) -> Counter:
    return Counter(
        wf._u32(body, offset) for offset in range(0, len(body), 4)
    )


def unreachable_by_permutation(ours: bytes, target: bytes) -> int:
    """Words of ours that NO reordering of our stream can turn into a target
    word.

    The shipped `instruction_permutation` mode reorders our words 1-to-1 and
    changes none of them, so the multiset intersection is an exact upper bound
    on what permutation alone can ever reach.  This is the number that decides
    whether a residual is a schedule difference wearing an opcode costume: it
    needs no offset pairing and therefore cannot invent rows the way every
    positional census can.
    """
    ours_words = words(ours)
    target_words = words(target)
    common = sum((ours_words & target_words).values())
    return len(ours) // 4 - common


def scan():
    rows = []
    exact = 0
    compared = 0
    for unit in units():
        our_object, is_raw = our_path(unit)
        if our_object is None:
            continue
        try:
            our_data = bytearray(our_object.read_bytes())
            target_data = bytearray((OBJ / (unit + ".o")).read_bytes())
            our_sections = wf._sections(our_data)
            target_sections = wf._sections(target_data)
        except Exception:
            continue
        target_map = {
            symbol.name: symbol
            for symbol in text_functions(target_data, target_sections)
        }
        for symbol in text_functions(our_data, our_sections):
            target_symbol = target_map.get(symbol.name)
            if target_symbol is None or target_symbol.size != symbol.size:
                continue
            our_text = our_sections[symbol.section_index]
            target_text = target_sections[target_symbol.section_index]
            ours = bytes(our_data[
                our_text.offset + symbol.value:
                our_text.offset + symbol.value + symbol.size
            ])
            target = bytes(target_data[
                target_text.offset + target_symbol.value:
                target_text.offset + target_symbol.value + target_symbol.size
            ])
            compared += 1
            if ours == target:
                exact += 1
                continue
            our_histogram = opcode_histogram(ours)
            target_histogram = opcode_histogram(target)

            store_target, store_ours = {}, {}
            load_target, load_ours = {}, {}
            for pairs, to_target, to_ours in (
                (STORE_PAIRS, store_target, store_ours),
                (LOAD_PAIRS, load_target, load_ours),
            ):
                for update, plain in pairs.items():
                    delta = target_histogram[update] - our_histogram[update]
                    if delta > 0:
                        to_target[MNEMONIC[update]] = delta
                    elif delta < 0:
                        to_ours[MNEMONIC[update]] = -delta
                    del plain
            if not (store_target or store_ours or load_target or load_ours):
                continue
            # THE PAYOFF NUMBER the mandatory-result policy demands.  A fused
            # site costs at most two words on our side (the split base advance
            # plus the plain store), so `residual_after_class` is a GENEROUS
            # lower bound on what would still differ if this class shipped and
            # discharged every fused site in the function.  A customer the
            # class would UNPARK is one whose residual_after_class is 0.
            fused_sites = sum(store_target.values()) + sum(store_ours.values())
            differing = differing_words(ours, target)
            rows.append({
                "unit": unit,
                "function": symbol.name,
                "insns": symbol.size // 4,
                "differing_words": differing,
                "fused_store_sites": fused_sites,
                "residual_after_class": max(differing - 2 * fused_sites, 0),
                "unreachable_by_permutation": unreachable_by_permutation(
                    ours, target
                ),
                "raw_body": is_raw,
                "store_fused_in_target": store_target,
                "store_fused_in_ours": store_ours,
                "load_fused_in_target": load_target,
                "load_fused_in_ours": load_ours,
            })
    rows.sort(key=lambda row: (-sum(row["store_fused_in_target"].values()),
                               row["differing_words"], row["function"]))
    return rows, compared, exact


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--json", default=str(BUILD / "wf_storefusion_census.json"),
        help="where to write the full roster (default under build/GUNE5D/)",
    )
    args = parser.parse_args()

    rows, compared, exact = scan()
    store_rows = [row for row in rows if row["store_fused_in_target"]]
    inverse_rows = [
        row for row in rows
        if row["store_fused_in_ours"] and not row["store_fused_in_target"]
    ]
    load_rows = [
        row for row in rows
        if (row["load_fused_in_target"] or row["load_fused_in_ours"])
        and not row["store_fused_in_target"] and not row["store_fused_in_ours"]
    ]

    print(f"compared {compared} equal-size function pair(s); "
          f"{exact} already byte-identical")
    print()
    print(f"STORE-WITH-UPDATE FUSED IN TARGET (the commissioned direction): "
          f"{len(store_rows)}")
    for row in store_rows:
        forms = " ".join(
            f"+{count} {name}" for name, count in
            sorted(row["store_fused_in_target"].items())
        )
        print(f"  {row['unit']}::{row['function']}  "
              f"insns={row['insns']} words={row['differing_words']}  {forms}")
        print(f"      residual_after_class={row['residual_after_class']}  "
              f"unreachable_by_permutation="
              f"{row['unreachable_by_permutation']}")
    unparked = [
        row for row in store_rows if row["residual_after_class"] == 0
    ]
    print(f"  UNPARK PAYOFF (customers this class alone would close): "
          f"{len(unparked)}")
    print()
    print(f"STORE-WITH-UPDATE FUSED IN OURS (the inverse direction): "
          f"{len(inverse_rows)}")
    for row in inverse_rows:
        forms = " ".join(
            f"+{count} {name}" for name, count in
            sorted(row["store_fused_in_ours"].items())
        )
        print(f"  {row['unit']}::{row['function']}  "
              f"insns={row['insns']} words={row['differing_words']}  {forms}")
    print()
    print(f"LOAD-side update-form imbalance only (sibling class): "
          f"{len(load_rows)}")
    for row in load_rows:
        forms = " ".join(
            [f"T+{count} {name}" for name, count in
             sorted(row["load_fused_in_target"].items())]
            + [f"O+{count} {name}" for name, count in
               sorted(row["load_fused_in_ours"].items())]
        )
        print(f"  {row['unit']}::{row['function']}  "
              f"insns={row['insns']} words={row['differing_words']}  {forms}")

    Path(args.json).parent.mkdir(parents=True, exist_ok=True)
    Path(args.json).write_text(json.dumps({
        "compared": compared,
        "exact": exact,
        "store_fused_in_target": store_rows,
        "store_fused_in_ours": inverse_rows,
        "load_only": load_rows,
    }, indent=2), encoding="utf-8")
    print()
    print(f"wrote {args.json}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
