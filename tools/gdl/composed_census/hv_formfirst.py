"""HV run-30: the FORM/REGFIELD ARROW-ORDER defect, and its repair.

THE DEFECT.  ha_close.classify decides a differing word pair like this:

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
li/copy pair never is.  Everything else keeps its current arrow.

Nothing is simulated: the corrected classifier is patched into the SHIPPED
ha_close.build_rule and every verdict is webfrank.apply_patch's own residual
count against the extracted retail object.
"""
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

import webfrank as wf          # noqa: E402
import ha_close as ha          # noqa: E402

_ORIGINAL = ha.classify


def corrected_classify(ow, tw):
    """ha_close.classify with the form arrow lifted above the regfield arrow
    for kind-mismatched copy forms."""
    if ow == tw:
        return "same", None
    if ha.regfield_only(ow, tw):
        ours, theirs = wf.decode_copy_form(ow), wf.decode_copy_form(tw)
        if ours is not None and theirs is not None and ours[0] != theirs[0]:
            # li vs copy: a renaming cannot bridge these.  Fall through to the
            # ORIGINAL classifier's form logic by re-asking it about a pair it
            # would have routed correctly had regfield_only not shadowed it.
            forced = _ORIGINAL.__wrapped__(ow, tw) \
                if hasattr(_ORIGINAL, "__wrapped__") else None
            if forced is not None:
                return forced
            return _form_arrow(ours, theirs)
        return "regfield", None
    return _ORIGINAL(ow, tw)


def _form_arrow(ours, theirs):
    """The form half of ha_close.classify, applied to already-decoded pairs."""
    same_dest = ours[1] == theirs[1]
    if ours[0] == "li" and theirs[0] == "copy":
        if theirs[2] == 0:
            return "other", None
        if same_dest:
            return "form", ("fwd", ha.PURE_FWD + ["constant_dataflow_recolor"])
        return "form", ("fwd_rc", ["constant_dataflow_recolor"])
    if ours[0] == "copy" and theirs[0] == "li":
        if ours[2] == 0:
            return "other", None
        if same_dest:
            return "form", ("inv",
                            ha.PURE_INV + ["constant_dataflow_inverse_recolor"])
        return "form", ("inv_rc", ["constant_dataflow_inverse_recolor"])
    return "other", None


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

if __name__ == "__main__":
    ha.classify = corrected_classify          # the one-line ordering repair
    import hv_repair as hr                    # imported AFTER the patch
    out = []
    for unit, fn in TARGETS:
        rule, note = None, ""
        try:
            rule, _res, note = ha.build_rule(unit, fn)
            if not rule:
                rule, note = hr.repair(unit, fn, verbose=False)
                note = "via repair: " + str(note)
        except Exception as exc:                            # noqa: BLE001
            note = f"{type(exc).__name__}: {exc}"
        verdict = "CLOSES" if rule else "refuses"
        print(f"{verdict:8} {unit}::{fn} -- {str(note)[:170]}", flush=True)
        rec = {"unit": unit, "fn": fn, "verdict": verdict, "note": str(note)}
        if rule:
            rec["rule"] = rule
        out.append(rec)
    with open(os.path.join(ROOT, "build", "HV_formfirst.json"), "w") as fh:
        json.dump(out, fh, indent=1)
    print(f"\n{sum(1 for r in out if r['verdict'] == 'CLOSES')} of {len(out)} "
          f"CLOSE under the corrected arrow order")
