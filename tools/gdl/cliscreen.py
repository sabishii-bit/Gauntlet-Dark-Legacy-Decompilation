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

RUN-59 ITEM 9 widens the help half of this to the whole tool corpus. Two
tools were reported (`wf_rederive_pin.py --help`, `wf_dump.py --help`) for
exiting NON-ZERO with the docstring on STDERR, so the capture pattern
AGENTS.md documents —

    $o = python <tool> --help; $code = $LASTEXITCODE

— reports a FAILURE for a request that succeeded, and a caller who filters
stdout gets nothing. Censused at 434460f28 with
build/t3_scratch/t3_help_census.py over all 276 modules in `tools/gdl` and
`tools/gdl/composed_census`: 215 already exit 0 on stdout (argparse, mostly)
and 61 do not — 36 exit non-zero writing to stderr (a `SystemExit(__doc__)`
or a bare `IndexError` traceback), 20 exit 1 or 2 with the text on stdout,
and 5 exit 0 printing NOTHING. `help_only` is what the 61 gained; the
unknown-flag REFUSAL above is deliberately NOT widened, because that half
needs each tool's own flag vocabulary and a negative control per tool.

IMPORTABLE CORE: screen_argv, unknown_flags, help_only and screen — pure
over a list of argument strings; no build, no filesystem, and importing this
module has no side effects.
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


def help_only(doc: str | None = None, usage: str | None = None,
              argv: list[str] | None = None) -> None:
    """Answer `-h`/`--help` on STDOUT at exit 0, and do nothing else.

    The half of `screen` that every tool can adopt without a per-tool
    negative control: it never refuses anything, so no live invocation can
    change behaviour, and a tool that already had a help path keeps it for
    every other argument shape.

    A SUCCESSFUL help request exits 0 with the text on stdout. Missing or
    wrong ARGUMENTS are a different event and keep their own status — a
    tool that prints its usage at exit 2 for no arguments is correct, and
    this call fires only on an EXPLICIT help flag, so that distinction is
    preserved rather than flattened.

    Call it as the first statement of `main()`, or before any module-level
    work in a tool that has no `main()`: `--help` must not do work, must
    not build, and must not write (run-53 item 2 found one that wrote a
    rules JSON on its way to printing help).
    """
    argv = sys.argv[1:] if argv is None else argv
    if not any(arg in HELP_FLAGS for arg in argv):
        return
    if usage:
        print(usage)
        if doc:
            print()
    if doc:
        print(doc.strip())
    raise SystemExit(0)


if __name__ == "__main__":
    # A library, not a command: say so on stdout at exit 0. Exiting
    # silently at 0 is indistinguishable from a tool that ran and found
    # nothing (run-59 item 9).
    print(__doc__.strip())
