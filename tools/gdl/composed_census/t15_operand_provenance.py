"""OPERAND-PROVENANCE SCREEN: do the two streams read the same VALUES?

IMPORTABLE CORE: screen_pair, compare_values, value_token, Stream,
reaching_definitions — pure over parsed objects, no build, no side effects.

Every shipped view of PointLineDist2D says REGISTER_ONLY.  `webfrank_audit`
prints `classification: REGISTER_ONLY`, `wf_census.classify` labels every
differing word `regfield`, `fndiff --ops` reports an identical opcode
multiset, and `wf_word_diff` reports CLASS RECOLOR.  All four are correct
about the ENCODINGS and all four are blind to the fact measured by
attempt.WC_pointlinedist2d-its-pin-covers-a-reassociated-product-not-a-recolor
.20260903.v1: at +0x30 our `fmul f1,f0,f5` forms x*r while the target's
`fmul f0,f1,f1` forms r*r.  Same opcode, same position, only register fields
differ -- and the two words multiply DIFFERENT operand pairs.  That rule ships
on an `unproven_recolor_audit` human escape whose note calls the residual an
"operand/load-order exchange ... dataflow-equivalent".

This screen answers the question those views do not ask: for each differing
register-field slot, what VALUE does each stream read there?

THE DISCRIMINANT.  A register-field-only pair has the same length and the same
opcode at every position, so positions correspond exactly.  For each differing
USE slot the screen resolves both sides to a VALUE TOKEN and compares them:

  * reaching definitions come from a CFG fixpoint over `webfrank._successors`
    (a linear backwards scan reports `entry` for any value defined round a
    loop back edge -- measured: it flagged CritterLineCollide's r4/r5 webs,
    which are defined inside the loop the function jumps into);
  * `_savegpr_N`/`_restgpr_N`/`_savefpr_N`/`_restfpr_N` millicode preserves
    registers, so those calls are not definitions (measured: without this,
    every value read after a prologue `bl _savefpr_26` resolves to the call);
  * copies (`mr`/`fmr`/`addi rD,rS,0`) are chased and `li` resolves to its
    literal, which is what keeps the machine-proved value-equality rules out
    of the flagged set;
  * two values defined at DIFFERENT indexes are still equal when their
    defining instructions have the same form and their own inputs compare
    equal, recursively to `--depth` (default 3);
  * a commutative operand pair (`webfrank._commutative_shifts`) is compared
    crosswise as well as straight, so an exchanged multiply is COMMUTED-EQUAL
    and not a value difference.

Verdicts, per function: NOT-REGISTER-ONLY (a differing word changes
non-register bits, so this screen does not apply), OPERAND-VALUE-DIFF (at
least one differing use reads a different value -- REGISTER_ONLY is a
misdescription), EXCHANGE (every value difference is mirrored by an
identically-formed word elsewhere, i.e. a reordered pair rather than a
recolor), BASE-FORM-DIFF, UNDECIDED, PROVENANCE-CONSISTENT.

CALIBRATION IS TWO-SIDED and ships with the tool (`--calibrate`):

  KNOWN POSITIVE, 1: game/world/btricol::PointLineDist2D must flag at +0x30.
  KNOWN NEGATIVES: pinned functions carrying a MACHINE-PROVED recolor whose
    rule has NO pre-recolor stage, so our raw body is the image the rule
    actually recolors.  Rules declaring `instruction_permutation` or an
    `equivalent_*_form` stage are EXCLUDED by name: the build proves those
    over an intermediate image this screen does not stage, exactly the
    staging-artifact error claim.CE_constant-equality-closure-demand-census-
    image-wide.20260903.v1 measured at five-fold.  Rules resting on
    `unproven_recolor_audit` are excluded too -- an inspection escape is not
    ground truth.

The screen is ADVISORY.  It refuses nothing and writes no config: its output
is the fact a human audit escape needs to see before it is granted.

Usage, from the repository root:

    python tools/gdl/composed_census/t15_operand_provenance.py --calibrate
    python tools/gdl/composed_census/t15_operand_provenance.py --only <fn>
    python tools/gdl/composed_census/t15_operand_provenance.py [--pinned]
        [--unit <unit>] [--depth 3] [--out <path.json>]
"""
import argparse
import glob
import json
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                ".."))
import webfrank as wf  # noqa: E402
import fndiff  # noqa: E402

SRC = "build/GUNE5D/src"
OBJ = "build/GUNE5D/obj"
WEBFRANK_JSON = "config/GUNE5D/webfrank.json"
DEFAULT_DEPTH = 3
ENTRY = frozenset({"entry"})
BANK_NAME = {"g": "r", "f": "f"}
# Stages that rewrite the body BEFORE the register stage sees it, so our raw
# object is not the image the shipped rule recolors.
PRE_RECOLOR_STAGES = ("instruction_permutation", "equivalent_copy_form",
                      "equivalent_mask_form", "equivalent_zero_form")
# A constant-pool datum: our anonymous `@N` object against dtk's `lbl_ADDR`
# naming of a whole contiguous run.  The two spellings NEVER match by name
# even when they name the same constant (AGENTS.md, residual discipline 3), so
# comparing them by name would flag every pool read in the image.  WHICH datum
# a relocation names is decided by the datum screens that already ship --
# `fndiff.datum_multiset_screen` (prefix comparison) and
# `webfrank.verify_datum_binding` -- and is deliberately out of scope here:
# this screen decides operand PROVENANCE.  Every equality that rested on the
# wildcard is counted per function as `pool_datum_assumptions`.
# The SECTION-symbol spelling is the same fact one level coarser: our object
# relocates against `..bss.0`/`.rodata.0` plus an addend where dtk names the
# individual datum `lbl_ADDR` (measured on game/game/controls::do_vibe, whose
# every differing word reads `..bss.0` against `lbl_802407B8`).
_POOL_SYMBOL_RE = re.compile(r"^(@\d+|lbl_[0-9A-Fa-f]{6,8}|\.+[A-Za-z])")


def _normalize_symbol(name):
    if not name:
        return None
    name = name.strip()
    return "pool-datum" if _POOL_SYMBOL_RE.match(name) else name


# ---------------------------------------------------------------------------
# object plumbing (same shape as wf_proof_mode_ledger.py)
# ---------------------------------------------------------------------------

def _load(path):
    data = bytearray(open(path, "rb").read())
    return data, wf._sections(data)


def _functions(data, sections):
    out = {}
    for sym in wf._symbols(data, sections):
        if sym.size and 0 <= sym.section_index < len(sections):
            if sections[sym.section_index].name == ".text":
                out[sym.name] = sym
    return out


def _raw_object(unit_obj):
    """The pre-webfrank body, so a pinned function is not flattered."""
    head, tail = os.path.split(unit_obj)
    body = os.path.join(head, ".postprocess", "body", tail)
    return body if os.path.exists(body) else unit_obj


def rules_by_function():
    """{(unit, function): [stage names]} from config/GUNE5D/webfrank.json."""
    out = {}
    config = json.load(open(WEBFRANK_JSON))
    for unit, rules in config["units"].items():
        for rule in rules:
            stages = [key for key in rule
                      if key not in ("function", "mechanism", "before_sha256",
                                     "after_sha256", "audit")]
            out[(unit.replace("\\", "/"), rule["function"])] = stages
    return out


# ---------------------------------------------------------------------------
# reaching definitions and value tokens
# ---------------------------------------------------------------------------

def reaching_definitions(words, successors, calls, call_names):
    """Per-index {(bank, register): frozenset of defining indexes}.

    A missing key means the incoming (``entry``) value.  Merge at a join is
    the union, so a value defined on one path and live from entry on another
    resolves to a two-element set and is compared as such.
    """
    states = [None] * len(words)
    states[0] = {}
    pending = [0]
    while pending:
        index = pending.pop()
        state = dict(states[index])
        word = words[index]
        if calls[index]:
            helper = wf._helper_call(call_names.get(index))
            if helper is None:
                for key in wf._CALL_VOLATILE:
                    state[key] = frozenset({index})
            elif helper[0] == "rest":
                _, bank, first = helper
                for number in range(first, 32):
                    state[(bank, number)] = ENTRY
        else:
            _reads, writes = wf._word_effects(word)
            for resource in writes:
                if isinstance(resource, tuple) and resource[0] in ("g", "f"):
                    state[resource] = frozenset({index})
        for successor in successors[index]:
            known = states[successor]
            if known is None:
                merged = state
            else:
                merged = dict(known)
                changed = False
                for key in set(known) | set(state):
                    union = known.get(key, ENTRY) | state.get(key, ENTRY)
                    if merged.get(key, ENTRY) != union:
                        merged[key] = union
                        changed = True
                if not changed:
                    continue
            states[successor] = merged
            pending.append(successor)
    return states


class Stream:
    """One side of the pair: words, CFG, relocations, reaching definitions."""

    def __init__(self, blob, relocations, jumptable_offsets=()):
        self.words = [wf._u32(blob, off) for off in range(0, len(blob), 4)]
        self.relocated = {off // 4 for off in relocations}
        self.reloc_names = {off // 4: name
                            for off, (_kind, name) in relocations.items()}
        jumptable = {off // 4 for off in jumptable_offsets}
        self.successors, self.calls = wf._successors(
            self.words, self.relocated, jumptable)
        self.call_names = {index: self.reloc_names.get(index)
                           for index, flag in enumerate(self.calls) if flag}
        self.defs = reaching_definitions(
            self.words, self.successors, self.calls, self.call_names)
        self._memo = {}

    def canonical(self, index):
        """The word with every register field cleared, plus any relocation
        symbol: everything about the instruction EXCEPT which registers it
        names."""
        word = self.words[index]
        try:
            mask = wf.register_slot_mask(word)
        except ValueError:
            mask = 0
        return (word & ~mask, _normalize_symbol(self.reloc_names.get(index)))


def value_token(stream, index, bank, register, depth):
    """A canonical name for the VALUE register ``bank``/``register`` holds at
    ``index`` in one stream."""
    key = (index, bank, register, depth)
    memo = stream._memo.get(key)
    if memo is not None:
        return memo
    if index >= len(stream.defs) or stream.defs[index] is None:
        token = ("unreachable", index)
        stream._memo[key] = token
        return token
    sites = stream.defs[index].get((bank, register), ENTRY)
    if len(sites) > 1:
        parts = sorted(
            (_token_at_def(stream, site, bank, register, depth - 1)
             for site in sites), key=repr)
        token = ("phi", tuple(parts))
    else:
        token = _token_at_def(stream, next(iter(sites)), bank, register, depth)
    stream._memo[key] = token
    return token


def _token_at_def(stream, site, bank, register, depth):
    if site == "entry":
        return ("entry", bank, register)
    if stream.calls[site]:
        return ("call", site, bank, register)
    if depth <= 0:
        return ("insn", site)
    word = stream.words[site]
    if site not in stream.relocated:
        form = wf.decode_copy_form(word)
        if form is not None and bank == "g" and form[1] == register:
            if form[0] == "li":
                return ("const", form[2])
            if form[0] == "copy":
                return value_token(stream, site, "g", form[2], depth)
        move = wf._fpr_move(word)
        if move is not None and bank == "f" and move[0] == register:
            return value_token(stream, site, "f", move[1], depth)
    try:
        operands = wf.instruction_operands(word)
    except ValueError:
        return ("insn", site)
    uses = {}
    for use_bank, shift, role, zero_none in operands:
        number = (word >> shift) & 0x1F
        if role not in ("u", "b"):
            continue
        if zero_none and number == 0:
            uses[shift] = ("literal-zero",)
        else:
            uses[shift] = value_token(stream, site, use_bank, number,
                                      depth - 1)
    # A commutative pair is CANONICALISED inside the token, not just compared
    # crosswise at the top level: an exchanged multiply feeding another
    # exchanged multiply (InitAnim +0xb0 into +0xb4) is only equal when the
    # inner exchange is absorbed too.  A literal-zero operand never sorts --
    # RA=0 is a form, not a value, exactly as `_recolor_transfer` guards it.
    pair = wf._commutative_shifts(word)
    if pair is not None and all(shift in uses for shift in pair) \
            and ("literal-zero",) not in (uses[pair[0]], uses[pair[1]]):
        first, second = sorted((uses[pair[0]], uses[pair[1]]), key=repr)
        uses[pair[0]], uses[pair[1]] = first, second
    return ("expr", stream.canonical(site),
            tuple(uses[shift] for shift in sorted(uses)))


# ---------------------------------------------------------------------------
# the screen
# ---------------------------------------------------------------------------

EQUAL, DIFFERENT, UNDECIDED = "EQUAL", "DIFFERENT", "UNDECIDED"


def compare_values(ours, target):
    """Three-valued comparison of two value tokens.

    UNDECIDED is returned wherever the answer rests on a leaf this screen did
    not expand -- an ``insn`` leaf at the ``--depth`` limit, an unreachable
    word.  Calibrated: without it, CritterResolveMultipleTargets read as a
    value difference purely because two otherwise identical chains bottomed
    out at ``insn 13`` against ``insn 15``, and 5 of the 6 remaining
    false positives were the same shape.  A screen that reports an
    unexpanded leaf as a difference is asserting something it did not
    measure.
    """
    if ours == target:
        return EQUAL
    if ours[0] in ("insn", "unreachable", "chase-limit") or \
            target[0] in ("insn", "unreachable", "chase-limit"):
        return UNDECIDED
    if ours[0] != target[0]:
        return DIFFERENT
    if ours[0] == "expr":
        if ours[1] != target[1] or len(ours[2]) != len(target[2]):
            return DIFFERENT
        return _combine(compare_values(a, b)
                        for a, b in zip(ours[2], target[2]))
    if ours[0] == "phi":
        return _compare_sets(ours[1], target[1])
    return DIFFERENT


def _combine(results):
    verdict = EQUAL
    for result in results:
        if result == DIFFERENT:
            return DIFFERENT
        if result == UNDECIDED:
            verdict = UNDECIDED
    return verdict


def _compare_sets(ours, target):
    """Multiset comparison: a phi's incoming values are unordered."""
    if len(ours) != len(target):
        return DIFFERENT
    remaining = list(target)
    leftover = []
    for token in ours:
        for index, other in enumerate(remaining):
            if compare_values(token, other) == EQUAL:
                remaining.pop(index)
                break
        else:
            leftover.append(token)
    if not leftover:
        return EQUAL
    if len(leftover) != len(remaining):
        return DIFFERENT
    return _combine(compare_values(a, b)
                    for a, b in zip(sorted(leftover, key=repr),
                                    sorted(remaining, key=repr)))


def screen_pair(ours, target, depth=DEFAULT_DEPTH):
    """``(rows, verdict)`` for one aligned function pair."""
    if len(ours.words) != len(target.words):
        return [], "NOT-COMPARABLE"
    rows = []
    structural = False
    for index, (our_word, target_word) in enumerate(
            zip(ours.words, target.words)):
        if our_word == target_word:
            continue
        at = "0x%x" % (index * 4)
        try:
            mask = wf.register_slot_mask(our_word)
            operands = wf.instruction_operands(our_word)
        except ValueError:
            structural = True
            rows.append({"at": at, "verdict": "UNDECODABLE"})
            continue
        if (our_word ^ target_word) & ~mask:
            structural = True
            rows.append({"at": at, "verdict": "NON-REGISTER-BITS"})
            continue
        pair = wf._commutative_shifts(our_word)
        by_shift = {shift: (bank, role, zero_none) for bank, shift, role,
                    zero_none in operands}
        values = {}
        for shift, (bank, role, zero_none) in by_shift.items():
            if role not in ("u", "b"):
                continue
            our_reg = (our_word >> shift) & 0x1F
            target_reg = (target_word >> shift) & 0x1F
            if zero_none and 0 in (our_reg, target_reg):
                continue
            values[shift] = (
                value_token(ours, index, bank, our_reg, depth),
                value_token(target, index, bank, target_reg, depth))
        crosswise = False
        if pair is not None and all(shift in values for shift in pair):
            first, second = pair
            straight = _combine((compare_values(*values[first]),
                                 compare_values(*values[second])))
            if straight != EQUAL:
                crosswise = _combine((
                    compare_values(values[first][0], values[second][1]),
                    compare_values(values[second][0], values[first][1]))
                ) == EQUAL
        for bank, shift, role, zero_none in operands:
            our_reg = (our_word >> shift) & 0x1F
            target_reg = (target_word >> shift) & 0x1F
            if our_reg == target_reg:
                continue
            row = {"at": at, "shift": shift, "bank": bank, "role": role,
                   "ours": "%s%d" % (BANK_NAME[bank], our_reg),
                   "target": "%s%d" % (BANK_NAME[bank], target_reg)}
            if zero_none and (our_reg == 0) != (target_reg == 0):
                row["verdict"] = "BASE-FORM-DIFF"
            elif role == "d":
                row["verdict"] = "DEST-RENAME"
            elif shift not in values:
                row["verdict"] = "UNDECIDED"
            else:
                our_value, target_value = values[shift]
                row["ours_value"] = _render(our_value)
                row["target_value"] = _render(target_value)
                row["_ours_value"] = our_value
                row["_target_value"] = target_value
                row["_canonical"] = ours.canonical(index)
                row["_canonical_target"] = target.canonical(index)
                result = compare_values(our_value, target_value)
                if result == EQUAL:
                    row["verdict"] = "VALUE-EQUAL"
                elif crosswise and pair is not None and shift in pair:
                    row["verdict"] = "COMMUTED-EQUAL"
                elif result == UNDECIDED:
                    row["verdict"] = "UNDECIDED-DEPTH"
                else:
                    row["verdict"] = "VALUE-DIFF"
            rows.append(row)
    _mark_exchanges(rows)
    verdicts = {row["verdict"] for row in rows}
    if structural:
        return _clean(rows), "NOT-REGISTER-ONLY"
    if "VALUE-DIFF" in verdicts:
        return _clean(rows), "OPERAND-VALUE-DIFF"
    if "EXCHANGE" in verdicts:
        return _clean(rows), "EXCHANGE"
    if "UNDECIDED" in verdicts or "UNDECIDED-DEPTH" in verdicts:
        return _clean(rows), "UNDECIDED"
    if "BASE-FORM-DIFF" in verdicts:
        return _clean(rows), "BASE-FORM-DIFF"
    return _clean(rows), "PROVENANCE-CONSISTENT"


def _mark_exchanges(rows):
    """A value difference mirrored by an identically-formed word elsewhere is
    a REORDERED PAIR, not a recolor: our word at A reads what the target reads
    at B and vice versa.  Reported as EXCHANGE so it is never counted as an
    operand-value difference."""
    diffs = [row for row in rows if row["verdict"] == "VALUE-DIFF"]
    for i, row in enumerate(diffs):
        if row["verdict"] != "VALUE-DIFF":
            continue
        for other in diffs[i + 1:]:
            if other["verdict"] != "VALUE-DIFF":
                continue
            # The two words must swap FORMS as well as values: our word at A
            # is the target's word at B and vice versa.  Comparing ours-to-
            # ours would miss game/world/tower::SumnerDoSpeech, where the
            # exchanged pair carries two DIFFERENT relocations (gPlayers and
            # a pool datum) that travel with their instructions.
            if other["_canonical"] != row["_canonical_target"] or \
                    row["_canonical"] != other["_canonical_target"]:
                continue
            if compare_values(other["_ours_value"],
                              row["_target_value"]) == EQUAL and \
                    compare_values(other["_target_value"],
                                   row["_ours_value"]) == EQUAL:
                row["verdict"] = other["verdict"] = "EXCHANGE"
                row["exchange_with"] = other["at"]
                other["exchange_with"] = row["at"]
                break


def _clean(rows):
    for row in rows:
        for key in ("_ours_value", "_target_value", "_canonical",
                    "_canonical_target"):
            row.pop(key, None)
    return rows


def _render(token):
    if token[0] == "expr":
        word, symbol = token[1]
        return "expr(0x%08x%s)" % (word, ("," + symbol) if symbol else "")
    if token[0] == "phi":
        return "phi[%s]" % ",".join(_render(part) for part in token[1])
    return "%s" % (token,)


def screen_function(our_data, our_sections, our_sym,
                    target_data, target_sections, target_sym, depth):
    our_text = our_sections[our_sym.section_index]
    target_text = target_sections[target_sym.section_index]
    our_blob = bytes(our_data[our_text.offset + our_sym.value:][:our_sym.size])
    target_blob = bytes(
        target_data[target_text.offset + target_sym.value:][:target_sym.size])
    if our_blob == target_blob or len(our_blob) != len(target_blob):
        return None
    ours = Stream(our_blob, wf._function_text_relocations(
        our_data, our_sections, our_sym.section_index,
        our_sym.value, our_sym.value + our_sym.size),
        wf._jumptable_targets(
            our_data, our_sections, our_sym.section_index,
            our_sym.value, our_sym.value + our_sym.size))
    target = Stream(target_blob, wf._function_text_relocations(
        target_data, target_sections, target_sym.section_index,
        target_sym.value, target_sym.value + target_sym.size),
        wf._jumptable_targets(
            target_data, target_sections, target_sym.section_index,
            target_sym.value, target_sym.value + target_sym.size))
    rows, verdict = screen_pair(ours, target, depth)
    differing = sum(1 for a, b in zip(ours.words, target.words) if a != b)
    wildcards = sum(
        1 for index in set(ours.reloc_names) | set(target.reloc_names)
        if ours.reloc_names.get(index) != target.reloc_names.get(index)
        and _normalize_symbol(ours.reloc_names.get(index)) == "pool-datum"
        and _normalize_symbol(target.reloc_names.get(index)) == "pool-datum")
    return {"insns": len(ours.words), "differing_words": differing,
            "verdict": verdict, "rows": rows,
            "pool_datum_assumptions": wildcards}


def census(unit_filter=None, function_filter=None, depth=DEFAULT_DEPTH):
    out = []
    for target_path in sorted(glob.glob(os.path.join(OBJ, "**", "*.o"),
                                        recursive=True)):
        unit = os.path.relpath(target_path, OBJ).replace("\\", "/")[:-2]
        if unit_filter and fndiff.unit_key(unit_filter) != unit:
            continue
        our_path = _raw_object(os.path.join(SRC, unit + ".o"))
        if not os.path.exists(our_path):
            continue
        try:
            our_data, our_sections = _load(our_path)
            target_data, target_sections = _load(target_path)
            ours = _functions(our_data, our_sections)
            targets = _functions(target_data, target_sections)
        except Exception:  # noqa: BLE001
            continue
        for name, our_sym in sorted(ours.items()):
            if function_filter and name != function_filter:
                continue
            target_sym = targets.get(name)
            if target_sym is None or target_sym.size != our_sym.size:
                continue
            try:
                result = screen_function(our_data, our_sections, our_sym,
                                         target_data, target_sections,
                                         target_sym, depth)
            except Exception as exc:  # noqa: BLE001
                out.append({"unit": unit, "function": name, "insns": 0,
                            "differing_words": 0, "verdict": "ERROR",
                            "error": "%s: %s" % (type(exc).__name__, exc),
                            "rows": []})
                continue
            if result is None:
                continue
            result.update({"unit": unit, "function": name})
            out.append(result)
    return out


UNINTERESTING = ("VALUE-EQUAL", "DEST-RENAME", "COMMUTED-EQUAL")


def _print_rows(entry, limit=None):
    shown = 0
    interesting = [row for row in entry["rows"]
                   if row["verdict"] not in UNINTERESTING]
    for row in interesting:
        if limit is not None and shown >= limit:
            print("       ... %d more" % (len(interesting) - shown))
            break
        shown += 1
        print("       %-7s %-4s %-4s -> %-4s  %-17s ours=%s target=%s"
              % (row["at"], row.get("role", "?"), row.get("ours", "?"),
                 row.get("target", "?"), row["verdict"],
                 row.get("ours_value"), row.get("target_value")))


def calibrate(by_key, pins):
    positives = [("game/world/btricol", "PointLineDist2D")]
    value_negatives, strict_negatives = [], []
    excluded_staged, excluded_unproven = 0, 0
    for key, stages in sorted(pins.items()):
        if key not in by_key:
            continue
        if "unproven_recolor_audit" in stages:
            excluded_unproven += 1
            continue
        if any(stage in stages for stage in PRE_RECOLOR_STAGES):
            excluded_staged += 1
            continue
        if "value_equality_recolor" in stages:
            value_negatives.append(key)
        elif "copy_register_fields" in stages or "register_fields" in stages:
            strict_negatives.append(key)
    report = {"positives": [], "value_equality_negatives": [],
              "strict_negatives": [],
              "excluded": {"pre_recolor_staged": excluded_staged,
                           "unproven_recolor_audit": excluded_unproven}}
    caught = 0
    for key in positives:
        entry = by_key.get(key)
        verdict = entry["verdict"] if entry else "NOT-A-CANDIDATE"
        hit = verdict == "OPERAND-VALUE-DIFF"
        caught += hit
        report["positives"].append({"unit": key[0], "function": key[1],
                                    "verdict": verdict, "caught": hit})
    false_positives = []
    for bucket, keys in (("value_equality_negatives", value_negatives),
                         ("strict_negatives", strict_negatives)):
        for key in keys:
            entry = by_key[key]
            flagged = entry["verdict"] == "OPERAND-VALUE-DIFF"
            report[bucket].append({"unit": key[0], "function": key[1],
                                   "verdict": entry["verdict"],
                                   "flagged": flagged})
            if flagged:
                false_positives.append((key, entry))
    negatives = len(value_negatives) + len(strict_negatives)
    print("CALIBRATION, two-sided")
    print("  POSITIVES (must flag): %d/%d caught" % (caught, len(positives)))
    for row in report["positives"]:
        print("     %-28s %-30s %s"
              % (row["unit"], row["function"], row["verdict"]))
        entry = by_key.get((row["unit"], row["function"]))
        if entry:
            _print_rows(entry, limit=2)
    print("  NEGATIVES (must not flag): %d machine-proved pinned recolors"
          % negatives)
    print("     %d value_equality_recolor + %d strict verify_consistent_"
          "recolor" % (len(value_negatives), len(strict_negatives)))
    print("     excluded by name: %d with a pre-recolor stage (%s), %d resting"
          " on unproven_recolor_audit"
          % (excluded_staged, "/".join(PRE_RECOLOR_STAGES), excluded_unproven))
    print("     FALSE POSITIVES: %d" % len(false_positives))
    for row in report["value_equality_negatives"]:
        print("     ve     %-26s %-30s %s"
              % (row["unit"], row["function"], row["verdict"]))
    for key, entry in false_positives:
        print("     FALSE POSITIVE %s::%s" % key)
        _print_rows(entry, limit=6)
    ok = caught == len(positives) and not false_positives
    report["ok"] = ok
    report["counts"] = {"positives": len(positives), "caught": caught,
                        "negatives": negatives,
                        "false_positives": len(false_positives)}
    print("  CALIBRATION %s" % ("OK" if ok else "FAILED"))
    return ok, report


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--unit")
    parser.add_argument("--only", help="function name")
    parser.add_argument("--pinned", action="store_true",
                        help="print pinned functions only")
    parser.add_argument("--depth", type=int, default=DEFAULT_DEPTH)
    parser.add_argument("--calibrate", action="store_true")
    parser.add_argument("--out", help="write the full table as JSON")
    args = parser.parse_args(argv)

    pins = rules_by_function()
    entries = census(args.unit, args.only, args.depth)
    by_key = {(entry["unit"], entry["function"]): entry for entry in entries}

    counts = {}
    for entry in entries:
        counts[entry["verdict"]] = counts.get(entry["verdict"], 0) + 1
    print("PAIRED FUNCTIONS WITH A DIFFERING BODY: %d" % len(entries))
    for verdict, count in sorted(counts.items()):
        print("  %-22s %d" % (verdict, count))
    print()

    flagged = [entry for entry in entries
               if entry["verdict"] == "OPERAND-VALUE-DIFF"]
    for entry in entries:
        stages = pins.get((entry["unit"], entry["function"])) or []
        entry["staged_base"] = any(stage in stages
                                   for stage in PRE_RECOLOR_STAGES)
    shown = [entry for entry in flagged
             if not args.pinned or (entry["unit"], entry["function"]) in pins]
    findings = [entry for entry in shown if not entry["staged_base"]]
    staged = [entry for entry in shown if entry["staged_base"]]
    print("OPERAND-VALUE-DIFF (%d shown of %d):" % (len(shown), len(flagged)))
    for entry in sorted(findings, key=lambda e: (e["unit"], e["function"])):
        stages = pins.get((entry["unit"], entry["function"]))
        print("  %-28s %-30s insns=%d words=%d %s"
              % (entry["unit"], entry["function"], entry["insns"],
                 entry["differing_words"],
                 ("PINNED %s" % ",".join(stages)) if stages else ""))
        _print_rows(entry, limit=4)
    if staged:
        print("  -- STAGED BASE, not findings: a pre-recolor stage rewrites")
        print("     the body before the register stage, so our raw object is")
        print("     not the image the rule recolors --")
        for entry in sorted(staged, key=lambda e: (e["unit"], e["function"])):
            print("     %-26s %-30s %s"
                  % (entry["unit"], entry["function"],
                     ",".join(pins[(entry["unit"], entry["function"])])))
    print()

    ok = True
    report = None
    if args.calibrate:
        ok, report = calibrate(by_key, pins)
    if args.out:
        os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
        with open(args.out, "w") as handle:
            json.dump({"entries": entries, "calibration": report}, handle,
                      indent=2, sort_keys=True)
        print("wrote %s" % args.out)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
