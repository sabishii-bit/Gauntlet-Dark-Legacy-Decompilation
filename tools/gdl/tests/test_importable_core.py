"""The IMPORTABLE CORE convention (run-43 item 10).

THE OBSERVATION. A lane sweeping N functions has two ways to use a tool:
spawn it N times, or import it once and call its functions. Which tools
support the second was readable only by reading the source, so sweeps
spawned — T12 measured a subprocess per function where two object parses
would have done. slotdiff and savedregs are the precedent: both are pure
over `fndiff.parse`/`fnasm.parse_fn` row lists and neither needs a build.

MEASURED before writing the convention, over all 62 modules in tools/gdl
(tools/gdl/composed_census/t13_importable_census.py, each imported in a
fresh subprocess with a bare argv — re-run it to re-measure):

    importable and silent   51
    importable but PRINTS    9   abicheck, add_remat_census, addr16_census,
                                 addr16_homing_census, addrlo_dest_census,
                                 addrlo_home_general_census,
                                 addrlo_inplace_census, addrlo_shadow_probe,
                                 build_rule
    NOT importable           2   pdb20_dump (opens the PDB at import),
                                 splice_rules (opens a rule file at import)

So the property is nearly universal and the gap was DISCOVERABILITY, not
capability. The convention is one grep-able docstring line naming the
functions a caller may use; this test keeps every such line honest — the
module must import silently and define every name it advertises.
"""

import contextlib
import importlib
import io
import re
import sys
import unittest
from pathlib import Path

GDL = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(GDL))

MARKER = re.compile(r"IMPORTABLE CORE:(.*?)(?:—|--)", re.S)


def advertised(text):
    """The function names one IMPORTABLE CORE line claims."""
    match = MARKER.search(text)
    if not match:
        return []
    return [name.strip() for name in
            re.split(r",|\band\b", match.group(1).replace("\n", " "))
            if name.strip()]


class ImportableCoreTests(unittest.TestCase):
    def modules_with_marker(self):
        """Every tools/gdl module whose MODULE DOCSTRING carries the line."""
        for path in sorted(GDL.glob("*.py")):
            text = path.read_text(encoding="utf-8", errors="replace")
            parts = text.split('"""')
            docstring = parts[1] if len(parts) >= 3 else ""
            if "IMPORTABLE CORE:" in docstring:
                yield path, docstring

    def test_the_convention_is_actually_in_use(self):
        marked = [path.name for path, _ in self.modules_with_marker()]
        self.assertGreaterEqual(len(marked), 5, marked)
        for expected in ("fndiff.py", "slotdiff.py", "savedregs.py",
                         "defake_gate.py", "nearmiss.py"):
            self.assertIn(expected, marked)

    def test_every_advertised_name_exists_and_is_callable(self):
        for path, text in self.modules_with_marker():
            names = advertised(text)
            self.assertTrue(names, path.name)
            module = importlib.import_module(path.stem)
            for name in names:
                self.assertTrue(hasattr(module, name),
                                f"{path.name} advertises {name!r}")
                self.assertTrue(callable(getattr(module, name)),
                                f"{path.name}.{name} is not callable")

    def test_the_parser_extracts_real_names(self):
        """A gate that silently parses zero names cannot fire at all."""
        names = advertised("IMPORTABLE CORE: alpha, beta and gamma — pure")
        self.assertEqual(names, ["alpha", "beta", "gamma"])
        self.assertEqual(advertised("no marker here"), [])
        by_module = {path.name: advertised(text)
                     for path, text in self.modules_with_marker()}
        self.assertIn("unit_key", by_module["fndiff.py"])
        self.assertIn("slot_map", by_module["slotdiff.py"])

    def test_importing_a_marked_module_prints_nothing(self):
        """The half a caller cannot work around: a module that does work at
        import cannot be used as a library at all."""
        for path, _text in self.modules_with_marker():
            buffer = io.StringIO()
            argv = sys.argv
            sys.argv = [path.stem]      # a bare argv, as a library import has
            try:
                with contextlib.redirect_stdout(buffer):
                    importlib.reload(importlib.import_module(path.stem))
            finally:
                sys.argv = argv
            self.assertEqual(buffer.getvalue(), "", path.name)


if __name__ == "__main__":
    unittest.main()
