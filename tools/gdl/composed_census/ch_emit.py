"""Emit ch_derive's composed rule for one window as JSON, or nothing.

    ch_emit.py <unit> <function> <lo> <hi>
"""
import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                ".."))                       # tools/gdl
import cliscreen  # noqa: E402
# Run-59 item 9: `--help` used to be an IndexError traceback on stderr.
cliscreen.help_only(__doc__)
import ch_derive  # noqa: E402

if len(sys.argv) < 5:
    raise SystemExit(__doc__.strip())
u, f = sys.argv[1], sys.argv[2]
lo, hi = int(sys.argv[3], 0), int(sys.argv[4], 0)
r = ch_derive.run(u, f, lo, hi)
if r:
    print(json.dumps(r, indent=2))
