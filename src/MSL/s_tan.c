#include "types.h"

#pragma dont_inline on

#define __HI(x) (((s32*) &(x))[0])
#define __LO(x) (((u32*) &(x))[1])

int __ieee754_rem_pio2(double x, double* y);
double __kernel_tan(double x, double y, int iy);

double tan(double x)
{
    double y[2], z = 0.0;
    int n, ix;

    ix = __HI(x);
    ix &= 0x7fffffff;
    if (ix <= 0x3fe921fb) {
        return __kernel_tan(x, z, 1);
    } else if (ix >= 0x7ff00000) {
        return x - x;
    } else {
        n = __ieee754_rem_pio2(x, y);
        return __kernel_tan(y[0], y[1], 1 - ((n & 1) << 1));
    }
}
