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
 * Status: NonMatching (all functions translated; FaceCamMat and QuickYawMat
 * retain instruction-scheduling differences).
 */

/* MBObjEntry (0x4C deferred-draw queue entry) and MBObject (the 0x80 mbnode
 * scene-graph node) are defined in include/game/mbobject.h. In this TU the
 * node's OBJECT_NODE payload (+0x70) is a resolved rom object/texture pointer,
 * reached via obj->data.romobj. */

/* ---- externs (resolved via symbols.txt) ---- */
extern void* fn_800BB29C(void* parent, void* name, int count); /* render-node/tree alloc */
extern void FatalError(const char* msg, int code);
extern void ErrorPrintf(const char* fmt, ...);
extern int MBWorldSphereVisible3(void* p, f32 f);       /* mb_camera.c */
extern void CopyMat4(void* mtx, MBObjEntry* e);     /* fill entry transform */
extern void qsort(void* base, u32 num, u32 size,
                  int (*cmp)(const void*, const void*));
extern void mbBlitGetPage(void);
extern void mbBlitSetPage(void);
extern void pbSendObjTextures(MBObject* obj);          /* 0x800C3AFC */
extern void MBDrawPsys(void* obj, MBObjEntry* e);     /* GX special dispatch */
extern void fn_800C38C0(MBObjEntry* e, MBObject* obj, int f); /* GX draw object */
extern void fn_800C1148(int a, int b, void* c);        /* debug bbox draw */
extern f32 NormalVector(f32* vec);
extern void vec4ApplyTrans__FR4vec4R4vec4R5mat44(f32* dst, f32* src,
                                                  f32* mtx);
extern void fn_800BD428(f32* vec, f32* yaw, f32* pitch);
extern void YawMat3(f32* mtx, f32 yaw);
extern void PitchMat3(f32* mtx, f32 pitch);
extern f32 atan2(f32 y, f32 x);
extern f64 __sin(f64 angle);
extern f64 __cos(f64 angle);

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
extern s32 draw_psys_on;              /* 0xE-dispatch enable */
extern s32 lbl_80344E90;              /* debug-bbox enable (DrawSortObjectsSub) */
extern u8* gWinGlobals;               /* 0x80344FC0 : window/model-mgr context */
extern s32 lbl_802A4B30[6];           /* current blit page block (.bss 0x18) */
extern s32 lbl_802C29F8[12];          /* profiler counters (.bss 0x30) */
extern u8 lbl_80116020[0x16];         /* debug bbox arg blob (.rodata) */
extern u8* lbl_80344EE8;
extern f32 lbl_80344E94;
extern f32 lbl_80344E98;
extern f32 lbl_80343EC0;
extern f32 lbl_80343EC4;

extern const char str_BadMBSetObject[];    /* "Bad MBSetObject"          */
extern const char str_TooManyPsys[];       /* "TOO MANY PSYS OBJECTS: %d" */
extern const char str_TooManyAlphaDist[];  /* "TOO MANY ALPHA DIST OBJECTS: %d" */
extern const char str_TooManyAlphaSort[];  /* "TOO MANY ALPHA SORT OBJECTS: %d" */

/* internal helpers (same TU) */
static void DrawSortObjectsSub(int start, MBObjEntry* base, int count);
static int CmpDist(const void* a, const void* b);
int AddDistObject(void* mtx, MBObject* obj, f32 dist);
int AddSortObject(void* mtx, MBObject* obj, f32 key);

/* =====================================================================
 * Object list init / node creation
 * ===================================================================== */

/* Allocate (or clear) the six render trees that objects are attached to. */
static const f32 sZero = 0.0f;

/* Inlined equality helper: both params are opaque locals inside the inlinee,
 * so MWCC keeps the var-first fcmpu operand order the target has (it would
 * canonicalize a visible-const compare const-first). mwld deadstrips the
 * standalone copy. */
static int feq(f32 a, f32 b) {
    return a == b;
}

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

/* Bind an object node to a rom object (shared guts of MBNewObject /
 * MBSetObject; defined before both so -inline auto folds it into each ---
 * the inlinee's param webs are what give MBNewObject its obj=r31/flags=r30
 * coloring; open-coding the body rotates them). */
static void SetObjectGuts(MBObject* obj, s32 objid) {
    u8* mgr = gWinGlobals;
    if (objid < 0) {
        FatalError(str_BadMBSetObject, 0x800000);
        obj->index = objid;
        obj->data.romobj = 0;
    } else {
        void** table = *(void***)(mgr + 0x30);
        table = (void**)table[(objid >> 16) * 4 + 1];
        obj->index = objid;
        obj->data.romobj = (u8*)*(void**)((u8*)table + 0x54) +
                      ((objid << 6) & 0x003FFFC0);
        obj->type = 2;
        obj->flags &= ~1u;
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
            SetObjectGuts(obj, objid);
            obj->flags |= flags;
        }
    }
    return obj;
}

/* Rebind an existing object node to a different rom object. */
void MBSetObject(MBObject* obj, s32 objid) {
    SetObjectGuts(obj, objid);
}

/* =====================================================================
 * Per-object draw decision + setup
 * ===================================================================== */

/* Returns 0 = cull, 1 = draw immediately, 2 = defer to a sort queue. */
int MBDrawObjectTest(MBObject* obj, void* cam, int allowDefer) {
    int cull;
    cull = !MBWorldSphereVisible3((u8*)cam + 48, *(f32*)((u8*)obj->data.romobj + 4));
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
void MBSetupObject(MBObject* obj, MBObjEntry* entry, int allowDefer,
                   f32 sortOverride, f32 zadd) {
    f32 fade;
    f32 key;

    lbl_802C29F8[0]++;
    lbl_802C29F8[5]++;
    if (lbl_802A4B30[1] == 0) {
        lbl_802C29F8[6]++;
    }
    if ((obj->flags & 0x00100400) != 0) {
        f32 transformed[4];

        vec4ApplyTrans__FR4vec4R4vec4R5mat44(
            transformed, &entry->mtx[12],
            (f32*)(*(u8**)(gWinGlobals + 4) + 0x2C0));
        if ((obj->flags & 0x00100000) != 0) {
            fade = (f32)(transformed[3] * (255.0 / lbl_80343EC4));
        } else {
            fade = (f32)(transformed[3] * (255.0 / lbl_80343EC0));
        }
        if (fade < 255.0) {
            if (fade < 0.0) {
                obj->alpha = 0;
            } else {
                obj->alpha = (u8)(s32)fade;
            }
            AddDistObject(entry, obj, transformed[3] + zadd);
            return;
        }
        obj->alpha = 0xFF;
    }
    if (allowDefer != 0 && (obj->flags & 0x800) != 0 &&
        (obj->alpha != 0 || (obj->flags & 0x40800000) != 0 ||
         (*(u32*)((u8*)obj->data.romobj + 8) & 1) != 0)) {
        f32 sortTransformed[4];

        vec4ApplyTrans__FR4vec4R4vec4R5mat44(
            sortTransformed, &entry->mtx[12],
            (f32*)(*(u8**)(gWinGlobals + 4) + 0x2C0));
        if (0.0 == sortOverride) {
            key = sortTransformed[3];
        } else {
            key = sortOverride;
        }
        AddSortObject(entry, obj, key + zadd);
    } else {
        pbSendObjTextures(obj);
        fn_800C38C0(entry, obj, 0);
    }
}

/* =====================================================================
 * Billboard matrix helpers (pure math; bodies stubbed)
 * ===================================================================== */

void TopFaceMat(f32* mtx) {
    u8 pad[16];
    f32 side[3];
    f32 dx = *(f32*)(lbl_80344EE8 + 0x94) - mtx[12];
    f32 dy = *(f32*)(lbl_80344EE8 + 0x98) - mtx[13];
    f32 dz = *(f32*)(lbl_80344EE8 + 0x9C) - mtx[14];
    f32 length;

    side[0] = dy * mtx[10] - dz * mtx[9];
    side[1] = dz * mtx[8] - dx * mtx[10];
    side[2] = dx * mtx[9] - dy * mtx[8];
    length = NormalVector(side);
    if (length < 0.01) {
        mtx[4] = sZero;
        mtx[5] = 1.0f;
        mtx[6] = sZero;
        mtx[0] = mtx[5] * mtx[10] - mtx[6] * mtx[9];
        mtx[1] = mtx[6] * mtx[8] - mtx[4] * mtx[10];
        mtx[2] = mtx[4] * mtx[9] - mtx[5] * mtx[8];
    } else {
        mtx[0] = side[0];
        mtx[1] = side[1];
        mtx[2] = side[2];
        mtx[4] = mtx[9] * mtx[2] - mtx[10] * mtx[1];
        mtx[5] = mtx[10] * mtx[0] - mtx[8] * mtx[2];
        mtx[6] = mtx[8] * mtx[1] - mtx[9] * mtx[0];
    }
}

void FaceCamMat(f32* mtx, f32 limit) {
    f32 toCamera[3];
    f32 yaw;
    f32 pitch;
    f32 hole;
    f32 cameraYaw;
    f32 cameraPitch;
    u8 pad[8];

    toCamera[0] = *(f32*)(lbl_80344EE8 + 0x94) - mtx[12];
    toCamera[1] = *(f32*)(lbl_80344EE8 + 0x98) - mtx[13];
    toCamera[2] = *(f32*)(lbl_80344EE8 + 0x9C) - mtx[14];
    if (feq(limit, sZero)) {
        goto no_pitch;
    } else {
        fn_800BD428(&mtx[8], &yaw, &pitch);
        fn_800BD428(toCamera, &cameraYaw, &cameraPitch);
        cameraYaw -= yaw;
        cameraPitch -= pitch;
        if (limit > sZero) {
            if (cameraPitch > limit) {
                cameraPitch = limit;
            } else if (cameraPitch < -limit) {
                cameraPitch = -limit;
            }
        }
        {
            f32 a = cameraYaw;
            YawMat3(mtx, a);
        }
        {
            f32 b = cameraPitch;
            PitchMat3(mtx, b);
        }
        return;
    }

no_pitch:
    {
        f32 mz = mtx[10];
        yaw = atan2(mtx[8], mz);
        cameraYaw = atan2(toCamera[0], toCamera[2]) - yaw;
        YawMat3(mtx, cameraYaw);
    }
}

void InitFrontFaceYaw(f32* cam) {
    f32 pitch;
    f32 yaw;

    fn_800BD428(cam, &yaw, &pitch);
    lbl_80344E98 = (f32)__sin(3.141592654 + yaw);
    lbl_80344E94 = (f32)__cos(3.141592654 + yaw);
}

void QuickYawMat(f32* mtx) {
    u8 pad[16];
    f32 mz = mtx[10];
    f32 objectYaw = atan2(mtx[8], mz);
    f32 cz = *(f32*)(lbl_80344EE8 + 0x8C);
    f32 cameraYaw = atan2(*(f32*)(lbl_80344EE8 + 0x84), cz);
    f32 yaw = (f32)(3.141592654 + cameraYaw) - objectYaw;
    f64 result;

    if (yaw > 3.141592654) {
        result = yaw - 6.283185308;
    } else if (yaw <= -3.141592654) {
        result = 6.283185308 + yaw;
    } else {
        result = yaw;
    }
    {
        f32 fy = (f32)result;
        YawMat3(mtx, fy);
    }
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
            if (draw_psys_on != 0) {
                MBDrawPsys(e->obj, e);
            }
        } else if (t == 2) {
            pbSendObjTextures(e->obj);
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
    e->key = sZero;
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
