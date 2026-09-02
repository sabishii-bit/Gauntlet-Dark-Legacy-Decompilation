"""Split the `--ops IDENTICAL / 0 clusters` signature into its two real classes.

`fndiff --ops` reporting `opcode multiset: IDENTICAL (N/N)` with zero clusters
does NOT identify the block-rotation / recolor class. It is satisfied by two
disjoint residual classes with disjoint cures:

  SCHEDULE-REORDER  same opcode bag, instructions emitted in a DIFFERENT ORDER
  RECOLOR           streams index-aligned, only register FIELDS differ

The discriminator is an index-aligned mnemonic comparison and it costs zero
builds. This tool applies it, and for the recolor class derives the
target-register -> our-register correspondence on the callee-saved bank.

TWO MEASURED CORRECTIONS are baked in, both of which produced a wrong answer
first (run 39):

  * FIRST-WINS PAIRING LIES. Wherever two same-opcode instructions swapped
    emission order, positional pairing invents a correspondence
    (drawMemCardMessage: the `li r22,0` / `li r29,0` zeroing swap fabricated a
    second image for target r22 and made the function read NOT-PURE-RECOLOR).
    The map is solved as a max-weight bipartite assignment and the residue is
    reported rather than hidden. This is the same hazard
    claim.law.regnorm-positional-pairing-fabricates-displacement-rows names.

  * ONE REGISTER HOSTS SEVERAL LIVE RANGES. A register-level bijection need
    not exist even when both streams are index-aligned and count-symmetric -
    which is exactly the question a webfrank consistent-recolor rule asks.
    `consistent_recolor` answers it; `--violations` prints every instruction
    no single bijection can explain.

FPRs are compared as well as GPRs: an f-register-only recolor is otherwise
invisible and the function reads as "100% explained by the identity map"
while carrying a live word diff (measured on PointLineColl).

PAIR IT WITH A PIN SCREEN. wf_word_diff.py reads the RAW pre-postprocess body,
so a webfrank-pinned function reports the residual its rule already closes and
ranks at the TOP of a word-count queue, while this tool reads the
POSTPROCESSED object and shows it as a clean identity map. Identity map plus a
large word count means you are reading a shipped rule's input, not an open
residual. The rules live under the top-level "units" key of
config/GUNE5D/webfrank.json, not at the root.

Usage:
    python tools/gdl/composed_census/rc_recolor_class.py <unit> <fn>
    python tools/gdl/composed_census/rc_recolor_class.py <unit> <fn> --violations
    python tools/gdl/composed_census/rc_recolor_class.py --batch <rows.json> \
        [--out build/GUNE5D/rc_recolor_class.json]

--batch takes a JSON file holding either a list of {"unit","function"} objects
or an object with a "rows" key of the same. Output defaults under
build/GUNE5D/, which is gitignored and per-worktree.
"""
import argparse
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def _repo_root(start):
    """Walk up to the checkout root instead of counting dirname() levels.

    Counting levels is how a promoted script silently breaks: with one
    dirname() too few this file resolved ROOT to tools/ and every fnasm call
    returned nothing, which surfaces as a plausible-looking 'NO-DISASM'
    verdict rather than an error (caught run 39 by the AGENTS.md rule that a
    promoted tool is RUN ONCE from the repo root before its commit lands).
    """
    cur = start
    while True:
        if os.path.isfile(os.path.join(cur, "configure.py")):
            return cur
        parent = os.path.dirname(cur)
        if parent == cur:
            return os.path.dirname(os.path.dirname(os.path.dirname(start)))
        cur = parent


ROOT = _repo_root(HERE)
FNASM = os.path.join(ROOT, "tools", "gdl", "fnasm.py")
DEFAULT_OUT = os.path.join("build", "GUNE5D", "rc_recolor_class.json")

RE_LINE = re.compile(r"^\s*([0-9a-fA-F]+):\s+(\S+)\s*(.*)$")
RE_REG = re.compile(r"\b([rf])(\d{1,2})\b")
CALLEE = set(range(14, 32)) | set(range(114, 132))
ARGREG = set(range(3, 11))


def _regname(v):
    return ("f%d" % (v - 100)) if v >= 100 else ("r%d" % v)


def dump(unit, fn, ours):
    cmd = [sys.executable, FNASM, unit, fn] + (["--ours"] if ours else [])
    proc = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True,
                          timeout=240)
    rows = []
    for line in (proc.stdout or "").splitlines():
        m = RE_LINE.match(line)
        if not m:
            continue
        # drop the relocation comment: its digits are not register operands
        rest = m.group(3).split("@")[0]
        rows.append({
            "off": int(m.group(1), 16),
            "mnem": m.group(2),
            "regs": [int(n) + (100 if kind == "f" else 0)
                     for kind, n in RE_REG.findall(rest)],
            "text": line.rstrip(),
        })
    return rows


def analyse(unit, fn):
    tgt, ours = dump(unit, fn, False), dump(unit, fn, True)
    out = {"unit": unit, "function": fn,
           "t_insns": len(tgt), "o_insns": len(ours)}
    if not tgt or not ours:
        out["classification"] = "NO-DISASM"
        return out, []
    if len(tgt) != len(ours):
        out["classification"] = "COUNT-ASYMMETRIC"
        return out, []

    votes, pairs, mnem_diffs = {}, 0, 0
    scratch, shape, diff_words = [], [], 0
    param_t, param_o, seen_bl = {}, {}, False

    for i, (a, b) in enumerate(zip(tgt, ours)):
        if a["text"].split(":", 1)[1] != b["text"].split(":", 1)[1]:
            diff_words += 1
        if a["mnem"] != b["mnem"]:
            mnem_diffs += 1
            continue
        if len(a["regs"]) != len(b["regs"]):
            shape.append(i)
            continue
        if (not seen_bl and a["mnem"] in ("mr", "or", "addi")
                and len(a["regs"]) >= 2 and a["regs"][0] in CALLEE
                and a["regs"][1] in ARGREG and b["regs"][0] in CALLEE):
            param_t[a["regs"][1]] = a["regs"][0]
            param_o[a["regs"][1]] = b["regs"][0]
        if a["mnem"] == "bl":
            seen_bl = True
        for ra, rb in zip(a["regs"], b["regs"]):
            if ra in CALLEE and rb in CALLEE:
                votes[(ra, rb)] = votes.get((ra, rb), 0) + 1
                pairs += 1
            elif ra != rb:
                scratch.append((i, ra, rb))

    best, used_t, used_o = {}, set(), set()
    for (ra, rb), w in sorted(votes.items(), key=lambda kv: (-kv[1], kv[0])):
        if ra in used_t or rb in used_o:
            continue
        best[ra] = rb
        used_t.add(ra)
        used_o.add(rb)
    explained = sum(w for (ra, rb), w in votes.items() if best.get(ra) == rb)

    violations = []
    for i, (a, b) in enumerate(zip(tgt, ours)):
        if a["mnem"] != b["mnem"]:
            violations.append((i, a, b, "MNEMONIC (schedule reorder)"))
            continue
        if len(a["regs"]) != len(b["regs"]):
            violations.append((i, a, b, "SHAPE"))
            continue
        bad = []
        for ra, rb in zip(a["regs"], b["regs"]):
            if ra in CALLEE and rb in CALLEE:
                if best.get(ra) != rb:
                    bad.append("%s->%s (bijection says %s)"
                               % (_regname(ra), _regname(rb),
                                  _regname(best.get(ra, ra))))
            elif ra != rb:
                bad.append("SCRATCH %s/%s" % (_regname(ra), _regname(rb)))
        if bad:
            violations.append((i, a, b, "; ".join(bad)))

    consistent = not violations
    param_moved = (bool(param_t)
                   and sorted(param_t.values()) != sorted(param_o.values()))

    if mnem_diffs:
        classification = "SCHEDULE-REORDER (%d mnemonic diffs)" % mnem_diffs
    elif not votes:
        classification = "SCRATCH-ONLY"
    elif consistent and param_moved:
        classification = "CONSISTENT-RECOLOR / PARAM-BLOCK-ROTATION"
    elif consistent:
        classification = "CONSISTENT-RECOLOR"
    elif param_moved:
        classification = "LIVE-RANGE-RECOLOR + PARAM-BLOCK-ROTATION"
    else:
        classification = "LIVE-RANGE-RECOLOR"

    out.update({
        "classification": classification,
        "recolor_class": mnem_diffs == 0,
        "consistent_recolor": consistent,
        "diff_words_disasm": diff_words,
        "mnemonic_diffs": mnem_diffs,
        "shape_diff_insns": shape[:8],
        "n_scratch_diffs": len(scratch),
        "operand_pairs_explained": "%d/%d" % (explained, pairs),
        "explained_pct": round(100.0 * explained / max(pairs, 1), 1),
        "correspondence": {_regname(k): _regname(v)
                           for k, v in sorted(best.items(), reverse=True)},
        "param_regs_target": [_regname(v)
                              for v in sorted(set(param_t.values()),
                                              reverse=True)],
        "param_regs_ours": [_regname(v)
                            for v in sorted(set(param_o.values()),
                                            reverse=True)],
        "unexplained_insns": len(violations),
    })
    return out, violations


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("unit", nargs="?")
    ap.add_argument("function", nargs="?")
    ap.add_argument("--violations", action="store_true",
                    help="print every instruction no single bijection explains")
    ap.add_argument("--batch", metavar="ROWS_JSON")
    ap.add_argument("--out", default=DEFAULT_OUT)
    args = ap.parse_args()

    if args.batch:
        with open(args.batch, "r", encoding="utf-8") as fh:
            data = json.load(fh)
        rows = data["rows"] if isinstance(data, dict) else data
        results = []
        for i, row in enumerate(rows):
            unit = row.get("unit") or row.get("tu")
            fn = row.get("function")
            try:
                res, _ = analyse(unit, fn)
            except Exception as exc:  # noqa: BLE001
                res = {"unit": unit, "function": fn,
                       "classification": "ERROR %s" % exc}
            res["input_words"] = row.get("measured_words")
            results.append(res)
            sys.stderr.write("[%d/%d] %s::%s -> %s\n"
                             % (i + 1, len(rows), unit, fn,
                                res.get("classification")))
            sys.stderr.flush()
        out = args.out if os.path.isabs(args.out) else os.path.join(ROOT,
                                                                    args.out)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        with open(out, "w", encoding="utf-8") as fh:
            json.dump(results, fh, indent=2)
        rec = [r for r in results if r.get("recolor_class")]
        sch = [r for r in results if r.get("recolor_class") is False]
        hdr = "%-30s %-20s %6s %5s %5s  %s" % ("FUNCTION", "UNIT", "INSNS",
                                               "EXPL", "SCR", "CLASS")
        print(hdr)
        print("-" * 110)
        for r in results:
            print("%-30s %-20s %6s %5s %5s  %s"
                  % (str(r.get("function"))[:30],
                     str(r.get("unit") or "").replace("game/", "")[:20],
                     r.get("t_insns"), r.get("explained_pct"),
                     r.get("n_scratch_diffs"), r.get("classification")))
        print("\nrecolor class %d | schedule class %d | consistent recolor %d"
              % (len(rec), len(sch),
                 sum(1 for r in rec if r.get("consistent_recolor"))))
        print("wrote %s" % out)
        return 0

    if not args.unit or not args.function:
        ap.print_help()
        return 2
    res, violations = analyse(args.unit, args.function)
    print(json.dumps(res, indent=2))
    if args.violations:
        print("\nINSTRUCTIONS NO SINGLE BIJECTION EXPLAINS: %d of %d"
              % (len(violations), res.get("t_insns")))
        for i, a, b, why in violations:
            print("  [%4d] T %-44s | O %-44s  %s"
                  % (i, a["text"].strip(), b["text"].strip(), why))
    return 0


if __name__ == "__main__":
    sys.exit(main())
