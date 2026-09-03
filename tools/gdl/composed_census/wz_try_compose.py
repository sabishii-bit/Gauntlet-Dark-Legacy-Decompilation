"""WZ lane (run 41): turn the demand table's CANDIDATES into VERDICTS.

For each named function this runs the real ``webfrank.apply_patch`` with the
SHIPPED composition (equivalent_copy_form edits synthesised by shape, then
copy_register_fields) and prints either BYTE-EQUAL or the guard's own
refusal string -- the check that fired, the word, and what was tried.
AGENTS.md discipline 14: a guard's refusal is a measurement of the guard, so
it is recorded verbatim rather than paraphrased.

No object is modified and no rule is written; this is a read-only screen.

    python WZ_scratch/wz_try_compose.py <unit> <function> [...]
    python WZ_scratch/wz_try_compose.py --table
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                                     # noqa: E402
from cn_analyze import our_object, target_object           # noqa: E402

CENSUS = os.path.join(ROOT, "build/GUNE5D/wz_zeroform_demand.json")


def census_table():
    """The demand table's candidate rows, read from the census.

    Hard-coding one run's roster is how a promoted tool rots: this reads
    wz_zeroform_demand.py's output instead, so the screen follows the tree.
    """
    if not os.path.exists(CENSUS):
        raise SystemExit(
            f"missing {CENSUS} — run "
            f"tools/gdl/composed_census/wz_zeroform_demand.py first")
    rows = json.load(open(CENSUS))["rows"]
    picked = [r for r in rows
              if not r["pinned"] and r["mnemonic_divergence"] == 0
              and r["tally"]["COPY_SERVED"] > 0]
    picked.sort(key=lambda r: r["differing_words"])
    return [(r["unit"], r["function"]) for r in picked]

OFFSET = re.compile(r"\+0x([0-9a-f]+)")


def synthesise(ours_word, target_word):
    """Candidate equivalent_copy_form edits for one word pair, by shape."""
    ours = wf.decode_copy_form(ours_word)
    theirs = wf.decode_copy_form(target_word)
    if ours is None or theirs is None:
        return []
    if ours[0] == "copy" and theirs[0] == "copy":
        if ours[1] == theirs[1] and ours[2] == theirs[2]:
            return [{"proof": "unconditional"}]
        return [{"proof": "unconditional_recolor"}]
    if ours[0] == "li" and theirs[0] == "copy":
        edits = []
        if ours[1] == theirs[1]:
            edits.append({"proof": "dominating_def"})
            edits.append({"proof": "dominating_def_across_calls"})
        edits += [{"proof": "constant_dataflow_recolor", "our_source": r}
                  for r in range(3, 32)]
        return edits
    if ours[0] == "copy" and theirs[0] == "li":
        return [{"proof": "dominating_def_inverse"},
                {"proof": "dominating_def_inverse_across_calls"}]
    return []


def attempt(unit, name):
    ours_path, kind = our_object(unit)
    target_path = target_object(unit)
    odata = bytearray(open(ours_path, "rb").read())
    tdata = bytearray(open(target_path, "rb").read())
    osec, tsec = wf._sections(odata), wf._sections(tdata)
    symbol = wf._find_symbol(odata, osec, name)
    text = osec[symbol.section_index]
    start = text.offset + symbol.value
    body = bytes(odata[start:start + symbol.size])
    tsym = wf._find_symbol(tdata, tsec, name)
    ttext = tsec[tsym.section_index]
    target = bytes(tdata[ttext.offset + tsym.value:
                          ttext.offset + tsym.value + tsym.size])
    if len(body) != len(target):
        return {"unit": unit, "function": name, "verdict": "COUNT-ASYMMETRIC",
                "detail": f"{len(body)//4} vs {len(target)//4} insns"}

    rule = {
        "function": name,
        "before_sha256": wf._sha256(body),
        "after_sha256": wf._sha256(target),
        "audit": {"classification": "MIXED", "instructions": len(body) // 4},
        "copy_register_fields": True,
    }
    copy_forms: list = []
    tried: list = []
    last = ""
    for _round in range(24):
        rule["equivalent_copy_form"] = list(copy_forms) if copy_forms else None
        candidate = {k: v for k, v in rule.items() if v is not None}
        probe = bytearray(odata)
        try:
            wf.apply_patch(probe, json.loads(json.dumps(candidate)),
                           bytes(tdata))
            final = bytes(probe[start:start + symbol.size])
            return {"unit": unit, "function": name,
                    "verdict": "BYTE-EQUAL" if final == target else "APPLIED-NOT-EQUAL",
                    "copy_forms": copy_forms, "tried": tried, "kind": kind}
        except ValueError as failure:
            last = str(failure)
        if "non-register instruction bits differ" not in last:
            break
        match = OFFSET.search(last)
        if not match:
            break
        offset = int(match.group(1), 16)
        if any(_parse(e["at"]) == offset for e in copy_forms):
            break
        options = synthesise(wf._u32(body, offset), wf._u32(target, offset))
        if not options:
            tried.append(f"+0x{offset:x}: no copy-form shape "
                         f"(O {wf._u32(body, offset):08x} "
                         f"T {wf._u32(target, offset):08x})")
            break
        placed = False
        for option in options:
            trial = copy_forms + [dict(option, at=hex(offset))]
            attempt_rule = dict(rule)
            attempt_rule["equivalent_copy_form"] = trial
            probe = bytearray(odata)
            try:
                wf.apply_patch(probe,
                               json.loads(json.dumps(
                                   {k: v for k, v in attempt_rule.items()
                                    if v is not None})),
                               bytes(tdata))
                copy_forms = trial
                placed = True
                break
            except ValueError as inner:
                text_inner = str(inner)
                if f"+0x{offset:x}" not in text_inner:
                    # the edit was accepted; a LATER stage refused
                    copy_forms = trial
                    placed = True
                    last = text_inner
                    break
        if not placed:
            tried.append(f"+0x{offset:x}: every copy-form mode refused")
            break
    return {"unit": unit, "function": name, "verdict": "REFUSED",
            "detail": last, "copy_forms": copy_forms, "tried": tried,
            "kind": kind}


def _parse(value):
    return int(value, 16) if isinstance(value, str) else value


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("pairs", nargs="*")
    parser.add_argument("--table", action="store_true")
    arguments = parser.parse_args()
    work = (census_table() if (arguments.table or not arguments.pairs)
            else [tuple(p.split("::")) for p in arguments.pairs])
    for unit, name in work:
        try:
            result = attempt(unit, name)
        except Exception as error:
            print(f"{unit}::{name}: TOOL ERROR {type(error).__name__}: {error}")
            continue
        print(f"\n{unit}::{name}  [{result['verdict']}]")
        if result.get("copy_forms"):
            print(f"  form edits accepted: {result['copy_forms']}")
        for note in result.get("tried", []):
            print(f"  tried: {note}")
        if result.get("detail"):
            print(f"  REFUSAL: {result['detail']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
