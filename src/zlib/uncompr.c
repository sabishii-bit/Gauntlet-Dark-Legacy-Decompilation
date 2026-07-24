/* uncompr.c -- decompress a memory buffer
 * Copyright (C) 1995-1996 Jean-loup Gailly.
 * From zlib 1.0.4 (ZLIB_VERSION "1.0.4", sizeof(z_stream) == 0x38).
 */

#include "types.h"

typedef u8 Bytef;
typedef u32 uInt;
typedef u32 uLong;
typedef uLong uLongf;
typedef void* voidpf;

typedef voidpf (*alloc_func)(voidpf opaque, uInt items, uInt size);
typedef void (*free_func)(voidpf opaque, voidpf address);

struct internal_state;

typedef struct z_stream_s {
    Bytef* next_in;   /* next input byte */
    uInt avail_in;    /* number of bytes available at next_in */
    uLong total_in;   /* total nb of input bytes read so far */
    Bytef* next_out;  /* next output byte should be put there */
    uInt avail_out;   /* remaining free space at next_out */
    uLong total_out;  /* total nb of bytes output so far */
    char* msg;        /* last error message, NULL if no error */
    struct internal_state* state; /* not visible by applications */
    alloc_func zalloc; /* used to allocate the internal state */
    free_func zfree;   /* used to free the internal state */
    voidpf opaque;     /* private data object passed to zalloc and zfree */
    int data_type;     /* best guess about the data type: ascii or binary */
    uLong adler;       /* adler32 value of the uncompressed data */
    uLong reserved;    /* reserved for future use */
} z_stream;

#define Z_NULL 0
#define Z_OK 0
#define Z_STREAM_END 1
#define Z_BUF_ERROR (-5)
#define Z_FINISH 4

#define ZLIB_VERSION "1.0.4"

int inflateInit_(z_stream* strm, const char* version, int stream_size);
int inflate(z_stream* strm, int flush);
int inflateEnd(z_stream* strm);

#define inflateInit(strm) inflateInit_((strm), ZLIB_VERSION, sizeof(z_stream))

/* ===========================================================================
     Decompresses the source buffer into the destination buffer.
*/
int uncompress(Bytef* dest, uLongf* destLen, const Bytef* source, uLong sourceLen)
{
    z_stream stream;
    int err;

    stream.next_in = (Bytef*) source;
    stream.avail_in = (uInt) sourceLen;
    /* Check for source > 64K on 16-bit machine: */
    if ((uLong) stream.avail_in != sourceLen) {
        return Z_BUF_ERROR;
    }

    stream.next_out = dest;
    stream.avail_out = (uInt) *destLen;
    if ((uLong) stream.avail_out != *destLen) {
        return Z_BUF_ERROR;
    }

    stream.zalloc = (alloc_func) 0;
    stream.zfree = (free_func) 0;

    err = inflateInit(&stream);
    if (err != Z_OK) {
        return err;
    }

    err = inflate(&stream, Z_FINISH);
    *destLen = stream.total_out;
    if (err != Z_STREAM_END) {
        inflateEnd(&stream);
        return err;
    }
    return inflateEnd(&stream);
}
