#include "types.h"

/* Midway library memory + async file system (Xbox ML_MEM.OBJ).
 *
 * Two layers in one TU:
 *   1. A bump/stack allocator carved out of one big OS heap block:
 *      - low allocations grow  mlmMemUsed  upward   (AllocMem/AllocMem32/AllocFile)
 *      - high allocations grow mlmMemLimit downward (AllocHiMem)
 *      - a lock stack lets callers pin/unwind the low watermark (LockMem/...)
 *   2. An async file reader with WAD-directory + zlib(uncompress) support,
 *      serviced a chunk at a time by serve_io()/do_threaded_io().
 *
 * Real Xbox-PDB names used where confirmed by error strings + behaviour
 * (AllocMem/AllocMem32/GetMemBase/ResetAllocTot/AllocFile/FileSize/FileExists/
 * get_path/MBSetupWad/MBGetFromWad/StartFileRead/InitMemHandler/BytesFree).
 * Wired NonMatching: the allocator core is decompiled faithfully; the file
 * layer is a structurally-faithful reconstruction (byte-match not attempted). */

/* ---- OS / heap ---- */
extern s32 DemoHeap;
extern void* OSAllocFromHeap(s32 heap, u32 size);
extern int OSCheckHeap(s32 heap);
extern void* memset(void* dst, int c, u32 n);

/* ---- diagnostics (ML_ERROR / PB_ERROR, other TUs) ---- */
extern void bulletproof_printf(char* fmt, ...);
extern void FatalError(char* fmt, int code);
extern void FatalErrorf(const char* fmt, ...);
extern int gErrorCode;

/* ---- CRT / MSL string + printf (other TUs) ---- */
extern int sprintf(char* buf, const char* fmt, ...);
extern u32 strlen(const char* s);
extern char* strcpy(char* d, const char* s);
extern char* strncpy(char* d, const char* s, u32 n);
extern char* strcat(char* d, const char* s);
extern char* strncat(char* d, const char* s, u32 n);
extern char* strrchr(const char* s, int c);

/* ---- PS2-style file shim (game/ps2/fakelib.c) ---- */
extern int sceOpen(const char* path, int flags);
extern int sceRead(int fd, void* buf, int len);
extern int sceReadAsync(int fd, int arg, int* status);
extern int sceLseek(int fd, int off, int whence);
extern int sceClose(int fd);
extern int sceFileSize(const char* path);
extern int sceFileExists(const char* path);

/* ---- zlib ---- */
extern int uncompress(void* dest, int* destLen, void* src, int srcLen);

/* ================= allocator state (.sbss / .bss) ================= */
extern int alloctot;      /* bytes taken from the high pool             */
extern int mlmLockSave;   /* saved low-watermark (lock bookkeeping)     */
extern int mlmLockSaveTop;
extern int mlmMemReserved; /* nonzero => alloc calls are illegal        */
extern int mlmMemLimit;   /* high boundary; hi-alloc decrements it      */
extern int mlmMemUsed;    /* low watermark; lo-alloc increments it      */
extern u8* mlmMemBase;    /* base of the managed block                  */
extern int mlmLockStack[8];
extern int gLowMemMode;   /* selects a smaller managed block            */

/* ================= file-system state ================= */
typedef struct MLFILE {
    /* 0x000 */ int state;      /* -1 free, 0 reading, 1 done            */
    /* 0x004 */ void* buffer;
    /* 0x008 */ int unk08;
    /* 0x00C */ int bytesRead;
    /* 0x010 */ int done;
    /* 0x014 */ int active;
    /* 0x018 */ int totalSize;
    /* 0x01C */ int fd;
    /* 0x020 */ char name[256];
    /* 0x120 */ int compressed;
    /* 0x124 */ void* compSrc;
    /* 0x128 */ int compSize;
} MLFILE; /* 0x12C */

extern MLFILE finfo_list[1];
extern MLFILE temp_finfo;
extern int mlmCurFileSlot;
extern int mlmServeTimeout;
extern int mlmCloseRes;
extern int mlmReadRes;
extern int mlmLastFileSize;

extern char mlmRootPath[];
extern const char mlmPathFmtWad[6]; /* "%s/%s" */
extern const char mlmPathFmt[3];    /* "%s"    */
extern const char mlmExtDefault[5]; /* ".ps2"  */
extern const char mlmPathSeparator[2];
extern const char mlmGameSubdirectory[]; /* "/gauntlet/" */

/* forward decls (address order kept) */
int do_threaded_io(MLFILE* f);
void get_path(char* out, char* wad, char* name);
int xReadFileSection(char* wad, char* name, int maxLen, void* dest);

/* ============================================================== *
 *  file layer                                                     *
 * ============================================================== */

void FreeHiMem(void)
{
}

#pragma opt_common_subs off
void serve_io(void)
{
    int i;
    int served;

    i = 0;
    served = 0;
    while (i++ < 1 && served == 0) {
        if (finfo_list[mlmCurFileSlot].done != -1 &&
            finfo_list[mlmCurFileSlot].done != 1) {
            served = do_threaded_io(&finfo_list[mlmCurFileSlot]);
        }
        if (++mlmCurFileSlot >= 1) {
            mlmCurFileSlot = 0;
        }
    }
}
#pragma opt_common_subs reset

/* WAD directory lookup: {count@+4, entries@+8}, entry = {key,ofs,size,pad} */
int MBGetFromWad(int* wad, int key, int* sizeOut)
{
    int* result;
    int offset;
    int* entry;
    u32 n;

    result = NULL;
    if (wad == NULL) {
        goto done;
    }
    if (wad != NULL) {
        goto search;
    }
    entry = NULL;
    goto assign;

search:
    offset = 0;
    for (n = wad[1]; n > 0; n--) {
        entry = (int*)(wad[2] + offset);
        if ((u32)key != (u32)*entry) {
            goto next;
        }
        goto assign;
next:
        offset += 0x10;
    }
    entry = NULL;

assign:
    result = entry;
done:
    if (result == NULL) {
        if (sizeOut != NULL) {
            *sizeOut = 0;
        }
        return 0;
    }
    if (sizeOut != NULL) {
        *sizeOut = result[2];
    }
    return result[1];
}

/* byte-swap a just-loaded WAD directory in place */
int MBSetupWad(int* wad, int base)
{
    u8* p;
    u32 v;
    u32 count;
    u32 i;

    if (wad == NULL) {
        return 0;
    }
    *wad = base;
    p = (u8*)*wad;
    wad[2] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    p = (u8*)(*wad + 4);
    wad[1] = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
    if ((u32)wad[2] >= (u32)mlmMemBase) {
        return 0;
    }
    wad[2] += base;
    p = (u8*)*wad;
    v = wad[2];
    p[0] = (u8)(v >> 24); p[1] = (u8)(v >> 16); p[2] = (u8)(v >> 8); p[3] = (u8)v;
    count = wad[1];
    for (i = 0; i < count; i++) {
        int* e = (int*)(wad[2] + i * 0x10);
        u8* q = (u8*)e;
        v = e[0]; q[0] = (u8)(v >> 24); q[1] = (u8)(v >> 16); q[2] = (u8)(v >> 8); q[3] = (u8)v;
        q = (u8*)&e[1];
        v = e[1]; q[0] = (u8)(v >> 24); q[1] = (u8)(v >> 16); q[2] = (u8)(v >> 8); q[3] = (u8)v;
        q = (u8*)&e[2];
        v = e[2]; q[0] = (u8)(v >> 24); q[1] = (u8)(v >> 16); q[2] = (u8)(v >> 8); q[3] = (u8)v;
        e[1] += base;
    }
    return 1;
}

/* open a file into the async handle slot */
MLFILE* StartFileRead(char* wad, char* name, int compress, char* dest)
{
    char path[256];
    char full[256];
    MLFILE* f;
    int slot;
    int fd;
    int size;

    if (alloctot != 0) {
        gErrorCode = 0xff;
        bulletproof_printf("Temporary high memory in use by %s\n", name);
    }
    slot = (finfo_list[0].state != -1);
    if (slot == 1) {
        gErrorCode = 0xff;
        bulletproof_printf("Too many open files (%d)\n", 1);
        return NULL;
    }
    f = &finfo_list[slot];
    get_path(path, wad, name);
    strcpy(full, path);
    fd = sceOpen(full, 1);
    if (fd < 0) {
        gErrorCode = 0xff;
        bulletproof_printf("Can't open '%s'\n", full);
        return NULL;
    }
    size = sceLseek(fd, 0, 2);
    if (size & 0xf) {
        size += 0x10 - (size & 0xf);
    }
    strncpy(f->name, full, 0x100);
    f->totalSize = size;
    sceLseek(fd, 0, 0);
    f->buffer = dest;
    f->bytesRead = 0;
    f->done = 0;
    f->fd = fd;
    f->state = 0;
    return f;
}

/* serve one chunk of an open file; decompress on completion */
int do_threaded_io(MLFILE* f)
{
    int status;
    u32 chunk;
    int r;
    void* buf;

    mlmReadRes = sceReadAsync(f->fd, 1, &status);
    if (status != 0) {
        return 1;
    }
    if (f->bytesRead < f->totalSize) {
        chunk = f->totalSize - f->bytesRead;
        if ((int)chunk > 0x8000) {
            chunk = 0x8000;
        }
        if (chunk & 0xf) {
            chunk += 0x10 - (chunk & 0xf);
        }
        buf = f->compressed ? f->compSrc : f->buffer;
        f->active = 1;
        r = sceRead(f->fd, (char*)buf + f->bytesRead, chunk);
        if (r >= 0) {
            f->bytesRead += chunk;
        }
        return 0;
    }
    if (f->compressed) {
        int destLen = f->compSize;
        r = uncompress(f->buffer, &destLen, f->compSrc, f->totalSize);
        if (r != 0) {
            gErrorCode = 0x80;
            bulletproof_printf("Error decompressing file. Can not continue\n");
        }
    }
    mlmServeTimeout = 0;
    while (1) {
        mlmReadRes = sceReadAsync(f->fd, 1, &status);
        if (status == 0) {
            break;
        }
        mlmServeTimeout++;
        if (mlmServeTimeout > 1500000000) {
            gErrorCode = 0xa0;
            bulletproof_printf("Timeout serving file %s (1)\n", f->name);
        }
    }
    mlmCloseRes = sceClose(f->fd);
    mlmMemLimit += alloctot;
    alloctot = 0;
    f->done = 1;
    return 0;
}

/* ============================================================== *
 *  allocator core (decompiled)                                    *
 * ============================================================== */

void InitMemHandler(void)
{
    int base;
    int size;

    mlmMemLimit = 0x1c88000;
    if (gLowMemMode != 0) {
        mlmMemLimit = 0x1b88000;
    }
    base = OSCheckHeap(DemoHeap);
    mlmMemLimit = base - 0x1000;
    mlmMemBase = OSAllocFromHeap(DemoHeap, mlmMemLimit);
    while (mlmMemBase == NULL) {
        mlmMemLimit -= 0x4000;
        mlmMemBase = OSAllocFromHeap(DemoHeap, mlmMemLimit);
    }
    size = mlmMemLimit;
    memset(mlmMemBase, 0, mlmMemLimit);
    bulletproof_printf("Available Memory = %d\n", size);
    mlmMemUsed = 0;
    mlmMemBase = (u8*)(((u32)mlmMemBase + 0x3f) & 0xffffffc0);
    mlmMemLimit = (mlmMemLimit & 0xffffffc0) - 0x40;
    {
        int i;
        for (i = 0; i < 8; i++) {
            mlmLockStack[i] = 0;
        }
    }
    finfo_list[0].state = -1;
    alloctot = 0;
}

int FileSystemReading(void)
{
    int reading = 0;

    if (finfo_list[0].done == 0)
        reading = 1;
    return reading;
}

int FileSystemBusy(void)
{
    int busy = 0;

    if (finfo_list[0].done != -1)
        busy = 1;
    return busy;
}

void LockMem(int slot)
{
    if (slot >= 8) {
        FatalError("Too Many Mem locks", 0x800000);
    }
    mlmLockStack[slot] = mlmMemUsed;
    mlmLockSaveTop = mlmLockSave;
}

void FreeUnlockedMem(int slot)
{
    int i;

    mlmMemUsed = mlmLockStack[slot];
    for (i = slot + 1; i < 8; i++) {
        mlmLockStack[i] = 0;
    }
    mlmLockSave = mlmLockSaveTop;
}

int BytesFree(void)
{
    return mlmMemLimit - mlmMemUsed;
}

void* GetMemBase(void)
{
    if (mlmMemReserved != 0) {
        gErrorCode = 0xe0e000;
        FatalErrorf("GetMemBase() called while mem reserved\n");
    }
    return mlmMemBase + (mlmMemUsed / 4) * 4;
}

void* AllocMem(u32 size)
{
    void* result;

    if (mlmMemReserved != 0) {
        gErrorCode = 0xa0a000;
        FatalErrorf("AllocMem() called while mem reserved\n");
    }
    if (size & 0xf) {
        size += 0x10 - (size & 0xf);
    }
    result = mlmMemBase + (mlmMemUsed / 4) * 4;
    mlmMemUsed += size;
    if (mlmMemUsed > mlmMemLimit) {
        gErrorCode = 0xc0c000;
        FatalErrorf("AllocMem failed %d bytes (exceeds by %d)\n",
                    size, mlmMemUsed - mlmMemLimit);
    }
    return result;
}

void* AllocMem32(int size)
{
    u8 unused[8];
    u32 aligned;
    int pad;
    void* result;

    aligned = (mlmMemUsed + 0x1f) & 0xffffffe0;
    pad = aligned - mlmMemUsed;
    size += pad;
    if (mlmMemLimit - mlmMemUsed < size) {
        return NULL;
    }
    if (mlmMemReserved != 0) {
        gErrorCode = 0xa0a000;
        FatalErrorf("AllocMem() called while mem reserved\n");
    }
    if (size & 0xf) {
        size += 0x10 - (size & 0xf);
    }
    result = mlmMemBase + (mlmMemUsed / 4) * 4;
    mlmMemUsed += size;
    if (mlmMemUsed > mlmMemLimit) {
        gErrorCode = 0xc0c000;
        FatalErrorf("AllocMem failed %d bytes (exceeds by %d)\n",
                    size, mlmMemUsed - mlmMemLimit);
    }
    return (u8*)result + pad;
}

void* AllocHiMem(u32 size)
{
    u32 tmp;
    u32 result;

    if (mlmMemReserved != 0) {
        gErrorCode = 0x808000;
        FatalErrorf("AllocMem() called while mem reserved\n");
    }
    if (size & 0xf) {
        size += 0x10 - (size & 0xf);
    }
    if ((int)(mlmMemUsed + size) > mlmMemLimit) {
        gErrorCode = 0x909000;
        FatalErrorf("AllocHiMem failed %d bytes (exceeds by %d)\n",
                    size, (mlmMemUsed + size) - mlmMemLimit);
    }
    tmp = ((u32)mlmMemBase + mlmMemLimit) - size;
    result = tmp & 0xffffff80;
    size += tmp - result;
    mlmMemLimit -= size;
    alloctot += size;
    return (void*)result;
}

int GetHiMemCacheTop(void)
{
    return (int)mlmMemBase + mlmMemLimit;
}

void ResetAllocTot(void)
{
    mlmMemLimit += alloctot;
    alloctot = 0;
}

/* ============================================================== *
 *  path + file helpers                                            *
 * ============================================================== */

void get_path(char* out, char* wad, char* name)
{
    char tmp[256];

    if (wad != NULL) {
        sprintf(tmp, mlmPathFmtWad, wad, name);
    } else {
        sprintf(tmp, mlmPathFmt, name);
    }
    if (!(name[0] == 'W' && name[1] == 'A' && name[2] == 'D') &&
        strrchr(tmp, '.') == NULL) {
        strcat(tmp, mlmExtDefault);
    }
    strcpy(out, mlmRootPath);
    if (tmp[0] != '/') {
        strcat(out, mlmGameSubdirectory);
    }
    strcat(out, tmp);
}

#pragma dont_inline on
int FileMap(char* wad, char* name, char* dst, s32 n, u32* handle, s32* sizeOut)
{
    char full[260];
    s32 size;

    *handle = 0;
    get_path(full, wad, name);
    size = sceFileSize(full);
    if (size & 0xF) {
        size += 0x10 - (size & 0xF);
    }
    *sizeOut = size;
    if (*sizeOut > 0) {
        strncpy(dst, wad, n);
        n = n - strlen(wad);
        if (n > 0) {
            strcat(dst, mlmPathSeparator);
            if (n - 1 > 0) {
                strncat(dst, name, n - 1);
            }
        }
        return 1;
    }
    return 0;
}
#pragma dont_inline off

int FileSize(char* wad, char* name)
{
    u8 unused[8];
    char full[256];
    u32 size;

    get_path(full, wad, name);
    size = sceFileSize(full);
    if (size & 0xf) {
        size += 0x10 - (size & 0xf);
    }
    return size;
}

int FileExists(char* wad, char* name)
{
    char full[256];

    get_path(full, wad, name);
    return sceFileExists(full) & 0xff;
}

void* AllocFile(char* wad, char* name)
{
    void* dest;
    int avail;
    int read;
    int used0;

    if (mlmMemReserved != 0) {
        gErrorCode = 0xe0e000;
        bulletproof_printf("GetMemBase() called while mem reserved\n");
    }
    dest = mlmMemBase + (mlmMemUsed >> 2) * 4;
    avail = mlmMemLimit - mlmMemUsed;
    used0 = mlmMemUsed;
    read = xReadFileSection(wad, name, avail, dest);
    if (avail > 0 && avail < read) {
        gErrorCode = 0x80;
        bulletproof_printf("File read overflowed %s size %d\n", name, read);
    }
    if (read < 0) {
        gErrorCode = 0xff;
        bulletproof_printf("AllocFile: Read failed\n");
    }
    if (read & 0xf) {
        read += 0x10 - (read & 0xf);
    }
    mlmMemUsed += read;
    if (mlmMemLimit < mlmMemUsed) {
        gErrorCode = 0xc0c000;
        bulletproof_printf("AllocMem failed %d bytes (exceeds by %d)\n",
                           read, mlmMemUsed - mlmMemLimit);
    }
    bulletproof_printf("-----ALLOC FILE %s %s MEM %06dk\n", wad, name, used0 >> 10);
    mlmLastFileSize = read;
    return dest;
}

int MLMReadFile(char* wad, char* name, int maxLen, void* dest)
{
    int read;

    read = xReadFileSection(wad, name, maxLen, dest);
    if (maxLen > 0 && read > maxLen) {
        gErrorCode = 0x80;
        FatalErrorf("File read overflowed: %s size:%d max:%d",
                    temp_finfo.name, read, maxLen);
    }
    return read;
}

int xReadFileSection(char* wad, char* name, int maxLen, void* dest)
{
    char full[256];
    int fd;
    int size;
    int read;

    get_path(full, wad, name);
    fd = sceOpen(full, 1);
    if (fd < 0) {
        bulletproof_printf("Can't load '%s' failed on open\n", full);
        return -1;
    }
    size = sceLseek(fd, 0, 2);
    if (size < 0) {
        sceClose(fd);
        bulletproof_printf("Can't load '%s' failed on Lseek\n", full);
        return -1;
    }
    sceLseek(fd, 0, 0);
    if (size & 0xf) {
        size += 0x10 - (size & 0xf);
    }
    if (maxLen < 1 || size < maxLen) {
        maxLen = size;
    }
    read = sceRead(fd, dest, maxLen);
    sceClose(fd);
    return read;
}

void ClearMemLocks(void)
{
    mlmLockSave = 0;
    mlmLockSaveTop = 0;
}
