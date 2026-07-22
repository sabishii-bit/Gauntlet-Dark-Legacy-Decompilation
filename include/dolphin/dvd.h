#ifndef DOLPHIN_DVD_H
#define DOLPHIN_DVD_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DVDDiskID {
    /* 0x00 */ char gameName[4];
    /* 0x04 */ char company[2];
    /* 0x06 */ u8 diskNumber;
    /* 0x07 */ u8 gameVersion;
    /* 0x08 */ u8 streaming;
    /* 0x09 */ u8 streamingBufSize;
    /* 0x0A */ u8 padding[22];
} DVDDiskID; // size 0x20

typedef struct DVDCommandBlock DVDCommandBlock;
typedef void (*DVDCBCallback)(s32 result, DVDCommandBlock* block);

struct DVDCommandBlock {
    /* 0x00 */ DVDCommandBlock* next;
    /* 0x04 */ DVDCommandBlock* prev;
    /* 0x08 */ u32 command;
    /* 0x0C */ s32 state;
    /* 0x10 */ u32 offset;
    /* 0x14 */ u32 length;
    /* 0x18 */ void* addr;
    /* 0x1C */ u32 currTransferSize;
    /* 0x20 */ u32 transferredSize;
    /* 0x24 */ DVDDiskID* id;
    /* 0x28 */ DVDCBCallback callback;
    /* 0x2C */ void* userData;
}; // size 0x30

typedef struct DVDFileInfo DVDFileInfo;
typedef void (*DVDCallback)(s32 result, DVDFileInfo* fileInfo);

struct DVDFileInfo {
    /* 0x00 */ DVDCommandBlock cb;
    /* 0x30 */ u32 startAddr;
    /* 0x34 */ u32 length;
    /* 0x38 */ DVDCallback callback;
}; // size 0x3C

typedef struct DVDBB2 {
    /* 0x00 */ u32 bootFilePosition;
    /* 0x04 */ u32 FSTPosition;
    /* 0x08 */ u32 FSTLength;
    /* 0x0C */ u32 FSTMaxLength;
    /* 0x10 */ void* FSTAddress;
    /* 0x14 */ u32 userPosition;
    /* 0x18 */ u32 userLength;
    /* 0x1C */ u32 padding0;
} DVDBB2; // size 0x20

typedef struct DVDDriveInfo {
    /* 0x00 */ u16 revisionLevel;
    /* 0x02 */ u16 deviceCode;
    /* 0x04 */ u32 releaseDate;
    /* 0x08 */ u8 padding[24];
} DVDDriveInfo; // size 0x20

typedef struct DVDDir {
    u32 entryNum;
    u32 location;
    u32 next;
} DVDDir;

typedef struct DVDDirEntry {
    u32 entryNum;
    BOOL isDir;
    char* name;
} DVDDirEntry;

typedef void (*DVDLowCallback)(u32 intType);

// dvdlow.c
void __DVDInitWA(void);
void __DVDInterruptHandler(s16 interrupt, void* context);
BOOL DVDLowRead(void* addr, u32 length, u32 offset, DVDLowCallback callback);
BOOL DVDLowSeek(u32 offset, DVDLowCallback callback);
BOOL DVDLowWaitCoverClose(DVDLowCallback callback);
BOOL DVDLowReadDiskID(DVDDiskID* diskID, DVDLowCallback callback);
BOOL DVDLowStopMotor(DVDLowCallback callback);
BOOL DVDLowRequestError(DVDLowCallback callback);
BOOL DVDLowInquiry(DVDDriveInfo* info, DVDLowCallback callback);
BOOL DVDLowAudioStream(u32 subcmd, u32 length, u32 offset, DVDLowCallback callback);
BOOL DVDLowRequestAudioStatus(u32 subcmd, DVDLowCallback callback);
BOOL DVDLowAudioBufferConfig(BOOL enable, u32 size, DVDLowCallback callback);
void DVDLowReset(void);
DVDLowCallback DVDLowSetResetCoverCallback(DVDLowCallback callback);
BOOL DVDLowBreak(void);
DVDLowCallback DVDLowClearCallback(void);
u32 DVDLowGetCoverStatus(void);
void __DVDLowSetWAType(u32 type, u32 seekLoc);

// dvdfs.c
void __DVDFSInit(void);
s32 DVDConvertPathToEntrynum(const char* pathPtr);
BOOL DVDFastOpen(s32 entrynum, DVDFileInfo* fileInfo);
BOOL DVDOpen(char* fileName, DVDFileInfo* fileInfo);
BOOL DVDClose(DVDFileInfo* fileInfo);
BOOL DVDGetCurrentDir(char* path, u32 maxlen);
BOOL DVDChangeDir(char* dirName);
BOOL DVDReadAsyncPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset,
                      DVDCallback callback, s32 prio);
s32 DVDReadPrio(DVDFileInfo* fileInfo, void* addr, s32 length, s32 offset, s32 prio);

// dvd.c
void DVDInit(void);
DVDDiskID* DVDGetCurrentDiskID(void);

#ifdef __cplusplus
}
#endif

#endif // DOLPHIN_DVD_H
