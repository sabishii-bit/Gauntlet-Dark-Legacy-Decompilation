#include "types.h"

/* Midway "bulletproof" object / GX register-setup TU (Xbox counterpart:
 * pb_objregs.obj - the D3D render-state layer; on GameCube this is the GX
 * equivalent). Covers the immediate-mode geometry pipeline and the render
 * register setters: cull mode, viewport, projection mode, vertex format,
 * TEV stage setup, the object-draw register block, and the texture-shift
 * debug dump. .text 0x800C3F58-0x800C6AD4.
 *
 * NonMatching: bodies are best-effort reconstructions / stubs. Function and
 * data names come from GX-call fingerprints and the "txsh:" /
 * "pbSetDODrawRegs: Texture not loaded" debug strings; several were confirmed
 * against the disassembly (SetViewportHeight, SetPerspectiveMode, SetCullMode).
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
u32 __cvt_fp2unsigned(f32 v);

/* --- game printf / log (fn_800BC2EC) --- */
extern int pbDebugPrintf(char* fmt, ...);

/* --- window globals (owned by pb_window / pb_global) --- */
extern void* gWinGlobals;

/* --- screen viewport dims { wd, ht, left, top, minz, maxz } --- */
extern f32 gScreenData[6];

/* --- projection matrices (perspective computed by pbProjCalc) --- */
extern f32 gPerspProjMtx[16];        /* lbl_802C9BC8 (bss) */
extern f32 orthoProjMtx[16];         /* lbl_8025EE24 (main globals block) */

/* --- cull-mode lookup table (indexed by state) --- */
extern u32 sCullModeTable[4];        /* lbl_801283B0 (.data) */

/* --- render-state shadow (sbss) --- */
static f32 sVpHeightScale;
static s8  sVpHeightInit;
static s32 sPerspMode;
static s8  sPerspModeInit;
static s32 sCullMode;
static s32 sVtxFormat;
static s32 sNumTevStages;

/* Immediate-mode quad emitter: sets cull from vtx[33], GXBegin(QUADS), and
 * streams position/color/uv into the GX FIFO for `count` verts. */
void pbDrawVerts(s32 count, u8* verts)
{
    if (count < 3) {
        return;
    }
    GXSetCullMode(sCullModeTable[verts[33] + 1]);
    GXBegin(0x98, 0, (u16)count);
}

/* Pixel/blend environment: z-compare, alpha-compare, z-mode, blend, updates. */
void SetGfxEnv(void)
{
    GXSetZCompLoc(1);
    GXSetAlphaCompare(7, 0, 0, 7, 0);
    GXSetZMode(1, 3, 1);
    GXSetBlendMode(1, 4, 5, 0);
    GXSetColorUpdate(1);
    GXSetAlphaUpdate(1);
}

/* Scale the viewport / scissor by a height factor (caches last value). */
void SetViewportHeight(f32 scale)
{
    if (!sVpHeightInit) {
        sVpHeightScale = 1.0f;
        sVpHeightInit = 1;
    }
    if (scale == sVpHeightScale) {
        return;
    }
    sVpHeightScale = scale;
    GXSetViewport(gScreenData[0], scale * gScreenData[3], gScreenData[1],
                  gScreenData[2], gScreenData[4], gScreenData[5]);
    GXSetScissor(__cvt_fp2unsigned(gScreenData[1]),
                 __cvt_fp2unsigned(gScreenData[2]),
                 __cvt_fp2unsigned(scale * gScreenData[3]),
                 __cvt_fp2unsigned(gScreenData[0]));
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

/* Set channel control + vertex descriptor (cached). */
void SetVertexFormat(s32 fmt)
{
    if (fmt != sVtxFormat) {
        sVtxFormat = fmt;
        GXSetChanCtrl(0, 0, 0, 1, 0, 0, 0);
        GXSetVtxDesc(0, 1);
    }
}

/* Configure TEV stages for a texture pass count. */
void SetTevStages(s32 stages)
{
    sNumTevStages = stages;
    GXSetTevOrder(0, 0, 0, 4);
    GXSetTevOp(0, 0);
    GXSetTevColorOp(0, 0, 0, 0, 1, 0);
    GXSetTevColorIn(0, 0xF, 0xF, 0xF, 8);
    GXSetNumTexGens(1);
    GXSetTevAlphaIn(0, 6, 0, 0, 0);
    GXSetTevAlphaOp(0, 0, 0, 0, 1, 0);
    GXSetNumTevStages(stages);
}

/* Big geometry pipeline: transform, light, clip and draw an object's verts. */
void sDrawGeom(void* obj)
{
    (void)obj;
    /* stub: transforms verts through Math3D_B ops then calls pbDrawVerts */
}

/* Debug: print the current texture-UV shift ("txsh: ..."). */
void setTexShift(void)
{
    pbDebugPrintf("txsh: %6.2f %6.2f  %6.3f %6.3f  ", 0.0, 0.0, 0.0, 0.0);
}

/* Set the object draw registers; bails if the object's texture isn't loaded. */
void pbSetDODrawRegs(void* obj)
{
    (void)obj;
    pbDebugPrintf("pbSetDODrawRegs: Texture not loaded");
}

/* Activate a texture object (GXLoadTexObj wrapper). */
void pbSetTexture(void* texObj)
{
    (void)texObj;
    /* stub */
}
