import json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
rows = json.load(open(os.path.join(HERE, "ch_sweep26.json")))
for r in rows:
    if r["closes"] and not r["shipped"]:
        print(f"=== {r['unit']}::{r['function']}  owned={r['owned']}  window {r['lo']}..{r['hi']}")
        print(json.dumps(r["rule"], indent=1))
        print()
