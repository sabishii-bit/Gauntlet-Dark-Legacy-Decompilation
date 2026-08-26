#include "types.h"
#include "game/worldinfo.h"
#include "game/worldobj.h"

/* Gauntlet world-object/scene module (Xbox WORLD.OBJ), region
 * 0x800A87C8 - 0x800AB8E0.  Wired NonMatching: the DOL bytes are substituted
 * for the linked image, but this TU is compiled for objdiff, so it just needs
 * to build cleanly.  It manages the loaded "worlds/<name>" scene: a tree of
 * WorldObj nodes (0x3C stride), their g3d display nodes, particle systems
 * ("PSYS"-tagged objects), keyframe animation, and the save/restore of each
 * object's initial transform.
 *
 * === HEADERS ADOPTED / TYPEDEF RECONCILED ==========================
 * This TU used to carry its own local `struct WorldInfo` based at 0x8028C9A8,
 * i.e. 228 (0xE4) bytes BEFORE the real struct, with synthetic field names
 * (f64/f196/f232/...).  It also carried a bogus `AtreeCB` at 0x8028CA8C.  Both
 * are gone; the canonical headers include/game/worldinfo.h (WorldInfo 0xA4 @
 * gWorldInfo 0x8028CA8C) and include/game/worldobj.h (WorldObj 0x3C) are now
 * adopted.  The 0xE4 blob preceding gWorldInfo decomposes cleanly as
 *
 *   0x8028C9A8  char       gWorldName[64]   world directory name (strcpy target)
 *   0x8028C9E8  WorldInfo  gWorldInfo2      secondary / overlay world  (0xA4)
 *   0x8028CA8C  WorldInfo  gWorldInfo       primary world              (0xA4)
 *
 * so 64 + 0xA4 == 0xE4, and the two InitWorldInfo(base+64) / (base+228) calls
 * in WorldSaveInitState land exactly on gWorldInfo2 and gWorldInfo.  The old
 * "AtreeCB" at 0x8028CA8C was simply gWorldInfo viewed with wrong names:
 *   AtreeCB.finfo(+128) == gWorldInfo.atreelist (0x80)
 *   AtreeCB.buf  (+132) == gWorldInfo.model     (0x84)
 * Every f<N> access maps to gWorldInfo.<field> at offset N-228 (e.g. f232 ->
 * wobjs@0x04, f324 -> nwobjs@0x60, f360 -> model@0x84, f368 -> worldanims@0x8C,
 * f372 -> nworldanims@0x90, f380 -> animdata@0x98).
 *
 * === FUNCTION NAMES (Xbox shell3D.pdb, module WORLD.OBJ) =============
 * Real names recovered via tools/gdl/xbmod.py --module WORLD.OBJ:
 *   DoWorldAnimSub      advance one object's keyframe animation      (0x800A87C8)
 *   FindWorldAnimNode   nearest animated-object node to a point      (0x800A8CD8)
 *   ResetWorlds         reset both worlds' state (a.k.a. InitWorlds) (0x800A8E30)
 *   NewWorld            create the two scene-root display nodes      (0x800A8E6C)
 *   WorldSaveInitState  alloc + snapshot each object's init xform    (0x800A8ECC)
 *   WorldRestoreInitState  restore that snapshot                     (0x800A9088)
 *   ToggleWorldDisplay  show/hide scene roots per WorldDisplay       (0x800A91B0)
 *   FinishLoadWorldAnim / StartLoadWorldAnim  atree file read        (0x800A92xx)
 *   WorldLoadModelStart / WorldLoadModelDone  model-load wrappers    (0x800A93xx)
 *   StartWorldLoad      multi-file "worlds"/"anim" load machine      (0x800A93C4)
 *   BGLoadWorldFile     streamed-read progress callback              (0x800A95AC)
 *   LoadWorldDone       finish load: alloc, copy name, report mem    (0x800A95CC)
 *   sSetupWorldHeader   big-endian fixup of loaded world header      (0x800A971C)
 *   FindWORLDOBJ / FindWorldObject  by-name subtree search           (0x800A9Cxx)
 *   InitWorldInfo       build a world-data block ("World Data")      (0x800A9E1C)
 *   WorldObjGetAllFlags OR the flag word down a parent list          (0x800AB47C)
 *   CreateWorldNode     recursively build g3d nodes for the tree     (0x800AB4A0)
 *   WorldPsysDeActivate / WorldPsysActivate  particle systems        (0x800AB5xx)
 *   NewWorldObject      create one object's g3d node                 (0x800AB780)
 *
 * sSetupWorldHeader is 1 byte (an empty stub) in the little-endian Xbox build
 * and only becomes the 0x534-byte byte-swap here because the GameCube is
 * big-endian.  GetWorldPsysIdx (Xbox local) is inlined into WorldPsysActivate.
 *
 * Most bodies are transcribed; the TU stays NonMatching (DOL bytes
 * substituted).  InitWorldInfo (0x1660 bytes) is fully reconstructed: opcode
 * streams are identical (1432/1432 insns) and only a single fcmpu operand-order
 * canonicalization (the gridsize != 0.0 test) plus semantic-name reloc
 * differences remain. */

#define WORLD_BSWAP16(v) \
    ((u16)((((u16)(v) & 0xFF) << 8) | (((u16)(v) >> 8) & 0xFF)))
#define WORLD_BSWAP32(v)                                                 \
    ((u32)((((u32)(v) & 0xFFu) << 24) | (((u32)(v) & 0xFF00u) << 8) |    \
           (((u32)(v) >> 8) & 0xFF00u) | (((u32)(v) >> 24) & 0xFFu)))

/* --- structs forward-declared in worldinfo.h, completed here as needed --- */

/* worldanim (Xbox 0x10): one active keyframe-animation track. */
struct worldanim {
    /* 0x00 */ s16   objidx;   /* index into gWorldInfo.wobjs                */
    /* 0x02 */ s16   nframes;  /* frame count                                */
    /* 0x04 */ u16   fixed;    /* 0x8000 = data offset already relocated     */
    /* 0x06 */ s16   state;    /* run-time state/direction flag bits         */
    /* 0x08 */ f32   curframe; /* current frame position (advanced by 30*dt) */
    /* 0x0C */ void* data;     /* keyframe stream (little-endian in file)    */
};

/* coltri (Xbox 0x28): a world collision triangle; +0x08 holds a reference
 * point used as the spawn position of a triangle-anchored particle system. */
struct coltri {
    /* 0x00 */ u8  _pad0[8];
    /* 0x08 */ f32 pos[3];
    /* 0x14 */ u8  _pad14[0x28 - 0x14];
};

/* WORLDPSYS (Xbox 0x138): a particle-system template; +0x06 is the id char
 * matched against the digit following "PSYS" in the object name. */
struct WORLDPSYS {
    /* 0x00 */ u8 _pad0[6];
    /* 0x06 */ s8 id;
    /* 0x07 */ u8 _pad7[0x138 - 7];
};

/* g3d display node (created by MBNewNode): translation @0x30, scale @0x40,
 * flags @0x60.  WorldObj.nodeptr points at one of these. */
typedef struct G3DNode {
    /* 0x00 */ u8  _pad0[48];
    /* 0x30 */ f32 x, y, z;      /* local translation                       */
    /* 0x3C */ u8  _pad3C[4];
    /* 0x40 */ f32 sx, sy, sz;   /* scale                                   */
    /* 0x4C */ u8  _pad4C[0x60 - 0x4C];
    /* 0x60 */ s32 dflags;       /* display flags                           */
} G3DNode;

/* streamed file-read handle: +16 status word. */
typedef struct FInfo {
    u8  _pad0[16];
    s32 status; /* +16 */
} FInfo;

/* --- the two separated globals preceding gWorldInfo --- */
extern char      lbl_8028C9A8[64]; /* current world directory name */
#define gWorldName lbl_8028C9A8
extern WorldInfo gWorldInfo2;     /* 0x8028C9E8 secondary / overlay world     */
/* gWorldInfo (0x8028CA8C) comes from game/worldinfo.h */

/* --- module data (names authoritative in symbols.txt) --- */
extern s32    world_load_state; /* 0x80344D68 load state machine (0..5)       */
extern FInfo* atree_finfo;      /* 0x80344D6C anim-tree file read handle       */
extern s32    WorldDisplay;     /* 0x80344D70 display mode                      */
extern s32*   lbl_80344D74;     /* saved parent (+0x18) fields                 */
extern float* lbl_80344D78;     /* saved x/y/z                                 */
extern FInfo* lbl_80344D7C;     /* world file read handle                      */
extern s32    lbl_80344D80;
extern s32    lbl_80344D84;
extern s32    lbl_80344D88;
extern void*  lbl_80344D8C;     /* default parent node                         */
extern void*  world_root1;     /* scene root 2                                */
extern void*  world_root0;     /* scene root 1                                */
extern WorldObj* lbl_80344D98;  /* secondary world object array base           */
extern WorldObj* world_objects;  /* current world object array base             */
extern void*  lbl_80344DA0;     /* secondary loaded world block                */
extern void*  lbl_80344DA4;     /* primary loaded world block                  */
extern void*  gSceneRoot;     /* default parent node                         */
extern s32    lbl_803447DC;     /* global animation pause/step flag            */
extern float  gClockFrameStep;  /* per-frame delta time (seconds)              */

extern s32    mlmMemUsed;
extern u8     gIdentityMatrix[];   /* node template / name data (.data)           */
extern char   lbl_801151D8[];   /* world save-state rodata blob                */
extern char   lbl_80115214[];   /* printf fmt "----- WORLD %s, MEM: %d -> "    */
extern char   lbl_80115230[];   /* printf fmt "%d  [WORLD=%dK]\n"              */
extern char   lbl_80115244[];   /* printf fmt "---- ALLOC World Data [%dK]\n"  */
extern char   lbl_80115264[];   /* "World psys: bad name: %s"                  */
extern char   lbl_80115280[];   /* "Unable to find world psys '%c'"           */
extern char   lbl_801152A0[];   /* "World obj with dynamic parent"            */

/* --- external API --- */
extern void  MBTreeSetFlags();          /* set node display flag / show           */
extern void  MBTreeClearFlags();          /* clear node display flag / hide         */
extern void  MBRemovePsys(void*);     /* free particle system                   */
extern void  MBOX_BGLoadModelDone(void);      /* model-load start                       */
extern void  MBOX_BGLoadModelStart(s32, s32);  /* model-load finish                      */
extern void  fn_8001267C(void*, s32, s32); /* close/abort file read            */
extern void  MBOX_SetObject(void*, void*);
extern G3DNode* MBNewNode(void*, void*, s32); /* create child display node    */
extern void  GetWorldMat(void*, f32*, s32);     /* fetch node world state       */
extern void  MBTreeSetZsortAdd(void*, s32, s32);
extern void  MBTreeSetAltTex(void*, s32, s32, s32);
extern void  MBNewWorldPsys(s32, void*, void*, s32, void*, void*); /* spawn psys   */
extern void  CopyMat4(void*, void*);
extern void  ZeroAnimData(void*);
extern s32   CalcAnimData(void*, f32*, void*, s32, s32, s32, f32); /* sample anim */
extern void  CreatePYRMatrix(void*, f32*);  /* apply anim rotation (variant A)      */
extern void  CreateRYPMatrix(void*, f32*);  /* apply anim rotation (variant B)      */
extern void  WorldObjectExplode(WorldObj*, f32*);
extern void  ErrorPrintf(const char*, ...);
extern void  bulletproof_printf(const char*, ...);
extern int   strcmp(const char*, const char*);
extern char* strstr(const char*, const char*);
extern char* strcpy(char*, const char*);
extern s32   FileSize(void*, const char*);
extern s32   FileExists(const char*, const char*);
extern FInfo* StartFileRead(void*, const char*, s32, s32, void*, void*);
extern void* MBOX_FindTexture_Sub(void*, s32, s32, s32, s32);
extern s32   MBOX_AllocModel(const char*);
extern s32   BytesFree(void);
extern void* AllocMem(s32);

/* InitWorldInfo (0x800A9E1C): allocate + initialise one WorldInfo's runtime
 * data from a just-loaded world block, and return the wobjs array base. */
WorldObj* InitWorldInfo(WorldInfo* wi, void* data);
extern struct animheader* SetupAnimHeader(void* block, void* arg);
extern void InitAnimData(void* adata, void* stream);
extern void fn_80011DCC(void* psys);
extern void InitDynobjGrid(void);

/* forward declarations (same TU) */
static void sSetupWorldHeader(u32* w);
void  BGLoadWorldFile(s32* h);
void* NewWorldObject(WorldObj* obj, WorldObj* parent);
s32   WorldPsysActivate(WorldObj* obj);
void  CreateWorldNode(WorldObj* base, WorldObj* obj, void* parent);
void  ToggleWorldDisplay(void);
WorldObj* FindWorldObject(WorldObj* node, char* name);

static const float lbl_80348778 = 0.0f;
static const double lbl_80348768 = 0.0;  /* invgridsize fallback   */
static const double lbl_80348788 = 0.5;  /* Newton half / center weight */
static const double lbl_80348790 = 3.0;  /* Newton three            */
static const double lbl_803487A8 = 1.0;  /* invgridsize numerator  */
extern f64 __frsqrte(f64 x);

/* byte-swap helpers: written through memory so each takes its value's
 * address (matches the original's stack-slot swap sequences).  Defined
 * before DoWorldAnimSub so they inline into its stream-header fixup. */
static inline u16 sSwapU16(u16 v) {
    u8* b = (u8*)&v;
    return (u16)(b[0] | (b[1] << 8));
}

static inline u32 sSwapU32(u32 v) {
    u32 r;
    u8* s = (u8*)&v;
    u8* d = (u8*)&r;
    d[0] = s[3];
    d[1] = s[2];
    d[2] = s[1];
    d[3] = s[0];
    return r;
}

static inline f32 sSwapF32(f32 v) {
    f32 r;
    *(u32*)&r = sSwapU32(*(u32*)&v);
    return r;
}

static inline int sWorldFloatEqual(f32 a, f32 b) {
    return a == b;
}

/* DoWorldAnimSub: advance one object's keyframe animation by one frame.
 * `wa` is the worldanim track; `panim` points at the object's animdata pointer
 * (panim[0] is the little-endian keyframe stream). `animBase` is the shared
 * animation-data block used by the stream's byte-swapped +4 offset. Returns
 * the swapped mode word from the stream header, or 0 when nothing animated. */
s32 DoWorldAnimSub(struct worldanim* wa, void** panim, u8* animBase) {
    s32 mode;
    WorldObj* obj;
    G3DNode* node;
    void* data;
    u8* d;
    u8* sequence;
    u32 f;
    s32 m1;
    s32 hdr2;
    f32 curframe;
    f32 xf[11]; /* sampled transform (pos @16, scale @32) from CalcAnimData */

    obj = &((WorldObj*)gWorldInfo.wobjs)[wa->objidx];
    data = panim[0];

    if (data == NULL) {
        return 0;
    }
    if ((node = (G3DNode*)obj->nodeptr) == NULL) {
        return 0;
    }

    /* Derive the per-track direction/mode bits from the object flags. */
    f = obj->flags;
    m1 = f & 0x00100000;
    if (m1 && (f & 0x00200000)) {
        wa->state &= ~1;               /* both set -> disable this frame */
    } else if (m1) {
        wa->state |= 1;
        wa->state &= ~0x300;
        wa->state |= 2;
    } else if (f & 0x00200000) {
        wa->state |= 1;
        wa->state &= ~0x300;
        wa->state &= ~2;
    } else {
        wa->state |= 1;
        wa->state &= ~2;
        wa->state |= 0x100;
    }

    if (!(wa->state & 1)) {
        obj->flags &= ~0x08000000;
        return 0;
    }

    /* Read the stream header (stored little-endian).  Byte-swaps go through
     * memory (the sSwap* helpers), matching the original's stack-slot swaps. */
    d = (u8*)data;
    sequence = animBase + sSwapU32(*(u32*)(d + 4));
    mode = sSwapU16(*(u16*)(d + 0));
    hdr2 = sSwapU16(*(u16*)(d + 2));

    if ((mode & 0xFFF) == 0) {
        /* No keyframes: reset to the template pose. */
        CopyMat4(gIdentityMatrix, node);
        ZeroAnimData(panim);
    } else {
        curframe = wa->curframe;
        if (CalcAnimData(panim, xf, sequence, mode, hdr2, wa->nframes, curframe) != 0) {
            /* Rotation. */
            if (mode & 7) {
                if (mode & 0x8000) {
                    CreatePYRMatrix(node, xf);
                } else {
                    CreateRYPMatrix(node, xf);
                }
            }
            /* Translation = object origin + sampled offset. */
            if (mode & 0x70) {
                node->x = obj->pos[0] + xf[4];
                node->y = obj->pos[1] + xf[5];
                node->z = obj->pos[2] + xf[6];
            }
            /* Scale. */
            if (mode & 0x700) {
                node->dflags |= 8;
                node->sx = xf[8];
                node->sy = xf[9];
                node->sz = xf[10];
            }
        }
    }

    /* Time step, unless globally paused or dt has stalled. */
    if ((lbl_803447DC != 0 && (wa->state & 0x100)) || gClockFrameStep <= 0.0) {
        return mode;
    }

    obj->flags &= ~0x00C00000;
    obj->flags |= 0x08000000;

    if (wa->state & 2) {
        /* Reverse. */
        wa->curframe = wa->curframe - 30.0 * gClockFrameStep;
        if (wa->curframe < 0.0) {
            if (wa->state & 0x200) {          /* ping-pong */
                wa->curframe = 0.0f;
                wa->state ^= 2;
            } else if (wa->state & 0x100) {   /* loop */
                wa->curframe = (f32)(wa->nframes - 1);
                if ((obj->flags & 0x100F0000) == 0x00050000) {
                    f32 m[16];
                    GetWorldMat(obj->nodeptr, m, 0);
                    WorldObjectExplode(obj, m + 12);
                }
            } else {                          /* one-shot: stop at start */
                wa->curframe = 0.0f;
                obj->flags &= ~0x08000000;
            }
            obj->flags |= 0x00400000;
        } else if ((s16)wa->curframe >= wa->nframes - 2) {
            if (obj->flags & 0x10000000) {
                obj->flags &= ~0x10000000;
                MBTreeClearFlags(obj->nodeptr, 2, 0);
            }
        }
    } else {
        /* Forward. */
        wa->curframe = wa->curframe + 30.0 * gClockFrameStep;
        if ((s16)wa->curframe >= wa->nframes - 1) {
            if (wa->state & 0x200) {          /* ping-pong */
                wa->curframe = (f32)(wa->nframes - 1);
                wa->state ^= 2;
            } else if (wa->state & 0x100) {   /* loop */
                wa->curframe = 0.0f;
                if ((obj->flags & 0x100F0000) == 0x00050000) {
                    f32 m[16];
                    GetWorldMat(obj->nodeptr, m, 0);
                    WorldObjectExplode(obj, m + 12);
                }
            } else {                          /* one-shot: clamp at end */
                wa->curframe = (f32)(wa->nframes - 1);
                obj->flags &= ~0x08000000;
            }
            obj->flags |= 0x00800000;
        } else if ((s16)wa->curframe <= 1) {
            if (obj->flags & 0x10000000) {
                obj->flags &= ~0x10000000;
                MBTreeClearFlags(obj->nodeptr, 2, 0);
            }
        }
    }
    return mode;
}

/* FindWorldAnimNode: return the display node of the animated world object
 * nearest to `point`, within `maxdist`.  (The original computes the true
 * distance via frsqrte+Newton; the squared-distance comparison used here
 * selects the identical object.) */
struct mbnode* FindWorldAnimNode(f32* point, f32 maxdist) {
    char* base = gWorldName;
    WorldObj* obj;
    WorldObj* best = NULL;
    f32 bestd = maxdist;
    s32 i;

    for (i = 0; i < *(s32*)(base + 372); i++) {
        u8* wa = *(u8**)(base + 368) + i * 16;
        f32 m[16]; /* node world-state; translation at +0x30 (index 12..14) */
        u8 _pad[16];
        f32 dx, dy, dz;
        volatile f32 tmp;
        f32 d2;

        if (*(u32*)(wa + 12) == 0) {
            continue;
        }
        obj = (WorldObj*)(*(u8**)(base + 232) + *(s16*)wa * 60);
        GetWorldMat(obj->nodeptr, m, 0);
        dy = m[12] - point[0];
        dx = m[13] - point[1];
        dz = m[14] - point[2];
        d2 = dz * dz + (d2 = dy * dy + dx * dx);
        if (d2 > lbl_80348778) {
            f64 y = __frsqrte(d2);
            y = lbl_80348788 * y * (lbl_80348790 - y * y * d2);
            y = lbl_80348788 * y * (lbl_80348790 - y * y * d2);
            y = lbl_80348788 * y * (lbl_80348790 - y * y * d2);
            tmp = (f32)(d2 * (lbl_80348788 * y * (lbl_80348790 - y * y * d2)));
            d2 = tmp;
        }
        if (d2 < bestd) {
            best = obj;
            bestd = d2;
        }
    }
    if (best != NULL) {
        return (struct mbnode*)best->nodeptr;
    }
    return NULL;
}

/* ResetWorlds: clear both worlds' runtime state (Xbox PDB name; InitWorlds is
 * the sibling candidate for this slot - both zero the same fields). */
void ResetWorlds(void) {
    register s32 zero;
    register s32* world_blob;

    zero = 0;
    world_blob = (s32*)gWorldName;
    world_root0 = (void*)zero;
    world_root1 = (void*)zero;
    lbl_80344DA4 = (void*)zero;
    lbl_80344DA0 = (void*)zero;
    world_blob[0x168 / 4] = -1;
    world_blob[0xC4 / 4] = -1;
    world_blob[0xE4 / 4] = zero;
    world_blob[0x40 / 4] = zero;
    lbl_80344D74 = (s32*)zero;
    lbl_80344D78 = (float*)zero;
}

/* NewWorld: create the two scene-root display nodes. */
void NewWorld(void* parent) {
    if (parent == 0) {
        parent = gSceneRoot;
    }
    world_root0 = MBNewNode(parent, gIdentityMatrix, 1);
    world_root1 = MBNewNode(parent, gIdentityMatrix, 1);
}

/* WorldSaveInitState: init each world, snapshot every object's parent link and
 * position for later restore, and build the display-node trees. */
void WorldSaveInitState(void) {
    s32 i;
    s32 memBase;
    char* base;

    WorldDisplay = 0;
    base = gWorldName;
    if (lbl_80344DA4 != 0) {
        u8** wobjsp;
        world_objects = InitWorldInfo((WorldInfo*)(base + 228), lbl_80344DA4);
        memBase = mlmMemUsed;
        lbl_80344D74 = AllocMem(*(s32*)lbl_80344DA4 * 4);
        lbl_80344D78 = AllocMem(*(s32*)lbl_80344DA4 * 12);
        wobjsp = (u8**)(base + 232);
        for (i = 0; i < *(s32*)(base + 324); i++) {
            ((s32*)lbl_80344D74)[i] = *(s32*)(*wobjsp + i * 60 + 24);
            lbl_80344D78[i * 3] = *(f32*)(*wobjsp + i * 60 + 28);
            lbl_80344D78[i * 3 + 1] = *(f32*)(*wobjsp + i * 60 + 32);
            lbl_80344D78[i * 3 + 2] = *(f32*)(*wobjsp + i * 60 + 36);
        }
        bulletproof_printf(lbl_801151D8, (mlmMemUsed - memBase) >> 10);
        lbl_80344D8C = world_root0;
        CreateWorldNode(world_objects, world_objects, 0);
        MBTreeSetFlags(world_root0, 0x1000, 1);
        WorldDisplay = 1;
    } else {
        world_objects = 0;
    }

    if (lbl_80344DA0 != 0) {
        lbl_80344D98 = InitWorldInfo((WorldInfo*)(base + 64), lbl_80344DA0);
        lbl_80344D8C = world_root1;
        CreateWorldNode(lbl_80344D98, lbl_80344D98, 0);
        MBTreeSetAltTex(world_root1, -2, *(s32*)(base + 364), 1);
        WorldDisplay = 2;
    } else {
        lbl_80344D98 = 0;
    }

    WorldDisplay = WorldDisplay - 1;
    ToggleWorldDisplay();
}

/* WorldRestoreInitState: restore each object's snapshotted parent + position
 * and re-arm each active animation track. */
void WorldRestoreInitState(void) {
    char* base = lbl_8028C9A8;

    if ((lbl_80344D74 == 0) || (lbl_80344D78 == 0)) {
        return;
    }
    {
        u8** wobjsp = (u8**)(base + 232);
        s32 i;
        for (i = 0; i < *(s32*)(base + 324); i++) {
            *(u32*)(*wobjsp + i * 60 + 16) &= 0xC31FFFFF;
            *(u32*)(*wobjsp + i * 60 + 24) = ((u32*)lbl_80344D74)[i];
            *(f32*)(*wobjsp + i * 60 + 28) = lbl_80344D78[i * 3];
            *(f32*)(*wobjsp + i * 60 + 32) = lbl_80344D78[i * 3 + 1];
            *(f32*)(*wobjsp + i * 60 + 36) = lbl_80344D78[i * 3 + 2];
        }
        for (i = 0; i < *(s32*)(base + 372); i++) {
            if (*(u32*)(*(u8**)(base + 380) + i * 160) != 0) {
                *(f32*)(*(u8**)(base + 368) + i * 16 + 8) = lbl_80348778;
            }
        }
    }
}

/* ToggleWorldDisplay: advance the display mode and show/hide the two scene
 * roots according to its low two bits. */
void ToggleWorldDisplay(void) {
    if (lbl_80344DA0 != 0) {
        switch (WorldDisplay) {
        default: WorldDisplay = 1; break;
        case 1:  WorldDisplay = 2; break;
        case 2:  WorldDisplay = 3; break;
        case 3:  WorldDisplay = 1; break;
        }
    } else {
        WorldDisplay = 1;
    }
    if (world_root0 != 0) {
        if (WorldDisplay & 1) {
            MBTreeClearFlags(world_root0, 2, 0);
        } else {
            MBTreeSetFlags(world_root0, 2, 0);
        }
    }
    if (world_root1 != 0) {
        if (WorldDisplay & 2) {
            MBTreeClearFlags(world_root1, 2, 0);
        } else {
            MBTreeSetFlags(world_root1, 2, 0);
        }
    }
}

/* FinishLoadWorldAnim: poll/close the anim-tree read. */
s32 FinishLoadWorldAnim(void) {
    if (atree_finfo->status != 0) {
        atree_finfo->status = -1;
        fn_8001267C(gWorldInfo.atreelist, gWorldInfo.model, -1);
        return 1;
    }
    return 0;
}

/* StartLoadWorldAnim: begin reading the "anim" file into the atree buffer. */
s32 StartLoadWorldAnim(void* dir) {
    if (gWorldInfo.atreelist != 0) {
        s32 size = FileSize(dir, "anim");
        atree_finfo = StartFileRead(dir, "anim", 0, size,
                                    gWorldInfo.atreelist, BGLoadWorldFile);
        return 1;
    }
    return 0;
}

/* WorldLoadModelStart: kick off the world model load. */
void WorldLoadModelStart(void) {
    MBOX_BGLoadModelDone();
}

/* WorldLoadModelDone: finish the world model load using the loaded handle. */
void WorldLoadModelDone(s32 a) {
    MBOX_BGLoadModelStart(a, gWorldInfo.model);
}

/* StartWorldLoad: multi-file "worlds"/"anim" load state machine. */
s32 StartWorldLoad(s32 arg) {
    char* name = gWorldName;
    char* buf = lbl_801151D8;

    if (world_load_state < 0)
        return 1;
    switch (world_load_state) {
    case 0:
        if (lbl_80344DA4 != 0) {
            s32 size = FileSize(name, "worlds");
            lbl_80344D7C = StartFileRead(name, "worlds", 0, size,
                                         lbl_80344DA4, BGLoadWorldFile);
            world_load_state = 1;
        } else {
            world_load_state = 2;
        }
        break;
    case 1:
        if (lbl_80344D7C->status != 0) {
            if (arg == 0)
                sSetupWorldHeader(lbl_80344DA4);
            lbl_80344D7C->status = -1;
            world_load_state = arg ? 100 : 2;
        }
        break;
    case 2:
        if (lbl_80344DA0 != 0) {
            s32 size = FileSize(name, &buf[36]);
            lbl_80344D7C = StartFileRead(name, &buf[36], 0, size,
                                         lbl_80344DA0, BGLoadWorldFile);
            world_load_state = 3;
        } else {
            world_load_state = 4;
        }
        break;
    case 3:
        if (lbl_80344D7C->status != 0) {
            if (arg == 0)
                sSetupWorldHeader(lbl_80344DA0);
            lbl_80344D7C->status = -1;
            world_load_state = arg ? 100 : 4;
        }
        break;
    case 4: {
        s32 model = *(s32*)(name + 360);

        *(s32*)(name + 364) =
            (s32)MBOX_FindTexture_Sub(&buf[48], 0, model, model, 1);
        world_load_state = 5;
    }
        /* fall through */
    case 5:
    default:
        return 1;
    }
    return 0;
}

/* BGLoadWorldFile: streamed-read progress callback.  status(+16)==2 -> done;
 * otherwise advance the running offset (+4) by the last chunk size (+8). */
void BGLoadWorldFile(s32* h) {
    if (h[4] == 2) {
        return;
    }
    h[1] = h[1] + h[2];
}

/* LoadWorldDone: finish a world load - alloc buffers, copy name, report mem.
 * All gWorldName/gWorldInfo stores go through the single lbl_8028C9A8 base
 * (gWorldName buffer; gWorldInfo fields live at +356/+360/+364). */
s32 LoadWorldDone(char* name) {
    s32* modelp;
    s32 freeBefore;
    s32 memBase;
    char* base = gWorldName;
    s32 size;
    lbl_80344D88 = 0;
    lbl_80344D84 = 0;
    lbl_80344D80 = 0;
    if (name != 0 && FileExists(name, "worlds") != 0) {
        freeBefore = BytesFree();
        *(modelp = (s32*)(base + 360)) = MBOX_AllocModel(name);
        lbl_80344D80 += freeBefore - BytesFree();
        memBase = mlmMemUsed;
        bulletproof_printf(lbl_80115214, name, memBase);
        size = FileSize(name, "worlds");
        lbl_80344DA4 = AllocMem(size);
        world_load_state = 0;
        lbl_80344D88 += size;
    } else {
        world_load_state = -1;
        return -1;
    }
    *(s32*)(base + 364) = 0;
    lbl_80344DA0 = 0;
    strcpy(base, name);
    if (FileExists(name, "anim") != 0) {
        size = FileSize(name, "anim");
        *(s32*)(base + 356) = (s32)AllocMem(size);
        lbl_80344D84 += size;
    } else {
        *(s32*)(base + 356) = 0;
    }
    bulletproof_printf(lbl_80115230, mlmMemUsed,
                       (mlmMemUsed - memBase) >> 10);
    return *modelp;
}

/* sSetupWorldHeader: byte-swap the loaded world-file header in place.  Xbox is
 * little-endian so the file needs no fixup there (the Xbox build compiles this
 * to a 1-byte stub); on the big-endian GameCube the 0x78-byte header is 30
 * consecutive 32-bit words - a mix of counts/offsets, floats and two vec3s at
 * +0x24 and +0x30 - each of which is byte-reversed.  (The byte-swap helpers
 * sSwapU16/sSwapU32/sSwapF32 are defined above, before DoWorldAnimSub.) */
static void sSetupWorldHeader(u32* w) {
    f32* f = (f32*)w;
    s32 i;

    w[0] = sSwapU32(w[0]);
    w[1] = sSwapU32(w[1]);
    w[2] = sSwapU32(w[2]);
    w[3] = sSwapU32(w[3]);
    w[4] = sSwapU32(w[4]);
    w[5] = sSwapU32(w[5]);
    w[6] = sSwapU32(w[6]);
    w[7] = sSwapU32(w[7]);
    w[8] = sSwapU32(w[8]);
    f[15] = sSwapF32(f[15]);
    w[16] = sSwapU32(w[16]);
    w[17] = sSwapU32(w[17]);
    w[18] = sSwapU32(w[18]);
    w[19] = sSwapU32(w[19]);
    w[20] = sSwapU32(w[20]);
    w[21] = sSwapU32(w[21]);
    w[22] = sSwapU32(w[22]);
    w[23] = sSwapU32(w[23]);
    w[24] = sSwapU32(w[24]);
    w[25] = sSwapU32(w[25]);
    w[26] = sSwapU32(w[26]);
    w[27] = sSwapU32(w[27]);
    w[28] = sSwapU32(w[28]);
    w[29] = sSwapU32(w[29]);
    for (i = 0; i < 3; i++) {
        f[9 + i] = sSwapF32(f[9 + i]);
        f[12 + i] = sSwapF32(f[12 + i]);
    }
}

/* FindWORLDOBJ: find an object by name from the current world root. */
WorldObj* FindWORLDOBJ(char* name) {
    WorldObj* node = world_objects;
    s16 index;

    while (node != 0) {
        if (strcmp(name, node->desc) != 0) {
            goto search_children;
        }
        goto done;
search_children:
        index = node->childidx;
        if (index >= 0) {
            WorldObj* r = FindWorldObject(&world_objects[index], name);
            if (r != 0) {
                node = r;
                goto done;
            }
        }
        index = node->nextidx;
        if (index >= 0) {
            node = &world_objects[index];
        } else {
            node = 0;
            goto done;
        }
    }
    node = 0;
done:
    return node;
}

/* FindWorldObject: recursive by-name subtree search. */
WorldObj* FindWorldObject(WorldObj* node, char* name) {
    while (node != 0) {
        if (strcmp(name, node->desc) == 0)
            return node;
        if (node->childidx >= 0) {
            WorldObj* r = FindWorldObject(&world_objects[node->childidx], name);
            if (r != 0)
                return r;
        }
        if (node->nextidx >= 0) {
            node = &world_objects[node->nextidx];
        } else {
            return 0;
        }
    }
    return 0;
}

/* ---- InitWorldInfo (0x800A9E1C) ------------------------------------------
 * Build one WorldInfo's runtime block from the loaded (little-endian) world
 * file image: fix up every table's endianness on the FIRST init only
 * (gWorldInfo.inited gates the swap; restores reuse the already-swapped
 * data), resolve the embedded offsets into pointers, derive the world
 * bounds/grid constants, allocate the coltri-checked and animdata arrays,
 * and arm the animation and particle-system tables.  Returns wobjs. */


WorldObj* InitWorldInfo(WorldInfo* wi, void* data) {
    s32* blob = (s32*)data;
    u8* base = (u8*)data;
    f32* fblob = (f32*)data;
    s32 memBase = mlmMemUsed;
    char* wg = gWorldName; /* base ptr; gWorldInfo.inited is *(s32*)(wg+228) */
    s32 i;
    s32 k;
    s32 n;
    s32 version;
    u8* p;
    f64 inv;
    f32 gsz;

    wi->wobjs = (struct worldobj*)(base + blob[1]);
    wi->ctris = (struct coltri*)(base + blob[3]);
    wi->gridrow = (struct GRIDROW*)(base + blob[8]);
    wi->grid = (struct GRIDENTRY*)(base + blob[5]);
    wi->gridobjlist = (char*)(base + blob[7]);
    wi->iteminfo = (struct iteminfo*)(base + blob[0x13]);
    wi->iteminst = (struct iteminst*)(base + blob[0x15]);
    wi->locators = (struct locator*)(base + blob[0x17]);

    if (*(s32*)(wg + 228) == 0) {
        /* world objects (stride 0x3C) */
        for (i = 0; i < blob[0]; i++) {
            p = (u8*)wi->wobjs + i * 0x3C;
            *(u16*)(p + 0x14) = sSwapU16(*(u16*)(p + 0x14));
            *(u16*)(p + 0x2C) = sSwapU16(*(u16*)(p + 0x2C));
            *(u16*)(p + 0x2E) = sSwapU16(*(u16*)(p + 0x2E));
            *(u16*)(p + 0x36) = sSwapU16(*(u16*)(p + 0x36));
            *(u32*)(p + 0x10) = sSwapU32(*(u32*)(p + 0x10));
            *(f32*)(p + 0x30) = sSwapF32(*(f32*)(p + 0x30));
            *(u32*)(p + 0x38) = sSwapU32(*(u32*)(p + 0x38));
            *(u32*)(p + 0x18) = sSwapU32(*(u32*)(p + 0x18));
            *(u32*)(p + 0x28) = sSwapU32(*(u32*)(p + 0x28));
            for (k = 0; k < 3; k++) {
                *(f32*)(p + 0x1C + k * 4) = sSwapF32(*(f32*)(p + 0x1C + k * 4));
            }
        }
        /* collision triangles (stride 0x28) */
        for (i = 0; i < blob[2]; i++) {
            p = (u8*)wi->ctris + i * 0x28;
            *(u16*)(p + 0x00) = sSwapU16(*(u16*)(p + 0x00));
            *(u16*)(p + 0x02) = sSwapU16(*(u16*)(p + 0x02));
            *(u16*)(p + 0x20) = sSwapU16(*(u16*)(p + 0x20));
            *(u16*)(p + 0x22) = sSwapU16(*(u16*)(p + 0x22));
            *(u16*)(p + 0x24) = sSwapU16(*(u16*)(p + 0x24));
            *(u16*)(p + 0x26) = sSwapU16(*(u16*)(p + 0x26));
            *(f32*)(p + 0x04) = sSwapF32(*(f32*)(p + 0x04));
            for (k = 0; k < 3; k++) {
                *(f32*)(p + 0x08 + k * 4) = sSwapF32(*(f32*)(p + 0x08 + k * 4));
                *(f32*)(p + 0x14 + k * 4) = sSwapF32(*(f32*)(p + 0x14 + k * 4));
            }
        }
        /* grid rows (stride 8) */
        for (i = 0; i < blob[0x11]; i++) {
            p = (u8*)wi->gridrow + i * 8;
            *(u16*)(p + 0x00) = sSwapU16(*(u16*)(p + 0x00));
            *(u16*)(p + 0x02) = sSwapU16(*(u16*)(p + 0x02));
            *(u32*)(p + 0x04) = sSwapU32(*(u32*)(p + 0x04));
        }
        /* grid object index list (halfwords) */
        n = (s32)((u32)(blob[5] - blob[7]) >> 1);
        for (i = 0; i < n; i++) {
            u16* hp = (u16*)wi->gridobjlist + i;
            *hp = sSwapU16(*hp);
        }
        /* grid entries (words) */
        n = (s32)((u32)(blob[8] - blob[5]) >> 2);
        for (i = 0; i < n; i++) {
            u32* wp = (u32*)wi->grid + i;
            *wp = sSwapU32(*wp);
        }
        /* item infos (stride 0x50) */
        for (i = 0; i < blob[0x12]; i++) {
            p = (u8*)wi->iteminfo + i * 0x50;
            *(u32*)p = sSwapU32(*(u32*)p);
            if (*(s32*)p != -1) {
                *(u16*)(p + 0x08) = sSwapU16(*(u16*)(p + 0x08));
                *(u16*)(p + 0x0A) = sSwapU16(*(u16*)(p + 0x0A));
                *(u16*)(p + 0x40) = sSwapU16(*(u16*)(p + 0x40));
                *(u16*)(p + 0x42) = sSwapU16(*(u16*)(p + 0x42));
                *(u16*)(p + 0x44) = sSwapU16(*(u16*)(p + 0x44));
                *(u16*)(p + 0x46) = sSwapU16(*(u16*)(p + 0x46));
                *(u16*)(p + 0x48) = sSwapU16(*(u16*)(p + 0x48));
                *(u16*)(p + 0x4A) = sSwapU16(*(u16*)(p + 0x4A));
                *(f32*)(p + 0x0C) = sSwapF32(*(f32*)(p + 0x0C));
                *(f32*)(p + 0x10) = sSwapF32(*(f32*)(p + 0x10));
                *(f32*)(p + 0x14) = sSwapF32(*(f32*)(p + 0x14));
                *(f32*)(p + 0x18) = sSwapF32(*(f32*)(p + 0x18));
                *(u32*)(p + 0x38) = sSwapU32(*(u32*)(p + 0x38));
                *(u32*)(p + 0x3C) = sSwapU32(*(u32*)(p + 0x3C));
                *(u32*)(p + 0x04) = sSwapU32(*(u32*)(p + 0x04));
                *(u32*)(p + 0x4C) = sSwapU32(*(u32*)(p + 0x4C));
                for (k = 0; k < 3; k++) {
                    *(f32*)(p + 0x1C + k * 4) =
                        sSwapF32(*(f32*)(p + 0x1C + k * 4));
                }
            } else {
                *(u32*)(p + 0x04) = sSwapU32(*(u32*)(p + 0x04));
                for (k = 0; k < 16; k++) {
                    *(u16*)(p + 0x08 + k * 2) =
                        sSwapU16(*(u16*)(p + 0x08 + k * 2));
                }
            }
        }
        /* item instances (stride 0x3C); payload layout depends on info type */
        for (i = 0; i < blob[0x14]; i++) {
            p = (u8*)wi->iteminst + i * 0x3C;
            *(u16*)(p + 0x00) = sSwapU16(*(u16*)(p + 0x00));
            *(u16*)(p + 0x04) = sSwapU16(*(u16*)(p + 0x04));
            *(u16*)(p + 0x06) = sSwapU16(*(u16*)(p + 0x06));
            switch (*(s32*)((u8*)wi->iteminfo + *(s16*)p * 0x50)) {
            case 2:
                *(u16*)(p + 0x34) = sSwapU16(*(u16*)(p + 0x34));
                *(u32*)(p + 0x30) = sSwapU32(*(u32*)(p + 0x30));
                break;
            case 5:
                *(u16*)(p + 0x30) = sSwapU16(*(u16*)(p + 0x30));
                *(u16*)(p + 0x32) = sSwapU16(*(u16*)(p + 0x32));
                *(u16*)(p + 0x38) = sSwapU16(*(u16*)(p + 0x38));
                *(u16*)(p + 0x3A) = sSwapU16(*(u16*)(p + 0x3A));
                break;
            case 4:
                *(u16*)(p + 0x30) = sSwapU16(*(u16*)(p + 0x30));
                *(u16*)(p + 0x32) = sSwapU16(*(u16*)(p + 0x32));
                *(u16*)(p + 0x38) = sSwapU16(*(u16*)(p + 0x38));
                *(f32*)(p + 0x34) = sSwapF32(*(f32*)(p + 0x34));
                break;
            case 3:
                *(u16*)(p + 0x30) = sSwapU16(*(u16*)(p + 0x30));
                *(u16*)(p + 0x32) = sSwapU16(*(u16*)(p + 0x32));
                *(u16*)(p + 0x34) = sSwapU16(*(u16*)(p + 0x34));
                *(u16*)(p + 0x36) = sSwapU16(*(u16*)(p + 0x36));
                break;
            case 9:
                *(u32*)(p + 0x30) = sSwapU32(*(u32*)(p + 0x30));
                break;
            case 11:
                *(u32*)(p + 0x30) = sSwapU32(*(u32*)(p + 0x30));
                *(u32*)(p + 0x34) = sSwapU32(*(u32*)(p + 0x34));
                break;
            case 12:
                *(u32*)(p + 0x30) = sSwapU32(*(u32*)(p + 0x30));
                *(f32*)(p + 0x34) = sSwapF32(*(f32*)(p + 0x34));
                *(f32*)(p + 0x38) = sSwapF32(*(f32*)(p + 0x38));
                break;
            case 13:
                *(u16*)(p + 0x38) = sSwapU16(*(u16*)(p + 0x38));
                *(u16*)(p + 0x3A) = sSwapU16(*(u16*)(p + 0x3A));
                *(f32*)(p + 0x30) = sSwapF32(*(f32*)(p + 0x30));
                *(u32*)(p + 0x34) = sSwapU32(*(u32*)(p + 0x34));
                break;
            case 10:
                *(u16*)(p + 0x30) = sSwapU16(*(u16*)(p + 0x30));
                *(u16*)(p + 0x32) = sSwapU16(*(u16*)(p + 0x32));
                break;
            case 1:
                *(u16*)(p + 0x30) = sSwapU16(*(u16*)(p + 0x30));
                break;
            case 6:
                *(u16*)(p + 0x30) = sSwapU16(*(u16*)(p + 0x30));
                *(u16*)(p + 0x32) = sSwapU16(*(u16*)(p + 0x32));
                break;
            }
            for (k = 0; k < 3; k++) {
                *(f32*)(p + 0x18 + k * 4) = sSwapF32(*(f32*)(p + 0x18 + k * 4));
                *(f32*)(p + 0x24 + k * 4) = sSwapF32(*(f32*)(p + 0x24 + k * 4));
            }
        }
        /* locators (stride 0x1C) */
        for (i = 0; i < blob[0x16]; i++) {
            p = (u8*)wi->locators + i * 0x1C;
            *(u16*)(p + 0x02) = sSwapU16(*(u16*)(p + 0x02));
            for (k = 0; k < 3; k++) {
                *(f32*)(p + 0x04 + k * 4) = sSwapF32(*(f32*)(p + 0x04 + k * 4));
                *(f32*)(p + 0x10 + k * 4) = sSwapF32(*(f32*)(p + 0x10 + k * 4));
            }
        }
    }

    wi->nwobjs = blob[0];
    wi->nctris = blob[2];
    wi->niteminfos = blob[0x12];
    wi->niteminsts = blob[0x14];
    wi->nlocators = blob[0x16];
    wi->worldmin[0] = fblob[9];
    wi->worldmin[1] = fblob[10];
    wi->worldmin[2] = fblob[11];
    wi->worldmax[0] = fblob[12];
    wi->worldmax[1] = fblob[13];
    wi->worldmax[2] = fblob[14];
    wi->worldsize[0] = wi->worldmax[0] - wi->worldmin[0];
    wi->worldsize[1] = wi->worldmax[1] - wi->worldmin[1];
    wi->worldsize[2] = wi->worldmax[2] - wi->worldmin[2];
    for (i = 0; i < 3; i++) {
        wi->worldcenter[i] =
            (f32)(lbl_80348788 * wi->worldsize[i] + wi->worldmin[i]);
    }
    wi->gridsize = fblob[0xF];
    gsz = fblob[0xF];
    if (!sWorldFloatEqual(gsz, lbl_80348778)) {
        inv = lbl_803487A8 / gsz;
    } else {
        inv = lbl_80348768;
    }
    wi->invgridsize = (f32)inv;
    wi->gridnumx = blob[0x10];
    wi->gridnumz = blob[0x11];
    wi->checknum = 1;
    InitDynobjGrid();
    wi->coltrichecked = AllocMem(blob[2]);
    for (i = 0; i < blob[2]; i++) {
        wi->coltrichecked[i] = 0;
    }

    /* header magic: 0xF00BABvv, vv = format version */
    if (((u32)blob[0x18] & 0xF00BAB00) == 0xF00BAB00) {
        version = blob[0x18] & 0xFF;
    } else {
        version = 0;
    }

    if (version >= 1 && blob[0x19] != 0) {
        if (*(s32*)(wg + 228) == 0) {
            wi->animheader = SetupAnimHeader(base + blob[0x19], 0);
        }
        wi->worldanims = (struct worldanim*)(base + blob[0x1B]);
        if (*(s32*)(wg + 228) == 0) {
            for (i = 0; i < blob[0x1A]; i++) {
                p = (u8*)wi->worldanims + i * 0x10;
                *(u16*)(p + 0x00) = sSwapU16(*(u16*)(p + 0x00));
                *(u16*)(p + 0x02) = sSwapU16(*(u16*)(p + 0x02));
                *(u16*)(p + 0x04) = sSwapU16(*(u16*)(p + 0x04));
                *(u16*)(p + 0x06) = sSwapU16(*(u16*)(p + 0x06));
                *(f32*)(p + 0x08) = sSwapF32(*(f32*)(p + 0x08));
                *(u32*)(p + 0x0C) = sSwapU32(*(u32*)(p + 0x0C));
            }
        }
        wi->nworldanims = blob[0x1A];
        wi->animdata = AllocMem(blob[0x1A] * 0xA0);
        if (*(s32*)(wg + 228) == 0) {
            for (i = 0; i < blob[0x1A]; i++) {
                p = (u8*)wi->animdata + i * 0xA0;
                *(u16*)(p + 0x08) = sSwapU16(*(u16*)(p + 0x08));
                *(u16*)(p + 0x0A) = sSwapU16(*(u16*)(p + 0x0A));
                *(u32*)(p + 0x04) = sSwapU32(*(u32*)(p + 0x04));
                *(u32*)(p + 0x0C) = sSwapU32(*(u32*)(p + 0x0C));
                *(u32*)(p + 0x00) = sSwapU32(*(u32*)(p + 0x00));
                for (k = 0; k < 3; k++) {
                    *(f32*)(p + 0x10 + k * 4) =
                        sSwapF32(*(f32*)(p + 0x10 + k * 4));
                    *(f32*)(p + 0x20 + k * 4) =
                        sSwapF32(*(f32*)(p + 0x20 + k * 4));
                    *(f32*)(p + 0x30 + k * 4) =
                        sSwapF32(*(f32*)(p + 0x30 + k * 4));
                    *(f32*)(p + 0x40 + k * 4) =
                        sSwapF32(*(f32*)(p + 0x40 + k * 4));
                    *(f32*)(p + 0x50 + k * 4) =
                        sSwapF32(*(f32*)(p + 0x50 + k * 4));
                    *(f32*)(p + 0x60 + k * 4) =
                        sSwapF32(*(f32*)(p + 0x60 + k * 4));
                    *(f32*)(p + 0x70 + k * 4) =
                        sSwapF32(*(f32*)(p + 0x70 + k * 4));
                    *(f32*)(p + 0x80 + k * 4) =
                        sSwapF32(*(f32*)(p + 0x80 + k * 4));
                    *(f32*)(p + 0x90 + k * 4) =
                        sSwapF32(*(f32*)(p + 0x90 + k * 4));
                }
            }
        }
        for (i = 0; i < wi->nworldanims; i++) {
            if (!(wi->worldanims[i].fixed & 0x8000)) {
                wi->worldanims[i].data =
                    base + (s32)wi->worldanims[i].data;
                wi->worldanims[i].fixed |= 0x8000;
            }
            InitAnimData((u8*)wi->animdata + i * 0xA0, wi->worldanims[i].data);
            wi->worldanims[i].state = 0x101;
            ((WorldObj*)wi->wobjs)[wi->worldanims[i].objidx].flags |=
                0x2000000;
        }
    } else {
        wi->animheader = 0;
        wi->worldanims = 0;
        wi->nworldanims = 0;
        wi->animdata = 0;
    }

    if (version >= 2 && blob[0x1D] != 0) {
        wi->worldpsys = (struct WORLDPSYS*)(base + blob[0x1D]);
        wi->nworldpsys = blob[0x1C];
        if (*(s32*)(wg + 228) == 0) {
            for (i = 0; i < blob[0x1C]; i++) {
                fn_80011DCC((u8*)wi->worldpsys + i * 0x138);
            }
        }
    } else {
        wi->worldpsys = 0;
        wi->nworldpsys = 0;
    }

    wi->inited = 1;
    bulletproof_printf(lbl_80115244, (mlmMemUsed - memBase) >> 10);
    return (WorldObj*)wi->wobjs;
}

/* WorldObjGetAllFlags: OR the +0x10 flag word across the +0x18 parent list. */
u32 WorldObjGetAllFlags(WorldObj* o) {
    u32 acc = 0;
    while (o != 0) {
        acc |= o->flags;
        o = o->parent;
    }
    return acc;
}

/* CreateWorldNode: recursively build g3d display nodes for the object tree. */
void CreateWorldNode(WorldObj* base, WorldObj* obj, void* parent) {
    while (1) {
        if (obj->flags & 0x800) {
            obj->nodeptr = NewWorldObject(obj, parent);
            if (!(obj->flags & 0x10000000)) {
                WorldPsysActivate(obj);
            }
        } else if (obj->nodeptr == (struct mbnode*)1 ||
                   (obj->flags & 0x80000000)) {
            obj->nodeptr = NewWorldObject(obj, parent);
            MBOX_SetObject(obj->nodeptr, obj);
            obj->flags |= 0x80000000;
        } else {
            obj->nodeptr = NewWorldObject(obj, parent);
        }
        if (obj->flags & 0x100) {
            obj->flags &= -59;
        }
        if (obj->childidx >= 0) {
            CreateWorldNode(base, &base[obj->childidx], obj);
        }
        if (obj->nextidx < 0) {
            return;
        }
        obj = &base[obj->nextidx];
    }
}

/* WorldPsysDeActivate: if the object's psys-active bit (0x00800000) is set,
 * free the particle system, clear that bit and set the "was-active" bit
 * (0x00400000). */
s32 WorldPsysDeActivate(WorldObj* o) {
    if (o->flags & 0x00800000) {
        MBRemovePsys(o->nodeptr);
        o->flags &= ~0x00800000;
        o->flags |= 0x00400000;
    }
    return 1;
}

/* WorldPsysActivate: look up the "PSYS<id>" template in gWorldInfo.worldpsys
 * and spawn the particle system for this object (with GetWorldPsysIdx inlined
 * as the id search). */
/* GetWorldPsysIdx (Xbox local fn, inlined here): find template by id char. */
static inline s32 GetWorldPsysIdx(s8 id, char* base, u8* tbl) {
    s32 i;

    i = 0;

    for (; i < *(s32*)(base + 388); i++) {
        if ((s8)tbl[i * 312 + 6] == id) {
            return i;
        }
    }
    ErrorPrintf(lbl_80115280, id);
    return -1;
}

s32 WorldPsysActivate(WorldObj* obj) {
    char* base = gWorldName;
    char* tag;
    s32 i;
    f32 pos[3];
    f32* posp;
    u8** wpsp;

    if (obj->flags & 0x00800000) {
        goto done;
    }

    tag = strstr((char*)obj, "PSYS");
    if (tag == NULL) {
        ErrorPrintf(lbl_80115264, obj);
        return 0;
    }

    i = GetWorldPsysIdx((s8)tag[4], base,
                        *(wpsp = (u8**)(base + 384)));
    if (i < 0) {
        goto done;
    }

    posp = NULL;
    if (*(s16*)((u8*)obj + 54) > 0) {
        u8* ct = *(u8**)(base + 236) + *(s32*)((u8*)obj + 56) * 40 + 8;
        pos[0] = (f32)(-1.0 * *(f32*)ct);
        pos[1] = (f32)(-1.0 * *(f32*)(ct + 4));
        pos[2] = (f32)(-1.0 * *(f32*)(ct + 8));
        posp = pos;
    }
    MBNewWorldPsys(0, *(void**)((u8*)obj + 40),
                   *wpsp + i * 312,
                   obj->flags & 0x1000, obj, posp);
    obj->flags &= ~0x00400000;
    obj->flags |= 0x00800000;

done:
    *(s32*)(*(u8**)((u8*)obj + 40) + 96) &= ~2;
    return 1;
}

/* NewWorldObject: create one object's g3d display node. */
void* NewWorldObject(WorldObj* obj, WorldObj* parent) {
    G3DNode* node;
    void* p;
    struct WorldObj* tmp;
    if (parent == 0)
        p = lbl_80344D8C;
    else
        p = parent->nodeptr;
    node = MBNewNode(p, 0, 1);
    node->x = obj->pos[0];
    node->y = obj->pos[1];
    node->z = obj->pos[2];
    if (parent != 0) {
        if (!(obj->flags & 0x01001000)) {
            if (parent->flags & 0x01001000) {
                ErrorPrintf(lbl_801152A0);
            }
            obj->pos[0] = parent->pos[0] + obj->pos[0];
            obj->pos[1] = parent->pos[1] + obj->pos[1];
            obj->pos[2] = parent->pos[2] + obj->pos[2];
        }
    }
    tmp = obj->parent;
    obj->parent = parent;
    MBTreeSetFlags(node, tmp, 0);
    if (parent != 0) {
        if (parent->flags & 0x02000000) {
            obj->flags |= 0x02000000;
        }
    }
    if ((obj->flags & 1) && !(obj->flags & 0x1000)) {
        MBTreeSetFlags(node, 4, 0);
    }
    if (obj->flags & 0x8000) {
        MBTreeSetFlags(node, 1, 0);
    }
    if (obj->flags & 0x400) {
        MBTreeSetZsortAdd(node, -2, 1);
    }
    return node;
}
