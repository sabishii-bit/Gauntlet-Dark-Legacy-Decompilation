"""HV run-30: the FORM/REGFIELD ARROW-ORDER defect, and its repair.

THE REPAIR SHIPPED; THIS FILE IS NOW ONLY THE EIGHT-CANDIDATE SWEEP
(run-53 item 2).  Everything below the next paragraph is the history of why
the arrow order matters and is kept because the mechanism is still the point.
What changed: this tool used to monkey-patch its own `corrected_classify`
over `ha_close.classify` before importing hv_repair.  `ha_close.classify`
has since GROWN the repair as a parameter -- its signature is now
`classify(ow, tw, form_first=False)` and `ha_close.build_rule` ENUMERATES
both orders (`for form_first in (False, True)`), adjudicating every candidate
through the shipped apply_patch.  The local patch's signature `(ow, tw)` no
longer matched that caller, so every candidate raised
`TypeError: corrected_classify() got an unexpected keyword argument
'form_first'`, the loop's `except Exception` turned each TypeError into a
per-candidate `refuses` line, and the summary totalled those lines into a
clean-looking `0 of 8 CLOSE ... exit 0`.  Nothing was ever classified.

That negative CONTRADICTED the corpus without saying so: the accepted
claim.HV_regfield-only-class-full-image-sweep-and-its-exhaustion.20260901.v1
records fn_80054E78 as closing under the corrected order, and
attempt.HV_fn80054e78-arrow-order-reclassification.20260901.v1 names THIS
FILE as the reproduction.  Restoring the patch would have been the wrong
cure twice over: it would reinstate a cruder classifier on top of a better
shipped one (the shipped `_same_copy_form` restricts form-first to the
li<->copy direction that is genuinely shadowed, which the local `_form_arrow`
did not), and it would keep the crash-as-refusal shape.  So the patch is
deleted, the sweep runs the SHIPPED classifier, and an exception is now
ABORTED -- counted separately from `refuses` and exiting non-zero -- because
a sweep that renders a crash as a verdict and totals it is the one output
shape indistinguishable from a real measured negative.
Measured after the change: 1 of 8 CLOSE (gamemain::fn_80054E78, arrow order
form-first, families={'fwd': 1, 'regfield': 18}), which is the corpus's own
answer.

THE DEFECT (history).  ha_close.classify used to decide a differing word
pair like this:

    if ow == tw:                 -> "same"
    if regfield_only(ow, tw):    -> "regfield"      <-- tested FIRST
    ... decode_copy_form ...     -> "form"          <-- unreachable for a
                                                        whole family

`mr rD,rS` and `li rD,K` are BOTH encoded as `addi` (opcode 14).  MWCC's move
form is `addi rD,rS,0` and its zero-load is `addi rD,0,0`; the two differ only
in the rA and rD REGISTER FIELDS.  So `regfield_only` returns True for every
mr-versus-li pair, the function returns "regfield", and `decode_copy_form` is
never consulted -- even though webfrank.decode_copy_form exists precisely to
separate these two ("addi treats a zero rA field as the literal value zero
rather than as GPR 0, so addi rD,0,0 is li rD,0 -- a constant load, not a copy
of r0", webfrank.py:2021-2024).

The consequence is not a missed optimisation, it is a MISROUTED PROOF.  The
pair is handed to copy_register_fields, which is asked to prove that some
renaming carries "the contents of r3" to "the literal zero".  No renaming
does, so verify_consistent_recolor refuses -- correctly -- and the refusal
surfaces as `base register presence differs (g3 vs g0)`.  The function then
reads as a hard wall, when the shipped equivalent_copy_form stage has served
exactly this arrow all along.

THE REPAIR, one line of ordering: when a lexically regfield-only pair decodes
to two DIFFERENT copy-form KINDS (one "li", one "copy"), the form arrow wins.
A register renaming can map source A to source B, so a copy/copy pair is
genuinely a recolor; it can never map a register's contents to a literal, so a
li/copy pair never is.  Everything else keeps its current arrow.  That is the
rule ha_close now implements itself, behind `form_first`.

Nothing is simulated: every verdict is webfrank.apply_patch's own residual
count against the extracted retail object.
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
# Repo root by landmark, so this runs unchanged from lane scratch AND from
# tools/gdl/composed_census after promotion (discipline 17).
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    _parent = os.path.dirname(ROOT)
    if _parent == ROOT:
        raise SystemExit("repo root not found above " + HERE)
    ROOT = _parent
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))
sys.path.insert(0, HERE)

import ha_close as ha          # noqa: E402

TARGETS = [
    ("game/enemy/critter", "CritterLoadDone"),
    ("game/sfx/psfx", "LoadPdataFile"),
    ("game/game/gamemain", "fn_80054E78"),
    ("game/ui/attract", "scroll_credits"),
    ("game/enemy/critter", "CritterLoadStartNext"),
    ("game/movie/movieplayer", "fn_800DBE98"),
    ("game/world/tower", "towerRecordLevelBeaten"),
    ("game/sys/memcard", "drawMemCardMessage"),
]

def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Re-prove the eight arrow-order Tier-A candidates through"
                    " the SHIPPED ha_close classifier and apply_patch.",
        epilog="ABORTED is reported separately from refuses and forces a"
               " non-zero exit: an exception rendered as a per-candidate"
               " verdict and then totalled is indistinguishable from a real"
               " negative (run-53 item 2).")
    parser.add_argument(
        "--out", default=os.path.join(ROOT, "build", "HV_formfirst.json"),
        help="where to write the per-candidate JSON (default under build/)")
    args = parser.parse_args(argv)

    import hv_repair as hr
    out = []
    for unit, fn in TARGETS:
        rule, note, verdict = None, "", None
        try:
            rule, _res, note = ha.build_rule(unit, fn)
            if not rule:
                rule, note = hr.repair(unit, fn, verbose=False)
                note = "via repair: " + str(note)
        except SystemExit as exc:
            # AGENTS.md discipline 20: SystemExit is NOT an Exception, and it
            # is this project's REFUSAL idiom, so a blanket `except Exception`
            # would let a refusal from webfrank or ha_close kill the sweep
            # mid-way instead of being counted.
            verdict, note = "ABORTED", f"SystemExit: {exc}"
        except Exception as exc:                            # noqa: BLE001
            verdict, note = "ABORTED", f"{type(exc).__name__}: {exc}"
        if verdict is None:
            verdict = "CLOSES" if rule else "refuses"
        print(f"{verdict:8} {unit}::{fn} -- {str(note)[:170]}", flush=True)
        rec = {"unit": unit, "fn": fn, "verdict": verdict, "note": str(note)}
        if rule:
            rec["rule"] = rule
        out.append(rec)
    with open(args.out, "w") as fh:
        json.dump(out, fh, indent=1)
    closes = sum(1 for r in out if r["verdict"] == "CLOSES")
    aborted = sum(1 for r in out if r["verdict"] == "ABORTED")
    print(f"\n{closes} of {len(out)} CLOSE under the enumerated arrow orders")
    if aborted:
        # NOT folded into the CLOSE count and NOT reported as refusals: these
        # candidates were never classified at all.
        print(f"{aborted} of {len(out)} ABORTED (not refused, not measured)")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
