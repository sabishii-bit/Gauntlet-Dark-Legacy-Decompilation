import re, os, json, collections

rep = json.load(open('build/GUNE5D/report.json', encoding='utf-8'))
fns = {}
for unit in rep.get('units', []):
    uname = unit.get('name', '')
    if uname.startswith('main/'):
        uname = uname[len('main/'):]
    for f in (unit.get('functions') or []):
        fns[(uname, f.get('name'))] = f.get('fuzzy_match_percent')
exact = {k for k, v in fns.items() if v == 100.0}
print("exact functions in report:", len(exact))

BR = re.compile(r'^/\*\s*([0-9A-F]{8})\s+[0-9A-F]{8}\s+(?:[0-9A-F]{2} ){4}\*/\s*(\S+)\s*(.*)$')
FN = re.compile(r'^\.fn\s+([\w$@.]+)')
TGT = re.compile(r'\.L_([0-9A-Fa-f]{8})')
COND = {'beq', 'bne', 'blt', 'bge', 'bgt', 'ble', 'bnl', 'bng', 'bso', 'bns'}

hits = collections.Counter()
examples = collections.defaultdict(list)
total_cond = 0

for root, d, files in os.walk('build/GUNE5D/asm'):
    for fn in files:
        if not fn.endswith('.s'):
            continue
        p = os.path.join(root, fn)
        txt = open(p, encoding='utf-8', errors='replace').read()
        cur = None
        insns = []
        for line in txt.splitlines():
            ls = line.strip()
            m2 = FN.match(ls)
            if m2:
                cur = m2.group(1)
            m = BR.match(ls)
            if m:
                insns.append((int(m.group(1), 16), m.group(2), m.group(3), cur))
        by = {a: (mn, op) for a, mn, op, fu in insns}
        unit = os.path.relpath(p, 'build/GUNE5D/asm').replace(os.sep, '/')[:-2]
        for a, mn, op, fu in insns:
            if mn not in COND:
                continue
            total_cond += 1
            m3 = TGT.search(op)
            if not m3:
                continue
            if int(m3.group(1), 16) != a + 8:
                continue
            nxt = by.get(a + 4)
            if not nxt or nxt[0] != 'b':
                continue
            key = (unit, fu)
            hits[key] += 1
            if len(examples[key]) < 2:
                examples[key].append(hex(a))

print("conditional branches scanned:", total_cond)
print("total unfused sites:", sum(hits.values()), "in", len(hits), "functions")
ex = [(k, v) for k, v in hits.items() if k in exact]
print()
print("=== sites inside ALREADY-EXACT functions ===", sum(v for k, v in ex), "in", len(ex), "functions")
for k, v in sorted(ex, key=lambda x: -x[1])[:30]:
    print("  %3d  %s :: %s   e.g. %s" % (v, k[0], k[1], examples[k]))
print()
print("=== gauntworld sites ===")
for k, v in sorted(hits.items()):
    if 'gauntworld' in k[0]:
        print("  %3d  %s :: %s   e.g. %s" % (v, k[0], k[1], examples[k]))
