import json, sys
d = json.load(open(sys.argv[1]))
laws = d.get('laws', [])
print('COUNT', len(laws))
if laws:
    print('KEYS', sorted(laws[0].keys()))
    print()
for l in laws:
    print('===', l.get('id') or l.get('record_id'))
    for k, v in l.items():
        if k in ('id', 'record_id'):
            continue
        s = json.dumps(v) if not isinstance(v, str) else v
        if len(s) > 1400:
            s = s[:1400] + ' ...[trunc]'
        print(f'  {k}: {s}')
    print()
