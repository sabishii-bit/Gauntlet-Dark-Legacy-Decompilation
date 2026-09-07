#!/usr/bin/env python3
"""The reusable whole-TU native-retirement certificate.

    python tools/gdl/retire_audit.py <unit> <function> --before-ref <rev>

Every WebFrank retirement so far re-wrote this certificate as a private script
(r70/r76/r77/r78/r84/r91 under tools/gdl/composed_census/). Six copies of one
argument is six chances to leave a check out, and the checks are not obvious:
r91 is the only one that compared the WHOLE prior processed ELF, r84 is the
only one that required a changed sibling to move monotonically TOWARD target,
and r71 is the only one that bound an anonymous BSS base to its named array.
This file is those arguments in one place, parameterized by (unit, function).

WHAT IS CERTIFIED

  fresh_edge_fidelity   The current source, recompiled through the REAL Ninja
                        edge (`cv_probe.read_edges`, the generated build graph
                        -- not a matchtool preset), reproduces the raw
                        pre-WebFrank object on disk BYTE for BYTE.
  scratch_edge_control  The same source compiled from the scratch directory,
                        with the one appended `-i <source dir>` the before-ref
                        replay needs, reproduces that object too. Without this
                        the before image would be measured through a compiler
                        command nothing has validated.
  before_image          `git show <rev>:<source>` compiled through that same
                        controlled command: the previous state, reconstructed,
                        not asserted.
  native_body           The function's raw body equals the target object's,
                        instruction for instruction. When it does not, every
                        differing instruction is CLASSIFIED (below) -- that is
                        the diagnosis a lane needs, and a failing audit prints
                        it rather than only a verdict.
  positional_relocs     Same count, and equal (offset, type, symbol, addend)
                        at every position, after exactly one normalization:
                        MWCC records R_PPC_EMB_SDA21 at instruction+2 where
                        dtk records it at instruction+0 (measured image-wide
                        by composed_census/ch_reloc_probe.py; @lo/@ha sit at
                        +2 on BOTH sides and are NOT normalized). An
                        unmodelled relocation type is a refusal, not a pass.
  sibling_bodies        Every OTHER function in the TU is byte-equal to its
                        previous state, bodies and positional relocations
                        both. A sibling named by --allow-changed-sibling may
                        differ only if every changed bit moves TOWARD the
                        target (r84's monotonic rule); an unlisted change is
                        a failure however good it looks.
  nontext_sections      Every allocated non-.text section (data, rodata,
                        sdata, sdata2, bss, sbss ...), its bytes, size,
                        alignment, flags and relocations, plus SHN_COMMON
                        symbols (size and alignment), plus the object's
                        exception records, equal before and after.
  rule_delta            The unit's configured rules changed by EXACTLY the
                        removal of the audited function's rule(s). A REMAINING
                        rule whose only change is its relocation hashes (the
                        anonymous-pool re-derivation class) must be named with
                        --allow-rederived-rule; a moved BODY hash is never
                        permitted. Other units' deltas are reported as NOT
                        CERTIFIED rather than silently tolerated.
  rule_replay           Both rule sets are really replayed through the real
                        postprocessor: before-rules on the before object,
                        current rules on the fresh object. Both must succeed,
                        the two processed objects must be byte-equal (a
                        retirement that changes the linked object is not a
                        retirement) or differ ONLY in the bodies of the
                        audited function and the permitted siblings, each
                        still monotonically toward target, and the result
                        must equal the object Ninja shipped.
  link_hash             `dtk shasum -c config/GUNE5D/build.sha1` over the
                        built DOL. The configured build hash, not the retail
                        input's.

INSTRUCTION CLASSES, for the differing words of a FAILING native_body:

  register-assignment  same instruction at this index; every differing bit
                       lies inside a five-bit register slot present in BOTH
                       operand forms.
  memory-access        same instruction, it addresses memory, and what
                       differs is the ACCESS: the displacement field, or one
                       side putting the literal zero in rA where the other
                       names a base register.
  immediate            same instruction, not a memory op, and the differing
                       bits lie outside every register slot -- a literal, a
                       shift/mask field, or a li-versus-copy rA zero.
  scheduling           the two words are not the same instruction at this
                       index (the streams are no longer aligned), or the
                       word is a control op whose displacement moved. Count
                       asymmetry classifies the WHOLE residual this way.
  relocation           every differing bit falls inside a field a relocation
                       patches; those bits belong to the LINKER and are not
                       evidence of a codegen difference either way.

The five classes are computed on top of `wf_word_diff.decode_word_class`,
the promoted classifier, rather than a seventh private decoder.

EXIT CODES.  0 PASS. 1 FAIL -- the measurement happened and a named check
did not hold; the verdict line names it. 2 REFUSED -- an input was missing
or a measurement could not be made, which is NOT a failing check and must
never be read as one. --before-ref is REQUIRED: a sibling inventory compared
against itself is not evidence, and two empty inventories are not a
certificate. It is also REFUSED when it holds a byte-identical audited source
to the working tree -- the same principle, enforced instead of assumed. Spell
the parent `<rev>~1`: the caret in `<rev>^` is consumed before it reaches argv
in this environment at every quoting level, so the audit would run against the
retirement commit itself (run-58 item 5). Refs resolve through
`git cat-file`, not `rev-parse --verify <rev>^{commit}`, which rejects every
revision here.

LIMITS. This is not a TU flip: a NonMatching unit still links its extracted
object, and `linkage` is reported so that stays visible. It certifies the
retirement of ONE function's rule against the previous state, not the
correctness of the recovered source, not compiler-internal causality, and
not the absence of other units' drift.

IMPORTABLE CORE: classify_word, classify_stream, object_image,
normalize_relocations, section_bases, relocation_addresses,
resolve_relocation_symbol, resolve_relocations -- pure over words/parsed
objects and the read-only config; no build and no printing.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    sys.path.insert(0, str(Path(__file__).resolve().parent))
from tools.fix_exception_objects import Elf
from tools.gdl import fndiff
from tools.gdl import pool_owner
from tools.gdl import poolval
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census import wf_word_diff as wd
from tools.gdl.exception_metadata import exception_records

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parents[2]
SHN_COMMON = 0xFFF2

#: ELF r_type -> the name `wf_word_diff._RELOC_FIELD_MASKS` is keyed by.
#: Deliberately partial: an unmodelled type owns unknown bits, and this tool
#: refuses rather than crediting the linker with a difference it may not own.
RELOC_TYPE_NAMES = {
    1: "R_PPC_ADDR32", 3: "R_PPC_ADDR16", 4: "R_PPC_ADDR16_LO",
    5: "R_PPC_ADDR16_HI", 6: "R_PPC_ADDR16_HA", 10: "R_PPC_REL24",
    11: "R_PPC_REL14", 12: "R_PPC_REL14_BRTAKEN", 13: "R_PPC_REL14_BRNTAKEN",
    26: "R_PPC_REL32", 109: "R_PPC_EMB_SDA21",
}
#: The one convention difference between MWCC's own relocation table and the
#: dtk-extracted target's. Measured by composed_census/ch_reloc_probe.py.
SDA21 = 109

MEMORY_OPCODES = frozenset(range(32, 56))  # integer + FP D-form loads/stores
MEMORY_XO31 = frozenset({
    23, 87, 119, 151, 183, 215, 247, 279, 311, 343, 375, 407, 439,
    535, 567, 599, 631, 663, 695, 727, 759, 790, 918, 662, 20, 150, 54, 86,
})


class Refused(Exception):
    """A measurement could not be made. Never a failing check."""


# ---------------------------------------------------------------- classifier
def _is_memory_word(word):
    """Does this instruction address memory?"""
    opcode = word >> 26
    if opcode in MEMORY_OPCODES or opcode in (46, 47):  # incl. lmw/stmw
        return True
    return opcode == 31 and ((word >> 1) & 0x3FF) in MEMORY_XO31


def classify_word(ours, target, reloc_types=()):
    """(class, decode_class) for ONE differing word pair at one index.

    `reloc_types` are the relocation TYPE NAMES either object carries at this
    instruction. The decode class comes from wf_word_diff; this function only
    decides which of the five reported classes it lands in, and the mapping is
    the part a caller can check both ways.
    """
    decode = wd.decode_word_class(ours, target, reloc_types)
    if decode == "RELOCATED":
        return "relocation", decode
    if decode in ("OPCODE", "BRANCH"):
        return "scheduling", decode
    memory = _is_memory_word(ours) and _is_memory_word(target)
    if decode in ("IMMEDIATE", "RA-ZERO"):
        return ("memory-access" if memory else "immediate"), decode
    return "register-assignment", decode


CLASSES = ("register-assignment", "scheduling", "memory-access",
           "immediate", "relocation")


def normalize_relocations(rows, source):
    """Positional relocations in one convention: (offset, type, symbol, addend).

    `source` is "ours" (MWCC) or "target" (dtk). Only R_PPC_EMB_SDA21 moves.
    An SDA21 entry that is not at instruction+2 on the MWCC side is a refusal:
    silently normalizing an offset the convention does not explain is how a
    real positional difference becomes an equal table.
    """
    out = []
    for offset, kind, name, addend in rows:
        if kind not in RELOC_TYPE_NAMES:
            raise Refused(f"unmodelled relocation type {kind} at +0x{offset:x}")
        if kind == SDA21 and source == "ours":
            if offset % 4 != 2:
                raise Refused(f"R_PPC_EMB_SDA21 at +0x{offset:x} is not at "
                              "instruction+2; the halfword convention does "
                              "not explain it")
            offset -= 2
        out.append((offset, kind, name, addend))
    return sorted(out)


_SPLIT_RUNS = None
_RELOCATION_ADDRESSES = None


def section_bases(unit, runs=None):
    """{section name: linked base address} for one unit's split runs.

    A section claimed by MORE THAN ONE run is omitted: with two runs an
    offset into the section does not determine an address, and picking one
    is how a wrong binding would read as equal.
    """
    global _SPLIT_RUNS
    if runs is None:
        if _SPLIT_RUNS is None:
            _SPLIT_RUNS = pool_owner.load_splits()
        runs = _SPLIT_RUNS
    counts, bases = {}, {}
    for owner, section, start, _end in runs:
        if owner != unit:
            continue
        counts[section] = counts.get(section, 0) + 1
        bases[section] = start
    return {section: base for section, base in bases.items()
            if counts[section] == 1}


def relocation_addresses(symbols=None):
    """{symbol name: linked address} from symbols.txt, dtk suffixes included.

    dtk spells a file-local symbol `gendir_8004FBC8` while our object emits
    it as `gendir`, so the stripped spelling is registered as an alias --
    but ONLY when it is unique and does not collide with a real symbol.
    `fndiff.strip_dtk_suffix` is the one reduction (run-59 item 4) and its
    placeholder guard is what stops 4,282 `lbl_*` names collapsing onto the
    single key `lbl`.
    """
    global _RELOCATION_ADDRESSES
    cache = symbols is None
    if cache and _RELOCATION_ADDRESSES is not None:
        return _RELOCATION_ADDRESSES
    if symbols is None:
        symbols = poolval.load_symbols()
    table = {name: info["addr"] for name, info in symbols.items()}
    counts = {}
    for name in table:
        stripped = fndiff.strip_dtk_suffix(name)
        if stripped != name:
            counts[stripped] = counts.get(stripped, 0) + 1
    aliases = {}
    for name, address in table.items():
        stripped = fndiff.strip_dtk_suffix(name)
        if stripped != name and stripped not in table \
                and counts[stripped] == 1:
            aliases[stripped] = address
    table.update(aliases)
    if cache:
        _RELOCATION_ADDRESSES = table
    return table


def resolve_relocation_symbol(name, bases, addresses):
    """The linked ADDRESS a relocation's symbol denotes, or None.

    RUN-59 ITEM 11. `positional_relocations` compared relocation SYMBOL
    NAMES, and the two sides of a retirement never spell a pool datum the
    same way: `object_image` normalizes our anonymous MWCC pool label to
    its allocation -- `("@", ".sdata2", 572, 4, 1, 0)` -- while dtk names
    the same datum `lbl_80346A4C`. Every enemy and critter retirement fails
    on that, on a NAMING limitation rather than a binding difference: the
    ER lane hand-verified all 14 of gendir_8004FBC8's bindings (same
    offset, type and addend, identical resolved addresses) while the audit
    reported FAIL.

    Both spellings denote an address, so both are resolved to one:
      * an anonymous entry, to its section's claimed base in splits.txt
        plus its offset in that section;
      * a named entry, to its symbols.txt address.
    An unresolvable name is returned as None so the caller keeps the NAME
    and the comparison stays fail-closed -- a datum whose ownership is not
    claimed must not silently compare equal to anything.

    This is a naming resolution, not a value-equality relaxation. The
    section BYTES are compared separately by `allocated_sections`, so a
    pool that holds different data still fails there; what this removes is
    only the difference in how the two toolchains SPELL the same address.
    """
    if isinstance(name, (list, tuple)):
        if len(name) < 3 or name[0] != "@":
            return None
        base = bases.get(name[1])
        return None if base is None else base + name[2]
    return addresses.get(name)


def resolve_relocations(rows, bases, addresses):
    """`rows` with every resolvable symbol replaced by its address."""
    out = []
    for offset, kind, name, addend in rows:
        address = resolve_relocation_symbol(name, bases, addresses)
        out.append((offset, kind,
                    ["address", address] if address is not None else name,
                    addend))
    return out


def _reloc_types_by_index(*reloc_lists):
    """{instruction index: (type name, ...)} over every table given."""
    table = {}
    for rows in reloc_lists:
        for offset, kind, _name, _addend in rows:
            table.setdefault(offset // 4, set()).add(
                RELOC_TYPE_NAMES.get(kind, f"UNMODELLED_{kind}"))
    return {index: tuple(sorted(names)) for index, names in table.items()}


def classify_stream(ours, target, reloc_types_by_index=None):
    """The differing-instruction report for two bodies.

    Count asymmetry is a determinate answer, not a missing measurement: the
    streams cannot be paired by index, so the whole residual is `scheduling`
    and no per-word row is invented for it.
    """
    if len(ours) != len(target) or len(ours) % 4:
        return {"aligned": False, "our_instructions": len(ours) // 4,
                "target_instructions": len(target) // 4,
                "counts": {name: 0 for name in CLASSES},
                "count_asymmetric": len(ours) != len(target),
                "rows": [], "differing_words": None,
                "summary": "count-asymmetric: target %d, ours %d instructions"
                           " -- the streams are not index-aligned, so the"
                           " whole residual is scheduling/inlining"
                           % (len(target) // 4, len(ours) // 4)}
    types = reloc_types_by_index or {}
    rows = []
    counts = {name: 0 for name in CLASSES}
    for offset in range(0, len(ours), 4):
        a = int.from_bytes(ours[offset:offset + 4], "big")
        b = int.from_bytes(target[offset:offset + 4], "big")
        if a == b:
            continue
        klass, decode = classify_word(a, b, types.get(offset // 4, ()))
        counts[klass] += 1
        rows.append({"offset": offset, "ours": f"{a:08x}", "target": f"{b:08x}",
                     "class": klass, "decode": decode,
                     "relocation_types": list(types.get(offset // 4, ()))})
    return {"aligned": True, "our_instructions": len(ours) // 4,
            "target_instructions": len(target) // 4,
            "count_asymmetric": False, "counts": counts, "rows": rows,
            "differing_words": len(rows),
            "summary": ", ".join(f"{name} {counts[name]}" for name in CLASSES)}


# ------------------------------------------------------------- object images
def _function_relocations(elf, section_index, start, size):
    rows = []
    for ri, rh in enumerate(elf.sh):
        if rh[1] == 9 and rh[7] == section_index:
            raise Refused("implicit-addend relocations are unsupported")
        if rh[1] != 4 or rh[7] != section_index:
            continue
        _, entries = elf.relas(elf.names[ri])
        rows.extend((off - start, info & 255, elf.symname(info >> 8).decode(), add)
                    for off, info, add in entries if start <= off < start + size)
    return sorted(rows)


def object_image(path):
    """The complete comparable image of one object.

    Anonymous pool labels (`@42`) are normalized to their own section, value,
    size, info and other fields: the NAME is a numbering artefact that shifts
    when an unrelated pool entry appears, while the allocation it describes is
    the fact. Everything else -- bodies, positional relocations, allocated
    section bytes and headers, SHN_COMMON allocations and exception records --
    is compared verbatim.
    """
    elf = Elf(str(path))
    anonymous = {}
    for i in range(elf.symcount):
        s = elf.sym(i)
        name = elf.symname(i).decode()
        if name.startswith("@"):
            if not 0 < s[5] < len(elf.sh):
                raise Refused(f"{path}: undefined anonymous pool symbol {name}")
            anonymous[name] = ("@", elf.names[s[5]], s[1], s[2], s[3], s[4])

    def pool_name(name):
        """Anonymous pool label -> its allocation; every other name unchanged.

        NOT called `rename`: tools/gdl/tests/test_t20_importable_purity.py
        screens an IMPORTABLE CORE function's call graph for writes by NAME,
        and a local helper called `rename` reads as `os.rename` to it.
        """
        return anonymous.get(name, name)

    functions, commons, symbols = {}, {}, {}
    for i in range(elf.symcount):
        s = elf.sym(i)
        name = elf.symname(i).decode()
        kind = s[3] & 15
        if kind == 4:  # STT_FILE names the source path, not runtime data
            continue
        if s[5] == SHN_COMMON:
            commons[name] = {"size": s[2], "alignment": s[1],
                             "binding": s[3] >> 4}
            continue
        if kind == 2 and s[5] and s[2]:
            section = elf.sh[s[5]]
            body = bytes(elf.data[section[4] + s[1]:section[4] + s[1] + s[2]])
            functions[name] = {
                "section": elf.names[s[5]], "offset": s[1], "size": s[2],
                "binding": s[3] >> 4, "body": body.hex(),
                "relocations": [[o, k, pool_name(n), a] for o, k, n, a in
                                _function_relocations(elf, s[5], s[1], s[2])]}
        if kind == 1 and 0 < s[5] < len(elf.sh):
            symbols[repr(pool_name(name))] = {
                "section": elf.names[s[5]], "offset": s[1], "size": s[2],
                "binding": s[3] >> 4}

    sections = {}
    for i, h in enumerate(elf.sh):
        if not h[2] & 2:  # not SHF_ALLOC
            continue
        rows = []
        for ri, rh in enumerate(elf.sh):
            if rh[1] == 9 and rh[7] == i:
                raise Refused("implicit-addend relocations are unsupported")
            if rh[1] == 4 and rh[7] == i:
                _, entries = elf.relas(elf.names[ri])
                rows.extend((off, info & 255, pool_name(elf.symname(info >> 8).decode()), add)
                            for off, info, add in entries)
        sections[elf.names[i]] = {
            "type": h[1], "flags": h[2], "size": h[5], "alignment": h[8],
            "bytes": None if h[1] == 8 else bytes(elf.data[h[4]:h[4] + h[5]]).hex(),
            "relocations": sorted([list(r) for r in rows])}
    # `exception_records` refuses (ValueError, or SystemExit in some callers)
    # when an extab payload carries relocations it cannot resolve from raw
    # bytes. That is a refusal to MEASURE, and it must surface as one rather
    # than as a traceback or, worse, as an object with no EH inventory.
    try:
        records = exception_records(bytes(elf.data))
    except (ValueError, KeyError, SystemExit) as error:
        raise Refused(f"{Path(path).name}: exception metadata not measurable: {error}")
    return json.loads(json.dumps({
        "functions": functions, "sections": sections, "symbols": symbols,
        "common": commons, "exception_records": records}))


#: A remaining rule may legitimately re-derive these two keys and nothing
#: else: they hash the window's RELOCATION table, which an unrelated
#: anonymous-pool renumbering upstream moves without touching one instruction
#: byte (webfrank.rederive_hint documents the class). The BODY hashes
#: `before_sha256`/`after_sha256` are never in this set -- a moved body hash
#: means codegen changed, and accepting a re-paste there would launder it.
REDERIVABLE_RULE_KEYS = ("before_relocations_sha256", "after_relocations_sha256")


def _erase_rederivable(value):
    """A deep copy with the relocation-hash leaves blanked, for comparison."""
    if isinstance(value, dict):
        return {k: ("<REDERIVED>" if k in REDERIVABLE_RULE_KEYS
                    else _erase_rederivable(v)) for k, v in value.items()}
    if isinstance(value, list):
        return [_erase_rederivable(v) for v in value]
    return value


def compare_images(before, after, allowed, target):
    """Whole-object equality, with named functions permitted to move.

    A permitted function may differ only in its BODY, only toward the target
    at every changed bit, and never in its positional relocations. Everything
    else -- allocated sections including the substituted .text, data symbols,
    SHN_COMMON allocations and exception records -- must be identical.
    """
    differences = []
    permitted = {}
    if before["functions"].keys() != after["functions"].keys():
        return {"ok": False, "differences": ["function roster changed"],
                "permitted": {},
                "added": sorted(after["functions"].keys() - before["functions"].keys()),
                "removed": sorted(before["functions"].keys() - after["functions"].keys())}
    expected = json.loads(json.dumps(before))
    section_bytes = {}
    for name, row in before["functions"].items():
        new = after["functions"][name]
        if row == new:
            continue
        if name not in allowed:
            differences.append("unpermitted function change: " + name)
            continue
        for key in ("section", "offset", "size", "binding", "relocations"):
            if row[key] != new[key]:
                differences.append(f"permitted function {name} changed {key}")
        target_row = target["functions"].get(name)
        if target_row is None:
            differences.append(f"permitted function {name} is absent from the target")
            continue
        try:
            permitted[name] = monotonic_words(bytes.fromhex(row["body"]),
                                              bytes.fromhex(new["body"]),
                                              bytes.fromhex(target_row["body"]))
        except Refused as error:
            differences.append(f"permitted function {name}: {error}")
            continue
        expected["functions"][name]["body"] = new["body"]
        section = row["section"]
        if section not in section_bytes:
            blob = expected["sections"].get(section, {}).get("bytes")
            if blob is None:
                differences.append(f"permitted function {name} has no stored section bytes")
                continue
            section_bytes[section] = bytearray.fromhex(blob)
        body = bytes.fromhex(new["body"])
        section_bytes[section][row["offset"]:row["offset"] + row["size"]] = body
    for section, blob in section_bytes.items():
        expected["sections"][section]["bytes"] = blob.hex()
    if expected != after:
        for key in ("sections", "symbols", "common", "exception_records"):
            if expected[key] != after[key]:
                differences.append("changed outside the permitted bodies: " + key)
    return {"ok": not differences, "differences": differences, "permitted": permitted}


def monotonic_words(before, after, target):
    """r84's rule: a changed sibling may only move TOWARD the target."""
    if not (len(before) == len(after) == len(target)) or len(before) % 4:
        raise Refused("changed sibling has a different instruction count")
    changed = exact = 0
    for i in range(0, len(before), 4):
        a, b, t = (int.from_bytes(x[i:i + 4], "big") for x in (before, after, target))
        if a == b:
            continue
        if ((a ^ t) & (b ^ t)) != (b ^ t):
            raise Refused(f"changed sibling introduced non-target bits at +0x{i:x}")
        changed += 1
        exact += b == t
    return {"changed_words": changed, "new_exact_words": exact}


# ------------------------------------------------------------------- process
def sha256(data):
    return hashlib.sha256(data).hexdigest()


def git_show(ref, path):
    proc = subprocess.run(["git", "show", f"{ref}:{path}"], cwd=str(REPO),
                          capture_output=True)
    if proc.returncode:
        raise Refused(f"git show {ref}:{path} failed: "
                      f"{proc.stderr.decode(errors='replace').strip()}")
    return proc.stdout


def resolve_commit(ref):
    """(object id, type) for `ref`, resolved with `git cat-file`.

    RUN-58 ITEM 5, first half. NOT `git rev-parse --verify <rev>^{commit}`:
    measured at 631d2e0ab under git 2.51.0 in this checkout, EVERY `^{...}`
    peel spelling fails with `fatal: Needed a single revision` -- the full
    40-character sha, the abbreviation, `HEAD` and a branch name alike, and
    `^{tree}` too -- while `git cat-file -t` resolves the same revision and
    plain `rev-parse --verify <sha>` resolves it as well. A ref check written
    on the peel syntax therefore rejects every valid revision it is given, so
    resolution here goes through `cat-file --batch-check`, which returns the
    id and the TYPE in one call and says `missing` or `ambiguous` in words.
    """
    proc = subprocess.run(["git", "cat-file", "--batch-check"],
                          input=(ref + "\n").encode(), cwd=str(REPO),
                          capture_output=True)
    fields = proc.stdout.decode(errors="replace").strip().split()
    if proc.returncode or len(fields) < 3:
        detail = " ".join(fields[1:]) or \
            proc.stderr.decode(errors="replace").strip() or "unresolvable"
        raise Refused(f"--before-ref {ref!r} does not resolve: {detail}"
                      f" (`git cat-file --batch-check` over {ref!r})")
    return fields[0], fields[1]


def load_build_edges():
    path = REPO / "build" / VERSION / "build_edges.json"
    if not path.exists():
        raise Refused(f"missing {path}; run `python configure.py`")
    return json.loads(path.read_text(encoding="utf-8"))


def postprocess_edge(edges_data, unit):
    """The unit's WebFrank edge from the generator snapshot, or None."""
    wanted = f"build/{VERSION}/src/{unit}.o"
    for edge in edges_data["edges"]:
        if wanted in [p.replace("\\", "/") for p in edge["outputs"]]:
            return edge
    return None


def unit_linkage(edges_data, unit):
    for row in edges_data.get("units", []):
        name = f"{Path(row['name']).with_suffix('').as_posix()}"
        if name == unit:
            return row.get("linkage")
    return None


def replay_rules(edge, config_path, source_object, out_object):
    """Run the REAL postprocessor over one object; refuse on anything else."""
    variables = {k: v.replace("\\", "/") for k, v in edge["variables"].items()}
    if edge["rule"] == "webfrank":
        command = [sys.executable, "tools/gdl/webfrank.py", str(source_object),
                   str(out_object), str(config_path), variables["webfrank_unit"],
                   "--target", variables["webfrank_target"],
                   "--image", variables["webfrank_image"]]
    elif edge["rule"] == "webfrank_globalize_atree":
        command = [sys.executable, "tools/gdl/atree_exports.py", str(source_object),
                   str(out_object), "--objcopy", variables["atree_objcopy"],
                   "--webfrank-config", str(config_path),
                   "--webfrank-unit", variables["webfrank_unit"],
                   "--target", variables["webfrank_target"],
                   "--image", variables["webfrank_image"]]
    else:
        raise Refused(f"unmodelled postprocessor rule {edge['rule']!r}")
    proc = subprocess.run(command, cwd=str(REPO), capture_output=True, text=True,
                          errors="replace")
    return proc, command


def compile_source(edge, source_path, out_object, workdir, extra_include=None):
    """Compile through the actual Ninja compiler edge; (bytes, trace)."""
    cflags = edge["cflags"]
    if extra_include:
        cflags = cflags + " -i " + extra_include
    trial = dict(edge, _command_trace=[])
    trial["src"] = Path(source_path).as_posix()
    obj, error = cv.compile_with(trial, edge["mw"], cflags, out_object, workdir)
    if error or not obj:
        raise Refused(f"compile failed for {source_path}: {error or 'no output'}")
    return Path(obj).read_bytes(), trial["_command_trace"]


# --------------------------------------------------------------------- audit
class Audit:
    def __init__(self, unit, function, before_ref, allow_changed=(),
                 keep=False, skip_link=False, allow_rederived=()):
        self.unit = unit
        self.function = function
        self.before_ref = before_ref
        self.allow_changed = list(allow_changed)
        self.allow_rederived = list(allow_rederived)
        self.keep = keep
        self.skip_link = skip_link
        self.checks = []
        self.folder = None

    def record(self, name, status, **values):
        self.checks.append(dict(name=name, status=status, **values))
        return status == "PASS"

    # -- inputs ------------------------------------------------------------
    def inputs(self):
        edges = cv.read_edges()
        if self.unit not in edges:
            raise Refused(f"no Ninja compile edge for unit {self.unit!r}")
        self.edge = edges[self.unit]
        self.edges_data = load_build_edges()
        self.source = REPO / self.edge["src"]
        self.raw_object = REPO / self.edge["body_o"]
        self.processed_object = REPO / f"build/{VERSION}/src/{self.unit}.o"
        self.target_object = REPO / f"build/{VERSION}/obj/{self.unit}.o"
        self.config = REPO / "config" / VERSION / "webfrank.json"
        self.compiler = REPO / "build/compilers" / self.edge["mw"] / "mwcceppc.exe"
        for label, path in (("source", self.source), ("raw object", self.raw_object),
                            ("processed object", self.processed_object),
                            ("target object", self.target_object),
                            ("rule config", self.config), ("compiler", self.compiler)):
            if not path.exists():
                raise Refused(f"missing {label}: {path} (run `ninja` first)")
        if not self.edge.get("raw"):
            raise Refused(f"{self.unit} has no pre-postprocessor body edge; "
                          "there is no rule to retire in this unit")
        self.post_edge = postprocess_edge(self.edges_data, self.unit)
        if self.post_edge is None:
            raise Refused(f"no postprocessor edge for {self.unit} in build_edges.json")
        self.before_oid = self.check_before_ref()
        self.record("inputs", "PASS", unit=self.unit, function=self.function,
                    before_ref=self.before_ref, before_ref_commit=self.before_oid,
                    source=self.edge["src"], raw_object=self.edge["body_o"],
                    target_object=str(self.target_object.relative_to(REPO)).replace("\\", "/"),
                    compiler=self.edge["mw"], compiler_sha256=sha256(self.compiler.read_bytes()),
                    cflags=self.edge["cflags"], postprocess_rule=self.post_edge["rule"],
                    linkage=unit_linkage(self.edges_data, self.unit),
                    target_sha256=sha256(self.target_object.read_bytes()))

    def check_before_ref(self):
        """Resolve --before-ref, and REFUSE one that is the current state.

        RUN-58 ITEM 5, second half. `--before-ref <sha>^` is the natural
        spelling of "the commit before the retirement", and in this
        environment the caret NEVER reaches argv: measured at 631d2e0ab,
        `--before-ref 631d2e0ab^` arrives at a Python script as
        `'631d2e0ab'`, unquoted, single-quoted, double-quoted and doubled
        (`^^`) alike, and `^{commit}` additionally spawns a nested shell
        (`-encodedCommand YwBvAG0AbQBpAHQA`). So the audit silently ran
        against the RETIREMENT COMMIT ITSELF.

        Reproduced end-to-end before this check existed:

          retire_audit.py game/sys/memcard memCardErrorPrompt \\
              --before-ref e7f9d740b        # the eaten-caret form

          -> [   PASS] before_image   source_changed: false
             [   FAIL] rule_delta
             FAILING CHECK(S): rule_delta        EXIT 1

        Two things are wrong with that. `before_image` is recorded PASS
        while the before image IS the after image -- the tool's own
        docstring says a sibling inventory compared against itself is not
        evidence. And exit 1 means "the measurement happened and a named
        check did not hold", so the operator reads a rule_delta defect in
        a retirement whose only defect is the ref they typed. A measurement
        that did not happen is REFUSED (exit 2), and it is refused BEFORE
        the three compiles and the link, not after them.

        Returns the resolved commit id and banks the before-image source so
        `build_images` does not run `git show` twice.
        """
        oid, kind = resolve_commit(self.before_ref)
        if kind != "commit":
            raise Refused(f"--before-ref {self.before_ref!r} names a {kind}, "
                          f"not a commit ({oid[:9]})")
        head_oid, _ = resolve_commit("HEAD")
        self.before_source = git_show(self.before_ref, self.edge["src"])
        if self.before_source == self.source.read_bytes():
            same = (" -- it resolves to HEAD itself"
                    if oid == head_oid else "")
            raise Refused(
                f"--before-ref {self.before_ref!r} ({oid[:9]}) holds a "
                f"BYTE-IDENTICAL {self.edge['src']} to the working tree"
                f"{same}, so the before image would be the after image and "
                "nothing would be compared. Pass the revision BEFORE the "
                f"change, spelled `{self.before_ref}~1`. Do NOT spell it "
                f"`{self.before_ref}^`: the caret is consumed before it "
                "reaches argv in this environment, at every quoting level, "
                "and the audit then runs against the very commit you meant "
                "to exclude.")
        return oid

    # -- fidelity and the before image ------------------------------------
    def build_images(self):
        self.folder = Path(tempfile.mkdtemp(prefix="ta_retire_audit_", dir=REPO / "build"))
        shipped = self.raw_object.read_bytes()
        fresh, trace = compile_source(self.edge, self.edge["src"],
                                      self.folder / "ta_fresh.o", self.folder)
        self.record("fresh_edge_fidelity", "PASS" if fresh == shipped else "FAIL",
                    shipped_sha256=sha256(shipped), fresh_sha256=sha256(fresh),
                    shipped_bytes=len(shipped), fresh_bytes=len(fresh),
                    command=trace[0]["argv"] if trace else None,
                    note=("the raw object on disk is what a fresh compile of the current "
                          "source produces" if fresh == shipped else
                          "the raw object on disk does NOT match a fresh compile; "
                          "run `ninja` before auditing"))
        include_dir = Path(self.edge["src"]).parent.as_posix()
        # SAME BASENAME, different directory. MWCC records the source file's
        # BASENAME in the object (measured: compiling this TU as
        # `before_memcard.c` produced an object 8 bytes longer than the
        # shipped one, string-table only), so a scratch copy named anything
        # else makes every later whole-ELF byte comparison impossible and
        # forces a weaker image-only check for no reason.
        control_dir = self.folder / "control"
        before_dir = self.folder / "before"
        control_dir.mkdir()
        before_dir.mkdir()
        basename = Path(self.edge["src"]).name
        scratch_source = control_dir / basename
        scratch_source.write_bytes(self.source.read_bytes())
        control, _ = compile_source(
            self.edge, scratch_source.relative_to(REPO).as_posix(),
            self.folder / "ta_control.o", self.folder, extra_include=include_dir)
        self.record("scratch_edge_control", "PASS" if control == shipped else "FAIL",
                    control_sha256=sha256(control), shipped_sha256=sha256(shipped),
                    appended_include=include_dir, scratch_basename=basename,
                    note=("compiling from the scratch directory with the appended "
                          "include path reproduces the same object BYTE for BYTE, "
                          "so the before image is measured through a validated "
                          "command and stays byte-comparable"))
        old_source = self.before_source   # banked by check_before_ref
        before_source_path = before_dir / basename
        before_source_path.write_bytes(old_source)
        before, _ = compile_source(
            self.edge, before_source_path.relative_to(REPO).as_posix(),
            self.folder / "ta_before.o", self.folder, extra_include=include_dir)
        self.before_rules = json.loads(git_show(self.before_ref, f"config/{VERSION}/webfrank.json"))
        self.record("before_image", "PASS", ref=self.before_ref,
                    before_source_sha256=sha256(old_source),
                    current_source_sha256=sha256(self.source.read_bytes()),
                    source_changed=old_source != self.source.read_bytes(),
                    before_object_sha256=sha256(before),
                    before_object_bytes=len(before))
        self.fresh_image = object_image(self.folder / "ta_fresh.o")
        self.before_image = object_image(self.folder / "ta_before.o")
        self.target_image = object_image(self.target_object)
        for label, image in (("before", self.before_image), ("fresh", self.fresh_image),
                             ("target", self.target_image)):
            if not image["functions"]:
                raise Refused(f"{label} object has no functions; an empty "
                              "inventory is not a certificate")
        self.paths = {"fresh": self.folder / "ta_fresh.o",
                      "before": self.folder / "ta_before.o"}

    # -- the function ------------------------------------------------------
    def native_body(self):
        ours = self.fresh_image["functions"].get(self.function)
        target = self.target_image["functions"].get(self.function)
        if ours is None or target is None:
            raise Refused(
                f"{self.function} is missing from the "
                f"{'fresh raw' if ours is None else 'target'} object")
        our_body = bytes.fromhex(ours["body"])
        target_body = bytes.fromhex(target["body"])
        types = _reloc_types_by_index(ours["relocations"], target["relocations"])
        residual = classify_stream(our_body, target_body, types)
        previous = self.before_image["functions"].get(self.function)
        before_residual = None
        if previous is not None:
            before_residual = classify_stream(
                bytes.fromhex(previous["body"]), target_body,
                _reloc_types_by_index(previous["relocations"], target["relocations"]))
        ok = our_body == target_body
        self.record("native_body", "PASS" if ok else "FAIL",
                    our_size=ours["size"], target_size=target["size"],
                    our_body_sha256=sha256(our_body), target_body_sha256=sha256(target_body),
                    binding_ours=ours["binding"], binding_target=target["binding"],
                    residual=residual,
                    before_differing_words=(before_residual or {}).get("differing_words"),
                    before_residual_summary=(before_residual or {}).get("summary"),
                    note=("the raw compiler body equals the target body"
                          if ok else "differing instructions classified above"))

    def positional_relocations(self):
        ours = self.fresh_image["functions"].get(self.function)
        target = self.target_image["functions"].get(self.function)
        if ours is None or target is None:
            return self.record("positional_relocations", "SKIPPED",
                               reason="function missing from an object")
        try:
            a = normalize_relocations(ours["relocations"], "ours")
            b = normalize_relocations(target["relocations"], "target")
            bases = section_bases(self.unit)
            addresses = relocation_addresses()
        except Refused as error:
            return self.record("positional_relocations", "REFUSED", reason=str(error))
        # Run-59 item 11: compare the ADDRESS each relocation binds, not the
        # two toolchains' different spellings of it. Unresolvable names keep
        # their name and still have to match verbatim.
        a = resolve_relocations(a, bases, addresses)
        b = resolve_relocations(b, bases, addresses)

        def rows(table):
            return [[o, RELOC_TYPE_NAMES[k], n, add] for o, k, n, add in table]

        def unresolved(table):
            return sorted({n if isinstance(n, str) else tuple(n)
                           for _o, _k, n, _a in table
                           if not (isinstance(n, list) and n and
                                   n[0] == "address")}, key=repr)

        return self.record("positional_relocations", "PASS" if a == b else "FAIL",
                           our_count=len(a), target_count=len(b),
                           ours=rows(a), target=rows(b),
                           differences=[[x, y] for x, y in
                                        zip(rows(a) + [None] * len(b), rows(b) + [None] * len(a))
                                        if x != y][:20],
                           section_bases={s: f"0x{b_:08x}"
                                          for s, b_ in sorted(bases.items())},
                           unresolved_ours=unresolved(a),
                           unresolved_target=unresolved(b),
                           normalization="R_PPC_EMB_SDA21 offsets moved from "
                                         "instruction+2 (MWCC) to instruction+0 (dtk);"
                                         " symbols resolved to linked addresses"
                                         " (anonymous pool label -> its section's"
                                         " splits.txt base + offset; named symbol ->"
                                         " its symbols.txt address). Unresolvable"
                                         " names are compared verbatim.")

    # -- the rest of the TU ------------------------------------------------
    def siblings(self):
        before = self.before_image["functions"]
        after = self.fresh_image["functions"]
        if before.keys() != after.keys():
            return self.record(
                "sibling_bodies", "FAIL", reason="function roster changed",
                added=sorted(after.keys() - before.keys()),
                removed=sorted(before.keys() - after.keys()))
        changed = sorted(n for n in before if n != self.function and before[n] != after[n])
        allowed = {}
        unexpected = []
        for name in changed:
            if name not in self.allow_changed:
                unexpected.append(name)
                continue
            target = self.target_image["functions"].get(name)
            if target is None:
                unexpected.append(name)
                continue
            try:
                allowed[name] = monotonic_words(
                    bytes.fromhex(before[name]["body"]),
                    bytes.fromhex(after[name]["body"]),
                    bytes.fromhex(target["body"]))
            except Refused as error:
                allowed[name] = {"error": str(error)}
                unexpected.append(name)
            if before[name]["relocations"] != after[name]["relocations"]:
                unexpected.append(name)
        return self.record("sibling_bodies", "FAIL" if unexpected else "PASS",
                           siblings_compared=len(before) - 1,
                           siblings_byte_equal=len(before) - 1 - len(changed),
                           changed=changed, unexpected_changes=sorted(set(unexpected)),
                           permitted_changes=allowed,
                           permitted_rule="a permitted sibling may only move toward "
                                          "the target at every changed bit")

    def nontext(self):
        differences = []
        for key in ("sections", "symbols", "common", "exception_records"):
            a, b = self.before_image[key], self.fresh_image[key]
            if key == "sections":
                a = {k: v for k, v in a.items() if k != ".text"}
                b = {k: v for k, v in b.items() if k != ".text"}
                for name in sorted(set(a) | set(b)):
                    if a.get(name) != b.get(name):
                        differences.append(f"section {name}")
            elif a != b:
                differences.append(key)
        counted = {k: len(self.fresh_image[k]) for k in ("sections", "symbols", "common")}
        counted["exception_records"] = len(self.fresh_image["exception_records"])
        counted["nontext_section_names"] = sorted(
            n for n in self.fresh_image["sections"] if n != ".text")
        return self.record("nontext_sections", "FAIL" if differences else "PASS",
                           differences=differences, compared=counted,
                           note="allocated data/rodata/sdata/sdata2/bss/sbss bytes, "
                                "headers and relocations, SHN_COMMON allocations, "
                                "data symbols and exception records")

    def rule_delta(self):
        current = json.loads(self.config.read_text(encoding="utf-8"))
        before = self.before_rules
        before_unit = before.get("units", {}).get(self.unit, [])
        after_unit = current.get("units", {}).get(self.unit, [])
        removed = [r["function"] for r in before_unit if r["function"] == self.function]
        expected = [r for r in before_unit if r["function"] != self.function]
        still_pinned = any(r["function"] == self.function for r in after_unit)
        # A remaining rule whose ONLY change is its relocation hashes is the
        # documented re-derivation class. It is permitted per rule, by name,
        # and it is never free: rule_replay below actually re-applies it and
        # requires the processed object to be unchanged, so a re-hash that
        # laundered a real codegen difference fails there.
        rederived = []
        if len(expected) == len(after_unit):
            relaxed = []
            for old, new in zip(expected, after_unit):
                if old != new and _erase_rederivable(old) == _erase_rederivable(new):
                    name = old.get("function")
                    rederived.append(name)
                    relaxed.append(new if name in self.allow_rederived else old)
                else:
                    relaxed.append(old)
            expected = relaxed
        unapproved_rederived = sorted(set(rederived) - set(self.allow_rederived))
        foreign = sorted(k for k in set(before.get("units", {})) | set(current.get("units", {}))
                         if k != self.unit
                         and before.get("units", {}).get(k) != current.get("units", {}).get(k))
        top_level = {k: v for k, v in current.items() if k != "units"} != \
                    {k: v for k, v in before.items() if k != "units"}
        ok = bool(removed) and not still_pinned and expected == after_unit and not top_level
        reasons = []
        if not removed:
            reasons.append(f"{self.function} carried no rule at {self.before_ref}")
        if still_pinned:
            reasons.append(f"{self.function} is STILL rule-served in the current config")
        if expected != after_unit:
            reasons.append("the unit's remaining rules are not exactly the previous ones")
        if unapproved_rederived:
            reasons.append("relocation hashes re-derived without --allow-rederived-rule: "
                           + ", ".join(unapproved_rederived))
        if top_level:
            reasons.append("webfrank.json top-level configuration changed")
        return self.record("rule_delta", "PASS" if ok and not reasons else "FAIL",
                           rules_before=[r["function"] for r in before_unit],
                           rules_after=[r["function"] for r in after_unit],
                           retired=removed, reasons=reasons,
                           rules_with_rederived_relocation_hashes=sorted(set(rederived)),
                           approved_rederived_rules=sorted(self.allow_rederived),
                           foreign_unit_deltas_not_certified=foreign)

    def rule_replay(self):
        before_config = self.folder / "ta_before_webfrank.json"
        before_config.write_text(json.dumps(self.before_rules, indent=2), encoding="utf-8")
        results = {}
        for label, source, config in (
                ("before", self.paths["before"], before_config),
                ("after", self.paths["fresh"], self.config)):
            out = self.folder / f"ta_{label}_processed.o"
            proc, command = replay_rules(self.post_edge, config, source, out)
            results[label] = {
                "returncode": proc.returncode, "produced": out.exists(),
                "rules": [r["function"] for r in
                          (self.before_rules if label == "before"
                           else json.loads(self.config.read_text(encoding="utf-8"))
                           ).get("units", {}).get(self.unit, [])],
                "stdout": proc.stdout.strip().splitlines()[-6:],
                "stderr": proc.stderr.strip().splitlines()[-6:],
                "command": command,
                "sha256": sha256(out.read_bytes()) if out.exists() else None}
        shipped = self.processed_object.read_bytes()
        equal = (results["before"]["sha256"] == results["after"]["sha256"]
                 and results["after"]["sha256"] is not None)
        fresh_shipped = results["after"]["sha256"] == sha256(shipped)
        failed = [label for label, row in results.items()
                  if row["returncode"] or not row["produced"]]
        reasons = []
        delta = None
        if failed:
            reasons.append("postprocessor refused for: " + ", ".join(failed))
        elif not equal:
            # Byte equality is the strongest outcome and the usual one. When
            # the retirement deliberately improved a caller, the processed
            # object MUST still be identical everywhere else, and the caller
            # must have moved toward the target -- not merely "differently".
            delta = compare_images(
                object_image(self.folder / "ta_before_processed.o"),
                object_image(self.folder / "ta_after_processed.o"),
                set(self.allow_changed) | {self.function}, self.target_image)
            if not delta["ok"]:
                reasons.append("the processed object changed outside the "
                               "permitted bodies: " + "; ".join(delta["differences"]))
        if not fresh_shipped:
            reasons.append("the replayed object differs from the one Ninja shipped")
        return self.record("rule_replay", "PASS" if not reasons else "FAIL",
                           replays=results, shipped_sha256=sha256(shipped),
                           processed_object_byte_equal=equal,
                           processed_object_delta=delta, reasons=reasons,
                           note="every remaining rule really re-applied; the "
                                "linked object must be unchanged by a retirement "
                                "except in explicitly permitted bodies")

    def link_hash(self):
        if self.skip_link:
            return self.record("link_hash", "SKIPPED", reason="--skip-link")
        dtk = REPO / "build" / "tools" / ("dtk.exe" if os.name == "nt" else "dtk")
        sha1 = REPO / "config" / VERSION / "build.sha1"
        dol = REPO / "build" / VERSION / "main.dol"
        for label, path in (("dtk", dtk), ("build.sha1", sha1), ("main.dol", dol)):
            if not path.exists():
                return self.record("link_hash", "REFUSED", reason=f"missing {label}: {path}")
        expected = sha1.read_text(encoding="utf-8").split()[0]
        actual = hashlib.sha1(dol.read_bytes()).hexdigest()
        proc = subprocess.run([str(dtk), "shasum", "-c", str(sha1)], cwd=str(REPO),
                              capture_output=True, text=True, errors="replace")
        return self.record("link_hash", "PASS" if proc.returncode == 0 and actual == expected else "FAIL",
                           configured_sha1=expected, built_sha1=actual,
                           dtk_returncode=proc.returncode,
                           dtk_output=(proc.stdout + proc.stderr).strip().splitlines()[-4:],
                           note="config/GUNE5D/build.sha1 through the build, not the "
                                "retail input's SHA-1")

    def run(self):
        try:
            self.inputs()
            self.build_images()
            self.native_body()
            self.positional_relocations()
            self.siblings()
            self.nontext()
            self.rule_delta()
            self.rule_replay()
            self.link_hash()
            status = "PASS"
        except Refused as error:
            self.checks.append({"name": "measurement", "status": "REFUSED",
                                "reason": str(error)})
            status = "REFUSED"
        except (OSError, ValueError, KeyError, IndexError, SystemExit) as error:
            # A helper that refuses via SystemExit is NOT caught by
            # `except Exception`; naming it here keeps a refusal a refusal
            # instead of a traceback the caller has to interpret.
            self.checks.append({"name": "measurement", "status": "REFUSED",
                                "reason": f"{type(error).__name__}: {error}"})
            status = "REFUSED"
        finally:
            if self.folder and not self.keep:
                shutil.rmtree(self.folder, ignore_errors=True)
        failing = [c["name"] for c in self.checks if c["status"] == "FAIL"]
        refused = [c["name"] for c in self.checks if c["status"] == "REFUSED"]
        skipped = [c["name"] for c in self.checks if c["status"] == "SKIPPED"]
        if refused:
            status = "REFUSED"
        elif failing:
            status = "FAIL"
        return {
            "schema_version": 1, "tool": "tools/gdl/retire_audit.py",
            "status": status, "unit": self.unit, "function": self.function,
            "before_ref": self.before_ref,
            "allowed_changed_siblings": self.allow_changed,
            "allowed_rederived_rules": self.allow_rederived,
            "failing_checks": failing, "refused_checks": refused,
            "skipped_checks": skipped, "checks": self.checks,
            "artifacts": (str(self.folder.relative_to(REPO)).replace("\\", "/")
                          if self.folder and self.keep else None),
            "limitations": [
                "Not a TU flip: a NonMatching unit still links its extracted object.",
                "Certifies one rule's retirement against the named previous state, "
                "not source correctness or compiler-internal causality.",
                "Other units' configuration deltas are reported, not certified.",
                "The scratch include path is validated against the current source only.",
            ],
        }


def format_report(result):
    out = [f"{result['status']}: retire_audit {result['unit']}::{result['function']}"
           f" (before-ref {result['before_ref']})"]
    for check in result["checks"]:
        out.append(f"  [{check['status']:>7}] {check['name']}")
        for key, value in check.items():
            if key in ("name", "status"):
                continue
            if key == "residual" and isinstance(value, dict):
                out.append(f"            residual: {value['summary']}")
                for row in value.get("rows", [])[:40]:
                    out.append(f"              +0x{row['offset']:04x} ours {row['ours']}"
                               f" target {row['target']}  {row['class']} ({row['decode']})")
                continue
            text = json.dumps(value) if not isinstance(value, str) else value
            if len(text) > 400:
                text = text[:400] + " ..."
            out.append(f"            {key}: {text}")
    if result["failing_checks"]:
        out.append("  FAILING CHECK(S): " + ", ".join(result["failing_checks"]))
    if result["refused_checks"]:
        out.append("  REFUSED: " + ", ".join(result["refused_checks"]))
    out.append("  LIMITS: " + " ".join(result["limitations"]))
    return "\n".join(out)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("unit", help="unit key, e.g. game/sys/memcard")
    parser.add_argument("function", help="the function whose rule was retired")
    parser.add_argument("--before-ref", required=True, metavar="REV",
                        help="the git revision holding the PREVIOUS state "
                             "(source and webfrank.json), e.g. HEAD~1")
    parser.add_argument("--allow-changed-sibling", action="append", default=[],
                        metavar="NAME",
                        help="a sibling permitted to change, and then only "
                             "monotonically toward the target (repeatable)")
    parser.add_argument("--allow-rederived-rule", action="append", default=[],
                        metavar="NAME",
                        help="a REMAINING rule permitted to differ only in its "
                             "relocation hashes (the anonymous-pool "
                             "re-derivation class); body hashes are never "
                             "permitted to move (repeatable)")
    parser.add_argument("--skip-link", action="store_true",
                        help="skip the DOL hash check (states it as SKIPPED, "
                             "which is not a PASS)")
    parser.add_argument("--keep-artifacts", action="store_true",
                        help="retain the scratch objects under build/")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--out", type=Path, help="write the JSON certificate here")
    args = parser.parse_args(argv)
    if args.out is not None:
        resolved = args.out.resolve()
        if not resolved.is_relative_to((REPO / "build").resolve()):
            parser.error("--out must be under this checkout's build/")
    result = Audit(args.unit, args.function, args.before_ref,
                   args.allow_changed_sibling, args.keep_artifacts,
                   args.skip_link, args.allow_rederived_rule).run()
    if args.out is not None:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2) if args.json else format_report(result))
    return {"PASS": 0, "FAIL": 1}.get(result["status"], 2)


if __name__ == "__main__":
    raise SystemExit(main())
