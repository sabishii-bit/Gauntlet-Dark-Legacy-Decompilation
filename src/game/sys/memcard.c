/*
 * memcard.c - savegame layer over dolphin/card (Xbox: MEMCARD.OBJ).
 *
 * GCN save files carry the fixed name "Gauntlet - Dark Legacy" and are
 * stamped with the "OKAY" (0x4F4B4159) magic + a byte-sum checksum.  The
 * PS2 serial strings ("BASLUS-20047save%04d", ...GameOpts, ...DirInfo) are
 * kept as the in-cache directory keys.
 *
 * This TU is NonMatching (the linked image comes from the extracted asm);
 * the goal here is a complete, compiling reconstruction with every function
 * named.  Names are taken from the Xbox MEMCARD.OBJ vocabulary where the
 * behaviour matches (saveMount is proven by the "SAVEMOUNT PORT/SLOT"
 * debug string); the internal engines/helpers use descriptive names.
 *
 * Functions are kept in address order (same-TU inlining depends on it).
 */
#include "types.h"
#include "game/dcs.h"

/* ---- libc ------------------------------------------------------------- */
int sprintf(char* dst, const char* fmt, ...);
char* strcpy(char* dst, const char* src);
char* strncpy(char* dst, const char* src, u32 n);
int strcmp(const char* a, const char* b);
void* memcpy(void* dst, const void* src, u32 n);
void* memset(void* dst, int c, u32 n);

/* ---- PS2 file-shim (game/fakelib.c) ---------------------------------- */
int sceOpen(const char* path, int flags, ...);
int sceLseek(int fd, int offset, int whence);
int sceRead(int fd, void* buf, int len);
int sceClose(int fd);

/* ---- dolphin/os ------------------------------------------------------- */
void* OSCreateHeap(void* lo, void* hi);
void* OSSetCurrentHeap(void* heap);
void* OSAllocFromHeap(void* heap, u32 size);
void OSDestroyHeap(void* heap);
u32 OSGetSoundMode(void);
void OSPanic(const char* file, int line, const char* fmt, ...);
void OSResetSystem(int reset, u32 resetCode, int forceMenu);
extern void* __OSCurrHeap;

/* ---- dolphin/card ----------------------------------------------------- */
int CARDProbe(s32 chan);
s32 CARDProbeEx(s32 chan, s32* memSize, s32* sectorSize);
s32 CARDCheck(s32 chan);
s32 CARDGetStatus(s32 chan, s32 fileNo, void* stat);
s32 CARDDelete(s32 chan, const char* fileName);
s32 CARDGetSerialNo(s32 chan, u32* serialNo);

/* ---- dolphin/vi ------------------------------------------------------- */
void VIWaitForRetrace(void);

/* ---- game/sys/cardutil.c (CARD command server) ----------------------- */
void cardInit(void);
void cardExit(void);
void cardStart(void* buf, u32 size, int arg);
void cardWaitResult(void);
s32 cardGetResult(void);
void cardMount(s32 chan, void* workArea, void* detachCb);
void cardUnmount(s32 chan);
void cardLoadFile(s32 chan, void* buf);
void cardReadFile(s32 chan, void* dst, void* src);
void cardWriteFile(s32 chan, void* name, void* buf);
s32 cardFormat(s32 chan);
s32 cardLock(void);
void cardUnlock(void);
s32 cardGetFreeBytes(void);
s32 cardGetTotalBytes(void);
s32 cardGetFreeFiles(void);
s32 cardGetUsedPercent(void);

/* ---- game/sys/sysservice.c ------------------------------------------- */
void sysHandleReset(void);
int sysTestFlags(int flag);
void sysClearFlags(int flag);
void sysSetFlags(int flag);
void sysResetService(void);

/* ---- pads (game) ------------------------------------------------------ */
s32 padMenuStickY(int pad);
int padButtonPressed(int pad, u32 mask);
int padButtonReleased(int pad, u32 mask);

/* ---- textures (game/g3d) --------------------------------------------- */
void* TEXGet(int bank, int index);
void TEXGetPalette(int index, void* out);

/* ---- misc game services ---------------------------------------------- */
u32 GetHiMemCacheTop(void);
int FileSystemReading(void);
void bulletproof_printf(const char* fmt, ...);
void pbPulseTime(void);
void fn_80067B0C(int flags);       /* service pump (movie/audio) while busy */

/* ---- UI / message-box (game) used by drawMemCardMessage -------------- */
void msgUpdate(void);
void* MBNewBlit(void* tex, int a, int b);
void* MBNewTempQuad(void);
void MBEndFrame(void);
void MBRemoveBlit(void* blit);
int FixMLineText(const char* text, char* work, void* lineOut);
int DrawNormalText(const char* text, int color, f32 scale);
int FontHeight(int color, f32 scale);
int TextMLines(const char* text);
void mbBlitProject(void* blit, int w);
void mbBlitCalcWidth(void* blit, int x, int y, f32 z);
void DrawTextSub(int a, const char* text, int color, f32 s1, int f, f32 s2, int g);
void vibrators_off(void);
void fn_800B6B80(void);
void fn_800B5D20(int a);
void fn_800B5CCC(int a);
void fn_800B2940(void* blit, int a);

/* ---- data: in-memory directory + options ----------------------------- */
typedef struct GameOpts {
    u32 data[8];                   /* 32-byte options block; [2] = stereo   */
} GameOpts;

typedef struct SaveFileBlock {
    u32 w[1293];                   /* one 5172-byte per-file save data block */
} SaveFileBlock;

extern u8 lbl_80274578[];          /* dir-info tables: stride 132 (8x16 +4) */
extern u8 lbl_8025EE80[];          /* VMU/dir working buffer (dir @+0x156F8)*/
extern u8 optglobals[0xA0];        /* options TU globals; prefs at +0x80 */
#define gameOpts (*(GameOpts*)&optglobals[0x80])
extern u8* lbl_80343C74;           /* staged save record (opts@+8, dir@+0xA1C8)*/
extern char lbl_803472D8[8];       /* default dir name (sdata, SDA21)       */
extern char lbl_803472E0[8];       /* dir name variant (sdata, SDA21)       */
extern char lbl_803472E8[8];       /* replacement dir name (sdata, SDA21)   */
extern u8 lbl_8026A2C4[0x6C];      /* card write descriptor/name buffer     */

/* string pools */
extern char lbl_801131C0[];        /* .rodata pool: "Finish save cache..."; */
                                   /*  +40 "Beginning...", +80 GameOpts key, */
                                   /*  +104 DirInfo key, +124 save%04d,      */
                                   /*  +148 "Entered SAVEMOUNT PORT/SLOT",   */
                                   /*  +608 "MEMCARD.C", +620 opening.bnr err,*/
                                   /*  +672 banner.tpl, +696 icon.tpl,       */
                                   /*  +716 "Gauntlet Save Data",            */
                                   /*  +920/+956 unsupported tex format panic*/
extern char lbl_801131E8[];        /* "Beginning save cache transaction..." */
extern char lbl_80113548[];        /* "/opening.bnr"                        */
extern char lbl_8011CDE0[];        /* .data prompt pool (see decoded map)   */
extern char lbl_8011D2A0[];        /* "...Was Removed. Game Save disabled."  */
extern char lbl_8011D550[];        /* "Gauntlet - Dark Legacy" (save name)  */

/* menu option-pointer arrays (sdata) */
extern char* lbl_80343C6C[2];      /* {retry, "Continue Without Saving"}    */
extern char* lbl_80343C68[1];      /* {"OK"}                                */

/* save-cache / card state (sbss + sdata) */
extern s32 lbl_803449F0;           /* per-block byte budget                 */
extern s32 lbl_803449D0;           /* prefs_loaded flag                     */
extern s32 lbl_803449D4;
extern u8 lbl_803449D8;
extern s32 lbl_803449DC;
extern s32 lbl_803449E0;
extern u32 lbl_803449E4;           /* banner palette handle                 */
extern u32 lbl_803449E8;           /* icon palette handle                   */
extern u32 lbl_803449EC;           /* opening-banner texture handle         */
extern s32 lbl_803449F4;           /* per-frame service counter (wrap 60)   */
extern u32 lbl_803449F8;           /* built save-image size                 */
extern u8* lbl_803449FC;           /* file buffer                           */
extern u8* lbl_80344A00;           /* card workArea                         */
extern u8* lbl_80344A04;           /* directory buffer                      */
extern void* lbl_80344A08;         /* previous heap                         */
extern void* lbl_80344A0C;         /* save heap                             */
extern s32 lbl_80344A10[2];        /* per-(port+slot) cached free bytes     */
extern s32 lbl_80344A14;           /* per-(port+slot) card-present flag      */
extern s32 lbl_80344A18;           /* per-(port+slot) card state (3=ready)  */
extern s32 lbl_80344A20;           /* card serial word 0                    */
extern s32 lbl_80344A24;           /* card serial word 1                    */
extern u32 lbl_80343C78;           /* directory refresh flags               */

/* misc timing / UI globals */
extern s32 sSeconds;               /* frame-driven second counter           */
extern float lbl_803472F0;         /* probe time-out threshold (seconds)    */
extern double lbl_803472F8;        /* int->double magic bias (0x43300000..) */
extern void* gWinGlobals;
extern u8 gTextWorkBuf[0x800];
extern u8 gTextFormatBuf[0x404];
extern s32 lbl_80344568;
extern s32 lbl_80344A30;
extern u8 lbl_80344A5C;
extern u8 lbl_80344A5D;
extern s32 lbl_80344A54;
extern float lbl_80344A58;
extern u8 lbl_802A5D1C[];
extern u8 lbl_802A4AA4[];
extern s32 lbl_80343CCC;
extern char lbl_80347368[8];
extern float lbl_80347370;
extern float lbl_80347374;
extern float lbl_80347378;

/* ---- forward (same TU, later addresses) ------------------------------ */
void pageSaveCacheIn(void);
void pageSaveCacheOut(void);
s32 saveMenuPrompt(const char* msg, char** options, s32 count);
void getSaveFileName(char* dst, s32 fileNo);
u8 beginSaveCacheTransaction(void);
void restoreSaveCache(u32 size);
void beginSaveTransaction(void);
u8 loadGauntletSave(void);
int writeGauntletSave(void);
u8* loadOpeningBanner(void);
u8* buildSaveImage(const char* name, void* hdr, int bannerTex, int iconTex,
                   int fmtA, int fmtB, const char* comment);
void cardRemovedCallback(int arg);
u8 vmu_exists(s32 chan, const char* name, s32* fileNoOut);
s32 saveMount(s32 port, s32 slot, s32 doFormat);
void drawMemCardMessage(const char* msg, char** options, s32 count1, s32 count2);

/*
 * add_vmu_file - write one 16-byte directory entry {size, time, name[8]}
 * into the in-memory dir table, then commit the whole save record to the
 * card.  (Was labelled saveSave; the actual player-data save is saveSave
 * below - this one only touches the directory + full commit.)
 * PARKED 79/80: one extra addi copying `row` into its preserved register.
 */
int add_vmu_file(int a, int b, int c, const char* name, u32 v0, u32 v1)
{
    u8* row = lbl_80274578 + a * 132 + b * 132;
    u8* rec;
    int result;
    u8 unused[8]; /* matches original frame */

    rec = row + c * 16;
    strncpy((char*) (rec + 8), name, 8);
    *(u32*) rec = v0;
    *(u32*) (rec + 4) = v1;
    result = ((u8) beginSaveCacheTransaction() == 1) ? 1 : 0;
    if (result) {
        memcpy((u8*) lbl_80343C74 + 41416, row, 128);
        writeGauntletSave();
        lbl_80343C78 |= 0xFFFFFFFF;
        cardExit();
        cardWaitResult();
        OSSetCurrentHeap(lbl_80344A08);
        OSDestroyHeap(lbl_80344A0C);
        restoreSaveCache(0x310000);
        sysClearFlags(64);
        bulletproof_printf(lbl_801131C0);
        lbl_803449EC = 0;
    } else {
        cardExit();
        cardWaitResult();
        OSSetCurrentHeap(lbl_80344A08);
        OSDestroyHeap(lbl_80344A0C);
        restoreSaveCache(0x310000);
        sysClearFlags(64);
        bulletproof_printf(lbl_801131C0);
        lbl_803449EC = 0;
    }
    return result;
}

/* saveExists - does the numbered/dir save exist on the mounted card? */
int saveExists(void)
{
    s32 result = 0;
    s32 x = 0;
    s32* pPresent = &lbl_80344A14;
    char name[64];
    s32 fileNo;
    u8 r;
    char* saveName = lbl_8011D550;

    if (lbl_80344A18 == 3 && *pPresent == 1) {
        fileNo = -1;
        if (saveMount(0, 0, 0) <= 0) {
            r = 0;
        } else {
            u8 ok = (0 <= x && x <= 1);

            if (!ok) {
                r = 0;
            } else {
                getSaveFileName(name, fileNo);
                r = vmu_exists(0, saveName, &fileNo);
            }
        }
        if (r != 0) {
            result = 1;
        }
    }
    return result;
}

/*
 * get_vmu_directory - pull the staged directory into the VMU work buffer
 * then normalize the 8 directory entry names.
 * PARKED 116/116 (opcodes match): DST address-expr scheduling differs.
 */
int get_vmu_directory(int a, int b)
{
    u8* base = lbl_8025EE80;
    int i = 1; /* transaction flag, then reused as the dir-scan counter */
    int bit = b + a * 4;
    s32 off;
    u8 pad[16]; /* unused, matches original frame */

    if (lbl_80343C78 & (1 << bit)) {
        if ((u8) beginSaveCacheTransaction() != 1) {
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
        restoreSaveCache(0x310000);
        sysClearFlags(64);
        bulletproof_printf(lbl_801131C0);
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

/*
 * vmu_directory_exists - map the cached card state to a save result code
 * (in-memory only, no card access).  1 = a valid save is present.
 * PARKED: dead arms preserved from the original.
 */
s32 vmu_directory_exists(void)
{
    s32 state = lbl_80344A18;
    s32 result = 0;
    s32* p = &lbl_80344A14;
    s32 present;

    if (state == -1) {
        return result;
    }
    present = *p;
    if (present == 1) {
        result = (state == 3) ? 1 : -1;
        return result;
    }
    if (result != 0) {
        return result;
    }
    if (present == 1) {
        return -2;
    }
    return -1;
}

/* serve_memcard - per-frame service tick (60-frame wrap counter) */
void serve_memcard(void)
{
    lbl_803449F4++;
    if (lbl_803449F4 > 60) {
        lbl_803449F4 = 0;
    }
}

/*
 * saveLoad - mount, run the load engine, then copy one file's data block
 * out of the staged record into the caller's buffer.
 */
int saveLoad(int port, int slot, int fileNo, void* dst)
{
    char* rpool = lbl_801131C0;
    u8 unused_hi[8];
    char name[64];
    u8 unused[80];
    u8 ret;
    u8 ok;

    if (saveMount(port, slot, 0) <= 0) {
        return 0;
    }
    ok = (0 <= port && port <= 1);
    if (!ok) {
        return 0;
    }
    /* inlined getSaveFileName(name, fileNo) */
    if (fileNo == -2) {
        strcpy(name, rpool + 80);
    } else if (fileNo == -1) {
        strcpy(name, rpool + 104);
    } else {
        sprintf(name, rpool + 124, fileNo + 1);
    }
    lbl_803449EC = 0;
    lbl_803449F8 = 0;
    bulletproof_printf(rpool + 40);
    while (FileSystemReading() != 0) {
        fn_80067B0C(-1);
    }
    beginSaveTransaction();
    lbl_80344A04 = (u8*) OSAllocFromHeap(__OSCurrHeap, 0x2D44C0);
    if ((u8) loadGauntletSave()) {
        *(SaveFileBlock*) dst =
            *(SaveFileBlock*) ((u8*) lbl_80343C74 + fileNo * 5172 + 40);
        ret = 1;
    } else {
        ret = 0;
    }
    cardExit();
    cardWaitResult();
    OSSetCurrentHeap(lbl_80344A08);
    OSDestroyHeap(lbl_80344A0C);
    dcsAramReadTop((void*)(GetHiMemCacheTop() - 0x310000), 0x310000);
    sysClearFlags(64);
    bulletproof_printf(rpool);
    lbl_803449EC = 0;
    return ret;
}

/*
 * saveSave - mount, run the load engine (to refresh the directory), stage
 * options + the caller's player-data block + the dir table, then commit.
 */
int saveSave(int port, int slot, int fileNo, void* src)
{
    char* rpool = lbl_801131C0;
    u8 unused_hi[16];
    char name[64];
    u8 unused_lo[16];

    if (saveMount(port, slot, 0) <= 0) {
        return 0;
    }
    /* inlined getSaveFileName(name, fileNo) (key built, result unused) */
    if (fileNo == -2) {
        strcpy(name, rpool + 80);
    } else if (fileNo == -1) {
        strcpy(name, rpool + 104);
    } else {
        sprintf(name, rpool + 124, fileNo + 1);
    }
    lbl_803449EC = 0;
    lbl_803449F8 = 0;
    bulletproof_printf(rpool + 40);
    while (FileSystemReading() != 0) {
        fn_80067B0C(-1);
    }
    beginSaveTransaction();
    lbl_80344A04 = (u8*) OSAllocFromHeap(__OSCurrHeap, 0x2D44C0);
    loadGauntletSave();
    *(GameOpts*) ((u8*) lbl_80343C74 + 8) = gameOpts;
    *(SaveFileBlock*) ((u8*) lbl_80343C74 + fileNo * 5172 + 40) =
        *(SaveFileBlock*) src;
    memcpy((u8*) lbl_80343C74 + 41416, lbl_80274578, 128);
    writeGauntletSave();
    lbl_80343C78 |= 0xFFFFFFFF;
    cardExit();
    cardWaitResult();
    OSSetCurrentHeap(lbl_80344A08);
    OSDestroyHeap(lbl_80344A0C);
    dcsAramReadTop((void*)(GetHiMemCacheTop() - 0x310000), 0x310000);
    sysClearFlags(64);
    bulletproof_printf(rpool);
    lbl_803449EC = 0;
    return 1;
}

/* fixed size of a Gauntlet save in bytes */
s32 saveFileSize(void)
{
    return 128272;
}

/* saveGetFreeBytes - free bytes for a (port,slot), cached on first use.
 * Near-match: address-expr scheduling of &lbl_80344A10[port][slot] and the
 * MWCC `&&` short-circuit ordering differ (regalloc-only, parked). */
s32 saveGetFreeBytes(s32 port, s32 slot)
{
    s32* p;
    u8 ok;
    u8 unused[16]; /* matches original frame */

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

/* check_prefs_loaded - load game options from the card once */
void check_prefs_loaded(void)
{
    char* st = lbl_801131C0;
    GameOpts* opts = &gameOpts;
    u8 pad[16]; /* unused, matches original frame */

    if (saveMount(0, 0, 0) == 0) {
        return;
    }
    if (lbl_803449D0 != 0) {
        return;
    }
    lbl_803449D0 = 1;
    if ((u8) beginSaveCacheTransaction()) {
        *opts = *(GameOpts*) ((u8*) lbl_80343C74 + 8);
    }
    cardExit();
    cardWaitResult();
    OSSetCurrentHeap(lbl_80344A08);
    OSDestroyHeap(lbl_80344A0C);
    restoreSaveCache(0x310000);
    sysClearFlags(64);
    bulletproof_printf(st);
    lbl_803449EC = 0;
    opts->data[2] = (OSGetSoundMode() == 0) ? 0 : 1;
}

/* init_all_dir_info - reset the dir-info table and card state */
void init_all_dir_info(void)
{
    s32 off;
    s32 zero;
    s32 fill;
    u8* base;
    int i;
    u8 pad[48]; /* unused, matches original frame */

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

/*
 * MemCardCreateGaunt - create a fresh Gauntlet save: reset the directory,
 * mount with format enabled, stage cleared data + options, then commit.
 */
int MemCardCreateGaunt(int port, int slot)
{
    s32 off;
    u8* base = lbl_80274578;
    int i;

    for (i = 0, off = 0; i < 8; i++, off += 16) {
        u8* e = base + off;

        *(s32*) e = -1;
        *(s32*) (e + 4) = -1;
        strcpy((char*) (e + 8), lbl_803472D8);
    }
    if (saveMount(port, slot, 1) <= 0) {
        return 0;
    }
    lbl_803449EC = 0;
    lbl_803449F8 = 0;
    bulletproof_printf(lbl_801131E8);
    while (FileSystemReading() != 0) {
        fn_80067B0C(-1);
    }
    beginSaveTransaction();
    lbl_80344A04 = (u8*) OSAllocFromHeap(__OSCurrHeap, 0x2D44C0);
    loadGauntletSave();
    *(GameOpts*) ((u8*) lbl_80343C74 + 8) = gameOpts;
    memcpy((u8*) lbl_80343C74 + 41416, lbl_80274578, 128);
    memset((u8*) lbl_80343C74 + 40, 0, 0x10000 - 24160);
    writeGauntletSave();
    lbl_80343C78 |= 0xFFFFFFFF;
    cardExit();
    cardWaitResult();
    OSSetCurrentHeap(lbl_80344A08);
    OSDestroyHeap(lbl_80344A0C);
    dcsAramReadTop((void*)(GetHiMemCacheTop() - 0x310000), 0x310000);
    sysClearFlags(64);
    bulletproof_printf(lbl_801131C0);
    lbl_803449EC = 0;
    return 1;
}

/*
 * saveMount - probe + mount the card in (port,slot); optionally format it.
 * Proven name: prints "Entered SAVEMOUNT PORT/SLOT %d,%d".  Maps the CARD
 * probe/mount/check status codes into the cached (state, present) pair and
 * returns 1 only when the card is fully ready (state 3, present 1).
 */
s32 saveMount(s32 port, s32 slot, s32 doFormat)
{
    char* pool = lbl_801131C0;
    s32* pState = &lbl_80344A18 + port + slot;
    s32* pPresent = &lbl_80344A14 + port + slot;
    s32 idx = slot + port * 4;
    s32 probe;
    s32 memSize;
    s32 sectorSize;
    u8 mounted = 0;
    u8 retry;

    bulletproof_printf(pool + 148, port, slot);   /* "Entered SAVEMOUNT..." */
    if (idx > 1) {
        return 0;
    }

    /* poll the slot until CARDProbeEx reports a stable status */
    do {
        retry = 0;
        probe = CARDProbeEx(0, &memSize, &sectorSize);
        if (probe == -2 || probe == 0) {
            *pState = 0;
            *pPresent = 0;
        } else if (probe == -128 || probe == -3) {
            *pState = -1;
            *pPresent = -1;
        } else if (probe == -1) {
            retry = 1;
        }
        if (retry) {
            VIWaitForRetrace();
        }
    } while (retry);

    sysSetFlags(64);
    dcsAramWriteTop((void*)(GetHiMemCacheTop() - 0x310000), 0x310000);
    lbl_80344A0C = OSCreateHeap((void*) (GetHiMemCacheTop() - 0x310000),
                                (void*) (GetHiMemCacheTop()));
    lbl_80344A08 = OSSetCurrentHeap(lbl_80344A0C);
    lbl_80344A00 = (u8*) OSAllocFromHeap(__OSCurrHeap, 8192);
    lbl_803449FC = (u8*) OSAllocFromHeap(__OSCurrHeap, 0x10000 - 24576);
    cardStart(lbl_80344A00 + 8192, 8192, 18);
    cardWaitResult();
    cardMount(0, lbl_803449FC, cardRemovedCallback);
    cardWaitResult();

    lbl_80344A10[idx] = -1;
    if (cardGetResult() == 0) {
        *pPresent = 1;
        *pState = 3;
        mounted = 1;
    } else {
        s32 r = cardGetResult();

        if (r == -5) {
            *pPresent = 0;
            *pState = 2;
        } else if (r == -13 || r == -128) {
            *pPresent = 0;
            *pState = (r == -13) ? 1 : 2;
        } else if (r == -6) {
            *pPresent = 0;
            *pState = 2;
            mounted = 1;
        } else if (r == -2) {
            *pPresent = 0;
            *pState = 0;
        } else if (r == -3) {
            *pState = -1;
            *pPresent = -1;
        }
        if (doFormat) {
            cardFormat(0);
            cardWaitResult();
            if (cardGetResult() == 0) {
                *pState = 3;
                *pPresent = 1;
            }
        }
    }

    if (mounted) {
        s32 c = CARDCheck(0);

        if (c == -13 || c == -128 || c == -6) {
            *pPresent = 0;
            *pState = 2;
        } else if (c == -1) {
            *pPresent = 0;
            *pState = 2;
        } else if (c < -1 && c >= -2) {
            /* ok */
        } else if (c >= 1) {
            /* ok */
        } else if (c == 0) {
            *pPresent = 1;
            *pState = 3;
        }
    }

    cardExit();
    cardWaitResult();
    OSSetCurrentHeap(lbl_80344A08);
    OSDestroyHeap(lbl_80344A0C);
    dcsAramReadTop((void*)(GetHiMemCacheTop() - 0x310000), 0x310000);
    sysClearFlags(64);
    lbl_80343C78 |= 0xFFFFFFFF;

    if (*pState == 3 && *pPresent == 1) {
        bulletproof_printf(pool + 184);           /* "SAVEMOUNT RETURNING 1" */
        return 1;
    }
    bulletproof_printf(pool + 212);               /* "SAVEMOUNT RETURNING -1"*/
    {
        s32* serial = &lbl_80344A20 + port * 2 + slot * 2;

        serial[1] = 0;
        serial[0] = -1;
    }
    return 0;
}

/*
 * InitPreferences - one-time preferences load with a full save-cache
 * transaction (the transaction body is inlined rather than calling the
 * shared beginSaveCacheTransaction helper).
 * PARKED 78/78: only two flag-reset constants land in swapped registers.
 */
int InitPreferences(void)
{
    int ret = 0;
    u8 pad[24]; /* unused, matches original frame */

    if (lbl_803449D0 == 0) {
        lbl_803449EC = 0;
        lbl_803449D0 = 1;
        lbl_803449F8 = 0;
        bulletproof_printf(lbl_801131E8);
        while (FileSystemReading() != 0) {
            fn_80067B0C(-1);
        }
        beginSaveTransaction();
        lbl_80344A04 = (u8*) OSAllocFromHeap(__OSCurrHeap, 0x2D44C0);
        if ((u8) loadGauntletSave()) {
            gameOpts = *(GameOpts*) ((u8*) lbl_80343C74 + 8);
            ret = 1;
        } else {
            ret = 0;
        }
        cardExit();
        cardWaitResult();
        OSSetCurrentHeap(lbl_80344A08);
        OSDestroyHeap(lbl_80344A0C);
        dcsAramReadTop((void*)(GetHiMemCacheTop() - 0x310000), 0x310000);
        sysClearFlags(64);
        bulletproof_printf(lbl_801131C0);
        lbl_803449EC = 0;
        gameOpts.data[2] = (OSGetSoundMode() == 0) ? 0 : 1;
    }
    return ret;
}

/* getSaveFileName - slot -2 = options, -1 = directory info, else numbered */
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

/*
 * beginSaveCacheTransaction - wait for the loader, page the staging block
 * out of ARAM, build the save heap, allocate workArea + file + directory
 * buffers, then scan/load the card via loadGauntletSave.
 */
u8 beginSaveCacheTransaction(void)
{
    u8* buf;
    u32 size;
    u8* lo;
    u8 pad[24]; /* unused, matches original frame */

    lbl_803449EC = 0;
    lbl_803449F8 = 0;
    bulletproof_printf(lbl_801131E8);
    while (FileSystemReading() != 0) {
        fn_80067B0C(-1);
    }
    size = 0x310000;
    sysSetFlags(64);
    buf = (u8*) GetHiMemCacheTop();
    dcsAramWriteTop(buf - 0x310000, size);
    lo = buf - 0x310000;
    lbl_80344A0C = OSCreateHeap(lo, lo + size);
    lbl_80344A08 = OSSetCurrentHeap(lbl_80344A0C);
    lbl_80344A00 = (u8*) OSAllocFromHeap(__OSCurrHeap, 8192);
    lbl_803449FC = (u8*) OSAllocFromHeap(__OSCurrHeap, 0xA000);
    cardStart(lbl_80344A00 + 8192, 8192, 18);
    cardWaitResult();
    lbl_80344A04 = (u8*) OSAllocFromHeap(__OSCurrHeap, 0x2D44C0);
    return loadGauntletSave();
}

/*
 * loadGauntletSave - the card scan/mount/load/verify engine.  Probes and
 * mounts the card (retrying or aborting through saveMenuPrompt on every
 * CARD error code), loads the directory, finds the "Gauntlet - Dark Legacy"
 * file, reads it, verifies the byte-sum checksum and the "OKAY" magic, and
 * offers to delete/recreate a corrupt file.  Returns 1 when a valid save
 * was loaded, 0 otherwise.  (Structural reconstruction of the 0xD64 giant;
 * the ~15 inlined per-error retry menus are folded into the retry loop.)
 */
u8 loadGauntletSave(void)
{
    char* dpool = lbl_8011CDE0;
    char* rpool = lbl_801131C0;
    u8 hadCard;
    u8 needCheck;
    u8 needFormat;
    s32 probe;
    s32 memSize;
    s32 sectorSize;
    u32 serial[2];
    f64 startSec;
    u8 result = 0;
    int i;

retry:
    hadCard = 0;
    needCheck = 0;
    needFormat = 0;
    for (i = 0; i < 2; i++) {
        pageSaveCacheOut();
        drawMemCardMessage(dpool + 1712, 0, 0, 0);   /* "Accessing..." */
        pageSaveCacheIn();
    }

    /* time-limited probe */
    pbPulseTime();
    startSec = (f64) sSeconds;
    for (;;) {
        probe = CARDProbeEx(0, &memSize, &sectorSize);
        pbPulseTime();
        if ((f64) sSeconds - startSec > lbl_803472F0) {
            if (saveMenuPrompt(dpool + 1052, lbl_80343C6C, 2) == 1) {
                lbl_80344A24 = 0;
                lbl_80344A18 = -1;
                lbl_80344A14 = -1;
                lbl_80344A20 = 0;
                return 0;
            }
            goto retry;
        }
        if (probe != -1) {
            break;
        }
    }

    if (probe == -2 || probe == -128) {
        if (saveMenuPrompt(dpool + 1052, lbl_80343C6C, 2) == 1) {
            lbl_80344A24 = 0;
            lbl_80344A18 = -1;
            lbl_80344A14 = -1;
            lbl_80344A20 = 0;
            return 0;
        }
        goto retry;
    }
    if (probe == -3) {
        if (saveMenuPrompt(dpool + 256, lbl_80343C6C, 2) == 1) {
            lbl_80344A24 = 0;
            lbl_80344A18 = -1;
            lbl_80344A14 = -1;
            lbl_80344A20 = 0;
            return 0;
        }
        goto retry;
    }
    if (memSize != 8192) {
        if (saveMenuPrompt(dpool + 44, lbl_80343C6C, 2) == 1) {
            lbl_80344A24 = 0;
            lbl_80344A18 = -1;
            lbl_80344A14 = -1;
            lbl_80344A20 = 0;
            return 0;
        }
        goto retry;
    }

    cardMount(0, lbl_803449FC, cardRemovedCallback);
    cardWaitResult();
    {
        s32 r = cardGetResult();
        char* fmtMsg = dpool + 776;                 /* "must be formatted" */

        if (r == -13) {
            fmtMsg = dpool + 824;                   /* wrong market */
            needFormat = 1;
        } else if (r == -6) {
            fmtMsg = dpool + 948;                   /* corrupt */
            hadCard = 1;
            needCheck = 1;
        } else if (r == -5 || r == -128) {
            if (saveMenuPrompt(rpool + 240, lbl_80343C6C, 2) == 1) {
                lbl_80344A24 = 0;
                lbl_80344A18 = -1;
                lbl_80344A14 = -1;
                lbl_80344A20 = 0;
                return 0;
            }
            goto retry;
        } else if (r == -3) {
            if (saveMenuPrompt(dpool + 256, lbl_80343C6C, 2) == 1) {
                lbl_80344A24 = 0;
                lbl_80344A18 = -1;
                lbl_80344A14 = -1;
                lbl_80344A20 = 0;
                return 0;
            }
            goto retry;
        } else if (r < -3 && r >= -13) {
            if (saveMenuPrompt(rpool + 392, lbl_80343C6C, 2) == 1) {
                lbl_80344A24 = 0;
                lbl_80344A18 = -1;
                lbl_80344A14 = -1;
                lbl_80344A20 = 0;
                return 0;
            }
            goto retry;
        }

        if (needCheck) {
            s32 c = CARDCheck(0);

            if (c == -6) {
                needFormat = 1;
            } else if (c == -3) {
                if (saveMenuPrompt(dpool + 256, lbl_80343C6C, 2) == 1) {
                    lbl_80344A24 = 0;
                    lbl_80344A18 = -1;
                    lbl_80344A14 = -1;
                    lbl_80344A20 = 0;
                    return 0;
                }
                goto retry;
            } else if (c == -128) {
                if (saveMenuPrompt(dpool + 1052, lbl_80343C6C, 2) == 1) {
                    lbl_80344A24 = 0;
                    lbl_80344A18 = -1;
                    lbl_80344A14 = -1;
                    lbl_80344A20 = 0;
                    return 0;
                }
                goto retry;
            }
        }

        if ((lbl_80344A20 != 0 || lbl_80344A24 != 0) && needFormat) {
            if (saveMenuPrompt(rpool + 1516, lbl_80343C6C, 2) == 1) {
                lbl_80344A24 = 0;
                lbl_80344A18 = -1;
                lbl_80344A14 = -1;
                lbl_80344A20 = 0;
            } else {
                goto retry;
            }
            return 0;
        }

        if (needFormat) {
            /* offer a format (3-option prompt) */
            s32 choice = saveMenuPrompt(fmtMsg, (char**) (dpool + 1880), 3);

            if (choice == 2) {
                cardUnmount(0);
                cardWaitResult();
                lbl_80344A24 = 0;
                lbl_80344A18 = -1;
                lbl_80344A14 = -1;
                lbl_80344A20 = 0;
                return 0;
            }
            if (choice != 1) {
                for (i = 0; i < 2; i++) {
                    pageSaveCacheOut();
                    drawMemCardMessage(dpool + 1796, 0, 0, 0); /* Formatting */
                    pageSaveCacheIn();
                }
                if (cardFormat(0) != 0) {
                    if (saveMenuPrompt(dpool + 256, lbl_80343C6C, 2) == 1) {
                        lbl_80344A24 = 0;
                        lbl_80344A18 = -1;
                        lbl_80344A14 = -1;
                        lbl_80344A20 = 0;
                        return 0;
                    }
                    goto retry;
                }
                saveMenuPrompt(rpool + 548, 0, 0);  /* "Format Successful." */
                lbl_80344A18 = 3;
                lbl_80344A14 = 1;
            }
        }
    }

    cardLoadFile(0, lbl_80344A04);
    cardWaitResult();
    if (cardGetResult() == 0) {
        s32 count = cardLock();
        s32 off = count * 23360;

        while (count != 0) {
            s32 fileNo = *(s32*) (lbl_80344A04 + off - 256);
            char stat[64];

            off -= 23360;
            count--;
            CARDGetStatus(0, fileNo, stat);
            if (strcmp(stat, lbl_8011D550) == 0) {
                hadCard = 1;
                *(s32*) (lbl_80344A04 + off + 23104) = fileNo;
                break;
            }
        }
        if (cardUnlock(), cardWaitResult(), cardGetResult() != 0) {
            saveMenuPrompt(rpool + 568, lbl_80343C68, 1);
            return 0;
        }
    }

    if (hadCard) {
        /* found the save file - read + verify it */
        u32 sum;
        u8* p;
        u8* img;

        if (lbl_803449EC == 0) {
            lbl_803449EC = (u32) loadOpeningBanner();
            if (lbl_803449EC == 0) {
                OSPanic(rpool + 608, 1019, rpool + 620);
            }
            lbl_803449E8 = 0;
            lbl_803449E4 = 0;
            TEXGetPalette(0, rpool + 672);
            TEXGetPalette(0, rpool + 696);
        }
        img = buildSaveImage(lbl_8011D550, rpool + 716, (int) lbl_803449EC,
                             (int) lbl_803449E4, 2, 0, (char*) lbl_803449E8);
        cardReadFile(0, img, (void*) lbl_803449F8);
        cardWaitResult();

        /* copy image into the staged record and recompute checksum */
        memcpy(lbl_80343C74, img, 5193 * 8);
        sum = 0;
        for (p = lbl_80343C74; p < lbl_80343C74 + 0x10000 - 23992; p++) {
            sum += *p;
        }
        if (*(u32*) lbl_80343C74 == sum &&
            *(u32*) (lbl_80343C74 + 4) == 0x4F4B4159 /* "OKAY" */) {
            result = 1;
        } else {
            saveMenuPrompt(rpool + 736, lbl_80343C68, 1);  /* corrupt */
            {
                s32 d = CARDDelete(0, lbl_8011D550);

                if (d == -5 || d == -10 || d == -128 || (d < -1 && d >= -2)) {
                    saveMenuPrompt(rpool + 832, lbl_80343C68, 1);
                    lbl_80344A24 = 0;
                    lbl_80344A18 = -1;
                    lbl_80344A14 = -1;
                    lbl_80344A20 = 0;
                    return 0;
                }
                goto retry;
            }
        }
    }

    CARDGetSerialNo(0, serial);
    cardUnmount(0);
    cardWaitResult();

    if ((lbl_80344A20 != 0 || lbl_80344A24 != 0) &&
        ((u32) lbl_80344A20 != serial[0] || (u32) lbl_80344A24 != serial[1])) {
        if (saveMenuPrompt(rpool + 1516, lbl_80343C6C, 2) == 1) {
            lbl_80344A24 = 0;
            lbl_80344A18 = -1;
            lbl_80344A14 = -1;
            lbl_80344A20 = 0;
            return 0;
        }
        goto retry;
    }

    if (result) {
        if ((lbl_80344A20 != 0 || lbl_80344A24 != 0) &&
            ((u32) serial[0] != (u32) lbl_80344A20 ||
             (u32) serial[1] != (u32) lbl_80344A24)) {
            if (saveMenuPrompt(rpool + 404, lbl_80343C6C, 2) == 1) {
                lbl_80344A24 = 0;
                lbl_80344A18 = -1;
                lbl_80344A14 = -1;
                lbl_80344A20 = 0;
                return 0;
            }
            goto retry;
        }
        lbl_80344A24 = serial[1];
        lbl_80344A18 = 3;
        lbl_80344A14 = 1;
        lbl_80344A20 = serial[0];
    } else {
        /* no valid save present - check free space / offer creation */
        s32 needFiles = 1;
        s32 needBlocks = (cardGetTotalBytes(),
                          ((0x10000 - 1401 + cardGetTotalBytes()) /
                           cardGetTotalBytes()));

        if (cardGetUsedPercent() < needBlocks || cardGetFreeFiles() < needFiles) {
            s32 c = saveMenuPrompt(rpool + 600, (char**) (rpool + 1892), 3);

            if (c == 1) {
                OSResetSystem(1, 0x80000000, 1);
            } else if (c == 2) {
                return 0;
            }
            goto retry;
        }
    }
    return result;
}

/*
 * writeGauntletSave - the card write/commit engine.  Loads the opening
 * banner + palettes on first use, shows the "Accessing..." dialog, mounts
 * the card, stamps "OKAY" + checksum into the staged record, builds the
 * save image and writes it with cardWriteFile, then re-reads it back.
 * (Reconstruction of the 0x2F8 engine; retry menus folded into the loop.)
 */
int writeGauntletSave(void)
{
    char* dpool = lbl_8011CDE0;
    char* rpool = lbl_801131C0;
    u32 serial[2];
    u8* img;
    u8* p;
    u32 sum;
    int i;

    serial[0] = 0;
    serial[1] = 0;

    if (lbl_803449EC == 0) {
        lbl_803449EC = (u32) loadOpeningBanner();
        if (lbl_803449EC == 0) {
            OSPanic(rpool + 608, 1019, rpool + 620);
        }
        lbl_803449E8 = 0;
        lbl_803449E4 = 0;
        TEXGetPalette(0, rpool + 672);
        TEXGetPalette(0, rpool + 696);
    }

retry:
    for (i = 0; i < 2; i++) {
        pageSaveCacheOut();
        drawMemCardMessage(dpool + 1432, 0, 0, 0);    /* "Accessing..." */
        pageSaveCacheIn();
    }

    cardMount(0, lbl_803449FC, cardRemovedCallback);
    cardWaitResult();
    if (cardGetResult() != 0) {
        return 0;
    }
    CARDGetSerialNo(0, serial);

    if ((lbl_80344A20 != 0 || lbl_80344A24 != 0) &&
        ((u32) lbl_80344A20 != serial[0] || (u32) lbl_80344A24 != serial[1])) {
        if (saveMenuPrompt(rpool + 1516, lbl_80343C6C, 2) == 1) {
            lbl_80344A24 = 0;
            lbl_80344A18 = -1;
            lbl_80344A14 = -1;
            lbl_80344A20 = 0;
        }
        return 0;
    }

    /* stamp record header + checksum */
    img = buildSaveImage(lbl_8011D550, rpool + 716, (int) lbl_803449EC,
                         (int) lbl_803449E4, 2, 0, (char*) lbl_803449E8);
    *(u32*) (lbl_80343C74 + 4) = 0x4F4B4159;           /* "OKAY" */
    *(u32*) lbl_80343C74 = 0;
    sum = 0;
    for (p = lbl_80343C74; p < lbl_80343C74 + 0x10000 - 23992; p++) {
        sum += *p;
    }
    *(u32*) lbl_80343C74 = sum;
    memcpy(img, lbl_80343C74, 5193 * 8);

    for (i = 0; i < 2; i++) {
        pageSaveCacheOut();
        drawMemCardMessage(dpool + 1712, 0, 0, 0);
        pageSaveCacheIn();
    }

    cardWriteFile(0, lbl_8026A2C4, (void*) lbl_803449F8);
    cardWaitResult();
    if (cardGetResult() != 0) {
        if (saveMenuPrompt(rpool + 1516, lbl_80343C6C, 2) == 1) {
            lbl_80344A24 = 0;
            lbl_80344A18 = -1;
            lbl_80344A14 = -1;
            lbl_80344A20 = 0;
            return 0;
        }
        goto retry;
    }

    lbl_80344A24 = serial[1];
    lbl_80344A20 = serial[0];
    cardReadFile(0, lbl_80344A04, (void*) lbl_803449F8);
    cardWaitResult();
    cardUnmount(0);
    cardWaitResult();
    return 1;
}

/*
 * vmu_exists - mount the card, load its directory, and search for a file
 * named `name`.  Fills *fileNoOut with the matching card file number and
 * returns non-zero when found.
 */
u8 vmu_exists(s32 chan, const char* name, s32* fileNoOut)
{
    u8* buf;
    u8* workArea;
    u8 found = 0;
    u8 pad[8];

    sysSetFlags(64);
    workArea = (u8*) (GetHiMemCacheTop() - 0x310000);
    dcsAramWriteTop(workArea, 0x310000);
    lbl_80344A0C = OSCreateHeap(workArea, workArea + 0x310000);
    lbl_80344A08 = OSSetCurrentHeap(lbl_80344A0C);
    lbl_80344A00 = (u8*) OSAllocFromHeap(__OSCurrHeap, 8192);
    lbl_803449FC = (u8*) OSAllocFromHeap(__OSCurrHeap, 0x10000 - 24576);
    cardStart(lbl_80344A00 + 8192, 8192, 18);
    cardWaitResult();
    cardMount(0, lbl_803449FC, cardRemovedCallback);
    cardWaitResult();
    if (cardGetResult() != 0) {
        return 0;
    }

    buf = (u8*) OSAllocFromHeap(__OSCurrHeap, 0x2D44C0);
    cardLoadFile(chan, buf);
    cardWaitResult();
    if (cardGetResult() == 0) {
        s32 count = cardLock();
        s32 off = count * 23360;

        while (count != 0) {
            s32 fileNo = *(s32*) (buf + off - 256);
            char stat[64];

            off -= 23360;
            count--;
            CARDGetStatus(chan, fileNo, stat);
            if (strcmp(stat, name) == 0) {
                *fileNoOut = *(s32*) (buf + off + 23104);
                found = 1;
                break;
            }
        }
        cardUnlock();
        cardWaitResult();
    }

    cardUnmount(0);
    cardWaitResult();
    cardExit();
    cardWaitResult();
    OSSetCurrentHeap(lbl_80344A08);
    OSDestroyHeap(lbl_80344A0C);
    dcsAramReadTop((void*)(GetHiMemCacheTop() - 0x310000), 0x310000);
    sysClearFlags(64);
    return found;
}

/*
 * cardRemovedCallback - idle callback while the card is out: page the save
 * cache out of ARAM, poll for a card while showing the "insert" dialog,
 * then page it back in.  (Passed to cardMount as the detach handler.)
 */
void cardRemovedCallback(int arg)
{
    if (arg == 0) {
        pageSaveCacheOut();
        while (CARDProbe(0) == 0) {
            drawMemCardMessage(lbl_8011D2A0, 0, 0, 0);
        }
        pageSaveCacheIn();
    }
}

/*
 * buildSaveImage - serialize the GCI save image: file name + comment, then
 * the banner (uncompressed/CI) and up to 8 animated icon frames pulled from
 * the texture bank.  Returns the write cursor past the image.
 * (Reconstruction of the 0x3DC serializer; exact texture-format branches
 * are captured but not byte-verified.)
 */
u8* buildSaveImage(const char* name, void* hdr, int bannerTex, int iconTex,
                   int fmtA, int fmtB, const char* comment)
{
    u8* base = lbl_8025EE80 + 0x10000;
    u8* out;
    s32 blockSize;
    s32 total;
    int i;
    int bit;
    u16* fmtWord = (u16*) (base - 19336);
    u16* animWord = (u16*) (base - 19334);

    memset(base - 19388, 0, 108);
    total = lbl_803449F0;
    blockSize = cardGetTotalBytes();
    total = (total + cardGetTotalBytes() - 1) / blockSize;
    *(s32*) (base - 19356) = total * cardGetTotalBytes();

    if (lbl_803449F8 == 0) {
        lbl_803449F8 = (u32) OSAllocFromHeap(__OSCurrHeap,
                                             *(u32*) (base - 19356));
    }
    memset((void*) lbl_803449F8, 0, *(u32*) (base - 19356));

    strncpy((char*) (base - 19388), name, 32);
    out = (u8*) lbl_803449F8;
    *(s32*) (base - 19332) = 0;
    strncpy((char*) out, comment + 6176, 32);
    strncpy((char*) (out + 32), comment, 32);
    *(s32*) (base - 19340) = (s32) ((u8*) out - (u8*) lbl_803449F8) + 64;

    /* banner */
    if (bannerTex == 0) {
        *(u8*) (base - 19342) = 2;
        memcpy(out + 64, (u8*) hdr + 32, 6144);
        out += 6208;
    } else {
        void** tex = (void**) TEXGet(bannerTex, 0);
        s32 fmt = *(s32*) ((u8*) tex[0] + 4);

        if (fmt == 9) {
            *(u8*) (base - 19342) = 1;
        } else if (fmt == 5) {
            *(u8*) (base - 19342) = 2;
        } else {
            OSPanic(lbl_801131C0 + 608, 856, lbl_801131C0 + 920);
        }
        if ((*(u8*) (base - 19342) & 3) == 2) {
            memcpy(out + 64, (u8*) tex[0] + 8, 6144);
            out += 6208;
        } else {
            memcpy(out + 64, (u8*) tex[0] + 8, 3072);
            memcpy(out + 3136, (u8*) tex[1] + 8, 512);
            out += 3648;
        }
    }

    /* icon animation frames */
    if (iconTex != 0) {
        int lastFrame = 0;

        *fmtWord = 0;
        for (i = 0, bit = 0; i < 8 && i < *(s32*) ((u8*) iconTex + 4);
             i++, bit += 2) {
            void** tex = (void**) TEXGet(iconTex, i);
            s32 fmt = *(s32*) ((u8*) tex[0] + 4);
            s32 code;

            if (fmt == 5) {
                code = 2;
            } else if (fmt == 9) {
                code = 1;
            } else {
                OSPanic(lbl_801131C0 + 608, 901, lbl_801131C0 + 956);
                code = 0;
            }
            *fmtWord = (u16) ((*fmtWord & ~(3 << bit)) | (code << bit));
            *animWord = (u16) ((*animWord & ~(3 << bit)) | (fmtA << bit));
        }
        for (; i < 8; i++, bit += 2) {
            *animWord = (u16) (*animWord & ~(3 << bit));
        }
        *(u8*) (base - 19342) |= fmtB;

        for (i = 0, bit = 0; i < 8 && i < *(s32*) ((u8*) iconTex + 4);
             i++, bit += 2) {
            void** tex = (void**) TEXGet(iconTex, i);
            s32 code = (*fmtWord >> bit) & 3;

            if (code == 2) {
                memcpy(out, (u8*) tex[0] + 8, 2048);
                out += 2048;
            } else if (code == 1) {
                memcpy(out, (u8*) tex[0] + 8, 1024);
                lastFrame = i;
                out += 1024;
            }
        }
        if (lastFrame >= 0) {
            void** tex = (void**) TEXGet(iconTex, i);

            memcpy(out, (u8*) tex[1] + 8, 512);
            out += 512;
        }
    }
    return out;
}

/* loadOpeningBanner - load /opening.bnr (0x1960-byte BNR1) via the PS2 shim */
u8* loadOpeningBanner(void)
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

/*
 * beginSaveTransaction - page the staging block out of ARAM and build the
 * save work heap inside it.
 * PARKED 1-slot residual: the D38F8-dst addis schedules one slot earlier.
 */
void beginSaveTransaction(void)
{
    u8* buf;
    u32 size = 0x310000;
    u8* lo;
    u8 pad[8]; /* unused, matches original frame */

    sysSetFlags(64);
    buf = (u8*) GetHiMemCacheTop();
    dcsAramWriteTop(buf - 0x310000, 0x310000);
    lo = buf - 0x310000;
    lbl_80344A0C = OSCreateHeap(lo, lo + size);
    lbl_80344A08 = OSSetCurrentHeap(lbl_80344A0C);
    lbl_80344A00 = (u8*) OSAllocFromHeap(__OSCurrHeap, 8192);
    lbl_803449FC = (u8*) OSAllocFromHeap(__OSCurrHeap, 0xA000);
    cardStart(lbl_80344A00 + 8192, 8192, 18);
    cardWaitResult();
}

/* restoreSaveCache - page the save cache back from ARAM into main memory */
void restoreSaveCache(u32 size)
{
    u32 top = GetHiMemCacheTop();
    u8* transferSize = (u8*)size;

    dcsAramReadTop((void*)(top - (u32)transferSize), (u32)transferSize);
}

/* memCardErrorPrompt - one modal card-error menu; resets state on cancel */
s32 memCardErrorPrompt(const char* msg)
{
    s32 r = saveMenuPrompt(msg, lbl_80343C6C, 2);

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

/*
 * saveMenuPrompt - modal option menu: pages the save cache out of ARAM
 * every 30 frames so the service pump (movie music etc.) keeps running,
 * and returns the chosen option index.
 */
s32 saveMenuPrompt(const char* msg, char** options, s32 count)
{
    s32 sel = 0;
    s32 timer = 30;
    u8 had;

    dcsAramWrite((void*)(GetHiMemCacheTop() - 0x310000), 0x9E0000, 0x310000);
    dcsAramRead(0xCF0000, (void*)(GetHiMemCacheTop() - 0x310000), 0x310000);
    had = sysTestFlags(64);
    if (had) {
        sysClearFlags(64);
    }
    for (;;) {
        s32 stick;

        drawMemCardMessage(msg, options, count, sel);
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
            dcsAramWrite((void*)(GetHiMemCacheTop() - 0x310000), 0xCF0000, 0x310000);
            dcsAramRead(0x9E0000, (void*)(GetHiMemCacheTop() - 0x310000), 0x310000);
            fn_80067B0C(-1);
            dcsAramWrite((void*)(GetHiMemCacheTop() - 0x310000), 0x9E0000, 0x310000);
            dcsAramRead(0xCF0000, (void*)(GetHiMemCacheTop() - 0x310000), 0x310000);
        }
        if (padButtonReleased(-1, 256) != 0) {
            break;
        }
    }
    dcsAramWrite((void*)(GetHiMemCacheTop() - 0x310000), 0xCF0000, 0x310000);
    dcsAramRead(0x9E0000, (void*)(GetHiMemCacheTop() - 0x310000), 0x310000);
    if (had) {
        sysSetFlags(64);
    }
    return sel;
}

/* pageSaveCacheIn - swap the save cache back in (0xCF0000 -> main -> 0x9E0000) */
void pageSaveCacheIn(void)
{
    dcsAramWrite((void*)(GetHiMemCacheTop() - 0x310000), 0xCF0000, 0x310000);
    dcsAramRead(0x9E0000, (void*)(GetHiMemCacheTop() - 0x310000), 0x310000);
}

/* pageSaveCacheOut - swap the save cache out (0x9E0000 -> main -> 0xCF0000) */
void pageSaveCacheOut(void)
{
    dcsAramWrite((void*)(GetHiMemCacheTop() - 0x310000), 0x9E0000, 0x310000);
    dcsAramRead(0xCF0000, (void*)(GetHiMemCacheTop() - 0x310000), 0x310000);
}

/*
 * drawMemCardMessage - render one frame of a modal memory-card dialog: a
 * multi-line message plus an optional column of `count1` selectable options
 * (item `count2` highlighted).  Uses the game's window / message-blit / text
 * subsystems.  (Reconstruction of the 0x330 UI renderer.)
 */
void drawMemCardMessage(const char* msg, char** options, s32 count1, s32 count2)
{
    void* win;
    void* savedBusy;
    void* extra = 0;
    void* blit = 0;
    void* quad = 0;
    char lines[64];
    int nLines;
    int i;
    int widest;
    int lineH;
    int boxW;
    int boxH;
    int x;
    int y;

    lbl_80344A5C = 1;
    lbl_80344A5D = 1;
    win = gWinGlobals;
    savedBusy = (void*) lbl_80344568;
    sysResetService();
    vibrators_off();
    lbl_80344568 = 1;
    lbl_80344A30++;
    msgUpdate();
    fn_800B6B80();
    fn_800B5D20(lbl_80344A30 - 1);

    if (gWinGlobals != 0 && *(void**) ((u8*) win + 48) != 0) {
        extra = (u8*) *(void**) ((u8*) win + 48) + 4;
    }
    if (extra != 0 && *(s32*) ((u8*) extra + 12) == 0 &&
        strcmp((char*) lbl_802A5D1C, lbl_80347368) == 0) {
        blit = MBNewBlit((void*) lbl_80343CCC, 0, 0);
    } else {
        quad = MBNewTempQuad();
    }

    if (*(s32*) ((u8*) lbl_802A4AA4 + 24) != 0) {
        lbl_80344A54 = 6;
        lbl_80344A58 = lbl_80347370;
    } else {
        lbl_80344A54 = 0;
        lbl_80344A58 = lbl_80347374;
    }

    nLines = FixMLineText(msg, (char*) gTextWorkBuf, lines);
    widest = 0;
    for (i = 0; i < nLines; i++) {
        int w = DrawNormalText(((char**) lines)[i], lbl_80344A54, lbl_80344A58);

        if (w > widest) {
            widest = w;
        }
    }
    lineH = FontHeight(lbl_80344A54, lbl_80344A58) + 3;
    boxH = (lineH + 6) * (TextMLines(msg) + count1) + 60;
    boxW = widest + 96;
    if (boxW < 256) {
        boxW = 256;
    } else if (boxW > 512) {
        boxW = 512;
    }
    x = 256 - (boxW / 2);
    y = 160 - (boxH / 2);

    if (blit != 0) {
        mbBlitProject(blit, boxW);
        mbBlitCalcWidth(blit, x, y, lbl_80347378);
    } else {
        mbBlitProject(quad, boxW);
        mbBlitCalcWidth(quad, x, y, lbl_80347378);
        fn_800B2940(quad, -1);
    }

    strcpy((char*) gTextFormatBuf, msg);
    y += 32;
    for (i = 0; i < nLines; i++) {
        DrawTextSub(-256, ((char**) lines)[i], lbl_80344A54, lbl_80344A58,
                    0x16003 /* style */, lbl_80347378, 0);
        y += lineH;
    }
    y += 3;
    for (i = 0; i < count1; i++) {
        int color = (i == count2) ? 0xFF : 0x16003;

        DrawTextSub(-256, options[i], lbl_80344A54, lbl_80344A58, color,
                    lbl_80347378, 0);
        y += lineH;
    }

    MBEndFrame();
    if (blit != 0) {
        MBRemoveBlit(blit);
    }
    fn_800B5CCC(lbl_80344A30 - 1);
    lbl_80344568 = (s32) savedBusy;
    lbl_80344A30--;
}
