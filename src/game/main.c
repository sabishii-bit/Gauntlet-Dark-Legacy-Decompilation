/*
 * main.c - game entry (Xbox: MAIN.OBJ: main_init, sendPlayerPos, playMovie,
 * test_movies, DefunctThreads, GauntletMain). GCN main() is GauntletMain
 * merged with the console entry.
 */
#include "types.h"

void DEMOInit(void* renderMode);
int DVDOpen(const char* path, void* fileInfo);

void GXSetCopyClear(void* color, u32 z);
void GXSetZMode(int enable, int func, int update);
void GXSetVtxAttrFmt(int fmt, int attr, int cnt, int type, int frac);
void C_MTXOrtho(void* m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);
void GXSetProjection(void* m, int type);
void PSMTXIdentity(void* m);
void GXLoadPosMtxImm(void* m, u32 id);
void GXSetCullMode(int mode);
void GXSetNumChans(u8 n);
void GXSetNumTevStages(u8 n);
void GXSetNumTexGens(u8 n);
void GXSetChanCtrl(int chan, int enable, int amb, int mat, int lights, int diff, int attn);
void GXClearVtxDesc(void);
void GXSetVtxDesc(int attr, int type);
void GXSetTexCoordGen2(int dst, int fn, int src, u32 mtx, int normalize, u32 pt);
void GXSetTevKColor(int sel, void* color);
void GXSetTevKAlphaSel(int stage, int sel);
void GXSetTevOrder(int stage, int coord, int map, int color);
void GXSetTevOp(int stage, int mode);
void GXSetTevAlphaIn(int stage, int a, int b, int c, int d);
void GXSetTevAlphaOp(int stage, int op, int bias, int scale, int clamp, int out);
void GXSetTevColorOp(int stage, int op, int bias, int scale, int clamp, int out);
void GXSetTevColorIn(int stage, int a, int b, int c, int d);
void GXInvalidateTexAll(void);
void GXGetViewportv(f32* vp);

void ARInit(u32* stack_index_addr, u32 num_entries);
void ARQInit(void);
void AIInit(u8* stack);
void AXInit(void);
void sndVoiceInit(void);

void pbPulseTime(void);
void fn_800BE968(void);
void fn_800D6234(void);

/* main globals block: DVDFileInfo @0, ortho Mtx44 @60, viewport @124 */
extern u8 lbl_8025EDE8[];
extern char lbl_8011304C[]; /* "check.txt" */

extern f32 lbl_80344594;
extern f64 lbl_803471B8;
extern f32 lbl_80344980;
extern f32 lbl_80344984;
extern u32 lbl_80343C64;  /* copy-clear color */
extern f32 lbl_803472B4;  /* 1.0f */
extern f32 lbl_803472B8;  /* -1.0f */
extern u32 lbl_803472B0;  /* tev kcolor */

/* peak tracker over the frame meter (Xbox data: vb_last_print) */
void fn_80067AE0(f32 t, f32 v)
{
    f32 x = (f32) (lbl_803471B8 + (lbl_80344594 + t));

    if (x > lbl_80344980) {
        lbl_80344980 = x;
        lbl_80344984 = v;
    }
}

/* per-frame service pump */
void fn_80067B0C(int flags)
{
    if (flags & 1) {
        pbPulseTime();
    }
    if (flags & 2) {
        fn_800BE968();
    }
    if (flags & 4) {
        fn_800D6234();
    }
}

void main_init(void)
{
    u8* g;
    f32 ident[3][4];
    u32 clear;
    u32 kcolor;

    /* PARKED 1-insn residual: our g init takes a temp (addi r0 + mr r30)
       where the target's lands in r30 directly; decl-init, statement,
       register, and direct-ref forms all tried. */
    g = lbl_8025EDE8;
    DEMOInit(0);
    while (DVDOpen(lbl_8011304C, g) == 0) {
    }
    clear = lbl_80343C64;
    GXSetCopyClear(&clear, 0);
    GXSetZMode(1, 6, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
    C_MTXOrtho(g + 60, lbl_803472B4, lbl_803472B8, lbl_803472B8,
               lbl_803472B4, lbl_803472B8, lbl_803472B4);
    GXSetProjection(g + 60, 1);
    PSMTXIdentity(ident);
    GXLoadPosMtxImm(ident, 0);
    GXSetCullMode(0);
    GXSetNumChans(1);
    GXSetNumTevStages(1);
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetTexCoordGen2(1, 1, 5, 60, 0, 125);
    kcolor = lbl_803472B0;
    GXSetTevKColor(0, &kcolor);
    GXSetTevKAlphaSel(1, 28);
    GXSetTevOrder(2, 255, 255, 4);
    GXSetTevOp(2, 4);
    GXSetTevAlphaIn(2, 7, 5, 0, 7);
    GXSetTevAlphaOp(2, 0, 0, 0, 1, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    GXSetVtxAttrFmt(0, 14, 1, 4, 0);
    GXSetTexCoordGen2(0, 1, 4, 60, 0, 125);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetNumTexGens(1);
    GXSetTevOp(0, 0);
    GXSetTevColorOp(0, 0, 0, 1, 1, 0);
    GXSetTevColorIn(0, 15, 10, 8, 15);
    GXInvalidateTexAll();
    GXSetVtxDesc(13, 1);
    GXGetViewportv((f32*) (g + 124));
    ARInit(0, 0);
    ARQInit();
    AIInit(0);
    AXInit();
    sndVoiceInit();
}
