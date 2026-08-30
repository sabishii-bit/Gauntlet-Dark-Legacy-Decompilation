#include "types.h"

/* BTRICOL -- bounding-triangle / line collision geometry (GCN BTRICOL.OBJ,
 * 0x8002109C-0x80022734).  Names are the real ones from the Xbox build's
 * BTRICOL.OBJ (shell3D.pdb).  The GameCube MWCC build emits this TU's
 * functions in the reverse of the Xbox link order, so TriLineCol (last in the
 * PDB) is first here and PointLineDist2D (near the front of the PDB) is last.
 *
 * The two public entry points -- TriLineCol and BTriLineCol -- are exported
 * (called from the world/collision code at CTriListCollide); everything else
 * (the vector-frame transforms and the point/line distance helpers) is static.
 *
 * NOTE: this TU is wired NonMatching.  The bodies below are faithful, compiling
 * reconstructions of the disassembly's data-flow, not yet byte-matched; the
 * heavy collision routines carry best-effort bodies.  The .sdata2 FP-constant
 * pool (0x80345D40-0x80345DB8) is therefore still owned by the auto splits. */

typedef struct Vec {
    f32 x, y, z;
} Vec;

/* Trig frame passed to the vector-frame transforms: cos/sin/tan of the
 * surface orientation packed at offsets 0/4/8. */
typedef struct ColFrame {
    f32 c; /* 0x0 cos */
    f32 s; /* 0x4 sin */
    f32 t; /* 0x8 */
} ColFrame;

typedef struct WorldTri {
    s16 layerLo;
    s16 layerHi;
    f32 scale;
    Vec norm;
    Vec center;
    s16 x0;
    s16 z0;
    s16 x1;
    s16 z1;
} WorldTri;

/* Read-only collision-query line, owned elsewhere (.bss 0x8023F7E8). */
extern f32 gColQueryLine[8]; /* [0..2] = p0, [4..6] = p1 */
extern const f64 lbl_80345D40;
extern const f32 lbl_80345D70;
extern const f32 lbl_80345D50;
extern const f64 lbl_80345D78;
extern const f64 lbl_80345D80;
extern const f64 lbl_80345D88;
extern const f32 lbl_80345D90;
extern const f64 lbl_80345D98;
extern const f64 lbl_80345DA0;
extern const f64 lbl_80345DA8;
extern const f32 lbl_80345DB0;
extern f64 __frsqrte(f64 value);

/* PSVEC-style helpers in the g3d math library. */
extern f32  fqdist(f32 x, f32 z);
extern f32 SlowNormalVector(Vec* vector);

/* forward decls (address order) */
s32         TriLineCol(WorldTri* tri, Vec* out);
f32         BTriLineCol(WorldTri* tri, Vec* out, f32 radius);
static void BodyVectorNorm(Vec* in, Vec* out, ColFrame* f, f32 c);
static void WorldVectorNorm(Vec* out, f32 x, f32 y, f32 z, f32 c,
                            ColFrame* f);
static f32  LineLineDist3D2D(Vec* a0, Vec* a1, Vec* out,
                             Vec* b0, Vec* b1, s32 flattenY);
static f32  LineLineDist(Vec* pointB, Vec* dirB, Vec* out,
                         Vec* pointA, Vec* dirA, f32 lenB, f32 lenA);
static f32  PointLineDist2D(Vec* p0, Vec* p1, Vec* dir, Vec* out);
static inline f32 btri_fabsf(f32 x) {
    *(u32*)&x &= 0x7fffffff;
    return x;
}

/* ------------------------------------------------------------------ */
/* TriLineCol -- collide the query line (gColQueryLine) against a       */
/* single triangle, returning the hit fraction (-1 when no hit).       */
/* ------------------------------------------------------------------ */
s32 TriLineCol(WorldTri* tri, Vec* out) {
    u8 padHigh[20];
    Vec norm;
    u8 pad3[4];
    Vec tpB;
    u8 pad2[4];
    Vec tpA;
    u8 pad1[4];
    Vec v1;
    u8 pad0[4];
    f32 cx;
    f32 cy;
    f32 cz;
    f32 v3x;
    f32 v3z;
    f32 dx1;
    f32 dz1;
    f32 tpx;
    f32 tpz;
    f32 t;
    f32 x0;
    f32 z0;
    f32 x1;
    f32 z1;
    u8 unused[8];

    cx = tri->center.x;
    cy = tri->center.y;
    cz = tri->center.z;
    norm.x = tri->norm.x;
    norm.y = tri->norm.y;
    norm.z = tri->norm.z;
    v1.x = gColQueryLine[4] - cx;
    v1.y = gColQueryLine[5] - cy;
    v1.z = gColQueryLine[6] - cz;
    BodyVectorNorm(&v1, &tpB, (ColFrame*)&norm, tri->scale);
    if ((f64)tpB.y < 0.0) {
        return 0;
    }

    v1.x = gColQueryLine[0] - cx;
    v1.y = gColQueryLine[1] - cy;
    v1.z = gColQueryLine[2] - cz;
    BodyVectorNorm(&v1, &tpA, (ColFrame*)&norm, tri->scale);
    if (tpB.y < tpA.y) {
        return 0;
    }
    if ((tpB.y > 0.0 && tpA.y > 0.0) ||
        (tpB.y < 0.0 && tpA.y < 0.0)) {
        return 0;
    }

    dx1 = tpA.x - tpB.x;
    dz1 = tpA.z - tpB.z;
    if ((f64)fqdist(dx1, dz1) > 0.01) {
        f32 ayB;
        f32 ayA;
        f32 sum;
        sum = (ayB = btri_fabsf(tpB.y)) +
              (ayA = btri_fabsf(tpA.y));
        if (sum == 0.0f) {
            t = (f32)(1000.0 * (f64)btri_fabsf(tpB.y));
        } else {
            t = btri_fabsf(tpB.y) / sum;
        }
        tpx = dx1 * t + tpB.x;
        tpz = dz1 * t + tpB.z;
    } else {
        tpx = tpB.x;
        tpz = tpB.z;
    }

    x0 = (f32)((f64)tri->x0 * 0.015625);
    z0 = (f32)((f64)tri->z0 * 0.015625);
    x1 = (f32)((f64)tri->x1 * 0.015625);
    z1 = (f32)((f64)tri->z1 * 0.015625);

    v3x = x0 * tpz - z0 * tpx;
    if ((f64)v3x > 0.0) {
        return 0;
    }
    v3x = (x1 - x0) * (tpz - z0) - (z1 - z0) * (tpx - x0);
    if ((f64)v3x > 0.0) {
        return 0;
    }
    v3z = -x1 * (tpz - z1) - (-z1 * (tpx - x1));
    if ((f64)v3z > 0.0) {
        return 0;
    }

    if (out != 0) {
        WorldVectorNorm(out, tpx, 0.0f, tpz, tri->scale, (ColFrame*)&norm);
        out->x += cx;
        out->y += cy;
        out->z += cz;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* BTriLineCol -- swept (radius > 0) triangle / line collision built    */
/* on the LineLineDist3D2D edge tests and the frame transforms.        */
/* ------------------------------------------------------------------ */
f32 BTriLineCol(WorldTri* tri, Vec* out, f32 radius) {
    u8 highFrame[24];
    Vec norm;
    u8 padI[4];
    Vec tpB;
    u8 padH[4];
    Vec tpA;
    u8 padG[4];
    Vec v1;
    u8 padF[4];
    Vec o2;
    u8 padE[8];
    Vec zero2;
    u8 padD[4];
    Vec ex0;
    u8 padC[4];
    Vec ex1;
    u8 padB[4];
    struct {
        Vec value;
        u8 pad[8];
    } tmp1;
    Vec tmp2;
    f32 r2;
    f32 dist;
    f32 cx;
    f32 cy;
    f32 cz;
    f32 px;
    f32 pz;
    f32 zero;
    f32 dx;
    f32 dz;
    f32 num;
    f32 d;
    s32 cross;

    zero = lbl_80345D50;
    cx = tri->center.x;
    cy = tri->center.y;
    cz = tri->center.z;
    px = zero;
    pz = zero;
    norm.x = tri->norm.x;
    norm.y = tri->norm.y;
    norm.z = tri->norm.z;
    v1.x = gColQueryLine[4] - cx;
    v1.y = gColQueryLine[5] - cy;
    v1.z = gColQueryLine[6] - cz;
    BodyVectorNorm(&v1, &tpB, (ColFrame*)&norm, tri->scale);
    if ((f64)tpB.y < lbl_80345D40) {
        return lbl_80345D70;
    }
    v1.x = gColQueryLine[0] - cx;
    v1.y = gColQueryLine[1] - cy;
    v1.z = gColQueryLine[2] - cz;
    BodyVectorNorm(&v1, &tpA, (ColFrame*)&norm, tri->scale);
    if (tpB.y < tpA.y) {
        return lbl_80345D70;
    }
    r2 = radius * radius;
    if ((tpB.y > lbl_80345D40 && tpA.y > lbl_80345D40) ||
        (tpB.y < *(volatile const f64*)&lbl_80345D40 &&
         tpA.y < lbl_80345D40)) {
        if ((tpB.y > radius && tpA.y > radius) ||
            (tpB.y < -radius && tpA.y < -radius)) {
            return lbl_80345D70;
        }
        cross = 0;
    } else {
        f32 ayA;
        f32 ayB;
        f32 sum;
        f32 t;

        cross = 1;
        dx = tpA.x - tpB.x;
        dz = tpA.z - tpB.z;
        if ((f64)fqdist(dx, dz) > lbl_80345D78) {
            sum = (ayB = btri_fabsf(tpB.y)) + (ayA = btri_fabsf(tpA.y));
            if (lbl_80345D40 == sum) {
                cross = 0;
            } else {
                t = btri_fabsf(tpB.y) / sum;
                px = tpB.x + dx * t;
                pz = tpB.z + dz * t;
            }
        } else {
            px = tpB.x;
            pz = tpB.z;
        }
    }
    zero2.x = lbl_80345D50;
    zero2.y = lbl_80345D50;
    zero2.z = lbl_80345D50;
    ex0.x = (f32)((f64)tri->x0 * 0.015625);
    ex0.z = (f32)((f64)tri->z0 * 0.015625);
    ex1.x = (f32)((f64)tri->x1 * 0.015625);
    ex1.z = (f32)((f64)tri->z1 * 0.015625);
    if (cross != 0) {
        cross = 1;
        num = ex0.x * pz - ex0.z * px;
        if ((f64)num > lbl_80345D40) {
            dist = LineLineDist3D2D(&tpB, &tpA, &o2, &zero2, &ex0, 1);
            cross = 0;
        }
        num = (ex1.x - ex0.x) * (pz - ex0.z) - (ex1.z - ex0.z) * (px - ex0.x);
        if ((f64)num > lbl_80345D40) {
            d = LineLineDist3D2D(&tpB, &tpA, &tmp1.value, &ex0, &ex1, 1);
            if (cross != 0 || d < dist) {
                dist = d;
                o2.x = tmp1.value.x;
                o2.y = tmp1.value.y;
                o2.z = tmp1.value.z;
            }
            cross = 0;
        }
        num = -ex1.x * (pz - ex1.z) - (-ex1.z * (px - ex1.x));
        if ((f64)num > lbl_80345D40) {
            d = LineLineDist3D2D(&tpB, &tpA, &tmp1.value, &zero2, &ex1, 1);
            if (cross != 0 || d < dist) {
                dist = d;
                o2.x = tmp1.value.x;
                o2.y = tmp1.value.y;
                o2.z = tmp1.value.z;
            }
            cross = 0;
        }
        if (cross == 0) {
            if (dist > r2) {
                return lbl_80345D70;
            }
        } else {
            o2.x = px;
            o2.y = lbl_80345D50;
            o2.z = pz;
        }
    } else {
        s32 side;
        s32 okB = 1;
        s32 okA = 1;
        f32 bx;
        f32 bz;
        f32 ax;
        f32 az;

        if (btri_fabsf(tpB.y) < btri_fabsf(tpA.y)) {
            side = 1;
        } else if (tpB.y == tpA.y) {
            side = 3;
        } else {
            side = 2;
        }
        bx = tpB.x;
        bz = tpB.z;
        ax = tpA.x;
        az = tpA.z;
        num = ex0.x * bz - ex0.z * bx;
        if ((f64)num > *(volatile const f64*)&lbl_80345D40) {
            okB = 0;
            if (!(side & 2)) {
                goto classified;
            }
        }
        num = ex0.x * az - ex0.z * ax;
        if ((f64)num > *(volatile const f64*)&lbl_80345D40) {
            okA = 0;
            if (!(side & 1)) {
                goto classified;
            }
        }
        num = (ex1.x - ex0.x) * (bz - ex0.z) - (ex1.z - ex0.z) * (bx - ex0.x);
        if ((f64)num > *(volatile const f64*)&lbl_80345D40) {
            okB = 0;
            if (!(side & 2)) {
                goto classified;
            }
        }
        num = (ex1.x - ex0.x) * (az - ex0.z) - (ex1.z - ex0.z) * (ax - ex0.x);
        if ((f64)num > *(volatile const f64*)&lbl_80345D40) {
            okA = 0;
            if (!(side & 1)) {
                goto classified;
            }
        }
        num = -ex1.x * (bz - ex1.z) - (-ex1.z * (bx - ex1.x));
        if ((f64)num > *(volatile const f64*)&lbl_80345D40) {
            okB = 0;
            if (!(side & 2)) {
                goto classified;
            }
        }
        num = -ex1.x * (az - ex1.z) - (-ex1.z * (ax - ex1.x));
        if ((f64)num > lbl_80345D40) {
            okA = 0;
        }
    classified:
        if (okB && (side & 1)) {
            o2.x = bx;
            o2.y = tpB.y;
            o2.z = bz;
            o2.y = lbl_80345D50;
        } else if (okA && (side & 2)) {
            o2.x = ax;
            o2.y = tpA.y;
            o2.z = az;
            o2.y = lbl_80345D50;
        } else {
            dist = LineLineDist3D2D(&tpB, &tpA, &o2, &zero2, &ex0, 0);
            d = LineLineDist3D2D(&tpB, &tpA, &tmp2, &ex0, &ex1, 0);
            if (d < dist) {
                dist = d;
                o2.x = tmp2.x;
                o2.y = tmp2.y;
                o2.z = tmp2.z;
            }
            d = LineLineDist3D2D(&tpB, &tpA, &tmp2, &zero2, &ex1, 0);
            if (d < dist) {
                dist = d;
                o2.x = tmp2.x;
                o2.y = tmp2.y;
                o2.z = tmp2.z;
            }
            if (dist > r2) {
                return lbl_80345D70;
            }
        }
    }
    if (out != NULL) {
        WorldVectorNorm(out, o2.x, o2.y, o2.z, tri->scale, (ColFrame*)&norm);
        out->x += cx;
        out->y += cy;
        out->z += cz;
    }
    return dist;
}
/* ------------------------------------------------------------------ */
/* Rotate a body-space vector into world space using a packed cos/sin  */
/* surface frame.  Degenerate frames (|sin| ~ 1) collapse to identity  */
/* or a 180-degree flip.                                               */
/* ------------------------------------------------------------------ */
static void BodyVectorNorm(Vec* in, Vec* out, ColFrame* f, f32 c) {
    f32 s = f->s;
    if ((f64)s > lbl_80345D80) {
        out->x = in->x;
        out->y = in->y;
        out->z = in->z;
        return;
    }
    if ((f64)s < lbl_80345D88) {
        out->x = in->x;
        out->y = -in->y;
        out->z = -in->z;
        return;
    }
    {
        f32 cs;
        f32 neg_ix;
        f32 cs_scaled;
        f32 t;
        f32 t_scaled;
        f32 iy;
        f32 ix;
        f32 iz;
        f32 iy_s;
        f32 mid_y;
        f32 iz_cs_scaled;
        f32 result_y;
        f32 neg_iz;
        f32 result_x;
        f32 negix_cs_scaled;
        f32 negiz_s;

        cs = f->c;
        iy = in->y;
        cs_scaled = cs * c;
        iz = in->z;
        ix = in->x;
        iy_s = iy * s;
        t = f->t;
        neg_ix = -ix;
        mid_y = ix * cs + iy_s;
        t_scaled = t * c;
        iz_cs_scaled = iz * cs_scaled;
        result_y = iz * t + mid_y;
        neg_iz = -iz;
        result_x = neg_ix * t_scaled + iz_cs_scaled;
        negix_cs_scaled = neg_ix * cs_scaled;
        negiz_s = neg_iz * s;
        out->x = result_x;
        out->y = result_y;
        out->z = t_scaled * negiz_s +
                 (s * negix_cs_scaled +
                  c * (iy * (lbl_80345D90 - s * s)));
    }
}

/* ------------------------------------------------------------------ */
/* Inverse of BodyVectorNorm: fold a world-space vector back into the  */
/* surface body frame.                                                 */
/* ------------------------------------------------------------------ */
static void WorldVectorNorm(Vec* out, f32 x, f32 y, f32 z, f32 c,
                            ColFrame* f) {
    f32 s = f->s;
    if ((f64)s > lbl_80345D80) {
        out->x = x;
        out->y = y;
        out->z = z;
        return;
    }
    if ((f64)s < lbl_80345D88) {
        out->x = x;
        out->y = -y;
        out->z = -z;
        return;
    }
    {
        f32 cs;
        f32 cs_scaled;
        f32 t;
        f32 t_scaled;

        cs = f->c;
        t = f->t;
        cs_scaled = cs * c;
        t_scaled = t * c;

        out->x = s * (-z * cs_scaled) + (-x * t_scaled + y * cs);
        out->y = y * s + c * (z * (lbl_80345D90 - s * s));
        out->z = t_scaled * (-z * s) + (x * cs_scaled + y * t);
    }
}

/* ------------------------------------------------------------------ */
/* Combined 3D/2D line-line distance test used by the triangle-edge    */
/* passes: computes both the full 3D distance (LineLineDist) and the   */
/* projected 2D distance (PointLineDist2D) and returns the smaller.    */
/* ------------------------------------------------------------------ */
#pragma opt_propagation off
static f32 LineLineDist3D2D(Vec* a0, Vec* a1, Vec* out,
                            Vec* b0, Vec* b1, s32 flattenY) {
    volatile f64 highPad;
    Vec da;
    volatile f32 daPad;
    Vec db;
    volatile f32 middle0;
    volatile f32 middle1;
    u8 unused[8];
    Vec point;
    struct {
        f32 pad[3];
        volatile f32 result;
    } sqrtLocal;
    register f32 length;

    da.x = a1->x - a0->x;
    da.y = a1->y - a0->y;
    da.z = a1->z - a0->z;
    db.x = b1->x - b0->x;
    db.y = b1->y - b0->y;
    db.z = b1->z - b0->z;

    length = da.x * da.x + da.z * da.z;
    if (length > lbl_80345D50) {
        f64 guess;
        guess = __frsqrte((f64)length);
        guess = lbl_80345D98 * guess *
                (lbl_80345DA0 - length * (guess * guess));
        guess = lbl_80345D98 * guess *
                (lbl_80345DA0 - length * (guess * guess));
        guess = lbl_80345D98 * guess *
                (lbl_80345DA0 - length * (guess * guess));
        sqrtLocal.result =
            (f32)(length *
                  (lbl_80345D98 * guess *
                   (lbl_80345DA0 - length * (guess * guess))));
        length = sqrtLocal.result;
    }

    if ((f64)length < lbl_80345D78) {
        f32 y;
        register f32 absA1;
        register f32 absA0;
        volatile f64 absPad;
        f32 distance = PointLineDist2D(a0, b0, &db, out);

        if (flattenY != 0) {
            y = lbl_80345D50;
        } else {
            sqrtLocal.pad[1] = a1->y;
            *(u32*)&sqrtLocal.pad[1] &= 0x7fffffff;
            absA0 = a0->y;
            absA1 = sqrtLocal.pad[1];
            sqrtLocal.pad[2] = absA0;
            *(u32*)&sqrtLocal.pad[2] &= 0x7fffffff;
            if (sqrtLocal.pad[2] <= absA1) {
                y = a0->y;
            } else {
                y = a1->y;
            }
        }
        out->y = lbl_80345D50;
        return y * y + distance;
    }

    {
        register f32 zero;
        register f32 z;
        f32 lenB;
        f32 lenA;

        point.x = b0->x;
        point.y = b0->y;
        z = b0->z;
        zero = lbl_80345D50;
        point.z = z;
        point.y = zero;
        db.y = zero;
        lenB = SlowNormalVector(&db);
        lenA = SlowNormalVector(&da);
        return LineLineDist(&point, &db, out, a0, &da, lenB, lenA);
    }
}
#pragma opt_propagation reset

/* ------------------------------------------------------------------ */
/* Shortest distance between two 3D line segments; closest points are  */
/* returned in *outA / *outB.                                          */
/* ------------------------------------------------------------------ */
#pragma dont_inline on
#pragma opt_propagation off
static f32 LineLineDist(Vec* pointB, Vec* dirB, Vec* out,
                        Vec* pointA, Vec* dirA, f32 lenB, f32 lenA) {
    u8 highPad[40];
    Vec tmpB;
    u8 endpointPad[4];
    Vec tmpA;
    u8 unused[48];
    struct {
        Vec value;
        u8 unused[44];
    } cpA[1];
    Vec dstTmp[3];
    Vec cpP[3];
    struct {
        u8 unused0[28];
        Vec value;
        u8 unused1[24];
    } cpB[1];
    Vec* endB;
    Vec* endA;
    Vec* dst;
    f32 cx;
    f32 cy;
    f32 cz;
    f32 denom;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 tB;
    f32 tA;
    f32 t;
    f32 inv;
    f32 dA2;
    f32 dB2;

    cy = dirB->z * dirA->x - dirB->x * dirA->z;
    cx = dirB->y * dirA->z - dirB->z * dirA->y;
    cz = dirB->x * dirA->y - dirB->y * dirA->x;
    denom = cx * cx + cy * cy;
    denom = cz * cz + denom;
    dx = pointA->x - pointB->x;
    dy = pointA->y - pointB->y;
    dz = pointA->z - pointB->z;
    if (denom != lbl_80345D50) {
        endB = NULL;
        endA = NULL;
        inv = (f32)(lbl_80345DA8 / denom);
        {
            f32 a1 = cz * (dx * dirA->y);
            f32 a2 = cx * (dy * dirA->z);
            f32 a3 = cy * (dz * dirA->x);
            f32 b1 = dz * (cx * dirA->y);
            f32 b2 = dx * (cy * dirA->z);
            f32 b3 = dy * (cz * dirA->x);
            f32 num = a1 + a2;

            num = a3 + num;
            num = num - b1;
            num = num - b2;
            num = num - b3;
            tB = inv * num;
        }
        if (tB <= lbl_80345D50) {
            endB = pointB;
        } else if (tB >= lenB) {
            tmpB.x = pointB->x + dirB->x * lenB;
            tmpB.y = pointB->y + dirB->y * lenB;
            tmpB.z = pointB->z + dirB->z * lenB;
            endB = &tmpB;
        }
        {
            f32 a1 = cz * (dx * dirB->y);
            f32 a2 = cx * (dy * dirB->z);
            f32 a3 = cy * (dz * dirB->x);
            f32 b1 = dz * (cx * dirB->y);
            f32 b2 = dx * (cy * dirB->z);
            f32 b3 = dy * (cz * dirB->x);
            f32 num = a1 + a2;

            num = a3 + num;
            num = num - b1;
            num = num - b2;
            num = num - b3;
            tA = inv * num;
        }
        if (tA <= *(volatile const f32*)&lbl_80345D50) {
            endA = pointA;
        } else if (tA >= lenA) {
            tmpA.x = pointA->x + dirA->x * lenA;
            tmpA.y = pointA->y + dirA->y * lenA;
            tmpA.z = pointA->z + dirA->z * lenA;
            endA = &tmpA;
        }
        if (endB == NULL && endA == NULL) {
            goto interior;
        }
        if (endB != NULL) {
            Vec* cp = &cpA[0].value;

            t = (endB->z - pointA->z) * dirA->z +
                ((endB->x - pointA->x) * dirA->x +
                 (endB->y - pointA->y) * dirA->y);
            if (t < *(volatile const f32*)&lbl_80345D50) {
                cp->x = pointA->x;
                cp->y = pointA->y;
                cp->z = pointA->z;
            } else if (t >= lenA) {
                cp->x = pointA->x + dirA->x * lenA;
                cp->y = pointA->y + dirA->y * lenA;
                cp->z = pointA->z + dirA->z * lenA;
            } else {
                cp->x = pointA->x + dirA->x * t;
                cp->y = pointA->y + dirA->y * t;
                cp->z = pointA->z + dirA->z * t;
            }
            dy = cp->y - endB->y;
            dx = cp->x - endB->x;
            dz = cp->z - endB->z;
            dB2 = dz * dz + (dx * dx + (dy * dy));
        } else {
            dB2 = lbl_80345DB0;
        }
        if (endA != NULL) {
            dst = out;
            if (out == NULL) {
                dst = &dstTmp[0];
            }
            t = (endA->z - pointB->z) * dirB->z +
                ((endA->x - pointB->x) * dirB->x +
                 (endA->y - pointB->y) * dirB->y);
            if (t < lbl_80345D50) {
                dst->x = pointB->x;
                dst->y = pointB->y;
                dst->z = pointB->z;
            } else if (t >= lenB) {
                dst->x = pointB->x + dirB->x * lenB;
                dst->y = pointB->y + dirB->y * lenB;
                dst->z = pointB->z + dirB->z * lenB;
            } else {
                dst->x = pointB->x + dirB->x * t;
                dst->y = pointB->y + dirB->y * t;
                dst->z = pointB->z + dirB->z * t;
            }
            dy = dst->y - endA->y;
            dx = dst->x - endA->x;
            dz = dst->z - endA->z;
            dA2 = dz * dz + (dx * dx + (dy * dy));
        } else {
            dA2 = lbl_80345DB0;
        }
        if (dB2 < dA2) {
            out->x = endB->x;
            out->y = endB->y;
            out->z = endB->z;
        } else {
            dB2 = dA2;
        }
        goto done;
    interior:
        out->x = pointB->x + dirB->x * tB;
        out->y = pointB->y + dirB->y * tB;
        out->z = pointB->z + dirB->z * tB;
        tmpA.x = pointA->x + dirA->x * tA;
        tmpA.y = pointA->y + dirA->y * tA;
        tmpA.z = pointA->z + dirA->z * tA;
        dy = tmpA.y - out->y;
        dx = tmpA.x - out->x;
        dz = tmpA.z - out->z;
        dB2 = dz * dz + (dx * dx + (dy * dy));
        goto done;
    } else {
        t = dy * dirB->y;
        t = dx * dirB->x + t;
        t = dz * dirB->z + t;
        if (t < lbl_80345D50) {
            f32 u = (pointB->y - pointA->y) * dirA->y;

            u = (pointB->x - pointA->x) * dirA->x + u;
            u = (pointB->z - pointA->z) * dirA->z + u;
            {
            Vec* cp = &cpP[0];

            if (u < lbl_80345D50) {
                cp->x = pointA->x;
                cp->y = pointA->y;
                cp->z = pointA->z;
            } else if (u >= lenA) {
                cp->x = pointA->x + dirA->x * lenA;
                cp->y = pointA->y + dirA->y * lenA;
                cp->z = pointA->z + dirA->z * lenA;
            } else {
                cp->x = pointA->x + dirA->x * u;
                cp->y = pointA->y + dirA->y * u;
                cp->z = pointA->z + dirA->z * u;
            }
            dy = cp->y - pointB->y;
            dx = cp->x - pointB->x;
            dz = cp->z - pointB->z;
            out->x = pointB->x;
            out->y = pointB->y;
            out->z = pointB->z;
            }
            dB2 = dz * dz + (dx * dx + (dy * dy));
            goto done;
        } else if (t >= lenB) {
            out->x = pointB->x + dirB->x * lenB;
            out->y = pointB->y + dirB->y * lenB;
            out->z = pointB->z + dirB->z * lenB;
            t = (out->z - pointA->z) * dirA->z +
                ((out->x - pointA->x) * dirA->x +
                 (out->y - pointA->y) * dirA->y);
            {
            Vec* cp = &cpB[0].value;

            if (t < lbl_80345D50) {
                cp->x = pointA->x;
                cp->y = pointA->y;
                cp->z = pointA->z;
            } else if (t >= lenA) {
                cp->x = pointA->x + dirA->x * lenA;
                cp->y = pointA->y + dirA->y * lenA;
                cp->z = pointA->z + dirA->z * lenA;
            } else {
                cp->x = pointA->x + dirA->x * t;
                cp->y = pointA->y + dirA->y * t;
                cp->z = pointA->z + dirA->z * t;
            }
            dy = cp->y - out->y;
            dx = cp->x - out->x;
            dz = cp->z - out->z;
            }
            dB2 = dz * dz + (dx * dx + (dy * dy));
            goto done;
        } else {
            out->x = pointB->x + dirB->x * t;
            out->y = pointB->y + dirB->y * t;
            out->z = pointB->z + dirB->z * t;
            dy = out->y - pointA->y;
            dx = out->x - pointA->x;
            dz = out->z - pointA->z;
            dB2 = dz * dz + (dx * dx + (dy * dy));
            goto done;
        }
    }
done:
    return dB2;
}
#pragma opt_propagation reset
#pragma dont_inline off

/* ------------------------------------------------------------------ */
/* 2D (xz-plane) distance from the segment [p0,p1] to a point, writing */
/* the closest point to *out.  Uses the PPC frsqrte + Newton-Raphson   */
/* reciprocal-square-root idiom for the segment length.                */
/* ------------------------------------------------------------------ */
#pragma opt_propagation off
static f32 PointLineDist2D(Vec* p0, Vec* p1, Vec* dir, Vec* out) {
    u8 unused[40];
    struct {
        f32 pad;
        volatile f32 result;
    } sqrtLocal;
    register f32 length;
    f64 guess;
    f32 inverse;
    f32 nx;
    f32 ny;
    f32 nz;
    f32 distance;

    length = dir->x * dir->x + dir->z * dir->z;
    if (length > 0.0f) {
        guess = __frsqrte((f64)length);

        guess = lbl_80345D98 * guess *
                (lbl_80345DA0 - length * guess * guess);
        guess = lbl_80345D98 * guess *
                (lbl_80345DA0 - length * guess * guess);
        guess = lbl_80345D98 * guess *
                (lbl_80345DA0 - length * guess * guess);
        sqrtLocal.result = (f32)(length *
                                 (lbl_80345D98 * guess *
                                  (lbl_80345DA0 - length * guess * guess)));
        length = sqrtLocal.result;
    }
    if ((f64)length == lbl_80345D40) {
        f32 dx = p1->x - p0->x;
        f32 dz = p1->z - p0->z;
        out->x = p1->x;
        out->y = p1->y;
        out->z = p1->z;
        return dx * dx + dz * dz;
    }

    inverse = (f32)(lbl_80345DA8 / (f64)length);
    nx = dir->x * inverse;
    ny = dir->y * inverse;
    nz = dir->z * inverse;
    distance = (p0->x - p1->x) * nx + (p0->z - p1->z) * nz;
    if (distance < 0.0f) {
        out->x = p1->x;
        out->y = p1->y;
        out->z = p1->z;
    } else if (distance >= length) {
        out->x = p1->x + dir->x;
        out->y = p1->y + dir->y;
        out->z = p1->z + dir->z;
    } else {
        out->x = nx * distance + p1->x;
        out->y = ny * distance + p1->y;
        out->z = nz * distance + p1->z;
    }

    {
        f32 dx = p0->x - out->x;
        f32 dz = p0->z - out->z;
        return dx * dx + dz * dz;
    }
}
#pragma opt_propagation reset
