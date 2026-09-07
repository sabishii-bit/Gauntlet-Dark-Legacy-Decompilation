"""List every shipped WebFrank rule: unit, function and its stages.

Takes no arguments. Also writes ch_shipped.json beside this script, which
ch_roster.py and ch_harvest.py read.
"""
import json, os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# Run-59 item 9: ROOT was `HERE/..` = tools/gdl, so the config read raised
# FileNotFoundError and this script could not run at all from its promoted
# location.
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
import cliscreen  # noqa: E402
cliscreen.help_only(__doc__)
d = json.load(open(os.path.join(ROOT, "config", "GUNE5D", "webfrank.json")))
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
json.dump([f for _u, f, _s in names], open(os.path.join(HERE, "ch_shipped.json"), "w"))
