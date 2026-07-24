/* g3dMath3D.cpp -- Midway's 3D math classes (vec2/vec3/vec4, mat22..mat44,
 * quat, Math3D). Names and class layout from the Xbox build's g3dMath3D.obj
 * (183 symbols); the GCN port delegates to dolphin PSMTX/PSVEC where it can.
 * CHUNK 1: the vec/mat44 operator cluster 0x800AE67C-0x800AEA54.
 */

extern "C" {
#include "dolphin/mtx.h"
void* memset(void* dst, int c, unsigned long n);
void* memcpy(void* dst, const void* src, unsigned long n);
/* mathfunc.c (PS2 sceSamp0 port) -- the mat44/camera code builds on it */
void sceSamp0MulMatrix(float* m, float* m1, float* m2);
void sceSamp0ApplyMatrix(float* v0, float* m, float* v1);
void sceSamp0TransposeMatrix(float* m0, float* m1);
void sceSamp0CopyMatrix(float* m0, float* m1);
void sceSamp0Normalize(float* v0, float* v1);
void sceSamp0OuterProduct(float* v0, float* v1, float* v2);
}

typedef float f32;
typedef unsigned char u8;

class vec3 {
  public:
    f32 x, y, z;
    vec3() {}
    vec3(f32 ax, f32 ay, f32 az) : x(ax), y(ay), z(az) {}
    void operator=(const vec3& o);
};

class vec4 {
  public:
    f32 x, y, z, w;
    vec4() {}
    void operator=(const vec4& o);
};

class mat44 {
  public:
    f32 m[4][4];
    mat44() {}
    void operator=(const mat44& o);
    void identity();
};

class Math3D_B {
  public:
    static f32 DotProduct(vec3& a, vec3& b);
};

/* The GCN port rewrites the Xbox by-value operators as void dst-first
   helpers; callers routinely pass the same object as dst and src (the
   serialized per-component codegen follows from that aliasing).
   These names are invented -- no 3-arg forms exist in the Xbox PDB. */
void vec4Normalize(vec4& d, vec4& v);
void vec4Cross(vec4& d, vec4& a, vec4& b);
void vec3Div(vec3& d, vec3& v, f32 s);
void vec4Div(vec4& d, vec4& v, f32 s);
void vec3Mul(vec3& d, vec3& a, vec3& b);
void vec4Mul(vec4& d, vec4& a, vec4& b);
void vec4Sub(vec4& d, vec4& a, vec4& b);
void vec4Add(vec4& d, vec4& a, vec4& b);
void vec3Scale(vec3& d, vec3& v, f32 s);
void vec4Scale(vec4& d, vec4& v, f32 s);
void vec4FTOI(long* d, vec4& v);


/* ================= chunk 2: 0x800ADE30-0x800AE67C ================= */

f32 vec3LengthSquared(vec3& v);
void vec4Apply(vec4& d, vec4& v, mat44& m);
void vec4ApplyTrans(vec4& d, vec4& v, mat44& m);
void vec4ApplyTransBody(vec4& d, mat44& m, vec3& v);

/* 0x800ADE30 */
f32 vec3LengthSquared(vec3& v)
{
    return Math3D_B::DotProduct(v, v);
}

/* 0x800ADE54 */
void vec4Apply(vec4& d, vec4& v, mat44& m)
{
    sceSamp0ApplyMatrix((f32*) &d, (f32*) &m, (f32*) &v);
}

/* 0x800ADE80 */
void vec4ApplyTrans(vec4& d, vec4& v, mat44& m)
{
    vec4ApplyTransBody(d, m, *(vec3*) &v);
}

/* 0x800ADEAC */
long mathStub0(void)
{
    return 0;
}

/* 0x800ADEB4 */
void mathStub1(void)
{
}

/* 0x800ADEB8 */
void mathStub1b(void)
{
}

/* 0x800ADEBC */
long mathStub2(void)
{
    return 0;
}

/* 0x800ADEC4 */
long mathStub3(void)
{
    return 0;
}

/* 0x800ADECC */
void vec3Clamp(vec4& d, vec4& v, f32 lo, f32 hi)
{
    memcpy(&d, &v, 0x10);
    if (d.x < lo) {
        d.x = lo;
    }
    if (d.y < lo) {
        d.y = lo;
    }
    if (d.z < lo) {
        d.z = lo;
    }
    if (d.x > hi) {
        d.x = hi;
    }
    if (d.y > hi) {
        d.y = hi;
    }
    if (d.z > hi) {
        d.z = hi;
    }
}

/* 0x800ADF74 */
void mat44SetRows(mat44& m, vec4& r0, vec4& r1, vec4& r2, vec4& r3)
{
    memcpy(m.m[0], &r0, 0x10);
    memcpy(m.m[1], &r1, 0x10);
    memcpy(m.m[2], &r2, 0x10);
    memcpy(m.m[3], &r3, 0x10);
}

/* 0x800ADFE0: negate the three basis rows, normalize each (guarded),
   assemble with 0,0,0,1 bottom row, then transpose in place. */
void mat44InvBasis(mat44& d, vec4& r0, vec4& r1, vec4& r2)
{
    vec4 t;
    mat44 w;
    int i, j;

    t.x = r0.x * -1.0f;
    t.y = r0.y * -1.0f;
    t.z = r0.z * -1.0f;
    t.w = r0.w * -1.0f;
    if (t.z * t.z + t.x * t.x + t.y * t.y < 0.00000000000001f) {
        w.m[0][2] = 0.0f;
        w.m[0][1] = 0.0f;
        w.m[0][0] = 0.0f;
    } else {
        PSVECNormalize((Vec*) &t, (Vec*) w.m[0]);
    }
    w.m[0][3] = t.w;

    t.x = r1.x * -1.0f;
    t.y = r1.y * -1.0f;
    t.z = r1.z * -1.0f;
    t.w = r1.w * -1.0f;
    if (t.z * t.z + t.x * t.x + t.y * t.y < 0.00000000000001f) {
        w.m[1][2] = 0.0f;
        w.m[1][1] = 0.0f;
        w.m[1][0] = 0.0f;
    } else {
        PSVECNormalize((Vec*) &t, (Vec*) w.m[1]);
    }
    w.m[1][3] = t.w;

    t.x = r2.x * -1.0f;
    t.y = r2.y * -1.0f;
    t.z = r2.z * -1.0f;
    t.w = r2.w * -1.0f;
    if (t.z * t.z + t.x * t.x + t.y * t.y < 0.00000000000001f) {
        w.m[2][2] = 0.0f;
        w.m[2][1] = 0.0f;
        w.m[2][0] = 0.0f;
    } else {
        PSVECNormalize((Vec*) &t, (Vec*) w.m[2]);
    }
    w.m[2][3] = t.w;

    w.m[3][2] = 0.0f;
    w.m[3][1] = 0.0f;
    w.m[3][0] = 0.0f;
    w.m[3][3] = 1.0f;

    for (i = 0; i < 4; i++) {
        d.m[i][i] = w.m[i][i];
        for (j = i + 1; j < 4; j++) {
            f32 a = w.m[i][j];
            d.m[i][j] = w.m[j][i];
            d.m[j][i] = a;
        }
    }
}

/* 0x800AE244 */
void mat44LookAt(mat44& m, vec4& p, vec4& zd, vec4& yd)
{
    mat44 t;
    mat44 w;
    vec4 vc;
    vec4 vz;

    memset(&w, 0, 0x40);
    w.m[3][3] = 1.0f;
    w.m[2][2] = 1.0f;
    w.m[1][1] = 1.0f;
    w.m[0][0] = 1.0f;
    sceSamp0OuterProduct((f32*) &vc, (f32*) &yd, (f32*) &zd);
    sceSamp0Normalize(w.m[0], (f32*) &vc);
    sceSamp0Normalize((f32*) &vz, (f32*) &zd);
    sceSamp0OuterProduct(w.m[1], (f32*) &vz, w.m[0]);
    PSMTXCopy((MtxPtr) &w, (MtxPtr) &w);
    memcpy(w.m[3], w.m[3], 0x10);
    w.m[3][0] += p.x;
    w.m[3][1] += p.y;
    w.m[3][2] += p.z;
    w.m[3][3] += p.w;
    sceSamp0CopyMatrix((f32*) &t, (f32*) &w);
    sceSamp0TransposeMatrix((f32*) &m, (f32*) &w);
    m.m[3][0] = -(w.m[3][0] * t.m[0][0] + w.m[3][1] * t.m[0][1] + w.m[3][2] * t.m[0][2]);
    m.m[3][1] = -(w.m[3][0] * t.m[1][0] + w.m[3][1] * t.m[1][1] + w.m[3][2] * t.m[1][2]);
    m.m[3][2] = -(w.m[3][0] * t.m[2][0] + w.m[3][1] * t.m[2][1] + w.m[3][2] * t.m[2][2]);
    m.m[2][3] = 0.0f;
    m.m[1][3] = 0.0f;
    m.m[0][3] = 0.0f;
    m.m[3][3] = 1.0f;
}

/* 0x800AE3E8: rigid-body inverse (transpose rotation, negated translation) */
void mat44InvRigid(mat44& d, mat44& s)
{
    mat44 t;

    sceSamp0CopyMatrix((f32*) &t, (f32*) &s);
    sceSamp0TransposeMatrix((f32*) &d, (f32*) &s);
    d.m[3][0] = -(t.m[3][0] * t.m[0][0] + t.m[3][1] * t.m[0][1] + t.m[3][2] * t.m[0][2]);
    d.m[3][1] = -(t.m[3][0] * t.m[1][0] + t.m[3][1] * t.m[1][1] + t.m[3][2] * t.m[1][2]);
    d.m[3][2] = -(t.m[3][0] * t.m[2][0] + t.m[3][1] * t.m[2][1] + t.m[3][2] * t.m[2][2]);
    d.m[2][3] = 0.0f;
    d.m[1][3] = 0.0f;
    d.m[0][3] = 0.0f;
    d.m[3][3] = 1.0f;
}

/* 0x800AE4C4: d = a x b via a transposed temp of b */
void mat44Mult(mat44& d, mat44& a, mat44& b)
{
    mat44 t;
    mat44 pad0; /* unused, matches original frame */
    mat44 pad1;
    mat44 pad2;
    int i, j;

    PSMTXCopy((MtxPtr) &b, (MtxPtr) &t);
    memcpy(t.m[3], b.m[3], 0x10);
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            d.m[i][j] = a.m[j][0] * t.m[i][0] + a.m[j][1] * t.m[i][1] + a.m[j][2] * t.m[i][2] +
                        a.m[j][3] * t.m[i][3];
        }
    }
}

/* 0x800AE580 */
void vec4ApplyTransBody(vec4& d, mat44& m, vec3& v)
{
    vec3 t;
    u8 pad[0x58]; /* unused, matches original frame */

    memcpy(&t, &v, 0xC);
    d.x = m.m[0][0] * t.x + m.m[1][0] * t.y + m.m[2][0] * t.z + m.m[3][0];
    d.y = m.m[0][1] * t.x + m.m[1][1] * t.y + m.m[2][1] * t.z + m.m[3][1];
    d.z = m.m[0][2] * t.x + m.m[1][2] * t.y + m.m[2][2] * t.z + m.m[3][2];
    d.w = m.m[0][3] * t.x + m.m[1][3] * t.y + m.m[2][3] * t.z + m.m[3][3];
}

/* 0x800AE67C */
void mat44::operator=(const mat44& o)
{
    PSMTXCopy((MtxPtr) &o, (MtxPtr) this);
    memcpy(m[3], o.m[3], 0x10);
}

/* 0x800AE6C4 */
void mat44::identity()
{
    memset(this, 0, 0x40);
    m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
}

/* 0x800AE70C */
void vec4Normalize(vec4& d, vec4& v)
{
    u8 pad[8]; /* unused, matches original frame */

    if (v.x * v.x + v.y * v.y + v.z * v.z >= 0.00000000000001f) {
        PSVECNormalize((Vec*) &v, (Vec*) &d);
    } else {
        d.x = d.y = d.z = 0.0f;
    }
    d.w = v.w;
}

/* 0x800AE790 */
void vec4Cross(vec4& d, vec4& a, vec4& b)
{
    vec4 pad; /* unused, matches original frame */

    PSVECCrossProduct((Vec*) &a, (Vec*) &b, (Vec*) &d);
    d.w = 1.0f;
}

/* 0x800AE7D0 */
f32 Math3D_B::DotProduct(vec3& a, vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

/* 0x800AE7F8 */
void vec3Div(vec3& d, vec3& v, f32 s)
{
    d.x = v.x / s;
    d.y = v.y / s;
    d.z = v.z / s;
}

/* 0x800AE820 */
void vec4Div(vec4& d, vec4& v, f32 s)
{
    d.x = v.x / s;
    d.y = v.y / s;
    d.z = v.z / s;
    d.w = v.w / s;
}

/* 0x800AE854 */
void vec3Mul(vec3& d, vec3& a, vec3& b)
{
    d.x = a.x * b.x;
    d.y = a.y * b.y;
    d.z = a.z * b.z;
}

/* 0x800AE888 */
void vec4Mul(vec4& d, vec4& a, vec4& b)
{
    d.x = a.x * b.x;
    d.y = a.y * b.y;
    d.z = a.z * b.z;
    d.w = a.w * b.w;
}

/* 0x800AE8CC */
void vec4Sub(vec4& d, vec4& a, vec4& b)
{
    d.x = a.x - b.x;
    d.y = a.y - b.y;
    d.z = a.z - b.z;
    d.w = a.w - b.w;
}

/* 0x800AE910 */
void vec4Add(vec4& d, vec4& a, vec4& b)
{
    d.x = a.x + b.x;
    d.y = a.y + b.y;
    d.z = a.z + b.z;
    d.w = a.w + b.w;
}

/* 0x800AE954 */
void vec3Scale(vec3& d, vec3& v, f32 s)
{
    d.x = v.x * s;
    d.y = v.y * s;
    d.z = v.z * s;
}

/* 0x800AE97C */
void vec4Scale(vec4& d, vec4& v, f32 s)
{
    d.x = v.x * s;
    d.y = v.y * s;
    d.z = v.z * s;
    d.w = v.w * s;
}

/* 0x800AE9B0 (invented name; float->int vec4 convert, dst-first) */
void vec4FTOI(long* d, vec4& v)
{
    d[0] = (long) v.x;
    d[1] = (long) v.y;
    d[2] = (long) v.z;
    d[3] = (long) v.w;
}

/* 0x800AEA0C */
void vec3::operator=(const vec3& o)
{
    memcpy(this, &o, 0xC);
}

/* 0x800AEA30 */
void vec4::operator=(const vec4& o)
{
    memcpy(this, &o, 0x10);
}
