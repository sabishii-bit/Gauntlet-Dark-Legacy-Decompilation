#ifndef __GX_H__
#define __GX_H__

#include "types.h"
#include "dolphin/gx.h"

#define GX_WRITE_U8(ub) GXWGFifo.u8 = (u8)(ub)

#define GX_WRITE_U16(us) GXWGFifo.u16 = (u16)(us)

#define GX_WRITE_U32(ui) GXWGFifo.u32 = (u32)(ui)

#define GX_WRITE_F32(f) GXWGFifo.f32 = (f32)(f);

#define GX_WRITE_XF_REG(addr, value)                                          \
    do {                                                                      \
        GX_WRITE_U8(0x10);                                                    \
        GX_WRITE_U32(0x1000 + (addr));                                        \
        GX_WRITE_U32(value);                                                  \
    } while (0)

#define GX_WRITE_XF_REG_2(addr, value)                                        \
    do {                                                                      \
        GX_WRITE_U32(value);                                                  \
    } while (0)

#define GX_WRITE_XF_REG_F(addr, value)                                        \
    do {                                                                      \
        GX_WRITE_F32(value);                                                  \
    } while (0)

#define GX_WRITE_RAS_REG(value)                                               \
    do {                                                                      \
        GX_WRITE_U8(0x61);                                                    \
        GX_WRITE_U32(value);                                                  \
    } while (0)

#define GX_WRITE_SOME_REG2(a, b, c, addr)                                     \
    do {                                                                      \
        long regAddr;                                                         \
        GX_WRITE_U8(a);                                                       \
        GX_WRITE_U8(b);                                                       \
        GX_WRITE_U32(c);                                                      \
        regAddr = addr;                                                       \
        if (regAddr >= 0 && regAddr < 4) {                                    \
            gx->indexBase[regAddr] = c;                                       \
        }                                                                     \
    } while (0)

#define GX_WRITE_SOME_REG3(a, b, c, addr)                                     \
    do {                                                                      \
        long regAddr;                                                         \
        GX_WRITE_U8(a);                                                       \
        GX_WRITE_U8(b);                                                       \
        GX_WRITE_U32(c);                                                      \
        regAddr = addr;                                                       \
        if (regAddr >= 0 && regAddr < 4) {                                    \
            gx->indexStride[regAddr] = c;                                     \
        }                                                                     \
    } while (0)

#define GX_WRITE_SOME_REG4(a, b, c, addr)                                     \
    do {                                                                      \
        long regAddr;                                                         \
        GX_WRITE_U8(a);                                                       \
        GX_WRITE_U8(b);                                                       \
        GX_WRITE_U32(c);                                                      \
        regAddr = addr;                                                       \
    } while (0)

#define GET_REG_FIELD(reg, size, shift)                                       \
    ((int)((reg) >> (shift)) & ((1 << (size)) - 1))

#define SET_REG_FIELD(line, reg, size, shift, val)                            \
    do {                                                                      \
        (reg) = ((u32)(reg) & ~(((1 << (size)) - 1) << (shift))) |            \
                ((u32)(val) << (shift));                                      \
    } while (0)

/* GXAttr.c */

void __GXSetVCD(void);
void __GXSetVAT(void);

/* GXBump.c */

void __GXUpdateBPMask(void);
void __GXFlushTextureState(void);

/* GXFifo.c */

// GXFifoObj private data
struct __GXFifoObj {
    u8* base;
    u8* top;
    u32 size;
    u32 hiWatermark;
    u32 loWatermark;
    void* rdPtr;
    void* wrPtr;
    s32 count;
    u8 bind_cpu;
    u8 bind_gp;
};

void __GXSaveCPUFifoAux(struct __GXFifoObj* realFifo);
void __GXFifoInit(void);
void __GXInsaneWatermark(void);
void __GXCleanGPFifo(void);

/* GXGeometry.c */

void __GXSetDirtyState(void);
void __GXSendFlushPrim(void);
void __GXSetGenMode(void);

/* GXInit.c */

struct __GXData_struct {
    // total size: 0x4F8
    unsigned short unk;           // offset 0x0, size 0x2
    unsigned short bpSent;        // offset 0x2, size 0x2
    unsigned short vNum;          // offset 0x4, size 0x2
    unsigned short vLim;          // offset 0x6, size 0x2
    unsigned long cpEnable;       // offset 0x8, size 0x4
    unsigned long cpStatus;       // offset 0xC, size 0x4
    unsigned long cpClr;          // offset 0x10, size 0x4
    unsigned long vcdLo;          // offset 0x14, size 0x4
    unsigned long vcdHi;          // offset 0x18, size 0x4
    unsigned long vatA[8];        // offset 0x1C, size 0x20
    unsigned long vatB[8];        // offset 0x3C, size 0x20
    unsigned long vatC[8];        // offset 0x5C, size 0x20
    unsigned long lpSize;         // offset 0x7C, size 0x4
    unsigned long matIdxA;        // offset 0x80, size 0x4
    unsigned long matIdxB;        // offset 0x84, size 0x4
    unsigned long indexBase[4];   // offset 0x88, size 0x10
    unsigned long indexStride[4]; // offset 0x98, size 0x10
    unsigned long ambColor[2];    // offset 0xA8, size 0x8
    unsigned long matColor[2];    // offset 0xB0, size 0x8
    unsigned long suTs0[8];       // offset 0xB8, size 0x20
    unsigned long suTs1[8];       // offset 0xD8, size 0x20
    unsigned long suScis0;        // offset 0xF8, size 0x4
    unsigned long suScis1;        // offset 0xFC, size 0x4
    unsigned long tref[8];        // offset 0x100, size 0x20
    unsigned long iref;           // offset 0x120, size 0x4
    unsigned long bpMask;         // offset 0x124, size 0x4
    unsigned long IndTexScale0;   // offset 0x128, size 0x4
    unsigned long IndTexScale1;   // offset 0x12C, size 0x4
    unsigned long tevc[16];       // offset 0x130, size 0x40
    unsigned long teva[16];       // offset 0x170, size 0x40
    unsigned long tevKsel[8];     // offset 0x1B0, size 0x20
    unsigned long cmode0;         // offset 0x1D0, size 0x4
    unsigned long cmode1;         // offset 0x1D4, size 0x4
    unsigned long zmode;          // offset 0x1D8, size 0x4
    unsigned long peCtrl;         // offset 0x1DC, size 0x4
    unsigned long cpDispSrc;      // offset 0x1E0, size 0x4
    unsigned long cpDispSize;     // offset 0x1E4, size 0x4
    unsigned long cpDispStride;   // offset 0x1E8, size 0x4
    unsigned long cpDisp;         // offset 0x1EC, size 0x4
    unsigned long cpTexSrc;       // offset 0x1F0, size 0x4
    unsigned long cpTexSize;      // offset 0x1F4, size 0x4
    unsigned long cpTexStride;    // offset 0x1F8, size 0x4
    unsigned long cpTex;          // offset 0x1FC, size 0x4
    unsigned char cpTexZ;         // offset 0x200, size 0x1
    unsigned long genMode;        // offset 0x204, size 0x4
    GXTexRegion TexRegions[8];    // offset 0x208, size 0x80
    GXTexRegion TexRegionsCI[4];  // offset 0x288, size 0x40
    unsigned long nextTexRgn;     // offset 0x2C8, size 0x4
    unsigned long nextTexRgnCI;   // offset 0x2CC, size 0x4
    GXTlutRegion TlutRegions[20]; // offset 0x2D0, size 0x140
    GXTexRegion* (*texRegionCallback)(GXTexObj*,
                                      GXTexMapID); // offset 0x410, size 0x4
    GXTlutRegion* (*tlutRegionCallback)(
        unsigned long);          // offset 0x414, size 0x4
    GXAttrType nrmType;          // offset 0x418, size 0x4
    unsigned char hasNrms;       // offset 0x41C, size 0x1
    unsigned char hasBiNrms;     // offset 0x41D, size 0x1
    unsigned long projType;      // offset 0x420, size 0x4
    float projMtx[6];            // offset 0x424, size 0x18
    float vpLeft;                // offset 0x43C, size 0x4
    float vpTop;                 // offset 0x440, size 0x4
    float vpWd;                  // offset 0x444, size 0x4
    float vpHt;                  // offset 0x448, size 0x4
    float vpNearz;               // offset 0x44C, size 0x4
    float vpFarz;                // offset 0x450, size 0x4
    unsigned char fgRange;       // offset 0x454, size 0x1
    float fgSideX;               // offset 0x458, size 0x4
    unsigned long tImage0[8];    // offset 0x45C, size 0x20
    unsigned long tMode0[8];     // offset 0x47C, size 0x20
    unsigned long texmapId[16];  // offset 0x49C, size 0x40
    unsigned long tcsManEnab;    // offset 0x4DC, size 0x4
    unsigned long tevTcEnab;     // offset 0x4E0, size 0x4 (not in melee's 0x4F4 struct)
    GXPerf0 perf0;               // offset 0x4E4, size 0x4
    GXPerf1 perf1;               // offset 0x4E8, size 0x4
    unsigned long perfSel;       // offset 0x4EC, size 0x4
    unsigned char inDispList;    // offset 0x4F0, size 0x1
    unsigned char dlSaveContext; // offset 0x4F1, size 0x1
    unsigned char dirtyVAT;      // offset 0x4F2, size 0x1
    unsigned long dirtyState;    // offset 0x4F4, size 0x4
}; // size = 0x4F8

extern struct __GXData_struct* gx;
extern u16* __memReg;
extern u16* __peReg;
extern u16* __cpReg;
extern u32* __piReg;

void __GXInitGX(void);

/* GXMisc.c */

void __GXBypass(u32 reg);
u16 __GXReadPEReg(u32 reg);
void __GXPEInit(void);

/* GXSave.c */

void __GXShadowDispList(void* list, u32 nbytes);
void __GXShadowIndexState(u32 idx_reg, u32 reg_data);
void __GXPrintShadowState(void);

/* GXStubs.c */

void __GXSetRange(float nearz, float fgSideX);

/* GXTexture.c */

void __GetImageTileCount(GXTexFmt fmt, u16 wd, u16 ht, u32* rowTiles,
                         u32* colTiles, u32* cmpTiles);
void __GXSetSUTexRegs(void);
void __GXGetSUTexSize(GXTexCoordID coord, u16* width, u16* height);
void __GXSetTmemConfig(u32 arg0);

/* GXTransform.c */

void __GXSetMatrixIndex(GXAttr matIdxAttr);

#endif
