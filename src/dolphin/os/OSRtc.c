#include "types.h"

typedef struct OSSram {
    /* 0x00 */ u16 checkSum;
    /* 0x02 */ u16 checkSumInv;
    /* 0x04 */ u32 ead0;
    /* 0x08 */ u32 ead1;
    /* 0x0C */ u32 counterBias;
    /* 0x10 */ s8 displayOffsetH;
    /* 0x11 */ u8 ntd;
    /* 0x12 */ u8 language;
    /* 0x13 */ u8 flags;
} OSSram; // size 0x14

typedef struct OSSramEx {
    /* 0x00 */ u8 flashID[2][12];
    /* 0x18 */ u32 wirelessKeyboardID;
    /* 0x1C */ u16 wirelessPadID[4];
    /* 0x24 */ u8 dvdErrorCode;
    /* 0x25 */ u8 _padding0;
    /* 0x26 */ u8 flashIDChecksum[2];
    /* 0x28 */ u32 __padding1[2];
} OSSramEx; // size 0x2C

struct SramControl {
    /* 0x00 */ u8 sram[0x40];
    /* 0x40 */ u32 offset;
    /* 0x44 */ int enabled;
    /* 0x48 */ int locked;
    /* 0x4C */ int sync;
    /* 0x50 */ void (*callback)(void);
};

int OSDisableInterrupts(void);
int OSRestoreInterrupts(int level);
void DCInvalidateRange(void* addr, u32 nBytes);

int EXILock(long chan, unsigned long dev, void (*callback)());
int EXIUnlock(long chan);
int EXISelect(long chan, unsigned long dev, unsigned long freq);
int EXIDeselect(long chan);
int EXISync(long chan);
int EXIImm(long chan, void* buf, long len, unsigned long type, void (*callback)());
int EXIImmEx(long chan, void* buf, long len, unsigned long type);
int EXIDma(long chan, void* buf, long len, unsigned long type, void (*callback)());

static struct SramControl Scb __attribute__((aligned(32)));

static int ReadSram(void* buffer);
static void WriteSramCallback();
static int WriteSram(void* buffer, unsigned long offset, unsigned long size);
static void* LockSram(unsigned long offset);
static int UnlockSram(int commit, unsigned long offset);

static int ReadSram(void* buffer) {
    int err;
    unsigned long cmd;

    DCInvalidateRange(buffer, 0x40);
    if (!EXILock(0, 1, NULL)) {
        return 0;
    }
    if (!EXISelect(0, 1, 3)) {
        EXIUnlock(0);
        return 0;
    }
    cmd = 0x20000100;
    err = 0;
    err |= !EXIImm(0, &cmd, 4, 1, 0);
    err |= !EXISync(0);
    err |= !EXIDma(0, buffer, 0x40, 0, NULL);
    err |= !EXISync(0);
    err |= !EXIDeselect(0);
    EXIUnlock(0);
    return !err;
}

static void WriteSramCallback() {
    int unused;

    Scb.sync = WriteSram(&Scb.sram[Scb.offset], Scb.offset, 0x40 - Scb.offset);
    if (Scb.sync != 0) {
        Scb.offset = 0x40;
    }
}

static int WriteSram(void* buffer, unsigned long offset, unsigned long size) {
    int err;
    unsigned long cmd;

    if (!EXILock(0, 1, WriteSramCallback)) {
        return 0;
    }
    if (!EXISelect(0, 1, 3)) {
        EXIUnlock(0);
        return 0;
    }
    offset <<= 6;
    cmd = ((offset + 0x100) | 0xA0000000);
    err = 0;
    err |= !EXIImm(0, &cmd, 4, 1, 0);
    err |= !EXISync(0);
    err |= !EXIImmEx(0, buffer, size, 1);
    err |= !EXIDeselect(0);
    EXIUnlock(0);
    return !err;
}

void __OSInitSram() {
    Scb.locked = Scb.enabled = 0;
    Scb.sync = ReadSram(&Scb);
    Scb.offset = 0x40;
}

static void* LockSram(unsigned long offset) {
    int enabled;

    enabled = OSDisableInterrupts();
    if (Scb.locked) {
        OSRestoreInterrupts(enabled);
        return NULL;
    }
    Scb.enabled = enabled;
    Scb.locked = 1;
    return &Scb.sram[offset];
}

struct OSSram* __OSLockSram() {
    return (struct OSSram*)LockSram(0);
}

struct OSSramEx* __OSLockSramEx(void) {
    return (struct OSSramEx*)LockSram(0x14);
}

static int UnlockSram(int commit, unsigned long offset) {
    unsigned short* p;

    if (commit != 0) {
        if (offset == 0) {
            u8* sram = Scb.sram;

            if ((sram[0x13] & 3) > 2U) {
                sram[0x13] &= 0xFFFFFFFC;
            }

            {
                struct OSSram* sram = (struct OSSram*)&Scb.sram[0];
                sram->checkSum = sram->checkSumInv = 0;
                for (p = (unsigned short*)&sram->counterBias;
                     p < ((u16*)&Scb.sram[sizeof(struct OSSram)]); p++) {
                    sram->checkSum += *p;
                    sram->checkSumInv += ~(*p);
                }
            }
        }
        if (offset < Scb.offset) {
            Scb.offset = offset;
        }
        Scb.sync = WriteSram(&Scb.sram[Scb.offset], Scb.offset, 0x40 - Scb.offset);
        if (Scb.sync != 0) {
            Scb.offset = 0x40;
        }
    }
    Scb.locked = 0;
    OSRestoreInterrupts(Scb.enabled);
    return Scb.sync;
}

int __OSUnlockSram(int commit) {
    UnlockSram(commit, 0);
}

int __OSUnlockSramEx(int commit) {
    UnlockSram(commit, 0x14);
}

int __OSSyncSram() {
    return Scb.sync;
}

unsigned long OSGetSoundMode() {
    struct OSSram* sram = __OSLockSram();
    unsigned long mode = (sram->flags & 4) ? 1 : 0;

    __OSUnlockSram(0);
    return mode;
}

void OSSetSoundMode(unsigned long mode) {
    struct OSSram* sram;
    int unused;

    mode *= 4;
    mode &= 4;
    sram = __OSLockSram();
    if (mode == (sram->flags & 4)) {
        __OSUnlockSram(0);
        return;
    }
    sram->flags &= 0xFFFFFFFB;
    sram->flags |= mode;
    __OSUnlockSram(1);
}

u16 OSGetWirelessID(s32 chan) {
    u16* var_r3;
    u16 id;
    u8 _[4];

    var_r3 = LockSram(0x14);
    id = var_r3[chan + 0xE];
    UnlockSram(0, 0x14);
    return id;
}

void OSSetWirelessID(long chan, u16 id) {
    struct OSSramEx* sram = __OSLockSramEx();

    if (sram->wirelessPadID[chan] != id) {
        sram->wirelessPadID[chan] = id;
        __OSUnlockSramEx(1);
        return;
    }
    __OSUnlockSramEx(0);
}
