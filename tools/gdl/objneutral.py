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

FRESHNESS. A stale object is the one input that turns this tool into a liar:
comparing yesterday's compile against a bank of yesterday's compile answers
NEUTRAL for an edit that was never built. Both `bank` and `check` therefore
refuse (exit 2) when the object is out of date. The compiler is run with
`-MMD` and the build declares `deps = gcc`, so Ninja CONSUMES each `.d` file
into `.ninja_deps` and no depfile survives next to the object; the recorded
dependency list is read back with `ninja -t deps <object>`, and any input
whose mtime is newer than the object is named in the refusal. `ninja -n
<object>` is consulted as well and is authoritative -- note that it exits 0
whether or not work is pending, so the verdict is its text ("no work to do"),
never its exit code. When Ninja cannot be run at all the report says
FRESHNESS UNVERIFIED and names why; silence would be indistinguishable from a
verified fresh object.

PROVENANCE. `bank` records the HEAD commit, its tree id and the working-tree
status fingerprint. `check` WARNS when HEAD has moved since the bank: a merge,
rebase or branch switch can replace the source under a bank without touching
its object, and three lanes have banked across one. The warning is not a
refusal -- banking before a deliberate merge is legitimate -- but it is
printed above the verdict and carried in the JSON.

VERDICTS, per function and for the unit:
  IDENTICAL   same bytes, same relocation tuples
  RENUMBERED  same bytes; relocations differ ONLY by anonymous pool numbers
  CHANGED     anything else, with the first differing byte and the tuples
  ADDED / REMOVED   the function exists on only one side

EVERY CHANGED, ADDED and REMOVED function is printed, always. `--limit` caps
only the per-function detail lines (relocation rows) and the neutral
RENUMBERED listing; `--limit 0` means no cap anywhere. Hiding a changed
function behind a default cap, and crashing on the flag that would have shown
it, is how a not-neutral edit reads as a short clean report.

Exit 0 when every function is IDENTICAL or RENUMBERED, 1 when any is not,
2 when the comparison could not be made (no bank, unreadable object, stale
object).

IMPORTABLE CORE: read_object, function_rows, is_anonymous, compare_relocs,
compare_functions, verdict_of, parse_ninja_deps, dry_run_verdict,
newer_inputs, freshness_verdict, head_warnings -- pure over bytes, parsed
rows and captured command output; no build and no printing.
"""
import argparse
import hashlib
import json
import re
import shutil
import struct
import subprocess
import sys
import time
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
# Freshness: is the object we are about to compare the CURRENT compile?
#
# Pure functions over captured command output, so the parsing and the verdict
# are testable without a build; `freshness()` below is the one impure wrapper.

#: `ninja -t deps` header line: "<output>: #deps N, deps mtime M (VALID)".
_DEPS_HEADER = re.compile(r"^(?P<target>\S.*?): #deps (?P<count>\d+),"
                          r".*?\((?P<state>VALID|STALE)\)\s*$")
#: `ninja -n` says exactly this, on stdout, when the target needs no work.
_NO_WORK = "no work to do"


def _same_path(a, b):
    return str(a).replace("\\", "/").lower() == str(b).replace("\\", "/").lower()


def parse_ninja_deps(text, target):
    """(inputs, deps_state) that Ninja recorded for `target`.

    `deps = gcc` means Ninja swallowed the compiler's `-MMD` depfile: this is
    that depfile, read back out of `.ninja_deps`. Returns ([], None) when the
    target has no recorded entry -- an unbuilt or unknown output -- which is
    NOT the same as "no dependencies" and is reported as unverified upstream.
    """
    inputs, state, inside = [], None, False
    for line in str(text).splitlines():
        header = _DEPS_HEADER.match(line.strip()) if line[:1].strip() else None
        if header:
            inside = _same_path(header.group("target"), target)
            if inside:
                state = header.group("state")
            continue
        if inside and line.startswith("    ") and line.strip():
            inputs.append(line.strip())
    return inputs, state


def dry_run_verdict(stdout, returncode=0):
    """'up-to-date' | 'out-of-date' | 'unknown' from `ninja -n <target>`.

    Ninja exits 0 whether or not it printed pending work, so the exit code is
    not the discriminant; a NONZERO exit only means the question failed.
    """
    if returncode != 0:
        return "unknown"
    return "up-to-date" if _NO_WORK in str(stdout).lower() else "out-of-date"


def newer_inputs(object_mtime, inputs, root=None):
    """[{'path', 'mtime'}] for recorded inputs newer than the object.

    Paths come back from Ninja either repo-relative or absolute; both are
    resolved against `root`. An input that cannot be stat'ed is skipped here
    and does not fabricate staleness -- `ninja -n` still answers for it.
    """
    root = Path(root if root is not None else REPO)
    rows = []
    for name in inputs:
        path = Path(name)
        if not path.is_absolute():
            path = root / path
        try:
            mtime = path.stat().st_mtime
        except OSError:
            continue
        if mtime > object_mtime:
            rows.append({"path": str(name).replace("\\", "/"), "mtime": mtime})
    return sorted(rows, key=lambda row: -row["mtime"])


def freshness_verdict(dry_run, newer, deps_state=None, deps_recorded=True):
    """{'state', 'stale', 'reason'} from the two independent signals.

    'stale' is asserted only on positive evidence: Ninja says work is pending,
    or a recorded input is newer than the object. Everything else that stops
    the question from being answered is 'unverified', reported as such, and
    never silently treated as fresh.
    """
    if dry_run == "out-of-date":
        return {"state": "STALE", "stale": True,
                "reason": "ninja -n reports pending work for this object"}
    if newer:
        return {"state": "STALE", "stale": True,
                "reason": "%d recorded dependency input(s) are newer than the"
                          " object" % len(newer)}
    if dry_run == "unknown":
        return {"state": "UNVERIFIED", "stale": False,
                "reason": "ninja -n could not answer for this object"}
    if not deps_recorded:
        return {"state": "UNVERIFIED", "stale": False,
                "reason": "ninja has no recorded dependency list for this"
                          " object; only the dry run was consulted"}
    if deps_state == "STALE":
        return {"state": "UNVERIFIED", "stale": False,
                "reason": "ninja marks its own recorded dependency list STALE"}
    return {"state": "FRESH", "stale": False,
            "reason": "ninja -n has no work to do and no recorded input is"
                      " newer than the object"}


def _run(args, root=None):
    """CompletedProcess, or None when the program could not be launched."""
    try:
        return subprocess.run(args, cwd=str(root if root is not None else REPO),
                              capture_output=True, text=True)
    except OSError:
        return None


def freshness(object_path, root=None, ninja="ninja"):
    """Ask Ninja whether `object_path` is the current compile. Impure."""
    root = Path(root if root is not None else REPO)
    path = Path(object_path)
    try:
        target = path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return {"checked": False, "state": "UNVERIFIED", "stale": False,
                "target": str(path),
                "reason": "object is outside this checkout; ninja cannot be"
                          " asked about it",
                "newer_inputs": [], "dry_run": "unknown", "inputs": 0}
    try:
        object_mtime = path.stat().st_mtime
    except OSError as error:
        return {"checked": False, "state": "UNVERIFIED", "stale": False,
                "target": target, "reason": "cannot stat the object: %s" % error,
                "newer_inputs": [], "dry_run": "unknown", "inputs": 0}
    deps = _run([ninja, "-t", "deps", target], root)
    dry = _run([ninja, "-n", target], root)
    if deps is None or dry is None:
        return {"checked": False, "state": "UNVERIFIED", "stale": False,
                "target": target,
                "reason": "ninja is not runnable from this checkout",
                "newer_inputs": [], "dry_run": "unknown", "inputs": 0}
    inputs, deps_state = ([], None) if deps.returncode != 0 else \
        parse_ninja_deps(deps.stdout, target)
    verdict = dry_run_verdict(dry.stdout, dry.returncode)
    newer = newer_inputs(object_mtime, inputs, root)
    result = freshness_verdict(verdict, newer, deps_state, bool(inputs))
    return {"checked": True, "state": result["state"], "stale": result["stale"],
            "reason": result["reason"], "target": target,
            "newer_inputs": newer, "dry_run": verdict, "inputs": len(inputs),
            "deps_state": deps_state,
            "ninja_message": " ".join(dry.stdout.split())[:200]}


def require_fresh(unit, object_path, root=None, ninja="ninja"):
    """The freshness dict, refusing (Unavailable) on positive staleness."""
    state = freshness(object_path, root, ninja)
    if state["stale"]:
        named = ", ".join(row["path"] for row in state["newer_inputs"][:6])
        raise Unavailable(
            "%s is STALE: %s. Newer than the object: %s. Run `ninja` before"
            " banking or checking %s; a comparison against a stale object"
            " reports the previous compile as NEUTRAL."
            % (state["target"], state["reason"],
               named or "(ninja named no specific input)", unit))
    return state


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


def _git(*args, root=None):
    done = _run(["git", *args], root)
    if done is None or done.returncode != 0:
        return None
    return done.stdout


def _head_commit(root=None):
    out = _git("rev-parse", "HEAD", root=root)
    return out.strip() if out else None


def git_state(root=None):
    """The bank's provenance stamp: which commit and which tree it was taken on.

    A bank is only meaningful against the source it was compiled from. HEAD
    moving under a bank (a merge, a rebase, a branch switch) replaces that
    source without touching the banked object, so the commit, its tree id and
    a fingerprint of the working-tree status are all recorded here and the
    commit is compared again on every check.
    """
    head = _head_commit(root)
    tree = _git("rev-parse", "HEAD^{tree}", root=root)
    status = _git("status", "--porcelain", root=root)
    lines = sorted(line.rstrip() for line in status.splitlines()
                   if line.strip()) if status is not None else None
    return {"head": head,
            "head_tree": tree.strip() if tree else None,
            "worktree_dirty": None if lines is None else bool(lines),
            "worktree_entries": None if lines is None else len(lines),
            "worktree_status_sha256": None if lines is None else
            hashlib.sha256("\n".join(lines).encode("utf-8")).hexdigest()}


def head_warnings(banked, current):
    """[str] provenance warnings comparing a bank's stamp to the current one.

    A moved HEAD is a WARNING, not a refusal: banking before a deliberate
    merge is legitimate work. A bank with no recorded commit at all also
    warns, because "no warning" must never mean "not checked".
    """
    warnings = []
    before, after = banked.get("head"), (current or {}).get("head")
    if not before:
        warnings.append(
            "this bank records no HEAD commit (taken by an older objneutral,"
            " or outside a git checkout): whether the source moved under it"
            " cannot be determined -- re-bank to get this check")
    elif after and before != after:
        warnings.append(
            "HEAD MOVED since this bank was taken: %s -> %s. A merge, rebase"
            " or branch switch replaces the source under a bank without"
            " touching its object; re-bank before trusting a NEUTRAL verdict."
            % (before[:12], after[:12]))
    elif not after:
        warnings.append("the current HEAD could not be read, so this bank's"
                        " commit could not be re-checked")
    if before and after and before == after:
        tree_before = banked.get("head_tree")
        tree_after = (current or {}).get("head_tree")
        if tree_before and tree_after and tree_before != tree_after:
            warnings.append(
                "same HEAD but a different tree id (%s -> %s): the commit was"
                " amended under this bank" % (tree_before[:12], tree_after[:12]))
    return warnings


def bank(unit, tag="default", raw=True, ninja="ninja"):
    path, note = current_object(unit, raw)
    if not Path(path).is_file():
        raise Unavailable(f"{path} does not exist; run ninja first")
    fresh = require_fresh(unit, path, ninja=ninja)
    obj = read_object(path)
    object_path, meta_path = bank_paths(unit, tag)
    BANK.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(path, object_path)
    source = next((REPO / f"src/{unit}{ext}" for ext in (".c", ".cpp")
                   if (REPO / f"src/{unit}{ext}").is_file()), None)
    meta = {"schema_version": 2, "unit": unit, "tag": tag,
            "object": str(Path(path).relative_to(REPO)).replace("\\", "/"),
            "object_note": note, "object_sha256": obj["sha256"],
            "functions": len(obj["functions"]),
            "text_bytes": len(obj["text"]),
            "relocations": len(obj["relocs"]),
            "banked_at": time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime()),
            "freshness": fresh,
            "source_sha256": (hashlib.sha256(source.read_bytes()).hexdigest()
                              if source else None)}
    meta.update(git_state())
    meta_path.write_text(json.dumps(meta, indent=1) + "\n", encoding="utf-8")
    return meta


def check(unit, tag="default", raw=True, ninja="ninja"):
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
    fresh = require_fresh(unit, path, ninja=ninja)
    current_git = git_state()
    warnings = head_warnings(meta, current_git)
    if not fresh["checked"] or fresh["state"] == "UNVERIFIED":
        warnings.append("FRESHNESS UNVERIFIED: %s. This verdict may describe"
                        " a previous compile." % fresh["reason"])
    before, after = read_object(object_path), read_object(path)
    rows = compare_functions(function_rows(before), function_rows(after))
    counts = {}
    for row in rows:
        counts[row["verdict"]] = counts.get(row["verdict"], 0) + 1
    neutral = all(row["verdict"] in ("IDENTICAL", "RENUMBERED")
                  for row in rows)
    return {"schema_version": 2, "tool": "tools/gdl/objneutral.py",
            "unit": unit, "tag": tag, "neutral": neutral,
            "banked": meta, "current_object": note,
            "current_sha256": after["sha256"],
            "object_identical": before["sha256"] == after["sha256"],
            "freshness": fresh, "git": current_git, "warnings": warnings,
            "counts": counts, "functions": rows,
            "limits": ["Body and positional relocations only: data, BSS,"
                       " exception records and section sizes are NOT"
                       " compared -- run datadiff.py --sections.",
                       "Neutrality against a bank is not a match against the"
                       " target; fndiff/probe still answer that question."]}


#: Verdicts that are never elided from the text report at any --limit.
NOT_NEUTRAL = ("CHANGED", "ADDED", "REMOVED")


def _detail_lines(row, limit):
    """The per-function detail listing -- the ONLY thing --limit caps."""
    lines = []
    renumbered = row.get("renumbered_relocations", [])
    changed = row.get("changed_relocations", [])
    for entry in (renumbered if limit is None else renumbered[:limit]):
        lines.append(f"        renumbered +0x{entry['offset']:x}:"
                     f" {entry['before']} -> {entry['after']}"
                     "  (anonymous pool entry; same slot, same addend)")
    if limit is not None and len(renumbered) > limit:
        lines.append("        ... %d more renumbered pool entr(y/ies)"
                     % (len(renumbered) - limit))
    for entry in (changed if limit is None else changed[:limit]):
        if "reason" in entry and "before" in entry \
                and not isinstance(entry["before"], list):
            lines.append(f"        {entry['reason']}:"
                         f" {entry['before']} -> {entry['after']}")
            continue
        lines.append(f"        CHANGED +0x{entry['offset']:x}:"
                     f" {entry['before'][2]}+{entry['before'][3]}"
                     f" -> {entry['after'][2]}+{entry['after'][3]}"
                     f"  ({entry['reason']})")
    if limit is not None and len(changed) > limit:
        lines.append("        ... %d more changed relocation(s) (use --json)"
                     % (len(changed) - limit))
    if "first_differing_byte" in row:
        lines.append(f"        first differing byte +0x"
                     f"{row['first_differing_byte']:x};"
                     f" {row['differing_words']} differing word(s);"
                     f" size {row['size_before']} -> {row['size_after']}")
    return lines


def format_check(result, limit=12):
    """The text report. `limit` (None = unlimited) caps DETAIL lines only.

    Every CHANGED/ADDED/REMOVED function is listed unconditionally: a report
    that elides the rows it exists to surface reads exactly like a clean one.
    Only the neutral RENUMBERED listing and each function's relocation detail
    are capped, and both say how many rows they withheld.
    """
    banked = result["banked"]
    fresh = result.get("freshness") or {}
    out = [f"OBJNEUTRAL {result['unit']} (tag {result['tag']})",
           f"  banked   {banked['object']}"
           f"  sha {banked['object_sha256'][:12]}"
           f"  head {str(banked.get('head'))[:12]}"
           f"  at {banked.get('banked_at') or 'unrecorded'}",
           f"  current  {result['current_object']}"
           f"  sha {result['current_sha256'][:12]}"
           f"  head {str((result.get('git') or {}).get('head'))[:12]}",
           f"  freshness {fresh.get('state', 'UNVERIFIED')}"
           f" -- {fresh.get('reason', 'not checked')}"]
    for warning in result.get("warnings", []):
        out.append("  WARNING: " + warning)
    out += ["  " + ("the whole object is byte-identical"
                    if result["object_identical"] else
                    "the object bytes differ; comparing per function"),
            "  " + ", ".join(f"{verdict} {count}" for verdict, count
                             in sorted(result["counts"].items()))]
    rows = result["functions"]
    for row in [r for r in rows if r["verdict"] in NOT_NEUTRAL]:
        out.append(f"    {row['verdict']:<10} {row['function']}")
        out += _detail_lines(row, limit)
    renumbered_rows = [r for r in rows if r["verdict"] == "RENUMBERED"]
    for row in (renumbered_rows if limit is None else renumbered_rows[:limit]):
        out.append(f"    {row['verdict']:<10} {row['function']}")
        out += _detail_lines(row, limit)
    if limit is not None and len(renumbered_rows) > limit:
        out.append("    ... %d more RENUMBERED function(s) (neutral; --limit 0"
                   " or --json for all)" % (len(renumbered_rows) - limit))
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
                        help="cap the per-function DETAIL lines and the"
                             " neutral RENUMBERED listing (0 = no cap)."
                             " CHANGED/ADDED/REMOVED functions are always"
                             " printed in full")
    parser.add_argument("--ninja", default="ninja",
                        help="ninja executable used for the freshness check")
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
    # --limit 0 means "no cap"; it must reach format_check as None and it
    # must not become None for a negative value either.
    limit = None if args.limit is not None and args.limit <= 0 else args.limit
    try:
        if args.action == "bank":
            meta = bank(unit, args.tag, raw=not args.no_raw, ninja=args.ninja)
            print(json.dumps(meta, indent=1) if args.json else
                  "BANKED %s (tag %s): %s, %d functions, %d text bytes,"
                  " %d relocations\n  from %s (%s)\n  head %s  tree %s"
                  "  freshness %s"
                  % (unit, args.tag, meta["object_sha256"][:12],
                     meta["functions"], meta["text_bytes"],
                     meta["relocations"], meta["object"], meta["object_note"],
                     str(meta.get("head"))[:12], str(meta.get("head_tree"))[:12],
                     meta["freshness"]["state"]))
            return 0
        result = check(unit, args.tag, raw=not args.no_raw, ninja=args.ninja)
    except Unavailable as error:
        print(f"OBJNEUTRAL REFUSED: {error}")
        return 2
    if args.out is not None:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(json.dumps(result, indent=1) + "\n",
                            encoding="utf-8")
    print(json.dumps(result, indent=1) if args.json
          else format_check(result, limit))
    return 0 if result["neutral"] else 1


if __name__ == "__main__":
    sys.exit(main())
