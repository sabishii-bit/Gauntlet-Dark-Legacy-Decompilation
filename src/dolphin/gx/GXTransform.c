#include "types.h"
#include "dolphin/gx.h"

#include "__gx.h"

#pragma fp_contract off

void GXSetProjection(f32 mtx[4][4], GXProjectionType type)
{
    u32 reg;

    gx->projType = type;
    gx->projMtx[0] = mtx[0][0];
    gx->projMtx[2] = mtx[1][1];
    gx->projMtx[4] = mtx[2][2];
    gx->projMtx[5] = mtx[2][3];
    if (type == GX_ORTHOGRAPHIC) {
        gx->projMtx[1] = mtx[0][3];
        gx->projMtx[3] = mtx[1][3];
    } else {
        gx->projMtx[1] = mtx[0][2];
        gx->projMtx[3] = mtx[1][2];
    }

    reg = 0x00061020;
    GX_WRITE_U8(0x10);
    GX_WRITE_U32(reg);
    GX_WRITE_XF_REG_F(32, gx->projMtx[0]);
    GX_WRITE_XF_REG_F(33, gx->projMtx[1]);
    GX_WRITE_XF_REG_F(34, gx->projMtx[2]);
    GX_WRITE_XF_REG_F(35, gx->projMtx[3]);
    GX_WRITE_XF_REG_F(36, gx->projMtx[4]);
    GX_WRITE_XF_REG_F(37, gx->projMtx[5]);
    GX_WRITE_XF_REG_2(38, gx->projType);
    gx->bpSent = 1;
}

static asm void WriteMTXPS4x3(register f32 mtx[3][4], register volatile f32* dest)
{
    // clang-format off
    psq_l f0, 0x00(mtx), 0, 0
    psq_l f1, 0x08(mtx), 0, 0
    psq_l f2, 0x10(mtx), 0, 0
    psq_l f3, 0x18(mtx), 0, 0
    psq_l f4, 0x20(mtx), 0, 0
    psq_l f5, 0x28(mtx), 0, 0
    psq_st f0, 0(dest), 0, 0
    psq_st f1, 0(dest), 0, 0
    psq_st f2, 0(dest), 0, 0
    psq_st f3, 0(dest), 0, 0
    psq_st f4, 0(dest), 0, 0
    psq_st f5, 0(dest), 0, 0
    // clang-format on
}

static asm void WriteMTXPS3x3from3x4(register f32 mtx[3][4], register volatile f32* dest)
{
    // clang-format off
    psq_l f0, 0x00(mtx), 0, 0
    lfs   f1, 0x08(mtx)
    psq_l f2, 0x10(mtx), 0, 0
    lfs   f3, 0x18(mtx)
    psq_l f4, 0x20(mtx), 0, 0
    lfs   f5, 0x28(mtx)
    psq_st f0, 0(dest), 0, 0
    stfs   f1, 0(dest)
    psq_st f2, 0(dest), 0, 0
    stfs   f3, 0(dest)
    psq_st f4, 0(dest), 0, 0
    stfs   f5, 0(dest)
    // clang-format on
}

static asm void WriteMTXPS4x2(register f32 mtx[2][4], register volatile f32* dest)
{
    // clang-format off
    psq_l f0, 0x00(mtx), 0, 0
    psq_l f1, 0x08(mtx), 0, 0
    psq_l f2, 0x10(mtx), 0, 0
    psq_l f3, 0x18(mtx), 0, 0
    psq_st f0, 0(dest), 0, 0
    psq_st f1, 0(dest), 0, 0
    psq_st f2, 0(dest), 0, 0
    psq_st f3, 0(dest), 0, 0
    // clang-format on
}

void GXLoadPosMtxImm(f32 mtx[3][4], u32 id)
{
    u32 reg;
    u32 addr;

    addr = id * 4;
    reg = addr | 0xB0000;

    GX_WRITE_U8(0x10);
    GX_WRITE_U32(reg);
    WriteMTXPS4x3(mtx, &GXWGFifo.f32);
}

void GXLoadNrmMtxImm(f32 mtx[3][4], u32 id)
{
    u32 reg;
    u32 addr;

    addr = id * 3 + 0x400;
    reg = addr | 0x80000;

    GX_WRITE_U8(0x10);
    GX_WRITE_U32(reg);
    WriteMTXPS3x3from3x4(mtx, &GXWGFifo.f32);
}

void GXSetCurrentMtx(u32 id)
{
    SET_REG_FIELD(0x2A5, gx->matIdxA, 6, 0, id);
    __GXSetMatrixIndex(GX_VA_PNMTXIDX);
}

void GXLoadTexMtxImm(f32 mtx[][4], u32 id, GXTexMtxType type)
{
    u32 reg;
    u32 addr;
    u32 count;

    if (id >= GX_PTTEXMTX0) {
        addr = (id - GX_PTTEXMTX0) * 4 + 0x500;
    } else {
        addr = id * 4;
    }
    count = (type == GX_MTX2x4) ? 8 : 12;
    reg = addr | ((count - 1) << 16);

    GX_WRITE_U8(0x10);
    GX_WRITE_U32(reg);
    if (type == GX_MTX3x4) {
        WriteMTXPS4x3(mtx, &GXWGFifo.f32);
    } else {
        WriteMTXPS4x2(mtx, &GXWGFifo.f32);
    }
}

void GXSetViewportJitter(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz, u32 field)
{
    f32 sx;
    f32 sy;
    f32 sz;
    f32 ox;
    f32 oy;
    f32 oz;
    f32 zmin;
    f32 zmax;
    u32 reg;

    if (field == 0) {
        top -= 0.5f;
    }
    sx = wd / 2.0f;
    sy = -ht / 2.0f;
    ox = 342.0f + (left + (wd / 2.0f));
    oy = 342.0f + (top + (ht / 2.0f));
    zmin = 1.6777215e7f * nearz;
    zmax = 1.6777215e7f * farz;
    sz = zmax - zmin;
    oz = zmax;
    gx->vpLeft = left;
    gx->vpTop = top;
    gx->vpWd = wd;
    gx->vpHt = ht;
    gx->vpNearz = nearz;
    gx->vpFarz = farz;
    if (gx->fgRange != 0) {
        __GXSetRange(nearz, gx->fgSideX);
    }
    reg = 0x5101A;
    GX_WRITE_U8(0x10);
    GX_WRITE_U32(reg);
    GX_WRITE_XF_REG_F(26, sx);
    GX_WRITE_XF_REG_F(27, sy);
    GX_WRITE_XF_REG_F(28, sz);
    GX_WRITE_XF_REG_F(29, ox);
    GX_WRITE_XF_REG_F(30, oy);
    GX_WRITE_XF_REG_F(31, oz);
    gx->bpSent = 1;
}

void GXSetViewport(f32 left, f32 top, f32 wd, f32 ht, f32 nearz, f32 farz)
{
    GXSetViewportJitter(left, top, wd, ht, nearz, farz, 1U);
}

void GXGetViewportv(f32* vp)
{
    vp[0] = gx->vpLeft;
    vp[1] = gx->vpTop;
    vp[2] = gx->vpWd;
    vp[3] = gx->vpHt;
    vp[4] = gx->vpNearz;
    vp[5] = gx->vpFarz;
}

void GXSetScissor(u32 left, u32 top, u32 wd, u32 ht)
{
    u32 tp;
    u32 lf;
    u32 bm;
    u32 rt;

    tp = top + 342;
    lf = left + 342;
    bm = tp + ht - 1;
    rt = lf + wd - 1;

    SET_REG_FIELD(0x3BF, gx->suScis0, 11, 0, tp);
    SET_REG_FIELD(0x3C0, gx->suScis0, 11, 12, lf);
    SET_REG_FIELD(0x3C2, gx->suScis1, 11, 0, bm);
    SET_REG_FIELD(0x3C3, gx->suScis1, 11, 12, rt);

    GX_WRITE_RAS_REG(gx->suScis0);
    GX_WRITE_RAS_REG(gx->suScis1);
    gx->bpSent = 0;
}

void GXSetScissorBoxOffset(s32 x_off, s32 y_off)
{
    u32 reg = 0;
    u32 hx;
    u32 hy;

    hx = (u32)(x_off + 342) >> 1;
    hy = (u32)(y_off + 342) >> 1;

    SET_REG_FIELD(0x405, reg, 10, 0, hx);
    SET_REG_FIELD(0x406, reg, 10, 10, hy);
    SET_REG_FIELD(0x407, reg, 8, 24, 0x59);
    GX_WRITE_RAS_REG(reg);
    gx->bpSent = 0;
}

void GXSetClipMode(GXClipMode mode)
{
    GX_WRITE_XF_REG(5, mode);
    gx->bpSent = 1;
}

void __GXSetMatrixIndex(GXAttr matIdxAttr)
{
    if (matIdxAttr < GX_VA_TEX4MTXIDX) {
        GX_WRITE_SOME_REG4(8, 0x30, gx->matIdxA, -12);
        GX_WRITE_XF_REG(24, gx->matIdxA);
    } else {
        GX_WRITE_SOME_REG4(8, 0x40, gx->matIdxB, -12);
        GX_WRITE_XF_REG(25, gx->matIdxB);
    }
    gx->bpSent = 1;
}
