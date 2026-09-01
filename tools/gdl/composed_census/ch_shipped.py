import json, os
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
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
