"""Image-wide screen: which paired functions close under the VALUE-EQUALITY
recolor proof mode that verify_consistent_recolor refuses?

The mode landed 2026-09-01 18:36Z; the last census of this population (CH,
15:37Z) predates it and screened with the strictly weaker strict-renaming
proof.  This re-screens every candidate with the stronger proof.

Candidate = equal-size paired function, not already byte-identical, every
differing word register-field-only (so copy_register_fields alone reaches the
target).  For each, iterate the declared substitutions/compare_exchanges the
refusals name until the proof closes or stops making progress.
"""
import os
import re
import sys
import glob
import json

PINS = {}
for _unit, _rules in json.load(
        open("config/GUNE5D/webfrank.json"))["units"].items():
    for _rule in _rules:
        PINS.setdefault(_unit.replace("\\", "/"), set()).add(_rule["function"])

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import webfrank as wf  # noqa: E402

SRC = "build/GUNE5D/src"
OBJ = "build/GUNE5D/obj"
MAX_DECLS = 40


def load(path):
    data = bytearray(open(path, "rb").read())
    return data, wf._sections(data)


def functions(data, sections):
    out = {}
    for sym in wf._symbols(data, sections):
        if sym.size and 0 <= sym.section_index < len(sections):
            sec = sections[sym.section_index]
            if sec.name == ".text":
                out[sym.name] = sym
    return out


def raw_object(unit_obj):
    """Prefer the pre-webfrank body so pinned functions are not flattered."""
    head, tail = os.path.split(unit_obj)
    body = os.path.join(head, ".postprocess", "body", tail)
    return body if os.path.exists(body) else unit_obj


def screen(ours_data, ours_sections, ours_sym, tgt_data, tgt_sections, tgt_sym):
    ot = ours_sections[ours_sym.section_index]
    tt = tgt_sections[tgt_sym.section_index]
    ours_fn = bytes(ours_data[ot.offset + ours_sym.value:][:ours_sym.size])
    tgt_fn = bytes(tgt_data[tt.offset + tgt_sym.value:][:tgt_sym.size])
    if ours_fn == tgt_fn:
        return None
    # every differing word must be register-field-only
    n = 0
    for off in range(0, len(ours_fn), 4):
        c, t = wf._u32(ours_fn, off), wf._u32(tgt_fn, off)
        if c == t:
            continue
        n += 1
        if (c ^ t) & ~wf.register_slot_mask(c):
            return None
    our_rel = wf._function_text_relocations(
        ours_data, ours_sections, ours_sym.section_index,
        ours_sym.value, ours_sym.value + ours_sym.size)
    tgt_rel = wf._function_text_relocations(
        tgt_data, tgt_sections, tgt_sym.section_index,
        tgt_sym.value, tgt_sym.value + tgt_sym.size)
    jump = wf._jumptable_targets(
        ours_data, ours_sections, ours_sym.section_index,
        ours_sym.value, ours_sym.value + ours_sym.size)
    calls = {o: nm for o, (k, nm) in our_rel.items() if k == 10}

    strict = None
    try:
        wf.verify_consistent_recolor(
            ours_fn, tgt_fn, jumptable_targets=sorted(jump),
            relocated_offsets=sorted(our_rel), call_targets=calls)
        strict = "PASS"
    except Exception as exc:  # noqa: BLE001
        strict = "refuse: %s" % exc

    subs: list = []
    exch: list = []
    last = None
    for _ in range(MAX_DECLS):
        try:
            wf.verify_value_equality_recolor(
                ours_fn, tgt_fn, jumptable_targets=sorted(jump),
                relocated_offsets=sorted(our_rel),
                target_relocated_offsets=sorted(tgt_rel),
                call_targets=calls, substitutions=subs,
                compare_exchanges=exch)
            return (n, strict, "CLOSES", subs, exch)
        except Exception as exc:  # noqa: BLE001
            msg = str(exc)
            if msg == last:
                return (n, strict, "veq refuse: %s" % msg, subs, exch)
            last = msg
            if "undeclared value-equality substitution" in msg:
                # Aggregate form: the verifier lists every escape it wants
                # declared at once.  _format_substitution renders each as
                # "+0xNN gA->gB".
                found = re.findall(
                    r"\+0x([0-9a-f]+) ([gf])(\d+)->[gf](\d+)", msg)
                added = 0
                for off, bank, a, b in found:
                    entry = {"at": "0x%s" % off, "bank": bank,
                             "ours": int(a), "target": int(b)}
                    if entry not in subs:
                        subs.append(entry)
                        added += 1
                if added:
                    continue
                return (n, strict, "veq refuse: %s" % msg, subs, exch)
            if "undeclared" in msg and "exchange" in msg:
                found = re.findall(
                    r"\+0x([0-9a-f]+) \(([gf])(\d+),[gf](\d+)\)"
                    r"<->\([gf](\d+),[gf](\d+)\)", msg)
                added = 0
                for off, bank, a, b, c, d in found:
                    entry = {"at": "0x%s" % off, "bank": bank,
                             "ours": [int(a), int(b)],
                             "target": [int(c), int(d)]}
                    if entry not in exch:
                        exch.append(entry)
                        added += 1
                if added:
                    continue
                return (n, strict, "veq refuse: %s" % msg, subs, exch)
            m = re.match(r"\+0x([0-9a-f]+): use of ([gf])(\d+) is not "
                         r"value-equal to [gf](\d+)", msg)
            if m:
                entry = {"at": "0x%s" % m.group(1), "bank": m.group(2),
                         "ours": int(m.group(3)), "target": int(m.group(4))}
                if entry in subs:
                    return (n, strict, "veq refuse: %s" % msg, subs, exch)
                subs.append(entry)
                continue
            m = re.match(r"\+0x([0-9a-f]+): comparison .*?"
                         r"\(([gf])(\d+),[gf](\d+)\).*?\([gf](\d+),[gf](\d+)\)",
                         msg)
            if m:
                entry = {"at": "0x%s" % m.group(1), "bank": m.group(2),
                         "ours": [int(m.group(3)), int(m.group(4))],
                         "target": [int(m.group(5)), int(m.group(6))]}
                if entry in exch:
                    return (n, strict, "veq refuse: %s" % msg, subs, exch)
                exch.append(entry)
                continue
            return (n, strict, "veq refuse: %s" % msg, subs, exch)
    return (n, strict, "veq: declaration budget exhausted", subs, exch)


rows = []
for tgt_path in glob.glob(os.path.join(OBJ, "**", "*.o"), recursive=True):
    unit = os.path.relpath(tgt_path, OBJ).replace("\\", "/")[:-2]
    our_path = raw_object(os.path.join(SRC, unit + ".o"))
    if not os.path.exists(our_path):
        continue
    try:
        od, osec = load(our_path)
        td, tsec = load(tgt_path)
        ofns, tfns = functions(od, osec), functions(td, tsec)
    except Exception:  # noqa: BLE001
        continue
    for name, osym in ofns.items():
        tsym = tfns.get(name)
        if tsym is None or tsym.size != osym.size:
            continue
        try:
            res = screen(od, osec, osym, td, tsec, tsym)
        except Exception as exc:  # noqa: BLE001
            continue
        if res is None:
            continue
        rows.append((unit, name) + res)

print("REGISTER-FIELD-ONLY PAIRED FUNCTIONS: %d" % len(rows))
closers = [r for r in rows if r[4] == "CLOSES"]
new_wins = [r for r in closers if r[3] != "PASS"]
print("  strict recolor PASSES : %d" % len([r for r in rows if r[3] == "PASS"]))
print("  value-equality CLOSES : %d" % len(closers))
print("  *** CLOSES BUT STRICT REFUSED (the new population): %d"
      % len(new_wins))
print()
for unit, name, n, strict, verdict, subs, exch in sorted(new_wins):
    print("NEW  %-34s %-30s words=%d subs=%d exch=%d"
          % (unit, name, n, len(subs), len(exch)))
    print("       strict: %s" % strict)
    print("       subs=%s exch=%s" % (subs, exch))
print()
print("STILL REFUSED BY BOTH  (PINNED = already served by a shipped rule; the")
print("raw-body read is deliberate, so shipped rules appear here as canaries):")
open_rows, pinned_rows = [], []
for unit, name, n, strict, verdict, subs, exch in sorted(rows):
    if verdict == "CLOSES" or strict == "PASS":
        continue
    (pinned_rows if name in PINS.get(unit, ()) else open_rows).append(
        (unit, name, n, verdict))
print("  -- PINNED (%d) --" % len(pinned_rows))
for unit, name, n, verdict in pinned_rows:
    print("     %-30s %-30s words=%d  %s" % (unit, name, n, verdict))
print("  -- GENUINELY OPEN (%d) --" % len(open_rows))
for unit, name, n, verdict in open_rows:
    print("     %-30s %-30s words=%d  %s" % (unit, name, n, verdict))

