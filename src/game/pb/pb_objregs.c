#include "types.h"

/* Midway "bulletproof" object / GX register-setup TU (Xbox counterpart:
 * pb_objregs.obj - the D3D render-state layer; on GameCube this is the GX
 * equivalent). Covers the immediate-mode geometry pipeline and the render
 * register setters: cull mode, viewport, projection mode, vertex format,
 * TEV stage setup, the object-draw register block, and the texture-shift
 * debug dump. .text 0x800C3F58-0x800C6AD4.
 *
 * Xbox pb_objregs.obj roster (shell3D.pdb): setLmapInfo / setTexInfo /
 * setTexShift / setPrimColor / setPrimAlpha / setChrome / setupKeepA /
 * pbSetupPosLights / pbResetDORegs / pbInitDORegs / SetVertexFormat /
 * SetMultiPassTextureParams / SetCullMode / SetPerspectiveMode /
 * SetViewportHeight / sSetGFXEnv / sFlushSubVertBuffer / sFlushVertBuffer /
 * sDrawGeom / pbSetDORegs.  GC-verified identities:
 *   pbInitDORegs = pbInitDORegs   (kept fn_: referenced by Matching mb_main.c)
 *   pbResetDORegs = pbResetDORegs  (kept fn_: referenced by mb_blit/pb_window)
 *   pbSetupPosLights = pbSetupPosLights (kept fn_: referenced by pb_objects.c)
 *   SetMultiPassTextureParams = SetMultiPassTextureParams, sSetGFXEnv = sSetGFXEnv
 *   (existing provisional names kept to avoid cross-TU churn).
 */

/* --- GX externs (owned by the SDK gx lib) --- */
typedef u8 GXBool;
void GXSetCullMode(u32 mode);
void GXBegin(u32 type, u32 fmt, u16 nverts);
void GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz);
void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht);
void GXSetProjection(f32* mtx, u32 type);
void GXSetChanCtrl(s32 chan, GXBool en, u32 amb, u32 mat, u32 lights, u32 diff, u32 attn);
void GXSetVtxDesc(u32 attr, u32 type);
void GXSetZMode(GXBool cmpEn, u32 func, GXBool updEn);
void GXSetZCompLoc(GXBool beforeTex);
void GXSetBlendMode(u32 type, u32 srcF, u32 dstF, u32 op);
void GXSetColorUpdate(GXBool en);
void GXSetAlphaUpdate(GXBool en);
void GXSetAlphaCompare(u32 comp0, u8 ref0, u32 op, u32 comp1, u8 ref1);
void GXSetTevOrder(u32 stage, u32 coord, u32 map, u32 color);
void GXSetTevOp(u32 stage, u32 mode);
void GXSetTevColorOp(u32 stage, u32 op, u32 bias, u32 scale, GXBool clamp, u32 out);
void GXSetTevAlphaOp(u32 stage, u32 op, u32 bias, u32 scale, GXBool clamp, u32 out);
void GXSetTevColorIn(u32 stage, u32 a, u32 b, u32 c, u32 d);
void GXSetTevAlphaIn(u32 stage, u32 a, u32 b, u32 c, u32 d);
void GXSetNumTexGens(u32 n);
void GXSetNumTevStages(u32 n);
void GXLoadTexObj(void* obj, u8 map);

/* --- write-gather pipe (immediate-mode vertex FIFO) --- */
typedef union {
    u8  vu8;
    u16 vu16;
    u32 vu32;
    f32 vf32;
} PbWGPipe;
volatile PbWGPipe GXWGFifo : 0xCC008000;

static inline void GXPosition3f32(f32 x, f32 y, f32 z)
{
    GXWGFifo.vf32 = x;
    GXWGFifo.vf32 = y;
    GXWGFifo.vf32 = z;
}
static inline void GXColor4u8(u8 r, u8 g, u8 b, u8 a)
{
    GXWGFifo.vu8 = r;
    GXWGFifo.vu8 = g;
    GXWGFifo.vu8 = b;
    GXWGFifo.vu8 = a;
}
static inline void GXTexCoord2f32(f32 s, f32 t)
{
    GXWGFifo.vf32 = s;
    GXWGFifo.vf32 = t;
}

/* --- game printf / log --- */
extern int bulletproof_printf(char* fmt, ...);
extern void FatalError(char* fmt, int code);

/* --- pb math library (C++ TUs; mangled names are valid C identifiers) --- */
extern void mat44InvRigid__FR5mat44R5mat44(f32* dst, f32* src);
extern void vec4ApplyTrans__FR4vec4R4vec4R5mat44(f32* dst, f32* src, f32* m);
extern f32 vec3LengthSquared__FR4vec3(f32* v);
extern void __as__4vec4FRC4vec4(f32* dst, f32* src);

/* --- pb_texture layer --- */
extern void fn_800C7928(u32, u32);
extern void fn_800C7558(u32 handle);

/* ------------------------------------------------------------------ */
/* module structs                                                      */
/* ------------------------------------------------------------------ */

/* immediate-mode vertex, 0x24 bytes */
typedef struct PbVtx {
    u8  c[4];        /* 0x00 : RGBA */
    f32 uv[4];       /* 0x04 : two UV pairs */
    f32 x, y, z;     /* 0x14 */
    u8  pad20;       /* 0x20 */
    u8  cull;        /* 0x21 */
    u8  pad22[2];
} PbVtx;

static inline void emitVtxHead(PbVtx* v)
{
    GXPosition3f32(v->x, v->y, v->z);
    GXColor4u8(v->c[0], v->c[1], v->c[2], v->c[3]);
}

/* GFX environment blob handed to sSetGFXEnv */
typedef struct PbGfxEnv {
    u32 m00;         /* 0x00 -> fn_800C7928 arg */
    u8  blendOn;     /* 0x04 */
    u8  colorUpd;    /* 0x05 */
    u8  alphaUpd;    /* 0x06 */
    u8  _pad07;
    u32 srcFactor;   /* 0x08 */
    u32 dstFactor;   /* 0x0C */
    u8  alphaOn;     /* 0x10 */
    u8  _pad11[3];
    s32 alphaFunc;   /* 0x14 */
    u8  alphaRef;    /* 0x18 */
    u8  zCmpEn;      /* 0x19 */
    u8  _pad1a[2];
    u32 zFunc;       /* 0x1C */
} PbGfxEnv;

/* draw-object register block (*lbl_80343F4C) */
typedef struct PbDoRegs {
    s32  m00, m04, m08, m0c;   /* 0x00 */
    f32  f10, f14, f18, f1c;   /* 0x10 */
    f32  f20, f24, f28, f2c;   /* 0x20 */
    s32  m30, m34;             /* 0x30 */
    f32  f38;                  /* 0x38 */
    s32  m3c;                  /* 0x3C */
    s32  m40;                  /* 0x40 */
    u32  m44;                  /* 0x44 */
    f32  f48, f4c;             /* 0x48 */
    s32  m50, m54;             /* 0x50 */
    s32  m58, m5c;             /* 0x58 */
    s32  m60, m64;             /* 0x60 */
    u8   _pad68[8];
    s32  numLights;            /* 0x70 */
    f32* lights;               /* 0x74 */
    s32  m78, m7c, m80;        /* 0x78 */
    u8   _pad84[0x38];
    s32  mbc;                  /* 0xBC */
    s32  mc0;                  /* 0xC0 */
    s32  mc4;                  /* 0xC4 */
    u32  mc8;                  /* 0xC8 */
    s32  mcc, md0, md4;        /* 0xCC */
    s32  md8;                  /* 0xD8 */
    f32  fdc;                  /* 0xDC */
    s32  me0, me4;             /* 0xE0 */
    s32  me8;                  /* 0xE8 */
} PbDoRegs;

/* per-object register/command builder (the "rdb" object) */
typedef struct PbRegPair {
    u32 lo;                    /* 0x00 */
    u32 hi;                    /* 0x04 */
} PbRegPair;

typedef struct PbDOObj {
    u8   _pad00[0x10];
    f32  r, g, b;              /* 0x10 */
    u8   _pad1c[4];
    f32  shu, shv, shu2, shv2; /* 0x20 */
    s64  q30;                  /* 0x30 */
    u8   _pad38[8];
    u32  m40;                  /* 0x40 */
    u8   _pad44[4];
    f32  f48;                  /* 0x48 */
    u8   _pad4c[4];
    u32  m50;                  /* 0x50 */
    u8   _pad54[0x24];
    u32  flags78;              /* 0x78 */
    u8   _pad7c[0xc];
    PbRegPair regs[5];         /* 0x88 */
    u8   regid[8];             /* 0xB0 */
    s32  nregs;                /* 0xB8 */
    u32  dirty;                /* 0xBC */
    u8   _padc0[0x14];
    s32  clamp;                /* 0xD4 */
    void* clampp;              /* 0xD8 */
    u8   _paddc[0xc];
    s32  shiftOn;              /* 0xE8 */
    u32  lmap;                 /* 0xEC */
} PbDOObj;

/* render-state shadow block (bss @0x802C5430; dtk carves the tail into
 * lbl_802C5578/lbl_802C5580..., but the code anchors it all here) */
typedef struct PbDrawState {
    u8  _pad000[0x148];
    u32 f148;    /* 0x148 : current DO texture handle */
    u8  _pad14c[4];
    u32 f150;    /* 0x150 */
    u32 f154;    /* 0x154 */
    u8  f158;    /* 0x158 */
    u8  _pad159[3];
    u32 f15c;    /* 0x15C */
    u8  f160;    /* 0x160 */
    u8  f161;    /* 0x161 */
    u8  _pad162[2];
    u32 f164;    /* 0x164 */
    u8  _pad168[0x20];
    u32 f188;    /* 0x188 */
    u8  _pad18c[4];
    u32 f190;    /* 0x190 */
    u32 f194;    /* 0x194 */
    u8  f198;    /* 0x198 */
    u8  _pad199[3];
    u32 f19c;    /* 0x19C */
    u8  f1a0;    /* 0x1A0 */
    u8  f1a1;    /* 0x1A1 */
    u8  _pad1a2[2];
    u32 f1a4;    /* 0x1A4 */
} PbDrawState;

/* texture-shift / chrome context (*lbl_80343F50) */
typedef struct PbTexShiftCtx {
    u8  _pad00[0x14];
    s32 m14;     /* 0x14 */
    s32 m18;     /* 0x18 */
    u8  _pad1c[0x10];
    f32 f2c;     /* 0x2C */
    f32 f30;     /* 0x30 */
    f32 f34;     /* 0x34 */
    f32 f38;     /* 0x38 */
    u8  _pad3c[4];
    u32 dbg;     /* 0x40 */
    s32 m44;     /* 0x44 */
} PbTexShiftCtx;

/* texture bank / entry views (shared shape with pb_texture.c) */
typedef struct PbTexEntry {  /* 16 bytes */
    u8  b0;
    s8  bank;    /* 0x1 */
    u16 flags;   /* 0x2 */
    u8  _pad4[4];
    u16 lmap;    /* 0x8 */
    u16 w;       /* 0xA */
    u16 h;       /* 0xC */
    s16 slot;    /* 0xE */
} PbTexEntry;

typedef struct PbTexBank {
    u8  _pad00[0x58];
    PbTexEntry* entries;   /* 0x58 */
    u8  _pad5c[0x1c];
    u8* loaded;            /* 0x78 */
    u8  _pad7c[4];
    u8* texObjs;           /* 0x80 : GXTexObj[] (0x30 each) */
} PbTexBank;

typedef struct PbTexBankRef {  /* 16 bytes */
    u8  _pad0[4];
    PbTexBank* bank;       /* 0x4 */
    u8  _pad8[8];
} PbTexBankRef;

/* screen/persp sub-block (gWinGlobals->scr) */
typedef struct PbScr {
    u8  _pad00[0x48];
    f32 f48;     /* 0x48 */
    f32 f4c;     /* 0x4C */
} PbScr;

/* per-texture context (gWinGlobals->texCtx; pb_texture's PbTexCtx) */
typedef struct PbTexCtxV {
    u8  _pad00[0x2bc];
    u32 m2bc;    /* 0x2BC */
    u32 m2c0;    /* 0x2C0 */
} PbTexCtxV;

/* env sub-block (gWinGlobals->env) */
typedef struct PbEnvBlk {
    u8  _pad00[0x30];
    u32 m30;     /* 0x30 */
} PbEnvBlk;

/* *gWinGlobals view used by this TU */
typedef struct PbORGlobals {
    u8    _pad00[0x04];
    PbScr* scr;            /* 0x04 */
    PbEnvBlk* env;         /* 0x08 */
    u8    _pad0c[0x08];
    u32* volatile hook14;  /* 0x14 */
    u8    _pad18[0x04];
    u8*   lights;          /* 0x1C */
    u32* volatile hook20;  /* 0x20 */
    u32* volatile hook24;  /* 0x24 */
    u8    _pad28[0x08];
    PbTexBankRef* banks;   /* 0x30 */
    u32* volatile hook34;  /* 0x34 */
    u8    _pad38[0x08];
    PbTexCtxV* texCtx;     /* 0x40 */
} PbORGlobals;

extern PbORGlobals* gWinGlobals;

/* ------------------------------------------------------------------ */
/* module data                                                         */
/* ------------------------------------------------------------------ */

/* --- screen viewport dims { wd, ht, left, top, minz, maxz } --- */
extern f32 gScreenData[6];

/* --- projection matrices (perspective computed by pbProjCalc) --- */
extern f32 gPerspProjMtx[16];        /* gPerspProjMtx (bss) */
extern f32 orthoProjMtx[16];         /* orthoProjMtx (main globals block) */

/* --- cull-mode lookup table (indexed by state) --- */
extern u32 sCullModeTable[4];        /* sCullModeTable (.data) */

/* --- default texshift ("texsh_def") --- */
extern f32 lbl_801283A0[4];

/* --- strings --- */
extern char str_txsh[];              /* "txsh: %6.2Lf %6.2Lf  %6.3Lf..." */
extern char str_TexNotLoaded[];      /* "pbSetDODrawRegs: Texture not loaded" */
extern char lbl_80348F8C[];          /* "\n" (sdata2) */

/* --- sdata block --- */
extern u32 lbl_80343F48;             /* value copied into lightmap packets */
extern PbDoRegs* lbl_80343F4C;       /* the DO register block */
extern PbTexShiftCtx* lbl_80343F50;  /* texture-shift / chrome context */
extern u8 lbl_80343F58;              /* z-update shadow ("sZMod") */
extern u8 lbl_80343F60[4];           /* clamp-mode lookup */
extern u32 lbl_80343F70;             /* current texture width */
extern u32 lbl_80343F74;             /* current texture height */

/* --- bss --- */
extern PbDrawState lbl_802C5430;     /* render-state shadow */
extern u32 lbl_802C5578[];           /* current DO texture handle (carved) */
extern u8 lbl_802C71F8[0x240];       /* transformed positional lights */
extern u8 lbl_802C2E28[];            /* lightmap palette block */

/* --- sbss --- */
extern u32 lbl_803450B8[2];          /* default hook14 block */
extern u32 lbl_803450C0[2];          /* default hook20 block */
extern u32 lbl_803450C8[2];          /* default hook24 block */
extern u32 lbl_803450D0[2];          /* default hook34 block */
extern s32 lbl_803450F0;             /* shared-bank texture index */
extern s32 lbl_80345118;
extern s8  lbl_8034511C;

/* --- render-state shadow (sbss statics) --- */
static f32 sVpHeightScale;
static s8  sVpHeightInit;
static s32 sPerspMode;
static s8  sPerspModeInit;
static s32 sCullMode;
static s32 sVtxFormat;
static s32 sNumTevStages;

/* Immediate-mode quad emitter: sets cull from vtx[0x21], GXBegin(QUADS), and
 * streams position/color/uv into the GX FIFO for `count` verts.
 * (Xbox counterpart: sFlushSubVertBuffer.) */
void pbDrawVerts(s32 count, u8* verts)
{
    s32 o1;
    PbVtx* v;

    if (count >= 3) {
        s32 cull = verts[0x21] + 1;
        if (cull != sCullMode) {
            sCullMode = cull;
            GXSetCullMode(sCullModeTable[cull]);
        }
        GXBegin(0x98, 0, (u16)count);
        o1 = 0;
        for (; count > 0; count--) {
            v = (PbVtx*)(verts + o1);
            GXPosition3f32(v->x, v->y, v->z);
            GXColor4u8(v->c[0], v->c[1], v->c[2], v->c[3]);
            GXTexCoord2f32(v->uv[0], v->uv[1]);
            if (sVtxFormat == 1) {
                GXTexCoord2f32(v->uv[2], v->uv[3]);
            }
            o1 += 0x24;
        }
    }
}

/* Pixel/blend environment from a state blob (Xbox: sSetGFXEnv). */
void sSetGFXEnv(PbGfxEnv* e)
{
    GXBool v;

    fn_800C7928(e->m00, 0);
    v = 1;
    if (e->alphaOn && e->alphaFunc != 7) {
        v = 0;
    }
    GXSetZCompLoc(v);
    if (e->alphaOn) {
        GXSetAlphaCompare(e->alphaFunc, e->alphaRef, 0, 7, 0);
    } else {
        GXSetAlphaCompare(7, 0, 0, 7, 0);
    }
    GXSetZMode(e->zCmpEn, e->zFunc, lbl_80343F58);
    if (e->blendOn) {
        GXSetBlendMode(1, e->srcFactor, e->dstFactor, 0);
    } else {
        GXSetBlendMode(0, 0, 0, 0);
    }
    GXSetColorUpdate(e->colorUpd);
    GXSetAlphaUpdate(e->alphaUpd);
}

/* Scale the viewport / scissor by a height factor (caches last value). */
void SetViewportHeight(f32 scale)
{
    if (!sVpHeightInit) {
        sVpHeightInit = 1;
        sVpHeightScale = 1.0f;
    }
    if (scale != sVpHeightScale) {
        sVpHeightScale = scale;
        GXSetViewport(gScreenData[0], gScreenData[1], gScreenData[2],
                      scale * gScreenData[3], gScreenData[4], gScreenData[5]);
        GXSetScissor((u32)gScreenData[0], (u32)gScreenData[1],
                     (u32)gScreenData[2], (u32)(scale * gScreenData[3]));
    }
}

/* Toggle perspective vs orthographic projection (caches last mode). */
void SetPerspectiveMode(s32 mode)
{
    if (!sPerspModeInit) {
        sPerspMode = 0;
        sPerspModeInit = 1;
    }
    if (mode == sPerspMode) {
        return;
    }
    sPerspMode = mode;
    if (mode == 0) {
        GXSetProjection(orthoProjMtx, 1);
    } else {
        GXSetProjection(gPerspProjMtx, 0);
    }
}

/* Set backface cull mode (cached). */
void SetCullMode(s32 mode)
{
    if (mode != sCullMode) {
        sCullMode = mode;
        GXSetCullMode(sCullModeTable[mode]);
    }
}

/* Set channel control + vertex descriptors for a vertex format (cached). */
void SetVertexFormat(s32 fmt)
{
    u32 chan;
    u32 d13;
    u32 d14;
    u32 d11;

    if (fmt != sVtxFormat) {
        sVtxFormat = fmt;
        chan = 1;
        d13 = 1;
        d14 = 0;
        d11 = 1;
        switch (fmt) {
        case 0:
            break;
        case 1:
            d14 = 1;
            break;
        case 2:
            chan = 0;
            d11 = 0;
            break;
        case 3:
            chan = 0;
            d11 = 0;
            d13 = 0;
            break;
        }
        GXSetChanCtrl(4, 0, 0, chan, 0, 0, 2);
        GXSetVtxDesc(13, d13);
        GXSetVtxDesc(14, d14);
        GXSetVtxDesc(11, d11);
    }
}

/* Configure the TEV stages for a texture pass mode
 * (Xbox: SetMultiPassTextureParams). */
void SetMultiPassTextureParams(s32 stages)
{
    s32 old = sNumTevStages;
    u32 n;

    if (stages != sNumTevStages) {
        sNumTevStages = stages;
        n = 1;
        switch (stages) {
        case 1:
            GXSetTevOrder(1, 1, 1, 0xFF);
            GXSetTevOp(1, 4);
            GXSetTevColorOp(1, 0, 0, 0, 1, 0);
            GXSetTevColorIn(1, 0xF, 0, 9, 0xF);
            GXSetNumTexGens(2);
            n = 2;
            break;
        case 2:
            GXSetTevOrder(1, 0, 1, 4);
            GXSetTevAlphaIn(1, 0, 6, 4, 7);
            GXSetTevAlphaOp(1, 0xE, 0, 0, 1, 0);
            GXSetTevColorOp(1, 0, 0, 1, 1, 0);
            GXSetTevColorIn(1, 0xF, 10, 8, 0xF);
            n = 3;
            break;
        case 3:
            GXSetTevOp(0, 4);
            break;
        }
        switch (old) {
        case 1:
            GXSetNumTexGens(1);
            break;
        case 3:
            GXSetTevOp(0, 0);
            GXSetTevColorOp(0, 0, 0, 1, 1, 0);
            GXSetTevColorIn(0, 0xF, 10, 8, 0xF);
            break;
        }
        GXSetNumTevStages(n);
    }
}

/* Big geometry pipeline: transform, light, clip and draw an object's verts. */
void sDrawGeom(void* obj)
{
    (void)obj;
    /* stub: transforms verts through the vec/mat ops then calls pbDrawVerts */
}

/* chrome / environment UV generator (Xbox: setChrome) -- skeleton. */
void fn_800C5598(void)
{
}

void pbResetDORegs(void);

/* Public init entry: reset the DO register block (Xbox: pbInitDORegs). */
void pbInitDORegs(void)
{
    pbResetDORegs();
}

/* Reset the draw-object register defaults (Xbox: pbResetDORegs). */
void pbResetDORegs(void)
{
    PbDoRegs* p = lbl_80343F4C;

    p->mc4 = -1;
    p->mc8 = 0xFFFFFF00;
    p->mcc = -1;
    p->md0 = -1;
    p->md4 = -1;
    p->md8 = 0;
    p->me8 = 1;
    p->fdc = 0.0f;
    p->mc0 = 0;
    p->me0 = 0;
    p->me4 = 0;
    p->mbc = 4;
    p->f10 = 128.0f;
    p->f18 = 128.0f;
    p->f14 = 128.0f;
    p->f1c = 255.0f;
    p->f20 = 256.0f;
    p->f24 = 256.0f;
    p->f28 = 0.0f;
    p->f2c = 0.0f;
    p->m34 = -1;
    p->m30 = -1;
    p->m40 = -1;
    p->m44 = -1;
    p->f38 = 0.0f;
    p->m3c = 0;
    p->f48 = 0.0f;
    p->f4c = 1.0f;
    p->m44 = 0x302C4000;
    p->numLights = 0;
    p->lights = 0;
    p->m78 = 0;
    p->m7c = 0;
    p->m80 = 0;
    p->m00 = -1;
    p->m04 = -1;
    p->m0c = -1;
    p->m08 = -1;
    p->m64 = -1;
    p->m60 = -1;
    p->m54 = -1;
    p->m50 = -1;
    p->m58 = 0;
    p->m5c = 0;
}

/* Transform + range-cull the world's positional lights into lbl_802C71F8
 * (max 12) and point the DO regs at them (Xbox: pbSetupPosLights). */
s32 pbSetupPosLights(f32 extra, s32 a, s32 b, f32* m)
{
    f32 inv[16];
    f32* out;
    PbORGlobals* g = gWinGlobals;
    u8* light;
    s32 i;
    s32 hits;

    mat44InvRigid__FR5mat44R5mat44(inv, m);
    hits = 0;
    for (i = 0; i < *(s32*)(g->lights + 0xA0); i++) {
        light = g->lights + i * 32 + 0xDC;
        out = (f32*)(lbl_802C71F8 + hits * 32);
        vec4ApplyTrans__FR4vec4R4vec4R5mat44(out, (f32*)light, inv);
        if (vec3LengthSquared__FR4vec3(out) <
            (extra + *(f32*)(g->lights + i * 4 + 0x25C)) *
                (extra + *(f32*)(g->lights + i * 4 + 0x25C))) {
            __as__4vec4FRC4vec4(out + 4, (f32*)(light + 16));
            out[3] = *(f32*)(light + 12);
            hits++;
        }
        if (hits >= 12) {
            break;
        }
    }
    if (hits != 0) {
        lbl_80343F4C->lights = (f32*)lbl_802C71F8;
        lbl_80343F4C->numLights = hits;
        return 0;
    }
    lbl_80343F4C->lights = 0;
    lbl_80343F4C->numLights = 0;
    return 0;
}

/* Emit a texture-address packet + reset the shared texture regs. */
void fn_800C5D44(u32* pkt, s32 xy)
{
    PbDrawState* st = &lbl_802C5430;
    pkt[0] = 0x5C000;
    pkt[1] = 0x44;
    pkt[1] = 0x44;
    st->f188 = (xy & 0xFFFF) | ((xy >> 16) << 16);
    st->f198 = 0;
    st->f1a0 = 0;
    st->f190 = 4;
    st->f194 = 5;
    st->f19c = 0;
    st->f1a1 = 1;
    st->f1a4 = 6;
}

/* the big DO register writer (Xbox: pbSetDORegs) -- skeleton. */
void fn_800C5DA8(void)
{
}

/* Apply z-test / blend-test register deltas for an object
 * (Xbox candidate: setTexInfo). */
void fn_800C6350(PbDOObj* obj, s32 flags, u32 mask)
{
    PbORGlobals* g = gWinGlobals;
    PbDrawState* st = &lbl_802C5430;

    if (mask & 0x80) {
        PbEnvBlk* env = g->env;
        u8* q;
        s32 zon = 0;
        u32 val = env->m30;
        if (flags & 0x80) {
            zon = 1;
        }
        lbl_80343F58 = 1;
        if (flags & 0x80) {
            lbl_80343F58 = 0;
        }
        q = (u8*)obj + obj->nregs * 8;
        *(u32*)(q + 0x8C) = val;
        *(u32*)(q + 0x88) = zon;
        obj->regid[obj->nregs] = 0x4E;
        obj->nregs = obj->nregs + 1;
    }
    if (mask & 0x40) {
        u32 v;
        if (flags & 0x40) {
            v = 0x3001D;
        } else {
            v = 0x5001D;
        }
        obj->m40 = v;
        obj->dirty = 4;
        st->f158 = 1;
        st->f160 = 2;
        st->f161 = 1;
        st->f15c = 4;
        if (flags & 0x40) {
            st->f164 = 7;
        } else {
            st->f164 = 6;
        }
    }
    if (obj->flags78 & 2) {
        return;
    }
    {
        s32 hi = flags & 0x800000;
        s32 t;
        if (hi) {
            t = 0x48;
        } else {
            t = 0x44;
        }
        if (obj->q30 == t) {
            return;
        }
        if (hi) {
            st->f150 = 4;
            st->f154 = 1;
        } else {
            st->f150 = 4;
            st->f154 = 5;
        }
        obj->q30 = t;
        obj->dirty = 4;
    }
}

/* Set an object's base color / intensity registers
 * (Xbox candidate: setPrimColor). */
void fn_800C64A4(PbDOObj* obj, u32 flags, u8* node)
{
    PbORGlobals* g = gWinGlobals;
    f32 base = 1.0f;
    f32* p;
    f32 s;

    if (flags & 0x2000) {
        p = (f32*)(g->lights + 0x88);
    } else if (flags & 0x5000) {
        u32 f = obj->flags78 | 1;
        p = (f32*)(g->lights + 0x80);
        obj->flags78 = f;
    } else {
        obj->flags78 &= ~1;
        asm {}
        p = (f32*)(g->lights + 0x90);
        base = 0.0f;
    }
    obj->f48 = p[1] + (f32)*(s16*)(node + 0x6A) + base;
    if (flags & 0x100) {
        s = (128.0f / 255.0f) * p[0];
        obj->r = (f32)((*(u32*)(node + 100) >> 16) & 0xFF) * s;
        obj->g = (f32)((*(u32*)(node + 100) >> 8) & 0xFF) * s;
        obj->b = (f32)(*(u32*)(node + 100) & 0xFF) * s;
        obj->flags78 |= 8;
    } else {
        s = 128.0f * p[0];
        obj->r = s;
        obj->b = s;
        obj->g = s;
        obj->flags78 &= ~8;
    }
    obj->dirty = 4;
}

/* Debug: apply / print the current texture-UV shift ("txsh: ..."). */
void setTexShift(PbDOObj* obj, f32* sh, f32* alt, s32 chrome)
{
    s32 on = 1;
    u32 dbg = 0;
    PbTexShiftCtx* ctx = lbl_80343F50;
    PbORGlobals* g = gWinGlobals;

    if (ctx->m44 == 0) {
        dbg = ctx->dbg;
    }
    if (sh == 0) {
        if (alt == 0) {
            if (chrome == 0) {
                if (obj->shiftOn == 0) {
                    return;
                }
                on = 0;
            }
            sh = lbl_801283A0;
        } else {
            sh = alt;
        }
    }
    if (chrome != 0) {
        f32 x;
        f32 y;
        if (ctx->m14 == 0 && ctx->m18 == 0) {
            g->scr->f4c = 0.0f;
            g->scr->f48 = 0.0f;
        }
        x = 0.5f * sh[0] * lbl_80343F50->f34;
        y = 0.5f * sh[2] * lbl_80343F50->f38;
        obj->shu = x;
        obj->shv = y;
        obj->shu2 = x + sh[1] + g->scr->f4c * lbl_80343F50->f30;
        obj->shv2 = y + sh[3] + g->scr->f48 * lbl_80343F50->f2c;
        if (dbg & 0x800) {
            bulletproof_printf(str_txsh, obj->shu, obj->shv, obj->shu2,
                               obj->shv2);
        }
        if (dbg & 0x1000) {
            bulletproof_printf("\n");
        }
    } else {
        obj->shu = 256.0f * sh[0];
        obj->shv = 256.0f * sh[2];
        obj->shu2 = sh[1];
        obj->shv2 = sh[3];
    }
    obj->shiftOn = on;
}

/* Set the object draw registers; bails if the object's texture isn't
 * loaded. */
void pbSetDODrawRegs(PbDOObj* obj, u32 handle)
{
    u8 unused[8];
    u32 lo = handle & 0xFFFF;
    PbORGlobals* g = gWinGlobals;
    s32 n = obj->nregs;
    PbTexBankRef* banks = g->banks;
    PbTexEntry* t = &banks[(u16)(handle >> 16)].bank->entries[(u16)handle];
    PbTexBank* bank0;
    s32 val;
    s32 loaded;

    if (t->flags & 0x100) {
        bank0 = banks[0].bank;
        loaded = bank0->loaded[lbl_803450F0];
        t = &bank0->entries[lbl_803450F0];
    } else {
        loaded = banks[(u16)(handle >> 16)].bank->loaded[lo];
    }
    if (loaded == 0) {
        FatalError(str_TexNotLoaded, 0x800000);
    }
    obj->regid[n] = 0x3F;
    n = n + 1;
    lbl_802C5578[0] = handle;
    val = lbl_80343F60[(t->flags >> 2) & 3];
    if (obj->clamp != val) {
        obj->clamp = val;
        obj->clampp = &obj->regs[n];
        obj->regs[n].hi = val;
        obj->regs[n].lo = 0;
        obj->regid[n] = 8;
        n = n + 1;
    }
    obj->m50 = (g->texCtx->m2c0 << 5) | ((g->texCtx->m2bc & 1) << 6);
    obj->nregs = n;
    if (t->flags & 0x40) {
        obj->dirty = 4;
        obj->lmap = (u32)(lbl_802C2E28 + t->lmap * 16);
    } else if (obj->lmap != 0) {
        obj->dirty = 4;
        obj->lmap = 0;
    }
}

/* Emit a lightmap texture packet + reset the shared texture regs
 * (Xbox candidate: setLmapInfo). */
void fn_800C68F4(u32* pkt, s32 x, u32 y)
{
    PbDrawState* st = &lbl_802C5430;
    pkt[0] = 0x5C000;
    pkt[1] = 0x89;
    pkt[12] = lbl_80343F48;
    st->f188 = (y & 0xFFFF) | (x << 16);
    st->f198 = 0;
    st->f1a0 = 0;
    st->f190 = 0;
    st->f194 = 4;
    st->f19c = 0;
    st->f1a1 = 1;
    st->f1a4 = 6;
    st->f150 = 4;
    st->f154 = 5;
}

/* Default-hook installers: point gWinGlobals sub-blocks at the local
 * defaults (called from the Matching pb_global.c -- keep fn_ names). */
void fn_800C6960(void)
{
    PbORGlobals* g = gWinGlobals;
    if (g->hook14) {
        return;
    }
    asm {}
    g->hook14 = lbl_803450B8;
}

void fn_800C697C(void)
{
    gWinGlobals->hook14 = lbl_803450B8;
}

void fn_800C698C(void)
{
    PbORGlobals* g = gWinGlobals;
    if (g->hook20) {
        return;
    }
    asm {}
    g->hook20 = lbl_803450C0;
}

void fn_800C69A8(void)
{
    gWinGlobals->hook20 = lbl_803450C0;
}

void fn_800C69B8(void)
{
    PbORGlobals* g = gWinGlobals;
    if (g->hook24) {
        return;
    }
    asm {}
    g->hook24 = lbl_803450C8;
}

void fn_800C69D4(void)
{
    gWinGlobals->hook24 = lbl_803450C8;
}

void fn_800C69E4(void)
{
    PbORGlobals* g = gWinGlobals;
    if (g->hook34) {
        return;
    }
    asm {}
    g->hook34 = lbl_803450D0;
}

void fn_800C6A00(void)
{
    gWinGlobals->hook34 = lbl_803450D0;
}

/* Activate a texture by handle: GXLoadTexObj + width/height shadow. */
void pbSetTexture(u32 handle, u32 flag)
{
    u8 unused[8];
    u8* banks = (u8*)gWinGlobals->banks;
    PbTexBank* bank = *(PbTexBank**)(banks + (s16)(handle >> 16) * 16 + 4);
    PbTexEntry* t =
        (PbTexEntry*)((u8*)bank->entries + (s16)(handle & 0xFFFF) * 16);

    if (t->slot != -1) {
        u8* texObj = (*(PbTexBank**)(banks + t->bank * 16 + 4))->texObjs;
        texObj += t->slot * 48;
        fn_800C7558(handle);
        GXLoadTexObj(texObj, flag);
        lbl_80343F70 = t->w;
        lbl_80343F74 = t->h;
    }
}

/* One-time init of the texture shadow state. */
void fn_800C6AB4(void)
{
    if (lbl_8034511C) {
        return;
    }
    lbl_80345118 = -1;
    lbl_8034511C = 1;
}
