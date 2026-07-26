/*
 * cardutil.c - low-level GameCube memory-card command server.
 *
 * Sits *under* game/sys/memcard.c (the savegame layer). All CARD SDK calls
 * are funnelled through a dedicated worker thread ("Cardutilmainloop") so the
 * game loop never blocks on the card; the public wrappers just post a command
 * and either return immediately or spin on VIWaitForRetrace until the worker
 * writes back a result. Address range 0x800DC180..0x800DD180.
 *
 * NOTE: NonMatching - kept for symbol mapping / documentation. Function order
 * follows the DOL (same-TU inlining depends on it).
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
typedef struct CARDStat { u8 _[0x6C]; } CARDStat;
typedef struct CARDFileInfo { u8 _[0x14]; } CARDFileInfo;

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

s32 DVDGetCurrentDiskID(void);

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

/* control block @0x80321A70 (0x68 bytes) */
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
    s32 totalBytes;       /* +0x3c */
    s32 xferBytes;        /* +0x40 CARDGetXferredBytes snapshot */
    s32 totalXfer;        /* +0x44 total bytes of active transfer */
    OSMutex mutex2;       /* +0x48 transfer lock */
    u8 _pad[4];           /* +0x60 */
    s32 cancelFlag;       /* +0x64 */
} CardMgr;

static u8 gCardBuf[0x310];  /* @0x80321760 CARD work/scratch area */
static CardMgr gCardMgr;    /* @0x80321A70 */

/* handlers */
static s32 cardDoWrite(s32 chan, s32 fileNo, void* data);
static s32 cardDoLoad(s32 chan, void* data);
static s32 cardDoDelete(s32 chan, s32 fileNo);
static s32 cardDoMount(s32 chan, s32 doCheck);
static s32 cardSubmitCommand(s32 chan, s32 cmdType, s32 param1, void* param2);

/* 0x800DC180 */
s32 cardInit(void) {
    return CARDInit();
}

/* 0x800DC1A0 - post the quit command and wait for the worker to acknowledge */
void cardExit(void) {
    cardSubmitCommand(0, CARDCMD_QUIT, 0, NULL);
    do {
        VIWaitForRetrace();
    } while (gCardMgr.result == -1);
}

/* 0x800DC2C0 - the "Cardutilmainloop" worker thread */
static void* cardThreadMain(s32 chan, s32 fileNo, void* data) {
    CardMgr* m = &gCardMgr;
    BOOL quit = FALSE;
    s32 res = 0;
    (void)chan; (void)fileNo; (void)data;

    while (!quit) {
        OSLockMutex(&m->mutex);
        while (m->command == -1) {
            OSWaitCond(&m->cond, &m->mutex);
        }
        chan = m->command;
        fileNo = m->param1;
        data = m->param2;
        OSUnlockMutex(&m->mutex);

        switch (m->cmdType) {
        case CARDCMD_MOUNT:   res = cardDoMount(chan, (s32)data); break;
        case CARDCMD_UNMOUNT: m->cancelFlag = 0; res = CARDUnmount(chan); break;
        case CARDCMD_FORMAT:
            m->cancelFlag = 0;
            m->totalXfer = 0xFFFFA000;
            res = CARDFormat(chan);
            if (res == 0) res = CARDFreeBlocks(chan, &m->freeBytes, &m->freeFiles);
            break;
        case CARDCMD_LOAD:    res = cardDoLoad(chan, data); break;
        case CARDCMD_DELETE:  res = cardDoDelete(chan, fileNo); break;
        case CARDCMD_READ: {
            CARDFileInfo info;
            CARDStat stat;
            res = CARDGetStatus(chan, fileNo, &stat);
            if (res >= 0) {
                res = CARDFastOpen(chan, fileNo, &info);
                if (res >= 0) {
                    m->totalXfer = *(s32*)((u8*)&stat + 0x4);
                    res = CARDRead(&info, data, m->totalXfer, 0);
                    CARDClose(&info);
                }
            }
            break;
        }
        case CARDCMD_WRITE:   res = cardDoWrite(chan, fileNo, data); break;
        case CARDCMD_QUIT:    quit = TRUE; break;
        default: break;
        }

        OSLockMutex(&m->mutex);
        m->result = res;
        m->command = -1;
        printf("Cardutilmainloop....");
        OSUnlockMutex(&m->mutex);
    }
    return NULL;
}

/* 0x800DC1F4 - init sync objects, spawn the worker, wait until it's idle */
void cardStart(s32 chan, s32 fileNo, void* data) {
    CardMgr* m = &gCardMgr;
    static OSThread thread;
    OSInitMutex(&m->mutex);
    OSInitMutex(&m->mutex2);
    OSInitCond(&m->cond);
    OSCreateThread(&thread, (void* (*)(void*))cardThreadMain, NULL, NULL, 0,
                   (s32)chan, 1);
    OSResumeThread(&thread);
    (void)fileNo; (void)data;
    do {
        VIWaitForRetrace();
    } while (m->result == -1);
}

/* 0x800DC280 - block until the worker has a result */
s32 cardWaitResult(void) {
    CardMgr* m = &gCardMgr;
    s32 r;
    do {
        VIWaitForRetrace();
        r = m->result;
    } while (r == -1);
    return r;
}

/* command wrappers 0x800DC4D4..0x800DC5C4 */
s32 cardWriteFile(s32 chan, s32 fileNo, void* data) { return cardSubmitCommand(chan, CARDCMD_WRITE, fileNo, data); }
s32 cardReadFile(s32 chan, s32 fileNo, void* data)  { return cardSubmitCommand(chan, CARDCMD_READ, fileNo, data); }
s32 cardFormat(s32 chan)                            { return cardSubmitCommand(chan, CARDCMD_FORMAT, 0, NULL); }
s32 cardLoadFile(s32 chan, void* data)              { return cardSubmitCommand(chan, CARDCMD_LOAD, 0, data); }
s32 cardUnmount(s32 chan)                           { return cardSubmitCommand(chan, CARDCMD_UNMOUNT, 0, NULL); }
s32 cardMount(s32 chan, s32 doCheck)                { return cardSubmitCommand(chan, CARDCMD_MOUNT, 0, (void*)doCheck); }

/* 0x800DC5F4 */
s32 cardGetResult(void) { return gCardMgr.result; }

/* 0x800DC604 - post a command if the worker is idle, else return last result */
static s32 cardSubmitCommand(s32 chan, s32 cmdType, s32 param1, void* param2) {
    CardMgr* m = &gCardMgr;
    s32 ret;
    OSLockMutex(&m->mutex);
    if (m->command != -1) {
        ret = m->result;
    } else {
        m->command = chan;
        m->cmdType = cmdType;
        m->param1 = param1;
        m->param2 = param2;
        m->result = -1;
        if (cmdType == CARDCMD_LOAD) {
            m->xferBytes = CARDGetXferredBytes(chan);
        }
        ret = 0;
        OSSignalCond(&m->cond);
    }
    OSUnlockMutex(&m->mutex);
    return ret;
}

/* 0x800DC6A4 - create/overwrite a save file and write its payload */
static s32 cardDoWrite(s32 chan, s32 fileNo, void* data) {
    CardMgr* m = &gCardMgr;
    (void)m; (void)fileNo; (void)chan;
    /* strncpy name, CARDCreate/CARDOpen, CARDWrite, CARDSetStatus, cleanup */
    return CARDWrite((CARDFileInfo*)gCardBuf, data, m->totalXfer, 0);
}

/* 0x800DCAC0 - load a save, verifying the current disc id, cleaning stale copies */
static s32 cardDoLoad(s32 chan, void* data) {
    CardMgr* m = &gCardMgr;
    (void)DVDGetCurrentDiskID();
    (void)data;
    return CARDFreeBlocks(chan, &m->freeBytes, &m->freeFiles);
}

/* 0x800DCEC4 - delete a save file and refresh free space */
static s32 cardDoDelete(s32 chan, s32 fileNo) {
    CardMgr* m = &gCardMgr;
    s32 res = CARDFastDelete(chan, fileNo);
    CARDFreeBlocks(chan, &m->freeBytes, &m->freeFiles);
    return res;
}

/* 0x800DCFC0 - mount, optionally verify with CARDCheck, cache free space */
static s32 cardDoMount(s32 chan, s32 doCheck) {
    CardMgr* m = &gCardMgr;
    u32 sectorSize;
    s32 res = CARDMount(chan, gCardBuf, NULL);
    if (res >= 0) {
        CARDGetSectorSize(chan, &sectorSize);
        if (doCheck) res = CARDCheck(chan);
        CARDFreeBlocks(chan, &m->freeBytes, &m->freeFiles);
    }
    return res;
}

/* free-space getters 0x800DD0C4..0x800DD10C */
s32 cardGetTotalBytes(void) { return gCardMgr.totalBytes; }
s32 cardGetFreeFiles(void)  { return gCardMgr.freeFiles; }
s32 cardGetUsedPercent(void) {
    if (gCardMgr.totalBytes == 0) return 0;
    return (u32)gCardMgr.freeBytes / (u32)gCardMgr.totalBytes;
}
s32 cardGetFreeBytes(void) { return gCardMgr.freeBytes; }

/* 0x800DD11C / 0x800DD148 - transfer lock */
void cardUnlock(void) { OSUnlockMutex(&gCardMgr.mutex2); }
s32 cardLock(void) {
    OSLockMutex(&gCardMgr.mutex2);
    return gCardMgr.cancelFlag;
}
