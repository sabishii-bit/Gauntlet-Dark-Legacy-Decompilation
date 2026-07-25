#include "types.h"

#pragma dont_inline on

#define __HI(x) (((s32*) &(x))[0])
#define __LO(x) (((u32*) &(x))[1])

double scalbn(double x, int n);

double ldexp(double x, int n)
{
    return scalbn(x, n);
}
