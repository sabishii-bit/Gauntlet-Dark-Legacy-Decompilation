#!/usr/bin/env python3
"""The VOLATILE-register residual, both streams — what savedregs cannot read.

WHY THIS EXISTS. `savedregs` compares the callee-saved bank and says so
plainly: "every VOLATILE register, which this tool never reads". Three stuck
residuals measured at 54e14df4 are entirely or partly in that blind spot:

  game/ui/message::msgDraw          8 differing rows touch NO callee-saved
                                    register at all (savedregs' own count)
  game/g3d/g3dpad::G3DUpdatePadStatus
                                    72 REGISTER_ONLY rows around an inline
                                    branchless abs(): target r9/r10 where
                                    ours has r7/r10
  game/g3d/gcontrolpads::G3DReadControlPadStates
                                    3 sites where the target COPIES the
                                    register holding zero (`mr r30,r29`,
                                    `addi r7,r29,0`) and ours re-materializes
                                    it (`li r30,0`, `li r7,0`)

Eleven source-level probes across those two TUs were refuted, including two
byte-identical A/Bs that proved a class boundary. The open question is no
longer "which spelling" but "is what remains a relabeling, or a real
difference?" — and nothing answered that for volatiles.

WHAT IT DOES. Aligns the two streams on the opcode-sequence alignment (the
same correspondence `fnasm --diff` and `fndiff --ops` print, reused from
savedregs), then classifies every differing row:

  COLOUR      same mnemonic, same operand arity, and every difference is one
              volatile register standing in for another
  MATERIALIZE one side loads a constant (`li rD,K`) where the other copies a
              register (`mr rD,rS` / `addi rD,rS,0`) into the SAME rD
  SAVED-COLOUR a register-for-register swap touching the callee-saved bank:
              savedregs' subject, reported separately so its findings are not
              double-counted here as real differences
  STRUCTURAL  anything else — a real difference, not a relabeling
  UNPAIRED    a one-sided row (the streams hold different counts here)

Then it builds the volatile correspondence map out of the COLOUR rows and
reports whether it is CONSISTENT (each target volatile stands for exactly one
of ours and vice versa — one relabeling explains every row) or INCONSISTENT
(some target volatile maps to two or more of ours, so no single relabeling
does, and something real is hiding in the colour).

WHAT A CONSISTENT VERDICT DOES NOT MEAN. It is evidence, not proof, and it is
NOT a licence to stop: it says the differing rows are all mutually compatible
with one relabeling, which is what a pure allocator-colour residual looks like
— and also what a residual looks like when one upstream decision renamed
everything downstream of it. It does not identify the decision, and it never
proves a function unreachable from source. An INCONSISTENT verdict is the more
actionable one: it names rows that no relabeling explains.

Usage (from the repository root):
  python tools/gdl/volregs.py game/g3d/gcontrolpads G3DReadControlPadStates
  python tools/gdl/volregs.py game/ui/message msgDraw --rows
  python tools/gdl/volregs.py --all                  # every NonMatching fn
  python tools/gdl/volregs.py --all --out build/GUNE5D/volregs.json
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from collections import defaultdict
from pathlib import Path

TOOLS = Path(__file__).resolve().parent
REPO = TOOLS.parent.parent
sys.path.insert(0, str(TOOLS))

import fnasm  # noqa: E402
import savedregs  # noqa: E402

try:                       # noqa: E402
    import cliscreen
except ImportError:        # imported as tools.gdl.<module>
    from tools.gdl import cliscreen

REPORT = REPO / "build" / "GUNE5D" / "report.json"
COPY_RE = re.compile(r"^(?:mr|mr\.)$")


def volatiles(operands):
    """The volatile registers among these operands, in order."""
    out = []
    for op in operands:
        for tok in re.findall(r"\b[rf]\d+\b", op):
            if savedregs.register_class(tok) == "volatile":
                out.append(tok)
    return out


def is_copy(mnemonic, operands):
    """Is this a register-to-register copy? Returns the source, or None."""
    if COPY_RE.match(mnemonic or "") and len(operands) >= 2:
        return operands[1]
    if mnemonic == "addi" and len(operands) == 3 and operands[2] in ("0", "0x0"):
        return operands[1]
    return None


def is_constant_load(mnemonic, operands):
    """Is this materializing a literal into its destination?"""
    return mnemonic in ("li", "lis") and len(operands) == 2


REG_TOKEN = re.compile(r"\b[rf]\d+\b")


def skeleton(operand):
    """(text with every register replaced by %, [registers in order]).

    Registers live INSIDE operands, not only as whole operands: `0(r8)` and
    `0(r6)` differ by one register and nothing else. Comparing whole operand
    strings read that as a structural difference and mislabelled plain colour
    rows (measured on G3DReadControlPadStates: `lwz r4,0(r8)` vs
    `lwz r4,0(r6)` came back STRUCTURAL).
    """
    regs = REG_TOKEN.findall(operand)
    return REG_TOKEN.sub("%", operand), regs


def classify(t_text, o_text):
    """(class, substitutions) for one differing aligned row."""
    t_mn, t_ops = savedregs.parse_instruction(t_text)
    o_mn, o_ops = savedregs.parse_instruction(o_text)
    if t_mn == o_mn and len(t_ops) == len(o_ops):
        subs, saved = [], False
        for a, b in zip(t_ops, o_ops):
            if a == b:
                continue
            a_skel, a_regs = skeleton(a)
            b_skel, b_regs = skeleton(b)
            # anything but the registers differing is a real difference
            if a_skel != b_skel or len(a_regs) != len(b_regs):
                return "STRUCTURAL", []
            for ra, rb in zip(a_regs, b_regs):
                if ra == rb:
                    continue
                ca, cb = (savedregs.register_class(ra),
                          savedregs.register_class(rb))
                if ca == "volatile" and cb == "volatile":
                    subs.append((ra, rb))
                elif {ca, cb} <= {"callee-saved", "volatile"}:
                    # a swap touching the callee-saved bank is savedregs'
                    # subject. Reporting it as STRUCTURAL would double-count
                    # its findings as real differences: msgDraw read 13
                    # STRUCTURAL before this split, 5 of them its known
                    # callee-saved permutation. An ABI register (r1 stack
                    # pointer, r2, r13 small-data base) is NOT in that set:
                    # standing one in for another is a real difference, not a
                    # colour question, so it falls through to STRUCTURAL.
                    saved = True
                else:
                    return "STRUCTURAL", []
        if saved:
            return "SAVED-COLOUR", subs
        return ("COLOUR", subs) if subs else ("STRUCTURAL", [])
    # different mnemonics: the constant-vs-copy shape, same destination
    if t_ops and o_ops and t_ops[0] == o_ops[0]:
        t_copy, o_copy = is_copy(t_mn, t_ops), is_copy(o_mn, o_ops)
        if (t_copy and is_constant_load(o_mn, o_ops)) or \
           (o_copy and is_constant_load(t_mn, t_ops)):
            return "MATERIALIZE", []
    return "STRUCTURAL", []


def verdict_for(fwd, back):
    """(ambiguous, ambiguous_reverse, verdict) from the substitution maps.

    CONSISTENT requires a bijection in BOTH directions: a target volatile
    standing for two of ours is ambiguous, and so is one of ours standing for
    two of the target's. Checking only the forward direction calls a
    many-to-one map consistent, which is exactly the case where a relabeling
    does not explain the rows.
    """
    ambiguous = {k: sorted(v) for k, v in fwd.items() if len(v) > 1}
    ambiguous_back = {k: sorted(v) for k, v in back.items() if len(v) > 1}
    if not fwd:
        return ambiguous, ambiguous_back, "NO-COLOUR"
    if ambiguous or ambiguous_back:
        return ambiguous, ambiguous_back, "INCONSISTENT"
    return ambiguous, ambiguous_back, "CONSISTENT"


def analyze(unit, fn, raw=False):
    """Classify one function's volatile residual. Returns a dict or an error."""
    target_rows, _n, err = fnasm.parse_fn(unit, fn, ours=False)
    if err:
        return {"error": f"target: {err}"}
    our_rows, _n, err = fnasm.parse_fn(unit, fn, ours=True, raw=raw)
    if err:
        return {"error": f"ours: {err}"}
    if not target_rows and not our_rows:
        return {"error": "no instructions decoded in either stream"}

    counts = defaultdict(int)
    rows = []
    fwd, back = defaultdict(set), defaultdict(set)
    for t_row, o_row in savedregs.aligned_rows(target_rows, our_rows):
        if t_row is None or o_row is None:
            counts["UNPAIRED"] += 1
            rows.append({"class": "UNPAIRED",
                         "target": t_row[1] if t_row else None,
                         "ours": o_row[1] if o_row else None})
            continue
        t_text, o_text = t_row[1], o_row[1]
        if t_text == o_text:
            counts["same"] += 1
            continue
        kind, subs = classify(t_text, o_text)
        counts[kind] += 1
        for a, b in subs:
            fwd[a].add(b)
            back[b].add(a)
        rows.append({"class": kind, "target": t_text, "ours": o_text,
                     "subs": subs, "offset": t_row[0]})

    ambiguous, ambiguous_back, verdict = verdict_for(fwd, back)
    return {
        "unit": unit, "fn": fn, "verdict": verdict,
        "counts": dict(counts), "rows": rows,
        "map": {k: sorted(v)[0] for k, v in fwd.items() if len(v) == 1},
        "ambiguous": ambiguous, "ambiguous_reverse": ambiguous_back,
        "target_insns": len(target_rows), "our_insns": len(our_rows),
    }


def nonmatching_functions():
    """[(unit, fn)] for every function of a NonMatching TU, from report.json."""
    cfg = (REPO / "configure.py").read_text(encoding="utf-8", errors="replace")
    tus = set()
    for tu in re.findall(r'Object\(NonMatching,\s*"([^"]+)"', cfg):
        tus.add(re.sub(r"\.(c|cpp)$", "", tu))
    if not REPORT.exists():
        return []
    report = json.loads(REPORT.read_text(encoding="utf-8"))
    out = []
    for unit in report.get("units", []):
        name = unit.get("name", "")
        key = name.split("/", 1)[1] if "/" in name else name
        if key not in tus:
            continue
        for fn in unit.get("functions", []):
            if float(fn.get("fuzzy_match_percent", 0.0)) < 100.0:
                out.append((key, fn["name"]))
    return out


def print_one(result, show_rows=False):
    if "error" in result:
        print(result["error"])
        return 1
    c = result["counts"]
    print(f"== {result['unit']}::{result['fn']} volatile residual "
          f"(target {result['target_insns']} insns, ours {result['our_insns']})")
    print(f"  rows: {c.get('same', 0)} identical, "
          f"{c.get('COLOUR', 0)} COLOUR, {c.get('SAVED-COLOUR', 0)} SAVED-COLOUR, "
          f"{c.get('MATERIALIZE', 0)} MATERIALIZE, "
          f"{c.get('STRUCTURAL', 0)} STRUCTURAL, {c.get('UNPAIRED', 0)} UNPAIRED")
    if result["map"]:
        pairs = ", ".join(f"{a}->{b}" for a, b in sorted(result["map"].items()))
        print(f"  volatile correspondence: {pairs}")
    for label, data in (("target volatile standing for SEVERAL of ours",
                         result["ambiguous"]),
                        ("our volatile standing for SEVERAL of the target's",
                         result["ambiguous_reverse"])):
        for reg, others in sorted(data.items()):
            print(f"  AMBIGUOUS {label}: {reg} -> {', '.join(others)}")
    print(f"  VERDICT: {result['verdict']}", end="  ")
    if result["verdict"] == "CONSISTENT":
        print("one relabeling explains every differing row — the signature of"
              " pure allocator colour, and ALSO of one upstream decision that"
              " renamed everything after it. Evidence, not proof, and it names"
              " no decision.")
    elif result["verdict"] == "INCONSISTENT":
        print("no single relabeling explains these rows — the AMBIGUOUS lines"
              " above are the actionable ones.")
    elif result["verdict"] == "NO-COLOUR":
        print("no volatile substitutions at all; any residual here is"
              " STRUCTURAL or MATERIALIZE, read those rows.")
    if c.get("MATERIALIZE"):
        print(f"  NOTE: {c['MATERIALIZE']} MATERIALIZE row(s) — one stream"
              " loads a constant where the other copies a register holding it."
              " That is a value-reuse decision, not a relabeling, and it"
              " survives any permutation.")
    if show_rows:
        for r in result["rows"]:
            if r["class"] == "same":
                continue
            off = f"@0x{r['offset']:X}" if r.get("offset") is not None else "     "
            print(f"    {r['class']:11} {off:8} T: {r['target'] or '--':32} "
                  f"O: {r['ours'] or '--'}")
    return 0


def main():
    cliscreen.help_only(__doc__)
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("unit", nargs="?")
    ap.add_argument("fn", nargs="?")
    ap.add_argument("--rows", action="store_true", help="print every differing row")
    ap.add_argument("--raw", action="store_true", help="pre-postprocess body")
    ap.add_argument("--all", action="store_true",
                    help="census every non-exact function of a NonMatching TU")
    ap.add_argument("--out", help="write the census as JSON here")
    args = ap.parse_args()

    if args.all:
        pairs = nonmatching_functions()
        if not pairs:
            print("volregs: no NonMatching functions found — run a full ninja")
            return 2
        tally = defaultdict(int)
        results = []
        for unit, fn in pairs:
            r = analyze(unit, fn)
            if "error" in r:
                tally["unreadable"] += 1
                continue
            tally[r["verdict"]] += 1
            if r["counts"].get("MATERIALIZE"):
                tally["with MATERIALIZE rows"] += 1
            if r["counts"].get("STRUCTURAL"):
                tally["with STRUCTURAL rows"] += 1
            if r["counts"].get("SAVED-COLOUR"):
                tally["with SAVED-COLOUR rows"] += 1
            results.append(r)
        print(f"{len(pairs)} non-exact function(s) in NonMatching TUs")
        for k in ("CONSISTENT", "INCONSISTENT", "NO-COLOUR", "unreadable",
                  "with MATERIALIZE rows", "with STRUCTURAL rows",
                  "with SAVED-COLOUR rows"):
            print(f"  {k:24} {tally[k]}")
        print("\nCONSISTENT means one relabeling covers every differing row in"
              " that function; it is evidence of pure colour, never proof, and"
              " it names no decision. INCONSISTENT functions are where a"
              " relabeling does NOT cover the rows — read those first.")
        worst = sorted(results,
                       key=lambda r: -r["counts"].get("STRUCTURAL", 0))[:12]
        if worst and worst[0]["counts"].get("STRUCTURAL"):
            print("\nmost STRUCTURAL rows (real differences, not colour):")
            for r in worst:
                n = r["counts"].get("STRUCTURAL", 0)
                if not n:
                    break
                print(f"  {n:4} {r['unit']}::{r['fn']}  [{r['verdict']}]")
        if args.out:
            Path(args.out).write_text(json.dumps(results, indent=1))
            print(f"\nwrote {args.out}")
        return 0

    if not args.unit or not args.fn:
        print(__doc__)
        return 2
    unit = re.sub(r"\.(c|cpp)$", "", args.unit.replace("\\", "/").strip("/"))
    if unit.startswith("src/"):
        unit = unit[len("src/"):]
    return print_one(analyze(unit, args.fn, raw=args.raw), show_rows=args.rows)


if __name__ == "__main__":
    sys.exit(main())
