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

--clean is the recommended iteration view: pool-name reloc noise (@N vs lbl_
for identical constants) is normalized away, every function ALWAYS ends with
a "== name: STATUS, N real diff lines" summary (so empty output can never be
mistaken for success), and mechanical hints are printed (frame delta -> the
dead-pad size to try). "MATCH (pool-name noise only)" = byte-identical after
link.

--ops collapses each function to its opcode stream (registers, operands and
relocs ignored) and prints only the structurally inserted/deleted/replaced
clusters. Use it to separate real shape differences (missing statements,
moved blocks, extra calls) from register-renumber noise -- this view is what
located infblock's missing t<19 clamp and stripped error-path frees.

--count prints one summary line per function (target/base insn counts, total
diff lines, and "real" diff lines excluding reloc-name-only noise) -- use it
as the per-iteration score instead of piping through grep -c.

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

BRANCH_RE = re.compile(
    r"\b(b|bl|ba|bla|beq|bne|bgt|blt|bge|ble|bso|bns|bdnz|bdz)"
    r"([+-]?)\s+(cr\d,)?([0-9a-f]+)\s*$"
)
REGISTER_RE = re.compile(r"\b(?:r(?:[12]?\d|3[01])|f(?:[12]?\d|3[01])|cr[0-7])\b")
PRIVATE_DATA_RE = re.compile(r"^\.{3}(?:rodata|data)\.\d+$")


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
    funcs = {}
    cur = None
    cur_start = 0
    for line in out.splitlines():
        m = re.match(r"^([0-9a-f]+) <(.+)>:$", line)
        if m:
            cur_start = int(m.group(1), 16)
            cur = m.group(2)
            if not cur.startswith("fn_"):
                cur = re.sub(r"_80[0-9A-Fa-f]{6}$", "", cur)
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
            # target "changed_80345368" pairs with our "changed"
            rel = re.sub(r"_80[0-9A-Fa-f]{6}(?=$|\+)", "", rel)
            for private, named in aliases.items():
                rel = re.sub(
                    rf"(?<=\s){re.escape(private)}(?=$|[+-])", named, rel
                )
            if rel.startswith("R_PPC_REL24") and funcs[cur]:
                funcs[cur][-1] = re.sub(r"<fn\+0x-?[0-9a-f]+>", "<reloc>",
                                        funcs[cur][-1])
            funcs[cur].append("    " + rel)
    return funcs


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
    return reloc_type, symbol


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


def frame_size(lines):
    for ln in lines:
        m = FRAME_RE.search(ln)
        if m:
            return int(m.group(1))
    return None


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
        hints.append(f"frame delta {delta:+d} -> try `u8 unused[{abs(delta)}]`"
                     if delta > 0 else
                     f"frame delta {delta:+d} -> our frame is BIGGER; drop {-delta}B of pad/locals")
    cat = classify_function(t, b)
    status = "MATCH (pool-name noise only)" if real == 0 and raw else \
             "EXACT" if real == 0 else cat
    hint_s = ("  HINT: " + "; ".join(hints)) if hints else ""
    noise_s = f" (+{noise} pool-name lines suppressed)" if noise else ""
    print(f"== {name}: {status}, {real} real diff lines{noise_s}{hint_s}")


def ops_diff(name, t, b):
    to, bo = opcodes(t), opcodes(b)
    sm = difflib.SequenceMatcher(None, to, bo, autojunk=False)
    clusters = [x for x in sm.get_opcodes() if x[0] != "equal"]
    print(f"==== {name}: target {len(to)} insns, ours {len(bo)}"
          + (" (opcode streams identical -- diffs are register/reloc only)"
             if not clusters else ""))
    # Multiset verdict: IDENTICAL means every difference is pure reorder
    # (SCHEDULE class); any +/- means something structural is hiding in the
    # diff even when insn counts look close.
    delta = Counter(to) - Counter(bo), Counter(bo) - Counter(to)
    if not delta[0] and not delta[1]:
        print(f"  opcode multiset: IDENTICAL ({len(to)}/{len(bo)})"
              " -- pure reorder, schedule-class residual")
    else:
        gains = " ".join(f"+{n} {op}" for op, n in sorted(delta[0].items()))
        losses = " ".join(f"-{n} {op}" for op, n in sorted(delta[1].items()))
        print(f"  opcode multiset: DIFFERS  target-only: {gains or '(none)'}"
              f"  ours-only: {losses or '(none)'}")
    # @0x offsets are function-relative byte offsets (index*4), directly
    # usable as fnasm.py 0xA:0xB slices on the target (T) / --ours (O) dumps.
    for tag, i1, i2, j1, j2 in clusters:
        print(f"  {tag:7} T[{i1}:{i2}]@{i1 * 4:x}-{i2 * 4:x}={to[i1:i2]}"
              f"  O[{j1}:{j2}]@{j1 * 4:x}-{j2 * 4:x}={bo[j1:j2]}")


def main():
    flags = ("-l", "--ops", "--count", "--classify", "--no-build", "--clean")
    args = [a for a in sys.argv[1:] if a not in flags]
    list_only = "-l" in sys.argv
    ops_only = "--ops" in sys.argv
    count_only = "--count" in sys.argv
    classify_only = "--classify" in sys.argv
    no_build = "--no-build" in sys.argv
    clean = "--clean" in sys.argv
    if not args or args[0] in ("--help", "-h", "help"):
        print(__doc__)
        return 1

    unit = args[0].replace("\\", "/").strip("/")
    if unit.startswith("src/"):
        unit = unit[len("src/"):]
    unit = re.sub(r"\.(c|cpp)$", "", unit)
    target_o = Path(f"build/{VERSION}/obj/{unit}.o")
    base_o = Path(f"build/{VERSION}/src/{unit}.o")

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
    names = [re.sub(r"_80[0-9A-Fa-f]{6}$", "", name)
             if not name.startswith("fn_") else name
             for name in args[1:]] or sorted(
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
            real = [l for l in diff if "R_PPC" not in l]
            ti = sum(1 for l in t if "R_PPC" not in l)
            bi = sum(1 for l in b if "R_PPC" not in l)
            print(f"DIFF {name}  insns {ti}/{bi}  lines {len(diff)}  real {len(real)}")
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
