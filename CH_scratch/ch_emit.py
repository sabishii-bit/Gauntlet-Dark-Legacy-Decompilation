import json
import sys
import ch_derive

u, f = sys.argv[1], sys.argv[2]
lo, hi = int(sys.argv[3], 0), int(sys.argv[4], 0)
r = ch_derive.run(u, f, lo, hi)
if r:
    print(json.dumps(r, indent=2))
