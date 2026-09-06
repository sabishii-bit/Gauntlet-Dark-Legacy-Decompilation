#!/usr/bin/env python3
"""CV lane driver: compile a TU's RAW body object under compiler/flag variants
and score every function against the dtk-extracted target.

Fidelity is established from build.ninja itself: the baseline command line is
read out of the generated build graph (not a matchtool preset), compiled to a
scratch object, and required to be BYTE-IDENTICAL to the current raw Ninja
object before ANY variant is trusted. Build outputs are not checked into Git.

Run from the repository root, after a green `ninja` (it needs both the split
target objects and the shipped body objects to exist):

  python tools/gdl/composed_census/cv_probe.py game/sys/sysservice --axes check
  python tools/gdl/composed_census/cv_probe.py game/audio/audio --axes opt
  python tools/gdl/composed_census/cv_probe.py game/sys/ml_mem --axes mw -j 1
  python tools/gdl/composed_census/cv_probe.py game/mb/mb_particle \
      --fn getSinCos --axes all

`--out` writes schema_version=1 JSON, including failures. Exit 0/PASS means
the requested finite experiment completed, NOT that the source matches or
all possible options were tested. Exit 1/FAIL means a requested command was
rejected; exit 2/UNRESOLVED means fidelity/input/measurement is insufficient.
Scores normalize relocation names; neither OK nor OK~ certifies datum binding.
Use --control-fn with --extra-flags to demonstrate option sensitivity in a
sibling. An unchanged function without a sensitive control is inconclusive.
Source pragmas are inventoried lexically, not evaluated as effective options.
See claim.law.R64_fixed-source-compiler-sweeps-do-not-establish-source-unreachability.20260905.v1.
"""
import argparse
import concurrent.futures as cf
import hashlib
import json
import os
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
from fndiff import unit_key  # noqa: E402


# ---------------------------------------------------------------- ninja parse
def read_edges():
    """{unit: dict(src, mw, cflags, body_o, rule, extab_padding)}"""
    text = (REPO / "build.ninja").read_text(encoding="utf-8", errors="replace")
    text = re.sub(r"\$\r?\n\s+", " ", text)  # join ninja line continuations
    rules = dict(re.findall(r"^rule (\S+)\n  command = ([^\n]+)", text, re.M))
    edges = {}
    # Pinned TUs compile to .postprocess/body/<x>.o (webfrank then rewrites it);
    # unpinned TUs compile straight to src/<x>.o.  Take BOTH -- an earlier
    # revision parsed only the body edges, which silently skipped every
    # unpinned TU in the population sweep.
    for m in re.finditer(
            r"^build (\S*build[\\/]%s[\\/]src[\\/]\S+\.o):\s+(mwcc\S*)\s+"
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
            "src": src.replace("\\", "/"), "rule": rule,
            "body_o": body_o.replace("\\", "/"), "raw": raw,
            "mw": vars.get("mw_version", "").replace("\\", "/"),
            "cflags": vars.get("cflags", ""),
            "extab_padding": vars.get("extab_padding"),
            "command_template": rules.get(rule),
        }
    return edges


def compile_commands(edge, mw, cflags, out_o):
    """Reproduce the compiler runner from Ninja; never guess wine on Linux.

    Only the compile and object-mutating cleanup stages are replayed. Ninja's
    dependency-file transformation is not part of the measured object bytes.
    """
    mwcc = REPO / "build" / "compilers" / mw / "mwcceppc.exe"
    if not mwcc.exists():
        raise ValueError(f"missing compiler {mw}")
    sjis = REPO / "build" / "tools" / "sjiswrap.exe"
    runner = []
    template = edge.get("command_template")
    if template:
        prefix = template.split("$cflags", 1)[0].replace("\\", "/").strip()
        # The generated Windows chain prefix is unnecessary without a shell.
        prefix = re.sub(r"^cmd /c\s+", "", prefix)
        marker = "build/tools/sjiswrap.exe" if "sjis" in edge["rule"] else "build/compilers/$mw_version/mwcceppc.exe"
        if marker not in prefix:
            raise ValueError("unsupported Ninja compiler command template")
        runner = shlex.split(prefix.split(marker, 1)[0])
        if any("$" in token or token in ("&&", "|") for token in runner):
            raise ValueError("unresolved Ninja compiler runner")
    elif os.name != "nt":
        raise ValueError("missing Ninja command template: cannot establish compiler runner")
    if os.name != "nt" and not runner:
        raise ValueError("Ninja template supplies no Windows compiler runner on this host")
    args = shlex.split(cflags, posix=True)
    cmd = runner + ([str(sjis)] if "sjis" in edge["rule"] else []) + [str(mwcc)] + args + [
        "-c", edge["src"], "-o", str(out_o)]
    commands = [("compile", cmd)]
    if "extab" in edge["rule"] and edge.get("extab_padding") is not None:
        dtk = REPO / "build" / "tools" / ("dtk.exe" if os.name == "nt" else "dtk")
        commands.append(("extab_clean", [str(dtk), "extab", "clean", "--padding",
                                        edge["extab_padding"], str(out_o), str(out_o)]))
    return commands


def compile_with(edge, mw, cflags, out_o, workdir):
    """Legacy (path, error) API; optional edge['_command_trace'] records execution."""
    trace = edge.get("_command_trace", [])
    try:
        commands = compile_commands(edge, mw, cflags, out_o)
    except (OSError, ValueError) as exc:
        return None, "UNRESOLVED: " + str(exc)
    # A rejected invocation must never inherit a prior successful output.
    Path(out_o).unlink(missing_ok=True)
    for stage, cmd in commands:
        row = {"stage": stage, "argv": cmd, "cwd": str(REPO)}
        trace.append(row)
        try:
            r = subprocess.run(cmd, capture_output=True, text=True, errors="replace",
                               cwd=str(REPO), timeout=120)
        except (OSError, subprocess.TimeoutExpired) as exc:
            row.update(status="UNRESOLVED", error=str(exc))
            return None, f"UNRESOLVED {stage}: {exc}"
        messages = r.stderr + r.stdout
        row.update(returncode=r.returncode, stdout=r.stdout, stderr=r.stderr)
        rejected = re.search(r"(?:unknown|unrecognized|illegal|invalid|ignored)\s+(?:command.line\s+)?option|option[^\n]*(?:ignored|unknown|unrecognized|invalid)", messages, re.I)
        if r.returncode or rejected:
            row["status"] = "FAIL"
            first = next((line for line in messages.splitlines() if line.strip()), "no diagnostic")
            return None, f"FAIL {stage} (exit {r.returncode}): {first[:240]}"
        if not Path(out_o).is_file():
            row["status"] = "UNRESOLVED"
            return None, f"UNRESOLVED {stage}: command succeeded without object output"
        row["status"] = "PASS"
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
AXES = ("check", "opt", "inline", "align", "mw", "all", "mwopt", "all2")


def variants(edge, which):
    """[(label, mw, cflags)]"""
    if which not in AXES:
        raise ValueError(f"unknown axes {which!r}; choose from {', '.join(AXES)}")
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
        # Selected 4-by-4 cross-product, NOT the Cartesian product of all axes.
        for mw in ("GC/1.2.5", "GC/1.2.5n", "GC/1.3", "GC/1.3.2"):
            for f in ("-opt nolifetimes", "-schedule on", "-opt nopropagation",
                      "-opt nocse"):
                out.append((f"{mw.split('/')[-1]}+{f.split()[-1]}", mw, base_cf + " " + f))
    return out


def sha(data):
    return hashlib.sha256(data).hexdigest()


def pragma_inventory(source):
    """Lexical evidence only: conditional/includes/macros need compiler controls."""
    path = REPO / source
    if not path.is_file():
        return {"status": "UNRESOLVED", "reason": "source file missing", "directives": []}
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    directives = []
    for i, line in enumerate(lines, 1):
        m = re.match(r"\s*#\s*pragma\s+(\w+)\b(.*)", line)
        if m:
            directives.append({"line": i, "name": m[1], "value": m[2].strip(),
                               "text": line.strip(), "lexical_region_end": len(lines)})
    for index, row in enumerate(directives):
        next_same = next((d for d in directives[index+1:] if d["name"] == row["name"]), None)
        if next_same:
            row["lexical_region_end"] = next_same["line"] - 1
    return {"status": "PASS", "source_sha256": sha(path.read_bytes()),
            "directives": directives, "effective_options": "UNRESOLVED",
            "scope_note": "Line ranges end before the next same-name directive or EOF; lexical only. Conditional preprocessing, includes, push/pop and MWCC option precedence are not evaluated. No canonical effective flags are inferred."}


def _assembly_hashes(functions):
    # These are intentionally NOT byte hashes or relocation-identity proofs.
    return {name: sha("\n".join(lines).encode()) for name, lines in functions.items()}


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("unit")
    ap.add_argument("--axes", default="check")
    ap.add_argument("--fn", action="append")
    ap.add_argument("-j", type=int, default=2)
    ap.add_argument("--quiet-ok", action="store_true")
    ap.add_argument("--out", type=Path, help="schema_version=1 JSON, including refusals")
    ap.add_argument("--extra-flags", action="append", default=[],
                    help="append one bounded custom trial per string; use --extra-flags='-opt level=1'")
    ap.add_argument("--control-fn", action="append", default=[],
                    help="sibling whose assembly must change to establish option sensitivity")
    a = ap.parse_args(argv)
    # run-43 item 8: the edge keys are the canonical `game/x/y` spelling, so
    # a `game/x/y.c` argument used to report "no body edge for ..." — which
    # reads as an unpinned TU, not as a spelling.
    unit = unit_key(a.unit)
    report = {"schema_version": 1, "tool": "cv_probe", "unit": unit,
              "status": "UNRESOLVED", "reasons": [], "axes": a.axes,
              "baseline": {}, "variants": [], "controls": [],
              "coverage": {"exhaustive": False, "scope": "Fixed full-TU source and finite listed command lines only; not a source-reachability proof."},
              "raw_boundary": "Actual Ninja compile edge, including extab cleanup when configured; not necessarily pristine compiler bytes.",
              "score_semantics": "matchtool diagnostic scores normalize relocation names and cannot certify relocated datum identity or whole-object matching."}

    def finish(status, reason=None):
        report["status"] = status
        if reason:
            report["reasons"].append(reason)
        print(f"{status}: {reason or 'requested finite experiment completed'}")
        if a.out:
            a.out.parent.mkdir(parents=True, exist_ok=True)
            a.out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
            print("wrote", a.out)
        return {"PASS": 0, "FAIL": 1, "UNRESOLVED": 2}[status]

    if a.axes not in AXES or a.j < 1:
        return finish("UNRESOLVED", f"invalid axes/jobs: axes must be one of {AXES}, jobs >= 1")
    if a.axes == "check" and (a.extra_flags or a.control_fn):
        return finish("UNRESOLVED", "--axes check is fidelity-only; custom trials/controls need a sweep axis")
    try:
        edges = read_edges()
    except (OSError, ValueError) as exc:
        return finish("UNRESOLVED", f"cannot read Ninja compile edges: {exc}")
    if unit not in edges:
        return finish("UNRESOLVED", f"no raw compile edge for {unit}")
    edge = edges[unit]
    report["edge"] = edge
    try:
        report["source_pragmas"] = pragma_inventory(edge["src"])
    except OSError as exc:
        return finish("UNRESOLVED", f"source pragma inventory unreadable: {exc}")
    target_o = REPO / "build" / VERSION / "obj" / f"{unit}.o"
    build_graph = REPO / "build.ninja"
    try:
        report["build_graph_sha256"] = sha(build_graph.read_bytes()) if build_graph.exists() else None
    except OSError as exc:
        return finish("UNRESOLVED", f"Ninja graph unreadable: {exc}")

    def compile_trial(label, mw, flags, output, td):
        trace = []
        traced_edge = dict(edge, _command_trace=trace)
        row = {"label": label, "compiler": mw, "ordered_cflags": flags,
               "commands": trace, "status": "UNRESOLVED"}
        compiler = REPO / "build/compilers" / mw / "mwcceppc.exe"
        try:
            row["compiler_sha256"] = sha(compiler.read_bytes()) if compiler.exists() else None
            row["ordered_flag_tokens"] = shlex.split(flags)
            got, err = compile_with(traced_edge, mw, flags, output, td)
            if err:
                row.update(status="FAIL" if err.startswith("FAIL") else "UNRESOLVED", reason=err)
                return row, None
            if got is None or not got.is_file():
                row["reason"] = "compiler returned no readable output"
                return row, None
            row.update(status="PASS", object_sha256=sha(got.read_bytes()), object_size=got.stat().st_size)
            return row, got
        except (OSError, ValueError, RuntimeError, SystemExit) as exc:
            row["reason"] = f"compile/measurement refusal: {exc}"
            return row, None

    with tempfile.TemporaryDirectory(prefix="cvprobe_") as td:
        # --- fidelity gate -------------------------------------------------
        base_out = Path(td) / "base.o"
        baseline, got = compile_trial("BASE", edge["mw"], edge["cflags"], base_out, td)
        report["baseline"] = baseline
        if got is None:
            return finish("UNRESOLVED", "baseline compile failed: " + baseline.get("reason", "unknown"))
        try:
            shipped = (REPO / edge["body_o"]).read_bytes()
        except OSError as exc:
            return finish("UNRESOLVED", f"raw Ninja object unreadable: {exc}")
        mine = got.read_bytes()
        baseline.update(reference_sha256=sha(shipped), reference_size=len(shipped), fidelity=mine == shipped)
        fid = "BYTE-IDENTICAL" if mine == shipped else \
              f"DIFFERS ({len(mine)} vs {len(shipped)} bytes)"
        print(f"unit={unit}  mw={edge['mw']}  rule={edge['rule']}")
        print(f"fidelity vs shipped body object: {fid}")
        if mine != shipped:
            return finish("UNRESOLVED", "baseline object DIFFERS; rebuild the actual raw Ninja output before sweeping")
        if a.axes == "check":
            return finish("PASS")

        if not target_o.is_file():
            return finish("UNRESOLVED", f"split target object missing: {target_o}")
        try:
            target_fns = matchtool.parse(target_o)
            base_fns = matchtool.parse(base_out)
        except (OSError, ValueError, RuntimeError, SystemExit) as exc:
            return finish("UNRESOLVED", f"object parsing refused: {exc}")
        if not target_fns or not base_fns:
            return finish("UNRESOLVED", "empty target or baseline function parse; no comparison made")
        base_scores = matchtool.score(target_fns, base_fns)
        fns = a.fn or [n for n, v in base_scores.items() if str(v) not in ("OK", "OK~")]
        unknown = sorted(set(fns + a.control_fn) - (target_fns.keys() & base_fns.keys()))
        if unknown:
            return finish("UNRESOLVED", "requested functions not present in both objects: " + ", ".join(unknown))
        if not fns:
            return finish("UNRESOLVED", "no unresolved functions selected; use --fn for an explicit calibration")
        baseline["scores"] = base_scores
        baseline["normalized_assembly_sha256"] = _assembly_hashes(base_fns)
        report["selected_functions"] = fns
        print(f"scoring {len(fns)} function(s): {', '.join(fns[:12])}")
        print("baseline: " + "  ".join(f"{n}={base_scores.get(n)}" for n in fns))

        jobs = variants(edge, a.axes)
        jobs.extend((f"CUSTOM{index}", edge["mw"], edge["cflags"] + " " + flags)
                    for index, flags in enumerate(a.extra_flags, 1))
        report["coverage"]["requested_variants"] = len(jobs)
        report["coverage"]["matrix"] = [{"label": label, "compiler": mw, "ordered_cflags": flags} for label, mw, flags in jobs]
        def one(item):
            index, v = item
            label, mw, cf = v
            if label == "BASE":
                return baseline
            o = Path(td) / f"variant_{index}.o"
            row, got = compile_trial(label, mw, cf, o, td)
            if got is not None:
                try:
                    parsed = matchtool.parse(got)
                    if not parsed:
                        raise ValueError("empty variant function parse")
                    row["scores"] = matchtool.score(target_fns, parsed)
                    row["normalized_assembly_sha256"] = _assembly_hashes(parsed)
                except (OSError, ValueError, RuntimeError, SystemExit) as exc:
                    row.update(status="UNRESOLVED", reason=f"variant parse refused: {exc}")
            return row

        with cf.ThreadPoolExecutor(max_workers=a.j) as ex:
            report["variants"] = list(ex.map(one, enumerate(jobs)))

        # ---- report -------------------------------------------------------
        strict = [n for n, v in base_scores.items() if str(v) in ("OK", "OK~")]
        print(f"\n{'variant':30s} {'target-fns':>10s}  {'lost-score-OK':>13s}   detail")
        rows = []
        hashes = {}
        for row in report["variants"]:
            label, r = row["label"], row.get("scores")
            if row["status"] != "PASS" or r is None:
                rows.append((10**9, label, row.get("reason", "unresolved"), 0))
                continue
            hashes.setdefault(row["object_sha256"], []).append(label)
            tot = matchtool.total_key({n: r.get(n, "MISS") for n in fns})
            broke = sum(1 for n in strict if str(r.get(n, "MISS")) not in ("OK", "OK~"))
            detail = " ".join(f"{n}={r.get(n)}" for n in fns)
            rows.append((tot, label, detail, broke))
            row.update(selected_score_total=tot, lost_baseline_score_ok=broke)
            if label != "BASE":
                missing_controls = [n for n in a.control_fn if n not in row["normalized_assembly_sha256"]]
                changed = [n for n in a.control_fn if n not in missing_controls and row["normalized_assembly_sha256"][n] != baseline["normalized_assembly_sha256"].get(n)]
                unchanged = [n for n in fns if row["normalized_assembly_sha256"].get(n) == baseline["normalized_assembly_sha256"].get(n)]
                report["controls"].append({"variant": label, "status": "PASS" if changed and not missing_controls else "UNRESOLVED",
                                           "missing_control_functions": missing_controls,
                                           "changed_control_functions": changed, "unchanged_selected_functions": unchanged,
                                           "meaning": "Changed sibling confirms option sensitivity somewhere in this TU, not its effective setting in an unchanged function. Pragma masking and source insensitivity remain distinct hypotheses."})
        for tot, label, detail, broke in sorted(rows):
            if a.quiet_ok and tot >= 10**9:
                continue
            flag = "SCORE-OK (not datum proof)" if tot == 0 else ""
            print(f"{label:30s} {str(tot):>10s}  {broke:>12d}   {detail[:110]} {flag}")
        report["duplicate_outputs"] = [{"object_sha256": digest, "variants": labels} for digest, labels in hashes.items() if len(labels) > 1]
        report["coverage"]["completed_variants"] = sum(row["status"] == "PASS" for row in report["variants"])
        failures = [row for row in report["variants"] if row["status"] != "PASS"]
        if failures:
            status = "FAIL" if any(row["status"] == "FAIL" for row in failures) else "UNRESOLVED"
            return finish(status, f"{len(failures)} requested variant(s) failed or remain unresolved; never count them as tested negative results")
        report["unchanged_output_interpretation"] = "UNRESOLVED unless a sensitive control and function-local context analysis distinguish masking from insensitivity; duplicate outputs do not exhaust an axis."
        if report["source_pragmas"]["status"] != "PASS":
            return finish("UNRESOLVED", "source pragma inventory unavailable")
    return finish("PASS")


if __name__ == "__main__":
    raise SystemExit(main())
