"""Print the closable-but-unshipped rules from the ch_sweep26 census artifact.

Reads build/GUNE5D/composed_census/ch_sweep26.json (regenerate with
python tools/gdl/composed_census/ch_sweep26.py).
"""
import json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(HERE))
import cliscreen  # noqa: E402
cliscreen.help_only(__doc__)  # the artifact load below runs at import time
import cc_artifact  # noqa: E402
rows = cc_artifact.load_artifact("ch_sweep26.json", "ch_show.py")
for r in rows:
    if r["closes"] and not r["shipped"]:
        print(f"=== {r['unit']}::{r['function']}  owned={r['owned']}  window {r['lo']}..{r['hi']}")
        print(json.dumps(r["rule"], indent=1))
        print()
