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
    vec4 normalize();
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
    static vec4 CrossProduct(vec4& a, vec4& b);
};

vec3 operator/(vec3& v, f32 s);
vec4 operator/(vec4& v, f32 s);
vec3 operator*(vec3& a, vec3& b);
vec4 operator*(vec4& a, vec4& b);
vec4 operator-(vec4& a, vec4& b);
vec4 operator+(vec4& a, vec4& b);
vec3 operator*(vec3& v, f32 s);
vec4 operator*(vec4& v, f32 s);

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
vec4 vec4::normalize()
{
    vec4 r;

    if (x * x + y * y + z * z < 0.00001f) {
        r.x = r.y = r.z = 0.0f;
    } else {
        PSVECNormalize((Vec*) this, (Vec*) &r);
    }
    r.w = w;
    return r;
}

/* 0x800AE790 */
vec4 Math3D_B::CrossProduct(vec4& a, vec4& b)
{
    vec4 r;

    PSVECCrossProduct((Vec*) &a, (Vec*) &b, (Vec*) &r);
    r.w = 1.0f;
    return r;
}

/* 0x800AE7D0 */
f32 Math3D_B::DotProduct(vec3& a, vec3& b)
{
    return a.y * b.y + a.x * b.x + a.z * b.z;
}

/* 0x800AE7F8 */
vec3 operator/(vec3& v, f32 s)
{
    vec3 r;

    r.x = v.x / s;
    r.y = v.y / s;
    r.z = v.z / s;
    return r;
}

/* 0x800AE820 */
vec4 operator/(vec4& v, f32 s)
{
    vec4 r;

    r.x = v.x / s;
    r.y = v.y / s;
    r.z = v.z / s;
    r.w = v.w / s;
    return r;
}

/* 0x800AE854 */
vec3 operator*(vec3& a, vec3& b)
{
    vec3 r;

    r.x = a.x * b.x;
    r.y = a.y * b.y;
    r.z = a.z * b.z;
    return r;
}

/* 0x800AE888 */
vec4 operator*(vec4& a, vec4& b)
{
    vec4 r;

    r.x = a.x * b.x;
    r.y = a.y * b.y;
    r.z = a.z * b.z;
    r.w = a.w * b.w;
    return r;
}

/* 0x800AE8CC */
vec4 operator-(vec4& a, vec4& b)
{
    vec4 r;

    r.x = a.x - b.x;
    r.y = a.y - b.y;
    r.z = a.z - b.z;
    r.w = a.w - b.w;
    return r;
}

/* 0x800AE910 */
vec4 operator+(vec4& a, vec4& b)
{
    vec4 r;

    r.x = a.x + b.x;
    r.y = a.y + b.y;
    r.z = a.z + b.z;
    r.w = a.w + b.w;
    return r;
}

/* 0x800AE954 */
vec3 operator*(vec3& v, f32 s)
{
    vec3 r;

    r.x = v.x * s;
    r.y = v.y * s;
    r.z = v.z * s;
    return r;
}

/* 0x800AE97C */
vec4 operator*(vec4& v, f32 s)
{
    vec4 r;

    r.x = v.x * s;
    r.y = v.y * s;
    r.z = v.z * s;
    r.w = v.w * s;
    return r;
}

/* 0x800AE9B0: FTOI vec4 -- name unknown yet, not in chunk 1 */

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
