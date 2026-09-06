"""Prove sndvoice's active raw compile edge and exact text after rule retirement.

Run after configure.py and a green ninja. PASS requires a fresh complete object
from the actual plain Ninja compiler edge, zero sndvoice WebFrank rules, and
every target function's exact bytes and positional named relocations. Optional
--before-object additionally proves complete equality with an archived prior
linked object, covering all symbol/data/EH bytes without normalization.

Allocated target metadata is retained, not silently equated across ELF formats.
The ordinary full-link checksum remains a separate required integration gate.
This is a manually invoked audit, not a hash lock on modders' source/builds.
GC/1.2.5n is a derived compiler; this does not claim stock compiler matching.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census.r68_aux_ownership_audit import object_inventory
from tools.fix_exception_objects import Elf

UNIT = "game/g3d/sndvoice"
FUNCTION = "sndVoiceUpdateAll"
OUTPUT = f"build/GUNE5D/src/{UNIT}.o"
SOURCE = f"src/{UNIT}.c"
MIX_FIELDS = (
    "vL", "vDeltaL", "vR", "vDeltaR", "vAuxAL", "vDeltaAuxAL",
    "vAuxAR", "vDeltaAuxAR", "vAuxBL", "vDeltaAuxBL", "vAuxBR",
    "vDeltaAuxBR", "vAuxBS", "vDeltaAuxBS", "vS", "vDeltaS",
    "vAuxAS", "vDeltaAuxAS",
)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def retired_edge(edges, config, ninja):
    """Never select a .postprocess/body object merely because it still exists."""
    if not isinstance(config.get("units"), dict):
        raise ValueError("missing WebFrank unit inventory")
    if config["units"].get(UNIT):
        raise ValueError("sndvoice still has active WebFrank rules")
    edge = edges[UNIT]
    expected = dict(src=SOURCE, body_o=OUTPUT, rule="mwcc_sjis", mw="GC/1.2.5n", raw=False)
    for key, value in expected.items():
        if edge.get(key) != value:
            raise ValueError("unexpected active compiler edge: " + key)
    template = " ".join((edge.get("command_template") or "").replace("\\", "/").split())
    if template != ("build/tools/sjiswrap.exe build/compilers/$mw_version/mwcceppc.exe "
                    "$cflags -MMD -c $in -o $basedir"):
        raise ValueError("compiler command is not the supported plain compile template")
    joined = re.sub(r"\$\r?\n\s+", " ", ninja).replace("\\", "/")
    producers = re.findall(r"^build ([^\n]+):\s+(\S+)\s+([^\n]+)", joined, re.M)
    selected = [(outputs, rule, inputs) for outputs, rule, inputs in producers
                if OUTPUT in outputs.split()]
    if len(selected) != 1 or selected[0][1] != "mwcc_sjis" or selected[0][2].split()[0] != SOURCE:
        raise ValueError("plain object does not have exactly one direct compiler producer")
    for outputs, _rule, _inputs in producers:
        if any(path.endswith("/g3d/.postprocess/body/sndvoice.o") for path in outputs.split()):
            raise ValueError("stale generated pre-WebFrank compile edge still exists")
    return edge


def require_same_object(actual, expected, label):
    if not actual or not expected or actual != expected:
        raise ValueError(label + ": complete object differs or is empty")
    return dict(byte_identical=True, size=len(actual), sha256=sha(actual))


def relocation_rows(rows, aliases=None):
    """Normalize only MWCC/dtk's documented SDA21 +2/+0 convention."""
    normalized = []
    seen = set()
    for offset, kind, name, addend in rows:
        if kind == 109 and offset % 4 in (0, 2):
            offset &= ~3
        if offset // 4 in seen:
            raise ValueError("multiple relocations on one instruction")
        seen.add(offset // 4)
        normalized.append((offset, kind, (aliases or {}).get(name, name), addend))
    return sorted(normalized)


def exact_functions(target, actual, aliases=None):
    """Positional tuples include offset, relocation type, symbol AND addend."""
    a, b = target["functions"], actual["functions"]
    if not a or a.keys() != b.keys() or FUNCTION not in a:
        raise ValueError("empty, missing or changed function roster")
    for name in a:
        for key in ("offset", "size", "body"):
            if a[name][key] != b[name][key]:
                raise ValueError(name + ": target " + key + " differs")
        if relocation_rows(a[name]["relocations"]) != relocation_rows(b[name]["relocations"], aliases):
            raise ValueError(name + ": target positional relocations differ")
        if (not a[name]["size"] or a[name]["size"] % 4
                or len(bytes.fromhex(a[name]["body"])) != a[name]["size"]):
            raise ValueError(name + ": empty or truncated function body")
    return dict(function_count=len(a), instruction_count=a[FUNCTION]["size"] // 4,
                named_positional_relocations=sum(len(fn["relocations"]) for fn in a.values()),
                raw_body_sha256=sha(bytes.fromhex(a[FUNCTION]["body"])))


def verify_data_base(actual, target, base_symbol):
    """Bind the one observed compiler section-base alias, not arbitrary pools."""
    if base_symbol != dict(section=".data", offset=0, size=0, kind=0, binding=0):
        raise ValueError("unexpected ...data.0 symbol identity")
    a, b = actual["symbols"]["sndDbTable"], target["symbols"]["sndDbTable"]
    for obj in (a, b):
        if (obj["section"], obj["offset"], obj["size"], obj["binding"]) != (".data", 0, 1932, 1):
            raise ValueError("sndDbTable no longer owns the expected base")
    if a["bytes"] != b["bytes"] or a["bytes"] is None:
        raise ValueError("sndDbTable initialized bytes differ")
    # This base is also used with literal instruction displacements reaching
    # sibling tables. Require the ENTIRE allocated nontext run, not a prefix.
    a_sections = {n: s for n, s in actual["sections"].items() if n != ".text"}
    b_sections = {n: s for n, s in target["sections"].items() if n != ".text"}
    if not a_sections or a_sections != b_sections:
        raise ValueError("allocated target nontext/data/BSS extent differs")
    if actual["exception_records"] != target["exception_records"]:
        raise ValueError("target exception metadata differs")
    return {"...data.0": "sndDbTable"}


def text_coverage(inventory):
    end = 0
    for fn in sorted(inventory["functions"].values(), key=lambda row: row["offset"]):
        if fn["offset"] != end:
            raise ValueError("function roster does not cover the complete text section")
        end += fn["size"]
    section = inventory["sections"][".text"]
    if (end != section["size"] or len(bytes.fromhex(section["bytes"])) != end
            or section["type"] != 1 or section["flags"] != 6 or section["alignment"] != 4):
        raise ValueError("unexpected complete text extent/alignment/flags")
    return end


def data_base(path):
    elf = Elf(str(path))
    matches = [elf.sym(i) for i in range(elf.symcount) if elf.symname(i).decode() == "...data.0"]
    if len(matches) != 1:
        raise ValueError("missing or duplicate ...data.0 symbol")
    symbol = matches[0]
    if not 0 < symbol[5] < len(elf.sh):
        raise ValueError("undefined compiler data base")
    return dict(section=elf.names[symbol[5]], offset=symbol[1], size=symbol[2],
                kind=symbol[3] & 15, binding=symbol[3] >> 4)


def layout_source():
    fields = ",\n".join("    offsetof(AXPBMIX, " + field + ")" for field in MIX_FIELDS)
    return ('#include "dolphin/ax.h"\n#include "game/sndvoice.h"\n'
            '#define offsetof(type, member) ((u32)&(((type*)0)->member))\n'
            'u32 r70_sndvoice_layout[] = {\n'
            '    sizeof(u16), sizeof(AXPBMIX), sizeof(SndVoice),\n'
            '    offsetof(SndVoice, mix), sizeof(((SndVoice*)0)->mix),\n'
            + fields + '\n};\n')


def check_layout(inventory):
    symbol = inventory["symbols"]["r70_sndvoice_layout"]
    expected = [2, 36, 88, 48, 40] + list(range(0, 36, 2))
    if symbol["size"] != len(expected) * 4 or symbol["bytes"] is None:
        raise ValueError("layout compiler datum is missing or truncated")
    actual = list(struct.unpack(">" + "I" * len(expected), bytes.fromhex(symbol["bytes"])))
    section = inventory["sections"][symbol["section"]]
    if section["relocations"] or actual != expected:
        raise ValueError("AXPBMIX/SndVoice sizeof/offsetof layout differs")
    return dict(u16_size=actual[0], axpbmix_size=actual[1], sndvoice_size=actual[2],
                input_mix_offset=actual[3], input_mix_halfwords=actual[4] // 2,
                mixer_output_halfwords=len(MIX_FIELDS),
                axpbmix_offsets=dict(zip(MIX_FIELDS, actual[5:])),
                semantics="18 mixer halfwords; input mix[20] also includes the separate VE pair. "
                          "SDK ABI struct-flattening idiom; not an arbitrary ISO-C portability proof.")


def audit(before_object=None):
    ninja_path, config_path = ROOT / "build.ninja", ROOT / "config/GUNE5D/webfrank.json"
    ninja, config_bytes = ninja_path.read_bytes(), config_path.read_bytes()
    edge = retired_edge(cv.read_edges(), json.loads(config_bytes), ninja.decode())
    raw_path, source_path = ROOT / edge["body_o"], ROOT / edge["src"]
    target_path = ROOT / f"build/GUNE5D/obj/{UNIT}.o"
    compiler_path = ROOT / "build/compilers" / edge["mw"] / "mwcceppc.exe"
    protected_paths = [ninja_path, config_path, raw_path, source_path, target_path, compiler_path,
                       ROOT / "include/dolphin/ax.h", ROOT / "include/game/sndvoice.h",
                       ROOT / "include/types.h"]
    protected = {p: p.read_bytes() for p in protected_paths}
    raw = protected[raw_path]
    folder = Path(tempfile.mkdtemp(prefix="r70_sndvoice_retirement_", dir=ROOT / "build"))
    trial = dict(edge, _command_trace=[])
    control, error = cv.compile_with(trial, edge["mw"], edge["cflags"], folder / "control.o", folder)
    if error or not control:
        raise ValueError((error or "no fresh compiler output") + ": " + json.dumps(trial["_command_trace"]))
    fidelity = require_same_object(control.read_bytes(), raw, "fresh active compiler fidelity")
    if [row["stage"] for row in trial["_command_trace"]] != ["compile"]:
        raise ValueError("unexpected object-transform stage")
    ours, target = object_inventory(raw_path), object_inventory(target_path)
    aliases = verify_data_base(ours, target, data_base(raw_path))
    exact = exact_functions(target, ours, aliases)
    if text_coverage(ours) != text_coverage(target):
        raise ValueError("complete text extents differ")
    layout_path = folder / "r70_sndvoice_layout.c"
    layout_path.write_text(layout_source(), encoding="utf-8", newline="\n")
    layout_trial = dict(edge, src=str(layout_path.relative_to(ROOT)), _command_trace=[])
    layout_obj, error = cv.compile_with(layout_trial, edge["mw"], edge["cflags"], folder / "layout.o", folder)
    if error or not layout_obj:
        raise ValueError((error or "no compiler layout object") + ": " + json.dumps(layout_trial["_command_trace"]))
    result = dict(schema_version=1, status="PASS", unit=UNIT, raw_object=edge["body_o"],
                  compiler=edge["mw"], compiler_kind="derived", flags=edge["cflags"],
                  active_webfrank_rules=0, active_webfrank_edge=False, fidelity=fidelity,
                  exact_functions=exact, layout=check_layout(object_inventory(layout_obj)),
                  relocation_conventions=dict(sda21="MWCC word+2 / dtk word+0; other offsets retained",
                                              proven_same_section_base=aliases),
                  allocated_nontext_target_exact=True, target_exception_metadata_exact=True,
                  complete_text_bytes=text_coverage(ours),
                  source_sha256=sha(protected[source_path]), compiler_sha256=sha(protected[compiler_path]),
                  protected_inputs={str(p.relative_to(ROOT)): sha(data) for p, data in protected.items()},
                  commands=trial["_command_trace"] + layout_trial["_command_trace"],
                  raw_inventory=ours, target_inventory=target,
                  artifact_folder=str(folder.relative_to(ROOT)),
                  full_link_gate="Separate: ninja -j2 must print build/GUNE5D/main.dol: OK")
    if before_object:
        result["prior_linked_object"] = require_same_object(raw, before_object.read_bytes(), "archived prior object")
        result["prior_linked_object"]["path"] = str(before_object)
        result["prior_linked_object"]["provenance"] = "Caller-supplied witness; equality alone does not authenticate its build history."
    stale = ROOT / f"build/GUNE5D/src/game/g3d/.postprocess/body/sndvoice.o"
    result["ignored_stale_body"] = dict(path=str(stale.relative_to(ROOT)), exists=stale.is_file())
    if stale.is_file():
        result["ignored_stale_body"].update(sha256=sha(stale.read_bytes()), differs_from_active=stale.read_bytes() != raw)
    for path, data in protected.items():
        if path.read_bytes() != data:
            raise ValueError("protected input changed during audit: " + str(path))
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--before-object", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    output = args.out.resolve()
    if not output.is_relative_to((ROOT / "build").resolve()) or not output.name.startswith("r70_sndvoice_") or output.suffix != ".json":
        parser.error("--out must name build/**/r70_sndvoice_*.json")
    try:
        result = audit(args.before_object)
    except (OSError, ValueError, KeyError, struct.error) as error:
        result = dict(schema_version=1, status="UNRESOLVED", error=str(error))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: value for key, value in result.items() if key not in (
        "raw_inventory", "target_inventory", "commands", "protected_inputs")}, indent=2))
    print("written", output)
    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
