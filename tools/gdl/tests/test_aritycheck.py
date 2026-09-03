"""aritycheck.py — the parameter-COUNT screen.

Why it exists: two accepted laws describe the same source configuration and
prescribe opposite fixes, and the discriminant between them is whether the
callee READS the trailing parameter — a question neither externcheck (type
classes) nor abicheck (GPR/FPR sequence assignment) asks.

  claim.law.NM_an-unread-trailing-parameter-is-invisible-in-the-callee-and-
  costs-the-caller-an-instruction.20260903.v1  (drop the parameter)
  claim.law.knr-extern-arity-can-be-faithful-not-a-defect.20260831.v1
  (leave it alone)

Both laws' proven instances are the calibration cases below: SetEnemyObj
(4 declared, trailing `unused`, a 3-argument sibling call — real 27 -> 10
in one build, then STRICT exact) and sndFxInit (2 declared, `wave` read,
attract.c:1107 passing one argument under a K&R extern — prototyping it
regressed init_attract_mode real 210 -> 211).

Live-corpus calibration, run 42 (src/ + include/, 285 + 114 files):
4 PHANTOM-CANDIDATE, 15 KNR-SHORT-CALL, 42 UNREAD-TRAILING. Getting there
took four measured false-positive classes out of the screen — see the
regression tests below; the first draft reported 16 KNR rows of which 14
were parse artifacts.
"""

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import aritycheck  # noqa: E402


def screen(**files):
    """Run the whole screen over an in-memory tree."""
    definitions, declarations, calls = [], [], []
    address_taken = set()
    for name, text in files.items():
        path = name.replace("__", "/")
        d, p, c, a = aritycheck.scan_file(path,
                                          aritycheck.strip_noise(text))
        definitions += d
        declarations += p
        calls += c
        address_taken |= a
    return aritycheck.analyse(definitions, declarations, calls,
                             address_taken)


class SetEnemyObjCalibrationTests(unittest.TestCase):
    """The NM law's proven instance, at the shape the record records it.

    src/game/game/gamemain.c:837 declared four parameters with the last
    named `unused`; src/game/enemy/enemy.c:5269 already passed three. That
    combination is the whole finding, and it must survive as a test because
    the tree no longer contains it — the arity was fixed at c8229d5ee.
    """

    GAMEMAIN = """
void SetEnemyObj(Enemy* enemy, s32 type, s32 level, s32 unused)
{
    enemy->type = type;
    enemy->level = level;
}
"""
    ENEMY = """
void init_enemy(Enemy* e, s32 type, s32 level)
{
    SetEnemyObj(e, type, level);
}
"""

    def test_the_proven_customer_is_the_top_verdict(self):
        rows = screen(gamemain_c=self.GAMEMAIN, enemy_c=self.ENEMY)
        self.assertEqual(1, len(rows), rows)
        self.assertEqual("PHANTOM-CANDIDATE", rows[0]["verdict"])
        self.assertEqual("SetEnemyObj", rows[0]["function"])
        self.assertEqual("unused", rows[0]["trailing_parameter"])
        self.assertEqual(0, rows[0]["trailing_reads_in_body"])
        self.assertEqual(1, len(rows[0]["short_call_sites"]))

    def test_the_fixed_tree_reports_nothing(self):
        """After c8229d5ee the definition takes three and the call passes
        three; the screen must go quiet or it is not a screen."""
        fixed = self.GAMEMAIN.replace(", s32 unused", "")
        self.assertEqual([], screen(gamemain_c=fixed, enemy_c=self.ENEMY))

    def test_a_trailing_parameter_that_IS_read_is_not_the_phantom_class(self):
        used = self.GAMEMAIN.replace("enemy->level = level;",
                                     "enemy->level = level + unused;")
        rows = screen(gamemain_c=used, enemy_c=self.ENEMY)
        self.assertEqual(["KNR-SHORT-CALL"], [r["verdict"] for r in rows])


class SndFxInitCalibrationTests(unittest.TestCase):
    """The KNR law's proven instance — and the case the first draft's
    declaration/call discriminant made invisible: every one of attract.c's
    six sndFxInit CALL sites was filed as a declaration because it ends in
    a semicolon, so the function never reached the census at all."""

    SNDFX = """
void sndFxInit(s32 mode, s32 wave)
{
    gMode = mode;
    gWave = wave;
}
"""
    ATTRACT = """
extern void sndFxInit();
void init_attract_mode(void)
{
    sndFxInit(0x8009, -1);
    sndFxInit(0x8008);
}
"""

    def test_the_short_call_is_seen_as_a_call_not_a_declaration(self):
        rows = screen(sndfx_c=self.SNDFX, attract_c=self.ATTRACT)
        self.assertEqual(["KNR-SHORT-CALL"], [r["verdict"] for r in rows])
        self.assertEqual(2, rows[0]["call_sites"])
        self.assertEqual(1, len(rows[0]["short_call_sites"]))

    def test_the_unprototyped_declaration_is_reported_as_evidence(self):
        rows = screen(sndfx_c=self.SNDFX, attract_c=self.ATTRACT)
        self.assertEqual(1, len(rows[0]["unprototyped_declarations"]))

    def test_a_prototype_is_not_counted_as_a_call_site(self):
        prototyped = self.ATTRACT.replace(
            "extern void sndFxInit();",
            "extern void sndFxInit(s32 mode, s32 wave);")
        rows = screen(sndfx_c=self.SNDFX, attract_c=prototyped)
        self.assertEqual(2, rows[0]["call_sites"])
        self.assertEqual([], rows[0]["unprototyped_declarations"])


class ArgumentCountingTests(unittest.TestCase):
    def test_a_string_literal_argument_still_counts(self):
        """MEASURED DEFECT: blanking literals left an argument that was pure
        whitespace, the empty-token filter dropped it, and
        `strncmp(sig, "VBNK", 4)` read as two arguments — which put strncmp,
        strcmp, strchr and memcmp into the census on a parse bug."""
        text = aritycheck.strip_noise('f(sig, "VBNK", 4);')
        inner = text[text.index("(") + 1:text.rindex(")")]
        self.assertEqual(3, aritycheck.argument_count(inner))

    def test_a_char_literal_argument_still_counts(self):
        text = aritycheck.strip_noise("f(path, '.');")
        inner = text[text.index("(") + 1:text.rindex(")")]
        self.assertEqual(2, aritycheck.argument_count(inner))

    def test_a_comma_inside_a_nested_call_does_not_split(self):
        self.assertEqual(2, aritycheck.argument_count("a, g(b, c)"))

    def test_void_and_empty_are_both_zero(self):
        self.assertEqual(0, aritycheck.argument_count(""))
        self.assertEqual(0, aritycheck.argument_count("void"))

    def test_a_function_pointer_parameter_is_one_argument(self):
        self.assertEqual(2,
                         aritycheck.argument_count("s32 n, void (*cb)(int)"))


class ParameterNameTests(unittest.TestCase):
    def test_names_are_read_through_pointers_and_arrays(self):
        self.assertEqual("p", aritycheck.parameter_name("char *p"))
        self.assertEqual("v", aritycheck.parameter_name("s32 v[4]"))
        self.assertEqual("cb",
                         aritycheck.parameter_name("void (*cb)(Enemy *)"))

    def test_an_unnamed_parameter_has_no_name(self):
        """An unnamed parameter is unread by construction, so returning the
        TYPE as its name manufactures the strongest verdict the tool has out
        of a declaration that says nothing."""
        self.assertIsNone(aritycheck.parameter_name("s32"))
        self.assertIsNone(aritycheck.parameter_name("Enemy*"))


class ExclusionTests(unittest.TestCase):
    def test_a_macro_body_call_is_not_a_call_site(self):
        """OSPanic entered the first census off two lines of
        src/MSL/placeholder.h: a macro body calls its callee with the
        MACRO's argument list, which says nothing about the callee."""
        rows = screen(
            a_c="void OSPanic(char* f, int l, char* m, int x) { use(x); }",
            b_h="#define ASSERT(c) OSPanic(__FILE__, __LINE__, #c)\n")
        self.assertEqual([], rows)

    def test_a_varargs_definition_is_excluded(self):
        rows = screen(a_c="void p(char* f, ...) { }\nvoid q(void){p(f);}")
        self.assertEqual([], rows)

    def test_an_address_taken_function_is_excluded(self):
        """Its arity is fixed by a function-pointer type and no call site
        can argue with it — the exclusion that removes every one of the
        three sites the NM record's spelling census found."""
        rows = screen(
            a_c="void cb(s32 region, s32 unused) { use(region); }\n"
                "void setup(void) { install(cb); cb(1); }")
        self.assertEqual([], rows)

    def test_a_definition_with_no_call_site_is_not_a_finding(self):
        """The cost of a phantom parameter is paid by CALLERS; with none in
        the tree there is nothing to pay it and nothing to compare against
        (MSL `__close_console`)."""
        rows = screen(a_c="int __close_console(int unused) { return 0; }")
        self.assertEqual([], rows)

    def test_a_comment_mentioning_the_parameter_is_not_a_read(self):
        """init_enemy carried a stale comment documenting the phantom 4th
        argument as faithful; counting it as a read would have hidden the
        finding behind the very prose that caused it."""
        rows = screen(
            a_c="/* unused is faithful, see the target */\n"
                "void f(s32 a, s32 unused) { use(a); }\n"
                "void g(void) { f(1); }")
        self.assertEqual(["PHANTOM-CANDIDATE"], [r["verdict"] for r in rows])


class DeclarationDiscriminantTests(unittest.TestCase):
    def test_a_statement_call_is_a_call(self):
        text = aritycheck.strip_noise("void g(void){ f(1,2); }")
        _d, declarations, calls, _a = aritycheck.scan_file("a.c", text)
        self.assertEqual([("f", 2)], [(c["name"], c["arity"])
                                      for c in calls])
        self.assertEqual([], declarations)

    def test_a_prototype_is_a_declaration(self):
        text = aritycheck.strip_noise("void f(int a, int b);")
        _d, declarations, calls, _a = aritycheck.scan_file("a.c", text)
        self.assertEqual([("f", 2)], [(d["name"], d["arity"])
                                      for d in declarations])
        self.assertEqual([], calls)

    def test_a_call_in_a_return_statement_is_a_call(self):
        text = aritycheck.strip_noise("int g(void){ return f(1); }")
        _d, declarations, calls, _a = aritycheck.scan_file("a.c", text)
        self.assertEqual(["f"], [c["name"] for c in calls])
        self.assertEqual([], declarations)

    def test_a_pointer_returning_definition_is_still_a_definition(self):
        text = aritycheck.strip_noise("Enemy *f(s32 a) { return 0; }")
        definitions, _p, _c, _a = aritycheck.scan_file("a.c", text)
        self.assertEqual([("f", 1)], [(d["name"], d["arity"])
                                      for d in definitions])


if __name__ == "__main__":
    unittest.main()
