"""WR lane (run 42): run one hand-written rule body through real apply_patch.

    python tools/gdl/composed_census/wr_try_rule.py <unit> <function> <rule-fragment.json>
    python tools/gdl/composed_census/wr_try_rule.py ... --image <retail.dol>

The fragment supplies everything except function/before_sha256/after_sha256,
which are filled from the live objects.  Prints BYTE-EQUAL / APPLIED-NOT-EQUAL
/ the guard's verbatim refusal, and on success writes the completed rule.

THE RETAIL IMAGE IS PASSED, AND ITS ABSENCE IS A REFUSAL (run-44 item 3, from
WS).  `apply_patch` takes the image as its fifth argument and this tool called
it with four, so every rule authored here was screened with `image=None` — the
L3 DATUM level of `verify_datum_binding` cannot run without it, and every word
that L3 would have decided fell through to L4, the pool CORRESPONDENCE, which
is a strictly weaker proof: a one-to-one map says nothing about which datum
each end holds, and that is exactly the hole
claim.law.CQ_copy-register-fields-can-rotate-constant-load-homes-without-their-
relocations.20260903.v1 records.

MEASURED at ca4074cb1 on the shipped game/mb/mb_font::MBRenderText rule, run
through this tool from its own fragment, with the level counts read straight
out of `verify_datum_binding`:

    four-argument call (image=None)   L1 27  L2 0  L3  0  L4 10
    five-argument call (image passed) L1 27  L2 0  L3 10  L4  0

Same rule, same bytes, same BYTE-EQUAL verdict — ten words moved from the
weakest proof to a byte comparison against the retail image.  The visible
symptom was the line `datum binding proved (name 27, address 0, datum 0);
10 word(s) rest on the pool correspondence alone (uninitialised data)`, and
those ten words are neither uninitialised nor unprovable; with the image they
are decided and the line does not print at all (apply_patch prints it only
when L4 is non-zero, so a full-strength screen is SILENT — which is why the
line below names the image it read).

webfrank's own main() refuses outright when the image is missing ("a screen
that quietly stops screening"); this now refuses the same way rather than
degrading, which is the whole point: the author of a NEW rule is the one
reader who cannot tell the difference from the output.
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                                      # noqa: E402
from cn_analyze import our_object, target_object            # noqa: E402


def default_image_path(root, version="GUNE5D"):
    """Where webfrank's own main() looks for the retail DOL.

    Spelled here so the two cannot drift: `config/<version>/webfrank.json`
    resolves the image as `<repo>/orig/<version>/sys/main.dol`.
    """
    return os.path.join(root, "orig", version, "sys", "main.dol")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("unit")
    parser.add_argument("function")
    parser.add_argument("fragment")
    parser.add_argument("--out", default=None)
    parser.add_argument(
        "--image", default=None,
        help="retail DOL, read by the L3 DATUM level of the datum screen "
             "(default: orig/GUNE5D/sys/main.dol). Missing = REFUSAL, never "
             "a silent degrade to the correspondence proof.")
    arguments = parser.parse_args()

    image_path = arguments.image or default_image_path(ROOT)
    if not os.path.exists(image_path):
        print(f"REFUSED: retail image {image_path} is missing, so relocation"
              " datum bindings cannot be proved and every word L3 would have"
              " decided would fall through to the weaker pool-correspondence"
              " proof. Run `python tools/gdl/provision_worktree.py`, or pass"
              " --image. (webfrank's own main() refuses here for the same"
              " reason; a screen that quietly stops screening is how"
              " claim.law.CQ_copy-register-fields-can-rotate-constant-load-"
              "homes-without-their-relocations.20260903.v1 shipped for ten"
              " days.)")
        return 1
    image = wf.RetailImage(image_path)
    # A full-strength screen prints NOTHING (apply_patch reports the level
    # split only when L4 is non-zero), so say which image L3 read — otherwise
    # "no datum line" and "the datum level never ran" look identical.
    print(f"[datum screen at FULL strength: L3 reads {image_path}]")

    ours_path, kind = our_object(arguments.unit)
    odata = bytearray(open(ours_path, "rb").read())
    tdata = bytearray(open(target_object(arguments.unit), "rb").read())
    osec, tsec = wf._sections(odata), wf._sections(tdata)
    osym = wf._find_symbol(odata, osec, arguments.function)
    tsym = wf._find_symbol(tdata, tsec, arguments.function)
    ostart = osec[osym.section_index].offset + osym.value
    tstart = tsec[tsym.section_index].offset + tsym.value
    body = bytes(odata[ostart:ostart + osym.size])
    target = bytes(tdata[tstart:tstart + tsym.size])

    with open(arguments.fragment, encoding="utf-8") as handle:
        fragment = json.load(handle)
    rule = {"function": arguments.function,
            "before_sha256": wf._sha256(body),
            "after_sha256": wf._sha256(target)}
    rule.update(fragment)

    probe = bytearray(odata)
    try:
        before, after, changed = wf.apply_patch(
            probe, json.loads(json.dumps(rule)), bytes(tdata),
            wf.load_symbol_addresses(
                os.path.join(ROOT, "config", "GUNE5D", "symbols.txt")),
            image)
    except ValueError as failure:
        print(f"REFUSED ({kind}): {failure}")
        return 1
    final = bytes(probe[ostart:ostart + osym.size])
    verdict = "BYTE-EQUAL" if final == target else "APPLIED-NOT-EQUAL"
    print(f"{verdict} ({kind}); {changed} word(s) changed")
    out = arguments.out or os.path.join(
        ROOT, "build", "GUNE5D", f"wr_rule_{arguments.function}.json")
    with open(out, "w", encoding="utf-8") as handle:
        json.dump(rule, handle, indent=2)
    print(f"  wrote {out}")
    return 0 if verdict == "BYTE-EQUAL" else 1


if __name__ == "__main__":
    raise SystemExit(main())
