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

Usage (from repo root):
  python tools/gdl/claimscope.py game/ps2/ml_fmath.c   # one unit
  python tools/gdl/claimscope.py --index               # unit -> owner map
  python tools/gdl/claimscope.py --self                # who am I

Exit 0 ok / 3 foreign / 0 undecidable (a warning, never a hard stop: the
field's coverage was 0 of 6 active claims when it shipped, and a gate that
refuses on absence would refuse every lane on day one).

IMPORTABLE CORE: load_claims, check_unit, lane_identity, normalize -- pure
over the record JSON, no database build and no compile.
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
    owners, mine = [], []
    for claim in claims:
        if not any(covers(entry, unit) for entry in claim["owned_units"]):
            continue
        owner = (claim.get("owner") or "").strip()
        if owner and (owner.lower() == lane_l
                      or owner.lower() in lane_l
                      or (lane_l and lane_l in owner.lower())):
            mine.append(claim)
        else:
            owners.append(claim)
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
    return {
        "unit": normalize(unit),
        "lane": lane,
        "status": status,
        "owners": [{"owner": c["owner"], "claim": c["id"]} for c in owners],
        "own_claims": [c["id"] for c in mine],
        "active_claims": len(claims),
        "claims_without_owned_units": len(blind),
    }


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
        }, indent=2))
        return 0
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
