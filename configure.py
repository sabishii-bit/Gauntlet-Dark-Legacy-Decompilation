#!/usr/bin/env python3

###
# Generates build files for the project.
# This file also includes the project configuration,
# such as compiler flags and the object matching status.
#
# Usage:
#   python3 configure.py
#   ninja
#
# Append --help to see available options.
###

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Any, Dict, List

# Foreign-CWD guard: configure writes build.ninja into the CURRENT directory,
# and running THIS checkout's configure from another checkout's CWD (or vice
# versa) has twice broken a checkout's build graph when the referenced
# worktree was later deleted. Refuse the mismatch instead of writing.
if Path.cwd().resolve() != Path(__file__).resolve().parent:
    sys.exit(
        "configure.py: refusing to run — the working directory is\n"
        f"  {Path.cwd().resolve()}\n"
        "but this script belongs to\n"
        f"  {Path(__file__).resolve().parent}\n"
        "cd (Set-Location) into the checkout you intend to configure first."
    )

from tools.project import (
    Object,
    ProgressCategory,
    ProjectConfig,
    calculate_progress,
    generate_build,
    is_windows,
)

# Game versions
DEFAULT_VERSION = 0
VERSIONS = [
    "GUNE5D",  # 0
]

parser = argparse.ArgumentParser()
parser.add_argument(
    "mode",
    choices=["configure", "progress"],
    default="configure",
    help="script mode (default: configure)",
    nargs="?",
)
parser.add_argument(
    "-v",
    "--version",
    choices=VERSIONS,
    type=str.upper,
    default=VERSIONS[DEFAULT_VERSION],
    help="version to build",
)
parser.add_argument(
    "--build-dir",
    metavar="DIR",
    type=Path,
    default=Path("build"),
    help="base build directory (default: build)",
)
parser.add_argument(
    "--binutils",
    metavar="BINARY",
    type=Path,
    help="path to binutils (optional)",
)
parser.add_argument(
    "--compilers",
    metavar="DIR",
    type=Path,
    help="path to compilers (optional)",
)
parser.add_argument(
    "--map",
    action="store_true",
    help="generate map file(s)",
)
parser.add_argument(
    "--debug",
    action="store_true",
    help="build with debug info (non-matching)",
)
if not is_windows():
    parser.add_argument(
        "--wrapper",
        metavar="BINARY",
        type=Path,
        help="path to wibo or wine (optional)",
    )
parser.add_argument(
    "--dtk",
    metavar="BINARY | DIR",
    type=Path,
    help="path to decomp-toolkit binary or source (optional)",
)
parser.add_argument(
    "--objdiff",
    metavar="BINARY | DIR",
    type=Path,
    help="path to objdiff-cli binary or source (optional)",
)
parser.add_argument(
    "--sjiswrap",
    metavar="EXE",
    type=Path,
    help="path to sjiswrap.exe (optional)",
)
parser.add_argument(
    "--ninja",
    metavar="BINARY",
    type=Path,
    help="path to ninja binary (optional)",
)
parser.add_argument(
    "--verbose",
    action="store_true",
    help="print verbose output",
)
parser.add_argument(
    "--non-matching",
    dest="non_matching",
    action="store_true",
    help="builds non-matching or modded objects",
)
parser.add_argument(
    "--experimental-p6-compiler",
    action="store_true",
    help="use the reviewed GC/1.2.5s compiler for registry.c",
)
parser.add_argument(
    "--warn",
    dest="warn",
    type=str,
    choices=["all", "off", "error"],
    help="how to handle warnings",
)
parser.add_argument(
    "--no-progress",
    dest="progress",
    action="store_false",
    help="disable progress calculation",
)
args = parser.parse_args()

config = ProjectConfig()
config.version = str(args.version)
version_num = VERSIONS.index(config.version)

# Apply arguments
config.build_dir = args.build_dir
config.dtk_path = args.dtk
config.objdiff_path = args.objdiff
config.binutils_path = args.binutils
config.compilers_path = args.compilers
config.generate_map = args.map
config.non_matching = args.non_matching
config.sjiswrap_path = args.sjiswrap
config.ninja_path = args.ninja
config.progress = args.progress
if not is_windows():
    config.wrapper = args.wrapper
# Don't build asm unless we're --non-matching
if not config.non_matching:
    config.asm_dir = None

p6_compiler_version = "GC/1.2.5n"
if args.experimental_p6_compiler:
    compiler_root = args.compilers or args.build_dir / "compilers"
    p6_compiler = compiler_root / "GC" / "1.2.5s" / "mwcceppc.exe"
    expected_p6_sha256 = (
        "5a4d1e1715954ddefc87a5a0dfbe38b6c3916e22214957b21af3bd147a760667"
    )
    if not p6_compiler.is_file():
        sys.exit(
            f"Experimental compiler not found: {p6_compiler}\n"
            "See tools/gdl/mwcc_p6/README.md to derive it from GC/1.2.5n."
        )
    actual_p6_sha256 = hashlib.sha256(p6_compiler.read_bytes()).hexdigest()
    if actual_p6_sha256 != expected_p6_sha256:
        sys.exit(
            f"Experimental compiler hash mismatch: {actual_p6_sha256}\n"
            f"Expected: {expected_p6_sha256}"
        )
    p6_compiler_version = "GC/1.2.5s"

# Tool versions
config.binutils_tag = "2.42-2"
config.compilers_tag = "20251118"
config.dtk_tag = "v1.8.3"
config.objdiff_tag = "v3.6.1"
config.sjiswrap_tag = "v1.2.2"
config.wibo_tag = "1.0.3"

# Project
config.config_path = Path("config") / config.version / "config.yml"
config.check_sha_path = Path("config") / config.version / "build.sha1"
config.asflags = [
    "-mgekko",
    "--strip-local-absolute",
    "-I include",
    f"-I build/{config.version}/include",
    f"--defsym BUILD_VERSION={version_num}",
]
config.ldflags = [
    "-fp hardware",
    "-nodefaults",
]
if args.debug:
    config.ldflags.append("-g")  # Or -gdwarf-2 for Wii linkers
if args.map:
    config.ldflags.append("-mapunused")
    # config.ldflags.append("-listclosure") # For Wii linkers

# Use for any additional files that should cause a re-configure when modified
config.reconfig_deps = []

# Optional numeric ID for decomp.me preset
# Can be overridden in libraries or objects
config.scratch_preset_id = None

# Base flags, common to most GC/Wii games.
# Generally leave untouched, with overrides added below.
cflags_base = [
    "-nodefaults",
    "-proc gekko",
    "-align powerpc",
    "-enum int",
    "-fp hardware",
    "-Cpp_exceptions off",
    "-O4,p",
    "-inline auto",
    '-pragma "cats off"',
    '-pragma "warn_notinlined off"',
    "-maxerrors 1",
    "-nosyspath",
    "-RTTI off",
    "-fp_contract on",
    "-str reuse",
    "-multibyte",  # For Wii compilers, replace with `-enc SJIS`
    "-i include",
    f"-i build/{config.version}/include",
    f"-DBUILD_VERSION={version_num}",
    f"-DVERSION_{config.version}",
]

# Debug flags
if args.debug:
    # Or -sym dwarf-2 for Wii compilers
    cflags_base.extend(["-sym on", "-DDEBUG=1"])
else:
    cflags_base.append("-DNDEBUG=1")

# Warning flags
if args.warn == "all":
    cflags_base.append("-W all")
elif args.warn == "off":
    cflags_base.append("-W off")
elif args.warn == "error":
    cflags_base.append("-W error")

# Metrowerks library flags
# Note: no -gccinc; the GC/1.x compilers used for the runtime don't support it.
cflags_runtime = [
    *cflags_base,
    "-use_lmw_stmw on",
    "-str reuse,pool,readonly",
    "-common off",
    "-inline auto",
]

# Dolphin demo library (DEMOInit.c): base flags but without the ,p
# suffix. MEASURED 2026-09-01: ,p is optimize-for-SPEED, not peephole —
# removing it changed loop unrolling wholesale, and #pragma peephole on
# was a no-op because peephole is already on under plain -O4. (A
# trailing -O4 would not clear the ,p, hence the rewrite.) Also
# readonly strings (large strings -> .rodata, <=8-byte -> .sdata2) and
# C++ exceptions on (the DOL carries extab/extabindex entries for this
# TU's functions).
cflags_demo = [
    *[("-O4" if flag == "-O4,p" else flag) for flag in cflags_base],
    "-Cpp_exceptions on",
    "-str reuse,readonly",
]

# REL flags
cflags_rel = [
    *cflags_base,
    "-sdata 0",
    "-sdata2 0",
]

# MetroTRK debugger stub flags.
# Matches GC/1.1p1; note: no -inline option (default), which is required
# to keep intra-TU calls (e.g. TRKHandleRequestEvent) out-of-line, and
# -common on (TRK globals are common symbols; also changes codegen for
# accesses to globals defined in the same TU).
cflags_trk = [
    "-nodefaults",
    "-proc gekko",
    "-align powerpc",
    "-enum int",
    "-fp hardware",
    "-Cpp_exceptions off",
    "-O4,p",
    '-pragma "cats off"',
    '-pragma "warn_notinlined off"',
    "-maxerrors 1",
    "-nosyspath",
    "-RTTI off",
    "-fp_contract on",
    "-multibyte",
    "-i include",
    f"-i build/{config.version}/include",
    f"-DBUILD_VERSION={version_num}",
    f"-DVERSION_{config.version}",
    "-DNDEBUG=1",
    "-use_lmw_stmw on",
    "-rostr",
    "-str reuse",
    "-common on",
    "-char signed",
    "-pool off",
    "-sdata 0",
    "-sdata2 0",
]

config.linker_version = "GC/1.3.2"

# The SI and EXI libraries are built with an inline nesting depth of 1:
# e.g. SIGetResponse inlines SIGetResponseRaw but keeps the nested
# SIGetStatus as a call; EXIGetID keeps EXIClearInterrupts calls inside
# inlined __EXIAttach/EXIUnlock bodies.
cflags_inline1 = [
    flag if flag != "-inline auto" else "-inline auto,level=1" for flag in cflags_base
]


# Helper function for Dolphin libraries
def DolphinLib(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "GC/1.2.5n",
        "cflags": cflags_base,
        "progress_category": "sdk",
        "objects": objects,
    }


# Helper function for REL script objects
def Rel(lib_name: str, objects: List[Object]) -> Dict[str, Any]:
    return {
        "lib": lib_name,
        "mw_version": "GC/1.3.2",
        "cflags": cflags_rel,
        "progress_category": "game",
        "objects": objects,
    }


Matching = True      # Object matches and should be linked
NonMatching = False  # Object does not match and should not be linked


# Object is only matching for specific versions
def MatchingFor(*versions):
    return config.version in versions


config.warn_missing_config = True
config.warn_missing_source = False
config.libs = [
    {
        "lib": "Runtime.PPCEABI.H",
        "mw_version": "GC/1.2.5",
        "cflags": cflags_runtime,
        "progress_category": "sdk",  # str | List[str]
        "objects": [
            Object(Matching, "Runtime.PPCEABI.H/__mem.c"),
            Object(Matching, "Runtime.PPCEABI.H/__va_arg.c"),
            Object(Matching, "Runtime.PPCEABI.H/global_destructor_chain.c"),
            Object(Matching, "Runtime.PPCEABI.H/__init_cpp_exceptions.cpp"),
            Object(
                Matching,
                "Runtime.PPCEABI.H/NMWException.cpp",
                extra_cflags=["-Cpp_exceptions on", "-RTTI on", "-str reuse,nopool"],
            ),
            Object(
                Matching,
                "Runtime.PPCEABI.H/ExceptionPPC.cpp",
                extra_cflags=["-Cpp_exceptions on", "-RTTI on", "-str reuse,nopool"],
            ),
            Object(Matching, "Runtime.PPCEABI.H/runtime.c"),
            Object(Matching, "Runtime.PPCEABI.H/__ppc_eabi_init.c"),
        ],
    },
    DolphinLib(
        "vi",
        [
            Object(Matching, "dolphin/vi/vi.c"),
        ],
    ),
    DolphinLib(
        "ar",
        [
            Object(Matching, "dolphin/ar/ar.c"),
            Object(Matching, "dolphin/ar/arq.c"),
        ],
    ),
    {
        "lib": "game",
        "mw_version": "GC/1.2.5",
        "cflags": cflags_base,
        "progress_category": "game",
        "objects": [
            Object(NonMatching, "game/world/worldcol.c", cflags=cflags_demo),
            Object(Matching, "game/anim/anim.c", cflags=cflags_demo),
            Object(Matching, "game/anim/anim_play.c", cflags=cflags_demo),
            Object(NonMatching, "game/anim/atree.c", cflags=cflags_demo),
            Object(NonMatching, "game/g3d/auxanim.c", cflags=cflags_demo),
            Object(NonMatching, "game/anim/action.c", cflags=cflags_demo),
            Object(Matching, "game/sys/main.c", cflags=cflags_demo),
            Object(NonMatching, "game/sys/memcard.c", cflags=cflags_demo),
            Object(NonMatching, "game/sys/cardutil.c", cflags=cflags_demo),
            Object(NonMatching, "game/sys/sysservice.c", cflags=cflags_demo),
            Object(NonMatching, "game/mb/mb_poly.c", cflags=cflags_demo),
            Object(NonMatching, "game/mb/mb_particle.c", cflags=cflags_demo),
            Object(NonMatching, "game/mb/mb_camera.c", cflags=cflags_demo),
            Object(NonMatching, "game/mb/mb_font.c", cflags=cflags_demo),
            Object(Matching, "game/mb/mb_lights.c", cflags=cflags_demo),
            Object(Matching, "game/mb/mb_main.c", cflags=cflags_demo),
            Object(NonMatching, "game/mb/mb_model.c", cflags=cflags_demo),
            Object(Matching, "game/mb/mb_struct.c", cflags=cflags_demo),
            Object(Matching, "game/mb/mb_tree.c", cflags=cflags_demo),
            Object(Matching, "game/mb/mb_util.c", cflags=cflags_demo),
            Object(Matching, "game/mb/mb_window.c", cflags=cflags_demo),
            Object(Matching, "game/ps2/ml_error.c", cflags=cflags_demo),
            Object(Matching, "game/ps2/ml_ffsincos.c", cflags=cflags_demo),
            Object(NonMatching, "game/ps2/ml_fmath.c", cflags=cflags_demo),
            Object(Matching, "game/g3d/sndvoice.c", mw_version="GC/1.2.5n"),
            Object(Matching, "game/g3d/gpads.c", mw_version="GC/1.2.5n"),
            Object(Matching, "game/sys/registry.c", mw_version=p6_compiler_version),
            Object(Matching, "game/sys/gutil.c", mw_version="GC/1.2.5n"),
            Object(Matching, "game/sys/texPalette.c", mw_version="GC/1.2.5n"),
            Object(Matching, "game/g3d/gcontrolpads.c", cflags=cflags_demo, mw_version="GC/1.2.5n"),
            Object(Matching, "game/crt/vsprintf.c", cflags=cflags_demo),
            Object(Matching, "game/pb/pbutils.c", cflags=cflags_demo, mw_version="GC/1.2.5n"),
            Object(NonMatching, "game/pb/pb_objects.c", cflags=cflags_demo, mw_version="GC/1.2.5n"),
            Object(NonMatching, "game/pb/pb_objregs.c", cflags=cflags_demo),
            Object(NonMatching, "game/pb/pb_texture.c", cflags=cflags_demo, mw_version="GC/1.2.5n"),
            Object(Matching, "game/pb/pb_tree.cpp", cflags=cflags_demo, mw_version="GC/1.2.5n"),
            Object(NonMatching, "game/pb/dbgtext.c", cflags=cflags_demo),
            Object(Matching, "game/pb/pb_winglobals.c", cflags=cflags_demo),
            Object(NonMatching, "game/pb/pb_error.c", cflags=cflags_demo),
            Object(NonMatching, "game/pb/pb_frame.c", cflags=cflags_demo),
            Object(Matching, "game/pb/pb_global.c", cflags=cflags_demo),
            Object(Matching, "game/ps2/mathfunc.c", cflags=cflags_demo),
            Object(NonMatching, "game/audio/dcs.c", cflags=cflags_demo),
            Object(Matching, "game/audio/buffile.c", cflags=cflags_demo),
            Object(NonMatching, "game/audio/dcsdrv.c", cflags=cflags_demo),
            Object(Matching, "game/audio/mempool.c", cflags=cflags_demo),
            Object(NonMatching, "game/audio/adstream.c", cflags=cflags_demo),
            Object(Matching, "game/g3d/g3dpad.c", cflags=cflags_demo),
            Object(NonMatching, "game/movie/movieplayer.cpp", cflags=cflags_demo),
            Object(Matching, "game/pb/pb_window.c", cflags=cflags_demo),
            Object(Matching, "game/g3d/g3dMath3D.cpp", cflags=cflags_demo),
            Object(NonMatching, "game/ps2/fakelib.c", cflags=cflags_demo),
            Object(Matching, "game/shop/shopquery.c", cflags=cflags_demo),
            Object(NonMatching, "game/sfx/sfx.c", cflags=cflags_demo),
            Object(NonMatching, "game/shop/shop.c", cflags=cflags_demo),
            Object(NonMatching, "game/audio/sndfx.c", cflags=cflags_demo),
            Object(NonMatching, "game/sound/sounds_evt.c", cflags=cflags_demo),
            Object(NonMatching, "game/sound/sounds.c", cflags=cflags_demo),
            Object(NonMatching, "game/world/tower.c", cflags=cflags_demo),
            Object(NonMatching, "game/ui/attract.c", cflags=cflags_demo),
            Object(NonMatching, "game/ui/options.c", cflags=cflags_demo),
            Object(Matching, "game/ui/message.c", cflags=cflags_demo),
            Object(NonMatching, "game/ui/screensaver.c", cflags=cflags_demo),
            Object(NonMatching, "game/ui/auxscreen.c", cflags=cflags_demo),
            Object(NonMatching, "game/ui/select.c", cflags=cflags_demo),
            Object(NonMatching, "game/ui/btext.c", cflags=cflags_demo),
            Object(Matching, "game/world/btricol.c", cflags=cflags_demo),
            Object(NonMatching, "game/boss/boss.c", cflags=cflags_demo),
            Object(NonMatching, "game/world/gauntworld.c", cflags=cflags_demo),
            Object(NonMatching, "game/world/camera.c", cflags=cflags_demo),
            Object(NonMatching, "game/game/combat.c", cflags=cflags_demo),
            Object(NonMatching, "game/world/newcam.c", cflags=cflags_demo),
            Object(NonMatching, "game/world/world.c", cflags=cflags_demo),
            Object(NonMatching, "game/world/items.c", cflags=cflags_demo),
            Object(Matching, "game/world/dyngrid.c", cflags=cflags_demo),
            Object(NonMatching, "game/world/dynobjgrid.c", cflags=cflags_demo),
            Object(NonMatching, "game/boss/bosscam.c", cflags=cflags_demo),
            Object(NonMatching, "game/pb/pb_diag.c", cflags=cflags_demo),
            Object(NonMatching, "game/audio/audio.c", cflags=cflags_demo),
            Object(Matching, "game/audio/soundmgr.c", cflags=cflags_demo, mw_version="GC/1.2.5"),
            Object(Matching, "game/sys/ml_mem.c", cflags=cflags_demo),
            Object(NonMatching, "game/mb/mb_blit.c", cflags=cflags_demo),
            Object(NonMatching, "game/enemy/enemy.c", cflags=cflags_demo),
            Object(Matching, "game/mb/mb_objects.c", cflags=cflags_demo),
            Object(NonMatching, "game/game/gamemain.c", cflags=cflags_demo),
            Object(NonMatching, "game/game/controls.c", cflags=cflags_demo),
            Object(NonMatching, "game/enemy/critter.c", cflags=cflags_demo),
            Object(NonMatching, "game/game/player.c", cflags=cflags_demo),
            Object(NonMatching, "game/game/pmotion.c", cflags=cflags_demo),
            Object(NonMatching, "game/sfx/psfx.c", cflags=cflags_demo),
            Object(Matching, "game/sys/psx2.c", cflags=cflags_demo),
            Object(Matching, "game/sys/recorder.c", cflags=cflags_demo),
        ],
    },
    {
        "lib": "zlib",
        "mw_version": "GC/1.2.5",
        "cflags": cflags_demo,
        "progress_category": "game",
        "objects": [
            Object(Matching, "zlib/adler32.c"),
            Object(Matching, "zlib/uncompr.c"),
            Object(Matching, "zlib/zutil.c"),
            Object(Matching, "zlib/infutil.c"),
            Object(Matching, "zlib/inffast.c"),
            Object(Matching, "zlib/infcodes.c"),
            Object(Matching, "zlib/infblock.c"),
            Object(Matching, "zlib/inflate.c"),
            Object(Matching, "zlib/inftrees.c"),
        ],
    },
    {
        "lib": "demo",
        "mw_version": "GC/1.2.5n",
        "cflags": cflags_demo,
        "progress_category": "game",
        "objects": [
            Object(Matching, "dolphin/demo/DEMOInit.c"),
        ],
    },
    DolphinLib(
        "ai",
        [
            Object(Matching, "dolphin/ai/ai.c"),
        ],
    ),
    {
        "lib": "dsp",
        "mw_version": "GC/1.2.5n",
        "cflags": [*cflags_base, "-i src/dolphin/dsp"],
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "dolphin/dsp/dsp.c"),
            Object(Matching, "dolphin/dsp/dsp_debug.c"),
            Object(Matching, "dolphin/dsp/dsp_task.c"),
        ],
    },
    {
        "lib": "ax",
        "mw_version": "GC/1.2.5n",
        "cflags": [*cflags_base, "-i src/dolphin/ax"],
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "dolphin/ax/AX.c"),
            Object(Matching, "dolphin/ax/AXAlloc.c"),
            Object(Matching, "dolphin/ax/AXAux.c"),
            Object(Matching, "dolphin/ax/AXCL.c"),
            Object(Matching, "dolphin/ax/AXOut.c"),
            Object(Matching, "dolphin/ax/AXSPB.c"),
            Object(Matching, "dolphin/ax/AXVPB.c"),
            Object(Matching, "dolphin/ax/AXProf.c"),
        ],
    },
    DolphinLib(
        "base",
        [
            Object(Matching, "dolphin/base/PPCArch.c"),
        ],
    ),
    DolphinLib(
        "db",
        [
            Object(Matching, "dolphin/db/db.c"),
            Object(Matching, "dolphin/db/odenotstub.c", mw_version="GC/1.2.5"),
            Object(Matching, "dolphin/db/AmcExi2Stubs.c", mw_version="GC/1.2.5"),
            Object(Matching, "dolphin/db/odemustubs.c", mw_version="GC/1.2.5"),
        ],
    ),
    {
        "lib": "MSL_C",
        "mw_version": "GC/1.2.5",
        "cflags": [*cflags_runtime, "-i src/MSL"],
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "MSL/abort_exit.c"),
            Object(Matching, "MSL/ansi_fp.c"),
            Object(Matching, "MSL/bsearch.c"),
            Object(Matching, "MSL/buffer_io.c"),
            Object(Matching, "MSL/critical_regions.c"),
            Object(Matching, "MSL/direct_io.c"),
            Object(Matching, "MSL/mbstring.c"),
            Object(Matching, "MSL/wchar_io.c"),
            Object(Matching, "MSL/ctype.c"),
            Object(Matching, "MSL/w_fabs.c"),
            Object(Matching, "MSL/s_ldexp.c"),
            Object(Matching, "MSL/e_rem_pio2.c"),
            Object(Matching, "MSL/s_fpclassify.c"),
            Object(Matching, "MSL/k_cos.c"),
            Object(Matching, "MSL/k_rem_pio2.c"),
            Object(Matching, "MSL/k_sin.c"),
            Object(Matching, "MSL/k_tan.c"),
            Object(Matching, "MSL/s_atan.c"),
            Object(Matching, "MSL/s_copysign.c"),
            Object(Matching, "MSL/s_cos.c"),
            Object(Matching, "MSL/s_floor.c"),
            Object(Matching, "MSL/s_frexp.c"),
            Object(Matching, "MSL/s_scalbn.c"),
            Object(Matching, "MSL/s_modf.c"),
            Object(Matching, "MSL/s_sin.c"),
            Object(Matching, "MSL/s_tan.c"),
            Object(Matching, "MSL/atanf.c"),
            Object(Matching, "MSL/sincos.c"),
            Object(Matching, "MSL/trigf_data.c"),
            Object(Matching, "MSL/mem.c"),
            Object(Matching, "MSL/misc_io.c"),
            Object(Matching, "MSL/printf.c"),
            Object(Matching, "MSL/qsort.c"),
            Object(Matching, "MSL/signal.c"),
            Object(Matching, "MSL/strfind.c"),
            Object(Matching, "MSL/time.c"),
            Object(Matching, "MSL/uart_console_io.c"),
            Object(Matching, "MSL/get_time.c"),
            Object(Matching, "MSL/mem_funcs.c"),
            Object(Matching, "MSL/string.c"),
        ],
    },
    DolphinLib(
        "dvd",
        [
            Object(Matching, "dolphin/dvd/dvdlow.c"),
            Object(Matching, "dolphin/dvd/dvdfs.c"),
            Object(Matching, "dolphin/dvd/dvd.c"),
            Object(Matching, "dolphin/dvd/dvdqueue.c"),
            Object(Matching, "dolphin/dvd/dvderror.c"),
            Object(Matching, "dolphin/dvd/fstload.c"),
        ],
    ),
    {
        # cflags_base plus the gx source dir on the include path, so the gx
        # TUs can `#include "__gx.h"` (MWCC -nosyspath doesn't search the
        # source file's own directory).
        "lib": "gx",
        "mw_version": "GC/1.2.5n",
        "cflags": [*cflags_base, "-i src/dolphin/gx"],
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "dolphin/gx/GXInit.c"),
            Object(Matching, "dolphin/gx/GXFifo.c"),
            Object(Matching, "dolphin/gx/GXAttr.c"),
            Object(Matching, "dolphin/gx/GXMisc.c"),
            Object(Matching, "dolphin/gx/GXGeometry.c"),
            Object(Matching, "dolphin/gx/GXFrameBuf.c"),
            Object(Matching, "dolphin/gx/GXLighting.c"),
            Object(Matching, "dolphin/gx/GXTexture.c"),
            Object(Matching, "dolphin/gx/GXBump.c"),
            Object(Matching, "dolphin/gx/GXTev.c"),
            Object(Matching, "dolphin/gx/GXPixel.c"),
            Object(Matching, "dolphin/gx/GXStubs.c"),
            Object(Matching, "dolphin/gx/GXTransform.c"),
            Object(Matching, "dolphin/gx/GXPerf.c"),
        ],
    },
    DolphinLib(
        "mtx",
        [
            Object(Matching, "dolphin/mtx/mtx.c"),
            Object(Matching, "dolphin/mtx/mtxvec.c"),
            Object(Matching, "dolphin/mtx/mtx44.c"),
            Object(Matching, "dolphin/mtx/vec.c"),
        ],
    ),
    DolphinLib(
        "pad",
        [
            Object(Matching, "dolphin/pad/PADClamp.c"),
            Object(Matching, "dolphin/pad/Pad.c"),
        ],
    ),
    {
        "lib": "si",
        "mw_version": "GC/1.2.5n",
        "cflags": cflags_inline1,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "dolphin/si/SIBios.c"),
            Object(Matching, "dolphin/si/SISamplingRate.c"),
        ],
    },
    {
        "lib": "exi",
        "mw_version": "GC/1.2.5n",
        "cflags": cflags_inline1,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "dolphin/exi/EXIBios.c"),
            Object(Matching, "dolphin/exi/EXIUart.c"),
        ],
    },
    {
        "lib": "card",
        "mw_version": "GC/1.2.5n",
        "cflags": [*cflags_base, "-i src/dolphin/card"],
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "dolphin/card/CARDBios.c"),
            Object(Matching, "dolphin/card/CARDUnlock.c"),
            Object(Matching, "dolphin/card/CARDRdwr.c"),
            Object(Matching, "dolphin/card/CARDBlock.c"),
            Object(Matching, "dolphin/card/CARDDir.c"),
            Object(Matching, "dolphin/card/CARDCheck.c"),
            Object(Matching, "dolphin/card/CARDMount.c"),
            Object(Matching, "dolphin/card/CARDFormat.c"),
            Object(Matching, "dolphin/card/CARDOpen.c"),
            Object(Matching, "dolphin/card/CARDCreate.c"),
            Object(Matching, "dolphin/card/CARDRead.c"),
            Object(Matching, "dolphin/card/CARDWrite.c"),
            Object(Matching, "dolphin/card/CARDDelete.c"),
            Object(Matching, "dolphin/card/CARDStat.c"),
            Object(Matching, "dolphin/card/CARDRename.c"),
        ],
    },
    DolphinLib(
        "os",
        [
            Object(Matching, "dolphin/os/__start.c"),
            Object(Matching, "dolphin/os/OS.c"),
            Object(Matching, "dolphin/os/OSAlarm.c"),
            Object(Matching, "dolphin/os/OSAlloc.c"),
            Object(Matching, "dolphin/os/OSArena.c"),
            Object(Matching, "dolphin/os/OSAudioSystem.c"),
            Object(Matching, "dolphin/os/OSCache.c"),
            Object(Matching, "dolphin/os/OSContext.c"),
            Object(Matching, "dolphin/os/OSError.c"),
            Object(Matching, "dolphin/os/OSFont.c"),
            Object(Matching, "dolphin/os/OSInterrupt.c"),
            Object(Matching, "dolphin/os/OSLink.c"),
            Object(Matching, "dolphin/os/OSMemory.c"),
            Object(Matching, "dolphin/os/OSMutex.c"),
            Object(Matching, "dolphin/os/OSReboot.c"),
            Object(Matching, "dolphin/os/OSReset.c"),
            Object(Matching, "dolphin/os/OSResetSW.c"),
            Object(Matching, "dolphin/os/OSRtc.c"),
            Object(Matching, "dolphin/os/OSSync.c"),
            Object(Matching, "dolphin/os/OSThread.c"),
            Object(Matching, "dolphin/os/OSTime.c"),
        ],
    ),
    {
        "lib": "TRK_MINNOW_DOLPHIN",
        "mw_version": "GC/1.1p1",
        "cflags": cflags_trk,
        "progress_category": "sdk",
        "objects": [
            Object(Matching, "TRK_MINNOW_DOLPHIN/Portable/mem_TRK.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/ppc/Generic/exception.s"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/Portable/mainloop.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/Portable/nubevent.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/Portable/nubinit.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/Portable/mutex_TRK.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/Portable/msg.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/Portable/msgbuf.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/Portable/serpoll.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/Portable/dispatch.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/Portable/msghndlr.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/Portable/support.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/Portable/notify.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/Portable/main_TRK.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/ppc/Generic/flush_cache.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/ppc/Generic/targimpl.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/ppc/Generic/mpc_7xx_603e.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/Os/dolphin/targcont.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/Os/dolphin/dolphin_trk.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/Os/dolphin/dolphin_trk_glue.c"),
            Object(Matching, "TRK_MINNOW_DOLPHIN/Portable/usr_put.c"),
        ],
    },
]


# Optional callback to adjust link order. This can be used to add, remove, or reorder objects.
# This is called once per module, with the module ID and the current link order.
#
# For example, this adds "dummy.c" to the end of the DOL link order if configured with --non-matching.
# "dummy.c" *must* be configured as a Matching object in order to be linked.
def link_order_callback(module_id: int, objects: List[str]) -> List[str]:
    # Don't modify the link order for matching builds
    if not config.non_matching:
        return objects
    if module_id == 0:  # DOL
        return objects + ["dummy.c"]
    return objects


# Uncomment to enable the link order callback.
# config.link_order_callback = link_order_callback


# Optional extra categories for progress tracking
# Adjust as desired for your project
config.progress_categories = [
    ProgressCategory("game", "Game Code"),
    ProgressCategory("sdk", "SDK Code"),
]
config.progress_each_module = args.verbose

# Post-compile fixup for the C++ exception runtime TUs: reproduces the
# original CodeWarrior link-time weak-function dead-stripping that mwld's
# CLI cannot perform (see tools/fix_exception_objects.py).
exc_nmw_obj = f"build/{config.version}/src/Runtime.PPCEABI.H/NMWException.o"
exc_ppc_obj = f"build/{config.version}/src/Runtime.PPCEABI.H/ExceptionPPC.o"
exc_stamp = f"build/{config.version}/src/Runtime.PPCEABI.H/exception_fixup.stamp"
config.custom_build_rules = [
    {
        "name": "frank",
        "command": "$python tools/gdl/frank.py $in $out --verbose",
        "description": "FRANK $out",
    },
    {
        "name": "webfrank",
        "command": "$python tools/gdl/webfrank.py $in $out $webfrank_config $webfrank_unit --target $webfrank_target",
        "description": "WEBFRANK $out",
    },
    {
        "name": "p6frank",
        "command": "$python tools/gdl/p6frank.py $in $out $p6frank_config $p6frank_unit --target $p6frank_target",
        "description": "P6FRANK $out",
    },
    {
        "name": "fix_exception_objects",
        "command": f"$python tools/fix_exception_objects.py {exc_nmw_obj} {exc_ppc_obj} $out",
        "description": "FIXUP $out",
    },
    {
        "name": "retail_dol",
        "command": "$python tools/gdl/retaildol.py $in $out",
        "description": "RETAILDOL $out",
    },
]
webfrank_config = Path(f"config/{config.version}/webfrank.json")
webfrank_units = json.loads(webfrank_config.read_text(encoding="utf-8"))["units"]
config.object_postprocesses = {}

# Exact-match postprocessors intentionally depend on extracted retail objects
# and fixed input/postimage hashes.  A modded/non-matching build must always
# compile and link the editable source without those target-dependent rewrites.
if not config.non_matching:
    config.object_postprocesses = {
        unit: {
            "rule": "webfrank",
            "implicit": [
                "tools/gdl/webfrank.py",
                str(webfrank_config),
                f"build/{config.version}/obj/{unit}.o",
            ],
            "variables": {
                "webfrank_config": str(webfrank_config),
                "webfrank_unit": unit,
                "webfrank_target": f"build/{config.version}/obj/{unit}.o",
            },
        }
        for unit in webfrank_units
    }
p6frank_config = Path(f"config/{config.version}/p6frank.json")
p6frank_units = json.loads(p6frank_config.read_text(encoding="utf-8"))["units"]
if not config.non_matching:
    for unit in p6frank_units:
        if args.experimental_p6_compiler and unit == "game/sys/registry":
            continue
        if unit in config.object_postprocesses:
            raise ValueError(f"multiple object postprocessors configured for {unit}")
        config.object_postprocesses[unit] = {
            "rule": "p6frank",
            "implicit": [
                "tools/gdl/p6frank.py",
                str(p6frank_config),
                f"build/{config.version}/obj/{unit}.o",
            ],
            "variables": {
                "p6frank_config": str(p6frank_config),
                "p6frank_unit": unit,
                "p6frank_target": f"build/{config.version}/obj/{unit}.o",
            },
        }
config.custom_build_steps = {
    "post-compile": [
        {
            "rule": "fix_exception_objects",
            "inputs": [exc_nmw_obj, exc_ppc_obj],
            "implicit": ["tools/fix_exception_objects.py"],
            "outputs": [exc_stamp],
        },
    ],
}

# Post-build: splice the retail DOL's unreproducible extab padding bytes from
# the user's own original DOL into a copy of the verified cleaned-target
# output, producing a byte-perfect retail artifact (see tools/gdl/retaildol.py
# for the fail-closed guards). Skipped for mod builds and when the original
# DOL is not present.
retail_orig_dol = Path(f"orig/{config.version}/sys/main.dol")
if not config.non_matching and retail_orig_dol.exists():
    config.custom_build_steps["post-build"] = [
        {
            "rule": "retail_dol",
            "inputs": [f"build/{config.version}/main.dol"],
            "implicit": ["tools/gdl/retaildol.py", str(retail_orig_dol)],
            "outputs": [f"build/{config.version}/main.retail.dol"],
        },
    ]
# Optional extra arguments to `objdiff-cli report generate`
config.progress_report_args = [
    # Marks relocations as mismatching if the target value is different
    # Default is "functionRelocDiffs=none", which is most lenient
    # "--config functionRelocDiffs=data_value",
]

if args.mode == "configure":
    # Write build.ninja and objdiff.json
    generate_build(config)
elif args.mode == "progress":
    # Print progress information
    calculate_progress(config)
else:
    sys.exit("Unknown mode: " + args.mode)
