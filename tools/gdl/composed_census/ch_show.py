import json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cc_artifact  # noqa: E402
rows = cc_artifact.load_artifact("ch_sweep26.json", "ch_show.py")
for r in rows:
    if r["closes"] and not r["shipped"]:
        print(f"=== {r['unit']}::{r['function']}  owned={r['owned']}  window {r['lo']}..{r['hi']}")
        print(json.dumps(r["rule"], indent=1))
        print()
