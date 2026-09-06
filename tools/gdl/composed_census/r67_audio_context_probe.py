#!/usr/bin/env python3
"""Bounded full-TU AudioStreamPlay context experiment; never edits production.

The opaque-record, local-wrapper/pad and shared-pool variants are diagnostics,
NOT recovered original structure or proposals to retain compatibility scaffolds.
PASS means the finite experiment ran with an exact raw Ninja baseline. It is
not whole-TU equality, source unreachability, or a claim that WebFrank is needed.
Generated sources/objects and JSON stay under build/. Run after a fresh ninja.
"""
from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "tools/gdl"))
sys.path.insert(0, str(ROOT / "tools/gdl/composed_census"))
import cv_probe
import fndiff
from cn_analyze import load as load_function
import webfrank as wf
from exception_metadata import compare_exception_records, exception_records

UNIT = "game/audio/audio"
FUNCTION = "AudioStreamPlay"


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def stream_block(source):
    start = source.index("s32 AudioStreamPlay(s32 id, s32 loopMode, s32 vol)\n{")
    end = source.index("\nvoid AudioStreamEndCbLoop(void)\n", start)
    return start, end, source[start:end]


def variants(source):
    start, end, block = stream_block(source)
    pool = block.replace("u8* state = sAudioState;", "u8* state = sAudioState;\n    const char* messages = sAudioTimeoutMsg;")
    pool = pool.replace('"Audio Stream bad file: %s"', "messages + 220")
    pool = pool.replace('"Audio Stream no buffer memory: %s"', "messages + 248")
    pool = pool.replace('"Audio Stream Err: %s"', "messages + 284")
    pool_source = source[:start] + pool + source[end:]

    def typed(text):
        declaration = "extern u8 sAudioState[];"
        if text.count(declaration) != 1:
            raise ValueError("state declaration changed; rederive experiment")
        text = text.replace(declaration, "R67_STATE_DECLARATION")
        text = re.sub(r"\bsAudioState\b", "((u8*)&sAudioState)", text)
        return text.replace("R67_STATE_DECLARATION", "typedef struct R67AudioStateProbe { u8 opaque[24]; } R67AudioStateProbe;\nextern R67AudioStateProbe sAudioState;")

    direct = block.replace("u8* state = sAudioState;", "u8* state;")
    direct = re.sub(r"\bstate\b", "sAudioState", direct).replace("u8* sAudioState;", "u8* state;")
    def typed_local(text):
        result = typed(text)
        a, b, body = stream_block(result)
        body = body.replace("u8* state = ((u8*)&sAudioState);", "R67AudioStateProbe* state = &sAudioState;")
        body = body.replace("state +", "(u8*)state +").replace("vol, state)", "vol, (u8*)state)")
        return result[:a] + body + result[b:]

    prefix = '''const char sAudioTimeoutMsg[] = "Audio Play Timeout";
const char sAudioBankNotLoadedMsg[] = "AUDIO: BANK %s NOT LOADED. SOUND:%s\\n";
const char lbl_80111304[] = "AUDIO: UNABLE TO FIND MODE %s";
const char lbl_80111324[] = "RESETTING AUDIO AND TRYING AGAIN";
const u8 lbl_80111348[] = "audatps2.rom";
const char r67_audio_bank_bad_file[] = "Audio Bank bad file: %s";
const char r67_audio_bank_load_failed_code[] = "aud_load_bank failed: %d";
const char r67_audio_bank_load_failed[] = "aud_load_bank failed";
'''
    pool_owned = source.replace('#include "types.h"\n', '#include "types.h"\n' + prefix)
    boundary = stream_block(pool_owned)[0]
    prior_pragmas_removed = re.sub(r"^\s*#pragma[^\n]*\n", "", pool_owned[:boundary], flags=re.MULTILINE) + pool_owned[boundary:]
    owned_state = source.replace("extern u8 sAudioState[];", "u8 sAudioState[24];")
    def member_view(text):
        result = typed_local(text).replace("u8 opaque[24];", "u8 pad00[12]; void* callback_0C; u8 pad10[1032]; char filename_418[128];")
        a, b, body = stream_block(result)
        body = body.replace("(char*)((u8*)state + 1048)", "state->filename_418")
        body = body.replace("*(void**)((u8*)state + 12)", "state->callback_0C")
        body = body.replace("vol, (u8*)state)", "vol, state)")
        return result[:a] + body + result[b:]
    def local_wrapper(text, pad=256):
        a, b, body = stream_block(text)
        body = body.replace("u8* state = sAudioState;", "struct { u8* base; } state;")
        body = body.replace("    if (sAudioSuspend != 0)", "    state.base = sAudioState;\n    if (sAudioSuspend != 0)")
        body = body.replace("state +", "state.base +").replace("vol, state)", "vol, state.base)")
        body = body.replace("volatile u8 unused[256];", f"volatile u8 unused[{pad}];")
        return text[:a] + body + text[b:]
    return {
        "scratch_mirror": source,
        "direct_state_uses": source[:start] + direct + source[end:],
        "record_address_control": typed(source),
        "shared_pool_control": pool_source,
        "record_and_pool_control": typed(pool_source),
        "typed_local_record_control": typed_local(source),
        "typed_local_record_and_pool_control": typed_local(pool_source),
        "own_state_24_control": owned_state,
        "own_rodata_prefix_control": pool_owned,
        "own_rodata_prefix_no_prior_pragmas_control": prior_pragmas_removed,
        "own_state_and_rodata_control": pool_owned.replace("extern u8 sAudioState[];", "u8 sAudioState[24];"),
        "member_view_control": member_view(source),
        "member_view_and_owned_rodata_control": member_view(pool_owned),
        "local_wrapper_diagnostic_control": local_wrapper(source),
        "local_wrapper_and_owned_rodata_diagnostic_control": local_wrapper(pool_owned),
        "local_wrapper_owned_rodata_pad248_diagnostic_control": local_wrapper(pool_owned, pad=248),
    }


def function_metrics(target, ours):
    target_insns = fndiff.instruction_lines(target)
    ours_insns = fndiff.instruction_lines(ours)
    return {
        "target_instructions": len(target_insns),
        "ours_instructions": len(ours_insns),
        "target_frame": fndiff.frame_size(target),
        "ours_frame": fndiff.frame_size(ours),
        "instruction_text_equal": target_insns == ours_insns,
        "assembly_with_relocations_equal": target == ours,
        "classification": fndiff.classify_function(target, ours),
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=Path("build/GUNE5D/r67_audio_context_probe.json"))
    parser.add_argument("--flags", action="store_true", help="also cross the owned-prefix context with a finite option/variant list")
    args = parser.parse_args(argv)
    out = args.out.resolve()
    build = (ROOT / "build").resolve()
    if not out.is_relative_to(build):
        parser.error("--out must stay beneath this checkout's build/")
    directory = out.parent / (out.stem + "_artifacts")
    directory.mkdir(parents=True, exist_ok=True)
    edge = cv_probe.read_edges()[UNIT]
    source_path = ROOT / edge["src"]
    raw = ROOT / edge["body_o"]
    target_path = ROOT / "build/GUNE5D/obj/game/audio/audio.o"
    fixed = ROOT / "build/GUNE5D/src/game/audio/audio.o"
    protected = [source_path, raw, target_path, fixed, ROOT / "build.ninja", ROOT / "config/GUNE5D/webfrank.json"]
    before = {str(p.relative_to(ROOT)): sha(p) for p in protected}
    baseline_path = directory / "r67_audio_baseline.o"
    baseline, error = cv_probe.compile_with(edge, edge["mw"], edge["cflags"], baseline_path, directory)
    if error or baseline is None or baseline.read_bytes() != raw.read_bytes():
        result = {"schema_version": 1, "status": "UNRESOLVED", "error": error or "raw baseline mismatch", "variants": []}
        out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result))
        return 2
    original = source_path.read_text(encoding="utf-8")
    target_functions = fndiff.parse(target_path)
    raw_functions = fndiff.parse(raw)
    target_loaded = load_function(str(target_path), FUNCTION)
    target_body = target_loaded[3:5]
    target_symbol = target_loaded[2]
    target_full_relocations = wf._function_text_relocations_full(target_loaded[0], target_loaded[1], target_symbol.section_index, target_symbol.value, target_symbol.value + target_symbol.size)
    baseline_bodies = {name: load_function(str(raw), name)[3] for name in raw_functions}
    baseline_sections = fndiff.object_sections(raw, readable=None)[1]
    result = {"schema_version": 1, "status": "PASS", "scope": "Fixed full-TU source; finite diagnostics, not recovered source or source-unreachability proof", "baseline_fidelity": {"raw_sha256": sha(raw), "recompiled_sha256": sha(baseline)}, "compiler": edge["mw"], "cflags": edge["cflags"], "source_pragmas": [line for line in original.splitlines() if re.match(r"\s*#\s*pragma\b", line)], "variants": [], "protected_before": before}
    experiments = [(name, text, edge["mw"], edge["cflags"]) for name, text in variants(original).items()]
    if args.flags:
        for tag, key in (("prefix", "own_rodata_prefix_control"), ("prefix_no_prior_pragmas", "own_rodata_prefix_no_prior_pragmas_control")):
            prefix_source = variants(original)[key]
            for label, compiler, cflags in cv_probe.variants(edge, "opt")[1:]:
                experiments.append((tag + "_" + re.sub(r"\W", "_", label), prefix_source, compiler, cflags))
            experiments.append((tag + "_125n", prefix_source, "GC/1.2.5n", edge["cflags"]))
    for ordinal, (name, text, compiler, cflags) in enumerate(experiments):
        # MWCC has a shorter pathname limit than the Windows filesystem.
        # Keep descriptive names in JSON, not in generated source basenames.
        source = directory / f"r67_audio_v{ordinal:03d}.c"
        obj = source.with_suffix(".o")
        source.write_text(text, encoding="utf-8", newline="\n")
        trial = copy.deepcopy(edge)
        trial["src"] = str(source)
        trial["_command_trace"] = []
        compiled, error = cv_probe.compile_with(trial, compiler, cflags, obj, directory)
        row = {"name": name, "source": str(source.relative_to(ROOT)), "source_sha256": sha(source), "compiler": compiler, "compiler_sha256": sha(ROOT / "build/compilers" / compiler / "mwcceppc.exe"), "cflags": cflags, "commands": trial["_command_trace"]}
        if error or compiled is None:
            row.update(status="FAIL", error=error)
            result["status"] = "FAIL"
        else:
            functions = fndiff.parse(obj)
            sections = fndiff.object_sections(obj, readable=None)[1]
            changed = [f for f in raw_functions if functions.get(f) != raw_functions[f]]
            row.update(status="PASS", object=str(obj.relative_to(ROOT)), object_sha256=sha(obj), metrics=function_metrics(target_functions[FUNCTION], functions[FUNCTION]), changed_functions=changed, section_sizes={s: len(b) for s,b in sections.items()}, nontext_baseline_equal={s: sections.get(s) == b for s,b in baseline_sections.items() if s != ".text"})
            row["exception_metadata"] = compare_exception_records(exception_records(target_path.read_bytes()), exception_records(obj.read_bytes()))
            loaded = load_function(str(obj), FUNCTION)
            body, relocations = loaded[3:5]
            symbol = loaded[2]
            full_relocations = wf._function_text_relocations_full(loaded[0], loaded[1], symbol.section_index, symbol.value, symbol.value + symbol.size)
            row["raw_function_evidence"] = {
                "body_bytes_equal": body == target_body[0],
                "body_sha256": hashlib.sha256(body).hexdigest(),
                "target_body_sha256": hashlib.sha256(target_body[0]).hexdigest(),
                "raw_relocation_tables_equal": full_relocations == target_full_relocations,
                "normalized_assembly_with_relocations_equal": target_functions[FUNCTION] == functions[FUNCTION],
                "relocations_with_addends": sorted(full_relocations.items()),
                "target_relocations_with_addends": sorted(target_full_relocations.items()),
                "compiler_private_aliases": fndiff.compiler_private_aliases(obj),
                "scope": "Exact unlinked bytes and separately reported raw relocation records/normalized assembly. Raw tables may differ by section-base alias and MWCC SDA21 halfword convention; not local data ownership or final-link equality.",
            }
            if name.startswith("local_wrapper"):
                row["raw_body_changed_functions"] = [fn for fn, previous in baseline_bodies.items() if load_function(str(obj), fn)[3] != previous]
            if len(sections.get(".rodata", b"")) >= 220:
                row["retail_rodata_prefix_220_equal"] = sections[".rodata"][:220] == fndiff.dol_read(0x801112C8, 220)
            row["assembly_path"] = str(source.with_suffix(".assembly.json").relative_to(ROOT))
            source.with_suffix(".assembly.json").write_text(json.dumps({"target": target_functions[FUNCTION], "ours": functions[FUNCTION]}, indent=2) + "\n", encoding="utf-8")
            if name == "scratch_mirror" and (changed or sections != baseline_sections or exception_records(obj.read_bytes()) != exception_records(raw.read_bytes())):
                row.update(status="UNRESOLVED", error="scratch pathname changed measured output; do not trust variant comparisons")
                result["status"] = "UNRESOLVED"
        result["variants"].append(row)
        summary = {k: row[k] for k in ("status", "metrics") if k in row}
        summary["changed_function_count"] = len(row.get("changed_functions", []))
        print(name, json.dumps(summary), flush=True)
        if name == "scratch_mirror" and row["status"] != "PASS":
            break
    result["protected_after"] = {str(p.relative_to(ROOT)): sha(p) for p in protected}
    result["protected_unchanged"] = before == result["protected_after"]
    if not result["protected_unchanged"]:
        result["status"] = "FAIL"
    out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(result["status"], out)
    return {"PASS": 0, "FAIL": 1, "UNRESOLVED": 2}[result["status"]]


if __name__ == "__main__":
    raise SystemExit(main())
