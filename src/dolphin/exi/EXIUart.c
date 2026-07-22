#include "types.h"
#include "dolphin/exi.h"

u32 OSGetConsoleType(void);

static s32 Chan;
static u32 Dev;
static u32 Enabled;
static u32 BarnacleEnabled;

u32 InitializeUART(u32 baudRate) {
    if (BarnacleEnabled == 0xA5FF005A) {
        return 0;
    }

    if (!(OSGetConsoleType() & 0x10000000)) {
        Enabled = 0;
        return 2;
    }

    Enabled = 0xA5FF005A;
    Chan = 0;
    Dev = 1;
    return 0;
}

u32 ReadUARTN(void* bytes, u32 length) {
    return 4;
}

static int QueueLength(void) {
    u32 cmd;

    if (EXISelect(Chan, Dev, EXI_FREQ_8M) == 0) {
        return -1;
    }
    cmd = 0x20010000;
    EXIImm(Chan, &cmd, 4, EXI_WRITE, NULL);
    EXISync(Chan);
    EXIImm(Chan, &cmd, 1, EXI_READ, NULL);
    EXISync(Chan);
    EXIDeselect(Chan);
    return 0x10 - (cmd >> 24);
}

u32 WriteUARTN(void* buf, u32 len) {
    u32 cmd;
    long xLen;
    int qLen;
    char* ptr;
    int locked;
    u32 error;

    if (Enabled != 0xA5FF005A) {
        return 2;
    }

    locked = EXILock(Chan, Dev, 0);
    if (locked == 0) {
        return 0;
    } else {
        ptr = (char*)buf;
    }

    while ((u32)ptr - (u32)buf < len) {
        if (*ptr == 0xA) {
            *ptr = 0xD;
        }
        ptr++;
    }

    error = 0;
    cmd = 0xA0010000;
    while (len) {
        qLen = QueueLength();
        if (qLen < 0) {
            error = 3;
            break;
        }
        if (qLen >= 12 || (u32)qLen >= len) {
            if (EXISelect(Chan, Dev, EXI_FREQ_8M) == 0) {
                error = 3;
                break;
            }
            EXIImm(Chan, &cmd, 4, EXI_WRITE, NULL);
            EXISync(Chan);
            while (qLen && len) {
                if (qLen < 4 && (u32)qLen < len) {
                    break;
                }
                xLen = (len < 4) ? (long)len : 4;
                EXIImm(Chan, buf, xLen, EXI_WRITE, NULL);
                (u8*)buf += xLen;
                len -= xLen;
                qLen -= xLen;
                EXISync(Chan);
            }
            EXIDeselect(Chan);
        }
    }
    EXIUnlock(Chan);
    return error;
}
