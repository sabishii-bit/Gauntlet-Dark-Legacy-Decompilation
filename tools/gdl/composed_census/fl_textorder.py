"""FL lane: text-ORDER check (flip-audit fingerprint #4).

Name-paired scores (fndiff, objdiff, claimcheck, datadiff) compare each
function against its SAME-NAMED counterpart, so they are structurally blind
to the ORDER functions occupy in .text. A TU whose functions are each
byte-identical but emitted in a different source order still links to the
wrong bytes, and every score in the project reads 100%.

This is the hand check the MF recipe describes -- two `objdump -t` dumps
compared by hand -- mechanised. Compares the .text FUNC symbol sequence of
the dtk-extracted TARGET object against our linked object.

Usage (from the repo root):
    python tools/gdl/composed_census/fl_textorder.py --calibrate \
        game/mb/mb_camera game/pb/dbgtext ...

Exit 0 = order and offsets identical; 1 = order identical but a body has a
different SIZE (the flip still shifts); 2 = order or roster differs.

CALIBRATION IS PART OF THE TOOL. A symbol-table reader that returns nothing
prints a clean pass, which is the exact false-all-clear this check exists to
prevent (the first draft of this script did precisely that: it reused
fndiff.object_sections, a DATA-symbol reader, and reported "TEXT ORDER:
IDENTICAL" for five TUs on 0 functions). So the parse REFUSES an empty
roster, and --calibrate proves both sides on a known-good and a known-bad
pair before any verdict is believed.
"""
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]   # tools/gdl/composed_census/..3
OBJDUMP = ROOT / "build" / "binutils" / "powerpc-eabi-objdump.exe"

# objdump -t row, verbatim:
#   "00000000 g     F .text\t000001a0 MBWorldToScreen3D"
# The FLAGS field is seven fixed columns that CONTAIN SPACES ("g     F"), so a
# whitespace-split regex silently matches nothing -- that is exactly how the
# first draft of this script produced five vacuous passes. Split on the TAB
# that objdump puts between the section and the size, then slice by column.
ROW = re.compile(r"^([0-9a-f]{8}) (.{7}) (\S+)$")


def unit_key(unit):
    text = str(unit).replace("\\", "/").strip().strip("/")
    if text.startswith("src/"):
        text = text[len("src/"):]
    return re.sub(r"\.(c|cpp)$", "", text)


def text_order(objpath):
    """[(offset, size, name)] for .text FUNCTION symbols, ascending."""
    out = subprocess.run([str(OBJDUMP), "-t", str(objpath)],
                         capture_output=True, text=True, check=True).stdout
    rows = []
    for line in out.splitlines():
        if "\t" not in line:
            continue
        left, right = line.split("\t", 1)
        m = ROW.match(left)
        if not m:
            continue
        value, flags, section = m.groups()
        if section != ".text":
            continue
        if "F" not in flags:          # F = function; 'd'/'O'/'l' are not
            continue
        parts = right.split(None, 1)
        if len(parts) != 2:
            continue
        size, name = parts
        name = name.replace(".hidden ", "").strip()
        rows.append((int(value, 16), int(size, 16), name))
    rows.sort()
    return rows


def check(unit, quiet=False):
    key = unit_key(unit)
    tgt = ROOT / "build" / "GUNE5D" / "obj" / (key + ".o")
    ours = ROOT / "build" / "GUNE5D" / "src" / (key + ".o")
    if not quiet:
        print("=== %s ===" % key)
    if not tgt.exists() or not ours.exists():
        print("  MISSING OBJECT (target exists=%s, ours exists=%s)"
              % (tgt.exists(), ours.exists()))
        return 2
    t, o = text_order(tgt), text_order(ours)
    # The anti-vacuous-pass guard: an empty roster is a PARSE FAILURE, never
    # a pass. Both sides must be non-empty before any verdict is printed.
    if not t or not o:
        print("  PARSE FAILED -- target %d fn(s), ours %d fn(s). An empty"
              " roster is NOT a pass; the reader is broken." % (len(t), len(o)))
        return 2
    tn = [r[2] for r in t]
    on = [r[2] for r in o]
    if not quiet:
        print("  target %d fn(s), ours %d fn(s)" % (len(tn), len(on)))
    rc = 0
    if set(tn) != set(on):
        print("  ROSTER DIFFERS  target-only=%s  ours-only=%s"
              % ([n for n in tn if n not in set(on)],
                 [n for n in on if n not in set(tn)]))
        rc = 2
    if tn == on:
        if not quiet:
            print("  TEXT ORDER: IDENTICAL")
        bad = [(a, b) for a, b in zip(t, o) if a[0] != b[0] or a[1] != b[1]]
        if not bad:
            if not quiet:
                print("  TEXT OFFSETS+SIZES: IDENTICAL")
        else:
            print("  TEXT OFFSETS/SIZES DIFFER (order right, a body's SIZE"
                  " moved -- the flip still shifts every later function):")
            for a, b in bad:
                print("    %-28s target @0x%-5x size 0x%-4x | ours @0x%-5x"
                      " size 0x%-4x  (delta %+d)"
                      % (a[2], a[0], a[1], b[0], b[1], b[1] - a[1]))
            rc = max(rc, 1)
    else:
        print("  TEXT ORDER DIFFERS -- invisible to every name-paired score:")
        for i in range(max(len(tn), len(on))):
            a = tn[i] if i < len(tn) else "-"
            b = on[i] if i < len(on) else "-"
            print("    %2d  %-30s %-30s %s"
                  % (i, a, b, "  " if a == b else "<< MISMATCH"))
        rc = 2
    return rc


def calibrate():
    """Two-sided proof the reader works before any verdict is trusted.

    POSITIVE side: a fully Matching TU must read as identical.
    NEGATIVE side: a synthetic reversal of one roster must be REJECTED, and
    a TU with a known instruction-count deficit must show a SIZE delta.
    """
    print("--- CALIBRATION (both sides) ---")
    ok = True

    # POSITIVE: a linked, fully-Matching TU.
    pos = "game/mb/mb_poly"
    tgt = ROOT / "build/GUNE5D/obj" / (pos + ".o")
    if tgt.exists():
        t = text_order(tgt)
        o = text_order(ROOT / "build/GUNE5D/src" / (pos + ".o"))
        good = bool(t) and [r[2] for r in t] == [r[2] for r in o]
        print("  POSITIVE %-20s %d fn(s) both sides, order identical = %s"
              % (pos, len(t), good))
        ok = ok and good
        # NEGATIVE 1: reversing one side must be detected.
        rev = list(reversed([r[2] for r in o]))
        detected = rev != [r[2] for r in t]
        print("  NEGATIVE reversed roster detected as a mismatch = %s"
              % detected)
        ok = ok and detected
    else:
        print("  POSITIVE skipped: %s target object absent" % pos)

    # NEGATIVE 2: pb_frame is 4 bytes long in .text (claimcheck says
    # obj 0x1ee4 > claim 0x1ee0) -- a real SIZE delta the check must SEE.
    t = text_order(ROOT / "build/GUNE5D/obj/game/pb/pb_frame.o")
    o = text_order(ROOT / "build/GUNE5D/src/game/pb/pb_frame.o")
    tsz, osz = sum(r[1] for r in t), sum(r[1] for r in o)
    print("  NEGATIVE pb_frame .text total target 0x%x vs ours 0x%x -> size"
          " delta visible = %s" % (tsz, osz, tsz != osz))
    ok = ok and (tsz != osz)
    print("  CALIBRATION: %s" % ("PASS" if ok else "FAIL"))
    return 0 if ok else 2


if __name__ == "__main__":
    args = [a for a in sys.argv[1:] if a != "--calibrate"]
    rc = 0
    if "--calibrate" in sys.argv[1:]:
        rc = calibrate()
    for u in args:
        rc = max(rc, check(u))
    sys.exit(rc)
