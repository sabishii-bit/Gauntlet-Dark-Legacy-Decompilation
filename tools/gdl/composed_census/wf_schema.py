"""Print the webfrank.json entry for one unit/function as a schema template."""
import json
import sys

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
