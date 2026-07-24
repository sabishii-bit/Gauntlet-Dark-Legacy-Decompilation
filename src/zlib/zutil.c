/* zutil.c -- target dependent utility functions for the compression library
 * From zlib 1.0.4, cut down by the game: zcfree is a no-op (the game frees
 * decompression memory wholesale through its pools), zcalloc forwards to the
 * game allocator. zlibVersion/zError/z_errmsg are dead-stripped.
 */

#include "types.h"

typedef void* voidpf;

void* gAlloc(u32 size);

void zcfree(voidpf opaque, voidpf ptr)
{
}

voidpf zcalloc(voidpf opaque, u32 items, u32 size)
{
    return gAlloc(items * size);
}
