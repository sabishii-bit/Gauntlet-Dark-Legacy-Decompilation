/*
 * mb_particle.c - MB particle system (MB_PARTICLE.OBJ).
 *
 * The engine's general particle-system module: emitters that spawn, animate
 * and draw sprite particles (fireworks, flames, sparks, "FIREWORK" preset,
 * etc.). A particle system ("psys") is described by a 0x130-byte Psys
 * descriptor that hangs off a scene node at mbnode->data.psys (mbnode + 0x70).
 * MBInitPsys builds a 120000-byte block pool (allocPsysMem / freePsysMem) plus
 * per-psys index and usage arrays, and validates the built-in preset table
 * (initPresetList).
 *
 * Creation path: MBNewPsysDefault / MBNewWorldPsys / MBNewPsysDescrip build a
 * scene node (createPsysNode -> allocPsys) and apply a descriptor blob
 * (setWorldParms). MBPsysSet* apply individual attributes (guarded so they are
 * rejected once emitting has begun: e_phase > 1). MBPsysStartFrame advances the
 * global clock, frees queued psys and spawns deferred world/firework/flame
 * effects.
 *
 * Per frame MBDrawPsys runs the emitter state machine (delay/emit/active/fade/
 * dead, psys->e_phase @ 0x37), emits new particles by calling the mode-selected
 * pos_func / dir_func generators wired by setupNewPMode, advances live
 * particles through the ppos_func integrators and submits each sprite through
 * DrawPsysSub (GX quads). MBDrawPsysTest is the visibility pre-cull.
 *
 * Structs come from include/game/psys.h (Psys/PsysParm/PsysDescrip/PsysMem*)
 * and include/game/mbobject.h (MBObject scene node), both offset-verified from
 * the Xbox shell3D PDB against this TU's GC asm.
 *
 * Text range 0x800CBC4C..0x800D190C. GC function order is scrambled relative to
 * the Xbox source. The small integrators / generators / setters are exact
 * reconstructions from the target asm (many byte-matching); the large bodies
 * (MBDrawPsys, DrawPsysSub, setWorldParms, setupNewPMode, MBNewPsysDescrip) are
 * documented reconstructions of the observed call/flow shape, not exact math.
 * Functions that touch the module-global block are NonMatching on the global
 * access alone (target reaches them through a pooled absolute base at
 * 0x80128710; our statics land in the SDA).
 *
 * Uncertain PDB assignments (behaviorally inferred, not string-anchored):
 *   MBPsysSetDebugNode (0x800CEA18), MBPsysSetERate4 (0x800D0E3C),
 *   MBPsysSetEVolume  (0x800D0EB0).
 */
#include "types.h"
#include "game/psys.h"
#include "game/mbobject.h"

/* --- externs (other TUs) --- */
void* memset(void* p, int c, u32 n);
void  ErrorPrintf(const char* fmt, ...);
u32   pbRand(void);
void* AllocMem(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7, f64 f8,
               s32 size, void* tag, s32 a, s32 b, s32 c, s32 d, s32 e, s32 g);
s32   MBOX_FindTexture(const char* name, s32* out);   /* texture-by-name lookup */
MBObject* MBNewNode(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                      f64 f8, f32 depth, void* verts, s32 flag, s32 tex,
                      void* v2, s32 a, s32 b, s32 c);      /* scene-node create */
void  MBRemoveNode(MBObject* node, s32 mode);              /* scene-node free */
int   AddPsysObject(void* fn, MBObject* node);            /* traverse visitor */
BOOL  MBWorldSphereVisible3(f64 radius, void* bounds);              /* frustum/sphere cull */
f32   mbInvSqrtLookup(f64 x);                                 /* rsqrt / normalize */
f64   fqdist(f64 a, f64 b);                          /* hypot accumulate */
void  pbBlitSetTexture(s32 tex);                               /* bind texture page */
void  pbBlitSetDrawRegs(u32 a, u32 b);                          /* set blend/tev mode */
void  fn_800C7914(void* a, void* b);                      /* project helper */
void  __as__4vec3FRC4vec3(u32 a, u32 b);                          /* copy vec */
void  sceSamp0MultVec(void* out, const f32* m, const f32* v);
void  GXSetChanMatColor(s32 chan, void* color);
void  GXBegin(s32 prim, s32 fmt, s32 count);
void  SetMultiPassTextureParams(s32 n);
void  SetVertexFormat(s32 fmt);
void  SetCullMode(s32 mode);
void  SetPerspectiveMode(s32 mode);
void  SetViewportHeight(f32 h);
void  pbBlitSetTexture(s32 tex);

extern void* gWinGlobals;
extern f32   gVpScaleY;
extern f32   psysInfo[];     /* per-parm scale/min/max config table */

/* --- TU-owned globals (real addresses in .data/.bss/.sbss) --- */
static s32       gPsysActive;      /* 0x80128710 live psys count */
static Psys*     gPsysList;        /* 0x80128714 all-psys list head */
static s32       gPsysRemoved;     /* 0x80128718 freed-this-frame count */
static MBObject* gPsysRmQueue;     /* 0x8012871c remove queue head */
static s32       gPsysIdCounter;   /* 0x80128720 next psys id */
static s32       gPsysFrame;       /* 0x80128724 global frame counter */
static f32       gPsysFrameFrac;   /* 0x80128728 sub-frame time */
static s32       gDefTexA;         /* 0x8012872c "particle2_a" texture */
static s32       gDefTexXp;        /* 0x80128730 "particle2_xp" texture */
static s32       gPoolTotal;       /* 0x8012873c bytes free in block pool */
static s32       gPoolCount;       /* 0x80128740 free block count */
static PsysMemBlock* gPoolBase;    /* 0x80128748 pool base */
static PsysMemBlock* gPoolFree;    /* 0x80128754 free-list cursor */
static s16       gDirSlot;         /* 0x80128764 shared dir slot cache */
static s16       gPosSlot;         /* 0x80128766 shared pos slot cache */

extern s32   gPsysDisabled;        /* 0x80128768 traverse filter / disable id */

/* --- forward decls (behavioural helpers, static in the PDB) --- */
static void getPPosLinear(Psys* p, f32* out, f32* dir, f32* org, f32 t);
static void getPPosSpeed(Psys* p, f32* out, f32* dir, f32* org, f32 t);
static void getPPosGrav(Psys* p, f32* out, f32* dir, f32* org, f32 t);
static void getPPosSpeedGrav(Psys* p, f32* out, f32* dir, f32* org, f32 t);
static s32  getNewPosRectShare(Psys* p, MBObject* node, s32 z);
static s32  getNewPosRectUnique(Psys* p, MBObject* node, s32 z);
static s32  getNewPosFrame(Psys* p, MBObject* node);
static s32  getNewPosSingle2(void);
static s32  getNewPosSingle1(Psys* p, MBObject* node);
static void getNewDirConeShare(Psys* p, MBObject* node, s32 z);
static void getNewDirConeUnique(Psys* p, MBObject* node, s32 z);
static void getNewDirSphere(Psys* p, MBObject* node, s32 z);
static s32  getNewDirFrame(Psys* p, MBObject* node);
static s32  getNewDirSingle2(void);
static s32  getNewDirSingle1(Psys* p, MBObject* node);
static void getOrthoVecs(f32* a, f32* b, f32* dir);
static void getCurrentDir(Psys* p, MBObject* node, f32* out);
static f64  getSinCos(f64 ang, f32* sinOut);
static void DrawPsysSub(void);
static void setupNewPMode(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6,
                          f64 f7, f64 f8, Psys* p);
static void setupParms(Psys* p);
static void setWorldParms(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6,
                          f64 f7, f64 f8, MBObject* node, Psys* p,
                          PsysDescrip* wp, f32* over, s32 a, s32 b, s32 c,
                          s32 d);
static Psys* allocPsys(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                       f64 f8, s32 fromArena, s32 a, s32 b, s32 c, s32 d,
                       s32 e, s32 g);
static s32* listFindHandle(s32 id, s32 base);
static void freePsys(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                     f64 f8, MBObject* node);
static void* allocPsysMem(s32 size, s32 tag);
static void  freePsysMem(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                         f64 f8, void* blk);
static void initPresetList(void);
static void setPTimeVal(f32 sec, Psys* p);

MBObject* createPsysNode(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                         f64 f8, f32* verts, f32 depth, s32 flag, s32 tex,
                         s32 a, s32 b, s32 c, s32 d);
MBObject* MBNewPsysDescrip(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6,
                           f64 f7, f64 f8, void* a, f32 depth, s32 flag,
                           s32 tex, s32 b, s32 c, s32 d, s32 e);
MBObject* MBPsysFirework(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                         f64 f8, f32* verts, f32 depth, s32 a, s32 b, s32 c,
                         s32 d, s32 e, s32 g);
MBObject* MBPsysFlame(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                      f64 f8, s32 a, s32 tex, f32* verts, s32 b, s32 c, s32 d,
                      s32 e, s32 g);

/* ======================================================================= *
 *  Emit-mode integrators (psys->ppos_func, per live particle over time)   *
 * ======================================================================= */

/* 0x800CCE58 - constant velocity: pos = t*dir + org */
static void getPPosLinear(Psys* p, f32* out, f32* dir, f32* org, f32 t) {
    out[0] = dir[0] * t + org[0];
    out[1] = dir[1] * t + org[1];
    out[2] = dir[2] * t + org[2];
}

/* 0x800CCDD0 - speed-scaled velocity */
static void getPPosSpeed(Psys* p, f32* out, f32* dir, f32* org, f32 t) {
    out[0] = t * (p->p_speed * dir[0]) + org[0];
    out[1] = t * (p->p_speed * dir[1]) + org[1];
    out[2] = t * (p->p_speed * dir[2]) + org[2];
}

/* 0x800CCE1C - gravity on Y */
static void getPPosGrav(Psys* p, f32* out, f32* dir, f32* org, f32 t) {
    out[0] = dir[0] * t + org[0];
    out[1] = t * (p->p_gravity * t + dir[1]) + org[1];
    out[2] = dir[2] * t + org[2];
}

/* 0x800CCD7C - speed-scaled velocity + gravity on Y */
static void getPPosSpeedGrav(Psys* p, f32* out, f32* dir, f32* org, f32 t) {
    out[0] = t * (p->p_speed * dir[0]) + org[0];
    out[1] = t * (p->p_gravity * t + p->p_speed * dir[1]) + org[1];
    out[2] = t * (p->p_speed * dir[2]) + org[2];
}

/* ======================================================================= *
 *  New-position generators (psys->pos_func, initial spawn positions)      *
 * ======================================================================= */

/* fill a slot with a random point inside the emit rect, model->world xformed */
static void randRectPos(Psys* p, MBObject* node, f32* slot) {
    f32* m = p->e_vol;         /* rect extents + 3x3 basis + origin */
    f64 rx = (f64)m[0] * (pbRand() & 0x7fff);
    f64 ry = (f64)m[1] * (pbRand() & 0x7fff);
    f32 rz = (f32)((f64)m[2] * (pbRand() & 0x7fff));
    slot[0] = m[15] + rz * m[8] + (f32)(rx * m[3] + ry * m[6]);
    slot[1] = m[16] + rz * m[9] + (f32)(rx * m[4] + ry * m[7]);
    slot[2] = m[17] + rz * m[10] + (f32)(rx * m[5] + ry * m[8]);
}

/* 0x800CCE8C - cycling (shared) rect position */
static s32 getNewPosRectShare(Psys* p, MBObject* node, s32 z) {
    s32 idx = (s16)p->pos_next;
    if (idx < 0) {
        return -1;
    }
    randRectPos(p, node, p->init_pos_lst[idx]);
    return idx;
}

/* 0x800CD02C - unique (free-slot) rect position */
static s32 getNewPosRectUnique(Psys* p, MBObject* node, s32 z) {
    s32 count = (s16)p->dir_max;
    s32 idx = (s32)((pbRand() & 0x7fff) % (count ? count : 1));
    while (p->pos_use_lst[idx] == 0xff) {
        idx = (idx == 0) ? count - 1 : idx - 1;
    }
    p->pos_use_lst[idx]++;
    randRectPos(p, node, p->init_pos_lst[idx]);
    return idx;
}

/* 0x800CD254 - one shared position per frame (from node origin) */
static s32 getNewPosFrame(Psys* p, MBObject* node) {
    s32 idx;
    if (gPosSlot >= 0) {
        return gPosSlot;
    }
    idx = (s16)p->pos_next;
    if (idx < 0) {
        return idx;
    }
    p->init_pos_lst[idx][0] = node->mat[3][0];
    p->init_pos_lst[idx][1] = node->mat[3][1];
    p->init_pos_lst[idx][2] = node->mat[3][2];
    gPosSlot = (s16)idx;
    return idx;
}

/* 0x800CD2EC - trivial (always slot 0) */
static s32 getNewPosSingle2(void) {
    return 0;
}

/* 0x800CD2F4 - single static position (node origin), computed once */
static s32 getNewPosSingle1(Psys* p, MBObject* node) {
    f32* slot;
    p->pos_func = (PsysPosFunc)getNewPosSingle2;   /* swap to trivial */
    p->flags &= ~8;
    slot = p->init_pos_lst[0];
    slot[0] = node->mat[3][0];
    slot[1] = node->mat[3][1];
    slot[2] = node->mat[3][2];
    return 0;
}

/* ======================================================================= *
 *  New-direction generators (psys->dir_func)                              *
 * ======================================================================= */

/* 0x800CD9F8 - two orthonormal vectors spanning the plane normal to dir */
static void getOrthoVecs(f32* a, f32* b, f32* dir) {
    f32 x = dir[0], y = dir[1], z = dir[2];
    if (x < 0.1 && y < 0.1 && z != 0.0) {
        a[0] = -z; a[1] = 0.0f; a[2] = x;
        b[0] = y * x; b[1] = -(z * z) - x * x; b[2] = y * z;
    } else {
        a[0] = y; a[1] = -x; a[2] = 0.0f;
        b[0] = z * x; b[1] = z * y; b[2] = -(x * x) - y * y;
    }
}

/* 0x800CDA94 - transform the psys base direction by the node basis, normalize.
 * Never inlined in the target (bl getCurrentDir from every caller). */
#pragma dont_inline on
static void getCurrentDir(Psys* p, MBObject* node, f32* out) {
    f32 dx = p->e_dir[0] * node->mat[0][0] + p->e_dir[1] * node->mat[1][0] +
             p->e_dir[2] * node->mat[2][0];
    f32 dy = p->e_dir[0] * node->mat[0][1] + p->e_dir[1] * node->mat[1][1] +
             p->e_dir[2] * node->mat[2][1];
    f32 dz = p->e_dir[0] * node->mat[0][2] + p->e_dir[1] * node->mat[1][2] +
             p->e_dir[2] * node->mat[2][2];
    f32 mag = dx * dx + dy * dy + dz * dz;
    f32 s;
    if (mag < 0.7 || mag > 1.3) {
        s = p->p_speed * mbInvSqrtLookup(mag);
    } else {
        s = p->p_speed;
    }
    out[0] = dx * s;
    out[1] = dy * s;
    out[2] = dz * s;
}
#pragma dont_inline off

/* 0x800CDB74 - fast sin/cos polynomial; returns cos, writes sin */
static f64 getSinCos(f64 ang, f32* sinOut) {
    f32 s = (f32)(0.5 - ang);
    f32 a2 = (f32)(ang * ang);
    f32 s2 = s * s;
    *sinOut = s * s2 * (1.0f + s2 * (-0.16f + s2 * 0.008f)) + s;
    return (f64)(f32)(ang * (a2 * (1.0f + a2 * (-0.16f + a2 * 0.008f))) + ang);
}

/* 0x800CD330 - cycling (shared) cone direction */
static void getNewDirConeShare(Psys* p, MBObject* node, s32 z) {
    f32 dir[3], ux[3], uy[3];
    f32 sn, cs2;
    f64 cs;
    s32 idx = (s16)p->dir_next;
    if (idx < 0) {
        return;
    }
    getCurrentDir(p, node, dir);
    getOrthoVecs(ux, uy, dir);
    cs = getSinCos((f64)(f32)(p->e_angle * (pbRand() & 0x7fff)), &sn);
    cs2 = (f32)getSinCos((f64)(f32)(pbRand() & 0x7fff), &sn);
    if (pbRand() & 4) {
        cs2 = -cs2;
    }
    p->init_dir_lst[idx][0] = cs2 * uy[0] + sn * dir[0] + (f32)(cs * ux[0]);
    p->init_dir_lst[idx][1] = cs2 * uy[1] + sn * dir[1] + (f32)(cs * ux[1]);
    p->init_dir_lst[idx][2] = cs2 * uy[2] + sn * dir[2] + (f32)(cs * ux[2]);
}

/* 0x800CD4DC - unique (free-slot) cone direction */
static void getNewDirConeUnique(Psys* p, MBObject* node, s32 z) {
    f32 dir[3], ux[3], uy[3];
    f32 sn, cs2;
    f64 cs;
    s32 count = (s16)p->dir_max;
    s32 idx = (s32)((pbRand() & 0x7fff) % (count ? count : 1));
    while (p->dir_use_lst[idx] == 0xff) {
        idx = (idx == 0) ? count - 1 : idx - 1;
    }
    p->dir_use_lst[idx]++;
    getCurrentDir(p, node, dir);
    getOrthoVecs(ux, uy, dir);
    cs = getSinCos((f64)(f32)(p->e_angle * (pbRand() & 0x7fff)), &sn);
    cs2 = (f32)getSinCos((f64)(f32)(pbRand() & 0x7fff), &sn);
    if (pbRand() & 4) {
        cs2 = -cs2;
    }
    p->init_dir_lst[idx][0] = cs2 * uy[0] + sn * dir[0] + (f32)(cs * ux[0]);
    p->init_dir_lst[idx][1] = cs2 * uy[1] + sn * dir[1] + (f32)(cs * ux[1]);
    p->init_dir_lst[idx][2] = cs2 * uy[2] + sn * dir[2] + (f32)(cs * ux[2]);
}

/* 0x800CD718 - random spherical direction (unit cube rejection + normalize) */
static void getNewDirSphere(Psys* p, MBObject* node, s32 z) {
    f32* slot;
    f64 dx, dy, dz, len;
    s32 count = (s16)p->dir_max;
    s32 idx = (s32)((pbRand() & 0x7fff) % (count ? count : 1));
    while (p->dir_use_lst[idx] == 0xff) {
        idx = (idx == 0) ? count - 1 : idx - 1;
    }
    p->dir_use_lst[idx]++;
    slot = p->init_dir_lst[idx];
    dx = (f32)((pbRand() & 0x7fff) * 6.1e-5 - 1.0);
    dy = (f32)((pbRand() & 0x7fff) * 6.1e-5 - 1.0);
    dz = (f32)((pbRand() & 0x7fff) * 6.1e-5 - 1.0);
    len = fqdist(fqdist(dx, dz), dy);
    if (len <= 1.0) {
        slot[0] = (f32)dx; slot[1] = (f32)dy; slot[2] = (f32)dz;
    } else {
        len = 1.0 / len;
        slot[0] = (f32)(dx * len); slot[1] = (f32)(dy * len); slot[2] = (f32)(dz * len);
    }
}

/* 0x800CD90C - cycling (frame) direction */
static s32 getNewDirFrame(Psys* p, MBObject* node) {
    s32 idx = gDirSlot;
    if (idx < 0) {
        idx = (s16)p->dir_next;
        if (idx < 0) {
            return idx;
        }
        getCurrentDir(p, node, p->init_dir_lst[idx]);
        gDirSlot = (s16)idx;
    }
    return idx;
}

/* 0x800CD9B0 - trivial (always slot 0) */
static s32 getNewDirSingle2(void) {
    return 0;
}

/* 0x800CD9B8 - single static direction, computed once */
static s32 getNewDirSingle1(Psys* p, MBObject* node) {
    p->dir_func = (PsysDirFunc)getNewDirSingle2;   /* swap to trivial */
    p->flags &= ~8;
    getCurrentDir(p, node, p->init_dir_lst[0]);
    return 0;
}

/* ======================================================================= *
 *  Drawing                                                                *
 * ======================================================================= */

/* 0x800CC8F4 - project one particle sprite and submit a GX quad.
 * Documented flow: cull against the psys screen-rect, back-project two
 * corners, then GXBegin(GX_QUADS) and stream 4 verts to the FIFO. */
static void DrawPsysSub(void) {
    f32 eye[3];
    f32 corner[4];
    __as__4vec3FRC4vec3((u32)eye, (u32)corner);
    sceSamp0MultVec(corner, (f32*)&gWinGlobals, eye);
    GXBegin(0x98, 0, 4);
}

/* 0x800CBC4C - per-frame emitter state machine + particle draw pass.
 * Documented flow: run the emitter phase machine (psys->e_phase: 0 delay .. 8
 * dead), emit new particles via pos_func/dir_func, age the ring through
 * ppos_func and draw each live particle with DrawPsysSub. Giant (NonMatching). */
void MBDrawPsys(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                MBObject* node) {
    Psys* p = (Psys*)node->data.psys;
    s32 phase = p->e_phase;
    s32 dt = gPsysFrame - p->e_last_time;
    if ((u32)dt > 0xf) {
        dt = 1;
    }
    gDirSlot = -1;
    gPosSlot = -1;
    if (phase == 0) {
        setupParms(p);
        setupNewPMode(f1, f2, f3, f4, f5, f6, f7, 0.0, p);
        p->e_phase = 2;
        (void)listFindHandle(p->id, (s32)&gPsysRmQueue);
    }
    if (p->p_lst != NULL) {
        DrawPsysSub();
    }
}

/* 0x800CDBE0 - visibility pre-cull; sets psys->e_isvis draw flag */
BOOL MBDrawPsysTest(MBObject* node, void* draw) {
    Psys* p = (Psys*)node->data.psys;
    s32 vis;
    if (p->e_phase >= 6) {
        vis = 1;
    } else {
        vis = MBWorldSphereVisible3((f64)p->max_dist, (u8*)draw + 0x30);
    }
    p->e_isvis = vis;
    if (vis == 0 && p->p_nactive != 0) {
        vis = 1;
    }
    return vis != 0;
}

/* 0x800CDC5C - MBTraversePsys visitor: guard non-psys / filtered nodes */
s32 MBTraversePsys(MBObject* node, void* fn) {
    Psys* p = (Psys*)node->data.psys;
    if (p == NULL || gPsysDisabled != 0) {
        if (p == NULL) {
            ErrorPrintf("MBTraversePsys: PSYS node with psys=0");
            return 1;
        }
        if (gPsysDisabled != p->id) {
            return 0;
        }
    }
    AddPsysObject(fn, node);
    return 0;
}

/* ======================================================================= *
 *  Emitter setup                                                          *
 * ======================================================================= */

/* 0x800CE758 - recompute per-emit interpolation rates for a psys.
 * Documented reconstruction (NonMatching). */
static void setupParms(Psys* p) {
    if (p->e_life == 0) {
        p->e_life = 1;
    }
    if (p->p_tex == NULL) {
        p->p_tex = (struct ROMTEX*)gDefTexA;
    }
}

/* 0x800CDCE4 - choose spawn generators + size the ring/index/usage buffers.
 * Wires dir_func/pos_func/ppos_func based on the emit distribution and
 * animation flags, then carves the per-psys buffers out of the block pool (or
 * the world arena). Giant (NonMatching); documented flow. */
static void setupNewPMode(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6,
                          f64 f7, f64 f8, Psys* p) {
    void* posFn;
    void* dirFn;
    void* ppFn;
    s32   mode = (s32)p->dir_bits;   /* distribution: single/frame/sphere/cone */
    s32   count = (s16)p->dir_max;
    s32   ringBytes;
    void* buf;
    s32   shared = (p->flags & 0x80) != 0;

    if (count < 1) {
        count = 1;
    }
    switch (mode) {
    case 0:  dirFn = shared ? (void*)getNewDirFrame : (void*)getNewDirSingle1; break;
    case 2:  dirFn = (void*)getNewDirSphere; break;
    case 4:  dirFn = (void*)getNewDirConeUnique; break;
    default: dirFn = (void*)getNewDirConeShare; break;
    }
    switch (mode) {
    case 0:  posFn = shared ? (void*)getNewPosFrame : (void*)getNewPosSingle1; break;
    case 2:  posFn = (void*)getNewPosRectUnique; break;
    default: posFn = (void*)getNewPosRectShare; break;
    }
    if (p->p_speed != 0.0f) {
        ppFn = (p->p_gravity != 0.0f) ? (void*)getPPosSpeedGrav : (void*)getPPosSpeed;
    } else {
        ppFn = (p->p_gravity != 0.0f) ? (void*)getPPosGrav : (void*)getPPosLinear;
    }

    p->flags |= 4 | 8;
    p->dir_func = (PsysDirFunc)dirFn;
    p->pos_func = (PsysPosFunc)posFn;
    p->ppos_func = (PsysPPosFunc)ppFn;

    ringBytes = count * 2 + count * 0xc + count * 0xc;
    if (p->worldname == NULL) {
        buf = allocPsysMem(ringBytes, p->id);
    } else {
        buf = AllocMem(f1, f2, f3, f4, f5, f6, f7, f8, ringBytes, 0,
                       0, 0, 0, 0, 0, 0);
    }
    if (buf == NULL) {
        ErrorPrintf("No mem for psys id=%d", p->id);
        p->p_lst = NULL;
        return;
    }
    memset(buf, 0, ringBytes);
    p->p_lst = (u16*)buf;
    p->init_pos_lst = (f32(*)[3])((u8*)buf + count * 2);
    p->init_dir_lst = (f32(*)[3])((u8*)p->init_pos_lst + count * 0xc);
}

/* ======================================================================= *
 *  World-psys descriptor application                                       *
 * ======================================================================= */

/* 0x800CEBC0 - apply a WORLDPSYS descriptor blob to a live psys node.
 * Walks the wp->fields_used bitfield, applying each present attribute (emit
 * time, counts, lifetime, colour envelope, speed, gravity, textures, flags)
 * with the same clamping the individual MBPsysSet* setters use. Giant
 * (NonMatching); documented flow. */
static void setWorldParms(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6,
                          f64 f7, f64 f8, MBObject* node, Psys* p,
                          PsysDescrip* wp, f32* over, s32 a, s32 b, s32 c,
                          s32 d) {
    if (wp->pversion < 0x100) {
        ErrorPrintf("setWorldParms: WORLDPSYS type is bad");
        return;
    }
    if (wp->fields_used & 0x2) {
        p->dir_max = (u16)wp->max_directions;
    }
    if (wp->fields_used & 0x10) {
        p->e_life = (u16)wp->e_lifefade[0];
        p->e_fade = (u16)wp->e_lifefade[1];
    }
    if (wp->fields_used & 0x40) {
        setPTimeVal(wp->e_angle, p);
    }
    if (wp->fields_used & 0x2000) {
        p->p_speed = (f32)(wp->p_speed * (1.0 / 30.0));
    }
    if (wp->fields_used & 0x4000) {
        p->p_texidx = MBOX_FindTexture(wp->p_texname1, 0);
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
MBObject* MBNewWorldPsys(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                         f64 f8, f32* verts, f32 depth, PsysDescrip* wp,
                         s32 tex, char* name, f32* over, s32 a, s32 g) {
    MBObject* node = createPsysNode(f1, f2, f3, f4, f5, f6, f7, f8, verts, depth,
                                    (name != NULL), tex, 0, 0, 0, 0);
    if (node != NULL) {
        Psys* p = (Psys*)node->data.psys;
        p->worldname = (name != NULL && *name != '\0') ? name : NULL;
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
MBObject* MBNewPsysDescrip(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6,
                           f64 f7, f64 f8, void* wp, f32 depth, s32 flag,
                           s32 tex, s32 b, s32 c, s32 d, s32 e) {
    MBObject* node = createPsysNode(f1, f2, f3, f4, f5, f6, f7, f8, 0, depth,
                                    flag, tex, 0, 0, 0, 0);
    if (node != NULL && wp != NULL) {
        setWorldParms(f1, f2, f3, f4, f5, f6, f7, f8, node,
                      (Psys*)node->data.psys, (PsysDescrip*)wp, 0, 0, 0, 0, 0);
    }
    return node;
}

/* 0x800CFA84 - firework preset (deferred build through MBNewPsysDescrip) */
MBObject* MBPsysFirework(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                         f64 f8, f32* verts, f32 depth, s32 a, s32 b, s32 c,
                         s32 d, s32 e, s32 g) {
    return MBNewPsysDescrip(f1, f2, f3, f4, f5, f6, f7, f8, verts, depth, 0,
                            a, b, c, d, e);
}

/* 0x800CF8EC - flame preset */
MBObject* MBPsysFlame(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                      f64 f8, s32 a, s32 tex, f32* verts, s32 b, s32 c, s32 d,
                      s32 e, s32 g) {
    return MBNewPsysDescrip(f1, f2, f3, f4, f5, f6, f7, f8, verts, (f32)a, 0,
                            tex, b, c, d, e);
}

/* 0x800D079C - default psys node (no descriptor), stores render flags */
MBObject* MBNewPsysDefault(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6,
                           f64 f7, f64 f8, f32* verts, f32 depth, s32 flags,
                           s32 tex, s32 a, s32 b, s32 c, s32 d) {
    MBObject* node;
    if (depth == 0.0f) {
        depth = 1.0f;   /* default draw depth (0x80344eb8) */
    }
    node = createPsysNode(f1, f2, f3, f4, f5, f6, f7, f8, verts, depth, 0, tex,
                          a, b, c, d);
    if (node != NULL) {
        node->flags = flags;
    }
    return node;
}

/* 0x800D07FC - create a scene node + attach a fresh psys descriptor */
MBObject* createPsysNode(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                         f64 f8, f32* verts, f32 depth, s32 flag, s32 tex,
                         s32 a, s32 b, s32 c, s32 d) {
    MBObject* node;
    Psys* p;
    if (gPsysDisabled == -1) {
        return NULL;
    }
    node = MBNewNode(f1, f2, f3, f4, f5, f6, f7, f8, depth, verts, 0xe, tex,
                       verts, a, b, c);
    if (node == NULL) {
        return NULL;
    }
    p = allocPsys(f1, f2, f3, f4, f5, f6, f7, f8, flag, 0, 0, tex, 0, 0, 0);
    if (p == NULL) {
        node->data.psys = NULL;
        MBRemoveNode(node, 1);
        return NULL;
    }
    node->data.psys = p;
    p->node = (struct mbnode*)node;   /* back-link */
    if (tex != 0) {
        if (tex == -1) {
            p->flags |= 0x80;
        } else if (tex == -2) {
            p->flags |= 0x40;
        } else {
            p->flags |= 0xc0;
        }
    }
    p->e_last_time = gPsysFrame;
    p->e_phase = 0;
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
    p->e_life = 300;
    p->e_fade = 300;
    p->p_speed = 1.0f;
    p->dir_max = 0x14;
    if (gDefTexA == 0 || gDefTexXp == 0) {
        s32 ta = MBOX_FindTexture("particle2_a", 0);
        s32 tx = MBOX_FindTexture("particle2_xp", 0);
        gDefTexA = ta;
        gDefTexXp = tx;
    }
    return p;
}

/* ======================================================================= *
 *  Per-attribute setters (rejected once emitting has begun: e_phase > 1)  *
 * ======================================================================= */

/* 0x800D0B14 - MBPsysSetPTex: bind particle texture by handle.
 * Documented reconstruction (NonMatching; target resolves p_tex through the
 * gWinGlobals texture table via a packed page|sub index). */
void MBPsysSetPTex(MBObject* node, u32 tex) {
    Psys* p = (Psys*)node->data.psys;
    p->p_texidx = tex;
    p->p_tex = (struct ROMTEX*)tex;   /* resolved page ptr */
}

/* 0x800D0B44 - MBPsysScalePParm: multiply a parameter gradient by a scalar */
void MBPsysScalePParm(f32 s, MBObject* node, s32 parm) {
    Psys* p = (Psys*)node->data.psys;
    if (p->e_phase > 1) {
        ErrorPrintf("Setting PSYS attribute after draw begins");
        return;
    }
    if (parm >= 5) {
        ErrorPrintf("MBPsysSetPParm: parm %d too big", parm);
        return;
    }
    p->p_parms[parm].i.life_start *= s;
    p->p_parms[parm].i.life_end   *= s;
    p->p_parms[parm].i.fade_start *= s;
    p->p_parms[parm].i.fade_end   *= s;
}

/* 0x800D0BD8 - MBPsysSetPParm: set a 4-key parameter gradient (scaled+clamped).
 * Documented reconstruction (NonMatching; scale/min/max come from psysInfo,
 * reached in the target through a pooled absolute base). */
void MBPsysSetPParm(f32 v0, f32 v1, f32 v2, f32 v3, MBObject* node, s32 parm) {
    Psys* p = (Psys*)node->data.psys;
    f32* base = psysInfo;
    f32* info;
    f32 scale, mn, mx;
    f32 a;
    if (p->e_phase > 1) {
        ErrorPrintf("Setting PSYS attribute after draw begins");
        return;
    }
    if (parm >= 5) {
        ErrorPrintf("MBPsysSetPParm: parm %d too big", parm);
        return;
    }
    info = base + parm * 4;
    scale = info[55];
    mn = info[56];
    mx = info[57];
    a = v0 * scale; if (a < mn) a = mn; else if (a > mx) a = mx;
    p->p_parms[parm].i.life_start = a;
    a = v1 * scale; if (a < mn) a = mn; else if (a > mx) a = mx;
    p->p_parms[parm].i.life_end = a;
    a = v2 * scale; if (a < mn) a = mn; else if (a > mx) a = mx;
    p->p_parms[parm].i.fade_start = a;
    a = v3 * scale; if (a < mn) a = mn; else if (a > mx) a = mx;
    p->p_parms[parm].i.fade_end = a;
}

/* 0x800D0D8C - lifetime helper: emission cone angle (deg -> half-angle rad) */
static void setPTimeVal(f32 sec, Psys* p) {
    if (sec < 0.0f) {
        p->e_angle = -1.0f;
    } else if (sec < 1.0f) {
        p->e_angle = 0.0f;
    } else if (sec < 359.0f) {
        p->e_angle = (f32)(3.141592654 * sec / 360.0);
    } else {
        p->e_angle = -1.0f;
    }
}

/* 0x800D0CF0 - MBPsysSetPTime: emission cone angle (writes e_angle) */
void MBPsysSetPTime(f32 sec, MBObject* node) {
    Psys* p = (Psys*)node->data.psys;
    if (p->e_phase > 1) {
        ErrorPrintf("Setting PSYS attribute after draw begins");
    } else {
        setPTimeVal(sec, p);
    }
}

/* 0x800D0DEC - MBPsysSetPSpeed: particle speed multiplier */
void MBPsysSetPSpeed(f64 v, MBObject* node) {
    Psys* p = (Psys*)node->data.psys;
    if (p->e_phase > 1) {
        ErrorPrintf("Setting PSYS attribute after draw begins");
    } else {
        p->p_speed = (f32)(v * (1.0 / 30.0));
    }
}

/* 0x800D0E3C - MBPsysSetERate4: emission-rate envelope (4 keys).
 * NOTE: PDB name inferred (4-value envelope at psys+0xd0). */
void MBPsysSetERate4(f64 v0, f64 v1, f64 v2, f64 v3, MBObject* node) {
    Psys* p = (Psys*)node->data.psys;
    if (p->e_phase > 1) {
        ErrorPrintf("Setting PSYS attribute after draw begins");
    } else {
        p->e_rate.i.life_start = (f32)(v0 * (1.0 / 30.0));
        p->e_rate.i.life_end   = (f32)(v1 * (1.0 / 30.0));
        p->e_rate.i.fade_start = (f32)(v2 * (1.0 / 30.0));
        p->e_rate.i.fade_end   = (f32)(v3 * (1.0 / 30.0));
    }
}

/* 0x800D0EB0 - MBPsysSetEVolume: particle life/fade base+range (bytes, clamped).
 * NOTE: PDB name inferred (byte fields psys+0x60/0x61). */
void MBPsysSetEVolume(f32 base, f32 range, MBObject* node) {
    Psys* p = (Psys*)node->data.psys;
    f64 v;
    if (p->e_phase > 1) {
        ErrorPrintf("Setting PSYS attribute after draw begins");
        return;
    }
    v = 30.0 * base;
    v = (v < 1.0) ? 1.0 : (v > 255.0) ? 255.0 : v;
    p->p_life = (s32)v;
    v = 30.0 * range;
    v = (v < 0.0) ? 0.0 : (v > 255.0) ? 255.0 : v;
    p->p_fade = (s32)v;
}

/* 0x800D0F68 - MBPsysSetETime: emit duration + fade (with forever flag) */
void MBPsysSetETime(f32 dur, f32 rep, MBObject* node) {
    Psys* p = (Psys*)node->data.psys;
    f64 v;
    if (p->e_phase > 1) {
        ErrorPrintf("Setting PSYS attribute after draw begins");
        return;
    }
    v = 30.0 * dur;
    v = (v < 1.0) ? 1.0 : (v > 65535.0) ? 65535.0 : v;
    p->e_life = (s32)v;
    v = 30.0 * rep;
    v = (v < 0.0) ? 0.0 : (v > 65535.0) ? 65535.0 : v;
    p->e_fade = (s32)v;
    p->flags &= ~1;
    if (dur < 0.0f) {
        p->e_life = 0xffff;
        p->flags |= 2;
    } else if (rep < 0.0f) {
        p->e_fade = 0xffff;
        p->flags |= 2;
    }
}

/* ======================================================================= *
 *  Per-frame driver, removal, and pool management                         *
 * ======================================================================= */

/* 0x800D1074 - advance the clock, free queued psys, spawn deferred effects.
 * Documented reconstruction (NonMatching). */
void MBPsysStartFrame(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                      f64 f8) {
    MBObject* node = gPsysRmQueue;
    gPsysFrame++;
    gPsysFrameFrac = 0.0f;
    while (node != NULL) {
        MBObject* next = node->child;
        freePsys(f1, f2, f3, f4, f5, f6, f7, f8, node);
        node = next;
    }
    gPsysRmQueue = NULL;
    gPsysRemoved = 0;
    (void)MBPsysSetDebugNode(0, 0);
}

/* 0x800D12F0 - MBRemovePsys: mark a psys node for removal */
void MBRemovePsys(MBObject* node) {
    if (node != NULL) {
        if (node->type != 0x0e) {
            node = node->child;
        }
        if (node == NULL || node->type != 0x0e) {
            ErrorPrintf("MBRemovePsys: Non psys node");
        } else {
            Psys* p = (Psys*)node->data.psys;
            if (p->e_phase < 6) {
                p->e_phase = 6;
            }
        }
    }
}

/* 0x800D1364 - listFindHandle: walk an id chain to a matching link slot */
static s32* listFindHandle(s32 id, s32 base) {
    s32* link = (s32*)(base + 4);
    u32 cur;
    while ((cur = *link) != 0 && cur != (u32)id) {
        link = (s32*)(cur + 0x24);
    }
    return link;
}

/* 0x800D138C - freePsys: release a psys node's buffers back to the pool.
 * Documented reconstruction (NonMatching). */
static void freePsys(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                     f64 f8, MBObject* node) {
    if (node->child != NULL) {
        node->child->data.psys = NULL;
        MBRemoveNode(node->child, 1);
        node->child = NULL;
    }
    if (*((s32*)node + 1) == 0) {   /* not world-owned */
        Psys* p = (Psys*)node->data.psys;
        if (p != NULL) {
            freePsysMem(f1, f2, f3, f4, f5, f6, f7, f8, p);
            node->data.psys = NULL;
        }
        freePsysMem(f1, f2, f3, f4, f5, f6, f7, f8, node);
    }
}

/* 0x800D1404 - allocPsysMem: first-fit split allocator over the block pool.
 * Documented reconstruction (NonMatching). */
static void* allocPsysMem(s32 size, s32 tag) {
    u32 need;
    PsysMemBlock* b;
    if (size <= 0 || size > gPoolTotal) {
        return NULL;
    }
    need = (size + 0x1f) & 0xfffffff0;
    b = gPoolFree;
    do {
        if ((s32)need <= b->bytes) {
            b->id = tag;
            if ((u32)(b->bytes - need) < 0x131) {
                gPoolFree = b->next ? b->next : gPoolBase;
                b->bytes = -b->bytes;
                gPoolCount--;
                gPoolTotal -= (-b->bytes);
                return b + 1;
            }
            gPoolTotal -= need;
            gPoolCount--;
            b->bytes = -(s32)need;
            return b + 1;
        }
        b = b->next ? b->next : gPoolBase;
    } while (b != gPoolFree);
    return NULL;
}

/* 0x800D1530 - freePsysMem: return a block, coalescing neighbours.
 * Documented reconstruction (NonMatching). */
static void freePsysMem(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                        f64 f8, void* mem) {
    PsysMemBlock* b = (PsysMemBlock*)mem - 1;
    if (b->bytes >= 0) {
        ErrorPrintf("freePsysMem: bad free block. Error=%d", b->bytes);
        return;
    }
    b->bytes = -b->bytes;
    gPoolTotal += b->bytes;
    gPoolCount++;
    gPoolFree = b;
}

/* 0x800D1724 - initPresetList: checksum + validate the built-in preset table.
 * Documented reconstruction (NonMatching). */
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

/* 0x800D1800 - MBInitPsys: build the 120000-byte block pool + index arrays.
 * Documented reconstruction (NonMatching). */
void MBInitPsys(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                f64 f8) {
    PsysMemBlock* base;
    gPsysActive = 0;
    gPsysList = NULL;
    gPsysRmQueue = NULL;
    gPsysRemoved = 0;
    gPsysIdCounter = 0;
    gPsysFrame = 0;
    gPsysFrameFrac = 1.0f;
    gPoolTotal = 120000;
    gPoolCount = 1;
    base = (PsysMemBlock*)AllocMem(f1, f2, f3, f4, f5, f6, f7, f8, 120000, 0, 0,
                                   0, 0, 0, 0, 0);
    gPoolBase = base;
    gPoolFree = base;
    base->bytes = gPoolTotal;
    base->next = NULL;
    base->prev = NULL;
    base->id = 0;
    initPresetList();
}
