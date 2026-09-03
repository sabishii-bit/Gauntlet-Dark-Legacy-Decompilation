/*
 * cardutil.c - low-level GameCube memory-card command server.
 *
 * Sits *under* game/sys/memcard.c (the savegame layer). All CARD SDK calls
 * are funnelled through a dedicated worker thread ("Cardutilmainloop") so the
 * game loop never blocks on the card; the public wrappers just post a command
 * and either return immediately or spin on VIWaitForRetrace until the worker
 * writes back a result. Address range 0x800DC180..0x800DD180.
 *
 * NOTE: NonMatching - the directory-management handlers (cardDoWrite/DoLoad/
 * DoDelete) are fully reconstructed from the target asm; they operate on the
 * shared savegame-directory table (an array of CardDirEntry) whose buffer is
 * handed in from memcard.c. Their icon/banner-offset bookkeeping keeps regalloc
 * residuals, so the TU stays NonMatching; the other handlers are byte-exact.
 * Function order follows the DOL (same-TU inlining depends on it).
 *
 * PARKED light-match residuals (do not re-hunt):
 *   cardStart - instruction stream now matches; fndiff only reports the
 *     private `...bss.0` alias used for gCardBuf.
 *   cardSubmitCommand - target recomputes &M.mutex at the unlock instead of
 *     caching it, so it needs one fewer saved register (CSE/regalloc).
 *   cardDoWrite/cardDoLoad - stack slots aligned; residual is icon-offset
 *     strength-reduction + regalloc; gCardBuf overlay shows as a `...bss.0`
 *     reloc in fndiff (false positive, byte-identical).
 */
#include "types.h"

/* --- dolphin/os --- */
typedef struct OSMutex { u8 _[0x18]; } OSMutex;
typedef struct OSCond { u8 _[0x08]; } OSCond;
typedef struct OSThread { u8 _[0x318]; } OSThread;

void OSInitMutex(OSMutex* m);
void OSLockMutex(OSMutex* m);
void OSUnlockMutex(OSMutex* m);
void OSInitCond(OSCond* c);
void OSWaitCond(OSCond* c, OSMutex* m);
void OSSignalCond(OSCond* c);
BOOL OSCreateThread(OSThread* t, void* (*func)(void*), void* param,
                    void* stack, u32 stackSize, s32 prio, u16 flags);
s32 OSResumeThread(OSThread* t);
void VIWaitForRetrace(void);

/* --- dolphin/card --- */
typedef struct CARDStat {
    char fileName[0x20];   /* 0x00 */
    u32 length;            /* 0x20 */
    u32 time;              /* 0x24 */
    u8 gameName[4];        /* 0x28 */
    u8 company[2];         /* 0x2c */
    u8 bannerFormat;       /* 0x2e */
    u8 _pad0;              /* 0x2f */
    u32 iconAddr;          /* 0x30 */
    u16 iconFormat;        /* 0x34 */
    u16 iconSpeed;         /* 0x36 */
    u32 commentAddr;       /* 0x38 */
    u32 offsetBanner;      /* 0x3c */
    u32 offsetBannerTlut;  /* 0x40 */
    u32 offsetIcon[8];     /* 0x44 */
    u32 offsetIconTlut;    /* 0x64 */
    u32 offsetData;        /* 0x68 */
} CARDStat;                /* 0x6c */
typedef struct CARDFileInfo {
    s32 chan;              /* 0x00 */
    s32 fileNo;            /* 0x04 */
    s32 offset;            /* 0x08 */
    s32 length;            /* 0x0c */
    u16 iBlock;            /* 0x10 */
} CARDFileInfo;            /* 0x14 */

s32 CARDInit(void);
s32 CARDMount(s32 chan, void* workArea, void* detachCallback);
s32 CARDUnmount(s32 chan);
s32 CARDGetSectorSize(s32 chan, u32* size);
s32 CARDCheck(s32 chan);
s32 CARDFreeBlocks(s32 chan, s32* byteNotUsed, s32* filesNotUsed);
s32 CARDFormat(s32 chan);
s32 CARDGetStatus(s32 chan, s32 fileNo, CARDStat* stat);
s32 CARDSetStatus(s32 chan, s32 fileNo, CARDStat* stat);
s32 CARDCreate(s32 chan, const char* name, u32 size, CARDFileInfo* info);
s32 CARDOpen(s32 chan, const char* name, CARDFileInfo* info);
s32 CARDFastOpen(s32 chan, s32 fileNo, CARDFileInfo* info);
s32 CARDClose(CARDFileInfo* info);
s32 CARDRead(CARDFileInfo* info, void* buf, s32 len, s32 offset);
s32 CARDWrite(CARDFileInfo* info, const void* buf, s32 len, s32 offset);
s32 CARDRename(s32 chan, const char* old, const char* new_);
s32 CARDFastDelete(s32 chan, s32 fileNo);
u32 CARDGetXferredBytes(s32 chan);

u8* DVDGetCurrentDiskID(void);

void* memset(void* p, int c, u32 n);
void* memcpy(void* d, const void* s, u32 n);
void* memmove(void* d, const void* s, u32 n);
int memcmp(const void* a, const void* b, u32 n);
char* strncpy(char* d, const char* s, u32 n);
u32 strlen(const char* s);
void DCFlushRange(void* p, u32 n);
void DCStoreRange(void* p, u32 n);
int printf(const char* fmt, ...);

/* card commands (see cardCmdJumptable @0x80129710) */
enum {
    CARDCMD_NOP = 0,
    CARDCMD_MOUNT = 1,
    CARDCMD_UNMOUNT = 2,
    CARDCMD_FORMAT = 3,
    CARDCMD_LOAD = 4,
    CARDCMD_DELETE = 5,
    CARDCMD_READ = 6,
    CARDCMD_WRITE = 7,
    CARDCMD_QUIT = 8
};

/* control block @0x80321A70 (0x68 bytes). The thread / command server code
 * reaches it as &gCardBuf[0x310] instead of via the gCardMgr symbol, so the
 * whole control block is addressed off the gCardBuf base register. */
typedef struct CardMgr {
    OSMutex mutex;        /* +0x00 primary lock */
    OSCond cond;          /* +0x18 command-ready condition */
    s32 command;          /* +0x20 channel, -1 == idle */
    s32 cmdType;          /* +0x24 CARDCMD_* */
    s32 param1;           /* +0x28 file number */
    void* param2;         /* +0x2c buffer / arg */
    s32 result;           /* +0x30 last CARD result, -1 while pending */
    s32 freeBytes;        /* +0x34 CARDFreeBlocks byteNotUsed */
    s32 freeFiles;        /* +0x38 CARDFreeBlocks filesNotUsed */
    s32 totalBytes;       /* +0x3c sector size snapshot */
    s32 xferBytes;        /* +0x40 CARDGetXferredBytes snapshot */
    s32 totalXfer;        /* +0x44 total bytes of active transfer */
    OSMutex mutex2;       /* +0x48 transfer lock */
    void* dir;            /* +0x60 savegame directory table (array of CardDirEntry) */
    s32 dirCount;         /* +0x64 live entries in dir[]; also read by cardLock */
} CardMgr;

/* One in-RAM savegame-directory entry (0x5B40 bytes). The dir table that
 * memcard.c hands to cardDoLoad is an array of these. The first 0x5A00 bytes
 * are the icon/banner/comment/save-data workspace; the metadata trailer keeps
 * the CARD fileNo, a copy of the CARDStat, and the recomputed icon offsets. */
typedef struct CardDirEntry {
    u8 data[0x5A00];      /* 0x0000 icon+banner+comment+save workspace */
    u8 comment[0x40];     /* 0x5A00 32x2 comment block */
    s32 fileNo;           /* 0x5A40 CARD file number */
    CARDStat stat;        /* 0x5A44 status snapshot (0x6C) */
    u32 dataOffset;       /* 0x5AB0 running icon-data offset */
    u32 iconOffset[8];    /* 0x5AB4 per-frame image offset */
    u32 _gap[6];          /* 0x5AD4 */
    u32 iconTlut[8];      /* 0x5AEC per-frame tlut index */
    u8 _tail[0x34];       /* 0x5B0C pad to stride */
} CardDirEntry;           /* 0x5B40 */

/* gCardBuf-based overlay: the control block lives just past the work area. */
typedef struct CardMgrBuf {
    u8 _pad[0x310];
    CardMgr mgr;
} CardMgrBuf;

static u8 gCardBuf[0x310]; /* @0x80321760 CARD work/scratch area */
static CardMgr gCardMgr;   /* @0x80321A70 */

/* control block addressed off gCardBuf (keeps gCardBuf as the base register) */
#define M (((CardMgrBuf*)gCardBuf)->mgr)

/* handlers */
static void* cardThreadMain(void* arg, s32 arg2, s32 arg3);
static s32 cardDoWrite(s32 chan, CARDStat* stat, void* data);
static s32 cardDoLoad(s32 chan, void* dirBuf);
static s32 cardDoDelete(s32 chan, s32 fileNo);
static s32 cardDoMount(s32 chan, void* workArea);
static s32 cardSubmitCommand(s32 chan, s32 cmdType, s32 param1, void* param2, s32 param3);

/* 0x800DC180 */
s32 cardInit(void) {
    return CARDInit();
}

/* 0x800DC1A0 - post the quit command and wait for the worker to acknowledge */
void cardExit(void) {
    u8 unused[8];
    cardSubmitCommand(0, CARDCMD_QUIT, 0, NULL, 0);
    do {
        VIWaitForRetrace();
    } while (gCardMgr.result == -1);
}

static inline s32 cardDoRead(s32 chan, s32 fileNo, void* data, CardMgrBuf* card) {
    CARDStat stat;
    CARDFileInfo info;
    u8 unused[8];
    s32 res;
    u32 length;

    res = CARDGetStatus(chan, fileNo, &stat);
    if (res < 0) {
        return res;
    }
    res = CARDFastOpen(chan, fileNo, &info);
    if (res < 0) {
        return res;
    }
    length = stat.length;
    card->mgr.totalXfer = length;
    res = CARDRead(&info, data, length, 0);
    CARDClose(&info);
    return res;
}

/* 0x800DC1F4 - init sync objects, spawn the worker, wait until it's idle */
void cardStart(s32 chan, s32 fileNo, void* data) {
    u8 unused[8];
    OSInitMutex(&M.mutex);
    OSInitMutex(&M.mutex2);
    OSInitCond(&M.cond);
    OSCreateThread((OSThread*)gCardBuf, (void* (*)(void*))cardThreadMain, NULL,
                   (void*)chan, fileNo, (s32)data, 1);
    OSResumeThread((OSThread*)gCardBuf);
    do {
        VIWaitForRetrace();
    } while (M.result == -1);
}

static inline BOOL cardResultPending(s32 value, s32* result) {
    *result = value;
    return value == -1;
}

/* 0x800DC280 - block until the worker has a result */
s32 cardWaitResult(void) {
    register CardMgr* m = &gCardMgr;
    s32 r;
    s32 value;
    do {
        VIWaitForRetrace();
        value = m->result;
    } while (cardResultPending(value, &r));
    return r;
}

/* 0x800DC2C0 - the "Cardutilmainloop" worker thread.  Three declared
 * parameters: the target reserves 12 bytes of parameter save area at r1+8,
 * which is what places the locals at r1+20; only the first is used, and
 * OSCreateThread takes the address through a cast. */
static void* cardThreadMain(void* arg, s32 arg2, s32 arg3) {
    CardMgrBuf* card = (CardMgrBuf*)gCardBuf;
    OSMutex* mutex2;
    s32* command;
    BOOL quit;
    s32* freeFiles;
    s32* freeBytes;
    OSCond* cond;
    void* data;
    s32 chan;
    s32 res;
    s32 cmdType, fileNo;
    (void)arg;

    cond = &card->mgr.cond;
    mutex2 = &card->mgr.mutex2;
    freeFiles = &card->mgr.freeFiles;
    freeBytes = &card->mgr.freeBytes;
    command = &card->mgr.command;
    quit = FALSE;

    while (!quit) {
        OSLockMutex(&card->mgr.mutex);
        while (*command == -1) {
            OSWaitCond(cond, &card->mgr.mutex);
        }
        chan = *command;
        cmdType = card->mgr.cmdType;
        fileNo = card->mgr.param1;
        data = card->mgr.param2;
        OSUnlockMutex(&card->mgr.mutex);

        switch (cmdType) {
        case CARDCMD_MOUNT:
            res = cardDoMount(chan, data);
            break;
        case CARDCMD_UNMOUNT:
            OSLockMutex(mutex2);
            card->mgr.dirCount = 0;
            OSUnlockMutex(mutex2);
            res = CARDUnmount(chan);
            break;
        case CARDCMD_FORMAT: {
            s32 status;
            OSLockMutex(mutex2);
            card->mgr.dirCount = 0;
            OSUnlockMutex(mutex2);
            card->mgr.totalXfer = 0xA000;
            status = CARDFormat(chan);
            if (status == 0)
                status = CARDFreeBlocks(chan, freeBytes, freeFiles);
            res = status;
            break;
        }
        case CARDCMD_LOAD:
            res = cardDoLoad(chan, data);
            break;
        case CARDCMD_DELETE:
            res = cardDoDelete(chan, fileNo);
            break;
        case CARDCMD_READ:
            res = cardDoRead(chan, fileNo, data, card);
            break;
        case CARDCMD_WRITE:
            /* the WRITE command carries a CARDStat* in the param1 ("fileNo")
             * slot; the payload buffer travels in param2. */
            res = cardDoWrite(chan, (CARDStat*)fileNo, data);
            break;
        case CARDCMD_QUIT:
            quit = TRUE;
            break;
        default:
            break;
        }

        OSLockMutex(&card->mgr.mutex);
        card->mgr.result = res;
        *command = -1;
        printf("Cardutilmainloop....\n");
        OSUnlockMutex(&card->mgr.mutex);
    }
    return NULL;
}

/* command wrappers 0x800DC4D4..0x800DC5C4 */
s32 cardWriteFile(s32 chan, s32 fileNo, void* data) { return cardSubmitCommand(chan, CARDCMD_WRITE, fileNo, data, 0); }
s32 cardReadFile(s32 chan, s32 fileNo, void* data)  { return cardSubmitCommand(chan, CARDCMD_READ, fileNo, data, 0); }
s32 cardFormat(s32 chan)                            { return cardSubmitCommand(chan, CARDCMD_FORMAT, 0, NULL, 0); }
s32 cardLoadFile(s32 chan, void* data)              { return cardSubmitCommand(chan, CARDCMD_LOAD, 0, data, 0); }
s32 cardUnmount(s32 chan)                           { return cardSubmitCommand(chan, CARDCMD_UNMOUNT, 0, NULL, 0); }
s32 cardMount(s32 chan, void* workArea, s32 arg3)   { return cardSubmitCommand(chan, CARDCMD_MOUNT, 0, workArea, arg3); }

/* 0x800DC5F4 */
s32 cardGetResult(void) { return gCardMgr.result; }

/* 0x800DC604 - post a command if the worker is idle, else return last result */
#pragma opt_common_subs off
static s32 cardSubmitCommand(s32 chan, s32 cmdType, s32 param1, void* param2, s32 param3) {
    s32 ret;
    s32* command;
    (void)param3;
    OSLockMutex(&M.mutex);
    if (*(command = &M.command) != -1) {
        ret = M.result;
    } else {
        *command = chan;
        M.cmdType = cmdType;
        M.param1 = param1;
        M.param2 = param2;
        M.result = -1;
        if (cmdType != CARDCMD_LOAD) {
            M.xferBytes = CARDGetXferredBytes(chan);
        }
        ret = 0;
        OSSignalCond(&M.cond);
    }
    OSUnlockMutex(&M.mutex);
    return ret;
}
#pragma opt_common_subs reset

/* 0x800DC6A4 - create/overwrite a save file (atomically, via a "~name" temp)
 * and refresh the in-RAM directory cache. `stat` describes the file, `data`
 * is the payload buffer (icon/banner/comment/save-data laid out per stat). */
static s32 cardDoWrite(s32 chan, CARDStat* stat, void* data) {
    CARDFileInfo info;
    char name[0x21];
    char tmpName[0x21];
    s32 reopened;
    s32 newFileNo;
    s32 existingFileNo;
    s32 res;
    u8* e;
    u8* end;
    int i;

    reopened = 0;
    existingFileNo = 0;
    strncpy(name, stat->fileName, 0x20);
    name[0x20] = existingFileNo;
    if (strlen(name) >= 0x20) {
        return -12;
    }
    if (name[0] == 0x7e) {
        return -128;
    }
    tmpName[0] = 0x7e;
    strncpy(tmpName + 1, stat->fileName, 0x1f);
    tmpName[0x20] = existingFileNo;

    /* remember any pre-existing copy so we can delete it after the temp write */
    existingFileNo = -1;
    if (CARDOpen(chan, name, &info) == 0) {
        existingFileNo = info.fileNo;
        CARDClose(&info);
    }

    M.totalXfer = stat->length + 0x8000;
    if (existingFileNo >= 0 && existingFileNo < 0x7f) {
        M.totalXfer += 0x4000;
    }

    res = CARDCreate(chan, tmpName, stat->length, &info);
    if (res < 0) {
        /* no room for the temp: fall back to overwriting in place if possible */
        if (existingFileNo >= 0) {
            res = CARDOpen(chan, name, &info);
            if (res == 0) {
                reopened = 1;
            }
        }
        if (!reopened) {
            return res;
        }
    }

    newFileNo = info.fileNo;
    res = CARDWrite(&info, data, stat->length, 0);
    CARDClose(&info);
    if (res < 0) {
        goto fail;
    }
    if ((res = CARDSetStatus(chan, newFileNo, stat)) < 0) {
    fail:
        return res;
    }

    if (!reopened) {
        /* commit the temp: drop the old copy, then rename "~name" -> "name" */
        if (existingFileNo >= 0 && existingFileNo < 0x7f) {
            res = CARDFastDelete(chan, existingFileNo);
            if (res < 0) {
                return res;
            }
        }
        res = CARDRename(chan, tmpName, name);
        if (res < 0) {
            return res;
        }
    }

    if (M.dir == NULL) {
        return CARDFreeBlocks(chan, &M.freeBytes, &M.freeFiles);
    }

    OSLockMutex(&M.mutex2);
    if (existingFileNo == -1) {
        e = (u8*)M.dir + M.dirCount * 0x5b40;
        M.dirCount++;
    } else {
        end = (u8*)M.dir + M.dirCount * 0x5b40;
        for (e = (u8*)M.dir; e < end; e += 0x5b40) {
            if (*(s32*)(e + 0x5a40) == existingFileNo) {
                break;
            }
        }
        if (e == end) {
            M.dirCount++;
        }
    }

    memset(e + 0x5a00, 0, 0x40);
    if (stat->commentAddr <= stat->length - 0x40) {
        memmove(e + 0x5a00, (u8*)data + stat->commentAddr, 0x40);
    }
    *(u32*)(e + 0x5ab0) = 0;
    if (stat->bannerFormat != 0 || stat->iconFormat != 0) {
        s32 iconCount;
        s32 ciCount;
        int spShift;
        int fmtShift;

        memmove(e, (u8*)data + stat->iconAddr, stat->offsetData - stat->iconAddr);
        DCFlushRange(e, stat->offsetData - stat->iconAddr);

        iconCount = 0;
        ciCount = 0;
        spShift = 0;
        fmtShift = 0;
        for (i = 0; i < 8; i++) {
            s32 sp = (stat->iconSpeed >> spShift) & 3;
            if (sp == 0) {
                break;
            }
            *(u32*)(e + 0x5ab4 + i * 4) = *(u32*)(e + 0x5ab0);
            *(u32*)(e + 0x5aec + i * 4) = ciCount;
            *(u32*)(e + 0x5ab0) += sp << 2;
            if ((stat->iconFormat >> fmtShift) & 3) {
                ciCount++;
                fmtShift += 2;
            }
            iconCount++;
            spShift += 2;
        }
        if ((stat->bannerFormat & 4) == 4 && iconCount > 2) {
            int k;
            int count = iconCount - 2;
            int dstOff = i * 4;
            int shift = count * 2;
            int srcIndex = count;

            for (k = 0; k < count; k++) {
                s32 sp = (stat->iconSpeed >> shift) & 3;
                *(u32*)(e + 0x5ab4 + dstOff) = *(u32*)(e + 0x5ab0);
                *(u32*)(e + 0x5aec + dstOff) =
                    ((u32*)e)[srcIndex + (0x5aec / sizeof(u32))];
                *(u32*)(e + 0x5ab0) += sp << 2;
                shift -= 2;
                srcIndex--;
                dstOff += 4;
            }
        }
    }

    memcpy(e + 0x5a44, stat, 0x6c);
    *(s32*)(e + 0x5a40) = newFileNo;
    OSUnlockMutex(&M.mutex2);
    return CARDFreeBlocks(chan, &M.freeBytes, &M.freeFiles);
}

/* 0x800DCAC0 - (re)build the in-RAM directory: scan every CARD file, keep the
 * ones belonging to the current disc, salvage/purge orphaned "~name" temps,
 * and cache each save's icon/banner block into its directory entry. */
#pragma opt_propagation off
static s32 cardDoLoad(s32 chan, void* dirBuf) {
    CARDFileInfo info;
    char tmpName[0x24];
    char tmpTilde[0x24];
    u8* diskID;
    CARDStat* st;
    s32 res;
    s32 fileNo;
    u8* dir;
    u8* e;
    int i;

    diskID = DVDGetCurrentDiskID();
    res = 0;
    OSLockMutex(&M.mutex2);
    M.dir = dir = (u8*)dirBuf;
    M.dirCount = res;
    OSUnlockMutex(&M.mutex2);
    if (dirBuf == NULL) {
        return 0;
    }
    memset(dirBuf, 0, 0x2d44c0);

    for (fileNo = 0; fileNo < 0x7f; fileNo++) {
        e = dir + M.dirCount * 0x5b40;
        st = (CARDStat*)(e + 0x5a44);
        if (CARDGetStatus(chan, fileNo, st) < 0) {
            continue;
        }
        if (memcmp(st->gameName, diskID, 4) != 0) {
            continue;
        }
        if (memcmp(st->company, diskID + 4, 2) != 0) {
            continue;
        }

        if (st->fileName[0] == 0x7e) {
            /* orphaned temp: try to promote it, otherwise delete it */
            strncpy(tmpTilde, st->fileName, 0x20);
            tmpTilde[0x20] = 0;
            strncpy(tmpName, tmpTilde + 1, 0x20);
            tmpName[0x20] = 0;
            if (st->commentAddr <= st->length - 0x40 &&
                CARDRename(chan, tmpTilde, tmpName) == 0) {
                fileNo--;
                continue;
            }
            res = CARDFastDelete(chan, fileNo);
            if (res < 0) {
                return res;
            }
            res = CARDFreeBlocks(chan, &M.freeBytes, &M.freeFiles);
            if (res < 0) {
                return res;
            }
            continue;
        }

        memset(e + 0x5a00, 0, 0x40);
        if (st->commentAddr <= st->length - 0x40) {
            s32 base;
            s32 len;
            s32 t = CARDFastOpen(chan, fileNo, &info);
            if (t < 0) {
                return t;
            }
            base = st->commentAddr & ~0x1ff;
            len = ((st->commentAddr + 0x40) - base + 0x1ff) & ~0x1ff;
            res = CARDRead(&info, e, len, base);
            CARDClose(&info);
            if (res < 0) {
                return res;
            }
            memmove(e + 0x5a00, e + (st->commentAddr & 0x1ff), 0x40);
        }

        if ((st->bannerFormat != 0 || st->iconFormat != 0) &&
            st->offsetData <= st->length && st->iconAddr < st->offsetData) {
            s32 iconCount;
            int spShift;
            s32 ciCount;
            int fmtShift;
            s32 base;
            s32 len;
            s32 t = CARDFastOpen(chan, fileNo, &info);
            if (t < 0) {
                return t;
            }
            base = st->iconAddr & ~0x1ff;
            len = ((st->offsetData - base) + 0x1ff) & ~0x1ff;
            res = CARDRead(&info, e, len, base);
            CARDClose(&info);
            if (res < 0) {
                return res;
            }
            memmove(e, e + (st->iconAddr & 0x1ff), st->offsetData - st->iconAddr);
            DCFlushRange(e, st->offsetData - st->iconAddr);

            *(u32*)(e + 0x5ab0) = 0;
            i = 0;
            iconCount = 0;
            spShift = 0;
            ciCount = 0;
            fmtShift = 0;
            for (; i < 8; i++) {
                s32 sp = (st->iconSpeed >> spShift) & 3;
                if (sp == 0) {
                    break;
                }
                *(u32*)(e + 0x5ab4 + i * 4) = *(u32*)(e + 0x5ab0);
                *(u32*)(e + 0x5aec + i * 4) = ciCount;
                *(u32*)(e + 0x5ab0) += sp << 2;
                if ((st->iconFormat >> fmtShift) & 3) {
                    ciCount++;
                    fmtShift += 2;
                }
                iconCount++;
                spShift += 2;
            }
            if ((st->bannerFormat & 4) == 4 && iconCount > 2) {
                int k;
                int count = iconCount - 2;
                int dstOff = i * 4;
                int shift = count * 2;
                int srcIndex = count;

                for (k = 0; k < count; k++) {
                    s32 sp = (st->iconSpeed >> shift) & 3;
                    *(u32*)(e + 0x5ab4 + dstOff) = *(u32*)(e + 0x5ab0);
                    *(u32*)(e + 0x5aec + dstOff) =
                        ((u32*)e)[srcIndex + (0x5aec / sizeof(u32))];
                    *(u32*)(e + 0x5ab0) += sp << 2;
                    shift -= 2;
                    srcIndex--;
                    dstOff += 4;
                }
            }
        }

        *(s32*)(e + 0x5a40) = fileNo;
        OSLockMutex(&M.mutex2);
        M.dirCount++;
        OSUnlockMutex(&M.mutex2);
    }
    return res;
}
#pragma opt_propagation reset

/* 0x800DCEC4 - delete a save file, compact its slot out of the in-RAM
 * directory cache, and refresh free space. */
static s32 cardDoDelete(s32 chan, s32 fileNo) {
    s32 res;
    u8* e;

    M.totalXfer = 0x4000;
    res = CARDFastDelete(chan, fileNo);
    if (res < 0) {
        return res;
    }
    if (M.dir != NULL) {
        for (e = (u8*)M.dir; e < (u8*)M.dir + M.dirCount * 0x5b40; e += 0x5b40) {
            if (*(s32*)(e + 0x5a40) == fileNo) {
                OSLockMutex(&M.mutex2);
                memmove(e, e + 0x5b40,
                        ((u8*)M.dir + M.dirCount * 0x5b40) - (e + 0x5b40));
                M.dirCount--;
                DCStoreRange(e, ((u8*)M.dir + M.dirCount * 0x5b40) - e);
                OSUnlockMutex(&M.mutex2);
            }
        }
    }
    return CARDFreeBlocks(chan, &M.freeBytes, &M.freeFiles);
}

/* 0x800DCFC0 - mount, decode the return code, cache free space */
static s32 cardDoMount(s32 chan, void* workArea) {
    s32 res;
    OSLockMutex(&M.mutex2);
    M.dirCount = 0;
    OSUnlockMutex(&M.mutex2);
    M.freeFiles = 0;
    M.freeBytes = 0;
    M.totalXfer = 0xA000;
    res = CARDMount(chan, workArea, NULL);
    switch (res) {
    case -6:
    case 0: {
        s32 t = CARDGetSectorSize(chan, (u32*)&M.totalBytes);
        if (t < 0)
            return t;
        res = CARDCheck(chan);
        break;
    }
    case -13: {
        s32 t = CARDGetSectorSize(chan, (u32*)&M.totalBytes);
        if (t < 0)
            return t;
        break;
    }
    default:
        break;
    }
    if (res == 0)
        res = CARDFreeBlocks(chan, &M.freeBytes, &M.freeFiles);
    return res;
}

/* free-space getters 0x800DD0C4..0x800DD10C */
s32 cardGetTotalBytes(void) { return gCardMgr.totalBytes; }
s32 cardGetFreeFiles(void)  { return gCardMgr.freeFiles; }
s32 cardGetUsedPercent(void) {
    u32 total = gCardMgr.totalBytes;
    if (total != 0)
        return gCardMgr.freeBytes / total;
    return 0;
}
s32 cardGetFreeBytes(void) { return gCardMgr.freeBytes; }

/* 0x800DD11C / 0x800DD148 - transfer lock */
void cardUnlock(void) { OSUnlockMutex(&gCardMgr.mutex2); }
s32 cardLock(void) {
    OSLockMutex(&gCardMgr.mutex2);
    return gCardMgr.dirCount;
}
