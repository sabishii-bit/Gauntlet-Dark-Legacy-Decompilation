#include "types.h"

#pragma dont_inline on

#define __HI(x) (((s32*) &(x))[0])
#define __LO(x) (((u32*) &(x))[1])

double frexp(double x, int* eptr)
{
    static const double two54 = 1.80143985094819840000e+16;

    int hx, ix, lx;

    hx = __HI(x);
    ix = 0x7fffffff & hx;
    lx = __LO(x);
    *eptr = 0;
    if (ix >= 0x7ff00000 || ((ix | lx) == 0)) {
        return x;
    }
    if (ix < 0x00100000) {
        x *= two54;
        hx = __HI(x);
        ix = hx & 0x7fffffff;
        *eptr = -54;
    }
    *eptr += (ix >> 20) - 1022;
    hx = (hx & 0x800fffff) | 0x3fe00000;
    __HI(x) = hx;
    return x;
}
