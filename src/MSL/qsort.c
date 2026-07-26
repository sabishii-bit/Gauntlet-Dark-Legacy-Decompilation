#include "stdlib.h"

/* heapsort with byte-wise element swap */
void qsort(void* base, size_t nmemb, size_t size,
           int (*cmp)(const void*, const void*))
{
    size_t k;
    size_t n;
    size_t j;
    unsigned char* b = (unsigned char*) base;
    unsigned char* pk;
    unsigned char* pir;
    unsigned char* pc;
    unsigned char* pj;

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
            signed char* x = (signed char*) (pir - 1);
            signed char* y = (signed char*) (pk - 1);
            size_t i;
            int t;
            for (i = size + 1; --i != 0;) {
                t = y[1];
                y[1] = x[1];
                x[1] = t;
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
            if (j < n) {
                unsigned char* pn = pj + size;
                if (cmp(pj, pn) < 0) {
                    pj = pn;
                    j++;
                }
            }
            if (cmp(pc, pj) < 0) {
                signed char* x = (signed char*) (pj - 1);
                signed char* y = (signed char*) (pc - 1);
                size_t i;
                int t;
                for (i = size + 1; --i != 0;) {
                    t = y[1];
                    y[1] = x[1];
                    x[1] = t;
                    y++;
                    x++;
                }
            } else {
                break;
            }
        }
    }
}
