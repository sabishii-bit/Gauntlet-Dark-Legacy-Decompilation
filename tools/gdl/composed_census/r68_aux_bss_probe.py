"""Full-TU auxscreen small-BSS declaration/ownership experiment, no source writes.

Test source-order, foreign-extern and their coupled change with every function
body/flag/pragma held fixed. Scratch C files are diagnostic variants only.
The target-order variant removes the unreferenced, target-unidentified wiz_mode
definition and retains two foreign globals as extern with extracted ownership.
"""
import argparse
import json
from pathlib import Path
import re
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
from tools.gdl.composed_census import cv_probe as cv
from tools.gdl.composed_census import r68_aux_ownership_audit as audit

TARGET_ORDER = audit.BSS_ORDER
FOREIGN = ("good_wiz_enabled", "good_wiz_exit_timer")
ORIGINAL_ORDER = TARGET_ORDER[:14] + ("wiz_mode", "good_wiz_timer", "all_rune_stones") + TARGET_ORDER[16:] + FOREIGN


def source_forms(source):
    pattern = rb"(?m)^(?:extern )?(?:void\*|s32|f32) (?:" + b"|".join(n.encode() for n in TARGET_ORDER + FOREIGN + ("wiz_mode",)) + rb");\r?$"
    matches = list(re.finditer(pattern, source))
    names = [re.search(rb"(\w+);", m[0])[1].decode() for m in matches]
    if len(names) != len(set(names)) or set(names) not in (set(ORIGINAL_ORDER), set(TARGET_ORDER + FOREIGN)):
        raise ValueError("unexpected or duplicated state declarations")
    if len(re.findall(rb"\bwiz_mode\b", source)) != names.count("wiz_mode"):
        raise ValueError("wiz_mode is not unused")
    start, end = matches[0].start(), matches[-1].end()
    if source[end-1:end] == b"\r":
        end -= 1  # leave the final CRLF attached to the untouched suffix
    block = source[start:end]
    if len(re.findall(rb";", block)) != len(matches):
        raise ValueError("foreign declaration in the selected state block")
    lines = {re.search(rb"(\w+);", m[0])[1].decode(): m[0].rstrip(b"\r").removeprefix(b"extern ") for m in matches}
    lines["wiz_mode"] = b"s32 wiz_mode;"
    newline = b"\r\n" if b"\r\n" in source else b"\n"
    orders = {"original": ORIGINAL_ORDER, "reverse_only": list(reversed(ORIGINAL_ORDER)),
              "foreign_extern_only": [n for n in ORIGINAL_ORDER if n != "wiz_mode"],
              "target_reverse_and_foreign_extern": list(reversed(TARGET_ORDER)) + list(FOREIGN)}
    result = {"baseline": source}
    for label, order in orders.items():
        body = newline.join((b"extern " if label not in ("original", "reverse_only") and n in FOREIGN else b"") + lines[n] for n in order)
        result[label] = source[:start] + body + source[end:]
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    output = args.out.resolve()
    if not output.is_relative_to(ROOT / "build") or not output.name.startswith("r68_aux_"):
        parser.error("output must be build/r68_aux_*.json")
    result = dict(schema_version=1, status="UNRESOLVED", scope=__doc__)
    try:
        edge = cv.read_edges()[audit.UNIT]
        source = (ROOT / edge["src"]).read_bytes()
        raw = (ROOT / edge["body_o"]).read_bytes()
        baseline = audit.object_inventory(ROOT / edge["body_o"])
        folder = Path(tempfile.mkdtemp(prefix="r68_aux_bss_", dir=ROOT / "build"))
        result.update(artifacts=str(folder.relative_to(ROOT)), variants={})
        for label, text in source_forms(source).items():
            trial = dict(edge, _command_trace=[])
            if label != "baseline":
                path = folder / (label + ".c")
                path.write_bytes(text)
                trial["src"] = str(path.relative_to(ROOT))
            obj, error = cv.compile_with(trial, edge["mw"], edge["cflags"], folder / (label + ".o"), folder)
            if error or not obj:
                raise ValueError(error or "missing compiler output")
            if label == "baseline" and obj.read_bytes() != raw:
                raise ValueError("fresh raw-object fidelity failed")
            current = audit.object_inventory(obj)
            bss = {n: s for n, s in current["symbols"].items() if s["section"] == ".sbss"}
            result["variants"][label] = dict(trace=trial["_command_trace"], source_sha256=audit.digest(text),
                object_sha256=audit.digest(obj.read_bytes()), bss=bss,
                sbss=current["sections"][".sbss"],
                function_changes=audit.changed_functions(baseline["functions"], current["functions"]),
                exception_records_unchanged=baseline["exception_records"] == current["exception_records"],
                target_relative_offsets_exact=all(n in bss and bss[n]["offset"] == i * 4 for i, n in enumerate(TARGET_ORDER)),
                changed_initialized_sections=[n for n, s in baseline["sections"].items() if s["type"] != 8 and current["sections"].get(n) != s])
        if (ROOT / edge["src"]).read_bytes() != source or (ROOT / edge["body_o"]).read_bytes() != raw:
            raise ValueError("production source/raw changed during probe")
        result.update(status="PASS", protected_unchanged=True, baseline_fidelity=True)
    except (OSError, ValueError, KeyError) as error:
        result["error"] = str(error)
    output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(result["status"], result.get("error", "bounded experiment completed"), output)
    return 0 if result["status"] == "PASS" else 2


if __name__ == "__main__":
    raise SystemExit(main())
