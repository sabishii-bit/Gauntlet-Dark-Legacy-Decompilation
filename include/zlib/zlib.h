/* zlib.h -- interface of the 'zlib' general purpose compression library
 * version 1.0.4, Jul 24th, 1996. Trimmed to the pieces present in the
 * GameCube build (inflate side only).
 */

#ifndef _ZLIB_H
#define _ZLIB_H

#include "types.h"

typedef u8 Byte;
typedef u32 uInt;
typedef u32 uLong;
typedef Byte Bytef;
typedef char charf;
typedef int intf;
typedef uInt uIntf;
typedef uLong uLongf;
typedef void* voidpf;
typedef void* voidp;

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

typedef z_stream* z_streamp;

#define Z_NO_FLUSH 0
#define Z_PARTIAL_FLUSH 1
#define Z_SYNC_FLUSH 2
#define Z_FULL_FLUSH 3
#define Z_FINISH 4

#define Z_OK 0
#define Z_STREAM_END 1
#define Z_NEED_DICT 2
#define Z_ERRNO (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR (-3)
#define Z_MEM_ERROR (-4)
#define Z_BUF_ERROR (-5)
#define Z_VERSION_ERROR (-6)

#define Z_DEFLATED 8

#define Z_NULL 0

#define ZLIB_VERSION "1.0.4"

int inflateInit_(z_streamp strm, const char* version, int stream_size);
int inflateInit2_(z_streamp strm, int windowBits, const char* version, int stream_size);
int inflate(z_streamp strm, int flush);
int inflateEnd(z_streamp strm);
int uncompress(Bytef* dest, uLongf* destLen, const Bytef* source, uLong sourceLen);
uLong adler32(uLong adler, const Bytef* buf, uInt len);

#define inflateInit(strm) inflateInit_((strm), ZLIB_VERSION, sizeof(z_stream))
#define inflateInit2(strm, windowBits) inflateInit2_((strm), (windowBits), ZLIB_VERSION, sizeof(z_stream))

#endif /* _ZLIB_H */
