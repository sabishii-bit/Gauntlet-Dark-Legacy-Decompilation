"""Run-50 item 8: the IMPORTABLE CORE convention's missing falsifier.

THE OBSERVATION (T19). `test_importable_core.py` checks that every
advertised name EXISTS, is callable, and that importing the module prints
nothing. It never checks the property the convention actually promises.
AGENTS.md states it as: the named functions "are pure over parsed data,
they never build, and importing the module has no side effects" -- and
nothing asserted the middle clause, which is the one a sweep depends on.
A sweep that calls an advertised function N times and discovers on call 1
that it shells out to `ninja` has paid for the convention twice.

WHAT IS CHECKED, and why these two and not "purity" in general. Each
advertised name is resolved to its module-level definition and its
INTRA-MODULE call graph is walked:

  BUILD   any subprocess whose argv mentions ninja / configure.py /
          mwcceppc / mwldeppc. This is the clause AGENTS.md states.
  WRITE   write_text / write_bytes / mkdir / unlink / rmtree / rename /
          copyfile / copy2 / utime / touch / makedirs, or `open(..., "w")`.
          A library function that writes is not usable in a sweep either.

READ-ONLY SUBPROCESSES ARE ALLOWED, deliberately: `fndiff.objdump` IS an
advertised name and running objdump is the entire point of it. The
convention's word is "build", not "subprocess".

TWO-SIDED CALIBRATION at run-50 HEAD (scratch t20_purity_census.py), over
the 15 modules carrying the marker across tools/gdl and composed_census:
  BUILD reachability      0 advertised functions  <- ships GREEN
  WRITE reachability      0
  read-only subprocess   12 (fndiff x8, claimscope x3, t15_whoemits x1)
and the FALSE-POSITIVE half, which changed the design: a first draft
included `replace`, `copy` and `remove` in the write set and scored 12 of
the 15 modules impure -- on `unit.replace("\\\\", "/")`, a STRING method.
Those three are removed by name. A second draft resolved names only to
`FunctionDef` and reported `t15_operand_provenance.Stream` (a class) as
missing.

The gate is NOT VACUOUS: `test_the_checker_fires_on_a_module_that_builds`
and `..._that_writes` run it over synthetic sources that do exactly those
things, so a checker that silently stopped matching would fail here rather
than pass everything.
"""
import ast
import re
import sys
import unittest
from pathlib import Path

GDL = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GDL))

# The marker must START a line: `t13_importable_census.py` DOCUMENTS the
# convention in prose ("carrying IMPORTABLE CORE: ..."), and a mid-sentence
# match turns half a paragraph into "advertised names".
MARKER = re.compile(r"^\s*IMPORTABLE CORE:(.*?)(?:—|--)", re.S | re.M)

BUILD_TOKENS = ("ninja", "configure.py", "mwcceppc", "mwldeppc")
SUBPROCESS_CALLS = {"run", "check_output", "check_call", "Popen", "call",
                    "system"}
# Only unambiguous filesystem writers. `replace`/`copy`/`remove` are str,
# dict and list methods first — see the module docstring.
WRITE_CALLS = {"write_text", "write_bytes", "mkdir", "unlink", "rmtree",
               "rename", "copyfile", "copy2", "utime", "touch", "makedirs"}
WRITE_MODES = {"w", "wb", "a", "ab", "w+", "r+"}


def advertised(docstring):
    """Identifier-shaped names one IMPORTABLE CORE line claims."""
    match = MARKER.search(docstring or "")
    if not match:
        return []
    names = [name.strip() for name in
             re.split(r",|\band\b", match.group(1).replace("\n", " "))]
    return [name for name in names if name.isidentifier()]


def impurities(source):
    """{advertised name: [finding, ...]} for one module's source text."""
    tree = ast.parse(source)
    names = advertised(ast.get_docstring(tree) or "")
    definitions = {
        node.name: node for node in tree.body
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef,
                             ast.ClassDef))
    }
    findings = {}
    for name in names:
        node = definitions.get(name)
        if node is None:
            findings[name] = ["NOT A MODULE-LEVEL DEFINITION"]
            continue
        seen, frontier, hits = {name}, [node], []
        while frontier:
            for sub in ast.walk(frontier.pop()):
                if isinstance(sub, ast.Call):
                    func = sub.func
                    label = (func.attr if isinstance(func, ast.Attribute)
                             else getattr(func, "id", ""))
                    if label in definitions and label not in seen:
                        seen.add(label)
                        frontier.append(definitions[label])
                    if label in WRITE_CALLS:
                        hits.append(f"WRITE:{label}")
                    if label == "open":
                        for arg in list(sub.args[1:]) + [
                                kw.value for kw in sub.keywords]:
                            if (isinstance(arg, ast.Constant)
                                    and arg.value in WRITE_MODES):
                                hits.append("WRITE:open")
                    if label in SUBPROCESS_CALLS:
                        dumped = ast.dump(sub)
                        if any(token in dumped for token in BUILD_TOKENS):
                            hits.append(f"BUILD:{label}")
        if hits:
            findings[name] = sorted(set(hits))
    return findings


def marked_modules():
    for base in (GDL, GDL / "composed_census"):
        for path in sorted(base.glob("*.py")):
            source = path.read_text(encoding="utf-8", errors="replace")
            try:
                tree = ast.parse(source)
            except SyntaxError:
                continue
            if advertised(ast.get_docstring(tree) or ""):
                yield path, source


class ImportableCorePurity(unittest.TestCase):

    def test_the_marker_is_in_use_where_this_gate_can_see_it(self):
        paths = [path.name for path, _ in marked_modules()]
        self.assertGreaterEqual(len(paths), 10, paths)
        for expected in ("fndiff.py", "slotdiff.py", "savedregs.py",
                         "defake_gate.py", "nearmiss.py"):
            self.assertIn(expected, paths)

    def test_no_advertised_function_reaches_a_build(self):
        offenders = {}
        for path, source in marked_modules():
            for name, hits in impurities(source).items():
                builds = [hit for hit in hits if hit.startswith("BUILD")]
                if builds:
                    offenders[f"{path.name}::{name}"] = builds
        self.assertEqual(offenders, {},
                         "IMPORTABLE CORE promises 'no build'")

    def test_no_advertised_function_reaches_a_write(self):
        offenders = {}
        for path, source in marked_modules():
            for name, hits in impurities(source).items():
                writes = [hit for hit in hits if hit.startswith("WRITE")]
                if writes:
                    offenders[f"{path.name}::{name}"] = writes
        self.assertEqual(offenders, {},
                         "a library function a sweep calls must not write")

    def test_every_advertised_name_resolves_to_a_definition(self):
        missing = {}
        for path, source in marked_modules():
            for name, hits in impurities(source).items():
                if "NOT A MODULE-LEVEL DEFINITION" in hits:
                    missing[f"{path.name}::{name}"] = hits
        self.assertEqual(missing, {})

    # --- the gate must be able to FIRE ----------------------------------

    BUILDS = '''"""IMPORTABLE CORE: refresh -- pure over parsed data."""
import subprocess


def refresh(unit):
    subprocess.run(["ninja", "build/GUNE5D/report.json"])
    return unit
'''

    WRITES = '''"""IMPORTABLE CORE: bank -- pure over parsed data."""
from pathlib import Path


def bank(text):
    Path("out.json").write_text(text)
'''

    OPENS = '''"""IMPORTABLE CORE: bank -- pure over parsed data."""


def bank(text):
    with open("out.json", "w") as handle:
        handle.write(text)
'''

    INDIRECT = '''"""IMPORTABLE CORE: outer -- pure over parsed data."""
import subprocess


def _inner():
    subprocess.run(["ninja", "x.o"])


def outer():
    return _inner()
'''

    def test_the_checker_fires_on_a_module_that_builds(self):
        self.assertEqual(impurities(self.BUILDS), {"refresh": ["BUILD:run"]})

    def test_the_checker_fires_on_a_module_that_writes(self):
        self.assertEqual(impurities(self.WRITES), {"bank": ["WRITE:write_text"]})

    def test_the_checker_fires_on_an_open_for_writing(self):
        self.assertEqual(impurities(self.OPENS), {"bank": ["WRITE:open"]})

    def test_the_checker_follows_intra_module_calls(self):
        # The interesting half: the advertised function is clean and its
        # private helper is not.
        self.assertEqual(impurities(self.INDIRECT), {"outer": ["BUILD:run"]})

    def test_a_read_only_subprocess_is_allowed(self):
        source = ('"""IMPORTABLE CORE: dump -- pure."""\n'
                  "import subprocess\n\n\n"
                  "def dump(path):\n"
                  "    return subprocess.run(['objdump', '-dr', path])\n")
        self.assertEqual(impurities(source), {})

    def test_prose_mentioning_the_marker_advertises_nothing(self):
        # t13_importable_census.py documents the convention in a sentence.
        source = ('"""A tool. It reports whether each module is carrying'
                  ' IMPORTABLE CORE: names, and whether the import prints'
                  ' -- see AGENTS.md."""\n')
        self.assertEqual(advertised(ast.get_docstring(ast.parse(source))), [])


if __name__ == "__main__":
    unittest.main()
