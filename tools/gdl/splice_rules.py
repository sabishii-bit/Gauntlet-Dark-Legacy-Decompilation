import json, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
new = json.loads((ROOT / "WF_scratch/pb_window_rules.json").read_text())
wf_path = ROOT / "config/GUNE5D/webfrank.json"
text = wf_path.read_text()
doc = json.loads(text)
cur = doc["units"]["game/pb/pb_window"]

assert [r["function"] for r in cur] == [r["function"] for r in new]

NOTE = (" RE-DERIVED 2026-09-01 (PW run 26) against the objects produced after the "
        "pbAspectRatio .sdata2 ordering helper landed: the helper shifts MWCC's pool "
        "label numbering, so every input hash below moved even though the windows, "
        "their orders and the mechanism are byte-for-byte the same as before. "
        "Re-proven end-to-end through apply_patch against the extracted retail object "
        "(after == target body).")


def merge(old_rule, new_rule):
    for key in ("before_sha256", "after_sha256"):
        old_rule[key] = new_rule[key]
    o = old_rule["instruction_permutation"]
    n = new_rule["instruction_permutation"]
    o_list = o if isinstance(o, list) else [o]
    n_list = n if isinstance(n, list) else [n]
    assert len(o_list) == len(n_list)
    for ow, nw in zip(o_list, n_list):
        assert ow["start"] == nw["start"] and ow["end"] == nw["end"] and ow["order"] == nw["order"], \
            f"window geometry changed: {ow['start']}..{ow['end']} {ow['order']} vs {nw['start']}..{nw['end']} {nw['order']}"
        for key in ("before_sha256", "after_sha256",
                    "before_relocations_sha256", "after_relocations_sha256"):
            ow[key] = nw[key]
    if NOTE not in old_rule["mechanism"]:
        old_rule["mechanism"] += NOTE


for old_rule, new_rule in zip(cur, new):
    merge(old_rule, new_rule)

marker = '\n    "game/pb/pb_window": ['
i = text.index(marker)
open_bracket = i + len(marker) - 1
depth = 0
for j in range(open_bracket, len(text)):
    if text[j] == '[':
        depth += 1
    elif text[j] == ']':
        depth -= 1
        if depth == 0:
            close_bracket = j
            break
else:
    raise SystemExit("unbalanced")

old_span = text[open_bracket:close_bracket + 1]
assert json.loads(old_span)  # sanity: the span really is the array

body = json.dumps(cur, indent=2, ensure_ascii=False)
# keep the file's existing compact style for the small integer arrays
import re
body = re.sub(r'"order": \[\s+([0-9,\s]+?)\s+\]',
              lambda m: '"order": [%s]' % ", ".join(m.group(1).split()).replace(",,", ","),
              body)
body = "\n".join(("    " + line) if k else line for k, line in enumerate(body.split("\n")))
out = text[:open_bracket] + body + text[close_bracket + 1:]

# the ONLY thing that may differ outside the spliced span is nothing at all
assert out[:open_bracket] == text[:open_bracket]
assert out[len(out) - (len(text) - close_bracket - 1):] == text[close_bracket + 1:]
json.loads(out)

if "--apply" in sys.argv:
    with open(wf_path, "w", encoding="utf-8", newline="") as fh:
        fh.write(out)
    print("spliced pb_window rules into config/GUNE5D/webfrank.json")
else:
    print("dry run OK; span %d..%d, new body %d bytes" % (open_bracket, close_bracket, len(body)))
