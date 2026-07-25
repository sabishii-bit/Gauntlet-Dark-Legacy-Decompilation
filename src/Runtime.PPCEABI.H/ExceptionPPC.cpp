#include "types.h"
#include "NMWException.h"
#include "MWCPlusLib.h"
#include "ExceptionPPC.h"
#include "__ppc_eabi_linker.h"

#define RETURN_ADDRESS 4

using std::terminate;
using std::unexpected;
using std::bad_exception;

typedef struct ThrowContext {
    f64 FPR[32];
    s32 GPR[32];
    s32 CR;
    char* SP;
    char* FP;
    char* throwSP;
    char* returnaddr;
    char* throwtype;
    void* location;
    void* dtor;
    CatchInfo* catchinfo;
} ThrowContext;

typedef struct MWExceptionInfo {
    ExceptionTableSmall* exception_record;
    char* current_function;
    char* action_pointer;
    char* code_section;
    char* data_section;
    char* TOC;
} MWExceptionInfo;

typedef struct FragmentInfo {
    ExceptionTableIndex* exception_start;
    ExceptionTableIndex* exception_end;
    char* code_start;
    char* code_end;
    char* data_start;
    char* data_end;
    char* TOC;
    int active;
} FragmentInfo;

typedef struct ProcessInfo {
    __eti_init_info* exception_info;
    char* TOC;
    int active;
} ProcessInfo;

typedef struct ActionIterator {
    MWExceptionInfo info;
    char* current_SP;
    char* current_FP;
    s32 current_R31;
} ActionIterator;

#define MAXFRAGMENTS 1
static ProcessInfo fragment_info[MAXFRAGMENTS];

typedef void (*DeleteFunc)(void*);

static void ExPPC_ThrowHandler(ThrowContext* context);
static void ExPPC_UnwindStack(ThrowContext* context, MWExceptionInfo* info, void* catcher);
static char* ExPPC_PopStackFrame(ThrowContext* context, MWExceptionInfo* info);
static exaction_type ExPPC_NextAction(ActionIterator* iter);
static void ExPPC_FindExceptionRecord(char* returnaddr, MWExceptionInfo* info);
static int ExPPC_FindExceptionFragment(char* returnaddr, FragmentInfo* frag);

/* clang-format off */
ASM static void ExPPC_LongJump(register ThrowContext* context, register void* newRTOC,
                               register void* newPC) {
#ifdef __MWERKS__
    nofralloc

    mr r8, r5
    mr r2, r4
    lwz r0, 0x180(r3)
    mtcrf 0xff, r0
    lmw r13, 0x134(r3)
    lfd f14, 0x70(r3)
    lfd f15, 0x78(r3)
    lfd f16, 0x80(r3)
    lfd f17, 0x88(r3)
    lfd f18, 0x90(r3)
    lfd f19, 0x98(r3)
    lfd f20, 0xa0(r3)
    lfd f21, 0xa8(r3)
    lfd f22, 0xb0(r3)
    lfd f23, 0xb8(r3)
    lfd f24, 0xc0(r3)
    lfd f25, 0xc8(r3)
    lfd f26, 0xd0(r3)
    lfd f27, 0xd8(r3)
    lfd f28, 0xe0(r3)
    lfd f29, 0xe8(r3)
    lfd f30, 0xf0(r3)
    lfd f31, 0xf8(r3)
    mtlr r8
    lwz r1, 0x18c(r3)
    lwz r3, 0x184(r3)
    lwz r3, 0x0(r3)
    stw r3, 0x0(r1)
    blr
#endif
}

ASM void __throw(char* throwtype, void* location, void* dtor) {
#ifdef __MWERKS__
    nofralloc

    mflr r0
    stw r0, 0x4(r1)
    stwu r1, -0x1c0(r1)
    stw r3, 0x8(r1)
    stw r4, 0xc(r1)
    stw r5, 0x10(r1)
    stmw r13, 0x14c(r1)
    stfd f14, 0x88(r1)
    stfd f15, 0x90(r1)
    stfd f16, 0x98(r1)
    stfd f17, 0xa0(r1)
    stfd f18, 0xa8(r1)
    stfd f19, 0xb0(r1)
    stfd f20, 0xb8(r1)
    stfd f21, 0xc0(r1)
    stfd f22, 0xc8(r1)
    stfd f23, 0xd0(r1)
    stfd f24, 0xd8(r1)
    stfd f25, 0xe0(r1)
    stfd f26, 0xe8(r1)
    stfd f27, 0xf0(r1)
    stfd f28, 0xf8(r1)
    stfd f29, 0x100(r1)
    stfd f30, 0x108(r1)
    stfd f31, 0x110(r1)
    mfcr r3
    stw r3, 0x198(r1)
    lwz r3, 0x0(r1)
    lwz r4, 0x4(r3)
    stw r3, 0x19c(r1)
    stw r3, 0x1a4(r1)
    stw r4, 0x1a8(r1)
    lwz r3, 0x8(r1)
    stw r3, 0x1ac(r1)
    lwz r3, 0xc(r1)
    stw r3, 0x1b0(r1)
    lwz r3, 0x10(r1)
    stw r3, 0x1b4(r1)
    addi r3, r1, 0x18
    bl ExPPC_ThrowHandler
    nop
    lwz r0, 0x1c4(r1)
    addi r1, r1, 0x1c0
    mtlr r0
    blr
#endif
}
/* clang-format on */

void __end__catch(CatchInfo* catchinfo) {
    if (catchinfo->location && catchinfo->dtor) {
        DTORCALL_COMPLETE(catchinfo->dtor, catchinfo->location);
    }
}

static inline s32 ExPPC_PopR31(char* SP, MWExceptionInfo* info) {
    f64* FPR_save_area;
    s32* GPR_save_area;
    int saved_FPRs;

    saved_FPRs = ET_GetSavedFPRs(info->exception_record->et_field);
    FPR_save_area = (f64*)(SP - saved_FPRs * 8);
    if (saved_FPRs & 1) {
        FPR_save_area = (f64*)((char*)FPR_save_area - 8);
    }
    GPR_save_area = (s32*)FPR_save_area;

    return GPR_save_area[-1];
}

static inline exaction_type ExPPC_CurrentAction(const ActionIterator* iter) {
    if (iter->info.action_pointer == 0) {
        return EXACTION_ENDOFLIST;
    }

    return ((ex_destroylocal*)iter->info.action_pointer)->action & EXACTION_MASK;
}

static inline int ExPPC_IsInSpecification(char* extype, ex_specification* spec) {
    s32 i, offset;

    for (i = 0; i < spec->specs; i++) {
        if (__throw_catch_compare(extype, spec->spec[i], &offset))
            return 1;
    }

    return 0;
}

static inline void ExPPC_HandleUnexpected(ThrowContext* context, MWExceptionInfo* info,
                                   ex_specification* unexp) {
    CatchInfo* catchinfo;

#pragma exception_terminate

    ExPPC_UnwindStack(context, info, unexp);

    catchinfo = (CatchInfo*)(context->FP + unexp->cinfo_ref);
    catchinfo->location = context->location;
    catchinfo->typeinfo = context->throwtype;
    catchinfo->dtor = context->dtor;
    catchinfo->stacktop = unexp;

    ExPPC_LongJump(context, info->TOC, info->current_function + unexp->pcoffset);
}

static void ExPPC_ThrowHandler(ThrowContext* context) {
    ActionIterator iter;
    MWExceptionInfo info;
    exaction_type action;
    CatchInfo* catchinfo;
    s32 offset;

    ExPPC_FindExceptionRecord(context->returnaddr, &info);

    if (info.exception_record == 0) {
        terminate();
    }

    context->FP =
        (ET_GetHasFramePtr(info.exception_record->et_field)) ? (char*)context->GPR[31]
                                                             : context->SP;

    if (context->throwtype == 0) {
        iter.info = info;
        iter.current_SP = context->SP;
        iter.current_FP = context->FP;
        iter.current_R31 = context->GPR[31];

        for (action = ExPPC_CurrentAction(&iter);; action = ExPPC_NextAction(&iter)) {
            switch (action) {
            case EXACTION_ACTIVECATCHBLOCK:
                break;
            case EXACTION_ENDOFLIST:
            case EXACTION_DESTROYLOCAL:
            case EXACTION_DESTROYLOCALCOND:
            case EXACTION_DESTROYLOCALPOINTER:
            case EXACTION_DESTROYLOCALARRAY:
            case EXACTION_DESTROYBASE:
            case EXACTION_DESTROYMEMBER:
            case EXACTION_DESTROYMEMBERCOND:
            case EXACTION_DESTROYMEMBERARRAY:
            case EXACTION_DELETEPOINTER:
            case EXACTION_DELETEPOINTERCOND:
            case EXACTION_CATCHBLOCK:
            case EXACTION_CATCHBLOCK_32:
            case EXACTION_SPECIFICATION:
                continue;
            case EXACTION_TERMINATE:
            default:
                terminate();
            }
            break;
        }

        catchinfo =
            (CatchInfo*)(iter.current_FP +
                         ((ex_activecatchblock*)iter.info.action_pointer)->cinfo_ref);
        context->throwtype = (char*)catchinfo->typeinfo;
        context->location = catchinfo->location;
        context->dtor = 0;
        context->catchinfo = catchinfo;
    } else {
        context->catchinfo = 0L;
    }

    iter.info = info;
    iter.current_SP = context->SP;
    iter.current_FP = context->FP;
    iter.current_R31 = context->GPR[31];

    for (action = ExPPC_CurrentAction(&iter);; action = ExPPC_NextAction(&iter)) {
        switch (action) {
        case EXACTION_CATCHBLOCK_32:
            if (__throw_catch_compare(context->throwtype,
                                      ((ex_catchblock_32*)iter.info.action_pointer)->catch_type,
                                      &offset)) {
                break;
            }
            continue;
        case EXACTION_CATCHBLOCK:
            if (__throw_catch_compare(context->throwtype,
                                      ((ex_catchblock*)iter.info.action_pointer)->catch_type,
                                      &offset)) {
                break;
            }
            continue;
        case EXACTION_SPECIFICATION:
            if (!ExPPC_IsInSpecification(context->throwtype,
                                         (ex_specification*)iter.info.action_pointer)) {
                ExPPC_HandleUnexpected(context, &info,
                                       (ex_specification*)iter.info.action_pointer);
            }
            continue;
        case EXACTION_ENDOFLIST:
        case EXACTION_DESTROYLOCAL:
        case EXACTION_DESTROYLOCALCOND:
        case EXACTION_DESTROYLOCALPOINTER:
        case EXACTION_DESTROYLOCALARRAY:
        case EXACTION_DESTROYBASE:
        case EXACTION_DESTROYMEMBER:
        case EXACTION_DESTROYMEMBERCOND:
        case EXACTION_DESTROYMEMBERARRAY:
        case EXACTION_DELETEPOINTER:
        case EXACTION_DELETEPOINTERCOND:
        case EXACTION_ACTIVECATCHBLOCK:
            continue;
        case EXACTION_TERMINATE:
        default:
            terminate();
        }
        break;
    }

    if (action == EXACTION_CATCHBLOCK_32) {
        ex_catchblock_32* catchblock_32;

        catchblock_32 = (ex_catchblock_32*)iter.info.action_pointer;
        ExPPC_UnwindStack(context, &info, catchblock_32);

        catchinfo = (CatchInfo*)(context->FP + catchblock_32->cinfo_ref);
        catchinfo->location = context->location;
        catchinfo->typeinfo = context->throwtype;
        catchinfo->dtor = context->dtor;

        if (*context->throwtype == '*') {
            catchinfo->sublocation = &catchinfo->pointercopy;
            catchinfo->pointercopy = *(s32*)context->location + offset;
        } else {
            catchinfo->sublocation = (char*)context->location + offset;
        }

        ExPPC_LongJump(context, info.TOC, info.current_function + catchblock_32->catch_pcoffset);
    } else {
        ex_catchblock* catchblock;

        catchblock = (ex_catchblock*)iter.info.action_pointer;
        ExPPC_UnwindStack(context, &info, catchblock);

        catchinfo = (CatchInfo*)(context->FP + catchblock->cinfo_ref);
        catchinfo->location = context->location;
        catchinfo->typeinfo = context->throwtype;
        catchinfo->dtor = context->dtor;

        if (*context->throwtype == '*') {
            catchinfo->sublocation = &catchinfo->pointercopy;
            catchinfo->pointercopy = *(s32*)context->location + offset;
        } else {
            catchinfo->sublocation = (char*)context->location + offset;
        }

        ExPPC_LongJump(context, info.TOC, info.current_function + catchblock->catch_pcoffset);
    }
}

extern void __unexpected(CatchInfo* catchinfo) {
    ex_specification* unexp = (ex_specification*)catchinfo->stacktop;

#pragma exception_magic

    try {
        unexpected();
    } catch (...) {
        if (ExPPC_IsInSpecification((char*)((CatchInfo*)&__exception_magic)->typeinfo, unexp)) {
            throw;
        }
        if (ExPPC_IsInSpecification("!bad_exception!!", unexp)) {
            throw bad_exception();
        }
        if (ExPPC_IsInSpecification("!std::bad_exception!!", unexp)) {
            throw bad_exception();
        }
    }
    terminate();
}

std::bad_exception::~bad_exception() throw() {}

static inline void ExPPC_DestroyLocal(ThrowContext* context, const ex_destroylocal* ex) {
    DTORCALL_COMPLETE(ex->dtor, context->FP + ex->local);
}

static inline void ExPPC_DestroyLocalCond(ThrowContext* context, const ex_destroylocalcond* ex) {
    int cond = ex_destroylocalcond_GetRegCond(ex->dlc_field)
                   ? (local_cond_type)context->GPR[ex->cond]
                   : *(local_cond_type*)(context->FP + ex->cond);

    if (cond) {
        DTORCALL_COMPLETE(ex->dtor, context->FP + ex->local);
    }
}

static inline void ExPPC_DestroyLocalPointer(ThrowContext* context, const ex_destroylocalpointer* ex) {
    void* pointer = ex_destroylocalpointer_GetRegPointer(ex->dlp_field)
                        ? (void*)context->GPR[ex->pointer]
                        : *(void**)(context->FP + ex->pointer);

    DTORCALL_COMPLETE(ex->dtor, pointer);
}

static inline void ExPPC_DestroyLocalArray(ThrowContext* context, const ex_destroylocalarray* ex) {
    char* ptr = context->FP + ex->localarray;
    s32 n = ex->elements;
    s32 size = ex->element_size;

    for (ptr = ptr + size * n; n > 0; n--) {
        ptr -= size;
        DTORCALL_COMPLETE(ex->dtor, ptr);
    }
}

static inline void ExPPC_DestroyMember(ThrowContext* context, const ex_destroymember* ex) {
    char* objectptr = ex_destroymember_GetRegPointer(ex->dm_field)
                          ? (char*)context->GPR[ex->objectptr]
                          : *(char**)(context->FP + ex->objectptr);

    DTORCALL_COMPLETE(ex->dtor, objectptr + ex->offset);
}

static inline void ExPPC_DestroyBase(ThrowContext* context, const ex_destroymember* ex) {
    char* objectptr = ex_destroymember_GetRegPointer(ex->dm_field)
                          ? (char*)context->GPR[ex->objectptr]
                          : *(char**)(context->FP + ex->objectptr);

    DTORCALL_PARTIAL(ex->dtor, objectptr + ex->offset);
}

static inline void ExPPC_DestroyMemberCond(ThrowContext* context, const ex_destroymembercond* ex) {
    char* objectptr = ex_destroymembercond_GetRegPointer(ex->dmc_field)
                          ? (char*)context->GPR[ex->objectptr]
                          : *(char**)(context->FP + ex->objectptr);
    int cond = ex_destroymembercond_GetRegCond(ex->dmc_field)
                   ? (vbase_ctor_arg_type)context->GPR[ex->cond]
                   : *(vbase_ctor_arg_type*)(context->FP + ex->cond);

    if (cond) {
        DTORCALL_PARTIAL(ex->dtor, objectptr + ex->offset);
    }
}

static inline void ExPPC_DestroyMemberArray(ThrowContext* context, const ex_destroymemberarray* ex) {
    char* ptr = ex_destroymemberarray_GetRegPointer(ex->dma_field)
                    ? (char*)context->GPR[ex->objectptr]
                    : *(char**)(context->FP + ex->objectptr);
    s32 n = ex->elements;
    s32 size = ex->element_size;

    ptr += ex->offset;

    for (ptr = ptr + size * n; n > 0; n--) {
        ptr -= size;
        DTORCALL_COMPLETE(ex->dtor, ptr);
    }
}

static inline void ExPPC_DeletePointer(ThrowContext* context, const ex_deletepointer* ex) {
    char* objectptr = ex_deletepointer_GetRegPointer(ex->dp_field)
                          ? (char*)context->GPR[ex->objectptr]
                          : *(char**)(context->FP + ex->objectptr);

    ((DeleteFunc)ex->deletefunc)(objectptr);
}

static inline void ExPPC_DeletePointerCond(ThrowContext* context, const ex_deletepointercond* ex) {
    char* objectptr = ex_deletepointercond_GetRegPointer(ex->dpc_field)
                          ? (char*)context->GPR[ex->objectptr]
                          : *(char**)(context->FP + ex->objectptr);
    int cond = ex_deletepointercond_GetRegCond(ex->dpc_field)
                   ? (local_cond_type)context->GPR[ex->cond]
                   : *(local_cond_type*)(context->FP + ex->cond);

    if (cond) {
        ((DeleteFunc)ex->deletefunc)(objectptr);
    }
}

static void ExPPC_UnwindStack(ThrowContext* context, MWExceptionInfo* info, void* catcher) {
    exaction_type action;

#pragma exception_terminate

    for (;;) {
        if (info->action_pointer == 0) {
            char* return_addr;

            return_addr = ExPPC_PopStackFrame(context, info);
            ExPPC_FindExceptionRecord(return_addr, info);

            if (info->exception_record == 0) {
                terminate();
            }

            context->FP = (ET_GetHasFramePtr(info->exception_record->et_field))
                              ? (char*)context->GPR[31]
                              : context->SP;
            continue;
        }

        action = ((ex_destroylocal*)info->action_pointer)->action;

        switch (action & EXACTION_MASK) {
        case EXACTION_BRANCH:
            info->action_pointer =
                ((char*)info->exception_record) + ((ex_branch*)info->action_pointer)->target;
            break;
        case EXACTION_DESTROYLOCAL:
            ExPPC_DestroyLocal(context, (ex_destroylocal*)info->action_pointer);
            info->action_pointer += sizeof(ex_destroylocal);
            break;
        case EXACTION_DESTROYLOCALCOND:
            ExPPC_DestroyLocalCond(context, (ex_destroylocalcond*)info->action_pointer);
            info->action_pointer += sizeof(ex_destroylocalcond);
            break;
        case EXACTION_DESTROYLOCALPOINTER:
            ExPPC_DestroyLocalPointer(context, (ex_destroylocalpointer*)info->action_pointer);
            info->action_pointer += sizeof(ex_destroylocalpointer);
            break;
        case EXACTION_DESTROYLOCALARRAY:
            ExPPC_DestroyLocalArray(context, (ex_destroylocalarray*)info->action_pointer);
            info->action_pointer += sizeof(ex_destroylocalarray);
            break;
        case EXACTION_DESTROYBASE:
            ExPPC_DestroyBase(context, (ex_destroymember*)info->action_pointer);
            info->action_pointer += sizeof(ex_destroymember);
            break;
        case EXACTION_DESTROYMEMBER:
            ExPPC_DestroyMember(context, (ex_destroymember*)info->action_pointer);
            info->action_pointer += sizeof(ex_destroymember);
            break;
        case EXACTION_DESTROYMEMBERCOND:
            ExPPC_DestroyMemberCond(context, (ex_destroymembercond*)info->action_pointer);
            info->action_pointer += sizeof(ex_destroymembercond);
            break;
        case EXACTION_DESTROYMEMBERARRAY:
            ExPPC_DestroyMemberArray(context, (ex_destroymemberarray*)info->action_pointer);
            info->action_pointer += sizeof(ex_destroymemberarray);
            break;
        case EXACTION_DELETEPOINTER:
            ExPPC_DeletePointer(context, (ex_deletepointer*)info->action_pointer);
            info->action_pointer += sizeof(ex_deletepointer);
            break;
        case EXACTION_DELETEPOINTERCOND:
            ExPPC_DeletePointerCond(context, (ex_deletepointercond*)info->action_pointer);
            info->action_pointer += sizeof(ex_deletepointercond);
            break;
        case EXACTION_CATCHBLOCK:
            if (catcher == (void*)info->action_pointer)
                return;
            info->action_pointer += sizeof(ex_catchblock);
            break;
        case EXACTION_CATCHBLOCK_32:
            if (catcher == (void*)info->action_pointer)
                return;
            info->action_pointer += sizeof(ex_catchblock_32);
            break;
        case EXACTION_ACTIVECATCHBLOCK: {
            CatchInfo* catchinfo;

            catchinfo =
                (CatchInfo*)(context->FP + ((ex_activecatchblock*)info->action_pointer)->cinfo_ref);

            if (catchinfo->dtor) {
                if (context->location == catchinfo->location) {
                    context->dtor = catchinfo->dtor;
                } else {
                    DTORCALL_COMPLETE(catchinfo->dtor, catchinfo->location);
                }
            }
            info->action_pointer += sizeof(ex_activecatchblock);
        } break;
        case EXACTION_SPECIFICATION:
            if (catcher == (void*)info->action_pointer)
                return;
            /* advance by the 12-byte header only: retail's sizeof did not
               count the spec[1] flexible tail */
            info->action_pointer += (sizeof(ex_specification) - sizeof(char*)) +
                                    ((ex_specification*)info->action_pointer)->specs * sizeof(void*);
            break;
        default:
            terminate();
        }

        if (action & EXACTION_ENDBIT)
            info->action_pointer = 0;
    }
}

static char* ExPPC_PopStackFrame(ThrowContext* context, MWExceptionInfo* info) {
    char *SP, *callers_SP;
    f64* FPR_save_area;
    s32* GPR_save_area;
    int saved_GPRs, saved_FPRs;
    int i, j;

    SP = context->SP;
    callers_SP = *(char**)SP;
    saved_FPRs = ET_GetSavedFPRs(info->exception_record->et_field);

    FPR_save_area = (f64*)(callers_SP - saved_FPRs * 8);

    for (i = 32 - saved_FPRs, j = 0; i < 32; ++i, ++j) {
        context->FPR[i] = FPR_save_area[j];
    }

    saved_GPRs = ET_GetSavedGPRs(info->exception_record->et_field);
    GPR_save_area = (s32*)FPR_save_area;
    GPR_save_area -= saved_GPRs;

    for (i = 32 - saved_GPRs, j = 0; i < 32; ++i, ++j) {
        context->GPR[i] = GPR_save_area[j];
    }

    context->SP = callers_SP;
    return *(char**)(callers_SP + RETURN_ADDRESS);
}

static exaction_type ExPPC_NextAction(ActionIterator* iter) {
    exaction_type action;

    for (;;) {
        if (iter->info.action_pointer == 0 ||
            ((action = ((ex_destroylocal*)iter->info.action_pointer)->action) & EXACTION_ENDBIT) !=
                0) {
            char *return_addr, *callers_SP;

            callers_SP = *(char**)iter->current_SP;

            if (ET_GetSavedGPRs(iter->info.exception_record->et_field)) {
                iter->current_R31 = ExPPC_PopR31(callers_SP, &iter->info);
            }

            return_addr = *(char**)(callers_SP + RETURN_ADDRESS);

            ExPPC_FindExceptionRecord(return_addr, &iter->info);

            if (iter->info.exception_record == 0) {
                terminate();
            }

            iter->current_SP = callers_SP;
            iter->current_FP = (ET_GetHasFramePtr(iter->info.exception_record->et_field))
                                   ? (char*)iter->current_R31
                                   : iter->current_SP;

            if (iter->info.action_pointer == 0)
                continue;
        } else {
            switch (action) {
            case EXACTION_DESTROYLOCAL:
                iter->info.action_pointer += sizeof(ex_destroylocal);
                break;
            case EXACTION_DESTROYLOCALCOND:
                iter->info.action_pointer += sizeof(ex_destroylocalcond);
                break;
            case EXACTION_DESTROYLOCALPOINTER:
                iter->info.action_pointer += sizeof(ex_destroylocalpointer);
                break;
            case EXACTION_DESTROYLOCALARRAY:
                iter->info.action_pointer += sizeof(ex_destroylocalarray);
                break;
            case EXACTION_DESTROYBASE:
            case EXACTION_DESTROYMEMBER:
                iter->info.action_pointer += sizeof(ex_destroymember);
                break;
            case EXACTION_DESTROYMEMBERCOND:
                iter->info.action_pointer += sizeof(ex_destroymembercond);
                break;
            case EXACTION_DESTROYMEMBERARRAY:
                iter->info.action_pointer += sizeof(ex_destroymemberarray);
                break;
            case EXACTION_DELETEPOINTER:
                iter->info.action_pointer += sizeof(ex_deletepointer);
                break;
            case EXACTION_DELETEPOINTERCOND:
                iter->info.action_pointer += sizeof(ex_deletepointercond);
                break;
            case EXACTION_CATCHBLOCK:
                iter->info.action_pointer += sizeof(ex_catchblock);
                break;
            case EXACTION_CATCHBLOCK_32:
                iter->info.action_pointer += sizeof(ex_catchblock_32);
                break;
            case EXACTION_ACTIVECATCHBLOCK:
                iter->info.action_pointer += sizeof(ex_activecatchblock);
                break;
            case EXACTION_SPECIFICATION:
                /* 12-byte header only; see ExPPC_UnwindStack */
                iter->info.action_pointer +=
                    (sizeof(ex_specification) - sizeof(char*)) +
                    ((ex_specification*)iter->info.action_pointer)->specs * sizeof(void*);
                break;
            default:
                terminate();
            }
        }

        action = ((ex_destroylocal*)iter->info.action_pointer)->action & EXACTION_MASK;

        if (action == EXACTION_BRANCH) {
            iter->info.action_pointer = ((char*)iter->info.exception_record) +
                                        ((ex_branch*)iter->info.action_pointer)->target;
            action = ((ex_destroylocal*)iter->info.action_pointer)->action & EXACTION_MASK;
        }
        return action;
    }
}

static void ExPPC_FindExceptionRecord(char* returnaddr, MWExceptionInfo* info) {
    FragmentInfo* fragment;
    FragmentInfo frag;
    ExceptionTableIndex *exceptionindex, *p;
    u32 returnoffset;
    s32 i, m, n;

    info->exception_record = 0;
    info->action_pointer = 0;

    if ((ExPPC_FindExceptionFragment(returnaddr, &frag)) == 0)
        return;
    fragment = &frag;

    info->code_section = fragment->code_start;
    info->data_section = fragment->data_start;
    info->TOC = fragment->TOC;

    returnoffset = returnaddr - fragment->code_start;
    exceptionindex = fragment->exception_start;
    for (i = 0, n = fragment->exception_end - fragment->exception_start;;) {
        if (i > n)
            return;
        p = &exceptionindex[m = (i + n) / 2];

        if (returnoffset < p->functionoffset) {
            n = m - 1;
        } else if (returnoffset > p->functionoffset + ETI_GetFunctionSize(p->eti_field)) {
            i = m + 1;
        } else
            break;
    }
    info->current_function = fragment->code_start + p->functionoffset;
    info->exception_record = ETI_GetDirectStore(p->eti_field)
                                 ? (ExceptionTableSmall*)(&p->exceptionoffset)
                                 : (ExceptionTableSmall*)(fragment->data_start + p->exceptionoffset);

    returnoffset -= p->functionoffset;

    if (ET_IsLargeTable(info->exception_record->et_field)) {
        ExceptionTableLarge* etl = (ExceptionTableLarge*)info->exception_record;
        ExceptionRangeLarge* erl;

        for (erl = etl->ranges; erl->start != 0; erl++) {
            u32 range_end = erl->start + (erl->size * 4);

            if (erl->start <= returnoffset && range_end >= returnoffset) {
                info->action_pointer = (char*)etl + erl->action;
                break;
            }
        }
    } else {
        ExceptionTableSmall* ets = (ExceptionTableSmall*)info->exception_record;
        ExceptionRangeSmall* ers;

        for (ers = ets->ranges; ers->start != 0; ers++) {
            if (ers->start <= returnoffset && ers->end >= returnoffset) {
                info->action_pointer = (char*)ets + ers->action;
                break;
            }
        }
    }
}

static int ExPPC_FindExceptionFragment(char* returnaddr, FragmentInfo* frag) {
    ProcessInfo* f;
    int i;
    __eti_init_info* eti_info;

    for (i = 0, f = fragment_info; i < MAXFRAGMENTS; ++i, ++f) {
        if (f->active) {
            eti_info = f->exception_info;
            while (1) {
                if (eti_info->code_size == 0)
                    break;
                if (returnaddr >= eti_info->code_start &&
                    returnaddr < (char*)eti_info->code_start + eti_info->code_size) {
                    frag->exception_start = (ExceptionTableIndex*)eti_info->eti_start;
                    frag->exception_end = (ExceptionTableIndex*)eti_info->eti_end;
                    frag->code_start = 0;
                    frag->code_end = 0;
                    frag->data_start = 0;
                    frag->data_end = 0;
                    frag->TOC = f->TOC;
                    frag->active = f->active;
                    return 1;
                }
                eti_info++;
            }
        }
    }

    return 0;
}

void __unregister_fragment(int fragmentID) {
    ProcessInfo* f;
    if (fragmentID >= 0 && fragmentID < MAXFRAGMENTS) {
        f = &fragment_info[fragmentID];
        f->exception_info = 0;
        f->TOC = 0;
        f->active = 0;
    }
}

int __register_fragment(struct __eti_init_info* info, char* TOC) {
    ProcessInfo* f;
    int i;

    for (i = 0, f = fragment_info; i < MAXFRAGMENTS; ++i, ++f) {
        if (f->active == 0) {
            f->exception_info = info;
            f->TOC = TOC;
            f->active = 1;
            return i;
        }
    }

    return -1;
}
