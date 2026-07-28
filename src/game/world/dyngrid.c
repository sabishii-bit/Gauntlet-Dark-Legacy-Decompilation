#include "types.h"
#include "game/dyngrid.h"

/* game/world/dyngrid.c  (Xbox PDB module DYNGRID.OBJ)
 *
 * Fixed 64x64 spatial grid used to accelerate "what enemies / items are near
 * point P" queries.  Each cell holds the head index of a per-cell singly
 * linked list (one list of enemies, one of items); objects thread themselves
 * through their cell via a link field.  Rebuilt every frame by SetupDynGrid.
 *
 * This static enemy/item grid ("enegrid" here) is distinct from the dynamic
 * world-object grid in dynobjgrid.c (the authentic "dyngrid").
 *
 * .text range: 0x800433EC - 0x80043A78 (6 functions).
 *
 * Function map (GC addr -> DYNGRID.OBJ name; GC emits in reverse Xbox source
 * order).  Despite their names, the first pair walks sItems and the second
 * walks gEnemies; this is verified from both the link fields and callers:
 *   0x800433EC NextGridEnemy   - advance sItems iterator, return index/-1
 *   0x80043490 StartEnemyGrid  - seed sItems iterator from center + radius
 *   0x800435EC NextGridItem    - advance gEnemies iterator, return index/-1
 *   0x80043694 StartItemGrid   - seed gEnemies iterator from center + radius
 *   0x800437F0 SetupDynGrid    - clear grid, relink all enemies then items
 *                                (PlaceEnemyInGrid/PlaceItemInGrid inlined)
 *   0x80043A04 InitDynGrid     - derive grid origin/scale from world bounds
 *
 * DynGridX/DynGridZ and the two Place* helpers present in the Xbox build were
 * inlined by the GC compiler and have no standalone GC address.
 */

/* --- grid geometry --- */
typedef struct DynGridCell {
    s16 item;  /* head index into the item list for this cell (-1 == empty) */
    s16 enemy; /* head index into the enemy list for this cell (-1 == empty) */
} DynGridCell;

#define DYNGRID_DIM 64

/* Objects stored in the grid.  Only the fields the grid touches are modelled;
 * real strides are 0xF0 (enemy) and 0x394 (item). */
typedef struct GridEnemy {
    void* def;    /* +0x00 owner/def ptr; *(s32*)def == -1 means retired */
    u8 _04[0x30];
    f32 x;        /* +0x34 */
    u8 _38[0x04];
    f32 z;        /* +0x3C */
    u8 _40[0x84];
    s16 alive;    /* +0xC4 (-1 == not in play) */
    u8 _c6[0x0C];
    s16 link;     /* +0xD2 next enemy in this cell */
    u8 _d4[0x1C]; /* pad to stride 0xF0 */
} GridEnemy;

typedef struct GridItem {
    u8 _00[0x34];
    f32 x;        /* +0x34 */
    u8 _38[0x04];
    f32 z;        /* +0x3C */
    u8 _40[0x74];
    s32 active;   /* +0xB4 (0 == slot unused) */
    u8 _b8[0x144];
    s16 link;     /* +0x1FC next item in this cell */
    u8 _1fe[0x196]; /* pad to stride 0x394 */
} GridItem;

/* World bounds record owned elsewhere (lbl_8028CA8C, 0xA4 bytes). */
typedef struct WorldBounds {
    u8 _00[0x18];
    f32 min_x;    /* +0x18 */
    u8 _1c[0x04];
    f32 min_z;    /* +0x20 */
    f32 max_x;    /* +0x24 */
    u8 _28[0x04];
    f32 max_z;    /* +0x2C */
    u8 _30[0x74];
} WorldBounds;

/* External object pools / counts (owned by the enemy & item managers).  Named
 * by their real symbols.txt addresses; friendly aliases below. */
extern GridEnemy* sItems;        /* shared item/enemy pool base (items.c) */
extern s32 sNumItems;         /* enemy pool count */
extern GridItem gEnemies[];      /* item pool base (0x80251C18) */
extern s32 gNumEnemies;          /* item pool count */
extern WorldBounds gWorldInfo; /* world bounds */

#define ene_pool sItems
#define ene_pool_num sNumItems
#define itm_pool gEnemies
#define itm_pool_num gNumEnemies
#define gWorldBounds gWorldInfo

/* Tuning constants that live in the module's read-only pool. */
extern const f32 lbl_803466A0;   /* 0.0f */
extern const f32 lbl_803466A4;   /* grid extent multiplier */
extern const f64 lbl_803466A8;   /* 64.0 */

#define kGridZero lbl_803466A0
#define kGridExtentMul lbl_803466A4
#define kGridDim lbl_803466A8

/* --- module state (DYNGRID.OBJ file-locals) --- */
extern DynGridCell enegrid[DYNGRID_DIM][DYNGRID_DIM]; /* lbl_8024CE00 */

extern s32 itm_x, itm_z;                 /* current item iterator cell */
extern s32 itm_min_x, itm_min_z;         /* item query bbox (cells) */
extern s32 itm_max_x, itm_max_z;
extern s32 itm_idx;                      /* current item list index */

extern s32 ene_x, ene_z;                 /* current enemy iterator cell */
extern s32 ene_min_x, ene_min_z;         /* enemy query bbox (cells) */
extern s32 ene_max_x, ene_max_z;
extern s32 ene_idx;                      /* current enemy list index */

extern f32 enegrid_ene_pad;              /* radius margin for enemy queries */
extern f32 enegrid_itm_pad;              /* radius margin for item queries */
extern f32 enegrid_z0;                   /* grid origin Z (world min Z) */
extern f32 enegrid_x0;                   /* grid origin X (world min X) */
extern f32 enegrid_invwidth;             /* cells per world unit */
extern f32 enegrid_width;                /* grid physical size */

static int clampcell(int v)
{
    if (v < 0)
        v = 0;
    if (v >= DYNGRID_DIM)
        v = DYNGRID_DIM - 1;
    return v;
}

/* Advance the historically Enemy-named iterator over sItems. */
s32 NextGridEnemy(void)
{
    s32 idx;

    while ((idx = ene_idx) < 0) {
        ene_x++;
        if (ene_x > ene_max_x) {
            ene_z++;
            if (ene_z > ene_max_z)
                return -1;
            ene_x = ene_min_x;
        }
        ene_idx = enegrid[ene_z][ene_x].enemy;
    }
    if (idx >= 0) {
        ene_idx = ene_pool[idx].link;
        return idx;
    }
    return -1;
}

/* Seed the enemy iterator for a circular query at pos (x=pos[0], z=pos[2])
 * with radius r, then load the first candidate cell. */
void StartEnemyGrid(f32 r, f32* pos)
{
    int mnx, mnz;
    f32 pad;
    u8 unused[16];

    if (r > kGridZero)
        pad = enegrid_ene_pad + r;
    else
        pad = -r;

    ene_min_x = mnx = clampcell((int)((pos[0] - pad - enegrid_x0) * enegrid_invwidth));
    ene_min_z = mnz = clampcell((int)((pos[2] - pad - enegrid_z0) * enegrid_invwidth));
    ene_max_x = clampcell((int)((pos[0] + pad - enegrid_x0) * enegrid_invwidth));
    ene_max_z = clampcell((int)((pos[2] + pad - enegrid_z0) * enegrid_invwidth));

    ene_x = mnx;
    ene_z = mnz;
    ene_idx = enegrid[mnz][mnx].enemy;
}

/* Historically Item-named counterpart; this one walks gEnemies. */
s32 NextGridItem(void)
{
    s32 idx;

    while ((idx = itm_idx) < 0) {
        itm_x++;
        if (itm_x > itm_max_x) {
            itm_z++;
            if (itm_z > itm_max_z)
                return -1;
            itm_x = itm_min_x;
        }
        itm_idx = enegrid[itm_z][itm_x].item;
    }
    if (idx >= 0) {
        itm_idx = itm_pool[idx].link;
        return idx;
    }
    return -1;
}

void StartItemGrid(f32 r, f32* pos)
{
    int mnx, mnz;
    f32 pad;
    u8 unused[16];

    if (r > kGridZero)
        pad = enegrid_itm_pad + r;
    else
        pad = -r;

    itm_min_x = mnx = clampcell((int)((pos[0] - pad - enegrid_x0) * enegrid_invwidth));
    itm_min_z = mnz = clampcell((int)((pos[2] - pad - enegrid_z0) * enegrid_invwidth));
    itm_max_x = clampcell((int)((pos[0] + pad - enegrid_x0) * enegrid_invwidth));
    itm_max_z = clampcell((int)((pos[2] + pad - enegrid_z0) * enegrid_invwidth));

    itm_x = mnx;
    itm_z = mnz;
    itm_idx = enegrid[mnz][mnx].item;
}

/* Rebuild the whole grid for this frame: wipe all cells, then push every live
 * enemy and item onto the head of its cell's list.  (PlaceEnemyInGrid and
 * PlaceItemInGrid were inlined into this function by the GC compiler.) */
void SetupDynGrid(void)
{
    s32 i, cx;
    u8 unused[32];
    register DynGridCell (*grid)[DYNGRID_DIM] = enegrid;
    DynGridCell* row;
    DynGridCell* cell;
    register s32 cz;

    cz = 0;
    while (cz < DYNGRID_DIM) {
        row = grid[cz];
        for (cx = 0; cx < DYNGRID_DIM; cx++) {
            cell = &row[cx];
            cell->item = -1;
            cell->enemy = -1;
        }
        cz++;
    }

    for (i = 0; i < ene_pool_num; i++) {
        s16 old;
        s16* slot;
        GridEnemy* e = &ene_pool[i];
        if (e->alive == -1)
            continue;
        if (*(s32*)e->def == -1)
            continue;
        cx = clampcell((int)((e->x - enegrid_x0) * enegrid_invwidth));
        cz = clampcell((int)((e->z - enegrid_z0) * enegrid_invwidth));
        slot = &grid[cz][cx].enemy;
        old = *slot;
        *slot = (s16)i;
        e->link = old;
    }

    {
    f32 itemX0 = enegrid_x0;
    f32 itemInv = enegrid_invwidth;
    f32 itemZ0 = enegrid_z0;
    for (i = 0; i < itm_pool_num; i++) {
        s16 old;
        s16* slot;
        GridItem* it = &itm_pool[i];
        if (!it->active)
            continue;
        cx = clampcell((int)((it->x - itemX0) * itemInv));
        cz = clampcell((int)((it->z - itemZ0) * itemInv));
        slot = &grid[cz][cx].item;
        old = *slot;
        *slot = (s16)i;
        it->link = old;
    }
    }
}

/* Derive grid origin and world->cell scale from the current world bounds.
 * pads are the per-object-class query margins. */
void InitDynGrid(f32 item_pad, f32 enemy_pad)
{
    f32 ext_x, ext_z, ext;

    enegrid_x0 = gWorldBounds.min_x;
    enegrid_z0 = gWorldBounds.min_z;

    ext_x = gWorldBounds.max_x - enegrid_x0;
    ext_z = gWorldBounds.max_z - enegrid_z0;
    ext = (ext_x > ext_z) ? ext_x : ext_z;

    enegrid_itm_pad = item_pad;
    enegrid_ene_pad = enemy_pad;
    enegrid_width = ext * kGridExtentMul;
    enegrid_invwidth = (f32)(kGridDim / enegrid_width);
}
