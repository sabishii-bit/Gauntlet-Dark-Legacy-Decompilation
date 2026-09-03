#!/usr/bin/env python3
"""The callee-saved CORRESPONDENCE TABLE, both streams, zero builds.

WHY THIS EXISTS. `--ops` compares OPCODE MULTISETS, so a function whose two
streams differ only in WHICH callee-saved register holds which local reads as
"opcode multiset: IDENTICAL" and the residual looks like register noise. MV
spent three lanes on game/movie/movieplayer.cpp::fn_800D8BCC that way — a
count-EXACT 215/215 function with a stable 4-token multiset — and the answer
appeared the moment the two save-register assignments were written side by
side: the numbering is split by declared WIDTH ahead of declaration order
(claim.law.MV_callee-saved-numbering-has-a-width-class-ahead-of-declaration-
order.20260902.v1, extending claim.law.SP_initializer-presence-outranks-
declaration-position-in-callee-saved-numbering.20260901.v1).

MWCC hands callee-saved GPRs out DOWNWARD FROM r31, in an order decided
before the declaration sequence is walked. So the register a local lands in
is a readable fact about the compiler's grouping, and the target's
assignment is a readable specification of the declaration list that would
reproduce it. This prints both, aligned, with the permutation named.

    python tools/gdl/savedregs.py game/movie/movieplayer fn_800D8BCC
    python tools/gdl/savedregs.py game/game/player do_players --uses
    python tools/gdl/savedregs.py <unit> <fn> --per-web  # later live ranges
    python tools/gdl/savedregs.py <unit> <fn> --raw     # pre-webfrank body

WHAT ITS ALL-CLEAR MEANS, AND WHAT IT DOES NOT (run 41). The table compares
the FIRST definition of each CALLEE-SAVED register. It used to close a clean
comparison with "Whatever residual remains is NOT a save-register assignment
question" — a claim about the whole function drawn from that narrow
comparison. Three records took it as a premise while fn_800D8BCC's actual
residual was a VOLATILE colour cascade this tool never reads, beside later
webs in registers it compared only once. Every run now prints what was not
compared: the number of later definitions in each stream, the number of
differing rows touching no callee-saved register at all (or UNSCREENED when
the counts differ and rows cannot pair), and any later-web role mismatch.

`--per-web` IS THE LIFETIME VIEW (run 42). Its first form paired later
definitions by ORDINAL WITHIN A REGISTER, which is correct only while no
live range changes register — precisely the case the view exists for. On
fn_800D8BCC it aligned the target's `add r19,r12,r0` against our
`li r19,0`, two unrelated ranges, and printed four DIFFERENT ROLE rows.
Pairing by ALIGNED POSITION instead (the opcode-sequence alignment
`fnasm --diff` prints) reads the same function as: the target's r21 range
IS our r19 range — same role, same position, a PERMUTATION; and the
target's r19 loop pointer lives in VOLATILE r12 in ours — an ESCAPE the
callee-saved bank cannot see from the inside. Verdicts are `in place`,
`PERMUTED rA->rB`, `ESCAPED rA->rB` (ours failed to promote), `INTRUDER`
(ours over-promoted a volatile role), `UNPAIRED` and — since run 48 —
`UNRESOLVED`.

`UNRESOLVED` IS A REFUSAL, NOT A RESIDUAL CLASS (run-48 item 6). Pairing is
by ROLE, and a role is a short string: `li 0` is every zero-initialisation
in the function. Where the role's candidates sit in more than one register
and NONE of them sits at the aligned position, the old code returned the
nearest one AS A REGISTER MAPPING, and a confident wrong attribution of
exactly that kind carried a whole hypothesis. Measured live on
game/anim/action::DoPlayerAction: the target's `@0x88 li r19,0` has two
candidates, r24 and r18, BOTH 24 aligned rows away — the row now says so
instead of naming r24. Calibrated over 17,688 live-range rows: 98 rows in 52
functions (0.55%) refuse; 17,590 keep their verdict.

READING IT. The rows are ordered by FIRST DEFINITION in each stream, which
is the order the allocator handed the registers out. A row whose two
register numbers agree is a web already in the right place. A row where they
disagree is a PERMUTATION: the same role, a different register, and no
amount of within-class declaration reordering will fix it if the classes
themselves are wrong. The summary line names the mapping target->ours for
every disagreeing row.

WHAT IT IS NOT. This reads the SAVE-REGISTER assignment, not the stack
frame: for slot layout use `slotdiff.py`, which probe already auto-invokes
on a save-set delta. The two are complementary — slotdiff says the save SET
differs, this says what the saved registers are FOR.

Zero builds by construction: it decodes the objects already on disk through
fnasm.parse_fn. If an object is missing it says so rather than building one,
because a build here would silently rescore the thing you are reading.

IMPORTABLE CORE: parse_instruction, first_definitions, all_definitions,
lifetime_pairs, correspondence, permutation, format_table, in_window,
contenders, is_ambiguous, unresolved_note — all pure over
decoded row lists, no build and no printing, and importing this module has
no side effects. Call them in-process for a sweep instead of one subprocess
per function (run-43 item 10; the convention is documented in AGENTS.md).
"""

import re
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))

import fnasm  # noqa: E402

# PPC EABI: r14-r31 and f14-f31 are callee-saved. r13 is the small-data base
# and r1 the stack pointer -- neither is a local's home, and including them
# would put ABI plumbing in a table about locals.
GPR_SAVED = tuple(f"r{n}" for n in range(31, 13, -1))
FPR_SAVED = tuple(f"f{n}" for n in range(31, 13, -1))

# Mnemonics that do NOT define their first operand. Stores write memory;
# compares write a condition register; branches write nothing; every `mt*`
# form MOVES TO a special register and its first operand is a SOURCE
# (`mtctr r31`, `mtcrf 0x8,r30`) — without that entry a loop-count setup
# manufactures a phantom definition of a callee-saved register, which is
# the one place this tool reads a source as a destination.
NON_DEFINING = re.compile(
    r"^(?:st|b|cmp|tw|dcb|icb|sync|isync|eieio|nop|mt)")

# Register CLASSES. The correspondence table proper only reads the
# callee-saved bank, but a lifetime that MOVED between banks is invisible
# from inside it: fn_800D8BCC keeps a loop pointer in callee-saved r19 where
# ours keeps the same value in volatile r12, and no amount of callee-saved
# bookkeeping can see that. Pairing lifetimes needs the whole file.
CALLEE_SAVED = frozenset(
    [f"r{n}" for n in range(14, 32)] + [f"f{n}" for n in range(14, 32)])
ABI_FIXED = frozenset(("r1", "r2", "r13"))


def register_class(register):
    """"callee-saved" | "volatile" | "abi" | None for a non-register token."""
    if register in CALLEE_SAVED:
        return "callee-saved"
    if register in ABI_FIXED:
        return "abi"
    if re.fullmatch(r"[rf]\d+", register or ""):
        return "volatile"
    return None


INSN_RE = re.compile(r"^([a-z][a-z0-9._+-]*)\s+(.*)$")


def parse_instruction(text):
    """(mnemonic, [operands]) for a decoded row, or (None, []).

    The relocation suffix fnasm appends (`  @sym(TYPE)`) is stripped: it is
    not an operand and would otherwise ride along on the last one.
    """
    text = (text or "").split("  @")[0].strip()
    match = INSN_RE.match(text)
    if not match:
        return (text or None), []
    operands = [part.strip()
                for part in match.group(2).split(",") if part.strip()]
    return match.group(1), operands


def defines(mnemonic, operands, register):
    """Does this instruction WRITE `register`?

    PPC puts the destination first in every value-producing form
    (`addi rD,rA,K`, `lwz rD,K(rA)`, `mr rD,rS`, `lis rD,K`), so the test is
    "first operand is this register, and the mnemonic is one that defines".
    `lmw rD,K(r1)` is excluded with the other epilogue plumbing by the
    caller, not here.
    """
    if not mnemonic or not operands:
        return False
    if NON_DEFINING.match(mnemonic):
        return False
    return operands[0] == register


def frame_size(rows):
    """The frame this function opens, from `stwu r1,-N(r1)`, or None."""
    for _offset, text in rows or []:
        mnemonic, operands = parse_instruction(text)
        if mnemonic == "stwu" and len(operands) >= 2 and operands[0] == "r1":
            match = re.match(r"^(-?\d+)\(r1\)$", operands[1])
            if match:
                return -int(match.group(1))
    return None


def multiple_save_base(rows):
    """The lowest register in an `stmw rN,K(r1)`, or None.

    A function saving with stmw commits to a CONTIGUOUS run, so its save set
    is decided by one number and the table's job is to say which locals fill
    it.
    """
    for _offset, text in rows or []:
        mnemonic, operands = parse_instruction(text)
        if mnemonic == "stmw" and operands:
            match = re.match(r"^r(\d+)$", operands[0])
            if match:
                return int(match.group(1))
    return None


def first_definitions(rows, registers):
    """[(register, offset, text)] in FIRST-DEFINITION order.

    That order is the order the allocator handed the registers out, which is
    what makes two streams comparable row-by-row even when the register
    NUMBERS differ. Prologue `stmw`/`lmw` never define, so they cannot
    manufacture a row.
    """
    seen = {}
    for offset, text in rows or []:
        mnemonic, operands = parse_instruction(text)
        if mnemonic in ("lmw", "stmw"):
            continue
        for register in registers:
            if register in seen:
                continue
            if defines(mnemonic, operands, register):
                seen[register] = (offset, text)
    return sorted(((register, offset, text)
                   for register, (offset, text) in seen.items()),
                  key=lambda row: row[1])


def all_definitions(rows, registers):
    """{register: [(offset, text), ...]} — EVERY definition, in order.

    `first_definitions` keeps one row per register, which is the right unit
    for the assignment question and the wrong unit for the claim the table
    used to close with. One register hosts SEVERAL live ranges across a
    function, and two streams can agree on the first definition of all
    thirteen while disagreeing on later ones.
    """
    found = {register: [] for register in registers}
    for offset, text in rows or []:
        mnemonic, operands = parse_instruction(text)
        if mnemonic in ("lmw", "stmw"):
            continue
        for register in registers:
            if defines(mnemonic, operands, register):
                found[register].append((offset, text))
    return {register: rows_ for register, rows_ in found.items() if rows_}


def web_mismatches(target_rows, our_rows, registers):
    """[(register, ordinal, target_role, our_role)] beyond the FIRST def.

    Definitions of one register are paired by ORDINAL within that register,
    which is exact when both streams define it the same number of times and
    is reported as a count difference when they do not.
    """
    target_all = all_definitions(target_rows, registers)
    our_all = all_definitions(our_rows, registers)
    out, count_diffs = [], []
    for register in sorted(set(target_all) | set(our_all)):
        t_defs = target_all.get(register, [])
        o_defs = our_all.get(register, [])
        if len(t_defs) != len(o_defs):
            count_diffs.append((register, len(t_defs), len(o_defs)))
        for ordinal, (t_def, o_def) in enumerate(zip(t_defs, o_defs)):
            if ordinal == 0:
                continue          # the assignment table already owns this
            t_role, o_role = role(t_def[1]), role(o_def[1])
            if t_role != o_role:
                out.append((register, ordinal, t_role, o_role))
    return out, count_diffs


_ANY_SAVED_RE = re.compile(
    r"\b(?:r(?:1[4-9]|2\d|3[01])|f(?:1[4-9]|2\d|3[01]))\b")


def rows_outside_the_saved_bank(target_rows, our_rows):
    """(differing rows touching NO callee-saved register, comparable?).

    The other half of the false all-clear: this whole table says nothing
    about VOLATILE registers, and MV's fn_800D8BCC residual was a volatile
    colour cascade. When the two streams hold the same instruction count the
    rows pair by index, so the number is exact; otherwise it is not
    computable and must be reported as unknown rather than as zero.
    """
    if len(target_rows or []) != len(our_rows or []):
        return None, False
    count = 0
    for (_t_off, t_text), (_o_off, o_text) in zip(target_rows, our_rows):
        if t_text == o_text:
            continue
        if _ANY_SAVED_RE.search(t_text) or _ANY_SAVED_RE.search(o_text):
            continue
        count += 1
    return count, True


def use_counts(rows, registers):
    """{register: times it appears as any operand}."""
    counts = {register: 0 for register in registers}
    for _offset, text in rows or []:
        _mnemonic, operands = parse_instruction(text)
        joined = ",".join(operands)
        for register in registers:
            if re.search(rf"\b{register}\b", joined):
                counts[register] += 1
    return counts


def role(text):
    """A defining instruction's ROLE: what value the register receives.

    The destination is dropped (that is the question, not the answer) and
    the two MWCC spellings of a copy are folded together: `addi rD,rS,0` and
    `mr rD,rS` are the same role and differ only by which the scheduler
    emitted. Without that fold the target's `addi r31,r25,0` and our
    `mr r31,r25` read as different roles and the table calls a matching web
    permuted.
    """
    mnemonic, operands = parse_instruction(text)
    if mnemonic is None:
        return text or ""
    sources = operands[1:]
    if mnemonic == "addi" and len(sources) == 2 and sources[1] == "0":
        mnemonic, sources = "mr", sources[:1]
    return f"{mnemonic} {','.join(sources)}".strip()


def web_role(text):
    """`role()` with SELF-REFERENCES masked, so a role is register-free.

    `role()` drops the destination but leaves it in the sources, so an
    induction step reads as `addi r21,4` in one stream and `addi r19,4` in
    the other — different strings for the same live range, which is exactly
    the comparison a PERMUTED lifetime needs to survive. Masking the defined
    register to `%` makes the two roles equal and lets the permutation be
    named instead of reported as five unrelated role mismatches
    (measured on fn_800D8BCC: 4 of its 5 ordinal "DIFFERENT ROLE" rows).
    """
    mnemonic, operands = parse_instruction(text)
    if mnemonic is None or not operands:
        return role(text)
    destination = operands[0]
    folded = role(text)
    return re.sub(rf"\b{re.escape(destination)}\b", "%", folded)


def definition_target(text):
    """The register this row DEFINES, or None if it defines no register."""
    mnemonic, operands = parse_instruction(text)
    if mnemonic in ("lmw", "stmw"):
        return None
    if not mnemonic or not operands:
        return None
    if NON_DEFINING.match(mnemonic):
        return None
    return operands[0] if register_class(operands[0]) else None


def aligned_rows(target_rows, our_rows):
    """[(target_row|None, our_row|None)] on the opcode-sequence alignment.

    The same correspondence `fnasm --diff` and `fndiff --ops` print. Raw
    offset pairing is only correct when the two streams hold equal counts
    AND no insertion has drifted them; this is correct in both cases, and
    when the counts differ it degrades to one-sided rows instead of to a
    silently wrong pairing.
    """
    import difflib
    t_ops = [(row[1].split() or [""])[0] for row in target_rows or []]
    o_ops = [(row[1].split() or [""])[0] for row in our_rows or []]
    out = []
    for _tag, i1, i2, j1, j2 in difflib.SequenceMatcher(
            None, t_ops, o_ops, autojunk=False).get_opcodes():
        for k in range(max(i2 - i1, j2 - j1)):
            t_index = i1 + k if i1 + k < i2 else None
            o_index = j1 + k if j1 + k < j2 else None
            out.append((target_rows[t_index] if t_index is not None else None,
                        our_rows[o_index] if o_index is not None else None))
    return out


# How far, in ALIGNED ROWS, a definition may move and still be read as the
# same live range in the other stream. It bounds phase 2 only — phase 1
# (same register AND same role) is unwindowed, because that pairing needs no
# distance evidence.
#
# CALIBRATED, not chosen: swept 6/12/24/48/96 over the 267 nonmatching
# functions with both objects on disk (54 units). `in place` is completely
# window-invariant (3682 at every setting) and PERMUTED barely moves
# (1530 -> 1603 across a 16x sweep, 4.8%). ESCAPED/INTRUDER trade directly
# against UNPAIRED and ARE window-sensitive (escaped 189 -> 304, intruder
# 117 -> 235, unpaired 1265 -> 816), so read an escape/intrusion verdict
# with its printed row delta rather than as a bare count. 24 is the knee:
# past it the p90 reach runs away (8 rows at 24, 21 at 96) while the
# escape/intruder yield per row of window collapses.
LIFETIME_WINDOW = 24


def _definition_index(rows, target_rows, our_rows, ours):
    """[(aligned_position, register, row)] for every register definition.

    Positions come from the opcode-sequence alignment, so a target row and
    the our-row it aligns with share a position and a move in the schedule
    is a small position delta rather than an arbitrary byte-offset one.
    """
    positions = {}
    for position, (t_row, o_row) in enumerate(
            aligned_rows(target_rows, our_rows)):
        row = o_row if ours else t_row
        if row is not None:
            positions[id(row)] = position
    out = []
    for row in rows or []:
        register = definition_target(row[1])
        if register and register_class(register) != "abi":
            out.append((positions.get(id(row), 0), register, row))
    return out


def in_window(candidates, position, window=LIFETIME_WINDOW):
    """The candidates within `window` aligned rows of `position`."""
    return [c for c in candidates if abs(c[0] - position) <= window]


def contenders(near, position):
    """The candidates tied at the MINIMUM aligned distance from `position`."""
    if not near:
        return []
    best = min(abs(c[0] - position) for c in near)
    return [c for c in near if abs(c[0] - position) == best]


def is_ambiguous(near, position):
    """Do the CLOSEST candidates disagree about the ANSWER? (run-48 item 6)

    THE DEFECT. Phase 2 pairs a live range by ROLE and then takes the NEAREST
    candidate, and a role is a short string: `li 0` is the role of every
    zero-initialisation in the function. On game/movie/movieplayer::
    fn_800D8BCC the ordinal pairing put the target's `add r19,r12,r0` against
    our `li r19,0` — two unrelated ranges — and the run-42 rewrite fixed the
    PAIRING while leaving the tie-break: where several equally-plausible
    candidates sit inside the window, the nearest one was reported as a
    verdict carrying a register mapping, and NM fitted a whole hypothesis to
    one such attribution.

    THE DISCRIMINANT IS DISTANCE ZERO, and it was CHOSEN FROM THREE
    MEASURED CANDIDATES rather than reasoned to. Distance 0 means the two
    definitions sit at the SAME position in the opcode-sequence alignment —
    the alignment's own answer, not a proximity ranking. So a UNIQUE
    candidate at distance 0 decides the row; with none (or with two at zero
    in different registers), any register disagreement inside the window is
    a choice made by proximity alone, and that is a guess.

    THE NEGATIVE SIDE PICKED THIS. Swept over every live-range row in the
    tree (17,688 rows, 2,998 function pairs) at bb44ef4ab:
      A  tie-at-the-minimum only        3 rows / 3 fns (0.02%) — too narrow;
                                        an exact numeric tie is an accident,
                                        and 95 pure-proximity mappings
                                        survive it
      B  distance-0-decisive (SHIPPED) 98 rows / 52 fns (0.55%) — PERMUTED
                                        1365->1327, ESCAPED 228->195,
                                        INTRUDER 158->134
      C  any window disagreement      389 rows / 115 fns (2.20%) — REFUTED
                                        by the corpus: it turns all four of
                                        the rows the accepted law
                                        claim.law.T12_pairing-callee-saved-
                                        definitions-by-ordinal-within-a-
                                        register-fails-exactly-when-a-
                                        lifetime-moves.20260903.v1 names in
                                        its expiry check (`--per-web` must
                                        keep naming r21->r19 x2 and
                                        r19->r12 x2) into refusals, and
                                        `fnasm game/movie/movieplayer
                                        fn_800D8BCC 0x1c0:0x260 --diff`
                                        shows all four are CORRECT (target
                                        `1c4 li r21,0` against our `1c4 li
                                        r19,0`, and the whole r21 range at
                                        `22c`/`23c`/`240` against our r19
                                        range at the same offsets). Each has
                                        a UNIQUE candidate at distance 0 with
                                        its alternatives 4, 1, 22 and 1 rows
                                        further out.
    A and B both keep that law's four rows; C refuses measurements, which is
    the opposite failure to the one this item is about.
    """
    at_zero = {candidate[1] for candidate in near
               if candidate[0] == position}
    if len(at_zero) == 1:
        return False
    return len({candidate[1] for candidate in near}) > 1


def unresolved_note(anchor, near):
    """Why a row is UNRESOLVED, naming every candidate and its distance."""
    tied = contenders(near, anchor[0])
    distance = abs(tied[0][0] - anchor[0]) if tied else 0
    options = ", ".join(
        f"{candidate[1]} ({abs(candidate[0] - anchor[0])} aligned rows away)"
        for candidate in sorted(near, key=lambda c: abs(c[0] - anchor[0])))
    zero = {c[1] for c in near if c[0] == anchor[0]}
    why = ("no candidate sits at the aligned position at all, so only"
           f" proximity separates them (nearest is {distance} row(s) away)"
           if not zero else
           f"{len(zero)} candidates share the aligned position, in different"
           " registers")
    return (f"role `{web_role(anchor[2][1])}` matches {len(near)} candidate"
            f" definition(s) in {len({c[1] for c in near})} different"
            f" registers, and {why} — {options}. A choice among these would"
            " be a proximity GUESS, not a measurement, so no mapping is"
            " claimed here. Decide it from the aligned view"
            " (`fnasm <unit> <fn> 0xA:0xB --diff`) before quoting a"
            " permutation.")


def lifetime_pairs(target_rows, our_rows):
    """[(label, target_row|None, our_row|None, verdict, note)] per LIVE RANGE.

    A callee-saved register hosts several live ranges, and the run-41 item
    is that the per-web view paired them by ORDINAL WITHIN A REGISTER — a
    pairing correct only while no lifetime changes register. On fn_800D8BCC
    it aligned the target's `add r19,r12,r0` against our `li r19,0` (two
    unrelated ranges), printed four DIFFERENT ROLE rows, and hid the actual
    finding: the target's r21 range IS our r19 range with an identical role,
    and the target's r19 range lives in volatile r12 in ours.

    Pairing is ROLE-FIRST, in three phases, because pairing on position
    alone re-reports a reordered prologue as a permutation (measured: naive
    position pairing called 3 of fn_800D8BCC's 5 "permutations" on rows the
    first-definition table correctly reads as an emission-order difference):

      1. same register AND same role, nearest in position -> `in place`
         (a schedule move is not a permutation)
      2. same role, any register, within LIFETIME_WINDOW aligned rows ->
         PERMUTED (callee-saved to callee-saved) / ESCAPED (to a volatile) /
         INTRUDER (ours promotes a role the target keeps volatile)
      3. leftovers: same register with a different role -> DIFFERENT ROLE;
         otherwise the aligned counterpart decides ESCAPED vs UNPAIRED.
    """
    t_defs = _definition_index(target_rows, target_rows, our_rows, False)
    o_defs = _definition_index(our_rows, target_rows, our_rows, True)
    aligned_partner = {}
    for t_row, o_row in aligned_rows(target_rows, our_rows):
        if t_row is not None and o_row is not None:
            aligned_partner[id(t_row)] = o_row
            aligned_partner[id(o_row)] = t_row
    t_saved = [d for d in t_defs if d[1] in CALLEE_SAVED]
    o_saved = [d for d in o_defs if d[1] in CALLEE_SAVED]
    used_t, used_o, out = set(), set(), []

    def take(t_def, o_def, verdict, note, anchor_ours=False):
        # The row LABEL names the callee-saved range being discussed. For an
        # INTRUDER that range is OURS (the target side is the volatile the
        # role legitimately lives in), so the label must follow it or the
        # row reads as a statement about a volatile register.
        if t_def is not None:
            used_t.add(id(t_def[2]))
        if o_def is not None:
            used_o.add(id(o_def[2]))
        anchor = o_def if (anchor_ours or t_def is None) else t_def
        if (t_def is not None and o_def is not None
                and verdict not in ("in place", "DIFFERENT ROLE",
                                    "UNRESOLVED")):
            # The window sweep says ESCAPED/INTRUDER counts depend on how far
            # the pairing is allowed to reach, so every such row carries its
            # reach and the reader can discount a distant one.
            note = (f"{note}; {abs(t_def[0] - o_def[0])} aligned rows apart"
                    if note else
                    f"{abs(t_def[0] - o_def[0])} aligned rows apart")
        out.append((t_def[0] if t_def else o_def[0],
                    anchor[1], anchor_ours or t_def is None,
                    t_def[2] if t_def else None,
                    o_def[2] if o_def else None, verdict, note))

    def nearest(candidates, position):
        free = [c for c in candidates if id(c[2]) not in used_o]
        return min(free, key=lambda c: abs(c[0] - position)) if free else None

    # Phase 1 — same register, same role. A move in the schedule is not a
    # permutation, and the first-definition table above already says so.
    for t_def in t_saved:
        match = nearest([o for o in o_saved
                         if o[1] == t_def[1]
                         and web_role(o[2][1]) == web_role(t_def[2][1])],
                        t_def[0])
        if match:
            take(t_def, match, "in place", "")

    # Phase 2 — same role, different register, near in position.
    for t_def in t_saved:
        if id(t_def[2]) in used_t:
            continue
        free = [o for o in o_defs if id(o[2]) not in used_o
                and web_role(o[2][1]) == web_role(t_def[2][1])]
        near = in_window(free, t_def[0])
        if not near:
            continue
        match = min(near, key=lambda c: abs(c[0] - t_def[0]))
        if is_ambiguous(near, t_def[0]):
            take(t_def, match, "UNRESOLVED", unresolved_note(t_def, near))
            continue
        if match[1] in CALLEE_SAVED:
            take(t_def, match, f"PERMUTED {t_def[1]}->{match[1]}",
                 "same role, a different callee-saved register")
        else:
            take(t_def, match, f"ESCAPED {t_def[1]}->{match[1]}",
                 "same role, ours keeps it in a VOLATILE")
    for o_def in o_saved:
        if id(o_def[2]) in used_o:
            continue
        free = [t for t in t_defs
                if id(t[2]) not in used_t
                and web_role(t[2][1]) == web_role(o_def[2][1])]
        near = in_window(free, o_def[0])
        if not near:
            continue
        match = min(near, key=lambda c: abs(c[0] - o_def[0]))
        if is_ambiguous(near, o_def[0]):
            take(match, o_def, "UNRESOLVED", unresolved_note(o_def, near),
                 anchor_ours=True)
            continue
        take(match, o_def, f"INTRUDER {o_def[1]} (target uses {match[1]})",
             "same role, ours promotes a range the target keeps volatile",
             anchor_ours=True)

    # Phase 3 — leftovers. Same register with a different value is a real
    # role mismatch; otherwise the aligned counterpart says whether the
    # range escaped to a volatile or has no counterpart at all.
    for t_def in t_saved:
        if id(t_def[2]) in used_t:
            continue
        match = nearest([o for o in o_saved if o[1] == t_def[1]], t_def[0])
        if match is not None and abs(match[0] - t_def[0]) <= LIFETIME_WINDOW:
            take(t_def, match, "DIFFERENT ROLE",
                 f"target `{role(t_def[2][1])}`"
                 f" vs ours `{role(match[2][1])}`")
            continue
        partner = aligned_partner.get(id(t_def[2]))
        partner_reg = definition_target(partner[1]) if partner else None
        if partner_reg and register_class(partner_reg) == "volatile":
            take(t_def, (t_def[0], partner_reg, partner),
                 f"ESCAPED {t_def[1]}->{partner_reg}",
                 "ours keeps this position in a VOLATILE (role differs)")
        else:
            take(t_def, None, "UNPAIRED",
                 f"target holds `{role(t_def[2][1])}`; ours has no matching"
                 " definition")
    for o_def in o_saved:
        if id(o_def[2]) in used_o:
            continue
        take(None, o_def, "UNPAIRED",
             f"ours holds `{role(o_def[2][1])}`; the target has no matching"
             " definition")

    out.sort(key=lambda row: (row[0], row[1]))
    ordinal_seen, labelled = {}, []
    for position, register, ours_only, t_row, o_row, verdict, note in out:
        side = "ours " if ours_only else ""
        ordinal = ordinal_seen.get((side, register), 0)
        ordinal_seen[(side, register)] = ordinal + 1
        labelled.append((f"{side}{register}[{ordinal}]", t_row, o_row,
                         verdict, note))
    return labelled


def lifetime_summary(pairs):
    """{verdict-class: count} plus the permutation/escape mappings."""
    counts = {"in place": 0, "permuted": 0, "escaped": 0,
              "intruder": 0, "unpaired": 0, "different role": 0,
              "unresolved": 0}
    maps = {"permuted": {}, "escaped": {}, "intruder": {}}
    for _label, _t, _o, verdict, _note in pairs:
        head = verdict.split()[0]
        if verdict == "in place":
            counts["in place"] += 1
        elif head == "UNRESOLVED":
            counts["unresolved"] += 1
        elif head == "DIFFERENT":
            counts["different role"] += 1
        elif head in ("PERMUTED", "ESCAPED"):
            key = "permuted" if head == "PERMUTED" else "escaped"
            counts[key] += 1
            maps[key][verdict.split()[1]] = maps[key].get(
                verdict.split()[1], 0) + 1
        elif head == "INTRUDER":
            counts["intruder"] += 1
            maps["intruder"][verdict.split()[1]] = maps["intruder"].get(
                verdict.split()[1], 0) + 1
        else:
            counts["unpaired"] += 1
    return counts, maps


def correspondence(target_defs, our_defs):
    """[(register, target_row_or_None, our_row_or_None)], r31 downward.

    PAIRED BY REGISTER NUMBER, deliberately. The first design paired by
    first-definition ORDER and was refuted the first time it was run on the
    function the law came from: fn_800D8BCC's assignment already MATCHES the
    target (r31 the r25 copy, r30 the slwi, r29 the live zero, r28/r27/r26
    the three byte locals) and only the EMISSION ORDER of those definitions
    differs, so order-pairing reported six permuted webs where there are
    none. Assignment and schedule are different questions; this table
    answers the assignment one and reports order separately.
    """
    by_register = {}
    for row in target_defs:
        by_register.setdefault(row[0], [None, None])[0] = row
    for row in our_defs:
        by_register.setdefault(row[0], [None, None])[1] = row
    return [(register, pair[0], pair[1])
            for register, pair in sorted(
                by_register.items(),
                key=lambda kv: (kv[0][0], -int(kv[0][1:])))]


def permutation(pairs):
    """{register: (target_role, our_role)} for webs holding different values.

    This is the width-class signal: the same register carrying a different
    ROLE in the two streams means the allocator grouped the locals
    differently, which is a declaration-list fact.
    """
    moved = {}
    for register, target_row, our_row in pairs:
        if not target_row or not our_row:
            continue
        target_role, our_role = role(target_row[2]), role(our_row[2])
        if target_role != our_role:
            moved[register] = (target_role, our_role)
    return moved


def emission_order(defs):
    """The registers in first-definition order — the schedule, not the map."""
    return [row[0] for row in defs]


def scope_banner(target_rows, our_rows, registers, lpairs,
                 count_diffs):
    """The lines that say what this table did NOT compare.

    Run-41 item 5. The closing line used to read "Whatever residual remains
    is NOT a save-register assignment question", which is a claim about the
    whole function made from a comparison of the FIRST definition of each
    CALLEE-SAVED register. Three records took that all-clear as a premise
    (attempt.MV_fn800d8bcc-duplicated-branch-locals-belong-to-the-common-
    block.20260903.v1 and its two predecessors), and fn_800D8BCC's actual
    residual was a VOLATILE colour cascade — a class this table never looks
    at — beside later webs it never compared.
    """
    lines = []
    target_all = all_definitions(target_rows, registers)
    our_all = all_definitions(our_rows, registers)
    later = sum(max(0, len(rows_) - 1) for rows_ in target_all.values())
    ours_later = sum(max(0, len(rows_) - 1) for rows_ in our_all.values())
    outside, comparable = rows_outside_the_saved_bank(target_rows, our_rows)
    lines.append(
        f"  SCOPE OF THIS TABLE: the FIRST definition of each callee-saved"
        f" register. Not compared above: {later} later definition(s) in the"
        f" target and {ours_later} in ours (one register hosts several live"
        " ranges), and every VOLATILE register, which this tool never reads."
        " `--per-web` compares the later definitions.")
    if comparable:
        lines.append(
            f"  ROWS THIS TABLE CANNOT SEE: {outside} differing"
            " instruction row(s) at aligned offsets touch NO callee-saved"
            " register at all"
            + (" — a volatile-register residual, which is what three"
               " records mistook this table's all-clear for."
               if outside else "."))
    else:
        lines.append(
            "  ROWS THIS TABLE CANNOT SEE: not computable — the two streams"
            " hold different instruction counts, so rows do not pair by"
            " offset. Treat the volatile-register question as UNSCREENED,"
            " not as clean.")
    counts, maps = lifetime_summary(lpairs)
    later_role = sum(1 for label, _t, _o, verdict, _n in lpairs
                     if verdict.startswith("DIFFERENT ROLE")
                     and not label.endswith("[0]"))
    if later_role:
        lines.append(
            f"  LATER-WEB MISMATCH: {later_role} definition(s)"
            " beyond the first hold a different value in the SAME register"
            " in the two streams (`--per-web` lists them). The assignment"
            " verdict above does NOT cover these.")
    for key, headline in (
            ("permuted", "LIFETIME PERMUTATION"),
            ("escaped", "LIFETIME ESCAPED TO A VOLATILE"),
            ("intruder", "VOLATILE ROLE PROMOTED IN OURS")):
        if not counts[key]:
            continue
        mapping = ", ".join(
            f"{name}{'' if n == 1 else f' x{n}'}"
            for name, n in sorted(maps[key].items()))
        lines.append(
            f"  {headline}: {counts[key]} live range(s) — {mapping}."
            + (" Same role, different callee-saved register: a"
               " declaration-list fact, not a schedule one."
               if key == "permuted" else
               " The target keeps this value in a callee-saved register"
               " across a call or a loop and ours does not — a LIFETIME"
               " question, invisible to the first-definition table above."
               if key == "escaped" else
               " Ours spends a callee-saved register where the target uses"
               " a volatile — the save set is paying for a range that does"
               " not need it."))
    if counts["unresolved"]:
        # RUN-48 ITEM 6. These rows used to carry a register mapping chosen
        # by proximity among several equally-plausible candidates sharing one
        # short role string (`li 0` is every zero-initialisation in the
        # function). A confident wrong attribution of exactly that kind
        # carried a whole hypothesis, so the tie is now REPORTED rather than
        # broken.
        lines.append(
            f"  UNRESOLVED LIVE RANGE(S): {counts['unresolved']} — the role"
            " matches several candidate definitions in DIFFERENT registers"
            " inside the pairing window, so no mapping is claimed. These are"
            " NOT clean rows and NOT permutations; each names its candidates"
            " with their distances, and the aligned view"
            " (`fnasm <unit> <fn> 0xA:0xB --diff`) decides them. Never quote"
            " a permutation from one of these.")
    if counts["unpaired"]:
        lines.append(
            f"  UNPAIRED LIVE RANGE(S): {counts['unpaired']} — one stream"
            " defines a callee-saved register at an aligned position where"
            " the other defines nothing at all.")
    if count_diffs:
        lines.append(
            "  DEFINITION-COUNT DIFFERS on "
            + ", ".join(f"{register} ({t} vs {o})"
                        for register, t, o in count_diffs)
            + " — one stream redefines the register more often, so the"
              " later webs do not pair one-to-one.")
    return lines


def format_per_web(target_rows, our_rows, registers, lpairs=None):
    """Every LIVE RANGE, paired across the streams by aligned position.

    Not by ordinal within a register: that pairing is only correct while no
    lifetime changes register, and the case the view exists for is exactly
    the case where one does.
    """
    if lpairs is None:
        lpairs = lifetime_pairs(target_rows, our_rows)
    lines = ["  PER-WEB: every live range (one row per DEFINITION), paired"
             " across the streams by ALIGNED POSITION, not by ordinal within"
             " a register — a range that changed register still pairs",
             f"  {'RANGE':<11}{'TARGET@':>7}  {'definition':<30}"
             f"  {'OURS@':>7}  {'definition':<30}  VERDICT"]

    def cell(row):
        if row is None:
            return f"{'--':>7}  {'(nothing at this position)':<30}"
        offset, text = row
        shown = text if len(text) <= 30 else text[:27] + "..."
        return f"{'@0x%x' % offset:>7}  {shown:<30}"

    for label, target_row, our_row, verdict, note in lpairs:
        suffix = f"  <= {verdict}" if verdict != "in place" else "  in place"
        if note:
            suffix += f" ({note})"
        lines.append(f"  {label:<11}{cell(target_row)}"
                     f"  {cell(our_row)}{suffix}")
    counts, _maps = lifetime_summary(lpairs)
    lines.append(
        "  LIFETIME TOTALS: "
        + ", ".join(f"{n} {name}" for name, n in counts.items() if n))
    return lines


def format_table(unit, fn, target_rows, our_rows, show_uses=False,
                 per_web=False):
    registers = GPR_SAVED + FPR_SAVED
    target_defs = first_definitions(target_rows, registers)
    our_defs = first_definitions(our_rows, registers)
    pairs = correspondence(target_defs, our_defs)
    moved = permutation(pairs)

    target_frame, our_frame = frame_size(target_rows), frame_size(our_rows)
    target_stmw, our_stmw = (multiple_save_base(target_rows),
                             multiple_save_base(our_rows))

    lines = [f"== {unit}::{fn} callee-saved correspondence"
             f" (target {len(target_rows)} insns, ours {len(our_rows)})"]
    lines.append(f"  frame: target {target_frame}  ours {our_frame}"
                 + (f"   stmw base: target r{target_stmw}"
                    f" ours r{our_stmw}"
                    if target_stmw or our_stmw else ""))
    if not target_defs and not our_defs:
        lines.append("  no callee-saved register is defined in either"
                     " stream — this function has no saved-register webs,"
                     " so there is nothing here to correspond. (A frame or"
                     " slot residual is slotdiff.py's question.)")
        return "\n".join(lines)

    target_uses = use_counts(target_rows, registers) if show_uses else {}
    our_uses = use_counts(our_rows, registers) if show_uses else {}
    lines.append("  one row per callee-saved REGISTER (r31 downward); the"
                 " cell is that register's FIRST DEFINITION in each stream")
    lines.append(f"  {'REG':<7} {'TARGET@':>7}  {'first definition':<30}"
                 f"  {'OURS@':>7}  {'first definition':<30}")
    for register, target_row, our_row in pairs:

        def render(row, uses):
            if row is None:
                return f"{'--':>7}  {'(never defined here)':<30}"
            _register, offset, text = row
            shown = text if len(text) <= 30 else text[:27] + "..."
            return f"{'@0x%x' % offset:>7}  {shown:<30}"

        flag = ""
        if register in moved:
            flag = "  <= DIFFERENT ROLE"
        elif target_row is None or our_row is None:
            flag = "  <= ONE STREAM ONLY"
        tag = register
        if show_uses:
            tag = (f"{register}"
                   f" {target_uses.get(register, 0)}/{our_uses.get(register, 0)}")
        lines.append(f"  {tag:<7} {render(target_row, target_uses)}"
                     f"  {render(our_row, our_uses)}{flag}")

    if moved:
        lines.append(f"  ROLE MISMATCH on {len(moved)} web(s):")
        for register in sorted(moved, key=lambda r: (r[0], -int(r[1:]))):
            target_role, our_role = moved[register]
            lines.append(f"    {register}: target holds `{target_role}`,"
                         f" ours holds `{our_role}`")
        lines.append(
            "  A web holding a DIFFERENT VALUE in the same register is a"
            " DECLARATION-LIST fact, not a scheduling one: MWCC numbers"
            " callee-saved GPRs downward from r31 in groups fixed before"
            " declaration order is walked (initialised locals before"
            " uninitialised ones, and NARROW/byte-typed locals as their own"
            " class ahead of every word-sized local). Reorder WITHIN a class"
            " to move a web one position; a cross-class reorder is"
            " byte-identical and proves the boundary rather than moving it.")
    elif len(target_defs) == len(our_defs):
        lines.append("  ASSIGNMENT MATCHES: every callee-saved register"
                     " holds the same value at its FIRST definition in both"
                     " streams. That closes the first-definition assignment"
                     " question and nothing else — read the scope lines"
                     " below before treating it as an all-clear.")

    lpairs = lifetime_pairs(target_rows, our_rows)
    _later_mismatches, count_diffs = web_mismatches(
        target_rows, our_rows, registers)
    lines.extend(scope_banner(target_rows, our_rows, registers,
                              lpairs, count_diffs))

    target_order = emission_order(target_defs)
    our_order = emission_order(our_defs)
    if target_order != our_order:
        lines.append(
            "  EMISSION ORDER DIFFERS (a SCHEDULE fact, reported separately"
            " because it is a different question from the assignment"
            " above):")
        lines.append(f"    target: {' '.join(target_order)}")
        lines.append(f"    ours:   {' '.join(our_order)}")

    if len(target_defs) != len(our_defs):
        lines.append(
            f"  SAVE-SET SIZE DIFFERS: target defines {len(target_defs)}"
            f" callee-saved web(s), ours {len(our_defs)}. The extra web is a"
            " local the other stream kept somewhere else, or an allocator"
            " copy — `slotdiff.py` owns the frame side of that question.")
    if per_web:
        lines.extend(format_per_web(target_rows, our_rows, registers,
                                    lpairs))
    return "\n".join(lines)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) < 2 or "--help" in sys.argv or "-h" in sys.argv:
        print(__doc__)
        return 2
    unit = re.sub(r"\.(c|cpp)$", "",
                  args[0].replace("\\", "/").strip("/"))
    if unit.startswith("src/"):
        unit = unit[len("src/"):]
    fn = args[1]
    raw = "--raw" in sys.argv

    target_rows, _names, error = fnasm.parse_fn(unit, fn, ours=False)
    if error:
        print(f"target: {error}")
        return 1
    our_rows, _names, error = fnasm.parse_fn(unit, fn, ours=True, raw=raw)
    if error:
        print(f"ours: {error}")
        return 1
    if not target_rows and not our_rows:
        print(f"no instructions decoded for {fn} in either stream — check"
              " the function name")
        return 1
    print(format_table(unit, fn, target_rows, our_rows,
                       show_uses="--uses" in sys.argv,
                       per_web="--per-web" in sys.argv))
    return 0


if __name__ == "__main__":
    sys.exit(main())
