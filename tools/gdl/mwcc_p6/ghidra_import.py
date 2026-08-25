# Ghidra script: import recovered MWCC GC/1.2.5 annotations.
#@category GDL.MWCC
#@menupath Tools.GDL.Import MWCC GC 1.2.5 annotations

"""Apply SHA-locked GC/1.2.5 annotations without distributing compiler bytes.

The addresses come from live mwcc-debugger captures and static analysis of the
exact retail GC/1.2.5 executable.  GC/1.2.5n has the same image layout, so one
annotation profile covers both explicitly allowed hashes.

This file is deliberately compatible with Ghidra's Jython runtime as well as
Python 3 syntax checking.  It must be run inside Ghidra.
"""

import hashlib
import os
import re

from ghidra.program.model.data import ArrayDataType
from ghidra.program.model.data import ByteDataType
from ghidra.program.model.data import CategoryPath
from ghidra.program.model.data import DataTypeConflictHandler
from ghidra.program.model.data import DWordDataType
from ghidra.program.model.data import PointerDataType
from ghidra.program.model.data import StructureDataType
from ghidra.program.model.data import WordDataType
from ghidra.program.model.data import VoidDataType
from ghidra.program.model.listing import CodeUnit
from ghidra.program.model.symbol import SourceType


PROFILES = {
    "0443b5c02b1aa7b575b61e0e24c4d5ad6bed8fd54cc42de5a2204a5216001914": {
        "name": "GC/1.2.5 retail build 179",
        "ninji": False,
    },
    "ccf4b465cec73b5aae9c5c5543dcf8cda8a62aba246f89e2e0b200d742f2e55c": {
        "name": "GC/1.2.5n (Ninji epilogue patch)",
        "ninji": True,
    },
}


# Confirmed names are tied to in-binary trace strings or independently observed
# control flow.  Inferred names state the exact observed role but are not claimed
# as recovered Metrowerks source identifiers.
FUNCTIONS = [
    (0x0042CD10, "MWCC_IRO_Optimizer", "confirmed frontend optimizer dispatcher"),
    (0x0042C9D0, "MWCC_IRO_ExpressionPropagation", "confirmed IRO pass"),
    (0x004351C0, "MWCC_CodeGen_Generator", "confirmed backend coordinator"),
    (0x00437230, "MWCC_CodeGen_PreallocateObjectRegisters", "confirmed object-register preallocation and half-open class coalescing-window setup pass"),
    (0x00449E30, "MWCC_IRO_BuildflowGraph", "confirmed IRO pass"),
    (0x0044AB00, "MWCC_IRO_ScalarizeClassDataMembers", "confirmed IRO pass"),
    (0x0044ADE0, "MWCC_RewriteBitFieldTemps", "confirmed IRO pass"),
    (0x0044DF00, "MWCC_IRO_CommonSubs", "confirmed IRO pass"),
    (0x00455930, "MWCC_IRO_EvaluateConditionals", "confirmed IRO pass"),
    (0x00455A70, "MWCC_IRO_ConstantFolding", "confirmed IRO pass"),
    (0x00456620, "MWCC_IRO_RemoveLabels", "confirmed IRO pass"),
    (0x00456670, "MWCC_IRO_RemoveRedundantJumps", "confirmed IRO pass"),
    (0x00456860, "MWCC_IRO_RemoveUnreachable", "confirmed IRO pass"),
    (0x00456A60, "MWCC_IRO_DoJumpChaining", "confirmed IRO pass"),
    (0x00456BA0, "MWCC_IRO_RangePropagateInFNode", "confirmed IRO pass"),
    (0x00458970, "MWCC_IRO_CopyAndConstantPropagation", "confirmed IRO pass"),
    (0x00459B30, "MWCC_IRO_UseDef", "confirmed IRO pass"),
    (0x0045FA80, "MWCC_IRO_LoopUnroller", "confirmed IRO pass wrapper"),
    (0x00461040, "MWCC_IRO_FindLoops", "confirmed by self-naming trace"),
    (0x0049CF70, "MWCC_Operands_PropagateFlags", "inferred exact flag-propagation helper"),
    (0x0049D0B0, "MWCC_PCode_RemoveUnreachable", "inferred initial PCode cleanup"),
    (0x0049D0F0, "MWCC_PCode_BuildPredecessors", "inferred predecessor construction"),
    (0x0049D1B0, "MWCC_PCode_NewBasicBlock", "live-observed basic-block constructor"),
    (0x0049D240, "MWCC_PCode_NewLabel", "live-observed label constructor"),
    (0x0049D270, "MWCC_PCode_CloneInstruction", "live-observed PCode clone path"),
    (0x004A25D0, "MWCC_PCodeUtilities_EmitInstruction", "inferred variadic PCode emitter"),
    (0x004ABA30, "MWCC_StackFrameEABI_MergePrologueEpilogue", "inferred exact merge role"),
    (0x004ABE90, "MWCC_StackFrameEABI_GeneratePrologueEpilogue", "inferred exact generation role"),
    (0x004C1720, "MWCC_Registers_GetInfo", "confirmed object-to-register-info lookup and allocation helper"),
    (0x004C17C0, "MWCC_Registers_CloseCoalesceWindow", "confirmed class coalescing-window close helper"),
    (0x004C1850, "MWCC_Registers_UpdateCoalesceWindow", "confirmed class coalescing-window update helper"),
    (0x004C1900, "MWCC_Registers_CheckpointCoalesceWindow", "confirmed class coalescing-window checkpoint helper"),
    (0x004C1980, "MWCC_Registers_BeginCoalesceWindow", "confirmed class coalescing-window begin helper"),
    (0x004C19F0, "MWCC_Coloring_ClaimHighestSavedVR", "confirmed class-2 saved-color allocator; scans physical colors 31 down through 20 and reserves the first free color"),
    (0x004C1A20, "MWCC_Coloring_ClaimHighestSavedFPR", "confirmed class-1 saved-color allocator; scans physical colors 31 down through 14 and reserves the first free color"),
    (0x004C1A50, "MWCC_Coloring_ClaimHighestSavedGPR", "confirmed class-0 saved-color allocator; scans physical colors 31 down through 14 and reserves the first free color"),
    (0x004C2560, "MWCC_CMangler_GetLinkName", "live-validated cached link-name resolver"),
    (0x004C4430, "MWCC_COptimizer_Optimize", "inferred optimization-level dispatcher"),
    (0x004C4530, "MWCC_COptimizer_Level4", "confirmed pass order and entry"),
    (0x004C4910, "MWCC_COptimizer_Level3", "confirmed pass order and entry"),
    (0x004CCA50, "MWCC_Scheduler_DependenceTestStub", "confirmed dead scheduler-model predicate; every path returns zero"),
    (0x004CCAE0, "MWCC_Scheduler_Schedule", "confirmed scheduler driver and machine-model selector"),
    (0x004CCBF0, "MWCC_Scheduler_ScheduleBlock", "confirmed cycle-driven per-block list scheduler"),
    (0x004CCDC0, "MWCC_Scheduler_PickInstruction", "confirmed urgency/release-count/height/descriptor/text-order selector"),
    (0x004CCF10, "MWCC_Scheduler_BuildDependencies", "confirmed backward dependency-graph construction"),
    (0x004CD2F0, "MWCC_Scheduler_AddWildcardMemoryDeps", "confirmed conservative object-less memory dependency pass"),
    (0x004CD4A0, "MWCC_Scheduler_AddObjectMemoryDeps", "confirmed object-keyed load/store dependency pass"),
    (0x004CD650, "MWCC_Scheduler_AddVolatileMemoryDeps", "inferred exact volatile-memory dependency role"),
    (0x004CD7C0, "MWCC_Scheduler_AddRegisterDeps", "confirmed per-register RAW/WAR/WAW dependency pass"),
    (0x004CD910, "MWCC_Scheduler_AddEdge", "confirmed edge insertion and critical-path height propagation"),
    (0x004CD9D0, "MWCC_Scheduler_ResetBlockState", "confirmed per-block dependency-table reset"),
    (0x004CDEF0, "MWCC_Coloring_AllocateRegisters", "inferred exact coloring coordinator"),
    (0x004CE1A0, "MWCC_Coloring_CommitAssignments", "confirmed PCode operand rewrite and object-color commit pass"),
    (0x004CE2D0, "MWCC_Coloring_SelectColors", "confirmed simplify-stack pop and lowest-set-bit free-color selection pass; corpus vetoes a global preference reversal"),
    (0x004CE400, "MWCC_Coloring_SimplifyGraph", "confirmed ascending-vreg low-degree scan, LIFO simplify stack, and spill-candidate ranking pass"),
    (0x004CE5F0, "MWCC_Coloring_SetupVRs", "confirmed vector-register interference-node setup"),
    (0x004CE710, "MWCC_Coloring_SetupFPRs", "confirmed floating-point interference-node setup"),
    (0x004CE850, "MWCC_Coloring_SetupGPRs", "confirmed general-purpose interference-node setup"),
    (0x0052DBC0, "MWCC_SchedulerModelDefault_Serializes", "confirmed default-model barrier predicate"),
    (0x0052DBE0, "MWCC_SchedulerModelDefault_Advance", "confirmed default-model pipeline and retirement advance"),
    (0x0052DF20, "MWCC_SchedulerModelDefault_OnIssue", "confirmed default-model instruction issue hook"),
    (0x0052DFA0, "MWCC_SchedulerModelDefault_CanIssue", "confirmed default-model structural-hazard predicate"),
    (0x0052E000, "MWCC_SchedulerModelDefault_Reset", "confirmed default-model per-block reset"),
    (0x0052E0B0, "MWCC_SchedulerModelDefault_Latency", "confirmed default-model latency calculation"),
    (0x00530050, "MWCC_SpillCode_IsDeadInstruction", "confirmed class-sensitive dead-definition predicate"),
    (0x005301B0, "MWCC_SpillCode_InitializeLiveness", "confirmed liveness allocation, local-set, return-seed, and solve driver"),
    (0x00530410, "MWCC_SpillCode_SolveLiveness", "confirmed backward fixed-point live-in/live-out solver"),
    (0x00530530, "MWCC_SpillCode_BuildLocalLiveness", "confirmed upward-exposed use and local definition builder"),
    (0x00530A00, "MWCC_SpillCode_BuildInterference", "confirmed six-stage interference/coalescing pipeline driver"),
    (0x00530A80, "MWCC_SpillCode_MarkLastUses", "confirmed backward last-use marker pass"),
    (0x00530C00, "MWCC_SpillCode_MaterializeGraph", "confirmed triangular-matrix to interference-node conversion"),
    (0x00530E00, "MWCC_SpillCode_CoalesceCopies", "confirmed class-copy coalescing, lower-vreg canonical roots, half-open window checks, and root operand rewrite"),
    (0x00531290, "MWCC_SpillCode_ConstructInterference", "confirmed live-set interference-matrix construction"),
    (0x00532790, "MWCC_SpillCode_ComputeSpillCosts", "confirmed weighted use/definition spill-cost accumulation"),
]


GLOBALS = [
    (0x00560CB4, "MWCC_gNodeNames", None, "75-entry AST node-name table"),
    (0x005654B0, "MWCC_gPCodeOpcodeDescriptors", None, "468 descriptors, 0x10 bytes each"),
    (0x00574D70, "MWCC_gSchedulerModelDefault", "scheduler_model", "default issue-width-two scheduler machine model selected by -proc gekko"),
    (0x00574D90, "MWCC_gSchedulerTimingDefault", "scheduler_opcode_table", "468 six-byte default-model opcode timing records"),
    (0x0057F6C0, "MWCC_gTemporaryObjects", "pointer", "debugger-validated temporary-object list"),
    (0x0058712C, "MWCC_gFrameCallArgsSize", "u32", "secondary/outgoing-call frame cursor"),
    (0x00587130, "MWCC_gCurrentCodeGenItem", "pointer", "current lowering item/statement"),
    (0x00587C74, "MWCC_gPCodeBlocks", "pointer", "head of physical PCode basic-block list"),
    (0x00587E3C, "MWCC_gInterferenceGraph", "pointer", "interference graph root"),
    (0x005876A0, "MWCC_gTrailingObjectList", "pointer", "register-object list processed after local objects"),
    (0x005882AC, "MWCC_gInitialObjectList", "pointer", "initial register-object list"),
    (0x00587FB8, "MWCC_gLocalObjectList", "pointer", "local register-object list"),
    (0x0058806C, "MWCC_gPostInitialObjectList", "pointer", "post-initial register-object list"),
    (0x005880C4, "MWCC_gCurrentPCodeBlock", "pointer", "live-observed current physical block"),
    (0x005880CC, "MWCC_gFrameBaseSize", "u32", "linkage/base frame size"),
    (0x00581B7C, "MWCC_gSchedulerTerminatorNode", "pointer", "current scheduling graph terminator node"),
    (0x00581B80, "MWCC_gSchedulerModel", "pointer", "selected SchedulerMachineModel pointer"),
    (0x00581B84, "MWCC_gSchedulerMaxHeight", "u16", "running maximum critical-path height; not reset between blocks"),
    (0x0058846C, "MWCC_gUsedVirtualRegistersFPR", "u16", "FPR virtual-register counter"),
    (0x0058846E, "MWCC_gUsedVirtualRegistersGPR", "u16", "GPR virtual-register counter"),
    (0x0058308C, "MWCC_gCoalescedRegisters", "pointer", "class-local 16-bit coalescing-parent map"),
    (0x005882DA, "MWCC_gGPRCoalesceFirst", "u16", "inclusive GPR coalescing-window start"),
    (0x005882E2, "MWCC_gGPRCoalesceLast", "u16", "GPR coalescing-window end snapshot; effective monotonic range is half-open"),
    (0x005882DC, "MWCC_gFPRCoalesceFirst", "u16", "inclusive FPR coalescing-window start"),
    (0x005882E0, "MWCC_gFPRCoalesceLast", "u16", "FPR coalescing-window end snapshot; effective monotonic range is half-open"),
    (0x00588464, "MWCC_gVRCoalesceFirst", "u16", "inclusive vector coalescing-window start"),
    (0x0058846A, "MWCC_gVRCoalesceLast", "u16", "vector coalescing-window end snapshot; effective monotonic range is half-open"),
    (0x0058842C, "MWCC_gInitialObjectVRLast", "u16", "initial-object vector-register end snapshot"),
    (0x0058845A, "MWCC_gInitialObjectGPRLast", "u16", "initial-object GPR end snapshot"),
    (0x0058845C, "MWCC_gInitialObjectFPRLast", "u16", "initial-object FPR end snapshot"),
    (0x0058849A, "MWCC_gUsedVirtualRegistersVR", "u16", "vector virtual-register counter"),
    (0x00584224, "MWCC_gProcessorModel", "u8", "processor model byte; -proc gekko stores 8"),
    (0x00584230, "MWCC_gSchedulerModelOverride", "u8", "nonzero forces the CPU-7 scheduler model path"),
    (0x00587648, "MWCC_gVirtualRegistersActive", "u32", "nonzero enables virtual-register-sized scheduler tables and descriptor tie-break"),
]


AST_CAPTURE_POINTS = [
    (0x0043539D, "initial_code"),
    (0x004353C4, "after_optimizations"),
    (0x004353C9, "final_code"),
]


PCODE_CAPTURE_POINTS = [
    (0x00435AFF, "initial_code"),
    # -O2 optimizer
    (0x004C4486, "O2_after_common_subexpression_elimination"),
    (0x004C44B9, "O2_after_copy_propagation"),
    (0x004C44E9, "O2_after_add_propagation"),
    # -O3 optimizer
    (0x004C4926, "O3_after_common_subexpression_elimination"),
    (0x004C4959, "O3_after_copy_propagation_1"),
    (0x004C4993, "O3_after_add_propagation_1"),
    (0x004C49F6, "O3_after_loop_code_motion"),
    (0x004C4A26, "O3_after_loop_strength_reduction"),
    (0x004C4A2D, "O3_after_copy_propagation_2"),
    (0x004C4A5E, "O3_after_loop_transforms"),
    (0x004C4A65, "O3_after_copy_propagation_3"),
    (0x004C4A6B, "O3_after_add_propagation_2"),
    (0x004C4AA7, "O3_after_copy_propagation_4"),
    (0x004C4ADB, "O3_after_constant_propagation"),
    (0x004C4B0B, "O3_after_load_deletion"),
    (0x004C4B3B, "O3_after_add_propagation_3"),
    (0x004C4B6E, "O3_after_common_subexpression_elimination_2"),
    (0x004C4B75, "O3_after_copy_propagation_5"),
    # -O4 optimizer
    (0x004C4546, "O4_after_common_subexpression_elimination_1"),
    (0x004C4579, "O4_after_copy_propagation_1"),
    (0x004C45B3, "O4_after_add_propagation_1"),
    (0x004C4616, "O4_after_loop_code_motion"),
    (0x004C4646, "O4_after_loop_strength_reduction"),
    (0x004C464D, "O4_after_copy_propagation_2"),
    (0x004C467E, "O4_after_loop_transforms"),
    (0x004C4685, "O4_after_copy_propagation_3"),
    (0x004C468B, "O4_after_add_propagation_2"),
    (0x004C46C7, "O4_after_copy_propagation_4"),
    (0x004C46FB, "O4_after_constant_propagation_1"),
    (0x004C472B, "O4_after_load_deletion"),
    (0x004C475D, "O4_after_copy_propagation_5"),
    (0x004C476C, "O4_after_add_propagation_3"),
    (0x004C47A5, "O4_after_array_register_transforms"),
    (0x004C47D5, "O4_after_constant_propagation_2"),
    (0x004C47DC, "O4_after_copy_propagation_6"),
    (0x004C480D, "O4_after_common_subexpression_elimination_2"),
    (0x004C4817, "O4_after_copy_propagation_7"),
    (0x004C48AB, "O4_after_code_motion_2"),
    (0x004C48DE, "O4_after_common_subexpression_elimination_3"),
    (0x004C48E5, "O4_after_copy_propagation_8"),
    # Shared CodeGen_Generator tail
    (0x00435B7E, "shared_after_scheduling_1"),
    (0x00435BDA, "shared_after_peephole_forward"),
    (0x00435BFE, "shared_before_regalloc"),
    (0x00435C03, "shared_after_regalloc"),
    (0x00435CB8, "shared_after_prologue_epilogue_generation"),
    (0x00435D20, "shared_after_peephole"),
    (0x00435D75, "shared_after_scheduling_2"),
]


SPECIAL_SITES = [
    (
        0x00435AFA,
        "MWCC_P6_PredecessorHookBoundary",
        "P6 carrier intervention boundary: immediately before predecessor "
        "construction. gPCodeBlocks is complete; predecessor lists are not.",
    ),
    (
        0x00435AFF,
        "MWCC_P6_InitialPCodeBoundary",
        "Return from predecessor construction and exact initial-PCode capture point.",
    ),
    (
        0x00456670,
        "MWCC_RRJ_FunctionEntry",
        "IRO_RemoveRedundantJumps; the branch-around-goto subcase is inside this function.",
    ),
    (
        0x00456706,
        "MWCC_RRJ_BranchAroundGotoBegin",
        "Start of the branch-around-goto subcase observed through 0x00456789.",
    ),
    (
        0x0045671E,
        "MWCC_RRJ_BranchAroundGotoDecision",
        "Conditional gate for the branch-around-goto subcase; disabling it did not change regFind PCode layout.",
    ),
    (
        0x004ABD9A,
        "MWCC_125n_EpilogueHookSite",
        "GC/1.2.5n redirects here to its cave; stock continues the EABI merge path.",
    ),
    (
        0x004ABDB3,
        "MWCC_125n_EpilogueReturnSite",
        "Second small GC/1.2.5n edit in the EABI merge path.",
    ),
    (
        0x0049CF70,
        "MWCC_125n_FlagHelper",
        "Existing helper called by 1.2.5n with 0x400 (PCodeInstruction_CoalesceDisabled).",
    ),
    (
        0x004CE3F4,
        "MWCC_RegallocCaptureBoundary",
        "Exact live debugger boundary for register-allocation capture.",
    ),
    (
        0x004CE381,
        "MWCC_Coloring_LowestFreeChoice",
        "Exact color-choice instruction. EAX is the available-color mask, "
        "EBX is the current InterferenceNode, and ECX is the lowest set-bit "
        "index selected as the physical color. CritterResolveMultipleTargets "
        "observed mask 0x1ff0 and stock r4 here while retail needs r6; the "
        "raw-exact CritterGetTarget control follows this same policy. This "
        "localizes that residual upstream to graph/simplify state.",
    ),
    (
        0x004CE3BF,
        "MWCC_Coloring_SavedColorChoice",
        "Saved-color assignment after the volatile available mask reaches zero. "
        "EAX is the class-specific highest free saved physical color and EBX is "
        "the current InterferenceNode. AllocMem32 observed r38->r31, r32->r30, "
        "and r33->r29 here; raw-exact AllocMem observed r32->r31 and r33->r30.",
    ),
    (
        0x004CE44C,
        "MWCC_Coloring_LowDegreeRemovalPath",
        "First low-degree simplify-removal path. EAX is the interference "
        "node and EDX is its effective degree before neighbor decrements. "
        "The getSinCos trace observed every noncoalesced FPR f33..f54 here "
        "in ascending order during scan pass one.",
    ),
    (
        0x005310D9,
        "MWCC_Coalescing_UnionCommit",
        "Coalescing parent-map commit. ESI is the higher child root, CX is "
        "the lower canonical parent, and EAX is the u16 parent map. "
        "getSinCos observed its only FPR union here as f32 -> fixed f1.",
    ),
    (
        0x004CCCCC,
        "MWCC_Scheduler_GraphReadyBoundary",
        "Per-block dependency nodes, heights, deadlines, and predecessor counts are complete; cycle-driven issue begins next.",
    ),
    (
        0x004CCE3F,
        "MWCC_Scheduler_UrgencyTieBreak",
        "Start of the confirmed strict-win urgency comparison in Scheduler_PickInstruction.",
    ),
    (
        0x004CCE61,
        "MWCC_Scheduler_ReleaseCountTieBreak",
        "Confirmed comparison of successors whose predecessor count is one; strictly more releases wins.",
    ),
    (
        0x004CCEA9,
        "MWCC_Scheduler_HeightTieBreak",
        "Confirmed critical-path-height comparison; strictly larger height wins.",
    ),
    (
        0x004CCEBB,
        "MWCC_Scheduler_DescriptorTieBreak",
        "Confirmed opcode-descriptor byte-9 comparison; strictly smaller value wins while virtual registers are active.",
    ),
    (
        0x004CCEF2,
        "MWCC_Scheduler_TextOrderTieBreak",
        "Final exact tie keeps the earlier-textual candidate. AllocFile reaches this tier; sysPollResetButton is decided earlier by release count.",
    ),
]


def sha256_file(path):
    digest = hashlib.sha256()
    stream = open(path, "rb")
    try:
        while True:
            chunk = stream.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    finally:
        stream.close()
    return digest.hexdigest().lower()


def identify_profile():
    """Fail closed unless the imported executable is one of the two profiles."""
    digest = None
    try:
        digest = currentProgram.getExecutableSHA256()
    except Exception:
        digest = None
    if digest:
        digest = str(digest).lower()

    args = list(getScriptArgs())
    supplied = args[0] if args else None
    if supplied:
        supplied = os.path.abspath(supplied)
        if not os.path.isfile(supplied):
            raise RuntimeError("compiler path does not exist: " + supplied)
        file_digest = sha256_file(supplied)
        if digest and file_digest != digest:
            raise RuntimeError(
                "supplied compiler hash does not match Ghidra program hash: "
                + file_digest
                + " != "
                + digest
            )
        digest = file_digest

    if not digest:
        executable_path = str(currentProgram.getExecutablePath() or "")
        if executable_path and os.path.isfile(executable_path):
            digest = sha256_file(executable_path)

    if digest not in PROFILES:
        raise RuntimeError(
            "unsupported or unavailable executable SHA-256: "
            + str(digest)
            + "; pass the original compiler path as the first script argument"
        )

    if currentProgram.getDefaultPointerSize() != 4:
        raise RuntimeError("expected a 32-bit i386 PE program")
    return digest, PROFILES[digest]


def checked_address(value):
    address = toAddr(value)
    if not currentProgram.getMemory().contains(address):
        raise RuntimeError("profile address is not mapped: 0x{0:08x}".format(value))
    return address


def set_comment(address, text):
    currentProgram.getListing().setComment(address, CodeUnit.PLATE_COMMENT, text)


def create_or_update_label(address, name, primary):
    symbols = currentProgram.getSymbolTable().getSymbols(address)
    while symbols.hasNext():
        symbol = symbols.next()
        if symbol.getName() == name:
            if primary:
                symbol.setPrimary()
            return symbol
    return createLabel(address, name, primary)


def annotate_function(value, name, evidence):
    address = checked_address(value)
    function = currentProgram.getFunctionManager().getFunctionAt(address)
    if function is None:
        disassemble(address)
        function = createFunction(address, name)
    if function is not None:
        function.setName(name, SourceType.USER_DEFINED)
    else:
        create_or_update_label(address, name, True)
    set_comment(address, "MWCC GC/1.2.5: " + evidence + ".")


def annotate_site(value, name, text, category):
    address = checked_address(value)
    create_or_update_label(address, name, False)
    set_comment(address, text)
    currentProgram.getBookmarkManager().setBookmark(
        address, "Analysis", category, text
    )


def make_structures():
    """Install packed, address-backed runtime PCode structure layouts."""
    dtm = currentProgram.getDataTypeManager()
    category = CategoryPath("/MWCC/GC_1_2_5")
    pointer = PointerDataType(VoidDataType.dataType, 4, dtm)
    byte = ByteDataType.dataType
    word = WordDataType.dataType
    dword = DWordDataType.dataType

    link = StructureDataType(category, "PCodeLink", 0x08, dtm)
    link.replaceAtOffset(0x00, pointer, 4, "next", "PCodeLink*")
    link.replaceAtOffset(0x04, pointer, 4, "block", "PCodeBlock*")

    operand = StructureDataType(category, "PCodeOperand", 0x0C, dtm)
    operand.replaceAtOffset(0x00, byte, 1, "kind", "0=GPR, 1=FPR, 2=SPR, 3=CR, 4=immediate, 5=memory, 6=label")
    operand.replaceAtOffset(0x01, byte, 1, "access", "operand access/flags byte")
    operand.replaceAtOffset(0x02, dword, 4, "value", "register, immediate, or label pointer depending on kind")
    operand.replaceAtOffset(0x06, pointer, 4, "object", "compiler object for immediate/memory forms")
    operand.replaceAtOffset(0x0A, word, 2, "reserved_0a", None)

    insn = StructureDataType(category, "PCodeInstruction", 0x1C, dtm)
    insn.replaceAtOffset(0x00, pointer, 4, "next", "PCodeInstruction*")
    insn.replaceAtOffset(0x04, pointer, 4, "previous", "PCodeInstruction*")
    insn.replaceAtOffset(0x08, pointer, 4, "block", "PCodeBlock*")
    insn.replaceAtOffset(0x0C, ArrayDataType(byte, 8, 1), 8, "internal_0c", "includes reaching-definition state")
    insn.replaceAtOffset(0x14, word, 2, "opcode", "PCode opcode index")
    insn.replaceAtOffset(0x16, dword, 4, "flags", "includes PCodeInstruction_CoalesceDisabled=0x400")
    insn.replaceAtOffset(0x1A, word, 2, "operand_count", "operands follow at +0x1c, stride 0x0c")

    block = StructureDataType(category, "PCodeBlock", 0x30, dtm)
    block.replaceAtOffset(0x00, pointer, 4, "next", "physical-order next")
    block.replaceAtOffset(0x04, pointer, 4, "previous", "physical-order previous")
    block.replaceAtOffset(0x08, pointer, 4, "label", "label +4 points back to this block")
    block.replaceAtOffset(0x0C, pointer, 4, "predecessors", "PCodeLink*")
    block.replaceAtOffset(0x10, pointer, 4, "successors", "PCodeLink*")
    block.replaceAtOffset(0x14, pointer, 4, "instructions", "first PCodeInstruction*")
    block.replaceAtOffset(0x18, pointer, 4, "last", "last PCodeInstruction*")
    block.replaceAtOffset(0x1C, dword, 4, "index", "physical block index")
    block.replaceAtOffset(0x20, dword, 4, "line", "source line")
    block.replaceAtOffset(0x24, dword, 4, "internal_24", None)
    block.replaceAtOffset(0x28, dword, 4, "loop_weight", "loop nesting/weight")
    block.replaceAtOffset(0x2C, word, 2, "instruction_count", None)
    block.replaceAtOffset(0x2E, word, 2, "flags", None)

    label = StructureDataType(category, "PCodeLabel", 0x08, dtm)
    label.replaceAtOffset(0x00, dword, 4, "internal_00", None)
    label.replaceAtOffset(0x04, pointer, 4, "block", "PCodeBlock*")

    scheduler_edge = StructureDataType(category, "SchedulerEdge", 0x0C, dtm)
    scheduler_edge.replaceAtOffset(0x00, pointer, 4, "next", "SchedulerEdge*")
    scheduler_edge.replaceAtOffset(0x04, pointer, 4, "target", "SchedulerNode*")
    scheduler_edge.replaceAtOffset(0x08, word, 2, "latency", "dependency latency in cycles")
    scheduler_edge.replaceAtOffset(0x0A, word, 2, "reserved_0a", None)

    scheduler_node = StructureDataType(category, "SchedulerNode", 0x1A, dtm)
    scheduler_node.replaceAtOffset(0x00, pointer, 4, "next_textual", "SchedulerNode*")
    scheduler_node.replaceAtOffset(0x04, pointer, 4, "previous_textual", "SchedulerNode*")
    scheduler_node.replaceAtOffset(0x08, pointer, 4, "successors", "SchedulerEdge*")
    scheduler_node.replaceAtOffset(0x0C, pointer, 4, "instruction", "PCodeInstruction*")
    scheduler_node.replaceAtOffset(0x10, word, 2, "latency", "instruction latency")
    scheduler_node.replaceAtOffset(0x12, word, 2, "ready_cycle", "earliest issue cycle from issued predecessors")
    scheduler_node.replaceAtOffset(0x14, word, 2, "deadline", "global maximum height minus this node's height")
    scheduler_node.replaceAtOffset(0x16, word, 2, "height", "critical-path height to block end")
    scheduler_node.replaceAtOffset(0x18, word, 2, "predecessor_count", "unissued predecessors")

    scheduler_model = StructureDataType(category, "SchedulerMachineModel", 0x20, dtm)
    scheduler_model.replaceAtOffset(0x00, dword, 4, "issue_width", "maximum instructions selected per cycle")
    scheduler_model.replaceAtOffset(0x04, dword, 4, "zero_war_waw_latency", "nonzero gives GPR/FPR/VR WAR and WAW edges latency zero")
    scheduler_model.replaceAtOffset(0x08, pointer, 4, "latency", "latency(PCodeInstruction*)")
    scheduler_model.replaceAtOffset(0x0C, pointer, 4, "reset", "reset model state for one block")
    scheduler_model.replaceAtOffset(0x10, pointer, 4, "can_issue", "structural-hazard predicate")
    scheduler_model.replaceAtOffset(0x14, pointer, 4, "on_issue", "occupy unit and completion slot")
    scheduler_model.replaceAtOffset(0x18, pointer, 4, "advance", "advance one cycle")
    scheduler_model.replaceAtOffset(0x1C, pointer, 4, "serializes", "barrier predicate")

    scheduler_timing = StructureDataType(category, "SchedulerOpcodeTiming", 0x06, dtm)
    scheduler_timing.replaceAtOffset(0x00, byte, 1, "unit", "functional-unit class")
    scheduler_timing.replaceAtOffset(0x01, byte, 1, "latency", "base result latency")
    scheduler_timing.replaceAtOffset(0x02, byte, 1, "occupancy", "initial unit occupancy")
    scheduler_timing.replaceAtOffset(0x03, byte, 1, "stage2_countdown", "second pipeline-stage countdown")
    scheduler_timing.replaceAtOffset(0x04, byte, 1, "stage3_countdown", "third pipeline-stage countdown")
    scheduler_timing.replaceAtOffset(0x05, byte, 1, "serialize", "nonzero makes the opcode a scheduling barrier")

    pcode_liveness = StructureDataType(category, "PCodeBlockLiveness", 0x10, dtm)
    pcode_liveness.replaceAtOffset(0x00, pointer, 4, "use", "upward-exposed-use bitset")
    pcode_liveness.replaceAtOffset(0x04, pointer, 4, "definition", "local-definition bitset")
    pcode_liveness.replaceAtOffset(0x08, pointer, 4, "live_in", "fixed-point block live-in bitset")
    pcode_liveness.replaceAtOffset(0x0C, pointer, 4, "live_out", "fixed-point block live-out bitset")

    interference_node = StructureDataType(category, "InterferenceNode", 0x16, dtm)
    interference_node.replaceAtOffset(0x00, pointer, 4, "next", "temporary simplify/select stack link")
    interference_node.replaceAtOffset(0x04, pointer, 4, "object", "associated CompilerObject* when present")
    interference_node.replaceAtOffset(0x08, dword, 4, "spill_cost", "weighted spill-cost numerator")
    interference_node.replaceAtOffset(0x0C, word, 2, "virtual_register", "class-local virtual-register number")
    interference_node.replaceAtOffset(0x0E, word, 2, "degree", "current interference degree")
    interference_node.replaceAtOffset(0x10, word, 2, "physical_register", "selected color or coalesced-root index")
    interference_node.replaceAtOffset(0x12, word, 2, "flags", "allocator/coalescing flags")
    interference_node.replaceAtOffset(0x14, word, 2, "neighbor_count", "u16 neighbor indices follow at +0x16")

    for data_type in (
        link,
        operand,
        insn,
        block,
        label,
        scheduler_edge,
        scheduler_node,
        scheduler_model,
        scheduler_timing,
        pcode_liveness,
        interference_node,
    ):
        dtm.addDataType(data_type, DataTypeConflictHandler.REPLACE_HANDLER)

    return {
        "pointer": pointer,
        "u8": byte,
        "u16": word,
        "u32": dword,
        "scheduler_model": scheduler_model,
        "scheduler_opcode_table": ArrayDataType(scheduler_timing, 468, 0x06),
    }


def annotate_global(value, name, type_name, text, data_types):
    address = checked_address(value)
    create_or_update_label(address, name, True)
    if type_name and currentProgram.getListing().getDefinedDataAt(address) is None:
        createData(address, data_types[type_name])
    set_comment(address, "MWCC GC/1.2.5 global: " + text + ".")


def sanitize(text):
    return re.sub(r"[^A-Za-z0-9_]", "_", text)


def main():
    digest, profile = identify_profile()
    data_types = make_structures()

    for value, name, evidence in FUNCTIONS:
        annotate_function(value, name, evidence)

    for value, name, type_name, text in GLOBALS:
        annotate_global(value, name, type_name, text, data_types)

    for value, stage in AST_CAPTURE_POINTS:
        annotate_site(
            value,
            "MWCC_AST_Capture_" + sanitize(stage),
            "Exact mwcc-debugger AST capture boundary: " + stage.replace("_", " ") + ".",
            "MWCC AST capture",
        )

    for value, stage in PCODE_CAPTURE_POINTS:
        annotate_site(
            value,
            "MWCC_PCode_Capture_" + sanitize(stage) + "_{0:08X}".format(value),
            "Exact mwcc-debugger PCode capture boundary: " + stage.replace("_", " ") + ".",
            "MWCC PCode capture",
        )

    for value, name, text in SPECIAL_SITES:
        annotate_site(value, name, text, "MWCC compiler research")

    if profile["ninji"]:
        annotate_function(
            0x00506510,
            "MWCC_125n_EpiloguePatchCave",
            "GC/1.2.5n cave that applies CoalesceDisabled to stack teardown PCode",
        )
        annotate_site(
            0x0050653D,
            "MWCC_125n_EpiloguePatchCaveEnd",
            "End of the 46-byte GC/1.2.5n cave span.",
            "MWCC compiler research",
        )

    println(
        "Imported MWCC annotations for {0} ({1}); {2} functions, {3} globals, "
        "{4} AST and {5} PCode capture points.".format(
            profile["name"],
            digest,
            len(FUNCTIONS) + (1 if profile["ninji"] else 0),
            len(GLOBALS),
            len(AST_CAPTURE_POINTS),
            len(PCODE_CAPTURE_POINTS),
        )
    )


main()
