#include "types.h"

#pragma dont_inline on

#define __HI(x) (((s32*) &(x))[0])
#define __LO(x) (((u32*) &(x))[1])

int __fpclassifyd(double x)
{
    switch (__HI(x) & 0x7ff00000) {
    case 0x7ff00000:
        if ((__HI(x) & 0x000fffff) || ((s32) __LO(x) != 0)) {
            return 1;
        }
        return 2;
    case 0x00000000:
        if ((__HI(x) & 0x000fffff) || ((s32) __LO(x) != 0)) {
            return 5;
        }
        return 3;
    default:
        return 4;
    }
}
