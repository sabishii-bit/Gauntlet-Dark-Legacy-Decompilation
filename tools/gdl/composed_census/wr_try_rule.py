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

CHAINED STAGES (run-53 item 3, from
claim.law.CX_a-webfrank-function-can-carry-chained-rule-stages-and-replaying-
one-alone-refuses-on-a-hash-that-is-not-drift.20260904.v1).  A function may
carry MORE THAN ONE entry in config/GUNE5D/webfrank.json, and those entries
are chained STAGES, not duplicates: `entry0.after_sha256 == entry1.before_
sha256`.  This tool replayed exactly one fragment, so replaying a later stage
alone compared the RAW body against the INTERMEDIATE hash and refused with
the same `hash != expected` message a drifted pin produces.  Reproduced at
c7b741799 on the law's own worked case, `game/world/btricol::LineLineDist`:

    <stage0>  -> REFUSED (raw postprocess body): LineLineDist: output hash
                 cb0b03c8... != expected 06e7f0aa...
    <stage1>  -> REFUSED (raw postprocess body): non-register instruction
                 bits differ at +0x14: 0x02030008

Neither is a verdict on the pin (`fndiff --clean` reads MATCH, real 0).  Two
things close it.  A fragment file may now be a JSON LIST of stages, folded in
file order onto one buffer; and `--from-config` loads the function's shipped
entries straight out of webfrank.json in file order, which is the form a
per-pin audit needs — the law's consequence (1) is that a tool taking "the"
entry for a function silently takes the LAST one.  Any single-stage refusal on
a multi-entry function now names the chain in the message, so the benign cause
is not mistaken for drift.

PERMUTATION WINDOWS NEED THEIR OWN FOUR HASHES, and their absence used to be
a bare `KeyError: 'before_sha256'` out of webfrank.py:4058 (reproduced at
c7b741799 with a hand-written `instruction_permutation` window).  The window
hashes are NOT the function hashes and are not filled from the objects here:
they come from `wr_perm_hash.py`, which also prints the permuted window
against the target word by word — the evidence an author actually needs before
shipping the window.  The refusal now names that tool and prints the exact
command for the window it read.
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


PERMUTATION_HASH_KEYS = ("before_sha256", "after_sha256",
                         "before_relocations_sha256",
                         "after_relocations_sha256")


def permutation_windows(stage):
    """Every `instruction_permutation` window in a stage, as a list.

    webfrank accepts the key as one object OR a list of them, so a screen
    that reads only the object spelling misses every multi-window rule.
    """
    windows = stage.get("instruction_permutation")
    if windows is None:
        return []
    return list(windows) if isinstance(windows, list) else [windows]


def missing_window_hashes(stage):
    """(window, [missing key, ...]) for each window short of its four hashes.

    These are NOT the function's before/after hashes and are not derivable
    from the objects the way those are: they pin the exact bytes and the
    exact relocation set the permutation moves. `wr_perm_hash.py` computes
    them and prints the permuted window against the target.
    """
    out = []
    for window in permutation_windows(stage):
        absent = [key for key in PERMUTATION_HASH_KEYS if key not in window]
        if absent:
            out.append((window, absent))
    return out


def shipped_entries(root, unit, function, version="GUNE5D"):
    """A function's webfrank.json entries, IN FILE ORDER.

    File order is application order (the chained-stages law), so this never
    sorts and never de-duplicates: a function with two entries has two
    stages, and taking "the" entry silently takes the last one.
    """
    path = os.path.join(root, "config", version, "webfrank.json")
    if not os.path.exists(path):
        return []
    with open(path, encoding="utf-8") as handle:
        config = json.load(handle)
    units = config.get("units", config) if isinstance(config, dict) else {}
    entries = units.get(unit) or units.get(unit.strip("/")) or []
    return [entry for entry in entries if entry.get("function") == function]


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
    parser.add_argument("fragment", nargs="?", default=None,
                        help="rule fragment JSON: one stage object, or a LIST"
                             " of stage objects folded in file order."
                             " Omit it with --from-config.")
    parser.add_argument(
        "--from-config", action="store_true",
        help="replay the function's SHIPPED webfrank.json entries as the"
             " chain, in file order — the form a per-pin audit needs, since"
             " a tool taking 'the' entry for a function takes the last one.")
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

    shipped = shipped_entries(ROOT, arguments.unit, arguments.function)
    if arguments.from_config:
        if arguments.fragment:
            print("REFUSED: pass a fragment file OR --from-config, not both.")
            return 2
        if not shipped:
            print(f"REFUSED: config/GUNE5D/webfrank.json has no entry for"
                  f" {arguments.unit}::{arguments.function}, so there is no"
                  f" shipped chain to replay. Pass a fragment file instead.")
            return 1
        stages = [dict(entry) for entry in shipped]
        print(f"[--from-config: {len(stages)} shipped stage(s) in FILE ORDER]")
    else:
        if not arguments.fragment:
            print("REFUSED: give a fragment file, or --from-config.")
            return 2
        with open(arguments.fragment, encoding="utf-8") as handle:
            fragment = json.load(handle)
        # A LIST is a chain. Folding it here is the whole point: replaying a
        # later stage alone compares the RAW body against the INTERMEDIATE
        # hash and refuses on a cause that is not drift.
        stages = [dict(stage) for stage in fragment] \
            if isinstance(fragment, list) else [dict(fragment)]
        if len(stages) > 1:
            print(f"[chained fragment: {len(stages)} stages, folded in file"
                  f" order]")

    # A permutation window carries FOUR hashes of its own, and their absence
    # was a bare KeyError out of webfrank.py:4058. Screened here, before any
    # object is touched, with the command that produces them.
    for index, stage in enumerate(stages):
        for window, absent in missing_window_hashes(stage):
            order = ",".join(str(value) for value in window.get("order", []))
            print(
                f"REFUSED: stage {index}'s instruction_permutation window"
                f" {window.get('start')}..{window.get('end')} is missing"
                f" {', '.join(absent)}.\n"
                "  A window's four hashes are NOT the function's two and are"
                " not filled from the objects here: they pin the exact bytes"
                " and the exact relocation set the permutation moves.\n"
                "  Compute them (and see the permuted window against the"
                " target, word by word) with:\n"
                f"    python tools/gdl/composed_census/wr_perm_hash.py"
                f" {arguments.unit} {arguments.function}"
                f" {window.get('start')} {window.get('end')} {order}\n"
                "  then paste its window object into the fragment."
                " (Without this the failure was"
                " `KeyError: 'before_sha256'` from webfrank.py:4058, which"
                " reads as a webfrank bug rather than an incomplete rule.)")
            return 2

    symbols = wf.load_symbol_addresses(
        os.path.join(ROOT, "config", "GUNE5D", "symbols.txt"))
    probe = bytearray(odata)
    rules = []
    for index, stage in enumerate(stages):
        current = bytes(probe[ostart:ostart + osym.size])
        last = index == len(stages) - 1
        # Every stage's INPUT hash is read from the buffer as it stands, the
        # same way the single-stage path always read the function's own
        # before_sha256 from the live object. Only the FINAL output hash is
        # bound to external truth — the target body — because that is the one
        # the verdict rests on. An intermediate stage that declares its own
        # after_sha256 (every shipped chained entry does) keeps it.
        rule = {"function": arguments.function,
                "before_sha256": wf._sha256(current)}
        if last:
            rule["after_sha256"] = wf._sha256(target)
        rule.update(stage)
        if last:
            rule["after_sha256"] = stage.get("after_sha256",
                                             wf._sha256(target))
        if not last and "after_sha256" not in rule:
            print(
                f"REFUSED: stage {index} of {len(stages)} declares no"
                " after_sha256, and an INTERMEDIATE stage's output hash"
                " cannot be derived before the stage runs — that hash IS the"
                " next stage's before_sha256 (the chained-stages law).\n"
                "  Every shipped chained entry carries it; take it from"
                " config/GUNE5D/webfrank.json, or re-derive the pin with"
                " tools/gdl/composed_census/t16_rederive_body.py.")
            return 2
        rules.append(rule)
        try:
            _before, _after, changed = wf.apply_patch(
                probe, json.loads(json.dumps(rule)), bytes(tdata), symbols,
                image)
        except ValueError as failure:
            print(f"REFUSED ({kind}) at stage {index} of {len(stages)}:"
                  f" {failure}")
            if len(stages) == 1 and len(shipped) > 1:
                # The law's whole point: this message is identical to a
                # drifted pin's and the cause here is benign.
                print(
                    f"  NOTE: {arguments.unit}::{arguments.function} ships"
                    f" {len(shipped)} CHAINED entries in webfrank.json, and"
                    " you replayed one stage. A single stage compares the RAW"
                    " body against an INTERMEDIATE hash, so this refusal is"
                    " NOT drift and there is nothing to re-derive. Replay the"
                    " whole chain with --from-config"
                    " (claim.law.CX_a-webfrank-function-can-carry-chained-"
                    "rule-stages-and-replaying-one-alone-refuses-on-a-hash-"
                    "that-is-not-drift.20260904.v1).")
            return 1
        if len(stages) > 1:
            print(f"  stage {index}: applied, {changed} word(s) changed")
    final = bytes(probe[ostart:ostart + osym.size])
    verdict = "BYTE-EQUAL" if final == target else "APPLIED-NOT-EQUAL"
    print(f"{verdict} ({kind}); {len(stages)} stage(s) applied")
    out = arguments.out or os.path.join(
        ROOT, "build", "GUNE5D", f"wr_rule_{arguments.function}.json")
    with open(out, "w", encoding="utf-8") as handle:
        json.dump(rules if len(rules) > 1 else rules[0], handle, indent=2)
    print(f"  wrote {out}")
    return 0 if verdict == "BYTE-EQUAL" else 1


if __name__ == "__main__":
    raise SystemExit(main())
