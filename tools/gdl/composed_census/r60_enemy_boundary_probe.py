#!/usr/bin/env python3
"""Scratch-only reconstruction of the enemy/gamemain boundary.

No production source, split, or rule is edited. Baseline raw-object fidelity
is mandatory on BOTH TUs; report all function additions/removals and changed
words. Generated candidates need relocation, postprocessor and linked gates
before retention. This is a migration experiment, not proof of equivalence.

Generation is for the pre-migration checkout a0401ee3c. After migration,
--verify-installed compares against the saved baseline objects in build/.
"""
import argparse
import difflib
import hashlib
import json
import re
import shlex
import subprocess

from cv_probe import REPO, compile_with, read_edges
from pj_body_ab import compare_words, disassemble_words
from probe import function_span
from fndiff import parse

ENEMY = "game/enemy/enemy"
GAME = "game/game/gamemain"
MOVED = """init_enemy_vars format_brain SetEnemyObj fn_800508A0
fn_80050910 AllocEnemy LoadEnemy fn_80050DD8 GetEnemyType fn_800510A4
fn_80051164 fn_800511D0 fn_80051480 fn_80051568 fn_800516F8 fn_80051C78
fn_80051E1C EnemyTypePrefix EnemyTypeDesc EnemyDescType fn_8005207C
fn_800520C8 PlayersAverageLevel findWorldName""".split()


def excerpt(source, name):
    span = function_span(source, name)
    if span is None:
        raise ValueError("missing pre-migration function " + name + "; generation requires the a0401ee3c source layout")
    start, end = span
    return "".join(source.splitlines(keepends=True)[start:end])


def changed_rows(before, after, expected):
    changed = compare_words(before, after)
    return {"functions": len(after), "missing": sorted(expected - set(after)),
            "unexpected": sorted(set(after) - expected),
            "changed": [{"function": n, "kind": k, "before_insns": d[0],
                         "after_insns": d[1], "words": len(d[2])}
                        for n, k, d in changed]}


def migrate(enemy, game):
    bodies = {name: excerpt(game, name) for name in MOVED}
    # The 22 externally emitted functions follow the verified target order.
    # Both existing inline helpers are visible before their callers.
    ordered = ["PlayersAverageLevel", "findWorldName"] + MOVED[:-2]
    original_moved = "\n".join(bodies[name] for name in ordered)
    declarations = []
    for match in re.finditer(r'^(?:DECL_SECT\("[^"]*"\)\s*)?extern\s+[^;{}]+;', game, re.M):
        decl = match.group()
        clean = re.sub(r"/\*.*?\*/", "", decl, flags=re.S)
        clean = re.sub(r'^DECL_SECT\("[^"]*"\)\s*', '', clean)
        name_match = re.search(r"(\w+)\s*(?:\([^;]*|\[[^;]*)?;$", clean)
        if not name_match:
            continue
        name = name_match.group(1)
        if re.search(r"\b" + name + r"\b", original_moved) and not re.search(r"\b" + name + r"\b", enemy):
            if decl not in declarations:
                declarations.append(decl)
    preamble = '\n/* Enemy loading, targeting and milestone tail: recovered TU ownership. */\n#include "game/item.h"\n'
    for name in ("Row36", "MilestoneParam", "MilestonePool"):
        match = re.search(r"typedef struct " + name + r"\s*\{.*?\}\s*" + name + r";", game, re.S)
        if not match:
            raise ValueError("missing type " + name)
        preamble += match.group() + "\n"
    preamble += "\n".join(declarations) + "\n"
    preamble += 'static char sBossGenName[] = "BOSSGEN";\n'
    preamble += "extern s32 fn_80051480(f32* pos);\nextern char* fn_80051E1C(s32 world, s32 lvl, s32 flag);\n"
    preamble += next(line for line in game.splitlines() if line.startswith("#define DIST3")) + "\n"
    for name in ordered:
        body = bodies[name]
        # Reconcile existing views, without adding a new linker symbol or
        # altering the declared objects used by the old enemy functions.
        body = re.sub(r"\bgPlayers\b", "gPlayers.players", body)
        body = re.sub(r"\blbl_8011AF48\b", "((Row36*)lbl_8011AF48)", body)
        body = re.sub(r"\bgWadAtreeHeaders\b", "((void**)gWadAtreeHeaders)", body)
        body = re.sub(r"\blbl_80112370\b", "((char*)lbl_80112370)", body)
        if name == "init_enemy_vars":
            # Retail +0x254 calls format_brain with incoming r3 (slot)
            # untouched since entry. The old cross-TU no-argument call
            # never represented that dependency in C.
            body = body.replace("format_brain();", "format_brain(slot);")
        if name == "fn_800508A0":
            body = body.replace("    s32 i;", "    s32* pool = lbl_80250E00;\n    s32 i;")
            body = body.replace("lbl_80250E00[", "pool[")
            body = body.replace("        idx = pool[8 + i];", "        s32* index_row = pool + i;\n        s32* resource_row;\n        idx = index_row[8];\n        resource_row = pool + idx;")
            body = body.replace("pool[345 + idx]", "resource_row[345]").replace("pool[300 + idx]", "resource_row[300]")
            body = "#pragma opt_propagation off\n" + body + "#pragma opt_propagation reset\n"
        if name in ("fn_800508A0", "fn_8005207C"):
            body = "#pragma dont_inline on\n" + body + "#pragma dont_inline off\n"
        if name == "fn_800511D0":
            body = "#pragma opt_common_subs off\n#pragma opt_propagation off\n" + body + "#pragma opt_propagation reset\n#pragma opt_common_subs on\n"
        preamble += "\n" + body
    # One old byte walk of sItems remains a byte walk when its declaration
    # becomes the canonical Item pointer needed by the incoming function.
    enemy = enemy.replace("extern u8* sItems;", "struct Item;\nextern struct Item* sItems;")
    enemy = enemy.replace("    EnemyPlayerView view[4];", "    EnemyPlayerView view[4];\n    Player players[4];")
    enemy = enemy.replace("(sItems + e->guard_closest", "((u8*)sItems + e->guard_closest")
    enemy = enemy.replace("extern struct item* PlaceItem(s32 a, s32 b, char* name, s32 c);", "struct Item;\nextern struct Item* PlaceItem(s32 a, s32 b, char* name, void* c);")
    enemy = enemy.replace("item = PlaceItem(", "item = (struct item*)PlaceItem(")
    enemy = enemy.replace("0x800444C0 - 0x80050054", "0x800444C0 - 0x800520CC")
    enemy = enemy.replace("gamemain.c (starts 0x80050054)", "gamemain.c (starts 0x800520CC)")
    enemy = enemy.replace("via gamemain.c init_enemy_vars", "via init_enemy_vars")
    enemy = re.sub(r'/\* game/item.h is deliberately NOT included:.*?\*/',
                  '/* item.h is included at the loading/targeting tail, after the existing\n * byte-walk code. The Item pointer is forward-declared for that earlier use. */',
                  enemy, count=1, flags=re.S)
    enemy = enemy.replace(" * Matching is intentionally not attempted here (high mismatch accepted); this\n * file exists to carry the symbol map and keep the tree green.\n", " * The complete TU must match code and data before being linked from source.\n")
    enemy += preamble
    for name in sorted(ordered, key=lambda name: function_span(game, name)[0], reverse=True):
        start, end = function_span(game, name)
        lines = game.splitlines(keepends=True)
        del lines[start:end]
        game = "".join(lines)
    # Empty pragma brackets left by migrated functions have no consumer.
    game = re.sub(r'#pragma dont_inline on\n\s*#pragma dont_inline off\n', '', game)
    game = re.sub(r'#pragma opt_common_subs off\n#pragma opt_propagation off\n\s*#pragma opt_propagation reset\n#pragma opt_common_subs on\n', '', game)
    old_intro = game.index(' * On Xbox this code was split')
    intro_end = game.index('\n */', old_intro)
    game = game[:old_intro] + ''' * The current GameCube split is [0x800520CC, 0x80055CB8): the
 * GAMEDEFS prefix followed by the main game-flow functions. Enemy helpers
 * before this range belong to enemy.c; the world-loader block after it
 * belongs to gauntworld.c. Retail string-pool bases and exception-table
 * boundaries corroborate those module cuts independently of the Xbox names.
 * This TU remains NonMatching until all code and owned data match.
''' + game[intro_end:]
    game = re.sub(r'^static char sBossGenName\[\].*\n', '', game, flags=re.M)
    # Remaining callers keep declarations, not definitions from the other TU.
    forward = "\n/* Entry points now owned by enemy.c. */\n"
    for name in MOVED:
        signature = bodies[name].split("{", 1)[0].strip()
        if not signature.startswith("static "):
            forward += signature + ";\n"
    insertion = game.index("/* ================================================================== */\n/* Function bodies")
    game = game[:insertion] + forward + game[insertion:]
    return {ENEMY: enemy, GAME: game}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compile-only", action="store_true", help="compile existing generated candidates after scratch edits")
    parser.add_argument("--patch", action="store_true", help="print a source patch, do not apply it")
    parser.add_argument("--show", help="show target/candidate normalized assembly for one incoming function")
    parser.add_argument("--compare-baseline", action="store_true")
    parser.add_argument("--verify-installed", action="store_true")
    args = parser.parse_args()
    edges = read_edges()
    out = REPO / "build/r60_enemy_boundary"
    out.mkdir(parents=True, exist_ok=True)
    source = {unit: (REPO / edges[unit]["src"]).read_text(encoding="utf-8") for unit in (ENEMY, GAME)}
    paths = {unit: out / (unit.rsplit("/", 1)[1] + ".c") for unit in source}
    if args.verify_installed:
        before = {unit: disassemble_words(str(out / (paths[unit].stem + "_baseline.o"))) for unit in source}
        if not all(before.values()):
            raise RuntimeError("saved baseline objects are required")
        combined = dict(before[ENEMY], **before[GAME])
        for unit in source:
            expected = set(before[unit]) | set(MOVED) if unit == ENEMY else set(before[unit]) - set(MOVED)
            row = changed_rows(combined, disassemble_words(str(REPO / edges[unit]["body_o"])), expected)
            print(json.dumps(dict(unit=unit, **row)))
            if row["missing"] or row["unexpected"]:
                raise RuntimeError("function inventory changed")
        return
    if args.show:
        old = out / "gamemain_baseline.o" if args.compare_baseline else REPO / "build/GUNE5D/obj/game/game/gamemain.o"
        print("\n".join(difflib.unified_diff(parse(old)[args.show], parse(out / "enemy.o")[args.show], fromfile="BASELINE" if args.compare_baseline else "TARGET", tofile="MOVED")))
        return
    if args.patch:
        print("*** Begin Patch")
        for unit in source:
            diff = list(difflib.unified_diff(source[unit].splitlines(), paths[unit].read_text(encoding="utf-8").splitlines(), n=3, lineterm=""))
            if diff:
                print("*** Update File: " + str(REPO / edges[unit]["src"]))
                for line in diff[2:]:
                    print("@@" if line.startswith("@@") else line)
        print("*** End Patch")
        return
    if not args.compile_only:
        candidates = migrate(source[ENEMY], source[GAME])
        for unit in source:
            paths[unit].write_text(candidates[unit], encoding="utf-8")
    before = {}
    for unit in source:
        edge = edges[unit]
        baseline, error = compile_with(edge, edge["mw"], edge["cflags"], out / (paths[unit].stem + "_baseline.o"), out)
        if error or baseline.read_bytes() != (REPO / edge["body_o"]).read_bytes():
            raise RuntimeError(error or "whole-object baseline fidelity failed: " + unit)
        before[unit] = disassemble_words(str(baseline))
        print("FIDELITY " + unit + " whole raw object identical", flush=True)
    all_before = dict(before[ENEMY], **before[GAME])
    results = []
    for unit in source:
        edge = edges[unit]
        obj, error = compile_with(dict(edge, src=str(paths[unit])), edge["mw"], edge["cflags"], out / (paths[unit].stem + ".o"), out)
        if error:
            # compile_with deliberately shortens diagnostics; preserve the
            # full compiler error in the scratch log and print it here.
            compiler = REPO / "build/compilers" / edge["mw"] / "mwcceppc.exe"
            command = [str(compiler), *shlex.split(edge["cflags"]), "-c", str(paths[unit]), "-o", str(out / (paths[unit].stem + ".o"))]
            proc = subprocess.run(command, cwd=REPO, capture_output=True, text=True)
            print(proc.stdout + proc.stderr, flush=True)
            raise RuntimeError(error)
        after = disassemble_words(str(obj))
        expected = set(before[unit]) | set(MOVED) if unit == ENEMY else set(before[unit]) - set(MOVED)
        row = dict(unit=unit, **changed_rows(all_before, after, expected), source_sha256=hashlib.sha256(paths[unit].read_bytes()).hexdigest())
        results.append(row)
        print(json.dumps(row), flush=True)
    (out / "results.json").write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
