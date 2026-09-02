#!/usr/bin/env python3
"""Which refusals does webfrank PROMISE, and which does a test PIN?

Run from the repository root:
  python tools/gdl/composed_census/t8_refusal_coverage.py
  python tools/gdl/composed_census/t8_refusal_coverage.py --out build/GUNE5D/t8_refusals.json

WHY THIS EXISTS. WF found the M-form Rc hole because no shipped rule sat
on a record-form rotate: the promise was in the code and nothing held it
there. A proof class's refusals ARE its contract, so an unpinned refusal
is an unpinned contract clause.

READ THE TWO CATEGORIES DIFFERENTLY — this is the whole point of the
report, and totalling them hides it:

  BOUNDARY   a class-boundary refusal. A hole here ADMITS a rewrite the
             class does not actually prove. These are the ones worth a
             test.
  BAIL-OUT   "unsupported opcode", "out of range", "not reachable",
             "did not converge". These fail CLOSED: a hole refuses MORE,
             never less, so an untested one cannot admit anything
             unsound. A large unpinned count here is not alarming.

The pinning test is a heuristic (does a distinctive phrase of the message
appear anywhere in tools/gdl/tests/?), so treat `pinned` as a lower bound
and read the UNPINNED BOUNDARY list as the queue.
"""
import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / "tools" / "gdl" / "webfrank.py"
TEST_DIR = ROOT / "tools" / "gdl" / "tests"

# A refusal whose failure direction is "refuse more", never "admit more".
BAIL_OUT_RE = re.compile(
    r"unsupported|not reachable|out of range|outside the function"
    r"|did not converge|not in the split map|needs the split map"
    r"|expected (?:at most )?one relocation section|invalid \w+ offset"
    r"|is not one\b|without discoverable",
    re.I,
)

STOP_WORDS = {
    "instruction", "cannot", "does", "with", "that", "this", "from", "into",
    "have", "must", "same", "which", "would", "there", "under", "than",
    "then", "were", "when", "each", "only", "their", "here", "over", "both",
}


def refusal_messages(source: str) -> list[str]:
    """Every ValueError message webfrank can raise, f-slots elided."""
    out = []
    for raw in re.findall(r"raise ValueError\(\s*(.*?)\)\s*\n", source, re.S):
        parts = re.findall(r'(?:f?")((?:[^"\\]|\\.)*)"', raw)
        if not parts:
            continue
        text = " ".join(parts).replace("\\n", " ")
        text = re.sub(r"\{[^}]*\}", "<>", text)
        text = re.sub(r"\s+", " ", text).strip()
        if text:
            out.append(text)
    return out


def assertion_patterns(tests: str) -> list[re.Pattern]:
    """Every `assertRaisesRegex(..., "PATTERN")` in the test corpus.

    This is what "pinned" actually MEANS: a test asserts a regex against
    the refusal message. Matching keywords instead under-reported — the
    run-38 tests assert short regexes like "volatile" and "not injective",
    which no 6-word phrase probe can see, so the tool reported its own
    freshly-closed queue items as still open.
    """
    out = []
    for raw in re.findall(
            r"assertRaisesRegex\(\s*\w+\s*,\s*(.*?)\)\s*:", tests, re.S):
        quoted = (r'(?:[rf]?")((?:[^"\\]|\\.)*)"'
                  r"|(?:[rf]?')((?:[^'\\]|\\.)*)'")
        for part in re.findall(quoted, raw):
            text = part[0] or part[1]
            if not text.strip():
                continue
            try:
                out.append(re.compile(text, re.I))
            except re.error:
                continue
    return out


def is_pinned(message: str, tests: str,
              patterns: list[re.Pattern]) -> bool:
    if any(p.search(message) for p in patterns):
        return True
    probe = " ".join(message.split()[:6]).rstrip(".,:")
    if probe and probe.lower() in tests:
        return True
    words = [w for w in re.findall(r"[a-z_]{4,}", message.lower())
             if w not in STOP_WORDS]
    return any(f"{a} {b}" in tests for a, b in zip(words, words[1:]))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out",
                    default=str(ROOT / "build" / "GUNE5D"
                               / "t8_refusal_coverage.json"))
    args = ap.parse_args()

    source = SRC.read_text(encoding="utf-8")
    # Case-SENSITIVE for pattern extraction (`assertRaisesRegex` is
    # mixed-case), lowercased only for the phrase fallback. Lowercasing
    # first made the extractor silently find zero patterns and the tool
    # reported its own freshly-closed queue items as still open.
    test_source = "\n".join(p.read_text(encoding="utf-8")
                            for p in sorted(TEST_DIR.glob("test_*.py")))
    tests = test_source.lower()

    patterns = assertion_patterns(test_source)
    rows = []
    for message in sorted(set(refusal_messages(source))):
        rows.append({
            "message": message,
            "category": "BAIL-OUT" if BAIL_OUT_RE.search(message)
                        else "BOUNDARY",
            "pinned": is_pinned(message, tests, patterns),
        })

    def count(category, pinned):
        return sum(1 for r in rows
                   if r["category"] == category and r["pinned"] is pinned)

    summary = {
        "refusal_sites": len(rows),
        "boundary_pinned": count("BOUNDARY", True),
        "boundary_unpinned": count("BOUNDARY", False),
        "bailout_pinned": count("BAIL-OUT", True),
        "bailout_unpinned": count("BAIL-OUT", False),
    }
    Path(args.out).parent.mkdir(parents=True, exist_ok=True)
    Path(args.out).write_text(
        json.dumps({"summary": summary, "rows": rows}, indent=1),
        encoding="utf-8")

    print(f"{summary['refusal_sites']} distinct refusal message(s) in"
          f" {SRC.relative_to(ROOT).as_posix()}")
    print(f"  BOUNDARY  {summary['boundary_pinned']} pinned,"
          f" {summary['boundary_unpinned']} UNPINNED  <- the queue")
    print(f"  BAIL-OUT  {summary['bailout_pinned']} pinned,"
          f" {summary['bailout_unpinned']} unpinned  (fails closed;"
          " not alarming)")
    unpinned = [r for r in rows
                if r["category"] == "BOUNDARY" and not r["pinned"]]
    if unpinned:
        print("\nUNPINNED BOUNDARY refusals:")
        for row in unpinned:
            print(f"  {row['message'][:140]}")
    print(f"\nwrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
