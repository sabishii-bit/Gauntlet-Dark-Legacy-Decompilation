"""CH lane run-26 HALF 2: rerun the screens-1-3 sweep with the FIXED pair
(bipartite derivation + instruction-keyed SDA21 relocation binding).

Difference from run-25's ch_sweep.py: that script SKIPPED owned TUs outright,
so its output cannot serve as a census.  Measurement is free and ownership only
bars AUTHORING, so this derives everywhere and TAGS ownership instead.  Run-26
ownership annotations below are historical run-26 labels, not current claims.
"""
import json
import os
import sys
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
# Run-59 item 9: `HERE/../tools/gdl` is tools/gdl/tools/gdl. This one still
# ran, but only because an early sibling import happened to put tools/gdl
# on the path as a side effect.
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))
sys.path.insert(0, HERE)
import cc_artifact  # noqa: E402
import cn_census as census  # noqa: E402
import webfrank as wf  # noqa: E402
import ch_derive  # noqa: E402

# Historical run-26 ownership (2026-09-01), not a measurement filter or
# evidence of current ownership. Coordinate current authoring separately.
OWNED = {
    "game/pb/pb_window": "PW", "game/sfx/sfx": "PE", "game/world/items": "IT",
    "game/game/controls": "IH", "game/world/camera": "WF",
    "game/movie/movieplayer": "WF", "game/anim/atree": "DF/RC excl",
    "game/world/dynobjgrid": "RC excl", "game/audio/sndfx": "RC",
    "game/enemy/enemy": "RC", "game/world/gauntworld": "RC",
}


def survivors():
    out = []
    for unit in census.units():
        op, _is_raw = census.our_path(unit)
        if not op:
            continue
        try:
            odata = bytearray(open(op, "rb").read())
            tdata = bytearray(open(os.path.join(census.OBJ, unit + ".o"), "rb").read())
            osec, tsec = wf._sections(odata), wf._sections(tdata)
        except Exception:
            continue
        tmap = {s.name: s for s in census.functions(tdata, tsec)}
        for s in census.functions(odata, osec):
            t = tmap.get(s.name)
            if t is None or t.size != s.size:
                continue
            try:
                ot, tt = osec[s.section_index], tsec[t.section_index]
                ours = bytes(odata[ot.offset + s.value:ot.offset + s.value + s.size])
                tgt = bytes(tdata[tt.offset + t.value:tt.offset + t.value + t.size])
            except Exception:
                continue
            r = census.scan_function(ours, tgt)
            if r:
                out.append((unit, s.name, r[2], r[3]))
    return out


import cliscreen  # noqa: E402


def main():
    # Run-59 item 9: `--help` used to run the 9 s sweep AND REWRITE the
    # tracked ch_sweep26.json -- a help request with a side effect on disk,
    # the exact shape run-53 item 2 was raised for.
    cliscreen.help_only(__doc__)
    shipped = set(cc_artifact.load_artifact("ch_shipped.json",
                                            "ch_sweep26.py"))
    rows = []
    for unit, fn, lo, hi in survivors():
        own = OWNED.get(unit)
        mark = f"  [TU owned: {own}]" if own else ""
        ship = "  [SHIPPED]" if fn in shipped else ""
        print(f"== {unit}::{fn}{mark}{ship}")
        try:
            rule = ch_derive.run(unit, fn, lo, hi)
        except Exception:
            print("   ERROR\n" + traceback.format_exc(limit=2))
            rule = None
        rows.append({"unit": unit, "function": fn, "lo": hex(lo), "hi": hex(hi),
                     "owned": own, "shipped": fn in shipped,
                     "closes": rule is not None, "rule": rule})
    closed = [r for r in rows if r["closes"]]
    new = [r for r in closed if not r["shipped"]]
    print(f"\n=== SWEEP RESULT: {len(closed)}/{len(rows)} derive to residual 0")
    print(f"=== of those, NOT already shipped: {len(new)}")
    for r in new:
        own = f"  [owned {r['owned']}]" if r["owned"] else "  <-- AUTHORABLE"
        print(f"  {r['unit']}::{r['function']}{own}")
    written = cc_artifact.write_artifact("ch_sweep26.json", rows,
                                         cc_artifact.out_override(sys.argv))
    print(f"\nwrote {cc_artifact.artifact_label(written)}")


if __name__ == "__main__":
    main()
