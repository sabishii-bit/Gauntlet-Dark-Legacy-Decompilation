"""CE lane (run 43): IMAGE-WIDE DEMAND CENSUS for the constant-equality closure.

Gate for claim.WR_constant-equality-closure-class-proposal.20260903.v1, whose
own census covered four named functions plus two controls and said so
("NOT SCREENED IMAGE-WIDE ... the integrator should require an image-wide
screen before commissioning anything").  AGENTS.md: "A capability census asks
UNPARK PAYOFF, not site population."

Method, per paired equal-size function whose raw body differs from the target:

  1. Classify every differing word with wf_census.classify.
  2. TIER 1 - every differing word is `regfield`: the register stage alone can
     reach the target, no form edit needed.
     TIER 2 - differing words are regfield + copy-form sites and NOTHING else
     (`other` == 0): auto-derive one equivalent_copy_form edit per copy-form
     site by asking the SHIPPED guard itself (each candidate proof mode is
     offered to equivalent_copy_form and only an accepted one is kept), then
     apply the accepted set.  This is the composition scroll_credits needed;
     a tier-1-only census would have missed the proposal's own payoff row.
     Anything with a structural differing word is out of scope and counted.
  3. Require copy_register_fields(pre_image, target) == target, i.e. the
     register stage can finish the function.  Otherwise it is not a candidate
     for EITHER column and no widening of the relation can serve it.
  4. Run the run-42 prototype driver twice over the same images: the SHIPPED
     value-equality relation, and the relation seeded with constant equality.

  UNPARK PAYOFF = rows where the shipped column REFUSES and the closure column
  PROVES with byte-equality.  Rows PROVED by both are the control set (the
  closure must not change a verdict that already holds), rows refused by both
  are unserved either way.

    python tools/gdl/composed_census/ce_const_closure_census.py \
        [--out PATH] [--limit N]

Read-only: no object, no source and no webfrank surface is written.
Requires a completed `ninja` so both build/GUNE5D/src and build/GUNE5D/obj
are current; stale objects silently misreport.
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                                          # noqa: E402
from wf_census import units, our_path, functions, OBJ, classify  # noqa: E402
from wr_const_closure_probe import prove                        # noqa: E402

COPY_FORM = ("fwd", "fwd_rc", "inv", "inv_rc")


STAGE_KEYS = ("copy_register_fields", "instruction_permutation",
              "post_recolor_permutation", "equivalent_copy_form",
              "equivalent_mask_form", "equivalent_zero_form",
              "value_equality_recolor", "unproven_recolor_audit",
              "memory_disambiguation", "register_fields", "recolors")


def pinned_set():
    path = os.path.join(ROOT, "config", "GUNE5D", "webfrank.json")
    config = json.load(open(path, encoding="utf-8"))
    out, classes = set(), {}
    for unit, rules in config.get("units", config).items():
        for rule in rules:
            out.add((unit, rule["function"]))
            classes[(unit, rule["function"])] = [
                key for key in STAGE_KEYS if rule.get(key)]
    return out, classes


def form_candidates(ours, target, offset):
    """Proof modes worth offering at one differing word.

    CALIBRATION NOTE (this is the census defect the first run had).  A site
    where ours is `addi rD,rS,0` and the target is `li rD',K` differs ONLY in
    register fields at the ENCODING level -- both are opcode 14 with immediate
    0 -- so wf_census.classify calls it `regfield` and a form-site detector
    keyed on that label never offers it to equivalent_copy_form.  That is
    exactly scroll_credits' +0xe4 and +0x174, the proposal's own payoff row,
    which the first pass of this census scored as tier 1 and watched refuse
    with `base register presence differs (g25 vs g0)`.  Detect form sites by
    DECODED COPY FORM, never by the encoding-level regfield label.
    """
    our_form = wf.decode_copy_form(wf._u32(ours, offset))
    target_form = wf.decode_copy_form(wf._u32(target, offset))
    if our_form is None or target_form is None:
        return []
    if our_form[0] == "copy" and target_form[0] == "li":
        return [{"at": offset, "proof": "constant_dataflow_inverse_recolor"}]
    if our_form[0] == "li" and target_form[0] == "copy":
        return [{"at": offset, "proof": "constant_dataflow_recolor",
                 "our_source": r} for r in range(1, 32)]
    if our_form[0] == "copy" and target_form[0] == "copy":
        return [{"at": offset, "proof": "unconditional_recolor"}]
    return []


def derive_edits(pre, target, sites, common):
    """Ask the SHIPPED guard which proof mode (if any) each site accepts.

    Nothing here decides equivalence: every edit that survives was accepted by
    equivalent_copy_form itself, so a census hit cannot claim a proof the
    build would not also make.
    """
    accepted, rejected = [], []
    for offset, required in sites:
        for edit in form_candidates(pre, target, offset):
            try:
                wf.equivalent_copy_form(pre, target, [edit], **common)
            except ValueError:
                continue
            accepted.append(edit)
            break
        else:
            if required:
                rejected.append(offset)
    return accepted, rejected


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--out", default=os.path.join(
        ROOT, "build", "GUNE5D", "ce_const_closure_census.json"))
    parser.add_argument("--limit", type=int, default=0)
    arguments = parser.parse_args()

    pinned, pin_class = pinned_set()
    rows = []
    tally = {"paired": 0, "differ": 0, "tier1": 0, "tier2": 0,
             "structural_out": 0, "form_derive_failed": 0,
             "register_stage_cannot_finish": 0}

    for unit in units():
        path, _raw = our_path(unit)
        if not path:
            continue
        try:
            our_data = bytearray(open(path, "rb").read())
            target_data = bytearray(
                open(os.path.join(OBJ, unit + ".o"), "rb").read())
            our_sections = wf._sections(our_data)
            target_sections = wf._sections(target_data)
        except Exception:
            continue
        target_by_name = {s.name: s
                          for s in functions(target_data, target_sections)}
        for our_symbol in functions(our_data, our_sections):
            target_symbol = target_by_name.get(our_symbol.name)
            if target_symbol is None or target_symbol.size != our_symbol.size:
                continue
            tally["paired"] += 1
            our_text = our_sections[our_symbol.section_index]
            target_text = target_sections[target_symbol.section_index]
            ours = bytes(our_data[our_text.offset + our_symbol.value:
                                  our_text.offset + our_symbol.value
                                  + our_symbol.size])
            target = bytes(target_data[
                target_text.offset + target_symbol.value:
                target_text.offset + target_symbol.value + target_symbol.size])
            if ours == target:
                continue
            tally["differ"] += 1

            counts, form_sites = {}, []
            for offset in range(0, len(ours), 4):
                our_word = wf._u32(ours, offset)
                target_word = wf._u32(target, offset)
                if our_word == target_word:
                    continue
                kind = classify(our_word, target_word)
                counts[kind] = counts.get(kind, 0) + 1
                if kind in COPY_FORM:
                    form_sites.append((offset, True))
                elif kind == "regfield":
                    # An OPTIONAL form site: reachable by the register stage
                    # alone, but possibly a copy-versus-constant pair whose
                    # form change is what the relation needs.  Offered, and
                    # dropped silently if the guard refuses it.
                    form_sites.append((offset, False))
            if counts.get("other"):
                tally["structural_out"] += 1
                continue

            relocations = wf._function_text_relocations(
                our_data, our_sections, our_symbol.section_index,
                our_symbol.value, our_symbol.value + our_symbol.size)
            target_relocations = wf._function_text_relocations(
                target_data, target_sections, target_symbol.section_index,
                target_symbol.value,
                target_symbol.value + target_symbol.size)
            jumptable = wf._jumptable_targets(
                our_data, our_sections, our_symbol.section_index,
                our_symbol.value, our_symbol.value + our_symbol.size)
            call_targets = {o: n for o, (k, n) in relocations.items()
                            if k == 10}
            common = dict(relocated_offsets=set(relocations),
                          target_relocated_offsets=set(target_relocations),
                          jumptable_offsets=jumptable,
                          call_targets=call_targets,
                          relocation_types={o // 4: k for o, (k, _n)
                                            in relocations.items()})

            pre = ours
            edits, rejected = derive_edits(ours, target, form_sites, common)
            if rejected:
                tally["form_derive_failed"] += 1
                continue
            if edits:
                pre, _n = wf.equivalent_copy_form(ours, target, edits,
                                                  **common)
                tally["tier2"] += 1
            else:
                tally["tier1"] += 1

            try:
                post, _n = wf.copy_register_fields(pre, target)
            except ValueError:
                tally["register_stage_cannot_finish"] += 1
                continue
            if post != target:
                tally["register_stage_cannot_finish"] += 1
                continue

            # THE STRICT COLUMN IS THE HONESTY GATE.  apply_patch tries
            # verify_consistent_recolor FIRST and only reaches the wider modes
            # when it refuses, so a function the strict proof already serves is
            # a customer for nothing, however the wider columns score it.
            # Without this column the first pass of this census reported four
            # already-proved pins as payoff.
            try:
                wf.verify_consistent_recolor(
                    pre, post, jumptable_targets=jumptable,
                    relocated_offsets=set(relocations),
                    call_targets=call_targets)
                strict = {"verdict": "PROVED"}
            except ValueError as failure:
                strict = {"verdict": "REFUSED", "why": str(failure)}

            verdicts = {}
            for label, closure in (("shipped", False), ("closure", True)):
                failure, subs, exch = prove(
                    pre, post,
                    jumptable_targets=jumptable,
                    relocated_offsets=set(relocations),
                    target_relocated_offsets=set(target_relocations),
                    call_targets=call_targets, constant_closure=closure)
                verdicts[label] = ({"verdict": "REFUSED", "why": failure}
                                   if failure else
                                   {"verdict": "PROVED",
                                    "substitutions": len(subs),
                                    "exchanges": len(exch)})
            rows.append({
                "unit": unit, "function": our_symbol.name,
                "insns": our_symbol.size // 4,
                "tier": 2 if edits else 1,
                "form_edits": len(edits),
                "differing_words": sum(counts.values()),
                "pinned": (unit, our_symbol.name) in pinned,
                "pin_class": sorted(pin_class.get((unit, our_symbol.name), [])),
                "strict": strict,
                "shipped": verdicts["shipped"], "closure": verdicts["closure"],
                "byte_equal": post == target,
            })
            if arguments.limit and len(rows) >= arguments.limit:
                break
        if arguments.limit and len(rows) >= arguments.limit:
            break

    strict_served = [r for r in rows if r["strict"]["verdict"] == "PROVED"]
    open_rows = [r for r in rows if r["strict"]["verdict"] == "REFUSED"]
    shipped_served = [r for r in open_rows
                      if r["shipped"]["verdict"] == "PROVED"]
    payoff = [r for r in open_rows
              if r["shipped"]["verdict"] == "REFUSED"
              and r["closure"]["verdict"] == "PROVED"]
    unserved = [r for r in open_rows
                if r["shipped"]["verdict"] == "REFUSED"
                and r["closure"]["verdict"] == "REFUSED"]
    changed_control = [r for r in shipped_served
                       if r["closure"]["verdict"] != "PROVED"
                       or r["closure"].get("substitutions")
                       != r["shipped"].get("substitutions")
                       or r["closure"].get("exchanges")
                       != r["shipped"].get("exchanges")]

    print("CONSTANT-EQUALITY CLOSURE - IMAGE-WIDE DEMAND CENSUS")
    for key in ("paired", "differ", "structural_out", "form_derive_failed",
                "register_stage_cannot_finish", "tier1", "tier2"):
        print(f"  {key:32} {tally[key]}")
    print(f"  {'screened (all three columns)':32} {len(rows)}")
    print(f"  {'  strict recolor already PROVES':32} {len(strict_served)}"
          f"   (customers for nothing)")
    print(f"  {'  strict REFUSES (open rows)':32} {len(open_rows)}")
    print()
    # STAGING FIDELITY.  This census stages every function the same way: raw
    # body -> auto-derived copy-form edits -> copy_register_fields.  A pin that
    # carries an instruction_permutation, a post_recolor_permutation, or its
    # OWN equivalent_copy_form edit list is verified by the build over a
    # DIFFERENT intermediate image, so a refusal in this census's shipped
    # column says nothing about that pin -- the build already proves it (the
    # rule declares no value_equality_recolor and no unproven_recolor_audit,
    # and apply_patch has no third acceptance path).  Those rows are staging
    # artifacts, not demand, and calling them payoff would have overstated the
    # class's value five-fold.
    foreign_stages = {"instruction_permutation", "post_recolor_permutation",
                      "equivalent_copy_form", "equivalent_mask_form",
                      "equivalent_zero_form", "memory_disambiguation",
                      "register_fields", "recolors"}
    for row in rows:
        row["staging_replayed"] = not (set(row["pin_class"])
                                       & foreign_stages)
    real = [r for r in payoff if r["staging_replayed"]]
    artifact = [r for r in payoff if not r["staging_replayed"]]

    print(f"UNPARK PAYOFF (strict REFUSED, shipped value-equality REFUSED, "
          f"closure PROVED): {len(payoff)} rows, of which "
          f"{len(real)} are real demand and {len(artifact)} are staging "
          f"artifacts")
    for label, group in (("REAL DEMAND", real), ("STAGING ARTIFACT", artifact)):
        print(f"  -- {label} --")
        for row in sorted(group, key=lambda r: -r["insns"]):
            served = ("+".join(row["pin_class"]) if row["pinned"]
                      else "UNPINNED - a new rule here is +1 EQUIVALENT")
            print(f"  {row['unit']}::{row['function']}  {row['insns']} insns  "
                  f"tier {row['tier']} ({row['form_edits']} form edit(s))  "
                  f"{row['closure']['substitutions']} sub(s) "
                  f"{row['closure']['exchanges']} exch  "
                  f"byte-equal={row['byte_equal']}")
            print(f"      served today by: {served}")
            print(f"      shipped refusal: {row['shipped']['why']}")

    unproven = [r for r in rows if "unproven_recolor_audit" in r["pin_class"]]
    retired = [r for r in unproven if r in payoff]
    print()
    print(f"UNPROVEN PINS REACHED BY THIS CENSUS: {len(unproven)}; "
          f"RETIRED BY THE CLOSURE: {len(retired)}")
    for row in unproven:
        mark = "RETIRED " if row in payoff else "stands  "
        print(f"  {mark}{row['unit']}::{row['function']}  {row['insns']} insns"
              f"  closure: {row['closure']['verdict']}"
              f" {row['closure'].get('why', '')}")
    print()
    print(f"ALREADY SERVED BY THE SHIPPED VALUE-EQUALITY MODE: "
          f"{len(shipped_served)}; declarations changed by the closure: "
          f"{len(changed_control)}")
    for row in changed_control:
        print(f"  !! {row['unit']}::{row['function']}  "
              f"shipped={row['shipped']}  closure={row['closure']}")
    print()
    print(f"UNSERVED BY EITHER WIDER COLUMN: {len(unserved)}")
    reasons = {}
    for row in unserved:
        why = row["closure"]["why"]
        key = why.split(":", 1)[-1].strip()[:64]
        reasons[key] = reasons.get(key, 0) + 1
    for why, count in sorted(reasons.items(), key=lambda kv: -kv[1]):
        print(f"  {count:4}  {why}")
    print()
    print("UNSERVED ROWS (the residue a further widening would have to face):")
    for row in sorted(unserved, key=lambda r: -r["insns"]):
        print(f"  {row['unit']}::{row['function']}  {row['insns']} insns  "
              f"{'PINNED(' + '+'.join(row['pin_class']) + ')' if row['pinned'] else 'unpinned'}"
              f"  closure refusal: {row['closure']['why']}")

    os.makedirs(os.path.dirname(arguments.out), exist_ok=True)
    with open(arguments.out, "w", encoding="utf-8") as handle:
        json.dump({"tally": tally, "rows": rows,
                   "payoff": [f"{r['unit']}::{r['function']}" for r in payoff]},
                  handle, indent=2)
    print(f"\nwrote {arguments.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
