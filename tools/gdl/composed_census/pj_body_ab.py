#!/usr/bin/env python3
"""PJ lane (run 55): A/B a TU's UN-POSTPROCESSED body object across an edit.

WHY THIS EXISTS.  `fndiff --count`/`real` cannot arbitrate an own-pool-ghost
cleanup: claim.law.own-pool-ghost-extern-flatters-fndiff-reloc-rows.20260831
.v1 shows `real` is DEFINED to get worse when you stop borrowing the target's
symbol name, and defake_gate therefore reports REGRESSION on a correct fix.
The question those tools cannot answer is the only one that matters for a
byte-parity bar: DID ANY INSTRUCTION WORD MOVE?  This answers it by building
`build/GUNE5D/src/<unit>/.postprocess/body/<name>.o` -- the object BEFORE
WebFrank runs, so no rule can mask a change -- from a git ref and from the
working tree, and comparing the two disassemblies function by function.

It is also the instrument named in the falsifier of
claim.law.PJ_replacing-an-extern-scalar-read-with-a-literal-is-not-codegen-
neutral-it-removes-a-reload-barrier.20260904.v1.

    python tools/gdl/composed_census/pj_body_ab.py game/enemy/enemy
    python tools/gdl/composed_census/pj_body_ab.py game/enemy/enemy \
        --vs-target
    python tools/gdl/composed_census/pj_body_ab.py game/game/gamemain \
        --ref ca98f65d3

Exit 0 when no instruction word moved, 1 when some did, 2 on a build or
usage failure.  `--vs-target` additionally scores both sides against
`build/GUNE5D/obj/<unit>.o` (the dtk-split TARGET), which is what decides
whether a change that DID move words moved them the right way.

IMPORTANT: this tool REWRITES the working-tree source while it runs and
restores it byte-for-byte in a finally block.  Per AGENTS.md residual-work
discipline 21, RE-READ the file in your editor before your next edit.

IMPORTABLE CORE: disassemble_words, compare_words, differing_vs_target
(pure over an object path; they never build and importing has no effect).
"""
from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))

_INSN = re.compile(r'^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} ){4})')
_LABEL = re.compile(r'^([0-9a-f]{8}) <([^>]+)>:')


def _objdump() -> str:
    """The powerpc-eabi objdump provisioned under build/."""
    for cand in (
        os.path.join(ROOT, 'build', 'binutils', 'powerpc-eabi-objdump.exe'),
        os.path.join(ROOT, 'build', 'binutils', 'powerpc-eabi-objdump'),
    ):
        if os.path.exists(cand):
            return cand
    for base, _dirs, files in os.walk(os.path.join(ROOT, 'build')):
        for name in files:
            if name.startswith('powerpc-eabi-objdump'):
                return os.path.join(base, name)
    raise SystemExit('objdump not found under build/ -- run '
                     'tools/gdl/provision_worktree.py first')


def disassemble_words(obj_path: str) -> dict[str, list[tuple[int, str, str]]]:
    """{function: [(offset, word_hex, disasm_line)]} for an object's .text."""
    out = subprocess.run([_objdump(), '-d', '-j', '.text', obj_path],
                         capture_output=True, text=True).stdout
    functions: dict[str, list[tuple[int, str, str]]] = {}
    current, base = None, 0
    for line in out.split('\n'):
        label = _LABEL.match(line)
        if label:
            current, base = label.group(2), int(label.group(1), 16)
            functions[current] = []
            continue
        insn = _INSN.match(line)
        if insn and current is not None:
            functions[current].append((int(insn.group(1), 16) - base,
                                       insn.group(2).replace(' ', ''),
                                       line.strip()))
    return functions


def compare_words(before: dict, after: dict) -> list[tuple]:
    """[(function, kind, detail)] for every function whose words moved."""
    moved = []
    for name in sorted(set(before) & set(after)):
        a, b = before[name], after[name]
        if len(a) != len(b):
            moved.append((name, 'COUNT', (len(a), len(b), [])))
            continue
        rows = [(x[0], x[2], y[2]) for x, y in zip(a, b) if x[1] != y[1]]
        if rows:
            moved.append((name, 'WORDS', (len(a), len(b), rows)))
    return moved


def differing_vs_target(side: dict, target: dict) -> dict[str, int | None]:
    """{function: differing instruction words vs the target, None if counts
    disagree (a count-asymmetric function is not word-comparable)}."""
    scores: dict[str, int | None] = {}
    for name in sorted(set(side) & set(target)):
        a, t = side[name], target[name]
        if len(a) != len(t):
            scores[name] = None
            continue
        scores[name] = sum(1 for x, y in zip(a, t) if x[1] != y[1])
    return scores


def _body_object(unit: str) -> str:
    directory, name = os.path.split(unit)
    return os.path.join('build', 'GUNE5D', 'src', directory,
                        '.postprocess', 'body', name + '.o')


def _build(rel_obj: str) -> None:
    absolute = os.path.join(ROOT, rel_obj)
    if os.path.exists(absolute):
        os.remove(absolute)
    proc = subprocess.run(['ninja', '-j2', rel_obj.replace('\\', '/')],
                          cwd=ROOT, capture_output=True, text=True)
    if not os.path.exists(absolute):
        sys.stdout.write(proc.stdout[-4000:])
        sys.stderr.write(proc.stderr[-4000:])
        raise SystemExit(2)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    parser.add_argument('unit', help='e.g. game/enemy/enemy')
    parser.add_argument('--ref', default='HEAD',
                        help='git ref for the BEFORE side (default HEAD)')
    parser.add_argument('--vs-target', action='store_true',
                        help='also score both sides against the dtk-split '
                             'target object')
    args = parser.parse_args()

    unit = args.unit.replace('\\', '/')
    if unit.startswith('src/'):
        unit = unit[4:]
    if unit.endswith('.c') or unit.endswith('.cpp'):
        unit = unit.rsplit('.', 1)[0]
    source_rel = 'src/' + unit + '.c'
    source_abs = os.path.join(ROOT, source_rel)
    if not os.path.exists(source_abs):
        raise SystemExit('no such source: ' + source_rel)
    rel_obj = _body_object(unit)

    working = open(source_abs, 'rb').read()
    show = subprocess.run(['git', 'show', '%s:%s' % (args.ref, source_rel)],
                          cwd=ROOT, capture_output=True)
    if show.returncode:
        sys.stderr.write(show.stderr.decode('utf-8', 'replace'))
        raise SystemExit(2)
    reference = show.stdout
    if reference == working:
        print('the working tree already equals %s for %s -- nothing to A/B'
              % (args.ref, source_rel))
        return 0

    try:
        with open(source_abs, 'wb') as handle:
            handle.write(reference)
        os.utime(source_abs, None)
        _build(rel_obj)
        before = disassemble_words(os.path.join(ROOT, rel_obj))
        with open(source_abs, 'wb') as handle:
            handle.write(working)
        os.utime(source_abs, None)
        _build(rel_obj)
        after = disassemble_words(os.path.join(ROOT, rel_obj))
    finally:
        with open(source_abs, 'wb') as handle:
            handle.write(working)
        os.utime(source_abs, None)
        print('[%s RESTORED to the working-tree bytes (%d) -- RE-READ IT '
              'BEFORE YOUR NEXT EDIT]' % (source_rel, len(working)))

    print('unit %s: %s vs working tree, %d function(s) in both objects'
          % (unit, args.ref, len(set(before) & set(after))))
    moved = compare_words(before, after)
    for name, kind, (count_a, count_b, rows) in moved:
        if kind == 'COUNT':
            print('  %-28s INSTRUCTION COUNT %d -> %d' % (name, count_a,
                                                          count_b))
            continue
        print('  %-28s %d word(s) moved; first +0x%x' % (name, len(rows),
                                                         rows[0][0]))
        for offset, old, new in rows[:6]:
            print('       +0x%-6x %s | %s' % (offset, old, new))
    print('functions with moved instruction words: %d' % len(moved))

    if args.vs_target:
        target_path = os.path.join(ROOT, 'build', 'GUNE5D', 'obj',
                                   *unit.split('/')) + '.o'
        if not os.path.exists(target_path):
            print('SKIP --vs-target: missing %s (run '
                  'provision_worktree.py --resplit)' % target_path)
        else:
            target = disassemble_words(target_path)
            score_a = differing_vs_target(before, target)
            score_b = differing_vs_target(after, target)
            names = [n for n in score_a
                     if score_a[n] is not None and score_b.get(n) is not None]
            total_a = sum(score_a[n] for n in names)
            total_b = sum(score_b[n] for n in names)
            for name in names:
                if score_a[name] != score_b[name]:
                    print('  %-28s vs target %5d -> %-5d %s'
                          % (name, score_a[name], score_b[name],
                             'BETTER' if score_b[name] < score_a[name]
                             else 'WORSE'))
            print('raw differing words vs target: %d -> %d (%+d) over %d '
                  'word-comparable function(s)'
                  % (total_a, total_b, total_b - total_a, len(names)))
            print('functions at zero differing words: %d -> %d'
                  % (sum(1 for n in names if score_a[n] == 0),
                     sum(1 for n in names if score_b[n] == 0)))
    return 1 if moved else 0


if __name__ == '__main__':
    raise SystemExit(main())
