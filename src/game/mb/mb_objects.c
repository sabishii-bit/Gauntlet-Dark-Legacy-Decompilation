#include "types.h"

/* Midway "MB" 3D object list / billboard layer (GCN mb_objects.obj region,
 * .text 0x800B90A4-0x800B9E4C). This module maintains the per-frame draw
 * lists for camera-facing "objects" (sprites/billboards attached to model
 * nodes) and the three deferred draw queues used for correct alpha sorting:
 *
 *   - psys queue  (particle-system objects, drawn unsorted)
 *   - dist queue  (alpha objects sorted back-to-front by camera distance)
 *   - sort queue  (alpha objects sorted back-to-front by explicit key)
 *
 * Each queue is a fixed array of 256 entries (MBObjEntry, 0x4C bytes:
 * a 4x4 transform, a float sort key, the owning MBObject*, and a blit page).
 * MBSetupObject decides, from an object's flags, whether it draws immediately
 * or is deferred into the dist/sort queue; MBEndFrame later flushes the queues
 * via MBDrawPsysObjects / MBDrawDistObjects / MBDrawSortObjects, each of which
 * qsort()s by CmpDist and hands the run to DrawSortObjectsSub.
 *
 * Function names are from shell3D.pdb (mb_objects.obj). Reverse Xbox source
 * order maps cleanly here; the four billboard-matrix helpers (TopFaceMat,
 * FaceCamMat, InitFrontFaceYaw, QuickYawMat) are assigned by that reverse
 * order and are the only tentative names -- all four are pure-math and are
 * stubbed below. Behaviourally-anchored names (string / flag / queue-count
 * evidence) are firm: MBInitObjects, MBNewObject, MBSetObject,
 * MBDrawObjectTest, MBSetupObject, MBDraw{Psys,Dist,Sort}Objects,
 * DrawSortObjectsSub, Add{Psys,Dist,Sort}Object, InitSortObjects, CmpDist.
 *
 * Status: NonMatching (documentation slice; symbols mapped in symbols.txt,
 * DOL still built from the original bytes). Queue-management bodies are
 * reconstructed; the billboard-matrix helpers and the low-level GX draw
 * dispatch (via fn_800C3AFC/fn_800C38C0/fn_800CBC4C) are stubbed.
 */

#pragma dont_inline on

#define MB_OBJ_QUEUE_MAX 256

/* One deferred-draw queue entry. Size 0x4C. */
typedef struct MBObjEntry {
    /* 0x00 */ f32 mtx[16];        /* transform (4x4) */
    /* 0x40 */ f32 key;            /* sort key: camera distance / explicit z */
    /* 0x44 */ struct MBObject* obj;
    /* 0x48 */ u32 page;           /* blit / texture page */
} MBObjEntry;

/* A drawable object node. Only the touched fields are named. */
typedef struct MBObject {
    /* 0x00 */ u8 _pad0[0x52];
    /* 0x52 */ s8 drawType;        /* 2 = normal, 0xC/0xE = special dispatch */
    /* 0x53 */ u8 flag53;
    /* 0x54 */ u8 _pad54[0x0C];
    /* 0x60 */ u32 flags;          /* draw/category flag bits */
    /* 0x64 */ u8 _pad64[0x08];
    /* 0x6C */ s32 objid;          /* rom object index, or -1 */
    /* 0x70 */ void* romTex;       /* resolved rom texture pointer */
} MBObject;

/* ---- externs (resolved via symbols.txt) ---- */
extern void* fn_800BB29C(int a, void* name, int b);   /* render-node/tree alloc */
extern void FatalError(const char* msg, int code);
extern void ErrorPrintf(const char* fmt, ...);
extern int fn_800B5704(void* v);                      /* float-triple valid test */
extern void fn_800BE8F4(MBObject* obj, MBObjEntry* e); /* fill entry transform */
extern void qsort(void* base, u32 num, u32 size,
                  int (*cmp)(const void*, const void*));
extern void* mbBlitGetPage(void);
extern void mbBlitSetPage(void* p);
extern void fn_800CBC4C(int a, MBObjEntry* e);         /* GX special dispatch */
extern void fn_800C3AFC(void);                          /* GX draw begin */
extern void fn_800C38C0(MBObjEntry* e, MBObject* obj, int f); /* GX draw object */
extern void fn_800C1148(int a, int b, void* c);         /* debug bbox draw */

/* module data (see symbols.txt) */
extern MBObjEntry mbPsysObjects[MB_OBJ_QUEUE_MAX];   /* 0x802A61B8 */
extern MBObjEntry mbDistObjects[MB_OBJ_QUEUE_MAX];   /* 0x802AADB8 */
extern MBObjEntry mbSortObjects[MB_OBJ_QUEUE_MAX];   /* 0x802AF9B8 */
extern s32 mbNumPsysObjects;                          /* 0x80344E9C */
extern s32 mbNumDistObjects;                          /* 0x80344EA0 */
extern s32 mbNumSortObjects;                          /* 0x80344EA4 */

/* mb_objects render trees (purpose unconfirmed -> kept as lbl_ names) */
extern void* lbl_80344EBC;   /* tree A (flag bit 18 clear) */
extern void* lbl_80344EB8;   /* tree B (flag bit 18 set)   */
extern void* lbl_80344EB4;
extern void* lbl_80344EB0;
extern void* lbl_80344EAC;
extern void* lbl_80344EA8;
extern void* lbl_80127D60;   /* tree ctor argument blob */

/* shared globals owned by other MB TUs */
extern u32 gWinDebug;                 /* 0x80343FB8 : [0]=hide-objs [1]=... */
extern s32 lbl_80343F9C;              /* draw-bbox enable */
extern s32 lbl_80344E90;              /* debug flag (DrawSortObjectsSub) */
extern u8* gWinGlobals;               /* 0x80344FC0 : window/model-mgr context */
extern u32 lbl_802A4B30;             /* current blit page block */

extern const char str_BadMBSetObject[];    /* "Bad MBSetObject"          */
extern const char str_TooManyPsys[];       /* "TOO MANY PSYS OBJECTS: %d" */
extern const char str_TooManyAlphaDist[];  /* "TOO MANY ALPHA DIST OBJECTS: %d" */
extern const char str_TooManyAlphaSort[];  /* "TOO MANY ALPHA SORT OBJECTS: %d" */

/* internal helpers (same TU) */
static void DrawSortObjectsSub(int start, MBObjEntry* base, int count);
static int CmpDist(const void* a, const void* b);

/* =====================================================================
 * Object list init / node creation
 * ===================================================================== */

/* Allocate (or clear) the six render trees that objects are attached to. */
void MBInitObjects(int enable) {
    if (enable) {
        lbl_80344EBC = fn_800BB29C(0, &lbl_80127D60, 1);
        lbl_80344EB8 = fn_800BB29C(0, &lbl_80127D60, 1);
        lbl_80344EB4 = fn_800BB29C(0, &lbl_80127D60, 1);
        lbl_80344EB0 = fn_800BB29C(0, &lbl_80127D60, 7);
        lbl_80344EAC = fn_800BB29C(0, &lbl_80127D60, 8);
        lbl_80344EA8 = fn_800BB29C(0, &lbl_80127D60, 8);
        *(u32*)((u8*)lbl_80344EB8 + 0x60) |= 4;
        *(u32*)((u8*)lbl_80344EBC + 0x60) |= 4;
        *(u32*)((u8*)lbl_80344EB0 + 0x60) |= 4;
        *(u32*)((u8*)lbl_80344EAC + 0x60) |= 4;
        *(u32*)((u8*)lbl_80344EA8 + 0x60) |= 4;
    } else {
        lbl_80344EBC = 0;
        lbl_80344EB8 = 0;
        lbl_80344EB4 = 0;
        lbl_80344EB0 = 0;
        lbl_80344EAC = 0;
        lbl_80344EA8 = 0;
    }
}

/* Resolve an objid to its rom texture pointer via the model manager. */
static void* MBRomTex(s32 objid) {
    u8* mgr = gWinGlobals;
    void** table;
    if (objid < 0) {
        FatalError(str_BadMBSetObject, 0x800000);
        return 0;
    }
    table = *(void***)(mgr + 0x30);
    table = (void**)((u8*)table[(objid >> 16) * 4 + 1] + 0x54);
    return (u8*)table + (((u32)objid << 6) & 0x00FFFFC0);
}

/* Create a new object node in tree A/B (by flag bit 18) and bind it. */
MBObject* MBNewObject(s32 objid, int unused, int isTemp, u32 flags) {
    void* tree;
    MBObject* obj;

    if (isTemp) {
        tree = (flags & 0x00002000) ? lbl_80344EB8 : lbl_80344EBC;
    } else {
        tree = 0;
    }

    if (objid == -1) {
        obj = (MBObject*)fn_800BB29C((int)tree, &lbl_80127D60, 1);
    } else {
        obj = (MBObject*)fn_800BB29C((int)tree, &lbl_80127D60, 2);
        if (obj != 0) {
            if (objid < 0) {
                FatalError(str_BadMBSetObject, 0x800000);
                obj->objid = objid;
                obj->romTex = 0;
            } else {
                obj->objid = objid;
                obj->romTex = MBRomTex(objid);
                obj->drawType = 2;
                obj->flags &= ~1u;
            }
            obj->flags |= flags;
        }
    }
    return obj;
}

/* Rebind an existing object node to a different rom object. */
void MBSetObject(MBObject* obj, s32 objid) {
    if (objid < 0) {
        FatalError(str_BadMBSetObject, 0x800000);
        obj->objid = objid;
        obj->romTex = 0;
    } else {
        obj->objid = objid;
        obj->romTex = MBRomTex(objid);
        obj->drawType = 2;
        obj->flags &= ~1u;
    }
}

/* =====================================================================
 * Per-object draw decision + setup
 * ===================================================================== */

/* Returns 0 = cull, 1 = draw immediately, 2 = defer to a sort queue. */
int MBDrawObjectTest(MBObject* obj, void* cam, int allowDefer) {
    int valid;
    valid = fn_800B5704((u8*)obj->romTex + 4);
    if (((u32*)&gWinDebug)[0] != 0 && ((u32*)&gWinDebug)[1] != 0) {
        valid = 0;
    }
    if (valid == 0) {
        return 0;
    }
    if (obj->flags & 0x00100400) {
        return 2;
    }
    if (allowDefer != 0 && (obj->flags & 0x00000800)) {
        if (obj->flag53 == 0 && !(obj->flags & 0x40800000) &&
            (((u8*)obj->romTex)[8] & 1)) {
            return 2;
        }
        return 2;
    }
    return 1;
}

/* Transform an object into view space, bump the frame profiler counters,
 * and route it to the correct deferred queue (or draw path). */
void MBSetupObject(MBObject* obj, void* mtx, void* cam) {
    (void)obj;
    (void)mtx;
    (void)cam;
    /* NonMatching: profiler bookkeeping + matrix transform + AddDistObject /
     * AddSortObject routing; body reconstructed from bytes, stubbed here. */
}

/* =====================================================================
 * Billboard matrix helpers (pure math; names tentative, bodies stubbed)
 * ===================================================================== */

void TopFaceMat(void* dst, void* src, void* cam) {
    (void)dst; (void)src; (void)cam;
}

void FaceCamMat(void* dst, void* src, void* cam) {
    (void)dst; (void)src; (void)cam;
}

void InitFrontFaceYaw(void* cam) {
    (void)cam;
}

void QuickYawMat(void* dst, f32 yaw) {
    (void)dst; (void)yaw;
}

/* =====================================================================
 * Deferred-queue flush (called from MBEndFrame)
 * ===================================================================== */

void MBDrawPsysObjects(void) {
    void* page = mbBlitGetPage();
    DrawSortObjectsSub(0, mbPsysObjects, mbNumPsysObjects);
    mbBlitSetPage(page);
}

void MBDrawDistObjects(void) {
    qsort(mbDistObjects, mbNumDistObjects, sizeof(MBObjEntry), CmpDist);
    DrawSortObjectsSub(0, mbDistObjects, mbNumDistObjects);
}

void MBDrawSortObjects(void) {
    qsort(mbSortObjects, mbNumSortObjects, sizeof(MBObjEntry), CmpDist);
    DrawSortObjectsSub(0, mbSortObjects, mbNumSortObjects);
}

/* Draw a run of queue entries, dispatching on each object's drawType. */
static void DrawSortObjectsSub(int start, MBObjEntry* base, int count) {
    int i;
    MBObjEntry* e = base;
    for (i = start; i < count; i++, e++) {
        s8 t = e->obj->drawType;
        if (t == 0xC) {
            /* nothing */
        } else if (t == 0xE) {
            if (lbl_80344E90 != 0) {
                fn_800CBC4C(0, e);
            }
        } else if (t == 2) {
            fn_800C3AFC();
            lbl_802A4B30 = e->page;
            fn_800C38C0(e, e->obj, 0);
        }
        if (lbl_80343F9C != 0) {
            fn_800C1148(0, 0, 0);
        }
    }
}

/* =====================================================================
 * Deferred-queue insertion
 * ===================================================================== */

void AddPsysObject(void* mtx, MBObject* obj) {
    MBObjEntry* e;
    if (mbNumPsysObjects >= 0xFF) {
        ErrorPrintf(str_TooManyPsys, mbNumPsysObjects);
        return;
    }
    e = &mbPsysObjects[mbNumPsysObjects++];
    fn_800BE8F4(obj, e);
    e->key = 1.0f;
    e->obj = obj;
    e->page = *((u32*)&lbl_802A4B30 + 1);
}

void AddDistObject(f32 dist, MBObject* obj) {
    MBObjEntry* e;
    if (mbNumDistObjects >= 0xFF) {
        ErrorPrintf(str_TooManyAlphaDist, mbNumDistObjects);
        return;
    }
    e = &mbDistObjects[mbNumDistObjects++];
    fn_800BE8F4(obj, e);
    e->key = dist;
    e->obj = obj;
    e->page = *((u32*)&lbl_802A4B30 + 1);
}

void AddSortObject(f32 key, MBObject* obj) {
    MBObjEntry* e;
    if (mbNumSortObjects >= 0xFF) {
        ErrorPrintf(str_TooManyAlphaSort, mbNumSortObjects);
        return;
    }
    e = &mbSortObjects[mbNumSortObjects++];
    fn_800BE8F4(obj, e);
    e->key = key;
    e->obj = obj;
    e->page = *((u32*)&lbl_802A4B30 + 1);
}

/* Reset the three deferred queues at the start of a frame. */
void InitSortObjects(void) {
    mbNumSortObjects = 0;
    mbNumDistObjects = 0;
    mbNumPsysObjects = 0;
}

/* qsort comparator: back-to-front by key. */
static int CmpDist(const void* a, const void* b) {
    f32 d = ((const MBObjEntry*)b)->key - ((const MBObjEntry*)a)->key;
    if (d < 0.0f) return -1;
    if (d > 0.0f) return 1;
    return 0;
}
