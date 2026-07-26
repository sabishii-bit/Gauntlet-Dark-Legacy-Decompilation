#include "types.h"
#include "game/mbobject.h"

/* Midway "MB" 3D object list / billboard layer (GCN mb_objects.obj region,
 * .text 0x800B90A4-0x800B9E4C). This module maintains the per-frame draw
 * lists for camera-facing "objects" (sprites/billboards attached to model
 * nodes) and the three deferred draw queues used for correct alpha sorting:
 *
 *   - psys queue  (particle-system objects, drawn unsorted)
 *   - dist queue  (alpha objects sorted back-to-front by camera distance)
 *   - sort queue  (alpha objects sorted back-to-front by explicit key)
 *
 * Each queue is a fixed array of MBObjEntry (0x4C bytes: a 4x4 transform, a
 * float sort key, the owning MBObject*, and a blit page). MBSetupObject decides,
 * from an object's flags, whether it draws immediately or is deferred into the
 * dist/sort queue; MBEndFrame later flushes the queues via MBDrawPsysObjects /
 * MBDrawDistObjects / MBDrawSortObjects, each of which qsort()s by CmpDist and
 * hands the run to DrawSortObjectsSub.
 *
 * Status: NonMatching (billboard-matrix helpers + MBSetupObject stubbed).
 */

/* MBObjEntry (0x4C deferred-draw queue entry) and MBObject (the 0x80 mbnode
 * scene-graph node) are defined in include/game/mbobject.h. In this TU the
 * node's OBJECT_NODE payload (+0x70) is a resolved rom object/texture pointer,
 * reached via obj->data.romobj. */

/* ---- externs (resolved via symbols.txt) ---- */
extern void* fn_800BB29C(void* parent, void* name, int count); /* render-node/tree alloc */
extern void FatalError(const char* msg, int code);
extern void ErrorPrintf(const char* fmt, ...);
extern int fn_800B5704(void* p, f32 f);                /* float-triple valid test */
extern void CopyMat4(void* mtx, MBObjEntry* e);     /* fill entry transform */
extern void qsort(void* base, u32 num, u32 size,
                  int (*cmp)(const void*, const void*));
extern void mbBlitGetPage(void);
extern void mbBlitSetPage(void);
extern void pbSendObjTextures(void);                   /* 0x800C3AFC */
extern void MBDrawPsys(void* obj, MBObjEntry* e);     /* GX special dispatch */
extern void fn_800C38C0(MBObjEntry* e, MBObject* obj, int f); /* GX draw object */
extern void fn_800C1148(int a, int b, void* c);        /* debug bbox draw */

/* module data (see symbols.txt) */
extern MBObjEntry mbPsysObjects[256];                /* 0x802A61B8 */
extern MBObjEntry mbDistObjects[256];                /* 0x802AADB8 */
extern MBObjEntry mbSortObjects[256];                /* 0x802AF9B8 */
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
extern u8 lbl_80127D60[0x40];   /* tree ctor argument blob (.data) */

/* shared globals owned by other MB TUs */
extern s32* gWinDebug;               /* 0x80343FB8 : ptr; [0]=hide-objs [1]=... */
extern s32 lbl_80343F9C;              /* 0xE-dispatch enable */
extern s32 lbl_80344E90;              /* debug-bbox enable (DrawSortObjectsSub) */
extern u8* gWinGlobals;               /* 0x80344FC0 : window/model-mgr context */
extern u32 lbl_802A4B30[6];           /* current blit page block (.bss 0x18) */
extern s32 lbl_802C29F8[12];          /* profiler counters (.bss 0x30) */
extern u8 lbl_80116020[0x16];         /* debug bbox arg blob (.rodata) */

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
        lbl_80344EBC = fn_800BB29C(0, lbl_80127D60, 1);
        lbl_80344EB8 = fn_800BB29C(0, lbl_80127D60, 1);
        lbl_80344EB4 = fn_800BB29C(0, lbl_80127D60, 1);
        lbl_80344EB0 = fn_800BB29C(0, lbl_80127D60, 7);
        lbl_80344EAC = fn_800BB29C(0, lbl_80127D60, 8);
        lbl_80344EA8 = fn_800BB29C(0, lbl_80127D60, 8);
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

/* Create a new object node under a parent tree and bind it to a rom object. */
MBObject* MBNewObject(s32 objid, void* name, void* parent, u32 flags) {
    MBObject* obj;

    if (parent == 0) {
        parent = (flags & 0x00002000) ? lbl_80344EBC : lbl_80344EB8;
    }

    if (objid == -1) {
        obj = (MBObject*)fn_800BB29C(parent, name, 1);
    } else {
        obj = (MBObject*)fn_800BB29C(parent, name, 2);
        if (obj != 0) {
            u8* mgr = gWinGlobals;
            if (objid < 0) {
                FatalError(str_BadMBSetObject, 0x800000);
                obj->index = objid;
                obj->data.romobj = 0;
            } else {
                void** table = *(void***)(mgr + 0x30);
                obj->index = objid;
                obj->data.romobj = (u8*)*(void**)((u8*)table[(objid >> 16) * 4 + 1] + 0x54) +
                              ((objid << 6) & 0x003FFFC0);
                obj->type = 2;
                obj->flags &= ~1u;
            }
            obj->flags |= flags;
        }
    }
    return obj;
}

/* Rebind an existing object node to a different rom object. */
void MBSetObject(MBObject* obj, s32 objid) {
    u8* mgr = gWinGlobals;
    if (objid < 0) {
        FatalError(str_BadMBSetObject, 0x800000);
        obj->index = objid;
        obj->data.romobj = 0;
    } else {
        void** table = *(void***)(mgr + 0x30);
        obj->index = objid;
        obj->data.romobj = (u8*)*(void**)((u8*)table[(objid >> 16) * 4 + 1] + 0x54) +
                      ((objid << 6) & 0x003FFFC0);
        obj->type = 2;
        obj->flags &= ~1u;
    }
}

/* =====================================================================
 * Per-object draw decision + setup
 * ===================================================================== */

/* Returns 0 = cull, 1 = draw immediately, 2 = defer to a sort queue. */
int MBDrawObjectTest(MBObject* obj, void* cam, int allowDefer) {
    int cull;
    cull = !fn_800B5704((u8*)cam + 48, *(f32*)((u8*)obj->data.romobj + 4));
    if (gWinDebug[0] != 0 && gWinDebug[1] != 0) {
        cull = 0;
    }
    if (cull) {
        return 0;
    }
    if (obj->flags & 0x00100400) {
        return 2;
    }
    if (allowDefer != 0 && (obj->flags & 0x00000800)) {
        if (obj->alpha != 0 || (obj->flags & 0x40800000) ||
            (*(u32*)((u8*)obj->data.romobj + 8) & 1)) {
            return 2;
        }
    }
    return 1;
}

/* Transform an object into view space and route it to the correct queue. */
void MBSetupObject(MBObject* obj, void* mtx, void* cam) {
    (void)obj;
    (void)mtx;
    (void)cam;
    /* NonMatching: profiler bookkeeping + matrix transform + AddDistObject /
     * AddSortObject routing; stubbed. */
}

/* =====================================================================
 * Billboard matrix helpers (pure math; bodies stubbed)
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
    mbBlitGetPage();
    DrawSortObjectsSub(0, mbPsysObjects, mbNumPsysObjects);
    mbBlitSetPage();
}

void MBDrawDistObjects(void) {
    s32 n = mbNumDistObjects;
    qsort(mbDistObjects, n, sizeof(MBObjEntry), CmpDist);
    DrawSortObjectsSub(0, mbDistObjects, n);
}

void MBDrawSortObjects(void) {
    s32 n = mbNumSortObjects;
    qsort(mbSortObjects, n, sizeof(MBObjEntry), CmpDist);
    DrawSortObjectsSub(0, mbSortObjects, n);
}

/* Draw a run of queue entries, dispatching on each object's drawType. */
static void DrawSortObjectsSub(int start, MBObjEntry* base, int count) {
    int i;
    MBObjEntry* e = &base[start];
    for (i = start; i < count; i++, e++) {
        s8 t = e->obj->type;
        if (t == 12) {
            /* nothing */
        } else if (t == 14) {
            if (lbl_80343F9C != 0) {
                MBDrawPsys(e->obj, e);
            }
        } else if (t == 2) {
            pbSendObjTextures();
            lbl_802A4B30[1] = e->page;
            fn_800C38C0(e, e->obj, 0);
            lbl_802C29F8[7]++;
        }
        if (lbl_80344E90 != 0) {
            fn_800C1148(0, 0, lbl_80116020);
        }
    }
}

/* =====================================================================
 * Deferred-queue insertion
 * ===================================================================== */

int AddPsysObject(void* mtx, MBObject* obj) {
    MBObjEntry* e;
    if (mbNumPsysObjects >= 0xFF) {
        ErrorPrintf(str_TooManyPsys, mbNumPsysObjects);
        return 0;
    }
    e = &mbPsysObjects[mbNumPsysObjects++];
    CopyMat4(mtx, e);
    e->key = 0.0f;
    e->obj = obj;
    e->page = lbl_802A4B30[1];
    return 1;
}

int AddDistObject(void* mtx, MBObject* obj, f32 dist) {
    MBObjEntry* e;
    if (mbNumDistObjects >= 0xFF) {
        ErrorPrintf(str_TooManyAlphaDist, mbNumDistObjects);
        return 0;
    }
    if (dist > 536870912.0f) {
        dist = 536870912.0f;
    }
    e = &mbDistObjects[mbNumDistObjects++];
    CopyMat4(mtx, e);
    e->key = dist + obj->zsort_add;
    e->obj = obj;
    e->page = lbl_802A4B30[1];
    if (e->obj->flags & 0x00400000) {
        e->key *= 1e-05;
    } else if (e->obj->flags & 0x00080000) {
        e->key *= 0.001;
    }
    return 1;
}

int AddSortObject(void* mtx, MBObject* obj, f32 key) {
    MBObjEntry* e;
    if (mbNumSortObjects >= 1023) {
        ErrorPrintf(str_TooManyAlphaSort, mbNumSortObjects);
        return 0;
    }
    if (key > 536870912.0f) {
        key = 536870912.0f;
    }
    e = &mbSortObjects[mbNumSortObjects++];
    CopyMat4(mtx, e);
    e->key = key + obj->zsort_add;
    e->obj = obj;
    e->page = lbl_802A4B30[1];
    if (e->obj->flags & 0x00100400) {
        e->key = e->key - 30000.0;
    } else if (e->obj->flags & 0x00400000) {
        e->key = e->key - 20000.0;
    } else if (e->obj->flags & 0x00080000) {
        e->key = e->key - 10000.0;
    }
    return 1;
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
    if (d > 0.0) return 1;
    if (d < 0.0) return -1;
    return 0;
}
