"""Re-derive permutation specs against the CURRENT objects.

Never trusts banked offsets: everything below is read out of the objects
that ninja just produced.

Usage: python tools/gdl/rule_derive.py [unit] [function ...]
       python tools/gdl/rule_derive.py <unit> <function> --emit [--class K]
Runs from ANY checkout: paths resolve relative to this file's repo, and
the OURS side prefers the raw compiler output (.postprocess/body/) so a
unit that already has webfrank rules is derived from its true residual.

--emit writes a DRAFT `config/GUNE5D/webfrank.json` entry for the function:
the shipped key names, the live before/after body hashes, the differing-word
runs as window candidates, and — the part that cost NM roughly eight calls
of reading webfrank.py — WHICH VERIFIER each rule class actually runs, with
that verifier's own composition refusals quoted from the source. It is a
draft and says so: every proof obligation is left as a `<REQUIRED:...>`
placeholder, because the hashes are mechanical and the mechanism is not.
`--list-classes` prints the map alone, with no objects needed.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from tools.gdl.webfrank import (  # noqa: E402
    _find_symbol,
    _function_text_relocations,
    _sections,
    _u32,
)


# The class -> verifier map, read out of webfrank.py's own dispatch
# (apply_patch) and the entry points it calls. Each row is
# (rule key, verifier entry point, what it proves, composition refusals).
# NM read webfrank internals about eight times to assemble this by hand.
RULE_CLASSES = {
    "instruction_permutation": {
        "verifier": "permute_instruction_atoms + verify_relocation_binding",
        "proves": "the moved instructions are pairwise independent (no RAW,"
                  " WAR or WAW hazard and no store/control op in the window),"
                  " and each window's relocations stay bound to the atoms"
                  " they patch",
        "refuses": [
            "a window containing a control instruction",
            "`memory_disambiguation` without a permutation — it only"
            " refines the dependence audit",
        ],
        "requires_target": True,
    },
    "memory_disambiguation": {
        "verifier": "permute_instruction_atoms (dependence audit refinement)",
        "proves": "two memory references in a permutation window cannot"
                  " alias, so the store may move",
        "refuses": ["standing alone: it is meaningless without an"
                    " `instruction_permutation`"],
        "requires_target": True,
    },
    "equivalent_copy_form": {
        "verifier": "equivalent_copy_form -> prove_constant_source /"
                    " prove_constant_dataflow",
        "proves": "our word and the target's compute the SAME value by a"
                  " different form (li vs a copy of a proved-constant"
                  " register), from a dominating definition in the same"
                  " straight-line span",
        "refuses": ["a span crossing a control instruction, a relocated"
                    " word, or an interposed redefinition"],
        "requires_target": True,
    },
    "equivalent_mask_form": {
        "verifier": "equivalent_mask_form -> prove_zero_bits",
        "proves": "the mask our stream applies is redundant because the"
                  " bits it clears are provably already zero",
        "refuses": ["riding on `unproven_recolor_audit`: the range proof"
                    " reads our PRE-recolor registers, so an unproven"
                    " renaming would launder it into a claim about a"
                    " different value"],
        "requires_target": True,
    },
    "equivalent_zero_form": {
        "verifier": "equivalent_zero_form -> prove_zero_result",
        "proves": "the value our word produces is provably zero, so the"
                  " target's differently-spelled zero is the same value",
        "refuses": [
            "riding on `unproven_recolor_audit` (same laundering hazard as"
            " the mask class)",
            "composing with ANY register stage (`copy_register_fields`,"
            " `recolors`, `register_fields`): it writes the TARGET's word"
            " into our colouring, which no recolor proof models",
        ],
        "requires_target": True,
    },
    "copy_register_fields": {
        "verifier": "copy_register_fields -> verify_consistent_recolor",
        "proves": "every differing register field is one consistent"
                  " renaming — a position-consistent bisimulation between"
                  " the two colourings",
        "refuses": ["a renaming that is not a function (one of our"
                    " registers mapping to two of the target's, or vice"
                    " versa) unless `unproven_recolor_audit` is declared"],
        "requires_target": True,
    },
    "recolors": {
        "verifier": "verify_consistent_recolor",
        "proves": "the same bisimulation as `copy_register_fields`, over"
                  " explicitly declared regions",
        "refuses": ["an inconsistent renaming, absent an"
                    " `unproven_recolor_audit`"],
        "requires_target": True,
    },
    "register_fields": {
        "verifier": "verify_consistent_recolor",
        "proves": "the same bisimulation, per declared field edit",
        "refuses": ["an inconsistent renaming, absent an"
                    " `unproven_recolor_audit`"],
        "requires_target": True,
    },
    "value_equality_recolor": {
        "verifier": "verify_value_equality_recolor",
        "proves": "the register stage's OUTPUT is value-equal to the"
                  " target even where the renaming is not a bisimulation"
                  " — the project's outer class boundary",
        "refuses": [
            "standing without a register stage: it exists to prove that"
            " stage's output",
            "riding on `unproven_recolor_audit`",
        ],
        "requires_target": True,
    },
    "post_recolor_permutation": {
        "verifier": "permute_instruction_atoms, run AFTER the recolor",
        "proves": "the permutation is legal in the TARGET colouring",
        "refuses": [
            "standing without a register stage",
            "riding on `unproven_recolor_audit`: the permutation is audited"
            " in the target colouring, which only a machine-proven recolor"
            " reaches",
        ],
        "requires_target": True,
    },
    "unproven_recolor_audit": {
        "verifier": "(none — it DISABLES the recolor proof)",
        "proves": "nothing. It is the deliberate escape hatch that accepts"
                  " a renaming verify_consistent_recolor refused, and the"
                  " mask, zero, value-equality and post-permutation classes"
                  " all refuse to compose with it",
        "refuses": ["composition with equivalent_mask_form,"
                    " equivalent_zero_form, value_equality_recolor and"
                    " post_recolor_permutation"],
        "requires_target": False,
    },
}


def positional_args():
    """argv positionals, with `--class KIND`'s VALUE excluded.

    `--class equivalent_copy_form` does not start with `--`, so a naive
    "drop the flags" filter reads the class name as a function name and
    then fails to find that symbol.
    """
    out, skip = [], False
    for arg in sys.argv[1:]:
        if skip:
            skip = False
            continue
        if arg == "--class":
            skip = True
            continue
        if arg.startswith("--"):
            continue
        out.append(arg)
    return out


def _default_unit():
    args = positional_args()
    return (args[0] if args else "game/pb/pb_window").strip("/")


UNIT = _default_unit()
_ours = ROOT / "build" / "GUNE5D" / "src" / (UNIT + ".o")
_raw = _ours.parent / ".postprocess" / "body" / _ours.name
OURS = _raw if _raw.is_file() else _ours
TARGET = ROOT / "build" / "GUNE5D" / "obj" / (UNIT + ".o")


def load(path, name):
    data = path.read_bytes()
    sections = _sections(data)
    symbol = _find_symbol(data, sections, name)
    text = sections[symbol.section_index]
    start = text.offset + symbol.value
    body = data[start:start + symbol.size]
    relocations = _function_text_relocations(
        data, sections, symbol.section_index,
        symbol.value, symbol.value + symbol.size,
    )
    return body, relocations


def differing_runs(ours, target):
    """Contiguous runs of differing words, as [[offset, ...], ...]."""
    runs = []
    for offset in range(0, len(ours), 4):
        if _u32(ours, offset) == _u32(target, offset):
            continue
        if runs and offset == runs[-1][-1] + 4:
            runs[-1].append(offset)
        else:
            runs.append([offset])
    return runs


def report(name):
    ours, our_relocations = load(OURS, name)
    target, target_relocations = load(TARGET, name)
    print(f"=== {name}: ours {len(ours)} bytes, target {len(target)} bytes")
    if len(ours) != len(target):
        print("  SIZE MISMATCH")
        return
    runs = differing_runs(ours, target)
    print(f"  differing words: {sum(len(run) for run in runs)}")
    for run in runs:
        print(f"  run +0x{run[0]:x}..+0x{run[-1] + 4:x}  ({len(run)} words)")
        for offset in run:
            our_reloc = (our_relocations.get(offset)
                         or our_relocations.get(offset + 2))
            their_reloc = (target_relocations.get(offset)
                           or target_relocations.get(offset + 2))
            marks = []
            if our_reloc:
                marks.append(f"ours={our_reloc[0]}:{our_reloc[1]}")
            if their_reloc:
                marks.append(f"tgt={their_reloc[0]}:{their_reloc[1]}")
            print(f"    +0x{offset:03x}  {_u32(ours, offset):08x} -> "
                  f"{_u32(target, offset):08x}  {' '.join(marks)}")


def class_note(kind):
    row = RULE_CLASSES[kind]
    note = [f"VERIFIER: {row['verifier']} — proves {row['proves']}."]
    if row["refuses"]:
        note.append("REFUSES: " + "; ".join(row["refuses"]) + ".")
    return " ".join(note)


def print_class_map():
    print("webfrank rule class -> verifier (read from webfrank.py's own"
          " apply_patch dispatch):")
    for kind in RULE_CLASSES:
        print(f"\n  {kind}")
        for line in class_note(kind).split(". "):
            if line.strip():
                print(f"      {line.strip().rstrip('.')}.")


def emit(name, kind):
    """Print a DRAFT webfrank.json entry for one function."""
    import hashlib

    ours, _our_relocations = load(OURS, name)
    target, _target_relocations = load(TARGET, name)
    if len(ours) != len(target):
        raise SystemExit(
            f"{name}: size mismatch ({len(ours)} vs {len(target)}) — a"
            " count-asymmetric residual is outside EVERY postprocessor"
            " class, so there is no rule to draft")
    runs = differing_runs(ours, target)
    entry = {
        "function": name,
        "before_sha256": hashlib.sha256(ours).hexdigest(),
        "after_sha256": hashlib.sha256(target).hexdigest(),
        "mechanism": (
            "<REQUIRED: the source-exhaustion provenance (cite the parked or"
            " capped attempt record with literal probed_form axes, per"
            " AGENTS.md's Mandatory result policy), then the derivation of"
            " every window and every proof obligation below.> "
            + class_note(kind)),
        "_draft_note": (
            "DRAFT from tools/gdl/rule_derive.py --emit. The two body hashes"
            " are read from the objects ninja just produced and are correct"
            " as of this build; everything else is a placeholder. A rule"
            " additionally requires SOURCE-EXHAUSTION provenance and"
            " integrator review — this tool does not authorise one."),
    }
    if kind == "instruction_permutation":
        entry[kind] = [
            {
                "start": f"0x{run[0]:x}",
                "end": f"0x{run[-1] + 4:x}",
                "order": "<REQUIRED: the atom order, 0-based within this"
                         " window>",
                "before_sha256": hashlib.sha256(
                    ours[run[0]:run[-1] + 4]).hexdigest(),
                "after_sha256": hashlib.sha256(
                    target[run[0]:run[-1] + 4]).hexdigest(),
            }
            for run in runs
        ]
    elif kind in ("recolors", "register_fields", "equivalent_copy_form",
                  "equivalent_mask_form", "equivalent_zero_form"):
        entry[kind] = [
            {"start": f"0x{run[0]:x}", "end": f"0x{run[-1] + 4:x}",
             "site": "<REQUIRED: the per-site obligation this class's"
                     " verifier discharges>"}
            for run in runs
        ]
    elif kind == "copy_register_fields":
        entry[kind] = True
    else:
        entry[kind] = "<REQUIRED: see the verifier note in `mechanism`>"
    print(json.dumps({"units": {UNIT: [entry]}}, indent=2))
    print()
    print(f"# {len(runs)} differing run(s), "
          f"{sum(len(run) for run in runs)} word(s).")
    print("# Insert the entry under the existing `units` key with a SURGICAL"
          " text edit — a json.dump round-trip reformats every other lane's"
          " rules (AGENTS.md trap 6) — then re-run `python configure.py`"
          " before ninja, or the WEBFRANK edge is never created.")


def main():
    if "--list-classes" in sys.argv:
        print_class_map()
        return 0
    args = positional_args()
    # argv[1] is the UNIT (consumed above); function names start at argv[2].
    # The old sys.argv[1:] loop consumed the unit as a function name and
    # crashed — two lanes hit it.
    funcs = args[1:]
    if not funcs:
        if UNIT == "game/pb/pb_window":
            funcs = ["pbWinSetup", "pbProjCalc"]
        else:
            raise SystemExit(
                "usage: rule_derive.py <unit> <function> [function ...]"
                "\n       rule_derive.py <unit> <fn> --emit [--class KIND]"
                "\n       rule_derive.py --list-classes")
    if "--emit" in sys.argv:
        kind = "instruction_permutation"
        if "--class" in sys.argv:
            index = sys.argv.index("--class") + 1
            if index >= len(sys.argv):
                raise SystemExit("--class needs a rule class name; run"
                                 " --list-classes for the map")
            kind = sys.argv[index]
        if kind not in RULE_CLASSES:
            raise SystemExit(
                f"unknown rule class {kind!r}. Known classes: "
                + ", ".join(RULE_CLASSES))
        for function in funcs:
            emit(function, kind)
        return 0
    for function in funcs:
        report(function)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
