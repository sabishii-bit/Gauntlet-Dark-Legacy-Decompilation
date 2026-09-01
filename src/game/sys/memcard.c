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
s32 cardWaitResult(void);
s32 cardGetResult(void);
void cardMount(s32 chan, void* workArea, void* detachCb);
void cardUnmount(s32 chan);
s32 cardLoadFile(s32 chan, void* buf);
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
void serve_busy(int flags);       /* service pump (movie/audio) while busy */

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
void DrawTextSub(int x, int y, int color, f32 s1, int style, f32 s2, const char* text);
void vibrators_off(void);
void MBHideMarkedMessages(void);
void MBLockMessages(int a);
void MBUnlockMessages(int a);
void MBBlitSetColor(void* blit, int a);

/* ---- data: in-memory directory + options ----------------------------- */
typedef struct GameOpts {
    u32 data[8];                   /* 32-byte options block; [2] = stereo   */
} GameOpts;

typedef struct SaveFileBlock {
    u32 w[1293];                   /* one 5172-byte per-file save data block */
} SaveFileBlock;

#ifndef offsetof
#define offsetof(type, memb) ((u32) & ((type*)0)->memb)
#endif

/* one 16-byte directory entry (lbl_80274578 table row) */
typedef struct DirEntry {
    u32 size;                      /* +0 */
    u32 time;                      /* +4 */
    char name[8];                  /* +8 */
} DirEntry;

/* one dir-info table block: 8 entries + trailing count/flag word (stride
 * 132 = 8*16+4, per the lbl_80274578 comment); init_all_dir_info's
 * post-loop write to base+128 is this trailing word. */
typedef struct DirTable {
    DirEntry entries[8];           /* +0 (128 bytes) */
    u32 count;                     /* +128 */
} DirTable;

/* the staged save record at lbl_80343C74 (opts@+8, dir@+0xA1C8 per the
 * existing extern comment; 0xA1C8 == 41416 == offsetof(files)+8*5172, and
 * 41416+128 == 41544 == sizeof(SaveBlob), confirming the layout). */
typedef struct SaveRecord {
    u32 checksum;                  /* +0: byte-sum checksum */
    u32 okay;                      /* +4: "OKAY" magic 0x4F4B4159 */
    GameOpts opts;                 /* +8 (32 bytes) */
    SaveFileBlock files[8];        /* +40 (8*5172 = 41376 bytes) */
    u8 dir[128];                   /* +41416 */
} SaveRecord;

#define MEMCARD_STRING_POOL                                                    \
    "Finish save cache transaction......\n\0\0\0\0"                           \
    "Beginning save cache transaction......\n\0"                               \
    "BASLUS-20047GameOpts\0\0\0\0"                                        \
    "BASLUS-20047DirInfo\0"                                                 \
    "BASLUS-20047save%04d"

extern u8 lbl_80274578[];          /* dir-info tables: stride 132 (8x16 +4) */
extern u8 lbl_8025EE80[];          /* VMU/dir working buffer (dir @+0x156F8)*/
extern u8 optglobals[0x40];        /* options TU globals before prefs block */
extern GameOpts optionsAudioAndPrefs30;
#define gameOpts optionsAudioAndPrefs30
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
extern s32 prefs_loaded;           /* prefs_loaded flag                     */
extern s32 lbl_803449D4;
extern u8 lbl_803449D8;
extern s32 lbl_803449DC;
extern s32 lbl_803449E0;
extern u32 lbl_803449E4;           /* banner palette handle                 */
extern u32 lbl_803449E8;           /* icon palette handle                   */
extern u32 lbl_803449EC;           /* opening-banner texture handle         */
extern s32 vmu_update_counter;           /* per-frame service counter (wrap 60)   */
extern u32 lbl_803449F8;           /* built save-image size                 */
extern u8* lbl_803449FC;           /* file buffer                           */
extern u8* lbl_80344A00;           /* card workArea                         */
extern u8* lbl_80344A04;
extern void* lbl_80344A08;         /* previous heap                         */
extern void* lbl_80344A0C;         /* save heap                             */
extern s32 lbl_80344A10[2];        /* per-(port+slot) cached free bytes     */
extern s32 lbl_80344A14;           /* per-(port+slot) card-present flag      */
extern s32 lbl_80344A18;           /* per-(port+slot) card state (3=ready)  */
extern s32 lbl_80344A20;           /* card serial word 0                    */
extern s32 lbl_80344A24;           /* card serial word 1                    */
extern u32 lbl_80343C78;           /* directory refresh flags               */

/* misc timing / UI globals */
extern u32 sSeconds;               /* frame-driven second counter           */
extern float lbl_803472F0;         /* probe time-out threshold (seconds)    */
extern double lbl_803472F8;        /* int->double magic bias (0x43300000..) */
extern void* gWinGlobals;
extern char gTextWorkBuf[0x800];
extern char gTextFormatBuf[0x404];
extern s32 gGameBusy;
extern s32 gModalRenderDepth;
extern u8 gDiskErrorShown;
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
int beginSaveCacheTransaction(void);
void restoreSaveCache(u32 size);
void beginSaveTransaction(void);
int loadGauntletSave(void);
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
 * PARKED 79/79 real 10: base-accumulator r0 web + slwi r5 in-place rotation.
 */
typedef struct TexAnimHdr {
    u32 type;      /* +0 */
    u32 numFrames; /* +4 */
} TexAnimHdr;

#pragma opt_propagation off
int add_vmu_file(int a, int b, int c, const char* name, u32 v0, u32 v1)
{
    u8* rec;
    u8* row;
    int result;
    u8 unused[16]; /* matches original frame */

    rec = lbl_80274578;
    rec += a * 132;
    row = rec + b * 132;
    rec = row + c * 16;
    strncpy((char*) (rec + 8), name, 8);
    *(u32*) rec = v0;
    *(u32*) (rec + offsetof(DirEntry, time)) = v1;
    if ((u8) beginSaveCacheTransaction() == 1) {
        result = 1;
    } else {
        result = 0;
    }
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
    union {
        char* ptr;
    } saveName;
    s32 x;
    s32 result;
    s32* pPresent;
    char name[64];
    s32 fileNo;
    s32 r;

    saveName.ptr = lbl_8011D550;
    result = 0;
    x = 0;
    pPresent = &lbl_80344A14;

    if (lbl_80344A18 == 3 && *pPresent == 1) {
        u8 ok;

        fileNo = -1;
        if (saveMount(0, 0, 0) <= 0) {
            r = 0;
            goto check;
        }
        if (x < 0 || x > 1) {
            ok = 0;
        } else {
            ok = 1;
        }
        if (!ok) {
            r = 0;
            goto check;
        }
        getSaveFileName(name, fileNo);
        r = vmu_exists(0, saveName.ptr, &fileNo);
    check:
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
#pragma opt_common_subs off
int get_vmu_directory(int a, int b)
{
    u8* base = lbl_8025EE80;
    int success = 1;
    int bit = b + a * 4;
    u8 pad[16]; /* unused, matches original frame */

    if (lbl_80343C78 & (1 << bit)) {
        if ((u8) beginSaveCacheTransaction() != 1) {
            success = 0;
        }
        if (success) {
            u8* dst = base + 0x10000;

            dst += a * 132;
            dst += b * 132;
            dst += 22264;
            memcpy(dst, (u8*) lbl_80343C74 + 41416, 128);
        }
        cardExit();
        cardWaitResult();
        OSSetCurrentHeap(lbl_80344A08);
        OSDestroyHeap(lbl_80344A0C);
        restoreSaveCache(0x310000);
        sysClearFlags(64);
        bulletproof_printf(lbl_801131C0);
        lbl_803449EC = 0;
        if (success) {
            lbl_80343C78 &= ~(1 << bit);
        }
    } else {
        u8* dst = base + 0x10000;

        dst += a * 132;
        dst += b * 132;
        dst += 22264;
        memcpy(dst, (u8*) lbl_80343C74 + 41416, 128);
    }
    if (success == 0) {
        return -1;
    }
    {
        s32 off;
        u8* dir = base + 0x10000;
        char* nm;
        int i;

        dir += a * 132;
        dir += b * 132;
        dir += 22264;

        for (i = 0, off = 0; i < 8; i++, off += 16) {
            u8* e = dir + off;

            if (*(s32*) e == -1 || (s8) e[8] == 0) {
                strcpy((char*) (e + 8), lbl_803472D8);
            } else {
                nm = (char*) (e + 8);

                if (strcmp(nm, lbl_803472D8) == 0 ||
                    strcmp(nm, lbl_803472E0) == 0) {
                    strcpy(nm, lbl_803472E8);
                }
            }
        }
        return i;
    }
}
#pragma opt_common_subs reset

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

    if (state != -1) {
        present = *p;
        if (present == 1) {
            result = (state == 3) ? 1 : -1;
        } else if (result == 0) {
            if (present == 1) {
                result = -2;
            } else {
                result = -1;
            }
        }
    }
    return result;
}

/* serve_memcard - per-frame service tick (60-frame wrap counter) */
void serve_memcard(void)
{
    vmu_update_counter++;
    if (vmu_update_counter > 60) {
        vmu_update_counter = 0;
    }
}

/*
 * saveLoad - mount, run the load engine, then copy one file's data block
 * out of the staged record into the caller's buffer.
 */
int saveLoad(int port, int slot, int fileNo, void* dst)
{
    u8 unused_hi[12];
    char name[64];
    u8 unused[84];
    u8 ret;
    u8 ok;

    if (saveMount(port, slot, 0) <= 0) {
        return 0;
    }
    if (port < 0 || port > 1) {
        ok = 0;
    } else {
        ok = 1;
    }
    if (!ok) {
        return 0;
    }
    /* inlined getSaveFileName(name, fileNo) */
    if (fileNo == -2) {
        strcpy(name, MEMCARD_STRING_POOL + 80);
    } else if (fileNo == -1) {
        strcpy(name, MEMCARD_STRING_POOL + 104);
    } else {
        sprintf(name, MEMCARD_STRING_POOL + 124, fileNo + 1);
    }
    lbl_803449EC = 0;
    lbl_803449F8 = 0;
    bulletproof_printf(MEMCARD_STRING_POOL + 40);
    while (FileSystemReading() != 0) {
        serve_busy(-1);
    }
    beginSaveTransaction();
    lbl_80344A04 = (u8*) OSAllocFromHeap(__OSCurrHeap, 0x2D44C0);
    if ((u8) loadGauntletSave()) {
        *(SaveFileBlock*) dst =
            ((SaveFileBlock*) ((u8*) lbl_80343C74 + 40))[fileNo];
        ret = 1;
    } else {
        ret = 0;
    }
    cardExit();
    cardWaitResult();
    OSSetCurrentHeap(lbl_80344A08);
    OSDestroyHeap(lbl_80344A0C);
    {
        register u8* aramTop;
        register u32 aramSize;

        aramTop = (u8*) GetHiMemCacheTop();
        dcsAramReadTop((void*)((u32)aramTop - 0x310000), (aramSize = 0x310000));
    }
    sysClearFlags(64);
    bulletproof_printf(MEMCARD_STRING_POOL);
    lbl_803449EC = 0;
    return ret;
}

/*
 * saveSave - mount, run the load engine (to refresh the directory), stage
 * options + the caller's player-data block + the dir table, then commit.
 */
int saveSave(int port, int slot, int fileNo, void* src)
{
    u8 unused_hi[16];
    char name[64];
    u8 unused_lo[16];

    if (saveMount(port, slot, 0) <= 0) {
        return 0;
    }
    /* inlined getSaveFileName(name, fileNo) (key built, result unused) */
    if (fileNo == -2) {
        strcpy(name, MEMCARD_STRING_POOL + 80);
    } else if (fileNo == -1) {
        strcpy(name, MEMCARD_STRING_POOL + 104);
    } else {
        sprintf(name, MEMCARD_STRING_POOL + 124, fileNo + 1);
    }
    lbl_803449EC = 0;
    lbl_803449F8 = 0;
    bulletproof_printf(MEMCARD_STRING_POOL + 40);
    while (FileSystemReading() != 0) {
        serve_busy(-1);
    }
    beginSaveTransaction();
    lbl_80344A04 = (u8*) OSAllocFromHeap(__OSCurrHeap, 0x2D44C0);
    loadGauntletSave();
    *(GameOpts*) ((u8*) lbl_80343C74 + offsetof(SaveRecord, opts)) = gameOpts;
    ((SaveFileBlock*) ((u8*) lbl_80343C74 + 40))[fileNo] =
        *(SaveFileBlock*) src;
    memcpy((u8*) lbl_80343C74 + 41416, lbl_80274578, 128);
    writeGauntletSave();
    lbl_80343C78 |= 0xFFFFFFFF;
    cardExit();
    cardWaitResult();
    OSSetCurrentHeap(lbl_80344A08);
    OSDestroyHeap(lbl_80344A0C);
    {
        register u8* aramTop;
        register u32 aramSize;

        aramTop = (u8*) GetHiMemCacheTop();
        dcsAramReadTop((void*)((u32)aramTop - 0x310000), (aramSize = 0x310000));
    }
    sysClearFlags(64);
    bulletproof_printf(MEMCARD_STRING_POOL);
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
    u8 unused[24]; /* matches original frame */

    p = (s32*)((u8*)lbl_80344A10 + port * 4 + slot * 4);
    if (*p >= 0) {
        return *p;
    }
    if (port < 0 || port > 1) {
        ok = 0;
    } else {
        ok = 1;
    }
    if (!ok) {
        return -1;
    }
    *p = cardGetFreeBytes();
    return *p;
}

/* check_prefs_loaded - load game options from the card once */
void check_prefs_loaded(void)
{
    char* st;
    GameOpts* opts = &gameOpts;
    u8 pad[16]; /* unused, matches original frame */

    if (saveMount(0, 0, 0) == 0) {
        return;
    }
    if (prefs_loaded != 0) {
        return;
    }
    prefs_loaded = 1;
    if ((u8) beginSaveCacheTransaction()) {
        *opts = *(GameOpts*) ((u8*) lbl_80343C74 + offsetof(SaveRecord, opts));
    }
    cardExit();
    cardWaitResult();
    OSSetCurrentHeap(lbl_80344A08);
    OSDestroyHeap(lbl_80344A0C);
    restoreSaveCache(0x310000);
    sysClearFlags(64);
    st = lbl_801131C0;
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
    do {
        u8* e = base + off;

        *(s32*) e = fill;
        *(s32*) (e + offsetof(DirEntry, time)) = fill;
        strcpy((char*) (e + 8), lbl_803472D8);
        i++;
        off += 16;
    } while (i < 8);
    *(s32*) (base + offsetof(DirTable, count)) = zero;
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
    int i;
    u8* base;
    u32 transferSize;
    u8 unused[144];

    i = 0;
    base = lbl_80274578 + port * 132 + slot * 132;
    do {
        u8* e = base + i * 16;

        *(s32*) e = -1;
        *(s32*) (e + offsetof(DirEntry, time)) = -1;
        strcpy((char*) (e + 8), lbl_803472D8);
        i++;
    } while (i < 8);
    if (saveMount(port, slot, 1) <= 0) {
        return 0;
    }
    lbl_803449EC = 0;
    lbl_803449F8 = 0;
    bulletproof_printf(lbl_801131E8);
    while (FileSystemReading() != 0) {
        serve_busy(-1);
    }
    beginSaveTransaction();
    lbl_80344A04 = (u8*) OSAllocFromHeap(__OSCurrHeap, 0x2D44C0);
    loadGauntletSave();
    *(GameOpts*) ((u8*) lbl_80343C74 + offsetof(SaveRecord, opts)) = gameOpts;
    memcpy((u8*) lbl_80343C74 + 41416, lbl_80274578, 128);
    memset((u8*) lbl_80343C74 + 40, 0, 0x10000 - 24160);
    writeGauntletSave();
    lbl_80343C78 |= 0xFFFFFFFF;
    cardExit();
    cardWaitResult();
    OSSetCurrentHeap(lbl_80344A08);
    OSDestroyHeap(lbl_80344A0C);
    base = (u8*) GetHiMemCacheTop();
    dcsAramReadTop((void*)((u32)base - 0x310000), (transferSize = 0x310000));
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
#pragma opt_common_subs off
s32 saveMount(s32 port, s32 slot, s32 doFormat)
{
    s32 memSize;
    s32 sectorSize;
    s32 portOff;
    s32 slotOff;
    s32* pPresent;
    s32* pState;
    s32 chan;
    char* pool = lbl_801131C0;
    u8* top;
    u32 aramSize;
    u32 transferSize;
    s32 probe;
    s32 r;
    u8 mounted = 0;
    u8 retry;
    u32 lo;
    u8 unused[20];
    s32 idx;
    s32* serial;

    portOff = port << 2;
    idx = slot + portOff;
    chan = idx;
    bulletproof_printf(pool + 148, port, slot);   /* "Entered SAVEMOUNT..." */
    if (idx > 1) {
        return 0;
    }
    slotOff = slot << 2;
    pState = (s32*) ((u8*) &lbl_80344A18 + portOff + slotOff);
    pPresent = (s32*) ((u8*) &lbl_80344A14 + portOff + slotOff);

    /* poll the slot until CARDProbeEx reports a stable status */
    do {
        retry = 0;
        probe = CARDProbeEx(chan, &memSize, &sectorSize);
        switch (probe) {
        case -128:
        case -3:
            *pState = -1;
            *pPresent = -1;
            break;
        case -1:
            retry = 1;
            break;
        case -2:
            *pState = 0;
            *pPresent = 0;
            /* fallthrough */
        case 0:
            *pState = 0;
            *pPresent = 0;
            break;
        }
        if (retry) {
            VIWaitForRetrace();
        }
    } while (retry);

    aramSize = 0x310000;
    sysSetFlags(64);
    top = (u8*) GetHiMemCacheTop();
    dcsAramWriteTop(top - 0x310000, aramSize);
    lbl_80344A0C = OSCreateHeap((void*) (lo = (u32) top - 0x310000), (void*) (lo + aramSize));
    lbl_80344A08 = OSSetCurrentHeap(lbl_80344A0C);
    lbl_80344A00 = (u8*) OSAllocFromHeap(__OSCurrHeap, 8192);
    lbl_803449FC = (u8*) OSAllocFromHeap(__OSCurrHeap, 0x10000 - 24576);
    cardStart(lbl_80344A00 + 8192, 8192, 18);
    cardWaitResult();
    cardMount(chan, lbl_803449FC, cardRemovedCallback);
    r = cardWaitResult();

    *(s32*) ((u8*) &lbl_80344A10 + portOff + slotOff) = -1;
    if (r == 0) {
        *pPresent = 1;
        *pState = 3;
        mounted = 1;
    } else {
        switch (r) {
        case -6:
            *pPresent = 0;
            *pState = 2;
            mounted = 1;
            break;
        case -13:
            *pPresent = 0;
            *pState = 1;
            break;
        case -128:
            *pPresent = 0;
            *pState = 2;
            break;
        case -2:
            *pPresent = 0;
            *pState = 0;
            break;
        case -3:
            *pState = -1;
            *pPresent = -1;
            break;
        case -1:
            break;
        case -5:
            *pPresent = 0;
            *pState = 2;
            break;
        }
        if (doFormat) {
            cardFormat(chan);
            if (cardWaitResult() == 0) {
                *pState = 3;
                *pPresent = 1;
            }
        }
    }

    if (mounted) {
        switch (CARDCheck(0)) {
        case -128:
        case -13:
        case -6:
        case -5:
        case -3:
        case -1:
            *pPresent = 0;
            *pState = 2;
            break;
        case -4:
            break;
        case -2:
            break;
        case 0:
            *pPresent = 1;
            *pState = 3;
            break;
        }
    }

    cardExit();
    cardWaitResult();
    OSSetCurrentHeap(lbl_80344A08);
    OSDestroyHeap(lbl_80344A0C);
    top = (u8*) GetHiMemCacheTop();
    dcsAramReadTop((void*)((u32)top - 0x310000), (transferSize = 0x310000));
    sysClearFlags(64);
    lbl_80343C78 |= 0xFFFFFFFF;

    if (*pState == 3 && *pPresent == 1) {
        bulletproof_printf(pool + 184);           /* "SAVEMOUNT RETURNING 1" */
        return 1;
    }
    bulletproof_printf(pool + 212);               /* "SAVEMOUNT RETURNING -1"*/
    {
        s32 serialSlotOff;
        s32 serialPortOff;
        s32* serialBase;
        serialPortOff = port << 3;
        serialSlotOff = slot << 3;
        serialSlotOff = serialPortOff + serialSlotOff;
        serialBase = &lbl_80344A20;
        serial = (s32*) ((u8*) serialBase + serialSlotOff);
    }
    serial[0] = serial[1] = 0;
    return -1;
}
#pragma opt_common_subs reset

/*
 * InitPreferences - one-time preferences load with a full save-cache
 * transaction (the transaction body is inlined rather than calling the
 * shared beginSaveCacheTransaction helper).
 * PARKED 78/78: only two flag-reset constants land in swapped registers.
 */
int InitPreferences(void)
{
    int ret = 0;
    register u8* aramTop;
    register u32 aramSize;
    u8 pad[24]; /* unused, matches original frame */

    if (prefs_loaded == 0) {
        prefs_loaded = 1;
        lbl_803449EC = 0;
        lbl_803449F8 = 0;
        bulletproof_printf(lbl_801131E8);
        while (FileSystemReading() != 0) {
            serve_busy(-1);
        }
        beginSaveTransaction();
        lbl_80344A04 = (u8*) OSAllocFromHeap(__OSCurrHeap, 0x2D44C0);
        if ((u8) loadGauntletSave()) {
            ret = 1;
            gameOpts = *(GameOpts*) ((u8*) lbl_80343C74 + offsetof(SaveRecord, opts));
        } else {
            ret = 0;
        }
        cardExit();
        cardWaitResult();
        OSSetCurrentHeap(lbl_80344A08);
        OSDestroyHeap(lbl_80344A0C);
        aramTop = (u8*)GetHiMemCacheTop();
        dcsAramReadTop(aramTop - 0x310000, aramSize = 0x310000);
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
int beginSaveCacheTransaction(void)
{
    u8* buf;
    u32 size;
    u8* lo;
    u8 pad[24]; /* unused, matches original frame */

    lbl_803449EC = 0;
    lbl_803449F8 = 0;
    bulletproof_printf(lbl_801131E8);
    while (FileSystemReading() != 0) {
        serve_busy(-1);
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
/* Retail expands this prompt per call site; each expansion has its own keep
 * flag (whole-function webs under opt_lifetimes off burn r14-r24 in decl
 * order; the last three spill to the frame). */
#define CARD_RETRY_PROMPT(msg_, keep)                                       \
    do {                                                                    \
        switch (saveMenuPrompt((msg_), lbl_80343C6C, 2)) {                  \
        case 0:                                                             \
            keep = 1;                                                       \
            break;                                                          \
        case 1:                                                             \
            keep = 0;                                                       \
            lbl_80344A24 = keep;                                            \
            lbl_80344A18 = -1;                                              \
            lbl_80344A14 = -1;                                              \
            lbl_80344A20 = keep;                                            \
            break;                                                          \
        }                                                                   \
        if ((u8)keep != 0) {                                                \
            goto retry;                                                     \
        }                                                                   \
        return 0;                                                           \
    } while (0)

/* Late-site variant: the zero flows through an existing multi-def variable
 * (retail reuses the scan slot), and keep lives on the stack. */
#define CARD_RETRY_PROMPT_Z(msg_, keep, zv)                                 \
    do {                                                                    \
        switch (saveMenuPrompt((msg_), lbl_80343C6C, 2)) {                  \
        case 0:                                                             \
            keep = 1;                                                       \
            break;                                                          \
        case 1:                                                             \
            lbl_80344A24 = zv;                                              \
            lbl_80344A18 = -1;                                              \
            keep = zv;                                                      \
            lbl_80344A14 = -1;                                              \
            lbl_80344A20 = zv;                                              \
            break;                                                          \
        }                                                                   \
        if ((u8)keep != 0) {                                                \
            goto retry;                                                     \
        }                                                                   \
        return 0;                                                           \
    } while (0)

typedef struct SaveBlob {
    u8 bytes[41544];
} SaveBlob;

typedef struct OptsBlob {
    u8 bytes[32];
} OptsBlob;

#pragma opt_lifetimes off
int loadGauntletSave(void)
{
    char* dpool = lbl_8011CDE0;
    char* rpool = lbl_801131C0;
    s32 result;
    s32 scan;
    s32 scratch;
    s32 sel;
    s32 sel2;
    s32 volatile bigSize;
    u8* volatile dirTab;
    char** volatile opts;
    void* volatile removedCb;
    u8 keepPair;
    u8 keepSerial;
    u8 keepSerial2;
    s32 keepTime;
    s32 keepProbe3;
    s32 keepProbe2;
    s32 keepSize;
    s32 keepMount5;
    s32 keepMount2;
    s32 keepMount3;
    s32 keepCheck3;
    s32 keepCheck128;
    s32 keepFmt3;
    s32 keepFmtDef;
    s32 fileNo;
    u8 volatile found;
    u8 volatile needUnmount;
    u8 volatile needCheck;
    char* checkMsg;
    u32 volatile neededPct;
    u32 serial[2];
    s32 memSize;
    s32 sectorSize;
    u8 stat[112];
    u8 _pad0[8];
    f32 timeout = lbl_803472F0;
    f32 startSec;

    bigSize = 0x10000;
    dirTab = lbl_80274578;
    opts = (char**)&optionsAudioAndPrefs30;
    removedCb = cardRemovedCallback;
    checkMsg = dpool + 776;

retry:
    found = 0;
    result = 0;
    needUnmount = 0;
    scratch = 0;
    needCheck = 0;
    for (scan = 0; scan < 2; scan++) {
        pageSaveCacheOut();
        drawMemCardMessage(dpool + 1712, 0, 0, 0);
        pageSaveCacheIn();
    }

    pbPulseTime();
    startSec = (f32)sSeconds;
    for (;;) {
        scan = CARDProbeEx(0, &memSize, &sectorSize);
        pbPulseTime();
        if ((f32)sSeconds - startSec > timeout) {
            CARD_RETRY_PROMPT(dpool + 1052, keepTime);
        }
        if (scan != -1) {
            break;
        }
    }

    switch (scan) {
    case -3:
        CARD_RETRY_PROMPT(dpool + 256, keepProbe3);
    case -2:
    case -128:
        CARD_RETRY_PROMPT(dpool + 1052, keepProbe2);
    case 0:
    default:
        break;
    }

    if (memSize != 8192) {
        CARD_RETRY_PROMPT(dpool + 44, keepSize);
    }

    cardMount(0, lbl_803449FC, removedCb);
    cardWaitResult();
    switch (cardGetResult()) {
    case -13:
        checkMsg = dpool + 824;
        scratch = 1;
        break;
    case -6:
    case 0:
        checkMsg = dpool + 948;
        needCheck = 1;
        needUnmount = 1;
        break;
    case -5:
    case -128:
        CARD_RETRY_PROMPT(rpool + 240, keepMount5);
    case -2:
        CARD_RETRY_PROMPT(rpool + 392, keepMount2);
    case -3:
        CARD_RETRY_PROMPT(dpool + 256, keepMount3);
    default:
        break;
    }

    if (needCheck) {
        switch (CARDCheck(0)) {
        case -6:
            scratch = 1;
            break;
        case -3:
            CARD_RETRY_PROMPT(dpool + 256, keepCheck3);
        case -128:
            CARD_RETRY_PROMPT(dpool + 1052, keepCheck128);
        case 0:
        default:
            break;
        }
    }

    scan = 0;
    if ((((u32)lbl_80344A20 ^ scan) | ((u32)lbl_80344A24 ^ scan)) != 0 &&
        (u8)scratch != 0) {
        CARD_RETRY_PROMPT_Z(dpool + 1516, keepPair, scan);
    }

    if ((u8)scratch != 0) {
        switch (saveMenuPrompt(checkMsg, (char**)(dpool + 1880), 3)) {
        case 0:
            sel = 0;
            break;
        case 1:
            sel = 1;
            break;
        case 2:
            sel = 2;
            lbl_80344A24 = 0;
            lbl_80344A18 = -1;
            lbl_80344A14 = -1;
            lbl_80344A20 = 0;
            break;
        }

        switch (sel) {
        case 1:
            goto retry;
        case 0:
            for (scratch = 0; scratch < 2; scratch++) {
                pageSaveCacheOut();
                drawMemCardMessage(dpool + 1796, 0, 0, 0);
                pageSaveCacheIn();
            }
            cardFormat(0);
            switch (cardWaitResult()) {
            case -3:
                CARD_RETRY_PROMPT(dpool + 256, keepFmt3);
            case 0:
                saveMenuPrompt(rpool + 548, 0, 0);
                lbl_80344A18 = 3;
                lbl_80344A14 = 1;
                break;
            default:
                CARD_RETRY_PROMPT(dpool + 1052, keepFmtDef);
            }
            break;
        case 2:
            if (needUnmount != 0) {
                cardUnmount(0);
                cardWaitResult();
            }
            lbl_80344A24 = 0;
            lbl_80344A18 = -1;
            lbl_80344A14 = -1;
            lbl_80344A20 = 0;
            return 0;
        }
    }

    cardLoadFile(0, lbl_80344A04);
    switch (cardWaitResult()) {
    case 0:
        break;
    default:
        goto scan_done;
    }
    {
        result = cardLock();
        while (scratch = result * 23360, result != 0) {
            scan = scratch - 256;
            scratch -= 23360;
            result--;
            CARDGetStatus(0, *(s32*)(lbl_80344A04 + scan), stat);
            if (strcmp((char*)stat, dpool + 1904) == 0) {
                found = 1;
                fileNo = *(s32*)(lbl_80344A04 + scan);
                break;
            }
        }
        cardUnlock();
        if (cardWaitResult() != 0) {
            saveMenuPrompt(rpool + 568, lbl_80343C68, 1);
            return 0;
        }
        if (found == 0) {
            result = 0;
        } else {
            s32 rdres;

            if (lbl_803449EC == 0) {
                lbl_803449EC = (u32)loadOpeningBanner();
                if (lbl_803449EC == 0) {
                    OSPanic(rpool + 608, 1019, rpool + 620);
                }
                lbl_803449E8 = 0;
                lbl_803449E4 = 0;
                TEXGetPalette((int)&lbl_803449E4, rpool + 672);
                TEXGetPalette((int)&lbl_803449E8, rpool + 696);
            }
            scratch = (s32)buildSaveImage(dpool + 1904, (void*)lbl_803449EC,
                                          (int)lbl_803449E4, (int)lbl_803449E8,
                                          2, 0, (const char*)(rpool + 716));
            cardReadFile(0, (void*)fileNo, (void*)lbl_803449F8);
            rdres = cardWaitResult();
            *(SaveBlob*)lbl_80343C74 = *(SaveBlob*)scratch;

            {
                u32 saved = *(u32*)lbl_80343C74;
                s32 n;
                u32 sum = 0;
                u8* q;
                u8* q2;

                *(u32*)lbl_80343C74 = sum;
                q = lbl_80343C74;
                q2 = lbl_80343C74;
                for (n = bigSize - 23992; n != 0; n--) {
                    sum += *q2++;
                }
                if (saved == sum && *(u32*)(q + offsetof(SaveRecord, okay)) == 0x4F4B4159) {
                } else {
                    saveMenuPrompt(rpool + 736, lbl_80343C68, 1);
                    switch (CARDDelete(0, dpool + 1904)) {
                    case -128:
                    case -10:
                    case -5:
                    case -4:
                    case -3:
                    case -1:
                        break;
                    default:
                        goto retry;
                    }
                    saveMenuPrompt(rpool + 832, lbl_80343C68, 1);
                    lbl_80344A24 = 0;
                    lbl_80344A18 = -1;
                    lbl_80344A14 = -1;
                    lbl_80344A20 = 0;
                    return 0;
                }
                if (rdres != 0) {
                    result = 0;
                } else {
                    lbl_80344A18 = 3;
                    lbl_80344A14 = 1;
                    result = 1;
                }
            }
        }
    }

scan_done:
    CARDGetSerialNo(0, serial);
    cardUnmount(0);
    cardWaitResult();
    scan = 0;
    if ((((u32)lbl_80344A20 ^ scan) | ((u32)lbl_80344A24 ^ scan)) != 0) {
        if ((((u32)lbl_80344A20 ^ serial[0]) |
             ((u32)lbl_80344A24 ^ serial[1])) != 0) {
            CARD_RETRY_PROMPT_Z(dpool + 1516, keepSerial, scan);
        }
    }

    if ((u8)result != 0) {
        scan = 0;
        neededPct = 0;
        scratch = 0;
        if ((((u32)lbl_80344A20 ^ scan) | ((u32)lbl_80344A24 ^ scan)) != 0 &&
            (((u32)lbl_80344A20 ^ serial[0]) |
             ((u32)lbl_80344A24 ^ serial[1])) != 0) {
            CARD_RETRY_PROMPT_Z(dpool + 404, keepSerial2, scan);
        }
    } else {
        scratch = cardGetTotalBytes();
        neededPct = (u32)((cardGetTotalBytes() + 0x10000 - 1401) / scratch);
        scratch = 1;
    }

    if ((u32)cardGetUsedPercent() < neededPct ||
        (u32)cardGetFreeFiles() < (u32)scratch) {
        switch (saveMenuPrompt(dpool + 600, (char**)(dpool + 1892), 3)) {
        case 0:
            sel2 = 0;
            break;
        case 1:
            sel2 = 1;
            break;
        case 2:
            sel2 = 2;
            lbl_80344A24 = 0;
            lbl_80344A18 = -1;
            lbl_80344A14 = -1;
            lbl_80344A20 = 0;
            break;
        }
        switch (sel2) {
        case 0:
            goto retry;
        case 1:
            OSResetSystem(1, 0x80000000, 1);
            break;
        case 2:
            return 0;
        }
    } else {
        if ((u8)result == 0) {
            saveMenuPrompt(dpool + 1344, lbl_80343C68, 1);
            *(OptsBlob*)lbl_80343C74 = *(OptsBlob*)opts;
            memcpy(lbl_80343C74 + 41416, dirTab, 128);
            memset(lbl_80343C74 + 40, 0, bigSize - 24160);
            if ((int)(u8)writeGauntletSave() == 1) {
                saveMenuPrompt(dpool + 1296, lbl_80343C68, 1);
                lbl_80344A24 = serial[1];
                result = 1;
                lbl_80344A18 = 3;
                lbl_80344A14 = 1;
                lbl_80344A20 = serial[0];
            } else {
                saveMenuPrompt(dpool + 1316, lbl_80343C68, 1);
                goto retry;
            }
        } else {
            lbl_80344A24 = serial[1];
            lbl_80344A18 = 3;
            lbl_80344A14 = 1;
            lbl_80344A20 = serial[0];
        }
    }
    return result;
}
#pragma opt_lifetimes reset

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
    u8 pad[8]; /* dead stack below serial, matches original frame */
    u8* img;
    u8* p;
    u8* q;
    u32 sum;
    int i;
    s32 zero;
    u8 retryFlag;
    u8 retryFlag2;
    u32 count;
    u32 big;
    u32 okay;

    serial[1] = 0;
    serial[0] = 0;

    if (lbl_803449EC == 0) {
        lbl_803449EC = (u32) loadOpeningBanner();
        if (lbl_803449EC == 0) {
            OSPanic(rpool + 608, 1019, rpool + 620);
        }
        lbl_803449E8 = 0;
        lbl_803449E4 = 0;
        TEXGetPalette((int)&lbl_803449E4, rpool + 672);
        TEXGetPalette((int)&lbl_803449E8, rpool + 696);
    }

    okay = 0x4F4B4159;
    big = 0x10000;
retry:
    for (i = 0; i < 2; i++) {
        pageSaveCacheOut();
        drawMemCardMessage(dpool + 1432, 0, 0, 0);    /* "Accessing..." */
        pageSaveCacheIn();
    }

    cardMount(0, lbl_803449FC, cardRemovedCallback);
    if (cardWaitResult() != 0) {
        goto mount_fail;
    }
    CARDGetSerialNo(0, serial);
    goto mount_cont;
mount_fail:
    return 0;
mount_cont:
    zero = 0;
    if (((((u32) lbl_80344A20) ^ zero) | (((u32) lbl_80344A24) ^ zero)) != 0 &&
        ((serial[0] ^ (u32) lbl_80344A20) | (serial[1] ^ (u32) lbl_80344A24)) !=
            0) {
        switch (saveMenuPrompt(dpool + 1516, lbl_80343C6C, 2)) {
        case 0:
            retryFlag = 1;
            break;
        case 1:
            lbl_80344A24 = zero;
            lbl_80344A18 = -1;
            retryFlag = zero;
            lbl_80344A14 = -1;
            lbl_80344A20 = zero;
            break;
        }
        if (retryFlag) {
            goto retry;
        }
        return 0;
    }

    /* stamp record header + checksum */
    img = buildSaveImage(dpool + 1904, (void*) lbl_803449EC,
                         (int) lbl_803449E4, (int) lbl_803449E8, 2, 0,
                         (const char*) (rpool + 716));
    count = big - 23992;
    sum = 0;
    *(u32*) (lbl_80343C74 + offsetof(SaveRecord, okay)) = okay; /* "OKAY" */
    *(u32*) lbl_80343C74 = sum;
    p = lbl_80343C74;
    q = p;
    for (; count != 0; count--) {
        sum += *q++;
    }
    *(u32*) p = sum;
    *(SaveBlob*) img = *(SaveBlob*) lbl_80343C74;

    for (i = 0; i < 2; i++) {
        pageSaveCacheOut();
        drawMemCardMessage(dpool + 1712, 0, 0, 0);
        pageSaveCacheIn();
    }

    cardWriteFile(0, lbl_8026A2C4, (void*) lbl_803449F8);
    if (cardWaitResult() != 0) {
        switch (saveMenuPrompt(dpool + 1516, lbl_80343C6C, 2)) {
        case 0:
            retryFlag2 = 1;
            break;
        case 1:
            retryFlag2 = 0;
            lbl_80344A24 = retryFlag2;
            lbl_80344A18 = -1;
            lbl_80344A14 = -1;
            lbl_80344A20 = retryFlag2;
            break;
        }
        if (retryFlag2) {
            goto retry;
        }
        return 0;
    }

    lbl_80344A24 = serial[1];
    lbl_80344A20 = serial[0];
    cardLoadFile(0, lbl_80344A04);
    cardWaitResult();
    cardUnmount(0);
    cardWaitResult();
    return 1;
}

/* one cardLock()-scanned directory entry: fileNo lives at +23104 within a
 * 23360-byte stride.  Verified two independent ways: loadGauntletSave and
 * this function both hardcode the same 23360/256 pair, and algebraically
 * `off - 256 == (off - 23360) + 23104` - i.e. the pre-decrement "off-256"
 * read below is the SAME address as the post-decrement "entry+23104" read,
 * just computed one iteration early. */
typedef struct CardDirEntry {
    u8 unknown[23104];
    s32 fileNo;                       /* +23104 */
    u8 unknown2[23360 - 23104 - 4];   /* +23108: trailing bytes unresolved */
} CardDirEntry;

/*
 * vmu_exists - mount the card, load its directory, and search for a file
 * named `name`.  Fills *fileNoOut with the matching card file number and
 * returns non-zero when found.
 */
u8 vmu_exists(s32 chan, const char* name, s32* fileNoOut)
{
    u8* top = (u8*) GetHiMemCacheTop();
    u32 aramSize;
    s32 dirSize = 0x2D44C0;
    u32 transferSize;
    u8* buf;
    u8* lo;
    s32 found;

    found = 0;
    aramSize = 0x310000;

    sysSetFlags(64);
    top = (u8*) GetHiMemCacheTop();
    dcsAramWriteTop(top - 0x310000, aramSize);
    lo = top - 0x310000;
    lbl_80344A0C = OSCreateHeap(lo, lo + aramSize);
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

    buf = (u8*) OSAllocFromHeap(__OSCurrHeap, dirSize);
    cardLoadFile(chan, buf);
    if (cardWaitResult() == 0) {
        s32 count;
        s32 off;

        count = cardLock();
        goto initLoop;
loopBody:
        {
            s32 fileNo = *(s32*) (buf + off -
                                  (sizeof(CardDirEntry) - offsetof(CardDirEntry, fileNo)));
            volatile u8 _pad0[12];
            char stat[108];
            volatile u8 _pad1[12];

            off -= 23360;
            count--;
            CARDGetStatus(chan, fileNo, stat);
            if (strcmp(stat, name) == 0) {
                u8* entry = buf + off;

                *fileNoOut = *(s32*) (entry + offsetof(CardDirEntry, fileNo));
                found = 1;
                goto loopEnd;
            }
        }
        goto loopTest;
initLoop:
        off = count * 23360;
loopTest:
        if (count != 0) {
            goto loopBody;
        }
loopEnd:
        cardUnlock();
        cardWaitResult();
    }

    cardUnmount(0);
    cardWaitResult();
    cardExit();
    cardWaitResult();
    OSSetCurrentHeap(lbl_80344A08);
    OSDestroyHeap(lbl_80344A0C);
    top = (u8*) GetHiMemCacheTop();
    dcsAramReadTop((void*)((u32)top - 0x310000), (transferSize = 0x310000));
    sysClearFlags(64);
    return (u8)found;
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
 * buildSaveImage's scratch header, assembled at pool+0x10000-19388 (108
 * bytes total, matching the whole-struct `memset(..., 0, 108)` at the top
 * of the function).  Field offsets verified against the target's
 * -19388/-19356/-19342/-19340/-19336 literal displacements; the pad
 * regions between them are genuinely unresolved.
 */
typedef struct SaveImageHeader {
    char name[32];      /* +0:  save name */
    s32 imageSize;       /* +32: total built image byte size */
    u8 pad1[10];          /* +36: unresolved */
    u8 bannerFmt;          /* +46: banner CI/RGB5A3 format (1=CI,2=RGB5A3); fmtB ORed in */
    u8 pad2;                /* +47: unresolved */
    s32 dataOffset;         /* +48: cursor past name+comment (64) */
    u16 iconFmt;             /* +52: 2 bits/frame icon texture format */
    u16 iconSpeed;            /* +54: 2 bits/frame icon animation speed */
    s32 reserved;              /* +56: zeroed, unresolved */
    u8 pad3[48];                 /* +60..+107 */
} SaveImageHeader;

/* buildSaveImage's `hi`/`sizeHi`/`hi2` locals (and `pool+0x10000` directly)
 * all equal SaveImageHeader's base address PLUS 19388; SIH_OFF folds a
 * field's negative displacement from that point back to the same literal
 * MWCC already emits (19388 - offsetof == the original bare-hex constant). */
#define SIH_OFF(field) (19388 - offsetof(SaveImageHeader, field))

/* mirrors game/sys/texPalette.c's TEXHeader (format @ +4); TEXGet actually
 * returns a TEXDescriptorPtr {TEXHeaderPtr; CLUTHeaderPtr;}, so `tex[0]` is
 * the TEXHeaderPtr this views. */
typedef struct TexHeaderView {
    u8 pad0[4];
    u32 format;        /* +4 */
} TexHeaderView;

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
    u8* pool = lbl_8025EE80;
    char* rpool = lbl_801131C0;
    s32 blockSize;
    u8* hi;
    s32 total;
    s32* sizePtr;
    u8* sizeHi;
    u8* out;
    s32 bytes;
    int i;
    int bit;

    fmtA = fmtA & 3;
    fmtB = fmtB & 4;

    memset(pool + 0x10000 - 19388, 0, 108);
    total = lbl_803449F0;
    blockSize = cardGetTotalBytes();
    total = (total + cardGetTotalBytes() - 1) / blockSize;
    bytes = total * cardGetTotalBytes();
    sizeHi = pool + 0x10000;
    sizePtr = (s32*) (sizeHi - SIH_OFF(imageSize));
    *(s32*) (sizeHi - SIH_OFF(imageSize)) = bytes;

    if (lbl_803449F8 == 0) {
        lbl_803449F8 = (u32) OSAllocFromHeap(__OSCurrHeap, *(u32*) sizePtr);
    }
    memset((void*) lbl_803449F8, 0, *(u32*) sizePtr);

    strncpy((char*) (pool + 0x10000 - 19388), name, 32);
    out = (u8*) lbl_803449F8;
    hi = pool + 0x10000;
    *(s32*) (hi - SIH_OFF(reserved)) = 0;
    strncpy((char*) out, (char*) hdr + 6176, 32);
    strncpy((char*) (out + 32), comment, 32);
    *(s32*) (hi - SIH_OFF(dataOffset)) = (s32) ((out + 64) - (u8*) lbl_803449F8);

    /* banner */
    if ((u32) bannerTex == 0) {
        *(u8*) (hi - SIH_OFF(bannerFmt)) = 2;
        memcpy(out + 64, (u8*) hdr + 32, 6144);
        out += 6208;
    } else {
        void** tex = (void**) TEXGet(bannerTex, 0);

        switch (*(s32*) ((u8*) tex[0] + offsetof(TexHeaderView, format))) {
        case 5:
            *(u8*) (hi - SIH_OFF(bannerFmt)) = 2;
            break;
        case 9:
            *(u8*) (hi - SIH_OFF(bannerFmt)) = 1;
            break;
        default:
            OSPanic(rpool + 608, 856, rpool + 920);
        }
        if ((*(u8*) (pool + 0x10000 - SIH_OFF(bannerFmt)) & 3) == 2) {
            memcpy(out + 64, ((u8**) tex[0])[2], 6144);
            out += 6208;
        } else {
            memcpy(out + 64, ((u8**) tex[0])[2], 3072);
            memcpy(out + 3136, ((u8**) tex[1])[2], 512);
            out += 3648;
        }
    }

    /* icon animation frames */
    if ((u32) iconTex != 0) {
        u16* fmtW = (u16*) (hi - SIH_OFF(iconFmt));
        u16* animW = (u16*) (hi - SIH_OFF(iconSpeed));
        u16* clearAnimW;
        s32 lastFrame;
        s32 j;
        u8* hi2;

        for (i = 0, bit = 0;
             (u32) i < ((TexAnimHdr*) iconTex)->numFrames && i < 8;
             i++, bit += 2) {
            void** tex = (void**) TEXGet(iconTex, i);

            switch (*(s32*) ((u8*) tex[0] + offsetof(TexHeaderView, format))) {
            case 5: {
                u32 mask = 3;
                u32 oldFmt = *fmtW;
                u32 code;

                mask <<= bit;
                code = 2;
                oldFmt &= ~mask;
                code <<= bit;
                *fmtW = (u16) (oldFmt | code);
                break;
            }
            case 9: {
                u32 mask = 3;
                u32 oldFmt = *fmtW;
                u32 code;

                mask <<= bit;
                code = 1;
                oldFmt &= ~mask;
                code <<= bit;
                *fmtW = (u16) (oldFmt | code);
                break;
            }
            default:
                OSPanic(rpool + 608, 901, rpool + 956);
            }
            *animW = (u16) ((*animW & ~(3 << bit)) | (fmtA << bit));
        }

        clearAnimW = (u16*) (pool + 0x10000 - SIH_OFF(iconSpeed));
        for (bit = i << 1; i < 8; i++, bit += 2) {
            *clearAnimW &= ~(3 << bit);
        }

        hi2 = pool + 0x10000;
        *(u8*) (hi2 - SIH_OFF(bannerFmt)) |= fmtB;
        lastFrame = -1;
        for (j = 0, bit = 0;
             (u32) j < ((TexAnimHdr*) iconTex)->numFrames && j < 8;
             j++, bit += 2) {
            void** tex = (void**) TEXGet(iconTex, j);

            switch ((*(u16*) (hi2 - SIH_OFF(iconFmt)) >> bit) & 3) {
            case 2:
                memcpy(out, ((u8**) tex[0])[2], 2048);
                out += 2048;
                break;
            case 1:
                memcpy(out, ((u8**) tex[0])[2], 1024);
                lastFrame = j;
                out += 1024;
                break;
            }
        }
        if (lastFrame >= 0) {
            void** tex = (void**) TEXGet(iconTex, j);

            memcpy(out, ((u8**) tex[1])[2], 512);
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
    u32 size = 0x310000;
    u32 transferSize;
    u8* buf;
    u8* lo;
    u8 pad[8]; /* unused, matches original frame */

    sysSetFlags(64);
    buf = (u8*) GetHiMemCacheTop();
    dcsAramWriteTop((void*)((u32)buf - 0x310000),
                    (transferSize = 0x310000));
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
    register s32 r = saveMenuPrompt(msg, lbl_80343C6C, 2);

    switch (r) {
    case 0:
        return 1;
    case 1: {
        register s32 zero = 0;
        lbl_80344A24 = zero;
        {
            register s32 neg = -1;
            r = zero;
            lbl_80344A18 = neg;
            lbl_80344A14 = neg;
        }
        lbl_80344A20 = zero;
        break;
    }
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
            serve_busy(-1);
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
 * Minimal view of *gWinGlobals's `banks` field.  gWinGlobals is one large,
 * per-TU-polymorphic global (same 0x80344FC0 symbol as game/pb/pb_winglobals.c's
 * PbWGGlobals): that TU's verified layout places a PbWGBankRef* at +0x30
 * (48), matching this function's `win+48` displacement exactly.  Only the
 * one field this function reads (the ref's own `bank` pointer, at +4 in
 * PbWGBankRef) is modeled; the further `bank+12` read below has no
 * corroborating field name in either TU (pb_winglobals.c also leaves that
 * region as opaque padding) and is left raw.
 */
typedef struct WinGlobalsBankRef {
    s32 count;      /* +0 */
    void* bank;      /* +4, mirrors pb_winglobals.c's PbWGBankRef.bank */
} WinGlobalsBankRef;

typedef struct WinGlobalsView {
    u8 pad[0x30];                    /* +0: unmodeled here */
    WinGlobalsBankRef* banks;        /* +0x30, mirrors PbWGGlobals.banks */
} WinGlobalsView;

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
    void* quad;
    char lines[64];
    u8 pad[48]; /* dead stack below lines, matches original frame */
    int nLines;
    int i;
    int widest;
    int lineH;
    int boxW;
    int boxH;
    int x;
    int y;

    win = gWinGlobals;
    gDiskErrorShown = 1;
    savedBusy = (void*) gGameBusy;
    lbl_80344A5D = 1;
    sysResetService();
    vibrators_off();
    gGameBusy = 1;
    gModalRenderDepth++;
    msgUpdate();
    MBHideMarkedMessages();
    MBLockMessages(gModalRenderDepth - 1);

    if (gWinGlobals != 0 && *(void**) ((u8*) win + offsetof(WinGlobalsView, banks)) != 0) {
        extra = (u8*) *(void**) ((u8*) win + offsetof(WinGlobalsView, banks)) +
                offsetof(WinGlobalsBankRef, bank);
    }
    if (extra != 0 && *(s32*) ((u8*) extra + 12) == 0 &&
        strcmp((char*) lbl_802A5D1C, lbl_80347368) == 0) {
        blit = MBNewBlit((void*) lbl_80343CCC, 0, 0);
    } else {
        quad = MBNewTempQuad();
    }

    if (*(u32*) ((u8*) lbl_802A4AA4 + 24) != 0) {
        lbl_80344A54 = 6;
        lbl_80344A58 = lbl_80347370;
    } else {
        lbl_80344A54 = 0;
        lbl_80344A58 = lbl_80347374;
    }

    nLines = FixMLineText(msg, gTextWorkBuf, lines);
    widest = 0;
    for (i = 0; i < nLines; i++) {
        int w = DrawNormalText(((char**) lines)[i], lbl_80344A54, lbl_80344A58);

        if (w > widest) {
            widest = w;
        }
    }
    lineH = FontHeight(lbl_80344A54, lbl_80344A58) + 3;
    boxH = (lineH + 6) * (count1 + TextMLines(msg)) + 60;
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
        MBBlitSetColor(quad, -1);
    }

    strcpy(gTextFormatBuf, msg);
    y += 32;
    for (i = 0; i < nLines; i++) {
        DrawTextSub(-256, y, lbl_80344A54, lbl_80344A58, 0x160C03,
                    lbl_80347378, ((char**) lines)[i]);
        y += lineH;
    }
    y += 3;
    for (i = 0; i < count1; i++) {
        if (i == count2) {
            DrawTextSub(-256, y, lbl_80344A54, lbl_80344A58, 0xFFFFFF,
                        lbl_80347378, options[i]);
        } else {
            DrawTextSub(-256, y, lbl_80344A54, lbl_80344A58, 0x160C03,
                        lbl_80347378, options[i]);
        }
        y += lineH;
    }

    MBEndFrame();
    if (blit != 0) {
        MBRemoveBlit(blit);
    }
    MBUnlockMessages(gModalRenderDepth - 1);
    gGameBusy = (s32) savedBusy;
    gModalRenderDepth--;
}
