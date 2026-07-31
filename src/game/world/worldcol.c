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

typedef struct WorldCollisionResult {
    u8 _pad00[0x58];
    u8 query[0x30];
    f32 normal[3];
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
extern f64 lbl_80345730, lbl_803457A0, lbl_803457A8;
extern f32 lbl_80344164, lbl_80345764;
extern u8 gIdentityMatrix[], lbl_80127DA0[];
void CopyMat3(void* src, void* dst);   /* 0x800BE8C8 */

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

void SlideAlongWall(void) {
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
    if ((lbl_80344188 & 0x20) == 0 ||
        lbl_80345730 != (f64)lbl_80344164) {
        return 0;
    }
    return 1;
}
STUB(0x8000DD00, NextGrid)
STUB(0x8000DFEC, WorldObjCollide)
STUB(0x8000E3B8, CTriListCollide)
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
STUB(0x8000E73C, PointLineColl)

#undef STUB
