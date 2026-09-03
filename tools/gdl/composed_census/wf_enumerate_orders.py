"""WF lane: enumerate EVERY legal order for a permutation window and select
by proof, not by whichever bijection a search finds first.

claim.law.WF_enumerate-every-shape-consistent-order-and-select-by-proof
.20260901.v1 is the reason this exists.  On game/world/camera::debug_camera_pos
exactly TWO shape-consistent orders exist for the window [0x90,0xa8).  BOTH
pass check_permutation_dependences in its strictest form and BOTH pass
copy_register_fields; only one passes verify_consistent_recolor.  A greedy
identity-first search returns the unprovable one half the time, and the
resulting build failure reads like a property of the function rather than of
the search.

Any window holding two atoms of the same form is exposed to this, and windows
of same-form atoms are exactly what schedule residuals produce.

usage:
    python tools/gdl/composed_census/wf_enumerate_orders.py <unit> <fn> <lo:hi>
e.g.
    python tools/gdl/composed_census/wf_enumerate_orders.py \\
        game/world/camera debug_camera_pos 0x90:0xa8

Reads the RAW compiler body (build/GUNE5D/src/<dir>/.postprocess/body/<tu>.o)
where present so an already-pinned function still derives from its true
residual, and the dtk-extracted target from build/GUNE5D/obj/.  Requires a
completed ninja.
"""
import itertools
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
import webfrank as wf  # noqa: E402
from fndiff import unit_key  # noqa: E402


def load(path, function):
    with open(path, "rb") as handle:
        data = bytearray(handle.read())
    sections = wf._sections(data)
    symbol = wf._find_symbol(data, sections, function)
    text = sections[symbol.section_index]
    start = text.offset + symbol.value
    return (bytes(data[start:start + symbol.size]),
            wf._function_text_relocations(
                data, sections, symbol.section_index,
                symbol.value, symbol.value + symbol.size),
            wf._jumptable_targets(
                data, sections, symbol.section_index,
                symbol.value, symbol.value + symbol.size))


def our_object(unit):
    parts = unit_key(unit).split("/")   # run-43 item 8: accept `.c` too
    body = os.path.join(ROOT, "build", "GUNE5D", "src", *parts[:-1],
                        ".postprocess", "body", parts[-1] + ".o")
    if os.path.exists(body):
        return body
    return os.path.join(ROOT, "build", "GUNE5D", "src", *parts) + ".o"


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    unit, function, window = sys.argv[1], sys.argv[2], sys.argv[3]
    lo, hi = (int(value, 0) for value in window.split(":"))

    ours, our_relocations, our_jumptables = load(our_object(unit), function)
    target, _tr, _tj = load(
        os.path.join(ROOT, "build", "GUNE5D", "obj", *unit.split("/")) + ".o",
        function)
    if len(ours) != len(target):
        raise SystemExit(
            f"size mismatch: ours {len(ours)} target {len(target)}")

    count = (hi - lo) // 4
    region = ours[lo:hi]
    print(f"{unit}::{function} window +0x{lo:x}..+0x{hi:x} ({count} atoms)")
    print(f"region before_sha256 = {wf._sha256(region)}")

    calls = {offset: name
             for offset, (kind, name) in our_relocations.items() if kind == 10}
    proven = []
    for candidate in itertools.permutations(range(count)):
        permuted = b"".join(region[j * 4:j * 4 + 4] for j in candidate)
        image = ours[:lo] + permuted + ours[hi:]
        shaped = True
        for index in range(count):
            word = wf._u32(image, lo + 4 * index)
            wanted = wf._u32(target, lo + 4 * index)
            try:
                mask = wf.register_slot_mask(word)
            except ValueError:
                shaped = False
                break
            if (word ^ wanted) & ~mask:
                shaped = False
                break
        if not shaped:
            continue

        order = list(candidate)
        try:
            wf.check_permutation_dependences(region, order, None)
            dependences = "PASS"
        except ValueError as error:
            dependences = f"REFUSED ({error})"
        try:
            recolored, fields = wf.copy_register_fields(image, target)
            recolor = f"PASS ({fields} fields)"
        except ValueError as error:
            recolored, recolor = None, f"REFUSED ({error})"
        bisimulation = "not reached"
        if recolored is not None:
            try:
                wf.verify_consistent_recolor(
                    image, recolored,
                    jumptable_targets=our_jumptables,
                    relocated_offsets=set(our_relocations),
                    call_targets=calls)
                bisimulation = "PASS"
            except ValueError as error:
                bisimulation = f"REFUSED ({error})"

        ok = (dependences == "PASS" and recolor.startswith("PASS")
              and bisimulation == "PASS")
        if ok:
            proven.append(order)
        print(f"\norder {order}  after_sha256 {wf._sha256(permuted)}"
              f"{'   *** FULLY PROVEN ***' if ok else ''}")
        print(f"  dependences:  {dependences}")
        print(f"  recolor:      {recolor}")
        print(f"  bisimulation: {bisimulation}")

    print(f"\nfully proven orders: {len(proven)} {proven}")
    if len(proven) > 1:
        print("  NOTE: more than one order proves out; they produce different "
              "bytes, so pick the one whose after_sha256 matches the target "
              "region and record why.")


if __name__ == "__main__":
    main()
