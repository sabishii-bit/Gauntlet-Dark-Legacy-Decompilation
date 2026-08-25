/* pb_winglobals.c -- Midway "pb" window-list / marker TU (provisional
 * identity; all fns keep fn_ names -- several are referenced by name from
 * the Matching pb_global.c).
 *
 * Manages the 12-slot window/marker pool hanging off gWinGlobals->ctx
 * (default backing store lbl_802C4750, 0x650 bytes = 0x28C header +
 * 12 * 0x50 entries): alloc (fn_800C0ADC) / free (fn_800C0BD4) with a
 * free list at ctx+0xB8 and an active list at ctx+0xB4, plus the
 * projection-defaults block (gWinGlobals->proj = lbl_802C4DA0) and the
 * per-frame texture-timestamp flip (fn_800C1004).
 *
 * .text 0x800C0ADC-0x800C1174. Compiled -Cpp_exceptions on (cflags_demo).
 */

#include "types.h"

void __as__4vec4FRC4vec4(f32* dst, f32* src); /* vec4::operator= */
void mbBlitStub51E0(void);
void pbResetDORegs(void);
void Deci2Call();

/* pool entry, 0x50 bytes */
typedef struct PbWGWin {
    f32 v0[4];             /* 0x00 : vec4 */
    f32 v1[4];             /* 0x10 : vec4 */
    f32 f20;               /* 0x20 */
    f32 f24;               /* 0x24 */
    struct PbWGWin* next;  /* 0x28 */
    u32 id;                /* 0x2C */
    u8  _pad30[0x20];
} PbWGWin;

/* window-pool context (*gWinGlobals->ctx; default = lbl_802C4750) */
typedef struct PbWGCtx {
    u8   _pad00[0x78];
    f32  f78;              /* 0x78 */
    s32  m7c;              /* 0x7C */
    f32  f80, f84, f88, f8c, f90, f94, f98; /* 0x80 */
    s32  m9c;              /* 0x9C */
    s32  ma0;              /* 0xA0 */
    f32  fa4, fa8, fac, fb0;                /* 0xA4 */
    PbWGWin* active;       /* 0xB4 */
    PbWGWin* free;         /* 0xB8 */
    f32  fbc, fc0, fc4, fc8, fcc, fd0, fd4, fd8; /* 0xBC */
    u8   _paddc[0x1b0];
    PbWGWin wins[12];      /* 0x28C */
} PbWGCtx;

/* projection defaults block (*gWinGlobals->proj; default = lbl_802C4DA0) */
typedef struct PbWGProj {
    f32 f00;               /* 0x00 */
    f32 f04;               /* 0x04 */
    s32 m08;               /* 0x08 */
    s32 m0c;               /* 0x0C */
    u32 m10;               /* 0x10 */
    u32 m14;               /* 0x14 */
} PbWGProj;

/* texture bank views (shared shape with pb_objregs/pb_texture) */
typedef struct PbWGBank {
    u8   _pad00[0x48];
    s32  nslots;           /* 0x48 */
    u8   _pad4c[0x2c];
    u8*  stamps;           /* 0x78 : 8 bytes per slot */
} PbWGBank;

typedef struct PbWGBankRef {   /* 16 bytes */
    s32 m0;                /* 0x00 : count (in refs[0]) / busy flag */
    PbWGBank* bank;        /* 0x04 */
    u8  _pad8[8];
} PbWGBankRef;

/* *gWinGlobals view used by this TU */
typedef struct PbWGGlobals {
    u8    _pad00[0x1c];
    PbWGCtx* ctx;          /* 0x1C */
    u8    _pad20[0x08];
    u32* volatile hook28;  /* 0x28 */
    u8    _pad2c[0x04];
    PbWGBankRef* banks;    /* 0x30 */
    u8    _pad34[0x04];
    PbWGProj* volatile proj; /* 0x38 */
} PbWGGlobals;

extern PbWGGlobals* gWinGlobals;

extern s32 lbl_80343EE0;       /* window id counter */
extern u32 lbl_80343F04;       /* projection default +0x10 */
extern u32 lbl_80343F0C;       /* projection default +0x14 */
extern s32 lbl_80343F78;       /* texture-stamp flip index */
extern s32 lbl_80343F7C;       /* previous flip index */
extern u8 lbl_802C4750[];      /* default window ctx (0x650) */
extern u32 lbl_802C4DA0[];     /* default projection block */
extern u32 lbl_80344F88[2];    /* default hook28 block */
extern u32 lbl_803450F4[];     /* per-flip source values */
extern u32 lbl_803450FC[];     /* per-flip current values */

s32 fn_800C0BD4(u32 sel);
void pbResetWindowPool(void);
void fn_800C0E0C(void);
void fn_800C1004(void);

/* Allocate a window/marker from the free list (recycling the oldest if
 * empty), assign it a rolling id, and record its two vectors + uv. */
s32 fn_800C0ADC(f32* a, f32* b, f32 c, f32 d)
{
    PbWGGlobals* g = gWinGlobals;
    PbWGWin** head;
    PbWGWin* w;
    u32 id;

    if (g->ctx->free == 0) {
        fn_800C0BD4(0);
    }
    head = &g->ctx->free;
    w = *head;
    *head = w->next;
    lbl_80343EE0 = lbl_80343EE0 - 1;
    if (lbl_80343EE0 == 0) {
        lbl_80343EE0 = 0xFFF1;
    }
    id = (lbl_80343EE0 << 16) + (s32)((u8*)w - ((u8*)g->ctx + 0x28C)) / 80;
    w->id = id;
    w->next = g->ctx->active;
    g->ctx->active = w;
    __as__4vec4FRC4vec4(w->v0, b);
    __as__4vec4FRC4vec4(w->v1, a);
    w->f20 = c;
    w->f24 = d;
    return id;
}

/* Release a window by id (or the oldest active one when sel == 0) back
 * onto the free list. */
s32 fn_800C0BD4(u32 sel)
{
    PbWGGlobals* g = gWinGlobals;
    PbWGWin* t;
    PbWGWin** head;
    PbWGWin* cur;
    PbWGWin* prev;

    if (sel != 0) {
        PbWGCtx* ctx;
        if ((sel & 0xFFFF) > 12) {
            return 1;
        }
        ctx = g->ctx;
        t = (PbWGWin*)((u8*)ctx + (sel & 0xFFFF) * 0x50 + 0x28C);
        if (t->id != sel) {
            return 1;
        }
        head = (PbWGWin**)((u8*)ctx + 180);
        if (*head == t) {
            *head = t->next;
        } else {
            cur = *head;
            while (cur != 0 && cur->next != t) {
                cur = cur->next;
            }
            if (cur == 0) {
                return 1;
            }
            cur->next = t->next;
        }
    } else {
        PbWGWin* w = *(head = &g->ctx->active);
        if (w != 0) {
            t = w->next;
            if (t == 0) {
                *head = 0;
                t = w;
            } else {
                while (t->next != 0) {
                    w = t;
                    t = t->next;
                }
                w->next = 0;
            }
        } else {
            return 1;
        }
    }
    t->id = 0;
    t->next = g->ctx->free;
    g->ctx->free = t;
    return 0;
}

/* Reset all 12 pool entries and rebuild the free list (reversed). */
void pbResetWindowPool(void)
{
    PbWGGlobals* g = gWinGlobals;
    s32 i;
    u32 prev;

/* flat word-indexed view of the ctx page: word 163 = wins[0].v0[0] (0x28C).
 * The constant lives in the SUBSCRIPT so it folds into the INDEX register
 * (addi off+652 / stfsx), and every statement re-derefs g->ctx (pointer
 * stores force the reload) -- the target's exact shape. */
#define CTXF ((f32*)g->ctx)
#define CTXW ((u32*)g->ctx)
    prev = 0;
    for (i = 0; i < 240; i += 20) {
        CTXF[i + 163] = 1.0f;
        CTXF[i + 164] = 1.0f;
        CTXF[i + 165] = 1.0f;
        CTXF[i + 166] = 0.0f;
        CTXF[i + 167] = 0.0f;
        CTXF[i + 168] = 0.0f;
        CTXF[i + 169] = 0.0f;
        CTXF[i + 170] = 0.0f;
        CTXF[i + 171] = 0.0f;
        CTXF[i + 172] = 1.0f;
        CTXW[i + 173] = prev;
        CTXW[i + 174] = 0;
        prev = (u32)((u8*)g->ctx + i * 4 + 652);
    }
    g->ctx->free = (PbWGWin*)prev;
    g->ctx->active = 0;
    g->ctx->ma0 = 0;
#undef CTXF
#undef CTXW
}

/* Set the current window's first UV pair. */
void pbSetWindowUV0(f32 a, f32 b)
{
    PbWGGlobals* g = gWinGlobals;
    g->ctx->f80 = a;
    g->ctx->f84 = b;
}

/* Set the current window's second UV pair. */
void pbSetWindowUV1(f32 a, f32 b)
{
    PbWGGlobals* g = gWinGlobals;
    g->ctx->f88 = a;
    g->ctx->f8c = b;
}

/* -2048.0f lives in an 8-byte .sdata2 slot in the original (4B zero pad). */
const struct Neg2048 {
    f32 v;
    f32 pad;
} lbl_80348F00 = { -2048.0f, 0.0f };

/* second 1.0f pair object (does not dedup with the scalar literal pool) */
const struct Neg2048 lbl_80348F08 = { 1.0f, 0.0f };

/* Reset the whole window context to defaults. */
void fn_800C0E0C(void)
{
    PbWGGlobals* g = gWinGlobals;

    g->ctx->f78 = lbl_80348F00.v;
    g->ctx->m9c = 0;
    g->ctx->fa4 = 1.0f;
    g->ctx->fa8 = 1.0f;
    g->ctx->fac = 1.0f;
    g->ctx->fb0 = 0.0f;
    g->ctx->fbc = 1.0f;
    g->ctx->fc0 = 1.0f;
    g->ctx->fc4 = 1.0f;
    g->ctx->fc8 = 0.0f;
    g->ctx->fcc = 1.0f;
    g->ctx->fd0 = 0.0f;
    g->ctx->fd4 = 0.0f;
    g->ctx->fd8 = 0.0f;
    pbResetWindowPool();
    g->ctx->f84 = 0.0f;
    g->ctx->f80 = 1.0f;
    g->ctx->f8c = 0.0f;
    g->ctx->f88 = 1.0f;
    g->ctx->f94 = 0.0f;
    g->ctx->f90 = 1.0f;
    g->ctx->f98 = 1.0f;
    g->ctx->m7c = 1;
}

/* Install the default window ctx if unset, then reset it
 * (referenced from pb_global.c -- keep fn_). */
void fn_800C0F04(void)
{
    PbWGGlobals* g = gWinGlobals;
    if (g->ctx == 0) {
        g->ctx = (PbWGCtx*)lbl_802C4750;
    }
    fn_800C0E0C();
}

/* Force the default window ctx and reset it
 * (referenced from pb_global.c -- keep fn_). */
void fn_800C0F40(void)
{
    gWinGlobals->ctx = (PbWGCtx*)lbl_802C4750;
    fn_800C0E0C();
}

/* Install the default projection block if unset
 * (referenced from pb_global.c -- keep fn_). */
void fn_800C0F70(void)
{
    PbWGGlobals* g = gWinGlobals;
    if (g->proj) {
        return;
    }
    g->proj = (PbWGProj*)lbl_802C4DA0;
}

/* Force the default projection block and reset it
 * (referenced from pb_global.c -- keep fn_). */
void fn_800C0F90(void)
{
    PbWGGlobals* g = gWinGlobals;
    f32 one;
    g->proj = (PbWGProj*)lbl_802C4DA0;
    one = lbl_80348F08.v;
    g->proj->f00 = one;
    g->proj->f04 = one;
    g->proj->m08 = 0x800;
    g->proj->m0c = 0x800;
    g->proj->m10 = lbl_80343F04;
    g->proj->m14 = lbl_80343F0C;
}

void fn_800C0FE4(void)
{
}

/* Install the default hook28 block if unset
 * (referenced from pb_global.c -- keep fn_). */
void fn_800C0FE8(void)
{
    PbWGGlobals* g = gWinGlobals;
    if (g->hook28) {
        return;
    }
    g->hook28 = lbl_80344F88;
}

/* Advance the per-frame texture-stamp flip and clear the matching bit
 * out of every loaded bank's slot stamps (u64 per slot). */
void fn_800C1004(void)
{
    PbWGGlobals* g = gWinGlobals;
    s32 t;
    s32 i;

    t = lbl_80343F78 + 1;
    lbl_80343F7C = lbl_80343F78;
    lbl_80343F78 = t;
    if (t >= 2) {
        lbl_80343F78 = 0;
    }
    if (lbl_80343F78 >= 0) {
        u32 b = 1 << lbl_80343F78;
        u32 m;
        u32 mm[2];
        u64 mk;
        s32 off;

        m = ~(((b & 0xFF) << 24) | ((b & 0xFF) << 16) |
              ((b & 0xFF) << 8) | (b & 0xFF));
        mm[0] = m;
        mm[1] = mm[0];
        mk = *(u64*)mm;
        for (i = 0, off = 0; i < g->banks[0].m0; i++, off += 0x10) {
            s32* p = (s32*)((u8*)g->banks + off);
            s32 busy = p[4];
            s32* q = p + 1;
            if (busy == 0) {
                PbWGBank* bank = *(PbWGBank**)q;
                u32 nslots = bank->nslots;
                if (nslots != 0) {
                    u8* stamps = bank->stamps;
                    s32 so = 0;
                    s32 k;
                    for (k = (nslots + 7) >> 3; k > 0; k--) {
                        *(u64*)(stamps + so) &= mk;
                        so += 8;
                    }
                }
            }
        }
    }
    lbl_803450FC[lbl_80343F78] = lbl_803450F4[lbl_80343F78];
}

/* Per-frame window maintenance driver. */
void fn_800C1120(void)
{
    mbBlitStub51E0();
    pbResetDORegs();
    fn_800C1004();
}

/* Deci2Call(unused, (u16)flags, data). */
#pragma peephole on
void fn_800C1148(s32 unused, s32 flags, void* data)
{
    Deci2Call(unused, (u16)flags, data);
}
#pragma peephole off

void fn_800C116C(void)
{
}

void fn_800C1170(void)
{
}
