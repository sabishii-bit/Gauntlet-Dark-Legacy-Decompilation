#!/usr/bin/env python3
"""Where a composed_census tool's GENERATED data lives: build/, not the repo.

RUN-61 ITEM 6. Seven census tools wrote their JSON output beside their own
source, into `tools/gdl/composed_census/`, and those outputs were TRACKED.
Running any of them therefore dirtied the checkout, which is the same defect
class run-59 item 9 fixed for `--help` (three tools that wrote a tracked
file on their way to printing help) -- only here the write happens on the
ordinary path, so no help screen removes it.

CENSUS at 5cfc42acf, `git ls-files tools/gdl/composed_census`, every tracked
non-`.py` file and the tool that writes it:

    ch_census26.json   <- ch_census26.py   (read by ch_harvest, ch_roster,
                                            ch_zero)
    ch_closable.json   <- ch_closable.py
    ch_harvest.json    <- ch_harvest.py
    ch_roster.json     <- ch_roster.py     (read by ch_closable)
    ch_shipped.json    <- ch_shipped.py    (read by ch_harvest, ch_roster,
                                            ch_sweep26)
    ch_sweep26.json    <- ch_sweep26.py    (read by ch_show)
    cn_found.json      <- cn_search.py, rewritten by cn_final.py
                                           (read by cn_detail, cn_final)
    ch_sweep26.log     <- NO tool writes it: a captured console log
    pw_rec_attempt.json, pw_rec_law.json
                       <- NO tool writes or reads them: hand-authored
                          records, i.e. INPUTS, not generated output

The last three are named here because a census that lists only what it
changes is not a census. They stay tracked: nothing regenerates them, so
untracking them would destroy data rather than stop a tool dirtying a tree.

Generated artifacts now go to `build/<version>/composed_census/<name>`,
overridable with a repo-relative `--out PATH`. A reader whose artifact has
not been produced yet REFUSES and names the command that produces it, rather
than reading a stale in-tree copy or raising FileNotFoundError.

IMPORTABLE CORE: artifact_path, artifact_label, out_override -- pure over
strings, no filesystem effect at all. `write_artifact` creates the directory
and writes; `load_artifact` reads one file and refuses by SystemExit.
"""

import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
VERSION = "GUNE5D"
OUT_DIR = os.path.join(ROOT, "build", VERSION, "composed_census")

#: artifact basename -> the command that produces it, for refusal messages.
PRODUCERS = {
    "ch_census26.json": "python tools/gdl/composed_census/ch_census26.py",
    "ch_closable.json": "python tools/gdl/composed_census/ch_closable.py",
    "ch_harvest.json": "python tools/gdl/composed_census/ch_harvest.py",
    "ch_roster.json": "python tools/gdl/composed_census/ch_roster.py",
    "ch_shipped.json": "python tools/gdl/composed_census/ch_shipped.py",
    "ch_sweep26.json": "python tools/gdl/composed_census/ch_sweep26.py",
    "cn_found.json": "python tools/gdl/composed_census/cn_search.py",
}


def out_override(argv):
    """The repo-relative path after `--out`, or None. Never a directory.

    Deliberately permissive about the rest of argv: these tools take no
    other options, and a scan must not turn `--help` into an error.
    """
    for index, value in enumerate(argv):
        if value == "--out" and index + 1 < len(argv):
            return argv[index + 1]
        if value.startswith("--out="):
            return value[len("--out="):]
    return None


def artifact_path(name, out=None):
    """Absolute path `name` belongs at. PURE: creates nothing.

    `out` is taken repo-relative (an absolute path is honoured as given) so
    the same spelling works from any working directory. The directory is
    created by `write_artifact`, not here: `tools/gdl/tests/
    test_t20_importable_purity.py` screens every advertised IMPORTABLE CORE
    function's call graph for writes, and it caught the `os.makedirs` this
    used to do -- an IMPORTABLE CORE claim that a sweep can trust has to be
    true of the function, not of the intent.
    """
    if out:
        return out if os.path.isabs(out) else os.path.join(ROOT, out)
    return os.path.join(OUT_DIR, name)


def artifact_label(path):
    """The path as a repo-relative string, for a `wrote ...` line."""
    try:
        return os.path.relpath(path, ROOT).replace("\\", "/")
    except ValueError:
        return path


def write_artifact(name, data, out=None, indent=1):
    """Write one generated artifact and return its path."""
    path = artifact_path(name, out)
    os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=indent)
    return path


def load_artifact(name, reader=None):
    """Read a generated artifact, or REFUSE naming the command that makes it.

    A missing artifact is a missing INPUT, not an empty result: returning
    `{}` here would let a downstream census report "nothing found" for a
    measurement that was never run.
    """
    path = os.path.join(OUT_DIR, name)
    if not os.path.exists(path):
        raise SystemExit(
            "%s needs %s, which has not been generated in this checkout.\n"
            "It is build output, not tracked source (run-61 item 6). Run:\n"
            "    %s\n"
            "Expected at: %s"
            % (reader or "this tool", name,
               PRODUCERS.get(name, "its producing census tool"),
               artifact_label(path)))
    with open(path, encoding="utf-8") as handle:
        return json.load(handle)


if __name__ == "__main__":
    # A library, not a command: exiting 0 with no output would be
    # indistinguishable from a tool that ran and found nothing.
    print(__doc__.strip())
    print("\nartifact directory: " + artifact_label(OUT_DIR))
