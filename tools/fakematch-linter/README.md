# fakematch-lint

Review-only C/C++ reconstruction-debt scanner for decompilation projects.
It flags source constructs that commonly stand in for missing types, data or
control flow ("fakematching") so they can be reviewed against the target
binary. It never rewrites source, and a finding is a review candidate, not
proof of a fakematch.

The scanner tokenises and parses every file in-process with tree-sitter's C++
grammar and runs the rules in parallel. A full scan of a ~100-file game tree
takes about a second; the watch mode rescans only files whose bytes changed.

## Rules

| Rule  | Title                                                    | Basis              |
|-------|----------------------------------------------------------|--------------------|
| FM001 | Raw offset or indexed pointer cast                       | AST                |
| FM002 | Nested dereference through pointer casts                 | AST                |
| FM003 | Possible stack/allocation scaffolding                    | AST + lexical use  |
| FM004 | Float/address-shaped numeric byte array                  | byte-shape heuristic |
| FM005 | Assembly without an explicit reviewed exception          | lexical            |
| FM006 | Source-level compilation override (pragmas, attributes)  | lexical + AST      |
| FM007 | Unnamed hexadecimal expression constant                  | AST + mask filter  |
| FM008 | Configured postprocessor dependency (inventory, `--postprocessors`) | JSON config |
| FM009 | Unnamed constant-offset pointer or array access          | AST + lexical scope |

`fakematch-lint --explain FM001` prints the investigation steps, a conditional
before/after example, legitimate cases and unsafe shortcuts for a rule.
`fakematch-lint --list-rules` shows which rules the configuration enables.

Macros are scanned in a separate offset-preserving projection of `#define`
bodies, not expanded. Parser recovery regions are counted in every report; an
absence of findings inside one is not evidence of clean code.

## Install

```sh
# Into the target repository, without touching your PATH (recommended):
cargo install --path . --locked --root /path/to/decomp/tools/fakematch-lint
# or, from a git remote:
cargo install --git <repo-url> fakematch-lint --locked --root /path/to/decomp/tools/fakematch-lint
# or onto your PATH:
cargo install --path . --locked
```

With `--root`, the binary lands at `<root>/bin/fakematch-lint`; add that
directory to the repository's `.gitignore`. Building needs a Rust toolchain
(edition 2024) and a C compiler for the tree-sitter grammar.

## Configure

Run `fakematch-lint --init` in the repository root to write a documented
`fakematch.toml`, or copy [fakematch.example.toml](fakematch.example.toml).
Every key is optional; the defaults scan `src/` with all rules enabled.

```toml
schema_version = 1

[scan]
paths = ["src/game"]              # only these directories are ever reported

[rules.FM003]
trash_pattern = '(?i)^(?:trash\w*|unused\w*|pad\w*)$'

[rules.FM006]
pragma_severity = "warning"       # #pragma lines warn; attributes are errors

[policy]
exceptions = []                   # { fingerprint = "<64 hex>", reason = "..." }
pragma_allowlist = []

[postprocessors]                  # FM008
engines = [{ name = "WebFrank", path = "config/GUNE5D/webfrank.json" }]
unit_prefix = "game/"
```

Each rule has `enabled`, `severity` (`error` or `warning`) and its own knobs
(regexes, thresholds, keyword lists). The configuration file is validated
strictly: unknown keys, invalid regexes and absolute or escaping paths are
refused with exit code 2.

The repository root is `--root`, else the nearest ancestor of the current
directory containing `fakematch.toml`, else the current directory.

## Run

```sh
fakematch-lint                                  # scan the configured paths
fakematch-lint src/game/enemy/enemy.c           # one file (must be in scope)
fakematch-lint --out build/lint.json --limit 0  # full JSON report, no console cap
fakematch-lint --fail-on-findings --format github --limit 50   # CI
fakematch-lint --format problems --watch        # editor task with live rescans
fakematch-lint --rule FM001 --rule FM009        # restrict the report
fakematch-lint --postprocessors                 # include FM008
```

Output formats: `human` (default), `problems` (`path:line:col: severity FMnnn: message`,
absolute paths, for editor problem matchers), `github` (workflow annotations)
and `json` (the full report on stdout).

Exit codes: `0` scan complete; `1` unsuppressed errors with `--fail-on-findings`
or warnings with `--warnings-as-errors`; `2` missing input, invalid
configuration/policy, or an invalid suppression comment. Exit 2 is never a
clean result.

Watch mode prints `GDL_LINT_BEGIN` / `GDL_LINT_END status=N` around each
rescan (the markers are configurable) so a VS Code background task can drive
the Problems panel.

Saved configuration, policy and guidance changes are reloaded before rescanning.
Invalid edits report FM000 until corrected; the watcher never silently continues
with old rules. Unchanged settings keep the incremental source cache.
Block-scope function prototypes and local type members are not stack-array locals.

## Reviewed exceptions

Findings are never deleted; they are marked `suppressed` with the reason and
stay in the JSON report.

Inline comments must be standalone, single-line, name exact rules and give a
reason. Stale rules, unknown rules, unclosed or nested regions and overlapping
waivers of one finding fail the scan (exit 2) with the comment's location.

```c
// lint-allow-next-line FM007: FatalError status code, passed to the API verbatim
FatalError(message, 0x8000);

// lint-begin FM007: shipped audio table, final form
static const u16 table[] = { 0x0001, 0x0002 };
// lint-end FM007

// lint-file FM005: verified platform primitives   (before any code/directive)
```

`lint-allow-next-line` covers one declaration or statement including its
continuation lines, one `#define`/`#pragma`, a function header (FM006 only), or
one MWCC `asm` block/function (FM005 only). Whole function bodies are not
targets. FM000 and FM008 cannot be waived.

Policy exceptions in the configuration bind to a finding's `fingerprint`,
which is stable across line insertions but changes when the construct, its
enclosing function or its immediate token context changes. Pragma warnings
cannot be hidden by policy entries; use `--warnings-as-errors` to make them
blocking.

## Report

`--out` writes a JSON report (under the configured `report_dir`) containing
every finding with `rule`, `path`, `line`, `column`, `scope`, `message`,
`confidence`, `excerpt`, `fingerprint`, `severity`, `suppressed` and
rule-specific details (`variable`, `depth`, `directive`, `pragma_scope`,
`base`, `offset`, ...). It also carries per-file source hashes, counts by rule
and by file, parser-recovery regions, the guidance used, and hashes of the
configuration and policy so a report can be tied to exact inputs.

## Develop

```sh
cargo test          # unit tests plus the rule/suppression/CLI suites
cargo clippy --all-targets
```

Rules live in `src/rules/`, one module per rule implementing the `Rule`
trait over the shared per-file `Analysis` (tokens, both parse trees, function
spans, lexical scopes, mask operands). Adding a rule means adding a module, a
config struct in `src/config.rs`, and registering it in `src/rules/mod.rs`.
