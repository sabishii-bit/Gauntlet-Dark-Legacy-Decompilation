#ifndef __MEM_H
#define __MEM_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

DECL_SECT(".init") void* memset(void* dst, int val, size_t n);
DECL_SECT(".init") void __fill_mem(void* dst, int val, u32 n);
DECL_SECT(".init") void* memcpy(void* dst, const void* src, size_t n);

#ifdef __cplusplus
}
#endif

#endif
