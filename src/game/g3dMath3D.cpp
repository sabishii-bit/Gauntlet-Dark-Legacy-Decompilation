/* g3dMath3D.cpp -- Midway's 3D math classes (vec2/vec3/vec4, mat22..mat44,
 * quat, Math3D). Names and class layout from the Xbox build's g3dMath3D.obj
 * (183 symbols); the GCN port delegates to dolphin PSMTX/PSVEC where it can.
 * CHUNK 1: the vec/mat44 operator cluster 0x800AE67C-0x800AEA54.
 */

extern "C" {
#include "dolphin/mtx.h"
void* memset(void* dst, int c, unsigned long n);
void* memcpy(void* dst, const void* src, unsigned long n);
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
