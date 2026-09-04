#!/usr/bin/env python3
"""Cross-lane ownership screen for the edit loop: may THIS lane touch this unit?

Run-45's NM/BF collision cost ~18 duplicated tool calls and a duplicate exact
because two lanes worked one TU. The claim records advertised ownership, but
only as English prose in `attributes.scope`, which no tool can screen: measured
over all 250 `src/` units at c621fcbac against run-46's six live claims, the
substring screen behind `gdlmem claims --owns` fires on 20 units and 17 of them
(85%) are units no scope names -- `game/sys/main.c` matches FIVE of the six
claims on the word "main" (from the `main.dol: OK` gate line) and
`game/ui/select.c` matches the TOOL lane on "Select" (from `Select-Object`).
It also cannot read a negation: one lane's scope names another lane's three TUs
in order to EXCLUDE them, and the screen reports it as their co-owner.

So this module screens ONLY the machine-readable channel,
`attributes.owned_units` (a list of repo-relative unit paths or directory
prefixes). Three verdicts, and the distinction between the last two is the
whole point:

  ok          -- the unit is unowned, or owned by THIS lane
  foreign     -- another active claim's owned_units LISTS this unit
  undecidable -- at least one active claim carries no owned_units, so no
                 tool can clear the unit. NEVER reported as ok.

Lane identity comes from the untracked `LANE_LOCK` file AGENTS.md already
requires at every worker worktree root (first line = worker id), else
`$GDL_LANE`, else the git branch name.

WEBFRANK.JSON IS OWNED BY BLOCK, NOT BY FILE (run-48 item 6/7). It is one
file holding one block per unit, and AGENTS.md first-five-minutes trap 15b
makes re-deriving a pin the chore of whoever edits the TU -- "the upstream
freeze is a re-derivation chore, not a wall" -- with `probe --rederive-pin`
writing the file on the source lane's behalf. File-level ownership therefore
refused the one edit the workflow REQUIRES of every source lane. Reproduced
at 33a1bad50 with GDL_LANE=claude-fleet-worker-DA, the lane whose order tells
it to keep a pinned edit in game/world/items:

  claimscope.py config/GUNE5D/webfrank.json
    "status": "foreign", owner claude-fleet-worker-WF   exit 3

So `config/GUNE5D/webfrank.json#<unit>` is a first-class scope and resolves
MOST SPECIFIC FIRST: an explicit block entry in some claim's owned_units;
else the owner of <unit> itself (the source lane, by trap 15b); else the
file-level owner (the postprocessor lane, who owns the schema, the rule
bodies, and every block no source lane claims). Only the first scope that
matches anything decides. A query for the BARE file keeps file-level
semantics -- that is genuinely the postprocessor lane's.

CALIBRATED TWO-SIDED at 33a1bad50 over all 52 blocks in webfrank.json against
run-48's six active claims (T18_scratch/t18_calib_item7.py):
  POSITIVES   8 blocks resolve through the block's OWN UNIT -- audio,
              auxscreen (NC), gamemain, items (DA), mb_camera, dbgtext (FT),
              movieplayer (CU) and psfx (WF, who owns that unit as a source
              lane too). Seven of those change owner; every one was a
              foreign-claim refusal for exactly the lane trap 15b requires to
              make the edit.
  NEGATIVES  44 blocks resolve file-level and are unchanged.
File-level ownership is not merely coarse here, it is UNENFORCEABLE: the
chore is documented-unavoidable, so a screen that refuses it is a screen
every source lane learns to override.

Usage (from repo root):
  python tools/gdl/claimscope.py game/ps2/ml_fmath.c   # one unit
  python tools/gdl/claimscope.py 'config/GUNE5D/webfrank.json#game/world/items'
  python tools/gdl/claimscope.py --index               # unit -> owner map
  python tools/gdl/claimscope.py --blocks              # webfrank block owners
  python tools/gdl/claimscope.py --self                # who am I
  python tools/gdl/claimscope.py --audit               # every owned_units
                                                       # entry, one row each

Exit 0 ok / 3 foreign / 0 undecidable (a warning, never a hard stop: the
field's coverage was 0 of 6 active claims when it shipped, and a gate that
refuses on absence would refuse every lane on day one).

IMPORTABLE CORE: load_claims, check_unit, lane_identity, normalize,
split_block, resolution_scopes, webfrank_units, webfrank_block_owners --
pure over the record JSON, no database build and no compile.
"""

import json
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent
RECORD_DIRS = ("memory_graph/records", "memory_graph/inbox")

FOREIGN_EXIT = 3


def normalize(value):
    """Canonical spelling of a unit path: no src/, no extension, POSIX."""
    text = str(value or "").strip().replace("\\", "/").strip("/")
    while text.startswith("./"):
        text = text[2:]
    if text.startswith("src/"):
        text = text[4:]
    for ext in (".cpp", ".c", ".o", ".s"):
        if text.endswith(ext):
            return text[: -len(ext)]
    return text


def covers(entry, unit):
    entry, unit = normalize(entry), normalize(unit)
    if not entry or not unit:
        return False
    return unit == entry or unit.startswith(entry + "/")


# config/GUNE5D/webfrank.json is ONE FILE holding one BLOCK PER UNIT, and
# AGENTS.md (first-five-minutes trap 15b) makes re-deriving a pin the chore
# of whoever edits the TU: "the upstream freeze is a re-derivation chore, not
# a wall", and `probe --rederive-pin` writes this file on the source lane's
# behalf. File-level ownership therefore refuses the one edit the workflow
# REQUIRES of every source lane. Reproduced at 33a1bad50 with
# GDL_LANE=claude-fleet-worker-DA (which owns game/world/items and whose
# order tells it to keep a pinned edit):
#
#   claimscope.py config/GUNE5D/webfrank.json
#     "status": "foreign", owner claude-fleet-worker-WF   exit 3
#
# So the file resolves BY BLOCK. `config/GUNE5D/webfrank.json#<unit>` is a
# first-class scope, and it resolves through three levels, most specific
# first: an explicit block entry in some claim's owned_units; else the owner
# of <unit> ITSELF (the source lane, by trap 15b); else the file-level owner
# (WF, who owns the schema, the rule bodies and every block no source lane
# claims). A query for the BARE file keeps file-level semantics — that is
# genuinely WF's.
BLOCK_SEPARATOR = "#"


def split_block(unit):
    """(path, block) for a `path#block` scope, else (path, None)."""
    text = str(unit or "").strip().replace("\\", "/").strip("/")
    if BLOCK_SEPARATOR not in text:
        return text, None
    head, _, tail = text.partition(BLOCK_SEPARATOR)
    block = normalize(tail)
    return head.strip().strip("/"), (block or None)


def resolution_scopes(unit):
    """The scopes a query resolves through, MOST SPECIFIC FIRST.

    A bare unit resolves against itself. A block scope resolves against the
    explicit block entry, then the block's own unit, then the file.
    """
    path, block = split_block(unit)
    if block is None:
        return [path]
    return [f"{path}{BLOCK_SEPARATOR}{block}", block, path]


def load_claims(repo=REPO):
    """Every ACTIVE work_claim on disk, newest spelling of each id.

    Reads the JSON directly rather than the SQLite graph: this runs inside
    probe.py's inner loop, and `ensure_database` can spend ~30s rebuilding
    after any tools/gdl edit -- which is exactly when a lane is probing.
    """
    out = {}
    for rel in RECORD_DIRS:
        base = Path(repo) / rel
        if not base.is_dir():
            continue
        for path in base.rglob("work_claim.*.json"):
            try:
                rec = json.loads(path.read_text(encoding="utf-8"))
            except (OSError, ValueError):
                continue
            if rec.get("kind") != "work_claim":
                continue
            if rec.get("state") not in (None, "active"):
                continue
            if rec.get("released_at"):
                continue
            attrs = rec.get("attributes") or {}
            raw = attrs.get("owned_units")
            units = []
            if isinstance(raw, list):
                for item in raw:
                    norm = normalize(item) if isinstance(item, str) else ""
                    if norm and norm not in units:
                        units.append(norm)
            out[rec.get("id") or str(path)] = {
                "id": rec.get("id"),
                "owner": rec.get("owner"),
                "owned_units": units,
                "declared": isinstance(raw, list) and bool(raw),
                "path": str(path.relative_to(repo)),
            }
    return list(out.values())


def lane_identity(repo=REPO):
    """This worktree's lane id, and where it came from."""
    lock = Path(repo) / "LANE_LOCK"
    if lock.is_file():
        for line in lock.read_text(encoding="utf-8",
                                   errors="replace").splitlines():
            if line.strip():
                return line.strip(), "LANE_LOCK"
    env = os.environ.get("GDL_LANE")
    if env and env.strip():
        return env.strip(), "$GDL_LANE"
    try:
        branch = subprocess.run(["git", "branch", "--show-current"],
                                cwd=str(repo), capture_output=True,
                                text=True).stdout.strip()
    except OSError:
        branch = ""
    return (branch, "git branch") if branch else ("", "unknown")


def check_unit(unit, lane=None, claims=None, repo=REPO):
    """Verdict dict for one unit. Pure once `claims` is supplied."""
    claims = load_claims(repo) if claims is None else claims
    if lane is None:
        lane = lane_identity(repo)[0]
    lane_l = (lane or "").strip().lower()

    def partition(scope):
        owned, ours = [], []
        for claim in claims:
            if not any(covers(entry, scope)
                       for entry in claim["owned_units"]):
                continue
            owner = (claim.get("owner") or "").strip()
            if owner and (owner.lower() == lane_l
                          or owner.lower() in lane_l
                          or (lane_l and lane_l in owner.lower())):
                ours.append(claim)
            else:
                owned.append(claim)
        return owned, ours

    # MOST SPECIFIC SCOPE WINS, and only the first scope that matches
    # anything decides — a block owned by the source lane must not be
    # overruled by the file's owner further down the list.
    owners, mine, resolved_by = [], [], None
    for scope in resolution_scopes(unit):
        owners, mine = partition(scope)
        if owners or mine:
            resolved_by = scope
            break
    blind = [c for c in claims if not c["declared"]]
    if owners:
        status = "foreign"
    elif mine:
        # An explicit self-listing SETTLES the question. Calibration caught
        # this: without the clause a lane was told "undecidable" about the
        # very unit its own claim lists, because some other lane's claim had
        # no list -- an undecidable verdict on one's own property is noise
        # that trains the reader to ignore the screen.
        status = "ok"
    elif blind:
        status = "undecidable"
    else:
        status = "ok"
    verdict = {
        "unit": normalize(unit),
        "lane": lane,
        "status": status,
        "owners": [{"owner": c["owner"], "claim": c["id"]} for c in owners],
        "own_claims": [c["id"] for c in mine],
        "active_claims": len(claims),
        "claims_without_owned_units": len(blind),
    }
    _path, block = split_block(unit)
    if block is not None:
        verdict["block"] = block
        verdict["resolved_by"] = resolved_by
        verdict["resolution_scopes"] = resolution_scopes(unit)
    return verdict


WEBFRANK_CONFIG = "config/GUNE5D/webfrank.json"


def webfrank_units(repo=REPO):
    """Every unit with a block in webfrank.json, in file order."""
    path = Path(repo) / WEBFRANK_CONFIG
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return []
    units = data.get("units")
    return list(units) if isinstance(units, dict) else []


def webfrank_block_owners(claims=None, repo=REPO):
    """{unit: {owner, resolved_by}} for every block in webfrank.json.

    The whole point of the block model, made visible: a source lane sees its
    OWN name against the TU it is working, instead of one file-level owner
    over a file the workflow requires it to edit.
    """
    claims = load_claims(repo) if claims is None else claims
    out = {}
    for unit in sorted(webfrank_units(repo)):
        scope = f"{WEBFRANK_CONFIG}{BLOCK_SEPARATOR}{unit}"
        verdict = check_unit(scope, lane="", claims=claims, repo=repo)
        owners = sorted({o["owner"] for o in verdict["owners"] if o["owner"]})
        out[unit] = {
            "owners": owners or ["<unclaimed>"],
            "resolved_by": verdict.get("resolved_by"),
        }
    return out


def warn_or_refuse(unit, tool, repo=REPO, enforce=True, stream=None):
    """Print the screen's verdict; return an exit code (0 = proceed).

    `enforce` off (or GDL_CLAIM_OVERRIDE=1 in the environment) downgrades a
    foreign hit to a warning -- the escape hatch for the integrator, who
    legitimately edits every lane's TUs while merging.
    """
    stream = stream or sys.stderr
    if os.environ.get("GDL_CLAIM_SCREEN") == "off":
        return 0
    verdict = check_unit(unit, repo=repo)
    if verdict["status"] == "foreign":
        names = ", ".join(f"{o['owner']} ({o['claim']})"
                          for o in verdict["owners"])
        print(f"[{tool}] CLAIM CONFLICT: {verdict['unit']} is listed in"
              f" attributes.owned_units of an active claim owned by {names};"
              f" this worktree's lane is {verdict['lane'] or '<unknown>'}.",
              file=stream)
        if verdict.get("block") is not None:
            print(f"[{tool}] (block scope: resolved by"
                  f" {verdict['resolved_by']!r}, the most specific of"
                  f" {verdict['resolution_scopes']}. A webfrank block is"
                  " owned by whoever owns its UNIT — the pin re-derivation is"
                  " that lane's chore per AGENTS.md trap 15b — and falls back"
                  " to the file's owner only when no lane claims the unit.)",
                  file=stream)
        if enforce and os.environ.get("GDL_CLAIM_OVERRIDE") != "1":
            print(f"[{tool}] REFUSING. AGENTS.md: a foreign active claim is a"
                  " VETO on its entire scope. Coordinate through the"
                  " integrator, or set GDL_CLAIM_OVERRIDE=1 if you ARE the"
                  " owner under a different id.", file=stream)
            return FOREIGN_EXIT
        print(f"[{tool}] (override in effect — proceeding)", file=stream)
        return 0
    if verdict["status"] == "undecidable":
        print(f"[{tool}] claim screen UNDECIDABLE for {verdict['unit']}:"
              f" {verdict['claims_without_owned_units']} of"
              f" {verdict['active_claims']} active claim(s) carry no"
              " attributes.owned_units, so no tool can clear this unit."
              " Ask the integrator to list owned units on the claims.",
              file=stream)
    return 0


def real_units(repo=REPO):
    """Every unit path that actually exists as a source file in this tree."""
    base = Path(repo) / "src"
    out = set()
    if base.is_dir():
        for path in base.rglob("*"):
            if path.suffix in (".c", ".cpp") and path.is_file():
                out.add(normalize(path.relative_to(base).as_posix()))
    return out


AUDIT_STATUSES = ("unit", "prefix", "file", "UNRESOLVED")


def audit_owned_units(claims=None, repo=REPO):
    """EVERY owned-unit entry, with the status of what it resolves to.

    A claim's `owned_units` list is the ONLY channel the tools screen
    (AGENTS.md, run 46), so an entry that names nothing protects nothing —
    silently, because a unit no claim covers reads as FREE. Measured in run
    50: the PR lane's seed TU was unprotected because its claim said
    `game/ps2/ml_mem.c` while the file is `src/game/sys/ml_mem.c`; the
    directory is wrong and the basename is right, which is exactly the shape
    a basename lookup catches and no existing check looked for.

    THE ROW LIST IS TOTAL (run-54 item 1). Until now this returned rows only
    for entries that were NOT ordinary units — a resolving entry produced no
    row at all — so `--audit` printed a header counting every entry beside a
    row list counting a subset, and the arithmetic did not close. Two run-53
    lanes reported the same confusion from opposite sides: NC saw
    `owned_unit_entries: 10 ... 4 rows`, WR saw `owned_unit_entries: 14 with
    the same 4 rows`, because the rows are insensitive to exactly the entries
    that changed. A reader cannot distinguish "the other entries were checked
    and are fine" from "the audit never looked at them", and the second is
    what the note's own warning is about. So every entry gets a row, and the
    per-status tally in `--audit` sums to `owned_unit_entries` by
    construction.

    Statuses: `unit` (a real source unit under src/), `prefix` (an existing
    DIRECTORY — `tools/gdl`, `memory_graph` — a legitimate entry that names
    no source file, never a miss), `file` (an existing path that is neither,
    e.g. `config/GUNE5D/webfrank.json`, which the old code labelled `prefix`
    although nothing is under it), and `UNRESOLVED` (names nothing here).

    CALIBRATED TWO-SIDED over all 432 distinct work_claim versions in git
    history (112 owned-unit entries; accepted claims are deleted, so the live
    corpus is only 6 claims / 9 entries):
      POSITIVES  80 entries (71%) had no row at all and now report `unit`;
                 12 more reported `prefix` while being FILES, two distinct
                 paths (`config/GUNE5D/webfrank.json`, `tools/gdl/
                 webfrank.py`) — both of them WV's, i.e. the mislabel lands
                 on the postprocessor lane's scope every run.
      NEGATIVES  17 true directory prefixes keep `prefix` unchanged, and the
                 3 UNRESOLVED entries (`game/ps2/ml_mem` — the run-50 defect
                 — and `dolphin/demo`) keep their status, their
                 `did_you_mean` and the exit-1 verdict unchanged.
    """
    claims = claims if claims is not None else load_claims(repo)
    units = real_units(repo)
    by_base = {}
    for unit in units:
        by_base.setdefault(unit.rsplit("/", 1)[-1], []).append(unit)
    rows = []
    for claim in claims:
        for entry in claim["owned_units"]:
            key = normalize(entry)
            path, _block = split_block(key)
            path = normalize(path)
            row = {"owner": claim["owner"], "claim": claim["id"],
                   "entry": entry}
            if path in units:
                rows.append(dict(row, status="unit", resolves_to=path))
                continue
            if (Path(repo) / path).is_dir():
                rows.append(dict(row, status="prefix", resolves_to=path))
                continue
            existing = next((p for p in (key, path)
                             if (Path(repo) / p).exists()), None)
            if existing is not None:
                rows.append(dict(row, status="file", resolves_to=existing))
                continue
            base = path.rsplit("/", 1)[-1]
            rows.append(dict(row, **{
                "status": "UNRESOLVED",
                "did_you_mean": sorted(by_base.get(base, [])),
                "note": ("this entry names no source file and no directory in"
                         " this tree, so every unit it was meant to protect"
                         " reads as FREE to every tool that screens"
                         " owned_units"),
            }))
    return rows


def main():
    args = [a for a in sys.argv[1:]]
    if "--self" in args:
        lane, source = lane_identity()
        print(json.dumps({"lane": lane, "source": source}, indent=2))
        return 0
    claims = load_claims()
    if "--index" in args:
        index = {}
        for claim in claims:
            for entry in claim["owned_units"]:
                index.setdefault(entry, []).append(claim["owner"])
        print(json.dumps({
            "active_claims": len(claims),
            "claims_without_owned_units":
                sum(1 for c in claims if not c["declared"]),
            "owned_units_index": {k: sorted(set(v))
                                  for k, v in sorted(index.items())},
            "conflicts": {k: sorted(set(v)) for k, v in sorted(index.items())
                          if len(set(v)) > 1},
            "webfrank_blocks": webfrank_block_owners(claims),
        }, indent=2))
        return 0
    if "--blocks" in args:
        print(json.dumps(webfrank_block_owners(claims), indent=2))
        return 0
    if "--audit" in args:
        rows = audit_owned_units(claims)
        bad = [row for row in rows if row["status"] == "UNRESOLVED"]
        entries = sum(len(c["owned_units"]) for c in claims)
        by_status = {name: sum(1 for r in rows if r["status"] == name)
                     for name in AUDIT_STATUSES}
        print(json.dumps({
            "active_claims": len(claims),
            "claims_without_owned_units":
                sum(1 for c in claims if not c["declared"]),
            "owned_unit_entries": entries,
            "by_status": by_status,
            "rows_printed": len(rows),
            # The row list is TOTAL (run-54 item 1): this is the arithmetic
            # whose failure to close was the reported symptom, asserted here
            # rather than left to the reader.
            "accounting_closes": len(rows) == entries == sum(
                by_status.values()),
            "unresolved": len(bad),
            "rows": rows,
            "note": ("every entry gets a row: `unit` resolves to a source"
                     " unit, `prefix` to a directory, `file` to an existing"
                     " non-source path, and an UNRESOLVED entry protects"
                     " nothing — the units it was meant to cover read as FREE"
                     " to probe.py and defake_gate.py. A claim with NO"
                     " owned_units makes every unit UNDECIDABLE, never free."
                     " Fix the claim before dispatching."),
        }, indent=2))
        return 1 if bad else 0
    units = [a for a in args if not a.startswith("-")]
    if not units:
        print(__doc__)
        return 1
    worst = 0
    for unit in units:
        verdict = check_unit(unit, claims=claims)
        print(json.dumps(verdict, indent=2))
        if verdict["status"] == "foreign":
            worst = FOREIGN_EXIT
    return worst


if __name__ == "__main__":
    sys.exit(main())
