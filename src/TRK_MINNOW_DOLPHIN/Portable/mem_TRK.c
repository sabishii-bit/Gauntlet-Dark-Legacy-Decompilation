#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

DECL_SECT(".init") void* TRK_memcpy(void* dst, const void* src, size_t n);

DECL_SECT(".init") void* TRK_memcpy(void* dst, const void* src, size_t n) {
	const unsigned char* s = (const unsigned char*)src - 1;
	unsigned char* d = (unsigned char*)dst - 1;

	n++;
	while (--n != 0)
		*++d = *++s;
	return dst;
}

#ifdef __cplusplus
}
#endif
