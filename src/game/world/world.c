#include "types.h"

/* Gauntlet world-object/scene module (Xbox WORLD.OBJ), region
 * 0x800A87C8 - 0x800AB8E0.  Wired NonMatching so the tree stays green while
 * the real Xbox-PDB symbols are mapped onto the DOL bytes.  This TU manages
 * the loaded "worlds/<name>" scene: a tree of WORLDOBJ nodes (child at +44,
 * sibling at +46, stride 0x3C), their g3d display nodes, particle systems
 * ("PSYS" tagged objects), keyframe animation, and the save/restore of each
 * object's initial transform.
 *
 * The three module-level data globals match the Xbox WORLD.OBJ data symbols:
 *   world_load_state  0x80344D68 - load state machine (0..5) driven by StartWorldLoad
 *   atree_finfo       0x80344D6C - animation-tree file read handle
 *   WorldDisplay      0x80344D70 - display mode; ToggleWorldDisplay maps it to
 *                                  which of the two scene roots are visible
 *
 * IDENTIFIED functions (real WORLD.OBJ names; HIGH confidence unless noted).
 * Evidence = rodata strings ("ALLOC World Save State/Data", "World psys: bad
 * name", "World obj with dynamic parent", "worlds"/"anim"/"PSYS"), call graph,
 * and behaviour:
 *   DoWorldAnimSub        fn_800A87C8 - advance one object's keyframe animation
 *   NewWorld              fn_800A8E6C - create the two scene-root display nodes   [med]
 *   WorldSaveInitState    fn_800A8ECC - alloc + snapshot each object's init xform
 *   WorldRestoreInitState fn_800A9088 - restore the snapshot taken above
 *   ToggleWorldDisplay    fn_800A91B0 - show/hide scene roots per WorldDisplay    [med]
 *   FinishLoadWorldAnim   fn_800A92A0 - poll/close the atree_finfo read           [med]
 *   StartLoadWorldAnim    fn_800A92FC - begin reading "anim" file into atree_finfo
 *   StartWorldLoad        fn_800A93C4 - multi-file "worlds" load state machine    [med]
 *   BGLoadWorldFile       fn_800A95AC - background file-read progress callback
 *   LoadWorldDone         fn_800A95CC - finish load: alloc, copy name, report mem
 *   FindWORLDOBJ          fn_800A9C50 - find object by name from current world
 *   FindWorldObject       fn_800A9CFC - recursive by-name subtree search          [med]
 *   InitWorldInfo         fn_800A9E1C - alloc + init a world-data block ("World Data")
 *   WorldObjGetAllFlags   fn_800AB47C - OR the flag word down an object list
 *   CreateWorldNode       fn_800AB4A0 - recursively build g3d nodes for the tree
 *   WorldPsysDeActivate   fn_800AB5A4 - free an object's particle system
 *   WorldPsysActivate     fn_800AB5FC - look up "PSYS" name, spawn the particle sys
 *   NewWorldObject        fn_800AB780 - create one object's g3d node
 *
 * Left fn_XXXXXXXX (GC-specific / genuinely ambiguous; see session report):
 *   fn_800A8CD8 nearest-object-to-point search (distance, no clean PDB name)
 *   fn_800A8E30 world-globals reset (InitWorlds vs ResetWorlds ambiguous)
 *   fn_800A9088 restore helper, fn_800A9378/fn_800A9398 tiny model-load wrappers
 *   fn_800A971C big-endian byte-swap of a loaded world header (GC-only fixup)
 *
 * Only a compilable subset of bodies is transcribed; matching is out of scope
 * for this pass (NonMatching, DOL bytes substituted).  See symbols.txt for the
 * authoritative address->name mapping. */

/* --- WORLDINFO scene struct at 0x8028C9A8 (>= 0x180 bytes; +232 obj array,
 *     +324 obj count, +368/+372 psys array/count, +64/+196/+360 roots) --- */
typedef struct WorldObj WorldObj;
struct WorldObj {
    char name[6];      /* +0  strcmp key */
    s16 pyr;           /* +6  packed flags/orientation */
    u8 _pad8[8];       /* +8 */
    u32 flags;         /* +16 */
    u8 _pad20[4];      /* +20 */
    void* next;        /* +24 sibling list link used by WorldObjGetAllFlags */
    float x;           /* +28 */
    float y;           /* +32 */
    float z;           /* +36 */
    void* node;        /* +40 g3d display node */
    s16 child;         /* +44 first-child index (-1 = none) */
    s16 sibling;       /* +46 next-sibling index (-1 = none) */
    u8 _pad48[12];     /* +48 -> struct is 60 (0x3C) bytes */
};

/* streamed file-read handle: +16 status word */
typedef struct FInfo FInfo;
struct FInfo {
    u8 _pad0[16];
    s32 status;   /* +16 */
};

/* g3d display node: +48/+52/+56 position */
typedef struct G3DNode G3DNode;
struct G3DNode {
    u8 _pad0[48];
    float x;   /* +48 */
    float y;   /* +52 */
    float z;   /* +56 */
};

/* animation-tree control block at 0x8028CA8C: +128 finfo, +132 buffer */
typedef struct AtreeCB AtreeCB;
struct AtreeCB {
    u8 _pad0[128];
    void* finfo;  /* +128 */
    void* buf;    /* +132 */
};

/* --- module data (names authoritative in symbols.txt) --- */
extern s32   world_load_state; /* 0x80344D68 */
extern FInfo* atree_finfo;     /* 0x80344D6C (file read handle) */
extern s32   WorldDisplay;     /* 0x80344D70 */
extern s32*   lbl_80344D74; /* saved +24 fields */
extern float* lbl_80344D78; /* saved x/y/z */
extern s32   lbl_80344D80;
extern s32   lbl_80344D84;
extern s32   lbl_80344D88;
extern FInfo* lbl_80344D7C;    /* world file read handle */
extern void* lbl_80344D8C;     /* default parent node */
extern void* world_root1;     /* scene root */
extern void* world_root0;     /* scene root */
extern WorldObj* world_objects; /* current world object array base */
extern void* lbl_80344DA0;
extern void* lbl_80344DA4;
extern void* lbl_80344EB8;     /* default parent node */

typedef struct WorldInfo WorldInfo;
struct WorldInfo {
    u8 _pad0[64];
    s32 f64;         /* +64 */
    u8 _pad68[128];  /* +68 */
    s32 f196;        /* +196 */
    u8 _pad200[28];  /* +200 */
    s32 f228;        /* +228 */
    void* f232;      /* +232 obj array base */
    s32 f236;        /* +236 */
    u8 _pad240[84];  /* +240 */
    s32 f324;        /* +324 obj count */
    u8 _pad328[28];  /* +328 */
    s32 f356;        /* +356 */
    s32 f360;        /* +360 */
    s32 f364;        /* +364 */
    void* f368;      /* +368 psys array */
    s32 f372;        /* +372 psys count */
    u8 _pad376[4];   /* +376 */
    void* f380;      /* +380 */
    void* f384;      /* +384 psys table base */
    s32 f388;        /* +388 psys table count */
};
extern WorldInfo lbl_8028C9A8; /* WORLDINFO scene struct */
extern AtreeCB lbl_8028CA8C;   /* anim-tree control block */
extern u8      lbl_80127D60[]; /* node template / name data (.data) */

/* g3d node API (dolphin/g3d region 0x800BAxxx-0x800BExxx) */
extern void  fn_800BA368();  /* set node flag / show */
extern void  fn_800BA2C4();  /* clear node flag / hide */
extern void  MBRemovePsys(void*); /* free particle system */
extern void  fn_800B7758(void);
extern void  fn_8001267C(void*, void*, s32); /* close/abort file read */
extern void  fn_800B79AC();
extern void  fn_800B8E20(void*, void*);
extern G3DNode* fn_800BB29C(void*, void*, s32); /* redeclared: create child node */
extern void  fn_800BA784(void*, s32, s32);
extern void  ErrorPrintf(const char*, ...);
extern int   strcmp(const char*, const char*);
extern char  lbl_801152A0[]; /* error string */
extern s32   FileSize(void*, const char*);
extern FInfo* StartFileRead(void*, const char*, s32, s32, void*, void*);
extern void* fn_800B8B64(void*, s32, s32, s32, s32);
extern char  lbl_801151D8[]; /* world path/name rodata blob */
extern void* InitWorldInfo(void*);
extern void* AllocMem(s32);
extern s32   mlmMemUsed;
extern void  bulletproof_printf(const char*, ...);
extern void  fn_800BA56C(void*, s32, s32, s32);
extern s32   FileExists(const char*, const char*);
extern s32   BytesFree(void);
extern s32   fn_800B87FC(const char*);
extern char* strcpy(char*, const char*);
extern char  lbl_80115214[]; /* printf fmt */
extern char  lbl_80115230[]; /* printf fmt */
void BGLoadWorldFile(s32* h);
void fn_800A971C(void* p);

/* forward declarations (same TU) */
void* NewWorldObject(WorldObj* obj, WorldObj* parent);
s32   WorldPsysActivate(WorldObj* obj);

static const char lbl_80348798[] = "anim";
static const char lbl_803487A0[] = "worlds";

/* WorldObjGetAllFlags: OR the +16 flag word across the +24 sibling list. */
u32 WorldObjGetAllFlags(WorldObj* o) {
    u32 acc = 0;
    while (o != 0) {
        acc |= o->flags;
        o = (WorldObj*)o->next;
    }
    return acc;
}

/* BGLoadWorldFile: streamed-read progress callback.  status(+16)==2 -> done;
 * otherwise advance the running offset (+4) by the last chunk size (+8). */
void BGLoadWorldFile(s32* h) {
    if (h[4] == 2) {
        return;
    }
    h[1] = h[1] + h[2];
}

/* WorldPsysDeActivate: if the object's psys-active bit (0x00800000) is set,
 * free the particle system, clear that bit and set the "was-active" bit
 * (0x00400000). */
s32 WorldPsysDeActivate(WorldObj* o) {
    if (o->flags & 0x00800000) {
        MBRemovePsys(o->node);
        o->flags &= ~0x00800000;
        o->flags |= 0x00400000;
    }
    return 1;
}

void fn_800A9378(void) {
    fn_800B7758();
}

/* LoadWorldDone: finish a world load - alloc buffers, copy name, report mem. */
s32 LoadWorldDone(char* name) {
    WorldInfo* wi = &lbl_8028C9A8;
    s32 freeBefore;
    s32 memBase;
    s32 size;
    lbl_80344D88 = 0;
    lbl_80344D84 = 0;
    lbl_80344D80 = 0;
    if (name != 0 && FileExists(name, lbl_803487A0) != 0) {
        freeBefore = BytesFree();
        wi->f360 = fn_800B87FC(name);
        lbl_80344D80 += freeBefore - BytesFree();
        memBase = mlmMemUsed;
        bulletproof_printf(lbl_80115214, name, memBase);
        size = FileSize(name, lbl_803487A0);
        lbl_80344DA4 = AllocMem(size);
        lbl_80344D88 += size;
        world_load_state = 0;
        wi->f364 = 0;
        lbl_80344DA0 = 0;
        strcpy((char*)wi, name);
        if (FileExists(name, lbl_80348798) != 0) {
            size = FileSize(name, lbl_80348798);
            wi->f356 = (s32)AllocMem(size);
            lbl_80344D84 += size;
        } else {
            wi->f356 = 0;
        }
        bulletproof_printf(lbl_80115230, mlmMemUsed, (mlmMemUsed - memBase) >> 10);
        return wi->f360;
    } else {
        world_load_state = -1;
        return -1;
    }
}

/* CreateWorldNode: recursively build g3d display nodes for the object tree. */
void CreateWorldNode(WorldObj* base, WorldObj* obj, void* parent) {
    while (1) {
        if (obj->flags & 0x800) {
            obj->node = NewWorldObject(obj, parent);
            if (!(obj->flags & 0x10000000)) {
                WorldPsysActivate(obj);
            }
        } else if (obj->node == (void*)1 || (obj->flags & 0x80000000)) {
            obj->node = NewWorldObject(obj, parent);
            fn_800B8E20(obj->node, obj);
            obj->flags |= 0x80000000;
        } else {
            obj->node = NewWorldObject(obj, parent);
        }
        if (obj->flags & 0x100) {
            obj->flags &= -59;
        }
        if (obj->sibling >= 0) {
            CreateWorldNode(base, &base[obj->sibling], obj);
        }
        if (obj->child < 0) {
            return;
        }
        obj = &base[obj->child];
    }
}

void fn_800A9398(s32 a) {
    fn_800B79AC(a, lbl_8028CA8C.buf);
}

/* fn_800A8E30: reset world module state. */
void fn_800A8E30(void) {
    world_root0 = 0;
    world_root1 = 0;
    lbl_80344DA4 = 0;
    lbl_80344DA0 = 0;
    lbl_8028C9A8.f360 = -1;
    lbl_8028C9A8.f196 = -1;
    lbl_8028C9A8.f228 = 0;
    lbl_8028C9A8.f64 = 0;
    lbl_80344D74 = 0;
    lbl_80344D78 = 0;
}

/* NewWorld: create the two scene-root display nodes. */
void NewWorld(void* parent) {
    if (parent == 0) {
        parent = lbl_80344EB8;
    }
    world_root0 = fn_800BB29C(parent, lbl_80127D60, 1);
    world_root1 = fn_800BB29C(parent, lbl_80127D60, 1);
}

WorldObj* FindWorldObject(WorldObj* node, char* name);

/* FindWORLDOBJ: find an object by name from the current world root. */
WorldObj* FindWORLDOBJ(char* name) {
    WorldObj* node = world_objects;
    while (node != 0) {
        if (strcmp(name, node->name) == 0) {
            break;
        } else {
            if (node->sibling >= 0) {
                WorldObj* r = FindWorldObject(&world_objects[node->sibling], name);
                if (r != 0) {
                    node = r;
                    break;
                }
            }
            if (node->child >= 0) {
                node = &world_objects[node->child];
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
        if (strcmp(name, node->name) == 0)
            return node;
        if (node->sibling >= 0) {
            WorldObj* r = FindWorldObject(&world_objects[node->sibling], name);
            if (r != 0)
                return r;
        }
        if (node->child >= 0) {
            node = &world_objects[node->child];
        } else {
            return 0;
        }
    }
    return 0;
}

static const float lbl_80348778 = 0.0f;

/* WorldRestoreInitState: restore each object's snapshotted init transform. */
void WorldRestoreInitState(void) {
    WorldInfo* wi = &lbl_8028C9A8;
    s32 i;
    if (lbl_80344D74 == 0)
        return;
    if (lbl_80344D78 == 0)
        return;
    for (i = 0; i < wi->f324; i++) {
        WorldObj* o = &((WorldObj*)wi->f232)[i];
        o->flags &= 0xC31FFFFF;
        o->next = (void*)lbl_80344D74[i];
        o->x = lbl_80344D78[i * 3 + 0];
        o->y = lbl_80344D78[i * 3 + 1];
        o->z = lbl_80344D78[i * 3 + 2];
    }
    for (i = 0; i < wi->f372; i++) {
        if (*(s32*)((char*)wi->f380 + i * 160) != 0) {
            *(float*)((char*)wi->f368 + i * 16 + 8) = lbl_80348778;
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
            fn_800BA2C4(world_root0, 2, 0);
        } else {
            fn_800BA368(world_root0, 2, 0);
        }
    }
    if (world_root1 != 0) {
        if (WorldDisplay & 2) {
            fn_800BA2C4(world_root1, 2, 0);
        } else {
            fn_800BA368(world_root1, 2, 0);
        }
    }
}

/* StartWorldLoad: multi-file "worlds"/"anim" load state machine. */
s32 StartWorldLoad(s32 arg) {
    WorldInfo* wi = &lbl_8028C9A8;
    char* buf = lbl_801151D8;
    if (world_load_state < 0)
        return 1;
    switch (world_load_state) {
    case 0:
        if (lbl_80344DA4 != 0) {
            s32 size = FileSize(wi, lbl_803487A0);
            lbl_80344D7C = StartFileRead(wi, lbl_803487A0, 0, size,
                                         lbl_80344DA4, BGLoadWorldFile);
            world_load_state = 1;
        } else {
            world_load_state = 2;
        }
        break;
    case 1:
        if (lbl_80344D7C->status != 0) {
            if (arg == 0)
                fn_800A971C(lbl_80344DA4);
            lbl_80344D7C->status = -1;
            world_load_state = arg ? 100 : 2;
        }
        break;
    case 2:
        if (lbl_80344DA0 != 0) {
            s32 size = FileSize(wi, &buf[36]);
            lbl_80344D7C = StartFileRead(wi, &buf[36], 0, size,
                                         lbl_80344DA0, BGLoadWorldFile);
            world_load_state = 3;
        } else {
            world_load_state = 4;
        }
        break;
    case 3:
        if (lbl_80344D7C->status != 0) {
            if (arg == 0)
                fn_800A971C(lbl_80344DA0);
            lbl_80344D7C->status = -1;
            world_load_state = arg ? 100 : 4;
        }
        break;
    case 4:
        wi->f364 = (s32)fn_800B8B64(&buf[48], 0, wi->f360, wi->f360, 1);
        world_load_state = 5;
        /* fall through */
    case 5:
    default:
        return 1;
    }
    return 0;
}

/* StartLoadWorldAnim: begin reading the "anim" file into atree_finfo. */
s32 StartLoadWorldAnim(void* dir) {
    if (lbl_8028CA8C.finfo != 0) {
        s32 size = FileSize(dir, lbl_80348798);
        atree_finfo = StartFileRead(dir, lbl_80348798, 0, size, lbl_8028CA8C.finfo, BGLoadWorldFile);
        return 1;
    }
    return 0;
}

/* FinishLoadWorldAnim: poll/close the anim-tree read. */
s32 FinishLoadWorldAnim(void) {
    if (atree_finfo->status != 0) {
        atree_finfo->status = -1;
        fn_8001267C(lbl_8028CA8C.finfo, lbl_8028CA8C.buf, -1);
        return 1;
    }
    return 0;
}

/* NewWorldObject: create one object's g3d display node. */
void* NewWorldObject(WorldObj* obj, WorldObj* parent) {
    G3DNode* node;
    void* p;
    void* tmp;
    if (parent == 0)
        p = lbl_80344D8C;
    else
        p = parent->node;
    node = fn_800BB29C(p, 0, 1);
    node->x = obj->x;
    node->y = obj->y;
    node->z = obj->z;
    if (parent != 0) {
        if (!(obj->flags & 0x01001000)) {
            if (parent->flags & 0x01001000) {
                ErrorPrintf(lbl_801152A0);
            }
            obj->x = parent->x + obj->x;
            obj->y = parent->y + obj->y;
            obj->z = parent->z + obj->z;
        }
    }
    tmp = obj->next;
    obj->next = parent;
    fn_800BA368(node, tmp, 0);
    if (parent != 0) {
        if (parent->flags & 0x02000000) {
            obj->flags |= 0x02000000;
        }
    }
    if ((obj->flags & 1) && !(obj->flags & 0x1000)) {
        fn_800BA368(node, 4, 0);
    }
    if (obj->flags & 0x8000) {
        fn_800BA368(node, 1, 0);
    }
    if (obj->flags & 0x400) {
        fn_800BA784(node, -2, 1);
    }
    return node;
}
