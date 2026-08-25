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
typedef unsigned short u16;
typedef signed char s8;

typedef struct WorldCollisionResult {
    f32 hitPt[3];      /* 0x000 closest hit point */
    u8 _pad0c[0x4C];
    u8 query[0x30];    /* 0x058 */
    f32 normal[3];     /* 0x088 */
    u8 _pad94[132];
    f32 hitAlt[3];     /* 0x118 secondary-channel hit point */
    u8 _pad124[4];
    f32 bestAlt;       /* 0x128 secondary-channel best distance */
    void* objAlt;      /* 0x12C secondary-channel object */
    f32 qnorm[3];      /* 0x130 query direction normal */
    u8 _pad13c[4];
    f32 qdir2[3];      /* 0x140 query end/dir vector */
    u8 _pad14c[4];
    f32 qpos[3];       /* 0x150 query start point */
} WorldCollisionResult;

/* world object record (collidable prop) */
typedef struct WObj {
    u8 _pad00[0x10];
    u32 flags;        /* 0x10 */
    u8 _pad14[4];
    struct WObj* parent; /* 0x18 */
    f32 pos[3];       /* 0x1C */
    f32* mat;         /* 0x28 */
    u8 _pad2c[4];
    f32 radius;       /* 0x30 */
    u8 _pad34;
    u8 group;         /* 0x35 collision group bits */
    u8 _pad36[2];
    s32 triBase;      /* 0x38 */
} WObj;

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
    f32 scale;    /* 0x04 basis scale (inverse XZ length) */
    f32 nx;       /* 0x08 plane normal */
    f32 ny;       /* 0x0C */
    f32 nz;       /* 0x10 */
    u8  _pad14[20];
} WorldTri;

/* per-row column record of the static grid (stride 8) */
typedef struct WorldGridCol {
    u16 min;   /* 0x00 first occupied X cell   */
    u16 max;   /* 0x02 last occupied X cell    */
    u32 base;  /* 0x04 index into cell words   */
} WorldGridCol;

/* world grid description (0x8028CA8C block; only the walk fields typed) */
typedef struct WorldGridInfo {
    u8  _pad00[4];
    u8* objs;        /* 0x04 world object array, stride 0x3C */
    WorldTri* tris;  /* 0x08 triangle records          */
    WorldGridCol* cols; /* 0x0C per-Z-row column records */
    u32* cells;      /* 0x10 packed cell words (off|cnt<<22) */
    u8* lists;       /* 0x14 per-cell object/tri lists */
    f32 originX;     /* 0x18 grid origin X             */
    u8  _pad1c[4];
    f32 originZ;     /* 0x20 grid origin Z             */
    u8  _pad24[0x24];
    f32 cellW;    /* 0x48 world units per grid cell */
    f32 invCell;  /* 0x4C 1 / cellW                 */
    s32 gridW;    /* 0x50 cells across (X)          */
    s32 gridD;    /* 0x54 cells deep (Z)            */
    s32 stamp;    /* 0x58 rolling visited stamp     */
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
void GetWorldMat(void* node, f32* mtx, s32 mode);
static f32 CTriListCollide(f32 radius, s32 base, s32 count, WorldTri** outTri,
                           s16* idxList, f32* outPt, s32 layerLo, s32 layerHi,
                           s32 noFilter);
void MulBodyVecMat4(f32* src, f32* dst, f32* mtx);
extern f32 lbl_8023F7E8[3];
extern f32 lbl_80344198, lbl_8034419C, lbl_803441A0;
extern f64 lbl_80345770;
extern WObj* lbl_80344160;
extern s32 lbl_80344180;
extern WorldTri* lbl_80344184;
extern u8 gIdentityMatrix[], lbl_80127DA0[];
void CopyMat3(void* src, void* dst);   /* 0x800BE8C8 */
f32 NormalVector(f32* v);               /* 0x800BDA98 */
f32 fqdist();                           /* 0x800BCB44 */
void WorldVector(f32* v, f32* out, f32* mtx);
void MulVec4Mat4(f32* v, f32* out, f32* mtx);
void WorldDynCollide(s32 flags, s32 mode, f32 minx, f32 maxx, f32 minz,
                     f32 maxz, f32 dx, f32 dz, f32 slope, f32 rad);
extern f64 lbl_80345758;
void CreateMat3Norm(f32 scale, f32* mtx, f32* normal);
static s32 NextGrid(f32 a, f32 b, f32 c, f32 d, s32* gx, s32* gz);
static void WorldObjCollide(f32 rad, WObj* obj, s32 count, s16* list, f32* mtx);

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
#define WZERO64 (*(volatile f64*)&lbl_80345730)
#define WZERO32 (*(volatile f32*)&lbl_8034572C)

u32 SlideAlongWall(f32 radius, f32* pos, f32* vel, f32* wallpt, f32* normal)
{
    u32 result;
    s32 flag;
    f32 vz = vel[2];
    f32 vx = vel[0];
    f32 pen;
    f32 dotx;
    f32 dotz;
    f32 avx;
    f32 adotx;
    f32 avz;
    f32 adotz;
    f32 z1;

    pen = ((pos[0] + vx) - wallpt[0]) * normal[0] +
          ((pos[2] + vz) - wallpt[2]) * normal[2] - radius;
    if (vz < WZERO64) {
        vz = -vz;
    }
    if (vx < WZERO64) {
        avx = -vx;
    } else {
        avx = vx;
    }
    if (avx < vz) {
        flag = 1;
    } else {
        flag = 0;
    }
    if (!(pen < lbl_8034572C)) {
        goto ret0;
    }
    dotx = normal[0] * pen;
    result = 1;
    if (flag != 0 || (vx > lbl_8034572C && dotx > lbl_8034572C) ||
        ((z1 = WZERO32, vx < z1) && dotx < z1)) {
        if (vx < WZERO64) {
            vx = -vx;
        }
        if (dotx < WZERO64) {
            adotx = -dotx;
        } else {
            adotx = dotx;
        }
        if (adotx < lbl_80345738 * radius + vx) {
            vel[0] = vel[0] - dotx;
        } else {
            result = 0xFFFFFFFF;
            vel[0] = WZERO32;
        }
    }
    dotz = normal[2] * pen;
    if (flag != 0) {
        z1 = WZERO32;
        if (!(vel[2] > z1 && dotz > z1)) {
            z1 = WZERO32;
            if (!(vel[2] < z1)) {
                return result;
            }
            if (!(dotz < z1)) {
                return result;
            }
        }
    }
    avz = vel[2];
    if (avz < WZERO64) {
        avz = -avz;
    }
    if (dotz < WZERO64) {
        adotz = -dotz;
    } else {
        adotz = dotz;
    }
    if (adotz < lbl_80345738 * radius + avz) {
        vel[2] = vel[2] - dotz;
        return result;
    }
    result = 0xFFFFFFFF;
    vel[2] = WZERO32;
    return result;
ret0:
    return 0;
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

/* WorldCollide @0x8000D578 -- sweep a sphere/line query from `from` to `to`
 * through the static world grid (then the dynamic objects), recording the
 * closest hit in the shared collision channel and optionally writing a hit
 * basis/point back into `result`. */
u32 WorldCollide(f32 radius, void* fromv, void* tov, f32* result,
                 s32 flags, s32 mode) {
    WorldCollisionResult* res = &lbl_8023CA40;
    f32* from = (f32*)fromv;
    f32* to = (f32*)tov;
    s32 offGrid = 0;
    f32 dx;
    f32 dz;
    f32 slope;
    f32 t;
    s32 gx;
    s32 gz;
    WObj* obj;
    u32 oflags;
    f32 vecBuf[3];
    f32 mtxBuf[16];

    res->qpos[0] = from[0];
    res->qpos[1] = from[1];
    res->qpos[2] = from[2];
    res->qdir2[0] = to[0];
    res->qdir2[1] = to[1];
    res->qdir2[2] = to[2];
    res->qnorm[0] = to[0] - from[0];
    res->qnorm[1] = to[1] - from[1];
    res->qnorm[2] = to[2] - from[2];
    lbl_803441A0 = NormalVector(res->qnorm);

    if (from[0] <= to[0]) {
        lbl_80344168 = from[0];
        lbl_8034416C = from[2];
        lbl_80344170 = to[0];
        lbl_80344174 = to[2];
    } else {
        lbl_80344170 = from[0];
        lbl_80344174 = from[2];
        lbl_80344168 = to[0];
        lbl_8034416C = to[2];
    }
    if (from[1] < to[1]) {
        lbl_8034419C = from[1] - radius;
        lbl_80344198 = to[1] + radius;
    } else {
        lbl_8034419C = to[1] - radius;
        lbl_80344198 = from[1] + radius;
    }

    dx = lbl_80344170 - lbl_80344168;
    dz = lbl_80344174 - lbl_8034416C;
    slope = lbl_8034572C;
    if (dx != lbl_8034572C) {
        slope = dz / dx;
    }
    lbl_80344168 -= radius;
    lbl_80344170 += radius;
    if (dz >= lbl_8034572C) {
        lbl_8034416C -= radius;
        lbl_80344174 += radius;
    } else {
        lbl_8034416C += radius;
        lbl_80344174 -= radius;
    }

    lbl_80344178 = gWorldInfo.originX;
    lbl_8034417C = gWorldInfo.originZ;
    if (lbl_80344170 < lbl_80344178) {
        offGrid = 1;
    }
    if (lbl_80344168 < lbl_80344178) {
        if (lbl_8034572C != dx) {
            t = (lbl_80344178 - lbl_80344168) / dx;
            if ((f64)t < lbl_80345730) {
                t = -t;
            }
            lbl_8034416C = dz * t + lbl_8034416C;
        }
        lbl_80344168 = lbl_80344178;
    }
    if (lbl_80344174 < lbl_8034417C) {
        if (lbl_8034572C == dz) {
            offGrid = 1;
        } else {
            t = (lbl_8034417C - lbl_80344174) / dz;
            if ((f64)t < lbl_80345730) {
                t = -t;
            }
            lbl_80344170 = -(dx * t - lbl_80344170);
            lbl_80344174 = (f32)(lbl_80345758 + lbl_8034417C);
        }
    }
    if (lbl_8034416C < lbl_8034417C) {
        if (lbl_8034572C != dz) {
            t = (lbl_8034417C - lbl_8034416C) / dz;
            if ((f64)t < lbl_80345730) {
                t = -t;
            }
            lbl_80344168 = dx * t + lbl_80344168;
        }
        lbl_8034416C = lbl_8034417C;
    }

    gWorldInfo.stamp = gWorldInfo.stamp + 1;
    if (gWorldInfo.stamp > 0xFF) {
        gWorldInfo.stamp = 1;
    }
    lbl_8034418C = (char)gWorldInfo.stamp;
    lbl_80344160 = 0;
    lbl_80344164 = lbl_80345760;
    res->objAlt = 0;
    res->bestAlt = lbl_80345760;

    if (offGrid == 0) {
        f64 earlyZero;

        gx = (s32)(gWorldInfo.invCell * (lbl_80344168 - lbl_80344178));
        gz = (s32)(gWorldInfo.invCell * (lbl_8034416C - lbl_8034417C));
        gx = gx < 0 ? 0 :
             (gx > gWorldInfo.gridW - 1 ? gWorldInfo.gridW - 1 : gx);
        gz = gz < 0 ? 0 :
             (gz > gWorldInfo.gridD - 1 ? gWorldInfo.gridD - 1 : gz);
        earlyZero = lbl_80345730;
        do {
            WorldGridCol* col = &gWorldInfo.cols[gz];
            if (gx >= col->min && gx <= col->max) {
                u32 word = gWorldInfo.cells[col->base + (gx - col->min)];
                s32 off = word & 0x3FFFFF;
                s32 cnt = word >> 22;
                s32 i;
                for (i = 0; i < cnt; i++) {
                    u8* entry = gWorldInfo.lists + off;
                    s16 tcount = *(s16*)(entry + 2);
                    if (tcount != 0) {
                        s32 grp;
                        obj = (WObj*)(gWorldInfo.objs + *(s16*)entry * 0x3C);
                        grp = (s8)obj->group;
                        if (obj->parent != 0) {
                            grp |= (s8)obj->parent->group;
                        }
                        if ((obj->flags & flags) != 0 && (grp & mode) == 0 &&
                            obj->triBase >= 0) {
                            WorldObjCollide(radius, obj, tcount,
                                            (s16*)(entry + 4), 0);
                        }
                        if ((lbl_80344188 & 0x20) != 0 &&
                            earlyZero == (f64)lbl_80344164) {
                            break;
                        }
                        off = (tcount - 1) * 2 + off + 6;
                    }
                }
            }
            if ((lbl_80344188 & 0x20) != 0 &&
                earlyZero == (f64)lbl_80344164) {
                break;
            }
        } while (NextGrid(dx, dz, slope, radius, &gx, &gz) != 0);
    }

    if ((lbl_80344188 & 0x20) == 0 || lbl_80345730 != (f64)lbl_80344164) {
        WorldDynCollide(flags, mode, lbl_80344168, lbl_80344170, lbl_8034416C,
                        lbl_80344174, dx, dz, slope, radius);
    }

    if (result != 0 && lbl_80344160 != 0) {
        obj = lbl_80344160;
        if ((obj->flags & 0x1000000) != 0 && (lbl_80344188 & 6) != 0) {
            GetWorldMat(obj->mat, mtxBuf, 0);
        }
        if ((lbl_80344188 & 4) != 0) {
            obj = lbl_80344160;
            oflags = obj->flags;
            if ((oflags & 0x1000000) != 0) {
                WorldVector(&lbl_80344184->nx, vecBuf, mtxBuf);
                CreateMat3Norm(lbl_80344184->scale, result, vecBuf);
                NormalVector(result);
                NormalVector(result + 8);
            } else if ((oflags & 0x1000) != 0) {
                if ((oflags & 1) == 0) {
                    WorldVector(&lbl_80344184->nx, vecBuf, obj->mat);
                    CreateMat3Norm(lbl_80344184->scale, result, vecBuf);
                    NormalVector(result);
                    NormalVector(result + 8);
                } else {
                    CreateMat3Norm(lbl_80344184->scale, result,
                                   &lbl_80344184->nx);
                }
            } else {
                CreateMat3Norm(lbl_80344184->scale, result,
                               &lbl_80344184->nx);
            }
        }
        if ((lbl_80344188 & 2) != 0) {
            obj = lbl_80344160;
            oflags = obj->flags;
            if ((oflags & 0x1000000) != 0) {
                MulVec4Mat4(res->hitPt, result + 12, mtxBuf);
            } else if ((oflags & 0x1000) != 0) {
                if ((oflags & 1) == 0) {
                    MulVec4Mat4(res->hitPt, result + 12, obj->mat);
                } else {
                    result[12] = res->hitPt[0] + obj->mat[12];
                    result[13] = res->hitPt[1] + lbl_80344160->mat[13];
                    result[14] = res->hitPt[2] + lbl_80344160->mat[14];
                    result[15] = lbl_80345764;
                }
            } else {
                result[12] = res->hitPt[0];
                result[13] = res->hitPt[1];
                result[14] = res->hitPt[2];
                result[15] = lbl_80345764;
            }
        }
        *(WObj**)(result + 17) = lbl_80344160;
        result[16] = lbl_80344164;
    }
    return (u32)lbl_80344160;
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
    f64 z2;
    s32 xv = *gx;
    s32 zv = *gz;
    f32 step;
    s32 pos;
    f32 edge;
    f32 t;
    f32 minX;
    s32 idx;
    u8 _pad[8];

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
    z2 = *(volatile f64*)&lbl_80345730;
    if (z2 == a || edge - (minX = lbl_80344168) < lbl_80345724 * d) {
        t = lbl_8034416C;
    } else {
        edge = edge + gWorldInfo.cellW;
        b = edge - (minX + lbl_80345724 * d);
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
    if ((s32)(gWorldInfo.invCell * (t - lbl_8034417C)) < gWorldInfo.gridD) {
        if (idx >= 0) {
            goto commit;
        }
    }
    return 0;
commit:
    *gx = xv + 1;
    *gz = idx;
    if (xv + 1 < 0 || idx < 0) {
        FatalError(lbl_80110720, 0x800000);
    }
    return 1;
}
/* 0x8000DFEC -- sweep the query line against one world object: quick
 * sphere/line rejection, transform the query into object space, then run the
 * triangle list and record the best hit in the active result channel. */
static void WorldObjCollide(f32 rad, WObj* obj, s32 count, s16* list, f32* mtx)
{
    WorldCollisionResult* res = &lbl_8023CA40;
    u32 flags = obj->flags;
    f32 qx;
    f32 qy;
    f32 qz;
    f32* p;
    f32 reach;
    f32 lim;
    f32 t;
    f32 nx;
    f32 ny;
    f32 nz;
    f32 ymax;
    f32 ymin;
    f32 d;
    f32* m2;
    u8 unused[8];
    f32 v[3];
    f32 mtxBuf[16];
    WorldTri* triOut;

    if ((flags & 0x1000000) != 0) {
        if (mtx == 0) {
            GetWorldMat(obj->mat, mtxBuf, 0);
            mtx = mtxBuf;
        }
        p = mtx + 12;
    } else if ((flags & 0x1000) != 0) {
        p = obj->mat + 12;
    } else {
        p = obj->pos;
    }

    v[0] = p[0] - (qx = res->qpos[0]);
    v[1] = p[1] - (qy = res->qpos[1]);
    v[2] = p[2] - (qz = res->qpos[2]);
    reach = obj->radius + rad;
    lim = reach + lbl_803441A0;
    t = v[0] * (nx = res->qnorm[0]) + v[1] * (ny = res->qnorm[1]) +
        v[2] * (nz = res->qnorm[2]);
    if (t > lim || t < -reach) {
        return;
    }
    v[0] = nx * t + qx;
    v[1] = ny * t + qy;
    v[2] = nz * t + qz;
    v[0] = p[0] - v[0];
    v[1] = p[1] - v[1];
    v[2] = p[2] - v[2];
    if (v[0] * v[0] + v[1] * v[1] + v[2] * v[2] < reach * reach) {
        goto sphere_hit;
    }
    return;

sphere_hit:
    flags = obj->flags;
    if ((flags & 0x1000000) != 0) {
        MulBodyVecMat4(res->qpos, lbl_8023F7F8, mtx);
        MulBodyVecMat4(res->qdir2, lbl_8023F7E8, mtx);
        ymax = mtx[5] * (lbl_8034419C - mtx[13]);
        ymin = mtx[5] * (lbl_80344198 - mtx[13]);
    } else if ((flags & 0x1000) != 0) {
        m2 = obj->mat;
        if ((flags & 1) == 0) {
            MulBodyVecMat4(res->qpos, lbl_8023F7F8, m2);
            MulBodyVecMat4(res->qdir2, lbl_8023F7E8, m2);
            ymax = m2[5] * (lbl_8034419C - m2[13]);
            ymin = m2[5] * (lbl_80344198 - m2[13]);
        } else {
            lbl_8023F7F8[0] = qx - m2[12];
            lbl_8023F7F8[1] = qy - m2[13];
            lbl_8023F7F8[2] = qz - m2[14];
            lbl_8023F7E8[0] = res->qdir2[0] - m2[12];
            lbl_8023F7E8[1] = res->qdir2[1] - m2[13];
            lbl_8023F7E8[2] = res->qdir2[2] - m2[14];
            ymax = lbl_8034419C - m2[13];
            ymin = lbl_80344198 - m2[13];
        }
    } else {
        lbl_8023F7F8[0] = qx;
        lbl_8023F7F8[1] = qy;
        lbl_8023F7F8[2] = qz;
        lbl_8023F7E8[0] = res->qdir2[0];
        lbl_8023F7E8[1] = res->qdir2[1];
        lbl_8023F7E8[2] = res->qdir2[2];
        ymax = lbl_8034419C;
        ymin = lbl_80344198;
    }

    d = CTriListCollide(rad, obj->triBase, count, &triOut, list, v,
                        (s32)(lbl_80345770 * ymax), (s32)(lbl_80345770 * ymin),
                        obj->flags & 0x40);
    if (d >= lbl_80345730) {
        if ((obj->flags & 0x200) != 0) {
            f32* q;
            if (d < *(q = &res->bestAlt)) {
                res->objAlt = obj;
                *q = d;
                res->hitAlt[0] = v[0];
                res->hitAlt[1] = v[1];
                res->hitAlt[2] = v[2];
            }
        } else if (d < lbl_80344164) {
            lbl_80344160 = obj;
            lbl_80344164 = d;
            lbl_80344184 = triOut;
            lbl_80344180 =
                (s32)((u8*)triOut - (u8*)gWorldInfo.tris) / 40;
            res->hitPt[0] = v[0];
            res->hitPt[1] = v[1];
            res->hitPt[2] = v[2];
        }
    }
}
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
    f32 dy;
    f32 dx;
    f32 dz;
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
                    dy = hit[1] - org[1];
                    dx = hit[0] - org[0];
                    dz = hit[2] - org[2];
                    dd = (dy * dy + dx * dx) + dz * dz;
                } else {
                    dd = k788 * d;
                }
            } else {
                dy = hit[1] - org[1];
                dx = hit[0] - org[0];
                dz = hit[2] - org[2];
                dd = (dy * dy + dx * dx) + dz * dz;
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
        f32 nx;
        f32 nxs;
        f32 nz;
        f32 t;
        f32 zero;
        nz = normal[2];
        nx = normal[0];
        t = nz * scale;
        nxs = nx * scale;
        mtx[0] = -t;
        zero = lbl_8034572C;
        mtx[1] = zero;
        mtx[2] = nxs;
        mtx[3] = zero;
        mtx[4] = nx;
        mtx[5] = ny;
        mtx[6] = nz;
        mtx[7] = zero;
        mtx[8] = -nxs * ny;
        mtx[9] = scale * (lbl_80345764 - ny * ny);
        mtx[10] = -ny * t;
        mtx[11] = zero;
    }
}
/* PointLineColl @0x8000E73C -- project `point` onto segment [start,end], clamp
 * to the endpoints, store the closest point in `out`, and score the distance. */
void PointLineColl(f32* point, f32* start, f32* end, f32* out)
{
    f32 seg[3];
    f32 scratch[6];
    f32 len;
    f32 t;
    f32 dz;
    f32 dx;
    f32 dy;

    seg[0] = end[0] - start[0];
    seg[1] = end[1] - start[1];
    seg[2] = end[2] - start[2];
    len = NormalVector(seg);
    dx = point[0] - start[0];
    dy = point[1] - start[1];
    dz = point[2] - start[2];
    t = dx * seg[0] + dy * seg[1] + dz * seg[2];
    if (t <= lbl_8034572C) {
        len = fqdist(dx, dz);
        fqdist(len, dy);
        if (out != 0) {
            out[0] = start[0];
            out[1] = start[1];
            out[2] = start[2];
        }
    } else if (t >= len) {
        dx = point[0] - end[0];
        dz = point[2] - end[2];
        dy = point[1] - end[1];
        len = fqdist(dx, dz);
        fqdist(len, dy);
        if (out != 0) {
            out[0] = end[0];
            out[1] = end[1];
            out[2] = end[2];
        }
    } else {
        if (out == 0) {
            out = scratch;
        }
        out[0] = seg[0] * t + start[0];
        out[1] = seg[1] * t + start[1];
        out[2] = seg[2] * t + start[2];
        dx = point[0] - out[0];
        dz = point[2] - out[2];
        dy = point[1] - out[1];
        len = fqdist(dx, dz);
        fqdist(len, dy);
    }
}

#undef STUB
