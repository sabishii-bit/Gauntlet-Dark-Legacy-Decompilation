#!/usr/bin/env python3
"""CV lane driver: compile a TU's RAW body object under compiler/flag variants
and score every function against the dtk-extracted target.

Fidelity is established from build.ninja itself: the baseline command line is
read out of the generated build graph (not a matchtool preset), compiled to a
scratch object, and required to be BYTE-IDENTICAL to the checked-in
build/.../.postprocess/body/<unit>.o before any variant is trusted.

Run from the repository root, after a green `ninja` (it needs both the split
target objects and the shipped body objects to exist):

  python tools/gdl/composed_census/cv_probe.py game/sys/sysservice --axes check
  python tools/gdl/composed_census/cv_probe.py game/audio/audio --axes opt
  python tools/gdl/composed_census/cv_probe.py game/sys/ml_mem --axes mw -j 1
  python tools/gdl/composed_census/cv_probe.py game/mb/mb_particle \
      --fn getSinCos --axes all

`--axes check` runs the fidelity gate alone and exits 0 only on byte-identity.
The `broke-strict` column is the melee trade-off: how many of that TU's
currently byte-exact functions the variant destroys.  See
claim.law.CV_pinned-residuals-are-unreachable-across-the-local-mwcc-archive.20260901.v1.
"""
import argparse
import concurrent.futures as cf
import re
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]  # repo root (fixed after promotion out of CV_scratch)
VERSION = "GUNE5D"
sys.path.insert(0, str(REPO / "tools" / "gdl"))
import matchtool  # noqa: E402  (reuse its normalizer + scorer)


# ---------------------------------------------------------------- ninja parse
def read_edges():
    """{unit: dict(src, mw, cflags, body_o, rule, extab_padding)}"""
    text = (REPO / "build.ninja").read_text(encoding="utf-8", errors="replace")
    text = re.sub(r"\$\r?\n\s+", " ", text)  # join ninja line continuations
    edges = {}
    # Pinned TUs compile to .postprocess/body/<x>.o (webfrank then rewrites it);
    # unpinned TUs compile straight to src/<x>.o.  Take BOTH -- an earlier
    # revision parsed only the body edges, which silently skipped every
    # unpinned TU in the population sweep.
    for m in re.finditer(
            r"^build (\S*build\\%s\\src\\\S+\.o):\s+(mwcc\S*)\s+"
            r"(\S+?\.(?:cpp|c))(?=[\s|])[^\n]*\n((?:  \S+ = [^\n]*\n)+)"
            % VERSION, text, re.M):
        body_o, rule, src, block = m.groups()
        vars = dict(re.findall(r"^  (\S+) = (.*)$", block, re.M))
        unit = body_o.replace("\\", "/")
        unit = re.sub(r"^.*build/%s/src/" % VERSION, "", unit)
        raw = ".postprocess/body/" in unit
        unit = unit.replace(".postprocess/body/", "")
        unit = re.sub(r"\.o$", "", unit)
        if unit in edges and not raw:
            continue  # prefer the raw pre-webfrank edge
        edges[unit] = {
            "src": src, "rule": rule, "body_o": body_o, "raw": raw,
            "mw": vars.get("mw_version", "").replace("\\", "/"),
            "cflags": vars.get("cflags", ""),
            "extab_padding": vars.get("extab_padding"),
        }
    return edges


def compile_with(edge, mw, cflags, out_o, workdir):
    """Compile edge's source with the given compiler dir + cflags string."""
    mwcc = REPO / "build" / "compilers" / mw / "mwcceppc.exe"
    if not mwcc.exists():
        return None, f"missing compiler {mw}"
    sjis = REPO / "build" / "tools" / "sjiswrap.exe"
    args = shlex.split(cflags, posix=True)
    cmd = ([str(sjis)] if "sjis" in edge["rule"] else []) + [str(mwcc)] + args + [
        "-c", edge["src"], "-o", str(out_o)]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=str(REPO))
    if r.returncode or not Path(out_o).exists():
        first = next((l for l in (r.stderr + r.stdout).splitlines() if l.strip()), "?")
        return None, f"COMPILE FAIL: {first[:90]}"
    if "extab" in edge["rule"] and edge.get("extab_padding") is not None:
        dtk = REPO / "build" / "tools" / "dtk.exe"
        subprocess.run([str(dtk), "extab", "clean", "--padding",
                        edge["extab_padding"], str(out_o), str(out_o)],
                       capture_output=True, text=True, cwd=str(REPO))
    return Path(out_o), None


# ------------------------------------------------------------------ variants
# Axes NOT covered by matchtool's 24-combo matrix (which only varies
# -O4[,p] / -Cpp_exceptions / -str / -use_lmw_stmw).
OPT_AXIS = [
    "-opt nolifetimes", "-opt lifetimes",
    "-opt nopropagation", "-opt noschedule", "-opt schedule",
    "-opt nopeephole", "-opt peephole",
    "-opt nocse", "-opt nodeadcode", "-opt nodeadstore",
    "-opt noloopinvariants", "-opt nostrength", "-opt nodead",
    "-opt nofunctions", "-opt space", "-opt speed",
    "-opt level=3", "-opt level=2", "-opt level=1",
    "-schedule on", "-schedule off",
]
INLINE_AXIS = [
    "-inline auto,level=1", "-inline auto,level=2", "-inline auto,level=3",
    "-inline noauto", "-inline all", "-inline deferred", "-inline on",
    "-inline off",
]
ALIGN_AXIS = ["-func_align 4", "-func_align 8", "-func_align 16", "-func_align 32"]
MW_AXIS = ["GC/1.2.5", "GC/1.2.5n", "GC/1.1", "GC/1.1p1", "GC/1.0",
           "GC/1.3", "GC/1.3.2", "GC/1.3.2r", "GC/2.0", "GC/2.0p1",
           "GC/2.5", "GC/2.6", "GC/2.7", "GC/3.0a3", "GC/3.0a3.2",
           "GC/3.0a3.3", "GC/3.0a3.4", "GC/3.0a3p1", "GC/3.0a5", "GC/3.0a5.2",
           "Wii/1.0", "Wii/1.1", "Wii/1.3"]


def variants(edge, which):
    """[(label, mw, cflags)]"""
    base_mw, base_cf = edge["mw"], edge["cflags"]
    out = [("BASE", base_mw, base_cf)]
    if which in ("opt", "all"):
        for f in OPT_AXIS:
            out.append((f.replace(" ", ""), base_mw, base_cf + " " + f))
    if which in ("inline", "all"):
        for f in INLINE_AXIS:
            out.append((f.replace(" ", "").replace(",", "_"), base_mw, base_cf + " " + f))
    if which in ("align", "all"):
        for f in ALIGN_AXIS:
            out.append((f.replace(" ", ""), base_mw, base_cf + " " + f))
    if which in ("mw", "all"):
        for mw in MW_AXIS:
            if mw == base_mw:
                continue
            out.append(("mw:" + mw.replace("/", "_"), mw, base_cf))
    if which in ("mwopt", "all2"):
        # cross the two proven compilers with the two most promising opt flags
        for mw in ("GC/1.2.5", "GC/1.2.5n", "GC/1.3", "GC/1.3.2"):
            for f in ("-opt nolifetimes", "-schedule on", "-opt nopropagation",
                      "-opt nocse"):
                out.append((f"{mw.split('/')[-1]}+{f.split()[-1]}", mw, base_cf + " " + f))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("unit")
    ap.add_argument("--axes", default="check")
    ap.add_argument("--fn", action="append")
    ap.add_argument("-j", type=int, default=6)
    ap.add_argument("--quiet-ok", action="store_true")
    a = ap.parse_args()

    edges = read_edges()
    unit = a.unit.replace("\\", "/")
    if unit not in edges:
        print(f"no body edge for {unit}; sample: {list(edges)[:4]}")
        return 1
    edge = edges[unit]
    target_o = REPO / "build" / VERSION / "obj" / f"{unit}.o"
    target_fns = matchtool.parse(target_o)

    with tempfile.TemporaryDirectory(prefix="cvprobe_") as td:
        # --- fidelity gate -------------------------------------------------
        base_out = Path(td) / "base.o"
        got, err = compile_with(edge, edge["mw"], edge["cflags"], base_out, td)
        if err:
            print("BASELINE", err)
            return 1
        shipped = (REPO / edge["body_o"]).read_bytes()
        mine = got.read_bytes()
        fid = "BYTE-IDENTICAL" if mine == shipped else \
              f"DIFFERS ({len(mine)} vs {len(shipped)} bytes)"
        print(f"unit={unit}  mw={edge['mw']}  rule={edge['rule']}")
        print(f"fidelity vs shipped body object: {fid}")
        if a.axes == "check":
            return 0 if mine == shipped else 2

        base_scores = matchtool.score(target_fns, matchtool.parse(base_out))
        fns = a.fn or [n for n, v in base_scores.items() if str(v) not in ("OK", "OK~")]
        print(f"scoring {len(fns)} function(s): {', '.join(fns[:12])}")
        print("baseline: " + "  ".join(f"{n}={base_scores.get(n)}" for n in fns))

        jobs = variants(edge, a.axes)
        def one(v):
            label, mw, cf = v
            o = Path(td) / (re.sub(r"\W", "_", label) + ".o")
            got, err = compile_with(edge, mw, cf, o, td)
            if err:
                return label, err
            return label, matchtool.score(target_fns, matchtool.parse(o))

        results = {}
        with cf.ThreadPoolExecutor(max_workers=a.j) as ex:
            for label, res in ex.map(one, jobs):
                results[label] = res

        # ---- report -------------------------------------------------------
        strict = [n for n, v in base_scores.items() if str(v) in ("OK", "OK~")]
        print(f"\n{'variant':30s} {'target-fns':>10s}  {'broke-strict':>12s}   detail")
        rows = []
        for label, r in results.items():
            if not isinstance(r, dict):
                rows.append((10**9, label, r, 0))
                continue
            tot = matchtool.total_key({n: r.get(n, "MISS") for n in fns})
            broke = sum(1 for n in strict if str(r.get(n, "MISS")) not in ("OK", "OK~"))
            detail = " ".join(f"{n}={r.get(n)}" for n in fns)
            rows.append((tot, label, detail, broke))
        for tot, label, detail, broke in sorted(rows):
            if a.quiet_ok and tot >= 10**9:
                continue
            flag = "EXACT!" if tot == 0 else ""
            print(f"{label:30s} {str(tot):>10s}  {broke:>12d}   {detail[:110]} {flag}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
