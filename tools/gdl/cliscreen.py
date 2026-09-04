"""`--help` must never do work, and an unknown flag must never be swallowed.

RUN 53 ITEM 2. Four tools were reported for failing the cheapest possible
invocation. Reproduced verbatim from the repo root at c7b741799:

    python tools/gdl/composed_census/hv_formfirst.py --help
        -> runs its whole eight-function sweep and prints
           "0 of 8 CLOSE under the corrected arrow order", exit 0
    python tools/gdl/composed_census/hv_try.py --help
        -> IndexError: list index out of range   (hv_try.py line 22), exit 1
    python tools/gdl/rule_derive.py --help
        -> dumps a pb_window permutation analysis, exit 0
    python tools/gdl/build_rule.py --help
        -> dumps a pbWinSetup/pbProjCalc analysis AND WRITES
           build/GUNE5D/rules/game_pb_pb_window_rules.json, exit 0

The last one is the shape that matters: `--help` had a side effect on disk.
All four share one cause — the tool reads `sys.argv` with no argparse, and
either indexes it positionally or filters with `arg.startswith("--")`, which
silently DISCARDS every flag the tool does not implement. That is the same
defect class AGENTS.md records as "probe swallowed unknown flags 45 runs":
a mistyped or obsolete flag produces a confident run of the DEFAULT
behaviour, and the output cannot be told apart from the run the caller meant.

TWO-SIDED CALIBRATION, measured at c7b741799 (AGENTS.md's two-sided rule):

  POSITIVE. Of 128 modules under `tools/gdl` and `tools/gdl/composed_census`
  that read arguments at all, 75 use argparse and get `--help` for free;
  **53 read `sys.argv` with no argparse** and can swallow a flag. The class is
  41% of the arg-reading tool corpus, so this module exists rather than four
  copies of the same six lines. Only the four tools this item names are
  converted here; the other 49 are a measured population, not a claim that
  each is broken.

  NEGATIVE. The decisive half: a screen that REFUSES an unknown flag is a
  regression if any live caller passes one. Scanning every accepted record,
  every inbox proposal, AGENTS.md, README.md and `tools/gdl/tests/*.py` for
  text following each of the four tool names: `rule_derive.py` -> {--diff,
  --help}, `build_rule.py` -> {--help}, `hv_try.py` -> {--ops, --help},
  `hv_formfirst.py` -> {--help}. Every `--help` is a REPORT of this defect,
  and `--diff`/`--ops` belong to `fnasm`/`probe` invocations quoted in the
  same sentence, not to these tools. **Zero live invocations would be
  refused.** With an empty negative side the screen ships as a REFUSAL rather
  than as an advisory warning.

IMPORTABLE CORE: screen_argv, unknown_flags and screen — pure over a list of
argument strings; no build, no filesystem, and importing this module has no
side effects.
"""
from __future__ import annotations

import sys

HELP_FLAGS = ("-h", "--help")


def unknown_flags(argv: list[str], known: object) -> list[str]:
    """The `--flags` in ``argv`` that ``known`` does not contain.

    A flag is compared by its NAME, so `--out=PATH` is screened as `--out`:
    the `=VALUE` spelling is the one several of these tools use and it must
    not read as a different, unknown flag. `--` on its own ends flag parsing
    exactly as it does everywhere else, and a bare `-` is a positional.
    """
    known = set(known) | set(HELP_FLAGS)
    out: list[str] = []
    for arg in argv:
        if arg == "--":
            break
        if not arg.startswith("--") or arg == "--":
            continue
        name = arg.split("=", 1)[0]
        if name not in known:
            out.append(name)
    return out


def screen_argv(argv: list[str], known: object, usage: str | None = None,
                doc: str | None = None) -> None:
    """Handle `--help` and refuse unknown flags, BEFORE the tool does work.

    Call this as the first statement of a tool's `main()` (or before any
    module-level analysis). ``known`` is the tool's own flag vocabulary;
    ``usage`` is the one- or two-line invocation summary; ``doc`` is usually
    the module docstring.

    Raises SystemExit(0) for help and SystemExit(2) — argparse's usage-error
    status, with the message on stderr — for an unknown flag.
    SystemExit is the project's refusal idiom — note AGENTS.md discipline 20:
    it is NOT an `Exception`, so a caller wrapping this in `except Exception`
    to fail soft will exit instead. Wrap `except (Exception, SystemExit)` or
    handle SystemExit separately.
    """
    if any(arg in HELP_FLAGS for arg in argv):
        if usage:
            print(usage)
        if doc:
            print()
            print(doc.strip())
        raise SystemExit(0)
    bad = unknown_flags(argv, known)
    if bad:
        vocabulary = ", ".join(sorted(set(known))) or "(none)"
        # PRINT the message and raise SystemExit(2). `raise SystemExit("text")`
        # would print the text too, but sets `.code` to the STRING and the
        # process status to 1 — the same status a tool that merely failed
        # returns, which is exactly the confusion this screen exists to end.
        # 2 is argparse's usage-error status, so the four converted tools now
        # agree with the 75 argparse-based ones.
        print(f"unknown flag(s): {', '.join(bad)}\n"
              f"this tool's flags: {vocabulary}\n"
              + (usage + "\n" if usage else "")
              + "Refused rather than ignored: a swallowed flag produces a"
                " confident run of the DEFAULT behaviour whose output cannot"
                " be told apart from the run you meant.",
              file=sys.stderr)
        raise SystemExit(2)


def screen(known: object, usage: str | None = None,
           doc: str | None = None) -> None:
    """`screen_argv` over `sys.argv[1:]`."""
    screen_argv(sys.argv[1:], known, usage=usage, doc=doc)
