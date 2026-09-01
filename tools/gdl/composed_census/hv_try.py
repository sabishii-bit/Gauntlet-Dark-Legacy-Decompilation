"""Try an explicit window/order (or a small search over them) through the
SHIPPED apply_patch.  Usage:
    hv_try.py <unit> <fn> pre|post lo:hi:o,o,o [lo:hi:o,o,o ...]
"""
import json
import os
import sys
HERE = os.path.dirname(os.path.abspath(__file__))
# Repo root, located by landmark so this file runs unchanged from the
# lane scratch directory AND from tools/gdl/composed_census after
# promotion (discipline 17: a promoted script must actually run).
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    parent = os.path.dirname(ROOT)
    if parent == ROOT:
        raise SystemExit("repo root not found above " + HERE)
    ROOT = parent
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))
import ha_close as ha  # noqa: E402

unit, fn, placement = sys.argv[1], sys.argv[2], sys.argv[3]
wins = []
for spec in sys.argv[4:]:
    lo, hi, order = spec.split(":")
    wins.append({"lo": int(lo, 0), "hi": int(hi, 0),
                 "order": [int(x) for x in order.split(",")]})
kw = {"pre": wins} if placement == "pre" else {"post": wins}
rule, resid, note = ha.build_rule(unit, fn, **kw)
print(f"{unit}::{fn}: {'CLOSES' if rule else 'refuses'} -- {note}")
if rule:
    print(json.dumps(rule, indent=1))
