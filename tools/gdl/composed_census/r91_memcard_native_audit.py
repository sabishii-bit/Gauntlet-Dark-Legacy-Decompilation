"""Manual hash-bound certificate for R91 memCardErrorPrompt native retirement.

No compiler invocation or object mutation. The archived complete-Ninja-edge
controls, source delta, native positional bindings, all raw siblings and the
entire prior processed ELF are independently checked. This is not a TU flip
or a general compiler/source-reachability proof. Run after a successful Ninja.
"""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r67_runtime_visibility_probe import inventory
from tools.fix_exception_objects import Elf

UNIT = "game/sys/memcard"
FN = "memCardErrorPrompt"
MANIFEST = "build/r91_memcard_evidence_manifest.json"
MANIFEST_SHA256 = "2dcf634d9c4b1b2e9f547e206e489e3a19a087bad33f56bca6d716b84d2a9ca8"
BASE = "build/r91_memcard_archive/"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify_archive(root):
    raw = (root / MANIFEST).read_bytes()
    require(hashlib.sha256(raw).hexdigest() == MANIFEST_SHA256, "manifest identity")
    rows = json.loads(raw)["files"]
    require(len(rows) == 184, "archive file census")
    require(len({r['path'] for r in rows}) == len(rows), "duplicate archive path")
    for row in rows:
        path = (root / row["path"]).resolve()
        require(path.is_relative_to(root.resolve()), "archive path escapes root")
        data = path.read_bytes()
        require(len(data) == row["size"] and hashlib.sha256(data).hexdigest() == row["sha256"],
                "archive hash: " + row["path"])


def source_delta(before, after):
    start = before.index("s32 memCardErrorPrompt(const char* msg)\n{")
    end = before.index("\n/*", start)
    body = before[start:end]
    require(body.count("        register s32 zero = 0;") == 1, "source zero anchor")
    require(body.count("        lbl_80344A24 = zero;") == 1, "source store anchor")
    body = body.replace("        register s32 zero = 0;",
                        "        s32* serial = &lbl_80344A24;\n        register s32 zero = 0;")
    body = body.replace("        lbl_80344A24 = zero;", "        *serial = zero;")
    require(after == before[:start] + body + before[end:], "source delta exceeds scalar alias")


def rule_delta(before, after):
    expected = copy.deepcopy(before)
    pins = expected["units"][UNIT]
    require(sum(p["function"] == FN for p in pins) == 1, "expected exactly one old pin")
    expected["units"][UNIT] = [p for p in pins if p["function"] != FN]
    require(after == expected, "archive rule delta exceeds sole native retirement")


def native_function(candidate, target):
    require(candidate["size"] == target["size"] == 100, "native instruction count")
    require(len(bytes.fromhex(candidate["body"])) == len(bytes.fromhex(target["body"])) == 100,
            "native encoded body length")
    require(candidate["binding"] == target["binding"], "native symbol strength")
    require(candidate["body"] == target["body"], "native body")
    # The only tolerated convention difference is MWCC's halfword offset for
    # EMB_SDA21 versus DTK's instruction offset. Do not normalize other types.
    ours = candidate["relocations"]
    theirs = target["relocations"]
    require(len(ours) == len(theirs) == 6, "native relocation census")
    normalized = []
    for offset, kind, name, addend in ours:
        if kind == 109:
            require(offset % 4 == 2, "SDA21 source halfword convention")
            offset -= 2
        normalized.append((offset, kind, name, addend))
    require(normalized == [tuple(row) for row in theirs], "native positional datum binding")


def nontext(path):
    elf = Elf(str(path))
    result = {}
    for i, section in enumerate(elf.sh):
        if not section[2] & 2 or elf.names[i] == ".text":
            continue
        relocs = []
        for ri, rh in enumerate(elf.sh):
            if rh[1] == 4 and rh[7] == i:
                _, entries = elf.relas(elf.names[ri])
                relocs.extend((off, info & 255, elf.symname(info >> 8).decode(), add)
                              for off, info, add in entries)
        result[elf.names[i]] = (section[1], section[2], section[5], section[8],
            None if section[1] == 8 else bytes(elf.data[section[4]:section[4] + section[5]]), relocs)
    return result


def audit(root, live=True):
    verify_archive(root)
    path = lambda name: root / BASE / name
    before_source = path("r91_memcard_source.c").read_text()
    after_source = path("r91_memcard_installed_source.c").read_text()
    source_delta(before_source, after_source)
    before_rules = json.loads(path("r91_memcard_rules.json").read_text())
    after_rules = json.loads(path("r91_memcard_installed_rules.json").read_text())
    rule_delta(before_rules, after_rules)
    before_path = path("r91_memcard_raw.o")
    candidate_path = path("r91_memcard_installed_raw.o")
    target_path = path("r91_memcard_target.o")
    before, _ = inventory(before_path)
    candidate, _ = inventory(candidate_path)
    target, _ = inventory(target_path)
    native_function(candidate[FN], target[FN])
    old_body = bytes.fromhex(before[FN]["body"])
    target_body = bytes.fromhex(target[FN]["body"])
    require(len(old_body) == 100 and sum(old_body[i:i + 4] != target_body[i:i + 4]
                                        for i in range(0, 100, 4)) == 2,
            "baseline raw residual")
    require(len(before) == len(candidate) == 30, "whole function inventory")
    require({k: v for k, v in before.items() if k != FN} ==
            {k: v for k, v in candidate.items() if k != FN}, "raw sibling drift")
    require(nontext(before_path) == nontext(candidate_path), "allocated data/BSS/EH drift")
    require(path("r91_memcard_processed.o").read_bytes() ==
            path("r91_memcard_installed_processed.o").read_bytes(), "whole processed ELF drift")
    proof = json.loads(path("r91_memcard_native_proof.json").read_text())
    require(proof["remaining_rules"] == 4 and len(proof["rule_results"]) == 4, "remaining pin replay")
    require(path("native_fidelity/memcard.o").read_bytes() == candidate_path.read_bytes(),
            "fresh candidate actual-edge fidelity")
    require(path("prompt3/real_scalar_alias/r91_memcard.o").read_bytes() == candidate_path.read_bytes(),
            "private versus installed raw fidelity")
    for batch in ("init1", "init2", "init3", "init4", "prompt1", "prompt2", "prompt3"):
        manifest = json.loads(path(f"r91_memcard_{batch}_manifest.json").read_text())
        require(manifest["edge"] == proof["actual_edge"], "archived command edge drift")
        require(path(f"{batch}/baseline/r91_memcard.o").read_bytes() == before_path.read_bytes(),
                "complete baseline ELF fidelity")
    foreign = []
    if live:
        require((ROOT / "src/game/sys/memcard.c").read_text() == after_source, "live source drift")
        current_rules = json.loads((ROOT / "config/GUNE5D/webfrank.json").read_text())
        require(current_rules["units"][UNIT] == after_rules["units"][UNIT], "live owned rules drift")
        require({k: v for k, v in current_rules.items() if k != "units"} ==
                {k: v for k, v in after_rules.items() if k != "units"}, "live top-level config drift")
        foreign = sorted(k for k in set(current_rules["units"]) | set(after_rules["units"])
                         if k != UNIT and current_rules["units"].get(k) != after_rules["units"].get(k))
        edge = cv.read_edges()[UNIT]
        require(edge == proof["actual_edge"], "live compiler edge drift")
        require((ROOT / "build/compilers" / edge["mw"] / "mwcceppc.exe").read_bytes() ==
                path("r91_memcard_compiler.exe").read_bytes(), "compiler binary drift")
        require((ROOT / edge["body_o"]).read_bytes() == candidate_path.read_bytes(), "live raw ELF drift")
        require((ROOT / f"build/GUNE5D/src/{UNIT}.o").read_bytes() ==
                path("r91_memcard_processed.o").read_bytes(), "live processed ELF drift")
        require((ROOT / f"build/GUNE5D/obj/{UNIT}.o").read_bytes() == target_path.read_bytes(), "live target drift")
    return dict(status="PASS_NATIVE_RULE_RETIRED", function=FN, instructions="25/25",
                raw_words="2 -> 0", positional_bindings=6, raw_siblings_preserved=29,
                whole_prior_processed_elf_equal=True, remaining_unit_rules=4,
                foreign_config_deltas_not_certified=foreign,
                limitations="NonMatching TU fallback unchanged; not a TU flip. Other units' config deltas are outside this certificate.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, default=ROOT)
    parser.add_argument("--archive-only", action="store_true")
    args = parser.parse_args()
    try:
        print(json.dumps(audit(args.archive, not args.archive_only), indent=2))
    except (ValueError, KeyError, OSError, AssertionError) as error:
        print("FAIL:", error)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
