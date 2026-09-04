"""WS lane (run 55): image-wide DEMAND census for webfrank's blanket r1 gate.

WHY THIS EXISTS.  `check_permutation_dependences` refuses OUTRIGHT any
permutation region that redefines r1 --

    if ("g", 1) in writes:
        raise ValueError(f"atom {index}: permutation region redefines r1")

-- before any per-resource def-use analysis in the same function runs.  The
refusal is deliberate and test-pinned (tools/gdl/tests/test_webfrank.py,
`test_a_permutation_region_redefining_r1_is_refused`, whose comment states the
reason: reordering a region that redefines r1 moves every r1-relative access in
it onto a different frame).

game/enemy/critter::CritterDamagePlayer needs exactly such a region: its
epilogue swaps `addi r1,r1,64` with `mtlr r0`, two words that share no
resource at all.  Before proposing any refinement of a shipped guard, the
Mandatory result policy requires a DEMAND CENSUS -- count the functions the
refinement would unpark and name them, and treat a count of zero as a
refutation of the premise rather than a reason to build it.  This is that
count.

WHAT IT MEASURES, and its direction of error.  A function is counted when

  * our raw body and the target body are the same size (a count-asymmetric
    residual is outside every postprocessor class by construction, per
    claim.law.webfrank-cannot-close-a-count-asymmetric-residual.20260831.v1),
  * the two streams differ somewhere, and
  * some DIFFERING word index carries an r1-writing instruction in either
    stream -- that being the word a permutation window would have to contain.

It does NOT check that a permutation actually closes the function, so it is a
deliberate UPPER BOUND: a result of N means the true demand is at most N.  That
is the right direction for a demand census, and it is the opposite of the error
ch_closable.py made (see ha_close.py's header), which simulated a design the
project had not built and under-counted.

FOUNDING RUN, 2026-09-04 at 3b2e17c46: 3316 paired equal-size functions, 598
differing, DEMAND 48.  The rows closest to CritterDamagePlayer's shape are the
small ones -- fakelib::sDvdReadSync (5 differing words), dvdlow::AlarmForWA and
::AlarmForTimeout (10 each), critter::CritterDamagePlayer (10), btext::
FontHeight (10, r1 at 0x4c/0x50, the same adjacent-epilogue shape),
recorder::sReticleDepth (11), boss::HealthMeterStart (12).

Requires a completed `ninja` so build/GUNE5D/obj (the dtk-split target) and
build/GUNE5D/src (our objects, including the raw pre-postprocess bodies) are
both current; stale objects silently misreport.  Read-only: it writes only the
--out JSON.  A hit is a CANDIDATE for the census total, never a licence to
author anything.
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))   # tools/gdl (webfrank)
sys.path.insert(0, HERE)                    # sibling lane scaffolding

import webfrank as wf  # noqa: E402
from cn_analyze import our_object, target_object, load  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
DEFAULT_OUT = os.path.join("build", "GUNE5D", "ws_r1_demand_census.json")


def writes_r1(word):
    """Does this instruction write GPR 1, as webfrank's own decoder sees it?"""
    try:
        _reads, writes = wf._word_effects(word)
    except ValueError:
        return False
    return ("g", 1) in writes


def units(obj_root):
    out = []
    for root, _dirs, files in os.walk(obj_root):
        for name in files:
            if name.endswith(".o"):
                path = os.path.join(root, name)
                out.append(
                    os.path.relpath(path, obj_root)[:-2].replace("\\", "/"))
    return sorted(out)


def census():
    obj_root = os.path.join("build", "GUNE5D", "obj")
    if not os.path.isdir(obj_root):
        sys.exit(f"missing {obj_root} — run `ninja` first "
                 "(or provision_worktree.py --resplit)")
    paired = differing = skipped = 0
    rows = []
    for unit in units(obj_root):
        try:
            ours_path, _kind = our_object(unit)
            target_path = target_object(unit)
        except Exception:  # noqa: BLE001
            skipped += 1
            continue
        if not (os.path.exists(ours_path) and os.path.exists(target_path)):
            skipped += 1
            continue
        try:
            data = open(ours_path, "rb").read()
            sections = wf._sections(bytearray(data))
            names = [s.name for s in wf._symbols(bytearray(data), sections)
                     if s.size and s.name]
        except Exception:  # noqa: BLE001
            skipped += 1
            continue
        for function in names:
            try:
                *_, ours, _r, _j = load(ours_path, function)
                *_, target, _r2, _j2 = load(target_path, function)
            except Exception:  # noqa: BLE001
                continue
            if not ours or len(ours) != len(target) or len(ours) % 4:
                continue
            paired += 1
            diffs = [o for o in range(0, len(ours), 4)
                     if wf._u32(ours, o) != wf._u32(target, o)]
            if not diffs:
                continue
            differing += 1
            marked = [o for o in diffs
                      if writes_r1(wf._u32(ours, o))
                      or writes_r1(wf._u32(target, o))]
            if marked:
                rows.append({"unit": unit, "function": function,
                             "differing_words": len(diffs),
                             "r1_words": [hex(o) for o in marked]})
    rows.sort(key=lambda row: row["differing_words"])
    return paired, differing, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", default=DEFAULT_OUT,
                        help=f"report path (default {DEFAULT_OUT})")
    parser.add_argument("--limit", type=int, default=0,
                        help="print at most N rows (0 = all)")
    args = parser.parse_args()

    paired, differing, rows = census()
    print(f"paired equal-size functions: {paired}")
    print(f"  of those, differing:       {differing}")
    print(f"  DEMAND (upper bound): {len(rows)} function(s) whose residual "
          f"touches an r1-writing word")
    for row in (rows[:args.limit] if args.limit else rows):
        print(f"    {row['unit']}::{row['function']}  "
              f"{row['differing_words']} differing word(s), r1 at "
              f"{','.join(row['r1_words'])}")
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", encoding="utf-8") as handle:
        json.dump({"paired": paired, "differing": differing,
                   "demand": len(rows), "rows": rows}, handle, indent=1)
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
