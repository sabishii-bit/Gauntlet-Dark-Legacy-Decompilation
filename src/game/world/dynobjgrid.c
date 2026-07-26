#include "types.h"

/* game/world/dynobjgrid.c  (Xbox PDB module dynobjgrid.obj)
 *
 * Dynamic world-object collision grid.  Static geometry (walls, ramps, hazards
 * and other collidable world objects) is bucketed into a grid whose cell size
 * is dyngrid_width world units.  Unlike the enemy/item grid (dyngrid.c), the
 * grid here is sized to the level bounds and heap-allocated at load time, and a
 * rolling ObjCheckNum stamp is used so a single query never tests the same
 * object twice even when it spans several cells.
 *
 * .text range: 0x80043A78 - 0x800444C0 (4 functions), wired NonMatching so the
 * tree stays green while the real Xbox-PDB symbols are mapped.  dtk substitutes
 * the original DOL bytes for this range.
 *
 * Function map (GC addr -> dynobjgrid.obj name).  The GC order below is the
 * exact reverse of the Xbox source order (CalcMaxObjSize, InitDynobjList,
 * InitDynobjGrid, CreateDynobjGrid, NextDynGrid, WorldDynCollide), which
 * confirms the module; CalcMaxObjSize and InitDynobjList were inlined into
 * InitDynobjGrid by the GC compiler:
 *   0x80043A78 WorldDynCollide  - public query: stamp ObjCheckNum, compute the
 *                                 swept cell bbox, walk it via NextDynGrid
 *   0x80043CCC NextDynGrid      - DDA walk of the cells a segment crosses;
 *                                 FatalError("GRID ERROR") on out-of-range cell
 *   0x800440C8 CreateDynobjGrid - per-frame rebuild: clear grid, rasterize each
 *                                 dynobj's footprint into the covered cells
 *   0x80044358 InitDynobjGrid   - load-time alloc: size the grid from the world
 *                                 bounds, allocate dyngrid / dynobj_list /
 *                                 dyngrid_list (InitDynobjList inlined)
 */

/* A collidable world object as tracked by the grid (real record is 0x44 bytes;
 * only the fields this module touches are modelled). */
typedef struct DynObj {
    s16 obj_idx;  /* +0x40 index of the source world object */
    s16 mark;     /* +0x42 last ObjCheckNum that visited this entry */
} DynObj;

/* --- module state (extern so this documentation TU emits only .text) --- */
extern s16* dyngrid;         /* lbl_80344704 cell -> dyngrid_list head/index */
extern s32 dyngridsize;      /* lbl_80344708 num_dyngridx * num_dyngridz */
extern s32 num_dyngridx;     /* lbl_8034470C */
extern s32 num_dyngridz;     /* lbl_80344710 */
extern s32 ObjCheckNum;      /* lbl_80344714 rolling per-query stamp */
extern s32 dyngrid_index;    /* lbl_80344700 write cursor into dyngrid_list */
extern DynObj* dynobj_list;  /* lbl_803446F0 per-object records */
extern s32 dynobj_count;     /* lbl_803446F4 */
extern s16* dyngrid_list;    /* lbl_803446F8 pool of per-cell object entries */
extern s32 dyngrid_count;    /* lbl_803446FC capacity of dyngrid_list */
extern f32 dyngrid_width;    /* lbl_80343BF0 cell size (world units) */
extern f32 dyngrid_invwidth; /* lbl_80343BF4 1.0 / cell size */

/* World-bounds / world-object-list record owned elsewhere (lbl_8028CA8C). */
typedef struct WorldInfo {
    u32 hdr;       /* +0x00 packed count (object count in high bits) */
    void* objs;    /* +0x04 world object array base */
    u8 _08[0x08];
    void* records; /* +0x10 per-object collision record array */
    u8 _14[0x04];
    f32 min_x;     /* +0x18 */
    u8 _1c[0x04];
    f32 min_z;     /* +0x20 */
    f32 max_x;     /* +0x24 */
    u8 _28[0x04];
    f32 max_z;     /* +0x2C */
    u8 _30[0x74];
} WorldInfo;

extern WorldInfo gWorldInfo; /* lbl_8028CA8C */

/* Allocator / helpers owned by other modules. */
extern void* AllocMem(s32 size);
extern void* memset(void* p, s32 c, s32 n);
extern void fn_800BB614(void* a, void* b, s32 c); /* per-object grid prep */
extern void FatalError(const char* msg, ...);
extern const char aGridError[]; /* lbl_80112360 == "GRID ERROR" */

/* Load-time initialisation: derive the grid dimensions from the level bounds,
 * then allocate the grid array, the per-object record list, and the entry pool
 * (CalcMaxObjSize + InitDynobjList were inlined here). */
void InitDynobjGrid(void)
{
    s32 i, n, total;

    num_dyngridx = (s32)((gWorldInfo.max_x - gWorldInfo.min_x) / dyngrid_width) + 1;
    num_dyngridz = (s32)((gWorldInfo.max_z - gWorldInfo.min_z) / dyngrid_width) + 1;
    dyngridsize = num_dyngridx * num_dyngridz;
    dyngrid = (s16*)AllocMem(dyngridsize * 2);

    dynobj_count = (s32)(gWorldInfo.hdr >> 22);
    dynobj_list = (DynObj*)AllocMem(dynobj_count * 68);

    /* Sum, over every object, the number of cells its footprint can touch to
     * size the shared per-cell entry pool. */
    total = 0;
    for (i = 0; i < dynobj_count; i++) {
        n = 0; /* cells covered by object i (footprint / cell area, +2 margin) */
        total += (n + 2) * (n + 2);
    }
    dyngrid_count = total + 1;
    dyngrid_list = (s16*)AllocMem(dyngrid_count * 4);
    memset(dyngrid_list, 0, dyngrid_count * 4);
    dyngrid_index = 0;
}

/* Per-frame rebuild: clear the entry pool and the grid, then rasterize each
 * live object's footprint bbox into every cell it overlaps. */
void CreateDynobjGrid(void)
{
    s32 i, cx, cz, xlo, xhi, zlo, zhi;

    memset(dyngrid_list, 0, (dyngrid_index + 1) * 4);
    memset(dyngrid, 0, dyngridsize * 2);
    dyngrid_index = 0;

    for (i = 0; i < dynobj_count; i++) {
        DynObj* d = &dynobj_list[i];
        /* fn_800BB614 refreshes the object's world-space footprint. */
        fn_800BB614(d, gWorldInfo.objs, 0);

        /* footprint bbox -> cell range, clamped to [0, num_dyngrid*-1] */
        xlo = 0;
        xhi = num_dyngridx - 1;
        zlo = 0;
        zhi = num_dyngridz - 1;

        for (cz = zlo; cz <= zhi; cz++) {
            for (cx = xlo; cx <= xhi; cx++) {
                dyngrid_index++;
                /* link this object entry into cell (cx,cz)'s chain */
                (void)cx;
                (void)cz;
            }
        }
    }
}

/* DDA walk of the cells crossed by the swept segment (x0,z0)->(x1,z1); every
 * dynobj found in a visited cell whose mark != ObjCheckNum is reported to the
 * caller and then stamped.  FatalErrors "GRID ERROR" if a computed cell index
 * leaves the grid.  Line-stepping math documented rather than transcribed. */
void NextDynGrid(f32* seg0, f32* seg1, f32 half, s32 axis)
{
    s32 cx = (s32)((seg0[0] - gWorldInfo.min_x) * dyngrid_invwidth);
    s32 cz = (s32)((seg0[2] - gWorldInfo.min_z) * dyngrid_invwidth);

    if (cx < 0 || cx >= num_dyngridx || cz < 0 || cz >= num_dyngridz)
        FatalError(aGridError);

    (void)seg1;
    (void)half;
    (void)axis;
    /* ... integer-Bresenham style advance through dyngrid[], collecting
     * dynobj_list entries whose mark != ObjCheckNum ... */
}

/* Public collision query over a moving box.  Bumps the ObjCheckNum stamp
 * (wrapping at 64000 so freed stamps recycle), computes the swept cell bbox
 * from the world bounds and cell scale, then hands the bbox to NextDynGrid. */
void WorldDynCollide(f32* pos, f32* vel, f32 hx, f32 hz)
{
    s32 stamp = ObjCheckNum + 1;
    if (stamp > 64000)
        stamp = 1;
    ObjCheckNum = stamp;

    (void)pos;
    (void)vel;
    (void)hx;
    (void)hz;
    /* min/max cell = clamp((pos +/- extent - world_min) * dyngrid_invwidth) */
    NextDynGrid(pos, pos, hx, 0);
}
