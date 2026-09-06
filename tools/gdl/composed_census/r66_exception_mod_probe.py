"""Bounded NMWException mod-literal probe; production files are read-only.

Copy src/Runtime.PPCEABI.H/NMWException.cpp to a build/ scratch directory,
then edit ONLY exception::what's literal from "exception" to "MODIFIED!".
Run with --modified-source pointing to that full-TU copy. The probe validates
that exact edit, compiles through the actual Ninja edge, and applies fix_nmw
only to fresh copies in a unique build/ directory. No normal test requires the
current fixup to overwrite the edit: an eventual preservation/refusal is also
reported. PASS means measurement completed, not that mod behavior is safe.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path[:0] = [str(ROOT), str(ROOT / "tools/gdl/composed_census")]
from tools import fix_exception_objects as fix
import cv_probe as cv

UNIT = "Runtime.PPCEABI.H/NMWException"
FUNCTION = b"what__Q23std9exceptionCFv"
ORIGINAL = 'const char* exception::what() const { return "exception"; }'
MODIFIED = 'const char* exception::what() const { return "MODIFIED!"; }'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def validate_source(original, modified):
    if original.count(ORIGINAL) != 1:
        raise ValueError("expected exactly one original exception::what literal")
    if modified != original.replace(ORIGINAL, MODIFIED):
        raise ValueError("modified full TU differs beyond the one authorized literal edit")


def inspect_object(path):
    data = Path(path).read_bytes()
    if data[:6] != b"\x7fELF\x01\x02":
        raise ValueError("expected ELF32 big-endian compiler object")
    elf = fix.Elf(str(path))
    symbols = [i for i in range(elf.symcount) if elf.symname(i) == FUNCTION]
    if len(symbols) != 1:
        raise ValueError("expected exactly one exception::what function symbol")
    fn = elf.sym(symbols[0])
    if fn[5] != elf.sec[".text"] or fn[2] == 0:
        raise ValueError("what function does not have a nonempty .text body")
    ro = elf.sec[".rodata"]
    ro_header = elf.sh[ro]
    rodata = bytes(elf.data[ro_header[4]:ro_header[4] + ro_header[5]])
    text_header = elf.sh[fn[5]]
    body = bytes(elf.data[text_header[4] + fn[1]:text_header[4] + fn[1] + fn[2]])
    references = []
    for section in (".rela.text", ".rela.sdata", ".rela.data"):
        _, relocs = elf.relas(section)
        for offset, info, addend in relocs:
            symbol = elf.sym(info >> 8)
            if symbol[5] != ro:
                continue
            position = symbol[1] + addend
            if not 0 <= position < len(rodata):
                raise ValueError("relocation's resolved .rodata offset is out of bounds")
            tail = rodata[position:]
            if b"\0" not in tail:
                raise ValueError("unterminated referenced string")
            references.append({"section": section, "offset": offset,
                "type": info & 255, "symbol": elf.symname(info >> 8).decode(),
                "symbol_value": symbol[1], "addend": addend,
                "resolved_rodata_offset": position, "string": tail.split(b"\0", 1)[0].decode("ascii"),
                "inside_what": section == ".rela.text" and fn[1] <= offset < fn[1] + fn[2]})
    what_references = [row for row in references if row["inside_what"]]
    if not what_references:
        raise ValueError("no relocated string reference found inside exception::what")
    return {"object_sha256": sha(data), "object_size": len(data),
            "rodata_size": len(rodata), "rodata_hex": rodata.hex(),
            "what_offset": fn[1], "what_size": fn[2], "what_words": [body[i:i+4].hex() for i in range(0, len(body), 4)],
            "what_body_sha256": sha(body), "what_strings": sorted({r["string"] for r in what_references}),
            "rodata_relocations": references}


def classify_effect(before, after):
    if before["what_strings"] != ["MODIFIED!"]:
        raise ValueError("compiled modification did not reach exception::what's referenced datum")
    if after["what_strings"] == ["MODIFIED!"]:
        return "PRESERVED"
    if after["what_strings"] == ["exception"]:
        return "OVERWRITTEN_WITH_RETAIL_LITERAL"
    return "OTHER_DATUM_CHANGE"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--modified-source", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=ROOT / "build/r66_exception_mod_probe.json")
    args = parser.parse_args(argv)
    args.out = args.out.resolve()
    if not args.out.is_relative_to((ROOT / "build").resolve()) or args.out == args.modified_source.resolve():
        print("UNRESOLVED: output must be a distinct artifact under this checkout's build/")
        return 2
    result = {"schema_version": 1, "status": "UNRESOLVED", "function": FUNCTION.decode(),
              "scope": "same-length literal modification in a full-TU scratch copy; fix_nmw on raw object copies only"}
    try:
        source = ROOT / "src/Runtime.PPCEABI.H/NMWException.cpp"
        modified = args.modified_source.resolve()
        if not modified.is_relative_to((ROOT / "build").resolve()) or modified == source:
            raise ValueError("modified source must be an isolated copy under this checkout's build/")
        validate_source(source.read_text(), modified.read_text())
        edge = cv.read_edges()[UNIT]
        production = ROOT / edge["body_o"]
        protected = [source, modified, production, ROOT / "tools/fix_exception_objects.py", ROOT / "build.ninja"]
        before = {str(path.relative_to(ROOT)): sha(path.read_bytes()) for path in protected}
        result.update(input_hashes=before, modified_source_sha256=sha(modified.read_bytes()), edge=edge)
        folder = Path(tempfile.mkdtemp(prefix="r66_exception_", dir=ROOT / "build"))
        result["artifacts_directory"] = str(folder)
        compiled = {}
        result["compile_commands"] = {}
        for label, input_source in (("baseline", source), ("baseline_repeat", source), ("modified", modified)):
            trace = []
            new_edge = dict(edge, src=str(input_source), _command_trace=trace)
            output, error = cv.compile_with(new_edge, edge["mw"], edge["cflags"], folder / (label + ".o"), folder)
            result["compile_commands"][label] = trace
            if error or output is None:
                raise ValueError(f"{label} compile refused: {error}")
            compiled[label] = output
        result["raw_baseline_reproduced"] = compiled["baseline"].read_bytes() == compiled["baseline_repeat"].read_bytes()
        if not result["raw_baseline_reproduced"]:
            raise ValueError("repeated untouched raw full-TU baseline differs")
        result["observations"] = {}
        for label in ("baseline", "modified"):
            raw = compiled[label]
            fixed = folder / (label + "_fixed.o")
            shutil.copyfile(raw, fixed)
            prior = inspect_object(raw)
            row = {"raw": prior, "fixed_path": str(fixed)}
            result["observations"][label] = row
            try:
                row["fix_nmw_changed"] = fix.fix_nmw(str(fixed))
                row["fixed"] = inspect_object(fixed)
            except (ValueError, AssertionError, KeyError, IndexError, RuntimeError, SystemExit) as error:
                row["fixup_refusal"] = str(error)
        baseline = result["observations"]["baseline"]
        if "fixed" not in baseline:
            raise ValueError("untouched baseline fixup refused; no calibrated mod comparison")
        # Mirror only the separate alignment step in fix.main IN MEMORY.
        # The production object is already mutated; do not call it raw.
        normalized = fix.Elf(baseline["fixed_path"])
        normalized.sh[normalized.sec[".rodata"]][8] = 4
        normalized.write_headers()
        result["fixed_baseline_plus_alignment_equals_production"] = bytes(normalized.data) == production.read_bytes()
        result["baseline_fidelity_boundary"] = "fix_nmw output plus fix.main's independent .rodata alignment=4 step versus the current in-place-mutated Ninja object"
        if not result["fixed_baseline_plus_alignment_equals_production"]:
            raise ValueError("scratch baseline does not reproduce current production fixup; rebuild before drawing conclusions")
        changed = result["observations"]["modified"]
        result["mod_effect"] = "REFUSED" if "fixup_refusal" in changed else classify_effect(changed["raw"], changed["fixed"])
        result["production_inputs_unchanged"] = before == {str(path.relative_to(ROOT)): sha(path.read_bytes()) for path in protected}
        if not result["production_inputs_unchanged"]:
            raise ValueError("production input changed during scratch experiment")
        result["status"] = "PASS"
    except (OSError, ValueError, KeyError, RuntimeError, SystemExit) as error:
        result["error"] = str(error)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(result["status"], result.get("mod_effect", result.get("error")), "wrote", args.out)
    for label, row in result.get("observations", {}).items():
        for phase in ("raw", "fixed"):
            if phase in row:
                print(label, phase, row[phase]["object_sha256"], "what strings", row[phase]["what_strings"])
    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
