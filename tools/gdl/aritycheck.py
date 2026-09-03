#!/usr/bin/env python3
"""The ARITY screen: definitions, call sites, and unread trailing parameters.

WHY THIS EXISTS. Two accepted laws describe the SAME source configuration —
a definition declaring N parameters while some call site passes fewer — and
they prescribe OPPOSITE fixes:

  claim.law.NM_an-unread-trailing-parameter-is-invisible-in-the-callee-and-
  costs-the-caller-an-instruction.20260903.v1
      A reconstructed prototype carrying one MORE trailing parameter than
      the retail function emits NO code in the callee (so the callee can
      measure byte-EXACT at either arity and its own bytes never settle the
      question) while every CALLER pays an instruction materialising the
      phantom argument. Dropping SetEnemyObj's 4th parameter took
      game/enemy/enemy::init_enemy from real 27 to real 10 in one build and
      then to STRICT exact.

  claim.law.knr-extern-arity-can-be-faithful-not-a-defect.20260831.v1
      A K&R `extern f();` beside a call passing fewer arguments than the
      definition takes is frequently FAITHFUL 2001 source, and "fixing" it
      regresses the caller (measured: init_attract_mode real 210 -> 211).

The discriminant between them is not the call site — it is whether the
callee ever READS the trailing parameter. That question is answerable from
source in zero builds and neither existing tool asks it: `externcheck.py`
ranks type-CLASS disagreements and `abicheck.py` models GPR/FPR sequence
assignment, so both are blind to parameter COUNT (AGENTS.md discipline 16,
and the NM record says so explicitly).

The NM record is also explicit that this class has NEVER BEEN COUNTED — its
spelling-based census (parameters literally named `unused`) found three
sites, all function-pointer callbacks, and it closes "treat the law as a
diagnostic PROCEDURE with one proven instance, not as a sized opportunity."
This tool is the census that sizing needs.

    python tools/gdl/aritycheck.py                     # ranked table
    python tools/gdl/aritycheck.py --function SetEnemyObj
    python tools/gdl/aritycheck.py --verdict PHANTOM-CANDIDATE
    python tools/gdl/aritycheck.py --json --out build/GUNE5D/arity.json

VERDICTS, strongest first:

  PHANTOM-CANDIDATE  a trailing parameter is never read in the body AND some
                     call site passes fewer arguments than the definition
                     declares. The NM class: the short call is the tree's
                     own evidence for the shorter arity, and the callee's
                     bytes cannot contradict it. Read the CALLER's aligned
                     view for an `ours-only -1 lwz|li|mr` beside the call.
  UNREAD-TRAILING    a trailing parameter is never read, every call passes
                     the full list. Still costs each caller the argument
                     setup, but nothing outside the definition argues for
                     the shorter arity, so it is a question, not a finding.
  KNR-SHORT-CALL     a call passes fewer arguments AND the parameter IS read
                     in the body. The KNR law owns this: the target's
                     argument-register setup at the call site decides, and
                     prototyping it is a codegen change, not a cleanup.

EVERY CALL SITE CARRIES ITS CALLER, A WEBFRANK-PIN MARKER, AND A SHORT/FULL
SPLIT (run-44 item 2, from AR). Two things the printed row could not say:

  * A PINNED CALLER CANNOT ANSWER THE QUESTION THIS TOOL ASKS. The trailer
    below sends the reader to the caller's aligned view, and fndiff scores
    the POSTPROCESSED object, so a webfrank-pinned caller reads `real` 0 by
    construction (AGENTS.md residual-work discipline 3). Three of AR's
    fifteen apparent refutations were pinned consumers — fn_800DA60C,
    fn_800DA6A4 and pbDiagDrawTexture — and taking those verdicts at face
    value would have closed three rows on no evidence at all. Re-measured
    at ca4074cb1: 150 pinned functions, 23 marked call-site labels, and two
    rows in which EVERY full call site is pinned.
  * WHO PAYS. The whole cost of a phantom trailing parameter is borne by
    callers that pass the FULL list, so `N call site(s)` alone cannot say
    whether a PHANTOM row has any payer at all — three of AR's four PHANTOM
    rows had short calls only. Rows now print `S SHORT + F FULL`, and a
    PHANTOM row with F = 0 is labelled NO PAYER.

THE CENSUS IS THIS COMMAND, NOT A NUMBER. `python tools/gdl/aritycheck.py`
prints the per-verdict tally on its second line; that line is what a record
or a work order quotes, with the commit it was run at. Three data points, all
from the same command: 4 PHANTOM-CANDIDATE / 15 KNR-SHORT-CALL / 42
UNREAD-TRAILING at run 42 (the calibration in tools/gdl/tests/
test_aritycheck.py), 1 / 14 / 42 at ca4074cb1 once the arity work those rows
produced landed, and 1 / 14 / 42 again at 0fd3bca5a — UNCHANGED across that
second interval. So the population neither reliably drifts nor reliably
holds, which is precisely why the number cannot be inherited from a
docstring: only a live run distinguishes "nothing moved" from "nobody
looked". The tests pin the two proven instances as FIXTURES, never the
tally, so a moving census can never fail the suite.

WHAT IT DOES NOT DO. It never edits, and a verdict is a place to look, not
a conclusion — both governing laws are settled against the TARGET BYTES at
the call site, never against the source shape this scan reads. Functions
whose address is taken are excluded outright: their arity is fixed by a
function-pointer type and no call site can argue with it (that exclusion
alone removes all three sites the NM record's spelling census found).
"""

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent

# Words that take a parenthesised list and are not calls.
KEYWORDS = frozenset((
    "if", "for", "while", "switch", "return", "sizeof", "do", "else",
    "case", "defined", "catch", "typedef", "struct", "union", "enum",
    "asm", "__attribute__", "static", "extern", "const", "volatile",
    "register", "unsigned", "signed", "inline", "new", "delete",
    "throw", "typeof", "offsetof", "va_start", "va_arg", "va_end",
))

_STRING_RE = re.compile(r"'(?:\\.|[^'\\])*'|\"(?:\\.|[^\"\\])*\"")
_BLOCK_COMMENT_RE = re.compile(r"/\*.*?\*/", re.S)
_LINE_COMMENT_RE = re.compile(r"//[^\n]*")
_CALLABLE_RE = re.compile(r"\b([A-Za-z_]\w*)\s*\(")
_IDENT_RE = re.compile(r"[A-Za-z_]\w*")


def strip_noise(text):
    """Comments blanked, string/char literals MASKED, offsets preserved.

    Offsets have to survive so a row can report a real line number, and a
    comment mentioning a parameter name must not count as a read — a stale
    comment documenting the phantom 4th argument as faithful is exactly what
    the init_enemy record had to delete.

    Literals become `~`, not spaces. Blanking them was a MEASURED defect:
    `strncmp(signature, "VBNK", 4)` left an argument that was entirely
    whitespace, the empty-token filter dropped it, and the call read as
    2 arguments against a 3-parameter definition — strncmp, strcmp, strchr
    and memcmp all rode into the census on that one bug. `~` is not an
    identifier character, so a literal still cannot spell a parameter name.
    """
    def blank(match):
        return re.sub(r"[^\n]", " ", match.group(0))

    def mask(match):
        return re.sub(r"[^\n]", "~", match.group(0))
    text = _BLOCK_COMMENT_RE.sub(blank, text)
    text = _LINE_COMMENT_RE.sub(blank, text)
    return _STRING_RE.sub(mask, text)


def preprocessor_lines(text):
    """1-based line numbers inside a `#...` directive, continuations too.

    A macro BODY calls its callee with the macro's own argument list, which
    has nothing to say about the callee's arity; counting those call sites
    put OSPanic in the census off two lines of `src/MSL/placeholder.h`.
    """
    inside, out = False, set()
    for number, line in enumerate(text.splitlines(), start=1):
        if inside or line.lstrip().startswith("#"):
            out.add(number)
            inside = line.rstrip().endswith("\\")
        else:
            inside = False
    return out


# Identifiers that can precede a call and are NOT a return type.
NOT_A_TYPE = frozenset((
    "return", "else", "do", "case", "goto", "new", "delete", "throw",
))


def previous_token(text, index):
    """The token immediately before `index`, skipping whitespace and `*`.

    This is the declaration/call discriminant. Classifying on the character
    AFTER the closing paren cannot work — `sndFxInit(0x8009, -1);` and
    `extern void sndFxInit();` both end in `;` — and treating every
    semicolon-terminated construct as a declaration made the tool's own
    control case invisible: all six of attract.c's sndFxInit CALL sites
    were filed as declarations and the function never reached the census
    (the KNR law's proven instance, missed by its own screen).
    """
    while index >= 0:
        while index >= 0 and (text[index].isspace() or text[index] == "*"):
            index -= 1
        if index < 0:
            return ""
        if text[index].isalnum() or text[index] == "_":
            end = index + 1
            while index >= 0 and (text[index].isalnum()
                                  or text[index] == "_"):
                index -= 1
            return text[index + 1:end]
        return text[index]
    return ""


def leads_a_declaration(text, index):
    """True when what precedes `index` reads as a RETURN TYPE."""
    token = previous_token(text, index)
    return (bool(token) and (token[0].isalpha() or token[0] == "_")
            and token not in NOT_A_TYPE)


def match_paren(text, open_index):
    """Index of the `)` closing the `(` at open_index, or None."""
    depth = 0
    for index in range(open_index, len(text)):
        char = text[index]
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
            if depth == 0:
                return index
    return None


def match_brace(text, open_index):
    """Index of the `}` closing the `{` at open_index, or None."""
    depth = 0
    for index in range(open_index, len(text)):
        char = text[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return index
    return None


def split_top_level(text):
    """Argument/parameter tokens, split on DEPTH-ZERO commas.

    A nested call or a function-pointer parameter carries its own commas,
    and splitting on every comma miscounts both — which would turn the
    census into noise on exactly the declarations most worth reading.
    """
    parts, depth, current = [], 0, []
    for char in text:
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
        if char == "," and depth == 0:
            parts.append("".join(current))
            current = []
            continue
        current.append(char)
    parts.append("".join(current))
    return [part.strip() for part in parts]


def argument_count(text):
    """How many arguments this parenthesised list holds (`void` == 0)."""
    parts = [part for part in split_top_level(text) if part]
    if not parts:
        return 0
    if len(parts) == 1 and re.fullmatch(r"void", parts[0].strip()):
        return 0
    return len(parts)


def parameter_name(token):
    """The declared NAME in one parameter token, or None.

    `s32 level` -> level; `char *p` -> p; `s32 v[4]` -> v;
    `void (*cb)(Enemy *)` -> cb; `s32` (unnamed) -> None.
    """
    token = token.strip()
    if not token or token == "void" or token == "...":
        return None
    pointer = re.search(r"\(\s*\*\s*([A-Za-z_]\w*)\s*\)", token)
    if pointer:
        return pointer.group(1)
    head = token.split("[")[0].strip()
    names = _IDENT_RE.findall(head)
    if len(names) < 2:
        # One identifier is a bare TYPE — `s32`, `Enemy*` — declaring an
        # UNNAMED parameter. An unnamed parameter is unread by construction,
        # so returning it as a name would manufacture the strongest verdict
        # this tool has out of a declaration that says nothing.
        return None
    return names[-1]


def is_knr_parameter_list(tokens):
    """True when every token is a bare identifier — a K&R definition head."""
    real = [token for token in tokens if token]
    return bool(real) and all(
        re.fullmatch(r"[A-Za-z_]\w*", token) for token in real)


def unit_of(path):
    """`src/game/pb/pb_diag.c` -> `game/pb/pb_diag`; None for a header.

    The webfrank config is keyed by unit, so a call site's file has to be
    reduced to one before its caller can be screened for a pin.
    """
    posix = str(path).replace("\\", "/")
    if not posix.startswith("src/"):
        return None
    if not posix.endswith((".c", ".cpp")):
        return None
    return re.sub(r"\.(c|cpp)$", "", posix[len("src/"):])


def load_webfrank_pins(root):
    """{(unit, function)} for every function carrying a webfrank rule.

    Run-44 item 2, from AR. The census's own decision procedure is "read
    the CALLER's aligned view for an ours-only -1 lwz|li|mr beside the
    call" — and that procedure is UNSOUND for a pinned caller, which reads
    `real` 0 by construction because fndiff scores the POSTPROCESSED object
    (AGENTS.md residual-work discipline 3). Three of AR's fifteen apparent
    refutations were pinned consumers (fn_800DA60C, fn_800DA6A4,
    pbDiagDrawTexture), and taking those verdicts at face value would have
    closed three rows on no evidence at all
    (claim.AR_probe-revert-restored-half-a-two-site-edit-and-aritycheck-
    needs-a-pin-column.20260903.v1).

    Reads the file the tool can already reach, and fails SOFT: a checkout
    with no webfrank.json gets an empty set and every site prints unmarked,
    which is the same answer it printed before this existed.
    """
    config = Path(root) / "config" / "GUNE5D" / "webfrank.json"
    try:
        data = json.loads(config.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return set()
    pins = set()
    for unit, rules in (data.get("units") or {}).items():
        if not isinstance(rules, list):
            continue
        for rule in rules:
            if isinstance(rule, dict) and rule.get("function"):
                pins.add((unit, rule["function"]))
    return pins


def attribute_callers(definitions, calls):
    """Fill each call's `caller` from the definition whose body contains it.

    A call at file scope (a static initializer) has no caller and keeps
    None — reporting one would be an invention, and the pin question has no
    answer there.
    """
    spans = [(d["span"][0], d["span"][1], d["name"]) for d in definitions
             if d.get("span")]
    for call in calls:
        index = call.get("index")
        if index is None:
            continue
        call["caller"] = next(
            (name for start, end, name in spans if start <= index < end),
            None)


def scan_file(path, text):
    """(definitions, declarations, calls, address_taken) for one file."""
    definitions, declarations, calls = [], [], []
    address_taken = set()
    referenced_with_call = set()
    directives = preprocessor_lines(text)
    for match in _CALLABLE_RE.finditer(text):
        name = match.group(1)
        open_index = match.end() - 1
        close_index = match_paren(text, open_index)
        if close_index is None:
            continue
        referenced_with_call.add(name)
        if name in KEYWORDS:
            continue
        inner = text[open_index + 1:close_index]
        rest = text[close_index + 1:close_index + 400]
        stripped = rest.lstrip()
        line = text.count("\n", 0, match.start()) + 1
        if stripped.startswith("{"):
            brace = close_index + 1 + (len(rest) - len(stripped))
            end = match_brace(text, brace)
            tokens = split_top_level(inner)
            definitions.append({
                "name": name, "line": line, "file": str(path),
                "params": tokens,
                "arity": argument_count(inner),
                "knr": is_knr_parameter_list(tokens),
                "body": text[brace:end + 1] if end else "",
                "span": (brace, end + 1) if end else None,
            })
        elif stripped.startswith(";") and leads_a_declaration(text,
                                                              match.start()
                                                              - 1):
            declarations.append({
                "name": name, "line": line, "file": str(path),
                "arity": argument_count(inner),
                "unprototyped": not inner.strip(),
            })
        elif line not in directives:
            calls.append({"name": name, "line": line, "file": str(path),
                          "arity": argument_count(inner),
                          "index": match.start(), "caller": None})
    attribute_callers(definitions, calls)
    for match in _IDENT_RE.finditer(text):
        name = match.group(0)
        after = text[match.end():match.end() + 40].lstrip()
        if after.startswith("("):
            continue
        if name in referenced_with_call:
            address_taken.add(name)
    return definitions, declarations, calls, address_taken


def collect(root, patterns=("src/**/*.c", "src/**/*.cpp",
                            "include/**/*.h", "src/**/*.h")):
    definitions, declarations, calls = [], [], []
    address_taken = set()
    for pattern in patterns:
        for path in sorted(Path(root).glob(pattern)):
            if not path.is_file():
                continue        # `src/Runtime.PPCEABI.H` is a DIRECTORY
            text = strip_noise(path.read_text(encoding="utf-8",
                                              errors="replace"))
            rel = path.relative_to(root).as_posix()
            d, p, c, a = scan_file(rel, text)
            definitions += d
            declarations += p
            calls += c
            address_taken |= a
    return definitions, declarations, calls, address_taken


def body_reads(body, name):
    """How many times `name` appears as an identifier in the body."""
    if not name:
        return 0
    return len(re.findall(rf"\b{re.escape(name)}\b", body))


def site_label(site, pins):
    """`file:line (N arg) in <caller>` plus the pin marker when it applies.

    The marker is the whole point of the column: a pinned caller reads
    `real` 0 by construction, so the aligned-view procedure this tool
    prints cannot decide the row from that site and must use the target
    half instead.
    """
    unit = unit_of(site["file"])
    caller = site.get("caller")
    where = f" in {caller}" if caller else " at file scope"
    pinned = bool(unit and caller and (unit, caller) in pins)
    marker = ("  [WEBFRANK-PINNED CALLER — this site reads real 0 by"
              " construction; decide from the TARGET half]" if pinned else "")
    return f"{site['file']}:{site['line']} ({site['arity']} arg){where}{marker}"


def is_pinned_site(site, pins):
    unit = unit_of(site["file"])
    caller = site.get("caller")
    return bool(unit and caller and (unit, caller) in pins)


def analyse(definitions, declarations, calls, address_taken, pins=frozenset()):
    """One row per definition that has an arity question worth reading."""
    by_name = {}
    for call in calls:
        by_name.setdefault(call["name"], []).append(call)
    decls_by_name = {}
    for declaration in declarations:
        decls_by_name.setdefault(declaration["name"], []).append(declaration)
    seen, rows = {}, []
    for definition in definitions:
        # A file-static helper can share a name across TUs; the first
        # definition wins and a duplicate is reported rather than merged.
        seen.setdefault(definition["name"], []).append(definition)
    for name, group in seen.items():
        definition = group[0]
        if definition["knr"] or definition["arity"] == 0:
            continue
        if definition["params"] and definition["params"][-1].strip() == "...":
            continue        # a varargs call site is SUPPOSED to vary
        if name in address_taken:
            continue
        sites = by_name.get(name, [])
        short = [site for site in sites if site["arity"] < definition["arity"]]
        trailing = parameter_name(definition["params"][-1])
        reads = body_reads(definition["body"], trailing) if trailing else None
        unread = trailing is not None and reads == 0
        knr_decls = [d for d in decls_by_name.get(name, [])
                     if d["unprototyped"]]
        if short and unread:
            verdict = "PHANTOM-CANDIDATE"
        elif short:
            verdict = "KNR-SHORT-CALL"
        elif unread and sites:
            # The whole cost of a phantom parameter is paid by CALLERS. A
            # definition with no call site in the tree pays nothing and has
            # nothing to be compared against (MSL's `__close_console`, whose
            # only caller is library-internal, arrived here that way).
            verdict = "UNREAD-TRAILING"
        else:
            continue
        # THE SHORT/FULL SPLIT (run-44 item 2). The printed row carried one
        # call-site COUNT, and the count cannot express the question that
        # decides a PHANTOM row: the whole cost of a phantom trailing
        # parameter is paid by callers that pass the FULL list, so a row
        # whose sites are ALL short has no payer and dropping the parameter
        # buys nothing. AR measured three of four PHANTOM rows that way.
        full = [site for site in sites if site["arity"] >= definition["arity"]]
        pinned_full = [site for site in full if is_pinned_site(site, pins)]
        pinned_sites = [site for site in sites if is_pinned_site(site, pins)]
        rows.append({
            "verdict": verdict,
            "function": name,
            "defined_at": f"{definition['file']}:{definition['line']}",
            "declared_arity": definition["arity"],
            "trailing_parameter": trailing,
            "trailing_reads_in_body": reads,
            "call_sites": len(sites),
            "short_call_site_count": len(short),
            "full_call_site_count": len(full),
            "pinned_full_call_site_count": len(pinned_full),
            "pinned_call_site_count": len(pinned_sites),
            # SHORT sites are listed individually as before (each carries
            # its own pin marker). Of the FULL sites only the PINNED ones
            # are listed and the rest merely counted: a 173-call-site row
            # would otherwise print 172 lines nobody reads, and the pinned
            # ones are exactly the sites where the printed decision
            # procedure does not work.
            "short_call_sites": [site_label(site, pins) for site in short],
            "pinned_full_call_sites": [site_label(site, pins)
                                       for site in pinned_full],
            "unprototyped_declarations": [f"{d['file']}:{d['line']}"
                                          for d in knr_decls],
            "duplicate_definitions": [f"{d['file']}:{d['line']}"
                                      for d in group[1:]],
        })
    order = {"PHANTOM-CANDIDATE": 0, "KNR-SHORT-CALL": 1,
             "UNREAD-TRAILING": 2}
    rows.sort(key=lambda row: (order[row["verdict"]],
                               -len(row["short_call_sites"]),
                               row["function"]))
    return rows


def format_rows(rows):
    lines = []
    counts = {}
    for row in rows:
        counts[row["verdict"]] = counts.get(row["verdict"], 0) + 1
    lines.append("== arity screen: definitions whose parameter COUNT is in"
                 " question (externcheck ranks type classes, abicheck models"
                 " register sequences; neither reads count)")
    lines.append("  " + ("  ".join(f"{verdict} {n}"
                                   for verdict, n in sorted(counts.items()))
                         or "no rows"))
    for row in rows:
        lines.append(
            f"  [{row['verdict']}] {row['function']}"
            f"  {row['defined_at']}  arity {row['declared_arity']},"
            f" trailing `{row['trailing_parameter']}` read"
            f" {row['trailing_reads_in_body']}x in the body,"
            f" {row['call_sites']} call site(s)"
            f" = {row['short_call_site_count']} SHORT"
            f" + {row['full_call_site_count']} FULL"
            f" ({row['pinned_full_call_site_count']} of the FULL sites"
            " webfrank-PINNED)")
        if (row["verdict"] == "PHANTOM-CANDIDATE"
                and row["full_call_site_count"] == 0):
            lines.append(
                "      NO PAYER: every call site is already short, so no"
                " caller materialises the trailing argument and dropping it"
                " buys nothing. The row is a naming question, not an"
                " instruction.")
        elif row["full_call_site_count"] and row["pinned_full_call_site_count"] \
                == row["full_call_site_count"]:
            lines.append(
                "      EVERY FULL SITE IS PINNED: the aligned-view procedure"
                " below cannot decide this row — a pinned caller reads real 0"
                " by construction. Decide from the TARGET half.")
        for site in row["short_call_sites"]:
            lines.append(f"      short call: {site}")
        for site in row["pinned_full_call_sites"]:
            lines.append(f"      full call: {site}")
        for site in row["unprototyped_declarations"]:
            lines.append(f"      K&R declaration (no prototype): {site}")
        for site in row["duplicate_definitions"]:
            lines.append(f"      also defined at: {site}")
    if rows:
        lines.append(
            "  DECIDE AT THE CALL SITE, NOT HERE: read the caller's aligned"
            " view (`fnasm.py <unit> <caller> 0xA:0xB --diff`) for an"
            " ours-only -1 lwz|li|mr beside the call. A PHANTOM-CANDIDATE"
            " with that residual is the NM class (drop the parameter); a"
            " KNR-SHORT-CALL whose target DOES set the argument register is"
            " a genuine arity bug, and one whose target does not is faithful"
            " 2001 source that must be left alone"
            " (claim.law.knr-extern-arity-can-be-faithful-not-a-defect"
            ".20260831.v1)."
            "\n  A WEBFRANK-PINNED CALLER CANNOT BE READ THAT WAY: fndiff"
            " scores the POSTPROCESSED object, so a pinned function reads"
            " real 0 by construction (AGENTS.md residual-work discipline 3)"
            " and its aligned view has nothing to say about the argument"
            " setup. Three of AR's fifteen refutations were pinned consumers"
            " and would have closed three rows on no evidence"
            " (claim.AR_probe-revert-restored-half-a-two-site-edit-and-"
            "aritycheck-needs-a-pin-column.20260903.v1). At a pinned site"
            " read the TARGET half instead (`fnasm.py <unit> <caller>"
            " 0xA:0xB`)."
            "\n  THE SHORT/FULL SPLIT IS THE PAYOFF: only a caller passing"
            " the FULL list materialises a phantom trailing argument, so a"
            " row with 0 FULL sites has no payer and dropping the parameter"
            " buys nothing — AR measured three of four PHANTOM rows that"
            " way, which the old single call-site COUNT could not express.")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--root", default=str(ROOT))
    ap.add_argument("--function", help="only this function")
    ap.add_argument("--verdict", help="only this verdict")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--out", help="write JSON here instead of stdout")
    args = ap.parse_args()

    rows = analyse(*collect(args.root), pins=load_webfrank_pins(args.root))
    if args.function:
        rows = [row for row in rows if row["function"] == args.function]
    if args.verdict:
        rows = [row for row in rows if row["verdict"] == args.verdict]
    if args.out:
        Path(args.out).write_text(json.dumps(rows, indent=2),
                                  encoding="utf-8")
        print(f"{len(rows)} row(s) -> {args.out}")
        return 0
    print(json.dumps(rows, indent=2) if args.json else format_rows(rows))
    return 0


if __name__ == "__main__":
    sys.exit(main())
