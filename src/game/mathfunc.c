/* mathfunc.c -- Midway's GCN port of the PS2 SDK sample math library
 * (sceSamp0*, the C reference of libvu0). On GCN most functions are thin
 * adapters over the dolphin paired-single MTX/VEC library, keeping the PS2
 * dst-first argument order and 4x4 column-vector matrix convention.
 * Names from the Xbox build's MATHFUNC.OBJ (shell3D.pdb).
 */

#include "dolphin/mtx.h"
#include "types.h"

typedef float SAMPVECTOR[4];
typedef float SAMPMATRIX[4][4];

/* the 3x4 part of a SAMPMATRIX aliases a dolphin Mtx */
#define MTX(m) ((MtxPtr) (m))
#define VEC(v) ((Vec*) (v))

/* g3dMath3D's vec3 copy (12 bytes); real C++ name pending that TU */
/* vec3::operator= from g3dMath3D.cpp (mangled name callable from C) */
void __as__4vec3FRC4vec3(void* dst, const void* src);

/* static helper, always inlined then stripped -- pools 0.0f first */
static void sceSamp0ZeroVector(SAMPVECTOR v)
{
    v[0] = v[1] = v[2] = 0.0f;
}

void sceSamp0RotCameraMatrix(SAMPMATRIX m, SAMPVECTOR p, SAMPVECTOR zd, SAMPVECTOR yd)
{
    SAMPVECTOR pad0; /* unused, matches original frame */
    Mtx w;
    SAMPVECTOR vc;
    SAMPVECTOR vx;
    SAMPVECTOR vy;
    SAMPVECTOR vz;
    u8 pad1[0x18]; /* unused, matches original frame */
    Mtx t;

    PSMTXIdentity(MTX(w));

    /* x = normalize(yd x zd) (sceSamp0Normalize written out) */
    PSVECCrossProduct(VEC(yd), VEC(zd), VEC(vc));
    if (vc[0] * vc[0] + vc[1] * vc[1] + vc[2] * vc[2] >= 1e-14f) {
        PSVECNormalize(VEC(vc), VEC(vx));
    } else {
        sceSamp0ZeroVector(vx);
    }
    w[0][0] = vx[0];
    w[1][0] = vx[1];
    w[2][0] = vx[2];

    /* z = normalize(zd) */
    if (zd[0] * zd[0] + zd[1] * zd[1] + zd[2] * zd[2] >= 1e-14f) {
        PSVECNormalize(VEC(zd), VEC(vz));
    } else {
        sceSamp0ZeroVector(vz);
    }
    w[0][2] = vz[0];
    w[1][2] = vz[1];
    w[2][2] = vz[2];

    /* y = z x x */
    PSVECCrossProduct(VEC(vz), VEC(vx), VEC(vy));
    w[0][1] = vy[0];
    w[1][1] = vy[1];
    w[2][1] = vy[2];

    PSMTXCopy(MTX(w), MTX(w));
    w[0][3] += p[0];
    w[1][3] += p[1];
    w[2][3] += p[2];
    PSMTXCopy(MTX(w), MTX(t));
    PSMTXTranspose(MTX(t), MTX(m));
    m[0][3] = -(t[0][3] * t[0][0] + t[1][3] * t[1][0] + t[2][3] * t[2][0]);
    m[1][3] = -(t[0][3] * t[0][1] + t[1][3] * t[1][1] + t[2][3] * t[2][1]);
    m[2][3] = -(t[0][3] * t[0][2] + t[1][3] * t[1][2] + t[2][3] * t[2][2]);
}

void sceSamp0MulMatrix(SAMPMATRIX m, SAMPMATRIX m1, SAMPMATRIX m2)
{
    PSMTXConcat(MTX(m1), MTX(m2), MTX(m));
}

void sceSamp0ApplyMatrix(SAMPVECTOR v0, SAMPMATRIX m, SAMPVECTOR v1)
{
    SAMPVECTOR t;

    __as__4vec3FRC4vec3(t, v1);
    v0[0] = m[0][0] * t[0] + m[1][0] * t[1] + m[2][0] * t[2];
    v0[1] = m[0][1] * t[0] + m[1][1] * t[1] + m[2][1] * t[2];
    v0[2] = m[0][2] * t[0] + m[1][2] * t[1] + m[2][2] * t[2];
    v0[3] = m[0][3] * t[0] + m[1][3] * t[1] + m[2][3] * t[2];
}

void sceSamp0MultVec(SAMPVECTOR v0, SAMPMATRIX m, SAMPVECTOR v1)
{
    PSMTXMultVec(MTX(m), VEC(v1), VEC(v0));
}

void sceSamp0TransposeMatrix(SAMPMATRIX m0, SAMPMATRIX m1)
{
    PSMTXTranspose(MTX(m1), MTX(m0));
}

void sceSamp0InversMatrix(SAMPMATRIX m0, SAMPMATRIX m1)
{
    SAMPVECTOR tv; /* unused, matches original frame */
    u8 pad[8];
    Mtx t;
    int i;

    PSMTXTranspose(MTX(m1), MTX(t));
    for (i = 0; i < 3; i++) {
        t[i][3] = m1[3][i];
    }
    PSMTXCopy(MTX(t), MTX(m0));
}

void sceSamp0CopyMatrix(SAMPMATRIX m0, SAMPMATRIX m1)
{
    SAMPMATRIX t0; /* unused, matches original frame */
    SAMPMATRIX t1;

    PSMTXCopy(MTX(m1), MTX(m0));
    __as__4vec3FRC4vec3(m0[3], m1[3]);
}

void sceSamp0CopyMatrix34(SAMPMATRIX m0, SAMPMATRIX m1)
{
    PSMTXCopy(MTX(m1), MTX(m0));
}

void sceSamp0Normalize(SAMPVECTOR v0, SAMPVECTOR v1)
{
    if (v1[0] * v1[0] + v1[1] * v1[1] + v1[2] * v1[2] >= 1e-14f) {
        PSVECNormalize(VEC(v1), VEC(v0));
    } else {
        sceSamp0ZeroVector(v0);
    }
}

void sceSamp0OuterProduct(SAMPVECTOR v0, SAMPVECTOR v1, SAMPVECTOR v2)
{
    PSVECCrossProduct(VEC(v1), VEC(v2), VEC(v0));
}
