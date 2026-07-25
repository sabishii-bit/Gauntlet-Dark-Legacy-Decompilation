#include "stdlib.h"

/* heapsort with byte-wise element swap */
void qsort(void* base, size_t nmemb, size_t size,
           int (*cmp)(const void*, const void*))
{
    unsigned char* b = (unsigned char*) base;
    unsigned char* pk;
    unsigned char* pir;
    unsigned char* pj;
    unsigned char* pc;
    size_t n;
    size_t k;
    size_t j;
    size_t i;
    char t;

    if (nmemb < 2) {
        return;
    }
    n = nmemb;
    k = nmemb / 2 + 1;
    pk = b + size * (nmemb / 2);
    pir = b + size * (nmemb - 1);
    for (;;) {
        if (k > 1) {
            k--;
            pk -= size;
        } else {
            unsigned char* x = pir - 1;
            unsigned char* y = pk - 1;
            for (i = size + 1; --i != 0;) {
                t = (char) y[1];
                y[1] = x[1];
                x[1] = (unsigned char) t;
                y++;
                x++;
            }
            n--;
            if (n == 1) {
                return;
            }
            pir -= size;
        }
        j = k;
        pj = b + size * (k - 1);
        while (j * 2 <= n) {
            j *= 2;
            pc = pj;
            pj = b + size * (j - 1);
            if (j < n && cmp(pj, pj + size) < 0) {
                pj += size;
                j++;
            }
            if (cmp(pc, pj) < 0) {
                unsigned char* x = pj - 1;
                unsigned char* y = pc - 1;
                for (i = size + 1; --i != 0;) {
                    t = (char) y[1];
                    y[1] = x[1];
                    x[1] = (unsigned char) t;
                    y++;
                    x++;
                }
            } else {
                break;
            }
        }
    }
}
