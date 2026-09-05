"""Read-only census of the exact add/addi/lwz address-forwarding shape.

This is NOT a WebFrank extension and rewrites nothing. The three-instruction
proof compares the load address and all 32 final GPR expressions for arbitrary
incoming registers, modulo 2**32. Window entry/relocation checks are separate.
Only same-offset windows are paired; longer or differently scheduled shapes
are not counted. Both fold directions and already-equal windows are reported.
"""
import argparse
from collections import Counter
import json
import struct
from pathlib import Path

from wf_census import our_path, functions, OBJ
from cn_analyze import wf

ROOT = Path(__file__).resolve().parents[3]
MASK32 = (1 << 32) - 1


def decode(words):
    if len(words) != 3:
        raise ValueError("expected exactly three instructions")
    a, b, c = words
    if a >> 26 != 31 or (a & 0x7ff) != 532:
        raise ValueError("first word is not non-record non-overflow add")
    if b >> 26 != 14 or c >> 26 != 32:
        raise ValueError("expected addi then lwz")
    if not ((b >> 16) & 31) or not ((c >> 16) & 31):
        raise ValueError("RA-zero address mode is outside this pattern")
    for word in words:
        if ((word >> 21) & 31) in (1, 2, 13):
            raise ValueError("SP/TOC/SDA writes are outside this pattern")


def add(left, right):
    result = Counter(dict(left))
    result.update(dict(right))
    return tuple(sorted((key, value & MASK32) for key, value in result.items() if value & MASK32))


def window_state(words):
    decode(words)
    registers = [(("r%d" % r, 1),) for r in range(32)]
    load_address = None
    for word in words:
        op = word >> 26
        d, a, b = (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31
        immediate = wf._sign_extend(word & 0xffff, 16)
        if op == 31:
            registers[d] = add(registers[a], registers[b])
        else:
            address = add(registers[a], (("constant", immediate),))
            if op == 14:
                registers[d] = address
            else:
                load_address = address
                registers[d] = (("loaded_word", 1),)
    return load_address, tuple(registers)


def prove_window(ours, target):
    """No target-byte copying: pure equivalence check, not a patch author."""
    ours_state = window_state(ours)
    target_state = window_state(target)
    if ours_state[0] != target_state[0]:
        raise ValueError("load effective addresses differ")
    if ours_state[1] != target_state[1]:
        raise ValueError("final register values differ")
    if ours == target:
        return "already_equal"
    od, td = wf._sign_extend(ours[2] & 0xffff, 16), wf._sign_extend(target[2] & 0xffff, 16)
    ok, tk = wf._sign_extend(ours[1] & 0xffff, 16), wf._sign_extend(target[1] & 0xffff, 16)
    if ok != tk or ok == 0:
        raise ValueError("not the same nonzero base advance")
    if od == td + ok:
        return "ours_folded"
    if td == od + tk:
        return "target_folded"
    raise ValueError("not a single signed-displacement fold")


def shape(words):
    """Count compiler forms separately from mismatches or rule customers."""
    decode(words)
    first, advance, load = words
    temp = (first >> 21) & 31
    pointer, advance_base = (advance >> 21) & 31, (advance >> 16) & 31
    loaded, load_base = (load >> 21) & 31, (load >> 16) & 31
    delta = wf._sign_extend(advance & 0xffff, 16)
    if not delta or advance_base != temp:
        return None
    if pointer != temp and load_base == temp and loaded == temp:
        return "folded"
    if pointer == temp and load_base == pointer and loaded != pointer:
        return "unfolded"
    return None


def body(data, sections, symbol):
    start = sections[symbol.section_index].offset + symbol.value
    return bytes(data[start:start + symbol.size])


def boundary_check(words, start, relocations, jumptables):
    if any(start <= offset < start + 12 for offset in relocations):
        raise ValueError("relocation inside window")
    successors, _calls = wf._successors(words, {x // 4 for x in relocations}, {x // 4 for x in jumptables})
    first = start // 4
    for origin, destinations in enumerate(successors):
        if not first <= origin < first + 3 and any(first < d < first + 3 for d in destinations):
            raise ValueError("control flow enters window interior")
    if any(start < offset < start + 12 for offset in jumptables):
        raise ValueError("address-taken window interior")


def configured_units(config):
    """Use the current manifest, never stale split objects left on disk."""
    prefix = "build/GUNE5D/obj/"
    result = []
    for row in config["units"]:
        target = row["target_path"].replace("\\", "/")
        if not target.startswith(prefix) or not target.endswith(".o"):
            raise ValueError("unexpected configured target path: " + target)
        unit = target[len(prefix):-2]
        if ".." in unit.split("/") or unit in result:
            raise ValueError("duplicate or invalid configured unit: " + unit)
        result.append(unit)
    return result


def scan():
    counts = Counter()
    hits, errors, stream_shapes = [], [], []
    pinned = {(u, p["function"]) for u, ps in json.loads((ROOT / "config/GUNE5D/webfrank.json").read_text())["units"].items() for p in ps}
    active_units = configured_units(json.loads((ROOT / "objdiff.json").read_text()))
    counts["configured_units"] = len(active_units)
    for unit in active_units:
        path, raw = our_path(unit)
        if not path:
            counts["units_without_compiled_object"] += 1
            continue
        try:
            od, td = Path(path).read_bytes(), (Path(OBJ) / (unit + ".o")).read_bytes()
            os, ts = wf._sections(od), wf._sections(td)
            tm = {s.name: s for s in functions(td, ts)}
            for s in functions(od, os):
                t = tm.get(s.name)
                if t is None:
                    continue
                counts["paired_functions"] += 1
                ours, target = body(od, os, s), body(td, ts, t)
                ow = list(struct.unpack(">%dI" % (len(ours) // 4), ours))
                tw = list(struct.unpack(">%dI" % (len(target) // 4), target))
                parity = len(ow) == len(tw)
                counts["equal_size_pairs" if parity else "unequal_size_pairs"] += 1
                for stream, words in (("ours", ow), ("target", tw)):
                    for i in range(len(words) - 2):
                        try:
                            form = shape(words[i:i+3])
                        except ValueError:
                            continue
                        if form:
                            counts[stream + "_" + form + "_sites"] += 1
                            stream_shapes.append(dict(unit=unit, function=s.name, stream=stream,
                                at=hex(i*4), form=form, words=[hex(w) for w in words[i:i+3]]))
                for i in range(min(len(ow), len(tw)) - 2):
                    try:
                        decode(ow[i:i+3])
                        decode(tw[i:i+3])
                    except ValueError:
                        continue
                    counts["paired_shape_windows"] += 1
                    try:
                        direction = prove_window(ow[i:i+3], tw[i:i+3])
                    except ValueError as refusal:
                        counts["refused: " + str(refusal)] += 1
                        continue
                    counts[direction] += 1
                    if direction == "already_equal":
                        continue
                    orels = wf._function_text_relocations(od, os, s.section_index, s.value, s.value+s.size)
                    trels = wf._function_text_relocations(td, ts, t.section_index, t.value, t.value+t.size)
                    oj = wf._jumptable_targets(od, os, s.section_index, s.value, s.value+s.size)
                    tj = wf._jumptable_targets(td, ts, t.section_index, t.value, t.value+t.size)
                    refusal = None
                    try:
                        boundary_check(ow, i*4, orels, oj)
                        boundary_check(tw, i*4, trels, tj)
                    except ValueError as error:
                        refusal = str(error)
                    differing = [j for j in range(len(ow)) if ow[j] != tw[j]] if parity else None
                    hits.append(dict(unit=unit, function=s.name, counts=[len(tw), len(ow)],
                        at=hex(i*4), direction=direction, ours=[hex(w) for w in ow[i:i+3]],
                        target=[hex(w) for w in tw[i:i+3]], raw=raw, pinned=(unit, s.name) in pinned,
                        window_proof="same effective address and every final GPR", boundary_refusal=refusal,
                        raw_differing_words=len(differing) if differing is not None else None,
                        remaining_words_outside_window=sum(not i <= j < i+3 for j in differing) if differing is not None else None))
        except (ValueError, OSError, SystemExit) as error:
            errors.append(dict(unit=unit, error=str(error)))
    return dict(counts=dict(counts), hits=hits, errors=errors, stream_shapes=stream_shapes,
        scope="same-offset add/addi/lwz windows only; not all address-formation differences",
        entire_body_payoff=[r["function"] for r in hits if r["remaining_words_outside_window"] == 0 and not r["boundary_refusal"]])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", type=Path, default=ROOT / "build/r62_address_fold_census.json")
    args = ap.parse_args()
    result = scan()
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in result.items() if k != "stream_shapes"}, indent=2))
    print("Full per-stream site list: " + str(args.out))


if __name__ == "__main__":
    main()
