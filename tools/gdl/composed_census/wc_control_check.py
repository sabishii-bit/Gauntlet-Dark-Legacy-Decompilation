"""WC lane (run 44): TWO-SIDED CONTROL for the constant-equality closure.

The regression set is exactly the rules that DECLARE value_equality_recolor:
apply_patch reaches the mode only for those, and only after the strict proof
refuses.  For each such rule this replays the real apply_patch, captures the
exact pre-recolor and post-recolor images the mode is proved over (by wrapping
the verifier in this script -- webfrank is not modified), and then re-runs the
shipped verifier over those same images twice:

    NARROW  = constant_equality=False, the rule's own declarations
    CLOSED  = constant_equality=True,  the rule's own declarations

What must hold depends on what the rule declares, and both directions are
checked:

  * a rule that does NOT declare `constant_equality` must prove under BOTH,
    with the same declarations - that is the no-silent-widening control;
  * a rule that DOES declare it must prove under CLOSED and REFUSE under
    NARROW - that is the anti-rot invariant apply_patch enforces by name
    ("declared but the value-equality proof succeeds without the closure").

Anything else is printed as !! FAILED.

    python tools/gdl/composed_census/wc_control_check.py
"""
import json
import os
import sys

ROOT = os.path.abspath(os.path.join(
    os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                                          # noqa: E402
from cn_analyze import our_object, target_object                # noqa: E402

REAL = wf.verify_value_equality_recolor
CAPTURED = []


def recorder(current, target, **kwargs):
    CAPTURED.append((current, target, dict(kwargs)))
    return REAL(current, target, **kwargs)


def declaration_demand(current, target, kwargs, constant_equality):
    """The declarations the mode would demand: PROVED with the rule's own
    lists, or the verbatim refusal."""
    arguments = dict(kwargs)
    arguments["constant_equality"] = constant_equality
    try:
        REAL(current, target, **arguments)
    except ValueError as failure:
        return f"REFUSED -- {failure}"
    return (f"PROVED with {len(arguments.get('substitutions') or ())} "
            f"declared substitution(s), "
            f"{len(arguments.get('compare_exchanges') or ())} exchange(s)")


def main():
    config = json.load(open(os.path.join(ROOT, "config", "GUNE5D",
                                         "webfrank.json"), encoding="utf-8"))
    units = config.get("units", config)
    failures = 0
    checked = 0
    wf.verify_value_equality_recolor = recorder
    for unit, rules in units.items():
        for rule in rules:
            if not rule.get("value_equality_recolor"):
                continue
            checked += 1
            ours_path, kind = our_object(unit)
            odata = bytearray(open(ours_path, "rb").read())
            tdata = bytes(open(target_object(unit), "rb").read())
            CAPTURED.clear()
            try:
                wf.apply_patch(odata, json.loads(json.dumps(rule)), tdata)
            except ValueError as failure:
                print(f"!! FAILED {unit}::{rule['function']}: rule does not "
                      f"replay at all ({failure})")
                failures += 1
                continue
            if not CAPTURED:
                print(f"!! FAILED {unit}::{rule['function']}: the "
                      f"value-equality proof never ran")
                failures += 1
                continue
            # A rule declaring `constant_equality` makes apply_patch run the
            # proof TWICE - once narrow, to assert the opt-in is still needed,
            # then once for real.  The last call is the load-bearing one.
            current, target, kwargs = CAPTURED[-1]
            narrow = declaration_demand(current, target, kwargs, False)
            closed = declaration_demand(current, target, kwargs, True)
            declared = rule["value_equality_recolor"]
            if declared.get("constant_equality"):
                held = (closed.startswith("PROVED")
                        and narrow.startswith("REFUSED"))
            else:
                held = narrow == closed and narrow.startswith("PROVED")
            mark = "  held  " if held else "!! FAILED"
            print(f"{mark} {unit}::{rule['function']}  ({kind}, "
                  f"{len(current)//4} insns, "
                  f"{len(declared.get('substitutions', ()))} sub / "
                  f"{len(declared.get('compare_exchanges', ()))} exch)")
            print(f"      NARROW: {narrow}")
            print(f"      CLOSED: {closed}")
            if not held:
                failures += 1
    print()
    print(f"value-equality rules checked: {checked}; controls failed: "
          f"{failures}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
