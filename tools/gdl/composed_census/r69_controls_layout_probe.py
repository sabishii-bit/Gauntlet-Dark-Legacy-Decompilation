"""Faithful full-TU controls linkage/order experiments; never writes production source.

Variants change declarations only. A fresh actual-Ninja source-path control
must reproduce the complete raw object. The report distinguishes exact named
allocation from body/relocation preservation and keeps negative outcomes.
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
from tools.gdl.composed_census.r68_aux_ownership_audit import object_inventory, changed_functions, digest
from tools.gdl import fndiff

UNIT = "game/game/controls"
GLOBAL_NAMES = {"ctrls_initialized", "lbl_80344618", "lbl_8034461C", "lbl_80344620"}


def block(source, section):
    begin = b"/* --- " + section.encode() + b" --- */"
    start = source.index(begin) + len(begin)
    end = source.index(b"/* ---", start)
    return start, end, source[start:end]


def small_forms(source):
    start, end, original = block(source, ".sbss")
    lines = [line for line in original.splitlines() if b";" in line]
    names = [re.search(rb"\b(\w+)(?:\[2\])?;", line)[1].decode() for line in lines]
    if len(names) != len(set(names)) or len(names) not in (17, 18):
        raise ValueError("unexpected small-state declaration roster")
    if len(re.findall(rb"\blbl_80344624\b", source)) != names.count("lbl_80344624"):
        raise ValueError("nominal terminal pad is referenced")
    actual = [(n, line) for n, line in zip(names, lines) if n != "lbl_80344624"]
    if len(actual) != 17 or not GLOBAL_NAMES <= set(names):
        raise ValueError("unexpected true small-state declaration roster")
    natural = sorted(actual, key=lambda x: 0x803445F0 if x[0] == "ctrls_initialized" else int(x[0][4:], 16))
    # Recreate both storage classes even when invoked after the repair. The
    # negative controls must not silently become copies of the retained form.
    natural = [(n, line.removeprefix(b"static ")) for n, line in natural]
    newline = b"\r\n" if b"\r\n" in source else b"\n"
    forms = {"baseline": source}
    for label, ordered, export in (("original_mixed", natural + [("lbl_80344624", b"u32 lbl_80344624;")], False),
            ("remove_unused_pad", natural, False),
            ("global_forward", natural, True), ("reverse_only", list(reversed(natural)), False),
            ("global_reverse", list(reversed(natural)), True)):
        body = newline*2 + newline.join((b"static " if not export and n not in GLOBAL_NAMES else b"") + line
                                       for n, line in ordered) + newline*2
        forms[label] = source[:start] + body + source[end:]
    return forms


def bss_export(source, selected=None):
    start, end, original = block(source, ".bss")
    lines = original.splitlines(keepends=True)
    for i, line in enumerate(lines):
        if line.startswith(b"static ") and (selected is None or selected.encode() in line):
            lines[i] = line.removeprefix(b"static ")
    return source[:start] + b"".join(lines) + source[end:]


def layout(obj, section, table):
    base = {".bss": 0x802407B8, ".sbss": 0x803445D8}[section]
    rows = []
    for name, s in obj["symbols"].items():
        if s["section"] != section:
            continue
        t = table.get(name)
        rows.append(dict(name=name, offset=s["offset"], size=s["size"], binding=s["binding"],
                         target=t, target_offset=None if t is None else t[1]-base,
                         exact_offset=t is not None and t[0] == section and t[1]-base == s["offset"]))
    return dict(section=obj["sections"][section], rows=rows,
                all_offsets_exact=bool(rows) and all(r["exact_offset"] for r in rows))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args(argv)
    out = args.out.resolve()
    if not out.is_relative_to(ROOT / "build") or not out.name.startswith("r69_controls_"):
        parser.error("output must name build/r69_controls_*.json")
    result = dict(status="UNRESOLVED")
    try:
        edge = cv.read_edges()[UNIT]
        source, raw = (ROOT / edge["src"]).read_bytes(), (ROOT / edge["body_o"]).read_bytes()
        before = object_inventory(ROOT / edge["body_o"])
        table = fndiff.symbol_table()
        forms = small_forms(source)
        forms["global_reverse_and_bss_all_export"] = bss_export(forms["global_reverse"])
        forms["global_reverse_and_ctl_export"] = bss_export(forms["global_reverse"], "PlayerControl")
        folder = Path(tempfile.mkdtemp(prefix="r69_controls_layout_", dir=ROOT / "build"))
        result.update(artifacts=str(folder.relative_to(ROOT)), variants={}, baseline=before)
        for label, text in forms.items():
            trial = dict(edge, _command_trace=[])
            if label != "baseline":
                path = folder / (label + ".c")
                path.write_bytes(text)
                trial["src"] = str(path.relative_to(ROOT))
            obj, error = cv.compile_with(trial, edge["mw"], edge["cflags"], folder / (label + ".o"), folder)
            if error or not obj:
                raise ValueError(error or "compiler produced no object")
            if label == "baseline" and obj.read_bytes() != raw:
                raise ValueError("actual raw Ninja compiler fidelity failed")
            current = object_inventory(obj)
            result["variants"][label] = dict(trace=trial["_command_trace"],
                source_sha256=digest(text), raw_sha256=digest(obj.read_bytes()),
                function_changes=changed_functions(before["functions"], current["functions"]),
                initialized_section_changes=[n for n, s in before["sections"].items()
                                             if s["type"] != 8 and s != current["sections"].get(n)],
                eh_unchanged=before["exception_records"] == current["exception_records"],
                bss=layout(current, ".bss", table), sbss=layout(current, ".sbss", table),
                inventory=current)
        if (ROOT / edge["src"]).read_bytes() != source or (ROOT / edge["body_o"]).read_bytes() != raw:
            raise ValueError("production source/raw changed during experiment")
        result.update(status="MEASURED", fidelity=True)
    except (OSError, ValueError, KeyError) as error:
        result.update(status="UNRESOLVED", error=str(error))
    out.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print(result["status"], result.get("error", "bounded declaration matrix completed"), out)
    return 0 if result["status"] == "MEASURED" else 2


if __name__ == "__main__":
    raise SystemExit(main())
