"""WF lane: image-wide census of the equivalent_copy_form INVERSE direction.

DC's law (claim.law.DC_copy-form-class-is-directional-...) says the shipped
class requires the TARGET word to be a register copy, so the inverse
(OURS is the copy, TARGET is the `li`) has no proof mode.  LZ's
payoff-inversion law says: census the population BEFORE building the rule,
because the last lane nearly shipped a rule with an empty payoff set.

Classification per differing word (equal-size functions only):
  regfield  - differs only in register operand fields (copy_register_fields)
  fwd       - ours li, target copy, SAME dest      (already served)
  fwd_rc    - ours li, target copy, dest DIFFERS   (form+recolor, v2)
  inv       - ours copy, target li, SAME dest      (THE INVERSE, unserved)
  inv_rc    - ours copy, target li, dest DIFFERS   (form+recolor, v2)
  other     - structural / immediate / everything else

Canary discipline (cn_census.py): the scan reads RAW .postprocess/body
output where present, so already-shipped rules must still show up.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
import webfrank as wf  # noqa: E402

OBJ = os.path.join(ROOT, "build", "GUNE5D", "obj")
SRC = os.path.join(ROOT, "build", "GUNE5D", "src")

# Shipped copy-form / composed rules: must remain visible from raw bodies.
CANARIES = {"dcsSampleAllocUpload", "MemCardCreateGaunt", "do_camera",
            "camera_init_for_gamemode", "memCardErrorPrompt"}


def units():
    out = []
    for dirpath, _dirs, files in os.walk(OBJ):
        for f in files:
            if f.endswith(".o"):
                p = os.path.join(dirpath, f)
                out.append(os.path.relpath(p, OBJ)[:-2].replace("\\", "/"))
    return sorted(out)


def our_path(unit):
    if "/" not in unit:
        plain = os.path.join(SRC, unit + ".o")
        return (plain, False) if os.path.exists(plain) else (None, False)
    d, base = unit.rsplit("/", 1)
    body = os.path.join(SRC, d, ".postprocess", "body", base + ".o")
    if os.path.exists(body):
        return body, True
    plain = os.path.join(SRC, unit + ".o")
    return (plain, False) if os.path.exists(plain) else (None, False)


def functions(data, sections):
    out = []
    for sym in wf._symbols(data, sections):
        if not sym.size or sym.size % 4:
            continue
        if sym.section_index >= len(sections):
            continue
        if not sections[sym.section_index].name.startswith(".text"):
            continue
        out.append(sym)
    return out


def classify(ours_word, tgt_word):
    try:
        pure = not ((ours_word ^ tgt_word) & ~wf.register_slot_mask(ours_word))
    except ValueError:
        pure = False
    if pure:
        return "regfield"
    a = wf.decode_copy_form(ours_word)
    b = wf.decode_copy_form(tgt_word)
    if a is None or b is None:
        return "other"
    if a[0] == "li" and b[0] == "copy":
        return "fwd" if a[1] == b[1] else "fwd_rc"
    if a[0] == "copy" and b[0] == "li":
        return "inv" if a[1] == b[1] else "inv_rc"
    return "other"


def main():
    rows = []
    seen_funcs = set()
    for unit in units():
        op, is_raw = our_path(unit)
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
                ot, tt = osec[s.section_index], tsec[t.section_index]
                ours = bytes(odata[ot.offset + s.value:
                                   ot.offset + s.value + s.size])
                tgt = bytes(tdata[tt.offset + t.value:
                                  tt.offset + t.value + t.size])
            except Exception:
                continue
            seen_funcs.add(s.name)
            counts = {}
            sites = {"inv": [], "inv_rc": [], "fwd": [], "fwd_rc": []}
            for off in range(0, len(ours), 4):
                ow, tw = wf._u32(ours, off), wf._u32(tgt, off)
                if ow == tw:
                    continue
                k = classify(ow, tw)
                counts[k] = counts.get(k, 0) + 1
                if k in sites:
                    sites[k].append((off, ow, tw))
            if not counts:
                continue
            rows.append((unit, s.name, s.size // 4, is_raw, counts, sites))

    print("CANARY CHECK (shipped rules must still be visible from raw bodies):")
    for c in sorted(CANARIES):
        print(f"  {'FOUND  ' if c in seen_funcs else 'MISSING'} {c}")
    print(f"  scanned {len(seen_funcs)} equal-size paired functions, "
          f"{len(rows)} with diffs\n")

    tot = {}
    for _u, _n, _i, _r, counts, _s in rows:
        for k, v in counts.items():
            tot[k] = tot.get(k, 0) + v
    print("IMAGE-WIDE DIFFERING-WORD CLASS TOTALS:")
    for k in sorted(tot, key=lambda x: -tot[x]):
        print(f"  {k:10} {tot[k]}")
    print()

    inv_rows = [r for r in rows if r[4].get("inv") or r[4].get("inv_rc")]
    print(f"FUNCTIONS CARRYING AN INVERSE SITE: {len(inv_rows)}\n")
    hdr = (f"{'unit':32} {'function':34} {'ins':>5} "
           f"{'inv':>4} {'invrc':>6} {'reg':>5} {'other':>6} {'fwd':>4} "
           f"{'fwdrc':>6}")
    print(hdr)
    for unit, name, ins, is_raw, c, _s in sorted(
            inv_rows, key=lambda r: (r[4].get("other", 0), r[2])):
        print(f"{unit:32} {name:34} {ins:5} "
              f"{c.get('inv', 0):4} {c.get('inv_rc', 0):6} "
              f"{c.get('regfield', 0):5} {c.get('other', 0):6} "
              f"{c.get('fwd', 0):4} {c.get('fwd_rc', 0):6}"
              f"{'' if is_raw else '  (plain obj)'}")

    print("\nINVERSE SITE DETAIL (functions with other == 0):")
    for unit, name, _ins, _r, c, sites in inv_rows:
        if c.get("other", 0):
            continue
        print(f"  {unit}::{name}")
        for k in ("inv", "inv_rc"):
            for off, ow, tw in sites[k]:
                print(f"    {k:6} +0x{off:x}  ours 0x{ow:08x} "
                      f"{wf.decode_copy_form(ow)}  target 0x{tw:08x} "
                      f"{wf.decode_copy_form(tw)}")


if __name__ == "__main__":
    main()
