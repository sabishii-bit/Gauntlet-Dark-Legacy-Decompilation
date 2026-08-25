/*
 * ml_fmath.c - Midway float matrix/vector math library (ML_FMATH.OBJ).
 *
 * The big Midway 3D math library: 3x3/4x4 matrix build/copy/mul/invert, yaw/pitch/
 * roll rotation-matrix builders (sin/cos), body<->world vector transforms,
 * PYR/YPR/RYP euler<->matrix (Create.../Extract... helpers), angle helpers, and the RNG
 * (Random/RandInt/Randomize over pbRand/srand).
 *
 * Range 0x800BCAAC..0x800BE964 (44 fns). Anchored via unique shape/callees:
 * RandInt/Random/Randomize (pbRand/srand), MulMat4 (mat44Mult), CopyMat3
 * (sceSamp0CopyMatrix34) / CopyMat4 (mat44::operator=). GC emits this module in
 * near-reverse PDB order with heavy inlining; the rotation-matrix and euler
 * helpers are left fn_ pending exact per-function verification. Calls the g3dMath3D
 * C++ mat44 primitives. cflags_demo, C++ exceptions on.
 *
 * Status: NonMatching. Core vector/matrix primitives are translated; the
 * Euler extraction/construction block remains stubbed.
 */
#include "game/ml_fmath.h"
#include "types.h"

extern void srand(u32 seed);
extern u32 pbRand(void);
extern u32 lbl_80344F08; /* sRandom */
extern f32 sYawCos;
extern f32 sYawSin;
extern f64 sqrt(f64 x);
extern f64 __fabs(f64 x);
extern f64 __frsqrte(f64 x);
extern f32 sin(f32 x);
extern f32 cos(f32 x);
extern f32 atan2(f32 y, f32 x);
extern f32 ffsin(f32 x);
extern f32 ffcos(f32 x);
extern void sceSamp0TransposeMatrix(f32* dst, const f32* src);
extern void sceSamp0CopyMatrix34(f32* dst, const f32* src);
extern void mat44Mult__FR5mat44R5mat44R5mat44(f32* dst, f32* lhs, f32* rhs);
extern void __as__5mat44FRC5mat44(f32* dst, const f32* src);
extern const f32 gIdentityMatrix[];
extern const f32 gMlFmathZero;
extern const f64 lbl_80348DA0;
extern const f64 lbl_80348DB0;
extern const f64 lbl_80348DB8;
extern const f64 lbl_80348DC0;
extern const f64 lbl_80348DC8;
extern const f64 lbl_80348DD0;
extern const f64 lbl_80348DD8;
extern const f64 lbl_80348DE0;
extern const f64 lbl_80348DE8;
extern const f64 lbl_80348DF0;
extern const f64 lbl_80348DF8;
extern volatile const f64 lbl_80348E00;
extern const f64 lbl_80348E08;
extern const f64 lbl_80348E10;
extern const f64 lbl_80348E18;
extern const f64 lbl_80348E20;
extern const f64 lbl_80348E28;
extern const f64 lbl_80348E30;
extern const f64 lbl_80348E38;
extern const f64 lbl_80348E40;
extern const f64 lbl_80348E48;
extern const f64 lbl_80348E50;
extern const f64 lbl_80348E58;
extern const f64 lbl_80348E60;
extern const f64 lbl_80348E68;
extern const f64 lbl_80348E78;
extern const f64 lbl_80348E80;
extern const f64 lbl_80348E88;
extern const f32 lbl_80348E90;
extern volatile const f64 lbl_80348E98;
extern const f64 lbl_80348EA0;
extern const f64 lbl_80348EA8;
extern const f64 lbl_80348EB0;
extern const f64 lbl_80348EB8;
extern const f64 lbl_80348EC0;
extern const f64 lbl_80348EC8;
extern const f64 lbl_80348ED0;

#define ML_ZERO_D (*(const f64*)&lbl_80348E00)

/* 0x800BCAAC - piecewise square-root approximation */
f32 smallsqrt(f32 value)
{
    if (value <= lbl_80348DA0)
        return gMlFmathZero;
    if (value <= lbl_80348DB0) {
        if (value <= lbl_80348DB8) {
            if (value <= lbl_80348DC0)
                return (f32)(lbl_80348DC8 * value);
            return (f32)(lbl_80348DD8 * value + lbl_80348DD0);
        }
        return (f32)(lbl_80348DE8 * value + lbl_80348DE0);
    }
    return (f32)(lbl_80348DF8 * (value - lbl_80348DB0) + lbl_80348DF0);
}

/* 0x800BCB44 - fast two-dimensional distance approximation */
f32 fqdist(f32 x, f32 y)
{
    f32 hi;

    if (x < lbl_80348E00)
        x = -x;
    if (y < lbl_80348E00)
        y = -y;
    if (x < lbl_80348DA0)
        return y;
    if (y < lbl_80348DA0)
        return x;

    if (x >= y) {
        hi = x;
    } else {
        hi = y;
        y = x;
    }

    if (y <= lbl_80348DB0 * hi) {
        if (y <= lbl_80348DB8 * hi) {
            if (y <= lbl_80348DC0 * hi)
                return (f32)(lbl_80348E08 * y + hi);
            return (f32)(lbl_80348E10 * y + hi);
        }
        if (y <= lbl_80348E18 * hi)
            return (f32)(lbl_80348E20 * y + hi);
        return (f32)(lbl_80348E28 * y + hi);
    }
    if (y <= lbl_80348E30 * hi) {
        if (y <= lbl_80348E38 * hi)
            return (f32)(lbl_80348E40 * y + hi);
        return (f32)(lbl_80348E48 * y + hi);
    }
    if (y <= lbl_80348E50 * hi)
        return (f32)(lbl_80348E58 * y + hi);
    return (f32)(lbl_80348E60 * y + hi);
}

/* 0x800BCCA8 */
u32 RandInt(u32 n) {
    lbl_80344F08 = pbRand();
    return lbl_80344F08 % n;
}

/* 0x800BCCE8 */
f32 Random(f32 scale) {
    lbl_80344F08 = pbRand();
    return (lbl_80344F08 & 0x7FFF) * scale / lbl_80348E68;
}

/* 0x800BCD48 */
void Randomize(u32 seed) {
    srand(seed);
}

/* 0x800BCD68 - extract yaw/pitch/roll angles from a 4x4 matrix. */
void ExtractYPR(const f32* matrix, f32* angles)
{
    f32 absValue = matrix[9];
    u8 unused[48];
    f32 angle2;
    f32 angle0;
    f32 angle1;
    f32 magnitude;
    f32 r;
    f32 y;
    f32 x;

    *(u32*)&absValue &= 0x7FFFFFFF;
    if (__fabs(lbl_80348E78 - absValue) < lbl_80348DA0) {
        f64 lockedAngle;
        angle1 = atan2(-matrix[2], matrix[0]);
        if (matrix[9] > gMlFmathZero) {
            lockedAngle = lbl_80348E80;
        } else {
            lockedAngle = lbl_80348E88;
        }
        angle0 = lockedAngle;
        angle2 = *(volatile f32*)&gMlFmathZero;
    } else {
        angle2 = atan2(-matrix[1], matrix[5]);
        magnitude = cos(angle2);
        if (ML_ZERO_D == magnitude) {
            if (angle2 > ML_ZERO_D) {
                f32 a = -matrix[1];
                r = atan2(matrix[9], a);
                x = -matrix[4];
                y = matrix[6];
                angle0 = r;
                angle1 = atan2(y, x);
            } else {
                f32 rr;
                f32 yy;
                x = matrix[1];
                rr = atan2(matrix[9], x);
                yy = matrix[6];
                yy = -yy;
                x = matrix[4];
                angle0 = rr;
                angle1 = atan2(yy, x);
            }
        } else {
            f32 yy;
            f32 rr;
            magnitude = matrix[5] / magnitude;
            y = matrix[9];
            rr = atan2(y, magnitude);
            yy = matrix[8];
            x = matrix[10];
            yy = yy / magnitude;
            x = x / magnitude;
            angle0 = rr;
            angle1 = atan2(yy, x);
        }
    }
    angles[0] = angle0;
    angles[1] = angle1;
    angles[2] = angle2;
}

/* 0x800BCED8 - extract pitch/yaw/roll angles from a 4x4 matrix. */
void ExtractPYR(const f32* matrix, f32* angles)
{
    f32 absValue = matrix[2];
    u8 unused[48];
    f32 angle0;
    f32 angle1;
    f32 angle2;
    f32 magnitude;
    f32 y;
    f32 x;
    f32 r;

    *(u32*)&absValue &= 0x7FFFFFFF;
    if (__fabs(lbl_80348E78 - absValue) < lbl_80348DA0) {
        f64 lockedAngle;
        f32 s = matrix[5];
        angle0 = atan2(matrix[9], s);
        if (matrix[2] > gMlFmathZero) {
            lockedAngle = lbl_80348E88;
        } else {
            lockedAngle = lbl_80348E80;
        }
        angle1 = lockedAngle;
        angle2 = *(volatile f32*)&gMlFmathZero;
    } else {
        angle0 = atan2(-matrix[6], matrix[10]);
        magnitude = cos(angle0);
        if (ML_ZERO_D == magnitude) {
            if (angle0 > ML_ZERO_D) {
                f32 a = -matrix[6];
                angle1 = atan2(-matrix[2], a);
                y = matrix[8];
                x = matrix[9];
                y = -y;
                x = -x;
                angle2 = atan2(y, x);
            } else {
                r = atan2(-matrix[2], matrix[6]);
                x = matrix[9];
                y = matrix[8];
                angle1 = r;
                angle2 = atan2(y, x);
            }
        } else {
            magnitude = matrix[10] / magnitude;
            y = -matrix[2];
            r = atan2(y, magnitude);
            y = matrix[1];
            x = matrix[0];
            y = -y;
            x = x / magnitude;
            y = y / magnitude;
            angle1 = r;
            angle2 = atan2(y, x);
        }
    }
    angles[0] = angle0;
    angles[1] = angle1;
    angles[2] = angle2;
}

/* 0x800BD050 */
void CreateYPRMatrix(f32* matrix, const f32* angles)
{
    f32 c0, s0, c1, s1, c2, s2;
    f32 a, b;

    c0 = ffcos(angles[0]);
    s0 = -ffsin(angles[0]);
    c1 = ffcos(angles[1]);
    s1 = -ffsin(angles[1]);
    c2 = ffcos(angles[2]);
    s2 = -ffsin(angles[2]);
    a = s1 * s0;
    b = c1 * s0;

    matrix[0] = c1 * c2 - a * s2;
    matrix[4] = -c1 * s2 - a * c2;
    matrix[8] = -s1 * c0;
    matrix[1] = c0 * s2;
    matrix[5] = c0 * c2;
    matrix[9] = -s0;
    matrix[2] = s1 * c2 + b * s2;
    matrix[6] = -s1 * s2 + b * c2;
    matrix[10] = c1 * c0;
    matrix[3] = gMlFmathZero;
    matrix[7] = gMlFmathZero;
    matrix[11] = gMlFmathZero;
    matrix[15] = lbl_80348E90;
}

/* 0x800BD154 */
void CreateRYPMatrix(f32* matrix, const f32* angles)
{
    volatile u8 unused[8];
    f32 c0 = ffcos(angles[0]);
    f32 s0 = -ffsin(angles[0]);
    f32 c1 = ffcos(angles[1]);
    f32 s1 = -ffsin(angles[1]);
    f32 c2 = ffcos(angles[2]);
    f32 b;
    f32 s2 = -ffsin(angles[2]);
    f32 a = -c2 * s1;
    b = -s2 * s1;

    matrix[0] = c2 * c1;
    matrix[4] = -s2 * c0 + a * s0;
    matrix[8] = s2 * s0 + a * c0;
    matrix[1] = s2 * c1;
    matrix[5] = c2 * c0 + b * s0;
    matrix[9] = -c2 * s0 + b * c0;
    matrix[2] = s1;
    matrix[6] = c1 * s0;
    matrix[10] = c1 * c0;
    matrix[3] = gMlFmathZero;
    matrix[7] = gMlFmathZero;
    matrix[11] = gMlFmathZero;
    matrix[15] = lbl_80348E90;
}

/* 0x800BD254 */
void CreatePYRMatrix(f32* matrix, const f32* angles)
{
    f32 b, c0, s0, c1, s1, c2, s2;

    c0 = ffcos(angles[0]);
    s0 = -ffsin(angles[0]);
    c1 = ffcos(angles[1]);
    s1 = -ffsin(angles[1]);
    c2 = ffcos(angles[2]);
    s2 = -ffsin(angles[2]);
    matrix[0] = c1 * c2;
    matrix[4] = -c1 * s2;
    matrix[8] = -s1;
    {
        f32 a = s0 * s1;

        matrix[1] = -a * c2 + c0 * s2;
        matrix[5] = a * s2 + c0 * c2;
    }
    matrix[9] = -s0 * c1;
    b = c0 * s1;
    matrix[2] = b * c2 + s0 * s2;
    matrix[6] = b * -s2 + s0 * c2;
    matrix[10] = c0 * c1;
    matrix[3] = gMlFmathZero;
    matrix[7] = gMlFmathZero;
    matrix[11] = gMlFmathZero;
    matrix[15] = lbl_80348E90;
}

/* 0x800BD360 */
f32 AddAngle(f32 angle, f32 amount)
{
    f64 twoPi;

    angle += amount;
    twoPi = lbl_80348E98;
    while (angle > lbl_80348EA0) {
        angle -= twoPi;
    }
    twoPi = lbl_80348E98;
    while (angle <= lbl_80348EA8) {
        angle += twoPi;
    }
    return angle;
}

/* 0x800BD3A4 */
f32 SubAngle(f32 angle, f32 amount)
{
    f64 twoPi;

    angle -= amount;
    twoPi = lbl_80348E98;
    while (angle > lbl_80348EA0) {
        angle -= twoPi;
    }
    twoPi = lbl_80348E98;
    while (angle <= lbl_80348EA8) {
        angle += twoPi;
    }
    return angle;
}

/* 0x800BD3E8 */
f32 FixAngle(f32 angle)
{
    f64 twoPi;

    twoPi = lbl_80348E98;
    while (angle > lbl_80348EA0) {
        angle -= twoPi;
    }
    twoPi = lbl_80348E98;
    while (angle <= lbl_80348EA8) {
        angle += twoPi;
    }
    return angle;
}

/* 0x800BD428 */
void GetYawPitch(const f32* vector, f32* yaw, f32* pitch)
{
    u8 unused[8];
    f32 z = vector[2];
    f32 x = vector[0];
    f32 distance;

    *yaw = atan2(x, z);
    distance = fqdist(vector[0], vector[2]);
    *pitch = atan2(vector[1], distance);
}

static inline void createDirNormalize(f32* vector, volatile f32* root)
{
    f32 length = vector[0] * vector[0] + vector[1] * vector[1] +
                 vector[2] * vector[2];
    f32 scale;

    if (length > gMlFmathZero) {
        f64 half = lbl_80348DB0;
        f64 three = lbl_80348EB8;
        f64 guess = __frsqrte(length);
        guess = half * guess * (three - guess * guess * length);
        guess = half * guess * (three - guess * guess * length);
        guess = half * guess * (three - guess * guess * length);
        *root = (f32)(length *
                      (half * guess * (three - guess * guess * length)));
        length = *root;
    }
    if ((f64)length <= ML_ZERO_D) {
        scale = lbl_80348E90;
    } else {
        scale = (f32)(lbl_80348E78 / length);
    }
    vector[0] *= scale;
    vector[1] *= scale;
    vector[2] *= scale;
}

/* 0x800BD488 */
#pragma opt_propagation off
void CreateDirMatrix(f32* matrix, f32* direction, f32* up)
{
    f32 angles[3];
    f32 distance;
    u8 unused[8];
    f32 length2;
    volatile f32 directionRoot;
    volatile f32 upRoot;
    u8 rootPad[8];

    length2 = direction[0] * direction[0] +
              direction[1] * direction[1] +
              direction[2] * direction[2];
    if ((f64)length2 < lbl_80348EB0) {
        sceSamp0CopyMatrix34(matrix, (f32*)gIdentityMatrix);
        matrix[15] = lbl_80348E90;
        return;
    }
    if (up == 0) {
        distance = fqdist(direction[0], direction[2]);
        angles[0] = (f32)(lbl_80348DB0 * atan2(direction[1], distance));
        distance = direction[2];
        angles[1] = atan2(direction[0], distance);
        angles[2] = gMlFmathZero;
        CreateYPRMatrix(matrix, angles);
        return;
    }

    matrix[8] = direction[0];
    matrix[9] = direction[1];
    matrix[10] = direction[2];
    createDirNormalize(&matrix[8], &directionRoot);

    matrix[4] = matrix[9] * up[2] - matrix[10] * up[1];
    matrix[5] = matrix[10] * up[0] - matrix[8] * up[2];
    matrix[6] = matrix[8] * up[1] - matrix[9] * up[0];
    createDirNormalize(&matrix[4], &upRoot);

    matrix[0] = matrix[5] * matrix[10] - matrix[6] * matrix[9];
    matrix[1] = matrix[6] * matrix[8] - matrix[4] * matrix[10];
    matrix[2] = matrix[4] * matrix[9] - matrix[5] * matrix[8];
    matrix[3] = gMlFmathZero;
    matrix[7] = gMlFmathZero;
    matrix[11] = gMlFmathZero;
    matrix[15] = lbl_80348E90;
}
#pragma opt_propagation reset

/* 0x800BD7C4 */
void ReflectVector2D(const f32* vector, const f32* normal, f32* out)
{
    f32 scale = (f32)(lbl_80348EC0 * -(vector[0] * normal[0] +
                              vector[2] * normal[2]));
    out[0] = vector[0] + normal[0] * scale;
    out[2] = vector[2] + normal[2] * scale;
}

/* 0x800BD804 */
void ReflectVector(const f32* vector, const f32* normal, f32* out)
{
    f32 scale =
        (f32)(lbl_80348EC0 * -(vector[0] * normal[0] + vector[1] * normal[1] +
                      vector[2] * normal[2]));
    out[0] = vector[0] + normal[0] * scale;
    out[1] = vector[1] + normal[1] * scale;
    out[2] = vector[2] + normal[2] * scale;
}

/* 0x800BD860 */
f64 SlowNormalVector2D(f32* vector)
{
    f32 magnitude = vector[0] * vector[0] + vector[2] * vector[2];
    f64 original = magnitude;
    f64 length;
    f32 scale;
    u8 unused[8];
    volatile f32 root;

    if (magnitude > gMlFmathZero) {
        f64 guess = __frsqrte(original);
        guess = lbl_80348DB0 * guess * (lbl_80348EB8 - original * (guess * guess));
        guess = lbl_80348DB0 * guess * (lbl_80348EB8 - original * (guess * guess));
        guess = lbl_80348DB0 * guess * (lbl_80348EB8 - original * (guess * guess));
        root = (f32)(original *
                     (lbl_80348DB0 * guess * (lbl_80348EB8 - original * (guess * guess))));
        length = root;
    } else {
        length = original;
    }
    if (length <= lbl_80348E00)
        scale = lbl_80348E90;
    else
        scale = (f32)(lbl_80348E78 / length);
    vector[0] *= scale;
    vector[1] = gMlFmathZero;
    vector[2] *= scale;
    return length;
}

/* 0x800BD938 */
f32 NormalVector2D(f32* vector)
{
    f32 length = fqdist(vector[0], vector[2]);
    f32 scale = lbl_80348E90;

    if ((f64)length <= lbl_80348E00)
        scale = lbl_80348E90;
    else
        scale = (f32)(lbl_80348E78 / length);
    vector[0] *= scale;
    vector[1] = gMlFmathZero;
    vector[2] *= scale;
    return length;
}

/* 0x800BD9B0 */
f64 SlowNormalVector(f32* vector)
{
    f32 magnitude = vector[0] * vector[0] + vector[1] * vector[1] +
                    vector[2] * vector[2];
    f64 original = magnitude;
    f64 length;
    f32 scale;
    u8 unused[8];
    volatile f32 root;

    if (magnitude > gMlFmathZero) {
        f64 guess = __frsqrte(original);
        guess = lbl_80348DB0 * guess * (lbl_80348EB8 - original * (guess * guess));
        guess = lbl_80348DB0 * guess * (lbl_80348EB8 - original * (guess * guess));
        guess = lbl_80348DB0 * guess * (lbl_80348EB8 - original * (guess * guess));
        root = (f32)(original *
                     (lbl_80348DB0 * guess * (lbl_80348EB8 - original * (guess * guess))));
        length = root;
    } else {
        length = original;
    }
    if (length <= lbl_80348E00)
        scale = lbl_80348E90;
    else
        scale = (f32)(lbl_80348E78 / length);
    vector[0] *= scale;
    vector[1] *= scale;
    vector[2] *= scale;
    return length;
}

/* 0x800BDA98 */
f32 NormalVector(f32* vector)
{
    f32 length = fqdist(fqdist(vector[0], vector[2]), vector[1]);
    f32 scale = lbl_80348E90;

    if ((f64)length <= lbl_80348E00)
        scale = lbl_80348E90;
    else
        scale = (f32)(lbl_80348E78 / length);
    vector[0] *= scale;
    vector[1] *= scale;
    vector[2] *= scale;
    return length;
}

/* 0x800BDB1C */
void MulBodyVecMat4(const f32* vector, f32* out, const f32* matrix)
{
    f32 x = vector[0] - matrix[12];
    f32 y = vector[1] - matrix[13];
    f32 z = vector[2] - matrix[14];

    out[0] = x * matrix[0] + y * matrix[1] + z * matrix[2];
    out[1] = x * matrix[4] + y * matrix[5] + z * matrix[6];
    out[2] = x * matrix[8] + y * matrix[9] + z * matrix[10];
}

/* 0x800BDB98 */
void MulVec4Mat3(const f32* vector, f32* out, const f32* matrix)
{
    f32 x = vector[0];
    f32 y = vector[1];
    f32 z = vector[2];

    out[0] = x * matrix[0] + y * matrix[4] + z * matrix[8];
    out[1] = x * matrix[1] + y * matrix[5] + z * matrix[9];
    out[2] = x * matrix[2] + y * matrix[6] + z * matrix[10];
}

/* 0x800BDBFC */
void MulVecMat3(const f32* vector, f32* out, const f32* matrix)
{
    f32 x = vector[0];
    f32 y = vector[1];
    f32 z = vector[2];

    out[0] = x * matrix[0] + y * matrix[4] + z * matrix[8];
    out[1] = x * matrix[1] + y * matrix[5] + z * matrix[9];
    out[2] = x * matrix[2] + y * matrix[6] + z * matrix[10];
}

/* 0x800BDC60 */
void MulVec4Mat4(const f32* vector, f32* out, const f32* matrix)
{
    f32 x = vector[0];
    f32 y = vector[1];
    f32 z = vector[2];

    out[0] = x * matrix[0] + y * matrix[4] + z * matrix[8] + matrix[12];
    out[1] = x * matrix[1] + y * matrix[5] + z * matrix[9] + matrix[13];
    out[2] = x * matrix[2] + y * matrix[6] + z * matrix[10] + matrix[14];
    out[3] = x * matrix[3] + y * matrix[7] + z * matrix[11] + matrix[15];
}

/* 0x800BDD00 */
void MulVecMat4(const f32* vector, f32* out, const f32* matrix)
{
    f32 x = vector[0];
    f32 y = vector[1];
    f32 z = vector[2];

    out[0] = x * matrix[0] + y * matrix[4] + z * matrix[8] + matrix[12];
    out[1] = x * matrix[1] + y * matrix[5] + z * matrix[9] + matrix[13];
    out[2] = x * matrix[2] + y * matrix[6] + z * matrix[10] + matrix[14];
}

/* Pitch a unit direction vector while preserving its yaw. */
void PitchVec3(const f32* vector, f32* out, f32 angle)
{
    u8 unused[8];
    f32 x = vector[0];
    f32 y = vector[1];
    f32 z = vector[2];
    f32 c = (f32)cos(angle);
    f32 s = (f32)sin(angle);

    out[0] = s * (x * y) + x * c;
    out[1] = y * c + s * (-(x * x + z * z));
    out[2] = s * (y * z) + z * c;
}

/* Rotate a vector around the world Y axis. */
void YawVec3(const f32* vector, f32* out, f32 angle)
{
    u8 unused[8];
    f32 x = vector[0];
    f32 z = vector[2];
    f32 c = (f32)cos(angle);
    f32 s = (f32)sin(angle);

    out[0] = x * c - z * s;
    out[1] = vector[1];
    out[2] = z * c + x * s;
}

/* 0x800BDE80 */
void WorldVector(const f32* vector, f32* out, const f32* matrix)
{
    f32 x = vector[0];
    f32 y = vector[1];
    f32 z = vector[2];

    out[0] = x * matrix[0] + y * matrix[4] + z * matrix[8];
    out[1] = x * matrix[1] + y * matrix[5] + z * matrix[9];
    out[2] = x * matrix[2] + y * matrix[6] + z * matrix[10];
}

/* 0x800BDEE4 */
void BodyVector(const f32* vector, f32* out, const f32* matrix)
{
    f32 x = vector[0];
    f32 y = vector[1];
    f32 z = vector[2];

    out[0] = x * matrix[0] + y * matrix[1] + z * matrix[2];
    out[1] = x * matrix[4] + y * matrix[5] + z * matrix[6];
    out[2] = x * matrix[8] + y * matrix[9] + z * matrix[10];
}

/* Multiply the rotational 3x3 portions of two column-major mat44s. */
void MulMat3(f32* lhs, f32* rhs, f32* out)
{
    f32 b0, b1, b2, b4, b5, b6, b8, b9, b10;
    f32 t2, t1, t0, u;
    f32 a0, a2, c0, c2, d0, d2;
    f32 a1;

    lhs[15] = rhs[15] = lbl_80348E90;
    b4 = rhs[4];
    a1 = lhs[1];
    b5 = rhs[5];
    b0 = rhs[0];
    a0 = lhs[0];
    b6 = rhs[6];
    b1 = rhs[1];
    b2 = rhs[2];
    b8 = rhs[8];
    a2 = lhs[2];
    b9 = rhs[9];
    b10 = rhs[10];
    u = a1 * b4;
    t1 = a1 * b5;
    t0 = u + a0 * b0;
    t2 = a1 * b6;
    t1 += a0 * b1;
    t0 += a2 * b8;
    t2 += a0 * b2;
    t1 += a2 * b9;
    out[0] = t0;
    t2 += a2 * b10;
    out[1] = t1;
    out[2] = t2;
    a1 = lhs[5];
    c0 = lhs[4];
    c2 = lhs[6];
    t0 = a1 * b4;
    t1 = a1 * b5;
    t2 = a1 * b6;
    t0 += c0 * b0;
    t1 += c0 * b1;
    t2 += c0 * b2;
    t0 += c2 * b8;
    t1 += c2 * b9;
    t2 += c2 * b10;
    out[4] = t0;
    out[5] = t1;
    out[6] = t2;
    a1 = lhs[9];
    d0 = lhs[8];
    d2 = lhs[10];
    t0 = a1 * b4;
    t1 = a1 * b5;
    t2 = a1 * b6;
    t0 += d0 * b0;
    t1 += d0 * b1;
    t2 += d0 * b2;
    t0 += d2 * b8;
    t1 += d2 * b9;
    t2 += d2 * b10;
    out[8] = t0;
    out[9] = t1;
    out[10] = t2;
}

static inline f32 mlSqrtAccurate(f32 value)
{
    volatile f32 result;

    if (value > gMlFmathZero) {
        f64 half = lbl_80348DB0;
        f64 three = lbl_80348EB8;
        f64 guess = __frsqrte((f64)value);
        guess = half * guess * (three - guess * guess * value);
        guess = half * guess * (three - guess * guess * value);
        guess = half * guess * (three - guess * guess * value);
        guess = half * guess * (three - guess * guess * value);
        result = (f32)(value * guess);
        return result;
    }
    return value;
}

/* Recover the scale carried by each basis vector of a mat44. */
void ExtractScaleMat4(const f32* matrix, f32* out)
{
    out[0] = mlSqrtAccurate(matrix[0] * matrix[0] + matrix[4] * matrix[4] +
                            matrix[8] * matrix[8]);
    out[1] = mlSqrtAccurate(matrix[1] * matrix[1] + matrix[5] * matrix[5] +
                            matrix[9] * matrix[9]);
    out[2] = mlSqrtAccurate(matrix[2] * matrix[2] + matrix[6] * matrix[6] +
                            matrix[10] * matrix[10]);
}

/* Compose two transforms, scaling the basis of rhs before multiplication. */
void MulMat4Scale(f32* lhs, f32* rhs, f32* out, f32* scale)
{
    f32 a0;
    f32 a1;
    f32 a2;
    f32 a4;
    f32 a5;
    f32 a6;
    f32 a8;
    f32 a9;
    f32 a10;
    f32 b0;
    f32 b1;
    f32 b2;
    f32 s;

    lhs[15] = lbl_80348E90;
    rhs[15] = lbl_80348E90;
    scale[3] = lbl_80348E90;

    s = scale[0];
    b1 = rhs[0] * s;
    b0 = rhs[1] * s;
    a4 = lhs[4];
    a5 = lhs[5];
    b2 = rhs[2] * s;
    a0 = lhs[0];
    a6 = lhs[6];
    a1 = lhs[1];
    a8 = lhs[8];
    a2 = lhs[2];
    a9 = lhs[9];
    a10 = lhs[10];
    out[0] = b1 * a0 + b0 * a4 + b2 * a8;
    out[1] = b1 * a1 + b0 * a5 + b2 * a9;
    out[2] = b1 * a2 + b0 * a6 + b2 * a10;

    s = scale[1];
    b1 = rhs[4] * s;
    b0 = rhs[5] * s;
    b2 = rhs[6] * s;
    out[4] = b1 * a0 + b0 * a4 + b2 * a8;
    out[5] = b1 * a1 + b0 * a5 + b2 * a9;
    out[6] = b1 * a2 + b0 * a6 + b2 * a10;

    s = scale[2];
    b1 = rhs[8] * s;
    b0 = rhs[9] * s;
    b2 = rhs[10] * s;
    out[8] = b1 * a0 + b0 * a4 + b2 * a8;
    out[9] = b1 * a1 + b0 * a5 + b2 * a9;
    out[10] = b1 * a2 + b0 * a6 + b2 * a10;

    b1 = rhs[12];
    b0 = rhs[13];
    b2 = rhs[14];
    out[12] = b1 * a0 + b0 * a4 + b2 * a8 + lhs[12];
    out[13] = b1 * a1 + b0 * a5 + b2 * a9 + lhs[13];
    out[14] = b1 * a2 + b0 * a6 + b2 * a10 + lhs[14];
}

/* 0x800BE360 */
void MulMat4(f32* lhs, f32* rhs, f32* out)
{
    lhs[15] = lbl_80348E90;
    rhs[15] = lbl_80348E90;
    mat44Mult__FR5mat44R5mat44R5mat44(out, lhs, rhs);
}

/* Post-multiply by a roll rotation. */
#pragma opt_propagation off
void RollMat3(f32* matrix, f32 angle)
{
    u8 unused0[8];
    f32 magnitude = angle;
    u8 unused1[8];
    s32 i;

    *(u32*)&magnitude &= 0x7FFFFFFF;
    if ((f64)magnitude < lbl_80348DA0)
        return;
    {
        f32 s = sin(angle);
        f32 c = cos(angle);
        for (i = 0; i < 3; i++) {
            f32 a = matrix[i];
            f32 b = matrix[4 + i];
            f32 newB;
            f32 newA;
            newA = c * a - b * s;
            newB = b * c + s * a;
            matrix[4 + i] = newB;
            matrix[i] = newA;
        }
    }
}
#pragma opt_propagation reset

/* Post-multiply by a pitch rotation. */
#pragma opt_propagation off
void PitchMat3(f32* matrix, f32 angle)
{
    u8 unused0[8];
    f32 magnitude = angle;
    u8 unused1[8];
    s32 i;

    *(u32*)&magnitude &= 0x7FFFFFFF;
    if ((f64)magnitude < lbl_80348DA0)
        return;
    {
        f32 s = sin(angle);
        f32 c = cos(angle);
        for (i = 0; i < 3; i++) {
            f32 a = matrix[4 + i];
            f32 b = matrix[8 + i];
            f32 newB;
            f32 newA;
            newA = a * c - b * s;
            newB = b * c + a * s;
            matrix[8 + i] = newB;
            matrix[4 + i] = newA;
        }
    }
}
#pragma opt_propagation reset

/* Post-multiply by a yaw rotation. */
#pragma opt_lifetimes off
#pragma opt_propagation off
void YawMat3(f32* matrix, f32 angle)
{
    f32 magnitude = angle;
    u8 unused1[8];
    s32 i;

    *(u32*)&magnitude &= 0x7FFFFFFF;
    if ((f64)magnitude < lbl_80348DA0)
        return;
    sYawSin = sin(angle);
    sYawCos = cos(angle);
    for (i = 0; i < 3; i++) {
        f32 s;
        f32 a;
        f32 c;
        f32 b;
        f32 newB;
        f32 newA;

        s = sYawSin;
        c = sYawCos;
        a = matrix[i];
        b = matrix[8 + i];
        newA = c * a - b * s;
        newB = b * c + s * a;

        matrix[8 + i] = newB;
        matrix[i] = newA;
    }
}
#pragma opt_propagation reset
#pragma opt_lifetimes reset

/* Pre-multiply by a roll rotation. */
#pragma opt_propagation off
void WRollMat3(f32* matrix, f32 angle)
{
    u8 unused0[8];
    f32 magnitude;
    u8 unused1[8];
    s32 row;

    magnitude = angle;
    *(u32*)&magnitude &= 0x7FFFFFFF;
    if ((f64)magnitude < lbl_80348DA0)
        return;
    {
        f32 s = sin(angle);
        f32 c = cos(angle);
        for (row = 0; row < 3; row++) {
            f32* v = matrix + row * 4;
            f32 a = v[0];
            f32 b = v[1];
            f32 newB;
            f32 newA;
            newA = c * a - s * b;
            newB = c * b + s * a;
            v[1] = newB;
            v[0] = newA;
        }
    }
}
#pragma opt_propagation reset

/* Pre-multiply by a pitch rotation. */
#pragma opt_propagation off
void WPitchMat3(register f32* matrix, f32 angle)
{
    u8 unused0[8];
    f32 magnitude = angle;
    u8 unused1[8];
    s32 row;

    *(u32*)&magnitude &= 0x7FFFFFFF;
    if ((f64)magnitude < lbl_80348DA0)
        return;
    {
        f32 s = sin(angle);
        f32 c = cos(angle);
        for (row = 0; row < 3; row++) {
            f32* v = matrix + row * 4;
            f32* aPtr;
            f32 a;
            f32 b;
            f32 newB;
            f32 newA;
            a = v[1];
            aPtr = v + 1;
            b = *(v += 2);
            newA = c * a - s * b;
            newB = c * b + s * a;
            *v = newB;
            *aPtr = newA;
        }
    }
}
#pragma opt_propagation reset

/* Pre-multiply by a yaw rotation. */
#pragma opt_propagation off
void WYawMat3(f32* matrix, f32 angle)
{
    u8 unused0[8];
    f32 magnitude = angle;
    u8 unused1[8];
    s32 row;

    *(u32*)&magnitude &= 0x7FFFFFFF;
    if ((f64)magnitude < lbl_80348DA0)
        return;
    {
        f32 s = sin(angle);
        f32 c = cos(angle);
        for (row = 0; row < 3; row++) {
            f32* v = matrix + row * 4;
            f32 a = v[0];
            f32 b = v[2];
            f32 newB;
            f32 newA;
            newA = c * a - s * b;
            newB = c * b + s * a;
            v[2] = newB;
            v[0] = newA;
        }
    }
}
#pragma opt_propagation reset

/* 0x800BE79C */
void ScaleMat3Vec3(const f32* matrix, f32* out, const f32* scale)
{
    const f32 (*src)[4] = (const f32 (*)[4])matrix;
    f32 (*dst)[4] = (f32 (*)[4])out;
    s32 row;
    s32 column;

    for (row = 0; row < 3; row++) {
        for (column = 0; column < 3; column++)
            dst[row][column] = src[row][column] * scale[column];
    }
}

/* Invert an orthonormal affine transform. */
void InvertMat4(const f32* matrix, f32* out)
{
    f32 dot;

    sceSamp0TransposeMatrix(out, matrix);
    out[3] = gMlFmathZero;
    out[7] = gMlFmathZero;
    out[11] = gMlFmathZero;
    dot = matrix[12] * matrix[0] + matrix[13] * matrix[1] +
          matrix[14] * matrix[2];
    out[12] = (f32)(lbl_80348EC8 * dot);
    dot = matrix[12] * matrix[4] + matrix[13] * matrix[5] +
          matrix[14] * matrix[6];
    out[13] = (f32)(lbl_80348EC8 * dot);
    dot = matrix[12] * matrix[8] + matrix[13] * matrix[9] +
          matrix[14] * matrix[10];
    out[14] = (f32)(lbl_80348EC8 * dot);
    out[15] = lbl_80348E90;
}

/* 0x800BE8C8 */
void CopyMat3(const f32* src, f32* dst)
{
    sceSamp0CopyMatrix34(dst, src);
}

/* 0x800BE8F4 */
void CopyMat4(const f32* src, f32* dst)
{
    __as__5mat44FRC5mat44(dst, src);
}

/* 0x800BE920 */
s32 Round(f32 value)
{
    if (value >= lbl_80348E00)
        value += lbl_80348DB0;
    else
        value += lbl_80348ED0;
    return (s32)value;
}
