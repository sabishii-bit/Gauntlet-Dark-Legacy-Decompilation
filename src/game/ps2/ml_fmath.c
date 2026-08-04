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

/* 0x800BCAAC - piecewise square-root approximation */
f32 smallsqrt(f32 value)
{
    if (value <= 0.0001)
        return 0.0f;
    if (value > 0.5)
        return (f32)(0.5858 * (value - 0.5) + 0.7071);
    if (value > 0.25)
        return (f32)(0.8284 * value + 0.2929);
    if (value <= 0.125)
        return (f32)(2.8284 * value);
    return (f32)(1.1716 * value + 0.2071);
}

/* 0x800BCB44 - fast two-dimensional distance approximation */
f32 fqdist(f32 x, f32 y)
{
    f32 lo;

    if (x < 0.0)
        x = -x;
    if (y < 0.0)
        y = -y;
    if (x < 0.0001)
        return y;
    if (y < 0.0001)
        return x;

    lo = x;
    if (y <= x) {
        lo = y;
        y = x;
    }

    if (0.5 * y < lo) {
        if (0.75 * y < lo) {
            if (0.875 * y < lo)
                return (f32)(0.414 * lo + y);
            return (f32)(0.376 * lo + y);
        }
        if (0.625 * y < lo)
            return (f32)(0.333 * lo + y);
        return (f32)(0.287 * lo + y);
    }
    if (0.25 * y < lo) {
        if (0.375 * y < lo)
            return (f32)(0.236 * lo + y);
        return (f32)(0.181 * lo + y);
    }
    if (0.125 * y < lo)
        return (f32)(0.124 * lo + y);
    return (f32)(0.064 * lo + y);
}

/* 0x800BCCA8 */
u32 RandInt(u32 n) {
    lbl_80344F08 = pbRand();
    return lbl_80344F08 % n;
}

/* 0x800BCCE8 */
f32 Random(f32 scale) {
    lbl_80344F08 = pbRand();
    return (lbl_80344F08 & 0x7FFF) * scale / 32767.0;
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
    f32 angle1;
    f32 angle0;
    f32 magnitude;

    *(u32*)&absValue &= 0x7FFFFFFF;
    if (__fabs(1.0 - absValue) < 0.0001) {
        f64 lockedAngle;
        angle1 = atan2(-matrix[2], matrix[0]);
        if (matrix[9] > 0.0f) {
            lockedAngle = 1.570796327;
        } else {
            lockedAngle = -1.570796327;
        }
        angle0 = lockedAngle;
        angle2 = 0.0f;
    } else {
        angle2 = atan2(-matrix[1], matrix[5]);
        magnitude = cos(angle2);
        if (magnitude == 0.0) {
            if (angle2 > 0.0) {
                angle0 = atan2(-matrix[1], matrix[9]);
                angle1 = atan2(-matrix[4], matrix[6]);
            } else {
                angle0 = atan2(matrix[1], matrix[9]);
                angle1 = atan2(-matrix[6], matrix[4]);
            }
        } else {
            magnitude = matrix[5] / magnitude;
            angle0 = atan2(matrix[9], magnitude);
            angle1 = atan2(matrix[8] / magnitude, matrix[10] / magnitude);
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

    *(u32*)&absValue &= 0x7FFFFFFF;
    if (__fabs(1.0 - absValue) < 0.0001) {
        f64 lockedAngle;
        angle0 = atan2(matrix[9], matrix[5]);
        if (matrix[2] > 0.0f) {
            lockedAngle = -1.570796327;
        } else {
            lockedAngle = 1.570796327;
        }
        angle1 = lockedAngle;
        angle2 = 0.0f;
    } else {
        angle0 = atan2(-matrix[6], matrix[10]);
        magnitude = cos(angle0);
        if (magnitude == 0.0) {
            if (angle0 > 0.0) {
                angle1 = atan2(-matrix[2], -matrix[6]);
                angle2 = atan2(-matrix[8], -matrix[9]);
            } else {
                angle1 = atan2(-matrix[2], matrix[6]);
                angle2 = atan2(matrix[8], matrix[9]);
            }
        } else {
            magnitude = matrix[10] / magnitude;
            angle1 = atan2(-matrix[2], magnitude);
            angle2 = atan2(-matrix[1] / magnitude, matrix[0] / magnitude);
        }
    }
    angles[0] = angle0;
    angles[1] = angle1;
    angles[2] = angle2;
}

/* 0x800BD050 */
void CreateYPRMatrix(f32* matrix, const f32* angles)
{
    f32 c0 = ffcos(angles[0]);
    f32 s0 = -ffsin(angles[0]);
    f32 c1 = ffcos(angles[1]);
    f32 s1 = -ffsin(angles[1]);
    f32 c2 = ffcos(angles[2]);
    f32 s2 = -ffsin(angles[2]);

    matrix[0] = c1 * c2 - (s1 * s0) * s2;
    matrix[4] = -c1 * s2 - (s1 * s0) * c2;
    matrix[8] = -s1 * c0;
    matrix[1] = c0 * s2;
    matrix[5] = c0 * c2;
    matrix[9] = -s0;
    matrix[2] = s1 * c2 + (c1 * s0) * s2;
    matrix[6] = -s1 * s2 + (c1 * s0) * c2;
    matrix[10] = c1 * c0;
    matrix[3] = 0.0f;
    matrix[7] = 0.0f;
    matrix[11] = 0.0f;
    matrix[15] = 1.0f;
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
    matrix[3] = 0.0f;
    matrix[7] = 0.0f;
    matrix[11] = 0.0f;
    matrix[15] = 1.0f;
}

/* 0x800BD254 */
void CreatePYRMatrix(f32* matrix, const f32* angles)
{
    f32 c0 = ffcos(angles[0]);
    f32 s0 = -ffsin(angles[0]);
    f32 c1 = ffcos(angles[1]);
    f32 s1 = -ffsin(angles[1]);
    f32 c2 = ffcos(angles[2]);
    f32 s2 = -ffsin(angles[2]);

    matrix[0] = c1 * c2;
    matrix[4] = -c1 * s2;
    matrix[8] = -s1;
    matrix[1] = -(s0 * s1) * c2 + c0 * s2;
    matrix[5] = (s0 * s1) * s2 + c0 * c2;
    matrix[9] = -s0 * c1;
    matrix[2] = (c0 * s1) * c2 + s0 * s2;
    matrix[6] = (c0 * s1) * -s2 + s0 * c2;
    matrix[10] = c0 * c1;
    matrix[3] = 0.0f;
    matrix[7] = 0.0f;
    matrix[11] = 0.0f;
    matrix[15] = 1.0f;
}

/* 0x800BD360 */
f32 AddAngle(f32 angle, f32 amount)
{
    angle += amount;
    while (angle > 3.141592654) {
        angle -= 6.283185308;
    }
    while (angle <= -3.141592654) {
        angle += 6.283185308;
    }
    return angle;
}

/* 0x800BD3A4 */
f32 SubAngle(f32 angle, f32 amount)
{
    angle -= amount;
    while (angle > 3.141592654) {
        angle -= 6.283185308;
    }
    while (angle <= -3.141592654) {
        angle += 6.283185308;
    }
    return angle;
}

/* 0x800BD3E8 */
f32 FixAngle(f32 angle)
{
    while (angle > 3.141592654) {
        angle -= 6.283185308;
    }
    while (angle <= -3.141592654) {
        angle += 6.283185308;
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

    if (length > 0.0f) {
        f64 guess = __frsqrte(length);
        guess = 0.5 * guess * (3.0 - guess * guess * length);
        guess = 0.5 * guess * (3.0 - guess * guess * length);
        guess = 0.5 * guess * (3.0 - guess * guess * length);
        *root = (f32)(length *
                      (0.5 * guess * (3.0 - guess * guess * length)));
        length = *root;
    }
    if ((f64)length <= 0.0) {
        scale = 1.0f;
    } else {
        scale = (f32)(1.0 / length);
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
    if ((f64)length2 < 0.001) {
        sceSamp0CopyMatrix34(matrix, (f32*)gIdentityMatrix);
        matrix[15] = 1.0f;
        return;
    }
    if (up == 0) {
        distance = fqdist(direction[0], direction[2]);
        angles[0] = (f32)(0.5 * atan2(direction[1], distance));
        distance = direction[2];
        angles[1] = atan2(direction[0], distance);
        angles[2] = 0.0f;
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
    matrix[3] = 0.0f;
    matrix[7] = 0.0f;
    matrix[11] = 0.0f;
    matrix[15] = 1.0f;
}
#pragma opt_propagation reset

/* 0x800BD7C4 */
void ReflectVector2D(const f32* vector, const f32* normal, f32* out)
{
    f32 scale = (f32)(2.0 * -(vector[0] * normal[0] +
                              vector[2] * normal[2]));
    out[0] = vector[0] + normal[0] * scale;
    out[2] = vector[2] + normal[2] * scale;
}

/* 0x800BD804 */
void ReflectVector(const f32* vector, const f32* normal, f32* out)
{
    f32 scale =
        (f32)(2.0 * -(vector[0] * normal[0] + vector[1] * normal[1] +
                      vector[2] * normal[2]));
    out[0] = vector[0] + normal[0] * scale;
    out[1] = vector[1] + normal[1] * scale;
    out[2] = vector[2] + normal[2] * scale;
}

/* 0x800BD860 */
f32 SlowNormalVector2D(f32* vector)
{
    f64 length = vector[0] * vector[0] + vector[2] * vector[2];
    f32 scale = 1.0f;
    volatile f32 root;

    if (length > 0.0) {
        f64 guess = __frsqrte(length);
        guess = 0.5 * guess * (3.0 - length * guess * guess);
        guess = 0.5 * guess * (3.0 - length * guess * guess);
        guess = 0.5 * guess * (3.0 - length * guess * guess);
        root = (f32)(length *
                     (0.5 * guess * (3.0 - length * guess * guess)));
        length = root;
    }
    if (length > 0.0)
        scale = (f32)(1.0 / length);
    vector[0] *= scale;
    vector[1] = 0.0f;
    vector[2] *= scale;
    return (f32)length;
}

/* 0x800BD938 */
f32 NormalVector2D(f32* vector)
{
    f32 length = fqdist(vector[0], vector[2]);
    f32 scale = 1.0f;

    if ((f64)length <= 0.0)
        scale = 1.0f;
    else
        scale = (f32)(1.0 / length);
    vector[0] *= scale;
    vector[1] = 0.0f;
    vector[2] *= scale;
    return length;
}

/* 0x800BD9B0 */
f32 SlowNormalVector(f32* vector)
{
    f64 length = vector[2] * vector[2] + vector[0] * vector[0] +
                 vector[1] * vector[1];
    f32 scale = 1.0f;
    volatile f32 root;

    if (length > 0.0) {
        f64 guess = __frsqrte(length);
        guess = 0.5 * guess * (3.0 - length * guess * guess);
        guess = 0.5 * guess * (3.0 - length * guess * guess);
        guess = 0.5 * guess * (3.0 - length * guess * guess);
        root = (f32)(length *
                     (0.5 * guess * (3.0 - length * guess * guess)));
        length = root;
    }
    if (length > 0.0)
        scale = (f32)(1.0 / length);
    vector[0] *= scale;
    vector[1] *= scale;
    vector[2] *= scale;
    return (f32)length;
}

/* 0x800BDA98 */
f32 NormalVector(f32* vector)
{
    f32 length = fqdist(fqdist(vector[0], vector[2]), vector[1]);
    f32 scale = 1.0f;

    if ((f64)length <= 0.0)
        scale = 1.0f;
    else
        scale = (f32)(1.0 / length);
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
    f32 b4, b5, b6, b0, b1, b2, b8, b9, b10;
    f32 a0, a1, a2;

    lhs[15] = rhs[15] = 0.0f;
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
    out[0] = a1 * b4 + a0 * b0 + a2 * b8;
    out[1] = a1 * b5 + a0 * b1 + a2 * b9;
    out[2] = a1 * b6 + a0 * b2 + a2 * b10;
    a1 = lhs[5];
    a0 = lhs[4];
    a2 = lhs[6];
    out[4] = a1 * b4 + a0 * b0 + a2 * b8;
    out[5] = a1 * b5 + a0 * b1 + a2 * b9;
    out[6] = a1 * b6 + a0 * b2 + a2 * b10;
    a1 = lhs[9];
    a0 = lhs[8];
    a2 = lhs[10];
    out[8] = a1 * b4 + a0 * b0 + a2 * b8;
    out[9] = a1 * b5 + a0 * b1 + a2 * b9;
    out[10] = a1 * b6 + a0 * b2 + a2 * b10;
}

/* Recover the scale carried by each basis vector of a mat44. */
void ExtractScaleMat4(const f32* matrix, f32* out)
{
    out[0] = (f32)sqrt(matrix[0] * matrix[0] + matrix[4] * matrix[4] +
                       matrix[8] * matrix[8]);
    out[1] = (f32)sqrt(matrix[1] * matrix[1] + matrix[5] * matrix[5] +
                       matrix[9] * matrix[9]);
    out[2] = (f32)sqrt(matrix[2] * matrix[2] + matrix[6] * matrix[6] +
                       matrix[10] * matrix[10]);
}

/* Compose two transforms, scaling the basis of rhs before multiplication. */
void MulMat4Scale(f32* lhs, f32* rhs, f32* out, f32* scale)
{
    s32 row;
    s32 column;

    lhs[15] = rhs[15] = scale[3] = 0.0f;
    for (row = 0; row < 3; row++) {
        for (column = 0; column < 3; column++) {
            out[row * 4 + column] =
                lhs[0 * 4 + column] * rhs[row * 4 + 0] * scale[row] +
                lhs[1 * 4 + column] * rhs[row * 4 + 1] * scale[row] +
                lhs[2 * 4 + column] * rhs[row * 4 + 2] * scale[row];
        }
    }
    for (column = 0; column < 3; column++) {
        out[12 + column] =
            lhs[column] * rhs[12] + lhs[4 + column] * rhs[13] +
            lhs[8 + column] * rhs[14] + lhs[12 + column];
    }
}

/* 0x800BE360 */
void MulMat4(f32* lhs, f32* rhs, f32* out)
{
    lhs[15] = 0.0f;
    rhs[15] = 0.0f;
    mat44Mult__FR5mat44R5mat44R5mat44(out, lhs, rhs);
}

/* Post-multiply by a roll rotation. */
void RollMat3(f32* matrix, f32 angle)
{
    u8 unused0[8];
    f32 magnitude = angle;
    u8 unused1[8];
    s32 i;

    *(u32*)&magnitude &= 0x7FFFFFFF;
    if ((f64)magnitude < 0.0001)
        return;
    {
        f32 s = sin(angle);
        f32 c = cos(angle);
        for (i = 0; i < 3; i++) {
            f32 a = matrix[i];
            f32 b = matrix[4 + i];
            matrix[4 + i] = b * c + s * a;
            matrix[i] = c * a - b * s;
        }
    }
}

/* Post-multiply by a pitch rotation. */
void PitchMat3(f32* matrix, f32 angle)
{
    u8 unused[16];
    f32 magnitude = angle;
    s32 i;

    *(u32*)&magnitude &= 0x7FFFFFFF;
    if ((f64)magnitude < 0.0001)
        return;
    {
        f32 s = sin(angle);
        f32 c = cos(angle);
        for (i = 0; i < 3; i++) {
            f32 a = matrix[4 + i];
            f32 b = matrix[8 + i];
            matrix[8 + i] = c * b + s * a;
            matrix[4 + i] = c * a - s * b;
        }
    }
}

/* Post-multiply by a yaw rotation. */
void YawMat3(f32* matrix, f32 angle)
{
    f32 magnitude = angle;
    s32 i;

    *(u32*)&magnitude &= 0x7FFFFFFF;
    if ((f64)magnitude < 0.0001)
        return;
    {
        f32 s = sin(angle);
        f32 c = cos(angle);
        for (i = 0; i < 3; i++) {
            f32 a = matrix[i];
            f32 b = matrix[8 + i];
            matrix[8 + i] = c * b + s * a;
            matrix[i] = c * a - s * b;
        }
    }
}

/* Pre-multiply by a roll rotation. */
void WRollMat3(f32* matrix, f32 angle)
{
    u8 unused0[8];
    f32 magnitude;
    u8 unused1[8];
    s32 row;

    magnitude = angle;
    *(u32*)&magnitude &= 0x7FFFFFFF;
    if ((f64)magnitude < 0.0001)
        return;
    {
        f32 s = sin(angle);
        f32 c = cos(angle);
        for (row = 0; row < 3; row++) {
            f32* v = matrix + row * 4;
            f32 a = v[0];
            f32 b = v[1];
            v[1] = c * b + s * a;
            v[0] = c * a - s * b;
        }
    }
}

/* Pre-multiply by a pitch rotation. */
void WPitchMat3(f32* matrix, f32 angle)
{
    u8 unused[16];
    f32 magnitude = angle;
    s32 row;

    *(u32*)&magnitude &= 0x7FFFFFFF;
    if ((f64)magnitude < 0.0001)
        return;
    {
        f32 s = sin(angle);
        f32 c = cos(angle);
        for (row = 0; row < 3; row++) {
            f32* v = matrix + row * 4;
            f32 a = v[1];
            f32 b = v[2];
            v[2] = c * b + s * a;
            v[1] = c * a - s * b;
        }
    }
}

/* Pre-multiply by a yaw rotation. */
void WYawMat3(f32* matrix, f32 angle)
{
    u8 unused[16];
    f32 magnitude = angle;
    s32 row;

    *(u32*)&magnitude &= 0x7FFFFFFF;
    if ((f64)magnitude < 0.0001)
        return;
    {
        f32 s = sin(angle);
        f32 c = cos(angle);
        for (row = 0; row < 3; row++) {
            f32* v = matrix + row * 4;
            f32 a = v[0];
            f32 b = v[2];
            v[2] = c * b + s * a;
            v[0] = c * a - s * b;
        }
    }
}

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
    sceSamp0TransposeMatrix(out, matrix);
    out[3] = 0.0f;
    out[7] = 0.0f;
    out[11] = 0.0f;
    out[12] = (f32)(-1.0 * (matrix[12] * matrix[0] +
                            matrix[13] * matrix[1] +
                            matrix[14] * matrix[2]));
    out[13] = (f32)(-1.0 * (matrix[12] * matrix[4] +
                            matrix[13] * matrix[5] +
                            matrix[14] * matrix[6]));
    out[14] = (f32)(-1.0 * (matrix[12] * matrix[8] +
                            matrix[13] * matrix[9] +
                            matrix[14] * matrix[10]));
    out[15] = 0.0f;
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
    if (value >= 0.0)
        value += 0.5;
    else
        value += -0.5;
    return (s32)value;
}
