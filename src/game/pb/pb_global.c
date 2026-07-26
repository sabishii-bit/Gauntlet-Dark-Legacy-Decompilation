#include "types.h"

#pragma dont_inline on

/* pb_global.c -- Midway "pb" graphics library global init/reset/close layer
 * (pb_global.obj on Xbox, 4 fns; the GCN build keeps 3 -- pbResetGlobal is
 * deadstripped). .text 0x800C33FC-0x800C3674. Function names from shell3D.pdb.
 *
 * pbInitGlobal brings up every pb sub-module in order, snapshots the module
 * block table, then verifies the shared status word and reports through
 * ErrorPrintf ("ERROR: pbInitGlobal did not init all modules: %08x\n").
 * pbCloseGlobal tears the same modules down (or re-inits) depending on the
 * shared status. pbSetupPBGPtrs (called by pbInitGlobal) wires up the 15-entry
 * pushbuffer global pointer table (lbl_80344FC8..lbl_80345000).
 */

/* --- shared pb-global block (0x802C5230, .bss, 0x90 bytes) --- */
typedef struct PBGlobal {
    u32 status;     /* 0x00 : init-phase / module bitmask */
    u8 _pad04[8];   /* 0x04 */
    void* src[15];  /* 0x0C : per-module blocks (filled during init) */
    u8 _pad48[8];   /* 0x48 */
    void* dst[15];  /* 0x50 : snapshot copied into the pushbuffer ptr table */
    u8 _pad8C[4];   /* 0x8C -> 0x90 */
} PBGlobal;

extern PBGlobal lbl_802C5230;
extern PBGlobal* volatile gWinGlobals; /* 0x80344FC0 : points at lbl_802C5230 */
extern u32 lbl_80344FC4;      /* 0x80344FC4 : "open" flag */

/* pushbuffer global pointer table (0x80344FC8..0x80345000) */
extern void* lbl_80344FC8;
extern void* lbl_80344FCC;
extern void* lbl_80344FD0;
extern void* lbl_80344FD4;
extern void* lbl_80344FD8;
extern void* lbl_80344FDC;
extern void* lbl_80344FE0;
extern void* lbl_80344FE4;
extern void* lbl_80344FE8;
extern void* lbl_80344FEC;
extern void* lbl_80344FF0;
extern void* lbl_80344FF4;
extern void* lbl_80344FF8;
extern void* lbl_80344FFC;
extern void* lbl_80345000;

extern char lbl_80116580[]; /* "ERROR: pbInitGlobal did not init all modules: %08x\n" */

extern void ErrorPrintf(const char* fmt, ...);
extern void pbInitWindow(void);
extern void pbSetDefaultWindow(void);
extern void pbCloseWindow(void);
extern void MBSetCurrentWindow(void);

/* pb sub-module bring-up / teardown entry points (other pb TUs) */
extern void fn_800C0F04(void);
extern void fn_800C0F40(void);
extern void fn_800C0F70(void);
extern void fn_800C0F90(void);
extern void fn_800C0FE8(void);
extern void fn_800C150C(void);
extern void fn_800C14F0(void);
extern void fn_800C31C4(void);
extern void fn_800C32D0(void);
extern void fn_800C33D0(void);
extern void fn_800C33EC(void);
extern void fn_800C379C(void);
extern void fn_800C37C4(void);
extern void fn_800C3880(void);
extern void fn_800C38A0(void);
extern void fn_800C6960(void);
extern void fn_800C697C(void);
extern void fn_800C698C(void);
extern void fn_800C69A8(void);
extern void fn_800C69B8(void);
extern void fn_800C69D4(void);
extern void fn_800C69E4(void);
extern void fn_800C6A00(void);
extern void fn_800C7948(void);
extern void fn_800C7974(void);

void pbSetupPBGPtrs(void);
void pbCloseGlobal(void);
void pbInitGlobal(void);

/* Wire up the 15-entry pushbuffer global pointer table from the module
 * snapshot held in the shared pb-global block. */
void pbSetupPBGPtrs(void)
{
    lbl_80344FC8 = lbl_802C5230.dst[0];
    lbl_80344FCC = lbl_802C5230.dst[1];
    lbl_80344FD0 = lbl_802C5230.dst[2];
    lbl_80344FD4 = lbl_802C5230.dst[3];
    lbl_80344FD8 = lbl_802C5230.dst[4];
    lbl_80344FDC = lbl_802C5230.dst[5];
    lbl_80344FE0 = lbl_802C5230.dst[6];
    lbl_80344FE4 = lbl_802C5230.dst[7];
    lbl_80344FE8 = lbl_802C5230.dst[8];
    lbl_80344FEC = lbl_802C5230.dst[9];
    lbl_80344FF0 = lbl_802C5230.dst[10];
    lbl_80344FF4 = lbl_802C5230.dst[11];
    lbl_80344FF8 = lbl_802C5230.dst[12];
    lbl_80344FFC = lbl_802C5230.dst[13];
    lbl_80345000 = lbl_802C5230.dst[14];
}

/* Tear down every pb sub-module in order (or re-init if not fully open). */
void pbCloseGlobal(void)
{
    gWinGlobals = &lbl_802C5230;
    lbl_80344FC4 = 0;
    if (gWinGlobals->status != 0x20000000) {
        pbInitGlobal();
    } else {
        fn_800C14F0();
        fn_800C31C4();
        fn_800C6960();
        MBSetCurrentWindow();
        fn_800C0F04();
        fn_800C698C();
        fn_800C69B8();
        fn_800C0FE8();
        fn_800C379C();
        fn_800C3880();
        fn_800C69E4();
        fn_800C0F70();
        fn_800C33D0();
        fn_800C7948();
        pbCloseWindow();
        fn_800C14F0();
        fn_800C379C();
        fn_800C31C4();
        fn_800C6960();
        MBSetCurrentWindow();
        fn_800C0F04();
        fn_800C698C();
        fn_800C69B8();
        fn_800C3880();
        fn_800C69E4();
        fn_800C0F70();
        fn_800C33D0();
        fn_800C7948();
        pbCloseWindow();
        lbl_80344FC4 = 1;
    }
}

/* Bring up every pb sub-module, snapshot the module table, verify the shared
 * status word, then publish the pushbuffer pointer table. */
void pbInitGlobal(void)
{
    PBGlobal* p;
    PBGlobal* g;

    p = &lbl_802C5230;
    gWinGlobals = p;
    g = gWinGlobals;
    g->status = 0;
    fn_800C150C();
    fn_800C37C4();
    fn_800C32D0();
    fn_800C697C();
    pbInitWindow();
    fn_800C0F40();
    fn_800C69A8();
    fn_800C69D4();
    fn_800C38A0();
    fn_800C6A00();
    fn_800C0F90();
    fn_800C33EC();
    fn_800C7974();
    pbSetDefaultWindow();
    g->status = 0x3FFFF;
    if (g->status != 0x3FFFF) {
        ErrorPrintf(lbl_80116580, g->status);
    }
    p->dst[0] = g->src[0];
    p->dst[1] = g->src[1];
    p->dst[2] = g->src[2];
    p->dst[3] = g->src[3];
    p->dst[4] = g->src[4];
    p->dst[5] = g->src[5];
    p->dst[6] = g->src[6];
    p->dst[7] = g->src[7];
    p->dst[8] = g->src[8];
    p->dst[9] = g->src[9];
    p->dst[10] = g->src[10];
    p->dst[11] = g->src[11];
    p->dst[12] = g->src[12];
    p->dst[13] = g->src[13];
    p->dst[14] = g->src[14];
    pbSetupPBGPtrs();
    g->status = 0x20000000;
    lbl_80344FC4 = 1;
}
