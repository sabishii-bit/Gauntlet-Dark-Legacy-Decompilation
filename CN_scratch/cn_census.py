"""CN lane: re-run HV's single-region permute+recolor census filter.

claim.HV_single-region-permute-plus-recolor-census.20260901.v1 states the filter:
  (1) equal function size (count deltas are ineligible; screen FIRST);
  (2) cluster the differing words, keep functions where AT MOST ONE cluster
      holds a word that is not a pure register-field difference;
  (3) that cluster, widened one slot each side, must be control-free;
  (4) derive an order matching atoms on register-erased form AND relocation
      identity, accept only orders passing check_permutation_dependences
      IN OUR COLOURING;
  (5) prove the composition to byte equality before writing a rule.

This script implements 1-3 and reports the survivors.  It scans OUR RAW output
(.postprocess/body when the TU has a webfrank unit, else the plain object) so
that postprocessed bytes cannot flatter the count -- which also means every
already-shipped composed rule is still visible as a residual here, giving the
scan built-in KNOWN-POSITIVE CANARIES.  A scanner without a canary lies.
"""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "tools", "gdl"))
import webfrank as wf  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OBJ = os.path.join(ROOT, "build", "GUNE5D", "obj")
SRC = os.path.join(ROOT, "build", "GUNE5D", "src")

# Functions whose residual is known to be a closable single-region composition.
# They are all currently SHIPPED as webfrank rules, so they only appear when the
# scan reads raw bodies -- exactly the property being tested.
CANARIES = {
    "dcsSampleAllocUpload", "MemCardCreateGaunt", "do_camera",
    "camera_init_for_gamemode", "memCardErrorPrompt",
}


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
    """Text-section symbols with a nonzero, word-aligned size."""
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


def scan_function(ours, tgt):
    """Return (verdict, detail) for HV screens 2-3."""
    diffs = [o for o in range(0, len(ours), 4)
             if wf._u32(ours, o) != wf._u32(tgt, o)]
    if not diffs:
        return None
    clusters, cur = [], [diffs[0]]
    for d in diffs[1:]:
        if d - cur[-1] <= 8:
            cur.append(d)
        else:
            clusters.append(cur)
            cur = [d]
    clusters.append(cur)

    impure = []
    for c in clusters:
        n = 0
        for off in c:
            ow, tw = wf._u32(ours, off), wf._u32(tgt, off)
            try:
                if (ow ^ tw) & ~wf.register_slot_mask(ow):
                    n += 1
            except ValueError:
                n += 1
        if n:
            impure.append((c, n))
    if len(impure) != 1:
        return None                      # screen 2
    c, _n = impure[0]
    lo = max(0, c[0] - 4)
    hi = min(len(ours), c[-1] + 8)
    for off in range(lo, hi, 4):
        if wf._is_control_instruction(wf._u32(ours, off)):
            return None                  # screen 3
        if wf._is_control_instruction(wf._u32(tgt, off)):
            return None
    return (len(diffs), len(clusters), lo, hi)


def main():
    hits = []
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
        tmap = {}
        for s in functions(tdata, tsec):
            tmap[s.name] = s
        for s in functions(odata, osec):
            t = tmap.get(s.name)
            if t is None or t.size != s.size:     # screen 1
                continue
            try:
                ot = osec[s.section_index]
                tt = tsec[t.section_index]
                ours = bytes(odata[ot.offset + s.value:
                                   ot.offset + s.value + s.size])
                tgt = bytes(tdata[tt.offset + t.value:
                                  tt.offset + t.value + t.size])
            except Exception:
                continue
            r = scan_function(ours, tgt)
            if r:
                hits.append((unit, s.name, s.size // 4, is_raw) + r)

    print(f"SCREENS 1-3 SURVIVORS: {len(hits)}\n")
    found = {h[1] for h in hits}
    print("CANARY CHECK (all are shipped rules; must appear from raw bodies):")
    for c in sorted(CANARIES):
        print(f"  {'FOUND  ' if c in found else 'MISSING'} {c}")
    ok = CANARIES <= found
    print(f"  => scanner is {'TRUSTWORTHY' if ok else 'LYING -- do not use'}\n")

    print(f"{'unit':34} {'function':38} {'ins':>5} {'diff':>5} {'cl':>3}  window")
    for unit, name, ins, is_raw, nd, ncl, lo, hi in sorted(
            hits, key=lambda h: (h[4], h[2])):
        tag = "" if is_raw else "  (plain obj)"
        star = " *CANARY" if name in CANARIES else ""
        print(f"{unit:34} {name:38} {ins:5} {nd:5} {ncl:3}  "
              f"[0x{lo:x},0x{hi:x}){tag}{star}")


if __name__ == "__main__":
    main()
