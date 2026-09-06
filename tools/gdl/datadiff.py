#!/usr/bin/env python3
"""Pre-link data-section byte comparator: our compiled object vs the DOL.

For each data-class section (.rodata .data .sdata .sdata2) claimed by a unit
in splits.txt, compare our object's emitted bytes against the original DOL
bytes at the claimed addresses. Words that carry a relocation in our object
are skipped (their final value exists only after the link) but their count
and placement are reported, which still exposes jumptable ORDER mismatches.

This catches what fndiff structurally normalizes away:
  - wrong constant VALUES (mathfunc's 1e-5 vs 1e-14 epsilon, atan tables)
  - wrong pool/table EMISSION ORDER (sincos duplicate 0.5f pairs)
  - initializer typos that only sha1 would catch after a full link

It also runs an ADVISORY dead-strip screen (see --deadstrip below).

Usage (from repo root):
  python tools/gdl/datadiff.py game/mathfunc
  python tools/gdl/datadiff.py MSL/sincos MSL/trigf_data
  python tools/gdl/datadiff.py --matching        # all Matching units
  python tools/gdl/datadiff.py --deadstrip game/ui/message   # screen only
  python tools/gdl/datadiff.py --no-deadstrip game/mathfunc  # byte diff only

Exit 1 if any byte mismatch found. Dead-strip warnings NEVER set exit 1:
they are advisory, because a zero-reference static can be legitimate
(claimed data a later flip will wire up, or a symbol another instrument
already accounts for).

`--sections --out PATH` emits schema-version 1 with PASS/FAIL/UNRESOLVED:
exit 0 means the stated section obligations passed, exit 1 means a measured
failed obligation, exit 2 means unreadable input or candidates still needing
ownership/relocation/link review. It does not certify whole-object equality.
Dotless extab/extabindex are read from objects and compared by function
identity through exception_metadata, not by their emitted record order.

ZERO-FILLED CLAIM SLACK IS ADVISORY, NOT A FLIP BLOCKER
-------------------------------------------------------
When every compared byte is EQUAL and the only residue is trailing claim
slack that is ZERO in the DOL, the unit links green: the linker regenerates
that padding from the next section's alignment. Scoring it as a MISMATCH
made `finish_tu.py` refuse to flip TUs of a class the project has already
shipped ~44 members of (measured 2026-09-03 at c621fcbac:
`datadiff.py --matching --no-deadstrip` = 53 zero-slack rows over 44
ALREADY-Matching, already-green units). Such rows now print `DATA-DEBT`,
are summarized at the end, and do NOT set the exit code. Pass
`--strict-slack` to restore the old refusing behaviour.

Each DATA-DEBT row is classified by whether the parent fix procedure is
even REACHABLE (claim.law.AF_dtk-rejects-an-unaligned-auto-split-start-so-
some-claim-slack-is-structural.20260903.v1): shrinking the claim to the
object's true extent makes dtk auto-generate a split at that address, which
it rejects unless the address is 4-byte aligned OR another unit already
claims the range. Measured over the 53 rows: 21 `shrinkable`, 32
`structural` (the shrink cannot be applied at all).

NONZERO slack still fails: real bytes are missing from our object.

`--sections` SPEAKS THE SAME VERDICT (run-49 item 3). It used to call every
per-section SIZE mismatch a FLIP BLOCKER while the byte mode called the SAME
bytes advisory: measured at 96d689120 on dolphin/si/SIBios, an
`Object(Matching, ...)` unit that is linked and green,

    plain      .data: DATA-DEBT 0xCD bytes compared, 0x3 claim slack
               (zero-filled; structural - advisory, not a flip blocker)  exit 0
    --sections .data: SIZE target 0xD0 vs ours 0xCD  <- FLIP BLOCKER      exit 1

Three bytes, two labels, opposite exit codes. `size_gap_class` now decides,
and `--strict-slack` restores the refusing behaviour for both modes at once.

IMPORTABLE CORE: section_verdict, shrink_reachable, size_gap_class -- pure
over bytes already read, no subprocess and no build.

DEAD-STRIP SCREEN (claim.law.base-cast-reconstruction-deadstrips-sibling-statics)
--------------------------------------------------------------------------------
mwld dead-strips local data that nothing relocates against. A one-symbol
base-cast reconstruction -- several `static` arrays tiling one target blob,
all reached through ONE of the symbols by casting -- therefore compiles,
links, and passes every OBJECT-level instrument (fndiff, claimcheck, and
datadiff's own byte comparison) while its sibling statics silently vanish
from the linked image, taking any string literal only they referenced with
them. That signature cost a full DOL-forensics session on game/ui/message.c.

This screen flags the exact precondition: a LOCAL (static) data symbol
defined in this object that NO relocation in the same object points at.
Reference resolution is by ADDRESS, not by name, so a static reached
through a section alias plus addend (`...rodata.0+0x28`) or through a
neighbouring symbol's addend counts as referenced -- name-only matching
false-positives on exactly the string literals this law cares about.
"""

import json
import os
import re
import struct
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parent.parent.parent
OBJDUMP = REPO / "build" / "binutils" / (
    "powerpc-eabi-objdump.exe" if os.name == "nt" else "powerpc-eabi-objdump")
SPLITS = REPO / "config" / VERSION / "splits.txt"
DOL = REPO / "orig" / VERSION / "sys" / "main.dol"

DATA_SECTIONS = (".rodata", ".data", ".sdata", ".sdata2")


class MeasurementUnavailable(RuntimeError):
    """A missing/failed measurement, never an empty successful comparison."""


def dump_object(obj, *flags):
    obj = Path(obj)
    if not obj.is_file():
        raise MeasurementUnavailable(f"missing object: {obj}")
    result = subprocess.run([str(OBJDUMP), *flags, str(obj)],
                            capture_output=True, text=True)
    if result.returncode:
        raise MeasurementUnavailable(
            f"objdump exited {result.returncode}: {result.stderr.strip()}")
    return result.stdout


def _unit_key(unit):
    """The canonical unit spelling, kept identical to `fndiff.unit_key`.

    Run-56 item 3. AGENTS.md names `fndiff.unit_key` as THE normalizer and
    says the core tools accept `game/x/y`, `game/x/y.c`, `src/game/x/y.c` and
    the backslash forms. datadiff did not normalize, so the documented
    `src/...` spelling of a REAL unit landed on the `no splits entry` branch
    and exited 0 — indistinguishable from a mistyped unit. Imported when
    fndiff is importable (it is IMPORTABLE CORE: no side effects at import),
    duplicated here only as a fallback so datadiff never fails to run over a
    path problem.
    """
    try:
        from fndiff import unit_key
    except ImportError:
        text = str(unit).replace("\\", "/").strip().strip("/")
        if text.startswith("src/"):
            text = text[len("src/"):]
        return re.sub(r"\.(c|cpp)$", "", text)
    return unit_key(unit)


def dol_read(va, size):
    data = DOL.read_bytes()
    text_off = struct.unpack(">7I", data[0x00:0x1C])
    data_off = struct.unpack(">11I", data[0x1C:0x48])
    text_addr = struct.unpack(">7I", data[0x48:0x64])
    data_addr = struct.unpack(">11I", data[0x64:0x90])
    text_size = struct.unpack(">7I", data[0x90:0xAC])
    data_size = struct.unpack(">11I", data[0xAC:0xD8])
    segments = (list(zip(text_off, text_addr, text_size))
                + list(zip(data_off, data_addr, data_size)))
    for off, addr, sz in segments:
        if addr and addr <= va and va + size <= addr + sz:
            fo = off + (va - addr)
            return data[fo:fo + size]
    return None


def parse_splits():
    units = {}
    cur = None
    for line in SPLITS.read_text(encoding="utf-8").splitlines():
        m = re.match(r"^(\S.+):$", line)
        if m:
            cur = m.group(1)
            units[cur] = {}
            continue
        m = re.match(r"^\t(\S+)\s+start:0x([0-9A-Fa-f]+) end:0x([0-9A-Fa-f]+)", line)
        if m and cur:
            units[cur][m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
    return units


def obj_sections(obj):
    """name -> bytes, from objdump -s."""
    out = dump_object(obj, "-s")
    secs = {}
    name = None
    for line in out.splitlines():
        m = re.match(r"^Contents of section (\S+):", line)
        if m:
            name = m.group(1)
            secs[name] = bytearray()
            continue
        if name:
            m = re.match(r"^ ([0-9a-f]+) ((?:[0-9a-f ]{8,9}){1,4})", line)
            if m:
                hexpart = m.group(2).replace(" ", "")
                secs[name] += bytes.fromhex(hexpart)
    return secs


def obj_relocs(obj):
    """section -> set of relocated word offsets."""
    out = dump_object(obj, "-r")
    relocs = {}
    sec = None
    for line in out.splitlines():
        m = re.match(r"^RELOCATION RECORDS FOR \[(\S+)\]", line)
        if m:
            sec = m.group(1)
            relocs[sec] = set()
            continue
        m = re.match(r"^([0-9a-f]+)\s+R_PPC_", line)
        if m and sec:
            relocs[sec].add(int(m.group(1), 16) & ~3)
    return relocs


# Sections whose local symbols mwld can dead-strip. BSS-class included:
# an unreferenced static array there vanishes just as silently.
STRIPPABLE_SECTIONS = (".rodata", ".data", ".sdata", ".sdata2",
                       ".bss", ".sbss", ".sbss2")

# objdump -t: "00000000 l     O .data\t00000040 lbl_80124E58"
SYM_RE = re.compile(
    r"^([0-9a-f]{8})\s(.{7})\s+(\S+)\s+([0-9a-f]{8})\s+(\S.*?)\s*$")

# objdump -r: "000001de R_PPC_ADDR16_HA   lbl_80124E58[+0x00000004]"
REL_RE = re.compile(
    r"^[0-9a-f]+\s+R_PPC_\S+\s+(\S+?)(?:\+0x([0-9a-f]+))?\s*$")


def obj_symbols(obj):
    """Parse objdump -t into symbol dicts.

    flags column is 7 chars: [lg] [w] [C] [W] [Ii] [dD] [FfO]
      flags[0] == 'l'  -> local (a C `static`, our dead-strip candidate)
      'O' in flags     -> an object (data), as opposed to a function/section
      'd' in flags     -> a section symbol
    """
    out = dump_object(obj, "-t")
    syms = []
    for line in out.splitlines():
        m = SYM_RE.match(line.replace("\t", " "))
        if not m:
            continue
        value, flags, sec, size, name = m.groups()
        syms.append({
            "value": int(value, 16),
            "size": int(size, 16),
            "section": sec,
            "name": name,
            "local": flags[0] == "l",
            "object": "O" in flags,
            "section_sym": "d" in flags,
        })
    return syms


def obj_reloc_refs(obj):
    """Every relocation target in the object as (symbol_name, addend)."""
    out = dump_object(obj, "-r")
    refs = []
    for line in out.splitlines():
        m = REL_RE.match(line)
        if m:
            refs.append((m.group(1), int(m.group(2), 16) if m.group(2) else 0))
    return refs


def deadstrip_check(unit, obj, quiet_ok=True):
    """Advisory: local data symbols with NO incoming relocation.

    Returns the list of unreferenced symbol dicts (never affects exit code).
    """
    syms = obj_symbols(obj)
    refs = obj_reloc_refs(obj)

    by_name = {}
    for s in syms:
        by_name.setdefault(s["name"], s)

    # A "section anchor" is a section symbol (.data) or a zero-size local
    # alias for one (...data.0). SOUNDNESS LIMIT: when a relocation names an
    # anchor, the displacement that selects which datum it reaches lives in
    # the ADDR16_HA/LO INSTRUCTION IMMEDIATES, not in the relocation addend,
    # so nothing in the relocation table says which symbols of that section
    # are live. Any symbol-level liveness claim there would be a guess, so we
    # declare the whole section opaque and report nothing in it. That costs
    # false negatives (a TU addressing its data off one section base is not
    # screenable) but keeps every warning we DO emit sound -- the right
    # trade for an advisory that must not cry wolf.
    def is_anchor(sym):
        return sym["section_sym"] or (
            sym["local"] and not sym["object"] and sym["size"] == 0)

    opaque = set()
    hit_addrs = {}          # section -> set of referenced addresses
    named = set()
    for name, addend in refs:
        named.add(name)
        base = by_name.get(name)
        if base is None:
            continue
        if is_anchor(base):
            opaque.add(base["section"])
            continue
        hit_addrs.setdefault(base["section"], set()).add(base["value"] + addend)

    unreferenced = []
    for s in syms:
        if not (s["local"] and s["object"]):
            continue
        if s["section"] not in STRIPPABLE_SECTIONS or s["size"] == 0:
            continue
        if s["section"] in opaque:
            continue
        if s["name"] in named:
            continue
        lo, hi = s["value"], s["value"] + s["size"]
        if any(lo <= a < hi for a in hit_addrs.get(s["section"], ())):
            continue
        unreferenced.append(s)

    if unreferenced:
        print(f"[{unit}] DEAD-STRIP WARNING: "
              f"{len(unreferenced)} local data symbol(s) with no incoming "
              f"relocation in this object -- mwld will strip them from the "
              f"linked image (advisory, not a failure):")
        for s in unreferenced:
            print(f"[{unit}]   {s['section']}+0x{s['value']:X} "
                  f"size 0x{s['size']:X}  {s['name']}")
        print(f"[{unit}]   see claim.law.base-cast-reconstruction-"
              f"deadstrips-sibling-statics: if these tile one target blob "
              f"reached through a sibling symbol, merge them into ONE "
              f"aggregate so a single live reference keeps the whole blob.")
    elif not quiet_ok:
        print(f"[{unit}] dead-strip screen: OK, every local data symbol "
              f"is relocated against")
    return unreferenced


def ours_object(base):
    """Select the active compiler output, never an abandoned transform file.

    The hash-bound generated graph owns this choice. Object existence or
    mtime cannot establish that a postprocessor is still active. This is a
    compiler-stage data measurement, not a final-link equivalence proof;
    dependency freshness remains the builder's responsibility.
    """
    from raw_object import RawObjectError, resolve_object
    try:
        selected = resolve_object(base, root=REPO, version=VERSION)
    except RawObjectError as exc:
        raise MeasurementUnavailable(str(exc)) from exc
    linked = REPO / "build" / VERSION / "src" / f"{base}.o"
    if (selected.path != linked and linked.is_file()
            and linked.stat().st_mtime < selected.path.stat().st_mtime):
        print(f"[note] {linked.name}: transformed object is STALE"
              " (older than its active compiler output); comparing the"
              " compiler-stage object")
    return selected.path


def claimed_starts(units):
    """section -> set of claim START addresses, for the neighbour test."""
    starts = {}
    for secs in units.values():
        for sec, (lo, _hi) in secs.items():
            starts.setdefault(sec, set()).add(lo)
    return starts


def shrink_reachable(end_true, sec, starts):
    """Is 'shrink the claim to the object's true extent' applicable here?

    claim.law.AF_dtk-rejects-an-unaligned-auto-split-start-so-some-claim-
    slack-is-structural: shrinking makes dtk auto-generate a split starting
    at end_true and hard-fail `Invalid alignment for split` unless that
    address is 4-byte aligned. When the following range is already CLAIMED
    by another unit no auto split is generated, so any alignment is fine.
    """
    if end_true % 4 == 0:
        return "shrinkable"
    if end_true in (starts or {}).get(sec, ()):
        return "shrinkable"
    return "structural"


def section_verdict(ours, orig, rel, lo, hi, sec=None, starts=None):
    """Pure classifier for one data section. No I/O, no build.

    ours/orig are bytes; rel is the set of relocated word offsets in ours;
    [lo, hi) is the splits.txt claim. Returns a dict with:
      bytebad     -- count of REAL defects (a flip blocker; drives exit 1)
      slack       -- trailing claimed bytes our object does not emit
      slack_zero  -- True when that slack is zero-filled in the DOL
      reach       -- 'shrinkable' | 'structural' for a zero-filled slack
      diffs       -- [(offset, ours4, dol4)] of differing non-reloc words
      skipped     -- relocated words not compared
      overrun     -- our object emits MORE than the claim
    """
    v = {"bytebad": 0, "slack": 0, "slack_zero": False, "reach": None,
         "diffs": [], "skipped": 0, "overrun": False, "compared": 0}
    if len(ours) > hi - lo:
        v["overrun"] = True
        v["bytebad"] += 1
    n = min(len(ours), len(orig))
    v["compared"] = n
    for i in range(0, n, 4):
        if i in rel:
            v["skipped"] += 1
            continue
        a = bytes(ours[i:i + 4])
        b = orig[i:i + len(a)]
        if a != b:
            v["bytebad"] += 1
            v["diffs"].append((i, a, b))
    tail = orig[n:]
    if tail:
        v["slack"] = len(tail)
        v["slack_zero"] = all(c == 0 for c in tail)
        if not v["slack_zero"]:
            # real bytes are missing from our object -- a true blocker
            v["bytebad"] += 1
        else:
            v["reach"] = shrink_reachable(lo + n, sec, starts)
    return v


def check_unit(unit, claims, run_deadstrip=True, starts=None,
               strict_slack=False, debt=None):
    obj = ours_object(unit.rsplit(".", 1)[0])
    if not obj.exists():
        raise MeasurementUnavailable(f"[{unit}] object not built ({obj})")
    secs = obj_sections(obj)
    relocs = obj_relocs(obj)
    if run_deadstrip:
        deadstrip_check(unit, obj)
    bad = 0
    for sec in DATA_SECTIONS:
        ours = secs.get(sec)
        claim = claims.get(sec)
        if ours is None and claim is None:
            continue
        if ours is None:
            print(f"[{unit}] {sec}: claimed 0x{claim[1]-claim[0]:X} but object emits nothing")
            bad += 1
            continue
        if claim is None:
            print(f"[{unit}] {sec}: object emits 0x{len(ours):X} but nothing claimed")
            bad += 1
            continue
        lo, hi = claim
        orig = dol_read(lo, hi - lo)
        if orig is None:
            print(f"[{unit}] {sec}: claim 0x{lo:08X} not inside DOL")
            bad += 1
            continue
        rel = relocs.get(sec, set())
        v = section_verdict(ours, orig, rel, lo, hi, sec=sec, starts=starts)
        if v["overrun"]:
            print(f"[{unit}] {sec}: object 0x{len(ours):X} bytes > claim 0x{hi-lo:X}")
        for i, a, b in v["diffs"][:8]:
            va = lo + i
            fa = struct.unpack(">f", a.ljust(4, b"\0"))[0]
            fb = struct.unpack(">f", b.ljust(4, b"\0"))[0]
            print(f"[{unit}] {sec}+0x{i:X} (VA 0x{va:08X}): "
                  f"ours {a.hex()} ({fa!r}) != dol {b.hex()} ({fb!r})")
        if len(v["diffs"]) > 8:
            print(f"[{unit}] {sec}: ... more mismatches suppressed")
        n = v["compared"]
        # Zero-filled claim slack is DATA-CREDIT DEBT, never a flip blocker:
        # every compared byte is equal and the linker regenerates the pad
        # from the next section's alignment. Blocking on it refused a class
        # the project has already shipped ~44 members of. It still costs
        # objdiff data credit on SOME units (claim.law.splits-claim-can-
        # swallow-link-alignment-padding), and the shrink fix is only
        # reachable for the `shrinkable` half (claim.law.AF_dtk-rejects-an-
        # unaligned-auto-split-...), so it is reported, not silenced.
        counted_slack = v["slack"] if (v["slack"] and v["slack_zero"]
                                       and strict_slack) else 0
        secbad = v["bytebad"] + (1 if counted_slack else 0)
        if v["slack"] and v["slack_zero"]:
            status = "MISMATCH" if secbad else "DATA-DEBT"
        else:
            status = "OK" if not secbad else "MISMATCH"
        extra = f", {v['skipped']} reloc words skipped" if v["skipped"] else ""
        if not v["slack"]:
            slack = ""
        elif v["slack_zero"]:
            slack = (f", 0x{v['slack']:X} claim slack (zero-filled;"
                     f" {v['reach']} — advisory, not a flip blocker)")
            if debt is not None:
                debt.append((unit, sec, v["slack"], v["reach"]))
        else:
            slack = f", 0x{v['slack']:X} claim slack (NONZERO!)"
        print(f"[{unit}] {sec}: {status} 0x{n:X} bytes compared{extra}{slack}")
        bad += secbad
    return bad


def section_sizes(obj):
    """{section name: size} from `objdump -h`."""
    out = dump_object(obj, "-h")
    table = {}
    for m in re.finditer(
            r"^\s*\d+\s+(\S+)\s+([0-9a-fA-F]{8})\s", out, re.M):
        table[m.group(1)] = int(m.group(2), 16)
    return table


def section_bytes(obj, sec):
    """The raw bytes of one section from `objdump -s`."""
    out = dump_object(obj, "-s", "-j", sec)
    data = bytearray()
    for line in out.splitlines():
        m = re.match(r"^ [0-9a-f]+ ((?:[0-9a-f]{2,8} ?){1,4}) ", line)
        if m:
            data += bytes.fromhex(m.group(1).replace(" ", ""))
    return bytes(data)


def size_gap_class(target_bytes, our_bytes, bss=False):
    """Classify a section SIZE mismatch: 'debt' classes vs 'blocker' classes.

    ONE VERDICT FOR ONE FACT (run-49 item 3). `--sections` called every size
    mismatch a FLIP BLOCKER while the byte mode called the SAME bytes
    advisory DATA-DEBT. Measured at 96d689120 on dolphin/si/SIBios — a unit
    that is `Object(Matching, ...)`, linked, and green:

        plain      .data: DATA-DEBT 0xCD bytes compared, 0x3 claim slack
                   (zero-filled; structural - advisory, not a flip blocker)
                   exit 0
        --sections .data: SIZE target 0xD0 vs ours 0xCD  <- FLIP BLOCKER
                   exit 1

    Same three bytes, two labels, opposite exit codes. The advisory label is
    the proven one: the byte mode's own reclassification (docstring above)
    was justified by 44 already-shipped units, and this unit is one of them.

    TWO-SIDED CENSUS at 96d689120 over `--sections --matching`, i.e. over
    units that have ALREADY flipped and link green, so every FLIP BLOCKER
    row there is a false positive by construction: 231 rows printed, 74
    flagged, and the 74 decompose as

        54  zero-filled tail over an identical head  -> debt-zero-slack
        19  .bss/.sbss claim slack, target larger    -> debt-bss-slack
         1  OURS LARGER than target                  -> stays a blocker
         0  nonzero tail (real bytes missing)        -> stays a blocker
         0  differing head                           -> stays a blocker

    The 54 are the same rows the byte mode already counts as DATA-DEBT (its
    own summary reads 54 sections over 45 units), so this is one label
    removed, not a new leniency. The 19 bss rows are the same argument made
    stronger: .bss occupies NO DOL bytes, and the byte mode does not screen
    bss at all, so nothing was ever measured against the image there.

    FALSIFIER: a unit whose only section residue is a zero-filled
    target-larger tail (or bss claim slack) and which nevertheless fails the
    full-link `main.dol: OK` gate. `--strict-slack` restores the refusing
    behaviour for both classes.

    The remaining blocker classes are unchanged, including OURS-LARGER,
    which is the case this mode exists for: the DOL-range byte check is
    structurally blind to our object emitting MORE than the target.
    """
    tlen = len(target_bytes) if target_bytes is not None else 0
    olen = len(our_bytes) if our_bytes is not None else 0
    if olen > tlen:
        return "blocker-ours-larger"
    if bss:
        return "debt-bss-slack"
    if target_bytes[:olen] != our_bytes:
        return "blocker-head-differs"
    if any(target_bytes[olen:]):
        return "blocker-nonzero-tail"
    return "debt-zero-slack"


def refine_unclaimed(gap, target_len, claimed):
    """Identify absent target ownership, without proving any source/data cure.

    The zero target size follows from missing split ownership. Emitted
    surplus may be dead-stripped or may need reconstruction; size alone
    decides neither. The conservative preflight refusal remains in place.
    """
    if gap == "blocker-ours-larger" and not target_len and not claimed:
        return "blocker-unclaimed-section"
    return gap


GAP_BLURB = {
    "debt-zero-slack": ("the target's extra bytes are ZERO over an identical"
                        " head — the same claim slack the byte mode scores"
                        " advisory; the linker regenerates it"),
    "debt-bss-slack": ("bss claim slack — bss occupies NO bytes in the DOL,"
                       " and the byte mode does not screen it at all"),
    "blocker-ours-larger": ("OURS is LARGER than the target — the DOL-range"
                            " byte check is structurally blind to this"),
    "blocker-unclaimed-section": (
        "UNCLAIMED SECTION — the target side is 0x0 because the dtk split"
        " assigned no bytes of this section to this TU. Ownership and"
        " link-reachability review required: surplus may be dead-stripped;"
        " its values are not proved correct or wrong. Use"
        " tools/gdl/composed_census/af_data_base_census.py for CANDIDATE"
        " bases, then verify bytes/relocations/ownership before changing"
        " config/GUNE5D/splits.txt. Do not automatically add its claim."),
    "blocker-nonzero-tail": ("the target's extra bytes are NONZERO — real"
                             " bytes are missing from ours"),
    "blocker-head-differs": ("the compared head DIFFERS, so this is not a"
                             " pure claim-slack gap"),
}


REPORT = REPO / "build" / VERSION / "report.json"


def report_extab_sections(base):
    """[(name, size, fuzzy%)] for a unit's extab-family sections.

    Supplemental report snapshot only, never a replacement for inspecting
    the actual object sections. Objects DO contain dotless extab sections;
    R60 refuted the old dot-only parser's circular absence claim. Missing
    report data is harmless here because the direct metadata audit decides.
    """
    try:
        data = json.loads(REPORT.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return []
    for unit in data.get("units", []):
        if unit.get("name", "").split("/", 1)[-1] == base:
            return [(s["name"], int(s["size"]),
                     s.get("fuzzy_match_percent"))
                    for s in unit.get("sections", [])
                    if "extab" in s.get("name", "")]
    return []


def exception_table(target_object, our_object):
    """Versioned identity-based metadata result, independent of record order."""
    from exception_metadata import exception_records, compare_exception_records
    try:
        target = exception_records(Path(target_object).read_bytes())
        ours = exception_records(Path(our_object).read_bytes())
        result = compare_exception_records(target, ours)
    except (OSError, ValueError) as exc:
        return {"schema_version": 1, "status": "UNRESOLVED", "error": str(exc)}
    result["schema_version"] = 1
    result["status"] = ("FAIL" if result["missing"] or result["changed"] else
                        "UNRESOLVED" if result["extra"] else "PASS")
    return result


def section_table(unit_key, strict_slack=False, debt=None, report=None):
    """--sections: ours-vs-target per-section size + match table.

    Compares our built object's data-class sections directly against the
    TARGET split object — the comparison the flip gate actually runs.
    Catches what the DOL-range byte check is structurally blind to: our
    object emitting MORE bytes than the target object (the DOL range
    compares fine; the objects differ), and surfaces the per-section
    match state that objdiff's all-or-nothing matched_data hides.

    A SIZE mismatch is classified by `size_gap_class` rather than being
    called a FLIP BLOCKER unconditionally — see that function for the
    contradiction this removes and the census behind it.
    """
    base = unit_key.rsplit(".", 1)[0]
    ours_o = ours_object(base)
    tgt_o = REPO / "build" / VERSION / "obj" / f"{base}.o"
    missing = [str(p) for p in (ours_o, tgt_o) if not p.exists()]
    if missing:
        raise MeasurementUnavailable(f"[{unit_key}] UNRESOLVED --sections: missing {missing}")

    content = section_bytes
    ts, os_ = section_sizes(tgt_o), section_sizes(ours_o)
    # What this .c actually claims in splits.txt, so an OURS-LARGER row can be
    # told apart from an UNCLAIMED one per row instead of by assumption.
    claims = parse_splits().get(unit_key, {})
    bad = 0
    compared = 0
    rows = []
    for sec in DATA_SECTIONS + (".bss", ".sbss", ".sbss2"):
        tlen, olen = ts.get(sec), os_.get(sec)
        if tlen is None and olen is None:
            continue
        compared += 1
        if tlen != olen:
            bss = sec.startswith((".bss", ".sbss"))
            tb = content(tgt_o, sec) if tlen and not bss else b""
            ob = content(ours_o, sec) if olen and not bss else b""
            if bss:
                # No contents exist to compare; the sizes decide.
                gap = ("blocker-ours-larger" if (olen or 0) > (tlen or 0)
                       else "debt-bss-slack")
            else:
                gap = size_gap_class(tb, ob, bss=False)
            gap = refine_unclaimed(gap, tlen or 0, sec in claims)
            blocks = gap.startswith("blocker") or strict_slack
            mark = "FLIP BLOCKER" if blocks else "DATA-DEBT"
            print(f"[{unit_key}] {sec}: SIZE target 0x{tlen or 0:X} vs"
                  f" ours 0x{olen or 0:X}  <- {mark} — {GAP_BLURB[gap]}"
                  + ("  [--strict-slack: counted as a blocker]"
                     if strict_slack and not gap.startswith("blocker")
                     else ""))
            rows.append({"section": sec, "target_size": tlen or 0, "ours_size": olen or 0,
                         "status": "UNRESOLVED" if gap in ("blocker-unclaimed-section", "blocker-ours-larger")
                         else "FAIL" if blocks else "PASS", "classification": gap,
                         "interpretation": GAP_BLURB[gap]})
            if blocks:
                bad += 1
            elif debt is not None:
                debt.append((unit_key, sec, (tlen or 0) - (olen or 0), gap))
            continue
        if sec.startswith((".bss", ".sbss")):
            print(f"[{unit_key}] {sec}: size 0x{tlen:X} equal (bss)")
            rows.append({"section": sec, "status": "PASS", "target_size": tlen,
                         "ours_size": olen, "scope": "BSS allocation size only; no stored payload"})
            continue
        tb, ob = content(tgt_o, sec), content(ours_o, sec)
        if len(tb) != tlen or len(ob) != olen:
            raise MeasurementUnavailable(f"[{unit_key}] {sec}: section dump size does not match its header")
        same = sum(1 for a, b in zip(tb, ob) if a == b)
        pct = 100.0 * same / len(tb) if tb else 100.0
        mark = "" if pct == 100.0 else "  <- FLIP BLOCKER (reloc words may"\
            " account for some — cross-check the byte mode)"
        print(f"[{unit_key}] {sec}: size 0x{tlen:X}, {pct:.1f}% bytes"
              f" equal{mark}")
        rows.append({"section": sec, "status": "PASS" if pct == 100.0 else "UNRESOLVED",
                     "target_size": tlen, "ours_size": olen, "raw_bytes_equal_percent": pct,
                     "scope": "raw section bytes; relocation payload identity remains a separate obligation"})
        if pct != 100.0:
            bad += 1
            # A percentage hides a finish-line residual: "98.2% equal"
            # was actually a fully-characterized 2-byte transposition a
            # worker had to hand-derive. Name the differing bytes when
            # they are few; summarize when they are not.
            diffs = [i for i, (a, b) in enumerate(zip(tb, ob)) if a != b]
            head = ", ".join(
                f"+0x{i:X} (T {tb[i]:02x} vs O {ob[i]:02x})"
                for i in diffs[:16])
            tail = f" … and {len(diffs) - 16} more" if len(diffs) > 16 else ""
            print(f"[{unit_key}] {sec}: {len(diffs)} byte(s) differ:"
                  f" {head}{tail}")
    eh_names = sorted({name for name in ts.keys() | os_.keys()
                       if name.lstrip(".") in ("extab", "extabindex")})
    eh = None
    if eh_names:
        for name in eh_names:
            print(f"[{unit_key}] {name}: OBJECT size target 0x{ts.get(name, 0):X}"
                  f" vs ours 0x{os_.get(name, 0):X}; compared by function identity, not record position")
        eh = exception_table(tgt_o, ours_o)
        print(f"[{unit_key}] exception metadata: {eh['status']} "
              f"target_records={eh.get('target_records', 'unreadable')} "
              f"ours_records={eh.get('ours_records', 'unreadable')} "
              f"missing={len(eh.get('missing', []))} changed={len(eh.get('changed', {}))} "
              f"extra={len(eh.get('extra', {}))}")
        if "error" in eh:
            print(f"[{unit_key}] {eh['error']}")
        for kind in ("missing", "changed", "extra"):
            if eh.get(kind):
                print(f"[{unit_key}] exception {kind}: {', '.join(eh[kind])}")
        if eh["status"] != "PASS":
            bad += 1
        print(f"[{unit_key}] extra EH records require link-reachability review;"
              " metadata equality is not whole-object/link equality")
    for name, size, fuzzy in report_extab_sections(base):
        pct = "n/a" if fuzzy is None else f"{fuzzy:.4f}%"
        print(f"[{unit_key}] {name}: {size} bytes, {pct} matched (report.json"
              " snapshot, may be stale; not the direct metadata verdict)")
    print(f"[{unit_key}] --sections: {compared} object data section(s)"
          f" compared, {bad} blocker(s)"
          + ("  (NOTHING TO COMPARE: neither object carries a data-class"
             " or EH section -- this is a verdict, not silence)" if not compared and not eh_names
             else ""))
    statuses = {r["status"] for r in rows}
    if eh is not None:
        statuses.add(eh["status"])
    if report is not None:
        report.append({"unit": unit_key, "status": "FAIL" if "FAIL" in statuses else
                       "UNRESOLVED" if "UNRESOLVED" in statuses else "PASS",
                       "sections": rows, "exception_metadata": eh,
                       "target_object": str(tgt_o.relative_to(REPO)),
                       "ours_object": str(ours_o.relative_to(REPO))})
    return bad


def main(argv=None):
    import argparse
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("units", nargs="*")
    parser.add_argument("--matching", action="store_true")
    parser.add_argument("--deadstrip", action="store_true")
    parser.add_argument("--no-deadstrip", action="store_true")
    parser.add_argument("--sections", action="store_true")
    parser.add_argument("--strict-slack", action="store_true")
    parser.add_argument("--out", type=Path, help="schema-1 --sections JSON; PASS/FAIL/UNRESOLVED")
    parser.add_argument("--objdump", type=Path, help="override the native platform binutils executable")
    options = parser.parse_args(argv)
    global OBJDUMP
    if options.objdump is not None:
        OBJDUMP = options.objdump
    if options.out and not options.sections:
        parser.error("--out currently requires --sections")
    args = (["--matching"] if options.matching else []) + options.units
    if options.deadstrip:
        args.append("--deadstrip")
    if options.no_deadstrip:
        args.append("--no-deadstrip")
    if options.sections:
        args.append("--sections")
    if options.strict_slack:
        args.append("--strict-slack")
    only_deadstrip = "--deadstrip" in args
    run_deadstrip = "--no-deadstrip" not in args
    only_sections = "--sections" in args
    strict_slack = "--strict-slack" in args
    args = [a for a in args
            if a not in ("--deadstrip", "--no-deadstrip", "--sections",
                         "--strict-slack")]
    if not args:
        print(__doc__)
        return 1
    # --deadstrip also accepts a direct path to any .o (synthetic tests,
    # objects outside the splits map).
    if only_deadstrip:
        direct = [a for a in args if a.endswith(".o")]
        for p in direct:
            obj = Path(p)
            if not obj.exists():
                print(f"[{p}] UNRESOLVED: no such object")
                return 2
            deadstrip_check(obj.name, obj, quiet_ok=False)
        args = [a for a in args if not a.endswith(".o")]
        if not args:
            return 0

    units = parse_splits()
    targets = []
    if args[0] == "--matching":
        cfg = (REPO / "configure.py").read_text(encoding="utf-8")
        targets = [m.group(1).rsplit(".", 1)[0] for m in
                   re.finditer(r'Object\(Matching, "([^"]+)"', cfg)]
    else:
        targets = [a.replace("\\", "/") for a in args]
    bad = 0
    starts = claimed_starts(units)
    debt = []
    section_debt = []
    unresolved = []
    measurements = []
    for t in targets:
        key = next((k for k in units if k.rsplit(".", 1)[0] == t or k == t), None)
        if key is None:
            # RUN-56 ITEM 3, and the third recurrence of run-50 item 3's rule
            # in this same tool: EMPTY OUTPUT CAN NEVER MEAN SUCCESS. Measured
            # at 105dbd1ea, `python tools/gdl/datadiff.py --sections
            # game/does/not/exist` printed `[game/does/not/exist] no splits
            # entry` and EXITED 0, so a scripted flip gate read a mistyped unit
            # as zero blockers; `textorder.py` exits 2 on the same input.
            #
            # Half of this is a SPELLING bug, not a typo. `src/game/enemy/
            # critter` is a spelling AGENTS.md says the core tools accept
            # (fndiff.unit_key is the one normalizer), and datadiff did not
            # normalize, so a correct unit named the documented way landed on
            # this branch too. Normalize first, then refuse.
            #
            # TWO-SIDED CALIBRATION at 105dbd1ea over configure.py's 255
            # `Object()` rows against splits.txt's 257 units: exactly ONE row
            # resolves to no splits entry —
            # `TRK_MINNOW_DOLPHIN/ppc/Generic/exception.s`, an assembly file,
            # not a C unit — and ZERO rows have a splits entry with an unbuilt
            # object. So refusing costs one `.s` row and catches every
            # mistyped unit.
            normalized = _unit_key(t)
            key = next((k for k in units
                        if k.rsplit(".", 1)[0] == normalized or k == normalized),
                       None)
            if key is None:
                print(f"[{t}] no splits entry -- UNRESOLVED UNIT, nothing was"
                      " compared (this is a REFUSAL, not a clean result)")
                unresolved.append(t)
                measurements.append({"unit": t, "status": "UNRESOLVED", "error": "no splits entry"})
                continue
            print(f"[{t}] resolved to splits unit {key}")
        if only_deadstrip:
            try:
                obj = ours_object(key.rsplit(".", 1)[0])
                deadstrip_check(key, obj, quiet_ok=False)
            except (OSError, ValueError, MeasurementUnavailable) as exc:
                print(f"[{key}] UNRESOLVED: {exc}")
                unresolved.append(key)
                measurements.append({"unit": key, "status": "UNRESOLVED", "error": str(exc)})
            continue
        if only_sections:
            try:
                bad += section_table(key, strict_slack=strict_slack,
                                     debt=section_debt, report=measurements)
            except (OSError, ValueError, MeasurementUnavailable) as exc:
                print(f"[{key}] UNRESOLVED: {exc}")
                unresolved.append(key)
                measurements.append({"unit": key, "status": "UNRESOLVED", "error": str(exc)})
            continue
        try:
            bad += check_unit(key, units[key], run_deadstrip=run_deadstrip,
                              starts=starts, strict_slack=strict_slack, debt=debt)
        except (OSError, ValueError, MeasurementUnavailable) as exc:
            print(f"[{key}] UNRESOLVED: {exc}")
            unresolved.append(key)
    if debt and not strict_slack:
        shrink = [d for d in debt if d[3] == "shrinkable"]
        total = sum(d[2] for d in debt)
        print(f"\nDATA-CREDIT DEBT (advisory, exit code unaffected):"
              f" {len(debt)} zero-filled claim-slack section(s) over"
              f" {len({d[0] for d in debt})} unit(s), {total} bytes;"
              f" {len(shrink)} shrinkable, {len(debt)-len(shrink)} structural"
              f" (dtk would reject the auto-split).")
        print("  shrinkable rows are fixable per claim.law.splits-claim-can-"
              "swallow-link-alignment-padding.20260901.v1 (shrink splits.txt"
              " END *and* the owning symbols.txt size in one edit).")
        print("  structural rows are NOT fixable: claim.law.AF_dtk-rejects-an-"
              "unaligned-auto-split-start-so-some-claim-slack-is-structural"
              ".20260903.v1.")
    if section_debt:
        zero = [d for d in section_debt if d[3] == "debt-zero-slack"]
        total = sum(d[2] for d in section_debt)
        print(f"\nSECTION-SIZE DEBT (advisory, exit code unaffected):"
              f" {len(section_debt)} section(s) over"
              f" {len({d[0] for d in section_debt})} unit(s), {total} bytes;"
              f" {len(zero)} zero-filled claim slack (the SAME rows the byte"
              f" mode reports as DATA-DEBT), {len(section_debt)-len(zero)}"
              " bss claim slack (no DOL bytes).")
        print("  --strict-slack counts these as flip blockers again;"
              " OURS-LARGER, a nonzero tail and a differing head always do.")
    statuses = {row["status"] for row in measurements}
    status = ("FAIL" if "FAIL" in statuses else "UNRESOLVED" if
              unresolved or "UNRESOLVED" in statuses else "FAIL" if bad else "PASS")
    if options.out:
        options.out.parent.mkdir(parents=True, exist_ok=True)
        options.out.write_text(json.dumps({"schema_version": 1, "status": status,
            "scope": "object data/BSS and supported function-indexed exception records; not full relocation/link equality",
            "units_selected": len(targets), "rows": measurements}, indent=2) + "\n", encoding="utf-8")
        print(f"DATADIFF {status}: wrote {options.out}")
    if unresolved:
        print(f"\nUNRESOLVED UNIT(S): {len(unresolved)} of {len(targets)}"
              f" argument(s) could not be compared -- {', '.join(unresolved)}."
              " NO comparison was made for them; do not read this run as a"
              " clean gate. Exit 2 means unresolved input or candidates;"
              " exit 1 means a measured failing obligation.")
    return {"PASS": 0, "FAIL": 1, "UNRESOLVED": 2}[status]


if __name__ == "__main__":
    sys.exit(main())
