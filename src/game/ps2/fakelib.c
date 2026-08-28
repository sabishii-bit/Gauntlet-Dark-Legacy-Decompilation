/*
 * fakelib.c - PS2 API compatibility shim, GCN edition (Xbox: fakelib.obj,
 * 583 entries; the GCN build keeps only what the game still calls).
 * File API backed by DVD*, pad API backed by G3D pad layer.
 */
#include "types.h"

s32 DVDConvertPathToEntrynum(const char* path);
int DVDClose(void* fileInfo);
int DVDReadAsyncPrio(void* fileInfo, void* buf, int len, int offset, void* cb, int prio);
int DVDOpen(const char* path, void* fileInfo);
int DVDGetCommandBlockStatus(void* block);

void ScrollMessageBox(char* msg);   /* disc-error message display (MESSAGE.OBJ) */
void sysHandleReset(void);        /* frame yield while waiting on DVD */
void sndTestStartAll(void);    /* post-error recovery after the error screen */

extern u8 gDiskErrorShown;       /* error-screen-shown flag (other TU) */
extern u8 gDvdScratchFileInfo[]; /* shared scratch DVDFileInfo (.bss) */
extern u8 sFileSlots[];          /* records @+1440+i*88, buffers @+1528+i*16416 */
extern u8 sDvdBusy;
extern u8 DiskErrorStr[];        /* Xbox PDB name */

u8 G3DIsPadEnabled(int idx);
void G3DGetControlPadAnalogStick(f32* x, f32* y, int pad, int stick);
f32 G3DGetControlPadAnalog(int pad, int axis);
u8 G3DControlPadButtonPressed(int pad, int button);

/* PS2 button bit per G3D button index */
static u32 sce_button_bits[16] = {
    0x8000, 0x2000, 0x1000, 0x4000, 0x0040, 0x0080, 0x0020, 0x0010,
    0x0200, 0x0008, 0x0000, 0x0000, 0x0000, 0x0000, 0x0800, 0x0400,
};
/* G3D button ids for the DualShock2 pressure bytes */
static u32 sce_pressure_ids[12] = { 1, 0, 2, 3, 7, 6, 4, 5, 10, 12, 11, 13 };
void G3DSetRumble(int idx, f32 small, f32 big);
int G3DGetActivePadCount(void);

/* PS2 file record, handle = (record pointer) >> 1:
 * +0x00 open flag (byte), +0x18 seek position,
 * +0x1C embedded DVDFileInfo (length lands at +0x50). */
typedef struct SCEFILE {
    u8 open;        /* 0x00 */
    u8 pad0[3];
    u8* buf;        /* 0x04 cache buffer */
    u32 bufOff;     /* 0x08 window offset inside buf */
    u32 chunk;      /* 0x0C window size */
    u32 winOff;     /* 0x10 window start (file offset) */
    u32 cursor;     /* 0x14 read cursor (file offset) */
    s32 pos;        /* 0x18 */
    u8 fileInfo[0x34]; /* 0x1C DVDFileInfo head */
    u32 size;       /* 0x50 (DVDFileInfo.length) */
    u8 rest[0x8];
} SCEFILE;

void* memcpy(void* dst, const void* src, u32 n);

#define SCEHANDLE(fd) ((SCEFILE*) ((fd) << 1))

/* unreferenced, stripped at link; its codegen pools the s32->f32 magic
 * double ahead of scePadSetActDirect's 255.0f literal (target pool order) */
static f32 sceIntToF32(s32 x)
{
    return (f32) x;
}

/* --- retained PS2 IOP/bootstrap API --------------------------------- */

void sceSifInitRpc(void)
{
}

int sceSifSyncIop(void)
{
    return 1;
}

int sceSifRebootIop(void)
{
    return 1;
}

int sceSifLoadFileReset(void)
{
    return 0;
}

int sceSifLoadModule(void)
{
    return 0;
}

int sceSifInitIopHeap(void)
{
    return 0;
}

int sceSifLoadElfPart(int a, int b, int* out)
{
    *out = 0;
    return 0;
}

int sceFsReset(void)
{
    return 0;
}

/* --- file API -------------------------------------------------------- */

u8 sceFileExists(const char* path)
{
    return DVDConvertPathToEntrynum(path) >= 0;
}

/* File size probe - open with retry, take length, close. */
int sceFileSize(const char* path)
{
    u8 fi[0x3C]; /* DVDFileInfo */
    u8 bufo[64];
    u8 bufc[64];
    int r;
    int size;

    do {
        r = DVDOpen(path, fi);
        if (r == 0) {
            int off = (int) (((u32) &bufo[31] & ~31) - (u32) &bufo);
            sDvdReadSync(gDvdScratchFileInfo, bufo + off, 32, 0);
        }
    } while (r == 0);
    size = *(u32*) (fi + 0x34); /* DVDFileInfo.length */
    do {
        r = DVDClose(fi);
        if (r == 0) {
            int off = (int) (((u32) &bufc[31] & ~31) - (u32) &bufc);
            sDvdReadSync(gDvdScratchFileInfo, bufc + off, 32, 0);
        }
    } while (r == 0);
    return size;
}

int sceLseek(int fd, int offset, int whence)
{
    SCEFILE* f = SCEHANDLE(fd);

    if (whence == 0) {
        f->pos = offset;
    } else if (whence == 1) {
        f->pos = f->pos + offset;
    } else {
        f->pos = f->size + offset;
    }
    if (f->pos >= 0) {
        if (f->pos <= f->size) {
            return f->pos;
        }
    }
    return -1;
}

int sceWrite(int fd, const void* buf, int len)
{
    return 0;
}

typedef struct SDvdMessageCarrier {
    char* message;
} SDvdMessageCarrier;

/* 0x800AEBF4: synchronous DVD read with disc-error UI (0x15C) */
int sDvdReadSync(void* fileInfo, void* buf, int len, int offset)
{
    SDvdMessageCarrier carrier;
    char* base = (char*) DiskErrorStr;
    int status;

    carrier.message = 0;
    gDiskErrorShown = (u32)carrier.message;
    sDvdBusy = (u32)carrier.message;
    if (DVDReadAsyncPrio(fileInfo, buf, len, offset, 0, 2) == 0) {
        sDvdBusy = 1;
        switch (DVDGetCommandBlockStatus(fileInfo)) {
        case -1:
            carrier.message = base + 176;
            break;
        case 5:
            carrier.message = base + 304;
            break;
        case 4:
        case 6:
            carrier.message = base + 336;
            break;
        case 11:
            carrier.message = base + 388;
            break;
        }
        if (carrier.message != 0) {
            ScrollMessageBox(carrier.message);
        }
    }
    do {
        status = DVDGetCommandBlockStatus(fileInfo);
        if (status == 3) {
            goto dvd_busy;
        }
        /* Keep the busy block between the range tests and error dispatch.
         * This is the source layout MWCC uses for the original block order. */
        if (status >= 3 || status < 0) {
            goto dvd_error;
        }
        goto dvd_done;

    dvd_busy:
        sDvdBusy = 1;
        goto dvd_done;

    dvd_error:
        {
            char* msg2;

            sDvdBusy = 1;
            msg2 = 0;
            switch (status) {
            case -1:
                msg2 = base + 176;
                break;
            case 5:
                msg2 = base + 304;
                break;
            case 4:
            case 6:
                msg2 = base + 336;
                break;
            case 11:
                msg2 = base + 388;
                break;
            }
            if (msg2 != 0) {
                ScrollMessageBox(msg2);
            }
        }

    dvd_done:
        sysHandleReset();
    } while (status != 0);
    if (gDiskErrorShown != 0) {
        sndTestStartAll();
    }
    return len;
}

/* 0x800AED50: buffered read through the 32-aligned cache window */
int sceRead(int fd, void* buf, int len)
{
    s32 span;
    s32 rem;
    s32 copied;
    SCEFILE* f;
    u32 a;

    f = SCEHANDLE(fd);
    rem = len;
    a = rem + 31;
    span = (f->pos + a) & ~31;
    a = f->pos & ~31;
    span = span - a;

    if (a < f->winOff || a >= f->winOff + f->chunk) {
        f->winOff = a;
    } else {
        span -= (s32) (f->cursor - a);
        a = f->cursor;
    }

    if (span > 0) {
        s32 space = f->winOff + f->chunk - a;
        s32 n = (span < space) ? span : space;

        if (a != f->cursor) {
            f->cursor = a;
        }
        if (n > 0) {
            if (sDvdReadSync(f->fileInfo, f->buf + (f->bufOff + a - f->winOff), n,
                            f->cursor) != n) {
                return -1;
            }
        }
        span -= n;
        f->cursor += n;
    }

    copied = f->winOff + f->chunk;
    copied = copied - f->pos;
    if (rem < copied) {
        copied = len;
    }
    memcpy(buf, f->buf + (f->bufOff + f->pos - f->winOff), copied);
    f->pos += copied;
    rem -= copied;

    while (span > f->chunk) {
        f->winOff = f->cursor;
        if (sDvdReadSync(f->fileInfo, f->buf + f->bufOff, f->chunk, f->cursor) !=
            f->chunk) {
            return -1;
        }
        f->cursor += f->chunk;
        f->pos += f->chunk;
        span -= f->chunk;
        memcpy((u8*) buf + copied, f->buf + f->bufOff, f->chunk);
        rem -= f->chunk;
        copied += f->chunk;
    }

    if (span > 0) {
        f->winOff = f->cursor;
        if (sDvdReadSync(f->fileInfo, f->buf + f->bufOff, span, f->cursor) != span) {
            return -1;
        }
        f->cursor += span;
        if (rem > 0) {
            memcpy((u8*) buf + copied, f->buf + f->bufOff, rem);
            copied += rem;
            f->pos += rem;
        }
    }
    return copied;
}

int sceClose(int fd)
{
    u8 buf[76];
    SCEFILE* f = SCEHANDLE(fd);
    int r;

    do {
        r = DVDClose(f->fileInfo);
        if (r == 0) {
            /* off declared INSIDE the loop body: MWCC's LICM hoists only the
               subf (aligned-base -> r29) and keeps the base+off add at the
               callsite inside the loop, matching Midway. Declaring off in the
               outer scope folds base+off -> aligned and hoists it whole. */
            int off = (int) (((u32) &buf[31] & ~31) - (u32) &buf[0]);
            sDvdReadSync(gDvdScratchFileInfo, buf + off, 32, 0);
        }
    } while (r == 0);
    f->open = 0;
    return 0;
}

/* 0x800AF02C: sceOpen(path, flags, ...) - open through the slot table,
 * handle is record-pointer >> 1 */
int sceOpen(const char* path, int flags, ...)
{
    SCEFILE* f;
    u8* base;
    int off;
    u8* fi;
    int i;
    int r;
    u8 rbuf[64];

    base = sFileSlots;
    if (!(flags & 1)) {
        return -1;
    }
    i = 0;
    if (*(base + 1440) != 0) {
        i = 1;
    }
    if (i == 1) {
        return -1;
    }
    f = (SCEFILE*) (base + 1440 + i * 88);
    if (*(u32*) &f & 1) {
        return -1;
    }
    f->buf = base + 1528 + i * 16416;

    fi = f->fileInfo;
    do {
        r = DVDOpen(path, fi);
        if (r == 0) {
            off = (int) (((u32) &rbuf[31] & ~31) - (u32) &rbuf[0]);
            sDvdReadSync(gDvdScratchFileInfo, rbuf + off, 32, 0);
        }
    } while (r == 0);

    f->pad0[0] = 1;
    f->open = 1;
    f->bufOff = (((u32) f->buf + 31) & ~31) - (u32) f->buf;
    f->chunk = 16384;
    f->pos = 0;
    f->cursor = 0;
    f->winOff = 0;
    return (u32) f >> 1;
}

/* --- pad API ---------------------------------------------------------- */

int WaitSema(int sema)
{
    return 0;
}

int SignalSema(int sema)
{
    return 0;
}

int CreateSema(void* param)
{
    return 0;
}

int GetThreadId(void)
{
    return 0;
}

void FlushCache(void)
{
}

void sceGsSyncPath(void)
{
}

int DIntr(void)
{
    return 0;
}

int EIntr(void)
{
    return 0;
}

int scePadEnterPressMode(void)
{
    return 1;
}

int scePadInfoPressMode(void)
{
    return 0;
}

int scePadSetMainMode(void)
{
    return 0;
}

int scePadGetReqState(void)
{
    return 0;
}

int scePadGetState(int port, int slot)
{
    if (G3DIsPadEnabled(slot + port * 4)) {
        return 6;
    }
    return 0;
}

int scePadSetActAlign(void)
{
    return 1;
}

int scePadSetActDirect(int port, int slot, u8* act)
{
    f32 on = (f32) (act[0] != 0);
    f32 str = (f32) act[1] / 255.0f;

    G3DSetRumble(slot + port * 4, on, str);
    return 1;
}

int scePadInfoMode(void)
{
    return 7;
}

int scePadInfoAct(void)
{
    return 0;
}

/* 0x800AF2DC: scePadRead - build a PS2 DualShock2 report from G3D pad state */
int scePadRead(int port, int slot, u8* data)
{
    s32 bits = 0;
    int i;
    int idx = slot + port * 4;
    f32 x, y;

    data[0] = 0;
    data[1] = 116;
    for (i = 0; i < 16; i++) {
        if (G3DControlPadButtonPressed(idx, i)) {
            bits |= sce_button_bits[i];
        }
    }
    data[2] = ~bits;
    data[3] = ~(bits >> 8);
    G3DGetControlPadAnalogStick(&x, &y, idx, 0);
    data[4] = (s32) (-127.0f * G3DGetControlPadAnalog(idx, 2) + 128.0f);
    data[5] = (s32) (-127.0f * G3DGetControlPadAnalog(idx, 3) + 128.0f);
    data[6] = (s32) (127.0f * x + 128.0f);
    data[7] = (s32) (-127.0f * y + 128.0f);
    for (i = 8; i < 20; i++) {
        data[i] = G3DControlPadButtonPressed(idx, sce_pressure_ids[i - 8]) ? 255 : 0;
    }
    return 32;
}

int scePadPortOpen(int port, int slot, void* data)
{
    if (slot + port * 4 < G3DGetActivePadCount()) {
        return 1;
    }
    return 0;
}

int scePadPortClose(void)
{
    return 1;
}

int sceMtapPortOpen(int port)
{
    if (port * 4 < G3DGetActivePadCount()) {
        return 1;
    }
    return 0;
}

int sceMtapGetConnection(int port)
{
    if (port * 4 < G3DGetActivePadCount()) {
        return 1;
    }
    return 0;
}

/* --- tail stubs -------------------------------------------------------- */

int sceMtapInit(void)
{
    return 1;
}

void sceMtapPortClose(void)
{
}

int sceGsExecLoadImage(void)
{
    return 0;
}

int sceGsSetDefLoadImage(void)
{
    return 0;
}

int Deci2Call(void)
{
    return 1;
}

int sceGsSyncV(void)
{
    return 1;
}

void sceGsSwapDBuff(void)
{
}

void sceGsSetDefDBuff(void)
{
}

void sceGsResetPath(void)
{
}

void sceGsResetGraph(void)
{
}
