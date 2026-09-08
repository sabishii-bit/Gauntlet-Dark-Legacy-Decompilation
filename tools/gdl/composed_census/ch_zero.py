"""List census rows whose positional 'other' count is zero, from ch_census26.

Reads build/GUNE5D/composed_census/ch_census26.json (regenerate with
python tools/gdl/composed_census/ch_census26.py).
"""
import json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(HERE))
import cliscreen  # noqa: E402
cliscreen.help_only(__doc__)  # the artifact load below runs at import time
import cc_artifact  # noqa: E402
d = cc_artifact.load_artifact("ch_census26.json", "ch_zero.py")
print("totals:", d["totals"])
t = d["totals"]
print("positional combined =", t.get("fwd_rc",0)+t.get("inv_rc",0))
print("r0-corrected WF figure would be 44/47 = 91; mine 41/42 = 83; delta 3+5=8\n")
zero = [r for r in d["rows"] if r["counts"].get("other", 0) == 0]
print(f"FUNCTIONS WITH other == 0 POSITIONALLY: {len(zero)}")
for r in sorted(zero, key=lambda r: r["insns"]):
    print(f"  {r['unit']:30} {r['function']:34} ins={r['insns']:5} {r['counts']}")
