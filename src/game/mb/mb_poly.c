/*
 * mb_poly.c - MB polygon-instance list (MB_POLY.OBJ).
 *
 * A small pool of "poly instances" (tris/quads) that the menu-board / HUD
 * layer draws through the pb blit pipeline. MBInitPolys builds the pool and a
 * hash context; MBNewPoly allocates an instance and its vertices out of a
 * per-header vertex arena; MBDrawPolyInsts walks a header's instance list,
 * transforms the shared vertex arena to screen space (PolyXfrmVerts) and
 * submits each instance through DoPolyInst/DoPolyInstSub (GX quads).
 *
 * Address range 0x800DDE34..0x800DE928. Names come from the Xbox shell3D PDB
 * (MB_POLY.OBJ). NonMatching - kept for symbol mapping / documentation;
 * function order follows the DOL. The GX-submission and vertex-projection
 * bodies (DoPolyInstSub, PolyXfrmVerts, MBPolyInstUpdateTex) are documented
 * reconstructions of the observed call/flow shape, not exact math.
 */
#include "types.h"

/* --- vertex / instance / header layout --- */
typedef struct PolyVert {
    f32 x;            /* +0x00 model-space position */
    f32 y;            /* +0x04 */
    f32 z;            /* +0x08 */
    u8  _0C[0x0C];    /* +0x0C scratch: projected screen x/y/z, clip */
    s16 u;            /* +0x18 texcoord u (fixed point) */
    s16 v;            /* +0x1A texcoord v */
    s16 flags;        /* +0x1C 1 = active vertex */
    u8  _1E[0x02];
} PolyVert;           /* 0x20 */

typedef struct PolyHeader {
    struct PolyInstance* head; /* +0x00 instance list */
    PolyVert* vertPool;        /* +0x04 base into gPolyVerts */
    s32 vertCap;               /* +0x08 vertex capacity */
    s32 vertUsed;              /* +0x0C vertices handed out */
} PolyHeader;                  /* 0x10 */

typedef struct PolyInstance {
    s16 type;                  /* +0x00 vertex count (3=tri, 4=quad) */
    s16 _02;                   /* +0x02 flags (nonzero = hidden) */
    s32 tex;                   /* +0x04 texture id, -1 = none */
    u32 color;                 /* +0x08 packed ARGB (low half = u scale) */
    s16 texW;                  /* +0x0C */
    s16 span;                  /* +0x0E draw-span advance */
    s32 vertBase;              /* +0x10 first vertex index in the arena */
    PolyVert* verts;           /* +0x14 */
    struct PolyInstance* prev; /* +0x18 */
    struct PolyInstance* next; /* +0x1C */
    PolyHeader* header;        /* +0x20 */
} PolyInstance;                /* 0x24 = 36 */

typedef struct PolyContext {
    u8  _00[0x52];
    s8  pass;                  /* +0x52 render pass id */
    u8  _53[0x0D];
    u32 flags;                 /* +0x60 */
    u8  _64[0x04];
    s16 texBase;               /* +0x68 */
    u8  _6A[0x02];
    PolyInstance* node;        /* +0x6C */
    PolyHeader* header;        /* +0x70 default header */
} PolyContext;

/* --- externs --- */
void* memset(void* p, int c, u32 n);
void ErrorPrintf(const char* fmt, ...);
PolyContext* fn_800BB29C(s32 a, void* cfg, s32 n); /* MB hash/context create */
void fn_800BA024(s32 tex);                          /* texture bind/lookup */
void fn_800BDC60(PolyVert* v, void* out, void* xf); /* xform vertex */
u32  __cvt_fp2unsigned(f32 x);
void mbBlitGetPage(void);
void mbBlitSetPage(void);
void SetTevStages(s32 n);
void SetVertexFormat(s32 fmt);
void SetCullMode(s32 mode);
void SetPerspectiveMode(s32 mode);
void SetViewportHeight(f32 h);
void PSMTXIdentity(void* m);
void GXLoadPosMtxImm(void* m, u32 id);
void pbBlitSetTexture(s32 tex);
void pbBlitSetDrawRegs(s32 a, s32 b, s32 c);
void fn_800C7914(void* a, void* b);
void GXBegin(s32 prim, s32 fmt, s32 count);

extern u8 lbl_80127D60[0x40];   /* hash config for fn_800BB29C */
extern void* gWinGlobals;       /* window/projection globals */
extern PolyVert lbl_80343F5C[]; /* shared screen-vertex scratch */

/* --- pool (real addresses in .bss/.sbss) --- */
static PolyInstance gPolyHash[128];   /* 0x80321C30 (0x1200) */
static PolyHeader   gPolyHeaders[16]; /* 0x80322E30 (0x100)  */
static PolyVert     gPolyVerts[512];  /* 0x80322F30 (0x4000) */
static s32          gPolyState;       /* 0x80345300 high-water instance count */
static s32          gPolyHeaderCount; /* 0x80345304 */
static s16          gPolyTexOfs;      /* 0x80345308 running tex offset */
static s32          gPolyVertOffset;  /* 0x8034530C arena allocation cursor */
static PolyContext* gPolyCtx;         /* 0x80345310 default context */

/* --- forward decls (DOL order) --- */
PolyHeader* MBCreatePolyHeader(s32 capacity);
void MBPolyInstUpdateTex(PolyInstance* inst, s32 tex);
void DoPolyInst(PolyInstance* inst, s32 mode, s32 phase);
s32  DoPolyInstSub(PolyInstance* inst, s32 useScratch);
void PolyXfrmVerts(PolyVert* verts, s32 count);

/* 0x800DDE34 - reset the instance pool and (optionally) build the hash ctx */
void MBInitPolys(BOOL useHash) {
    PolyContext* ctx;

    memset(gPolyHash, 0, sizeof(gPolyHash));
    gPolyHash[0].type = -1;
    gPolyState = 0;
    gPolyHeaderCount = 0;
    gPolyVertOffset = 0;

    if (useHash) {
        ctx = fn_800BB29C(0, lbl_80127D60, 10);
        if (ctx != NULL) {
            ctx->flags = 0;
            ctx->header = MBCreatePolyHeader(256);
            ctx->node = NULL;
        }
        gPolyCtx = ctx;
        ctx->flags |= 4;
    } else {
        gPolyCtx = NULL;
    }
}

/* 0x800DDEDC - carve a header + its vertex arena out of the shared pools */
PolyHeader* MBCreatePolyHeader(s32 capacity) {
    PolyHeader* h;

    if (gPolyHeaderCount >= 16) {
        ErrorPrintf("PolyHeaderList Full");
        return NULL;
    }
    h = &gPolyHeaders[gPolyHeaderCount++];
    h->head = NULL;
    h->vertPool = &gPolyVerts[gPolyVertOffset];
    h->vertCap = capacity;
    h->vertUsed = 0;
    gPolyVertOffset += capacity;
    return h;
}

/* 0x800DDF6C - allocate an instance, wire it into a header, seed its verts */
PolyInstance* MBNewPoly(PolyContext* ctx, s32 type, s32 tex, f32* verts) {
    PolyHeader* header;
    PolyInstance* inst;
    PolyInstance* tail;
    s32 free;
    s32 i;

    if (type < 3 || type > 4) {
        return NULL;
    }
    if (ctx == NULL) {
        ctx = gPolyCtx;
    }
    header = ctx->header;

    /* find the first free hash slot */
    free = 0;
    for (i = 0; i < gPolyState; i++) {
        if (gPolyHash[i].type <= 0) {
            break;
        }
        free++;
    }
    if (free >= 128) {
        ErrorPrintf("PolyInstanceList Full");
        return NULL;
    }
    if (free == gPolyState) {
        gPolyState++;
    }
    gPolyHash[gPolyState].type = -1;
    inst = &gPolyHash[free];

    if (inst->verts == NULL && header->vertUsed + 4 > header->vertCap) {
        ErrorPrintf("PolyInstanceList Vertlist Full");
        return NULL;
    }

    inst->_02 = 0;
    inst->tex = -1;
    inst->color = 0x80FFFFFF;
    inst->texW = 0;
    inst->span = 0;
    if (inst->verts == NULL) {
        inst->vertBase = header->vertUsed;
        inst->verts = &header->vertPool[inst->vertBase];
        header->vertUsed += 4;
    }
    for (i = 0; i < 4; i++) {
        inst->verts[i].x = 0.0f;
        inst->verts[i].y = 0.0f;
        inst->verts[i].z = 0.0f;
        inst->verts[i].flags = 0;
    }

    /* append to the header's instance list */
    if (header->head == NULL) {
        header->head = inst;
        inst->prev = NULL;
    } else {
        tail = header->head;
        while (tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = inst;
        inst->prev = tail;
    }
    inst->next = NULL;
    inst->header = header;
    inst->type = (s16)type;

    if (verts != NULL) {
        for (i = 0; i < type; i++) {
            inst->verts[i].x = verts[i * 3 + 0];
            inst->verts[i].y = verts[i * 3 + 1];
            inst->verts[i].z = verts[i * 3 + 2];
            inst->verts[i].flags = 1;
        }
    }
    MBPolyInstUpdateTex(inst, tex);
    return inst;
}

/* 0x800DE1CC - unlink an instance from its header list, free its verts */
void MBRemovePolyInst(PolyInstance* inst) {
    PolyVert* v = inst->verts;
    s32 i;

    for (i = 0; i < 4; i++) {
        v[i].flags = 0;
    }
    inst->type = 0;
    if (inst->prev != NULL) {
        inst->prev->next = inst->next;
        if (inst->next != NULL) {
            inst->next->prev = inst->prev;
        }
    } else {
        inst->header->head = inst->next;
        if (inst->next != NULL) {
            inst->next->prev = NULL;
        }
    }
    inst->next = NULL;
}

/* 0x800DE24C - pack an ARGB colour with a converted (inverted-half) alpha */
void MBPolyInstSetColorAlpha(PolyInstance* inst, u32 color, s32 alpha) {
    if (alpha < 0) {
        alpha = inst->color >> 24;
    }
    alpha = (255 - alpha) >> 1;
    inst->color = color | (alpha << 24);
}

/* 0x800DE274 - overwrite the instance's model-space vertices */
void MBPolyInstUpdateVerts(PolyInstance* inst, s32 count, f32* src) {
    PolyVert* dst = inst->verts;
    s32 i;

    inst->type = (s16)count;
    if (src == NULL) {
        return;
    }
    for (i = 0; i < count; i++) {
        dst[i].x = src[i * 3 + 0];
        dst[i].y = src[i * 3 + 1];
        dst[i].z = src[i * 3 + 2];
        dst[i].flags = 1;
    }
}

/* 0x800DE2D4 - recompute per-vertex texcoords for the instance's texture */
void MBPolyInstUpdateTex(PolyInstance* inst, s32 tex) {
    s32 n = inst->type;
    PolyVert* v = inst->verts;
    s32 i;

    if (tex < 0) {
        inst->tex = -1;
        for (i = 0; i < n; i++) {
            v[i].u = 0;
            v[i].v = 0;
        }
        return;
    }

    inst->tex = tex;
    fn_800BA024(tex);
    /* map each vertex against the per-type UV template, biased by the
       instance's texture-window size (low half of colour / texW). */
    for (i = 0; i < n; i++) {
        v[i].u = (s16)((f32)(u16)inst->color * 1.0f);
        v[i].v = (s16)((f32)inst->texW * 1.0f);
    }
}

/* 0x800DE3F0 - draw every instance under a context's header */
s32 MBDrawPolyInsts(PolyContext* ctx) {
    PolyHeader* header = ctx->header;
    PolyInstance* inst;

    if (header->head == NULL) {
        return 0;
    }
    mbBlitGetPage();
    PolyXfrmVerts(header->vertPool, header->vertUsed);
    gPolyTexOfs = ctx->texBase;

    for (inst = header->head; inst != NULL; inst = inst->next) {
        DoPolyInst(inst, ctx->pass, 0);
    }
    if (ctx->pass == 11) {
        for (inst = header->head; inst != NULL; inst = inst->next) {
            DoPolyInst(inst, ctx->pass, 1);
        }
    }
    mbBlitSetPage();
    return 0;
}

/* 0x800DE4AC - dispatch one instance to the GX submitter for a pass/phase */
void DoPolyInst(PolyInstance* inst, s32 mode, s32 phase) {
    if (inst->_02 != 0) {
        return;
    }
    if (inst->type < 3) {
        return;
    }
    if (phase != 0) {
        if (mode == 11) {
            DoPolyInstSub(inst, 0);
        }
    } else if (mode == 11) {
        gPolyTexOfs = (s16)(gPolyTexOfs + inst->span);
        DoPolyInstSub(inst, 0);
    } else {
        DoPolyInstSub(inst, 0);
    }
}

/* 0x800DE52C - set GX state and stream one instance's quad into the FIFO */
s32 DoPolyInstSub(PolyInstance* inst, s32 useScratch) {
    PolyVert* src;
    u8 mtx[0x30];
    s32 i;

    if (inst->type < 3) {
        return 0;
    }
    SetTevStages(0);
    SetVertexFormat(0);
    SetCullMode(0);
    SetPerspectiveMode(0);
    SetViewportHeight(0.0f);
    PSMTXIdentity(mtx);
    GXLoadPosMtxImm(mtx, 0);

    src = lbl_80343F5C;
    if (useScratch == 0) {
        pbBlitSetTexture(inst->tex);
        pbBlitSetDrawRegs(0, 0, 0);
    }

    /* build clip-space verts into the shared scratch, then emit them */
    for (i = 0; i < inst->type; i++) {
        fn_800C7914(&src[i], gWinGlobals);
    }
    GXBegin(0xA0, 0, inst->type);
    /* per-vertex position/colour/texcoord are written straight to the GX FIFO */
    for (i = 0; i < inst->type; i++) {
        (void)src[i];
    }
    return 1;
}

/* 0x800DE804 - project a header's vertex arena to screen space */
void PolyXfrmVerts(PolyVert* verts, s32 count) {
    u8 out[0x20];
    s32 i;

    for (i = 0; i < count; i++) {
        PolyVert* v = &verts[i];
        if (v->flags == 0) {
            continue;
        }
        fn_800BDC60(v, out, gWinGlobals);
        /* perspective divide + viewport map, clipping behind the eye;
           results (screen x/y/z) land in the vertex scratch fields. */
        (void)__cvt_fp2unsigned(0.0f);
    }
}
