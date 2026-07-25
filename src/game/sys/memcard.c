/*
 * memcard.c - savegame layer over dolphin/card (Xbox: MEMCARD.OBJ).
 * GCN save files keep the PS2 serial: "BASLUS-20047save%04d" etc.
 */
#include "types.h"

int sprintf(char* dst, const char* fmt, ...);
char* strcpy(char* dst, const char* src);

/* string block: cache-transaction logs @0/40, "BASLUS-20047GameOpts"@80,
 * "BASLUS-20047DirInfo"@104, "BASLUS-20047save%04d"@124 */
extern char lbl_801131C0[];

extern s32 lbl_80344A18;   /* card state */
extern u32 lbl_80343C78;   /* directory refresh flags */
extern s32 lbl_803449F4;   /* update counter (wraps at 60) */

u32 fn_800BF524(void);
int CARDProbe(s32 chan);
void fn_8006B210(const char* msg, int a, int b, int c);
void fn_800DDDF8(int arg);
void fn_800D38F8(u32 dst, u32 len);
void* OSCreateHeap(void* lo, void* hi);
void* OSSetCurrentHeap(void* heap);
void* OSAllocFromHeap(void* heap, u32 size);
void fn_800DC1F4(void* buf, u32 size, int arg);
void fn_800DC280(void);

extern void* __OSCurrHeap;
extern void* lbl_80344A0C;
extern void* lbl_80344A08;
extern u8* lbl_80344A00;
extern u8* lbl_803449FC;
extern char lbl_8011D2A0[];    /* insert-card message */
void fn_800D3874(u32 aramOffset, void* buf);
void fn_800D3970(u32 dst, u32 src, u32 len);
void fn_800D39E8(u32 dst, u32 src, u32 len);
extern s32 lbl_80344A14;









/* forward */
void fn_8006B188(void);
void fn_8006B1CC(void);

/* map card state to a save result code; the result!=0 / -2 arms are
 * statically dead (result starts at 0) but present in the original */
/* PARKED: target pre-loads result=0 into r3 (beqlr exit); ours folds the
 * known-zero through the dead result!=0 arm and sinks the init (3 forms
 * tried: arg, ternary, ptr+cached-state). ~3 insn-class diffs. */
s32 fn_800689CC(void)
{
    s32 result = 0;
    s32* p = &lbl_80344A14;
    s32 state = lbl_80344A18;

    if (state == -1) {
        return result;
    }
    if (*p == 1) {
        result = (state == 3) ? 1 : -1;
        return result;
    }
    if (result != 0) {
        return result;
    }
    if (*p == 1) {
        return -2;
    }
    return -1;
}

/* 60-frame wrap counter */
void fn_80068A28(void)
{
    lbl_803449F4++;
    if (lbl_803449F4 > 60) {
        lbl_803449F4 = 0;
    }
}

/* fixed size of a Gauntlet save in bytes */
s32 saveFileSize(void)
{
    return 128272;
}

/* slot -2 = game options, -1 = directory info, else numbered save */
void getSaveFileName(char* dst, s32 fileNo)
{
    char* base = lbl_801131C0;

    if (fileNo == -2) {
        strcpy(dst, base + 80);
    } else if (fileNo == -1) {
        strcpy(dst, base + 104);
    } else {
        sprintf(dst, base + 124, fileNo + 1);
    }
}

void set_directory_refresh_flags(u32 flags)
{
    lbl_80343C78 |= flags;
}

/* idle callback while the card is out: page the save cache out of ARAM,
 * poll for a card with the insert message up, page it back */
void fn_8006A9DC(int arg)
{
    if (arg == 0) {
        fn_8006B1CC();
        while (CARDProbe(0) == 0) {
            fn_8006B210(lbl_8011D2A0, 0, 0, 0);
        }
        fn_8006B188();
    }
}

/* begin save transaction: pull the staging block from ARAM and build the
 * save work heap inside it.
 * PARKED ~15-line residual: target computes heap-hi as (buf-0x310000)+r30
 * with 0x310000 hoisted; every literal/var mix we tried either folds the
 * round-trip or reassociates it (fakelib buf+off fold class). */
void fn_8006AEA8(void)
{
    u8* buf;
    u32 size = 0x310000;

    fn_800DDDF8(64);
    buf = (u8*) fn_800BF524();
    fn_800D38F8((u32) buf - 0x310000, 0x310000);
    lbl_80344A0C = OSCreateHeap(buf - 0x310000, (buf - 0x310000) + size);
    lbl_80344A08 = OSSetCurrentHeap(lbl_80344A0C);
    lbl_80344A00 = (u8*) OSAllocFromHeap(__OSCurrHeap, 8192);
    lbl_803449FC = (u8*) OSAllocFromHeap(__OSCurrHeap, 0xA000);
    fn_800DC1F4(lbl_80344A00 + 8192, 8192, 18);
    fn_800DC280();
}

void fn_8006AF44(u8* buf)
{
    u32 off = fn_800BF524();
    u8* b = buf;

    fn_800D3874(off - (u32) b, b);
}

/* swap the 3.2MB save cache blocks in ARAM (0x9E0000 <-> 0xCF0000) */
void fn_8006B188(void)
{
    fn_800D3970(fn_800BF524() - 0x310000, 0xCF0000, 0x310000);
    fn_800D39E8(0x9E0000, fn_800BF524() - 0x310000, 0x310000);
}

void fn_8006B1CC(void)
{
    fn_800D3970(fn_800BF524() - 0x310000, 0x9E0000, 0x310000);
    fn_800D39E8(0xCF0000, fn_800BF524() - 0x310000, 0x310000);
}
