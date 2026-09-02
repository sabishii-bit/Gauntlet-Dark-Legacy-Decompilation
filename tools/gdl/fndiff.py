#!/usr/bin/env python3
"""Per-function disassembly diff between the target object (extracted from the
DOL by dtk) and the base object (our compile), with addresses and branch
targets normalized so only real codegen differences survive.

Usage:
  python tools/gdl/fndiff.py dolphin/dvd/dvd.c              # all mismatching functions
  python tools/gdl/fndiff.py dolphin/dvd/dvd.c DVDInit      # specific function(s)
  python tools/gdl/fndiff.py dolphin/si/SIBios.c -l         # just list match status
  python tools/gdl/fndiff.py zlib/infblock.c --ops          # opcode-cluster view
  python tools/gdl/fndiff.py game/g3d/sndvoice.c --classify # semantic-risk class
  python tools/gdl/fndiff.py game/mb/mb_window.c --clean    # noise-free + hints
  python tools/gdl/fndiff.py game/sys/memcard.c writeGauntletSave --relocs
      # target/ours relocation-symbol-set delta, addresses resolved

--relocs is the relocation-defect view. `real` DROPS every reloc line and
--clean NORMALIZES pool names, so a wrong-callee or wrong-datum relocation
(a `bl` to the wrong function, a load of the wrong global) reads as MATCH in
every other view — a REL24 callee lives ENTIRELY in its relocation, carrying
no target in the unlinked word. --relocs resolves every symbol to its
symbols.txt address and prints the SET delta: two spellings of one address
(a benign rename) cancel, while a differing address surfaces as a target-only
/ ours-only row. It runs even at real 0. CB hand-rolled this twice; it was
decisive both times.

--clean is the recommended iteration view: pool-name reloc noise (@N vs lbl_
for identical constants) is normalized away, every function ALWAYS ends with
a "== name: STATUS, N real diff lines" summary (so empty output can never be
mistaken for success), and mechanical hints are printed (frame delta -> the
dead-pad size to try). "MATCH (pool-name noise only)" = byte-identical after
link. "MATCH-MODULO-RELOC-NAMING" = every instruction word agrees and every
relocation TYPE agrees in order, and the only residual is that one side
names a symbol the other emits as an anonymous local pool entry: 60
functions score real > 0 in this state while linking byte-identical, and
read as open work. Two CONCRETE symbols that disagree are never absorbed
into it -- that is a real relocation defect.

--ops collapses each function to its opcode stream (registers, operands and
relocs ignored) and prints only the structurally inserted/deleted/replaced
clusters. Use it to separate real shape differences (missing statements,
moved blocks, extra calls) from register-renumber noise -- this view is what
located infblock's missing t<19 clamp and stripped error-path frees.
Because that reduction keeps only the mnemonic, a pair that agrees on the
opcode and disagrees on a LITERAL is `equal` to the matcher and appears in
no cluster; --ops therefore also prints IMMEDIATE rows for those (relocated
fields, which the linker owns, excluded) and refuses to call such a function
"pure reorder, schedule-class residual".

--count prints one summary line per function (target/base insn counts, total
diff lines, and "real" diff lines) -- use it as the per-iteration score
instead of piping through grep -c.

TWO DIFFERENT NUMBERS ARE CALLED `real`, and reconciling them cost two
lanes a re-read each. --count's `real` drops EVERY reloc line from the raw
diff rows; --clean's `real` counts rows over reloc-NORMALIZED text. They
differ in either direction -- on game/pb/pb_objregs::sDrawGeom --count says
`real 1177` while --clean says `1189 real diff lines`. --clean now prints
the raw and non-reloc row counts beside its own figure whenever they
disagree, and names which one --count and probe.py report; the older
"(+N pool-name lines suppressed)" note could not do that, since it was
clamped to zero exactly when the filtered count was the larger of the two.

--classify deliberately uses conservative categories for native-port work:
EXACT, RELOCATION_ONLY, REGISTER_ONLY, SCHEDULE_CANDIDATE, OPERAND_DIFF, and
STRUCTURAL. BASE_ONLY identifies a source helper absent from the target object;
TARGET_ONLY identifies an original function absent from our object. Only the
first three establish that instruction order and all non-register operands
agree. SCHEDULE_CANDIDATE is a review queue, not proof of semantic equivalence.

The base object is rebuilt via ninja automatically whenever the source file
is newer (pass --no-build to skip). This prevents analyzing stale objects.
"""

from collections import Counter
import difflib
import re
import subprocess
import sys
from pathlib import Path

VERSION = "GUNE5D"
OBJDUMP = Path("build/binutils/powerpc-eabi-objdump.exe")
SYMBOLS_TXT = Path(f"config/{VERSION}/symbols.txt")

_POOL_SYMBOLS = None


def pool_symbols():
    """Names of .sdata2/.rodata data objects from symbols.txt.

    The lbl_* -> <local> normalizer was asymmetric: a splitter-NAMED pool
    constant (sBTextIntBias) scored as a real reloc diff against the
    target's anonymous entry purely because someone named it, inflating
    `real` in every TU with named pool entries (field report, 2026-08-31).
    Naming a pool constant must never change a score.
    """
    global _POOL_SYMBOLS
    if _POOL_SYMBOLS is None:
        symbols = set()
        if SYMBOLS_TXT.exists():
            pattern = re.compile(
                r"^(\S+)\s*=\s*\.(?:sdata2|rodata):.*type:object")
            for line in SYMBOLS_TXT.read_text(
                    encoding="utf-8", errors="replace").splitlines():
                match = pattern.match(line.strip())
                if match:
                    symbols.add(match.group(1))
        _POOL_SYMBOLS = frozenset(symbols)
    return _POOL_SYMBOLS


_SYMBOL_ADDRESSES = None


def symbol_addresses():
    """name -> absolute address for every symbols.txt entry (data identity)."""
    global _SYMBOL_ADDRESSES
    if _SYMBOL_ADDRESSES is None:
        table = {}
        if SYMBOLS_TXT.exists():
            pattern = re.compile(r"^(\S+)\s*=\s*\.\w+:0x([0-9A-Fa-f]+);")
            for line in SYMBOLS_TXT.read_text(
                    encoding="utf-8", errors="replace").splitlines():
                match = pattern.match(line.strip())
                if match:
                    name, addr = match.group(1), int(match.group(2), 16)
                    table[name] = addr
                    # parse() strips dtk address suffixes upstream; register
                    # the stripped alias so lookups still resolve.
                    stripped = re.sub(r"_80[0-9A-Fa-f]{6}$", "", name)
                    if stripped != name:
                        table.setdefault(stripped, addr)
        _SYMBOL_ADDRESSES = table
    return _SYMBOL_ADDRESSES

BRANCH_RE = re.compile(
    r"\b(b|bl|ba|bla|beq|bne|bgt|blt|bge|ble|bso|bns|bdnz|bdz)"
    r"([+-]?)\s+(cr\d,)?([0-9a-f]+)\s*$"
)
REGISTER_RE = re.compile(r"\b(?:r(?:[12]?\d|3[01])|f(?:[12]?\d|3[01])|cr[0-7])\b")
# ...bss.N included: its omission left thirteen byte-exact player.c
# functions reading "2 STRUCTURAL" and nearly queued as work. Widening
# moves `real` project-wide; gate baselines regenerate via --at-head.
PRIVATE_DATA_RE = re.compile(r"^\.{3}(?:rodata|data|bss|sbss|sdata2?)\.\d+$")


def compiler_private_aliases_from_symbols(symbol_table):
    """Map compiler-private data labels to named symbols at the same location."""
    locations = {}
    for line in symbol_table.splitlines():
        fields = line.split()
        if len(fields) < 4 or not re.fullmatch(r"[0-9a-fA-F]+", fields[0]):
            continue
        section, name = fields[-3], fields[-1]
        if section == "*UND*" or name.startswith(".") and not PRIVATE_DATA_RE.match(name):
            continue
        locations.setdefault((section, fields[0].lower()), []).append(name)

    aliases = {}
    for names in locations.values():
        named = [name for name in names if not PRIVATE_DATA_RE.match(name)]
        if len(named) != 1:
            continue
        for name in names:
            if PRIVATE_DATA_RE.match(name):
                aliases[name] = named[0]
    return aliases


def compiler_private_aliases(objfile: Path):
    out = subprocess.run(
        [str(OBJDUMP), "-t", str(objfile)], capture_output=True, text=True
    ).stdout
    return compiler_private_aliases_from_symbols(out)


def parse(objfile: Path):
    """Return {function_name: [normalized instruction/reloc lines]}."""
    aliases = compiler_private_aliases(objfile)
    out = subprocess.run(
        [str(OBJDUMP), "-dr", str(objfile)], capture_output=True, text=True
    ).stdout
    # First pass: which stripped names are UNIQUE? dtk-suffixed names strip
    # to their base for pairing, but two functions whose bases collide
    # (dtor_800DB21C / dtor_800DBB94 -> "dtor") must KEEP their suffixes —
    # collapsing them silently returned one function's rows for the other,
    # and a worker's first baseline was the wrong function's.
    raw_names = re.findall(r"^[0-9a-f]+ <(.+)>:$", out, re.M)
    strip_counts: dict[str, int] = {}
    for name in raw_names:
        if not name.startswith("fn_"):
            stripped = re.sub(r"_80[0-9A-Fa-f]{6}$", "", name)
            strip_counts[stripped] = strip_counts.get(stripped, 0) + 1

    funcs = {}
    cur = None
    cur_start = 0
    for line in out.splitlines():
        m = re.match(r"^([0-9a-f]+) <(.+)>:$", line)
        if m:
            cur_start = int(m.group(1), 16)
            cur = m.group(2)
            if not cur.startswith("fn_"):
                stripped = re.sub(r"_80[0-9A-Fa-f]{6}$", "", cur)
                if strip_counts.get(stripped, 0) <= 1:
                    cur = stripped
            funcs[cur] = []
            continue
        if cur is None:
            continue
        m = re.match(r"^\s+([0-9a-f]+):\s+(?:[0-9a-f]{2} ){4}\s*(.+)$", line)
        if m:
            ins = re.sub(r"<[^>]+>", "", m.group(2).strip())

            def normalize_branch(branch_match):
                target = int(branch_match.group(4), 16) - cur_start
                return (
                    f"{branch_match.group(1)}{branch_match.group(2)} "
                    f"{branch_match.group(3) or ''}<fn+0x{target:x}>"
                )

            ins = BRANCH_RE.sub(normalize_branch, ins)
            funcs[cur].append(ins.strip())
        elif "R_PPC" in line:
            rel = line.strip().split(maxsplit=1)[1]

            # dtk suffixes local symbol names with their address; strip so
            # target "changed_80345368" pairs with our "changed". But for
            # anonymous lbl_/jumptable_ symbols the suffix IS the identity:
            # stripping it collapsed every such target to bare "lbl" and
            # destroyed the address before any comparison — 111 of 405
            # sweep rows were this artifact and a real wrong-symbol defect
            # hid among them (claim.law.parse-strips-the-lbl-address-so-
            # signature-alone-cannot-clear-a-reloc-row).
            def strip_dtk_suffix(m):
                return m.group(0) if m.group(1) in ("lbl", "jumptable") \
                    else m.group(1)
            rel = re.sub(r"(\w+?)_80[0-9A-Fa-f]{6}(?=$|\+)",
                         strip_dtk_suffix, rel)
            for private, named in aliases.items():
                rel = re.sub(
                    rf"(?<=\s){re.escape(private)}(?=$|[+-])", named, rel
                )
            if rel.startswith("R_PPC_REL24") and funcs[cur]:
                funcs[cur][-1] = re.sub(r"<fn\+0x-?[0-9a-f]+>", "<reloc>",
                                        funcs[cur][-1])
            funcs[cur].append("    " + rel)
    return funcs


def opcode_multiset_signature(objfile: Path):
    """Per-function sha1 of the sorted opcode multiset.

    The gate's carrier-change discriminant: a respell at equal count can
    improve `real` while regressing fuzzy, and the whole gate chain
    (gate, ninja, DOL sha1) passed one end-to-end because nothing read
    fuzzy. A changed multiset at equal count means the CARRIER opcodes
    changed — that state must be arbitrated on fuzzy from a fresh
    report, never banked on real alone (claim.law.EN-equal-count-opcode-
    respell-must-be-arbitrated-on-fuzzy-not-real).
    """
    import hashlib
    table = parse(objfile)
    out = {}
    for name, lines in table.items():
        ops = sorted(ln.split()[0] for ln in lines
                     if ln and not ln.startswith("    "))
        out[name] = hashlib.sha1("\n".join(ops).encode()).hexdigest()[:12]
    return out


def raw_words_signature(objfile: Path):
    """Per-function sha1 of instruction WORDS ONLY (no reloc lines).

    Companion to raw_signature for the gate's naming-drift classification:
    a symbol rename changes reloc lines but no instruction word, so
    words-equal + real 0 (which resolves reloc ADDRESSES) proves the drift
    is naming-only — four workers hand-arbitrated exactly this case.
    """
    import hashlib
    out = subprocess.run(
        [str(OBJDUMP), "-dr", str(objfile)], capture_output=True, text=True
    ).stdout
    raw_names = re.findall(r"^[0-9a-f]+ <(.+)>:$", out, re.M)
    strip_counts: dict[str, int] = {}
    for name in raw_names:
        if not name.startswith("fn_"):
            stripped = re.sub(r"_80[0-9A-Fa-f]{6}$", "", name)
            strip_counts[stripped] = strip_counts.get(stripped, 0) + 1
    hashes = {}
    cur = None
    hasher = None
    for line in out.splitlines():
        m = re.match(r"^[0-9a-f]+ <(.+)>:$", line)
        if m:
            if cur is not None:
                hashes[cur] = hasher.hexdigest()[:12]
            cur = m.group(1)
            if not cur.startswith("fn_"):
                stripped = re.sub(r"_80[0-9A-Fa-f]{6}$", "", cur)
                if strip_counts.get(stripped, 0) <= 1:
                    cur = stripped
            hasher = hashlib.sha1()
            continue
        if cur is None:
            continue
        m = re.match(r"^\s+[0-9a-f]+:\s+((?:[0-9a-f]{2} ){4})", line)
        if m:
            hasher.update(m.group(1).encode())
    if cur is not None:
        hashes[cur] = hasher.hexdigest()[:12]
    return hashes


def raw_signature(objfile: Path):
    """Per-function sha1 of raw instruction words + raw relocation lines.

    The soundness backstop the score stack lacks: a change passed NEUTRAL
    real, IDENTICAL multiset, unchanged lines/counts/clusters AND
    defake_gate — while regressing fuzzy (operand-encoding change,
    2026-08-31). Nothing derived from normalized text can prove byte
    identity; this hash can. Compare ours-vs-ours across an edit.
    """
    import hashlib
    out = subprocess.run(
        [str(OBJDUMP), "-dr", str(objfile)], capture_output=True, text=True
    ).stdout
    raw_names = re.findall(r"^[0-9a-f]+ <(.+)>:$", out, re.M)
    strip_counts: dict[str, int] = {}
    for name in raw_names:
        if not name.startswith("fn_"):
            stripped = re.sub(r"_80[0-9A-Fa-f]{6}$", "", name)
            strip_counts[stripped] = strip_counts.get(stripped, 0) + 1
    hashes = {}
    cur = None
    cur_start = 0
    hasher = None
    for line in out.splitlines():
        m = re.match(r"^([0-9a-f]+) <(.+)>:$", line)
        if m:
            if cur is not None:
                hashes[cur] = hasher.hexdigest()[:12]
            cur = m.group(2)
            cur_start = int(m.group(1), 16)
            if not cur.startswith("fn_"):
                stripped = re.sub(r"_80[0-9A-Fa-f]{6}$", "", cur)
                if strip_counts.get(stripped, 0) <= 1:
                    cur = stripped
            hasher = hashlib.sha1()
            continue
        if cur is None:
            continue
        m = re.match(r"^\s+[0-9a-f]+:\s+((?:[0-9a-f]{2} ){4})", line)
        if m:
            hasher.update(m.group(1).encode())
        elif "R_PPC" in line:
            # Compiler-private pool labels (@N) renumber whenever ANY
            # sibling's pool changes — that churn is benign (fndiff --clean
            # suppresses it) and made the gate cry wolf on five untouched
            # functions. Hash the reloc with the private index normalized;
            # named symbols and addends still count.
            reloc = re.sub(r"@\d+\b", "@pool", line.strip())
            # A reloc line's leading offset is SECTION-relative, and
            # section-anchored targets (.text+0x…, .sdata+0x…) carry
            # layout-dependent addends — both shift for every downstream
            # sibling whenever any earlier function or datum grows, which
            # made the gate flag 27 untouched functions after a one-insn
            # win. Hash the offset function-relative and the anchored
            # addend as position-blind; a genuinely retargeted reloc still
            # differs through fndiff's real score (resolved symbol names).
            m = re.match(r"^([0-9a-f]+):\s*(.*)$", reloc)
            if m:
                rel = int(m.group(1), 16) - cur_start
                reloc = f"+{rel:#x}: {m.group(2)}"
            reloc = re.sub(r"(\.[A-Za-z][\w.]*)\+0x[0-9a-f]+", r"\1+off",
                           reloc)
            hasher.update(reloc.encode())
    if cur is not None:
        hashes[cur] = hasher.hexdigest()[:12]
    return hashes


def opcodes(lines):
    """Instruction lines only (no relocs), reduced to the mnemonic."""
    return [ln.split()[0] for ln in lines if ln and not ln.startswith("    ")]


def instruction_lines(lines):
    return [ln for ln in lines if ln and not ln.startswith("    ")]


def relocation_signature(line):
    """Normalize compiler-private labels while preserving public/call targets."""
    fields = line.split(maxsplit=1)
    reloc_type = fields[0] if fields else line.strip()
    symbol = fields[1] if len(fields) > 1 else ""
    if reloc_type == "R_PPC_REL24":
        return reloc_type, symbol
    local = re.fullmatch(r"(?:lbl|jumptable|@\d+)([+-].+)?", symbol)
    if local:
        symbol = "<local>" + (local.group(1) or "")
    else:
        head = re.match(r"([A-Za-z_@]\w*)([+-]0x[0-9a-fA-F]+|[+-]\d+)?$",
                        symbol)
        if head:
            name, addend = head.group(1), head.group(2)
            if name in pool_symbols():
                # Splitter-named pool constants normalize like lbl_ ones
                # (our side emits pool entries anonymously — address
                # resolution here would score every literal as a diff).
                symbol = "<local>" + (addend or "")
            else:
                # Two spellings of ONE address are the same relocation:
                # critter's `gControllerButtons+0x4` vs the target's
                # `sFlags` both resolve to 0x803445CC and link to identical
                # bytes — two functions sat mis-scored at real 2 over
                # exactly this (2026-08-31). Compare addresses, not names.
                base_addr = symbol_addresses().get(name)
                if base_addr is not None:
                    symbol = f"@0x{base_addr + int(addend or '0', 0):08X}"
    return reloc_type, symbol


def relocation_symbols(objfile: Path):
    """{fn: [(reloc_type, symbol_text), ...]} in instruction order.

    RAW symbol text, deliberately NOT put through relocation_signature:
    the caller (defake_gate's NAMING-DRIFT check) has to decide whether
    two spellings denote one datum by RESOLVING them, and a normalizer
    that collapses them first destroys exactly that evidence.
    """
    out = {}
    for name, lines in parse(objfile).items():
        rows = []
        for line in lines:
            if line.startswith("    "):
                parts = line.strip().split(maxsplit=1)
                rows.append((parts[0], parts[1] if len(parts) > 1 else ""))
        out[name] = rows
    return out


_RELOC_SYM_RE = re.compile(
    r"([A-Za-z_@.$][\w.$@]*)([+-]0x[0-9a-fA-F]+|[+-]\d+)?$")


def resolve_reloc_symbol(symbol):
    """Absolute address for a relocation symbol text, or None.

    Mirrors relocation_signature's address logic WITHOUT collapsing locals —
    the --relocs view resolves a public callee/datum to its symbols.txt
    address so two spellings of one address (a benign rename) compare equal,
    while a genuinely different callee shows as a set delta. Locals
    (lbl_/jumptable/@N) and splitter-named pool constants have no stable
    address and resolve to None (compared by normalized name instead).
    """
    head = _RELOC_SYM_RE.match((symbol or "").strip())
    if not head:
        return None
    name = head.group(1)
    if re.fullmatch(r"(?:lbl|jumptable|@\d+).*", name) or name in pool_symbols():
        return None
    base = symbol_addresses().get(name)
    if base is None:
        return None
    try:
        return base + int(head.group(2) or "0", 0)
    except ValueError:
        return None


# dtk names anonymous local labels (jump tables, pool/block labels) with an
# address suffix — jumptable_80120B4C, lbl_80347F3C — while our unlinked
# object emits them anonymously at a DIFFERENT address. relocation_signature
# only collapses the bare `lbl`/`jumptable`/`@N` spellings, so the suffixed
# forms would otherwise leak into the set as false deltas and bury a real
# wrong-callee row. A local label carries no stable cross-object address; it
# is compared by TYPE and COUNT alone (collapsed to <local>), exactly as the
# --clean view treats pool-name noise.
_LOCAL_LABEL_RE = re.compile(
    r"(?:lbl|jumptable|jtbl|@\d+)(?:_[0-9A-Fa-f]{6,8})?([+-].+)?$")


def _reloc_key(reloc_type, symbol, resolve):
    """Canonical identity of a relocation for set comparison.

    (type, 0xADDR) when the symbol resolves; (type, <local>+addend) for an
    anonymous local label (no stable cross-object address); otherwise
    (type, normalized symbol via relocation_signature).
    """
    addr = resolve(symbol)
    if addr is not None:
        return (reloc_type, f"0x{addr:08X}")
    local = _LOCAL_LABEL_RE.fullmatch((symbol or "").strip())
    if local:
        return (reloc_type, "<local>" + (local.group(1) or ""))
    _, norm = relocation_signature(f"{reloc_type} {symbol}".strip())
    return (reloc_type, norm)


def _reloc_display(reloc_type, symbol, resolve):
    addr = resolve(symbol)
    where = f"  ({'0x%08X' % addr})" if addr is not None else "  (unresolved)"
    return f"{reloc_type:<16} {symbol}{where}"


def reloc_set_delta(target_rows, ours_rows, resolve=None):
    """(target_only, ours_only, common_count) for two relocation lists.

    Each row is (reloc_type, raw_symbol). Multiset semantics: a relocation
    present twice on one side and once on the other is a one-row delta.
    Rows are keyed by resolved address when possible (so a rename cancels)
    and returned as human display strings with the address resolved.

    This is the score-invisible defect class fndiff otherwise cannot show:
    `real` DROPS every reloc line and `--clean` NORMALIZES pool names, so a
    wrong-callee / wrong-datum relocation reads as MATCH in every other view
    (claim.law.HV_defake-gate-naming-drift-is-a-false-benign-on-a-wrong-
    callee). CB hand-rolled this delta twice; it was decisive both times.
    """
    from collections import Counter
    resolve = resolve or resolve_reloc_symbol
    t_keys = Counter(_reloc_key(*row, resolve) for row in target_rows)
    b_keys = Counter(_reloc_key(*row, resolve) for row in ours_rows)
    target_only, ours_only = [], []
    seen = Counter()
    for reloc_type, symbol in target_rows:
        key = _reloc_key(reloc_type, symbol, resolve)
        if seen[key] < (t_keys[key] - b_keys.get(key, 0)):
            target_only.append(_reloc_display(reloc_type, symbol, resolve))
            seen[key] += 1
    seen = Counter()
    for reloc_type, symbol in ours_rows:
        key = _reloc_key(reloc_type, symbol, resolve)
        if seen[key] < (b_keys[key] - t_keys.get(key, 0)):
            ours_only.append(_reloc_display(reloc_type, symbol, resolve))
            seen[key] += 1
    common = sum((t_keys & b_keys).values())
    return target_only, ours_only, common


def relocs_diff(name, target_rows, ours_rows, resolve=None):
    """Print the target/ours relocation-symbol-set delta for one function."""
    target_only, ours_only, common = reloc_set_delta(
        target_rows, ours_rows, resolve=resolve)
    if not target_only and not ours_only:
        print(f"== {name}: relocation sets IDENTICAL ({common} reloc(s),"
              " addresses resolved)")
        return
    print(f"RELOCS {name}  ({common} shared;"
          f" {len(target_only)} target-only, {len(ours_only)} ours-only —"
          " a differing ADDRESS is a wrong callee/datum, not a rename)")
    if target_only:
        print("  target-only (target has, ours lacks):")
        for row in target_only:
            print(f"    {row}")
    if ours_only:
        print("  ours-only (ours has, target lacks):")
        for row in ours_only:
            print(f"    {row}")


def reloc_rows_from_lines(lines):
    """[(reloc_type, raw_symbol)] from one function's parsed lines.

    Same extraction as relocation_symbols() but off an already-parsed line
    list (the --relocs view has target/base parsed in hand)."""
    rows = []
    for line in lines:
        if line.startswith("    "):
            parts = line.strip().split(maxsplit=1)
            rows.append((parts[0], parts[1] if len(parts) > 1 else ""))
    return rows


def relocation_signatures(lines):
    """Relocations in instruction order, retaining semantically relevant targets."""
    result = []
    for line in lines:
        if line.startswith("    "):
            result.append(relocation_signature(line.strip()))
    return result


def erase_registers(line):
    """Keep opcodes, immediates, offsets and addressing modes; erase register colors."""
    return REGISTER_RE.sub("<reg>", line)


def semantic_tokens(lines):
    """Tokens suitable for conservative source-vs-target shape comparison."""
    result = []
    for line in lines:
        if line.startswith("    "):
            result.append(("reloc", relocation_signature(line.strip())))
        else:
            result.append(("insn", erase_registers(line)))
    return result


def classify_function(target_lines, base_lines):
    """Classify a residual by increasing semantic risk.

    REGISTER_ONLY requires identical instruction/relocation order and identical
    non-register operands. SCHEDULE_CANDIDATE requires the same multiset of
    register-erased operations, but does not claim reordered side effects are safe.
    """
    if target_lines == base_lines:
        return "EXACT"

    target_ins = instruction_lines(target_lines)
    base_ins = instruction_lines(base_lines)
    target_relocs = relocation_signatures(target_lines)
    base_relocs = relocation_signatures(base_lines)

    if target_ins == base_ins and target_relocs == base_relocs:
        return "RELOCATION_ONLY"

    target_sem = semantic_tokens(target_lines)
    base_sem = semantic_tokens(base_lines)
    if target_sem == base_sem:
        return "REGISTER_ONLY"

    target_ops = opcodes(target_lines)
    base_ops = opcodes(base_lines)
    if target_ops == base_ops:
        return "OPERAND_DIFF"

    if (len(target_ins) == len(base_ins)
            and Counter(target_sem) == Counter(base_sem)):
        return "SCHEDULE_CANDIDATE"

    return "STRUCTURAL"


FRAME_RE = re.compile(r"stwu\s+r1,-(\d+)\(r1\)")


def normalized_reloc_lines(lines):
    """Lines with reloc symbols collapsed to their signature: pool-name noise
    (@N vs lbl_ for identical constants) diffs to nothing."""
    out = []
    for ln in lines:
        if ln.startswith("    "):
            rt, sym = relocation_signature(ln.strip())
            out.append(f"    {rt} {sym}")
        else:
            out.append(ln)
    return out


IMM_RE = re.compile(r"-?\b(?:0x[0-9a-fA-F]+|\d+)\b")
# parse() rewrites every branch destination as <fn+0xNN> (negatives kept).
BRANCH_TARGET_RE = re.compile(r"<fn\+0x-?[0-9a-f]+>")


def immediates(line):
    """Numeric fields of one instruction, register colors erased."""
    return IMM_RE.findall(erase_registers(line))


def relocated_instructions(lines):
    """(instruction lines, per-instruction "carries a relocation" flags).

    An instruction whose immediate field is filled in by the LINKER holds
    the address bits in the target object and a zero in ours; comparing
    those literals is meaningless, and doing so made the first cut of the
    immediate flag fire on both halves of every `lis`/`addi` address pair
    (measured on G3DReadControlPadStates). The relocation line that
    follows the instruction is the discriminant.
    """
    ins, reloc = [], []
    for ln in lines:
        if not ln:
            continue
        if ln.startswith("    "):
            if reloc:
                reloc[-1] = True
        else:
            ins.append(ln)
            reloc.append(False)
    return ins, reloc


def immediate_deltas(t, b):
    """Aligned same-opcode pairs whose IMMEDIATE fields differ.

    --ops reduces every instruction to its mnemonic, so a pair that agrees
    on the opcode and disagrees on a literal is `equal` to the sequence
    matcher and never reaches the cluster list at all. When the multiset
    also matches, --ops printed "IDENTICAL -- pure reorder, schedule-class
    residual" over it: a false all-clear on a word that decides
    postprocessor eligibility.

    Relocated immediates are excluded (see relocated_instructions): the
    linker owns those bits, and the relocation itself is already scored by
    `real`.

    Returns [(t_index, b_index, kind, t_line, b_line)] with kind
    "branch" when the only differing literal is a normalized branch
    target (usually an artifact of upstream drift) and "immediate"
    otherwise.
    """
    ti, t_rel = relocated_instructions(t)
    bi, b_rel = relocated_instructions(b)
    to = [ln.split()[0] for ln in ti]
    bo = [ln.split()[0] for ln in bi]
    out = []
    for tag, i1, i2, j1, j2 in difflib.SequenceMatcher(
            None, to, bo, autojunk=False).get_opcodes():
        if tag != "equal":
            continue
        for k in range(i2 - i1):
            if t_rel[i1 + k] or b_rel[j1 + k]:
                continue
            tl, bl = ti[i1 + k], bi[j1 + k]
            if immediates(tl) == immediates(bl):
                continue
            kind = ("branch" if BRANCH_TARGET_RE.search(tl)
                    and BRANCH_TARGET_RE.sub("", tl)
                    == BRANCH_TARGET_RE.sub("", bl) else "immediate")
            out.append((i1 + k, j1 + k, kind, tl, bl))
    return out


def reloc_naming_only(t, b):
    """True when the ONLY residual is how relocation symbols are SPELLED.

    Requires, conservatively: identical instruction words in order;
    identical relocation TYPES in order; and every differing symbol pair
    to have an anonymous local (`<local>`, our side's un-named pool entry)
    on exactly one side. Two concrete addresses that disagree are a real
    relocation defect and are never absorbed here.
    """
    if instruction_lines(t) != instruction_lines(b):
        return False
    tr, br = relocation_signatures(t), relocation_signatures(b)
    if len(tr) != len(br):
        return False
    differs = False
    for (t_type, t_sym), (b_type, b_sym) in zip(tr, br):
        if t_type != b_type:
            return False
        if t_sym == b_sym:
            continue
        differs = True
        anon = [s.startswith("<local>") for s in (t_sym, b_sym)]
        if anon.count(True) != 1:
            return False
    return differs


def frame_size(lines):
    for ln in lines:
        m = FRAME_RE.search(ln)
        if m:
            return int(m.group(1))
    return None


def count_real(raw_rows):
    """`--count`'s `real`: raw diff rows with every reloc line dropped.

    Kept as one function so the two views cannot drift apart again — this
    is byte-for-byte the computation main()'s --count branch performs.
    """
    return sum(1 for line in raw_rows if "R_PPC" not in line)


def real_reconciliation(real, raw_rows, noise):
    """Reconcile the numbers this project all calls `real`.

    THE TWO-REALS CONFUSION (measured twice). `--count` reports
    `real` = raw diff rows minus every reloc line, while `--clean` reports
    `real` = diff rows over reloc-NORMALIZED text. They are different
    computations of a same-named quantity and can differ in EITHER
    direction: measured on game/pb/pb_objregs::sDrawGeom, `--count` says
    `real 1177` and `--clean` says `1189 real diff lines`, and neither
    line acknowledged that the other number existed.

    The old parenthetical could not close the gap, because it was computed
    as `raw - real` and clamped to zero — exactly the sDrawGeom case, where
    the filtered count is LARGER than the raw rows, printed nothing at all.
    """
    filtered = count_real(raw_rows)
    if filtered != real:
        return (f" [artifact-filtered; raw rows {len(raw_rows)}, of which"
                f" {filtered} non-reloc — {filtered} is the `real` that"
                " --count and probe.py report for this function]")
    if noise:
        return f" (+{noise} pool-name lines suppressed)"
    return ""


def clean_diff(name, t, b):
    """Noise-free diff + always-printed summary + mechanical hints.

    Empty output can never mean success: every function ends with a '==' line.
    """
    tn, bn = normalized_reloc_lines(t), normalized_reloc_lines(b)
    raw = [l for l in difflib.unified_diff(t, b, lineterm="", n=0)
           if l[:1] in "+-" and l[:3] not in ("+++", "---")]
    diff = list(difflib.unified_diff(tn, bn, "target", "base", lineterm="", n=2))
    real = sum(1 for l in diff if l[:1] in "+-" and l[:3] not in ("+++", "---"))
    noise = len(raw) - real if len(raw) > real else 0

    if real:
        print("=" * 20, name)
        for line in diff:
            print(line)

    hints = []
    tf, bf = frame_size(t), frame_size(b)
    if tf is not None and bf is not None and tf != bf:
        delta = tf - bf
        ti_n, bi_n = len(instruction_lines(t)), len(instruction_lines(b))
        if delta < 0 and ti_n > bi_n:
            # A bigger target with a smaller frame means MISSING CODE on
            # our side (C++ EH scaffolding is the known case), not excess
            # pad — the "drop pad" hint sent a worker chasing the
            # instrument on two dtors.
            hints.append(f"frame delta {delta:+d} BUT target has"
                         f" {ti_n - bi_n} MORE insns -> missing code"
                         " (EH scaffolding?), NOT excess pad")
        else:
            hints.append(
                f"frame delta {delta:+d} -> try `u8 unused[{abs(delta)}]`"
                if delta > 0 else
                f"frame delta {delta:+d} -> our frame is BIGGER;"
                f" drop {-delta}B of pad/locals")
    cat = classify_function(t, b)
    if real == 0:
        status = "MATCH (pool-name noise only)" if raw else "EXACT"
    elif reloc_naming_only(t, b):
        # A FINISHED function read as open: every instruction word agrees,
        # every relocation TYPE agrees in order, and the only residual is
        # that one side names a symbol the other emits as an anonymous
        # local pool entry. `real` counts those lines and the function
        # scored OPERAND_DIFF with N real diff lines despite linking
        # byte-identical (DVDCheckDisk, dolphin/dvd/dvd, a Matching TU).
        status = "MATCH-MODULO-RELOC-NAMING"
        hints.append("instruction words and reloc types all agree; the"
                     " residual is anonymous-local vs named-symbol"
                     " SPELLING. Link-equal unless the pool entry itself"
                     " is wrong — confirm by dumping this function's"
                     " relocation symbols, which no score sees")
    else:
        status = cat
    imm = [d for d in immediate_deltas(t, b) if d[2] == "immediate"]
    if imm and status not in ("EXACT", "MATCH-MODULO-RELOC-NAMING"):
        hints.append(f"{len(imm)} aligned same-opcode IMMEDIATE delta(s)"
                     " — see `--ops`, which lists them with offsets")
    hint_s = ("  HINT: " + "; ".join(hints)) if hints else ""
    print(f"== {name}: {status}, {real} real diff lines"
          f"{real_reconciliation(real, raw, noise)}{hint_s}")


def shiftable_gap(seq, lo, hi):
    """Can the [lo, hi) gap slide along ``seq`` and produce the same diff?

    A run of repeating opcodes makes an LCS gap's POSITION arbitrary: the
    same edit is expressible at several offsets, so the reported one is
    not where the residual lives. A lane spent a session on a cluster
    located this way — the recorded "7-instruction shortfall at
    T[66:76]@108-130" was an artifact of a dense repeating block, and the
    prologue was 6 instructions LONGER than target's, not shorter
    (attempt.SF_processeffects-pool-base-and-named-locals.20260901.v3).
    """
    if lo >= hi:
        return False
    if lo > 0 and seq[lo - 1] == seq[hi - 1]:
        return True
    return hi < len(seq) and seq[lo] == seq[hi]


def cluster_flags(tag, to, bo, i1, i2, j1, j2):
    flags = []
    if tag == "delete" and shiftable_gap(to, i1, i2):
        flags.append("SHIFTABLE")
    elif tag == "insert" and shiftable_gap(bo, j1, j2):
        flags.append("SHIFTABLE")
    elif tag == "replace" and (shiftable_gap(to, i1, i2)
                               or shiftable_gap(bo, j1, j2)):
        flags.append("SHIFTABLE")
    if i2 > i1 and j2 > j1 and Counter(to[i1:i2]) == Counter(bo[j1:j2]):
        flags.append("BALANCED")
    return flags


def ops_diff(name, t, b):
    to, bo = opcodes(t), opcodes(b)
    sm = difflib.SequenceMatcher(None, to, bo, autojunk=False)
    clusters = [x for x in sm.get_opcodes() if x[0] != "equal"]
    # Same-opcode/differing-immediate pairs live INSIDE the equal runs, so
    # they never reach the cluster list. Compute them before the headline:
    # "diffs are register/reloc only" was flatly false whenever one exists.
    imm_all = immediate_deltas(t, b)
    imm = [d for d in imm_all if d[2] == "immediate"]
    branch_imm = [d for d in imm_all if d[2] == "branch"]
    if clusters:
        head = ""
    elif imm:
        head = (f" (opcode streams identical, but {len(imm)} aligned"
                " IMMEDIATE delta(s) below -- NOT register/reloc only)")
    else:
        head = " (opcode streams identical -- diffs are register/reloc only)"
    print(f"==== {name}: target {len(to)} insns, ours {len(bo)}{head}")
    if clusters and len(to) != len(bo):
        # State the NET direction once. A per-cluster "shortfall" reads as
        # the whole story and has been taken for one.
        longer = "OURS" if len(bo) > len(to) else "TARGET"
        print(f"  net count: {longer} is longer by {abs(len(bo) - len(to))}"
              " -- read every cluster against this direction")
    # Multiset verdict: IDENTICAL means every difference is pure reorder
    # (SCHEDULE class); any +/- means something structural is hiding in the
    # diff even when insn counts look close.
    delta = Counter(to) - Counter(bo), Counter(bo) - Counter(to)
    if not delta[0] and not delta[1]:
        print(f"  opcode multiset: IDENTICAL ({len(to)}/{len(bo)})"
              + (f" -- but {len(imm)} IMMEDIATE word(s) differ at aligned"
                 " same-opcode positions (below): NOT pure reorder, NOT"
                 " schedule-class" if imm else
                 " -- pure reorder, schedule-class residual"))
    else:
        gains = " ".join(f"+{n} {op}" for op, n in sorted(delta[0].items()))
        losses = " ".join(f"-{n} {op}" for op, n in sorted(delta[1].items()))
        print(f"  opcode multiset: DIFFERS  target-only: {gains or '(none)'}"
              f"  ours-only: {losses or '(none)'}")
    # @0x offsets are function-relative byte offsets (index*4), directly
    # usable as fnasm.py 0xA:0xB slices on the target (T) / --ours (O) dumps.
    flagged = 0
    for tag, i1, i2, j1, j2 in clusters:
        flags = cluster_flags(tag, to, bo, i1, i2, j1, j2)
        if flags:
            flagged += 1
        note = f"  [{' + '.join(flags)}]" if flags else ""
        print(f"  {tag:7} T[{i1}:{i2}]@{i1 * 4:x}-{i2 * 4:x}={to[i1:i2]}"
              f"  O[{j1}:{j2}]@{j1 * 4:x}-{j2 * 4:x}={bo[j1:j2]}{note}")
    for ti, bi, kind, t_line, b_line in imm:
        print(f"  IMMEDIATE T[{ti}]@{ti * 4:x}  O[{bi}]@{bi * 4:x}"
              f"   T: {t_line}   O: {b_line}")
    if imm:
        print(f"  {len(imm)} IMMEDIATE row(s): the opcode agrees and a"
              " LITERAL does not. These sit inside the matcher's EQUAL runs,"
              " so no cluster above covers them and the multiset verdict"
              " cannot see them — a same-opcode immediate is an"
              " eligibility-deciding word, not schedule noise. Read each at"
              f" `fnasm <unit> {name} 0x{imm[0][0] * 4:x}:0x"
              f"{imm[0][0] * 4 + 4:x} --diff`.")
    if branch_imm:
        print(f"  ({len(branch_imm)} further same-opcode row(s) differ only"
              " in a normalized BRANCH TARGET — usually downstream of the"
              " clusters above; fix those first and re-read.)")
    if flagged:
        print(f"  {flagged} of {len(clusters)} clusters flagged: SHIFTABLE ="
              " the same edit is expressible at another offset, so THIS"
              " offset is not where the residual lives; BALANCED = the"
              " cluster's own opcode multiset agrees, i.e. a local reorder."
              " Confirm either against `fnasm <unit> <fn> 0xA:0xB --diff`"
              " before working it.")


def truncate_ops(ops_text, limit):
    """First `limit` lines of an --ops dump, plus a suppressed-IMMEDIATE note.

    probe.py and defake_gate.py print only the first N lines of --ops after
    a failed probe. IMMEDIATE rows sit at the BOTTOM of ops_diff — they live
    inside the matcher's EQUAL runs, so no cluster covers them — so a
    truncated view SILENTLY dropped exactly the same-opcode-immediate words
    that decide eligibility. CB read one such cut as a frame collapse when
    the real story was a changed literal below the fold (run 34 item 5). The
    cut is now announced, and the dropped IMMEDIATE count is named.
    """
    lines = ops_text.strip().splitlines()
    if len(lines) <= limit:
        return "\n".join(lines)
    kept, dropped = lines[:limit], lines[limit:]
    imm_dropped = sum(
        1 for line in dropped if line.lstrip().startswith("IMMEDIATE "))
    if imm_dropped:
        note = (f"  ... {imm_dropped} IMMEDIATE row(s) suppressed"
                f" ({len(dropped)} --ops line(s) hidden) — a same-opcode"
                " immediate decides eligibility and is NOT schedule noise;"
                " read the full `fndiff --ops` before concluding")
    else:
        note = (f"  ... {len(dropped)} more --ops line(s) suppressed"
                " (full view: `fndiff --ops`)")
    return "\n".join(kept + [note])


def main():
    flags = ("-l", "--ops", "--count", "--classify", "--no-build", "--clean",
             "--raw", "--relocs")
    args = [a for a in sys.argv[1:] if a not in flags]
    list_only = "-l" in sys.argv
    ops_only = "--ops" in sys.argv
    count_only = "--count" in sys.argv
    classify_only = "--classify" in sys.argv
    no_build = "--no-build" in sys.argv
    clean = "--clean" in sys.argv
    raw = "--raw" in sys.argv
    relocs_only = "--relocs" in sys.argv
    if not args or args[0] in ("--help", "-h", "help"):
        print(__doc__)
        return 1

    unit = args[0].replace("\\", "/").strip("/")
    if unit.startswith("src/"):
        unit = unit[len("src/"):]
    unit = re.sub(r"\.(c|cpp)$", "", unit)
    target_o = Path(f"build/{VERSION}/obj/{unit}.o")
    base_o = Path(f"build/{VERSION}/src/{unit}.o")
    if raw:
        # Score the RAW compiler output. On a webfrank-pinned TU the
        # src/ object is post-rewrite (every pinned function reads
        # real 0 by construction); a whole remediation lane did an
        # edit-reconfigure-restore dance for want of this flag.
        body = base_o.parent / ".postprocess" / "body" / base_o.name
        if body.is_file():
            base_o = body
            print(f"[--raw: scoring {body} (pre-webfrank compiler"
                  " output)]")
        else:
            print("[--raw: no .postprocess/body object — this TU has no"
                  " postprocessor stage; plain object is already raw]")

    # rebuild the base object if the source is newer (stale-object trap)
    if not no_build:
        src = next((Path(f"src/{unit}{ext}") for ext in (".c", ".cpp")
                    if Path(f"src/{unit}{ext}").exists()), None)
        if src and (not base_o.exists()
                    or src.stat().st_mtime > base_o.stat().st_mtime):
            r = subprocess.run(["ninja", str(base_o)], capture_output=True, text=True)
            if r.returncode != 0:
                print(f"NINJA FAILED rebuilding {base_o}:")
                tail = (r.stdout + r.stderr).splitlines()
                print(chr(10).join(tail[-15:]))
                return 1
            print(f"(rebuilt {base_o.name})")

    for p in (target_o, base_o):
        if not p.exists():
            print(f"missing: {p} (run ninja / check unit path)")
            return 1

    target, base = parse(target_o), parse(base_o)

    def resolve_name(name):
        """Requested-name resolution across the suffix convention: try the
        raw spelling first (collision-kept names like dtor_800DB21C stay
        suffixed in parse output), then the stripped form. Unconditional
        stripping made both dtors unreachable by their own names."""
        if name in target or name in base:
            return name
        if not name.startswith("fn_"):
            stripped = re.sub(r"_80[0-9A-Fa-f]{6}$", "", name)
            if stripped in target or stripped in base:
                return stripped
        return name

    names = [resolve_name(name) for name in args[1:]] or sorted(
        set(target) | set(base), key=lambda n: list(target).index(n) if n in target else 999
    )

    for name in names:
        t, b = target.get(name), base.get(name)
        if t is None or b is None:
            if t is None and b is None:
                print(f"MISSING-IN-BOTH  {name}")
                continue
            if classify_only:
                category = "BASE_ONLY" if t is None else "TARGET_ONLY"
                print(f"{category:<19} {name}")
                continue
            side = "target" if t is None else "base"
            print(f"ONLY-IN-{'BASE' if t is None else 'TARGET'}  {name}"
                  f"  (extra {side} fns are usually deadstripped statics)")
            continue
        if relocs_only:
            # First-class relocation-symbol-set delta with addresses resolved
            # from symbols.txt (run 34 item 4). Runs even at real 0: `real`
            # DROPS every reloc line and `--clean` NORMALIZES pool names, so a
            # wrong-callee/wrong-datum relocation is invisible to every other
            # view here.
            relocs_diff(name, reloc_rows_from_lines(t),
                        reloc_rows_from_lines(b))
            continue
        if t == b:
            if classify_only:
                print(f"EXACT               {name}")
            elif clean:
                print(f"== {name}: EXACT, 0 real diff lines")
            elif list_only or args[1:]:
                print(f"OK   {name}")
            continue
        if classify_only:
            category = classify_function(t, b)
            ti = len(instruction_lines(t))
            bi = len(instruction_lines(b))
            print(f"{category:<19} {name}  insns {ti}/{bi}")
            continue
        if list_only:
            print(f"DIFF {name}")
            continue
        if count_only:
            diff = [l for l in difflib.unified_diff(t, b, lineterm="", n=0)
                    if l[:1] in "+-" and l[:3] not in ("+++", "---")]
            real = count_real(diff)
            ti = sum(1 for l in t if "R_PPC" not in l)
            bi = sum(1 for l in b if "R_PPC" not in l)
            print(f"DIFF {name}  insns {ti}/{bi}  lines {len(diff)}  real {real}")
            continue
        if ops_only:
            ops_diff(name, t, b)
            continue
        if clean:
            clean_diff(name, t, b)
            continue
        print("=" * 20, name)
        for line in difflib.unified_diff(t, b, "target", "base", lineterm="", n=2):
            print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
