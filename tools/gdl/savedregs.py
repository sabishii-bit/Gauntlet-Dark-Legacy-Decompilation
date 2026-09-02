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
    python tools/gdl/savedregs.py <unit> <fn> --raw     # pre-webfrank body

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
# compares write a condition register; branches write nothing.
NON_DEFINING = re.compile(
    r"^(?:st|b|cmp|tw|dcb|icb|sync|isync|eieio|nop)")

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


def format_table(unit, fn, target_rows, our_rows, show_uses=False):
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
                     " holds the same value in both streams. Whatever"
                     " residual remains is NOT a save-register assignment"
                     " question.")

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
                       show_uses="--uses" in sys.argv))
    return 0


if __name__ == "__main__":
    sys.exit(main())
