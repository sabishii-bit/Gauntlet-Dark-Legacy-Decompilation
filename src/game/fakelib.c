/*
 * fakelib.c - PS2 API compatibility shim, GCN edition (Xbox: fakelib.obj,
 * 583 entries; the GCN build keeps only what the game still calls).
 * File API backed by DVD*, pad API backed by G3D pad layer.
 */
#include "types.h"

s32 DVDConvertPathToEntrynum(const char* path);
int DVDClose(void* fileInfo);

u8 G3DIsPadEnabled(int idx);
void G3DSetRumble(int idx, f32 small, f32 big);
int G3DGetActivePadCount(void);

/* PS2 file record, handle = (record pointer) >> 1:
 * +0x00 open flag (byte), +0x18 seek position,
 * +0x1C embedded DVDFileInfo (length lands at +0x50). */
typedef struct SCEFILE {
    u8 open;        /* 0x00 */
    u8 pad[0x17];
    s32 pos;        /* 0x18 */
    u8 fileInfo[0x34]; /* 0x1C DVDFileInfo head */
    u32 size;       /* 0x50 (DVDFileInfo.length) */
    u8 rest[0x8];
} SCEFILE;

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

/* 0x800AEAD0: DVDOpen + retry, then aligned read (0xC4) - TODO */
int fn_800AEAD0(const char* path)
{
    return 0;
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

/* 0x800AEBF4: synchronous DVD read with disc-error UI (0x15C) - TODO */
int fn_800AEBF4(void* fileInfo, void* buf, int len, int offset)
{
    return 0;
}

/* 0x800AED50: sceRead (0x260) - TODO */
int sceRead(int fd, void* buf, int len)
{
    return 0;
}

int sceClose(int fd)
{
    SCEFILE* f = SCEHANDLE(fd);

    while (DVDClose(f->fileInfo) == 0) {
    }
    f->open = 0;
    return 0;
}

/* 0x800AF02C: sceOpen(path, flags, ...) varargs (0x18C) - TODO */
int sceOpen(const char* path, int flags, ...)
{
    return -1;
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
    G3DSetRumble(slot + port * 4, (f32) (act[0] != 0), (f32) act[1] / 255.0f);
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

/* 0x800AF2DC: scePadRead (0x180) - TODO */
int scePadRead(int port, int slot, void* data)
{
    return 0;
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
