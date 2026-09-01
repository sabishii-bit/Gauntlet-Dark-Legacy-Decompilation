"""Is the relocation-offset mismatch a CODE difference or an ENCODING one?

Hypothesis from CTriListCollide's aligned view: our object records
R_PPC_EMB_SDA21 (type 109) relocations at instruction+2, while the extracted
target object records them at instruction+0, whereas @ha/@lo (types 4/6) sit
at instruction+2 in BOTH.  If true, any relocation-binding filter comparing
raw r_offset produces FALSE NEGATIVES on every window holding an SDA21
relocation -- which would make three "no candidate" verdicts artifacts of the
filter, not properties of the functions.

Measure it image-wide: for every function present in both our raw output and
the target, bucket relocations by type and report the offset parity (offset
mod 4) on each side.
"""
import collections
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))  # tools/gdl (fixed after promotion out of CN_scratch)
import webfrank as wf  # noqa: E402
sys.path.insert(0, os.path.dirname(__file__))
from cn_census import units, our_path, functions, OBJ  # noqa: E402


def main():
    ours_par = collections.Counter()
    tgt_par = collections.Counter()
    for unit in units():
        op, _raw = our_path(unit)
        if not op:
            continue
        try:
            odata = bytearray(open(op, "rb").read())
            tdata = bytearray(open(os.path.join(OBJ, unit + ".o"), "rb").read())
            osec, tsec = wf._sections(odata), wf._sections(tdata)
        except Exception:
            continue
        tmap = {s.name: s for s in functions(tdata, tsec)}
        for s in functions(odata, osec):
            t = tmap.get(s.name)
            if t is None or t.size != s.size:
                continue
            try:
                orel = wf._function_text_relocations(
                    odata, osec, s.section_index, s.value, s.value + s.size)
                trel = wf._function_text_relocations(
                    tdata, tsec, t.section_index, t.value, t.value + t.size)
            except Exception:
                continue
            for off, (ty, _sym) in orel.items():
                ours_par[(ty, (off - s.value) % 4)] += 1
            for off, (ty, _sym) in trel.items():
                tgt_par[(ty, (off - t.value) % 4)] += 1

    types = sorted({t for t, _p in ours_par} | {t for t, _p in tgt_par})
    print(f"{'type':>6}  {'ours @+0':>10} {'ours @+2':>10} "
          f"{'tgt @+0':>10} {'tgt @+2':>10}")
    for ty in types:
        print(f"{ty:6}  {ours_par[(ty,0)]:10} {ours_par[(ty,2)]:10} "
              f"{tgt_par[(ty,0)]:10} {tgt_par[(ty,2)]:10}")


if __name__ == "__main__":
    main()
