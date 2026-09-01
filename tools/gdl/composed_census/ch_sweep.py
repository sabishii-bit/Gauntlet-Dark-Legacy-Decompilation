"""CH lane: re-sweep the whole screens-1-3 population with the corrected
relocation binding and the derived (non-enumerative) order.

Two independent fixes over cn_search.py, either of which alone changes the
verdict on real functions:
  1. the order is DERIVED as a sparse bipartite matching, not enumerated, so
     there is no MAX_ATOMS = 8 bound and 20-atom windows are routine;
  2. relocation identity is keyed by the OWNING INSTRUCTION, because SDA21
     relocations are recorded at instruction+2 in our objects and at
     instruction+0 in the extracted targets (measured image-wide).

TUs owned by other lanes this run are excluded, not merely deprioritised.
"""
import os
import sys
import traceback

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))  # tools/gdl (fixed after promotion out of CN_scratch)
import webfrank as wf  # noqa: E402
sys.path.insert(0, os.path.dirname(__file__))
import cn_census as census  # noqa: E402
import ch_derive  # noqa: E402

OWNED = (
    "game/pb/pb_window", "game/movie/movieplayer", "game/world/items",
    "game/world/effects", "game/shop/shop", "game/game/controls",
    "game/audio/dcsdrv", "game/world/camera", "game/world/newcam",
)


def survivors():
    out = []
    for unit in census.units():
        op, is_raw = census.our_path(unit)
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
                ours = bytes(odata[ot.offset + s.value:
                                   ot.offset + s.value + s.size])
                tgt = bytes(tdata[tt.offset + t.value:
                                  tt.offset + t.value + t.size])
            except Exception:
                continue
            r = census.scan_function(ours, tgt)
            if r:
                out.append((unit, s.name, r[2], r[3]))
    return out


def main():
    closed, refused = [], []
    for unit, fn, lo, hi in survivors():
        if unit in OWNED:
            print(f"== {unit}::{fn}  SKIPPED (TU owned by another lane)")
            continue
        print(f"== {unit}::{fn}")
        try:
            r = ch_derive.run(unit, fn, lo, hi)
        except Exception:
            print("   ERROR\n" + traceback.format_exc(limit=2))
            continue
        (closed if r else refused).append(f"{unit}::{fn}")
    print(f"\nCLOSED ({len(closed)}):")
    for c in closed:
        print("  " + c)
    print(f"\nNOT CLOSED ({len(refused)}):")
    for c in refused:
        print("  " + c)


if __name__ == "__main__":
    main()
