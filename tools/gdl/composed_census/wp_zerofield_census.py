"""WP lane (run 45): IMAGE-WIDE DEMAND CENSUS for the commutative-exchange
ZERO-FIELD refinement, and its standing regression control.

Gate for claim.WC_commutative-exchange-zero-field-precision-proposal.20260903.v1,
whose own census covered the unproven pins plus the shipped value-equality
rules and said so verbatim ("NOT SCREENED IMAGE-WIDE ... Before commissioning
anything the integrator should require the image-wide screen that
claim.CE_constant-equality-closure-demand-census-image-wide.20260903.v1 is the
model for").  This is that sweep.  AGENTS.md: "A capability census asks UNPARK
PAYOFF, not site population."

THE COMPARISON IS LEGACY-vs-SHIPPED, NOT PROTOTYPE-vs-SHIPPED.  The run-44
prototype (wc_zerofield_probe.py) monkey-patched a REFINED transfer beside the
shipped one; once the refinement shipped, that comparison collapses to
shipped-vs-shipped and reads a payoff of zero, which looks exactly like a
refutation.  This tool instead carries the pre-run-45 POSITION-KEYED transfer
verbatim as `legacy_transfer` and scores it against webfrank's current one, so
the same table can be re-run at any later commit -- which is what the claim
record asks for ("the image-wide control run above, re-run after the change
rather than in a prototype").

Three columns over one pair of images, in apply_patch's own order:
  1. STRICT   verify_consistent_recolor
  2. LEGACY   the value-equality relation with the run-44 position-keyed zero
              tests, seeded with constant equality (the widest configuration
              that shipped BEFORE run 45)
  3. SHIPPED  webfrank's current transfer, same seeding

Method is the CE census's, with both of its recorded calibration defects
carried forward as fixes rather than re-discovered:

  DEFECT 1 (form-site detection).  Form sites are detected by DECODED COPY
  FORM (wf.decode_copy_form on both words, kinds differing), never by the
  encoding-level `regfield` label -- `addi rD,rS,0` against `li rD',K` is a
  pure register-field difference at the encoding level and a label-keyed
  detector never offers it to equivalent_copy_form.
  (claim.law.CE_a-copy-versus-constant-site-encodes-as-a-pure-register-field-
  difference.20260903.v1)

  DEFECT 2 (the FIRST-TRIED PROOF column).  apply_patch tries the columns in
  order and reaches a later one only when the earlier refuses, so a function
  an earlier column already serves is a customer for nothing however the later
  columns score it.  Every row carries `first_proof` and payoff counts only
  rows whose first proof is the shipped refinement.

  STAGING FIDELITY.  This census stages every function one way: raw body ->
  auto-derived copy-form edits -> copy_register_fields.  A pin carrying a
  permutation, a mask/zero form or its OWN copy-form edit list is verified by
  the build over a DIFFERENT intermediate image, so a refusal here says
  nothing about it; those rows are marked `staging_replayed: false` and
  reported separately as staging artifacts, never as demand.

    python tools/gdl/composed_census/wp_zerofield_census.py [--out PATH]
        [--limit N]

Read-only: no object, no source and no webfrank surface is written.  Requires
a completed `ninja` so build/GUNE5D/src and build/GUNE5D/obj are both current;
stale objects silently misreport.
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

import webfrank as wf                                          # noqa: E402
from wf_census import units, our_path, functions, OBJ, classify  # noqa: E402
from wr_const_closure_probe import prove                        # noqa: E402

COPY_FORM = ("fwd", "fwd_rc", "inv", "inv_rc")

STAGE_KEYS = ("copy_register_fields", "instruction_permutation",
              "post_recolor_permutation", "equivalent_copy_form",
              "equivalent_mask_form", "equivalent_zero_form",
              "value_equality_recolor", "unproven_recolor_audit",
              "memory_disambiguation", "register_fields", "recolors")

FOREIGN_STAGES = {"instruction_permutation", "post_recolor_permutation",
                  "equivalent_copy_form", "equivalent_mask_form",
                  "equivalent_zero_form", "memory_disambiguation",
                  "register_fields", "recolors"}


def legacy_transfer(index, cur, tgt, relation, renaming, our_copy,
                    target_copy, substitutions, exchanges):
    """webfrank._value_equality_transfer as it stood BEFORE run 45.

    The two differences from the shipped function, and nothing else: the
    `zero_involved` gate ORs the two RA|0 flags and then looks for a 0 in ANY
    of the four operand values, and the per-field zero test uses the flag of
    the position a value LANDS in rather than the field it came FROM.
    """
    opcode = cur >> 26
    if opcode in (46, 47):
        renaming = wf._recolor_transfer(index, cur, tgt, renaming)
        if opcode == 46:
            for number in range((cur >> 21) & 0x1F, 32):
                relation = wf._relation_define(relation, "g", number, number)
        return relation, renaming
    try:
        operands = wf.instruction_operands(cur)
    except ValueError as error:
        raise ValueError(f"+0x{index * 4:x}: {error}") from None
    allowed = 0
    for _, shift, _, _ in operands:
        allowed |= 0x1F << shift
    if (cur ^ tgt) & ~allowed:
        raise ValueError(
            f"+0x{index * 4:x}: non-register bits differ "
            f"(0x{cur:08x} vs 0x{tgt:08x})")
    fields = {
        shift: (bank, role, zero_none,
                (cur >> shift) & 0x1F, (tgt >> shift) & 0x1F)
        for bank, shift, role, zero_none in operands
    }
    compare_field = wf._compare_result_field(cur)
    pair = wf._commutative_shifts(cur)
    if pair is None and compare_field is not None:
        pair = (16, 11)
    remap: dict = {}
    if pair is not None and all(shift in fields for shift in pair):
        first, second = pair
        bank, _, zero_1, cur_1, tgt_1 = fields[first]
        _, _, zero_2, cur_2, tgt_2 = fields[second]
        zero_involved = (zero_1 or zero_2) and 0 in (cur_1, cur_2,
                                                     tgt_1, tgt_2)
        straight = ((bank, cur_1, tgt_1) in relation
                    and (bank, cur_2, tgt_2) in relation)
        if not straight and not zero_involved \
                and (bank, cur_1, tgt_2) in relation \
                and (bank, cur_2, tgt_1) in relation:
            remap = {first: tgt_2, second: tgt_1}
            if compare_field is not None:
                exchanges.add((index, bank, cur_1, cur_2, tgt_1, tgt_2))
    for shift, (bank, role, zero_none, cur_r, tgt_r) in fields.items():
        expected = remap.get(shift, tgt_r)
        if zero_none and (cur_r == 0 or expected == 0):
            if cur_r != expected:
                raise ValueError(
                    f"+0x{index * 4:x}: base register presence differs "
                    f"({bank}{cur_r} vs {bank}{expected})")
            continue
        if role not in ("u", "b"):
            continue
        if (bank, cur_r, expected) not in relation:
            raise ValueError(
                f"+0x{index * 4:x}: use of {bank}{cur_r} is not value-equal "
                f"to {bank}{expected}")
        if renaming.get((bank, cur_r)) != expected:
            substitutions.add((index, bank, cur_r, expected))
    for _, (bank, role, zero_none, cur_r, tgt_r) in fields.items():
        if zero_none and cur_r == 0:
            continue
        if role not in ("d", "b"):
            continue
        if our_copy is not None and target_copy is not None \
                and our_copy[1] == our_copy[2] \
                and target_copy[1] == target_copy[2]:
            continue
        relation = wf._relation_define(relation, bank, cur_r, tgt_r)
        wf._map_define(renaming, (bank, cur_r), tgt_r)
        if our_copy is not None and our_copy[0] == bank \
                and our_copy[1] == cur_r and our_copy[2] != cur_r:
            source = our_copy[2]
            relation |= {
                (bank, cur_r, other) for kind, one, other in tuple(relation)
                if kind == bank and one == source}
        if target_copy is not None and target_copy[0] == bank \
                and target_copy[1] == tgt_r and target_copy[2] != tgt_r:
            source = target_copy[2]
            relation |= {
                (bank, one, tgt_r) for kind, one, other in tuple(relation)
                if kind == bank and other == source}
    return relation, renaming


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
    """Proof modes worth offering at one differing word (DEFECT 1 fix)."""
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
    """Ask the SHIPPED guard which proof mode (if any) each site accepts."""
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


def run_column(pre, post, common_proof, *, legacy):
    real = wf._value_equality_transfer
    if legacy:
        wf._value_equality_transfer = legacy_transfer
    try:
        failure, subs, exch = prove(pre, post, constant_closure=True,
                                    **common_proof)
    finally:
        wf._value_equality_transfer = real
    if failure:
        return {"verdict": "REFUSED", "why": failure}
    return {"verdict": "PROVED", "substitutions": len(subs),
            "exchanges": len(exch)}


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--out", default=os.path.join(
        ROOT, "build", "GUNE5D", "wp_zerofield_census.json"))
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

            try:
                wf.verify_consistent_recolor(
                    pre, post, jumptable_targets=jumptable,
                    relocated_offsets=set(relocations),
                    call_targets=call_targets)
                strict = {"verdict": "PROVED"}
            except ValueError as failure:
                strict = {"verdict": "REFUSED", "why": str(failure)}

            common_proof = dict(
                jumptable_targets=jumptable,
                relocated_offsets=set(relocations),
                target_relocated_offsets=set(target_relocations),
                call_targets=call_targets)
            legacy = run_column(pre, post, common_proof, legacy=True)
            shipped = run_column(pre, post, common_proof, legacy=False)

            first = "none"
            for label, column in (("strict", strict), ("legacy", legacy),
                                  ("shipped", shipped)):
                if column["verdict"] == "PROVED":
                    first = label
                    break

            classes = sorted(pin_class.get((unit, our_symbol.name), []))
            rows.append({
                "unit": unit, "function": our_symbol.name,
                "insns": our_symbol.size // 4,
                "tier": 2 if edits else 1,
                "form_edits": len(edits),
                "differing_words": sum(counts.values()),
                "pinned": (unit, our_symbol.name) in pinned,
                "pin_class": classes,
                "staging_replayed": not (set(classes) & FOREIGN_STAGES),
                "strict": strict, "legacy": legacy, "shipped": shipped,
                "first_proof": first,
                "byte_equal": post == target,
            })
            if arguments.limit and len(rows) >= arguments.limit:
                break
        if arguments.limit and len(rows) >= arguments.limit:
            break

    payoff = [r for r in rows if r["first_proof"] == "shipped"
              and r["byte_equal"]]
    real = [r for r in payoff if r["staging_replayed"]]
    artifact = [r for r in payoff if not r["staging_replayed"]]
    strict_served = [r for r in rows if r["first_proof"] == "strict"]
    legacy_served = [r for r in rows if r["first_proof"] == "legacy"]
    unserved = [r for r in rows if r["first_proof"] == "none"]
    regressed = [
        r for r in rows
        if r["legacy"]["verdict"] == "PROVED"
        and (r["shipped"]["verdict"] != "PROVED"
             or r["shipped"].get("substitutions")
             != r["legacy"].get("substitutions")
             or r["shipped"].get("exchanges")
             != r["legacy"].get("exchanges"))]

    print("COMMUTATIVE-EXCHANGE ZERO-FIELD REFINEMENT "
          "- IMAGE-WIDE DEMAND CENSUS (legacy vs shipped)")
    for key in ("paired", "differ", "structural_out", "form_derive_failed",
                "register_stage_cannot_finish", "tier1", "tier2"):
        print(f"  {key:34} {tally[key]}")
    print(f"  {'screened (all three columns)':34} {len(rows)}")
    print(f"  {'  first proof = STRICT':34} {len(strict_served)}"
          f"   (customers for nothing)")
    print(f"  {'  first proof = LEGACY':34} {len(legacy_served)}"
          f"   (customers for nothing)")
    print(f"  {'  first proof = SHIPPED refinement':34} {len(payoff)}")
    print(f"  {'  proved by no column':34} {len(unserved)}")
    print()
    print(f"UNPARK PAYOFF (first proof = the refinement, byte-equal): "
          f"{len(payoff)} rows, of which {len(real)} are real demand and "
          f"{len(artifact)} are staging artifacts")
    for label, group in (("REAL DEMAND", real),
                         ("STAGING ARTIFACT", artifact)):
        print(f"  -- {label} --")
        for row in sorted(group, key=lambda r: -r["insns"]):
            served = ("+".join(row["pin_class"]) if row["pinned"]
                      else "UNPINNED - a new rule here is +1 EQUIVALENT")
            print(f"  {row['unit']}::{row['function']}  {row['insns']} insns  "
                  f"tier {row['tier']} ({row['form_edits']} form edit(s))  "
                  f"{row['shipped']['substitutions']} sub(s) "
                  f"{row['shipped']['exchanges']} exch")
            print(f"      served today by: {served}")
            print(f"      legacy refusal: {row['legacy']['why']}")

    unproven = [r for r in rows if "unproven_recolor_audit" in r["pin_class"]]
    print()
    print(f"RULES STILL RESTING ON unproven_recolor_audit AND REACHED BY "
          f"THIS CENSUS: {len(unproven)}")
    for row in unproven:
        print(f"  stands  {row['unit']}::{row['function']}  "
              f"{row['insns']} insns  "
              f"staging_replayed={row['staging_replayed']}  "
              f"shipped: {row['shipped']['verdict']} "
              f"{row['shipped'].get('why', '')}")
    print()
    legacy_proves = [r for r in rows if r["legacy"]["verdict"] == "PROVED"]
    print(f"REGRESSION CONTROL - rows the legacy column proves: "
          f"{len(legacy_proves)}; verdict or declaration count moved by the "
          f"refinement: {len(regressed)}")
    for row in regressed:
        print(f"  !! {row['unit']}::{row['function']}  "
              f"legacy={row['legacy']}  shipped={row['shipped']}")
    print()
    print(f"PROVED BY NO COLUMN: {len(unserved)} "
          f"(the residue a further widening would face)")
    reasons = {}
    for row in unserved:
        why = row["shipped"]["why"]
        key = why.split(":", 1)[-1].strip()[:64]
        reasons[key] = reasons.get(key, 0) + 1
    for why, count in sorted(reasons.items(), key=lambda kv: -kv[1])[:20]:
        print(f"  {count:4}  {why}")

    os.makedirs(os.path.dirname(arguments.out), exist_ok=True)
    with open(arguments.out, "w", encoding="utf-8") as handle:
        json.dump({"tally": tally, "rows": rows,
                   "payoff": [f"{r['unit']}::{r['function']}"
                              for r in payoff]}, handle, indent=2)
    print(f"\nwrote {arguments.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
