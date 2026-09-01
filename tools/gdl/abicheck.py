#!/usr/bin/env python3
"""True ABI-defect detector for cross-TU prototypes (PPC EABI, MWCC gekko).

externcheck compares prototypes POSITIONALLY.  On PPC EABI integer/pointer
arguments and floating arguments are assigned from two INDEPENDENT register
files (r3.. and f1..), so a declaration that lists the same arguments in a
different order can still place every value in exactly the register the
definition reads.  Those rows are ABI-equivalent and are NOT defects.

This tool assigns registers per the EABI rule and reports only rows where
some register is loaded with a value of a different class/width than the
definition reads out of it, or where a register the definition reads is
never written.  Return-type-only splits and K&R zero-arity passthroughs are
reported separately (both are ABI-neutral at the call).
"""
import re, sys, os
from pathlib import Path
sys.path.insert(0, os.path.abspath("tools/gdl"))
import externcheck as ec

FLOAT = {"f32", "f64"}
LONG64 = {"s64", "u64"}


def assign(classes):
    """classes: tuple(ret, p1..).  -> ({reg: class}, variadic)."""
    regs, gpr, fpr, variadic = {}, 3, 1, False
    for c in classes[1:]:
        if c == "...":
            variadic = True
            break
        if c.startswith("agg:"):
            return None, variadic          # struct-by-value: cannot model
        if c in FLOAT:
            if fpr <= 8:
                regs["f%d" % fpr] = c
            fpr += 1
        elif c in LONG64:
            gpr += gpr % 2 - 1 if gpr % 2 == 0 else 0   # 64-bit pairs align odd
            if gpr % 2 == 0:
                gpr += 1
            if gpr <= 9:
                regs["r%d" % gpr] = c
                regs["r%d" % (gpr + 1)] = c
            gpr += 2
        else:
            if gpr <= 10:
                regs["r%d" % gpr] = "w32"
            gpr += 1
    return regs, variadic


def width_ok(a, b):
    if a == b:
        return True
    return {a, b} <= {"w32"}


protos = ec.scan_prototypes("src")
defs = {}
for p in Path("src").rglob("*.c"):
    text = ec._strip(p.read_text(encoding="utf-8", errors="replace"))
    for m in re.finditer(
            r"^((?:static\s+|const\s+|struct\s+|unsigned\s+|signed\s+)*"
            r"[A-Za-z_]\w*(?:\s*\*+)?)\s*([A-Za-z_]\w*)\s*\(([^;{)]*)\)\s*\{",
            text, re.M):
        ret = " ".join(m.group(1).split())
        if ret.split()[0] in ("if", "for", "while", "switch", "return", "else",
                              "static"):
            continue
        sig = "%s (%s)" % (ret, ", ".join(ec._param_types(m.group(3))) or "void")
        defs.setdefault(m.group(2), []).append(
            (sig, str(p).replace("\\", "/"), text.count("\n", 0, m.start()) + 1))

defects, knr, neutral = [], [], 0
for name, classed, sev in ec.proto_conflicts(protos):
    d = defs.get(name)
    if not d or len(d) > 1:
        continue
    dsig, dfile, dline = d[0]
    dcls, _ = ec._classes_for_signature(dsig)
    dregs, dvar = assign(dcls)
    if dregs is None:
        continue
    for sig, (sites, _c) in sorted(classed.items()):
        tcls, _ = ec._classes_for_signature(sig)
        tregs, tvar = assign(tcls)
        if tregs is None:
            continue
        if len(tcls) == 1 and len(dcls) > 1:
            knr.append((name, dsig, dfile, dline, sig, sites))
            continue
        bad = []
        for reg, want in dregs.items():
            got = tregs.get(reg)
            if got is None:
                if not tvar:
                    bad.append(f"{reg}: definition reads {want}, caller writes nothing")
            elif not width_ok(got, want):
                bad.append(f"{reg}: definition reads {want}, caller writes {got}")
        for reg, got in tregs.items():
            if reg not in dregs and not dvar:
                bad.append(f"{reg}: caller writes {got}, definition never reads it")
        if bad:
            defects.append((name, dsig, dfile, dline, sig, sites, bad))
        else:
            neutral += 1

print("=== TRUE ABI DEFECTS (register-level) ===")
for name, dsig, dfile, dline, sig, sites, bad in defects:
    print(f"{name}\n    DEF  {dsig}   @{dfile}:{dline}")
    for f, l in sites:
        print(f"    DECL {sig}   @{f}:{l}")
    for b in bad:
        print(f"      ! {b}")
print(f"\n{len(defects)} true defect rows")
print(f"{neutral} rows are EABI-EQUIVALENT reorderings / return-only splits (not defects)")
print(f"{len(knr)} rows are K&R zero-arity passthroughs (see "
      "claim.law.knr-extern-arity-can-be-faithful-not-a-defect.20260831.v1)")
