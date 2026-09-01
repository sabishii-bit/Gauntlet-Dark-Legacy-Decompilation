#!/usr/bin/env python3
"""Stack-slot occupancy diff — the honest arbiter for frame/slot work.

`real` actively fights slot work: a design one 4-byte step from a
slot-exact map can score REGRESSED while four chained real-wins land
further from target (two lanes measured this independently). This tool
compares the actual r1 occupancy maps.

Usage:
  python tools/gdl/slotdiff.py game/boss/bosscam BossCamBossCalc

Per side it collects every r1 displacement from loads/stores
(N(r1) operands) AND from address-takes (addi rX,r1,N — invisible to a
displacement-only scan; 48 bytes of address-taken arrays hid there in
one session). Output: side-by-side slot sets, the symmetric difference,
and a verdict line. Frame size and save-set are printed for context.
Slots are a lower bound on occupancy (a slot only touched through a
copied pointer stays invisible) — treat IDENTICAL as strong evidence,
not proof; the gate still owns bytes.
"""

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fndiff  # noqa: E402

VERSION = "GUNE5D"
DISP_RE = re.compile(r"(-?\d+)\(r1\)")
ADDR_RE = re.compile(r"^addi\s+r\d+,\s*r1,\s*(-?\d+)\b")


def slot_map(lines):
    """{offset: use_count} over displacement uses and address-takes."""
    slots = {}
    for ln in lines:
        if ln.startswith("    "):
            continue
        for m in DISP_RE.finditer(ln):
            off = int(m.group(1))
            slots[off] = slots.get(off, 0) + 1
        m = ADDR_RE.match(ln)
        if m:
            off = int(m.group(1))
            slots[off] = slots.get(off, 0) + 1
    return slots


def save_set(lines):
    for ln in lines:
        m = re.match(r"^stmw\s+(r\d+),", ln)
        if m:
            return m.group(1)
    return "(no stmw)"


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) != 2:
        print(__doc__)
        return 2
    unit, fn = args
    unit = unit.replace("\\", "/").removeprefix("src/")
    unit = re.sub(r"\.(c|cpp)$", "", unit)
    target = fndiff.parse(Path(f"build/{VERSION}/obj/{unit}.o"))
    ours = fndiff.parse(Path(f"build/{VERSION}/src/{unit}.o"))
    def resolve(table, name):
        """Find `name` allowing a dtk `_80XXXXXX` suffix on either side —
        the suffix mismatch made this tool unavailable for exactly the
        functions one roster pointed it at."""
        if name in table:
            return name
        for cand in table:
            if cand.startswith(name + "_80") or name.startswith(cand + "_80"):
                return cand
        return None

    fn_t, fn_o = resolve(target, fn), resolve(ours, fn)
    if fn_t is None or fn_o is None:
        print(f"missing: {fn} (target: {fn_t is not None},"
              f" ours: {fn_o is not None})")
        return 1
    t, o = target[fn_t], ours[fn_o]
    ts, os_ = slot_map(t), slot_map(o)
    st, so = save_set(t), save_set(o)
    if st != so:
        # A save-set delta means an UNALLOCATED CALLEE-SAVED REGISTER, not
        # a local slot — the single most decisive fact for slot work, and a
        # worker mis-modeled a session by reading it as a scalar slot.
        print(f"!! SAVE-SET DELTA: target {st} vs ours {so} — the residual"
              " includes a callee-saved allocation difference, not (only)"
              " local slots")
    print(f"frame: target {fndiff.frame_size(t)}  ours {fndiff.frame_size(o)}"
          f"   saves: target {st}  ours {so}")
    if "--all" in sys.argv:
        # Full side-by-side map: the exclusive pair alone was genuinely
        # misleading twice (an independent temp region read as a moved
        # slot; two cancelling pad errors read as a clean frame). Workers
        # kept scripting this by hand.
        print("-- full slot map (offset: target-uses / ours-uses) --")
        for off in sorted(set(ts) | set(os_)):
            print(f"  {off:>4}: T {ts.get(off, '-'):>3} / O"
                  f" {os_.get(off, '-'):>3}")
    only_t = sorted(set(ts) - set(os_))
    only_o = sorted(set(os_) - set(ts))
    count_diff = sorted(off for off in set(ts) & set(os_)
                        if ts[off] != os_[off])
    for off in only_t:
        print(f"TARGET-ONLY slot {off:>4}  (uses {ts[off]})")
    for off in only_o:
        print(f"OURS-ONLY   slot {off:>4}  (uses {os_[off]})")
    for off in count_diff:
        print(f"USE-COUNT   slot {off:>4}  target {ts[off]} vs ours {os_[off]}")
    if only_t or only_o:
        verdict = f"SLOTS DIFFER ({len(only_t)}T/{len(only_o)}O exclusive)"
    elif count_diff:
        verdict = f"SLOTS ALIGNED, {len(count_diff)} use-count deltas"
    else:
        verdict = "SLOT MAP IDENTICAL"
    print(f"== {fn}: target {len(ts)} slots, ours {len(os_)} -> {verdict}")
    print("VERDICT (repeated):", verdict,
          "-- arbitrate slot work HERE, not on real"
          " (claim.law.real-can-underweight-a-large-alignment-gain)")
    print("CAVEAT: this verdict covers the r1 local block ONLY — EH-frame"
          " slots addressed off r31 are invisible (an IDENTICAL verdict"
          " coexisted with a live 8-byte EH-slot delta); on C++/EH"
          " functions also read the r31-relative addi/lwz rows.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
