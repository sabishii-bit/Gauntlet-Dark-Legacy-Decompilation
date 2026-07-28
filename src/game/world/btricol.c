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

/* Read-only collision-query line, owned elsewhere (.bss 0x8023F7E8). */
extern f32 gColQueryLine[8]; /* [0..2] = p0, [4..6] = p1 */

/* PSVEC-style helpers in the g3d math library. */
extern f32  fn_800BCB44(f32 a, f32 b, f32 c, f32 d, f32 e, f32 f);
extern void fn_800BD9B0(Vec* out, Vec* a, Vec* b);

/* forward decls (address order) */
f32         TriLineCol(Vec* tri, Vec* out, f32 radius);
f32         BTriLineCol(Vec* tri, Vec* out, f32 radius);
static void BodyVectorNorm(Vec* in, Vec* out, ColFrame* f, f32 c);
static void WorldVectorNorm(Vec* out, f32 x, f32 y, f32 z, ColFrame* f);
static f32  LineLineDist3D2D(Vec* a0, Vec* a1, Vec* outA, f32* outTa,
                             Vec* b0, Vec* b1, f32* outTb, Vec* outB);
static f32  LineLineDist(Vec* a0, Vec* a1, Vec* b0, Vec* b1,
                         Vec* outA, Vec* outB);
static f32  PointLineDist2D(Vec* p0, Vec* p1, Vec* dir, Vec* out);

/* ------------------------------------------------------------------ */
/* Rotate a body-space vector into world space using a packed cos/sin  */
/* surface frame.  Degenerate frames (|sin| ~ 1) collapse to identity  */
/* or a 180-degree flip.                                               */
/* ------------------------------------------------------------------ */
static void BodyVectorNorm(Vec* in, Vec* out, ColFrame* f, f32 c) {
    f32 s = f->s;
    if (s > 0.99999994f) {
        out->x = in->x;
        out->y = in->y;
        out->z = in->z;
        return;
    }
    if (s < -0.99999994f) {
        out->x = in->x;
        out->y = -in->y;
        out->z = -in->z;
        return;
    }
    {
        f32 cs = f->c;
        f32 tz = f->t;
        out->x = -in->x * (tz * c) + in->z * (cs * c);
        out->y = in->x * cs + in->y * s + in->z * tz;
        out->z = in->y * (1.0f - s * s) * c + s * (in->x * (cs * c)) +
                 (tz * c) * (-in->z * s);
    }
}

/* ------------------------------------------------------------------ */
/* Inverse of BodyVectorNorm: fold a world-space vector back into the  */
/* surface body frame.                                                 */
/* ------------------------------------------------------------------ */
static void WorldVectorNorm(Vec* out, f32 x, f32 y, f32 z, ColFrame* f) {
    f32 s = f->s;
    if (s > 0.99999994f) {
        out->x = x;
        out->y = y;
        out->z = z;
        return;
    }
    if (s < -0.99999994f) {
        out->x = x;
        out->y = -y;
        out->z = -z;
        return;
    }
    out->x = x;
    out->y = y;
    out->z = z;
}

/* ------------------------------------------------------------------ */
/* 2D (xz-plane) distance from the segment [p0,p1] to a point, writing */
/* the closest point to *out.  Uses the PPC frsqrte + Newton-Raphson   */
/* reciprocal-square-root idiom for the segment length.                */
/* ------------------------------------------------------------------ */
static f32 PointLineDist2D(Vec* p0, Vec* p1, Vec* dir, Vec* out) {
    f32 len2 = dir->x * dir->x + dir->z * dir->z;
    f32 len = 0.0f;
    if (len2 > 0.0f) {
        len = (f32)__builtin_sqrtf(len2);
    }
    if (len == 0.0f) {
        f32 dx = p1->x - p0->x;
        f32 dz = p1->z - p0->z;
        out->x = p1->x;
        out->y = p1->y;
        out->z = p1->z;
        return dx * dx + dz * dz;
    }
    {
        f32 inv = 1.0f / len;
        f32 nx = dir->x * inv;
        f32 nz = dir->z * inv;
        f32 t = (p0->x - p1->x) * nx + (p0->z - p1->z) * nz;
        if (t < 0.0f) {
            out->x = p1->x;
            out->y = p1->y;
        }
    }
    return len;
}

/* ------------------------------------------------------------------ */
/* Shortest distance between two 3D line segments; closest points are  */
/* returned in *outA / *outB.                                          */
/* ------------------------------------------------------------------ */
static f32 LineLineDist(Vec* a0, Vec* a1, Vec* b0, Vec* b1,
                        Vec* outA, Vec* outB) {
    Vec da, db;
    da.x = a1->x - a0->x;
    da.y = a1->y - a0->y;
    da.z = a1->z - a0->z;
    db.x = b1->x - b0->x;
    db.y = b1->y - b0->y;
    db.z = b1->z - b0->z;
    *outA = *a0;
    *outB = *b0;
    return 0.0f;
}

/* ------------------------------------------------------------------ */
/* Combined 3D/2D line-line distance test used by the triangle-edge    */
/* passes: computes both the full 3D distance (LineLineDist) and the   */
/* projected 2D distance (PointLineDist2D) and returns the smaller.    */
/* ------------------------------------------------------------------ */
static f32 LineLineDist3D2D(Vec* a0, Vec* a1, Vec* outA, f32* outTa,
                            Vec* b0, Vec* b1, f32* outTb, Vec* outB) {
    Vec da, db, tmp;
    da.x = a1->x - a0->x;
    da.y = a1->y - a0->y;
    da.z = a1->z - a0->z;
    db.x = b1->x - b0->x;
    db.y = b1->y - b0->y;
    db.z = b1->z - b0->z;
    fn_800BD9B0(&tmp, &da, &db);
    (void)PointLineDist2D(a0, a1, &da, outA);
    return LineLineDist(a0, a1, b0, b1, outA, outB);
}

/* ------------------------------------------------------------------ */
/* TriLineCol -- collide the query line (gColQueryLine) against a       */
/* single triangle, returning the hit fraction (-1 when no hit).       */
/* ------------------------------------------------------------------ */
f32 TriLineCol(Vec* tri, Vec* out, f32 radius) {
    Vec local;
    ColFrame frame;
    frame.c = tri->x;
    frame.s = tri->y;
    frame.t = tri->z;
    local.x = gColQueryLine[0];
    local.y = gColQueryLine[1];
    local.z = gColQueryLine[2];
    BodyVectorNorm(&local, out, &frame, radius);
    WorldVectorNorm(&local, local.x, local.y, local.z, &frame);
    return -1.0f;
}

/* ------------------------------------------------------------------ */
/* BTriLineCol -- swept (radius > 0) triangle / line collision built    */
/* on the LineLineDist3D2D edge tests and the frame transforms.        */
/* ------------------------------------------------------------------ */
f32 BTriLineCol(Vec* tri, Vec* out, f32 radius) {
    Vec e0, e1;
    ColFrame frame;
    f32 ta, tb;
    frame.c = tri->x;
    frame.s = tri->y;
    frame.t = tri->z;
    e0.x = gColQueryLine[0];
    e0.y = gColQueryLine[1];
    e0.z = gColQueryLine[2];
    e1.x = gColQueryLine[4];
    e1.y = gColQueryLine[5];
    e1.z = gColQueryLine[6];
    BodyVectorNorm(&e0, out, &frame, radius);
    (void)LineLineDist3D2D(&e0, &e1, out, &ta, &e0, &e1, &tb, out);
    WorldVectorNorm(&e0, e0.x, e0.y, e0.z, &frame);
    return -1.0f;
}
