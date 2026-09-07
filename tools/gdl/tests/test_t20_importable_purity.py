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

RUN-58 ITEM 8: THE CHECKER SCREENED BY NAME, NOT BY RESOLUTION. The write
set is a set of SPELLINGS, and it was matched against the spelling of the
call regardless of what that spelling is bound to. Reproduced with a
minimal module (build/T2_item8_repro.py at 7d5d509ba):

    def rename(table, name):        # a dict.get, module-level
        return table.get(name, name)
    def resolve_name(name):         # the advertised function
        return rename(ALIASES, name)

  ->  {'resolve_name': ['WRITE:rename']}

`regnorm.resolve_name` and `fndiff` both carry a helper of exactly this
shape (a dtk `_80XXXXXX` suffix resolver), so the next module to advertise
one fails the gate for calling a dictionary. THE SAME ROOT CAUSE HAS A
FALSE-NEGATIVE HALF, found while reproducing and equally measured:

    from shutil import rmtree as nuke
    def bank(path):
        nuke(path)          ->  {}    (a real rmtree, unseen)

A bare Name call is now RESOLVED against the module's own bindings before
it is classified:

  a module-level def/class     an intra-module callee: followed, never
                               classified by its spelling
  an import of a writer        classified under the CANONICAL name, so the
                               alias above reads `WRITE:rmtree`
  any other module binding     the stdlib spelling is SHADOWED: not a write
  unbound (`open`, builtins)   classified by spelling, exactly as before

An ATTRIBUTE call (`os.rename`, `Path(p).write_text`) is still classified
by its attribute name: a module-level def cannot shadow a method.

TWO-SIDED CALIBRATION at 7d5d509ba over the live population, 25 marked
modules across tools/gdl and composed_census:
  live offenders BEFORE the fix   0   (the live tree never tripped it)
  live offenders AFTER the fix    0   (unchanged -- no live verdict moves)
  the shadowed-helper module      1 -> 0 hits
  the aliased-rmtree module       0 -> 1 hit (`WRITE:rmtree`)
  `from os import rename`         1 -> 1 hit, still `WRITE:rename`
and every pre-existing FIRE test (build, write, open-for-writing, indirect
helper) is unchanged, which is the invalid-input half: a resolver that
resolved everything away would fail those four.
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


# The spellings whose RESOLUTION is worth tracking through an import or a
# module-level alias. `open` joins the two call sets because `from io import
# open` rebinds the builtin this checker classifies by name.
INTERESTING_CALLS = WRITE_CALLS | SUBPROCESS_CALLS | {"open"}


def _terminal_identifier(value):
    """The last identifier of an import/assignment source, or None.

    `shutil.rmtree` -> "rmtree"; `rmtree` -> "rmtree"; a call or a literal
    -> None (nothing to attribute a write to).
    """
    if isinstance(value, ast.Attribute):
        return value.attr
    if isinstance(value, ast.Name):
        return value.id
    return None


def module_bindings(tree):
    """{bound name: canonical writer spelling or None} at module level.

    Run-58 item 8. This is the RESOLUTION step the checker was missing.
    A value of None means "bound here to something that is not a known
    writer" -- the stdlib spelling is shadowed, so a call through that name
    is not a write. A string value is the canonical spelling to report,
    which is what turns `from shutil import rmtree as nuke` into
    `WRITE:rmtree` instead of nothing at all.
    """
    binds = {}
    for node in tree.body:
        if isinstance(node, ast.Import):
            for alias in node.names:
                binds[alias.asname or alias.name.split(".")[0]] = None
        elif isinstance(node, ast.ImportFrom):
            for alias in node.names:
                binds[alias.asname or alias.name] = (
                    alias.name if alias.name in INTERESTING_CALLS else None)
        elif isinstance(node, (ast.Assign, ast.AnnAssign)):
            targets = (node.targets if isinstance(node, ast.Assign)
                       else [node.target])
            source = _terminal_identifier(node.value)
            for target in targets:
                if isinstance(target, ast.Name):
                    binds[target.id] = (
                        source if source in INTERESTING_CALLS else None)
    return binds


def impurities(source):
    """{advertised name: [finding, ...]} for one module's source text."""
    tree = ast.parse(source)
    names = advertised(ast.get_docstring(tree) or "")
    definitions = {
        node.name: node for node in tree.body
        if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef,
                             ast.ClassDef))
    }
    bindings = module_bindings(tree)
    findings = {}
    for name in names:
        node = definitions.get(name)
        if node is None:
            findings[name] = ["NOT A MODULE-LEVEL DEFINITION"]
            continue
        seen, frontier, hits = {name}, [node], []
        while frontier:
            for sub in ast.walk(frontier.pop()):
                if not isinstance(sub, ast.Call):
                    continue
                func = sub.func
                if isinstance(func, ast.Attribute):
                    # A method call. No module-level definition can shadow
                    # it, so the attribute spelling IS the resolution.
                    label = resolved = func.attr
                else:
                    label = getattr(func, "id", "")
                    if label in definitions:
                        # RESOLVED to this module's own function. Follow it
                        # and never classify it by its spelling: a helper
                        # called `rename` that reads a dict scored
                        # WRITE:rename for weeks (run-58 item 8).
                        if label not in seen:
                            seen.add(label)
                            frontier.append(definitions[label])
                        continue
                    resolved = bindings.get(label, label)
                if resolved in WRITE_CALLS:
                    hits.append(f"WRITE:{resolved}")
                if resolved == "open":
                    for arg in list(sub.args[1:]) + [
                            kw.value for kw in sub.keywords]:
                        if (isinstance(arg, ast.Constant)
                                and arg.value in WRITE_MODES):
                            hits.append("WRITE:open")
                if resolved in SUBPROCESS_CALLS:
                    dumped = ast.dump(sub)
                    if any(token in dumped for token in BUILD_TOKENS):
                        hits.append(f"BUILD:{resolved}")
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

    # --- run-58 item 8: NAME vs RESOLUTION, both sides ------------------

    SHADOWED = '''"""IMPORTABLE CORE: resolve_name -- pure over parsed data."""

ALIASES = {"gendir_8004FBC8": "gendir"}


def rename(table, name):
    return table.get(name, name)


def resolve_name(name):
    return rename(ALIASES, name)
'''

    ALIASED_WRITER = '''"""IMPORTABLE CORE: bank -- pure over parsed data."""
from shutil import rmtree as nuke


def bank(path):
    nuke(path)
'''

    FROM_IMPORTED_WRITER = '''"""IMPORTABLE CORE: bank -- pure over parsed data."""
from os import rename


def bank(a, b):
    rename(a, b)
'''

    ASSIGNED_WRITER = '''"""IMPORTABLE CORE: bank -- pure over parsed data."""
import shutil

drop = shutil.rmtree


def bank(path):
    drop(path)
'''

    ALIASED_BUILDER = '''"""IMPORTABLE CORE: refresh -- pure over parsed data."""
from subprocess import run as spawn


def refresh(unit):
    spawn(["ninja", "build/GUNE5D/report.json"])
    return unit
'''

    def test_a_module_level_helper_is_not_a_write_because_of_its_name(self):
        # THE VALID INPUT. `rename` here is a dict.get; the old checker
        # matched the SPELLING and reported WRITE:rename.
        self.assertEqual(impurities(self.SHADOWED), {})

    def test_an_aliased_import_of_a_writer_still_fires(self):
        # THE INVALID INPUT, and the false-negative half of the same root
        # cause: reported under the CANONICAL name, not the alias.
        self.assertEqual(impurities(self.ALIASED_WRITER),
                         {"bank": ["WRITE:rmtree"]})

    def test_a_from_import_of_a_writer_still_fires(self):
        self.assertEqual(impurities(self.FROM_IMPORTED_WRITER),
                         {"bank": ["WRITE:rename"]})

    def test_a_module_level_alias_of_a_writer_still_fires(self):
        self.assertEqual(impurities(self.ASSIGNED_WRITER),
                         {"bank": ["WRITE:rmtree"]})

    def test_an_aliased_subprocess_that_builds_still_fires(self):
        self.assertEqual(impurities(self.ALIASED_BUILDER),
                         {"refresh": ["BUILD:run"]})

    def test_resolution_does_not_move_any_live_verdict(self):
        # The calibration in the module docstring, asserted: the live tree
        # was clean before this change and must still be clean after it,
        # so the resolver cannot have been fitted to the live population.
        offenders = {}
        for path, source in marked_modules():
            for name, hits in impurities(source).items():
                offenders[f"{path.name}::{name}"] = hits
        self.assertEqual(offenders, {})

    def test_bindings_report_the_canonical_writer_and_shadow_the_rest(self):
        tree = ast.parse(self.ALIASED_WRITER)
        self.assertEqual(module_bindings(tree), {"nuke": "rmtree"})
        tree = ast.parse(self.SHADOWED)
        self.assertEqual(module_bindings(tree), {"ALIASES": None})

    def test_prose_mentioning_the_marker_advertises_nothing(self):
        # t13_importable_census.py documents the convention in a sentence.
        source = ('"""A tool. It reports whether each module is carrying'
                  ' IMPORTABLE CORE: names, and whether the import prints'
                  ' -- see AGENTS.md."""\n')
        self.assertEqual(advertised(ast.get_docstring(ast.parse(source))), [])


if __name__ == "__main__":
    unittest.main()
