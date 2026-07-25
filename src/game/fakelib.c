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

void fn_8006B540(char* msg);   /* disc-error message display */
void fn_800DD604(void);        /* frame yield while waiting on DVD */
void fn_80042F98(void);        /* post-error recovery, gated by lbl_80344A5C */

extern u8 lbl_80344A5C;        /* error-screen-shown flag (other TU) */
extern u8 lbl_8025EDE8[];      /* shared scratch DVDFileInfo (.bss) */
extern u8 lbl_8028CB30[];      /* file slot table: records @+1440+i*88, buffers @+1528+i*16416 */
extern u8 lbl_80344DB8;        /* dvd-busy flag */
extern u8 lbl_80115860[];      /* disc error strings block */

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

/* --- misc PS2 kernel stubs (exact identities TBD) ------------------- */

void fn_800AEA54(void)
{
}

int fn_800AEA58(void)
{
    return 1;
}

int fn_800AEA60(void)
{
    return 1;
}

int fn_800AEA68(void)
{
    return 0;
}

int fn_800AEA70(void)
{
    return 0;
}

int fn_800AEA78(void)
{
    return 0;
}

int fn_800AEA80(int a, int b, int* out)
{
    *out = 0;
    return 0;
}

int fn_800AEA90(void)
{
    return 0;
}

/* --- file API -------------------------------------------------------- */

u8 sceFileExists(const char* path)
{
    return DVDConvertPathToEntrynum(path) >= 0;
}

/* 0x800AEAD0: file size probe - open with retry, take length, close */
int fn_800AEAD0(const char* path)
{
    u8 fi[0x3C]; /* DVDFileInfo */
    u8 bufo[64];
    u8 bufc[64];
    int off = (int) (((u32) &bufo[31] & ~31) - (u32) &bufo);
    int r;
    int size;

    do {
        r = DVDOpen(path, fi);
        if (r == 0) {
            fn_800AEBF4(lbl_8025EDE8, bufo + off, 32, 0);
        }
    } while (r == 0);
    size = *(u32*) (fi + 0x34); /* DVDFileInfo.length */
    off = (int) (((u32) &bufc[31] & ~31) - (u32) &bufc);
    do {
        r = DVDClose(fi);
        if (r == 0) {
            fn_800AEBF4(lbl_8025EDE8, bufc + off, 32, 0);
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

/* 0x800AEBF4: synchronous DVD read with disc-error UI (0x15C) */
int fn_800AEBF4(void* fileInfo, void* buf, int len, int offset)
{
    char* msg = 0;
    char* base = (char*) lbl_80115860;
    int status;

    lbl_80344A5C = 0;
    lbl_80344DB8 = 0;
    if (DVDReadAsyncPrio(fileInfo, buf, len, offset, 0, 2) == 0) {
        lbl_80344DB8 = 1;
        switch (DVDGetCommandBlockStatus(fileInfo)) {
        case -1:
            msg = base + 176;
            break;
        case 5:
            msg = base + 304;
            break;
        case 4:
        case 6:
            msg = base + 336;
            break;
        case 11:
            msg = base + 388;
            break;
        }
        if (msg != 0) {
            fn_8006B540(msg);
        }
    }
    do {
        status = DVDGetCommandBlockStatus(fileInfo);
        if (status != 3) {
            if (status >= 3 || status < 0) {
                char* msg2;

                lbl_80344DB8 = 1;
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
                    fn_8006B540(msg2);
                }
            }
        } else {
            lbl_80344DB8 = 1;
        }
        fn_800DD604();
    } while (status != 0);
    if (lbl_80344A5C != 0) {
        fn_80042F98();
    }
    return len;
}

/* 0x800AED50: buffered read through the 32-aligned cache window */
int sceRead(int fd, void* buf, int len)
{
    SCEFILE* f = SCEHANDLE(fd);
    u32 a = f->pos & ~31;
    s32 span = (((len + 31) + f->pos) & ~31) - a;
    s32 copied;

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
            if (fn_800AEBF4(f->fileInfo, f->buf + (f->bufOff + a - f->winOff), n,
                            f->cursor) != n) {
                return -1;
            }
        }
        span -= n;
        f->cursor += n;
    }

    copied = f->winOff + f->chunk - f->pos;
    if (len < copied) {
        copied = len;
    }
    memcpy(buf, f->buf + (f->bufOff + f->pos - f->winOff), copied);
    f->pos += copied;
    len -= copied;

    while (span > f->chunk) {
        f->winOff = f->cursor;
        if (fn_800AEBF4(f->fileInfo, f->buf + f->bufOff, f->chunk, f->cursor) !=
            f->chunk) {
            return -1;
        }
        f->cursor += f->chunk;
        f->pos += f->chunk;
        memcpy((u8*) buf + copied, f->buf + f->bufOff, f->chunk);
        span -= f->chunk;
        len -= f->chunk;
        copied += f->chunk;
    }

    if (span > 0) {
        f->winOff = f->cursor;
        if (fn_800AEBF4(f->fileInfo, f->buf + f->bufOff, span, f->cursor) != span) {
            return -1;
        }
        f->cursor += span;
        if (len > 0) {
            memcpy((u8*) buf + copied, f->buf + f->bufOff, len);
            copied += len;
            f->pos += len;
        }
    }
    return copied;
}

int sceClose(int fd)
{
    u8 buf[76];
    /* 32-byte-aligned offset into buf for the dummy retry read */
    int off = (int) (((u32) &buf[31] & ~31) - (u32) &buf[0]);
    SCEFILE* f = SCEHANDLE(fd);
    int r;

    do {
        r = DVDClose(f->fileInfo);
        if (r == 0) {
            /* PARKED 3-insn residual: target keeps off (subf) in r29 and
               rebuilds buf+off at the callsite; ours folds to the aligned
               pointer. Tried cast-transit, &buf[off], inlined helper. */
            fn_800AEBF4(lbl_8025EDE8, buf + off, 32, 0);
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

    base = lbl_8028CB30;
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

    off = (int) (((u32) &rbuf[31] & ~31) - (u32) &rbuf[0]);
    fi = f->fileInfo;
    do {
        r = DVDOpen(path, fi);
        if (r == 0) {
            fn_800AEBF4(lbl_8025EDE8, rbuf + off, 32, 0);
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

int fn_800AF1B8(void)
{
    return 0;
}

int fn_800AF1C0(void)
{
    return 0;
}

int fn_800AF1C8(void)
{
    return 0;
}

int fn_800AF1D0(void)
{
    return 0;
}

void fn_800AF1D8(void)
{
}

void fn_800AF1DC(void)
{
}

int fn_800AF1E0(void)
{
    return 0;
}

int fn_800AF1E8(void)
{
    return 0;
}

int fn_800AF1F0(void)
{
    return 1;
}

int fn_800AF1F8(void)
{
    return 0;
}

int fn_800AF200(void)
{
    return 0;
}

int fn_800AF208(void)
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

int fn_800AF24C(void)
{
    return 1;
}

int scePadSetActDirect(int port, int slot, u8* act)
{
    /* PARKED pool residual: target sdata2 = [s32-magic, 255.0f, u32-magic];
       ours pools 255.0f first regardless of source form (literal vs int vs
       cast-int divisor, temp order). 2 words swapped, text matches. */
    f32 on = (f32) (act[0] != 0);
    f32 str = (f32) act[1] / 255.0f;

    G3DSetRumble(slot + port * 4, on, str);
    return 1;
}

int scePadInfoMode(void)
{
    return 7;
}

int fn_800AF2D4(void)
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

int fn_800AF538(void)
{
    return 1;
}

void fn_800AF540(void)
{
}

int fn_800AF544(void)
{
    return 0;
}

int fn_800AF54C(void)
{
    return 0;
}

int fn_800AF554(void)
{
    return 1;
}

int fn_800AF55C(void)
{
    return 1;
}

void fn_800AF564(void)
{
}

void fn_800AF568(void)
{
}

void fn_800AF56C(void)
{
}

void fn_800AF570(void)
{
}
