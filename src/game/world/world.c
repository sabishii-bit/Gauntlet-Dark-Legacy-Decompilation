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
 * Only a compilable subset of bodies is transcribed; matching is out of scope
 * (NonMatching, DOL bytes substituted).  InitWorldInfo (~0x1660) is parked and
 * documented, not reconstructed. */

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
    /* 0x04 */ u8    _pad4[2];
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
extern char      gWorldName[64];  /* 0x8028C9A8 current world directory name  */
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
extern char   lbl_803487B0[];   /* "PSYS"                                      */

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

/* InitWorldInfo (0x800A9E1C, ~0x1660 bytes) - PARKED GIANT, documented only.
 *
 * Allocates and initialises one WorldInfo's runtime data from a just-loaded,
 * endian-fixed world block, and returns the wobjs array base (stored by the
 * callers into world_objects / lbl_80344D98).  From the region survey it calls
 * InitDynobjGrid, AllocMem, SetupAnimHeader, InitAnimData, fn_80011DCC and
 * bulletproof_printf(lbl_80115244 = "---- ALLOC World Data [%dK]\n", ...), and
 * dispatches object set-up through jumptable_80126C30.  It fills wobjs, ctris,
 * the collision grid (gridrow/grid/gridsize/gridnum*), world bounds, item and
 * locator arrays, the atree/animation lists and the worldpsys table.  Left as
 * ONLY-IN-TARGET (no body emitted) per the work-order: too large to
 * reconstruct usefully in this pass. */
extern WorldObj* InitWorldInfo(WorldInfo* wi);

/* forward declarations (same TU) */
static void sSetupWorldHeader(void* hdr);
void  BGLoadWorldFile(s32* h);
void* NewWorldObject(WorldObj* obj, WorldObj* parent);
s32   WorldPsysActivate(WorldObj* obj);
void  CreateWorldNode(WorldObj* base, WorldObj* obj, void* parent);
void  ToggleWorldDisplay(void);
WorldObj* FindWorldObject(WorldObj* node, char* name);

static const char lbl_80348798[] = "anim";
static const char lbl_803487A0[] = "worlds";
static const float lbl_80348778 = 0.0f;

/* DoWorldAnimSub: advance one object's keyframe animation by one frame.
 * `wa` is the worldanim track; `panim` points at the object's animdata pointer
 * (panim[0] is the little-endian keyframe stream). `animBase` is the shared
 * animation-data block used by the stream's byte-swapped +4 offset. Returns
 * the swapped mode word from the stream header, or 0 when nothing animated. */
s32 DoWorldAnimSub(struct worldanim* wa, void** panim, u8* animBase) {
    WorldObj* wobjs = (WorldObj*)gWorldInfo.wobjs;
    WorldObj* obj = &wobjs[wa->objidx];
    void* data = panim[0];
    G3DNode* node;
    u8* d;
    u8* sequence;
    u32 f;
    s32 mode;
    s32 nframes;
    f32 dt = gClockFrameStep;
    f32 xf[16]; /* sampled transform (pos @16, scale @32) from CalcAnimData */

    if (data == NULL) {
        return 0;
    }
    node = (G3DNode*)obj->nodeptr;
    if (node == NULL) {
        return 0;
    }

    /* Derive the per-track direction/mode bits from the object flags. */
    f = obj->flags;
    if (f & 0x00100000) {
        if (f & 0x00200000) {
            wa->state &= ~1;           /* both set -> disable this frame */
        } else {
            wa->state |= 1;
            wa->state &= ~0x300;
            wa->state |= 2;
        }
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

    /* Read the stream header (stored little-endian). */
    d = (u8*)data;
    mode = WORLD_BSWAP16(*(u16*)(d + 0));
    sequence = animBase + WORLD_BSWAP32(*(u32*)(d + 4));

    if ((mode & 0xFFF) == 0) {
        /* No keyframes: reset to the template pose. */
        CopyMat4(gIdentityMatrix, node);
        ZeroAnimData(panim);
    } else {
        nframes = wa->nframes;
        if (CalcAnimData(panim, xf, sequence, mode, 0, nframes, wa->curframe) != 0) {
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
    if ((lbl_803447DC != 0 && (wa->state & 0x100)) || dt <= 0.0f) {
        return mode;
    }

    obj->flags &= ~0x00C00000;
    obj->flags |= 0x08000000;

    if (wa->state & 2) {
        /* Reverse. */
        wa->curframe = wa->curframe - 30.0f * dt;
        if (wa->curframe < 0.0f) {
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
        } else if ((s32)wa->curframe >= wa->nframes - 2) {
            if (obj->flags & 0x10000000) {
                obj->flags &= ~0x10000000;
                MBTreeClearFlags(obj->nodeptr, 2, 0);
            }
        }
    } else {
        /* Forward. */
        wa->curframe = wa->curframe + 30.0f * dt;
        if ((s32)wa->curframe >= wa->nframes - 1) {
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
        } else if ((s32)wa->curframe <= 1) {
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
    WorldObj* wobjs = (WorldObj*)gWorldInfo.wobjs;
    WorldObj* best = NULL;
    f32 bestd2 = maxdist * maxdist;
    s32 i;

    for (i = 0; i < gWorldInfo.nworldanims; i++) {
        struct worldanim* wa = &gWorldInfo.worldanims[i];
        WorldObj* obj;
        f32 m[16]; /* node world-state; translation at +0x30 (index 12..14) */
        f32 dx, dy, dz, d2;

        if (wa->data == NULL) {
            continue;
        }
        obj = &wobjs[wa->objidx];
        GetWorldMat(obj->nodeptr, m, 0);
        dx = m[13] - point[1];
        dy = m[12] - point[0];
        dz = m[14] - point[2];
        d2 = dy * dy + dx * dx + dz * dz;
        if (d2 < bestd2) {
            best = obj;
            bestd2 = d2;
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
    world_root0 = 0;
    world_root1 = 0;
    lbl_80344DA4 = 0;
    lbl_80344DA0 = 0;
    gWorldInfo.model = -1;
    gWorldInfo2.model = -1;
    gWorldInfo.inited = 0;
    gWorldInfo2.inited = 0;
    lbl_80344D74 = 0;
    lbl_80344D78 = 0;
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

    WorldDisplay = 0;
    if (lbl_80344DA4 != 0) {
        WorldObj* wobjs;
        s32 count = *(s32*)lbl_80344DA4;
        world_objects = InitWorldInfo(&gWorldInfo);
        memBase = mlmMemUsed;
        lbl_80344D74 = AllocMem(count * 4);
        lbl_80344D78 = AllocMem(count * 12);
        wobjs = (WorldObj*)gWorldInfo.wobjs;
        for (i = 0; i < gWorldInfo.nwobjs; i++) {
            lbl_80344D74[i] = (s32)wobjs[i].parent;
            lbl_80344D78[i * 3 + 0] = wobjs[i].pos[0];
            lbl_80344D78[i * 3 + 1] = wobjs[i].pos[1];
            lbl_80344D78[i * 3 + 2] = wobjs[i].pos[2];
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
        lbl_80344D98 = InitWorldInfo(&gWorldInfo2);
        lbl_80344D8C = world_root1;
        CreateWorldNode(lbl_80344D98, lbl_80344D98, 0);
        MBTreeSetAltTex(world_root1, -2, gWorldInfo.whitetex, 1);
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
    WorldObj* wobjs = (WorldObj*)gWorldInfo.wobjs;
    s32 i;

    if (lbl_80344D74 == 0)
        return;
    if (lbl_80344D78 == 0)
        return;
    for (i = 0; i < gWorldInfo.nwobjs; i++) {
        WorldObj* o = &wobjs[i];
        o->flags &= 0xC31FFFFF;
        o->parent = (struct WorldObj*)lbl_80344D74[i];
        o->pos[0] = lbl_80344D78[i * 3 + 0];
        o->pos[1] = lbl_80344D78[i * 3 + 1];
        o->pos[2] = lbl_80344D78[i * 3 + 2];
    }
    for (i = 0; i < gWorldInfo.nworldanims; i++) {
        if (*(s32*)((char*)gWorldInfo.animdata + i * 0xA0) != 0) {
            gWorldInfo.worldanims[i].curframe = lbl_80348778;
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
        s32 size = FileSize(dir, lbl_80348798);
        atree_finfo = StartFileRead(dir, lbl_80348798, 0, size,
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
    char* buf = lbl_801151D8;
    if (world_load_state < 0)
        return 1;
    switch (world_load_state) {
    case 0:
        if (lbl_80344DA4 != 0) {
            s32 size = FileSize(gWorldName, lbl_803487A0);
            lbl_80344D7C = StartFileRead(gWorldName, lbl_803487A0, 0, size,
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
            s32 size = FileSize(gWorldName, &buf[36]);
            lbl_80344D7C = StartFileRead(gWorldName, &buf[36], 0, size,
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
    case 4:
        gWorldInfo.whitetex =
            (s32)MBOX_FindTexture_Sub(&buf[48], 0, gWorldInfo.model, gWorldInfo.model, 1);
        world_load_state = 5;
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

/* LoadWorldDone: finish a world load - alloc buffers, copy name, report mem. */
s32 LoadWorldDone(char* name) {
    s32 freeBefore;
    s32 memBase;
    s32 size;
    lbl_80344D88 = 0;
    lbl_80344D84 = 0;
    lbl_80344D80 = 0;
    if (name != 0 && FileExists(name, lbl_803487A0) != 0) {
        freeBefore = BytesFree();
        gWorldInfo.model = MBOX_AllocModel(name);
        lbl_80344D80 += freeBefore - BytesFree();
        memBase = mlmMemUsed;
        bulletproof_printf(lbl_80115214, name, memBase);
        size = FileSize(name, lbl_803487A0);
        lbl_80344DA4 = AllocMem(size);
        lbl_80344D88 += size;
        world_load_state = 0;
        gWorldInfo.whitetex = 0;
        lbl_80344DA0 = 0;
        strcpy(gWorldName, name);
        if (FileExists(name, lbl_80348798) != 0) {
            size = FileSize(name, lbl_80348798);
            gWorldInfo.atreelist = (struct atreelist*)AllocMem(size);
            lbl_80344D84 += size;
        } else {
            gWorldInfo.atreelist = 0;
        }
        bulletproof_printf(lbl_80115230, mlmMemUsed,
                           (mlmMemUsed - memBase) >> 10);
        return gWorldInfo.model;
    } else {
        world_load_state = -1;
        return -1;
    }
}

/* sSetupWorldHeader: byte-swap the loaded world-file header in place.  Xbox is
 * little-endian so the file needs no fixup there (the Xbox build compiles this
 * to a 1-byte stub); on the big-endian GameCube the 0x78-byte header is 30
 * consecutive 32-bit words - a mix of counts/offsets, floats and two vec3s at
 * +0x24 and +0x30 - each of which is byte-reversed. */
static void sSetupWorldHeader(void* hdr) {
    u32* w = (u32*)hdr;
    s32 i;
    for (i = 0; i < 30; i++) {
        w[i] = WORLD_BSWAP32(w[i]);
    }
}

/* FindWORLDOBJ: find an object by name from the current world root.  Near-match
 * (real=4): the target uses a single `node`-holding return register plus a
 * bne/b split on the strcmp; parked per the 3-attempt regalloc cap. */
WorldObj* FindWORLDOBJ(char* name) {
    WorldObj* node = world_objects;
    while (node != 0) {
        if (strcmp(name, node->desc) == 0) {
            break;
        } else {
            if (node->childidx >= 0) {
                WorldObj* r = FindWorldObject(&world_objects[node->childidx], name);
                if (r != 0) {
                    node = r;
                    break;
                }
            }
            if (node->nextidx >= 0) {
                node = &world_objects[node->nextidx];
            } else {
                node = 0;
                break;
            }
        }
    }
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
s32 WorldPsysActivate(WorldObj* obj) {
    G3DNode* node;
    char* tag;
    s8 id;
    s32 idx;
    f32 pos[3];
    f32* posp;

    if (obj->flags & 0x00800000) {
        goto done;
    }

    tag = strstr(obj->desc, lbl_803487B0);
    if (tag == NULL) {
        ErrorPrintf(lbl_80115264, obj);
        return 0;
    }

    /* GetWorldPsysIdx: match the id char after "PSYS" against the table. */
    id = (s8)tag[4];
    idx = -1;
    {
        s32 i;
        for (i = 0; i < gWorldInfo.nworldpsys; i++) {
            if ((s8)gWorldInfo.worldpsys[i].id == id) {
                idx = i;
                break;
            }
        }
    }
    if (idx < 0) {
        ErrorPrintf(lbl_80115280, id);
        goto done;
    }

    posp = NULL;
    if (obj->nctris > 0) {
        struct coltri* ct = &((struct coltri*)gWorldInfo.ctris)[obj->ctriidx];
        pos[0] = -ct->pos[0];
        pos[1] = -ct->pos[1];
        pos[2] = -ct->pos[2];
        posp = pos;
    }
    MBNewWorldPsys(0, obj->nodeptr, &gWorldInfo.worldpsys[idx],
                obj->flags & 0x1000, obj, posp);
    obj->flags &= ~0x00400000;
    obj->flags |= 0x00800000;

done:
    node = (G3DNode*)obj->nodeptr;
    node->dflags &= ~2;
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
