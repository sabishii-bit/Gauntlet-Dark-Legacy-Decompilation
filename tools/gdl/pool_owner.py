#!/usr/bin/env python3
"""The data-ownership step of the recovery protocol, for one TU.

    python tools/gdl/pool_owner.py game/sys/ml_mem
    python tools/gdl/pool_owner.py game/enemy/enemy --json --out build/x.json

Every pool question a retirement lane asks is the same four:

  WHAT is this datum?      address, section, size, the target's own bytes and
                           their decoded value -- an `@NN` name is not an
                           address and a value is not an identity.
  WHO owns it?             the split run in config/GUNE5D/splits.txt that
                           contains that address, or UNCLAIMED when no C TU
                           claims it (a dtk `auto_*` object holds it, and
                           claiming it is a split-ownership change needing
                           integrator review).
  Do WE emit it?           our raw object's anonymous pool entries, matched by
                           EXACT bytes. A value match is reported as a value
                           match and nothing more: two datums of equal value
                           are not one datum, so the row also says how many
                           of our entries carry that value.
  WHERE is it read?        every reference site, as function plus offset
                           inside that function, from the target object's
                           positional relocations and from ours.

and then one prediction: MWCC lays a function's pool out in FIRST-USE order,
so the datums of the TU's own claimed run should appear at ascending
addresses in the order the target text first references them. The tool prints
that predicted order against the actual address order and names the first
position where they disagree -- which is the shape of a pool-ownership or
emission-order defect, not a proof of one.

Sources, all read-only: config/GUNE5D/symbols.txt (names, addresses, sizes,
data kinds), config/GUNE5D/splits.txt (ownership runs), the extracted target
object under build/<v>/obj (bytes and reference sites), our raw
pre-postprocess object under build/<v>/src (anonymous entries), the split
assembly under build/<v>/asm (the authoritative label listing and decoded
directives for the TU's own run) and orig/<v>/sys/main.dol through
tools/gdl/poolval (the linked bytes behind a relocation).

REFUSALS (exit 2), never a silent empty table: a missing symbols/splits file,
a missing target object, a unit with no split entry, or an unreadable object.
An empty referenced-datum list is printed as an explicit "no pool datum is
referenced", not as a blank section.

LIMITS. Referencing is read off the target's relocations, so a datum reached
only through a computed address is invisible here. Equal bytes never prove
one original object, and this tool never proposes a binding: it reports
ownership evidence for a human decision.

IMPORTABLE CORE: load_splits, owner_of, decode_value, pool_datums, predict_first_use
-- pure over parsed data; no build and no printing.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import struct
import sys

if __package__ in (None, ""):
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    sys.path.insert(0, str(Path(__file__).resolve().parent))
from tools.fix_exception_objects import Elf
from tools.gdl import poolval

VERSION = "GUNE5D"
REPO = Path(__file__).resolve().parents[2]
#: The sections a literal pool lives in. `.data` is deliberately absent: a
#: writable global is a real object, never a literalization candidate.
POOL_SECTIONS = (".sdata2", ".rodata", ".sdata")
SPLIT_UNIT = re.compile(r"^(\S+\.(?:c|cpp|s)):\s*$")
SPLIT_RUN = re.compile(r"^\s+(\S+)\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)")
ASM_DATUM = re.compile(
    r"^#\s*(\S+):0x([0-9A-Fa-f]+)\s*\|\s*0x([0-9A-Fa-f]+)\s*\|\s*size:\s*0x([0-9A-Fa-f]+)\s*$")
ASM_OBJ = re.compile(r"^\.obj\s+(\S+?),")


class Refused(Exception):
    """A measurement could not be made; never an empty answer."""


# ------------------------------------------------------------------- inputs
def load_splits(path=None):
    """[(unit, section, start, end)] over every configured split run."""
    path = Path(path) if path else REPO / "config" / VERSION / "splits.txt"
    if not path.exists():
        raise Refused(f"missing splits file: {path}")
    runs = []
    unit = None
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = SPLIT_UNIT.match(line)
        if match:
            unit = re.sub(r"\.(c|cpp|s)$", "", match.group(1))
            continue
        run = SPLIT_RUN.match(line)
        if run and unit:
            runs.append((unit, run.group(1), int(run.group(2), 16), int(run.group(3), 16)))
    if not runs:
        raise Refused(f"{path} declares no split runs")
    return runs


def owner_of(runs, section, address):
    """The unit whose run contains this address, or "UNCLAIMED"."""
    for unit, name, start, end in runs:
        if name == section and start <= address < end:
            return unit
    return "UNCLAIMED"


def decode_value(data, size, kind=None):
    """Every reading of a pool datum's bytes, with none of them privileged.

    `preferred` names the reading the datum's own width and declared kind
    support, so a report does not print a 4-byte scalar's f64 -- which is
    read out of the NEXT datum's bytes and is how `f64 nan` ends up next to
    an s32 heap handle.
    """
    row = {"bytes": data[:size].hex(), "size": size, "data_kind": kind}
    if size >= 4 and len(data) >= 4:
        row["u32"] = struct.unpack_from(">I", data)[0]
        row["s32"] = struct.unpack_from(">i", data)[0]
        row["f32"] = struct.unpack_from(">f", data)[0]
    if len(data) >= 8 and size >= 8:
        row["f64"] = struct.unpack_from(">d", data)[0]
    printable = (lambda b: b and all(32 <= c < 127 or c in (9, 10, 13) for c in b))
    text = data[:size].rstrip(b"\0")
    if printable(text):
        row["string"] = text.decode("ascii", "replace")
    elif b"\0" in text:
        # AGENTS: "a target symbol covers a longer string run". dtk merges
        # consecutive strings under one `lbl_`, so the datum a reference
        # actually names is the segment at that offset, not the whole run.
        segments = [s for s in data[:size].split(b"\0") if s]
        if segments and all(printable(s) for s in segments):
            row["string_run"] = [s.decode("ascii", "replace") for s in segments]
            row["string"] = row["string_run"][0]
    # `data:` in symbols.txt is dtk's declared kind and outranks the width:
    # `gErrorCode ... size:0x8 data:4byte` is a 4-byte scalar in an 8-byte
    # slot, and reading its f64 splices the NEXT datum's bytes into the value.
    order = {"string": ("string",), "double": ("f64",), "float": ("f32",),
             "4byte": ("u32",), "2byte": ("bytes",), "byte": ("bytes",)}.get(
        kind, ("string", "f64", "f32", "u32") if size == 8 else
              ("string", "f32", "u32") if size == 4 else ("string",))
    row["preferred"] = next((name for name in order if name in row), "bytes")
    if "string_run" in row:
        row["preferred"] = "string_run"
    return row


def _elf_pool(path):
    """(anonymous entries, named entries) in one object's pool sections."""
    elf = Elf(str(path))
    anonymous, named = [], []
    for i in range(elf.symcount):
        s = elf.sym(i)
        if not 0 < s[5] < len(elf.sh):
            continue
        section = elf.names[s[5]]
        if section not in POOL_SECTIONS or (s[3] & 15) != 1:
            continue
        header = elf.sh[s[5]]
        data = bytes(elf.data[header[4] + s[1]:header[4] + s[1] + s[2]])
        row = {"name": elf.symname(i).decode(), "section": section,
               "offset": s[1], "size": s[2], "bytes": data.hex()}
        (anonymous if row["name"].startswith("@") else named).append(row)
    anonymous.sort(key=lambda r: (r["section"], r["offset"]))
    named.sort(key=lambda r: (r["section"], r["offset"]))
    return anonymous, named


def _function_references(path):
    """{symbol: [(function, offset in function, r_type)]} over .text."""
    elf = Elf(str(path))
    functions = []
    for i in range(elf.symcount):
        s = elf.sym(i)
        if (s[3] & 15) == 2 and s[5] and s[2] and elf.names[s[5]] == ".text":
            functions.append((s[1], s[1] + s[2], elf.symname(i).decode()))
    functions.sort()
    text = elf.sec.get(".text")
    references = {}
    for ri, rh in enumerate(elf.sh):
        if rh[1] != 4 or rh[7] != text:
            continue
        _, entries = elf.relas(elf.names[ri])
        for offset, info, _addend in entries:
            name = elf.symname(info >> 8).decode()
            owner = next((f for f in functions if f[0] <= offset < f[1]), None)
            references.setdefault(name, []).append(
                {"function": owner[2] if owner else "<outside any function>",
                 "offset": offset - (owner[0] if owner else 0),
                 "relocation_type": info & 255,
                 "text_offset": offset})
    for rows in references.values():
        rows.sort(key=lambda r: r["text_offset"])
    return references


def _asm_labels(unit):
    """[(section, address, size, label)] from the split assembly, or []."""
    path = REPO / "build" / VERSION / "asm" / (unit + ".s")
    if not path.exists():
        return []
    rows = []
    pending = None
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = ASM_DATUM.match(line)
        if match:
            pending = (match.group(1), int(match.group(3), 16), int(match.group(4), 16))
            continue
        obj = ASM_OBJ.match(line)
        if obj and pending:
            rows.append((pending[0], pending[1], pending[2], obj.group(1)))
            pending = None
    return [r for r in rows if r[0] in POOL_SECTIONS]


# -------------------------------------------------------------------- model
def pool_datums(unit, symbols, runs, references, target_sections, dol,
                our_anonymous, source_text):
    """One row per pool datum the target's text references."""
    blob, dol_sections = dol
    rows = []
    values = {}
    strings = {}
    for entry in our_anonymous:
        values.setdefault(entry["bytes"], []).append(entry)
        text = bytes.fromhex(entry["bytes"]).rstrip(b"\0")
        if text and all(32 <= c < 127 or c in (9, 10, 13) for c in text):
            strings.setdefault(text.decode("ascii", "replace"), []).append(entry)
    for name, sites in sorted(references.items()):
        info = symbols.get(name)
        if not info or info["section"] not in POOL_SECTIONS:
            continue
        size = info["size"] or 4
        address = info["addr"]
        linked = poolval.read_va(blob, dol_sections, address, max(size, 8)) or b""
        section = info["section"]
        own = target_sections.get(section)
        emitted = None
        if own is not None:
            run = next((r for r in runs if r[0] == unit and r[1] == section), None)
            if run and run[2] <= address < run[3]:
                start = address - run[2]
                emitted = own[start:start + size]
        source_bytes = emitted if emitted else linked[:size]
        matches = values.get(source_bytes.hex(), [])
        owner = owner_of(runs, section, address)
        value = decode_value(source_bytes if source_bytes else linked, size,
                             info.get("data"))
        # A whole-run byte comparison FAILS by construction when dtk merged
        # several strings under one `lbl_`: our object emits each string as
        # its own entry with its own padding. Compare the SEGMENTS, which is
        # what a reference at that offset actually names.
        segments = []
        for text in value.get("string_run", []):
            segments.append({"segment": text, "matches": [
                {"name": m["name"], "section": m["section"], "offset": m["offset"]}
                for m in strings.get(text, [])]})
        rows.append({
            "name": name, "address": f"0x{address:08X}", "section": section,
            "owner": owner, "owned_by_this_unit": owner == unit,
            "value": value,
            "our_string_segment_matches": segments,
            "our_string_segments_matched": sum(1 for s in segments if s["matches"]),
            "bytes_source": ("target object" if emitted else
                             "retail DOL" if linked else "UNAVAILABLE"),
            "declared_extern_in_source": bool(re.search(
                r"\bextern\b[^;\n]*\b" + re.escape(name) + r"\b", source_text)),
            "named_in_source": name in source_text,
            "our_equal_value_entries": [
                {"name": m["name"], "section": m["section"], "offset": m["offset"]}
                for m in matches],
            "our_equal_value_count": len(matches),
            "reference_sites": sites,
            "reference_count": len(sites),
            "first_reference_text_offset": sites[0]["text_offset"],
            # A `.sdata` datum is a WRITABLE global, not a literal: it may
            # never be literalized, and it is not laid out by first use.
            "disposition": "POOL" if section in (".sdata2", ".rodata") else "DATA",
            # dtk names an unnamed datum `lbl_<address>`. A datum with a real
            # name is a DECLARED object emitted in declaration order, so it
            # is outside the compiler's first-use pool model.
            "compiler_generated_name": bool(re.fullmatch(r"lbl_[0-9A-Fa-f]{8}", name)),
        })
    return rows


def predict_first_use(rows, unit):
    """First-use order against address order, for this unit's OWN datums.

    MWCC emits a pool entry when the code first needs it, so for the datums
    a TU owns the two orders should agree. The first disagreement is where
    an emission-order or ownership question starts; it is not a verdict.
    """
    own = [r for r in rows if r["owned_by_this_unit"]]
    # DECLARED objects and writable globals are outside the model: a named
    # global is emitted in DECLARATION order and a `.sdata` datum is not a
    # literal at all. Including them made ml_mem's own report "DISAGREE" on
    # `mlmGameSubdirectory`, which is not a pool-order fact about anything.
    modelled = [r for r in own
                if r["compiler_generated_name"] and r["disposition"] == "POOL"]
    excluded = [r["name"] for r in own if r not in modelled]
    by_use = sorted(modelled, key=lambda r: r["first_reference_text_offset"])
    by_address = sorted(modelled, key=lambda r: int(r["address"], 16))
    predicted = [r["name"] for r in by_use]
    actual = [r["name"] for r in by_address]
    disagreement = next((i for i, (a, b) in enumerate(zip(predicted, actual))
                         if a != b), None)
    return {"referenced_own_datums": len(own),
            "applicable": bool(modelled),
            "modelled_generated_literals": len(modelled),
            "excluded_named_or_writable": excluded,
            "predicted_first_use_order": predicted,
            "actual_address_order": actual,
            "orders_agree": bool(modelled) and predicted == actual,
            "first_disagreement_index": disagreement,
            "first_disagreement": None if disagreement is None else
            {"predicted": predicted[disagreement], "actual": actual[disagreement],
             # The address-order datum sitting earlier than any read we can
             # see means an EARLIER reference exists that relocations do not
             # show: inlined into a caller, computed, or simply not
             # reconstructed yet. That is the lead, not a defect verdict.
             "implied_missing_early_reference": bool(
                 by_address[disagreement]["first_reference_text_offset"]
                 > by_use[disagreement]["first_reference_text_offset"]),
             "actual_first_read_text_offset":
                 by_address[disagreement]["first_reference_text_offset"],
             "actual_first_read_function":
                 by_address[disagreement]["reference_sites"][0]["function"],
             "predicted_first_read_text_offset":
                 by_use[disagreement]["first_reference_text_offset"]},
            "basis": "MWCC emits a compiler-generated pool entry at first use; "
                     "declared globals and writable .sdata are excluded. A "
                     "disagreement is a question about emission order or "
                     "ownership, not a verdict"}


def analyze(unit, symbols=None, runs=None):
    symbols = symbols if symbols is not None else poolval.load_symbols()
    runs = runs if runs is not None else load_splits()
    if not symbols:
        raise Refused("symbols.txt yielded no symbols")
    if not any(r[0] == unit for r in runs):
        raise Refused(f"{unit} has no entry in splits.txt")
    target = REPO / "build" / VERSION / "obj" / (unit + ".o")
    if not target.exists():
        raise Refused(f"missing target object {target}; run `ninja` or "
                      "`python tools/gdl/provision_worktree.py --resplit`")
    raw = REPO / "build" / VERSION / "src" / unit
    raw = raw.parent / ".postprocess" / "body" / (raw.name + ".o")
    ours = raw if raw.exists() else REPO / "build" / VERSION / "src" / (unit + ".o")
    source = next((REPO / f"src/{unit}{ext}" for ext in (".c", ".cpp")
                   if (REPO / f"src/{unit}{ext}").exists()), None)
    try:
        references = _function_references(target)
        target_elf = Elf(str(target))
        target_sections = {
            target_elf.names[i]: bytes(target_elf.data[h[4]:h[4] + h[5]])
            for i, h in enumerate(target_elf.sh)
            if target_elf.names[i] in POOL_SECTIONS and h[1] != 8}
        our_anonymous, our_named = _elf_pool(ours) if ours.exists() else ([], [])
    except (OSError, ValueError, IndexError, KeyError) as error:
        raise Refused(f"could not read an object: {error}")
    source_text = source.read_text(encoding="utf-8", errors="replace") if source else ""
    rows = pool_datums(unit, symbols, runs, references, target_sections,
                       poolval.load_dol(), our_anonymous, source_text)
    claimed = [{"section": section, "start": f"0x{start:08X}", "end": f"0x{end:08X}",
                "size": end - start,
                "named_datums_in_symbols": sum(
                    1 for info in symbols.values()
                    if info["section"] == section and start <= info["addr"] < end),
                "target_section_bytes": len(target_sections.get(section, b""))}
               for u, section, start, end in runs
               if u == unit and section in POOL_SECTIONS]
    labels = _asm_labels(unit)
    return {
        "schema_version": 1, "tool": "tools/gdl/pool_owner.py", "unit": unit,
        "source": str(source.relative_to(REPO)).replace("\\", "/") if source else None,
        "target_object": str(target.relative_to(REPO)).replace("\\", "/"),
        "our_object": str(ours.relative_to(REPO)).replace("\\", "/") if ours.exists() else None,
        "claimed_pool_extent": claimed,
        "claims_no_pool_run": not claimed,
        "split_asm_labels": len(labels),
        "split_asm_first_labels": [row[3] for row in labels[:8]],
        "our_anonymous_pool_entries": len(our_anonymous),
        "our_named_pool_entries": len(our_named),
        "referenced_pool_datums": len(rows),
        "referenced_unclaimed": sum(1 for r in rows if r["owner"] == "UNCLAIMED"),
        "referenced_foreign": sorted({r["owner"] for r in rows
                                      if r["owner"] not in (unit, "UNCLAIMED")}),
        "datums": rows,
        "first_use_prediction": predict_first_use(rows, unit),
        "limitations": [
            "Reference sites come from relocations; a computed address is invisible.",
            "Equal bytes are a value match, never proof of one original object.",
            "Claiming an UNCLAIMED run is a split-ownership change needing review.",
        ],
    }


def format_report(result, limit=None):
    out = [f"POOL OWNERSHIP {result['unit']}  (target {result['target_object']})"]
    if result["claims_no_pool_run"]:
        out.append("  CLAIMED POOL EXTENT: none -- this unit claims no "
                   ".sdata2/.rodata/.sdata run in splits.txt")
    else:
        out.append("  CLAIMED POOL EXTENT:")
        for row in result["claimed_pool_extent"]:
            out.append(f"    {row['section']:8} {row['start']}..{row['end']}"
                       f"  {row['size']} bytes, {row['named_datums_in_symbols']}"
                       f" named datums, {row['target_section_bytes']} bytes in"
                       " the target object")
    out.append(f"  our object: {result['our_anonymous_pool_entries']} anonymous"
               f" + {result['our_named_pool_entries']} named pool entries;"
               f" split asm lists {result['split_asm_labels']} labels")
    if not result["datums"]:
        out.append("  REFERENCED POOL DATUMS: none -- this unit's target text "
                   "references no .sdata2/.rodata/.sdata datum")
    else:
        out.append(f"  REFERENCED POOL DATUMS: {result['referenced_pool_datums']}"
                   f" ({result['referenced_unclaimed']} UNCLAIMED; foreign owners:"
                   f" {', '.join(result['referenced_foreign']) or 'none'})")
        out.append(f"    {'datum':<20}{'address':<12}{'sect':<9}{'owner':<28}"
                   f"{'refs':<6}{'ours':<6}{'kind':<5}value")
        for row in result["datums"][:limit] if limit else result["datums"]:
            value = row["value"]
            preferred = value["preferred"]
            if preferred == "string_run":
                shown = (f"string run of {len(value['string_run'])}: "
                         + repr(value["string_run"][0])[:40])
            elif preferred == "string":
                shown = repr(value["string"])[:44]
            elif preferred in value:
                shown = f"{preferred} {value[preferred]!r}"
            else:
                shown = "bytes " + value["bytes"][:16]
            site = row["reference_sites"][0]
            out.append(f"    {row['name']:<20}{row['address']:<12}"
                       f"{row['section']:<9}{row['owner']:<28}"
                       f"{row['reference_count']:<6}"
                       f"{row['our_equal_value_count']:<6}"
                       f"{row['disposition']:<5}{shown}")
            out.append(f"      first read by {site['function']}+0x{site['offset']:x}"
                       + ("" if not row["our_equal_value_entries"] else
                          "; our equal-value entries: "
                          + ", ".join(f"{m['name']}@{m['section']}+0x{m['offset']:x}"
                                      for m in row["our_equal_value_entries"][:4]))
                       + ("" if not row["our_string_segment_matches"] else
                          f"; {row['our_string_segments_matched']} of"
                          f" {len(row['our_string_segment_matches'])} merged"
                          " string segments matched separately in our object")
                       + ("; declared extern in this source"
                          if row["declared_extern_in_source"] else ""))
        if limit and len(result["datums"]) > limit:
            out.append(f"    ... {len(result['datums']) - limit} more"
                       " (use --json or --limit 0 for all)")
    prediction = result["first_use_prediction"]
    if not prediction["modelled_generated_literals"]:
        # "0 of 0 datums agree" is not a finding; say there was nothing to
        # predict, so a reader cannot mistake it for a clean pool.
        out.append("  FIRST-USE ORDER: NOT APPLICABLE -- this unit owns no "
                   "referenced compiler-generated pool datum"
                   + (f" ({prediction['referenced_own_datums']} own datums are"
                      " declared globals or writable)"
                      if prediction["referenced_own_datums"] else ""))
        out.append("  LIMITS: " + " ".join(result["limitations"]))
        return "\n".join(out)
    out.append(f"  FIRST-USE ORDER over {prediction['modelled_generated_literals']}"
               f" of {prediction['referenced_own_datums']} own referenced"
               f" datums ({len(prediction['excluded_named_or_writable'])}"
               " declared/writable excluded): "
               + ("agrees with the address order"
                  if prediction["orders_agree"] else
                  f"DISAGREES at index {prediction['first_disagreement_index']}"
                  f" (first use {prediction['first_disagreement']['predicted']},"
                  f" address order {prediction['first_disagreement']['actual']})"))
    if prediction["first_disagreement"] and prediction["first_disagreement"][
            "implied_missing_early_reference"]:
        row = prediction["first_disagreement"]
        out.append(f"    {row['actual']} sits earlier in the pool than any read"
                   f" this tool can see (its first relocation-visible read is"
                   f" {row['actual_first_read_function']} at .text"
                   f" +0x{row['actual_first_read_text_offset']:x}, after"
                   f" +0x{row['predicted_first_read_text_offset']:x}): an"
                   " earlier reference is implied -- inlined, computed, or not"
                   " reconstructed yet.")
    out.append("  LIMITS: " + " ".join(result["limitations"]))
    return "\n".join(out)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("unit", help="unit key, e.g. game/sys/ml_mem")
    parser.add_argument("--limit", type=int, default=25,
                        help="datum rows to print (0 = all; default 25)")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--out", type=Path, help="write the JSON report here")
    args = parser.parse_args(argv)
    if args.out is not None and not args.out.resolve().is_relative_to((REPO / "build").resolve()):
        parser.error("--out must be under this checkout's build/")
    try:
        result = analyze(args.unit)
    except Refused as error:
        print(f"REFUSED: {error}", file=sys.stderr)
        return 2
    if args.out is not None:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2) if args.json
          else format_report(result, args.limit or None))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
