"""Which `apply_patch` call sites prove rules at FULL datum strength?

RUN-56 ITEM 7, from WS's run-55 report: "ha_close.prove() calls apply_patch
without the retail image, so the datum screen silently degrades". Reproduced
and priced.

`webfrank.apply_patch(data, patch, target_data, symbol_addresses, image)`
takes the retail image as its FIFTH argument. Without it the L3 DATUM level
of `verify_datum_binding` cannot run and every word it would have decided
falls through to L4, the pool CORRESPONDENCE -- a one-to-one map that says
nothing about which datum each end holds, which is exactly the hole
claim.law.CQ_copy-register-fields-can-rotate-constant-load-homes-without-
their-relocations.20260903.v1 records. Run 44 fixed ONE call site
(wr_try_rule.py) and wrote the mechanism down; nothing stopped the other
twenty-two from staying as they were, and nothing would have stopped a
twenty-fourth.

MEASURED at 5366a3a2f, by running all 162 SHIPPED webfrank rules through
`apply_patch` twice -- once with the image and once without, everything else
identical:

    DEGRADE without the image   59 rules (36%)
    unchanged                   98
    not comparable               5 (before-hash mismatch on the raw body)

So this is not an edge case: better than a third of the shipped corpus loses
datum strength on an image-less call, silently, with the same BYTE-EQUAL
verdict printed either way. The visible difference is one line:

    WEBFRANK pbProjCalc: datum binding proved (name 21, address 0, datum 0);
    30 word(s) rest on the pool correspondence alone (uninitialised data)

CALL-SITE CENSUS at the same commit, by AST (so a mention in a comment is
not a call and a call split over lines still counts): 26 sites, 3 pass the
image (webfrank's own main, t16_rederive_body, wr_try_rule) and 23 do not.
This run fixes ha_close.py, the reported one. The remaining 22 are DECLARED
DEBT below rather than quietly fixed en masse: they belong to several lanes'
derivation workflows, a blind edit to a rule-authoring path is exactly the
kind of change that should be made by someone measuring its output, and a
debt list a test pins can only shrink.

Exit 1 when a call site is neither in the clean set nor in the debt list --
i.e. when a NEW image-less call appears.

IMPORTABLE CORE: call_sites, classify -- pure over parsed source, no build.
"""
from __future__ import annotations

import argparse
import ast
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent.parent
ROOTS = ("tools/gdl", "tools/gdl/composed_census", "memory_graph")

# Sites that pass the image. Kept as data so the audit reports a REGRESSION
# (a site that stops passing it) as loudly as a new omission.
CLEAN = {
    "tools/gdl/webfrank.py",
    "tools/gdl/composed_census/t16_rederive_body.py",
    "tools/gdl/composed_census/wr_try_rule.py",
    "tools/gdl/composed_census/ha_close.py",
}

# The 22 image-less sites inherited at 5366a3a2f. Each proves rules with the
# datum screen at L4. Removing a name from this list is the fix; adding one
# needs the same justification any new debt does.
KNOWN_DEBT = {
    "tools/gdl/build_rule.py",
    "tools/gdl/composed_census/build_rule_pw.py",
    "tools/gdl/composed_census/ch_derive.py",
    "tools/gdl/composed_census/ch_harvest.py",
    "tools/gdl/composed_census/cn_derive.py",
    "tools/gdl/composed_census/cn_detail.py",
    "tools/gdl/composed_census/cn_final.py",
    "tools/gdl/composed_census/cn_search.py",
    "tools/gdl/composed_census/wc_control_check.py",
    "tools/gdl/composed_census/wc_try_permute2.py",
    "tools/gdl/composed_census/wc_unproven_sweep.py",
    "tools/gdl/composed_census/wc_ve_derive.py",
    "tools/gdl/composed_census/wc_zerofield_probe.py",
    "tools/gdl/composed_census/wf_livezero_derive.py",
    "tools/gdl/composed_census/wf_mkrule.py",
    "tools/gdl/composed_census/wr_ve_derive.py",
    "tools/gdl/composed_census/wz_derive.py",
    "tools/gdl/composed_census/wz_try_compose.py",
}


def call_sites(root: Path = REPO):
    """[(repo_relative_path, lineno, positional_count, keywords, has_image)]"""
    rows = []
    for prefix in ROOTS:
        directory = root / prefix
        if not directory.is_dir():
            continue
        for path in sorted(directory.glob("*.py")):
            try:
                tree = ast.parse(path.read_text(encoding="utf-8",
                                                errors="replace"))
            except SyntaxError:
                continue
            for node in ast.walk(tree):
                if not isinstance(node, ast.Call):
                    continue
                func = node.func
                name = (func.attr if isinstance(func, ast.Attribute)
                        else getattr(func, "id", ""))
                if name != "apply_patch":
                    continue
                keywords = sorted(k.arg for k in node.keywords if k.arg)
                has_image = len(node.args) >= 5 or "image" in keywords
                rows.append((path.relative_to(root).as_posix(), node.lineno,
                             len(node.args), keywords, has_image))
    return rows


def classify(rows):
    """(clean, debt, unexpected_omission, unexpected_regression)."""
    clean, debt, new_omission, regression = [], [], [], []
    for row in rows:
        path, _line, _n, _kw, has_image = row
        if has_image:
            clean.append(row)
            if path in KNOWN_DEBT:
                regression.append(row)   # listed as debt but now passes: fix
                                         # the list, not the code
        elif path in KNOWN_DEBT:
            debt.append(row)
        else:
            new_omission.append(row)
    return clean, debt, new_omission, regression


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--list", action="store_true",
                        help="print every call site, not only the failures")
    args = parser.parse_args(argv)

    rows = call_sites()
    clean, debt, new_omission, regression = classify(rows)
    print(f"apply_patch call sites: {len(rows)}")
    print(f"  image passed (full-strength datum screen): {len(clean)}")
    print(f"  known debt (datum screen at L4):           {len(debt)}")
    print(f"  UNDECLARED image-less call(s):             {len(new_omission)}")
    print(f"  listed as debt but now passing the image:  {len(regression)}")
    if args.list:
        for path, line, n, keywords, has_image in rows:
            mark = "image" if has_image else "L4   "
            print(f"    {mark}  {path}:{line}  positional={n}"
                  f" keywords={keywords}")
    for path, line, _n, _kw, _i in new_omission:
        print(f"UNDECLARED: {path}:{line} calls apply_patch without the"
              " retail image; the L3 datum level cannot run. Pass the image"
              " (see wr_try_rule.py) or add the file to KNOWN_DEBT with a"
              " reason.")
    for path, line, _n, _kw, _i in regression:
        print(f"STALE DEBT ENTRY: {path}:{line} now passes the image; remove"
              " it from KNOWN_DEBT.")
    if not new_omission and not regression:
        print("VERDICT: every call site is accounted for.")
    return 1 if (new_omission or regression) else 0


if __name__ == "__main__":
    sys.exit(main())
