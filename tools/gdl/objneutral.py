#!/usr/bin/env python3
"""Is this edit NEUTRAL against a banked object? (not against the target)

    python tools/gdl/objneutral.py bank  game/enemy/critter
    ...edit, ninja...
    python tools/gdl/objneutral.py check game/enemy/critter
    python tools/gdl/objneutral.py list  [game/enemy/critter]

Every other comparator in this repository scores our object against the
TARGET: fndiff, probe, defake_gate, datadiff, wf_word_diff. That is the right
question for matching work and the WRONG one for a byte-identity campaign,
where the claim is "this source change emits the same object as before" and
the target's own residual is beside the point. A de-fakematch pass converting
a raw offset into a real struct member, a header edit gated across four
consumers, a literal recovery -- each needs to prove it changed NOTHING, and
"the fuzzy score did not move" is not that proof.

WHAT IS COMPARED, per function defined in .text:

  BODY BYTES            the function's own bytes, byte for byte.
  POSITIONAL RELOCATIONS  every relocation inside the function as
                        (offset-within-function, type, symbol, addend), in
                        order. Bodies can be identical while a relocation
                        names a different datum -- that is the wrong-pool
                        class fndiff documents, and it is not visible in the
                        bytes at all, because the unlinked word is zero.

WHAT COUNTS AS NEUTRAL. Two relocations at the SAME offset, of the SAME
type, with the SAME addend, whose symbols are BOTH anonymous compiler pool
entries (`@NN`), are POOL RENUMBERING: our compiler numbers its anonymous
entries per object, so an unrelated edit elsewhere in the TU shifts the
numbers without changing what any instruction reads. A row like that is
reported as RENUMBERED, not as a change. Everything else is a change,
including an anonymous symbol paired with a NAMED one -- that is a binding
moving between a compiler literal and a declared object, which is exactly
what a literal recovery does and exactly what must not slip through as
"renumbering".

WHAT IS NOT COMPARED: data, BSS, exception records, section sizes, symbol
tables. Body neutrality is necessary, not sufficient. Run
`datadiff.py --sections <unit>` for the data obligations and a full `ninja`
for the link.

VERDICTS, per function and for the unit:
  IDENTICAL   same bytes, same relocation tuples
  RENUMBERED  same bytes; relocations differ ONLY by anonymous pool numbers
  CHANGED     anything else, with the first differing byte and the tuples
  ADDED / REMOVED   the function exists on only one side

Exit 0 when every function is IDENTICAL or RENUMBERED, 1 when any is not,
2 when the comparison could not be made (no bank, unreadable object).

IMPORTABLE CORE: read_object, function_rows, is_anonymous, compare_relocs,
compare_functions, verdict_of -- pure over bytes and parsed rows; no build
and no printing.
"""
import argparse
import hashlib
import json
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
VERSION = "GUNE5D"
BANK = REPO / "build" / VERSION / "objneutral"
#: Our compiler names an unnamed pool entry `@NN`; the number is per object.
ANONYMOUS = re.compile(r"@\d+$")
STT_FUNC = 2


class Unavailable(RuntimeError):
    """A comparison that could not be made, never a silent PASS."""


def _sh_name(data, stroff, offset):
    end = data.index(b"\0", stroff + offset)
    return data[stroff + offset:end].decode("utf-8", "replace")


def read_object(path):
    """{'text': bytes, 'functions': [...], 'relocs': [...]} from an ELF.

    Deliberately a small local reader rather than an objdump call: this runs
    once per bank and once per check, and the comparison must not depend on
    disassembly text formatting.
    """
    path = Path(path)
    try:
        data = path.read_bytes()
    except OSError as error:
        raise Unavailable(f"cannot read {path}: {error}") from error
    if data[:4] != b"\x7fELF" or len(data) < 0x34:
        raise Unavailable(f"{path} is not an ELF object")
    try:
        shoff, shentsize, shnum, shstrndx = (
            struct.unpack_from(">I", data, 0x20)[0],
            struct.unpack_from(">H", data, 0x2E)[0],
            struct.unpack_from(">H", data, 0x30)[0],
            struct.unpack_from(">H", data, 0x32)[0])
        headers = [list(struct.unpack_from(">10I", data, shoff + i * shentsize))
                   for i in range(shnum)]
        stroff = headers[shstrndx][4]
        names = [_sh_name(data, stroff, h[0]) for h in headers]
        index = {name: i for i, name in enumerate(names)}
        if ".text" not in index or ".symtab" not in index:
            raise Unavailable(f"{path} has no .text/.symtab to compare")
        text_index = index[".text"]
        text = data[headers[text_index][4]:
                    headers[text_index][4] + headers[text_index][5]]
        symtab = headers[index[".symtab"]]
        symstr = headers[symtab[6]][4]
        symbols = []
        for i in range(symtab[5] // 16):
            name_off, value, size, info, _other, shndx = struct.unpack_from(
                ">3I2BH", data, symtab[4] + i * 16)
            symbols.append({"name": _sh_name(data, symstr, name_off),
                            "value": value, "size": size,
                            "type": info & 0xF, "shndx": shndx})
        relocs = []
        if ".rela.text" in index:
            header = headers[index[".rela.text"]]
            for k in range(header[5] // 12):
                offset, info, addend = struct.unpack_from(
                    ">3I", data, header[4] + k * 12)
                symbol = symbols[info >> 8]["name"] if (info >> 8) < len(symbols) \
                    else "<symbol %d>" % (info >> 8)
                relocs.append({"offset": offset, "type": info & 0xFF,
                               "symbol": symbol, "addend": addend})
    except (IndexError, struct.error, ValueError) as error:
        raise Unavailable(f"malformed object {path}: {error}") from error
    functions = sorted(
        ({"name": s["name"], "start": s["value"], "size": s["size"]}
         for s in symbols
         if s["type"] == STT_FUNC and s["shndx"] == text_index and s["name"]),
        key=lambda row: (row["start"], row["name"]))
    return {"path": str(path), "text": text, "functions": functions,
            "relocs": sorted(relocs, key=lambda r: r["offset"]),
            "sha256": hashlib.sha256(data).hexdigest()}


def function_rows(obj):
    """{name: {'bytes': ..., 'relocs': [(offset, type, symbol, addend)]}}.

    A zero-sized function symbol keeps an entry with empty bytes rather than
    disappearing: an empty comparison must be visible, not absent.
    """
    rows = {}
    for row in obj["functions"]:
        start, size = row["start"], row["start"] + row["size"]
        rows[row["name"]] = {
            "start": row["start"], "size": row["size"],
            "bytes": bytes(obj["text"][start:size]),
            "relocs": [(r["offset"] - row["start"], r["type"], r["symbol"],
                        r["addend"])
                       for r in obj["relocs"]
                       if row["start"] <= r["offset"] < size]}
    return rows


def is_anonymous(symbol):
    """Is this an anonymous compiler pool entry whose NUMBER is per object?"""
    return bool(ANONYMOUS.fullmatch(str(symbol).strip()))


def compare_relocs(before, after):
    """(changed, renumbered) between two positional relocation lists.

    A pair is RENUMBERING only when offset, type and addend all agree and
    BOTH symbols are anonymous. An anonymous-versus-named pair is a change:
    that is a binding moving between a compiler literal and a declared
    object, which is the thing a literal recovery does on purpose.
    """
    changed, renumbered = [], []
    if len(before) != len(after):
        return [{"reason": "relocation COUNT differs",
                 "before": len(before), "after": len(after)}], []
    for old, new in zip(before, after):
        if old == new:
            continue
        same_slot = (old[0], old[1], old[3]) == (new[0], new[1], new[3])
        if same_slot and is_anonymous(old[2]) and is_anonymous(new[2]):
            renumbered.append({"offset": old[0], "before": old[2],
                               "after": new[2]})
            continue
        changed.append({"offset": old[0], "before": list(old),
                        "after": list(new),
                        "reason": ("same slot, but a named symbol is involved"
                                   if same_slot else
                                   "offset, type or addend differs")})
    return changed, renumbered


def verdict_of(body_equal, changed, renumbered):
    if not body_equal:
        return "CHANGED"
    if changed:
        return "CHANGED"
    return "RENUMBERED" if renumbered else "IDENTICAL"


def _first_difference(before, after):
    for index in range(min(len(before), len(after))):
        if before[index] != after[index]:
            return index
    return min(len(before), len(after)) if len(before) != len(after) else None


def compare_functions(before, after):
    """[row] per function, plus the ADDED/REMOVED ones. Pure."""
    rows = []
    for name in sorted(set(before) | set(after)):
        old, new = before.get(name), after.get(name)
        if old is None or new is None:
            rows.append({"function": name,
                         "verdict": "ADDED" if old is None else "REMOVED",
                         "size": (new or old)["size"]})
            continue
        body_equal = old["bytes"] == new["bytes"]
        changed, renumbered = compare_relocs(old["relocs"], new["relocs"])
        row = {"function": name,
               "verdict": verdict_of(body_equal, changed, renumbered),
               "size_before": old["size"], "size_after": new["size"],
               "renumbered_relocations": renumbered,
               "changed_relocations": changed}
        if not body_equal:
            offset = _first_difference(old["bytes"], new["bytes"])
            row["first_differing_byte"] = offset
            row["differing_words"] = sum(
                1 for i in range(0, min(len(old["bytes"]), len(new["bytes"])), 4)
                if old["bytes"][i:i + 4] != new["bytes"][i:i + 4])
        rows.append(row)
    return rows


# --------------------------------------------------------------------------
# Bank management (the only part that touches the filesystem)


def _safe(unit):
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", str(unit).strip("/"))


def bank_paths(unit, tag="default"):
    stem = f"{_safe(unit)}__{_safe(tag)}"
    return BANK / (stem + ".o"), BANK / (stem + ".json")


def current_object(unit, raw=True):
    """(path, note) for the object to compare, preferring the validated graph."""
    plain = REPO / "build" / VERSION / "src" / (unit + ".o")
    if not raw:
        return plain, "build path (no graph validation requested)"
    try:
        try:
            from .raw_object import RawObjectError, resolve_object
        except ImportError:
            from raw_object import RawObjectError, resolve_object
        selected = resolve_object(unit, root=REPO, version=VERSION)
        return Path(selected.path), selected.description
    except RawObjectError as error:
        # Named, never swallowed: a graph refusal is information about the
        # build, and falling back silently would compare an object nothing
        # validated.
        if not plain.is_file():
            raise Unavailable(f"no object for {unit}: {error}")
        return plain, f"UNVALIDATED build path (graph refused: {error})"
    except Exception as error:  # noqa: BLE001 - report, never guess
        raise Unavailable(f"could not resolve {unit}: {error}") from error


def _head_commit():
    try:
        done = subprocess.run(["git", "rev-parse", "HEAD"], cwd=str(REPO),
                              capture_output=True, text=True)
        return done.stdout.strip() if done.returncode == 0 else None
    except OSError:
        return None


def bank(unit, tag="default", raw=True):
    path, note = current_object(unit, raw)
    if not Path(path).is_file():
        raise Unavailable(f"{path} does not exist; run ninja first")
    obj = read_object(path)
    object_path, meta_path = bank_paths(unit, tag)
    BANK.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(path, object_path)
    source = next((REPO / f"src/{unit}{ext}" for ext in (".c", ".cpp")
                   if (REPO / f"src/{unit}{ext}").is_file()), None)
    meta = {"schema_version": 1, "unit": unit, "tag": tag,
            "object": str(Path(path).relative_to(REPO)).replace("\\", "/"),
            "object_note": note, "object_sha256": obj["sha256"],
            "functions": len(obj["functions"]),
            "text_bytes": len(obj["text"]),
            "relocations": len(obj["relocs"]),
            "head": _head_commit(),
            "source_sha256": (hashlib.sha256(source.read_bytes()).hexdigest()
                              if source else None)}
    meta_path.write_text(json.dumps(meta, indent=1) + "\n", encoding="utf-8")
    return meta


def check(unit, tag="default", raw=True):
    object_path, meta_path = bank_paths(unit, tag)
    if not object_path.is_file() or not meta_path.is_file():
        raise Unavailable(
            f"no banked object for {unit} (tag {tag}). Bank one BEFORE the"
            f" edit:\n    python tools/gdl/objneutral.py bank {unit}"
            + (f" --tag {tag}" if tag != "default" else ""))
    meta = json.loads(meta_path.read_text(encoding="utf-8"))
    if meta.get("unit") != unit:
        raise Unavailable(f"banked object {object_path} belongs to"
                          f" {meta.get('unit')!r}, not {unit!r}")
    path, note = current_object(unit, raw)
    before, after = read_object(object_path), read_object(path)
    rows = compare_functions(function_rows(before), function_rows(after))
    counts = {}
    for row in rows:
        counts[row["verdict"]] = counts.get(row["verdict"], 0) + 1
    neutral = all(row["verdict"] in ("IDENTICAL", "RENUMBERED")
                  for row in rows)
    return {"schema_version": 1, "tool": "tools/gdl/objneutral.py",
            "unit": unit, "tag": tag, "neutral": neutral,
            "banked": meta, "current_object": note,
            "current_sha256": after["sha256"],
            "object_identical": before["sha256"] == after["sha256"],
            "counts": counts, "functions": rows,
            "limits": ["Body and positional relocations only: data, BSS,"
                       " exception records and section sizes are NOT"
                       " compared -- run datadiff.py --sections.",
                       "Neutrality against a bank is not a match against the"
                       " target; fndiff/probe still answer that question."]}


def format_check(result, limit=12):
    out = [f"OBJNEUTRAL {result['unit']} (tag {result['tag']})",
           f"  banked   {result['banked']['object']}"
           f"  sha {result['banked']['object_sha256'][:12]}"
           f"  head {str(result['banked'].get('head'))[:12]}",
           f"  current  {result['current_object']}"
           f"  sha {result['current_sha256'][:12]}",
           "  " + ("the whole object is byte-identical"
                   if result["object_identical"] else
                   "the object bytes differ; comparing per function"),
           "  " + ", ".join(f"{verdict} {count}" for verdict, count
                            in sorted(result["counts"].items()))]
    interesting = [row for row in result["functions"]
                   if row["verdict"] != "IDENTICAL"]
    for row in interesting[:limit]:
        out.append(f"    {row['verdict']:<10} {row['function']}")
        for entry in row.get("renumbered_relocations", [])[:4]:
            out.append(f"        renumbered +0x{entry['offset']:x}:"
                       f" {entry['before']} -> {entry['after']}"
                       "  (anonymous pool entry; same slot, same addend)")
        for entry in row.get("changed_relocations", [])[:4]:
            if "reason" in entry and "before" in entry \
                    and not isinstance(entry["before"], list):
                out.append(f"        {entry['reason']}:"
                           f" {entry['before']} -> {entry['after']}")
                continue
            out.append(f"        CHANGED +0x{entry['offset']:x}:"
                       f" {entry['before'][2]}+{entry['before'][3]}"
                       f" -> {entry['after'][2]}+{entry['after'][3]}"
                       f"  ({entry['reason']})")
        if "first_differing_byte" in row:
            out.append(f"        first differing byte +0x"
                       f"{row['first_differing_byte']:x};"
                       f" {row['differing_words']} differing word(s);"
                       f" size {row['size_before']} -> {row['size_after']}")
    if len(interesting) > limit:
        out.append(f"    ... {len(interesting) - limit} more non-identical"
                   " function(s) (use --json)")
    out.append("  VERDICT: " + ("NEUTRAL -- every function is identical or"
                                " pool-renumbered only"
                                if result["neutral"] else
                                "NOT NEUTRAL -- see the rows above"))
    out.append("  LIMITS: " + " ".join(result["limits"]))
    return "\n".join(out)


def banks():
    if not BANK.is_dir():
        return []
    rows = []
    for meta_path in sorted(BANK.glob("*.json")):
        try:
            rows.append(json.loads(meta_path.read_text(encoding="utf-8")))
        except (OSError, ValueError):
            rows.append({"unit": None, "tag": None,
                         "error": f"unreadable {meta_path.name}"})
    return rows


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__.split("\n\n")[0],
        formatter_class=argparse.RawDescriptionHelpFormatter, epilog=__doc__)
    parser.add_argument("action", choices=("bank", "check", "list"))
    parser.add_argument("unit", nargs="?", help="unit key, e.g. game/enemy/critter")
    parser.add_argument("--tag", default="default",
                        help="name this bank (default: default)")
    parser.add_argument("--no-raw", action="store_true",
                        help="compare build/<v>/src/<unit>.o without asking"
                             " raw_object to validate the build graph")
    parser.add_argument("--limit", type=int, default=12,
                        help="non-identical function rows to print (0 = all)")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--out", type=Path, help="write the JSON result here")
    args = parser.parse_args(argv)
    if args.out is not None and not args.out.resolve().is_relative_to(
            (REPO / "build").resolve()):
        parser.error("--out must be under this checkout's build/")
    if args.action == "list":
        rows = [row for row in banks()
                if args.unit is None or row.get("unit") == args.unit]
        if args.json:
            print(json.dumps(rows, indent=1))
        elif not rows:
            print("no banked objects under build/%s/objneutral" % VERSION)
        else:
            for row in rows:
                print("%-34s %-12s %s functions, head %s"
                      % (row.get("unit"), row.get("tag"),
                         row.get("functions"), str(row.get("head"))[:12]))
        return 0
    if not args.unit:
        parser.error("%s needs a unit" % args.action)
    unit = str(args.unit).replace("\\", "/").strip("/")
    unit = re.sub(r"\.(c|cpp)$", "", unit[4:] if unit.startswith("src/")
                  else unit)
    try:
        if args.action == "bank":
            meta = bank(unit, args.tag, raw=not args.no_raw)
            print(json.dumps(meta, indent=1) if args.json else
                  "BANKED %s (tag %s): %s, %d functions, %d text bytes,"
                  " %d relocations\n  from %s (%s)"
                  % (unit, args.tag, meta["object_sha256"][:12],
                     meta["functions"], meta["text_bytes"],
                     meta["relocations"], meta["object"], meta["object_note"]))
            return 0
        result = check(unit, args.tag, raw=not args.no_raw)
    except Unavailable as error:
        print(f"OBJNEUTRAL REFUSED: {error}")
        return 2
    if args.out is not None:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(result, indent=1) + "\n",
                            encoding="utf-8")
    print(json.dumps(result, indent=1) if args.json
          else format_check(result, args.limit or None))
    return 0 if result["neutral"] else 1


if __name__ == "__main__":
    sys.exit(main())
