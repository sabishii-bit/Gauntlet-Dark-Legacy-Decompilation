/*
 * math1.c - MSL double-precision math core (fdlibm-derived).
 * Function order follows the DOL; dont_inline keeps the leaf calls.
 */
#include "types.h"

#pragma dont_inline on

#define __HI(x) (((s32*) &(x))[0])
#define __LO(x) (((u32*) &(x))[1])

double fabs(double x);
double ldexp(double x, int n);
double scalbn(double x, int n);
double copysign(double x, double y);
double floor(double x);
int __fpclassifyd(double x);
int __ieee754_rem_pio2(double x, double* y);
int __kernel_rem_pio2(double* x, double* y, int e0, int nx, int prec, const int* ipio2);
double __kernel_sin(double x, double y, int iy);
double __kernel_cos(double x, double y);
double __kernel_tan(double x, double y, int iy);

double fabs(double x)
{
    return __fabs(x);
}

double ldexp(double x, int n)
{
    return scalbn(x, n);
}

static const int npio2_hw[] = {
    0x3FF921FB, 0x400921FB, 0x4012D97C, 0x401921FB, 0x401F6A7A, 0x4022D97C,
    0x4025FDBB, 0x402921FB, 0x402C463A, 0x402F6A7A, 0x4031475C, 0x4032D97C,
    0x40346B9C, 0x4035FDBB, 0x40378FDB, 0x403921FB, 0x403AB41B, 0x403C463A,
    0x403DD85A, 0x403F6A7A, 0x40407E4C, 0x4041475C, 0x4042106C, 0x4042D97C,
    0x4043A28C, 0x40446B9C, 0x404534AC, 0x4045FDBB, 0x4046C6CB, 0x40478FDB,
    0x404858EB, 0x404921FB,
};

static const int two_over_pi[] = {
    0xA2F983, 0x6E4E44, 0x1529FC, 0x2757D1, 0xF534DD, 0xC0DB62,
    0x95993C, 0x439041, 0xFE5163, 0xABDEBB, 0xC561B7, 0x246E3A,
    0x424DD2, 0xE00649, 0x2EEA09, 0xD1921C, 0xFE1DEB, 0x1CB129,
    0xA73EE8, 0x8235F5, 0x2EBB44, 0x84E99C, 0x7026B4, 0x5F7E41,
    0x3991D6, 0x398353, 0x39F49C, 0x845F8B, 0xBDF928, 0x3B1FF8,
    0x97FFDE, 0x05980F, 0xEF2F11, 0x8B5A0A, 0x6D1F6D, 0x367ECF,
    0x27CB09, 0xB74F46, 0x3F669E, 0x5FEA2D, 0x7527BA, 0xC7EBE5,
    0xF17B3D, 0x0739F7, 0x8A5292, 0xEA6BFB, 0x5FB11F, 0x8D5D08,
    0x560330, 0x46FC7B, 0x6BABF0, 0xCFBC20, 0x9AF436, 0x1DA9E3,
    0x91615E, 0xE61B08, 0x659985, 0x5F14A0, 0x68408D, 0xFFD880,
    0x4D7327, 0x310606, 0x1556CA, 0x73A8C9, 0x60E27B, 0xC08C6B,
};

int __ieee754_rem_pio2(double x, double* y)
{
    static const double zero = 0.00000000000000000000e+00;
    static const double half = 5.00000000000000000000e-01;
    static const double two24 = 1.67772160000000000000e+07;
    static const double invpio2 = 6.36619772367581382433e-01;
    static const double pio2_1 = 1.57079632673412561417e+00;
    static const double pio2_1t = 6.07710050650619224932e-11;
    static const double pio2_2 = 6.07710050630396597660e-11;
    static const double pio2_2t = 2.02226624879595063154e-21;
    static const double pio2_3 = 2.02226624871116645580e-21;
    static const double pio2_3t = 8.47842766036889956997e-32;

    double z, w, t, r, fn;
    double tx[3];
    int e0, i, j, nx, n, ix, hx;

    hx = __HI(x);
    ix = hx & 0x7fffffff;
    if (ix <= 0x3fe921fb) {
        y[0] = x;
        y[1] = 0;
        return 0;
    }
    if (ix < 0x4002d97c) {
        if (hx > 0) {
            z = x - pio2_1;
            if (ix != 0x3ff921fb) {
                y[0] = z - pio2_1t;
                y[1] = (z - y[0]) - pio2_1t;
            } else {
                z -= pio2_2;
                y[0] = z - pio2_2t;
                y[1] = (z - y[0]) - pio2_2t;
            }
            return 1;
        } else {
            z = x + pio2_1;
            if (ix != 0x3ff921fb) {
                y[0] = z + pio2_1t;
                y[1] = (z - y[0]) + pio2_1t;
            } else {
                z += pio2_2;
                y[0] = z + pio2_2t;
                y[1] = (z - y[0]) + pio2_2t;
            }
            return -1;
        }
    }
    if (ix <= 0x413921fb) {
        t = fabs(x);
        n = (int) (t * invpio2 + half);
        fn = (double) n;
        r = t - fn * pio2_1;
        w = fn * pio2_1t;
        if (n < 32 && ix != npio2_hw[n - 1]) {
            y[0] = r - w;
        } else {
            j = ix >> 20;
            y[0] = r - w;
            i = j - (((__HI(y[0])) >> 20) & 0x7ff);
            if (i > 16) {
                t = r;
                w = fn * pio2_2;
                r = t - w;
                w = fn * pio2_2t - ((t - r) - w);
                y[0] = r - w;
                i = j - (((__HI(y[0])) >> 20) & 0x7ff);
                if (i > 49) {
                    t = r;
                    w = fn * pio2_3;
                    r = t - w;
                    w = fn * pio2_3t - ((t - r) - w);
                    y[0] = r - w;
                }
            }
        }
        y[1] = (r - y[0]) - w;
        if (hx < 0) {
            y[0] = -y[0];
            y[1] = -y[1];
            return -n;
        } else {
            return n;
        }
    }
    if (ix >= 0x7ff00000) {
        y[0] = y[1] = x - x;
        return 0;
    }
    z = x;
    e0 = (ix >> 20) - 1046;
    __HI(z) = ix - (e0 << 20);
    for (i = 0; i < 2; i++) {
        tx[i] = (double) ((int) (z));
        z = (z - tx[i]) * two24;
    }
    tx[2] = z;
    nx = 3;
    while (tx[nx - 1] == zero) {
        nx--;
    }
    n = __kernel_rem_pio2(tx, y, e0, nx, 2, two_over_pi);
    if (hx < 0) {
        y[0] = -y[0];
        y[1] = -y[1];
        return -n;
    }
    return n;
}

int __fpclassifyd(double x)
{
    /* PARKED: 6-word emission-order residual (4/5 arm order) */
    switch (__HI(x) & 0x7ff00000) {
    case 0x7ff00000:
        if ((__HI(x) & 0x000fffff) || ((s32) __LO(x) != 0)) {
            return 1;
        }
        return 2;
    default:
        return 5;
    case 0x00000000:
        if ((__HI(x) & 0x000fffff) || ((s32) __LO(x) != 0)) {
            return 4;
        }
        return 3;
    }
}

double __kernel_cos(double x, double y)
{
    static const double one = 1.00000000000000000000e+00;
    static const double C1 = 4.16666666666666019037e-02;
    static const double C2 = -1.38888888888741095749e-03;
    static const double C3 = 2.48015872894767294178e-05;
    static const double C4 = -2.75573143513906633035e-07;
    static const double C5 = 2.08757232129817482790e-09;
    static const double C6 = -1.13596475577881948265e-11;

    double a, hz, z, r, qx;
    int ix;

    ix = __HI(x) & 0x7fffffff;
    if (ix < 0x3e400000) {
        if (((int) x) == 0) {
            return one;
        }
    }
    z = x * x;
    r = z * (C1 + z * (C2 + z * (C3 + z * (C4 + z * (C5 + z * C6)))));
    if (ix < 0x3FD33333) {
        return one - (0.5 * z - (z * r - x * y));
    } else {
        if (ix > 0x3fe90000) {
            qx = 0.28125;
        } else {
            __HI(qx) = ix - 0x00200000;
            __LO(qx) = 0;
        }
        hz = 0.5 * z - qx;
        a = one - qx;
        return a - (hz - (z * r - x * y));
    }
}

int __kernel_rem_pio2(double* x, double* y, int e0, int nx, int prec, const int* ipio2)
{
    static const int init_jk[] = {2, 3, 4, 6};
    static const double PIo2[] = {
        1.57079625129699707031e+00, 7.54978941586159635335e-08,
        5.39030252995776476554e-15, 3.28200341580791294123e-22,
        1.27065575308067607349e-29, 1.22933308981111328932e-36,
        2.73370053816464559624e-44, 2.16741683877804819444e-51,
    };
    static const double zero = 0.0;
    static const double one = 1.0;
    static const double two24 = 1.67772160000000000000e+07;
    static const double twon24 = 5.96046447753906250000e-08;

    int jz, jx, jv, jp, jk, carry, n, iq[20], i, j, k, m, q0, ih;
    double z, fw, f[20], fq[20], q[20];

    jk = init_jk[prec];
    jp = jk;

    jx = nx - 1;
    jv = (e0 - 3) / 24;
    if (jv < 0) {
        jv = 0;
    }
    q0 = e0 - 24 * (jv + 1);

    j = jv - jx;
    m = jx + jk;
    for (i = 0; i <= m; i++, j++) {
        f[i] = (j < 0) ? zero : (double) ipio2[j];
    }

    for (i = 0; i <= jk; i++) {
        for (j = 0, fw = 0.0; j <= jx; j++) {
            fw += x[j] * f[jx + i - j];
        }
        q[i] = fw;
    }

    jz = jk;
recompute:
    for (i = 0, j = jz, z = q[jz]; j > 0; i++, j--) {
        fw = (double) ((int) (twon24 * z));
        iq[i] = (int) (z - two24 * fw);
        z = q[j - 1] + fw;
    }

    z = scalbn(z, q0);
    z -= 8.0 * floor(z * 0.125);
    n = (int) z;
    z -= (double) n;
    ih = 0;
    if (q0 > 0) {
        i = (iq[jz - 1] >> (24 - q0));
        n += i;
        iq[jz - 1] -= i << (24 - q0);
        ih = iq[jz - 1] >> (23 - q0);
    } else if (q0 == 0) {
        ih = iq[jz - 1] >> 23;
    } else if (z >= 0.5) {
        ih = 2;
    }

    if (ih > 0) {
        n += 1;
        carry = 0;
        for (i = 0; i < jz; i++) {
            j = iq[i];
            if (carry == 0) {
                if (j != 0) {
                    carry = 1;
                    iq[i] = 0x1000000 - j;
                }
            } else {
                iq[i] = 0xffffff - j;
            }
        }
        if (q0 > 0) {
            switch (q0) {
            case 1:
                iq[jz - 1] &= 0x7fffff;
                break;
            case 2:
                iq[jz - 1] &= 0x3fffff;
                break;
            }
        }
        if (ih == 2) {
            z = one - z;
            if (carry != 0) {
                z -= scalbn(one, q0);
            }
        }
    }

    if (z == zero) {
        j = 0;
        for (i = jz - 1; i >= jk; i--) {
            j |= iq[i];
        }
        if (j == 0) {
            for (k = 1; iq[jk - k] == 0; k++) {
            }

            for (i = jz + 1; i <= jz + k; i++) {
                f[jx + i] = (double) ipio2[jv + i];
                for (j = 0, fw = 0.0; j <= jx; j++) {
                    fw += x[j] * f[jx + i - j];
                }
                q[i] = fw;
            }
            jz += k;
            goto recompute;
        }
    }

    if (z == 0.0) {
        jz -= 1;
        q0 -= 24;
        while (iq[jz] == 0) {
            jz--;
            q0 -= 24;
        }
    } else {
        z = scalbn(z, -q0);
        if (z >= two24) {
            fw = (double) ((int) (twon24 * z));
            iq[jz] = (int) (z - two24 * fw);
            jz += 1;
            q0 += 24;
            iq[jz] = (int) fw;
        } else {
            iq[jz] = (int) z;
        }
    }

    fw = scalbn(one, q0);
    for (i = jz; i >= 0; i--) {
        q[i] = fw * (double) iq[i];
        fw *= twon24;
    }

    for (i = jz; i >= 0; i--) {
        for (fw = 0.0, k = 0; k <= jp && k <= jz - i; k++) {
            fw += PIo2[k] * q[i + k];
        }
        fq[jz - i] = fw;
    }

    switch (prec) {
    case 0:
        fw = 0.0;
        for (i = jz; i >= 0; i--) {
            fw += fq[i];
        }
        y[0] = (ih == 0) ? fw : -fw;
        break;
    case 1:
    case 2:
        fw = 0.0;
        for (i = jz; i >= 0; i--) {
            fw += fq[i];
        }
        y[0] = (ih == 0) ? fw : -fw;
        fw = fq[0] - fw;
        for (i = 1; i <= jz; i++) {
            fw += fq[i];
        }
        y[1] = (ih == 0) ? fw : -fw;
        break;
    case 3:
        for (i = jz; i > 0; i--) {
            fw = fq[i - 1] + fq[i];
            fq[i] += fq[i - 1] - fw;
            fq[i - 1] = fw;
        }
        for (i = jz; i > 1; i--) {
            fw = fq[i - 1] + fq[i];
            fq[i] += fq[i - 1] - fw;
            fq[i - 1] = fw;
        }
        for (fw = 0.0, i = jz; i >= 2; i--) {
            fw += fq[i];
        }
        if (ih == 0) {
            y[0] = fq[0];
            y[1] = fq[1];
            y[2] = fw;
        } else {
            y[0] = -fq[0];
            y[1] = -fq[1];
            y[2] = -fw;
        }
    }
    return n & 7;
}

double __kernel_sin(double x, double y, int iy)
{
    static const double half = 5.00000000000000000000e-01;
    static const double S1 = -1.66666666666666324348e-01;
    static const double S2 = 8.33333333332248946124e-03;
    static const double S3 = -1.98412698298579493134e-04;
    static const double S4 = 2.75573137070700676789e-06;
    static const double S5 = -2.50507602534068634195e-08;
    static const double S6 = 1.58969099521155010221e-10;

    double z, r, v;
    int ix;

    ix = __HI(x) & 0x7fffffff;
    if (ix < 0x3e400000) {
        if ((int) x == 0) {
            return x;
        }
    }
    z = x * x;
    v = z * x;
    r = S2 + z * (S3 + z * (S4 + z * (S5 + z * S6)));
    if (iy == 0) {
        return x + v * (S1 + z * r);
    } else {
        return x - ((z * (half * y - v * r) - y) - v * S1);
    }
}

double __kernel_tan(double x, double y, int iy)
{
    static const double one = 1.00000000000000000000e+00;
    static const double pio4 = 7.85398163397448278999e-01;
    static const double pio4lo = 3.06161699786838301793e-17;
    static const double T[] = {
        3.33333333333334091986e-01,  1.33333333333201242699e-01,
        5.39682539762260521377e-02,  2.18694882948595424599e-02,
        8.86323982359930005737e-03,  3.59207910759131235356e-03,
        1.45620945432529025516e-03,  5.88041240820264096874e-04,
        2.46463134818469906812e-04,  7.81794442939557092300e-05,
        7.14072491382608190305e-05,  -1.85586374855275456654e-05,
        2.59073051863633712884e-05,
    };

    double z, r, v, w, s;
    int ix, hx;

    hx = __HI(x);
    ix = hx & 0x7fffffff;
    if (ix < 0x3e300000) {
        if ((int) x == 0) {
            if (((ix | __LO(x)) | (iy + 1)) == 0) {
                return one / fabs(x);
            } else {
                return (iy == 1) ? x : -one / x;
            }
        }
    }
    if (ix >= 0x3FE59428) {
        if (hx < 0) {
            x = -x;
            y = -y;
        }
        z = pio4 - x;
        w = pio4lo - y;
        x = z + w;
        y = 0.0;
    }
    z = x * x;
    w = z * z;
    r = T[1] + w * (T[3] + w * (T[5] + w * (T[7] + w * (T[9] + w * T[11]))));
    v = z * (T[2] + w * (T[4] + w * (T[6] + w * (T[8] + w * (T[10] + w * T[12])))));
    s = z * x;
    r = y + z * (s * (r + v) + y);
    r += T[0] * s;
    w = x + r;
    if (ix >= 0x3FE59428) {
        v = (double) iy;
        return (double) (1 - ((hx >> 30) & 2)) * (v - 2.0 * (x - (w * w / (w + v) - r)));
    }
    if (iy == 1) {
        return w;
    } else {
        double a, t;
        z = w;
        __LO(z) = 0;
        v = r - (z - x);
        t = a = -one / w;
        __LO(t) = 0;
        s = one + t * z;
        return t + a * (s + t * v);
    }
}

double __atan(double x)
{
    static const double atanhi[] = {
        4.63647609000806093515e-01, 7.85398163397448278999e-01,
        9.82793723247329054082e-01, 1.57079632679489655800e+00,
    };
    static const double atanlo[] = {
        2.26987774529616870924e-17, 3.06161699786838301793e-17,
        1.39033110312309984516e-17, 6.12323399573676603587e-17,
    };
    static const double aT[] = {
        3.33333333333329318027e-01,  -1.99999999998764832476e-01,
        1.42857142725034663711e-01,  -1.11111104054623557880e-01,
        9.09088713343650656196e-02,  -7.69187620504482999495e-02,
        6.66107313738753120669e-02,  -5.83357013379057348645e-02,
        4.97687799461593236017e-02,  -3.65315727442169155270e-02,
        1.62858201153657823623e-02,
    };
    static const double one = 1.0;
    static const double huge = 1.0e300;

    double w, s1, s2, z;
    int ix, hx, id;

    hx = __HI(x);
    ix = hx & 0x7fffffff;
    if (ix >= 0x44100000) {
        if (ix > 0x7ff00000 || (ix == 0x7ff00000 && (__LO(x) != 0))) {
            return x + x;
        }
        if (hx > 0) {
            return atanhi[3] + atanlo[3];
        } else {
            return -atanhi[3] - atanlo[3];
        }
    }
    if (ix < 0x3fdc0000) {
        if (ix < 0x3e200000) {
            if (huge + x > one) {
                return x;
            }
        }
        id = -1;
    } else {
        x = fabs(x);
        if (ix < 0x3ff30000) {
            if (ix < 0x3fe60000) {
                id = 0;
                x = (2.0 * x - one) / (2.0 + x);
            } else {
                id = 1;
                x = (x - one) / (x + one);
            }
        } else {
            if (ix < 0x40038000) {
                id = 2;
                x = (x - 1.5) / (one + 1.5 * x);
            } else {
                id = 3;
                x = -1.0 / x;
            }
        }
    }
    z = x * x;
    w = z * z;
    s1 = z * (aT[0] + w * (aT[2] + w * (aT[4] + w * (aT[6] + w * (aT[8] + w * aT[10])))));
    s2 = w * (aT[1] + w * (aT[3] + w * (aT[5] + w * (aT[7] + w * aT[9]))));
    if (id < 0) {
        return x - x * (s1 + s2);
    } else {
        z = atanhi[id] - ((x * (s1 + s2) - atanlo[id]) - x);
        return (hx < 0) ? -z : z;
    }
}

double copysign(double x, double y)
{
    __HI(x) = (__HI(x) & 0x7fffffff) | (__HI(y) & 0x80000000);
    return x;
}

double __cos(double x)
{
    double y[2], z = 0.0;
    int n, ix;

    ix = __HI(x);
    ix &= 0x7fffffff;
    if (ix <= 0x3fe921fb) {
        return __kernel_cos(x, z);
    } else if (ix >= 0x7ff00000) {
        return x - x;
    } else {
        n = __ieee754_rem_pio2(x, y);
        switch (n & 3) {
        case 0:
            return __kernel_cos(y[0], y[1]);
        case 1:
            return -__kernel_sin(y[0], y[1], 1);
        case 2:
            return -__kernel_cos(y[0], y[1]);
        default:
            return __kernel_sin(y[0], y[1], 1);
        }
    }
}

double floor(double x)
{
    static const double huge = 1.0e300;

    int i0, i1, j0;
    unsigned int i, j;

    i0 = __HI(x);
    i1 = __LO(x);
    j0 = ((i0 >> 20) & 0x7ff) - 0x3ff;
    if (j0 < 20) {
        if (j0 < 0) {
            if (huge + x > 0.0) {
                if (i0 >= 0) {
                    i0 = i1 = 0;
                } else if (((i0 & 0x7fffffff) | i1) != 0) {
                    i0 = 0xbff00000;
                    i1 = 0;
                }
            }
        } else {
            i = (0x000fffff) >> j0;
            if (((i0 & i) | i1) == 0) {
                return x;
            }
            if (huge + x > 0.0) {
                if (i0 < 0) {
                    i0 += (0x00100000) >> j0;
                }
                i0 &= (~i);
                i1 = 0;
            }
        }
    } else if (j0 > 51) {
        if (j0 == 0x400) {
            return x + x;
        } else {
            return x;
        }
    } else {
        i = ((unsigned int) (0xffffffff)) >> (j0 - 20);
        if ((i1 & i) == 0) {
            return x;
        }
        if (huge + x > 0.0) {
            if (i0 < 0) {
                if (j0 == 20) {
                    i0 += 1;
                } else {
                    j = i1 + (1 << (52 - j0));
                    if (j < (unsigned int) i1) {
                        i0 += 1;
                    }
                    i1 = j;
                }
            }
            i1 &= (~i);
        }
    }
    __HI(x) = i0;
    __LO(x) = i1;
    return x;
}

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

double scalbn(double x, int n)
{
    static const double two54 = 1.80143985094819840000e+16;
    static const double twom54 = 5.55111512312578270212e-17;
    static const double huge = 1.0e+300;
    static const double tiny = 1.0e-300;

    int k, hx, lx;

    if (__fpclassifyd(x) <= 2 || 0.0 == x) {
        return x;
    }
    hx = __HI(x);
    lx = __LO(x);
    k = (hx & 0x7ff00000) >> 20;
    if (k == 0) {
        if ((lx | (hx & 0x7fffffff)) == 0) {
            return x;
        }
        x *= two54;
        hx = __HI(x);
        k = ((hx & 0x7ff00000) >> 20) - 54;
        if (n < -50000) {
            return tiny * x;
        }
    }
    k = k + n;
    if (k > 0x7fe) {
        return huge * copysign(huge, x);
    }
    if (k > 0) {
        __HI(x) = (hx & 0x800fffff) | (k << 20);
        return x;
    }
    if (k <= -54) {
        if (n > 50000) {
            return huge * copysign(huge, x);
        } else {
            return tiny * copysign(tiny, x);
        }
    }
    k += 54;
    __HI(x) = (hx & 0x800fffff) | (k << 20);
    return x * twom54;
}

double modf(double x, double* iptr)
{
    static const double one = 1.0;

    int i0, i1, j0;
    unsigned int i;

    i0 = __HI(x);
    i1 = __LO(x);
    j0 = ((i0 >> 20) & 0x7ff) - 0x3ff;
    if (j0 < 20) {
        if (j0 < 0) {
            __HI(*iptr) = i0 & 0x80000000;
            __LO(*iptr) = 0;
            return x;
        } else {
            i = (0x000fffff) >> j0;
            if (((i0 & i) | i1) == 0) {
                *iptr = x;
                __HI(x) &= 0x80000000;
                __LO(x) = 0;
                return x;
            } else {
                __HI(*iptr) = i0 & (~i);
                __LO(*iptr) = 0;
                return x - *iptr;
            }
        }
    } else if (j0 > 51) {
        *iptr = x * one;
        __HI(x) &= 0x80000000;
        __LO(x) = 0;
        return x;
    } else {
        i = ((unsigned int) (0xffffffff)) >> (j0 - 20);
        if ((i1 & i) == 0) {
            *iptr = x;
            __HI(x) &= 0x80000000;
            __LO(x) = 0;
            return x;
        } else {
            __HI(*iptr) = i0;
            __LO(*iptr) = i1 & (~i);
            return x - *iptr;
        }
    }
}

double __sin(double x)
{
    double y[2], z = 0.0;
    int n, ix;

    ix = __HI(x);
    ix &= 0x7fffffff;
    if (ix <= 0x3fe921fb) {
        return __kernel_sin(x, z, 0);
    } else if (ix >= 0x7ff00000) {
        return x - x;
    } else {
        n = __ieee754_rem_pio2(x, y);
        switch (n & 3) {
        case 0:
            return __kernel_sin(y[0], y[1], 1);
        case 1:
            return __kernel_cos(y[0], y[1]);
        case 2:
            return -__kernel_sin(y[0], y[1], 1);
        default:
            return -__kernel_cos(y[0], y[1]);
        }
    }
}

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
