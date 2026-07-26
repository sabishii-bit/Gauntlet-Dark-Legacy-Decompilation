/*
 * memcard.c - savegame layer over dolphin/card (Xbox: MEMCARD.OBJ).
 * GCN save files keep the PS2 serial: "BASLUS-20047save%04d" etc.
 * Functions are kept in address order (same-TU inlining depends on it).
 */
#include "types.h"

int sprintf(char* dst, const char* fmt, ...);
char* strcpy(char* dst, const char* src);
char* strncpy(char* dst, const char* src, u32 n);
int strcmp(const char* a, const char* b);
void* memcpy(void* dst, const void* src, u32 n);

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
void dcsAramReadTop(u32 aramOffset, void* buf);
void dcsAramWriteTop(u32 dst, u32 len);
void dcsAramWrite(u32 dst, u32 src, u32 len);
void dcsAramRead(u32 dst, u32 src, u32 len);
void cardStart(void* buf, u32 size, int arg);
void cardWaitResult(void);
s32 cardGetFreeBytes(void);
s32 padMenuStickY(int pad);
int padButtonPressed(int pad, u32 mask);
int padButtonReleased(int pad, u32 mask);
void sysHandleReset(void);
int sysTestFlags(int flag);
void sysClearFlags(int flag);
void sysSetFlags(int flag);
void fn_80067B0C(int flags);
void fn_8006B210(const char* msg, int a, int b, int c);
s32 fn_80069164(s32 a, s32 b, s32 c);
u8 fn_8006A82C(s32 chan, const char* msg, s32* fileNo);
void cardInit(void);
void cardExit(void);
void OSDestroyHeap(void* heap);
u32 OSGetSoundMode(void);

typedef struct GameOpts {
    u32 data[8];               /* 32-byte options block; [2] = stereo flag */
} GameOpts;

extern u8 lbl_80274578[];      /* dir-info table: 8 entries stride 16 */
extern u8 lbl_8025EE80[];      /* loaded VMU/dir buffer (dir @+0x156F8) */
extern GameOpts lbl_80274E80;  /* game options */
extern GameOpts* lbl_80343C74; /* staged save record ptr (opts@+8, data@+0xA1C8) */
extern char lbl_803472D8[8];   /* default dir name (sdata, SDA21) */
extern char lbl_803472E0[8];   /* dir name variant (sdata, SDA21) */
extern char lbl_803472E8[8];   /* replacement dir name (sdata, SDA21) */
extern char lbl_8011D550[];    /* prompt message */
extern s32 lbl_803449F0;
extern s32 lbl_803449D0;       /* prefs_loaded */
extern s32 lbl_803449D4;
extern u8 lbl_803449D8;
extern s32 lbl_803449DC;
extern s32 lbl_803449E0;
int fn_800697D0(void);

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
void getSaveFileName(char* dst, s32 fileNo);
int fn_800696E8(void);
void fn_8006AF44(u8* buf);
void fn_8006AEA8(void);

void fn_8006A534(void);

/* write a dir record + save-data block back to the card (Xbox: saveSave).
 * Both the if and else branches carry the full teardown (duplicated source).
 * PARKED 79/80: one extra addi copying `row` into its preserved register
 * (MWCC computes the row sum in a temp then moves it; pure regalloc). */
int fn_800685EC(int a, int b, int c, const char* name, u32 v0, u32 v1)
{
    u8* row = lbl_80274578 + a * 132 + b * 132;
    u8* rec;
    int result;

    rec = row + c * 16;
    strncpy((char*) (rec + 8), name, 8);
    *(u32*) rec = v0;
    *(u32*) (rec + 4) = v1;
    result = ((u8) fn_800696E8() == 1) ? 1 : 0;
    if (result) {
        memcpy(row, (u8*) lbl_80343C74 + 41416, 128);
        fn_8006A534();
        lbl_80343C78 |= 0xFFFFFFFF;
        cardExit();
        cardWaitResult();
        OSSetCurrentHeap(lbl_80344A08);
        OSDestroyHeap(lbl_80344A0C);
        fn_8006AF44((u8*) 0x310000);
        sysClearFlags(64);
        fn_800BC2EC(lbl_801131C0);
        lbl_803449EC = 0;
    } else {
        cardExit();
        cardWaitResult();
        OSSetCurrentHeap(lbl_80344A08);
        OSDestroyHeap(lbl_80344A0C);
        fn_8006AF44((u8*) 0x310000);
        sysClearFlags(64);
        fn_800BC2EC(lbl_801131C0);
        lbl_803449EC = 0;
    }
    return result;
}

/* does the numbered/dir save exist on the mounted card? */
int fn_80068728(void)
{
    int result = 0;
    s32 x = 0;
    char name[64];
    s32 fileNo;
    u8 r;

    if (lbl_80344A18 == 3 && *(&lbl_80344A14) == 1) {
        fileNo = -1;
        if (fn_80069164(0, 0, 0) <= 0) {
            r = 0;
        } else {
            u8 ok = (0 <= x && x <= 1);

            if (!ok) {
                r = 0;
            } else {
                getSaveFileName(name, fileNo);
                r = fn_8006A82C(0, lbl_8011D550, &fileNo);
            }
        }
        if (r != 0) {
            result = 1;
        }
    }
    return result;
}

/* load a save-data block from the staged record into the dir buffer, then
 * normalize the 8 directory entry names (Xbox: get_vmu_directory-ish).
 * PARKED 116/116 (opcodes match): DST address-expr scheduling (mulli/add
 * order + 0x156F8 constant split) and the loop's register coloring differ;
 * functionally exact. */
int fn_800687FC(int a, int b)
{
    u8* base = lbl_8025EE80;
    int i = 1; /* transaction flag, then reused as the dir-scan counter */
    int bit = b + a * 4;
    s32 off;
    u8 pad[16]; /* unused, matches original frame */

    if (lbl_80343C78 & (1 << bit)) {
        if ((u8) fn_800696E8() != 1) {
            i = 0;
        }
        if (i) {
            memcpy(base + 0x10000 + a * 132 + b * 132 + 22264,
                   (u8*) lbl_80343C74 + 41416, 128);
        }
        cardExit();
        cardWaitResult();
        OSSetCurrentHeap(lbl_80344A08);
        OSDestroyHeap(lbl_80344A0C);
        fn_8006AF44((u8*) 0x310000);
        sysClearFlags(64);
        fn_800BC2EC(lbl_801131C0);
        lbl_803449EC = 0;
        if (i) {
            lbl_80343C78 &= ~(1 << bit);
        }
    } else {
        memcpy(base + 0x10000 + a * 132 + b * 132 + 22264,
               (u8*) lbl_80343C74 + 41416, 128);
    }
    if (i == 0) {
        return -1;
    }
    {
        u8* dir = base + 0x10000 + a * 132 + b * 132 + 22264;

        for (i = 0, off = 0; i < 8; i++, off += 16) {
            u8* e = dir + off;

            if (*(s32*) e == -1 || (s8) e[8] == 0) {
                strcpy((char*) (e + 8), lbl_803472D8);
            } else {
                char* nm = (char*) (e + 8);

                if (strcmp(nm, lbl_803472D8) == 0 ||
                    strcmp(nm, lbl_803472E0) == 0) {
                    strcpy(nm, lbl_803472E8);
                }
            }
        }
    }
    return i;
}

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
    *p = cardGetFreeBytes();
    return *p;
}

/* load game options from the card once (Xbox: check_prefs_loaded) */
void check_prefs_loaded(void)
{
    char* st = lbl_801131C0;
    GameOpts* opts = &lbl_80274E80;
    u8 pad[16]; /* unused, matches original frame */

    if (fn_80069164(0, 0, 0) == 0) {
        return;
    }
    if (lbl_803449D0 != 0) {
        return;
    }
    lbl_803449D0 = 1;
    if ((u8) fn_800696E8()) {
        *opts = *(GameOpts*) ((u8*) lbl_80343C74 + 8);
    }
    cardExit();
    cardWaitResult();
    OSSetCurrentHeap(lbl_80344A08);
    OSDestroyHeap(lbl_80344A0C);
    fn_8006AF44((u8*) 0x310000);
    sysClearFlags(64);
    fn_800BC2EC(st);
    lbl_803449EC = 0;
    opts->data[2] = (OSGetSoundMode() == 0) ? 0 : 1;
}

/* reset the dir-info table and card state (Xbox: init_all_dir_info) */
void init_all_dir_info(void)
{
    s32 off;
    s32 zero;
    s32 fill;
    u8* base;
    int i;
    u8 pad[40]; /* unused, matches original frame */

    zero = 0;
    i = zero;
    off = zero;
    base = lbl_80274578;
    fill = -1;
    for (; i < 8; i++, off += 16) {
        u8* e = base + off;

        *(s32*) e = fill;
        *(s32*) (e + 4) = fill;
        strcpy((char*) (e + 8), lbl_803472D8);
    }
    *(s32*) (base + 128) = zero;
    lbl_803449F0 = 0x10000 - 1400;
    cardInit();
    lbl_80344A24 = 0;
    lbl_80344A20 = 0;
    lbl_80344A18 = -1;
    lbl_80344A14 = -1;
    lbl_80344A10[0] = -1;
    lbl_803449D4 = 0;
    lbl_803449DC = 0;
    lbl_803449E0 = -1;
    lbl_803449D8 = 0;
}

/* load preferences with a full transaction (Xbox: InitPreferences).
 * Same shape as check_prefs_loaded but the transaction body is inlined
 * (AEA8 + dir alloc + 697D0) rather than calling 696E8.
 * PARKED 78/78: only the three flag-reset constants (0/1) land in r4/r0
 * swapped vs r0/r4 (dead-before-call, pure register coloring). */
int fn_80069540(void)
{
    int ret = 0;
    u8 pad[24]; /* unused, matches original frame */

    if (lbl_803449D0 == 0) {
        lbl_803449EC = 0;
        lbl_803449D0 = 1;
        lbl_803449F8 = 0;
        fn_800BC2EC(lbl_801131E8);
        while (fn_800BF168() == 0) {
            fn_80067B0C(-1);
        }
        fn_8006AEA8();
        lbl_80344A04 = (u8*) OSAllocFromHeap(__OSCurrHeap, 0x2D44C0);
        if ((u8) fn_800697D0()) {
            lbl_80274E80 = *(GameOpts*) ((u8*) lbl_80343C74 + 8);
            ret = 1;
        } else {
            ret = 0;
        }
        cardExit();
        cardWaitResult();
        OSSetCurrentHeap(lbl_80344A08);
        OSDestroyHeap(lbl_80344A0C);
        dcsAramReadTop(fn_800BF524() - 0x310000, (void*) 0x310000);
        sysClearFlags(64);
        fn_800BC2EC(lbl_801131C0);
        lbl_803449EC = 0;
        lbl_80274E80.data[2] = (OSGetSoundMode() == 0) ? 0 : 1;
    }
    return ret;
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
int fn_800696E8(void)
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
    sysSetFlags(64);
    buf = (u8*) fn_800BF524();
    dcsAramWriteTop((u32) buf - 0x310000, size);
    lo = buf - 0x310000;
    lbl_80344A0C = OSCreateHeap(lo, lo + size);
    lbl_80344A08 = OSSetCurrentHeap(lbl_80344A0C);
    lbl_80344A00 = (u8*) OSAllocFromHeap(__OSCurrHeap, 8192);
    lbl_803449FC = (u8*) OSAllocFromHeap(__OSCurrHeap, 0xA000);
    cardStart(lbl_80344A00 + 8192, 8192, 18);
    cardWaitResult();
    lbl_80344A04 = (u8*) OSAllocFromHeap(__OSCurrHeap, 0x2D44C0);
    return fn_800697D0();
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

    sysSetFlags(64);
    buf = (u8*) fn_800BF524();
    dcsAramWriteTop((u32) (buf - 0x310000), 0x310000);
    lo = buf - 0x310000;
    lbl_80344A0C = OSCreateHeap(lo, lo + size);
    lbl_80344A08 = OSSetCurrentHeap(lbl_80344A0C);
    lbl_80344A00 = (u8*) OSAllocFromHeap(__OSCurrHeap, 8192);
    lbl_803449FC = (u8*) OSAllocFromHeap(__OSCurrHeap, 0xA000);
    cardStart(lbl_80344A00 + 8192, 8192, 18);
    cardWaitResult();
}

void fn_8006AF44(u8* buf)
{
    u32 off = fn_800BF524();
    u8* b = buf;

    dcsAramReadTop(off - (u32) b, b);
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

    dcsAramWrite(fn_800BF524() - 0x310000, 0x9E0000, 0x310000);
    dcsAramRead(0xCF0000, fn_800BF524() - 0x310000, 0x310000);
    had = sysTestFlags(64);
    if (had) {
        sysClearFlags(64);
    }
    for (;;) {
        s32 stick;

        fn_8006B210(msg, (int) state, count, sel);
        stick = padMenuStickY(-1);
        if (padButtonPressed(-1, 4) != 0 || stick > 0) {
            if (++sel >= count) {
                sel = 0;
            }
        } else if (padButtonPressed(-1, 8) != 0 || stick < 0) {
            if (--sel < 0) {
                sel = count - 1;
            }
        }
        sysHandleReset();
        if (--timer <= 0) {
            timer = 30;
            dcsAramWrite(fn_800BF524() - 0x310000, 0xCF0000, 0x310000);
            dcsAramRead(0x9E0000, fn_800BF524() - 0x310000, 0x310000);
            fn_80067B0C(-1);
            dcsAramWrite(fn_800BF524() - 0x310000, 0x9E0000, 0x310000);
            dcsAramRead(0xCF0000, fn_800BF524() - 0x310000, 0x310000);
        }
        if (padButtonReleased(-1, 256) != 0) {
            break;
        }
    }
    dcsAramWrite(fn_800BF524() - 0x310000, 0xCF0000, 0x310000);
    dcsAramRead(0x9E0000, fn_800BF524() - 0x310000, 0x310000);
    if (had) {
        sysSetFlags(64);
    }
    return sel;
}

/* swap the 3.2MB save cache blocks in ARAM (0x9E0000 <-> 0xCF0000) */
void fn_8006B188(void)
{
    dcsAramWrite(fn_800BF524() - 0x310000, 0xCF0000, 0x310000);
    dcsAramRead(0x9E0000, fn_800BF524() - 0x310000, 0x310000);
}

void fn_8006B1CC(void)
{
    dcsAramWrite(fn_800BF524() - 0x310000, 0x9E0000, 0x310000);
    dcsAramRead(0xCF0000, fn_800BF524() - 0x310000, 0x310000);
}
