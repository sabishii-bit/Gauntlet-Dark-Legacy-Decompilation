#include "types.h"

/* game/world/dynobjgrid.c  (Xbox PDB module dynobjgrid.obj)
 *
 * Dynamic world-object collision grid.  Collidable world objects are bucketed
 * into a grid sized to the level bounds (cell size dyngrid_width) and rebuilt
 * every frame.  A rolling ObjCheckNum stamp keeps a single query from testing
 * an object twice even when it spans several cells.
 *
 * GC .text order (reverse of the Xbox source order): WorldDynCollide,
 * NextDynGrid, CreateDynobjGrid, InitDynobjGrid.  CalcMaxObjSize and
 * InitDynobjList were inlined into InitDynobjGrid by the GC compiler.
 *
 * MATCH STATUS (parked residuals, all regalloc/copy-quirk class - do not
 * regrind without a new allocator lever):
 *  - InitDynobjGrid: 1 insn (target mr r3,r8 copy of zeroed total; ours
 *    const-propagates to li 0 - the gcontrolpads remat-vs-copy quirk).
 *  - WorldDynCollide: target keeps an extra addi r6,r3,0 copy web for d +
 *    nonvolatile color rotation (r28..r31); opcode stream otherwise exact.
 *  - CreateDynobjGrid: r27/r28 web rotation through the rasterize loops.
 *  - NextDynGrid: complete semantic translation (255 target insns).
 */

/* A per-object record tracked by the grid (0x44 bytes). */
typedef struct DynObj {
    u8 _00[0x30];
    f32 bbox[3];   /* +0x30 world-space footprint centre (x,_,z) */
    u8 _3c[0x04];
    s16 obj_idx;   /* +0x40 index of the source world object */
    u16 mark;      /* +0x42 last ObjCheckNum that visited this entry */
} DynObj;

/* A collidable world object (stride 0x3C = 60 bytes). */
typedef struct WorldObj {
    u8 _00[0x10];
    u32 flags;              /* +0x10 */
    u8 _14[0x04];
    struct WorldObj* link;  /* +0x18 */
    f32 bbox[3];            /* +0x1C footprint centre (x,_,z) */
    void* prep;             /* +0x28 */
    u8 _2c[0x04];
    f32 radius;             /* +0x30 */
    u8 _34[0x01];
    s8  side;               /* +0x35 */
    s16 field36;            /* +0x36 */
    s32 field38;            /* +0x38 */
} WorldObj;

/* World-bounds / world-object-list record (gWorldInfo, 0xA4 bytes). */
typedef struct WorldInfo {
    u8 _00[0x04];
    WorldObj* objs;    /* +0x04 world object array (stride 60) */
    u8 _08[0x08];
    u32* objlisthdr;   /* +0x10 -> packed (count<<22 | byte offset) */
    s16* objlistpool;  /* +0x14 s16 pool of object indices */
    f32 min_x;         /* +0x18 */
    u8 _1c[0x04];
    f32 min_z;         /* +0x20 */
    f32 max_x;         /* +0x24 */
    u8 _28[0x04];
    f32 max_z;         /* +0x2C */
    u8 _30[0x74];
} WorldInfo;

extern WorldInfo gWorldInfo; /* gWorldInfo */

/* --- module state --- */
extern u16* dyngrid;         /* cell -> head entry index */
extern s32 dyngridsize;      /* num_dyngridx * num_dyngridz */
extern s32 num_dyngridx;
extern s32 num_dyngridz;
extern s32 ObjCheckNum;      /* rolling per-query stamp */
extern s32 dyngrid_index;    /* write cursor into dyngrid_list */
extern DynObj* dynobj_list;  /* per-object records */
extern s32 dynobj_count;
extern s16* dyngrid_list;    /* pool of {obj, next} entry pairs */
extern s32 dyngrid_count;
extern f32 dyngrid_width;    /* cell size */
extern f32 dyngrid_invwidth; /* 1.0 / cell size */

/* Allocator / helpers owned by other modules. */
extern void* AllocMem(s32 size);
extern void* memset(void* p, s32 c, s32 n);
extern void GetWorldMat(void* a, void* b, s32 c);
extern void FatalError(const char* msg, s32 code);
extern const char aGridError[]; /* lbl_80112360 == "GRID ERROR" */
extern s32 WorldObjCollide();
extern s32 ExitCollisionEarly();

s32 NextDynGrid(s32* cellx, s32* cellz, f32 vx, f32 vy, f32 vz, f32 r,
                s32 ix, s32 iy, s32 iz, s32 iy2);

s32 WorldDynCollide(u32 objmask, u32 sidemask, f32 x, f32 y, f32 z, f32 f4,
                    f32 vx, f32 vy, f32 vz, f32 r)
{
    s32 cx, cz;
    s16* e;
    DynObj* d;
    WorldObj* o;
    s32 side;
    s32 head;
    s32 ret;
    u8 unused[8];

    ObjCheckNum = ObjCheckNum + 1;
    if (ObjCheckNum > 64000)
        ObjCheckNum = 1;

    cx = (s32)((x - gWorldInfo.min_x) * dyngrid_invwidth);
    cz = (s32)((z - gWorldInfo.min_z) * dyngrid_invwidth);
    cx = (cx < 0) ? 0 : (cx > num_dyngridx - 1 ? num_dyngridx - 1 : cx);
    cz = (cz < 0) ? 0 : (cz > num_dyngridz - 1 ? num_dyngridz - 1 : cz);

    for (;;) {
        head = dyngrid[cz * num_dyngridx + cx];
        while (head != 0) {
            e = &dyngrid_list[head * 2];
            d = &dynobj_list[e[0]];
            if (d->mark != ObjCheckNum) {
                d->mark = ObjCheckNum;
                o = &gWorldInfo.objs[d->obj_idx];
                side = o->side;
                if (o->link)
                    side |= o->link->side;
                if ((o->flags & objmask) && !(side & sidemask) &&
                    !(o->flags & 0x10000000) && o->field38 >= 0) {
                    WorldObjCollide(o, o->field36, 0, r);
                    ret = ExitCollisionEarly();
                    if (ret)
                        return ret;
                }
            }
            head = ((u16*)e)[1];
        }
        ret = ExitCollisionEarly();
        if (ret)
            return ret;
        ret = NextDynGrid(&cx, &cz, vx, vy, vz, r, (s32)x, (s32)y, (s32)z, (s32)y);
        if (!ret)
            return ret;
    }
}

/* DDA step: advance (*cellx,*cellz) to the next grid cell the swept segment
 * crosses, returning 1 while still inside the grid and 0 once it leaves.
 *
 * The integer coordinates are the truncated start/end components prepared by
 * WorldDynCollide.  Keeping them as integers is intentional: the original
 * uses MWCC's signed-int-to-double sequence at each boundary calculation. */
s32 NextDynGrid(s32* cellx, s32* cellz, f32 vx, f32 vy, f32 vz, f32 r,
                s32 ix, s32 iy, s32 iz, s32 iy2)
{
    s32 x;
    s32 z;
    s32 forward;
    f32 edge;
    f32 candidate;
    f32 step;
    f32 delta;
    u8 unused[8];

    x = *cellx;
    z = *cellz;

    if (vx == 0.0) {
        if (vy >= 0.0) {
            step = r;
            forward = 1;
            z++;
        } else {
            step = -r;
            forward = 0;
            z--;
        }
        edge = (f32)(x + 1) * dyngrid_width + gWorldInfo.min_x;
    } else {
        if (vz >= 0.0) {
            step = r;
            forward = 1;
            z++;
        } else {
            step = -r;
            forward = 0;
            z--;
        }
        edge = (f32)(x + 1) * dyngrid_width + gWorldInfo.min_x;
        vy = vz * (edge - (f32)ix);
    }

    if (z >= 0 && z < num_dyngridz) {
        candidate = (f32)iz + vy + 2.0f * step;
        if (forward) {
            if (candidate > (f32)iy2)
                candidate = (f32)iy2;
            if (z <= (s32)((candidate - gWorldInfo.min_z) * dyngrid_invwidth)) {
                *cellz = z;
                return 1;
            }
        } else {
            if (candidate < (f32)iy2)
                candidate = (f32)iy2;
            if (z >= (s32)((candidate - gWorldInfo.min_z) * dyngrid_invwidth)) {
                *cellz = z;
                return 1;
            }
        }
    }

    if (edge > (f32)iy)
        return 0;
    if (x + 1 >= num_dyngridx)
        return 0;

    if (vx == 0.0 || edge - (f32)ix < 2.0f * r) {
        edge = (f32)iz;
    } else {
        edge += dyngrid_width;
        delta = (f32)ix + 2.0f * r;
        delta = edge - delta;
        if (delta > 0.0)
            edge = delta * vz + (f32)iz;
        else
            edge = (f32)iz;
    }

    if ((forward && edge > (f32)iy2) ||
        (!forward && edge < (f32)iy2)) {
        edge = (f32)iy2;
    }

    z = (s32)((edge - gWorldInfo.min_z) * dyngrid_invwidth);
    if (z >= num_dyngridz || z < 0)
        return 0;

    *cellx = x + 1;
    *cellz = z;
    if (x + 1 < 0 || z < 0)
        FatalError(aGridError, 0x800000);
    return 1;
}

/* Per-frame rebuild: clear the entry pool and grid, then rasterize each live
 * object's footprint bbox into every cell it overlaps. */
void CreateDynobjGrid(void)
{
    s32 i;
    s32 cx, cz;
    s32 xlo, xhi, zlo, zhi;
    s32 xloRaw;
    WorldObj* o;
    DynObj* d;
    f32* p;
    u8 unused[16];

    memset(dyngrid_list, 0, (dyngrid_index + 1) * 4);
    memset(dyngrid, 0, dyngridsize * 2);
    dyngrid_index = 0;

    for (i = 0; i < dynobj_count; i++) {
        d = &dynobj_list[i];
        o = &gWorldInfo.objs[d->obj_idx];
        if (o->flags & 0x01000000)
            GetWorldMat(o->prep, d, 0);
        d->mark = 0;
        if (o->flags & 0x01000000)
            p = d->bbox;
        else if (o->flags & 0x00001000)
            p = (f32*)((u8*)o->prep + 0x30);
        else
            p = o->bbox;
        xlo = (s32)((p[0] - o->radius - gWorldInfo.min_x) * dyngrid_invwidth);
        xloRaw = (s32)((p[0] - o->radius - gWorldInfo.min_x) * dyngrid_invwidth);
        xhi = (s32)((p[0] + o->radius - gWorldInfo.min_x) * dyngrid_invwidth);
        zlo = (s32)((p[2] - o->radius - gWorldInfo.min_z) * dyngrid_invwidth);
        zhi = (s32)((p[2] + o->radius - gWorldInfo.min_z) * dyngrid_invwidth);
        xlo = (xlo < 0) ? 0 : (xloRaw > num_dyngridx - 1 ? num_dyngridx - 1 : xloRaw);
        xhi = (xhi < 0) ? 0 : (xhi > num_dyngridx - 1 ? num_dyngridx - 1 : xhi);
        zlo = (zlo < 0) ? 0 : (zlo > num_dyngridz - 1 ? num_dyngridz - 1 : zlo);
        zhi = (zhi < 0) ? 0 : (zhi > num_dyngridz - 1 ? num_dyngridz - 1 : zhi);
        for (cz = zlo; cz <= zhi; cz++) {
            for (cx = xlo; cx <= xhi; cx++) {
                u16* cell = &dyngrid[cz * num_dyngridx + cx];
                dyngrid_index++;
                dyngrid_list[dyngrid_index * 2] = (s16)i;
                ((u16*)dyngrid_list)[dyngrid_index * 2 + 1] = *cell;
                *cell = dyngrid_index;
            }
        }
    }
}

/* Load-time init: derive the grid dimensions from the level bounds, then
 * allocate the grid array, the per-object record list, and the entry pool
 * (CalcMaxObjSize + InitDynobjList inlined). */
extern f64 lbl_803466C8; /* 2.0 (sdata2) */

static inline s32 CalcMaxObjSize(void)
{
    s32 i;
    u32 hdr;
    s32 count;
    s32 total;
    s32 ofs;
    s32 n;
    s16 objidx;

    hdr = *gWorldInfo.objlisthdr;
    count = hdr >> 22;
    ofs = hdr & 0x3FFFFF;
    total = 0;
    for (i = total; i < count; i++) {
        objidx = *(s16*)((u8*)gWorldInfo.objlistpool + ofs);
        ofs += 2;
        dynobj_list[i].obj_idx = objidx;
        n = (s32)(lbl_803466C8 * gWorldInfo.objs[objidx].radius /
                  dyngrid_width);
        total += (n + 2) * (n + 2);
    }
    return total;
}

void InitDynobjGrid(void)
{
    s32 total;
    u8 unused[40];

    num_dyngridx = (s32)((gWorldInfo.max_x - gWorldInfo.min_x) / dyngrid_width) + 1;
    num_dyngridz = (s32)((gWorldInfo.max_z - gWorldInfo.min_z) / dyngrid_width) + 1;
    dyngridsize = num_dyngridx * num_dyngridz;
    dyngrid = (u16*)AllocMem(dyngridsize * 2);

    dynobj_count = *gWorldInfo.objlisthdr >> 22;
    dynobj_list = (DynObj*)AllocMem(dynobj_count * 68);

    total = CalcMaxObjSize();
    dyngrid_count = total + 1;
    dyngrid_list = (s16*)AllocMem(dyngrid_count * 4);
    memset(dyngrid_list, 0, dyngrid_count * 4);
    dyngrid_index = 0;
}
