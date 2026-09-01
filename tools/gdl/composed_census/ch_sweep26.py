"""CH lane run-26 HALF 2: rerun the screens-1-3 sweep with the FIXED pair
(bipartite derivation + instruction-keyed SDA21 relocation binding).

Difference from run-25's ch_sweep.py: that script SKIPPED owned TUs outright,
so its output cannot serve as a census.  Measurement is free and ownership only
bars AUTHORING, so this derives everywhere and TAGS ownership instead.  Run-26
ownership is taken from the live `gdlmem claims` scopes, not from run-25's list.
"""
import json
import os
import sys
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "tools", "gdl"))
sys.path.insert(0, os.path.join(HERE, "..", "tools", "gdl", "composed_census"))
sys.path.insert(0, HERE)
import cn_census as census  # noqa: E402
import webfrank as wf  # noqa: E402
import ch_derive  # noqa: E402

# Run-26 claim scopes (gdlmem claims, 2026-09-01).  Authoring only; not a
# measurement filter.
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


def main():
    shipped = set(json.load(open(os.path.join(HERE, "ch_shipped.json"))))
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
    json.dump(rows, open(os.path.join(HERE, "ch_sweep26.json"), "w"), indent=1)
    print(f"\nwrote {os.path.join(HERE, 'ch_sweep26.json')}")


if __name__ == "__main__":
    main()
