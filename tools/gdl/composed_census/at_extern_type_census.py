"""at_extern_type_census.py -- the EXTERN-TYPE census over the ADDR16_LO
home-copy class (class P), plus the per-instance count-parity screen.

Written for run 56 lane AT to decide, with ZERO builds, whether
claim.law.GC_a-named-pointer-local-pays-an-addr16-lo-home-copy-for-an-array-
decay-and-writes-its-home-directly-for-an-address-of-struct.20260904.v1
transfers off its single measured instance (game/pb/pb_error::fn_800C1174).

It answers three questions the corpus had no instrument for:

  1. For every class-P site, is the extern it materialises declared in OUR
     source as an ARRAY (decay), a STRUCT object, a POINTER object or a
     SCALAR?  The law predicts array-decay sites pay the `mr` home copy and
     address-of-struct sites do not.
  2. How many BYTE-EXACT shape-A homes (no copy paid) does each symbol have?
     A byte-exact shape-A home of an ARRAY-typed extern is the law's own
     falsifier, and so is a class-P payer whose extern is already a struct.
  3. For each class-P function, do the two streams have EQUAL instruction
     counts?  claim.law.webfrank-cannot-close-instruction-count-deltas
     .20260831.v1 makes a count-asymmetric residual provably outside every
     postprocessor class, so this column decides rule eligibility for the
     whole class rather than per function.

The class-P roster is derived exactly as al_control.py derives it (ours has
more shape-B/V sites than the target), by importing al_addrlo_positive's
parser -- one objdump parser and one regex set for both sides, so the
hex-vs-decimal immediate hazard of the .s-text censuses cannot arise.

Usage: python tools/gdl/composed_census/at_extern_type_census.py [--out PATH]
       (from the repo root; --out defaults under build/GUNE5D/)
"""
import os
import re
import sys
import json
import collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.getcwd(), "tools", "gdl"))
from pathlib import Path
import fndiff

# al_addrlo_positive runs main() at import; load it with that call stripped,
# exactly as al_control.py / al_twin.py do.
_src = open(os.path.join(HERE, "al_addrlo_positive.py")).read().replace(
    "\nmain()\n", "\n")
mod = type(sys)("alpos")
mod.__dict__["__file__"] = os.path.join(HERE, "al_addrlo_positive.py")
exec(compile(_src, "al_addrlo_positive.py", "exec"), mod.__dict__)

TGT, OURS = mod.TGT, mod.OURS

SCALARS = {"char", "signed char", "unsigned char", "short", "int", "long",
           "float", "double", "u8", "s8", "u16", "s16", "u32", "s32", "f32",
           "f64", "BOOL", "bool", "void"}

RANK = {"array-decay": 0, "struct-typed": 1, "pointer-object": 2,
        "scalar": 3, "unknown": 4}


def load():
    units = []
    for root, _d, names in os.walk(OURS):
        if ".postprocess" in root.replace("\\", "/"):
            continue
        for n in sorted(names):
            if not n.endswith(".o"):
                continue
            op = os.path.join(root, n)
            rel = os.path.relpath(op, OURS)
            tp = os.path.join(TGT, rel)
            if os.path.exists(tp):
                units.append((rel.replace("\\", "/")[:-2], tp, op))
    tgt, ours, exact, counts = {}, {}, set(), {}
    for unit, tp, op in units:
        # fndiff.parse is the BYTE-EXACT test and only that: its lists hold
        # normalized instruction AND relocation lines, so len() is NOT an
        # instruction count (LoadWorldDone: 84 insns + 37 relocs = 121).
        # Instruction counts come from fns_of, which folds each relocation
        # onto the instruction it belongs to.
        tparse, oparse = fndiff.parse(Path(tp)), fndiff.parse(Path(op))
        for name in oparse:
            if name in tparse and tparse[name] == oparse[name]:
                exact.add((unit, name))
        tn = {}
        for name, ins in mod.fns_of(tp):
            tgt[(unit, name)] = mod.analyse(ins)
            tn[name] = len(ins)
        for name, ins in mod.fns_of(op):
            ours[(unit, name)] = mod.analyse(ins)
            if name in tn:
                counts[(unit, name)] = (tn[name], len(ins))
    return tgt, ours, exact, counts


def source_files(root):
    out = []
    for base in ("src", "include"):
        b = os.path.join(root, base)
        for dirpath, _d, names in os.walk(b):
            for n in names:
                if n.endswith((".c", ".h", ".cpp")):
                    out.append(os.path.join(dirpath, n))
    return out


def classify_symbols(syms, root):
    hits = {s: [] for s in syms}
    for p in source_files(root):
        try:
            txt = open(p, encoding="utf-8", errors="replace").read()
        except OSError:
            continue
        for s in syms:
            if s not in txt:
                continue
            for i, line in enumerate(txt.splitlines(), 1):
                if not re.search(r"\b" + re.escape(s) + r"\s*(\[|;|=)", line):
                    continue
                ls = line.strip()
                if "(" in ls.split(s)[0]:
                    continue
                if not re.match(r"^(extern\s|static\s|const\s|struct\s|union\s"
                                r"|[A-Za-z_][A-Za-z_0-9]*\s+\**"
                                + re.escape(s) + r"\b)", ls):
                    continue
                hits[s].append((os.path.relpath(p, root).replace("\\", "/"),
                                i, ls))
    out = {}
    for s in syms:
        best = None
        for path, ln, ls in hits[s]:
            d = re.sub(r"/\*.*?\*/", "", ls)
            d = re.sub(r"//.*$", "", d).strip()
            core = d.rstrip(";").strip()
            if "[" in core.split("=")[0]:
                cls = "array-decay"
            else:
                toks = core.split("=")[0].strip().replace("*", " * ").split()
                toks = [t for t in toks if t not in
                        ("extern", "static", "const", "volatile")]
                if not toks:
                    cls = "unknown"
                elif "*" in toks:
                    cls = "pointer-object"
                else:
                    typ = (" ".join(toks[:-1]) if len(toks) > 1 else toks[0])
                    typ = typ.replace("struct ", "").replace("union ", "")
                    cls = "scalar" if typ.strip() in SCALARS else "struct-typed"
            if best is None or RANK[cls] < RANK[best[0]]:
                best = (cls, f"{path}:{ln}: {d}")
        out[s] = dict(cls=best[0] if best else "NO-DECL-FOUND",
                      evidence=best[1] if best else "",
                      n_decl_lines=len(hits[s]))
    return out


def main():
    root = os.getcwd()
    out_path = os.path.join("build", "GUNE5D", "at_extern_type_census.json")
    if "--out" in sys.argv:
        out_path = sys.argv[sys.argv.index("--out") + 1]

    tgt, ours, exact, counts = load()
    common = sorted(k for k in ours if k in tgt)

    def bcount(a):
        return sum(1 for s in a["sites"] if s["shape"] in ("B", "V"))

    P = [k for k in common if bcount(ours[k]) > bcount(tgt[k])]
    rows = []
    for k in P:
        for os_ in [s for s in ours[k]["sites"] if s["shape"] in ("B", "V")]:
            ts = [s for s in tgt[k]["sites"]
                  if s["sym"] == os_["sym"] and s["shape"] == "A"] or \
                 [s for s in tgt[k]["sites"]
                  if s["home"] == os_["home"] and s["shape"] == "A"]
            t, o = counts.get(k, (None, None))
            rows.append(dict(unit=k[0], fn=k[1], sym=os_["sym"],
                             ours_home=os_["home"], ours_gap=os_["idx"] - os_["lis"]
                             if os_["lis"] is not None else None,
                             target_paired=bool(ts),
                             target_home=ts[0]["home"] if ts else None,
                             target_insns=t, ours_insns=o,
                             count_parity=(t == o) if t is not None else None))

    syms = sorted({r["sym"] for r in rows})
    sym_class = classify_symbols(syms, root)

    # byte-exact shape-A homes per symbol: the falsifier column
    exact_A = collections.Counter()
    exact_A_examples = collections.defaultdict(list)
    for k in common:
        if k not in exact:
            continue
        for s in ours[k]["sites"]:
            if s["shape"] == "A" and s["sym"] in sym_class:
                exact_A[s["sym"]] += 1
                if len(exact_A_examples[s["sym"]]) < 3:
                    exact_A_examples[s["sym"]].append(f"{k[0]}::{k[1]}")

    for r in rows:
        r["extern_class"] = sym_class[r["sym"]]["cls"]
        r["symbol_exact_shapeA_homes"] = exact_A[r["sym"]]

    inst_tal = collections.Counter(r["extern_class"] for r in rows)
    sym_tal = collections.Counter(v["cls"] for v in sym_class.values())
    parity = collections.Counter(str(r["count_parity"]) for r in rows)
    fns = {(r["unit"], r["fn"]) for r in rows}
    fn_parity = collections.Counter(
        str(counts[(u, f)][0] == counts[(u, f)][1]) for u, f in fns
        if (u, f) in counts)

    # the two falsifier populations
    struct_payers = [r for r in rows if r["extern_class"] == "struct-typed"]
    array_exact_A = {s: exact_A[s] for s in syms
                     if sym_class[s]["cls"] == "array-decay" and exact_A[s]}

    out = dict(p_instances=len(rows), p_functions=len(fns), p_symbols=len(syms),
               instance_class_tally=dict(inst_tal),
               symbol_class_tally=dict(sym_tal),
               instance_count_parity=dict(parity),
               function_count_parity=dict(fn_parity),
               struct_typed_payers=len(struct_payers),
               array_decay_byte_exact_shapeA_homes=sum(array_exact_A.values()),
               symbols=sym_class, rows=rows,
               exact_shapeA_examples={k: v for k, v in exact_A_examples.items()})
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1)

    print(f"class-P instances : {len(rows)}")
    print(f"class-P functions : {len(fns)}")
    print(f"class-P symbols   : {len(syms)}")
    print()
    print("EXTERN CLASS, per instance:", dict(inst_tal))
    print("EXTERN CLASS, per symbol  :", dict(sym_tal))
    print()
    print("FALSIFIER A -- class-P payers whose extern is ALREADY a struct")
    print(f"  count: {len(struct_payers)}")
    for r in struct_payers:
        print(f"    {r['unit']}::{r['fn']:34} {r['sym']:22} "
              f"{sym_class[r['sym']]['evidence'][:70]}")
    print()
    print("FALSIFIER B -- BYTE-EXACT shape-A homes of ARRAY-typed externs")
    print(f"  count: {sum(array_exact_A.values())}")
    for s, n in sorted(array_exact_A.items(), key=lambda x: -x[1]):
        print(f"    {s:24} {n:3}  e.g. {', '.join(exact_shapeA_ex(exact_A_examples, s))}")
    print()
    print("COUNT PARITY (postprocessor eligibility, per class-P FUNCTION)")
    print(f"  {dict(fn_parity)}   True = T==O insns = rule-eligible on count")
    print("  NOTE: count parity is necessary, NOT sufficient. The opcode")
    print("  MULTISET must also agree; MBOX_BGLoadModelStart is 71/71 and")
    print("  still carries target-only +1 add / ours-only -1 mr.")
    print()
    print(f"  {'unit::fn':50} {'sym':22} {'class':14} {'T/O insns':>9} {'par':>5} {'exA':>4}")
    for r in rows:
        print(f"  {(r['unit']+'::'+r['fn'])[:50]:50} {r['sym'][:22]:22} "
              f"{r['extern_class']:14} "
              f"{str(r['target_insns'])+'/'+str(r['ours_insns']):>9} "
              f"{str(r['count_parity']):>5} {r['symbol_exact_shapeA_homes']:>4}")
    print()
    print("wrote", out_path)


def exact_shapeA_ex(d, s):
    return d.get(s) or ["-"]


main()
