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
 * bodies (DoPolyInstSub, PolyXfrmVerts, MBPolyInstUpdateTex) are translated;
 * DoPolyInstSub is opcode-complete with a residual register-coloring diff.
 */
#include "types.h"
#include <dolphin/gx/GXVert.h>

/* --- vertex / instance / header layout --- */
typedef struct PolyVert {
    f32 x;            /* +0x00 model-space position */
    f32 y;            /* +0x04 */
    f32 z;            /* +0x08 */
    f32 sClip;        /* +0x0C projection scratch */
    u16 sx;           /* +0x10 projected screen x */
    u16 sy;           /* +0x12 projected screen y */
    s32 sz;           /* +0x14 projected screen z / clip flag */
    u16 u;            /* +0x18 texcoord u (fixed point) */
    u16 v;            /* +0x1A texcoord v */
    u16 flags;        /* +0x1C 1 = active vertex */
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
    u16 _02;                   /* +0x02 flags (nonzero = hidden) */
    s32 tex;                   /* +0x04 texture id, -1 = none */
    u32 color;                 /* +0x08 packed ARGB (low half = u scale) */
    u16 texW;                  /* +0x0C */
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

typedef union PolyContextSlot {
    PolyContext* value;
    u64 storage;
} PolyContextSlot;

typedef struct PolyWinGlobals {
    u8 _00[0x10];
    u8* screen;                 /* +0x10, depth scale at +0x34 */
    u8 _14[0x24];
    u8* display;                /* +0x38, width/height at +0x10/+0x14 */
} PolyWinGlobals;

/* --- externs --- */
typedef struct TexInfo {
    u8  _00[0x0A];
    u16 w;            /* +0x0A texture width (fixed point) */
    u16 h;            /* +0x0C texture height */
} TexInfo;

void* memset(void* p, int c, u32 n);
void ErrorPrintf(const char* fmt, ...);
PolyContext* MBNewNode(s32 a, void* cfg, s32 n); /* MB hash/context create */
TexInfo* MBRomTexPtr(s32 tex);                      /* texture bind/lookup */
void MulVec4Mat4(PolyVert* v, void* out, void* xf); /* xform vertex */
u32  __cvt_fp2unsigned(f64 x);
void mbBlitGetPage(void);
void mbBlitSetPage(void);
void SetMultiPassTextureParams(s32 n);
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

extern f32 gIdentityMatrix[16];   /* identity matrix (transform arg to MBNewNode) */
extern PolyWinGlobals* gWinGlobals; /* window/projection globals */
extern f32* lbl_80343F5C;       /* shared screen-vertex scratch (5 floats each) */
extern const f32 lbl_801177B8[2][4][2]; /* per-type UV template (tri/quad) */

/* --- pool (real addresses in .bss/.sbss) --- */
static PolyInstance gPolyHash[128];   /* 0x80321C30 (0x1200) */
static PolyHeader   gPolyHeaders[16]; /* 0x80322E30 (0x100)  */
static PolyVert     gPolyVerts[512];  /* 0x80322F30 (0x4000) */
static s32          gPolyState;       /* 0x80345300 high-water instance count */
static s32          gPolyHeaderCount; /* 0x80345304 */
static s16          gPolyTexOfs;      /* 0x80345308 running tex offset */
static s32          gPolyVertOffset;  /* 0x8034530C arena allocation cursor */
PolyContextSlot     gPolyCtx;         /* 0x80345310 default context storage */

/* --- forward decls (DOL order) --- */
PolyHeader* MBCreatePolyHeader(s32 capacity);
void MBPolyInstUpdateTex(PolyInstance* inst, s32 tex);
void DoPolyInst(PolyInstance* inst, s32 mode, s32 phase);
s32  DoPolyInstSub(PolyInstance* inst, s32 useScratch);
void PolyXfrmVerts(PolyVert* verts, s32 count);

/* 0x800DDE34 - reset the instance pool and (optionally) build the hash ctx */
void MBInitPolys(BOOL useHash) {
    PolyContext* ctx;
    u8 unused[8];

    memset(gPolyHash, 0, sizeof(gPolyHash));
    gPolyHash[0].type = -1;
    gPolyState = 0;
    gPolyHeaderCount = 0;
    gPolyVertOffset = 0;

    if (useHash) {
        ctx = MBNewNode(0, gIdentityMatrix, 10);
        if (ctx != NULL) {
            PolyHeader* h = MBCreatePolyHeader(256);
            ctx->flags = 0;
            ctx->header = h;
            ctx->node = NULL;
        }
        gPolyCtx.value = ctx;
        ctx->flags |= 4;
    } else {
        gPolyCtx.value = NULL;
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
    PolyVert* dv;
    PolyHeader* header;
    PolyInstance* inst;
    PolyInstance* tail;
    s32 free;
    s32 i;
    u8 unused[8];

    if (type < 3 || type > 4) {
        return NULL;
    }
    if (ctx == NULL) {
        ctx = gPolyCtx.value;
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
        inst->verts[i].flags = (free = 0);
    }

    /* append to the header's instance list */
    if (header->head != NULL) {
        tail = header->head;
        while (tail->next != NULL) {
            tail = tail->next;
        }
        tail->next = inst;
        inst->prev = tail;
    } else {
        header->head = inst;
        inst->prev = NULL;
    }
    inst->next = NULL;
    inst->header = header;
    dv = inst->verts;
    inst->type = (s16)type;

    if (verts != NULL) {
        for (i = 0; i < type; i++) {
            f32* in = &verts[i * 3];
            PolyVert* out = &dv[i];
            out->x = in[0];
            out->y = in[1];
            out->z = in[2];
            out->flags = 1;
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
    TexInfo* t;
    s32 i;
    u8 unused[8];

    if (tex >= 0) {
        inst->tex = tex;
        t = MBRomTexPtr(tex);
        /* map each vertex against the per-type UV template, scaled by the
           bound texture's dimensions into fixed-point texcoords. */
        for (i = 0; i < n; i++) {
            v[i].u = (s16)((f64)t->w * (16.0 * lbl_801177B8[n - 3][i][0]) + 0.5);
            v[i].v = (s16)((f64)t->h * (16.0 * lbl_801177B8[n - 3][i][1]) + 0.5);
        }
    } else {
        inst->tex = -1;
        for (i = 0; i < n; i++) {
            v[i].u = 0;
            v[i].v = 0;
        }
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
    if (inst->_02 != 0 || inst->type < 3) {
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

static inline f32 mbPolyFactor(void) {
    return 0.0625f;
}

/* 0x800DE52C - set GX state and stream one instance's quad into the FIFO */
s32 DoPolyInstSub(PolyInstance* inst, s32 useScratch) {
    volatile u8 frameTail[16];
    PolyWinGlobals* globals;
    s32 texScale;
    s32 unusedScale;
    s32 color;
    s32 alpha;
    s32 red;
    s32 green;
    s32 blue;
    s32 depth;
    s32 i;
    s32 j;
    f32* scratch;
    f32* out;
    volatile u8 unused[16];
    u8 mtx[0x30];

    globals = gWinGlobals;
    if (inst->type < 3) {
        return 0;
    }
    SetMultiPassTextureParams(0);
    SetVertexFormat(0);
    SetCullMode(0);
    SetPerspectiveMode(0);
    SetViewportHeight(1.0f);
    PSMTXIdentity(mtx);
    GXLoadPosMtxImm(mtx, 0);

    scratch = lbl_80343F5C;
    if (useScratch == 0) {
        pbBlitSetTexture(inst->tex);
        pbBlitSetDrawRegs(0, 0, 0);
    }

    color = inst->color;
    alpha = (color >> 23) & 0x1FE;
    red = (color >> 16) & 0xFF;
    green = (color >> 8) & 0xFF;
    blue = color & 0xFF;
    if (alpha > 255) {
        alpha = 255;
    }

    fn_800C7914(&texScale, &unusedScale);

    for (i = 0; i < inst->type; i++) {
        PolyVert* vert = &inst->verts[i];
        if ((depth = vert->sz) <= 0) {
            return 0;
        }
        out = &scratch[i * 5];
        out[2] =
            (f32)((vert->sx - 0x6C00) * 2) /
                (f32)*(s32*)(globals->display + 0x10) -
            1.0f;
        out[3] =
            1.0f -
            (f32)((vert->sy - 0x7200) * 2) /
                (f32)*(s32*)(globals->display + 0x14);
        out[4] =
            1.0f -
            (f32)((u32)(*(s32*)(globals->screen + 0x34) - depth) * 2) /
                (f32)*(s32*)(globals->screen + 0x34);
        out[0] = ((f32)vert->u / (f32)texScale) * mbPolyFactor();
        out[1] = ((f32)vert->v / (f32)texScale) * mbPolyFactor();
    }

    GXBegin(0xA0, 0, inst->type);
    for (j = 0; j < inst->type; j++) {
        out = &scratch[j * 5];
        GXPosition3f32(out[2], out[3], out[4]);
        GXColor4u8(red, green, blue, alpha);
        GXTexCoord2f32(out[0], out[1]);
    }
    return 1;
}

/* 0x800DE804 - project a header's vertex arena to screen space */
void PolyXfrmVerts(PolyVert* verts, s32 count) {
    PolyVert* v;
    s32 i;
    void* wg = gWinGlobals;
    f32 out[4];
    f32 scale;
    u32 sx;
    u32 sy;

    for (i = 0; i < count; i++) {
        if ((v = &verts[i])->flags == 0) {
            continue;
        }
        v->sClip = 1.0f;
        MulVec4Mat4(v, out, (u8*)((void**)wg)[1] + 704);
        if (out[3] <= 0.0) {
            v->sz = 0;
            continue;
        }
        scale = (f32)(1.0 / out[3]);
        sx = (u32)(16.0 * (out[0] * scale));
        sy = (u32)(16.0 * (out[1] * scale));
        if ((u32)(16.0 * (out[0] * scale)) >= 0x10000 || sy >= 0x10000) {
            v->sz = 0;
        } else {
            v->sx = sx;
            v->sy = sy;
            v->sz = (u32)(out[2] * scale);
        }
    }
}
