"""WF lane run 37: image-wide census of the LIVE-ZERO VALUE class.

Answers the demand-census question the class was commissioned under, and the
one every future lane will ask: how many differing words image-wide are pairs
that this class -- and ONLY this class -- can take?

A candidate word is one where our word and the target's word at the same
offset both decode inside the class's opcode whitelist and write the SAME
GPR.  A candidate is PROVED when the class's own dataflow shows both results
are the literal zero on every path.  Words the existing equivalent_copy_form
already serves (both sides decode as a copy/li pair) are reported separately,
so the number attributable to THIS class is not inflated by them.

Offset-paired, so on schedule-class functions it both over- and under-reports
(claim.law.WF_range-proof-population-is-one-and-offset-paired-censuses-lie-
in-both-directions.20260901.v1).  A hit is a CANDIDATE, never a licence.

FOUNDING RUN, 2026-09-02, over 3283 count-symmetric function pairs: 530
candidate words in 119 functions, **0 proved**, alongside 607 words the
existing copy-form class already models.  READ THAT ZERO CORRECTLY -- it is a
measurement of the METHOD, not of the class.  Both shipped members of the
class (btext::DrawStringTextMLines +0x1f4, btext::FontInit +0x28) are INVISIBLE
here, because at their raw offsets our word is the neighbouring frame/base
computation and the class's own destination-agreement screen discards the pair;
the members only exist after the permutation stage has run.  An offset-paired
census cannot see any class whose members need a permutation first, so use this
tool to size the PERMUTATION-FREE population and never as a negative screen.
The 530 refusals break down as 417 "result form not in the known-zero table"
(overwhelmingly ordinary immediate differences between two `addi rD,rA,K`
words, correctly outside the class), 110 "not provably zero on every path",
2 unreachable sites and 1 unmodelled control form.
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = HERE
while not os.path.isdir(os.path.join(ROOT, "config", "GUNE5D")):
    parent = os.path.dirname(ROOT)
    if parent == ROOT:
        raise SystemExit("repo root not found above " + HERE)
    ROOT = parent
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl"))
sys.path.insert(0, os.path.join(ROOT, "tools", "gdl", "composed_census"))

import webfrank as wf                                     # noqa: E402
from cn_analyze import our_object, target_object, load     # noqa: E402


def units():
    for base, _dirs, files in os.walk(os.path.join(ROOT, "build/GUNE5D/obj")):
        for name in files:
            if not name.endswith(".o"):
                continue
            path = os.path.join(base, name)
            unit = os.path.relpath(path, os.path.join(ROOT, "build/GUNE5D/obj"))
            yield unit[:-2].replace("\\", "/")


def symbols(path):
    data = open(path, "rb").read()
    sections = wf._sections(data)
    names = []
    for symbol in wf._symbols(data, sections):
        if symbol.size and symbol.name:
            names.append(symbol.name)
    return names


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--out", default=os.path.join(
        ROOT, "build/GUNE5D/wf_zero_census.json"))
    arguments = parser.parse_args()

    rows = []
    copy_form_words = 0
    scanned = 0
    for unit in sorted(units()):
        try:
            ours_path, _kind = our_object(unit)
            target_path = target_object(unit)
        except Exception:
            continue
        if not (os.path.exists(ours_path) and os.path.exists(target_path)):
            continue
        try:
            names = set(symbols(target_path)) & set(symbols(ours_path))
        except Exception:
            continue
        for name in sorted(names):
            try:
                _a, _b, _c, ours, orel, ojt = load(ours_path, name)
                _d, _e, _f, target, trel, _tj = load(target_path, name)
            except Exception:
                continue
            if len(ours) != len(target) or not ours or len(ours) % 4:
                continue
            scanned += 1
            relocated = {offset // 4 for offset in orel}
            target_relocated = {offset // 4 for offset in trel}
            words = [wf._u32(ours, o) for o in range(0, len(ours), 4)]
            successors = calls = None
            for offset in range(0, len(ours), 4):
                word = wf._u32(ours, offset)
                wanted = wf._u32(target, offset)
                if word == wanted:
                    continue
                index = offset // 4
                if index in relocated or index in target_relocated:
                    continue
                try:
                    destination = wf.decode_zero_form_destination(word)
                    if destination != wf.decode_zero_form_destination(wanted):
                        continue
                except ValueError:
                    continue
                ours_form = wf.decode_copy_form(word)
                target_form = wf.decode_copy_form(wanted)
                served = (ours_form is not None and target_form is not None
                          and ours_form[1] == target_form[1])
                if served:
                    copy_form_words += 1
                    continue
                if successors is None:
                    try:
                        successors, calls = wf._successors(
                            words, relocated, {o // 4 for o in ojt})
                    except ValueError as error:
                        # A control form webfrank does not model: the class
                        # would fail closed on this function anyway.
                        rows.append({
                            "unit": unit, "function": name, "at": hex(offset),
                            "ours": f"{word:08x}", "target": f"{wanted:08x}",
                            "register": destination, "proved": False,
                            "refusal": f"cfg: {error}",
                        })
                        break
                proved = True
                failure = ""
                for candidate in (word, wanted):
                    try:
                        wf.prove_zero_result(
                            words, index, candidate, destination,
                            successors, calls, relocated,
                            relocation_types={
                                o // 4: t for o, (t, _n) in orel.items()},
                        )
                    except ValueError as error:
                        proved = False
                        failure = str(error)
                        break
                rows.append({
                    "unit": unit, "function": name, "at": hex(offset),
                    "ours": f"{word:08x}", "target": f"{wanted:08x}",
                    "register": destination, "proved": proved,
                    "refusal": "" if proved else failure,
                })

    proved = [row for row in rows if row["proved"]]
    print(f"scanned {scanned} count-symmetric function pairs")
    print(f"live-zero-class CANDIDATE words (outside equivalent_copy_form's "
          f"reach): {len(rows)}")
    print(f"  PROVED by the class's own dataflow: {len(proved)}")
    print(f"  refused: {len(rows) - len(proved)}")
    print(f"words the EXISTING copy-form class already models: "
          f"{copy_form_words}")
    functions = {}
    for row in proved:
        functions.setdefault((row["unit"], row["function"]), []).append(row)
    print(f"\nPROVED sites by function ({len(functions)} function(s)):")
    for (unit, name), group in sorted(functions.items()):
        print(f"  {unit}::{name}: {len(group)} site(s) "
              + ", ".join(row["at"] for row in group))
    json.dump(rows, open(arguments.out, "w"), indent=2)
    print(f"\nwrote {arguments.out}")


if __name__ == "__main__":
    main()
