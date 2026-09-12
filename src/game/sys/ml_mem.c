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
 * NonMatching: AllocFile retains a two-instruction argument-copy ordering
 * residual. The remaining native bodies are exact. The empty mlmRootPath
 * buffer still relies on extracted data pending its original extent. */

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
extern int sceOpen(const char* path, ...);
extern int sceRead(int fd, void* buf, int len);
extern int sceSifLoadElfPart(int fd, int arg, int* status);
extern int sceLseek(int fd, int off, int whence);
extern int sceClose(int fd);
extern int sceFileSize(const char* path);
extern int sceFileExists(const char* path);

/* ---- zlib ---- */
extern int uncompress(void* dest, int* destLen, void* src, int srcLen);

/* ================= allocator state (.sbss / .bss) ================= */
/* Separate module state, not a synthetic aggregate. GC accesses are all
 * 32-bit words; InitMemHandler also resets the public pbLoad counter.
 * MWCC emits these tentative definitions in reverse declaration order,
 * covering 0x80344F18..0x80344F4B followed by four alignment bytes. */
u8* mlmMemBase;    /* base of the managed block */
int mlmMemUsed;    /* low watermark; lo-alloc increments it */
int mlmMemLimit;   /* high boundary; hi-alloc decrements it */
int mlmMemReserved; /* nonzero prohibits allocation */
int mlmLastFileSize;
int mlmCurFileSlot;
/* pbPulseTime produces this as u32. Some consumer extern declarations
 * still disagree on signedness/volatile qualification; that shared
 * interface debt is not resolved by establishing ownership here. */
u32 pbLoad;
int mlmReadRes;
int mlmCloseRes;
int mlmServeTimeout;
int mlmLockSaveTop;
int mlmLockSave;   /* saved low watermark */
int alloctot;      /* bytes taken from the high pool */
extern int gLowMemMode;   /* selects a smaller managed block            */
extern int gDemoMode;
extern int __OSCurrHeap;

/* ================= file-system state ================= */
enum FinfoState {
    FINFO_FREE = -1,
    FINFO_IN_USE = 0,
    FINFO_READY = 1,
    FINFO_USER = 2
};

enum FileCmds {
    FILE_OPENING = 0,
    FILE_READING = 1
};

/* Xbox fileinfo field names and types, checked against StartFileRead's
 * stores and do_threaded_io's loads. The callback at +0 is only stored on
 * GC; keep the existing opaque callback API pending shared declaration
 * recovery, rather than claiming the Xbox function-pointer signature here. */
typedef struct fileinfo {
    /* 0x000 */ void* complete;
    /* 0x004 */ char* destbuf;
    /* 0x008 */ int destbufsize;
    /* 0x00C */ int bytes_read;
    /* 0x010 */ enum FinfoState done;
    /* 0x014 */ enum FileCmds cmd;
    /* 0x018 */ int file_len;
    /* 0x01C */ int fd;
    /* 0x020 */ char filename[256];
    /* 0x120 */ int compressed;
    /* 0x124 */ char* comp_buff;
    /* 0x128 */ int decomp_size;
} MLFILE; /* 0x12C */

/* ML_MEM owns two separate file records followed by eight lock watermarks.
 * GC record strides and accesses cover 0x12C bytes each; LockMem and
 * InitMemHandler bound the lock array at eight entries. The Xbox module
 * independently identifies finfo_list[1] and temp_finfo as fileinfo objects.
 * These are independent definitions, not one synthetic allocator block. */
static MLFILE finfo_list[1];
static MLFILE temp_finfo;
static int mlmLockStack[8];

extern char mlmRootPath[];
extern void ErrorPrintf(const char* fmt, ...);

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

void serve_io(void)
{
    int i;
    int served;

    i = 0;
    served = 0;
    while (i++ < 1 && served == 0) {
        MLFILE* f = &finfo_list[mlmCurFileSlot];
        if (f->done != FINFO_FREE && f->done != FINFO_READY) {
            served = do_threaded_io(f);
        }
        if (++mlmCurFileSlot >= 1) {
            mlmCurFileSlot = 0;
        }
    }
}

static inline int* FindWadEntry(int* wad, int key)
{
    int offset;
    int* entry;
    u32 n;

    if (wad == NULL) {
        return NULL;
    }
    for (n = 0, offset = 0; n < wad[1]; n++, offset += 0x10) {
        entry = (int*)(wad[2] + offset);
        if ((u32)key == (u32)*entry) {
            return entry;
        }
    }
    return NULL;
}

/* WAD directory lookup: {count@+4, entries@+8}, entry = {key,ofs,size,pad} */
int MBGetFromWad(int* wad, int key, int* sizeOut)
{
    int* result;

    result = NULL;
    if (wad != NULL) {
        result = FindWadEntry(wad, key);
    }
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
typedef union MbSwapWord {
    u32 word;
    u8 byte[4];
} MbSwapWord;

typedef struct MbSwapPair {
    MbSwapWord src;
    MbSwapWord dst;
} MbSwapPair;

typedef struct MbSwapWorkspace {
    u8 unused[48];
    MbSwapPair pair[6];
} MbSwapWorkspace;

#define MB_SWAP32_AT(output, value, swap) do { \
    (swap).src.word = (value); \
    (swap).dst.byte[0] = (swap).src.byte[3]; \
    (swap).dst.byte[1] = (swap).src.byte[2]; \
    (swap).dst.byte[2] = (swap).src.byte[1]; \
    (swap).dst.byte[3] = (swap).src.byte[0]; \
    (output) = (swap).dst.word; \
} while (0)

int MBSetupWad(int* wad, int base)
{
    u32 i;
    u32 offset;
    MbSwapWorkspace work;

    if (wad == NULL) {
        return 0;
    }
    *wad = base;
    MB_SWAP32_AT(wad[2], *(u32*)*wad, work.pair[5]);
    MB_SWAP32_AT(wad[1], *(u32*)(*wad + 4), work.pair[4]);
    if ((u32)wad[2] >= (u32)mlmMemBase) {
        return 0;
    }
    wad[2] += base;
    MB_SWAP32_AT(*(u32*)*wad, wad[2], work.pair[3]);
    for (i = 0, offset = 0; i < (u32)wad[1]; i++, offset += 0x10) {
        int* e = (int*)(wad[2] + offset);
        MB_SWAP32_AT(e[0], e[0], work.pair[2]);
        MB_SWAP32_AT(e[1], e[1], work.pair[1]);
        MB_SWAP32_AT(e[2], e[2], work.pair[0]);
        e[1] += base;
    }
    return 1;
}

/* open a file into the async handle slot */
MLFILE* StartFileRead(char* wad, char* name, int mode, int sizeHint,
                      char* dest, void* callback)
{
    char full[256];
    char path[256];
    int slot;
    MLFILE* f;
    int size;
    int fd;

    if (alloctot != 0) {
        gErrorCode = 0xff;
        FatalErrorf("Temporary high memory in use by somebody else when "
                    "StartFileRead called.  Check if another file is being "
                    "read!");
    }
    for (slot = 0; slot < 1; slot++) {
        f = &finfo_list[slot];
        if (f->done == FINFO_FREE) {
            break;
        }
    }
    if (slot == 1) {
        gErrorCode = 0xff;
        FatalErrorf("Too many open files: %d", slot);
        return NULL;
    }
    f = &finfo_list[slot];
    if (wad != NULL) {
        sprintf(path, "%s/%s", wad, name);
    } else {
        sprintf(path, "%s", name);
    }
    if ((name[0] != 'W' || name[1] != 'A' || name[2] != 'D') &&
        strrchr(path, '.') == NULL) {
        strcat(path, ".ps2");
    }
    strcpy(full, mlmRootPath);
    if (path[0] != '/') {
        strcat(full, "/gauntlet/");
    }
    strcat(full, path);
    fd = sceOpen(full, 1);
    if (fd < 0) {
        gErrorCode = 0xff;
        FatalErrorf("Can't open: %s.\n", full);
        return NULL;
    }
    size = sceLseek(fd, 0, 2);
    if (size & 0xf) {
        size += 0x10 - (size & 0xf);
    }
    strncpy(f->filename, full, 0x100);
    f->destbuf = dest;
    f->destbufsize = sizeHint;
    sceLseek(fd, 0, 0);
    f->complete = callback;
    f->file_len = size;
    f->bytes_read = 0;
    f->compressed = 0;
    sceClose(fd);
    fd = sceOpen(full, 0x8001);
    if (fd < 0) {
        gErrorCode = 0xff;
        FatalErrorf("Can't open: %s.\n", full);
        return NULL;
    }
    f->fd = fd;
    f->cmd = FILE_OPENING;
    f->done = FINFO_IN_USE;
    return f;
}

/* serve one chunk of an open file; decompress on completion */
int do_threaded_io(MLFILE* f)
{
    int destLen[1];
    int initialStatus;
    int waitStatus;
    u8 unused[4];
    int status;
    u32 chunk;
    void* buf;

    mlmReadRes = sceSifLoadElfPart(f->fd, 1, &initialStatus);
    status = initialStatus;
    if (status == 0) {
        if (f->bytes_read >= f->file_len) {
            if (f->compressed) {
                destLen[0] = f->decomp_size;
                if (uncompress(f->destbuf, destLen, f->comp_buff,
                               f->file_len) != 0) {
                    gErrorCode = 0x80;
                    FatalErrorf("Error decompressing file. Can not contine.");
                }
            }
            mlmServeTimeout = 0;
            while ((mlmReadRes = sceSifLoadElfPart(f->fd, 1, &waitStatus),
                    waitStatus != 0)) {
                if (++mlmServeTimeout > 1500000000) {
                    gErrorCode = 0xa0;
                    FatalErrorf("Timeout serving file %s (1)", f->filename);
                }
            }
            mlmCloseRes = sceClose(f->fd);
            mlmMemLimit += alloctot;
            alloctot = 0;
            f->done = FINFO_READY;
        } else {
            chunk = f->file_len - f->bytes_read;
            if ((int)chunk > 0x8000) {
                chunk = 0x8000;
            }
            if (chunk & 0xf) {
                chunk += 0x10 - (chunk & 0xf);
            }
            buf = f->compressed ? f->comp_buff : f->destbuf;
            f->cmd = FILE_READING;
            if (sceRead(f->fd, (char*)buf + f->bytes_read, chunk) >= 0) {
                f->bytes_read += chunk;
            }
        }
    }
    return !status;
}

/* ============================================================== *
 *  allocator core (decompiled)                                    *
 * ============================================================== */

static inline void InitMemHandlerClearLocks(void)
{
    int i;

    for (i = 0; i < 8; i++) {
        mlmLockStack[i] = 0;
    }
}

void InitMemHandler(void)
{
    mlmMemLimit = 0x1c00000;
    mlmMemLimit += 0x8000;
    mlmMemLimit += 0x80000;
    if (gDemoMode != 0) {
        mlmMemLimit -= 0x100000;
    }
    mlmMemLimit = OSCheckHeap(DemoHeap);
    mlmMemLimit -= 0x1000;
    mlmMemBase = OSAllocFromHeap(__OSCurrHeap, mlmMemLimit);
    if (mlmMemBase == NULL) {
        while (mlmMemBase == NULL) {
            mlmMemLimit -= 0x4000;
            mlmMemBase = OSAllocFromHeap(__OSCurrHeap, mlmMemLimit);
        }
    }
    memset(mlmMemBase, 0, mlmMemLimit);
    bulletproof_printf("Available Memory = %d\n", mlmMemLimit);
    mlmMemUsed = 0;
    mlmMemBase = (u8*)(((u32)mlmMemBase + 0x3f) & 0xffffffc0);
    mlmMemLimit = (mlmMemLimit & 0xffffffc0) - 0x40;
    InitMemHandlerClearLocks();
    finfo_list[0].done = FINFO_FREE;
    pbLoad = 0;
}

int FileSystemReading(void)
{
    int reading = 0;

    if (finfo_list[0].done == FINFO_IN_USE)
        reading = 1;
    return reading;
}

int FileSystemBusy(void)
{
    int busy = 0;

    if (finfo_list[0].done != FINFO_FREE)
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
        FatalErrorf("GetMemBase() called while mem reserved");
    }
    return mlmMemBase + (mlmMemUsed / 4) * 4;
}

void* AllocMem(int size)
{
    void* result;

    if (mlmMemReserved != 0) {
        gErrorCode = 0xa0a000;
        FatalErrorf("AllocMem() called while mem reserved");
    }
    if (size & 0xf) {
        size += 0x10 - (size & 0xf);
    }
    result = mlmMemBase + (mlmMemUsed / 4) * 4;
    mlmMemUsed += size;
    if (mlmMemUsed > mlmMemLimit) {
        gErrorCode = 0xc0c000;
        FatalErrorf("AllocMem failed: %d bytes, exceeds free by %d bytes",
                    size, mlmMemUsed - mlmMemLimit);
    }
    return result;
}

void* AllocMem32(int size)
{
    u32 aligned;
    int pad;
    void* result;

    aligned = (mlmMemUsed + 0x1f) & 0xffffffe0;
    pad = aligned - mlmMemUsed;
    size += pad;
    if (mlmMemLimit - mlmMemUsed < size) {
        return NULL;
    }
    result = AllocMem(size);
    return (u8*)result + pad;
}

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

void* AllocHiMem(u32 size)
{
    u32 tmp;
    u32 result;

    if (mlmMemReserved != 0) {
        gErrorCode = 0x808000;
        FatalErrorf("AllocMem() called while mem reserved");
    }
    if (size & 0xf) {
        size += 0x10 - (size & 0xf);
    }
    if ((int)(mlmMemUsed + size) > mlmMemLimit) {
        gErrorCode = 0x909000;
        FatalErrorf("AllocHiMem failed: %d bytes, exceeds free by %d bytes",
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

static inline void get_path_inline(char* out, char* wad, char* name)
{
    char tmp[256];

    if (wad != NULL) {
        sprintf(tmp, "%s/%s", wad, name);
    } else {
        sprintf(tmp, "%s", name);
    }
    if (!(name[0] == 'W' && name[1] == 'A' && name[2] == 'D') &&
        strrchr(tmp, '.') == NULL) {
        strcat(tmp, ".ps2");
    }
    strcpy(out, mlmRootPath);
    if (tmp[0] != '/') {
        strcat(out, "/gauntlet/");
    }
    strcat(out, tmp);
}

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
            strcat(dst, "/");
            if (n - 1 > 0) {
                strncat(dst, name, n - 1);
            }
        }
        return 1;
    }
    return 0;
}

int FileSize(char* wad, char* name)
{
    u8 unused[8];
    char full[256];
    u32 size;

    get_path_inline(full, wad, name);
    size = sceFileSize(full);
    if (size & 0xf) {
        size += 0x10 - (size & 0xf);
    }
    return size;
}

int FileExists(char* wad, char* name)
{
    char full[256];

    get_path_inline(full, wad, name);
    return sceFileExists(full) & 0xff;
}

void* AllocFile(char* wad, char* name)
{
    int avail;
    int read;
    void* dest;
    int used0;

    if (mlmMemReserved != 0) {
        gErrorCode = 0xe0e000;
        FatalErrorf("GetMemBase() called while mem reserved");
    }
    dest = mlmMemBase + (mlmMemUsed / 4) * 4;
    avail = mlmMemLimit - mlmMemUsed;
    read = xReadFileSection(wad, name, avail, dest);
    if (avail > 0 && read > avail) {
        gErrorCode = 0x80;
        FatalErrorf("File read overflowed: %s size:%d max:%d",
                    temp_finfo.filename, read, avail);
    }
    if (read < 0) {
        gErrorCode = 0xff;
        FatalErrorf("AllocFile: Read failed.");
    }
    used0 = mlmMemUsed;
    AllocMem(read);
    bulletproof_printf("==== ALLOC FILE=%s/%s, MEM:%06dk -> %06dk [%dk]\n",
                       wad, name, used0 >> 10, mlmMemUsed >> 10, read >> 10);
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
                    temp_finfo.filename, read, maxLen);
    }
    return read;
}

#pragma opt_propagation off
int xReadFileSection(char* wad, char* name, register int maxLen, register void* dest)
{
    char full[256];
    char tmp[256];
    int fd;
    int size;
    int read;
    register void* output;
    register int limit;

    limit = maxLen;
    output = dest;

    if (wad != NULL) {
        sprintf(tmp, "%s/%s", wad, name);
    } else {
        sprintf(tmp, "%s", name);
    }
    if (!(name[0] == 'W' && name[1] == 'A' && name[2] == 'D') &&
        strrchr(tmp, '.') == NULL) {
        strcat(tmp, ".ps2");
    }
    strcpy(full, mlmRootPath);
    if (tmp[0] != '/') {
        strcat(full, "/gauntlet/");
    }
    strcat(full, tmp);
    fd = sceOpen(full, 1);
    if (fd < 0) {
        ErrorPrintf("Can't load: %s, failed on open.\n", full);
        return -1;
    }
    size = sceLseek(fd, 0, 2);
    if (size < 0) {
        sceClose(fd);
        ErrorPrintf("Can't load: %s, failed on Lseek for size.\n", full);
        return -1;
    }
    sceLseek(fd, 0, 0);
    if (size & 0xf) {
        size += 0x10 - (size & 0xf);
    }
    if (limit <= 0 || limit > size) {
        limit = size;
    }
    read = sceRead(fd, output, limit);
    sceClose(fd);
    return read;
}
#pragma opt_propagation reset

void ClearMemLocks(void)
{
    mlmLockSave = 0;
    mlmLockSaveTop = 0;
}

void get_path(char* out, char* wad, char* name)
{
    char tmp[256];

    if (wad != NULL) {
        sprintf(tmp, "%s/%s", wad, name);
    } else {
        sprintf(tmp, "%s", name);
    }
    if (!(name[0] == 'W' && name[1] == 'A' && name[2] == 'D') &&
        strrchr(tmp, '.') == NULL) {
        strcat(tmp, ".ps2");
    }
    strcpy(out, mlmRootPath);
    if (tmp[0] != '/') {
        strcat(out, "/gauntlet/");
    }
    strcat(out, tmp);
}
