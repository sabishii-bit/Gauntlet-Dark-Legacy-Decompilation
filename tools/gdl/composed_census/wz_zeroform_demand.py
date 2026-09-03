"""WZ lane (run 41): DEMAND CENSUS for the shipped LIVE-ZERO VALUE class.

The question this answers is the one AGENTS.md requires of a capability
census and that the run-37 law restates: which words can ONLY
`equivalent_zero_form` serve, counted as WORD PAIRS and never as a family
label -- and, separately, which functions would that unpark.

Per differing word (index-aligned, count-symmetric functions only) we ask,
in this order:

  RELOCATED    either object relocates the word -> outside every class.
  COPY_SERVED  both words decode through `decode_copy_form`, i.e. the
               EXISTING equivalent_copy_form modes model the site (with or
               without a recolor stage).  This is the half of the live-zero
               family that never needed a new class.
  ZERO_ONLY    our word is NOT a copy form but both words decode through
               `decode_zero_form_destination`, destinations AGREE, and the
               known-zero-bit dataflow PROVES both results are the literal
               zero.  This is the exact niche equivalent_zero_form was
               commissioned for, and it is counted here as PROVED demand,
               not candidate demand.
  ZERO_BLOCKED the shape is right but the proof refuses; the refusal string
               is kept so a reader gets the instrumented reason.

The offset-paired scan is blind to a member whose two words are TRANSPOSED
(claim.law.WF_live-zero-family-straddles-the-copy-form-boundary...
population_note -- BOTH shipped members are transposed).  So every adjacent
pair of differing words is re-tested after swapping OUR two words, and any
site that becomes ZERO_ONLY or COPY_SERVED is reported in the TRANSPOSED
columns.  That is the widening the population_note says the founding census
lacked.

    python WZ_scratch/wz_zeroform_demand.py [--out PATH]
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    _parent = os.path.dirname(ROOT)
    if _parent == ROOT:
        raise SystemExit("repo root not found above " + HERE)
    ROOT = _parent
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                                     # noqa: E402
from cn_analyze import our_object, target_object, load     # noqa: E402
from unabsorbed import opcode_key, rule_served_functions   # noqa: E402


def units():
    base_dir = os.path.join(ROOT, "build/GUNE5D/obj")
    for base, _dirs, files in os.walk(base_dir):
        for name in files:
            if not name.endswith(".o"):
                continue
            path = os.path.join(base, name)
            unit = os.path.relpath(path, base_dir)
            yield unit[:-2].replace("\\", "/")


def symbols(path):
    data = open(path, "rb").read()
    sections = wf._sections(data)
    return [s.name for s in wf._symbols(data, sections) if s.size and s.name]


def classify(words, offset, ours_word, target_word, successors, calls,
             relocated, relocation_types):
    """Return (verdict, detail) for one differing word."""
    ours_copy = wf.decode_copy_form(ours_word)
    target_copy = wf.decode_copy_form(target_word)
    if ours_copy is not None and target_copy is not None:
        return "COPY_SERVED", f"{ours_copy} vs {target_copy}"
    try:
        ours_destination = wf.decode_zero_form_destination(ours_word)
        target_destination = wf.decode_zero_form_destination(target_word)
    except ValueError as failure:
        return "OTHER", str(failure)
    if ours_copy is not None:
        # ours IS a copy form; only the target side is exotic.  The copy-form
        # class owns the arrow it can decode, so this is not the zero niche.
        return "OTHER", "our word is a copy form, target is not"
    if ours_destination != target_destination:
        return "ZERO_BLOCKED", (
            f"destinations differ (r{ours_destination} vs "
            f"r{target_destination}) -- equivalent_zero_form proves a VALUE, "
            f"never a renaming")
    for label, candidate in (("ours", ours_word), ("target", target_word)):
        try:
            wf.prove_zero_result(
                words, offset // 4, candidate, ours_destination,
                successors, calls, relocated,
                relocation_types=relocation_types)
        except ValueError as failure:
            return "ZERO_BLOCKED", f"{label}: {failure}"
    return "ZERO_ONLY", f"both write the literal 0 to r{ours_destination}"


def scan(unit, name, ours_path, target_path, pinned):
    _a, _b, _c, ours, orel, ojt = load(ours_path, name)
    _d, _e, _f, target, trel, _tj = load(target_path, name)
    if len(ours) != len(target) or not ours or len(ours) % 4:
        return None
    diffs = [o for o in range(0, len(ours), 4)
             if wf._u32(ours, o) != wf._u32(target, o)]
    if not diffs:
        return None
    mnem = sum(1 for o in range(0, len(ours), 4)
               if opcode_key(wf._u32(ours, o)) != opcode_key(wf._u32(target, o)))
    relocated = {o // 4 for o in orel}
    target_relocated = {o // 4 for o in trel}
    words = [wf._u32(ours, o) for o in range(0, len(ours), 4)]
    successors, calls = wf._successors(
        words, relocated, {o // 4 for o in ojt})
    # The relocation TYPE map is not optional: passing None makes the
    # known-zero dataflow distrust every relocated word, which refuses
    # sites the shipped rules prove.  apply_patch always builds this.
    relocation_types = {
        offset // 4: reloc_type for offset, (reloc_type, _n) in orel.items()
    }

    tally = {"COPY_SERVED": 0, "ZERO_ONLY": 0, "ZERO_BLOCKED": 0,
             "OTHER": 0, "RELOCATED": 0}
    sites = []
    for offset in diffs:
        if offset // 4 in relocated or offset // 4 in target_relocated:
            tally["RELOCATED"] += 1
            continue
        verdict, detail = classify(
            words, offset, wf._u32(ours, offset), wf._u32(target, offset),
            successors, calls, relocated, relocation_types)
        tally[verdict] += 1
        if verdict in ("ZERO_ONLY", "ZERO_BLOCKED"):
            sites.append({"at": hex(offset), "verdict": verdict,
                          "ours": f"{wf._u32(ours, offset):08x}",
                          "target": f"{wf._u32(target, offset):08x}",
                          "detail": detail})

    # TRANSPOSED widening: swap OUR two words at each adjacent differing pair
    # and re-ask.  This is what an offset-paired census cannot see.
    transposed = {"COPY_SERVED": 0, "ZERO_ONLY": 0}
    transposed_sites = []
    for first, second in zip(diffs, diffs[1:]):
        if second != first + 4:
            continue
        swapped = list(words)
        swapped[first // 4], swapped[second // 4] = (
            swapped[second // 4], swapped[first // 4])
        # A permutation moves each word's RELOCATION with its atom, so the
        # relocation sets must be permuted too.  Screening the pre-swap sets
        # is what made this census blind to the class's own shipped members
        # (btext::FontInit: our ADDR16_LO at index 10, the target's at 11,
        # so both words read as RELOCATED at fixed offsets and the site
        # vanished).  That blindness is the population_note of
        # claim.law.WF_live-zero-family-straddles-the-copy-form-boundary...
        def _swap(indexes):
            out = set(indexes)
            a, b = first // 4, second // 4
            in_a, in_b = a in out, b in out
            out.discard(a)
            out.discard(b)
            if in_a:
                out.add(b)
            if in_b:
                out.add(a)
            return out
        relocated2 = _swap(relocated)
        target_relocated2 = target_relocated
        a, b = first // 4, second // 4
        relocation_types2 = {
            (b if index == a else a if index == b else index): kind
            for index, kind in relocation_types.items()
        }
        successors2, calls2 = wf._successors(
            swapped, relocated2, {o // 4 for o in ojt})
        for offset in (first, second):
            if swapped[offset // 4] == wf._u32(target, offset):
                continue
            if offset // 4 in relocated2 or offset // 4 in target_relocated2:
                continue
            verdict, detail = classify(
                swapped, offset, swapped[offset // 4],
                wf._u32(target, offset), successors2, calls2,
                relocated2, relocation_types2)
            if verdict in ("ZERO_ONLY", "COPY_SERVED"):
                transposed[verdict] += 1
                transposed_sites.append({
                    "window": f"[{hex(first)},{hex(second + 4)}) order [1,0]",
                    "at": hex(offset), "verdict": verdict, "detail": detail})

    return {
        "unit": unit, "function": name, "insns": len(ours) // 4,
        "differing_words": len(diffs), "mnemonic_divergence": mnem,
        "pinned": pinned, "tally": tally, "sites": sites,
        "transposed": transposed, "transposed_sites": transposed_sites,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--out", default=os.path.join(
        ROOT, "build/GUNE5D/wz_zeroform_demand.json"))
    arguments = parser.parse_args()

    rows = []
    scanned = 0
    skipped = []
    for unit in sorted(units()):
        try:
            ours_path, _kind = our_object(unit)
            target_path = target_object(unit)
        except Exception as error:
            skipped.append((unit, "*", f"objects: {error}"))
            continue
        if not (os.path.exists(ours_path) and os.path.exists(target_path)):
            continue
        try:
            names = set(symbols(target_path)) & set(symbols(ours_path))
            pins = rule_served_functions(unit, ROOT)
        except Exception as error:
            skipped.append((unit, "*", f"symbols: {error}"))
            continue
        for name in sorted(names):
            try:
                row = scan(unit, name, ours_path, target_path, name in pins)
            except Exception as error:
                skipped.append((unit, name, f"{type(error).__name__}: {error}"))
                continue
            if row is None:
                continue
            scanned += 1
            if (row["tally"]["ZERO_ONLY"] or row["tally"]["ZERO_BLOCKED"]
                    or row["transposed"]["ZERO_ONLY"]
                    or row["tally"]["RELOCATED"]):
                rows.append(row)

    rows.sort(key=lambda r: (-r["tally"]["ZERO_ONLY"],
                             -r["transposed"]["ZERO_ONLY"],
                             r["differing_words"]))
    print(f"count-symmetric functions with a residual scanned: {scanned}")
    print(f"functions with any zero-niche row: {len(rows)}")
    proved = [r for r in rows if r["tally"]["ZERO_ONLY"]
              or r["transposed"]["ZERO_ONLY"]]
    print(f"functions with a PROVED zero-only word (direct or transposed): "
          f"{len(proved)}")
    print()
    header = (f"{'unit::function':58} {'insns':>5} {'words':>5} {'mnem':>4} "
              f"{'pin':>3} {'Z':>2} {'Zt':>2} {'Zblk':>4} {'copy':>4}")
    print(header)
    print("-" * len(header))
    for row in rows:
        print(f"{row['unit'] + '::' + row['function']:58} "
              f"{row['insns']:5} {row['differing_words']:5} "
              f"{row['mnemonic_divergence']:4} "
              f"{'YES' if row['pinned'] else '-':>3} "
              f"{row['tally']['ZERO_ONLY']:2} "
              f"{row['transposed']['ZERO_ONLY']:2} "
              f"{row['tally']['ZERO_BLOCKED']:4} "
              f"{row['tally']['COPY_SERVED']:4}")
    print(f"\nSKIPPED (a census that swallows errors reports a false "
          f"all-clear): {len(skipped)}")
    for unit, name, reason in skipped[:40]:
        print(f"  {unit}::{name}  {reason}")
    os.makedirs(os.path.dirname(arguments.out), exist_ok=True)
    json.dump({"scanned": scanned, "rows": rows,
               "skipped": [{"unit": u, "function": f, "reason": r}
                           for u, f, r in skipped]},
              open(arguments.out, "w"), indent=2)
    print(f"\nwrote {arguments.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
