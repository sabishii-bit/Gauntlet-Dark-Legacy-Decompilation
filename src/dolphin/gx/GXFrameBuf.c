#include "types.h"
#include "dolphin/gx.h"

#include "__gx.h"

GXRenderModeObj GXNtsc480IntDf = { 0,
                                   640,
                                   480,
                                   480,
                                   40,
                                   0,
                                   640,
                                   480,
                                   1,
                                   0,
                                   0,
                                   { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                                     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
                                   { 8, 8, 10, 12, 10, 8, 8 } };

GXRenderModeObj GXMpal480IntDf = { 8,
                                   640,
                                   480,
                                   480,
                                   40,
                                   0,
                                   640,
                                   480,
                                   1,
                                   0,
                                   0,
                                   { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                                     6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
                                   { 8, 8, 10, 12, 10, 8, 8 } };

GXRenderModeObj GXPal528IntDf = { 4,
                                  640,
                                  528,
                                  528,
                                  40,
                                  23,
                                  640,
                                  528,
                                  1,
                                  0,
                                  0,
                                  { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                                    6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
                                  { 8, 8, 10, 12, 10, 8, 8 } };

GXRenderModeObj GXEurgb60Hz480IntDf = { 20,
                                        640,
                                        480,
                                        480,
                                        40,
                                        0,
                                        640,
                                        480,
                                        1,
                                        0,
                                        0,
                                        { 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
                                          6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6 },
                                        { 8, 8, 10, 12, 10, 8, 8 } };

void GXAdjustForOverscan(GXRenderModeObj* rmin, GXRenderModeObj* rmout, u16 hor, u16 ver)
{
    unsigned short hor2 = hor * 2;
    unsigned short ver2 = ver * 2;
    unsigned long verf;

    if (rmin != rmout) {
        *rmout = *rmin;
    }

    rmout->fbWidth = rmin->fbWidth - hor2;
    verf = (ver2 * rmin->efbHeight) / (u32)rmin->xfbHeight;
    rmout->efbHeight = rmin->efbHeight - verf;
    if (rmin->xFBmode == VI_XFBMODE_SF && (rmin->viTVmode & 2) != 2) {
        rmout->xfbHeight = rmin->xfbHeight - ver;
    } else {
        rmout->xfbHeight = rmin->xfbHeight - ver2;
    }
    rmout->viWidth = rmin->viWidth - hor2;
    rmout->viHeight = rmin->viHeight - ver2;
    rmout->viXOrigin = rmin->viXOrigin + hor;
    rmout->viYOrigin = rmin->viYOrigin + ver;
}

void GXSetDispCopySrc(u16 left, u16 top, u16 wd, u16 ht)
{
    gx->cpDispSrc = 0;
    SET_REG_FIELD(0x3BC, gx->cpDispSrc, 10, 0, left);
    SET_REG_FIELD(0x3BD, gx->cpDispSrc, 10, 10, top);
    SET_REG_FIELD(0x3BE, gx->cpDispSrc, 8, 24, 0x49);

    gx->cpDispSize = 0;
    SET_REG_FIELD(0x3C1, gx->cpDispSize, 10, 0, wd - 1);
    SET_REG_FIELD(0x3C2, gx->cpDispSize, 10, 10, ht - 1);
    SET_REG_FIELD(0x3C3, gx->cpDispSize, 8, 24, 0x4A);
}

void GXSetDispCopyDst(u16 wd, u16 ht)
{
    u16 stride;

    stride = (int)wd * 2;
    gx->cpDispStride = 0;
    SET_REG_FIELD(0x3FA, gx->cpDispStride, 10, 0, (stride >> 5));
    SET_REG_FIELD(0x3FB, gx->cpDispStride, 8, 24, 0x4D);
}

void GXSetDispCopyFrame2Field(GXCopyMode mode)
{
    SET_REG_FIELD(0x469, gx->cpDisp, 2, 12, mode);
    SET_REG_FIELD(0x46A, gx->cpTex, 2, 12, 0);
}

void GXSetCopyClamp(GXFBClamp clamp)
{
    u8 clmpB;
    u8 clmpT;

    clmpT = (clamp & 1) == 1;
    clmpB = (clamp & 2) == 2;

    SET_REG_FIELD(0x481, gx->cpDisp, 1, 0, clmpT);
    SET_REG_FIELD(0x482, gx->cpDisp, 1, 1, clmpB);

    SET_REG_FIELD(0x484, gx->cpTex, 1, 0, clmpT);
    SET_REG_FIELD(0x485, gx->cpTex, 1, 1, clmpB);
}

static u32 GXGetNumXfbLines(u32 efbHeight, u32 yScale)
{
    u32 num = (((efbHeight - 1) << 8) / yScale) + 1;

    if (yScale > 128 && yScale < 256) {
        while (!(yScale & 1)) {
            yScale >>= 1;
        }
        if ((efbHeight % yScale) == 0) {
            num++;
        }
    }
    if (num > 1024) {
        num = 1024;
    }
    return num;
}

u32 GXSetDispCopyYScale(f32 vscale)
{
    u8 enable;
    u32 iScale;
    u32 ht;

    iScale = (u32)(256.0f / vscale) & 0x1FF;
    GX_WRITE_RAS_REG(iScale | 0x4E000000);
    gx->bpSent = 0;
    enable = (iScale != 256);
    SET_REG_FIELD(0x4AB, gx->cpDisp, 1, 10, enable);
    ht = (u32)GET_REG_FIELD(gx->cpDispSize, 10, 10) + 1;
    return GXGetNumXfbLines(ht, iScale);
}

void GXSetCopyClear(GXColor clear_clr, u32 clear_z)
{
    u32 reg;

    reg = 0;
    SET_REG_FIELD(0x4C9, reg, 8, 0, clear_clr.r);
    SET_REG_FIELD(0x4CA, reg, 8, 8, clear_clr.a);
    SET_REG_FIELD(0x4CB, reg, 8, 24, 0x4F);
    GX_WRITE_RAS_REG(reg);

    reg = 0;
    SET_REG_FIELD(0x4CF, reg, 8, 0, clear_clr.b);
    SET_REG_FIELD(0x4D0, reg, 8, 8, clear_clr.g);
    SET_REG_FIELD(0x4D1, reg, 8, 24, 0x50);
    GX_WRITE_RAS_REG(reg);

    reg = 0;
    SET_REG_FIELD(0x4D5, reg, 24, 0, clear_z);
    SET_REG_FIELD(0x4D6, reg, 8, 24, 0x51);
    GX_WRITE_RAS_REG(reg);
    gx->bpSent = 0;
}

void GXSetCopyFilter(GXBool aa, const u8 sample_pattern[12][2], GXBool vf, const u8 vfilter[7])
{
    u32 msLoc[4];
    u32 coeff0;
    u32 coeff1;

    if (aa != 0) {
        msLoc[0] = 0;
        SET_REG_FIELD(0x4F5, msLoc[0], 4, 0, sample_pattern[0][0]);
        SET_REG_FIELD(0x4F6, msLoc[0], 4, 4, sample_pattern[0][1]);
        SET_REG_FIELD(0x4F7, msLoc[0], 4, 8, sample_pattern[1][0]);
        SET_REG_FIELD(0x4F8, msLoc[0], 4, 12, sample_pattern[1][1]);
        SET_REG_FIELD(0x4F9, msLoc[0], 4, 16, sample_pattern[2][0]);
        SET_REG_FIELD(0x4FA, msLoc[0], 4, 20, sample_pattern[2][1]);
        SET_REG_FIELD(0x4FB, msLoc[0], 8, 24, 1);

        msLoc[1] = 0;
        SET_REG_FIELD(0x4FE, msLoc[1], 4, 0, sample_pattern[3][0]);
        SET_REG_FIELD(0x4FF, msLoc[1], 4, 4, sample_pattern[3][1]);
        SET_REG_FIELD(0x500, msLoc[1], 4, 8, sample_pattern[4][0]);
        SET_REG_FIELD(0x501, msLoc[1], 4, 12, sample_pattern[4][1]);
        SET_REG_FIELD(0x502, msLoc[1], 4, 16, sample_pattern[5][0]);
        SET_REG_FIELD(0x503, msLoc[1], 4, 20, sample_pattern[5][1]);
        SET_REG_FIELD(0x504, msLoc[1], 8, 24, 2);

        msLoc[2] = 0;
        SET_REG_FIELD(0x507, msLoc[2], 4, 0, sample_pattern[6][0]);
        SET_REG_FIELD(0x508, msLoc[2], 4, 4, sample_pattern[6][1]);
        SET_REG_FIELD(0x509, msLoc[2], 4, 8, sample_pattern[7][0]);
        SET_REG_FIELD(0x50A, msLoc[2], 4, 12, sample_pattern[7][1]);
        SET_REG_FIELD(0x50B, msLoc[2], 4, 16, sample_pattern[8][0]);
        SET_REG_FIELD(0x50C, msLoc[2], 4, 20, sample_pattern[8][1]);
        SET_REG_FIELD(0x50D, msLoc[2], 8, 24, 3);

        msLoc[3] = 0;
        SET_REG_FIELD(0x510, msLoc[3], 4, 0, sample_pattern[9][0]);
        SET_REG_FIELD(0x511, msLoc[3], 4, 4, sample_pattern[9][1]);
        SET_REG_FIELD(0x512, msLoc[3], 4, 8, sample_pattern[10][0]);
        SET_REG_FIELD(0x513, msLoc[3], 4, 12, sample_pattern[10][1]);
        SET_REG_FIELD(0x514, msLoc[3], 4, 16, sample_pattern[11][0]);
        SET_REG_FIELD(0x515, msLoc[3], 4, 20, sample_pattern[11][1]);
        SET_REG_FIELD(0x516, msLoc[3], 8, 24, 4);
    } else {
        msLoc[0] = 0x01666666;
        msLoc[1] = 0x02666666;
        msLoc[2] = 0x03666666;
        msLoc[3] = 0x04666666;
    }
    GX_WRITE_RAS_REG(msLoc[0]);
    GX_WRITE_RAS_REG(msLoc[1]);
    GX_WRITE_RAS_REG(msLoc[2]);
    GX_WRITE_RAS_REG(msLoc[3]);

    coeff0 = 0;
    SET_REG_FIELD(0, coeff0, 8, 24, 0x53);
    coeff1 = 0;
    SET_REG_FIELD(0, coeff1, 8, 24, 0x54);
    if (vf != 0) {
        SET_REG_FIELD(0x52E, coeff0, 6, 0, vfilter[0]);
        SET_REG_FIELD(0x52F, coeff0, 6, 6, vfilter[1]);
        SET_REG_FIELD(0x530, coeff0, 6, 12, vfilter[2]);
        SET_REG_FIELD(0x531, coeff0, 6, 18, vfilter[3]);
        SET_REG_FIELD(0x532, coeff1, 6, 0, vfilter[4]);
        SET_REG_FIELD(0x533, coeff1, 6, 6, vfilter[5]);
        SET_REG_FIELD(0x534, coeff1, 6, 12, vfilter[6]);
    } else {
        SET_REG_FIELD(0, coeff0, 6, 0, 0);
        SET_REG_FIELD(0, coeff0, 6, 6, 0);
        SET_REG_FIELD(0, coeff0, 6, 12, 21);
        SET_REG_FIELD(0, coeff0, 6, 18, 22);
        SET_REG_FIELD(0, coeff1, 6, 0, 21);
        SET_REG_FIELD(0, coeff1, 6, 6, 0);
        SET_REG_FIELD(0, coeff1, 6, 12, 0);
    }
    GX_WRITE_RAS_REG(coeff0);
    GX_WRITE_RAS_REG(coeff1);
    gx->bpSent = 0;
}

void GXSetDispCopyGamma(GXGamma gamma)
{
    SET_REG_FIELD(0x54B, gx->cpDisp, 2, 7, gamma);
}

void GXCopyDisp(void* dest, GXBool clear)
{
    u32 reg;
    u32 tempPeCtrl;
    u32 phyAddr;
    u8 changePeCtrl;

    if (clear) {
        reg = gx->zmode;
        SET_REG_FIELD(0, reg, 1, 0, 1);
        SET_REG_FIELD(0, reg, 3, 1, 7);
        GX_WRITE_RAS_REG(reg);

        reg = gx->cmode0;
        SET_REG_FIELD(0, reg, 1, 0, 0);
        SET_REG_FIELD(0, reg, 1, 1, 0);
        GX_WRITE_RAS_REG(reg);
    }
    changePeCtrl = FALSE;
    if ((clear || (u32)GET_REG_FIELD(gx->peCtrl, 3, 0) == 3) &&
        (u32)GET_REG_FIELD(gx->peCtrl, 1, 6) == 1) {
        changePeCtrl = TRUE;
        tempPeCtrl = gx->peCtrl;
        SET_REG_FIELD(0, tempPeCtrl, 1, 6, 0);
        GX_WRITE_RAS_REG(tempPeCtrl);
    }
    GX_WRITE_RAS_REG(gx->cpDispSrc);
    GX_WRITE_RAS_REG(gx->cpDispSize);
    GX_WRITE_RAS_REG(gx->cpDispStride);

    phyAddr = (u32)dest & 0x3FFFFFFF;
    reg = 0;
    SET_REG_FIELD(0x5D8, reg, 21, 0, phyAddr >> 5);
    SET_REG_FIELD(0x5D9, reg, 8, 24, 0x4B);
    GX_WRITE_RAS_REG(reg);

    SET_REG_FIELD(0x5DC, gx->cpDisp, 1, 11, clear);
    SET_REG_FIELD(0x5DD, gx->cpDisp, 1, 14, 1);
    SET_REG_FIELD(0x5DE, gx->cpDisp, 8, 24, 0x52);
    GX_WRITE_RAS_REG(gx->cpDisp);

    if (clear) {
        GX_WRITE_RAS_REG(gx->zmode);
        GX_WRITE_RAS_REG(gx->cmode0);
    }
    if (changePeCtrl) {
        GX_WRITE_RAS_REG(gx->peCtrl);
    }
    gx->bpSent = 0;
}

void GXClearBoundingBox(void)
{
    u32 reg;

    reg = 0x550003FF;
    GX_WRITE_RAS_REG(reg);
    reg = 0x560003FF;
    GX_WRITE_RAS_REG(reg);
    gx->bpSent = 0;
}
