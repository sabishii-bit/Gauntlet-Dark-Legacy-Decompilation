#include "types.h"
#include "dolphin/gx.h"
#include "dolphin/pad.h"
#include "MWCPlusLib.h"
#include "NMWException.h"

/* Midway "MB" 2D blit / sprite library (GCN MB_BLIT.OBJ region,
 * .text 0x800B2988-0x800B53B0). Draws textured quads ("blits") through the
 * GX pipeline: blits are allocated from a fixed pool, linked into a render
 * node's list, positioned in screen space via the active pb window, then
 * emitted each frame by MBDrawBlits (textured) / DrawBlitFlatQuad (solid).
 *
 * Xbox-PDB names used where confident from string + call evidence:
 *   MBNewBlit, MBCreateBlit, MBInitBlits, MBResetBlits, MBRemoveBlit,
 *   MBBlitOrder, MBNewTempBlit, MBNewTempQuad, MBDrawBlits, DrawBlit,
 *   DrawBlitFlatQuad, pbBlitSetDrawRegs, pbBlitSetTexture.
 * mb*-prefixed lowercase names are descriptive (exact Midway identifier
 * unconfirmed): the coordinate-projection helpers, the g3dcolor C++
 * ctor/dtor cluster, and a handful of small setters.
 *
 * Status: NonMatching (the DOL still links the original object). Allocation,
 * linked-list management, coordinate conversion, window rescaling, draw-list
 * orchestration, animated lighting, render-state caching, and texture binding
 * are translated. The two raw GX vertex emitters (DrawBlitFlatQuad/DrawBlit)
 * remain as the principal untranslated paths. The whole MB_BLIT TU actually
 * begins at fn_800B27C4 (a timer helper below this region); this file covers
 * 0x800B2988 upward.
 *
 * Key data (see symbols.txt):
 *   blitPool[384]      @0x80296478  main blit pool (0x38 each)
 *   tempBlitPool[32]   @0x8029B878  per-frame temp blits
 *   tempQuadPool[256]  @0x8029BF78  per-frame temp quads (0x24 each)
 *   blitCount/tempBlitCount/tempQuadCount   high-water counters
 *   defaultBlitList    @0x80344DE4  default render node
 */

#define MB_BLIT_POOL_MAX   384
#define MB_TEMPBLIT_MAX    32
#define MB_TEMPQUAD_MAX    256

/* One 2D blit. Size 0x38. */
typedef struct MBBLIT {
    /* 0x00 */ s32 flags;      /* bit1 (0x2) live/removed marker, bit6 (0x40) */
    /* 0x04 */ s32 tex;        /* texture handle, init -1 */
    /* 0x08 */ s16 x;          /* screen x (12.4 or pixel) */
    /* 0x0A */ s16 y;          /* screen y */
    /* 0x0C */ u32 depth;      /* z / scale, from float via __cvt_fp2unsigned */
    /* 0x10 */ s16 width;
    /* 0x12 */ s16 height;
    /* 0x14 */ s16 u0;
    /* 0x16 */ s16 u1;
    /* 0x18 */ s16 v0;
    /* 0x1A */ s16 v1;
    /* 0x1C */ u32 color0;     /* four RGBA corner colors, init 0x80808080 */
    /* 0x20 */ u32 color1;
    /* 0x24 */ u32 color2;
    /* 0x28 */ u32 color3;
    /* 0x2C */ struct MBBLIT* prev;
    /* 0x30 */ struct MBBLIT* next;
    /* 0x34 */ void* node;     /* owning render node */
} MBBLIT;

/* A render node owns a doubly-linked list of blits at +0x70. */
typedef struct MBNODE {
    u8 _pad0[0x60];
    /* 0x60 */ u32 flags;
    u8 _pad64[8];
    /* 0x6C */ s32 unk6C;
    /* 0x70 */ MBBLIT* blits;
} MBNODE;

typedef struct MBTextureDef {
    u8 _pad00[2];
    u16 flags;
    u8 _pad04[6];
    u16 width;
    u16 height;
} MBTextureDef;

typedef struct MBScale {
    f32 x;
    f32 y;
    u8 _pad08[8];
    s32 viewport0;
    s32 viewport1;
} MBScale;

typedef struct MBWindow {
    u8 _pad00[0x38];
    MBScale* scale;
} MBWindow;

typedef struct PBBlendState {
    u8 group : 2;
    u8 source : 2;
    u8 _unused : 2;
    u8 target : 2;
    u8 _pad01[15];
} PBBlendState;

/* ---- externs ---- */
extern void FatalError(const char* msg, int code);
extern void ErrorPrintf(const char* fmt, ...);
extern void* memset(void* p, int c, u32 n);

extern void* fn_800BB29C(int a, const char* name, int b);   /* render-node alloc */
extern int fn_800B8B34(int name, int a, int b);              /* texture-by-name lookup */

extern MBBLIT blitPool[MB_BLIT_POOL_MAX];
extern u8 tempBlitPool[MB_TEMPBLIT_MAX * 0x38];
extern u8 tempQuadPool[MB_TEMPQUAD_MAX * 0x24];
extern s32 blitCount;
extern s32 tempBlitCount;
extern s32 tempQuadCount;
extern s32 lbl_80344DF8;
extern s32 lbl_80344E00;
extern s32 lbl_80344E04;
extern MBNODE* defaultBlitList;
extern MBNODE* gDiag_DEC;
extern MBNODE* gDiag_DE8;
extern s32 lbl_80343EA4;
extern s32 lbl_80343EA8;
extern s32 lbl_80343EAC;
extern f32 lbl_80343EA0;
extern u8 lbl_80345104;
extern f32 lbl_80344DD8;
extern f32 lbl_80344DDC;
extern f32 lbl_80344DE0;
extern u32 pbLoad;
extern s32 lbl_802C29F8[12];

extern const char str_TooManyTempBlits[];
extern const char str_TooManyQuads[];
extern const char str_BlitOrderDiffNodes[];
extern const char str_TooManyBlits[];
extern const char str_DrawBlitNoTex[];
extern void fn_800C36F8(void);
extern void fn_800C5B3C(void);
extern void fn_800C1120(s32);
extern int fn_800C7558(s32 texture);
extern int fn_800C780C(s32 id, s32 index, u32 mask);
extern void fn_800C7928(void* texture, s32);
extern f32 floorf(f32 value);
extern PADStatus* G3DGetPadStatusBuffer(void);
extern void __dl__FPv(void* object);
extern u8 lbl_80296450[];
extern MBTextureDef* MBRomTexPtr(s32 texture);
extern u32 __cvt_fp2unsigned(f32 value);
extern MBWindow* gWinGlobals;
extern const f32 lbl_80348AD0;
extern const f32 lbl_80348AA0;

/* internal helpers (defined below / same TU) */
static u32 mbInitBlitEntry(MBBLIT* b, int arg, int z);
static void mbBlitProject(MBBLIT* b, int a, int c);
static void mbBlitSetupVerts(MBBLIT* b, f32 u0, f32 u1, f32 v0, f32 v1);
void DrawBlit(MBBLIT* b);
void DrawBlitFlatQuad(void* q);
void mbBlitCalcRect(MBBLIT* b, s32* x, s32* y, f32* depth);
void mbBlitCalcWidth(MBBLIT* b, s32 x, s32 y, f64 depth);
void mbBlitCvtCoord(MBBLIT* b, f64 depth);
void mbBlitCalcY(MBBLIT* b, s32 y);
void mbBlitCalcClip(MBBLIT* b, f32 xScale, f32 yScale);
void mbBlitCalcX(MBBLIT* b, s32* width, s32* height);

void mbBlitCalcRect(MBBLIT* b, s32* x, s32* y, f32* depth) {
    MBWindow* window = gWinGlobals;
    f32 round;
    s32 coord;
    s32 value;

    if (x != 0) {
        coord = b->x;
        if (coord >= 0) {
            round = 0.5;
        } else {
            round = -0.5;
        }
        if ((b->flags & 0x40) != 0) {
            value = coord << 4;
        } else {
            value = (s32)(round + (f32)coord / window->scale->x);
        }
        *x = value;
    }
    if (y != 0) {
        coord = b->y;
        if (coord >= 0) {
            round = 0.5;
        } else {
            round = -0.5;
        }
        if ((b->flags & 0x40) != 0) {
            value = coord << 4;
        } else {
            value = (s32)(round + (f32)coord / window->scale->y);
        }
        *y = value;
    }
    if (depth != 0) {
        *depth = (f32)((f64)(f32)b->depth * 0.03125);
    }
}

void mbBlitCalcWidth(MBBLIT* b, s32 x, s32 y, f64 depth) {
    MBWindow* window = gWinGlobals;
    s16 value;

    if ((b->flags & 0x40) != 0) {
        value = x << 4;
    } else {
        value = (s16)(x * window->scale->x);
    }
    b->x = value;
    if ((b->flags & 0x40) != 0) {
        value = y << 4;
    } else {
        value = (s16)(y * window->scale->y);
    }
    b->y = value;
    if (depth >= 0.0) {
        b->depth = (u32)(f32)(s32)(32.0 * depth);
    }
}

void mbBlitCvtCoord(MBBLIT* b, f64 depth) {
    if (depth >= 0.0) {
        b->depth = (u32)(f32)(s32)(32.0 * depth);
    } else {
        b->depth = (s32)depth + 0x1000000;
    }
}

void mbBlitCalcY(MBBLIT* b, s32 y) {
    MBWindow* window = gWinGlobals;
    s32 value;

    if ((b->flags & 0x40) != 0) {
        value = y << 4;
    } else {
        value = (s32)(y * window->scale->y);
    }
    b->y = (s16)value;
}

void mbBlitCalcClip(MBBLIT* b, f32 xScale, f32 yScale) {
    s32 scaled;

    if (xScale > 0.0) {
        scaled = (s32)(0.5 + (f32)((f32)(u16)b->width * xScale));
        if ((b->flags & 0x1000) != 0) {
            b->x -= (s16)(0.5 * (scaled - (u16)b->width));
        }
        b->width = (s16)scaled;
    }
    if (yScale > 0.0) {
        scaled = (s32)(0.5 + (f32)((f32)(u16)b->height * yScale));
        if ((b->flags & 0x1000) != 0) {
            b->y -= (s16)(0.5 * (scaled - (u16)b->height));
        }
        b->height = (s16)scaled;
    }
}

#pragma dont_inline on
void mbBlitCalcX(MBBLIT* b, s32* width, s32* height) {
    if (width != 0) {
        if ((b->flags & 0x40) == 0) {
            *width =
                (s32)((f32)(u16)b->width / gWinGlobals->scale->x);
        } else {
            *width = (u16)b->width >> 4;
        }
    }
    if (height != 0) {
        if ((b->flags & 0x140) == 0) {
            *height =
                (s32)((f32)(u16)b->height / gWinGlobals->scale->y);
        } else {
            *height = (u16)b->height >> 4;
        }
    }
}
#pragma dont_inline off

s32 mbBlitReset33F8(MBBLIT* b) {
    if ((b->flags & 1) != 0) {
        return 1;
    }
    return 0;
}

void mbBlitInit3414(MBBLIT* b, s32 enabled) {
    if (enabled != 0) {
        b->flags |= 1;
    } else {
        b->flags &= ~1;
    }
}

s32 mbBlitStub343C(MBBLIT* b) {
    return b->flags;
}

u32 mbBlitUpdateEntry(MBBLIT* b, u32 keepMask, u32 setBits) {
    u32 oldFlags = b->flags;
    u32 newFlags = (oldFlags & keepMask) | setBits;
    u32 changed = oldFlags ^ newFlags;
    s16 swap;

    if (changed != 0) {
        if ((changed & 0x140) == 0) {
            b->flags = newFlags;
        } else {
            s32 x;
            s32 y;
            s32 width;
            s32 height;
            s32 value;
            MBWindow* window;

            mbBlitCalcRect(b, &x, &y, 0);
            mbBlitCalcX(b, &width, &height);
            b->flags = newFlags;
            window = gWinGlobals;
            if ((b->flags & 0x40) != 0) {
                value = x << 4;
            } else {
                value = (s32)(x * window->scale->x);
            }
            b->x = (s16)value;
            if ((b->flags & 0x40) != 0) {
                value = y << 4;
            } else {
                value = (s32)(y * window->scale->y);
            }
            b->y = (s16)value;
            if (lbl_80348AD0 >= 0.0f) {
                b->depth =
                    (u32)(f32)(s32)(32.0 * (f64)lbl_80348AD0);
            }
            mbBlitProject(b, width, height);
        }
        if ((changed & 0x20) != 0) {
            swap = b->u0;
            b->u0 = b->u1;
            b->u1 = swap;
        }
        if ((changed & 0x80) != 0) {
            swap = b->v0;
            b->v0 = b->v1;
            b->v1 = swap;
        }
        b->flags = newFlags;
    }
    return oldFlags;
}

/* =====================================================================
 * Blit creation / pool management
 * ===================================================================== */

/* Core allocator: find a free slot in blitPool, initialise it, link it into
 * node's list and place it at (x,y). Called by MBNewBlit / mbNewBlitSized. */
MBBLIT* MBCreateBlit(MBNODE* node, int tex, int x, int y, int w, int h) {
    MBBLIT* b;
    MBBLIT* p;
    int i;
    int slot;

    if (node == 0) {
        node = defaultBlitList;
    }

    /* find first slot at or below the high-water mark that is free */
    slot = 0;
    for (i = 0; i < blitCount; i++) {
        if ((blitPool[i].flags & 0x2) == 0) {
            break;
        }
        slot++;
    }
    if (slot >= MB_BLIT_POOL_MAX - 1) {
        FatalError(str_TooManyBlits, 0x800000);
        return 0;
    }
    if (slot >= blitCount) {
        blitCount = slot + 1;
    }
    b = &blitPool[slot];
    b->flags = 0;
    b->tex = -1;
    b->color0 = b->color1 = b->color2 = b->color3 = 0x80808080;
    b->prev = 0;
    b->next = 0;

    if (b == 0) {
        return 0;
    }

    /* append to node's blit list */
    if (node->blits != 0) {
        p = node->blits;
        while (p->next != 0) {
            p = p->next;
        }
        p->next = b;
        b->prev = p;
    } else {
        node->blits = b;
        b->prev = 0;
    }
    b->node = node;

    mbInitBlitEntry(b, tex, 0);
    if ((b->flags & 0x40) == 0) {
        b->x = (s16)(x * gWinGlobals->scale->x);
        b->y = (s16)(y * gWinGlobals->scale->y);
    } else {
        b->x = x << 4;
        b->y = y << 4;
    }
    b->depth = __cvt_fp2unsigned(32.0f);
    mbBlitProject(b, w, h);
    mbBlitSetupVerts(b, 0.0f, 1.0f, 0.0f, 1.0f);
    return b;
}

/* MBNewBlit(name, x, y): look up texture by name, create an auto-sized blit. */
MBBLIT* MBNewBlit(int name, int x, int y) {
    int tex = fn_800B8B34(name, 0, 1);
    return MBCreateBlit(0, tex, x, y, -1, -1);
}

/* MBNewBlit variant with explicit width/height. */
MBBLIT* mbNewBlitSized(int name, int x, int y, int w, int h) {
    int tex = fn_800B8B34(name, 0, 1);
    return MBCreateBlit(0, tex, x, y, w, h);
}

/* Per-frame temporary blit (32-entry ring, not linked to a node). */
MBBLIT* MBNewTempBlit(int a, int b, int c, int d, int e) {
    MBBLIT* blit;
    if (tempBlitCount >= MB_TEMPBLIT_MAX) {
        FatalError(str_TooManyTempBlits, 0x800000);
    }
    tempBlitCount++;
    blit = (MBBLIT*)&tempBlitPool[(tempBlitCount - 1) * 0x38];
    blit->flags = 0;
    blit->tex = -1;
    blit->prev = 0;
    blit->next = 0;
    blit->color0 = 0x80808080;
    blit->color1 = 0x80808080;
    blit->color2 = 0x80808080;
    blit->color3 = 0x80808080;
    mbInitBlitEntry(blit, a, 0);
    if ((blit->flags & 0x40) == 0) {
        blit->x = (s16)(b * gWinGlobals->scale->x);
        blit->y = (s16)(c * gWinGlobals->scale->y);
    } else {
        blit->x = b << 4;
        blit->y = c << 4;
    }
    blit->depth = __cvt_fp2unsigned(32.0f);
    mbBlitProject(blit, d, e);
    mbBlitSetupVerts(blit, 0.0f, 1.0f, 0.0f, 1.0f);
    return blit;
}

/* Per-frame temporary solid quad (256-entry ring, 0x24 stride). */
void* MBNewTempQuad(int a, int b) {
    u8* q;
    if (tempQuadCount >= MB_TEMPQUAD_MAX) {
        FatalError(str_TooManyQuads, 0x800000);
    }
    tempQuadCount++;
    q = &tempQuadPool[(tempQuadCount - 1) * 0x24];
    *(s32*)(q + 0x00) = 0;
    *(s32*)(q + 0x2C) = 0;
    *(s32*)(q + 0x30) = 0;
    *(s32*)(q + 0x04) = -1;
    *(u32*)(q + 0x1C) = 0x80808080;
    *(u32*)(q + 0x20) = 0x80808080;
    *(u32*)(q + 0x24) = 0x80808080;
    *(u32*)(q + 0x28) = 0x80808080;
    return &tempQuadPool[(tempQuadCount - 1) * 0x24];
}

/* Initialise the whole blit system + default render nodes. */
void MBInitBlits(int makeNodes) {
    MBNODE* n;
    memset(blitPool, 0, MB_BLIT_POOL_MAX * 0x38);
    blitPool[0].tex = -1;
    blitCount = 0;

    if (makeNodes) {
        n = (MBNODE*)fn_800BB29C(0, (const char*)0x80127D60, 13);
        if (n != 0) {
            n->flags = 0;
            n->blits = 0;
            n->unk6C = 0;
        }
        gDiag_DEC = n;
        n->flags |= 4;
        n = (MBNODE*)fn_800BB29C(0, (const char*)0x80127D60, 13);
        if (n != 0) {
            n->flags = 0;
            n->blits = 0;
            n->unk6C = 0;
        }
        defaultBlitList = n;
        n->flags |= 4;
        n = (MBNODE*)fn_800BB29C(0, (const char*)0x80127D60, 13);
        if (n != 0) {
            n->flags = 0;
            n->blits = 0;
            n->unk6C = 0;
        }
        gDiag_DE8 = n;
        n->flags |= 4;
    } else {
        gDiag_DEC = 0;
        defaultBlitList = 0;
        gDiag_DE8 = 0;
    }
    tempQuadCount = 0;
    lbl_80344E00 = 0;
    tempBlitCount = 0;
    lbl_80344DF8 = 0;
    lbl_80344E04 = 0;
}

/* Reset the per-frame temp counters. */
void MBResetBlits(void) {
    if (lbl_80344E04 == 0) {
        tempQuadCount = 0;
        tempBlitCount = 0;
    }
    lbl_80344E00 = 0;
    lbl_80344DF8 = 0;
}

/* Remove a blit: mark removed and unlink from its node list. */
int MBRemoveBlit(MBBLIT* b) {
    MBBLIT* adj;
    if (b == 0 || b->node == 0) {
        return 0;
    }
    b->flags = 2;
    adj = b->prev;
    if (adj != 0) {
        adj->next = b->next;
        adj = b->next;
        if (adj != 0) {
            adj->prev = b->prev;
        }
    } else {
        ((MBNODE*)b->node)->blits = b->next;
        if (b->next != 0) {
            b->next->prev = 0;
        }
    }
    b->next = 0;
    return 0;
}

/* Order two blits within the same node (draw-order swap). */
void MBBlitOrder(MBBLIT* a, MBBLIT* b) {
    if (a->node != b->node) {
        ErrorPrintf(str_BlitOrderDiffNodes);
        return;
    }
    if (b->prev != 0) {
        b->prev->next = b->next;
        if (b->next != 0) {
            b->next->prev = b->prev;
        }
    } else {
        ((MBNODE*)b->node)->blits = b->next;
        if (b->next != 0) {
            b->next->prev = 0;
        }
    }
    b->next = a->next;
    if (b->next != 0) {
        b->next->prev = b;
    }
    b->prev = a;
    a->next = b;
}

/* Recompute every live blit's screen position after a window change. */
void MBBlitUpdateWindow(f32 xScale, f32 yScale) {
    u8* base = (u8*)blitPool;
    s32 remaining = blitCount;
    s32 offset = 0;
    s32 scaled;
    MBBLIT* b;

    while (remaining-- > 0) {
        b = (MBBLIT*)(base + offset);
        if ((b->flags & 2) == 0 && (b->flags & 0x40) == 0) {
            scaled = (s32)(0.5 + (f32)((f32)b->x * xScale));
            b->x = (s16)scaled;
            scaled = (s32)(0.5 + (f32)((f32)b->y * yScale));
            b->y = (s16)scaled;
            scaled =
                (s32)(0.5 + (f32)((f32)(u16)b->width * xScale));
            b->width = (s16)scaled;
            if ((b->flags & 0x100) == 0) {
                scaled =
                    (s32)(0.5 + (f32)((f32)(u16)b->height * yScale));
                b->height = (s16)scaled;
            }
        }
        offset += sizeof(MBBLIT);
    }

    base = tempBlitPool;
    remaining = tempBlitCount;
    offset = 0;
    while (remaining-- > 0) {
        b = (MBBLIT*)(base + offset);
        if ((b->flags & 2) == 0 && (b->flags & 0x40) == 0) {
            scaled = (s32)(0.5 + (f32)((f32)b->x * xScale));
            b->x = (s16)scaled;
            scaled = (s32)(0.5 + (f32)((f32)b->y * yScale));
            b->y = (s16)scaled;
            scaled =
                (s32)(0.5 + (f32)((f32)(u16)b->width * xScale));
            b->width = (s16)scaled;
            if ((b->flags & 0x100) == 0) {
                scaled =
                    (s32)(0.5 + (f32)((f32)(u16)b->height * yScale));
                b->height = (s16)scaled;
            }
        }
        offset += sizeof(MBBLIT);
    }
}

/* =====================================================================
 * Drawing (GX pipeline) - stubbed pending full decompile
 * ===================================================================== */

s32 MBDrawBlits(MBNODE* node) {
    MBBLIT* b;
    s32* stats;
    MBWindow* window = gWinGlobals;
    f32 load;

    if (node->blits == 0 && node != gDiag_DE8) {
        return 0;
    }

    lbl_80343EA4 = -1;
    lbl_80343EA8 = -1;
    lbl_80343EAC = -1;
    load = (f32)pbLoad;
    lbl_80344DDC =
        (f32)(window->scale->viewport0 + window->scale->viewport1);
    lbl_80344DE0 = (f32)(1.0 / (f64)lbl_80344DDC);
    load -= lbl_80343EA0 * floorf(load / lbl_80343EA0);
    lbl_80344DD8 = load / lbl_80343EA0;

    stats = lbl_802C29F8;
    for (b = node->blits; b != 0; b = b->next) {
        if ((b->flags & 1) == 0) {
            stats[10]++;
            DrawBlit(b);
        }
    }

    if (node == gDiag_DE8) {
        u8* base = tempQuadPool;
        s32 i = 0;
        s32 offset = 0;

        for (i = 0; i < tempQuadCount; i++) {
            DrawBlitFlatQuad(base + offset);
            offset += 0x24;
        }
        lbl_80344E00 = 1;
    }
    if (node == gDiag_DE8) {
        u8* base = tempBlitPool;
        s32 i = 0;
        s32 offset = 0;

        for (i = 0; i < tempBlitCount; i++) {
            DrawBlit((MBBLIT*)(base + offset));
            offset += 0x38;
        }
        lbl_80344DF8 = 1;
    }

    lbl_80343EA4 = -1;
    lbl_80343EA8 = -1;
    lbl_80343EAC = -1;
    fn_800C36F8();
    fn_800C5B3C();
    return 0;
}

void DrawBlit(MBBLIT* b) {
    /* build vertices, bind texture (pbBlitSetTexture), set draw regs
     * (pbBlitSetDrawRegs), GXBegin/GXEnd a textured quad */
}

void DrawBlitFlatQuad(void* q) {
    /* GXBegin/GXEnd a solid (untextured) quad */
}

void pbBlitSetDrawRegs(u32 flags, u32 mode) {
    PBBlendState state;
    u8 pad[8];
    GXBool update;

    if (flags != lbl_80343EA8 || mode != lbl_80343EAC) {
        GXSetBlendMode(1, 4, 5, 0);
        GXSetZCompLoc(0);
        GXSetAlphaCompare(4, 2, 1, 4, 2);
        GXSetZMode(1, 6, 1);
        memset(&state, 0, 8);
        lbl_80343EA8 = flags;
        lbl_80343EAC = mode;
        if ((flags & 0x10000) != 0) {
            state.group = 0;
            state.source = 1;
            state.target = 1;
            GXSetAlphaCompare(2, 0, 1, 2, 0);
            GXSetBlendMode(1, 4, 5, 0);
        } else if ((flags & 0x20000) != 0) {
            state.group = 0;
            state.source = 1;
            state.target = 1;
            GXSetAlphaCompare(2, 0xFF, 1, 2, 0xFF);
            GXSetBlendMode(1, 4, 5, 0);
        } else if ((flags & 0x40000) != 0) {
            state.group = 0;
            state.source = 2;
            state.target = 2;
            GXSetAlphaCompare(7, 0, 1, 7, 0);
            GXSetBlendMode(1, 4, 0, 0);
        } else {
            GXSetAlphaCompare(4, 2, 1, 4, 2);
            if ((mode & 0x800000) != 0) {
                state.group = 0;
                state.source = 2;
                state.target = 1;
                GXSetBlendMode(1, 4, 1, 0);
            } else if ((flags & 0x200000) != 0) {
                state.group = 1;
                state.source = 2;
                state.target = 2;
                GXSetBlendMode(1, 0, 4, 0);
            } else {
                state.group = 0;
                state.source = 1;
                state.target = 1;
                GXSetBlendMode(1, 4, 5, 0);
            }
            if ((flags & 0x100000) != 0) {
                switch (state.group) {
                case 0:
                    switch (state.source) {
                    case 1:
                        switch (state.target) {
                        case 1:
                            GXSetBlendMode(1, 1, 0, 0);
                            break;
                        case 2:
                            GXSetBlendMode(1, 1, 3, 0);
                            break;
                        }
                        break;
                    case 2:
                        switch (state.target) {
                        case 1:
                            GXSetBlendMode(1, 1, 1, 0);
                            break;
                        case 2:
                            GXSetBlendMode(1, 1, 0, 0);
                            break;
                        }
                        break;
                    }
                    break;
                case 1:
                    switch (state.source) {
                    case 1:
                        switch (state.target) {
                        case 1:
                            GXSetBlendMode(1, 0, 1, 0);
                            break;
                        case 2:
                            GXSetBlendMode(1, 0, 0, 0);
                            break;
                        }
                        break;
                    case 2:
                        switch (state.target) {
                        case 1:
                            GXSetBlendMode(1, 2, 1, 0);
                            break;
                        case 2:
                            GXSetBlendMode(1, 0, 1, 0);
                            break;
                        }
                        break;
                    }
                    break;
                }
            }
        }
        if ((mode & 0x80) != 0) {
            update = 0;
        } else {
            update = 1;
        }
        if ((mode & 0x40) != 0 || (flags & 0x2000) != 0) {
            GXSetZMode(1, 7, update);
        } else {
            GXSetZMode(1, 6, update);
        }
    }
}

void pbBlitSetTexture(u32 tex) {
    int loaded;
    u32 high;
    u32 low;
    u8 pad[8];

    if (tex != lbl_80343EA4) {
        lbl_80345104 = 1;
        lbl_80343EA4 = tex;
        high = tex >> 16;
        low = tex & 0xFFFF;
        loaded = fn_800C780C(high, low, -1);
        if (loaded == 0) {
            loaded = fn_800C7558(tex);
            if (loaded == 0) {
                fn_800C1120(0);
                loaded = fn_800C7558(tex);
                if (loaded == 0) {
                    FatalError(str_DrawBlitNoTex, 0x800000);
                }
            }
            fn_800C780C(high, low, -1);
        }
        fn_800C7928((void*)tex, 0);
    }
}

s32 mbBlitCalcLight(s32 x, s32 y) {
    f64 value;
    f64 phase;
    f64 result;
    f64 output;

    value = (f64)(f32)(lbl_80344DE0 * (f32)(x + y));
    phase = (f64)(f32)(7.0 * (f64)lbl_80344DD8);
    if (value < 0.5) {
        if (phase < 1.0) {
            value = (f64)(f32)-(4.0 * value - phase);
        } else {
            if (phase < 2.0) {
                value =
                    (f64)(f32)(2.0 * value * (phase - 3.0) + 1.0);
            } else {
                if (phase < 3.0) {
                    value = (f64)(f32)(
                        2.0 * (3.0 - phase) * (0.5 - value) +
                        2.0 * (phase - 2.0) * value);
                } else {
                    if (phase < 4.0) {
                        value = (f64)(f32)(
                            2.0 * value +
                            2.0 * (3.0 - phase) * (0.5 - value));
                    } else {
                        if (phase < 5.0) {
                            value = (f64)(f32)(
                                2.0 * (3.0 - phase) * (0.5 - value) +
                                2.0 * (5.0 - phase) * value);
                        } else {
                            value = (f64)lbl_80348AA0;
                        }
                    }
                }
            }
        }
    } else {
        value = (f64)(f32)(1.0 - value);
        if (phase < 2.0) {
            value = (f64)lbl_80348AA0;
        } else {
            if (phase < 3.0) {
                value = (f64)(f32)(
                    2.0 * (phase - 4.0) * (0.5 - value) +
                    2.0 * (phase - 2.0) * value);
            } else {
                if (phase < 4.0) {
                    value = (f64)(f32)(
                        2.0 * value +
                        2.0 * (phase - 4.0) * (0.5 - value));
                } else {
                    if (phase < 5.0) {
                        value = (f64)(f32)(
                            2.0 * (phase - 4.0) * (0.5 - value) +
                            2.0 * (5.0 - phase) * value);
                    } else {
                        if (phase < 6.0) {
                            value = (f64)(f32)(
                                2.0 * value * (4.0 - phase) + 1.0);
                        } else {
                            value =
                                (f64)(f32)-(4.0 * value - (7.0 - phase));
                        }
                    }
                }
            }
        }
    }
    result = (f64)(f32)(value * 127.0);
    if (result < 0.0) {
        output = 0.0;
    } else if (result > 127.0) {
        output = 127.0;
    } else {
        output = result;
    }
    return (s32)(f32)output;
}

void mbBlitSetPage(int page) {
    lbl_80343EA4 = -1;
    lbl_80343EA8 = -1;
    lbl_80343EAC = -1;
    fn_800C36F8();
    fn_800C5B3C();
}

int mbBlitGetPage(void) {
    lbl_80343EA4 = -1;
    lbl_80343EA8 = -1;
    lbl_80343EAC = -1;
}

void mbBlitStub51E0(void) {
    lbl_80343EA4 = -1;
    lbl_80343EA8 = -1;
    lbl_80343EAC = -1;
}

void g3dcolorDtorThunk(void) {
}

void* g3dcolorDtor(void* object, s16 shouldDelete) {
    if (object != 0 && shouldDelete > 0) {
        __dl__FPv(object);
    }
    return object;
}

void* mbBlitColorArrayDtor(void* object, s16 shouldDelete) {
    if (object != 0) {
        __destroy_arr((u8*)object + 4, (ConstructorDestructor*)g3dcolorDtor,
                      1, 4);
        if (shouldDelete > 0) {
            __dl__FPv(object);
        }
    }
    return object;
}

void mbBlitPadTest(s32* manager) {
    s32 statusOffset = 0;
    s32 player = 0;
    s32 remaining = 4;
    u32 disconnected = 0;
    u32 present = 0;
    PADStatus* statuses;

    manager[0] = 0;
    statuses = G3DGetPadStatusBuffer();
    manager[2] = (s32)statuses;
    do {
        s8 error = *(s8*)((u8*)statuses + statusOffset + 10);
        u32 bit = 0x80000000U >> player;

        if (error == -1) {
            disconnected |= bit;
        } else if (error < -1) {
            if (error < -3) {
                goto next;
            }
        } else if (error >= 1) {
            goto next;
        }
        present |= bit;
        manager[manager[0] + 3] = player;
        manager[0]++;
next:
        player++;
        statusOffset += sizeof(PADStatus);
    } while (--remaining != 0);
}

void mbBlitStaticInit(void) {
    u8* manager = lbl_80296450 + 12;
    u8 pad[8];

    __construct_array(manager + 4, (ConstructorDestructor)g3dcolorDtorThunk,
                      (ConstructorDestructor)g3dcolorDtor, 1, 4);
    __register_global_object(manager, mbBlitColorArrayDtor, lbl_80296450);
}

/* =====================================================================
 * Coordinate / vertex helpers (descriptive names) - stubbed
 * ===================================================================== */

static u32 mbInitBlitEntry(MBBLIT* b, int tex, int delta) {
    s32 old = b->tex;

    if (tex >= 0) {
        b->tex = tex;
    }
    b->tex += delta;
    if (b->tex != old && (b->flags & 0xC00) != 0) {
        mbBlitProject(b,
                      (b->flags & 0x400) != 0 ? -1 : b->width,
                      (b->flags & 0x800) != 0 ? -1 : b->height);
    }
    return old;
}

static void mbBlitProject(MBBLIT* b, int width, int height) {
    u32 autoFlags = 0;

    if (width < 0 || height < 0) {
        MBTextureDef* texture = MBRomTexPtr(b->tex);

        if (width < 0) {
            width = texture->width;
            if ((texture->flags & 1) != 0 &&
                (b->flags & 0x1000000) == 0) {
                width <<= 1;
            }
            autoFlags |= 0x400;
        }
        if (height < 0) {
            height = texture->height;
            if ((texture->flags & 1) != 0 &&
                (b->flags & 0x1000000) == 0) {
                height <<= 1;
            }
            autoFlags |= 0x800;
        }
    }
    if (width != 0) {
        if ((b->flags & 0x40) == 0) {
            b->width = (s16)(width * gWinGlobals->scale->x);
        } else {
            b->width = width << 4;
        }
        b->flags &= ~0x400;
    }
    if (height != 0) {
        if ((b->flags & 0x140) == 0) {
            b->height = (s16)(height * gWinGlobals->scale->y);
        } else {
            b->height = height << 4;
        }
        b->flags &= ~0x800;
    }
    b->flags |= autoFlags;
}

static void mbBlitSetupVerts(MBBLIT* b, f32 u0, f32 u1, f32 v0, f32 v1) {
    MBTextureDef* texture = MBRomTexPtr(b->tex);
    f32 swap;

    if ((b->flags & 0x20) != 0) {
        swap = u0;
        u0 = u1;
        u1 = swap;
    }
    if ((b->flags & 0x80) != 0) {
        swap = v0;
        v0 = v1;
        v1 = swap;
    }
    if (u0 >= 0.0f) {
        b->u0 = (s16)(u0 * texture->width * 16.0f + 0.5f);
    }
    if (u1 >= 0.0f) {
        b->u1 = (s16)(u1 * texture->width * 16.0f + 0.5f);
    }
    if (v0 >= 0.0f) {
        b->v0 = (s16)(v0 * texture->height * 16.0f + 0.5f);
    }
    if (v1 >= 0.0f) {
        b->v1 = (s16)(v1 * texture->height * 16.0f + 0.5f);
    }
}
