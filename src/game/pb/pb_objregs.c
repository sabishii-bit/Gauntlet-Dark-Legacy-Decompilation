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
extern void vec4Apply__FR4vec4R4vec4R5mat44(f32* dst, f32* src, f32* m);
extern void vec4Normalize__FR4vec4R4vec4(f32* dst, f32* src);
extern void vec4Cross__FR4vec4R4vec4R4vec4(f32* dst, f32* a, f32* b);
extern void vec4Sub__FR4vec4R4vec4R4vec4(f32* dst, f32* a, f32* b);
extern f32 vec3LengthSquared__FR4vec3(f32* v);
extern void __as__4vec4FRC4vec4(f32* dst, f32* src);
extern f32 atan(f32 value);
extern f32 atan2(f32 y, f32 x);
extern f64 __frsqrte(f64 value);

static inline f32 pbSqrtAccurate(f32 value)
{
    struct {
        f32 pad[2];
        volatile f32 result;
    } local;

    if (value > 0.0f) {
        f64 guess = __frsqrte((f64)value);
        guess = 0.5 * guess * (3.0 - guess * guess * value);
        guess = 0.5 * guess * (3.0 - guess * guess * value);
        guess = 0.5 * guess * (3.0 - guess * guess * value);
        guess = 0.5 * guess * (3.0 - guess * guess * value);
        local.result = (f32)(value * guess);
        return local.result;
    }
    return value;
}

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
    u8   _pad84[0x34];
    s32  mb8;                  /* 0xB8 */
    s32  mbc;                  /* 0xBC */
    s32  mc0;                  /* 0xC0 */
    s32  mc4;                  /* 0xC4 */
    s32  mc8;                  /* 0xC8 */
    s32  mcc, md0, md4;        /* 0xCC */
    s32  md8;                  /* 0xD8 */
    f32  fdc;                  /* 0xDC */
    s32  me0, me4;             /* 0xE0 */
    s32  me8;                  /* 0xE8 */
    u32  mec;                  /* 0xEC */
    u32  mf0;                  /* 0xF0 */
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
    s32 m1c;     /* 0x1C */
    s32 m20;     /* 0x20 */
    s32 m24;     /* 0x24 */
    u8  _pad28[4];
    f32 f2c;     /* 0x2C */
    f32 f30;     /* 0x30 */
    f32 f34;     /* 0x34 */
    f32 f38;     /* 0x38 */
    s32 m3c;     /* 0x3C */
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
extern char lbl_80348F8C;            /* "\n" (sdata2) */
extern char lbl_801168D8[];          /* pbSetDORegs vector-debug strings */

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
extern f32 lbl_802C2A28[256];        /* material/chrome parameter table */
extern f32 lbl_802C5528[4];          /* transformed right vector */
extern f32 lbl_802C5538[4];          /* transformed up vector */

/* --- sbss --- */
extern u32 lbl_803450B8[2];          /* default hook14 block */
extern u32 lbl_803450C0[2];          /* default hook20 block */
extern u32 lbl_803450C8[2];          /* default hook24 block */
extern u32 lbl_803450D0[2];          /* default hook34 block */
extern s32 lbl_803450F0;             /* shared-bank texture index */
extern s32 lbl_80345118;
extern s8  lbl_8034511C;
extern f32 gVpScaleY;

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

/* --- sDrawGeom support ------------------------------------------------ */
extern void vec4Mul__FR4vec4R4vec4R4vec4(f32* dst, f32* a, f32* b);
extern void vec4Sub__FR4vec4R4vec4R4vec4(f32* dst, f32* a, f32* b);
extern void vec4Add__FR4vec4R4vec4R4vec4(f32* dst, f32* a, f32* b);
extern void vec4Scale__FR4vec4R4vec4f(f32* dst, f32* src, f32 s);
extern void vec4Div__FR4vec4R4vec4f(f32* dst, f32* src, f32 s);
extern void vec3Mul__FR4vec3R4vec3R4vec3(f32* dst, f32* a, f32* b);
extern void vec3Scale__FR4vec3R4vec3f(f32* dst, f32* src, f32 s);
extern void vec3Div__FR4vec3R4vec3f(f32* dst, f32* src, f32 s);
extern void vec3Clamp__FR4vec4R4vec4ff(f32* dst, f32* src, f32 lo, f32 hi);
extern void vec4FTOI__FPlR4vec4(s32* dst, f32* src);
extern f32 DotProduct__8Math3D_BFR4vec3R4vec3(f32* a, f32* b);
extern void sceSamp0InversMatrix(f32* dst, f32* src);
extern void sceSamp0MulMatrix(f32* dst, f32* a, f32* b);
extern void sceSamp0MultVec(f32* dst, f32* m, f32* v);

extern f32 gCameraMtx[16];    /* camera (view) matrix */
extern f32 lbl_802C9B38[16];  /* light-rotation matrix */
extern f32 lbl_802C9AF8[16];  /* directional light colour / ambient block */
extern u8 lbl_80128290[];     /* texture scale/offset table (+0xF0/+0x100) */

/* saved per-call shadow state (sbss) */
extern f64 lbl_80345060, lbl_80345078, lbl_80345080, lbl_80345088;
extern f32 lbl_8034504C, lbl_80345054, lbl_80345068, lbl_8034506C;
extern u32 lbl_80345044, lbl_80345050, lbl_80345058, lbl_80345070, lbl_80345074;
extern s32 lbl_80345048;
extern u8 lbl_80345040;       /* chrome/env-map path flag */
extern u8 lbl_80345041;       /* fresnel/curve path flag */
extern u32 lbl_80345090, lbl_80345094, lbl_80345098, lbl_8034509C;
extern u16 lbl_803450A0, lbl_803450A2, lbl_803450A4, lbl_803450A6;
extern s32 lbl_80345030;      /* draw-serial reset */
extern s32 lbl_80345130;      /* draw-serial counter */
extern f32 lbl_80343F54;      /* current draw param shadow */
extern u16 lbl_80343F64, lbl_80343F66; /* debug object-highlight window */
extern u16 lbl_80343F68;      /* debug packet-highlight limit */
extern u16 lbl_80343F6A;      /* debug vertex-highlight limit */

/* float constants (sdata2 pool) */
extern f32 lbl_80348F50;      /* 1.0 */
extern f32 lbl_80348F54;      /* light prescale divisor */
extern f32 lbl_80348F58;      /* texcoord fixed-point divisor */
extern f32 lbl_80348F5C;      /* packed-normal divisor */
extern f32 lbl_80348F60;      /* 0.0 */
extern f32 lbl_80348F64;      /* curve linear coefficient */
extern f32 lbl_80348F68;      /* curve cubic coefficient */
extern f32 lbl_80348F6C;      /* position fixed-point divisor */
extern f32 lbl_80348F70;      /* z-bias divisor */
extern f32 lbl_80348F74;      /* colour clamp max (255.0) */
extern f64 lbl_80348F90;
extern f64 lbl_80348F98;
extern f64 lbl_80348FA0;
extern f64 lbl_80348FA8;
extern f64 lbl_80348FB0;
extern f64 lbl_80348FB8;

static u32 pbSwap32(u32 x)
{
    union {
        u32 w;
        u8 b[4];
    } a, b;
    a.w = x;
    b.b[0] = a.b[3];
    b.b[1] = a.b[2];
    b.b[2] = a.b[1];
    b.b[3] = a.b[0];
    return b.w;
}

static u16 pbSwap16(u16 x)
{
    union {
        u16 h;
        u8 b[2];
    } a;
    a.h = x;
    return (u16)(a.b[1] | (a.b[0] << 8));
}

void pbDrawVerts(s32 count, u8* verts);

/* Big geometry pipeline: parse the PS2-format GIF packet stream (byte
 * swapped), transform + light each vertex into the shared PbVtx buffer at
 * lbl_802C5430+0x1A8 and flush the strips through pbDrawVerts. */
void sDrawGeom(u32* data, f32* mtx, u8* s, u32 flags)
{
    u8* st = (u8*)&lbl_802C5430;
    u8* tbl = lbl_80128290;
    f32 inv[16];        /* inverse object matrix */
    PbGfxEnv envSave;   /* saved env block st+0x128 */
    PbGfxEnv texSave;   /* saved tex block st+0x168 */
    f32 amb[4];         /* ambient accumulator */
    f32 tc[4];          /* decoded texcoords */
    f32 nrm[4];         /* decoded packed normal */
    f32 uv2[3];         /* decoded second uv set */
    f32 pos[4];         /* decoded position */
    f32 xf[4];          /* transformed position */
    f32 lit[4];         /* lit colour */
    f32 ta[4];
    f32 tb[4];
    f32 diff[4];
    f32 sc[4];
    s32 iout[4];
    f32* litbase;
    u32 hdr;
    u32 end;
    u32 idx;
    s32 pkt;
    u8 clipFlag;
    u8 pktClip;
    u8 vClip;
    u8 kick;
    u8 spec;
    u8 flat;
    u32 tag;
    u32 cnt;
    u32 tmp;
    f32 qv;
    u32 posw;
    s32 pfmt;
    s32 cfmt;
    u32 step;
    u32 posStride;
    u32 colStride;
    u8* posPtr;
    u8* nrmPtr;
    u8* uv2Ptr;
    u8* colPtr;
    u32 outOff;
    u32 v;
    u16 nv;
    s32 i;
    f32 x;
    f32 zero;
    f32 one;
    f32 normalDiv;
    f32 curveLinear;
    f32 curveCubic;
    f32 zBiasDiv;
    f32 colorMax;

    lbl_80345080 = *(f64*)(s + 0x60);
    *(u32*)(st + 0x128) = *(u32*)(st + 0x148);
    lbl_80345088 = *(f64*)(s + 0x50);
    spec = (*(u32*)(s + 0x78) & 0x80) != 0;
    st[0x14C] = 1;
    st[0x14D] = 1;
    st[0x14E] = 0;
    if (flags & 4) {
        lbl_80345068 = *(f32*)(s + 0x38);
        lbl_80345054 = (f32)*(u32*)(s + 0x40);
        lbl_8034504C = *(f32*)(s + 0x48);
        lbl_80343F54 = *(f32*)(s + 0x4C);
        *(f32*)(tbl + 0x100) = *(f32*)(s + 0x28);
        *(f32*)(tbl + 0x104) = *(f32*)(s + 0x2C);
        lbl_80345058 = *(u32*)(s + 0x44);
        lbl_80345060 = *(f64*)(s + 0x30);
        __as__4vec4FRC4vec4((f32*)(st + 0x118), (f32*)(s + 0x10));
        *(f32*)(tbl + 0xF0) = *(f32*)(s + 0x20);
        *(f32*)(tbl + 0xF4) = *(f32*)(s + 0x24);
        *(PbGfxEnv*)(st + 0x128) = *(PbGfxEnv*)(st + 0x148);
    }
    if (flags & 8) {
        lbl_80345070 = *(u32*)(s + 0x0);
        lbl_80345074 = *(u32*)(s + 0x4);
        lbl_80345078 = *(f64*)(s + 0x8);
        *(PbGfxEnv*)(st + 0x168) = *(PbGfxEnv*)(st + 0x188);
    }
    if (mtx != 0) {
        sceSamp0InversMatrix(inv, mtx);
        sceSamp0MulMatrix((f32*)(st + 0x1F88), gCameraMtx, inv);
        sceSamp0MulMatrix((f32*)(st + 0x1FC8), lbl_802C9B38, inv);
        __as__4vec4FRC4vec4((f32*)(st + 0x1F48), lbl_802C9AF8);
        __as__4vec4FRC4vec4((f32*)(st + 0x1F78), (f32*)(st + 0x118));
        lbl_80345044 = *(u32*)(s + 0x74);
        lbl_80345048 = *(s32*)(s + 0x70);
        lbl_8034506C = lbl_80345068;
        lbl_80345041 = (*(u32*)(s + 0x78) & 0x20) != 0;
        lbl_80345050 = *(u32*)(s + 0x78);
        __as__4vec4FRC4vec4(amb, lbl_802C9AF8 + 12);
        vec4Mul__FR4vec4R4vec4R4vec4((f32*)(st + 0x1F48), (f32*)(st + 0x1F48),
                                     (f32*)(st + 0x1F78));
        lbl_80345040 = (*(u32*)(s + 0x78) & 2) != 0;
        if (lbl_80345040 == 0) {
            for (i = 0; i < 3; i++) {
                amb[i] += lbl_8034504C;
            }
            if (!(*(u32*)(s + 0x78) & 1)) {
                vec3Mul__FR4vec3R4vec3R4vec3((f32*)(st + 0x1F78),
                                             (f32*)(st + 0x1F78), amb);
            } else {
                vec3Scale__FR4vec3R4vec3f((f32*)(st + 0x1F78),
                                          (f32*)(st + 0x1F78), lbl_8034504C);
                vec4Sub__FR4vec4R4vec4R4vec4((f32*)(st + 0x1F48),
                                             (f32*)(st + 0x1F48),
                                             (f32*)(st + 0x1F48));
            }
        } else {
            vec3Scale__FR4vec3R4vec3f((f32*)(st + 0x1F78), (f32*)(st + 0x1F78),
                                      lbl_8034504C);
            vec3Div__FR4vec3R4vec3f((f32*)(st + 0x1F78), (f32*)(st + 0x1F78),
                                    lbl_80348F54);
        }
    }
    lbl_80345094 = lbl_80345090;
    lbl_8034509C = lbl_80345098;
    lbl_803450A2 = lbl_803450A0;
    if (spec || lbl_80345040) {
        envSave = *(PbGfxEnv*)(st + 0x128);
        texSave = *(PbGfxEnv*)(st + 0x168);
    }
    if (lbl_80345040) {
        SetMultiPassTextureParams(1);
        SetVertexFormat(1);
    } else if (spec) {
        SetMultiPassTextureParams(2);
        SetVertexFormat(0);
    } else {
        SetMultiPassTextureParams(0);
        SetVertexFormat(0);
    }
    if (spec || lbl_80345040) {
        fn_800C7928(*(u32*)(st + 0x168), 1);
    }
    sSetGFXEnv((PbGfxEnv*)(st + 0x128));

    hdr = pbSwap32(data[0]);
    clipFlag = 0;
    if (lbl_803450A2 >= lbl_80343F64 && lbl_803450A2 < lbl_80343F66) {
        clipFlag = 1;
    }
    litbase = (f32*)(st + 0x1F78);
    end = ((hdr & 0xFFFF) + 1) * 4;
    idx = 2;
    pkt = 0;
    zero = lbl_80348F60;
    one = lbl_80348F50;
    normalDiv = lbl_80348F5C;
    curveLinear = lbl_80348F64;
    curveCubic = lbl_80348F68;
    zBiasDiv = lbl_80348F70;
    colorMax = lbl_80348F74;
    while (idx < end) {
        pktClip = clipFlag;
        if (!(pkt >= (s32)lbl_803450A4 && pkt < (s32)lbl_80343F68)) {
            pktClip = 0;
        }
        tag = pbSwap32(data[idx]);
        if (tag == 0) {
            break;
        }
        cnt = pbSwap32(data[idx + 1]);
        posPtr = (u8*)(data + idx + 6);
        idx += 5;
        tmp = pbSwap32(data[idx - 2]);
        qv = *(f32*)&tmp;
        flat = (one == qv);
        posw = pbSwap32(data[idx]);
        pfmt = posw >> 24;
        if (pfmt == 0x69) {
            idx += (((cnt + 1) * 0x30 + 0x1F) >> 5);
            posStride = 6;
            idx += 1;
        } else if (pfmt == 0x6A) {
            idx += (((cnt + 1) * 0x18 + 0x1F) >> 5);
            posStride = 3;
            idx += 1;
        } else {
            idx += (cnt + 1) * 3;
            posStride = 12;
            idx += 1;
        }
        tmp = pbSwap32(data[idx]);
        step = (((cnt << 4) + 0x1F) >> 5) + 1;
        nrmPtr = (u8*)(data + idx + 1);
        idx += step;
        tmp = pbSwap32(data[idx]);
        uv2Ptr = 0;
        if ((tmp & 0xFFF) == 3) {
            uv2Ptr = (u8*)(data + idx + 1);
            idx += step;
        }
        tmp = pbSwap32(data[idx]);
        colPtr = (u8*)(data + idx + 1);
        cfmt = tmp >> 24;
        if (cfmt == 0x6D) {
            colStride = 8;
            idx += cnt * 2 + 1;
        } else if (cfmt == 0x66) {
            colStride = 2;
            idx += step;
        } else {
            colStride = 4;
            idx += cnt + 1;
        }
        tmp = pbSwap32(data[idx]);
        outOff = 0;
        v = 0;
        idx += 1;
        pkt += 1;
        for (; v < cnt; v++) {
            vClip = pktClip;
            if (!(v >= lbl_803450A6 && v < lbl_80343F6A)) {
                vClip = 0;
            }
            /* texcoord decode */
            if (cfmt == 0x6D) {
                tc[0] = (f32)pbSwap16(*(u16*)(colPtr + 0));
                tc[1] = (f32)pbSwap16(*(u16*)(colPtr + 2));
                tc[2] = (f32)pbSwap16(*(u16*)(colPtr + 4));
                tc[3] = (f32)pbSwap16(*(u16*)(colPtr + 6));
            } else if (cfmt == 0x66) {
                tc[0] = (f32)colPtr[0];
                tc[1] = (f32)colPtr[1];
            } else {
                tc[0] = (f32)pbSwap16(*(u16*)(colPtr + 0));
                tc[1] = (f32)pbSwap16(*(u16*)(colPtr + 2));
            }
            vec4Div__FR4vec4R4vec4f(tc, tc, lbl_80348F58);
            /* packed normal + strip-kick bit */
            nv = pbSwap16(*(u16*)nrmPtr);
            nrm[0] = (f32)((s32)(s16)(nv & 0x1F) - 15) / normalDiv;
            nrm[1] = (f32)((s32)(s16)((nv >> 5) & 0x1F) - 15) / normalDiv;
            nrm[2] = (f32)((s32)(s16)((nv >> 10) & 0x1F) - 15) / normalDiv;
            nrm[3] = zero;
            kick = ((nv >> 15) & 1) != 0;
            if (uv2Ptr != 0) {
                u16 v2 = pbSwap16(*(u16*)uv2Ptr);
                uv2Ptr += 2;
                uv2[0] = (f32)((v2 & 0x1F) << 3);
                uv2[1] = (f32)((v2 >> 2) & 0xF8);
                uv2[2] = (f32)((v2 >> 7) & 0xF8);
            }
            if (lbl_80345041) {
                vec4Mul__FR4vec4R4vec4R4vec4(ta, (f32*)(st + 0xF8), nrm);
                vec4Mul__FR4vec4R4vec4R4vec4(tb, (f32*)(st + 0x108), nrm);
                tc[0] = ta[2] + (ta[0] + ta[1]);
                tc[1] = tb[2] + (tb[0] + tb[1]);
                if (lbl_80345050 & 0x40) {
                    x = tc[0];
                    tc[0] = curveLinear * x + curveCubic * (x * (x * x));
                    x = tc[1];
                    tc[1] = curveLinear * x + curveCubic * (x * (x * x));
                }
            }
            tc[0] = tc[0] * *(f32*)(tbl + 0xF0) + *(f32*)(tbl + 0x100);
            tc[1] = tc[1] * *(f32*)(tbl + 0xF4) + *(f32*)(tbl + 0x104);
            /* position decode */
            if (pfmt == 0x69) {
                pos[0] = (f32)(s16)pbSwap16(((u16*)posPtr)[0]);
                pos[1] = (f32)(s16)pbSwap16(((u16*)posPtr)[1]);
                pos[2] = (f32)(s16)pbSwap16(((u16*)posPtr)[2]);
            } else if (pfmt == 0x6A) {
                pos[0] = (f32)(s8)posPtr[0];
                pos[1] = (f32)(s8)posPtr[1];
                pos[2] = (f32)(s8)posPtr[2];
            } else {
                pos[0] = (f32)(s32)pbSwap32(((u32*)posPtr)[0]);
                pos[1] = (f32)(s32)pbSwap32(((u32*)posPtr)[1]);
                pos[2] = (f32)(s32)pbSwap32(((u32*)posPtr)[2]);
            }
            vec3Div__FR4vec3R4vec3f(pos, pos, lbl_80348F6C);
            sceSamp0MultVec(xf, (f32*)(st + 0x1F88), pos);
            xf[2] = xf[2] + lbl_8034506C / zBiasDiv;
            /* lighting */
            if (lbl_80345040) {
                __as__4vec4FRC4vec4(lit, litbase);
                vec3Mul__FR4vec3R4vec3R4vec3(lit, lit, uv2);
            } else {
                f32 d = DotProduct__8Math3D_BFR4vec3R4vec3((f32*)(st + 0x1FC8),
                                                           nrm);
                if (d < zero) {
                    d = zero;
                }
                if (d > one) {
                    d = one;
                }
                vec4Scale__FR4vec4R4vec4f(lit, (f32*)(st + 0x1F48), d);
                vec4Add__FR4vec4R4vec4R4vec4(lit, lit, litbase);
            }
            vec3Clamp__FR4vec4R4vec4ff(lit, lit, lbl_80348F60,
                                       lbl_80348F74);
            if (vClip) {
                lit[2] = zero;
                lit[1] = zero;
                lit[0] = zero;
                lit[3] = colorMax;
            }
            if (lbl_80345044 != 0) {
                u32 lo = 0;
                s32 li;
                for (li = 0; li < lbl_80345048; li++, lo += 0x20) {
                    u8* L = (u8*)lbl_80345044 + lo;
                    f32 d;
                    f32 dd;
                    f32 att;
                    vec4Sub__FR4vec4R4vec4R4vec4(diff, (f32*)L, pos);
                    d = DotProduct__8Math3D_BFR4vec3R4vec3(diff, nrm);
                    dd = DotProduct__8Math3D_BFR4vec3R4vec3(diff, diff);
                    att = -(*(f32*)(L + 0xC) * dd - one);
                    if (d > zero && att > zero) {
                        f32 w = *(f32*)(L + 0x1C) * att;
                        w = d * w / dd;
                        if (w > one) {
                            w = one;
                        }
                        vec4Scale__FR4vec4R4vec4f(sc, (f32*)(L + 0x10), w);
                        sc[3] = zero;
                        vec4Add__FR4vec4R4vec4R4vec4(lit, lit, sc);
                    }
                }
                vec3Clamp__FR4vec4R4vec4ff(lit, lit, lbl_80348F60,
                                           lbl_80348F74);
            }
            vec4FTOI__FPlR4vec4(iout, lit);
            {
                u8* out = st + outOff + 0x1A8;
                out[0] = (u8)iout[0];
                flat = flat == 0;
                out[1] = (u8)iout[1];
                posPtr += posStride;
                colPtr += colStride;
                out[2] = (u8)iout[2];
                out[3] = (u8)iout[3];
                *(f32*)(out + 0x04) = tc[0];
                *(f32*)(out + 0x08) = tc[1];
                *(f32*)(out + 0x0C) = tc[2];
                *(f32*)(out + 0x10) = tc[3];
                *(f32*)(out + 0x14) = xf[0];
                *(f32*)(out + 0x18) = xf[1];
                *(f32*)(out + 0x1C) = -xf[2];
                out[0x20] = kick;
                out[0x21] = (u8)flat;
            }
            nrmPtr += 2;
            outOff += 0x24;
        }
        /* flush strips: restart wherever a vertex carries the kick bit */
        {
            s32 start = 0;
            s32 j;
            u32 off = 0;
            for (j = 0; j < (s32)cnt; j++, off += 0x24) {
                if (st[off + 0x1C8] != 0 && (j - start) > 1) {
                    pbDrawVerts(j - start, st + start * 0x24 + 0x1A8);
                    start = j - 1;
                }
            }
            pbDrawVerts((s32)cnt - start, st + start * 0x24 + 0x1A8);
        }
    }
    if (spec || lbl_80345040) {
        *(PbGfxEnv*)(st + 0x128) = envSave;
        *(PbGfxEnv*)(st + 0x168) = texSave;
    }
    if (lbl_80345130 == 0) {
        lbl_80345030 = -1;
    }
    lbl_80345130 = lbl_80345130 + 1;
}

void pbSetDODrawRegs(PbDOObj* obj, u32 handle);
void fn_800C5D44(u32* pkt, s32 xy);
s32 fn_800C5DA8(PbDOObj* obj, s32 arg, u8* node, f32* matrix);
void fn_800C6350(PbDOObj* obj, s32 flags, u32 mask, u8* node);
void fn_800C64A4(PbDOObj* obj, u32 flags, u8* node);
void setTexShift(PbDOObj* obj, f32* sh, f32* alt, s32 chrome);
static void setLmapInfo(u32* pkt, s32 x, u32 y);

/* Apply the object/material state deltas and submit one geometry stream
 * (Xbox: pbSetDORegs). */
#pragma opt_lifetimes off
s32 pbSetDORegs(s32 unused, u32 texture, s32 textureMode, u32 material,
                u32 flags, s32 bank, f32* matrix, void* geometry, u8* node)
{
    u8 unusedStack[32];
    PbORGlobals* globals = gWinGlobals;
    PbDoRegs* state = lbl_80343F4C;
    s32 drawFlags = 0;
    u32 changedFlags;
    s32 textureShift;
    s32 textureShiftFlag;
    s32 matrixFlag;
    f32 viewportScale;

    state->mb8 = 0;
    state->md8 = 0;
    lbl_80345090 = bank;
    lbl_80345098 = texture;
    if (node != 0) {
        lbl_803450A0 = *(u16*)(node + 0x50);
    }

    if (state->mc4 != texture || state->mc8 != textureMode) {
        state->mc4 = texture;
        state->mc8 = textureMode;
        pbSetDODrawRegs((PbDOObj*)state, texture);
    }

    if (node != 0) {
        state->m80 = (s32)node;
        if (*(s32*)(globals->lights + 0x7C) != 0) {
            state->m78 |= 0x40;
        } else {
            state->m78 &= ~0x40;
        }

        if (material != 0 && (flags & 0x4000) == 0) {
            s32 value;
            value = bank << 16;
            value = (value & 0xFFFF0000) | (material & 0xFFFF);
            state->m78 |= 2;
            if (state->mcc != value) {
                state->mcc = value;
                setLmapInfo((u32*)state, bank, material);
                drawFlags = 8;
            }
            flags |= 0x2000;
        } else {
            state->m78 &= ~2;
            state->mcc = -1;
        }

        if (state->md0 == -1) {
            changedFlags = 0x189AF7C0;
        } else {
            changedFlags = flags ^ state->md0;
            changedFlags |= (flags | state->md0) & 0x181A0700;
        }
        state->md0 = flags;

        if (state->fdc != (f32)*(s16*)(node + 0x6A) ||
            (changedFlags & 0x7100) != 0) {
            changedFlags &= ~0x7100;
            state->fdc = (f32)*(s16*)(node + 0x6A);
            fn_800C64A4((PbDOObj*)state, flags, node);
        }

        if ((changedFlags & 0x100600) != 0) {
            changedFlags &= ~0x100600;
            if ((flags & 0x100600) != 0) {
                state->f1c = (f32)*(u8*)(node + 0x53);
                state->m78 |= 0x10;
            } else {
                state->f1c = lbl_80348F74;
                state->m78 &= ~0x10;
            }
            state->mbc = 4;
        }

        if ((changedFlags & 0x10000000) != 0) {
            changedFlags &= ~0x10000000;
            if ((flags & 0x10000000) != 0) {
                state->mf0 = (u32)&lbl_802C2A28[*(u8*)(node + 0x5E) * 4];
                state->mbc = 4;
            } else if (state->mf0 != 0) {
                state->mf0 = 0;
                state->mbc = 4;
            }
        }

        if ((changedFlags & 0xA0000) != 0) {
            changedFlags &= ~0xA0000;
            if ((flags & 0xA0000) != 0) {
                state->me0 = 1;
                state->m78 |= 0x20;
                fn_800C5DA8((PbDOObj*)state, 0, node, matrix);
                state->mcc = -1;
                state->md0 = -1;
            } else {
                state->me0 = 0;
                state->m78 &= ~0x20;
            }
        }

        if ((changedFlags & 0x08000000) != 0) {
            changedFlags &= ~0x08000000;
            if ((flags & 0x08000000) != 0) {
                state->me4 = *(s32*)(node + 0x58);
                fn_800C5D44((u32*)state, state->me4);
                state->mcc = -1;
                drawFlags = 8;
                state->m78 |= 0x80;
            } else {
                state->m78 &= ~0x80;
                state->me4 = 0;
            }
        }

        if (state->mc0 != *(s16*)(node + 0x68)) {
            state->mc0 = *(s16*)(node + 0x68);
            state->f38 = (f32)*(s16*)(node + 0x68) *
                         *(f32*)(globals->lights + 0x78);
            state->mbc = 4;
        }
        if (changedFlags != 0) {
            fn_800C6350((PbDOObj*)state, flags, changedFlags, node);
        }
    } else if (state->me4 != 0) {
        fn_800C5D44((u32*)state, state->me4);
        drawFlags = 8;
    } else if (material != 0) {
        s32 value;
        value = bank << 16;
        value = (value & 0xFFFF0000) | (material & 0xFFFF);
        if (state->mcc != value) {
            state->mcc = value;
            setLmapInfo((u32*)state, bank, material);
            drawFlags = 8;
        }
    }

    textureShift = state->mb8;
    if (state->mbc != 0) {
        setTexShift((PbDOObj*)state, (f32*)state->mec, (f32*)state->mf0,
                    state->m78 & 0x20);
        drawFlags += state->mbc;
        state->mbc = 0;
    }
    if (textureShift != 0) {
        textureShiftFlag = 1;
    } else {
        textureShiftFlag = 0;
    }
    if (matrix != 0) {
        matrixFlag = 2;
    } else {
        matrixFlag = 0;
    }
    state->m58 = textureShift;
    drawFlags = textureShiftFlag + drawFlags;
    drawFlags = matrixFlag + drawFlags;

    if (!sPerspModeInit) {
        sPerspMode = 0;
        sPerspModeInit = 1;
    }
    if (sPerspMode != 1) {
        sPerspMode = 1;
        GXSetProjection(gPerspProjMtx, 0);
    }

    viewportScale = gVpScaleY;
    if (!sVpHeightInit) {
        sVpHeightInit = 1;
        sVpHeightScale = lbl_80348F50;
    }
    if (viewportScale != sVpHeightScale) {
        sVpHeightScale = viewportScale;
        GXSetViewport(gScreenData[0], gScreenData[1], gScreenData[2],
                      viewportScale * gScreenData[3], gScreenData[4],
                      gScreenData[5]);
        GXSetScissor((u32)gScreenData[0], (u32)gScreenData[1],
                     (u32)gScreenData[2],
                     (u32)(viewportScale * gScreenData[3]));
    }

    sDrawGeom((u32*)geometry, matrix, (u8*)state, drawFlags);
    return 0;
}
#pragma opt_lifetimes reset

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
#pragma opt_propagation off
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
#pragma opt_propagation reset

/* Build the object-space texture basis and the draw-register packet. */
s32 fn_800C5DA8(PbDOObj* obj, s32 arg, u8* node, f32* matrix)
{
    PbORGlobals* g = gWinGlobals;
    f32 inverse[16];
    f32 look[4];
    f32 right[4];
    f32 up[4];
    f32 savedLook[4];
    f32 savedRight[4];
    f32 savedUp[4];
    s32 debugFlags;

    (void)node;
    debugFlags = 0;
    mat44InvRigid__FR5mat44R5mat44(inverse, matrix);

    if (lbl_80343F50->m3c != 0) {
        lbl_80343F50->m44++;
        if (lbl_80343F50->m44 > 20) {
            lbl_80343F50->m44 = 0;
            arg = lbl_80343F50->dbg;
            debugFlags = arg;
            if (arg != 0) {
                bulletproof_printf(&lbl_80348F8C);
            }
        }
    } else {
        lbl_80343F50->m44 = 1;
    }

    if (lbl_80343F50->m1c != 0) {
        vec4Sub__FR4vec4R4vec4R4vec4(look, (f32*)g->scr + 6,
                                     matrix + 12);
        vec4Cross__FR4vec4R4vec4R4vec4(right, look, (f32*)g->scr + 14);
        vec4Cross__FR4vec4R4vec4R4vec4(up, right, look);
        if (debugFlags != 0) {
            if (debugFlags & 1) {
                bulletproof_printf("w look=<%6.3Lf %6.3Lf %6.3Lf>  ",
                                   look[0], look[1], look[2]);
            }
            if (debugFlags & 0x40) {
                bulletproof_printf("w up=<%6.3Lf %6.3Lf %6.3Lf>  ",
                                   up[0], up[1], up[2]);
            }
            if (debugFlags & 2) {
                bulletproof_printf("w right=<%6.3Lf %6.3Lf %6.3Lf>  ",
                                   right[0], right[1], right[2]);
            }
        }
    } else {
        right[0] = lbl_80348F50;
        right[1] = lbl_80348F60;
        right[2] = lbl_80348F60;
        up[0] = lbl_80348F60;
        up[1] = lbl_80348F50;
        up[2] = lbl_80348F60;
        if (lbl_80343F50->m20 != 0) {
            vec4Apply__FR4vec4R4vec4R5mat44(
                right, right, (f32*)g->scr + 0x90);
            vec4Apply__FR4vec4R4vec4R5mat44(
                up, up, (f32*)g->scr + 0x90);
        } else {
            vec4Apply__FR4vec4R4vec4R5mat44(
                right, right, (f32*)g->scr + 0x80);
            vec4Apply__FR4vec4R4vec4R5mat44(
                up, up, (f32*)g->scr + 0x80);
        }
    if (debugFlags != 0) {
            if (debugFlags & 0x80) {
                bulletproof_printf("c up=<%6.3Lf %6.3Lf %6.3Lf>  ",
                                   up[0], up[1], up[2]);
            }
            if (debugFlags & 4) {
                bulletproof_printf("c right=<%6.3Lf %6.3Lf %6.3Lf>  ",
                                   right[0], right[1], right[2]);
            }
        }
    }

    if (lbl_80343F50->m24 != 0) {
        vec4Normalize__FR4vec4R4vec4(right, right);
        vec4Normalize__FR4vec4R4vec4(up, up);
        vec4Normalize__FR4vec4R4vec4(look, look);
    }
    if (lbl_80343F50->m18 != 0) {
        __as__4vec4FRC4vec4(savedRight, right);
        __as__4vec4FRC4vec4(savedUp, up);
        __as__4vec4FRC4vec4(savedLook, look);
    }

    if (lbl_80343F50->m20 != 0) {
        vec4Apply__FR4vec4R4vec4R5mat44(right, right, inverse);
        vec4Apply__FR4vec4R4vec4R5mat44(up, up, inverse);
    } else {
        vec4Apply__FR4vec4R4vec4R5mat44(right, right, matrix);
        vec4Apply__FR4vec4R4vec4R5mat44(up, up, matrix);
    }
    if (debugFlags != 0) {
        if (debugFlags & 0x100) {
            bulletproof_printf("m up=<%6.3Lf %6.3Lf %6.3Lf>  ",
                               up[0], up[1], up[2]);
        }
        if (debugFlags & 8) {
            bulletproof_printf("m right=<%6.3Lf %6.3Lf %6.3Lf>  ",
                               right[0], right[1], right[2]);
        }
    }

    vec4Normalize__FR4vec4R4vec4(right, right);
    vec4Normalize__FR4vec4R4vec4(up, up);
    if (debugFlags != 0) {
        if (debugFlags & 0x200) {
            bulletproof_printf("nm up=<%6.3Lf %6.3Lf %6.3Lf>  ",
                               up[0], up[1], up[2]);
        }
        if (debugFlags & 0x10) {
            bulletproof_printf("nm right=<%6.3Lf %6.3Lf %6.3Lf>  ",
                               right[0], right[1], right[2]);
        }
    }
    if (debugFlags != 0) {
        if (debugFlags & 0x400) {
            bulletproof_printf("s up=<%6.3Lf %6.3Lf %6.3Lf>  ",
                               up[0], up[1], up[2]);
        }
        if (debugFlags & 0x20) {
            bulletproof_printf("s right=<%6.3Lf %6.3Lf %6.3Lf>  ",
                               right[0], right[1], right[2]);
        }
    }

    __as__4vec4FRC4vec4(lbl_802C5528, right);
    __as__4vec4FRC4vec4(lbl_802C5538, up);
    if (obj->clamp != 0) {
        PbRegPair* reg = obj->clampp;
        if (reg != 0) {
            reg->hi = 0;
            reg->lo = 0;
        } else {
            obj->clampp = &obj->regs[obj->nregs];
            {
                s32 regIndex = obj->nregs;

                obj->regs[regIndex].hi = 0;
                obj->regs[regIndex].lo = 0;
            }
            obj->regid[obj->nregs] = 8;
            obj->nregs++;
        }
        obj->clamp = 0;
    }
    obj->dirty = 4;

    if (lbl_80343F50->m18 != 0) {
        f32 yaw;
        f32 length = pbSqrtAccurate(savedLook[0] * savedLook[0] +
                                    savedLook[2] * savedLook[2]);

        ((f32*)g->scr)[18] = atan(savedLook[1] / length);
        yaw = savedLook[2];
        ((f32*)g->scr)[19] = atan2(savedLook[0], yaw);
        if (debugFlags & 0x2000) {
            s32 pitchDegrees;
            s32 yawDegrees;

            yawDegrees = (s32)((180.0 * (f64)((f32*)g->scr)[19]) /
                               3.14159265358979323846);
            pitchDegrees = (s32)((180.0 * (f64)((f32*)g->scr)[18]) /
                                 3.14159265358979323846);
            bulletproof_printf("yaw,pitch= %4d %4d",
                               yawDegrees, pitchDegrees);
        }
        yaw = ((f32*)g->scr)[18];
        g->scr->f48 = (f32)((f64)yaw * lbl_80348FB0);
        yaw = ((f32*)g->scr)[19];
        g->scr->f4c = (f32)((f64)yaw * lbl_80348FB8);
    }
    return 0;
}

/* Apply z-test / blend-test register deltas for an object
 * (Xbox candidate: setTexInfo). */
void fn_800C6350(PbDOObj* obj, s32 flags, u32 mask, u8* node)
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
        s64 t;
        s32 hi;
        t = (hi = flags & 0x800000) ? 0x48 : 0x44;
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
        p = (f32*)(g->lights + 0x90);
        base = 0.0f;
        obj->flags78 &= ~1;
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
#pragma opt_propagation off
static void setLmapInfo(u32* pkt, s32 x, u32 y)
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
#pragma opt_propagation reset

/* Default-hook installers: point gWinGlobals sub-blocks at the local
 * defaults (called from the Matching pb_global.c -- keep fn_ names). */
void fn_800C6960(void)
{
    PbORGlobals* g = gWinGlobals;
    if (g->hook14) {
        return;
    }
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
