# Exact matching workflow and handoff policy (2026-08-21)

This file records the user-directed workflow for the current push to 55% exact
matched code. It overrides older notes wherever they recommend committing
handwritten assembly, raw instruction words, partial-assembly shims, worker
branches, or nonexact semantic reconstructions.

## Non-negotiable policy

- Implementations must be portable C or C++ source. Never add or retain
  handwritten assembly, inline assembly, raw instruction/data encodings, or
  generated machine-code blobs.
- Reading target disassembly is required and is not permission to copy it into
  source. Use it only to recover ABI, types, control flow, lifetimes, stack
  layout, and expression association.
- Commit only byte-exact function matches unless the user explicitly authorizes
  semantic/nonexact progress. A promising but nonexact experiment is research,
  not a retained source change.
- Preserve all existing exact functions and unrelated user changes.

## Repository and Git policy

- Work only in `W:\Repositories\GDL-Codex-Continue`.
- Treat `W:\Repositories\Gauntlet-Dark-Legacy-Decompilation` as Kimi's live,
  read-only checkout. Never edit, build, switch branches, commit, or push there.
- The only remote branch is `main`. Work on local `main`, commit audited gains
  directly, and push `main` immediately after verification.
- Do not create or push worker branches. Subagents never commit or push; the
  root agent alone stages, audits, commits, and pushes.
- The known `src/game/mb/mb_tree.c` end-of-file whitespace change is unrelated.
  Do not stage, restore, or include it in commits.
- Use plain one-line commit messages and no attribution trailers.

## Parallel-agent protocol

1. Root assigns each agent one exclusive translation unit. Agents announce the
   file and function before editing.
2. Agents screen `research/PARKED.txt`, `.claude/memory/`, retained mailbox
   history, and `git log -S<function>` before starting. Do not repeat a recorded
   allocator/scheduler cap without a genuinely new source lever.
3. Shared filesystem means edits are immediately visible. Never touch another
   agent's file or use broad restoration commands.
4. Agents perform bounded experiments with portable source only. On a cap they
   restore their own hunks completely and report the recovered source recipe
   and remaining compiler wall.
5. When an agent reaches exact, it reports the function, file, verification
   output, and source technique. Root independently audits the diff and owns the
   commit/push.

## Function workflow

1. Refresh objects/report before trusting percentages:
   `ninja build/GUNE5D/report.json`.
2. Build a candidate queue with `tools/gdl/nearmiss.py`; prefer meaningful
   structural gaps and high-byte-value functions over already documented
   register-only micro-walls.
3. Screen history and ownership. Skip stale report entries that are already
   exact and skip every documented park unless a new lever is named first.
4. Read target instructions with `tools/gdl/fnasm.py` and compare current code
   using `tools/gdl/fndiff.py --count`, `--ops`, and `--clean`.
5. Recover semantics and source architecture from target disassembly, read-only
   Ghidra decompilation/dataflow, nearby exact functions, headers, Xbox symbols,
   and call-site ABI evidence.
6. Work from structural facts first: true prototype, signedness, aggregate
   layout, source-owned globals, loop/branch shape, helper/inlining boundaries,
   declaration order, stack arrays/padding, pointer-update form, and floating
   expression association.
7. Rebuild the one object and measure after every coherent change. Two
   invariant probes on one axis kill that axis. Stop after at most three
   allocator/scheduler-only attempts and restore rather than churn.
8. Retain a function only when instruction count, opcode stream, and cleaned
   diff all prove exact.

## Commit gate

Before every commit, root must complete all of the following:

1. `fndiff.py <unit> <function> --count`
2. `fndiff.py <unit> <function> --ops`
3. `fndiff.py <unit> <function> --clean`
4. `git diff --check`
5. `ninja build/GUNE5D/ok` and confirm `main.dol: OK`
6. Stage only the intended portable source and documentation files.
7. Inspect the staged diff and reject any added handwritten assembly, raw
   instruction/data directives, opword/codeword patterns, machine-code arrays,
   or unrelated hunks.
8. Commit on local `main`, push `origin main`, then verify that the remote still
   exposes no worker branches.

## Current handoff state

- Goal: at least 55.00% overall exact matched code.
- Last verified report before this memory: 52.73%, 565,156 / 1,071,824 exact
  code bytes; 24,347 additional exact bytes were still required.
- Current main at the start of this continuation: `fb318bb0` (`Match
  init_screen2d`), pushed to `origin/main`.
- The remote was cleansed on 2026-08-21 and contained only `main` afterward.
- Always regenerate the report before quoting new progress.
- For known compiler-wall classifications and source-reachable matching laws,
  read `matching-playbook.md`. This file controls safety/Git/retention policy
  when older memories conflict.

## Handoff checklist

When stopping or transferring ownership, record: current `main` hash and remote
status; refreshed exact percentage/bytes and remaining bytes to 55%; exact
commits pushed in the session; active function/TU ownership; restored caps and
their best measured residual; any intentional dirty files; and the next three
screened, genuinely fresh candidates. Never leave an unexplained source edit.
