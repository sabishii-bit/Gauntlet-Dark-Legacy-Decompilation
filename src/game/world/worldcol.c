/*
 * worldcol.c -- GCN WORLDCOL.OBJ scaffold.
 *
 * The Xbox PDB roster has exactly 15 functions and the GCN object contains
 * exactly 15 functions in reverse source order.  The tail is additionally
 * pinned by behavior: CreateMat3Norm builds a basis and PointLineColl performs
 * the point/segment test.
 *
 * .text       0x8000CF40..0x8000E8E8
 * extab       0x800054E0..0x80005548
 * extabindex  0x800087A0..0x8000883C
 */

typedef signed int s32;
typedef unsigned int u32;
typedef float f32;
typedef unsigned char u8;
typedef signed short s16;

typedef struct WorldCollisionResult {
    u8 _pad00[0x58];
    u8 query[0x30];
    f32 normal[3];
    u8 _pad94[156];
    f32 qnorm[3];   /* 0x130 query direction normal */
} WorldCollisionResult;

typedef struct Vec3 {
    f32 x;
    f32 y;
    f32 z;
} Vec3;

typedef struct CollisionPoint {
    f32 x;
    f32 y;
    f32 z;
    f32 _unused;
} CollisionPoint;

typedef struct FloorCollisionResult {
    u8 _pad00[0x34];
    f32 floorY;
    u8 _pad38[0xC];
    s32 current;
} FloorCollisionResult;

extern s32 lbl_80344188;
extern f32 lbl_80344190;
extern f32 lbl_80344194;
extern f32 lbl_80345720;
extern f32 lbl_80345724;
extern f32 lbl_80345728;
extern f32 lbl_8034572C;
extern f32 lbl_80345740;
extern f32 lbl_80345744;
extern f32 lbl_80345748;
extern f32 lbl_8034574C;
extern f32 lbl_80345750;
extern f32 lbl_8023CA50[];
extern WorldCollisionResult lbl_8023CA40;
extern FloorCollisionResult gFloorCollisionResult;

typedef double f64;
extern f64 lbl_80345730, lbl_80345738, lbl_803457A0, lbl_803457A8;
extern f32 lbl_80344164, lbl_80345764;

/* one collision triangle record (stride 0x28) */
typedef struct WorldTri {
    s16 layerLo;  /* 0x00 */
    s16 layerHi;  /* 0x02 */
    u8  _pad04[4];
    f32 nx;       /* 0x08 plane normal */
    f32 ny;       /* 0x0C */
    f32 nz;       /* 0x10 */
    u8  _pad14[20];
} WorldTri;

/* world grid description (0x8028CA8C block; only the walk fields typed) */
typedef struct WorldGridInfo {
    u8  _pad00[8];
    WorldTri* tris;  /* 0x08 triangle records          */
    u8  _pad0c[60];
    f32 cellW;    /* 0x48 world units per grid cell */
    f32 invCell;  /* 0x4C 1 / cellW                 */
    s32 gridW;    /* 0x50 cells across (X)          */
    s32 gridD;    /* 0x54 cells deep (Z)            */
    u8  _pad58[4];
    char* marks;  /* 0x5C per-tri visited stamps    */
} WorldGridInfo;
extern WorldGridInfo gWorldInfo;
extern f32 lbl_80344168, lbl_8034416C, lbl_80344170, lbl_80344174,
    lbl_80344178, lbl_8034417C;
extern char lbl_80110720[]; /* "GRID ERROR" */
void FatalError(const char* msg, int code);
extern char lbl_8034418C;             /* current visited stamp */
extern f32 lbl_80345760, lbl_80345778;
extern f64 lbl_80345780, lbl_80345788, lbl_80345790, lbl_80345798;
extern f32 lbl_8023F7F8[3];           /* query origin */
s32 TriLineCol(WorldTri* tri, f32* hit);
f32 BTriLineCol(f32 radius, WorldTri* tri, f32* hit);
extern u8 gIdentityMatrix[], lbl_80127DA0[];
void CopyMat3(void* src, void* dst);   /* 0x800BE8C8 */
f32 NormalVector(f32* v);               /* 0x800BDA98 */
f32 fqdist();                           /* 0x800BCB44 */

u32 WorldCollide(f32 radius, void* from, void* to, f32* result,
                 s32 flags, s32 mode);

/* Camera collision uses the common world query with the camera mask. */
s32 CameraCollide(void* from, void* to) {
    u32 hit;

    lbl_80344188 = 3;
    lbl_80344194 = lbl_80345720;
    lbl_80344190 = lbl_80345724;
    hit = WorldCollide(lbl_80345728, from, to, lbl_8023CA50, 0x3E, 0);
    if (hit != 0) {
        return 1;
    }
    return 0;
}

u32 WeaponWallCollide(f32 radius, void* from, void* to, f32* normal) {
    u32 hit;

    lbl_80344188 = 7;
    lbl_80344194 = lbl_80345720;
    lbl_80344190 = lbl_80345724;
    hit = WorldCollide(radius, from, to,
                       (f32*)lbl_8023CA40.query, 0x23E, 4);
    if (normal != 0) {
        if (hit != 0) {
            normal[0] = lbl_8023CA40.normal[0];
            normal[1] = lbl_8023CA40.normal[1];
            normal[2] = lbl_8023CA40.normal[2];
        } else {
            normal[2] = normal[1] = normal[0] = lbl_8034572C;
        }
    }
    return hit;
}

/* SlideAlongWall @0x8000D034 -- push a horizontal move `vel` back out of a wall
 * plane (point `wallpt`, unit `normal`), sliding it along the surface unless the
 * remaining motion is too shallow, in which case the axis is zeroed. */
u32 SlideAlongWall(f32 radius, f32* pos, f32* vel, f32* wallpt, f32* normal)
{
    u32 result;
    f64 vz = (f64)vel[2];
    f64 vx = (f64)vel[0];
    f64 pen;
    f64 avz;
    f64 avx;
    f64 dotx;
    f64 dotz;

    pen = (f64)(f32)(
        (f64)(f32)(
            (f64)((f32)((f64)pos[0] + vx) - wallpt[0]) * (f64)normal[0] +
            (f64)(((f32)((f64)pos[2] + vz) - wallpt[2]) * normal[2])) -
        radius);
    avz = vz;
    if (vz < lbl_80345730) {
        avz = -vz;
    }
    avx = vx;
    if (vx < lbl_80345730) {
        avx = -vx;
    }
    if ((f64)lbl_8034572C <= pen) {
        return 0;
    }
    dotx = (f64)(f32)((f64)normal[0] * pen);
    result = 1;
    if (avx < avz ||
        ((f64)lbl_8034572C < vx && (f64)lbl_8034572C < dotx) ||
        (vx < (f64)lbl_8034572C && dotx < (f64)lbl_8034572C)) {
        f64 avx2 = vx;
        f64 adotx;
        if (vx < lbl_80345730) {
            avx2 = -vx;
        }
        adotx = dotx;
        if (dotx < lbl_80345730) {
            adotx = -dotx;
        }
        if (lbl_80345738 * radius + avx2 <= adotx) {
            result = 0xFFFFFFFF;
            vel[0] = lbl_8034572C;
        } else {
            vel[0] = (f32)((f64)vel[0] - dotx);
        }
    }
    dotz = (f64)(f32)((f64)normal[2] * pen);
    if (avx < avz) {
        if ((f64)vel[2] <= (f64)lbl_8034572C || dotz <= (f64)lbl_8034572C) {
            if ((f64)lbl_8034572C <= (f64)vel[2]) {
                return result;
            }
            if ((f64)lbl_8034572C <= dotz) {
                return result;
            }
        }
    }
    avz = (f64)vel[2];
    if (avz < lbl_80345730) {
        avz = -avz;
    }
    {
        f64 adotz = dotz;
        if (dotz < lbl_80345730) {
            adotz = -dotz;
        }
        if (adotz < lbl_80345738 * radius + avz) {
            vel[2] = (f32)((f64)vel[2] - dotz);
            return result;
        }
    }
    vel[2] = lbl_8034572C;
    return 0xFFFFFFFF;
}

u32 EnemyWallCollide(f32 radius, void* from, void* to, f32* normal) {
    u32 hit;

    lbl_80344188 = 7;
    lbl_80344194 = lbl_80345740;
    lbl_80344190 = lbl_80345744;
    hit = WorldCollide(radius, from, to,
                       (f32*)lbl_8023CA40.query, 0x13A, 2);
    if (normal != 0) {
        if (hit != 0) {
            normal[0] = lbl_8023CA40.normal[0];
            normal[1] = lbl_8023CA40.normal[1];
            normal[2] = lbl_8023CA40.normal[2];
        } else {
            normal[2] = normal[1] = normal[0] = lbl_8034572C;
        }
    }
    return hit;
}

u32 PlayerWallCollide(f32 radius, void* from, void* to, f32* normal) {
    u32 hit;

    lbl_80344188 = 7;
    lbl_80344194 = lbl_80345740;
    lbl_80344190 = lbl_80345744;
    hit = WorldCollide(radius, from, to,
                       (f32*)lbl_8023CA40.query, 0x13A, 1);
    if (normal != 0) {
        if (hit != 0) {
            normal[0] = lbl_8023CA40.normal[0];
            normal[1] = lbl_8023CA40.normal[1];
            normal[2] = lbl_8023CA40.normal[2];
        } else {
            normal[2] = normal[1] = normal[0] = lbl_8034572C;
        }
    }
    return hit;
}

u32 FastWallCollide(void* from, void* to, f32* normal, s32 mode) {
    u32 hit;
    f32 collisionScratch[4];
    WorldCollisionResult* result = &lbl_8023CA40;
    s32 queryMode = mode;

    lbl_80344188 = 0x20;
    if (normal != 0) {
        lbl_80344188 |= 2;
    }
    lbl_80344194 = lbl_80345740;
    lbl_80344190 = lbl_80345744;
    hit = WorldCollide(lbl_80345728, from, to,
                       (f32*)result->query, 0x13A, queryMode);
    if (normal != 0) {
        if (hit != 0) {
            normal[0] = result->normal[0];
            normal[1] = result->normal[1];
            normal[2] = result->normal[2];
        } else {
            normal[2] = normal[1] = normal[0] = lbl_8034572C;
        }
    }
    if (hit != 0) {
        return 1;
    }
    return 0;
}

f32 FloorPos(f32 fallback, f32 radius, Vec3* position, s32 mode) {
    u8 unused[8];
    CollisionPoint to;
    CollisionPoint from;
    s32 hit;

    gFloorCollisionResult.current = hit = 0;
    from.x = position->x;
    from.y = position->y;
    from.z = position->z;
    to.x = position->x;
    to.y = position->y;
    to.z = position->z;
    from.y += lbl_80345748;
    to.y += lbl_8034574C;
    lbl_80344188 = 7;
    lbl_80344188 |= 0x10;
    lbl_80344194 = lbl_80345750;
    lbl_80344190 = lbl_80345724;
    if (WorldCollide(radius, &from, &to, (f32*)&gFloorCollisionResult, 0x23C,
                     mode) != 0) {
        hit = 1;
    }
    if (hit != 0) {
        return gFloorCollisionResult.floorY;
    }
    return fallback;
}

u32 FloorCollide(f32 radius, f32 yFrom, f32 yTo, Vec3* position,
                 FloorCollisionResult* result, s32 useExtra, s32 mode) {
    CollisionPoint from;
    CollisionPoint to;

    if (result == 0) {
        result = &gFloorCollisionResult;
    }
    result->current = 0;
    from.x = position->x;
    from.y = position->y;
    from.z = position->z;
    to.x = position->x;
    to.y = position->y;
    to.z = position->z;
    from.y += yFrom;
    to.y += yTo;
    lbl_80344188 = 7;
    if (useExtra != 0) {
        lbl_80344188 |= 0x10;
    }
    lbl_80344194 = lbl_80345750;
    lbl_80344190 = lbl_80345724;
    return WorldCollide(radius, &from, &to, (f32*)result, 0x23C, mode);
}

#define STUB(address, name) void name(void) {}

u32 WorldCollide(f32 radius, void* from, void* to, f32* result,
                 s32 flags, s32 mode) {
    return 0;
}
/* ExitCollisionEarly @0x8000DCD8 -- true when the collision mask requests the
 * fast-exit bit and the recorded floor height matches the query plane. */
s32 ExitCollisionEarly(void)
{
    s32 result = 0;

    if ((lbl_80344188 & 0x20) != 0) {
        if (lbl_80345730 == (f64)lbl_80344164) {
            result = 1;
        }
    }
    return result;
}
/* 0x8000DD00 -- step the grid walk to the next cell along the ray.
 * a = X-direction component (zero = walking straight along Z), b = current
 * Z-cross for the vertical case, c = dz/dx slope, d = walk radius. */
static s32 NextGrid(f32 a, f32 b, f32 c, f32 d, s32* gx, s32* gz)
{
    s32 xv = *gx;
    s32 zv = *gz;
    f32 step;
    s32 pos;
    f32 edge;
    f32 t;
    s32 idx;
    f64 z2;

    if (lbl_80345730 == a) {
        if (b >= lbl_80345730) {
            step = d;
            pos = 1;
            zv = zv + 1;
        } else {
            step = -d;
            pos = 0;
            zv = zv - 1;
        }
        edge = (f32)(xv + 1) * gWorldInfo.cellW + lbl_80344178;
    } else {
        if (c >= lbl_80345730) {
            step = d;
            pos = 1;
            zv = zv + 1;
        } else {
            step = -d;
            pos = 0;
            zv = zv - 1;
        }
        edge = (f32)(xv + 1) * gWorldInfo.cellW + lbl_80344178;
        b = c * (edge - lbl_80344168);
    }

    if (zv >= 0 && zv < gWorldInfo.gridD) {
        t = lbl_80345724 * step + (lbl_8034416C + b);
        if (pos != 0) {
            if (t > lbl_80344174) {
                t = lbl_80344174;
            }
            if (zv <= (s32)(gWorldInfo.invCell * (t - lbl_8034417C))) {
                *gz = zv;
                return 1;
            }
        } else {
            if (t < lbl_80344174) {
                t = lbl_80344174;
            }
            if (zv >= (s32)(gWorldInfo.invCell * (t - lbl_8034417C))) {
                *gz = zv;
                return 1;
            }
        }
    }

    if (edge > lbl_80344170) {
        return 0;
    }
    if (xv + 1 >= gWorldInfo.gridW) {
        return 0;
    }
    z2 = lbl_80345730;
    if (z2 == a || edge - lbl_80344168 < lbl_80345724 * d) {
        t = lbl_8034416C;
    } else {
        edge = edge + gWorldInfo.cellW;
        b = edge - (lbl_80344168 + lbl_80345724 * d);
        if (b > z2) {
            t = b * c + lbl_8034416C;
        } else {
            t = lbl_8034416C;
        }
    }
    if ((pos != 0 && t > lbl_80344174) || (pos == 0 && t < lbl_80344174)) {
        t = lbl_80344174;
    }
    idx = (s32)(gWorldInfo.invCell * (t - lbl_8034417C));
    if ((s32)(gWorldInfo.invCell * (t - lbl_8034417C)) < gWorldInfo.gridD &&
        idx >= 0) {
        *gx = xv + 1;
        *gz = idx;
        if (xv + 1 < 0 || idx < 0) {
            FatalError(lbl_80110720, 0x800000);
        }
        return 1;
    }
    return 0;
}
STUB(0x8000DFEC, WorldObjCollide)
/* 0x8000E3B8 -- test the query line/sweep against a list of triangles,
 * keeping the closest hit.  Returns the best (scaled) hit distance. */
static f32 CTriListCollide(f32 radius, s32 base, s32 count, WorldTri** outTri,
                           s16* idxList, f32* outPt, s32 layerLo, s32 layerHi,
                           s32 noFilter)
{
    f64 k788;
    f64 zd;
    f32 best;
    f32 result;
    f32 rad;
    f64 k798;
    f64 k790;
    f32 zf;
    f32* org;
    s32 i;
    s32 ti;
    WorldTri* tri;
    WorldCollisionResult* res;
    f32 d;
    f32 dd;
    f32 dot;
    f32 hit[9];

    rad = radius;
    org = lbl_8023F7F8;
    result = lbl_80345778;
    best = lbl_80345760;
    k788 = lbl_80345788;
    zf = lbl_8034572C;
    k790 = lbl_80345790;
    k798 = lbl_80345798;
    zd = lbl_80345730;
    res = &lbl_8023CA40;
    for (i = 0; i < count; i++) {
        if (idxList != 0) {
            ti = base + idxList[i];
        } else {
            ti = base + i;
        }
        if (gWorldInfo.marks[ti] == lbl_8034418C) {
            continue;
        }
        gWorldInfo.marks[ti] = lbl_8034418C;
        tri = &gWorldInfo.tris[ti];
        if (noFilter == 0) {
            if (tri->ny < lbl_80344194) {
                continue;
            }
            if (tri->ny > lbl_80344190) {
                continue;
            }
            if (tri->layerHi < layerLo) {
                continue;
            }
            if (tri->layerLo > layerHi) {
                continue;
            }
        }
        if ((lbl_80344188 & 0x20) != 0) {
            if (TriLineCol(tri, hit) != 0) {
                d = lbl_80345730;
            } else {
                d = lbl_80345780;
            }
        } else {
            d = BTriLineCol(rad, tri, hit);
        }
        if (d >= zd) {
            if ((lbl_80344188 & 0x20) != 0) {
                result = d;
                if (outTri != 0) {
                    *outTri = tri;
                }
                if (outPt != 0) {
                    outPt[0] = hit[0];
                    outPt[1] = hit[1];
                    outPt[2] = hit[2];
                }
                break;
            }
            if ((lbl_80344188 & 0x10) != 0) {
                if (zd == d) {
                    dd = (hit[1] - org[1]) * (hit[1] - org[1]) +
                         (hit[0] - org[0]) * (hit[0] - org[0]) +
                         (hit[2] - org[2]) * (hit[2] - org[2]);
                } else {
                    dd = k788 * d;
                }
            } else {
                dd = (hit[1] - org[1]) * (hit[1] - org[1]) +
                     (hit[0] - org[0]) * (hit[0] - org[0]) +
                     (hit[2] - org[2]) * (hit[2] - org[2]);
            }
            dot = res->qnorm[1] * tri->ny + res->qnorm[0] * tri->nx +
                  res->qnorm[2] * tri->nz;
            if (dot < zf) {
                dot = -dot;
            }
            if (dot < k790) {
                dd = dd * k798;
            }
            if (dd < best) {
                best = dd;
                result = dd;
                if (outTri != 0) {
                    *outTri = tri;
                }
                if (outPt != 0) {
                    outPt[0] = hit[0];
                    outPt[1] = hit[1];
                    outPt[2] = hit[2];
                }
            }
        }
    }
    return result;
}
/* CreateMat3Norm @0x8000E674 -- build an orthonormal basis whose Y axis is the
 * given (scaled) surface normal; snap to the up/down identity near vertical. */
void CreateMat3Norm(f32 scale, f32* mtx, f32* normal)
{
    f32 ny = normal[1];

    if ((f64)ny > lbl_803457A0) {
        CopyMat3(gIdentityMatrix, mtx);
    } else if ((f64)ny < lbl_803457A8) {
        CopyMat3(lbl_80127DA0, mtx);
    } else {
        f32 nz = normal[2];
        f32 nx = normal[0];
        f32 t = nz * scale;
        f32 nxs = nx * scale;
        mtx[0] = -t;
        mtx[1] = lbl_8034572C;
        mtx[2] = nxs;
        mtx[3] = lbl_8034572C;
        mtx[4] = nx;
        mtx[5] = ny;
        mtx[6] = nz;
        mtx[7] = lbl_8034572C;
        mtx[8] = (f32)(-(f64)nxs * (f64)ny);
        mtx[9] = (f32)(scale *
                       -(f64)(f32)((f64)ny * (f64)ny - (f64)lbl_80345764));
        mtx[10] = (f32)(-(f64)ny * (f64)t);
        mtx[11] = lbl_8034572C;
    }
}
/* PointLineColl @0x8000E73C -- project `point` onto segment [start,end], clamp
 * to the endpoints, store the closest point in `out`, and score the distance. */
void PointLineColl(f32* point, f32* start, f32* end, f32* out)
{
    f32 scratch[6];
    f32 seg[3];
    f64 len;
    f64 dx;
    f64 dy;
    f64 dz;
    f64 t;

    seg[0] = end[0] - start[0];
    seg[1] = end[1] - start[1];
    seg[2] = end[2] - start[2];
    len = (f64)NormalVector(seg);
    dz = point[2] - start[2];
    dy = point[1] - start[1];
    dx = (f64)(f32)((f64)point[0] - (f64)start[0]);
    t = (f64)(f32)(dz * (f64)seg[2] +
                   (f64)(f32)(dx * (f64)seg[0] + (f64)(f32)(dy * (f64)seg[1])));
    if ((f64)lbl_8034572C < t) {
        if (t < len) {
            if (out == 0) {
                out = scratch;
            }
            out[0] = (f32)((f64)seg[0] * t + (f64)start[0]);
            out[1] = (f32)((f64)seg[1] * t + (f64)start[1]);
            out[2] = (f32)((f64)seg[2] * t + (f64)start[2]);
            dy = point[1] - out[1];
            len = (f64)fqdist((f64)(point[0] - out[0]),
                              (f64)(point[2] - out[2]));
            fqdist(len, dy);
        } else {
            dy = point[1] - end[1];
            len = (f64)fqdist((f64)(f32)((f64)point[0] - (f64)end[0]),
                              (f64)(point[2] - end[2]));
            fqdist(len, dy);
            if (out != 0) {
                out[0] = end[0];
                out[1] = end[1];
                out[2] = end[2];
            }
        }
    } else {
        len = (f64)fqdist(dx, dz);
        fqdist(len, dy);
        if (out != 0) {
            out[0] = start[0];
            out[1] = start[1];
            out[2] = start[2];
        }
    }
}

#undef STUB
