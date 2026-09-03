"""CE lane (run 43): datum-screen the EQUIVALENT tier (every webfrank-pinned
function) against the retail pool, on BOTH objects.

WHY THE TIER NEEDS ITS OWN SCREEN.  A rule-served function reads `real 0` in
every .text arbiter by construction, so a WRONG CONSTANT in it is invisible to
fndiff, to probe, to defake_gate and to the progress split alike.  That is the
camera_mode_target class: a function sat pinned at real 0 carrying two wrong
constants.  The only thing that can see it is a comparison of the DATA behind
the relocations, which is what cr_datum_screen does.

WHY BOTH OBJECTS.  The two shipped screens disagree about which object to read
and each is right about a different failure:

  * cr_datum_screen reads build/GUNE5D/src/<unit>.o -- the POSTPROCESSED
    object.  For a pinned function its register fields come from the RULE, so
    a difference there can be a rule artifact rather than a source defect.
    That is exactly the defect cq_raw_pool_screen was written to remove.
  * cq_raw_pool_screen reads .postprocess/body/<unit>.o -- the RAW compiler
    output -- but carries none of cr's calibrated false-positive suppressions
    (pointer tables, address keying, the T11 prefix law) and has no image mode.

So this audit runs cr's calibrated comparison over cq's raw object as well as
over the postprocessed one, and reports the disagreement as a measurement.
Rows that are VALUE-DELTA on the RAW object are source defects; rows that are
VALUE-DELTA only on the postprocessed object are rule artifacts.

    python tools/gdl/composed_census/ce_eq_datum_audit.py [--out PATH]
    python tools/gdl/composed_census/ce_eq_datum_audit.py --image \
        --out build/GUNE5D/ce_image_datum.json

Read-only.  Requires a completed `ninja`.

READING A ROW: a VALUE-DELTA is a datum finding only when a `B:` key appears
on at least one side.  Rows whose differing keys are all `A:` (an address, for
a datum with no bytes), `N:` (an unresolvable name) or `P:` (a pointer table
keyed by size) are naming or representation differences and imply no source
edit -- see claim.law.CE_a-bss-datum-has-no-bytes-so-the-datum-screen-keys-it-
by-address-on-one-side-and-by-name-on-the-other.20260903.v1, which measured
that class at 17 of 49 rows image-wide.
"""
import argparse
import json
import os
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools" / "gdl"))
sys.path.insert(0, str(ROOT / "tools" / "gdl" / "composed_census"))

import cr_datum_screen as cr                                   # noqa: E402
from fndiff import parse                                       # noqa: E402

_parsed = {}


def parsed(path):
    key = str(path)
    if key not in _parsed:
        try:
            _parsed[key] = parse(path)
        except Exception:
            _parsed[key] = {}
    return _parsed[key]


def screen_against(unit, fn, bobj):
    """cr_datum_screen.screen(), but against a caller-chosen OUR object."""
    tobj = ROOT / "build" / "GUNE5D" / "obj" / f"{unit}.o"
    tfns, bfns = parsed(tobj), parsed(bobj)
    if fn not in tfns or fn not in bfns:
        return None
    tl, bl = cr.objdata(tobj), cr.objdata(bobj)
    tc, bc = cr.relocs(tfns[fn]), cr.relocs(bfns[fn])
    cr.poolbytes(list(tc) + list(bc))
    tk, bk = Counter(), Counter()
    label = {}
    for symbol, count in tc.items():
        key, size = cr.datum_key(symbol, tl)
        tk[key] += count
        label.setdefault(key, []).append(f"T:{symbol}({size})")
    for symbol, count in bc.items():
        key, size = cr.datum_key(symbol, bl)
        bk[key] += count
        label.setdefault(key, []).append(f"O:{symbol}({size})")
    only_t, only_b = tk - bk, bk - tk
    for key in list(only_b):
        if not key.startswith("B:"):
            continue
        mine = bytes.fromhex(key[2:])
        for tkey in list(only_t):
            if tkey.startswith("B:") and (
                    bytes.fromhex(tkey[2:]).startswith(mine)
                    or mine.startswith(bytes.fromhex(tkey[2:]))):
                n = min(only_t[tkey], only_b[key])
                only_t[tkey] -= n
                only_b[key] -= n
                if only_t[tkey] <= 0:
                    del only_t[tkey]
                if only_b[key] <= 0:
                    del only_b[key]
                break
    return only_t, only_b, label


def raw_object(unit):
    final = ROOT / "build" / "GUNE5D" / "src" / f"{unit}.o"
    body = final.parent / ".postprocess" / "body" / f"{Path(unit).name}.o"
    return (body if body.exists() else final), body.exists()


def describe(blob_key):
    if not blob_key.startswith("B:"):
        return blob_key
    import struct
    blob = bytes.fromhex(blob_key[2:])
    out = blob[:16].hex()
    if len(blob) >= 8:
        out += f"  f64={struct.unpack('>d', blob[:8])[0]!r}"
    if len(blob) >= 4:
        out += f"  f32={struct.unpack('>f', blob[:4])[0]!r}"
    head = blob.split(b"\x00")[0]
    if head and all(32 <= b < 127 for b in head):
        out += f"  str={head[:60].decode('ascii')!r}"
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--out", default=str(
        ROOT / "build" / "GUNE5D" / "ce_eq_datum_audit.json"))
    parser.add_argument("--image", action="store_true",
                        help="every function of every NonMatching unit, not "
                             "just the webfrank-pinned ones")
    arguments = parser.parse_args()

    config = json.load(open(ROOT / "config" / "GUNE5D" / "webfrank.json",
                            encoding="utf-8"))
    pins = [(unit, rule["function"])
            for unit, rules in config.get("units", config).items()
            for rule in rules]
    if arguments.image:
        pinned = set(pins)
        pins = []
        report = json.loads(
            (ROOT / "build" / "GUNE5D" / "report.json").read_text())
        for entry in report.get("units", []):
            unit = entry.get("name", "").removeprefix("main/")
            if entry.get("metadata", {}).get("complete"):
                continue
            tobj = ROOT / "build" / "GUNE5D" / "obj" / f"{unit}.o"
            bobj, _has_raw = raw_object(unit)
            if not (tobj.exists() and Path(bobj).exists()):
                continue
            for fn in parsed(tobj):
                pins.append((unit, fn))
        print(f"image mode: {len(pins)} functions across NonMatching units "
              f"({len(pinned)} of them webfrank-pinned)")

    rows, tally = [], Counter()
    for unit, fn in pins:
        bobj, has_raw = raw_object(unit)
        post = screen_against(unit, fn,
                              ROOT / "build" / "GUNE5D" / "src" / f"{unit}.o")
        raw = screen_against(unit, fn, bobj)
        if raw is None:
            tally["unreadable"] += 1
            continue
        raw_t, raw_b, label = raw
        raw_delta = bool(raw_t or raw_b)
        post_delta = bool(post and (post[0] or post[1]))
        tally["screened"] += 1
        tally["raw VALUE-DELTA" if raw_delta else "raw VALUE-EQUAL"] += 1
        if post_delta != raw_delta:
            tally["screens DISAGREE"] += 1
        if not raw_delta:
            continue
        rows.append({
            "unit": unit, "function": fn, "has_raw_body": has_raw,
            "post_delta": post_delta,
            "target_only": [{"datum": describe(k), "n": n,
                             "symbols": label.get(k)}
                            for k, n in sorted(raw_t.items())],
            "ours_only": [{"datum": describe(k), "n": n,
                           "symbols": label.get(k)}
                          for k, n in sorted(raw_b.items())],
        })

    print("EQUIVALENT-TIER DATUM AUDIT (webfrank-pinned functions)")
    print(f"  pins in config: {len(pins)}")
    for key in sorted(tally):
        print(f"  {key:22} {tally[key]}")
    print()
    print(f"VALUE-DELTA ON THE RAW COMPILER OUTPUT "
          f"(= genuine source-value defect, invisible to every .text "
          f"arbiter): {len(rows)}")
    for row in rows:
        print(f"\n  {row['unit']}::{row['function']}  "
              f"raw_body={row['has_raw_body']}  "
              f"postprocessed_screen_agrees={row['post_delta']}")
        for entry in row["target_only"]:
            print(f"      TARGET-ONLY x{entry['n']}  {entry['datum']}")
            print(f"          {entry['symbols']}")
        for entry in row["ours_only"]:
            print(f"      OURS-ONLY   x{entry['n']}  {entry['datum']}")
            print(f"          {entry['symbols']}")

    os.makedirs(os.path.dirname(arguments.out), exist_ok=True)
    with open(arguments.out, "w", encoding="utf-8") as handle:
        json.dump({"tally": dict(tally), "rows": rows}, handle, indent=2)
    print(f"\nwrote {arguments.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
