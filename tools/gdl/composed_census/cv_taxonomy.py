#!/usr/bin/env python3
"""CV lane stage 1: catalogue every webfrank.json rule by mechanism class."""
import json, collections, os, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))  # repo root (fixed after promotion)
WF = os.path.join(ROOT, "config", "GUNE5D", "webfrank.json")

data = json.load(open(WF, encoding="utf-8"))
units = data["units"]

# Every non-bookkeeping key found on a rule = a mechanism stage.
BOOK = {"function", "before_sha256", "after_sha256", "mechanism", "audit",
        "notes", "comment"}

stagekeys = collections.Counter()
rows = []
for unit, rules in sorted(units.items()):
    for r in rules:
        stages = sorted(k for k in r.keys() if k not in BOOK)
        for s in stages:
            stagekeys[s] += 1
        audit = r.get("audit", {}) or {}
        rows.append({
            "unit": unit,
            "fn": r.get("function"),
            "stages": stages,
            "classification": audit.get("classification"),
            "instructions": audit.get("instructions"),
            "audit_keys": sorted(audit.keys()),
            "mech_len": len(r.get("mechanism", "") or ""),
            "mechanism": r.get("mechanism", "") or "",
        })

print("TOTAL RULES:", len(rows), " UNITS:", len(units))
print()
print("== stage-key population ==")
for k, v in stagekeys.most_common():
    print(f"  {k:38s} {v}")
print()
print("== audit classification ==")
for k, v in collections.Counter(r["classification"] for r in rows).most_common():
    print(f"  {str(k):38s} {v}")
print()
print("== stage COMBINATION (the taxonomy) ==")
combo = collections.Counter(tuple(r["stages"]) for r in rows)
for k, v in combo.most_common():
    print(f"  {v:4d}  {' + '.join(k) if k else '(none)'}")
print()
print("== audit sub-keys ==")
ak = collections.Counter()
for r in rows:
    for k in r["audit_keys"]:
        ak[k] += 1
for k, v in ak.most_common():
    print(f"  {k:38s} {v}")

json.dump(rows, open(os.path.join(ROOT, "build", "CV_rules.json"), "w",
                     encoding="utf-8"), indent=1)
print("\nwrote build/CV_rules.json")
