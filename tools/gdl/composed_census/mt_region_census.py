#!/usr/bin/env python3
"""Region completeness census: is a TU family actually open work?

Answers, in one command, the question every FRESH-TU work order asserts
but rarely verifies:

  * which TUs in a path family are split, written, and configured,
  * what each one scores in the CURRENT report.json,
  * whether any .text gap inside the family's address span is unclaimed
    by any split (a real unwritten hole),
  * whether any of them are postprocessor-assisted rather than STRICT.

Motivating measurement (run 35, lane MT): a work order dispatched a lane
to "write the MSL float trig TU [0x800EA674,0x800EAE2C)" whose banked
roster was said to live in the graph. `gdlmem search trigf` returned only
the work_claim itself, and this census showed all 40 MSL .text TUs at
100.00 fuzzy / 100.00 matched with zero gaps -- the region had been
matched two commits earlier. Cost of the census: about three seconds.

Usage, from the repository root:

    python tools/gdl/composed_census/mt_region_census.py --prefix MSL/
    python tools/gdl/composed_census/mt_region_census.py --prefix dolphin/
    python tools/gdl/composed_census/mt_region_census.py --prefix game/ui/ \\
        --out build/GUNE5D/mt_region_census_ui.json

Exit status is 0 when the family has open work (units that are not
Matching, missing sources, or unclaimed gaps) and 1 when the family is
complete -- so a dispatcher can gate on it:

    ... --prefix MSL/ ; if ($LASTEXITCODE -eq 1) { "already complete" }
"""

import argparse
import json
import os
import re
import sys

# repo root = three levels up from tools/gdl/composed_census/
ROOT = os.path.abspath(
    os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 os.pardir, os.pardir, os.pardir))


def parse_splits(path):
    """Return {unit: (text_lo, text_hi)} plus the set of all .text ranges."""
    units = {}
    every = []
    cur = None
    with open(path, encoding='utf-8') as fh:
        for line in fh:
            m = re.match(r'^(\S+\.\w+):\s*$', line)
            if m:
                cur = m.group(1)
                continue
            m = re.match(
                r'^\s+\.text\s+start:(0x[0-9A-Fa-f]+)\s+'
                r'end:(0x[0-9A-Fa-f]+)', line)
            if m and cur:
                lo, hi = int(m.group(1), 16), int(m.group(2), 16)
                units[cur] = (lo, hi)
                every.append((lo, hi))
    return units, every


def parse_configure(path):
    """Return {unit: Matching|NonMatching} from configure.py Object() rows."""
    text = open(path, encoding='utf-8').read()
    return {m.group(2): m.group(1) for m in
            re.finditer(r'Object\((\w+),\s*"([^"]+)"', text)}


def parse_report(path):
    """Return {unit_key: measures} keyed by the unit path without 'main/'."""
    if not os.path.exists(path):
        return {}
    rep = json.load(open(path, encoding='utf-8'))
    out = {}
    for u in rep.get('units', []):
        name = u.get('name', '')
        key = name.split('main/', 1)[-1]
        out[key] = u.get('measures', {}) or {}
    return out


def postproc_units(root, prefix):
    """Units named anywhere in the postprocessor configs (EQUIVALENT tier)."""
    hits = set()
    for cfg in ('config/GUNE5D/webfrank.json', 'config/GUNE5D/p6frank.json'):
        p = os.path.join(root, cfg)
        if not os.path.exists(p):
            continue
        blob = open(p, encoding='utf-8').read()
        for unit in re.findall(r'"([^"]*?\.\w+)"', blob):
            if unit.startswith(prefix):
                hits.add(unit)
    return hits


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.
                                 RawDescriptionHelpFormatter)
    ap.add_argument('--prefix', required=True,
                    help="unit path prefix, e.g. 'MSL/' or 'game/ui/'")
    ap.add_argument('--root', default=ROOT,
                    help='repository root (default: inferred)')
    ap.add_argument('--out', default=None,
                    help='optional JSON output path (default: stdout only)')
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    prefix = args.prefix

    units, every = parse_splits(os.path.join(root, 'config/GUNE5D/splits.txt'))
    status = parse_configure(os.path.join(root, 'configure.py'))
    scores = parse_report(os.path.join(root, 'build/GUNE5D/report.json'))
    pinned = postproc_units(root, prefix)

    fam = {u: r for u, r in units.items() if u.startswith(prefix)}
    if not fam and not any(u.startswith(prefix) for u in status):
        print(f'no units match prefix {prefix!r}', file=sys.stderr)
        return 2

    rows = []
    open_units = []
    print(f'=== {prefix} units: split / status / source / score ===')
    for unit in sorted(fam, key=lambda k: fam[k][0]):
        lo, hi = fam[unit]
        st = status.get(unit, 'NOT-IN-CONFIGURE')
        src = os.path.exists(os.path.join(root, 'src', unit))
        key = unit.rsplit('.', 1)[0]
        m = scores.get(key, {})
        fuzzy = m.get('fuzzy_match_percent')
        matched = m.get('matched_code_percent')
        tot = m.get('total_functions')
        mat = m.get('matched_functions')
        is_open = (st != 'Matching') or (not src)
        if is_open:
            open_units.append(unit)
        print(f'{unit:30s} [{lo:#010x},{hi:#010x}) {hi - lo:6d}B  '
              f'{st:14s} src={"Y" if src else "N"}  '
              f'fuzzy={fuzzy} matched={matched} fns={mat}/{tot}'
              f'{"   <<< OPEN" if is_open else ""}'
              f'{"  [POSTPROC]" if unit in pinned else ""}')
        rows.append({'unit': unit, 'text_start': lo, 'text_end': hi,
                     'status': st, 'source_present': src, 'fuzzy': fuzzy,
                     'matched_code_percent': matched,
                     'matched_functions': mat, 'total_functions': tot,
                     'postprocessor_assisted': unit in pinned,
                     'open': is_open})

    # units configured under the prefix but carrying no .text split
    dataonly = [u for u in status if u.startswith(prefix) and u not in fam]
    if dataonly:
        print(f'\n=== {prefix} units without a .text split (data-only) ===')
        for unit in sorted(dataonly):
            st = status[unit]
            src = os.path.exists(os.path.join(root, 'src', unit))
            key = unit.rsplit('.', 1)[0]
            m = scores.get(key, {})
            is_open = (st != 'Matching') or (not src)
            if is_open:
                open_units.append(unit)
            print(f'{unit:30s} (data-only)  {st:14s} '
                  f'src={"Y" if src else "N"} '
                  f'fuzzy={m.get("fuzzy_match_percent")} '
                  f'matched={m.get("matched_code_percent")}'
                  f'{"   <<< OPEN" if is_open else ""}')
            rows.append({'unit': unit, 'text_start': None, 'text_end': None,
                         'status': st, 'source_present': src,
                         'fuzzy': m.get('fuzzy_match_percent'),
                         'matched_code_percent':
                             m.get('matched_code_percent'),
                         'postprocessor_assisted': unit in pinned,
                         'open': is_open})

    # gap scan across the family's own address span, against ALL splits
    gaps = []
    if fam:
        lo_all = min(v[0] for v in fam.values())
        hi_all = max(v[1] for v in fam.values())
        print(f'\n=== .text span {lo_all:#010x}-{hi_all:#010x} gap scan ===')
        covered = sorted((lo, hi) for lo, hi in every
                         if hi > lo_all and lo < hi_all)
        cursor = lo_all
        for lo, hi in covered:
            if lo > cursor:
                print(f'  GAP {cursor:#010x}-{lo:#010x} '
                      f'({lo - cursor} bytes) UNCLAIMED BY ANY SPLIT')
                gaps.append({'start': cursor, 'end': lo})
            cursor = max(cursor, hi)
        if cursor < hi_all:
            print(f'  GAP {cursor:#010x}-{hi_all:#010x} UNCLAIMED')
            gaps.append({'start': cursor, 'end': hi_all})
        print(f'  gaps found: {len(gaps)}')

    complete = not open_units and not gaps
    print(f'\nOPEN units: {sorted(set(open_units)) if open_units else "NONE"}')
    print(f'UNCLAIMED gaps: {len(gaps)}')
    print(f'VERDICT: {prefix} is '
          f'{"COMPLETE - do not dispatch a write lane here" if complete else "OPEN"}')

    if args.out:
        outp = args.out if os.path.isabs(args.out) else os.path.join(root,
                                                                     args.out)
        os.makedirs(os.path.dirname(outp), exist_ok=True)
        with open(outp, 'w', encoding='utf-8') as fh:
            json.dump({'prefix': prefix, 'units': rows, 'gaps': gaps,
                       'open_units': sorted(set(open_units)),
                       'complete': complete}, fh, indent=2)
        print(f'wrote {outp}')

    return 1 if complete else 0


if __name__ == '__main__':
    sys.exit(main())
