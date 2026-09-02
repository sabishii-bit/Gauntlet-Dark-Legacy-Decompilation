#!/usr/bin/env python3
"""Image-wide WRONG-CONSTANT detector: join our pool relocations against the
target's named data symbols and compare the VALUES both sides actually read.

Why this exists
---------------
claim.law.SL_pool-constant-errors-are-score-invisible.20260901.v1: MWCC
materializes a literal into an .sdata2/.rodata pool entry and the function
merely LOADS it, so a wrong value changes no instruction word, no opcode, no
displacement and no relocation TYPE.  fndiff `real`, `--ops`, regnorm and
objdiff fuzzy are all computed over the instruction stream and are
structurally incapable of seeing it; the DOL sha1 cannot see it either,
because an unmatched function is linked from its ORIGINAL extracted object.
Three such bugs shipped that way (quarter-gravity 8.0-for-32.0f in
ProcessSpewItems; a 0.5f-for-0.01f collision radius in ProcessEffects; a
short "OFF" string in screen_limitation).

The check
---------
For every function present in both the target object
(build/<V>/obj/<unit>.o, extracted from the retail DOL by dtk) and ours
(build/<V>/src/<unit>.o), collect every relocation site that reads a
READ-ONLY constant (.sdata2/.rodata), resolve it to the actual BYTES --
target symbols through config/<V>/symbols.txt + orig main.dol, our anonymous
`@NNNN` pool entries through our own object's symbol table and section
contents -- and diff the two multisets.

The comparison is a MULTISET, not a positional join, and that is deliberate:
these functions are unmatched, so schedule and register allocation move the
load sites around.  A multiset is alignment-free, which is what lets the
gravity canary (target `lfs` 32.0f vs our `lfd` 8.0 at a re-scheduled site)
survive the join.  Sites are keyed by (opcode-class, kind), so a value that
is right but loaded at the wrong WIDTH still reports -- rule (2) of the law.

Comparison is on VALUE first and load width second, and that ordering was
forced by the dirty-positive control rather than chosen: a first cut keyed
the multiset on the exact mnemonic, and the gravity canary -- target
`lfs` 32.0f against our `lfd` 8.0 -- did not pair at all, so a KNOWN bug
reported as ordinary reconstruction debt.  Pairing on the numeric value
independently of the mnemonic is what makes the control fire.

Verdict classes, highest signal first:
  VALUE_MISMATCH  after pairing equal numeric values, constants remain on
                  BOTH sides: the target reads a value we never read and we
                  read a value it never reads.  This is the wrong-constant
                  signature; all three known bugs are VALUE_MISMATCHes.
                  Hand-verify every one.
  WIDTH_MISMATCH  identical values, different load width -- the target reads
                  a float where we read the same number as a double (or the
                  reverse).  Rule (2) of the law: a right value at the wrong
                  width is still a source-form error.
  IMBALANCE       one side references constants the other does not at all.
                  Usually ordinary reconstruction debt in a NonMatching
                  function (a missing statement takes its literal with it).
                  Reported, ranked last.

Usage
-----
  python tools/gdl/composed_census/cs_constsweep.py                # image-wide
  python tools/gdl/composed_census/cs_constsweep.py game/boss/boss # one unit
  python tools/gdl/composed_census/cs_constsweep.py --json out.json
  python tools/gdl/composed_census/cs_constsweep.py --all          # + IMBALANCE

Run from the repository root.  Read-only: it never builds, edits or scores.
"""

import argparse
import json
import re
import struct
import subprocess
import sys
from collections import Counter
from pathlib import Path

VERSION = "GUNE5D"
OBJDUMP = Path("build/binutils/powerpc-eabi-objdump.exe")
if not OBJDUMP.exists():                       # non-Windows fallback
    OBJDUMP = Path("build/binutils/powerpc-eabi-objdump")
SYMBOLS_TXT = Path(f"config/{VERSION}/symbols.txt")
DOL_PATH = Path(f"orig/{VERSION}/sys/main.dol")

# Read-only sections: a value here is a CONSTANT.  Writable sections hold
# runtime globals, whose contents are initial values rather than the thing
# the instruction semantically "means", so they are out of scope.
POOL_SECTIONS = {".sdata2", ".rodata"}

# Load width by mnemonic.  None => the instruction FORMS AN ADDRESS rather
# than reading a scalar, so the interesting value is the whole object.
LOAD_WIDTH = {
    "lfs": 4, "lfsu": 4, "lfsx": 4,
    "lfd": 8, "lfdu": 8, "lfdx": 8,
    "lwz": 4, "lwzu": 4, "lwzx": 4,
    "lha": 2, "lhau": 2, "lhz": 2, "lhzu": 2,
    "lbz": 1, "lbzu": 1,
}
ADDR_FORMING = {"addi", "addis", "lis", "li", "ori", "la", "subi"}

# Which relocation sites NAME their datum completely.  The distinction below
# is not conservatism for its own sake -- it was forced by a measured false
# positive, and getting it wrong costs precision on every string in the image.
#
#   EMB_SDA21           always OK.  The small-data form encodes the whole
#                       21-bit displacement from the SDA base in this one
#                       instruction, so both a load (`lfs f1,x@sda21(r2)`)
#                       and an address materialization (`li r5,x@sda21`)
#                       are fully resolved by the relocation alone.  This is
#                       the form MWCC uses for the .sdata2 constant pool and
#                       it is how the "OFF" string canary is caught.
#
#   ADDR16_LO on a LOAD OK.  In `lfsu f1,sym@l(r3)` the instruction's
#                       displacement field IS the relocated half, so
#                       symbol+addend names the datum exactly.
#
#   ADDR16_LO forming   NOT OK.  In `lis rX,sym@ha; addi rX,rX,sym@l` the
#   an ADDRESS          register only holds a BASE; the selecting
#                       displacement lives in a LATER instruction.  Our
#                       objects address a whole string pool off one such
#                       base while dtk gives the target a per-string
#                       symbol, so comparing "the object at the relocation"
#                       compares a pool base against one string.  Enabling
#                       this produced 14 false positives across sfx.c and
#                       combat.c in one run -- including the exact
#                       CONTAINER/CAM: pair that
#                       attempt.CB_screen-limitation-string-size-audit-and-
#                       reloc-arbiter-bounds.20260901.v1 had already
#                       resolved as a NON-defect.  Resolving these needs a
#                       dataflow model of the base register; until one
#                       exists they are counted as skipped coverage.
#
#   ADDR16_HA           never OK on its own: it is the high half of the
#                       address its LO partner names.
FULL_ADDRESS_RELOCS = {"R_PPC_EMB_SDA21", "R_PPC_ADDR16_LO"}


# --------------------------------------------------------------------------
# retail DOL + symbols.txt (target-side value resolution)
# --------------------------------------------------------------------------

SYM_RE = re.compile(
    r"^(?P<name>\S+)\s*=\s*(?P<sect>\.\w+):0x(?P<addr>[0-9A-Fa-f]+);"
    r"(?P<attrs>.*)$")


def load_symbols():
    table = {}
    if not SYMBOLS_TXT.exists():
        return table
    for line in SYMBOLS_TXT.read_text(encoding="utf-8",
                                      errors="replace").splitlines():
        m = SYM_RE.match(line.strip())
        if not m:
            continue
        attrs = m.group("attrs")
        size = re.search(r"size:0x([0-9A-Fa-f]+)", attrs)
        kind = re.search(r"data:(\w+)", attrs)
        table[m.group("name")] = {
            "section": m.group("sect"),
            "addr": int(m.group("addr"), 16),
            "size": int(size.group(1), 16) if size else None,
            "data": kind.group(1) if kind else None,
        }
    return table


def load_dol():
    blob = DOL_PATH.read_bytes()
    sections = []
    for i in range(18):                        # 7 text + 11 data
        off = struct.unpack_from(">I", blob, 0x00 + i * 4)[0]
        addr = struct.unpack_from(">I", blob, 0x48 + i * 4)[0]
        size = struct.unpack_from(">I", blob, 0x90 + i * 4)[0]
        if off and addr and size:
            sections.append((addr, addr + size, off))
    return blob, sections


def read_va(blob, sections, addr, count):
    for start, end, off in sections:
        if start <= addr < end:
            take = min(count, end - addr)
            return blob[off + (addr - start): off + (addr - start) + take]
    return None


# --------------------------------------------------------------------------
# objdump parsing
# --------------------------------------------------------------------------

FUNC_RE = re.compile(r"^([0-9a-f]+) <(.+)>:$")
INSN_RE = re.compile(r"^\s+([0-9a-f]+):\s+(?:[0-9a-f]{2} ){4}\s*(.+)$")
RELOC_RE = re.compile(r"^\s+([0-9a-f]+):\s+(R_PPC_\S+)\s+(.+)$")


def _strip_dtk(name):
    """dtk suffixes local symbols with their address.  For lbl_/jumptable_
    the suffix IS the identity, so it is kept (fndiff learned this the hard
    way: stripping collapsed every pool label to bare "lbl")."""
    if name.startswith(("fn_", "lbl_", "jumptable_")):
        return name
    return re.sub(r"_80[0-9A-Fa-f]{6}$", "", name)


def run_objdump(args, obj):
    r = subprocess.run([str(OBJDUMP)] + args + [str(obj)],
                       capture_output=True, text=True)
    return r.stdout


def parse_code(obj):
    """{function: [ {off, opcode, relocs:[(type, sym, addend)]} ]}"""
    out = run_objdump(["-dr"], obj)
    funcs, cur, last = {}, None, None
    raw_names = re.findall(r"^[0-9a-f]+ <(.+)>:$", out, re.M)
    strip_counts = Counter(_strip_dtk(n) for n in raw_names)
    for line in out.splitlines():
        m = FUNC_RE.match(line)
        if m:
            cur = m.group(2)
            stripped = _strip_dtk(cur)
            if strip_counts[stripped] <= 1:
                cur = stripped
            funcs.setdefault(cur, [])
            last = None
            continue
        if cur is None:
            continue
        m = INSN_RE.match(line)
        if m:
            text = m.group(2).strip()
            last = {"off": int(m.group(1), 16),
                    "opcode": text.split()[0],
                    "relocs": []}
            funcs[cur].append(last)
            continue
        m = RELOC_RE.match(line)
        if m and last is not None:
            value = m.group(3).strip()
            addend = 0
            am = re.search(r"([+-])0x([0-9a-f]+)$", value)
            if am:
                addend = int(am.group(2), 16) * (-1 if am.group(1) == "-" else 1)
                value = value[:am.start()]
            last["relocs"].append((m.group(2), value.strip(), addend))
    return funcs


def parse_symtab(obj):
    """{name: {value, section, size}} for symbols defined in this object."""
    out = run_objdump(["-t"], obj)
    table = {}
    for line in out.splitlines():
        if "\t" not in line:
            continue
        head, tail = line.split("\t", 1)
        head_fields = head.split()
        tail_fields = tail.split(None, 1)
        if len(head_fields) < 2 or len(tail_fields) < 2:
            continue
        if not re.fullmatch(r"[0-9a-f]+", head_fields[0]):
            continue
        section = head_fields[-1]
        if not section.startswith("."):
            continue
        try:
            size = int(tail_fields[0], 16)
        except ValueError:
            continue
        table[tail_fields[1].strip()] = {
            "value": int(head_fields[0], 16),
            "section": section,
            "size": size,
        }
    return table


def parse_sections(obj):
    """{section: bytes} plus {section: set(relocated byte offsets)}.

    The relocated-offset set is how jump tables and pointer tables are
    excluded: their CONTENTS are link-time addresses, not constants, so
    comparing their bytes across two different objects is meaningless.
    """
    out = run_objdump(["-s", "-r"], obj)
    data, relocs = {}, {}
    cur, mode = None, None
    for line in out.splitlines():
        m = re.match(r"^Contents of section (\S+):$", line)
        if m:
            cur, mode = m.group(1), "data"
            data[cur] = bytearray()
            continue
        m = re.match(r"^RELOCATION RECORDS FOR \[(\S+)\]:$", line)
        if m:
            cur, mode = m.group(1), "reloc"
            relocs.setdefault(cur, set())
            continue
        if mode == "data" and cur is not None:
            m = re.match(r"^ ([0-9a-f]+) ((?:[0-9a-f]{2,8} ?){1,4})", line)
            if m:
                start = int(m.group(1), 16)
                blob = bytes.fromhex(m.group(2).replace(" ", ""))
                if len(data[cur]) < start:
                    data[cur].extend(b"\0" * (start - len(data[cur])))
                data[cur][start:start + len(blob)] = blob
            continue
        if mode == "reloc" and cur is not None:
            m = re.match(r"^([0-9a-f]{8}) R_PPC_\S+", line)
            if m:
                off = int(m.group(1), 16)
                relocs[cur].update(range(off, off + 4))
    return {k: bytes(v) for k, v in data.items()}, relocs


# --------------------------------------------------------------------------
# value resolution
# --------------------------------------------------------------------------

class Resolver:
    """Turns (symbol, addend, width) into the constant BYTES both sides read."""

    def __init__(self, dol_syms, dol_blob, dol_sections):
        self.dol_syms = dol_syms
        self.dol_blob = dol_blob
        self.dol_sections = dol_sections

    def via_dol(self, sym, addend, width):
        info = self.dol_syms.get(sym)
        if info is None or info["section"] not in POOL_SECTIONS:
            return None
        size = info["size"] or 4
        # For an ADDRESS-forming site read a fixed window, never the declared
        # size: dtk and MWCC disagree about where one datum ends.  Measured
        # on shop.c, where the target symbol lbl_80348440 is declared
        # `size:0x1 data:byte` holding 'I' while our object carries the whole
        # "ITEM" string at the same address -- comparing declared sizes
        # reported a wrong constant where both sides address one identical
        # string.  render() trims the window at the NUL.
        take = width if width else 64
        raw = read_va(self.dol_blob, self.dol_sections,
                      info["addr"] + addend, take)
        if raw is None:
            return None
        return {"bytes": raw, "size": size, "section": info["section"],
                "data": info.get("data"), "origin": sym}

    def via_object(self, sym, addend, width, symtab, sects, data_relocs):
        info = symtab.get(sym)
        if info is None or info["section"] not in POOL_SECTIONS:
            return None
        blob = sects.get(info["section"])
        if blob is None:
            return None
        base = info["value"] + addend
        size = info["size"] or 4
        take = width if width else 64     # see via_dol: window, not size
        raw = blob[base:base + take]
        if width is not None and len(raw) < take:
            return None
        if not raw:
            return None
        # a relocated datum is a link-time address, never a constant
        touched = data_relocs.get(info["section"], set())
        if any(o in touched for o in range(base, base + take)):
            return None
        return {"bytes": raw, "size": size, "section": info["section"],
                "data": None, "origin": sym}


def render(raw, width, data_kind, is_float):
    """Return (display text, WIDTH-INDEPENDENT value token).

    The token must not encode the load width, or a right-valued constant
    read at the wrong width reports as a wrong VALUE.  Measured: the
    BossSpewCoins canary (target `lfs` 0.1f, ours `lfd` holding the same
    number) misclassified exactly that way until the `f` suffix was moved
    out of the token and into the display text alone.
    """
    if is_float and width == 4 and len(raw) == 4:
        f = struct.unpack(">f", raw)[0]
        return f"{f!r}f", ("num", f)
    if is_float and width == 8 and len(raw) == 8:
        d = struct.unpack(">d", raw)[0]
        return f"{d!r}", ("num", d)
    if width and len(raw) == width and width <= 4:
        v = int.from_bytes(raw, "big")
        return f"0x{v:x}", ("int", v)
    # Address-formed window: trim at the NUL so the comparison is between
    # STRINGS, independent of how each toolchain sized the symbol.  The
    # trailing bytes of the trimmed string still count, which is what keeps
    # the "OFF    " vs "OFF   " canary (a trailing-space difference) visible.
    if raw and (32 <= raw[0] < 127) and b"\0" in raw:
        text_bytes = raw.split(b"\0")[0]
        if all(32 <= b < 127 for b in text_bytes):
            text = text_bytes.decode("ascii", "replace")
            return (f'"{text}" (len {len(text_bytes)})',
                    ("bytes_str", text_bytes))
    return raw.hex(), ("bytes", raw)


# --------------------------------------------------------------------------
# the sweep
# --------------------------------------------------------------------------

def sweep_unit(unit, resolver, verbose=False):
    target_o = Path(f"build/{VERSION}/obj/{unit}.o")
    base_o = Path(f"build/{VERSION}/src/{unit}.o")
    if not target_o.exists() or not base_o.exists():
        return [], {"skipped_unit": 1}

    tcode = parse_code(target_o)
    bcode = parse_code(base_o)
    bsym = parse_symtab(base_o)
    bsect, breloc = parse_sections(base_o)
    tsym = parse_symtab(target_o)
    tsect, treloc = parse_sections(target_o)

    stats = Counter()
    findings = []

    for fn in sorted(set(tcode) & set(bcode)):
        label, sites = {}, {}
        tvals, tentries = collect(tcode[fn], resolver, tsym, tsect, treloc,
                                  stats, "target", label, sites)
        bvals, bentries = collect(bcode[fn], resolver, bsym, bsect, breloc,
                                  stats, "ours", label, sites)

        # Detector 2: POSITIONAL.  Align the two opcode streams and compare
        # the constants read by instructions that pair up.  This exists
        # because the set detector below is blind whenever the wrong value
        # also occurs legitimately elsewhere in the same function -- exactly
        # what happened to the ProcessEffects canary, a 2300-instruction
        # function that reads 0.5 in several places, so "we read 0.5 and the
        # target never does" was simply false at whole-function scope even
        # though one specific SITE read 0.5 where the target read 0.01f.
        # Conversely this detector cannot see the gravity canary, whose two
        # sides differ in load WIDTH (lfs vs lfd) and therefore never align.
        # The two detectors are complementary and both are needed; neither
        # alone reproduces all three known bugs.
        for hit in aligned_mismatches(tentries, bentries):
            verdict = ("SLIDE_SUSPECT" if hit.get("slide_suspect")
                       else "ALIGNED_MISMATCH")
            stats[verdict] += 1
            findings.append({
                "unit": unit, "function": fn, "verdict": verdict,
                "detector": "positional", "width_diffs": [],
                "target_only": [f"{label.get(hit['t_token'])} "
                                f"@0x{hit['t_off']:x} ({hit['t_sym']})"],
                "ours_only": [f"{label.get(hit['b_token'])} "
                              f"@0x{hit['b_off']:x} ({hit['b_sym']})"],
                "sites": {}, "target_sites": [], "ours_sites": [],
                "opcode": hit["op"],
            })
        # Compare which (mnemonic, value) pairs OCCUR, never how many times.
        # A pure count difference is CSE/rematerialization -- see the set
        # semantics note below -- so it is not a finding of any class.
        if set(tvals) == set(bvals):
            continue

        # Compare value SETS, not multisets, and pair on the VALUE alone
        # rather than on the mnemonic.
        #
        # Set semantics is the single most important precision decision in
        # this tool, and it was forced by measurement.  Under multiset
        # semantics, damage_enemy reported "target reads 1.0, we never do"
        # -- while BOTH sides referenced the SAME symbol lbl_80346810 with
        # the SAME value, target simply loading it 8 times to our 7.  The
        # count difference is CSE/rematerialization, i.e. allocator
        # behaviour, and has nothing to do with what the code MEANS; every
        # such row is noise.  bosscam's BossCamBossCalc was the same story
        # (4 loads of lbl_80345BF8 against our 3).  A wrong CONSTANT is a
        # statement about which VALUES a function reads at all, so that is
        # what gets compared.
        #
        # The recall cost is explicit: a wrong constant whose bad value
        # ALSO appears legitimately elsewhere in the same function will not
        # report.  All three known bugs still fire, because a wrong value
        # is normally foreign to the function that reads it.
        t_by_value = {v for (_op, v) in tvals}
        b_by_value = {v for (_op, v) in bvals}
        only_t = t_by_value - b_by_value
        only_b = b_by_value - t_by_value

        if only_t and only_b:
            verdict = "VALUE_MISMATCH"
        elif only_t or only_b:
            verdict = "IMBALANCE"
        else:
            # same values on both sides, so the multiset only differed in
            # the mnemonic carrying them
            verdict = "WIDTH_MISMATCH"

        widths = []
        if verdict == "WIDTH_MISMATCH":
            t_ops, b_ops = set(tvals) - set(bvals), set(bvals) - set(tvals)
            # walk BOTH directions: the differing pair can sit on either
            # side, and a target-only walk alone printed empty findings
            for tok in sorted({t for _o, t in t_ops | b_ops}, key=str):
                t_op = [o for o, w in t_ops if w == tok]
                b_op = [o for o, w in b_ops if w == tok]
                widths.append({
                    "value": label.get(tok, str(tok)),
                    "target_opcode": t_op[0] if t_op else
                    next((o for o, w in tvals if w == tok), "?"),
                    "ours_opcode": b_op[0] if b_op else
                    next((o for o, w in bvals if w == tok), "?"),
                    "count": sum(c for (_o, t), c in tvals.items()
                                 if t == tok)})

        def show(tokens):
            """Value plus how many sites read it, on whichever side has it."""
            out = []
            for tok in sorted(tokens, key=str):
                n = sum(c for (_o, t), c in tvals.items() if t == tok) or \
                    sum(c for (_o, t), c in bvals.items() if t == tok)
                out.append(f"{label.get(tok, str(tok))} x{n}")
            return out

        def show_sites(counter):
            return [f"{op}:{label.get(tok, str(tok))} x{n}"
                    for (op, tok), n in sorted(counter.items(),
                                               key=lambda x: str(x[0]))]

        stats[verdict] += 1
        findings.append({
            "unit": unit, "function": fn, "verdict": verdict,
            "width_diffs": widths,
            "target_only": show(only_t),
            "ours_only": show(only_b),
            # function-relative byte offsets, so a candidate goes straight
            # into `fnasm.py <unit> <fn> 0xA:0xB --diff` for the aligned read
            "sites": {label.get(tok, str(tok)): sites.get(tok, [])
                      for tok in list(only_t) + list(only_b)},
            "target_sites": show_sites(tvals),
            "ours_sites": show_sites(bvals),
        })
    return findings, stats


def aligned_mismatches(tentries, bentries):
    """Constants that DISAGREE at instructions which positionally align.

    Alignment is difflib over the two opcode streams.  Inside an `equal`
    block the k-th target instruction and the k-th of ours carry the same
    mnemonic and the same surrounding context, so if both read a constant
    and the values differ, that is a substitution at one identified site --
    the aligned-view finding a human would make by eye, mechanized.

    Only `equal` blocks are used.  Replace/insert/delete regions are exactly
    where the two reconstructions genuinely diverge, and pairing across them
    is how a positional differ fabricates rows (the same failure mode
    claim.law.regnorm-positional-pairing-fabricates-displacement-rows
    records for regnorm).
    """
    import difflib
    t_ops = [e["op"] for e in tentries]
    b_ops = [e["op"] for e in bentries]
    matcher = difflib.SequenceMatcher(None, t_ops, b_ops, autojunk=False)

    # ordered constant sequences, for the anti-slide test below
    t_consts = [e["token"] for e in tentries if e["token"] is not None]
    b_consts = [e["token"] for e in bentries if e["token"] is not None]
    t_index = {id(e): n for n, e in
               enumerate(e for e in tentries if e["token"] is not None)}
    b_index = {id(e): n for n, e in
               enumerate(e for e in bentries if e["token"] is not None)}

    out = []
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag != "equal":
            continue
        for k in range(i2 - i1):
            t, b = tentries[i1 + k], bentries[j1 + k]
            if t["token"] is None or b["token"] is None:
                continue
            if t["token"] == b["token"]:
                continue

            # ANTI-SLIDE.  A constant-HOIST block is a long run of loads
            # whose opcodes align perfectly while the two sides hoist
            # slightly different SETS, so the k-th load on each side is a
            # different source expression and every pair reports.  Measured
            # on ProcessEffects: eight consecutive rows in which our value
            # was exactly the target's PREVIOUS constant (target 0.5/ours
            # 0.2, target 5.0/ours 0.5, ...), with the int-to-float magic
            # 0x4330000080000000 pairing against a real literal -- proof the
            # pairing, not the source, was wrong.  If either side's value
            # equals the other's immediate neighbour in constant order, the
            # streams are offset here and the pair carries no information.
            p, q = t_index[id(t)], b_index[id(b)]
            slid = False
            for d in (-1, 1):
                if 0 <= p + d < len(t_consts) and \
                        t_consts[p + d] == b["token"]:
                    slid = True
                if 0 <= q + d < len(b_consts) and \
                        b_consts[q + d] == t["token"]:
                    slid = True
            if slid:
                continue

            out.append({"op": t["op"], "t_token": t["token"],
                        "b_token": b["token"], "t_off": t["off"],
                        "b_off": b["off"], "t_sym": t["sym"],
                        "b_sym": b["sym"]})

    # RUN DEMOTION, the second half of the anti-slide defence.  A genuine
    # wrong constant is a LOCAL defect: one site, occasionally two.  A dense
    # RUN of mismatching sites is the signature of a misaligned block --
    # the same ProcessEffects hoist block, whose surviving rows sat within
    # 0x0c-0x38 bytes of each other while the two real bugs in that TU were
    # isolated by thousands of bytes.  Demoted rows are kept and reported
    # separately rather than dropped, because "this block's constants are
    # collectively unverifiable" is itself a reviewable statement.
    out.sort(key=lambda h: h["t_off"])
    run, runs = [], []
    for hit in out:
        if run and hit["t_off"] - run[-1]["t_off"] <= 0x80:
            run.append(hit)
        else:
            runs.append(run)
            run = [hit]
    runs.append(run)
    for group in runs:
        for hit in group:
            hit["slide_suspect"] = len(group) >= 3
    return out


FLOAT_LOADS = {"lfs", "lfsu", "lfsx", "lfd", "lfdu", "lfdx"}


def collect(insns, resolver, symtab, sects, data_relocs, stats, side, label,
            sites):
    """Return (multiset of (mnemonic, value token), per-instruction entries).

    `entries` carries EVERY instruction, constant-bearing or not, because the
    positional detector aligns on the full opcode stream.
    """
    bag = Counter()
    entries = []
    for ins in insns:
        entry = {"off": ins["off"], "op": ins["opcode"], "token": None,
                 "sym": None}
        entries.append(entry)
        for rtype, sym, addend in ins["relocs"]:
            if rtype not in FULL_ADDRESS_RELOCS:
                if rtype == "R_PPC_ADDR16_HA":
                    stats["skipped_addr16_ha"] += 1   # LO partner covers it
                continue
            op = ins["opcode"]
            width = LOAD_WIDTH.get(op)
            if width is None and op not in ADDR_FORMING:
                stats["skipped_opcode"] += 1
                continue
            if width is None and rtype == "R_PPC_ADDR16_LO":
                # base register only; the selecting displacement is in a
                # later instruction (see FULL_ADDRESS_RELOCS)
                stats["skipped_addr16_base"] += 1
                continue
            if sym.startswith("jumptable"):
                stats["skipped_jumptable"] += 1
                continue
            got = resolver.via_object(sym, addend, width, symtab, sects,
                                      data_relocs)
            if got is None:
                got = resolver.via_dol(sym, addend, width)
            if got is None:
                stats[f"unresolved_{side}"] += 1
                continue
            text, token = render(got["bytes"], width, got.get("data"),
                                 op in FLOAT_LOADS)
            if width is None and token[0] != "bytes_str":
                # An address-forming site whose datum is not a
                # NUL-terminated string cannot be compared: the two
                # toolchains disagree about where the object ENDS, so the
                # window holds a whole const table on one side and a single
                # entry on the other.  Measured on btext.c and pb_objregs.c,
                # where a 60-byte target table was "differing" from our
                # 2-byte entry at the same address.  Strings are exempt
                # because the NUL defines the end on both sides.
                stats["skipped_addr_nonstring"] += 1
                continue
            label.setdefault(token, text)
            sites.setdefault(token, []).append(
                f"{side}@0x{ins['off']:x} {op} {got['origin']}")
            bag[(op, token)] += 1
            entry["token"] = token
            entry["sym"] = got["origin"]
            stats[f"resolved_{side}"] += 1
    return bag, entries


def discover_units():
    root = Path(f"build/{VERSION}/obj")
    units = []
    for p in sorted(root.rglob("*.o")):
        unit = p.relative_to(root).with_suffix("").as_posix()
        if Path(f"build/{VERSION}/src/{unit}.o").exists():
            units.append(unit)
    return units


def _finding_pairs(finding):
    """The SUBSTITUTION pairs of a finding: (target_value, ours_value) for
    every value target reads that we don't crossed with every value we read
    that target doesn't."""
    t_only = [str(v) for v in (finding.get("target_only") or [])]
    o_only = [str(v) for v in (finding.get("ours_only") or [])]
    return [(t, o) for t in t_only for o in o_only]


def value_corroboration(findings):
    """{(target_value, ours_value) -> set of (unit, function)}.

    Keyed on the SUBSTITUTION pair, not on individual values. A single common
    constant (100.0, 0.5) recurs across dozens of NonMatching residuals as
    ordinary reconstruction debt and is NOT signal; what marks a systematic
    source error — a shared #define or table entry read wrong in every
    consumer — is the SAME swap (target reads X where we read Y) recurring
    across a SET of functions. That is the gravity/collision bug signature
    the sweep exists for, so the sweep ranks pair-corroborated rows first
    (run 34 item 8).
    """
    corro: dict[tuple, set] = {}
    for f in findings:
        site = (f.get("unit"), f.get("function"))
        for pair in _finding_pairs(f):
            corro.setdefault(pair, set()).add(site)
    return corro


def corroboration_score(finding, corro):
    """(max functions sharing any one substitution pair, [((t, o), n>=2)]).

    A score of 1 means no substitution this function makes recurs anywhere
    else (uncorroborated); >=2 means the SAME target->ours swap appears in
    that many DISTINCT functions.
    """
    best = 1
    shared = set()
    for pair in _finding_pairs(finding):
        n = len(corro.get(pair, ()))
        if n > best:
            best = n
        if n >= 2:
            shared.add((pair, n))
    return best, sorted(shared)


def rank_value_mismatches(findings):
    """VALUE_MISMATCH findings, set-corroborated rows first.

    Stable within a corroboration tier (unit, function), so the ranking is
    deterministic and a corroborated systematic bug never hides behind a
    lone reconstruction difference that happened to sweep first.
    """
    corro = value_corroboration(findings)
    ranked = []
    for f in findings:
        best, shared = corroboration_score(f, corro)
        row = dict(f)
        row["corroboration"] = best
        row["corroborated_values"] = shared
        ranked.append(row)
    ranked.sort(key=lambda r: (-r["corroboration"],
                               r.get("unit") or "", r.get("function") or ""))
    return ranked


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("units", nargs="*", help="unit paths (default: all)")
    ap.add_argument("--json", help="write findings to this path")
    ap.add_argument("--all", action="store_true",
                    help="print IMBALANCE findings too (default: "
                         "SUBSTITUTION only)")
    args = ap.parse_args()

    if not OBJDUMP.exists():
        print(f"missing {OBJDUMP} — run ninja first", file=sys.stderr)
        return 2
    if not DOL_PATH.exists():
        print(f"missing {DOL_PATH}", file=sys.stderr)
        return 2

    resolver = Resolver(load_symbols(), *load_dol())
    units = args.units or discover_units()
    units = [re.sub(r"\.(c|cpp)$", "", u.replace("\\", "/").strip("/")
                    .removeprefix("src/")) for u in units]

    all_findings, stats = [], Counter()
    for unit in units:
        found, st = sweep_unit(unit, resolver)
        all_findings.extend(found)
        stats.update(st)

    def of(kind):
        return [f for f in all_findings if f["verdict"] == kind]

    vals, widths, imb = rank_value_mismatches(of("VALUE_MISMATCH")), \
        of("WIDTH_MISMATCH"), of("IMBALANCE")
    aligned = of("ALIGNED_MISMATCH")

    print(f"units swept: {len(units)}   "
          f"constant sites resolved: target={stats['resolved_target']} "
          f"ours={stats['resolved_ours']}")
    print(f"unresolved: target={stats['unresolved_target']} "
          f"ours={stats['unresolved_ours']}   "
          f"skipped: addr16-ha={stats['skipped_addr16_ha']} "
          f"addr16-base={stats['skipped_addr16_base']} "
          f"opcode={stats['skipped_opcode']} "
          f"jumptable={stats['skipped_jumptable']}")
    slide = of("SLIDE_SUSPECT")
    print(f"\nSLIDE_SUSPECT (dense run = misaligned block, not a bug): "
          f"{len(slide)}")
    print(f"\nALIGNED_MISMATCH (positional: same site, different value): "
          f"{len(aligned)}")
    for f in aligned:
        print(f"  {f['unit']} :: {f['function']}  [{f['opcode']}]")
        print(f"      target {f['target_only'][0]}")
        print(f"      ours   {f['ours_only'][0]}")
    print(f"\nVALUE_MISMATCH (wrong-constant signature): {len(vals)}")
    print(f"WIDTH_MISMATCH (right value, wrong load width): {len(widths)}")
    print(f"IMBALANCE      (reconstruction debt, low signal): {len(imb)}")

    corroborated = sum(1 for f in vals if f["corroboration"] >= 2)
    if corroborated:
        print(f"  ({corroborated} of {len(vals)} are SET-CORROBORATED — the"
              " SAME target->ours substitution recurs across >=2 functions,"
              " i.e. a likely systematic wrong constant; listed first)")
    for f in vals:
        tag = (f"  [SET-CORROBORATED x{f['corroboration']}]"
               if f["corroboration"] >= 2 else "")
        print(f"\n=== VALUE_MISMATCH {f['unit']} :: {f['function']}{tag}")
        print(f"    target reads, we never do: {f['target_only']}")
        print(f"    we read, target never does: {f['ours_only']}")
        for (t_val, o_val), n in f["corroborated_values"]:
            print(f"    corroborated: target {t_val} -> ours {o_val} recurs"
                  f" in {n} function(s)")
    for f in widths:
        print(f"\n=== WIDTH_MISMATCH {f['unit']} :: {f['function']}")
        for w in f["width_diffs"]:
            print(f"    {w['value']}: target {w['target_opcode']} "
                  f"<> ours {w['ours_opcode']}  x{w['count']}")
    if args.all:
        for f in imb:
            print(f"\n--- IMBALANCE {f['unit']} :: {f['function']}")
            if f["target_only"]:
                print(f"    target-only: {f['target_only']}")
            if f["ours_only"]:
                print(f"    ours-only:   {f['ours_only']}")

    if args.json:
        Path(args.json).write_text(json.dumps(
            {"stats": dict(stats), "findings": all_findings}, indent=1),
            encoding="utf-8")
        print(f"\nwrote {args.json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
