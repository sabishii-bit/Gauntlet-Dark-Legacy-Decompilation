#include "types.h"

#pragma dont_inline on

/* pb_objects.c -- Midway "pb" graphics library object layer (pb_objects.obj on
 * Xbox). .text 0x800C3674-0x800C3F58. Sits between pb_global.c (below) and
 * pb_objregs.c (above) in the PB C++ library run; compiled cflags_demo
 * (-O4 no-peephole, -Cpp_exceptions on, -str reuse,readonly), GC/1.2.5n.
 *
 * The GameCube build of this TU differs from the Xbox pb_objects.obj: the
 * GX/DEMO port keeps the object push-buffer allocator + module bring-up/reset
 * hooks (called by pb_global.c/pb_window.c/mb_*), the object draw dispatch
 * (pbDrawObject), the texture upload path (pbSendObjTextures) and the
 * interactive object-debug single-stepper (pbDebugObjSStep); the PC-only debug
 * helpers (pbObjectBsearch/printSelObj/pbDebugObjStart/End/calcObjCnt) are
 * dead-stripped.
 *
 * Function names: pbSendObjTextures / pbDebugObjSStep from shell3D.pdb and the
 * "NO-ObjDef" / "Obj Textures Larger than a page" / "PB_ODB_*" strings;
 * pbObjTexSub / pbSendObjTexturesSub are the two TU-local statics. The push-
 * buffer allocator + bring-up hooks stay fn_<addr> because pb_global.c (already
 * Matching) and pb_window/mb_* reference them by that name.
 *
 * NonMatching: bodies are reconstructions from the disassembly; several small
 * ones are byte-exact (see the worker report). Data is declared extern (defined
 * by the DOL); a future flip must add the .sdata/.sdata2/.rodata/.data/.bss
 * claims documented in the report.
 */

/* --- shared pb-global block (see pb_global.c) --- */
typedef struct PBObjPool {
    void* buf;        /* 0x00 : aligned scratch buffer (CreateSema result) */
    u8** freehead;    /* 0x04 : head of the 0x800-stride free list */
} PBObjPool;

typedef struct PBGlobal {
    u8 _pad00[0x2c];
    PBObjPool* objPool; /* 0x2c : object push-buffer pool control */
    void* dbg2;         /* 0x30 : secondary object block (0x10-stride array) */
} PBGlobal;

extern PBGlobal* gWinGlobals; /* 0x80344FC0 */

/* --- pb_objects private data (defined in the DOL) --- */
extern s16 lbl_80345018;       /* per-frame object counter (reset each frame) */
extern int lbl_80345020;       /* "objects module open" flag */
extern int lbl_80345024;       /* ping-pong buffer toggle */
extern int lbl_80345028;       /* alloc mode: 0 = ping-pong, else free-list */
extern int lbl_80343F28;       /* pooled scratch buffer ptr (init -1) */
extern void* lbl_80343F2C;     /* { buf0, buf1, 0 } ping-pong buffers (SDA21) */
extern int lbl_80343F3C;       /* draw-hook enable flag (init 1) */

extern u8 lbl_802C52C0[0x18];  /* PBObjPool storage */
extern u8 lbl_802C52D8[0x158]; /* secondary object block storage */
extern u8 lbl_802913C0[0x2000];/* object push-buffer pool (8 x 0x800 nodes) */
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
    int state;      /* 0x00 : current PB_ODB_* state */
    u8 _pad04[0x30];
    int step;       /* 0x34 : last stepped state */
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
extern void WaitSema(void* sema);
extern void DIntr(void);
extern void EIntr(void);
extern void* CreateSema(void* param);

extern void pbSetupPosLights(f32 extra, int, void*, void*); /* pb_objregs pos-light setup */
extern void fn_800C5598(int, u32, int, int, u32, int, void*, void*, void*); /* pb_objregs primitive emit */
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
int fn_800C38C0(void* a, u8* obj);
static u32 pbObjTexSub(void* obj, int lo, int hi, u32* flags);
int pbSendObjTextures(u8* obj);
static int pbSendObjTexturesSub(int idx, u8* def);
void pbDebugObjSStep(u8* obj, int state);

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
        u8** head;
        WaitSema(g->objPool->buf);
        DIntr();
        head = g->objPool->freehead;
        g->objPool->freehead = (u8**)head[0];
        EIntr();
        return head;
    }
}

/* Rebuild the object push-buffer free list (8 x 0x800-byte nodes). */
void fn_800C36F8(void)
{
    PBGlobal* g;
    u8* base;
    int i;

    g = gWinGlobals;
    if (lbl_80343F28 == -1) {
        lbl_80343F28 = (int)CreateSema(lbl_80128178);
    }
    g->objPool->buf = (void*)lbl_80343F28;
    g->objPool->freehead = (u8**)lbl_802913C0;
    for (i = 0; i < 7; i++) {
        base = (u8*)g->objPool->freehead;
        *(u8**)(base + i * 0x800) = base + (i + 1) * 0x800;
    }
    base = (u8*)g->objPool->freehead;
    *(u8**)(base + 0x3800) = 0;
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

/* Full bring-up hook: attach the pool control block and build the free list. */
void fn_800C37C4(void)
{
    PBGlobal* g;
    u8* base;
    int i;
    u8 unused[8];

    gWinGlobals->objPool = (PBObjPool*)lbl_802C52C0;
    g = gWinGlobals;
    if (lbl_80343F28 == -1) {
        lbl_80343F28 = (int)CreateSema(lbl_80128178);
    }
    g->objPool->buf = (void*)lbl_80343F28;
    g->objPool->freehead = (u8**)lbl_802913C0;
    for (i = 0; i < 7; i++) {
        base = (u8*)g->objPool->freehead;
        *(u8**)(base + i * 0x800) = base + (i + 1) * 0x800;
    }
    base = (u8*)g->objPool->freehead;
    *(u8**)(base + 0x3800) = 0;
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
int fn_800C38C0(void* a, u8* obj)
{
    u8* def;
    int hi;
    u8* v1c;
    int tex;
    int v25;
    int v24;
    u8 unusedA[4];
    u32 packed;
    int pcount;
    u8* prim;
    int stride;
    PBObjSlot* t;
    u32 flags;
    u8 unusedB[4];
    PBGlobal* g = gWinGlobals;

    packed = *(u32*)(obj + 0x6c);
    if (packed == 0) {
        return 0;
    }
    hi = packed >> 16;
    t = (PBObjSlot*)g->dbg2;
    def = *(u8**)(obj + 0x70);
    if (t[(packed >> 16) + 1].f0 != 0) {
        return 0;
    }
    if (*(int*)(def + 0xc) == 0) {
        return 0;
    }
    flags = *(u32*)(obj + 0x60) & 0x1090D7C0;
    tex = pbObjTexSub(obj, *(u16*)(def + 0x12), hi, &flags);
    v25 = *(s16*)(def + 0x16);
    v1c = *(u8**)(def + 0x1c);
    v24 = *(u16*)(def + 0x14);
    if (*(u32*)(def + 8) & 0x100) {
        flags |= 0x20000;
    }
    if (flags & 0x8000) {
        flags |= 0x20000;
    }
    if (lbl_80343F3C != 0) {
        pbSetupPosLights(*(f32*)(def + 4), 0, obj, a);
    }
    fn_800C5598(0, tex, v25, v24, flags, hi, a, v1c, obj);
    pcount = *(int*)(def + 0xc) - 1;
    if (pcount != 0) {
        prim = *(u8**)(def + 0x18);
        stride = *(u16*)(def + 0x10);
        do {
            u32 tx;
            v1c += stride << 4;
            tx = pbObjTexSub(obj, *(u16*)(prim + 2), hi, &flags);
            stride = *(u16*)(prim + 0);
            fn_800C5598(0, tx, *(s16*)(prim + 6), *(u16*)(prim + 4), flags, hi,
                        0, v1c, 0);
            prim += 8;
        } while (--pcount != 0);
    }
    return 0;
}

/* Resolve a texture-shift descriptor into a packed tex address / flag word. */
static u32 pbObjTexSub(void* objv, int lo, int hi, u32* flags)
{
    u8* obj = (u8*)objv;
    s16 t = *(s16*)(obj + 0x5c);

    *flags &= ~0x00080000;
    switch (t) {
    case -2:
        return *(u32*)(obj + 0x58);
    case -1:
        return (hi << 16) | (lo & 0xffff);
    case -4:
        *flags |= 0x08000000;
        return (hi << 16) | (lo & 0xffff);
    case -3:
        *flags |= 0x00080000;
        return *(u32*)(obj + 0x58);
    default:
        if (lo == t) {
            return *(u32*)(obj + 0x58);
        }
        return (hi << 16) | (lo & 0xffff);
    }
}

/* Upload an object's textures, retrying once via a cache flush; fatal if the
 * texture set will not fit a page. */
int pbSendObjTextures(u8* obj)
{
    int tex = 1;
    int shift = -1;
    int isTexShift;

    lbl_80343F40->step = 2;
    lbl_80343F40->obj = obj;
    lbl_80343F40->defName = *(char**)(*(u8**)(obj + 0x70) + 0x2c)
                                ? *(char**)(*(u8**)(obj + 0x70) + 0x2c)
                                : lbl_801167A4;
    if (lbl_80343F40->state != 0) {
        pbDebugObjSStep(obj, 2);
    }

    switch (*(s16*)(obj + 0x5c)) {
    case -1:
        isTexShift = 1;
        break;
    case -2:
        shift = *(int*)(obj + 0x58);
        isTexShift = 0;
        break;
    case -4:
        shift = *(int*)(obj + 0x58);
        isTexShift = 1;
        break;
    case -3:
        shift = *(int*)(obj + 0x58);
        isTexShift = 0;
        break;
    default:
        shift = *(int*)(obj + 0x58);
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
        tex = pbSendObjTexturesSub(*(u32*)(obj + 0x6c) >> 16, *(u8**)(obj + 0x70));
        if (tex == 0) {
            tex = 1;
            fn_800C1120(0);
            if ((u32)(shift + 0x10000) != 0xffff) {
                tex = fn_800C7558(shift);
            }
            if (tex != 0) {
                tex = pbSendObjTexturesSub(*(u32*)(obj + 0x6c) >> 16, *(u8**)(obj + 0x70));
            }
            if (tex == 0) {
                FatalError(lbl_801167B0, 0x800000);
            }
        }
    }

    lbl_80343F40->step = 3;
    if (lbl_80343F40->state != 0) {
        pbDebugObjSStep(obj, 3);
    }
    return tex;
}

/* Confirm every texture referenced by an object def is resident. */
static int pbSendObjTexturesSub(int idx, u8* def)
{
    u8* list;
    int tt;
    int hi;
    int count;
    PBObjSlot* t;

    t = (PBObjSlot*)gWinGlobals->dbg2;
    if (t[idx + 1].f0 != 0) {
        return 1;
    }
    if (*(int*)(def + 0xc) == 0) {
        return 1;
    }
    count = *(int*)(def + 0xc);
    tt = *(u16*)(def + 0x12);
    hi = idx << 16;
    list = *(u8**)(def + 0x18);
    do {
        if (tt < 0) {
            tt = 0;
        }
        tt |= hi;
        if (fn_800C7558(tt) == 0) {
            return 0;
        }
        tt = *(u16*)(list + 2);
        list += 8;
    } while (--count != 0);
    return 1;
}

/* Interactive object-draw debug single-stepper. */
void pbDebugObjSStep(u8* obj, int state)
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
        bulletproof_printf(names + 0x270, *(void**)(obj + 0x6c));
        if (*(char**)(*(u8**)(obj + 0x70) + 0x2c) != 0) {
            bulletproof_printf(names + 0x284,
                               *(char**)(*(u8**)(obj + 0x70) + 0x2c));
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
