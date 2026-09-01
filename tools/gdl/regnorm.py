#!/usr/bin/env python3
"""Register-normalized residual scan — the mandatory REGISTER_ONLY gate.

Per claim.law.identical-multiset-is-blind-to-displacements: instruction
count, opcode multiset, and `real` can all corroborate "register churn"
while hiding a wrong displacement or operand (a semantic field-selection
bug shipped under exactly that agreement). This tool answers the one
question those scores cannot: which differing words are explainable by a
register renaming, and which are STRUCTURAL?

Run it BEFORE labeling any residual REGISTER_ONLY, and after every
retained edit on such a residual. Two prior workers scripted this by
hand from fndiff --clean; this is that script, made durable.

Usage:
  python tools/gdl/regnorm.py game/game/pmotion get_player_pos
  python tools/gdl/regnorm.py game/enemy/enemy move_logic15 --map
  python tools/gdl/regnorm.py game/mb/mb_particle      # TU census mode:
      one summary row per common function

Output per aligned instruction pair:
  SAME        raw-identical (not printed unless --all)
  RENAMING    differs only in register fields — contributes to the map
  STRUCTURAL  differs beyond registers (opcode/displacement/immediate/reloc)
UNPAIRED lines (count mismatch) are always printed. --map prints the
ours->target register tally per register with an INCONSISTENT flag on any
register mapped to more than one target (the web-split tell: a colour
table needs def-site grouping, which a positional tally cannot see —
treat the map as a hint, not a sigma).

ARTIFACT ANNOTATORS. Not every STRUCTURAL row is a residual. Four classes
are called out, and the census prints GENUINE (= structural minus
artifacts) because ranking a roster on the raw structural number queues
finished functions:
  reloc-naming      pool @N vs a splitter-invented lbl_/jumptable_ name
  reloc-value       two symbol NAMES that denote the SAME datum — the
                    target's `potionicon_tab` against our `...bss.0`,
                    proven by both objects' symbol tables placing them at
                    one (section, offset). fndiff's private-alias mapper
                    covers ...data.N/...rodata.N but NOT ...bss.N, so
                    thirteen byte-exact player.c functions sat in a
                    "2 STRUCTURAL" band that a lane nearly queued as work
                    (claim.law.PL_regnorm-census-two-structural-band-is-a-
                    byte-exact-reloc-naming-signature.20260901.v1).
  branch-disp       displacement-only branch delta under a count delta
  schedule-disp     the two lines are each present in the OTHER stream:
                    the instruction moved, it did not change
  alignment         differing opcodes while the multisets are identical

CENSUS COLUMNS. Every summary row leads with two facts the structural
count cannot carry:
  T<n>/O<n> PARITY | COUNT<+-n>   instruction counts and whether they
        AGREE. This is the POSTPROCESSOR SCREEN: a count-asymmetric
        residual is provably outside every WebFrank/P6Frank class. It is
        NOT the same question as `unpaired` — 150 rows image-wide have
        unpaired > 0 with parity held (equal counts, different placement)
        and remain rule-eligible.
  slots aT/bO | slots=            exclusive r1 slot offsets on each side,
        i.e. whether this is a LAYOUT residual. A recolor and a frame
        residual are indistinguishable in the structural count; 11
        functions carry a slot delta under an IDENTICAL opcode multiset,
        3 of them with an outright frame-size delta. (Measured caveat:
        of 3032 census rows, ZERO had a slot delta while reading 0
        genuine — the GENUINE column has never given a false all-clear on
        a layout residual, so this column CLASSIFIES rows, it does not
        rescue missed ones.)

Offsets: @0xNN on paired/T rows is the TARGET function-relative byte
offset; on UNPAIRED-O rows it is OURS-relative. Both skip reloc
annotation lines. CLEAN-RENAMING is necessary, not sufficient, for a
recolor rule: webfrank_audit can still reject on a per-web
inconsistency the positional tally cannot see (live case: 0/0 verdict
with an f0/f2 FPR conflict) — always run the audit before authoring.
"""

import difflib
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fndiff  # noqa: E402
import slotdiff  # noqa: E402  (slot_map: the layout column)

VERSION = "GUNE5D"

POOL_RE = re.compile(r"@@?\d+")
NAMED_RE = re.compile(r"(lbl_|jumptable_)[0-9A-Fa-f]+")
SYMBOL_RE = re.compile(r"^(\S+)\s+(.*)$")


def aligned_pairs(target_lines, base_lines):
    """Align on register-erased text so renaming never breaks pairing."""
    t_norm = [fndiff.erase_registers(ln) for ln in target_lines]
    b_norm = [fndiff.erase_registers(ln) for ln in base_lines]
    matcher = difflib.SequenceMatcher(None, t_norm, b_norm, autojunk=False)
    pairs, t_only, b_only = [], [], []
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag == "equal":
            for k in range(i2 - i1):
                pairs.append((i1 + k, j1 + k))
        else:
            span = min(i2 - i1, j2 - j1)
            for k in range(span):
                pairs.append((i1 + k, j1 + k))
            t_only.extend(range(i1 + span, i2))
            b_only.extend(range(j1 + span, j2))
    return pairs, t_only, b_only


def register_tokens(line):
    return fndiff.REGISTER_RE.findall(line)


def byte_offsets(lines):
    """Instruction-relative byte offset per line index.

    Reloc annotation lines share their instruction's offset instead of
    inflating it — a worker pasted index*4 "offsets" into fnasm and
    landed in an unrelated region.
    """
    offsets, running = [], 0
    for line in lines:
        offsets.append(running)
        if not line.startswith("    "):
            running += 4
    return offsets


LABEL_RE = re.compile(r"^(?:lbl|jumptable)_([0-9A-Fa-f]{6,8})$")


def symbol_address(name):
    """Absolute address of a relocation target name, or None.

    Splitter-invented labels carry their address in the name; every other
    real symbol has one in symbols.txt.
    """
    if not name:
        return None
    match = LABEL_RE.match(name)
    if match:
        return int(match.group(1), 16)
    return fndiff.symbol_addresses().get(name)


_SYMBOL_LOCATIONS = {}


def symbol_locations(objfile):
    """{name: (section, offset)} and {(section, offset): {names}} for one object."""
    key = str(objfile)
    if key not in _SYMBOL_LOCATIONS:
        by_name, by_place = {}, {}
        out = subprocess.run([str(fndiff.OBJDUMP), "-t", str(objfile)],
                             capture_output=True, text=True).stdout
        for line in out.splitlines():
            fields = line.split()
            if len(fields) < 4 or not re.fullmatch(r"[0-9a-fA-F]+",
                                                   fields[0]):
                continue
            section, name = fields[-3], fields[-1]
            if section == "*UND*":
                continue
            place = (section, int(fields[0], 16))
            by_name[name] = place
            by_place.setdefault(place, set()).add(name)
        _SYMBOL_LOCATIONS[key] = (by_name, by_place)
    return _SYMBOL_LOCATIONS[key]


def split_reloc(text):
    """('R_PPC_ADDR16_HA', 'potionicon_tab', addend) from a reloc line."""
    match = SYMBOL_RE.match(text.strip().expandtabs())
    if not match:
        return None, None, 0
    reloc_type, symbol = match.group(1), match.group(2).strip()
    addend = 0
    addend_match = re.search(r"([+-])(0x[0-9a-fA-F]+|\d+)$", symbol)
    if addend_match:
        addend = int(addend_match.group(2), 0)
        if addend_match.group(1) == "-":
            addend = -addend
        symbol = symbol[:addend_match.start()]
    return reloc_type, symbol, addend


def make_object_resolver(target_obj, ours_obj):
    """Resolve a reloc symbol to the set of names denoting the same datum.

    This is the VALUE comparison: a symbol is reduced to the (section,
    offset) it points at IN ITS OWN OBJECT, then expanded to every name
    that object gives that location. Two rows naming one datum therefore
    share a name even when neither side spells it the same way.

    A symbol the object does not DEFINE (the dtk-extracted target object
    is text-only, so every data symbol in it is *UND*) has no location to
    expand, and its identity is simply its own name — hence the singleton
    fallback. The expansion therefore runs on whichever side actually
    defines the datum, which in practice is ours.
    """
    def resolve(side, symbol, addend):
        objfile = target_obj if side == "target" else ours_obj
        if not symbol:
            return frozenset(), frozenset()
        names = {symbol}
        try:
            by_name, by_place = symbol_locations(objfile)
            place = by_name.get(symbol)
            if place is not None:
                shifted = (place[0], place[1] + addend)
                names |= set(by_place.get(shifted, set()))
        except Exception:
            pass
        # Address identity is the other half of "compare VALUES, not
        # names", and the half that catches the target's splitter-invented
        # lbl_80275534 against our real `got_it`: the label CARRIES its
        # address, and symbols.txt gives the address of every real name.
        # fndiff.relocation_signature already settles the non-pool case
        # this way ("Compare addresses, not names").
        addresses = set()
        base = symbol_address(symbol)
        if base is not None:
            addresses.add(base + addend)
        for name in names:
            if name == symbol:
                continue
            other = symbol_address(name)
            if other is not None:
                # expanded names already sit AT the shifted location
                addresses.add(other)
        return frozenset(names), frozenset(addresses)
    return resolve


class Row:
    __slots__ = ("kind", "artifact", "offset", "target", "ours", "note")

    def __init__(self, kind, offset, target, ours, artifact=None, note=""):
        self.kind = kind
        self.artifact = artifact
        self.offset = offset
        self.target = target
        self.ours = ours
        self.note = note


class Result:
    def __init__(self):
        self.rows = []
        self.mapping = {}
        self.paired = 0
        self.t_only = []
        self.b_only = []
        # Layout/parity columns (run-31 item 5). Instruction counts and the
        # r1 slot maps of both sides, so a census row can say WHICH KIND of
        # residual it is instead of only how big it is.
        self.t_insns = 0
        self.o_insns = 0
        self.t_slots = {}
        self.o_slots = {}

    @property
    def count_delta(self):
        """ours minus target instruction count."""
        return self.o_insns - self.t_insns

    @property
    def count_parity(self):
        """Equal instruction counts.

        THE POSTPROCESSOR SCREEN: a count-asymmetric residual is provably
        outside every WebFrank/P6Frank class, and `unpaired` does not
        answer this — 150 census rows image-wide carry unpaired > 0 with
        parity HELD (equal counts, different placement), which reads like
        a count problem while the function is still rule-eligible.
        """
        return self.t_insns == self.o_insns

    @property
    def slot_exclusive(self):
        """(target-only, ours-only) r1 slot offsets."""
        return (sorted(set(self.t_slots) - set(self.o_slots)),
                sorted(set(self.o_slots) - set(self.t_slots)))

    @property
    def slot_use_deltas(self):
        """Shared slots whose USE COUNT differs."""
        return sorted(off for off in set(self.t_slots) & set(self.o_slots)
                      if self.t_slots[off] != self.o_slots[off])

    @property
    def renaming(self):
        return [r for r in self.rows if r.kind == "RENAMING"]

    @property
    def structural(self):
        return [r for r in self.rows if r.kind == "STRUCTURAL"]

    @property
    def artifacts(self):
        return [r for r in self.structural if r.artifact]

    @property
    def genuine(self):
        """Structural rows with no artifact explanation — the real work."""
        return [r for r in self.structural if not r.artifact]

    @property
    def unpaired(self):
        return len(self.t_only) + len(self.b_only)

    @property
    def displaced_unpaired(self):
        """Unpaired rows whose instruction simply moved within the stream."""
        return [r for r in self.rows
                if r.kind.startswith("UNPAIRED") and r.artifact]

    @property
    def verdict(self):
        if not self.structural and not self.unpaired:
            return "EXACT" if not self.renaming else "CLEAN-RENAMING"
        if self.structural:
            return "STRUCTURAL-PRESENT"
        return "COUNT-MISMATCH"

    def artifact_counts(self):
        """Every annotated row, structural and unpaired alike."""
        return Counter(r.artifact for r in self.rows if r.artifact)


def analyze(target_lines, ours_lines, resolver=None):
    """Classify every aligned row. Pure over the two line lists.

    ``resolver(side, symbol, addend) -> frozenset(names)`` supplies the
    reloc VALUE comparison; without one, only name-shaped artifacts are
    detected. Kept injectable so the annotator canary can drive every
    class without an object on disk.
    """
    result = Result()
    pairs, t_only, b_only = aligned_pairs(target_lines, ours_lines)
    result.paired = len(pairs)
    result.t_only, result.b_only = t_only, b_only
    t_off, b_off = byte_offsets(target_lines), byte_offsets(ours_lines)

    t_opcodes = Counter(fndiff.opcodes(target_lines))
    b_opcodes = Counter(fndiff.opcodes(ours_lines))
    multisets_identical = t_opcodes == b_opcodes
    count_delta = bool(t_only or b_only)
    # For the schedule-displacement annotator: an instruction that also
    # occurs in the OTHER stream moved, it did not change.
    t_instructions = Counter(fndiff.instruction_lines(target_lines))
    b_instructions = Counter(fndiff.instruction_lines(ours_lines))
    result.t_insns = sum(t_instructions.values())
    result.o_insns = sum(b_instructions.values())
    result.t_slots = slotdiff.slot_map(target_lines)
    result.o_slots = slotdiff.slot_map(ours_lines)

    for ti, bi in pairs:
        t_line, b_line = target_lines[ti], ours_lines[bi]
        offset = t_off[ti]
        if t_line == b_line:
            result.rows.append(Row("SAME", offset, t_line, b_line))
            continue
        t_is_reloc = t_line.startswith("    ")
        b_is_reloc = b_line.startswith("    ")
        if t_is_reloc and b_is_reloc:
            row = classify_reloc_pair(offset, t_line, b_line, resolver)
            result.rows.append(row)
            continue
        t_op = t_line.split()[0] if not t_is_reloc else ""
        b_op = b_line.split()[0] if not b_is_reloc else ""
        if t_op != b_op and multisets_identical:
            # Fabricated-row guard: when the opcode MULTISETS are
            # identical, a paired row with differing opcodes is
            # arithmetically an alignment artifact, not a real opcode
            # change — a whole record batch inherited such rows as
            # "codegen-form gaps".
            result.rows.append(Row(
                "STRUCTURAL", offset, t_line, b_line, "alignment",
                "multisets identical — confirm against the aligned"
                " fnasm --diff before believing"))
            continue
        t_stripbr = re.sub(r"^(b[a-z+.]*)\s+\S+", r"\1", t_line)
        b_stripbr = re.sub(r"^(b[a-z+.]*)\s+\S+", r"\1", b_line)
        if (t_op == b_op and t_op.startswith("b") and t_op != "bl"
                and count_delta and t_stripbr == b_stripbr):
            # Branch differing ONLY in its displacement, in a function
            # with a count delta: the shifted code moves every branch
            # target, so a 1-insn delta manufactures a flood of these
            # artifact rows that swamps the genuine signal (a lane
            # measured 19 artifacts hiding 1 real row).
            result.rows.append(Row(
                "STRUCTURAL", offset, t_line, b_line, "branch-disp",
                "displacement-only + count delta -> alignment artifact,"
                " usually not real"))
            continue
        if fndiff.erase_registers(t_line) == fndiff.erase_registers(b_line):
            result.rows.append(Row("RENAMING", offset, t_line, b_line))
            for ours_reg, tgt_reg in zip(register_tokens(b_line),
                                         register_tokens(t_line)):
                result.mapping.setdefault(ours_reg, {}).setdefault(tgt_reg, 0)
                result.mapping[ours_reg][tgt_reg] += 1
            continue
        if (not t_is_reloc and not b_is_reloc
                and b_instructions.get(t_line) and t_instructions.get(b_line)):
            # Each line is present in the other stream: this pair is one
            # window of a REORDER. DrawPsysSub's whole residual reads as
            # structural rows this way while being a pure schedule move
            # (attempt.MB_drawpsyssub-merged-disjunction-and-fifo-base-
            # schedule-cap.20260901.v1).
            result.rows.append(Row(
                "STRUCTURAL", offset, t_line, b_line, "schedule-disp",
                "both lines occur in the other stream -> displacement,"
                " not a changed instruction"))
            continue
        result.rows.append(Row("STRUCTURAL", offset, t_line, b_line))

    # A clean reorder does not produce mismatched PAIRS at all — the
    # aligner matches the common subsequence and drops the moved
    # instruction out as unpaired on both sides. The displacement
    # annotation has to reach those rows too, or the commonest shape of
    # the very class it was built for goes unannotated.
    for ti in t_only:
        line = target_lines[ti]
        moved = (not line.startswith("    ")
                 and bool(b_instructions.get(line)))
        result.rows.append(Row(
            "UNPAIRED-T", t_off[ti], line, None,
            "schedule-disp" if moved else None,
            "this exact instruction is present in OUR stream elsewhere"
            " -> displacement" if moved else ""))
    for bi in b_only:
        line = ours_lines[bi]
        moved = (not line.startswith("    ")
                 and bool(t_instructions.get(line)))
        result.rows.append(Row(
            "UNPAIRED-O", b_off[bi], None, line,
            "schedule-disp" if moved else None,
            "this exact instruction is present in the TARGET stream"
            " elsewhere -> displacement" if moved else ""))
    return result


def classify_reloc_pair(offset, t_line, b_line, resolver):
    """One aligned pair of relocation annotation lines."""
    t_txt, b_txt = t_line.strip(), b_line.strip()
    # A relocation annotation differing only in spelling (pool @N vs
    # lbl_, alias vs resolved address) rides on a register-only change
    # and is NOT structural — the first regnorm banner falsely failed an
    # actually-clean function on exactly this
    # (claim.law.regnorm-counts-reloc-annotation-rows-as-structural).
    if (fndiff.relocation_signature(t_txt)
            == fndiff.relocation_signature(b_txt)):
        return Row("RELOC-SAME", offset, t_line, b_line)
    t_type, t_sym, t_add = split_reloc(t_txt)
    b_type, b_sym, b_add = split_reloc(b_txt)
    if resolver is not None and t_type == b_type and t_type is not None:
        # THE VALUE COMPARISON. Resolve each symbol both to the set of
        # names denoting its location and to the absolute address it
        # points at; either an agreeing name or an agreeing address
        # proves one datum under two spellings.
        t_names, t_addrs = resolver("target", t_sym, t_add)
        b_names, b_addrs = resolver("ours", b_sym, b_add)
        shared_names = t_names & b_names
        shared_addrs = t_addrs & b_addrs
        if shared_names or shared_addrs:
            if shared_names:
                why = (f"both denote {sorted(shared_names)[0]} — same"
                       " (section, offset) in each object")
            else:
                why = (f"both resolve to {sorted(shared_addrs)[0]:#010x}"
                       " — same address, different spelling")
            return Row("STRUCTURAL", offset, t_line, b_line, "reloc-value",
                       why + "; naming only")
    if ((POOL_RE.search(t_txt) and NAMED_RE.search(b_txt))
            or (NAMED_RE.search(t_txt) and POOL_RE.search(b_txt))):
        # @NNNN on one side vs a splitter-invented lbl_/jumptable_ name
        # on the other is usually the SAME constant spelled two ways (a
        # lane hand-classified 4 of 5 "genuine" rows as exactly this).
        return Row(
            "STRUCTURAL", offset, t_line, b_line, "reloc-naming",
            "pool-vs-named spelling — likely the same constant; verify"
            " once per TU")
    return Row("STRUCTURAL", offset, t_line, b_line, None,
               "reloc target differs")


def summary_line(name, result):
    art = result.artifact_counts()
    bits = [f"{n} {kind}" for kind, n in sorted(art.items())]
    note = (f" ({' + '.join(bits)} artifacts — read the GENUINE rows"
            " first)" if bits else "")
    displaced = len(result.displaced_unpaired)
    moved = f" ({displaced} displaced)" if displaced else ""
    # COUNT PARITY: the postprocessor screen. `unpaired` alone cannot
    # answer it — 150 rows image-wide are unpaired-but-parity-held, which
    # is still rule-eligible, and reading unpaired as a count problem
    # queues them wrongly.
    counts = f"T{result.t_insns}/O{result.o_insns}"
    parity = ("PARITY" if result.count_parity
              else f"COUNT{result.count_delta:+d}")
    # SLOT DELTA: which KIND of residual this row is. A recolor and a
    # frame-layout residual look the same in the structural count.
    only_t, only_o = result.slot_exclusive
    if only_t or only_o:
        slots = f"slots {len(only_t)}T/{len(only_o)}O"
    elif result.slot_use_deltas:
        slots = f"slots= ({len(result.slot_use_deltas)} use-count)"
    else:
        slots = "slots="
    return (f"== {name}: {counts} {parity}, {slots},"
            f" {result.paired} paired,"
            f" {len(result.renaming)} renaming,"
            f" {len(result.structural)} STRUCTURAL"
            f" ({len(result.genuine)} genuine){note},"
            f" {result.unpaired} unpaired{moved} -> {result.verdict}")


def print_rows(result, show_all):
    for row in result.rows:
        if row.kind == "SAME":
            if show_all:
                print(f"SAME       {row.ours}")
            continue
        if row.kind == "RELOC-SAME":
            if show_all:
                print(f"RELOC-SAME {row.ours.strip()}")
            continue
        if row.kind == "RENAMING":
            print(f"RENAMING   @{row.offset:#06x}  T {row.target}"
                  f"   O {row.ours}")
            continue
        if row.kind.startswith("UNPAIRED"):
            text = row.target if row.kind == "UNPAIRED-T" else row.ours
            label = (row.kind.replace("UNPAIRED", "UNPAIRED-MOVED")
                     if row.artifact else row.kind)
            note = f"  [{row.note}]" if row.note else ""
            print(f"{label} @{row.offset:#06x}  {text}{note}")
            continue
        label = "STRUCTURAL"
        if row.artifact:
            label = {"reloc-naming": "STRUCT-RELOC-NAMING",
                     "reloc-value": "STRUCT-RELOC-VALUE",
                     "branch-disp": "STRUCT-BRANCH-DISP",
                     "schedule-disp": "STRUCT-SCHED-DISP",
                     "alignment": "STRUCT-ALIGNMENT"}[row.artifact]
        target = row.target.strip() if row.target.startswith("    ") \
            else row.target
        ours = row.ours.strip() if row.ours.startswith("    ") else row.ours
        note = f"  [{row.note}]" if row.note else ""
        print(f"{label} @{row.offset:#06x}  T {target}   O {ours}{note}")


def resolve_name(table, name):
    # allow a dtk _80XXXXXX suffix on either side (suffix mismatches
    # made this tool unavailable for exactly its roster's functions)
    if name in table:
        return name
    for cand in table:
        if cand.startswith(name + "_80") or name.startswith(cand + "_80"):
            return cand
    return None


def load_tables(bare):
    target = fndiff.parse(Path(f"build/{VERSION}/obj/{bare}.o"))
    ours = fndiff.parse(Path(f"build/{VERSION}/src/{bare}.o"))
    resolver = make_object_resolver(
        Path(f"build/{VERSION}/obj/{bare}.o"),
        Path(f"build/{VERSION}/src/{bare}.o"))
    return target, ours, resolver


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    show_all = "--all" in sys.argv
    show_map = "--map" in sys.argv
    if len(args) not in (1, 2):
        print(__doc__)
        return 2
    if len(args) == 1:
        # TU census mode: one summary row per common function. Two runs
        # of briefs ordered a "regnorm census first" that the tool could
        # not do; workers substituted hand-rolled loops. Now in-process:
        # one objdump per object instead of one subprocess per function.
        bare = re.sub(r"\.(c|cpp)$", "",
                      args[0].replace("\\", "/").removeprefix("src/"))
        target, ours, resolver = load_tables(bare)
        common = sorted(set(target) & set(ours))
        print(f"-- regnorm census: {bare} ({len(common)} paired"
              " functions; rank by GENUINE structural rows, then"
              " unpaired — real inverts tractability) --")
        print("   columns: T<n>/O<n> = instruction counts; PARITY /"
              " COUNT<+-n> = the POSTPROCESSOR SCREEN (a count-asymmetric"
              " residual is outside every WebFrank/P6Frank class, and"
              " unpaired>0 does NOT imply a count delta); slots aT/bO ="
              " exclusive r1 slot offsets, i.e. a LAYOUT residual, which"
              " the structural count cannot distinguish from a recolor")
        for name in common:
            result = analyze(target[name], ours[name], resolver)
            print(summary_line(name, result))
        return 0

    unit, fn = args
    unit = unit.replace("\\", "/").removeprefix("src/")
    unit = re.sub(r"\.(c|cpp)$", "", unit)
    target, ours, resolver = load_tables(unit)
    fn_t, fn_o = resolve_name(target, fn), resolve_name(ours, fn)
    if fn_t is None or fn_o is None:
        print(f"missing: {fn} (target: {fn_t is not None},"
              f" ours: {fn_o is not None})")
        return 1
    result = analyze(target[fn_t], ours[fn_o], resolver)
    print_rows(result, show_all)
    if show_map and result.mapping:
        print("-- register map (ours -> target, positional tally) --")
        for ours_reg in sorted(result.mapping,
                               key=lambda r: (r[0], int(r[1:]))
                               if r[1:].isdigit() else (r, 0)):
            targets = result.mapping[ours_reg]
            flag = "  INCONSISTENT" if len(targets) > 1 else ""
            body = " ".join(f"{tr}x{n}" for tr, n in
                            sorted(targets.items(), key=lambda kv: -kv[1]))
            print(f"  {ours_reg} -> {body}{flag}")
    print(summary_line(fn, result))
    print("VERDICT (repeated):", result.verdict,
          "-- REGISTER_ONLY label is only honest at 0 STRUCTURAL,"
          " 0 unpaired")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
