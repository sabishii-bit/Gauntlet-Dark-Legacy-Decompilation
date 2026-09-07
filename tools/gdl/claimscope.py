#!/usr/bin/env python3
"""Pure ownership-path helpers for explicitly supplied scope information.

There is no automatic ownership registry. Coordinate edits with the integrator
and inspect the worktree's LANE_LOCK; this module does not clear work for you.
check_unit, owned_unit_overlaps, webfrank_block_owners and audit_owned_units
require callers to supply claims explicitly. Their path-resolution behavior is
retained for existing consumers, without loading or storing project knowledge.

CLI: python tools/gdl/claimscope.py --self
"""

import json
import os
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent.parent

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


# Block scopes resolve explicitly supplied ownership most-specific-first:
# block entry, then its source unit, then the whole configuration file.
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
    """Refuse retired implicit loading rather than interpreting absence as free."""
    raise RuntimeError(
        "Automatic ownership registry is retired; coordinate scope explicitly "
        "and pass claims to the pure helpers.")



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
        # Explicit file carve-outs take precedence over directory prefixes.
        # Equal-specificity ownership remains a conflict.
        best = []
        for claim in claims:
            reach = max((len(normalize(entry))
                         for entry in claim["owned_units"]
                         if covers(entry, scope)), default=None)
            if reach is not None:
                best.append((reach, claim))
        if not best:
            return [], []
        finest = max(reach for reach, _ in best)
        owned, ours = [], []
        for reach, claim in best:
            if reach != finest:
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


def owned_unit_overlaps(claims=None, repo=REPO):
    """Report duplicate grants separately from intentional nested carve-outs.

    Explicit claims are required. check_unit resolves nested scopes by the
    most-specific path; equal paths owned by different lanes are conflicts.
    """
    claims = load_claims(repo) if claims is None else claims
    entries = [(claim.get("owner") or "", claim.get("id") or "",
                normalize(entry))
               for claim in claims for entry in claim["owned_units"]]
    duplicates, nested = {}, []
    for index, (owner_a, claim_a, entry_a) in enumerate(entries):
        for owner_b, claim_b, entry_b in entries[index + 1:]:
            if owner_a == owner_b:
                continue
            if entry_a == entry_b:
                duplicates.setdefault(entry_a, set()).update(
                    (owner_a, owner_b))
            elif covers(entry_a, entry_b) or covers(entry_b, entry_a):
                outer, inner = ((entry_a, entry_b)
                                if covers(entry_a, entry_b)
                                else (entry_b, entry_a))
                outer_owner = owner_a if outer == entry_a else owner_b
                inner_owner = owner_b if outer == entry_a else owner_a
                outer_claim = claim_a if outer == entry_a else claim_b
                inner_claim = claim_b if outer == entry_a else claim_a
                nested.append({
                    "outer": outer, "outer_owner": outer_owner,
                    "outer_claim": outer_claim,
                    "inner": inner, "inner_owner": inner_owner,
                    "inner_claim": inner_claim,
                    "resolution": (f"{inner} resolves to {inner_owner} (the"
                                   f" more specific entry); {outer_owner}"
                                   f" keeps the rest of {outer}"),
                })
    return {
        "duplicate": {entry: sorted(owners)
                      for entry, owners in sorted(duplicates.items())},
        "nested": nested,
        "note": ("`duplicate` is a real collision — two owners on one entry,"
                 " neither more specific, both refused. `nested` is a"
                 " carve-out: it resolves most-specific-first and is"
                 " advisory."),
    }


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
    """Resolve every explicitly supplied ownership entry against real paths.

    Statuses are unit (source TU), prefix (directory), file, or UNRESOLVED.
    Every entry produces a row; unresolved paths include basename suggestions.
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
                         " cannot be resolved from this supplied scope"),
            }))
    return rows


def main():
    args = sys.argv[1:]
    if args == ["--self"]:
        lane, source = lane_identity()
        print(json.dumps({"lane": lane, "source": source}, indent=2))
        return 0
    if args in (["--help"], ["-h"]):
        print(__doc__)
        return 0
    print("Automatic ownership registry is retired; coordinate scope explicitly. "
          "Only --self is available for local lane inspection.", file=sys.stderr)
    return 2



if __name__ == "__main__":
    sys.exit(main())
