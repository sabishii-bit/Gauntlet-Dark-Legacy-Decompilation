/*
 * buffile.c - DCS buffered file reader (BUFFILE.OBJ), GameCube port.
 *
 * Text 0x800D4308-0x800D4960.  A small double-buffered reader over the PS2
 * file API (sceOpen/sceRead/sceLseek/sceClose, shimmed by game/ps2/fakelib.c
 * on GameCube).  Used by dcs.c to stream BANK/VAG data off disc.  Debug:
 * "OPEN FILE %s", "FileBuf read %d wanted %d", "No more FILEBUF handles".
 *
 * NonMatching: reconstruction in progress.
 */
#include "types.h"

typedef struct FileBuf {
    s32 fd;
    s32 start;
    s32 size;
    s32 remaining;
    s32 bufferSize;
    s32 available;
    u8* cursor;
    u8 reserved[0x24];
    u8 buffer[0x20000];
} FileBuf;

typedef struct FileBufDesc {
    char* path;
    s32 start;
    s32 size;
} FileBufDesc;

extern FileBuf lbl_802FE0A0[];

int printf(const char* format, ...);
int sceOpen(const char* path, int flags, ...);
int sceLseek(int fd, int offset, int whence);
int sceRead(int fd, void* data, int length);
int sceClose(int fd);
void* memcpy(void* dst, const void* src, u32 length);

/* 0x800D4308  close handle, sceClose */
s32 FileBufClose(FileBuf* file)
{
    s32 result;

    result = 0;
    if (file != 0) {
        if (file->fd >= 0) {
            sceClose(file->fd);
        }
        file->fd = -1;
        result = 1;
        file->cursor = 0;
        file->available = 0;
        file->remaining = 0;
        file->size = 0;
        file->bufferSize = 0;
    }
    return result;
}

/* 0x800D436C  seek + refill buffer, sceLseek */
s32 FileBufSeek(FileBuf* file, s32 offset, s32 whence)
{
    s32 result;
    s32 seekMode;
    s32 position;

    result = 0;
    if (file == 0) {
        printf("DCSERROR: ");
        printf("Invalid FileBuf\n");
    } else if (file->bufferSize == 0) {
        printf("DCSERROR: ");
        printf("FileBufStart was never called!\n");
    } else {
        switch (whence) {
        case 0:
            offset += file->start;
            break;
        case 2:
            whence = 0;
            offset += file->start + file->size;
            break;
        case 1:
            whence = 0;
            offset += file->start + file->size - file->remaining;
            break;
        }

        switch (whence) {
        case 0:
            seekMode = 0;
            break;
        case 1:
            seekMode = 1;
            break;
        case 2:
            seekMode = 2;
            break;
        }

        position = sceLseek(file->fd, offset, seekMode);
        file->cursor = file->buffer;
        result = 1;
        file->available = 0;
        file->remaining = file->size - (position - file->start);
    }
    return result;
}

/* 0x800D44BC  copy N bytes out, refilling via sceRead */
s32 FileBufGet(FileBuf* file, void* destination, s32 length)
{
    s32 readSize;
    char* strings;
    s32 readResult;
    u8* output;
    u8* outputStart;
    s32 available;

    outputStart = destination;
    output = outputStart;
    strings = "DCSERROR: ";

    if (file == 0 || file->fd < 0 || file->cursor == 0) {
        printf(strings);
        printf("Invalid FileBuf\n");
    } else if (file->bufferSize == 0) {
        printf(strings);
        printf("FileBufStart was never called!\n");
    } else {
        while (length > (available = file->available) && file->remaining > 0) {
            if (available > 0) {
                memcpy(output, file->cursor, available);
                output += file->available;
                length -= file->available;
                file->available = 0;
            }

            file->cursor = file->buffer;
            readSize = file->remaining < file->bufferSize
                           ? file->remaining
                           : file->bufferSize;
            readResult = sceRead(file->fd, file->cursor, (readSize + 15) & ~15);
            if (readResult != ((readSize + 15) & ~15)) {
                if (readSize != file->remaining ||
                    ((readResult + 15) & ~15) != ((readSize + 15) & ~15)) {
                    printf(strings);
                    printf("FileBuf read %d wanted %d (nfile %d)\n",
                           readResult, readSize, file->remaining);
                }
            }
            if (readResult < 0) {
                break;
            }
            if (file->available < 0) {
                file->cursor -= file->available;
            }
            file->remaining -= readResult;
            file->available += readResult;
            if (file->remaining < 0) {
                file->available += file->remaining;
                file->remaining = 0;
            }
            if (readResult == 0) {
                break;
            }
        }

        if (length > file->available) {
            printf(strings);
            printf("FileBuf underrun by %d bytes\n", length - file->available);
            length = file->available;
        }
        if (length > 0) {
            memcpy(output, file->cursor, length);
            output += length;
            file->cursor += length;
            file->available -= length;
        }
    }
    return output - outputStart;
}

/* 0x800D46E8  rewind/reopen an existing handle */
s32 FileBufReopen(FileBuf* file)
{
    s32 result;

    result = 0;
    if (file->bufferSize == 0) {
        if (file->size <= 0) {
            file->size = sceLseek(file->fd, 0, 2);
            file->size -= file->start;
        }
        if (file->size <= 0) {
            sceClose(file->fd);
            file->fd = -1;
            result = -1;
        } else {
            file->bufferSize = 0x20000;
            FileBufSeek(file, 0, 0);
            result = 1;
        }
    }
    return result;
}

/* 0x800D4790  open a file ("OPEN FILE %s"), sceOpen */
s32 FileBufOpen(FileBuf* file, FileBufDesc* desc)
{
    volatile u8 scratch[16];
    s32 result;

    (void)scratch;
    if (file != 0) {
        if (file->fd >= 0) {
            sceClose(file->fd);
        }
        file->fd = -1;
        file->cursor = 0;
        file->available = 0;
        file->remaining = 0;
        file->size = 0;
        file->bufferSize = 0;
    }

    result = -1;
    file->fd = sceOpen(desc->path, 1);
    if (file->fd < 0) {
        printf("DCSERROR: ");
        printf("OPEN FILE %s\n", desc->path);
    } else {
        result = 1;
        file->start = desc->start;
        file->size = desc->size;
    }
    return result;
}

/* 0x800D4854  alloc handle+buffer and open, sceOpen */
FileBuf* FileBufStart(FileBufDesc* desc)
{
    s32 result;
    FileBuf* file;
    s32 i;

    file = 0;
    if (desc->path == 0) {
        return lbl_802FE0A0;
    }

    for (i = 0; i < 1; i++) {
        if (lbl_802FE0A0[i].cursor == 0) {
            file = &lbl_802FE0A0[i];
            break;
        }
    }
    if (i >= 1) {
        printf("DCSERROR: ");
        printf("No more FILEBUF handles\n");
    } else {
        result = -1;
        file->fd = sceOpen(desc->path, 1);
        if (file->fd < 0) {
            printf("DCSERROR: ");
            printf("OPEN FILE %s\n", desc->path);
        } else {
            result = 1;
            file->start = desc->start;
            file->size = desc->size;
        }
        if (result >= 0) {
            return file;
        }
    }
    return 0;
}
