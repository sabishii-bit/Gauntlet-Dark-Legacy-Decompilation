#include "types.h"
#include "game/mbobject.h"

/* pb_objects.c -- current GameCube PB memory/model/object reconstruction
 * envelope, .text 0x800C3674-0x800C3F58. Xbox divides these routines between
 * PB_MEM.OBJ and PB_OBJECTS.OBJ; that roster is not proof of one original GC TU.
 * Sits between pb_global.c and pb_objregs.c; compiled cflags_demo
 * (-O4 -inline auto, -Cpp_exceptions on, -str reuse,readonly), GC/1.2.5n.
 *
 * The GameCube build of this TU differs from the Xbox pb_objects.obj: the
 * GX/DEMO port keeps the object push-buffer allocator + module bring-up/reset
 * hooks (called by pb_global.c/pb_window.c/mb_*), the object draw dispatch
 * (pbDrawObject), the texture upload path (pbSendObjTextures) and the
 * interactive object-debug single-stepper (pbDebugObjSStep); other debug
 * helpers (pbObjectBsearch/printSelObj/calcObjCnt) are dead-stripped;
 * pbDebugObjStart/End are inlined into the texture-upload path.
 *
 * Function names: pbSendObjTextures / pbDebugObjSStep from shell3D.pdb and the
 * "NO-ObjDef" / "Obj Textures Larger than a page" / "PB_ODB_*" strings;
 * pbObjTexSub / pbSendObjTexturesSub are the outlined texture statics. The push-
 * buffer allocator + bring-up hooks stay fn_<addr> because pb_global.c (already
 * Matching) and pb_window/mb_* reference them by that name.
 *
 * NonMatching: bodies are reconstructions from the disassembly; several small
 * ones are byte-exact (see the worker report). Data is declared extern (defined
 * by the DOL); a future flip must add the .sdata/.sdata2/.rodata/.data/.bss
 * claims documented in the report.
 */

/* --- shared pb-global block (see pb_global.c) --- */
/* PBSPRBUF is the witnessed 0x800-byte free-list record: the 2044-byte buf is
 * its real payload. PBObjPool models only the first eight bytes of
 * PBGLOBAL_MEM, not the complete Xbox 0x14/GC external 0x18 control storage. */
typedef struct PBSPRBUF {
    struct PBSPRBUF* next;
    u8 buf[2044];
} PBSPRBUF;

typedef struct PBObjPool {
    int spr_buf_sema_id; /* 0x00 : semaphore ID returned by CreateSema */
    PBSPRBUF* freehead;    /* 0x04 : head of the 0x800-stride free list */
} PBObjPool;

typedef struct PBGlobal {
    u8 _pad00[0x2c];
    PBObjPool* objPool; /* 0x2c : object push-buffer pool control */
    void* dbg2;         /* 0x30 : secondary object block (0x10-stride array) */
} PBGlobal;

extern PBGlobal* gWinGlobals; /* 0x80344FC0 */

/* The draw/texture entry points consume the same MBObject (Xbox mbnode) as
 * mb_objects.c, not a PBWINDOW: lha +0x5c is texchangeidx, lwz +0x58 is
 * texaltidx, and +0x60/+0x6c/+0x70 are flags/index/data.romobj.
 *
 * The resolved payload is ROMOBJECT, corroborated by mb_model.c's loader
 * and research/xbox_symbols/misc.h. This partial view ends at ObjDef; it
 * does not describe the unused trailing words or claim a new array stride.
 * Draw and texture traversal verify SubObjCnt +0x0c, the four halfwords
 * +0x10..+0x16 (only LodK signed), and pointers +0x18/+0x1c/+0x2c.
 * SUBOBJECT is independently walked at eight-byte strides with lhz at
 * +0/+2/+4 and lha at +6. DataPtr remains a word pointer, byte-addressed
 * below because QWC advances the geometry stream in 16-byte quadwords. */
typedef struct PBSubObject {
    u16 QWC;
    u16 TexIdx;
    u16 LMIdx;
    s16 LodK;
} PBSubObject;

struct OBJDEF;
typedef struct PBRomObjectView {
    f32 InvRad;
    f32 BndRad;
    u32 Flags;
    s32 SubObjCnt;
    u16 SubObj0_QWC;
    u16 SubObj0_TexIdx;
    u16 SubObj0_LMIdx;
    s16 SubObj0_LodK;
    PBSubObject* SubObjPtr;
    u32* DataPtr;
    s32 VertCount;
    s32 TriCount;
    s32 IDnum;
    struct OBJDEF* ObjDef;
} PBRomObjectView;

/* --- pb_objects private data (defined in the DOL) --- */
extern s16 lbl_80345018;       /* per-frame object counter (reset each frame) */
extern int lbl_80345020;       /* "objects module open" flag */
extern int lbl_80345024;       /* ping-pong buffer toggle */
extern int lbl_80345028;       /* alloc mode: 0 = ping-pong, else free-list */
extern int lbl_80343F28;       /* scratch-buffer semaphore ID (init -1) */
/* The target holds three pointers { arena, arena + 0x2000, NULL } here.
 * This legacy scalar adapter preserves SDA addressing; declaring the actual
 * 12-byte array currently changes the native allocator. Extent remains debt. */
extern void* lbl_80343F2C;
extern int lbl_80343F3C;       /* draw-hook enable flag (init 1) */

extern u8 lbl_802C52C0[0x18];  /* PBObjPool storage */
extern u8 lbl_802C52D8[0x158]; /* secondary object block storage */
/* GC walks eight 0x800-byte nodes across the two current 0x2000 linker
 * symbols at 802913C0/802933C0. The incomplete extern describes the accessed
 * record, without claiming allocation or changing those storage boundaries. */
extern PBSPRBUF lbl_802913C0[];
extern u8 lbl_802913C0_hi[];   /* == lbl_802933C0, upper half of the pool */

extern u8 lbl_80128178[0x18];  /* semaphore parameter block */
extern u32 lbl_802913C0_ptr;

/* --- object-debug control (0x801281AC in .data, ptr held in lbl_80343F40) --- */
/* dbg2 is a 0x10-stride slot array; f0 of the NEXT slot is the busy word the
 * draw/texture paths test (base + idx*0x10 + 0x10). */
typedef struct PBObjSlot {
    int f0, f4, f8, fc;
} PBObjSlot;

typedef struct PBObjDebug {
    int state;      /* 0x00 : sstep, enables the single-stepper */
    u8 _pad04[0x30];
    int step;       /* 0x34 : cur_state, the current PB_ODB_* state */
    void* obj;      /* 0x38 : object being inspected */
    char* defName;  /* 0x3c : "NO-ObjDef" or the def name */
} PBObjDebug;
extern PBObjDebug* lbl_80343F40; /* -> 0x801281AC */

extern char lbl_801165B8[];    /* "PB_ODB_NOT_DRAWING_OBJECT" + format block */
extern char lbl_801167A4[];    /* "NO-ObjDef" */
extern char lbl_801167B0[];    /* "Obj Textures Larger than a page" */
extern void* lbl_80128190[7];  /* PB_ODB_* state name table */
extern char lbl_80348F38[];    /* "???" */
extern char lbl_80348F3C[];    /* "busy" */
extern char lbl_80348F44[];    /* "idle" */

/* --- externs into other TUs / the SDK --- */
extern int WaitSema(int sema);
extern int DIntr(void);
extern int EIntr(void);
extern int CreateSema(void* param);

extern void pbSetupPosLights(f32 extra, int, void*, void*); /* pb_objregs pos-light setup */
extern s32 pbSetDORegs(s32, u32, s32, u32, u32, s32, f32*, void*, u8*);
extern int fn_800C7558(int handle);              /* pb_texture: resolve texture */
extern void fn_800C1120(int);                    /* texture cache flush */
extern int fn_800C1148(int, int, char* prompt); /* debug pad query */
extern void FatalError(const char* msg, int code);
extern void bulletproof_printf(const char* fmt, ...);
extern u32 pbGetTime(void);

void fn_800C3674(void);
void* fn_800C3680(void);
void fn_800C36F8(void);
void fn_800C379C(void);
void fn_800C37C4(void);
void fn_800C3880(void);
void fn_800C38A0(void);
int fn_800C38C0(void* a, MBObject* obj);
static int pbObjTexSub(MBObject* obj, int lo, int hi, u32* flags);
int pbSendObjTextures(MBObject* obj);
static int pbSendObjTexturesSub(int idx, PBRomObjectView* def);
void pbDebugObjSStep(MBObject* obj, int state);

/* Reset the per-frame object counter. */
void fn_800C3674(void)
{
    lbl_80345018 = 0;
}

/* Allocate an object push-buffer: pop the free list, or ping-pong between the
 * two static buffers when the free-list mode is off. */
void* fn_800C3680(void)
{
    if (lbl_80345028 == 0) {
        void* r = (&lbl_80343F2C)[lbl_80345024];
        lbl_80345024 = lbl_80345024 ^ 1;
        return r;
    } else {
        PBGlobal* g = gWinGlobals;
        PBSPRBUF* head;
        WaitSema(g->objPool->spr_buf_sema_id);
        DIntr();
        head = g->objPool->freehead;
        g->objPool->freehead = head->next;
        EIntr();
        return head;
    }
}

/* Rebuild the object push-buffer free list (8 x 0x800-byte nodes). */
void fn_800C36F8(void)
{
    PBGlobal* g;
    PBSPRBUF* base;
    int i;

    g = gWinGlobals;
    if (lbl_80343F28 == -1) {
        lbl_80343F28 = CreateSema(lbl_80128178);
    }
    g->objPool->spr_buf_sema_id = lbl_80343F28;
    g->objPool->freehead = lbl_802913C0;
    for (i = 0; i < 7; i++) {
        base = g->objPool->freehead;
        base[i].next = &base[i + 1];
    }
    base = g->objPool->freehead;
    base[7].next = 0;
}

/* Light reset hook: attach the pool control block, flag the module open. */
void fn_800C379C(void)
{
    PBGlobal* g = gWinGlobals;
    if (g->objPool == 0) {
        g->objPool = (PBObjPool*)lbl_802C52C0;
    }
    lbl_80345020 = 1;
}

/* pbInitMem attaches the control then calls the existing pbSPFreeAll helper
 * (fn_800C36F8), as also witnessed by PS2/PDB PB_MEM. The GC call is inlined. */
void fn_800C37C4(void)
{
    u8 unused[8]; /* Unrecovered reservation: removing it shrinks frame 24 to 16. */

    gWinGlobals->objPool = (PBObjPool*)lbl_802C52C0;
    fn_800C36F8();
    lbl_80345020 = 1;
}

/* Secondary object block reset hook. */
void fn_800C3880(void)
{
    PBGlobal* g = gWinGlobals;
    if (g->dbg2 != 0) {
        return;
    }
    g->dbg2 = lbl_802C52D8;
}

/* Secondary object block bring-up hook. */
void fn_800C38A0(void)
{
    PBGlobal* g = gWinGlobals;
    g->dbg2 = lbl_802C52D8;
    *(int*)g->dbg2 = 0;
}

/* Draw one object: resolve the texture-shift, then emit its primitives via the
 * pb_objregs geometry path. */
int fn_800C38C0(void* a, MBObject* obj)
{
    int pcount;
    PBRomObjectView* def;
    int hi;
    u8* v1c;
    int tex;
    int v25;
    int v24;
    u8 unusedA[4];
    u32 packed;
    PBSubObject* prim;
    int stride;
    PBObjSlot* t;
    u32 flags;
    u8 unusedB[4];
    PBGlobal* g = gWinGlobals;

    packed = obj->index;
    if (packed == 0) {
        return 0;
    }
    hi = packed >> 16;
    t = (PBObjSlot*)g->dbg2;
    def = (PBRomObjectView*)obj->data.romobj;
    if (t[(packed >> 16) + 1].f0 != 0) {
        return 0;
    }
    if (def->SubObjCnt == 0) {
        return 0;
    }
    flags = obj->flags & 0x1090D7C0;
    tex = pbObjTexSub(obj, def->SubObj0_TexIdx, hi, &flags);
    v25 = def->SubObj0_LodK;
    v1c = (u8*)def->DataPtr;
    v24 = def->SubObj0_LMIdx;
    if (def->Flags & 0x100) {
        flags |= 0x20000;
    }
    if (flags & 0x8000) {
        flags |= 0x20000;
    }
    if (lbl_80343F3C != 0) {
        pbSetupPosLights(def->BndRad, 0, obj, a);
    }
    pbSetDORegs(0, tex, v25, v24, flags, hi, a, v1c, (u8*)obj);
    pcount = def->SubObjCnt - 1;
    if (pcount != 0) {
        prim = def->SubObjPtr;
        stride = def->SubObj0_QWC;
        do {
            u32 tx;
            v1c += stride << 4;
            tx = pbObjTexSub(obj, prim->TexIdx, hi, &flags);
            stride = prim->QWC;
            pbSetDORegs(0, tx, prim->LodK, prim->LMIdx, flags,
                        hi, 0, v1c, 0);
            prim++;
        } while (--pcount != 0);
    }
    return 0;
}

/* Resolve a texture-shift descriptor into a packed tex address / flag word. */
static int pbObjTexSub(MBObject* obj, int lo, int hi, u32* flags)
{
    s32 t = obj->texchangeidx;

    *flags &= ~0x00080000;
    switch (t) {
    case -1: {
        u32 result = hi << 16;
        result = (result & 0xFFFF0000) | (lo & 0xFFFF);
        return result;
    }
    case -2:
        return obj->texaltidx;
    case -4: {
        u32 result;
        *flags |= 0x08000000;
        result = hi << 16;
        result = (result & 0xFFFF0000) | (lo & 0xFFFF);
        return result;
    }
    case -3: {
        u32 result = obj->texaltidx;
        *flags |= 0x00080000;
        return result;
    }
    default:
        if (lo == t) {
            return obj->texaltidx;
        }
        {
            u32 result = hi << 16;
            result = (result & 0xFFFF0000) | (lo & 0xFFFF);
            return result;
        }
    }
}

/* PS2/PDB pbDebugObjStart/End are the original debug entry/exit helpers.
 * GC inlines both. In particular, ObjDef is read before either debug store;
 * OBJDEF starts with its name array, so the pointer itself is the name. */
static inline void pbDebugObjStart(MBObject* obj, int state)
{
    char* defName = (char*)((PBRomObjectView*)obj->data.romobj)->ObjDef;

    lbl_80343F40->step = state;
    lbl_80343F40->obj = obj;
    lbl_80343F40->defName = defName ? defName : lbl_801167A4;
    if (lbl_80343F40->state != 0) {
        pbDebugObjSStep(obj, state);
    }
}

static inline void pbDebugObjEnd(MBObject* obj, int state)
{
    lbl_80343F40->step = state;
    if (lbl_80343F40->state != 0) {
        pbDebugObjSStep(obj, state);
    }
}

/* Upload an object's textures, retrying once via a cache flush; fatal if the
 * texture set will not fit a page. */
int pbSendObjTextures(MBObject* obj)
{
    int tex = 1;
    int shift = -1;
    int isTexShift;

    pbDebugObjStart(obj, 2);

    switch (obj->texchangeidx) {
    case -1:
        isTexShift = 1;
        break;
    case -2:
        shift = obj->texaltidx;
        isTexShift = 0;
        break;
    case -4:
        shift = obj->texaltidx;
        isTexShift = 1;
        break;
    case -3:
        shift = obj->texaltidx;
        isTexShift = 0;
        break;
    default:
        shift = obj->texaltidx;
        isTexShift = 1;
        break;
    }

    if ((u32)(shift + 0x10000) != 0xffff) {
        tex = fn_800C7558(shift);
        if (tex == 0) {
            fn_800C1120(0);
            tex = fn_800C7558(shift);
        }
    }

    if (tex != 0 && isTexShift != 0) {
        tex = pbSendObjTexturesSub(obj->index >> 16, (PBRomObjectView*)obj->data.romobj);
        if (tex == 0) {
            tex = 1;
            fn_800C1120(0);
            if ((u32)(shift + 0x10000) != 0xffff) {
                tex = fn_800C7558(shift);
            }
            if (tex != 0) {
                tex = pbSendObjTexturesSub(obj->index >> 16, (PBRomObjectView*)obj->data.romobj);
            }
            if (tex == 0) {
                FatalError(lbl_801167B0, 0x800000);
            }
        }
    }

    pbDebugObjEnd(obj, 3);
    return tex;
}

/* Confirm every texture referenced by an object def is resident. */
static int pbSendObjTexturesSub(int idx, PBRomObjectView* def)
{
    PBSubObject* list;
    int tt;
    int hi;
    int count;
    PBObjSlot* t;

    t = (PBObjSlot*)gWinGlobals->dbg2;
    if (t[idx + 1].f0 != 0) {
        return 1;
    }
    if (def->SubObjCnt == 0) {
        return 1;
    }
    count = def->SubObjCnt;
    tt = def->SubObj0_TexIdx;
    hi = idx << 16;
    list = def->SubObjPtr;
    do {
        if (tt < 0) {
            tt = 0;
        }
        tt |= hi;
        if (fn_800C7558(tt) == 0) {
            return 0;
        }
        tt = list->TexIdx;
        list++;
    } while (--count != 0);
    return 1;
}

/* Interactive object-draw debug single-stepper. */
void pbDebugObjSStep(MBObject* obj, int state)
{
    char* names = lbl_801165B8;
    void** nameTab;
    u32 t0;
    int held;

    switch (state) {
    case 2:
        if (lbl_80343F40->state < 4) {
            return;
        }
    case 3:
        if (lbl_80343F40->state < 3) {
            return;
        }
    case 4:
        if (lbl_80343F40->state < 2) {
            return;
        }
    case 5:
        break;
    }

    t0 = pbGetTime();
    nameTab = &lbl_80128190[state];
    for (;;) {
        do {
            held = fn_800C1148(1, 0, names + 0x218);
            if (held == 0) {
                return;
            }
        } while ((u32)(pbGetTime() - t0) <= 0x23C34600);
        t0 = pbGetTime();
        bulletproof_printf(names + 0x230);
        bulletproof_printf(names + 0x24c,
                           (state <= 6) ? *(char**)nameTab : "???");
        bulletproof_printf(names + 0x25c, obj);
        bulletproof_printf(names + 0x270, obj->index);
        if (((PBRomObjectView*)obj->data.romobj)->ObjDef != 0) {
            bulletproof_printf(names + 0x284,
                               (char*)((PBRomObjectView*)obj->data.romobj)->ObjDef);
        }
        bulletproof_printf(names + 0x294);
        bulletproof_printf(names + 0x2ac, (held & 0x10) ? "busy" : "idle");
        bulletproof_printf(names + 0x2bc, (held & 0x08) ? "busy" : "idle");
        bulletproof_printf(names + 0x2cc, (held & 0x04) ? "busy" : "idle");
        bulletproof_printf(names + 0x2dc, (held & 0x02) ? "busy" : "idle");
        bulletproof_printf(names + 0x2ec, (held & 0x01) ? "busy" : "idle");
        bulletproof_printf(names + 0x2fc);
    }
}
