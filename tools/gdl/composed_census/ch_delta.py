"""CH lane run-26: explain the 83-vs-91 delta against WF's run-25 census.

My re-derivation reproduces regfield (7859), fwd (5) and inv (2) EXACTLY but
reads inv_rc 42 / fwd_rc 41 against WF's 47 / 44.  Since three of six buckets
agree to the word, the disagreement is in the _rc classification specifically.
Two candidate causes, both measurable:

  (A) the r0-source refusal.  decode_copy_form returns ("copy", rA, 0) for
      `or rA,r0,r0`.  WF's law says an rS of r0 is refused outright, but if
      that refusal lives only in the RULE and not in WF's CENSUS classifier,
      WF would count such pairs as _rc where I file them `other`.
  (B) tree drift: 2839 equal-size pairs now against WF's 2835.

This script counts both directly.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "tools", "gdl"))
sys.path.insert(0, os.path.join(HERE, "..", "tools", "gdl", "composed_census"))
import webfrank as wf  # noqa: E402
import ch_census26 as c26  # noqa: E402


def main():
    r0_fwd, r0_inv, both_copy, both_li = [], [], 0, 0
    for unit, name, insns, is_raw, buckets in c26.walk():
        for off in buckets.get("other", []):
            pass  # offsets only; re-decode below
    # Re-walk with full decode so we can bucket the `other` refusals by cause.
    for unit, name, insns, is_raw, buckets in c26.walk():
        if "other" not in buckets:
            continue
        op, _ = c26.census.our_path(unit)
        odata = bytearray(open(op, "rb").read())
        tdata = bytearray(open(os.path.join(c26.census.OBJ, unit + ".o"), "rb").read())
        osec, tsec = wf._sections(odata), wf._sections(tdata)
        tmap = {s.name: s for s in c26.census.functions(tdata, tsec)}
        s = wf._find_symbol(odata, osec, name)
        t = tmap[name]
        ot, tt = osec[s.section_index], tsec[t.section_index]
        ours = bytes(odata[ot.offset + s.value:ot.offset + s.value + s.size])
        tgt = bytes(tdata[tt.offset + t.value:tt.offset + t.value + t.size])
        for h in buckets["other"]:
            off = int(h, 16) if isinstance(h, str) else h
            a, b = wf._u32(ours, off), wf._u32(tgt, off)
            oc, tc = wf.decode_copy_form(a), wf.decode_copy_form(b)
            if oc is None or tc is None:
                continue
            if oc[0] == "li" and tc[0] == "copy" and tc[2] == 0:
                r0_fwd.append((unit, name, hex(off)))
            elif oc[0] == "copy" and tc[0] == "li" and oc[2] == 0:
                r0_inv.append((unit, name, hex(off)))
            elif oc[0] == "copy" and tc[0] == "copy":
                both_copy += 1
            elif oc[0] == "li" and tc[0] == "li":
                both_li += 1

    print("CAUSE (A): pairs filed `other` SOLELY by the r0-source refusal")
    print(f"  fwd arrow, target `mr rD,r0`   : {len(r0_fwd)}")
    for r in r0_fwd:
        print(f"      {r[0]}::{r[1]} {r[2]}")
    print(f"  inv arrow, ours `mr rD,r0`     : {len(r0_inv)}")
    for r in r0_inv:
        print(f"      {r[0]}::{r[1]} {r[2]}")
    print(f"\n  if WF's classifier lacked the r0 refusal it would read "
          f"fwd_rc +{len(r0_fwd)}, inv_rc +{len(r0_inv)}")
    print(f"\nfor reference, other decodable-but-unserved shapes in `other`:")
    print(f"  copy->copy (both sides a copy, immediate/reg mismatch): {both_copy}")
    print(f"  li->li     (an IMMEDIATE difference, never closable)  : {both_li}")


if __name__ == "__main__":
    main()
