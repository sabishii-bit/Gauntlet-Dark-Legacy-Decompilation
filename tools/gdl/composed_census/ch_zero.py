import json, os
HERE = os.path.dirname(os.path.abspath(__file__))
d = json.load(open(os.path.join(HERE, "ch_census26.json")))
print("totals:", d["totals"])
t = d["totals"]
print("positional combined =", t.get("fwd_rc",0)+t.get("inv_rc",0))
print("r0-corrected WF figure would be 44/47 = 91; mine 41/42 = 83; delta 3+5=8\n")
zero = [r for r in d["rows"] if r["counts"].get("other", 0) == 0]
print(f"FUNCTIONS WITH other == 0 POSITIONALLY: {len(zero)}")
for r in sorted(zero, key=lambda r: r["insns"]):
    print(f"  {r['unit']:30} {r['function']:34} ins={r['insns']:5} {r['counts']}")
