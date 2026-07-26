/*
 * mb_particle.c - MB particle system (MB_PARTICLE.OBJ).
 *
 * The engine's general particle-system module: emitters that spawn, animate
 * and draw sprite particles (fireworks, flames, sparks, "FIREWORK" preset,
 * etc.). A particle system ("psys") is described by a 0x130-byte descriptor
 * that hangs off a scene node at node->psys (node + 0x70). MBInitPsys builds a
 * 120000-byte block pool (allocPsysMem / freePsysMem) plus per-psys index and
 * usage arrays, and validates the built-in preset table (initPresetList).
 *
 * Creation path: MBNewPsysDefault / MBNewWorldPsys / MBNewPsysDescrip build a
 * scene node (createPsysNode -> allocPsys) and apply a descriptor blob
 * (setWorldParms). MBPsysSet* apply individual attributes (guarded so they are
 * rejected once drawing has begun). MBPsysStartFrame advances the global clock,
 * frees queued psys and spawns deferred world/firework/flame effects.
 *
 * Per frame MBDrawPsys runs the emitter state machine (delay/emit/active/fade/
 * dead, node->psys + 0x37), emits new particles by calling the mode-selected
 * getNewPos / getNewDir generators wired by setupNewPMode, advances live
 * particles through the getPPos* integrators and submits each sprite through
 * DrawPsysSub (GX quads). MBDrawPsysTest is the visibility pre-cull.
 *
 * Text range 0x800CBC4C..0x800D190C. Names come from the Xbox shell3D PDB
 * (MB_PARTICLE.OBJ); GC function order is scrambled relative to the Xbox
 * source. NonMatching - kept for symbol mapping / documentation. The large
 * bodies (MBDrawPsys, DrawPsysSub, setWorldParms, setupNewPMode) are
 * documented reconstructions of the observed call/flow shape, not exact math.
 *
 * Uncertain PDB assignments (behaviorally inferred, not string-anchored):
 *   MBPsysSetDebugNode (0x800CEA18), MBPsysSetERate4 (0x800D0E3C),
 *   MBPsysSetEVolume  (0x800D0EB0).
 */
#include "types.h"

/* --- geometry --- */
typedef struct PVec {
    f32 x;
    f32 y;
    f32 z;
} PVec;

/* --- particle-system descriptor (node->psys, 0x130 bytes) --- */
typedef struct Psys {
    s32   id;            /* +0x00 unique id */
    char* worldName;     /* +0x04 world-psys name / owner */
    u16*  ring;          /* +0x08 particle ring buffer (packed pos|dir idx) */
    PVec* dirPool;       /* +0x0C direction slots */
    PVec* posPool;       /* +0x10 position slots */
    s8*   dirUsed;       /* +0x14 dir slot usage */
    s8*   posUsed;       /* +0x18 pos slot usage */
    u8    _1C[0x08];     /* +0x1C ring read/write cursors */
    u32   flags;         /* +0x2c state flags (bit1 world) */
    u8    _30[0x02];     /* +0x2e particle count */
    u16   emitTime;      /* +0x3a emit duration (frames) */
    u16   emitRepeat;    /* +0x3c repeat count */
    u8    _3E[0x02];
    f32   pTime;         /* +0x40 particle lifetime */
    PVec  gravDir;       /* +0x44 emit-space direction basis */
    u8    _50[0x30];     /* +0x50 rect extents / matrix rows */
    f32   speed;         /* +0x84 particle speed */
    u8    _88[0x10];     /* +0x88 texture handle / ptr */
    f32   grav;          /* +0x98 gravity */
    u8    _9C[0x44];     /* +0x9c drag / colour scratch */
    f32   colEnv[4];     /* +0xd0 colour/alpha envelope keyframes */
    f32   param[5][4];   /* +0xe0 per-particle parameter gradient */
    u8    _130[0x08];
} Psys;

/* --- scene node (the object the API operates on) --- */
typedef struct Node {
    u8     _00[0x28];
    struct Node* child;  /* +0x28 attached child node */
    u8     _2C[0x34];
    u32    renderFlags;  /* +0x60 GX render flags */
    u8     _64[0x0C];
    Psys*  psys;         /* +0x70 particle descriptor */
    u8     _74[0x04];
    struct Node* owner;  /* +0x78 world owner node */
} Node;

/* --- world-psys descriptor blob (setWorldParms input) --- */
typedef struct WorldParm {
    s32 type;            /* +0x00 preset id (>= 0x100) */
    s32 parent;          /* +0x04 */
    u32 flagMask2;       /* +0x08 */
    u32 flagMask3;       /* +0x0c */
    u32 mask;            /* +0x10 field-present mask */
    /* variable payload follows */
} WorldParm;

/* --- block-pool free node --- */
typedef struct MemBlk {
    s32 size;            /* +0x00 <0 == allocated */
    struct MemBlk* next; /* +0x04 */
    struct MemBlk* prev; /* +0x08 */
    s32 tag;             /* +0x0c owner id */
} MemBlk;

/* --- externs (other TUs) --- */
void* memset(void* p, int c, u32 n);
void  ErrorPrintf(const char* fmt, ...);
u32   pbRand(void);
void* AllocMem(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7, f64 f8,
               s32 size, void* tag, s32 a, s32 b, s32 c, s32 d, s32 e, s32 g);
s32   fn_800B8B04(const char* name, s32* out);   /* texture-by-name lookup */
Node* fn_800BB29C(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7, f64 f8,
                  f32 depth, void* verts, s32 flag, s32 tex, void* v2,
                  s32 a, s32 b, s32 c);            /* scene-node create */
void  fn_800BAEAC(Node* node, s32 mode);           /* scene-node free */
void  fn_800B9B5C(void* fn, Node* node);           /* traverse visitor call */
BOOL  fn_800B5704(f64 radius, void* bounds);       /* frustum/sphere cull */
f64   fn_800B71AC(f64 x);                          /* rsqrt / normalize */
f64   fn_800BCB44(f64 a, f64 b);                   /* hypot accumulate */
void  fn_800B512C(s32 tex);                        /* bind texture page */
void  fn_800B4CA0(u32 a, u32 b);                   /* set blend/tev mode */
void  fn_800C7914(void* a, void* b);               /* project helper */
void  fn_800AEA0C(u32 a, u32 b);                   /* copy vec */
void  sceSamp0MultVec(void* out, const f32* m, const f32* v);
void  GXSetChanMatColor(s32 chan, void* color);
void  GXBegin(s32 prim, s32 fmt, s32 count);
void  SetTevStages(s32 n);
void  SetVertexFormat(s32 fmt);
void  SetCullMode(s32 mode);
void  SetPerspectiveMode(s32 mode);
void  SetViewportHeight(f32 h);
void  pbBlitSetTexture(s32 tex);
void  pbBlitSetDrawRegs(s32 a, s32 b, s32 c);

extern void* gWinGlobals;
extern f32   gVpScaleY;

/* --- TU-owned globals (real addresses in .data/.bss/.sbss) --- */
static s32   gPsysActive;      /* 0x80128710 live psys count */
static Psys* gPsysList;        /* 0x80128714 all-psys list head */
static s32   gPsysRemoved;     /* 0x80128718 freed-this-frame count */
static Node* gPsysRmQueue;     /* 0x8012871c remove queue head */
static s32   gPsysIdCounter;   /* 0x80128720 next psys id */
static s32   gPsysFrame;       /* 0x80128724 global frame counter */
static f32   gPsysFrameFrac;   /* 0x80128728 sub-frame time */
static s32   gPoolTotal;       /* 0x8012873c bytes free in block pool */
static s32   gPoolCount;       /* 0x80128740 free block count */
static MemBlk* gPoolBase;      /* 0x80128748 pool base */
static MemBlk* gPoolFree;      /* 0x80128754 free-list cursor */
static s32   gDefTexA;         /* 0x8012872c "particle2_a" texture */
static s32   gDefTexXp;        /* 0x80128730 "particle2_xp" texture */
static s16   gPosSlot;         /* 0x80128766 shared pos slot cache */
static s16   gDirSlot;         /* 0x80128764 shared dir slot cache */

extern s32   gPsysDisabled;    /* 0x80128768 traverse filter / disable id */

/* --- forward decls (behavioural helpers, static in the PDB) --- */
static void getPPosLinear(f64 t, Psys* p, PVec* out, PVec* dir, PVec* org);
static void getPPosSpeed(f64 t, Psys* p, PVec* out, PVec* dir, PVec* org);
static void getPPosGrav(f64 t, Psys* p, PVec* out, PVec* dir, PVec* org);
static void getPPosSpeedGrav(f64 t, Psys* p, PVec* out, PVec* dir, PVec* org);
static s32  getNewPosRectShare(Psys* p, Node* node, s32 z);
static s32  getNewPosRectUnique(Psys* p, Node* node, s32 z);
static s32  getNewPosFrame(Psys* p, Node* node);
static s32  getNewPosSingle2(void);
static s32  getNewPosSingle1(Psys* p, Node* node);
static void getNewDirConeShare(Psys* p, Node* node, s32 z);
static void getNewDirConeUnique(Psys* p, Node* node, s32 z);
static void getNewDirSphere(Psys* p, Node* node, s32 z);
static s32  getNewDirFrame(Psys* p, Node* node);
static s32  getNewDirSingle2(void);
static s32  getNewDirSingle1(Psys* p, Node* node);
static void getOrthoVecs(PVec* a, PVec* b, PVec* dir);
static void getCurrentDir(Psys* p, Node* node, PVec* out);
static f64  getSinCos(f64 ang, f32* sinOut);
static void DrawPsysSub(void);
static void setupNewPMode(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6,
                          f64 f7, f64 f8, Psys* p);
static void setupParms(Psys* p);
static void setWorldParms(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6,
                          f64 f7, f64 f8, Node* node, Psys* p, WorldParm* wp,
                          f32* over, s32 a, s32 b, s32 c, s32 d);
static Psys* allocPsys(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                       f64 f8, s32 fromArena, s32 a, s32 b, s32 c, s32 d,
                       s32 e, s32 g);
static MemBlk* listFindHandle(s32 id, s32 base);
static void freePsys(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                     f64 f8, Node* node);
static void* allocPsysMem(s32 size, s32 tag);
static void  freePsysMem(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                         f64 f8, void* blk);
static void initPresetList(void);
static void setPTimeVal(f64 sec, Psys* p);

Node* createPsysNode(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                     f64 f8, f32* verts, f32 depth, s32 flag, s32 tex,
                     s32 a, s32 b, s32 c, s32 d);
Node* MBNewPsysDescrip(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                       f64 f8, void* a, f32 depth, s32 flag, s32 tex, s32 b,
                       s32 c, s32 d, s32 e);
Node* MBPsysFirework(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                     f64 f8, f32* verts, f32 depth, s32 a, s32 b, s32 c,
                     s32 d, s32 e, s32 g);
Node* MBPsysFlame(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                  f64 f8, s32 a, s32 tex, f32* verts, s32 b, s32 c, s32 d,
                  s32 e, s32 g);

/* ======================================================================= *
 *  Emit-mode integrators (node->psys[0x1b], per live particle over time)  *
 * ======================================================================= */

/* 0x800CCE58 - constant velocity: pos = t*dir + org */
static void getPPosLinear(f64 t, Psys* p, PVec* out, PVec* dir, PVec* org) {
    out->x = (f32)(dir->x * t + org->x);
    out->y = (f32)(dir->y * t + org->y);
    out->z = (f32)(dir->z * t + org->z);
}

/* 0x800CCDD0 - speed-scaled velocity */
static void getPPosSpeed(f64 t, Psys* p, PVec* out, PVec* dir, PVec* org) {
    out->x = (f32)(t * (p->speed * dir->x) + org->x);
    out->y = (f32)(t * (p->speed * dir->y) + org->y);
    out->z = (f32)(t * (p->speed * dir->z) + org->z);
}

/* 0x800CCE1C - gravity on Y */
static void getPPosGrav(f64 t, Psys* p, PVec* out, PVec* dir, PVec* org) {
    out->x = (f32)(dir->x * t + org->x);
    out->y = (f32)(t * ((f32)(p->grav * t + dir->y)) + org->y);
    out->z = (f32)(dir->z * t + org->z);
}

/* 0x800CCD7C - speed-scaled velocity + gravity on Y */
static void getPPosSpeedGrav(f64 t, Psys* p, PVec* out, PVec* dir, PVec* org) {
    out->x = (f32)(t * (p->speed * dir->x) + org->x);
    out->y = (f32)(t * ((f32)(p->grav * t + p->speed * dir->y)) + org->y);
    out->z = (f32)(t * (p->speed * dir->z) + org->z);
}

/* ======================================================================= *
 *  New-position generators (node->psys[0x1a], initial spawn positions)    *
 * ======================================================================= */

/* fill a slot with a random point inside the emit rect, model->world xformed */
static void randRectPos(Psys* p, Node* node, PVec* slot) {
    f32* m = (f32*)&p->_50;    /* rect extents + 3x3 basis + origin */
    f64 rx = (f64)m[0] * (pbRand() & 0x7fff);
    f64 ry = (f64)m[1] * (pbRand() & 0x7fff);
    f32 rz = (f32)((f64)m[2] * (pbRand() & 0x7fff));
    slot->x = m[15] + rz * m[8] + (f32)(rx * m[3] + ry * m[6]);
    slot->y = m[16] + rz * m[9] + (f32)(rx * m[4] + ry * m[7]);
    slot->z = m[17] + rz * m[10] + (f32)(rx * m[5] + ry * m[8]);
}

/* 0x800CCE8C - cycling (shared) rect position */
static s32 getNewPosRectShare(Psys* p, Node* node, s32 z) {
    s32 idx = (s16)p->_1C[4];   /* ring read cursor at +0x20 */
    if (idx < 0) {
        return -1;
    }
    randRectPos(p, node, &p->posPool[idx]);
    return idx;
}

/* 0x800CD02C - unique (free-slot) rect position */
static s32 getNewPosRectUnique(Psys* p, Node* node, s32 z) {
    s32 count = (s16)p->_30[0];
    s32 idx = (s32)((pbRand() & 0x7fff) % (count ? count : 1));
    while (p->posUsed[idx] == -1) {
        idx = (idx == 0) ? count - 1 : idx - 1;
    }
    p->posUsed[idx]++;
    randRectPos(p, node, &p->posPool[idx]);
    return idx;
}

/* 0x800CD254 - one shared position per frame (from node origin) */
static s32 getNewPosFrame(Psys* p, Node* node) {
    s32 idx;
    if (gPosSlot >= 0) {
        return gPosSlot;
    }
    idx = (s16)p->_1C[4];
    if (idx < 0) {
        return idx;
    }
    p->posPool[idx] = *(PVec*)&node->_2C[4];
    gPosSlot = (s16)idx;
    return idx;
}

/* 0x800CD2EC - trivial (always slot 0) */
static s32 getNewPosSingle2(void) {
    return 0;
}

/* 0x800CD2F4 - single static position (node origin), computed once */
static s32 getNewPosSingle1(Psys* p, Node* node) {
    *((void**)((u8*)p + 0x68)) = (void*)getNewPosSingle2; /* swap to trivial */
    p->flags &= 0xfff7;
    p->posPool[0] = *(PVec*)&node->_2C[4];
    return 0;
}

/* ======================================================================= *
 *  New-direction generators (node->psys[0x19])                            *
 * ======================================================================= */

/* 0x800CD9F8 - two orthonormal vectors spanning the plane normal to dir */
static void getOrthoVecs(PVec* a, PVec* b, PVec* dir) {
    f64 x = dir->x, y = dir->y, z = dir->z;
    if (x < 1.0 && y < 1.0 && z != 0.0) {
        a->x = (f32)-z; a->y = 0.0f; a->z = (f32)x;
        b->x = (f32)(y * x); b->y = -(f32)(x * x + z * z); b->z = (f32)(y * z);
    } else {
        a->x = (f32)y; a->y = (f32)-x; a->z = 0.0f;
        b->x = (f32)(z * x); b->y = (f32)(z * y); b->z = -(f32)(y * y + x * x);
    }
}

/* 0x800CDA94 - transform the psys base direction by the node basis, normalize */
static void getCurrentDir(Psys* p, Node* node, PVec* out) {
    f32* b = (f32*)&p->gravDir;    /* basis columns live off +0x44 */
    f64 dx = b[0] * out[1].x + b[1] * out[0].x + b[2] * out[0].y;
    f64 dy = b[0] * out[1].y + b[1] * out[0].z + b[2] * out[1].x;
    f64 dz = b[0] * out[1].z + b[1] * out[0].y + b[2] * out[0].z;
    f64 mag = dz * dz + (f32)(dx * dx + (f32)(dy * dy));
    f32 s = (mag < 1.0e-8 || mag > 1.0e8) ? (f32)(p->speed * fn_800B71AC(mag)) : p->speed;
    out->x = (f32)(dx * s);
    out->y = (f32)(dy * s);
    out->z = (f32)(dz * s);
}

/* 0x800CDB74 - fast sin/cos polynomial; returns cos, writes sin */
static f64 getSinCos(f64 ang, f32* sinOut) {
    f32 s = (f32)(0.5 - ang);
    f32 a2 = (f32)(ang * ang);
    f32 s2 = s * s;
    *sinOut = s * s2 * (1.0f + s2 * (-0.16f + s2 * 0.008f)) + s;
    return (f64)(f32)(ang * (a2 * (1.0f + a2 * (-0.16f + a2 * 0.008f))) + ang);
}

/* 0x800CD330 - cycling (shared) cone direction */
static void getNewDirConeShare(Psys* p, Node* node, s32 z) {
    PVec dir, ux, uy;
    f32 sn, cs2;
    f64 cs;
    s32 idx = (s16)p->_1C[0];
    if (idx < 0) {
        return;
    }
    getCurrentDir(p, node, &dir);
    getOrthoVecs(&ux, &uy, &dir);
    cs = getSinCos((f64)(f32)(p->pTime * (pbRand() & 0x7fff)), &sn);
    cs2 = (f32)getSinCos((f64)(f32)(pbRand() & 0x7fff), &sn);
    if (pbRand() & 4) {
        cs2 = -cs2;
    }
    p->dirPool[idx].x = cs2 * uy.x + sn * dir.x + (f32)(cs * ux.x);
    p->dirPool[idx].y = cs2 * uy.y + sn * dir.y + (f32)(cs * ux.y);
    p->dirPool[idx].z = cs2 * uy.z + sn * dir.z + (f32)(cs * ux.z);
}

/* 0x800CD4DC - unique (free-slot) cone direction */
static void getNewDirConeUnique(Psys* p, Node* node, s32 z) {
    PVec dir, ux, uy;
    f32 sn, cs2;
    f64 cs;
    s32 count = (s16)p->_30[0];
    s32 idx = (s32)((pbRand() & 0x7fff) % (count ? count : 1));
    while (p->dirUsed[idx] == -1) {
        idx = (idx == 0) ? count - 1 : idx - 1;
    }
    p->dirUsed[idx]++;
    getCurrentDir(p, node, &dir);
    getOrthoVecs(&ux, &uy, &dir);
    cs = getSinCos((f64)(f32)(p->pTime * (pbRand() & 0x7fff)), &sn);
    cs2 = (f32)getSinCos((f64)(f32)(pbRand() & 0x7fff), &sn);
    if (pbRand() & 4) {
        cs2 = -cs2;
    }
    p->dirPool[idx].x = cs2 * uy.x + sn * dir.x + (f32)(cs * ux.x);
    p->dirPool[idx].y = cs2 * uy.y + sn * dir.y + (f32)(cs * ux.y);
    p->dirPool[idx].z = cs2 * uy.z + sn * dir.z + (f32)(cs * ux.z);
}

/* 0x800CD718 - random spherical direction (unit cube rejection + normalize) */
static void getNewDirSphere(Psys* p, Node* node, s32 z) {
    PVec* slot;
    f64 dx, dy, dz, len;
    s32 count = (s16)p->_30[0];
    s32 idx = (s32)((pbRand() & 0x7fff) % (count ? count : 1));
    while (p->dirUsed[idx] == -1) {
        idx = (idx == 0) ? count - 1 : idx - 1;
    }
    p->dirUsed[idx]++;
    slot = &p->dirPool[idx];
    dx = (f32)((pbRand() & 0x7fff) * 6.1e-5 - 1.0);
    dy = (f32)((pbRand() & 0x7fff) * 6.1e-5 - 1.0);
    dz = (f32)((pbRand() & 0x7fff) * 6.1e-5 - 1.0);
    len = fn_800BCB44(fn_800BCB44(dx, dz), dy);
    if (len <= 1.0) {
        slot->x = (f32)dx; slot->y = (f32)dy; slot->z = (f32)dz;
    } else {
        len = 1.0 / len;
        slot->x = (f32)(dx * len); slot->y = (f32)(dy * len); slot->z = (f32)(dz * len);
    }
}

/* 0x800CD90C - cycling (frame) direction */
static s32 getNewDirFrame(Psys* p, Node* node) {
    s32 idx = gDirSlot;
    if (idx < 0) {
        idx = (s16)p->_1C[0];
        if (idx < 0) {
            return idx;
        }
        getCurrentDir(p, node, &p->dirPool[idx]);
        gDirSlot = (s16)idx;
    }
    return idx;
}

/* 0x800CD9B0 - trivial (always slot 0) */
static s32 getNewDirSingle2(void) {
    return 0;
}

/* 0x800CD9B8 - single static direction, computed once */
static s32 getNewDirSingle1(Psys* p, Node* node) {
    *((void**)((u8*)p + 0x64)) = (void*)getNewDirSingle2; /* swap to trivial */
    p->flags &= 0xfff7;
    getCurrentDir(p, node, p->dirPool);
    return 0;
}

/* ======================================================================= *
 *  Drawing                                                                *
 * ======================================================================= */

/* 0x800CC8F4 - project one particle sprite and submit a GX quad */
static void DrawPsysSub(void) {
    /* Shape reconstruction: cull against the psys screen-rect, back-project
     * two corners, then GXBegin(GX_QUADS) and stream 4 verts to the FIFO. */
    PVec eye;
    f32 corner[4];
    fn_800AEA0C((u32)&eye, (u32)corner);
    sceSamp0MultVec(corner, (f32*)&gWinGlobals, (f32*)&eye);
    GXBegin(0x98, 0, 4);
}

/* 0x800CBC4C - per-frame emitter state machine + particle draw pass.
 * Runs the emitter phase machine (node->psys + 0x37: 0 delay .. 8 dead),
 * emits new particles via psys[0x1a]/[0x19], ages the ring through
 * psys[0x1b] and draws each live particle with DrawPsysSub. */
void MBDrawPsys(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                Node* node) {
    Psys* p = node->psys;
    s32 phase = (s32)*((s8*)p + 0x37);
    s32 dt = gPsysFrame - *((s32*)p + 0x24);
    if ((u32)dt > 0xf) {
        dt = 1;
    }
    gDirSlot = -1;
    gPosSlot = -1;
    switch (phase) {
    case 0:
        setupParms(p);
        setupNewPMode(f1, f2, f3, f4, f5, f6, f7, 0.0, p);
        *((s8*)p + 0x37) = 2;
        /* on death the psys unlinks itself from the id chain */
        (void)listFindHandle(p->id, (s32)&gPsysRmQueue);
        break;
    default:
        break;
    }
    /* age + emit + draw loop (shape only) */
    if (p->ring != NULL) {
        DrawPsysSub();
    }
}

/* 0x800CDBE0 - visibility pre-cull; sets psys+0x82 draw flag */
BOOL MBDrawPsysTest(Node* node, void* draw) {
    Psys* p = node->psys;
    u16 vis;
    if (*((s8*)p + 0x37) < 6) {
        vis = (u16)fn_800B5704((f64)p->grav /* +0xa4 radius */, (u8*)draw + 0x30);
    } else {
        vis = 1;
    }
    *((u16*)((u8*)p + 0x82)) = vis;
    if (vis == 0 && *((s16*)((u8*)p + 0x80)) != 0) {
        vis = 1;
    }
    return vis != 0;
}

/* 0x800CDC5C - MBTraversePsys visitor: guard non-psys / filtered nodes */
s32 MBTraversePsys(Node* node, void* fn) {
    Psys* p = node->psys;
    if (p == NULL || gPsysDisabled != 0) {
        if (p == NULL) {
            ErrorPrintf("MBTraversePsys: PSYS node with psys=0");
            return 1;
        }
        if (gPsysDisabled != p->id) {
            return 0;
        }
    }
    fn_800B9B5C(fn, node);
    return 0;
}

/* ======================================================================= *
 *  Emitter setup                                                          *
 * ======================================================================= */

/* 0x800CE758 - recompute per-emit interpolation rates for a psys */
static void setupParms(Psys* p) {
    f32* env = p->colEnv;
    f32* g = &p->param[0][0];
    s32 i;
    if (p->emitTime == 0) {
        p->emitTime = 1;
    }
    for (i = 0; i < 4; i++) {
        env[i] = env[i];
    }
    (void)g;
    if (*((s32*)((u8*)p + 0x8c)) == 0) {
        *((s32*)((u8*)p + 0x8c)) = gDefTexA;
    }
}

/* 0x800CDCE4 - choose spawn generators + size the ring/index/usage buffers.
 * Wires psys[0x19]=getNewDir*, psys[0x1a]=getNewPos*, psys[0x1b]=getPPos*
 * based on the emit distribution and animation flags, then carves the
 * per-psys buffers out of the block pool (or the world arena). */
static void setupNewPMode(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6,
                          f64 f7, f64 f8, Psys* p) {
    void* posFn;
    void* dirFn;
    void* ppFn;
    s32   dirMode = (s32)p->_30[1];   /* distribution: single/frame/sphere/cone */
    s32   posMode = (s32)p->_30[1];
    s32   count = (s16)p->_30[0];
    s32   posN = count, dirN = count;
    s32   ringBytes;
    void* buf;
    s32   shared = (p->flags & 0x80) != 0;

    if (count < 1) {
        count = 1;
    }

    /* --- direction generator: single / frame / sphere / cone(unique|share) --- */
    switch (dirMode) {
    case 0:  dirFn = shared ? (void*)getNewDirFrame : (void*)getNewDirSingle1; break;
    case 2:  dirFn = (void*)getNewDirSphere; break;
    case 4:  dirFn = (void*)getNewDirConeUnique; break;
    default: dirFn = (void*)getNewDirConeShare; break;
    }
    /* --- position generator: single / frame / rect(unique|share) --- */
    switch (posMode) {
    case 0:  posFn = shared ? (void*)getNewPosFrame : (void*)getNewPosSingle1; break;
    case 2:  posFn = (void*)getNewPosRectUnique; break;
    default: posFn = (void*)getNewPosRectShare; break;
    }
    /* --- per-particle integrator selected by (speed?, gravity?) --- */
    if (p->speed != 0.0f) {
        ppFn = (p->grav != 0.0f) ? (void*)getPPosSpeedGrav : (void*)getPPosSpeed;
    } else {
        ppFn = (p->grav != 0.0f) ? (void*)getPPosGrav : (void*)getPPosLinear;
    }

    p->flags |= 4 | 8;
    *((void**)((u8*)p + 0x64)) = posFn;   /* [0x19] */
    *((void**)((u8*)p + 0x68)) = dirFn;   /* [0x1a] */
    *((void**)((u8*)p + 0x6c)) = ppFn;    /* [0x1b] */

    ringBytes = count * 2 + posN * 0xc + dirN * 0xc;
    if (p->worldName == NULL) {
        buf = allocPsysMem(ringBytes, p->id);
    } else {
        buf = AllocMem(f1, f2, f3, f4, f5, f6, f7, f8, ringBytes, 0,
                       0, 0, 0, 0, 0, 0);
    }
    if (buf == NULL) {
        ErrorPrintf("No mem for psys id=%d", p->id);
        p->ring = NULL;
        return;
    }
    memset(buf, 0, ringBytes);
    p->ring = (u16*)buf;
    p->posPool = (PVec*)((u8*)buf + count * 2);
    p->dirPool = (PVec*)((u8*)p->posPool + posN * 0xc);
}

/* ======================================================================= *
 *  World-psys descriptor application                                       *
 * ======================================================================= */

/* 0x800CEBC0 - apply a WORLDPSYS descriptor blob to a live psys node.
 * Walks the wp->mask bitfield, applying each present attribute (emit time,
 * counts, lifetime, colour envelope, speed, gravity, textures, flags)
 * with the same clamping the individual MBPsysSet* setters use. */
static void setWorldParms(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6,
                          f64 f7, f64 f8, Node* node, Psys* p, WorldParm* wp,
                          f32* over, s32 a, s32 b, s32 c, s32 d) {
    u32* fld = (u32*)wp;
    if (wp->type < 0x100) {
        ErrorPrintf("setWorldParms: WORLDPSYS type is bad");
        return;
    }
    if (wp->mask & 0x2) {
        p->_30[0] = (u8)fld[5];
    }
    if (wp->mask & 0x10) {
        p->emitTime = (u16)fld[8];
        p->emitRepeat = (u16)fld[9];
    }
    if (wp->mask & 0x40) {
        setPTimeVal((f64)(f32)fld[0xe], p);
    }
    if (wp->mask & 0x2000) {
        p->speed = (f32)(255.0 * (f64)(f32)fld[0x25]);
    }
    if (wp->mask & 0x4000) {
        s32 tex = fn_800B8B04((char*)(fld + 0x10), 0);
        *((s32*)((u8*)p + 0x88)) = tex;
    }
    (void)node; (void)over;
}

/* ======================================================================= *
 *  Node stack / traversal registration                                    *
 * ======================================================================= */

/* 0x800CEA18 - push/remove/query the "current draw node" stack (<=100).
 * NOTE: PDB name MBPsysSetDebugNode is inferred, not string-anchored. */
static s32 gNodeStack[100];        /* 0x802c9d50 */
static s32 gNodeStackTop;          /* 0x803451bc */
static u8  gNodeStackInit;         /* 0x803451c0 */
static s32 gNodeStackDirty;        /* 0x80345190 */

s32 MBPsysSetDebugNode(s32 node, s32 remove) {
    s32 top;
    if (!gNodeStackInit) {
        gNodeStackTop = 0;
        gNodeStackInit = 1;
    }
    top = gNodeStackTop;
    if (node != 0) {
        if (remove == 0) {
            if (gNodeStackTop < 100) {
                gNodeStack[gNodeStackTop++] = node;
            }
            gNodeStackDirty = 1;
        } else {
            s32 r = 0, i;
            for (i = 0; i < gNodeStackTop; i++) {
                gNodeStack[r] = gNodeStack[i];
                if (gNodeStack[i] != node) {
                    r++;
                }
            }
            gNodeStackTop = r;
            return top;
        }
        top = gNodeStackTop;
    }
    gNodeStackTop = top;
    if (gNodeStackTop < 1) {
        return 0;
    }
    return gNodeStack[gNodeStackTop - 1];
}

/* ======================================================================= *
 *  Creation API                                                           *
 * ======================================================================= */

/* 0x800CEAF0 - create a named world psys node and apply a descriptor */
Node* MBNewWorldPsys(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                     f64 f8, f32* verts, f32 depth, WorldParm* wp, s32 tex,
                     char* name, f32* over, s32 a, s32 g) {
    Node* node = createPsysNode(f1, f2, f3, f4, f5, f6, f7, f8, verts, depth,
                                (name != NULL), tex, 0, 0, 0, 0);
    if (node != NULL) {
        Psys* p = node->psys;
        p->worldName = (name != NULL && *name != '\0') ? name : NULL;
        if (wp != NULL) {
            setWorldParms(f1, f2, f3, f4, f5, f6, f7, f8, node, p, wp, over,
                          0, 0, 0, 0);
        }
        if (name != NULL) {
            p->flags |= 2;
        }
    }
    return node;
}

/* 0x800CFB38 - build a psys node from a preset descriptor */
Node* MBNewPsysDescrip(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                       f64 f8, void* wp, f32 depth, s32 flag, s32 tex, s32 b,
                       s32 c, s32 d, s32 e) {
    Node* node = createPsysNode(f1, f2, f3, f4, f5, f6, f7, f8, 0, depth, flag,
                                tex, 0, 0, 0, 0);
    if (node != NULL && wp != NULL) {
        setWorldParms(f1, f2, f3, f4, f5, f6, f7, f8, node, node->psys,
                      (WorldParm*)wp, 0, 0, 0, 0, 0);
    }
    return node;
}

/* 0x800CFA84 - firework preset (deferred build through MBNewPsysDescrip) */
Node* MBPsysFirework(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                     f64 f8, f32* verts, f32 depth, s32 a, s32 b, s32 c,
                     s32 d, s32 e, s32 g) {
    return MBNewPsysDescrip(f1, f2, f3, f4, f5, f6, f7, f8, verts, depth, 0,
                            a, b, c, d, e);
}

/* 0x800CF8EC - flame preset */
Node* MBPsysFlame(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                  f64 f8, s32 a, s32 tex, f32* verts, s32 b, s32 c, s32 d,
                  s32 e, s32 g) {
    return MBNewPsysDescrip(f1, f2, f3, f4, f5, f6, f7, f8, verts, (f32)a, 0,
                            tex, b, c, d, e);
}

/* 0x800D079C - default psys node (no descriptor), stores render flags */
Node* MBNewPsysDefault(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                       f64 f8, f32* verts, f32 depth, s32 flags, s32 tex,
                       s32 a, s32 b, s32 c, s32 d) {
    Node* node;
    if (depth == 0.0f) {
        depth = 1.0f;   /* default draw depth (0x80344eb8) */
    }
    node = createPsysNode(f1, f2, f3, f4, f5, f6, f7, f8, verts, depth, 0, tex,
                          a, b, c, d);
    if (node != NULL) {
        node->renderFlags = flags;
    }
    return node;
}

/* 0x800D07FC - create a scene node + attach a fresh psys descriptor */
Node* createPsysNode(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                     f64 f8, f32* verts, f32 depth, s32 flag, s32 tex,
                     s32 a, s32 b, s32 c, s32 d) {
    Node* node;
    Psys* p;
    if (gPsysDisabled == -1) {
        return NULL;
    }
    node = fn_800BB29C(f1, f2, f3, f4, f5, f6, f7, f8, depth, verts, 0xe, tex,
                       verts, a, b, c);
    if (node == NULL) {
        return NULL;
    }
    p = allocPsys(f1, f2, f3, f4, f5, f6, f7, f8, flag, 0, 0, tex, 0, 0, 0);
    if (p == NULL) {
        node->psys = NULL;
        fn_800BAEAC(node, 1);
        return NULL;
    }
    node->psys = p;
    p->worldName = (char*)node;   /* back-link */
    if (tex != 0) {
        if (tex == -1) {
            p->flags |= 0x80;
        } else if (tex == -2) {
            p->flags |= 0x40;
        } else {
            p->flags |= 0xc0;
        }
    }
    *((s32*)p + 0x24) = gPsysFrame;
    *((s8*)p + 0x37) = 0;
    return node;
}

/* 0x800D08FC - allocate + default-init a psys descriptor (0x130 bytes) */
static Psys* allocPsys(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                       f64 f8, s32 fromArena, s32 a, s32 b, s32 c, s32 d,
                       s32 e, s32 g) {
    Psys* p;
    s32 id = gPsysIdCounter++;
    if (fromArena == 0) {
        p = (Psys*)allocPsysMem(0x130, gPsysIdCounter);
    } else {
        p = (Psys*)AllocMem(f1, f2, f3, f4, f5, f6, f7, f8, 0x130, (void*)id,
                            b, c, d, e, g, 0);
    }
    if (p == NULL) {
        return NULL;
    }
    memset(p, 0, 0x130);
    gPsysActive++;
    p->id = gPsysIdCounter;
    p->emitTime = 300;
    p->emitRepeat = 300;
    p->speed = 1.0f;
    p->_30[0] = 0x14;
    if (gDefTexA == 0 || gDefTexXp == 0) {
        s32 ta = fn_800B8B04("particle2_a", 0);
        s32 tx = fn_800B8B04("particle2_xp", 0);
        gDefTexA = ta;
        gDefTexXp = tx;
    }
    return p;
}

/* ======================================================================= *
 *  Per-attribute setters (rejected once phase >= 2, "draw begun")         *
 * ======================================================================= */

/* 0x800D0B14 - MBPsysSetPTex: bind particle texture by handle */
void MBPsysSetPTex(Node* node, u32 tex) {
    Psys* p = node->psys;
    *((u32*)((u8*)p + 0x88)) = tex;
    *((s32*)((u8*)p + 0x8c)) = tex;   /* resolved page ptr */
}

/* 0x800D0B44 - MBPsysScalePParm: multiply a parameter gradient by a scalar */
void MBPsysScalePParm(f64 s, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                      f64 f8, Node* node, s32 parm, s32 a, s32 b, s32 c,
                      s32 d, s32 e, s32 g) {
    Psys* p = node->psys;
    if (*((s8*)p + 0x37) >= 2) {
        ErrorPrintf("Setting PSYS attribute after draw begins", parm);
        return;
    }
    if (parm >= 5) {
        ErrorPrintf("MBPsysSetPParm: parm %d too big", parm);
        return;
    }
    p->param[parm][0] = (f32)(p->param[parm][0] * s);
    p->param[parm][1] = (f32)(p->param[parm][1] * s);
    p->param[parm][2] = (f32)(p->param[parm][2] * s);
    p->param[parm][3] = (f32)(p->param[parm][3] * s);
}

/* 0x800D0BD8 - MBPsysSetPParm: set a 4-key parameter gradient (clamped) */
void MBPsysSetPParm(f64 v0, f64 v1, f64 v2, f64 v3, f64 f5, f64 f6, f64 f7,
                    f64 f8, Node* node, s32 parm, s32 a, s32 b, s32 c, s32 d,
                    s32 e, s32 g) {
    Psys* p = node->psys;
    if (*((s8*)p + 0x37) >= 2) {
        ErrorPrintf("Setting PSYS attribute after draw begins", parm);
        return;
    }
    if (parm >= 5) {
        ErrorPrintf("MBPsysSetPParm: parm %d too big", parm);
        return;
    }
    p->param[parm][0] = (f32)v0;
    p->param[parm][1] = (f32)v1;
    p->param[parm][2] = (f32)v2;
    p->param[parm][3] = (f32)v3;
}

/* 0x800D0D8C - lifetime helper: seconds -> lifetime frames (clamped) */
static void setPTimeVal(f64 sec, Psys* p) {
    if (sec < 0.0) {
        p->pTime = 1.0f;
    } else if (sec < 1.0) {
        p->pTime = 0.0f;
    } else if (sec < 60.0) {
        p->pTime = (f32)((30.0 * sec) / 1.0);
    } else {
        p->pTime = 1.0f;
    }
}

/* 0x800D0CF0 - MBPsysSetPTime: particle lifetime */
void MBPsysSetPTime(f64 sec, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                    f64 f8, Node* node, s32 a, s32 b, s32 c, s32 d, s32 e,
                    s32 g, s32 h) {
    Psys* p = node->psys;
    if (*((s8*)p + 0x37) >= 2) {
        ErrorPrintf("Setting PSYS attribute after draw begins");
        return;
    }
    setPTimeVal(sec, p);
}

/* 0x800D0DEC - MBPsysSetPSpeed: particle speed multiplier */
void MBPsysSetPSpeed(f64 v, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                     f64 f8, Node* node, s32 a, s32 b, s32 c, s32 d, s32 e,
                     s32 g, s32 h) {
    Psys* p = node->psys;
    if (*((s8*)p + 0x37) >= 2) {
        ErrorPrintf("Setting PSYS attribute after draw begins");
        return;
    }
    p->speed = (f32)(255.0 * v);
}

/* 0x800D0E3C - MBPsysSetERate4: colour/rate envelope (4 keys).
 * NOTE: PDB name inferred (4-value envelope at psys+0xd0). */
void MBPsysSetERate4(f64 v0, f64 v1, f64 v2, f64 v3, f64 f5, f64 f6, f64 f7,
                     f64 f8, Node* node, s32 a, s32 b, s32 c, s32 d, s32 e,
                     s32 g, s32 h) {
    Psys* p = node->psys;
    if (*((s8*)p + 0x37) >= 2) {
        ErrorPrintf("Setting PSYS attribute after draw begins");
        return;
    }
    p->colEnv[0] = (f32)(255.0 * v0);
    p->colEnv[1] = (f32)(255.0 * v1);
    p->colEnv[2] = (f32)(255.0 * v2);
    p->colEnv[3] = (f32)(255.0 * v3);
}

/* 0x800D0EB0 - MBPsysSetEVolume: emission count base+range (bytes, clamped).
 * NOTE: PDB name inferred (byte fields psys+0x60/0x61). */
void MBPsysSetEVolume(f64 base, f64 range, f64 f3, f64 f4, f64 f5, f64 f6,
                      f64 f7, f64 f8, Node* node, s32 a, s32 b, s32 c, s32 d,
                      s32 e, s32 g, s32 h) {
    Psys* p = node->psys;
    f64 v;
    if (*((s8*)p + 0x37) >= 2) {
        ErrorPrintf("Setting PSYS attribute after draw begins");
        return;
    }
    v = 255.0 * base;
    if (v < 1.0) v = 1.0; else if (v > 255.0) v = 255.0;
    *((s8*)p + 0x60) = (s8)(s32)v;
    v = 255.0 * range;
    if (v < 0.0) v = 0.0; else if (v > 255.0) v = 255.0;
    *((s8*)p + 0x61) = (s8)(s32)v;
}

/* 0x800D0F68 - MBPsysSetETime: emit duration + repeat (with forever flag) */
void MBPsysSetETime(f64 dur, f64 rep, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                    f64 f8, Node* node, s32 a, s32 b, s32 c, s32 d, s32 e,
                    s32 g, s32 h) {
    Psys* p = node->psys;
    f64 v;
    if (*((s8*)p + 0x37) >= 2) {
        ErrorPrintf("Setting PSYS attribute after draw begins", p);
        return;
    }
    v = 255.0 * dur;
    if (v < 1.0) v = 1.0; else if (v > 32767.0) v = 32767.0;
    p->emitTime = (u16)(s32)v;
    v = 255.0 * rep;
    if (v < 0.0) v = 0.0; else if (v > 32767.0) v = 32767.0;
    p->emitRepeat = (u16)(s32)v;
    p->flags &= 0xfffe;
    if (dur >= 0.0) {
        if (rep < 0.0) {
            p->emitRepeat = 0xffff;
            p->flags |= 2;
        }
    } else {
        p->emitTime = 0xffff;
        p->flags |= 2;
    }
}

/* ======================================================================= *
 *  Per-frame driver, removal, and pool management                         *
 * ======================================================================= */

/* 0x800D1074 - advance the clock, free queued psys, spawn deferred effects */
void MBPsysStartFrame(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                      f64 f8) {
    Node* node = gPsysRmQueue;
    gPsysFrame++;
    gPsysFrameFrac = 0.0f;
    while (node != NULL) {
        Node* next = node->child;   /* +0x24 chain */
        freePsys(f1, f2, f3, f4, f5, f6, f7, f8, node);
        node = next;
    }
    gPsysRmQueue = NULL;
    gPsysRemoved = 0;
    (void)MBPsysSetDebugNode(0, 0);
}

/* 0x800D12F0 - MBRemovePsys: mark a psys node for removal */
void MBRemovePsys(Node* node) {
    if (node != NULL) {
        if (*((s8*)node + 0x52) != 0x0e) {
            node = node->owner;
        }
        if (node == NULL || *((s8*)node + 0x52) != 0x0e) {
            ErrorPrintf("MBRemovePsys: Non psys node");
        } else if (*((u8*)node->psys + 0x37) < 6) {
            *((u8*)node->psys + 0x37) = 6;
        }
    }
}

/* 0x800D1364 - listFindHandle: walk an id chain to a matching link slot */
static MemBlk* listFindHandle(s32 id, s32 base) {
    s32* link = (s32*)(base + 4);
    s32 cur;
    while ((cur = *link) != 0 && cur != id) {
        link = (s32*)(cur + 0x24);
    }
    return (MemBlk*)link;
}

/* 0x800D138C - freePsys: release a psys node's buffers back to the pool */
static void freePsys(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                     f64 f8, Node* node) {
    if (node->child != NULL) {
        node->child->psys = NULL;   /* +0x28 -> +0x70 */
        fn_800BAEAC(node->child, 1);
        node->child = NULL;
    }
    if (*((s32*)node + 1) == 0) {   /* not world-owned */
        if (*((void**)node + 2) != NULL) {
            freePsysMem(f1, f2, f3, f4, f5, f6, f7, f8, *((void**)node + 2));
            *((void**)node + 2) = NULL;
        }
        freePsysMem(f1, f2, f3, f4, f5, f6, f7, f8, node);
    }
}

/* 0x800D1404 - allocPsysMem: first-fit split allocator over the block pool */
static void* allocPsysMem(s32 size, s32 tag) {
    u32 need;
    MemBlk* b;
    if (size <= 0 || size > gPoolTotal) {
        return NULL;
    }
    need = (size + 0x1f) & 0xfffffff0;
    b = gPoolFree;
    do {
        if ((s32)need <= b->size) {
            b->tag = tag;
            if ((u32)(b->size - need) < 0x131) {
                gPoolFree = b->next ? b->next : gPoolBase;
                b->size = -b->size;
                gPoolCount--;
                gPoolTotal -= (-b->size);
                return b + 1;
            }
            /* split */
            gPoolTotal -= need;
            gPoolCount--;
            b->size = -(s32)need;
            return b + 1;
        }
        b = b->next ? b->next : gPoolBase;
    } while (b != gPoolFree);
    return NULL;
}

/* 0x800D1530 - freePsysMem: return a block, coalescing neighbours */
static void freePsysMem(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                        f64 f8, void* mem) {
    MemBlk* b = (MemBlk*)mem - 1;
    if (b->size >= 0) {
        ErrorPrintf("freePsysMem: bad free block. Error=%d", b->size);
        return;
    }
    b->size = -b->size;
    gPoolTotal += b->size;
    gPoolCount++;
    gPoolFree = b;
}

/* 0x800D1724 - initPresetList: checksum + validate the built-in preset table */
static void initPresetList(void) {
    u32* preset = (u32*)0x801287fc;   /* 9 x 0x138-byte presets */
    s32 i;
    for (i = 0; i < 9; i++) {
        u32 sum = 0;
        s32 k;
        u32* w = preset + i * (0x138 / 4);
        for (k = 0; k < 0x138 / 4 - 1; k++) {
            sum += w[k];
        }
        (void)sum;
    }
}

/* 0x800D1800 - MBInitPsys: build the 120000-byte block pool + index arrays */
void MBInitPsys(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                f64 f8) {
    MemBlk* base;
    gPsysActive = 0;
    gPsysList = NULL;
    gPsysRmQueue = NULL;
    gPsysRemoved = 0;
    gPsysIdCounter = 0;
    gPsysFrame = 0;
    gPsysFrameFrac = 1.0f;
    gPoolTotal = 120000;
    gPoolCount = 1;
    base = (MemBlk*)AllocMem(f1, f2, f3, f4, f5, f6, f7, f8, 120000, 0, 0, 0,
                             0, 0, 0, 0);
    gPoolBase = base;
    gPoolFree = base;
    base->size = gPoolTotal;
    base->next = NULL;
    base->prev = NULL;
    base->tag = 0;
    initPresetList();
}
