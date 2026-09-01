"""HV lane run-28: RECOLOR-REPAIR permutation search.

THE BLIND SPOT THIS EXISTS FOR.  Every census built so far (cn_census,
ch_census26, hb_census, and my own run-27 filter) derives its permutation
windows from the UNABSORBED words -- the ones that are neither identical nor a
pure register-field difference.  That is sound for the schedule class, and it
is BLIND to a whole population:

    a permutation whose atoms are ALL pure register-field differences.

Such a window contributes ZERO unabsorbed words, so no census forms a cluster
there and no searcher ever proposes it.  But it is exactly what turns a
NON-BIJECTIVE recolor into a bijective one: when our build and the target both
initialise two interchangeable values (two `li rD,0`, two `mr rD,r3`) and
assign them to each other's registers, the positional renaming is two-valued at
the first merge that joins them, and verify_consistent_recolor refuses -- while
transposing the two definitions makes the SAME recolor a clean bijection.

The refusal offset is therefore a POINTER, not a verdict: it names the merge
where the two-valued binding was observed, and the repair lives at the pair of
definitions upstream of it.  This module reads the refusal, enumerates every
shape- and relocation-consistent contiguous window upstream that is legal in
our colouring, and selects by proof through the shipped apply_patch.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# Repo root, located by landmark so this file runs unchanged from the
# lane scratch directory AND from tools/gdl/composed_census after
# promotion (discipline 17: a promoted script must actually run).
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    parent = os.path.dirname(ROOT)
    if parent == ROOT:
        raise SystemExit("repo root not found above " + HERE)
    ROOT = parent
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))
sys.path.insert(0, HERE)

import webfrank as wf          # noqa: E402
import ha_close as ha          # noqa: E402
import hv_perm as hv           # noqa: E402
from cn_analyze import our_object, target_object, load  # noqa: E402

# apply_patch wraps the refusal as "... (+0xNN: use of gA does not ...)",
# but webfrank_audit prints the bare exception message "+0xNN: use of ...".
# The parenthesised-only pattern parsed 0 of 11 audit rejections (measured
# 2026-09-01), so a lane pasting an audit reason in here got refusal_offset
# None and no repair search at all. Accept both spellings.
AT = re.compile(r"\(?\+0x([0-9a-f]+): ")
TYPE_ERRORS = []
MAX_ATOMS = 4
ROUNDS = 8


def refusal_offset(note):
    m = AT.search(note or "")
    return int(m.group(1), 16) if m else None


def candidate_windows(ours, tgt, orel, trel, limit_off, taken):
    """Every contiguous window upstream of `limit_off` admitting a non-identity
    shape- and relocation-consistent reordering that is legal in OUR colouring.
    """
    out = []
    n = len(ours)
    for size in range(2, MAX_ATOMS + 1):
        for lo in range(0, n - 4 * size + 4, 4):
            hi = lo + 4 * size
            if hi > limit_off + 4:
                continue
            if any(not (hi <= a or lo >= b) for a, b in taken):
                continue
            if not hv.window_ok(ours, tgt, lo, hi):
                continue
            for order in hv.enumerate_orders(ours, tgt, orel, trel, lo, hi,
                                             limit=24):
                try:
                    wf.check_permutation_dependences(bytes(ours[lo:hi]), order)
                except ValueError:
                    continue
                except TypeError as exc:
                    # SHIPPED-GUARD DEFECT, reported not worked around blindly:
                    # webfrank.check_permutation_dependences line ~956 sorts
                    # `broken`, a list of (atom, resource) keys whose resource
                    # is heterogeneous -- a str ("mem", "lr", "cr") for some
                    # resources and a tuple (('g', 4), ("stack", N)) for
                    # others.  Two broken chains on the SAME atom therefore
                    # compare str against tuple and raise TypeError from the
                    # ERROR PATH.  The refusal itself is correct; only its
                    # formatting dies.  Consequence: a legitimate refusal
                    # reaches every caller as an uncaught TypeError instead of
                    # the ValueError they all catch, so one bad candidate
                    # aborts a whole sweep instead of being skipped.
                    TYPE_ERRORS.append((lo, hi, tuple(order), str(exc)))
                    continue
                out.append({"lo": lo, "hi": hi, "order": order})
    return out


def repair(unit, fn, verbose=True):
    op, _k = our_object(unit)
    tp = target_object(unit)
    if not (os.path.exists(op) and os.path.exists(tp)):
        return None, "no object"
    _a, _b, _c, ours, orel, ojt = load(op, fn)
    _d, _e, _f, tgt, trel, _g = load(tp, fn)
    if len(ours) != len(tgt):
        return None, f"size mismatch ({len(ours)//4} vs {len(tgt)//4})"

    wins = []
    rule, _r, note = ha.build_rule(unit, fn)
    if rule:
        return rule, "TIER-A, no repair needed"
    for rnd in range(ROUNDS):
        off = refusal_offset(note)
        if off is None:
            return None, f"not a recolor refusal: {note}"
        if verbose:
            print(f"  round {rnd}: refusal at +0x{off:x}")
        taken = [(w["lo"], w["hi"]) for w in wins]
        cands = candidate_windows(ours, tgt, orel, trel, off, taken)
        if verbose:
            print(f"    {len(cands)} upstream candidate window(s)")
        best = None
        for cand in cands:
            trial = sorted(wins + [cand], key=lambda w: w["lo"])
            try:
                r2, _res, n2 = ha.build_rule(unit, fn, pre=trial)
            except Exception as exc:                       # noqa: BLE001
                continue
            if r2:
                return r2, (f"CLOSES after {len(trial)} repair window(s): "
                            + ", ".join(f"+0x{w['lo']:x}..+0x{w['hi']:x}"
                                        f"{w['order']}" for w in trial))
            o2 = refusal_offset(n2)
            if o2 is not None and o2 > off and (best is None or o2 > best[0]):
                best = (o2, cand, n2)
        if best is None:
            return None, f"no upstream repair advances past +0x{off:x}: {note}"
        wins = sorted(wins + [best[1]], key=lambda w: w["lo"])
        note = best[2]
        if verbose:
            print(f"    took +0x{best[1]['lo']:x}..+0x{best[1]['hi']:x} "
                  f"{best[1]['order']} -> refusal now +0x{best[0]:x}")
    return None, f"still refusing after {ROUNDS} rounds: {note}"


if __name__ == "__main__":
    import json
    u, f = sys.argv[1], sys.argv[2]
    r, note = repair(u, f)
    print(f"{u}::{f}: {note}")
    if TYPE_ERRORS:
        lo, hi, order, msg = TYPE_ERRORS[0]
        print(f"GUARD DEFECT: check_permutation_dependences raised TypeError "
              f"({msg}) on window +0x{lo:x}..+0x{hi:x} order {list(order)}; "
              f"{len(TYPE_ERRORS)} occurrence(s)")
    if r:
        print(json.dumps(r, indent=1))
