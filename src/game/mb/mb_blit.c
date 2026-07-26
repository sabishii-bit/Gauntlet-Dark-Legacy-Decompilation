#include "types.h"

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
 * Status: NonMatching (documentation slice; symbols mapped in symbols.txt,
 * DOL still built from the original bytes). The allocation / linked-list /
 * temp-pool functions are reconstructed with real bodies; the GX drawing
 * paths (DrawBlit / DrawBlitFlatQuad / pbBlitSetDrawRegs / mbBlitBuildQuad)
 * and the projection helpers are stubbed pending full decompile. The whole
 * MB_BLIT TU actually begins at fn_800B27C4 (a timer helper below this
 * region); this file covers 0x800B2988 upward.
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
    /* 0x0C */ s32 depth;      /* z / scale, from float via __cvt_fp2unsigned */
    /* 0x10 */ u8 _pad10[0x0C];
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
    u8 _pad0[0x70];
    /* 0x70 */ MBBLIT* blits;
} MBNODE;

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
extern MBNODE* defaultBlitList;
extern void* gDiag_DEC;
extern void* gDiag_DE8;

extern const char str_TooManyTempBlits[];
extern const char str_TooManyQuads[];
extern const char str_BlitOrderDiffNodes[];
extern const char str_TooManyBlits[];
extern const char str_DrawBlitNoTex[];

/* internal helpers (defined below / same TU) */
static void mbInitBlitEntry(MBBLIT* b, int arg, int z);
static void mbBlitProject(MBBLIT* b, int a, int c);
static void mbBlitSetupVerts(MBBLIT* b, f32 s, f32 t);

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
    mbBlitProject(b, x, y);
    mbBlitSetupVerts(b, 0.0f, 0.0f);
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
    return q;
}

/* Initialise the whole blit system + default render nodes. */
void MBInitBlits(int makeNodes) {
    void* n;
    memset(blitPool, 0, MB_BLIT_POOL_MAX * 0x38);
    blitPool[0].tex = -1;
    blitCount = 0;

    if (makeNodes) {
        n = fn_800BB29C(0, (const char*)0x80127D60, 13);
        if (n != 0) {
            *(s32*)((u8*)n + 96) = 0;
            *(s32*)((u8*)n + 112) = 0;
            *(s32*)((u8*)n + 108) = 0;
        }
        gDiag_DEC = n;
        *(s32*)((u8*)n + 96) |= 4;
        n = fn_800BB29C(0, (const char*)0x80127D60, 13);
        if (n != 0) {
            *(s32*)((u8*)n + 96) = 0;
            *(s32*)((u8*)n + 112) = 0;
            *(s32*)((u8*)n + 108) = 0;
        }
        defaultBlitList = (MBNODE*)n;
        /* ... additional default nodes ... */
    }
}

/* Reset the per-frame temp counters. */
void MBResetBlits(void) {
    tempBlitCount = 0;
    tempQuadCount = 0;
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
    }
    adj = b->next;
    if (adj != 0) {
        adj->prev = b->prev;
    }
    return 0;
}

/* Order two blits within the same node (draw-order swap). */
void MBBlitOrder(MBBLIT* a, MBBLIT* b) {
    if (a->node != b->node) {
        ErrorPrintf(str_BlitOrderDiffNodes);
        return;
    }
    /* ... swap link order ... */
}

/* Recompute every live blit's screen position after a window change. */
void MBBlitUpdateWindow(void) {
    int i;
    for (i = 0; i < blitCount; i++) {
        MBBLIT* b = &blitPool[i];
        if ((b->flags & 0x2) != 0 || (b->flags & 0x40) != 0) {
            continue;
        }
        mbBlitProject(b, b->x, b->y);
    }
}

/* =====================================================================
 * Drawing (GX pipeline) - stubbed pending full decompile
 * ===================================================================== */

void MBDrawBlits(void) {
    /* iterate render nodes, call DrawBlit / DrawBlitFlatQuad per entry */
}

void DrawBlit(MBBLIT* b) {
    /* build vertices, bind texture (pbBlitSetTexture), set draw regs
     * (pbBlitSetDrawRegs), GXBegin/GXEnd a textured quad */
}

void DrawBlitFlatQuad(void* q) {
    /* GXBegin/GXEnd a solid (untextured) quad */
}

void pbBlitSetDrawRegs(int mode) {
    /* GXSetBlendMode / GXSetZMode / GXSetZCompLoc / GXSetAlphaCompare */
}

int pbBlitSetTexture(int tex) {
    /* load/bind the texture; FatalError(str_DrawBlitNoTex) on failure */
    return 0;
}

void mbBlitBuildQuad(MBBLIT* b) {
    /* fill the 4 screen-space vertices from x/y/size/crop */
}

void mbBlitSetPage(int page) {
}

int mbBlitGetPage(void) {
    return 0;
}

/* =====================================================================
 * Coordinate / vertex helpers (descriptive names) - stubbed
 * ===================================================================== */

static void mbInitBlitEntry(MBBLIT* b, int tex, int z) {
    b->tex = tex;
}

static void mbBlitProject(MBBLIT* b, int x, int y) {
    b->x = (s16)x;
    b->y = (s16)y;
}

static void mbBlitSetupVerts(MBBLIT* b, f32 s, f32 t) {
}
