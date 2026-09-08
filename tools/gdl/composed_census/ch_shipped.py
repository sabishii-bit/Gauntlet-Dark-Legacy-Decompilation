"""List every shipped WebFrank rule: unit, function and its stages.

Takes no arguments except an optional repo-relative `--out PATH`. Writes
ch_shipped.json under build/GUNE5D/composed_census/ -- generated data, never
beside this source (run-61 item 6) -- which ch_roster.py, ch_harvest.py and
ch_sweep26.py read.

RUN 62: postprocessing retirement deleted config/GUNE5D/webfrank.json, so
this tool has no input on a native-only tree. It used to end in a raw
`FileNotFoundError` traceback, which reads as a broken tool rather than a
retired measurement; it now REFUSES, names the retirement and points at the
Git history where the rules remain readable.
"""
import json, os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
# Run-59 item 9: ROOT was `HERE/..` = tools/gdl, so the config read raised
# FileNotFoundError and this script could not run at all from its promoted
# location.
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
import cc_artifact  # noqa: E402
import cliscreen  # noqa: E402
cliscreen.help_only(__doc__)
CONFIG = os.path.join(ROOT, "config", "GUNE5D", "webfrank.json")
if not os.path.exists(CONFIG):
    raise SystemExit(
        "ch_shipped.py has no input: config/GUNE5D/webfrank.json was removed"
        " when build postprocessing was retired.\n"
        "There are no shipped rules to list on a native-only tree. Read the"
        " retired rule set from Git history at %s.\n"
        "Expected at: %s" % (cc_artifact.RULE_HISTORY_REF, CONFIG))
d = json.load(open(CONFIG))
n = 0
names = []
for unit, rules in sorted(d["units"].items()):
    for r in rules:
        n += 1
        stages = [k for k in r if k in ("instruction_permutation", "equivalent_copy_form",
                                        "copy_register_fields", "unproven_recolor_audit")]
        names.append((unit, r["function"], stages))
print(f"SHIPPED RULES: {n} across {len(d['units'])} units\n")
for u, f, s in names:
    print(f"  {u:34} {f:36} {'+'.join(s)}")
written = cc_artifact.write_artifact("ch_shipped.json",
                                     [f for _u, f, _s in names],
                                     cc_artifact.out_override(sys.argv),
                                     indent=None)
print(f"\nwrote {cc_artifact.artifact_label(written)}")
