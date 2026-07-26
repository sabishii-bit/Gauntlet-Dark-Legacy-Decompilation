#ifndef GAME_MBOBJECT_H
#define GAME_MBOBJECT_H

/*
 * mbobject.h - Midway "MB" 2D/scene-graph record structs.
 *
 * The MB layer draws menu-board / HUD / billboard content through a small set
 * of fixed-pool record types: textured 2D sprites ("blits"), instanced screen
 * polygons ("poly instances" + their vertex arena), a deferred alpha-sort draw
 * queue, and the shared scene-graph node (mbnode) every drawable hangs off of.
 *
 * Layouts are the Xbox shell3D PDB structs (blitinst / polyinst / PolyVert /
 * polyheader / mbnode, in research/xbox_symbols/misc.h), each field verified
 * offset-for-offset against the GameCube DOL asm (dtk-extracted mb_blit.o /
 * mb_objects.o / mb_poly.o via tools/gdl/fnasm.py). The GC and Xbox record
 * layouts are identical; the one GC-only type is MBObjEntry (the 0x4C
 * alpha-sort queue entry, which has no Xbox PDB counterpart).
 *
 * Sources:
 *   research/xbox_symbols/misc.h:
 *     blitinst   Size=0x38 (Id=2170)   -> MBBlit
 *     polyinst   Size=0x24 (Id=3299)   -> PolyInstance
 *     PolyVert   Size=0x20 (Id=3336)   -> PolyVert
 *     polyheader Size=0x10 (Id=3339)   -> PolyHeader
 *     mbnode     Size=0x80 (Id=3249)   -> MBObject
 *     enum MBNODE_TYPE (Id near 15236) -> MBNodeType
 *   GC asm (offset verification, fnasm.py on the mb_* TUs):
 *     MBCreateBlit / MBNewObject / DrawSortObjectsSub / AddDistObject
 *     MBNewPoly / MBPolyInstUpdateTex / DoPolyInst
 *
 * NonMatching TUs that model these records inline today:
 *   src/game/mb/mb_blit.c    (MBBLIT / MBNODE)
 *   src/game/mb/mb_objects.c (MBObject / MBObjEntry)
 *   src/game/mb/mb_poly.c    (PolyVert / PolyInstance / PolyHeader / PolyContext)
 */

#include "types.h"

struct MBBlit;
struct MBObject;
struct PolyVert;
struct PolyInstance;
struct PolyHeader;

/* Xbox: enum MBNODE_TYPE. The mbnode "type" byte (+0x52) selects both the
 * scene-graph behaviour and, for leaf nodes, which member of MBNodeData is
 * live. Verified in DrawSortObjectsSub (type read via lbz+extsb, compared to
 * 2 / 12 / 14) and in the fn_800BB29C(...) node-alloc calls across the MB TUs
 * (blit=13, object=2, poly=10). */
typedef enum MBNodeType {
    MB_EMPTY_NODE        = 0,
    MB_NULL_NODE         = 1,
    MB_OBJECT_NODE       = 2,
    MB_ALPHATREE_NODE    = 3,
    MB_NOALPHASORT_NODE  = 4,
    MB_DRAW_SPECIAL_NODE = 5,
    MB_MESH_OBJECT_NODE  = 6,
    MB_SORT_OBJECTS_NODE = 7,
    MB_DIST_OBJECTS_NODE = 8,
    MB_PSYS_DRAW_NODE    = 9,
    MB_POLY_NODE         = 10,
    MB_POLY_SHADOW_NODE  = 11,
    MB_OBJECT_BLIT_NODE  = 12,
    MB_BLIT_NODE         = 13,
    MB_PSYS_NODE         = 14,
    MB_FONT_NODE         = 15
} MBNodeType;

/*
 * One 2D blit (textured screen quad). Xbox: struct blitinst, Size=0x38.
 * GC-verified in MBCreateBlit: pool stride 56 (0x38), flags@0, tex@0x04=-1,
 * argb[4]@0x1C, prev@0x2C, next@0x30, node@0x34; the owning node's list head
 * lives in mbnode.data.blit (+0x70). flags bit1 (0x2) = live/removed marker
 * (tested via rlwinm.,30,30), bit6 (0x40) = hidden-from-window-reproject.
 */
typedef struct MBBlit {
    /* 0x00 */ u32 flags;
    /* 0x04 */ s32 tex_idx;          /* texture handle, init -1 */
    /* 0x08 */ s16 x;                /* screen x */
    /* 0x0A */ s16 y;                /* screen y */
    /* 0x0C */ u32 z;                /* depth / scale (fp -> u32) */
    /* 0x10 */ u16 width;
    /* 0x12 */ u16 height;
    /* 0x14 */ u16 s_left;           /* texcoord rect */
    /* 0x16 */ u16 s_right;
    /* 0x18 */ u16 t_top;
    /* 0x1A */ u16 t_bot;
    /* 0x1C */ u32 argb[4];          /* four corner colors, init 0x80808080 */
    /* 0x2C */ struct MBBlit* prev;
    /* 0x30 */ struct MBBlit* next;
    /* 0x34 */ struct MBObject* node;/* owning scene-graph (BLIT_NODE) */
} MBBlit;                            /* 0x38 */

/*
 * One screen-poly vertex. Xbox: struct PolyVert, Size=0x20.
 * GC-verified in MBNewPoly / MBPolyInstUpdateTex: arena stride 32 (0x20),
 * s@0x18 / t@0x1A written via sth, used@0x1C written via sth. pos[0..2] are
 * model-space x/y/z; pos[3] is the projection clip scratch (mb_poly's sClip).
 */
typedef struct PolyVert {
    /* 0x00 */ f32 pos[4];           /* model-space xyz + clip-w scratch */
    /* 0x10 */ u16 x;                /* projected screen x (12.4) */
    /* 0x12 */ u16 y;                /* projected screen y (12.4) */
    /* 0x14 */ u32 z;                /* projected screen z / clip flag */
    /* 0x18 */ u16 s;                /* texcoord s (fixed point) */
    /* 0x1A */ u16 t;                /* texcoord t (fixed point) */
    /* 0x1C */ u16 used;             /* 1 = active vertex */
    /* 0x1E */ u16 dummy;
} PolyVert;                          /* 0x20 */

/*
 * One poly instance (tri/quad). Xbox: struct polyinst, Size=0x24.
 * GC-verified in MBNewPoly / DoPolyInst: nverts@0 read via lha (signed),
 * hide@0x02 read via lhz (UNSIGNED - so u16, matching prior matching-work
 * notes), tex@0x04, argb@0x08, zmod@0x0C / dzmod@0x0E stored via sth,
 * vertidx@0x10, verts@0x14, prev@0x18, next@0x1C, header@0x20.
 * (mb_poly.c's reconstruction labelled 0x0C/0x0E "texW"/"span"; the
 *  authoritative PDB names are zmod/dzmod, and DoPolyInst uses dzmod as the
 *  running tex/z-offset advance.)
 */
typedef struct PolyInstance {
    /* 0x00 */ s16 nverts;           /* vertex count: 3=tri, 4=quad */
    /* 0x02 */ u16 hide;             /* nonzero = hidden */
    /* 0x04 */ s32 tex_idx;          /* texture id, -1 = none */
    /* 0x08 */ u32 argb;             /* packed color */
    /* 0x0C */ s16 zmod;             /* z-sort modifier */
    /* 0x0E */ s16 dzmod;            /* delta-z / draw advance */
    /* 0x10 */ s32 vertidx;          /* first vertex index in the arena */
    /* 0x14 */ struct PolyVert* verts;
    /* 0x18 */ struct PolyInstance* prev;
    /* 0x1C */ struct PolyInstance* next;
    /* 0x20 */ struct PolyHeader* header;
} PolyInstance;                      /* 0x24 */

/*
 * Poly-instance list header + its vertex arena. Xbox: struct polyheader,
 * Size=0x10. GC-verified in MBCreatePolyHeader / MBNewPoly: polys@0 (list
 * head), vertlist@0x04 (base into the shared vertex pool), maxverts@0x08,
 * numverts@0x0C.
 */
typedef struct PolyHeader {
    /* 0x00 */ struct PolyInstance* polys;
    /* 0x04 */ struct PolyVert* vertlist;
    /* 0x08 */ s32 maxverts;
    /* 0x0C */ s32 numverts;
} PolyHeader;                        /* 0x10 */

/*
 * Leaf-node payload union at mbnode+0x70. Xbox: mbnode.data is a union of
 * { polyheader* poly; blitinst* blit; PSYS* psys; }. On GC an OBJECT_NODE
 * stores its resolved rom-object/texture pointer in the same slot (mb_objects
 * derefs it at +4 / +8), so a generic member is included for that case.
 */
typedef union MBNodeData {
    struct PolyHeader* poly;         /* POLY_NODE / POLY_SHADOW_NODE */
    struct MBBlit*     blit;         /* BLIT_NODE / OBJECT_BLIT_NODE list head */
    void*              psys;         /* PSYS_NODE */
    void*              romobj;       /* OBJECT_NODE resolved rom object/texture */
} MBNodeData;

/*
 * The shared MB scene-graph node. Xbox: struct mbnode, Size=0x80.
 * Every drawable (object, blit list, poly list, particle system) is an mbnode;
 * mb_objects.c calls it "MBObject", mb_blit.c "MBNODE", mb_poly.c "PolyContext"
 * - they are the same 0x80 record viewed through different leaf payloads.
 * GC-verified in MBNewObject / DrawSortObjectsSub / AddDistObject:
 *   type@0x52 (signed byte: MBNodeType), zsort_add@0x54, flags@0x60,
 *   index@0x6C, data@0x70.
 */
typedef struct MBObject {
    /* 0x00 */ f32 mat[4][4];        /* local transform */
    /* 0x40 */ f32 scale[4];
    /* 0x50 */ u16 id;
    /* 0x52 */ s8  type;             /* MBNodeType */
    /* 0x53 */ u8  alpha;
    /* 0x54 */ f32 zsort_add;        /* per-node sort-key bias */
    /* 0x58 */ u32 texaltidx;
    /* 0x5C */ s16 texchangeidx;
    /* 0x5E */ u8  tex_shift_idx;
    /* 0x5F */ u8  extra_byte;
    /* 0x60 */ u32 flags;            /* draw / category flag bits */
    /* 0x64 */ u32 color;
    /* 0x68 */ s16 zmod;
    /* 0x6A */ s16 ambient_add;
    /* 0x6C */ u32 index;            /* rom object index / node index */
    /* 0x70 */ MBNodeData data;      /* leaf payload (poly/blit/psys/romobj) */
    /* 0x74 */ struct MBObject* parent;
    /* 0x78 */ struct MBObject* child;
    /* 0x7C */ struct MBObject* next;
} MBObject;                          /* 0x80 */

/*
 * One deferred alpha-sort draw-queue entry. GC-only (no Xbox PDB struct):
 * the psys/dist/sort queues in mb_objects.c are fixed arrays of this record.
 * GC-verified in AddDistObject: array stride 76 (0x4C); key@0x40 (float),
 * obj@0x44, page@0x48; fn_800BE8F4(mtx, e) fills the 0x40-byte transform.
 */
typedef struct MBObjEntry {
    /* 0x00 */ f32 mtx[16];          /* 4x4 transform */
    /* 0x40 */ f32 key;              /* sort key (camera distance / explicit z) */
    /* 0x44 */ struct MBObject* obj;
    /* 0x48 */ u32 page;             /* blit / texture page */
} MBObjEntry;                        /* 0x4C */

#endif /* GAME_MBOBJECT_H */
