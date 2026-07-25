#include "stdlib.h"

void* bsearch(const void* key, const void* base, size_t nmemb, size_t size,
              int (*cmp)(const void*, const void*))
{
    const char* q;
    const char* p;
    size_t lo;
    size_t hi;
    size_t mid;
    int r;

    if (key == NULL || base == NULL || nmemb == 0 || size == 0 || cmp == NULL) {
        return NULL;
    }
    q = (const char*) base;
    r = cmp(key, q);
    if (r == 0) {
        return (void*) q;
    }
    if (r < 0) {
        return NULL;
    }
    hi = nmemb - 1;
    lo = 1;
    while (lo <= hi) {
        mid = (lo + hi) >> 1;
        p = (const char*) base + size * mid;
        r = cmp(key, p);
        if (r == 0) {
            return (void*) p;
        }
        if (r < 0) {
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }
    return NULL;
}
