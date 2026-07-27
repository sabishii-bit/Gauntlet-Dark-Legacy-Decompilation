/*
 * ml_ffsincos.c - Midway fast float sin/cos (ml_ffsincos.obj).
 *
 * ffsin: polynomial sin over a reduced range with an input-range guard that
 * reports 'sin bad input: %f' via ErrorPrintf; ffcos(x) = ffsin(x + quarter turn).
 * (Xbox SIN_POLY is inlined into ffsin on GC.)
 *
 * Range 0x800BC8D8..0x800BCAAC (2 fns), between ml_error.c and ml_fmath.c. Owns
 * .sdata2 pool 0x80348D68..0x80348D98 (sin polynomial coefficients). Names from
 * Xbox shell3D PDB (ml_ffsincos.obj). cflags_demo, C++ exceptions on.
 *
 * Status: Matching.
 */

extern void ErrorPrintf(const char* fmt, ...);

float ffsin(float x);

/* 0x800BC8D8 */
float ffcos(float x)
{
    return ffsin((float)(1.570796327 + x));
}

/* 0x800BC904 */
float ffsin(float x)
{
    float p;
    float x2;
    float y;

    if (!((x <= 0.0f) || (x >= 0.0f))) {
        ErrorPrintf("sin bad input: %f", x);
        return 0.0f;
    }

    while (x < 0.0) {
        x += 6.283185308;
    }

    while (x >= 6.283185308) {
        x = (float)(x - 6.283185308);
    }

    if (x < 1.570796327) {
        x2 = x * x;
        p = x2;
        p *= 0.0000027557318844628753f;
        p = -0.00019841270113829523f + p;
        p = x2 * p;
        p = 0.008333333767950535f + p;
        p = x2 * p;
        p = -0.1666666716337204f + p;
        p = x2 * p;
        return x + x * p;
    }

    if (x < 3.141592654) {
        x = (float)(3.141592654 - x);
        x2 = x * x;
        p = x2;
        p *= 0.0000027557318844628753f;
        p = -0.00019841270113829523f + p;
        p = x2 * p;
        p = 0.008333333767950535f + p;
        p = x2 * p;
        p = -0.1666666716337204f + p;
        p = x2 * p;
        return x + x * p;
    }

    if (x < 4.712388981) {
        y = (float)(x - 3.141592654);
        x2 = y * y;
        p = x2;
        p *= 0.0000027557318844628753f;
        p = -0.00019841270113829523f + p;
        p = x2 * p;
        p = 0.008333333767950535f + p;
        p = x2 * p;
        p = -0.1666666716337204f + p;
        p = x2 * p;
        p = y + y * p;
        return -p;
    }

    y = (float)(6.283185308 - x);
    x2 = y * y;
    p = x2;
    p *= 0.0000027557318844628753f;
    p = -0.00019841270113829523f + p;
    p = x2 * p;
    p = 0.008333333767950535f + p;
    p = x2 * p;
    p = -0.1666666716337204f + p;
    p = x2 * p;
    p = y + y * p;
    return -p;
}
