/*
 * atanf.c - MSL float atan family (named atan/atan2 in this SDK vintage),
 * plus fabsf/acosf/floorf. Table-driven atan over 11.25-degree sub-octants
 * with hi/lo split constants.
 */
#include "types.h"

#define __HI(x) (((s32*) &(x))[0])

typedef union {
    u32 u;
    f32 f;
} FloatU32;

extern double __fabs(double);
extern float __fabsf(float);
extern double __frsqrte(double);

f32 atan(f32 x);
f32 atanf(f32 x);

__declspec(section ".data") FloatU32 __float_nan = { 0x7FFFFFFF };
__declspec(section ".data") FloatU32 __float_huge = { 0x7F800000 };

/*
 * [0..6]   odd polynomial coefficients for atan on the reduced interval
 * [7..18]  hi/lo parts of (cot(c) + tan(c)) sums per sub-octant
 * [19..32] hi/lo parts of the sub-octant center angles (leading 0 for the
 *          identity/reciprocal paths, indexed with i == -1)
 * [33..44] hi/lo parts of cot(c) per sub-octant
 */
static const f32 atan_tbl[] = {
    1.0f,
    -0.33333331f,
    0.19999887f,
    -0.1428165f,
    0.1104118f,
    -0.084597558f,
    0.047142435f,
    6.8284202f,
    3.2398281f,
    2.0f,
    1.446462f,
    1.1715729f,
    1.0395662f,
    7.135e-06f,
    8.2e-07f,
    0.0f,
    6.3e-07f,
    0.0f,
    0.0f,
    0.0f,
    0.39269f,
    0.58904862f,
    0.78539813f,
    0.98174697f,
    1.178097f,
    1.374446f,
    0.0f,
    9.0816984e-06f,
    2.3e-08f,
    6.3e-08f,
    7.04e-07f,
    2.5e-07f,
    7.9e-07f,
    2.4142129f,
    1.4966058f,
    1.0f,
    0.66817862f,
    0.41421357f,
    0.19891237f,
    5.62e-07f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
};

f32 fabsf(f32 x)
{
    return __fabsf(x);
}

f32 atan(f32 x)
{
    f32 y;
    f32 ysq;
    s32 i;
    int recip;
    s32 raw;
    s32 sign;

    i = -1;
    recip = 0;
    raw = __HI(x);
    __HI(x) = raw & 0x7FFFFFFF;
    sign = raw & 0x80000000;

    if (x >= 2.4142136f) {
        recip = 1;
        y = 1.0f / x;
    } else if (0.41421357f < x) {
        s32 hx = __HI(x);

        i = 0;
        switch (hx & 0x7F800000) {
        case 0x3F000000:
            if (hx >= 0x3F08D5B9) {
                i = 1;
            }
            if (hx >= 0x3F521801) {
                i += 1;
            }
            break;
        case 0x3F800000:
            i = 2;
            if (hx >= 0x3F9BF7EC) {
                i = 3;
            }
            if (hx >= 0x3FEF789E) {
                i += 1;
            }
            break;
        case 0x40000000:
            i = 4;
            break;
        }

        y = 1.0f / (atan_tbl[i + 33] + (x + atan_tbl[i + 39]));
        y = (atan_tbl[i + 33] - y * atan_tbl[i + 7]) +
            (atan_tbl[i + 39] - y * atan_tbl[i + 13]);
    } else {
        y = x;
    }

    ysq = y * y;
    y = y * ysq *
            (ysq * (ysq * (ysq * (ysq * (ysq * atan_tbl[6] + atan_tbl[5]) +
                                  atan_tbl[4]) +
                           atan_tbl[3]) +
                    atan_tbl[2]) +
             atan_tbl[1]) +
        y;
    y = y + atan_tbl[i + 27];
    y = y + atan_tbl[i + 20];

    if (recip) {
        y = y - 1.5707964f;
        if (sign) {
            return y;
        }
        return -y;
    }
    __HI(y) |= sign;
    return y;
}

f32 atanf(f32 x)
{
    return atan(x);
}

static f32 __rsqrtf(f32 x)
{
    f32 e;

    if (x > 0.0f) {
        e = __frsqrte(x);
        e = 0.5f * e * (3.0f - x * (e * e));
        e = 0.5f * e * (3.0f - x * (e * e));
        e = 0.5f * e * (3.0f - x * (e * e));
        return e;
    }
    if (!x) {
        goto huge;
    }
    return __float_nan.f;

huge:
    return __float_huge.f;
}

#pragma dont_inline on
f32 acosf(f32 x)
{
    return 1.5707964f - atanf(x * __rsqrtf(1.0f - x * x));
}
#pragma dont_inline reset

f32 atan2(f32 y, f32 x)
{
    s32 sx = __HI(x) & 0x80000000;
    s32 sy = __HI(y) & 0x80000000;

    if (sx == sy) {
        if (sx) {
            return atan(y / x) - 3.1415927f;
        }
        if (!x) {
            goto half;
        }
        return atan(y / x);

    half:
        return 1.5707964f;
    }
    if (x < 0.0f) {
        return 3.1415927f + atan(y / x);
    }
    if (!x) {
        goto pio2;
    }
    return atan(y / x);

pio2:
    __HI(y) = sy + 0x3FC90FDB;
    return y;
}

f32 floorf(f32 x)
{
    s32 n = (s32) x;
    f32 d = (f32) n - x;

    if (*(s32*) &d != 0) {
        s32 hx = __HI(x);

        if ((hx & 0x7F800000) < 0x4B800000) {
            goto adjust;
        }
        return x;

    adjust:
        if (hx & 0x80000000) {
            return (f32) --n;
        }
        return (f32) n;
    }
    return x;
}
