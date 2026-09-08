"""Print the webfrank.json entry for one unit/function as a schema template."""
import json
import sys
from pathlib import Path
if '--help' in sys.argv or '-h' in sys.argv:
    print(__doc__); raise SystemExit(0)
if not Path('config/GUNE5D/webfrank.json').exists():
    print('RETIRED: no production rule corpus; inspect Git history.')
    raise SystemExit(2)

d = json.load(open("config/GUNE5D/webfrank.json"))
units = d["units"]
print("version:", d["version"], " units:", len(units), " type:", type(units))
if isinstance(units, dict):
    names = sorted(units)
    print("UNIT NAMES (first 50):")
    for n in names[:50]:
        print("   ", n)
    if len(sys.argv) > 1:
        key = [n for n in names if sys.argv[1] in n]
        print("\nMATCHED:", key)
        for k in key[:1]:
            print(json.dumps(units[k], indent=2)[:4000])
