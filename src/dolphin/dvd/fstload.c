#include "types.h"
#include "dolphin/dvd.h"

void OSReport(const char* msg, ...);
void* OSGetArenaHi(void);
void OSSetArenaHi(void* newHi);
void* memcpy(void* dst, const void* src, u32 n);

#define OSPhysicalToCached(paddr) ((void*)((u32)(paddr) + 0x80000000))
#define OSRoundUp32B(x) (((u32)(x) + 32 - 1) & ~(32 - 1))

struct OSBootInfo_s {
    /* 0x00 */ DVDDiskID DVDDiskID;
    /* 0x20 */ u32 magic;
    /* 0x24 */ u32 version;
    /* 0x28 */ u32 memorySize;
    /* 0x2C */ u32 consoleType;
    /* 0x30 */ void* arenaLo;
    /* 0x34 */ void* arenaHi;
    /* 0x38 */ void* FSTLocation;
    /* 0x3C */ u32 FSTMaxLength;
};

BOOL DVDReadAbsAsyncForBS(DVDCommandBlock* block, void* addr, s32 length, s32 offset,
                          DVDCBCallback callback);
BOOL DVDReadDiskID(DVDCommandBlock* block, DVDDiskID* diskID, DVDCBCallback callback);
void DVDReset(void);
s32 DVDGetDriveStatus(void);

#define DVD_STATE_FATAL_ERROR -1
#define DVD_STATE_END 0
#define DVD_STATE_BUSY 1
#define DVD_STATE_WAITING 2
#define DVD_STATE_COVER_CLOSED 3
#define DVD_STATE_NO_DISK 4
#define DVD_STATE_COVER_OPEN 5
#define DVD_STATE_MOTOR_STOPPED 7

static u8 bb2Buf[64];

static u32 status;
static DVDBB2* bb2;
static DVDDiskID* idTmp;

static void cb(s32 result, DVDCommandBlock* block);

static void cb(s32 result, DVDCommandBlock* block) {
    if (result > 0) {
        switch (status) {
        case 0:
            status = 1;
            DVDReadAbsAsyncForBS(block, bb2, 0x20, 0x420, cb);
            return;
        case 1:
            status = 2;
            DVDReadAbsAsyncForBS(block, bb2->FSTAddress, (bb2->FSTLength + 0x1F) & 0xFFFFFFE0,
                                 bb2->FSTPosition, cb);
        default:
            return;
        }
    }
    if (result == -1) {
        return;
    } else if (result == -4) {
        status = 0;
        DVDReset();
        DVDReadDiskID(block, idTmp, cb);
    }
}

void __fstLoad(void) {
    struct OSBootInfo_s* bootInfo;
    DVDDiskID* id;
    u8 idTmpBuf[63];
    s32 state;
    void* arenaHi;
    static DVDCommandBlock block;

    arenaHi = OSGetArenaHi();
    bootInfo = (void*)OSPhysicalToCached(0);
    idTmp = (void*)OSRoundUp32B(idTmpBuf);
    bb2 = (void*)OSRoundUp32B(bb2Buf);
    DVDReset();
    DVDReadDiskID(&block, idTmp, cb);

    while (1) {
        state = DVDGetDriveStatus();
        if (state == 0) {
            break;
        }

        switch (state) {
        case DVD_STATE_FATAL_ERROR:
            break;
        case DVD_STATE_BUSY:
            break;
        case DVD_STATE_WAITING:
            break;
        case DVD_STATE_COVER_CLOSED:
            break;
        case DVD_STATE_NO_DISK:
            break;
        case DVD_STATE_COVER_OPEN:
            break;
        case DVD_STATE_MOTOR_STOPPED:
            break;
        }
    }

    bootInfo->FSTLocation = (void*)bb2->FSTAddress;
    bootInfo->FSTMaxLength = bb2->FSTMaxLength;
    id = &bootInfo->DVDDiskID;
    memcpy(id, idTmp, 0x20);
    OSReport("\n");
    OSReport("  Game Name ... %c%c%c%c\n", id->gameName[0], id->gameName[1], id->gameName[2],
             id->gameName[3]);
    OSReport("  Company ..... %c%c\n", id->company[0], id->company[1]);
    OSReport("  Disk # ...... %d\n", id->diskNumber);
    OSReport("  Game ver .... %d\n", id->gameVersion);
    OSReport("  Streaming ... %s\n", !(id->streaming) ? "OFF" : "ON");
    OSReport("\n");
    OSSetArenaHi((void*)bb2->FSTAddress);
}
