/*
 * memcard.c - savegame layer over dolphin/card (Xbox: MEMCARD.OBJ).
 * GCN save files keep the PS2 serial: "BASLUS-20047save%04d" etc.
 * Functions are kept in address order (same-TU inlining depends on it).
 */
#include "types.h"

int sprintf(char* dst, const char* fmt, ...);
char* strcpy(char* dst, const char* src);

int sceOpen(const char* path, int flags, ...);
int sceLseek(int fd, int offset, int whence);
int sceRead(int fd, void* buf, int len);
int sceClose(int fd);

int CARDProbe(s32 chan);

void* OSCreateHeap(void* lo, void* hi);
void* OSSetCurrentHeap(void* heap);
void* OSAllocFromHeap(void* heap, u32 size);
extern void* __OSCurrHeap;

u32 fn_800BF524(void);
int fn_800BF168(void);
void fn_800BC2EC(const char* fmt, ...);
void fn_800D3874(u32 aramOffset, void* buf);
void fn_800D38F8(u32 dst, u32 len);
void fn_800D3970(u32 dst, u32 src, u32 len);
void fn_800D39E8(u32 dst, u32 src, u32 len);
void fn_800DC1F4(void* buf, u32 size, int arg);
void fn_800DC280(void);
s32 fn_800DD10C(void);
s32 fn_800DDABC(int pad);
int fn_800DDB68(int pad, u32 mask);
int fn_800DDBF0(int pad, u32 mask);
void fn_800DD604(void);
int fn_800DDE08(int flag);
void fn_800DDDE8(int flag);
void fn_800DDDF8(int flag);
void fn_80067B0C(int flags);
void fn_8006B210(const char* msg, int a, int b, int c);
void fn_800697D0(void);

/* string block: cache-transaction logs @0/40, "BASLUS-20047GameOpts"@80,
 * "BASLUS-20047DirInfo"@104, "BASLUS-20047save%04d"@124 */
extern char lbl_801131C0[];
extern char lbl_801131E8[];    /* "Beginning save cache transaction......" */
extern char lbl_80113548[];    /* "/opening.bnr" */
extern char lbl_8011D2A0[];    /* insert-card message */

extern s32 lbl_80344A18;       /* card state */
extern s32 lbl_80344A14;
extern u32 lbl_80343C78;       /* directory refresh flags */
extern s32 lbl_80343C6C;
extern s32 lbl_803449F4;       /* update counter (wraps at 60) */
extern s32 lbl_803449EC;
extern s32 lbl_803449F8;
extern s32 lbl_80344A24;
extern s32 lbl_80344A20;
extern s32 lbl_80344A10[2];    /* per-(port+slot) cached handles */
extern void* lbl_80344A0C;     /* save heap */
extern void* lbl_80344A08;     /* previous heap */
extern u8* lbl_80344A04;       /* directory buffer */
extern u8* lbl_80344A00;       /* card workArea */
extern u8* lbl_803449FC;       /* file buffer */

/* forward (same TU, later addresses) */
void fn_8006B188(void);
void fn_8006B1CC(void);
s32 fn_8006AFE0(const char* msg, s32* state, s32 count);

/* map card state to a save result code; the result!=0 / -2 arms are
 * statically dead (result starts at 0) but present in the original.
 * PARKED: target pre-loads result=0 into r3 (beqlr exit); ours folds the
 * known-zero through the dead arm and sinks the init (3 forms tried). */
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

/* cached per-(port+slot) handle, created on first use */
s32 fn_80068DB0(s32 port, s32 slot)
{
    s32* p;
    u8 ok;

    p = &lbl_80344A10[port];
    p = &p[slot];
    if (*p >= 0) {
        return *p;
    }
    ok = (0 <= port && port <= 1);
    if (!ok) {
        return -1;
    }
    *p = fn_800DD10C();
    return *p;
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

/* begin save cache transaction: wait for the loader, pull the staging
 * block, build the save heap, allocate workArea + file + directory
 * buffers, then scan the card directory */
void fn_800696E8(void)
{
    u8* buf;
    u32 size;
    u8* lo;
    u8 pad[24]; /* unused, matches original frame */

    lbl_803449EC = 0;
    lbl_803449F8 = 0;
    fn_800BC2EC(lbl_801131E8);
    while (fn_800BF168() != 0) {
        fn_80067B0C(-1);
    }
    size = 0x310000;
    fn_800DDDF8(64);
    buf = (u8*) fn_800BF524();
    fn_800D38F8((u32) buf - 0x310000, size);
    lo = buf - 0x310000;
    lbl_80344A0C = OSCreateHeap(lo, lo + size);
    lbl_80344A08 = OSSetCurrentHeap(lbl_80344A0C);
    lbl_80344A00 = (u8*) OSAllocFromHeap(__OSCurrHeap, 8192);
    lbl_803449FC = (u8*) OSAllocFromHeap(__OSCurrHeap, 0xA000);
    fn_800DC1F4(lbl_80344A00 + 8192, 8192, 18);
    fn_800DC280();
    lbl_80344A04 = (u8*) OSAllocFromHeap(__OSCurrHeap, 0x2D44C0);
    fn_800697D0();
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

/* load /opening.bnr (0x1960-byte BNR1) through the PS2 file shim */
u8* fn_8006AE1C(void)
{
    s32 size;
    u8* buf;
    s32 fd;

    fd = sceOpen(lbl_80113548, 1);
    size = sceLseek(fd, 0, 2);
    buf = (u8*) OSAllocFromHeap(__OSCurrHeap, 6496);
    sceLseek(fd, 0, 0);
    sceRead(fd, buf, size);
    sceClose(fd);
    return buf;
}

/* begin save transaction: pull the staging block from ARAM and build the
 * save work heap inside it.
 * PARKED 1-slot residual: the D38F8-dst addis schedules one slot earlier
 * than the target's (arg-eval order); opcodes identical. */
void fn_8006AEA8(void)
{
    u8* buf;
    u32 size = 0x310000;
    u8* lo;
    u8 pad[8]; /* unused, matches original frame */

    fn_800DDDF8(64);
    buf = (u8*) fn_800BF524();
    fn_800D38F8((u32) (buf - 0x310000), 0x310000);
    lo = buf - 0x310000;
    lbl_80344A0C = OSCreateHeap(lo, lo + size);
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

s32 fn_8006AF7C(const char* msg)
{
    s32 r = fn_8006AFE0(msg, &lbl_80343C6C, 2);

    switch (r) {
    case 0:
        return 1;
    case 1:
        lbl_80344A24 = 0;
        r = 0;
        lbl_80344A18 = -1;
        lbl_80344A14 = -1;
        lbl_80344A20 = 0;
        break;
    }
    return r;
}

/* modal save-prompt menu: pages the save cache out of ARAM every 30
 * frames so the service pump (movie music etc.) keeps running */
s32 fn_8006AFE0(const char* msg, s32* state, s32 count)
{
    s32 sel = 0;
    s32 timer = 30;
    u8 had;

    fn_800D3970(fn_800BF524() - 0x310000, 0x9E0000, 0x310000);
    fn_800D39E8(0xCF0000, fn_800BF524() - 0x310000, 0x310000);
    had = fn_800DDE08(64);
    if (had) {
        fn_800DDDE8(64);
    }
    for (;;) {
        s32 stick;

        fn_8006B210(msg, (int) state, count, sel);
        stick = fn_800DDABC(-1);
        if (fn_800DDB68(-1, 4) != 0 || stick > 0) {
            if (++sel >= count) {
                sel = 0;
            }
        } else if (fn_800DDB68(-1, 8) != 0 || stick < 0) {
            if (--sel < 0) {
                sel = count - 1;
            }
        }
        fn_800DD604();
        if (--timer <= 0) {
            timer = 30;
            fn_800D3970(fn_800BF524() - 0x310000, 0xCF0000, 0x310000);
            fn_800D39E8(0x9E0000, fn_800BF524() - 0x310000, 0x310000);
            fn_80067B0C(-1);
            fn_800D3970(fn_800BF524() - 0x310000, 0x9E0000, 0x310000);
            fn_800D39E8(0xCF0000, fn_800BF524() - 0x310000, 0x310000);
        }
        if (fn_800DDBF0(-1, 256) != 0) {
            break;
        }
    }
    fn_800D3970(fn_800BF524() - 0x310000, 0xCF0000, 0x310000);
    fn_800D39E8(0x9E0000, fn_800BF524() - 0x310000, 0x310000);
    if (had) {
        fn_800DDDF8(64);
    }
    return sel;
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
