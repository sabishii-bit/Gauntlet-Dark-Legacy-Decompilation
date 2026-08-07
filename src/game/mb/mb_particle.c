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
 * pos_func / dir_func generators wired by setupNewPMode_800CDCE4, advances live
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
 * (MBDrawPsys, DrawPsysSub, setWorldParms, setupNewPMode_800CDCE4, MBNewPsysDescrip) are
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
void  FatalErrorf(const char* fmt, ...);
u32   pbRand(void);
void* AllocMem(s32 size);
s32   MBOX_FindTexture(const char* name, s32* out);   /* texture-by-name lookup */
MBObject* MBNewNode(s32 parent, void* tmpl, s32 mode);     /* scene-node create */
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
extern char  lbl_80116F30[]; /* "freePsysMem: bad free block..." */

typedef struct PsysModuleGlobals {
    u8 pad00[0x54];
    s16 dirSlot;
    s16 posSlot;
} PsysModuleGlobals;

extern PsysModuleGlobals lbl_80128710; /* retail module-global data block */

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
static s32  getNewDirConeShare(Psys* p, MBObject* node, s32 z);
static s32  getNewDirConeUnique(Psys* p, MBObject* node, s32 z);
static s32  getNewDirSphere(Psys* p, MBObject* node, s32 z);
static s32  getNewDirFrame(Psys* p, MBObject* node);
static s32  getNewDirSingle2(void);
static s32  getNewDirSingle1(Psys* p, MBObject* node);
static void getOrthoVecs(f32* a, f32* b, f32* dir);
static void getCurrentDir(Psys* p, MBObject* node, f32* out);
extern const f64 lbl_803491C8;
extern const f64 lbl_803491D0;
extern const f32 lbl_803491D8;
extern const f32 lbl_803491DC;
extern const f32 lbl_803491E0;
static f32  getSinCos(f32 ang, f32* sinOut);
static void DrawPsysSub(void);
static void setupNewPMode_800CDCE4(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6,
                          f64 f7, f64 f8, Psys* p);
static void setupParms(Psys* p);
static void setWorldParms(MBObject* node, Psys* p, PsysDescrip* wp, f32* over);
static Psys* allocPsys(s32 flag);
static s32* listFindHandle(s32 id, s32 base);
static void freePsys(MBObject* node);
static void* allocPsysMem(s32 size, s32 tag);
static void freePsysMem(void* blk);
static void initPresetList(void);
static void setPTimeVal(f32 sec, Psys* p);

MBObject* createPsysNode(s32 a, s32 b, s32 c, s32 d);
MBObject* MBNewPsysDescrip(s32 a, s32 b, s32 c, void* cfg);
MBObject* MBPsysFirework(s32 a, s32 b, s32 count, s32 m0, s32 m1, s32 m2,
                         f32 rate, f32 power, f32 sc0, f32 sc1, f32 sc2);
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
#pragma opt_lifetimes on
#pragma opt_propagation off
static s32 getNewPosRectShare(Psys* p, MBObject* node, s32 z) {
    u8 unused[8];
    f32 ry, rx, rz;
    f32* slot;
    register s32 idx;
    register Psys* psys;
    u16 last;
    register MBObject* obj;
    u16 max;

    idx = p->pos_next;
    psys = p;
    last = p->pos_last;
    obj = node;
    max = p->pos_max;

    if (idx == last) {
        idx = -1;
    } else if (idx == 0) {
        psys->pos_next = max - 1;
    } else {
        psys->pos_next = idx - 1;
    }
    if (idx < 0) {
        return idx;
    }
    slot = psys->init_pos_lst[idx];
    rx = (f32)((f64)psys->e_vol[0] *
               (3.051850947599719e-05 * (f64)(f32)(pbRand() & 0x7fff) - 0.5));
    ry = (f32)((f64)psys->e_vol[1] *
               (3.051850947599719e-05 * (f64)(f32)(pbRand() & 0x7fff) - 0.5));
    rz = (f32)((f64)psys->e_vol[2] *
               (3.051850947599719e-05 * (f64)(f32)(pbRand() & 0x7fff) - 0.5));
    slot[0] = rx * obj->mat[0][0] + ry * obj->mat[1][0] +
              rz * obj->mat[2][0] + obj->mat[3][0];
    slot[1] = rx * obj->mat[0][1] + ry * obj->mat[1][1] +
              rz * obj->mat[2][1] + obj->mat[3][1];
    slot[2] = rx * obj->mat[0][2] + ry * obj->mat[1][2] +
              rz * obj->mat[2][2] + obj->mat[3][2];
    return idx;
}
#pragma opt_lifetimes reset
#pragma opt_propagation reset

/* 0x800CD02C - unique (free-slot) rect position */
#pragma opt_lifetimes on
#pragma opt_propagation off
static s32 getNewPosRectUnique(Psys* p, MBObject* node, s32 z) {
    f32 ry, rx, rz;
    Psys* psys = p;
    s32 idx;
    u8* use = p->pos_use_lst;
    s32 count = p->pos_max;
    MBObject* obj = node;
    s32 first;
    s32 used;

    idx = (s32)(3.051850947599719e-05 * (f64)(f32)count *
                (f64)(f32)(pbRand() & 0x7fff));
    if (idx >= count) {
        idx = count - 1;
    }
    used = use[idx];
    if (used == 0xff) {
        first = idx;
        do {
            if (idx-- == 0) {
                idx = count - 1;
            }
            if (idx == first) {
                idx = 0;
                goto foundPos;
            }
            used = use[idx];
        } while (used == 0xff);
    }
    use[idx] = used + 1;
foundPos:
    if (idx < 0) {
        return idx;
    }
    if (psys->pos_use_lst[idx] > 1) {
        return idx;
    }
    use = (u8*)psys->init_pos_lst[idx];
    rx = (f32)((f64)psys->e_vol[0] *
               (3.051850947599719e-05 * (f64)(f32)(pbRand() & 0x7fff) - 0.5));
    ry = (f32)((f64)psys->e_vol[1] *
               (3.051850947599719e-05 * (f64)(f32)(pbRand() & 0x7fff) - 0.5));
    rz = (f32)((f64)psys->e_vol[2] *
               (3.051850947599719e-05 * (f64)(f32)(pbRand() & 0x7fff) - 0.5));
    ((f32*)use)[0] = rx * obj->mat[0][0] + ry * obj->mat[1][0] +
                     rz * obj->mat[2][0] + obj->mat[3][0];
    ((f32*)use)[1] = rx * obj->mat[0][1] + ry * obj->mat[1][1] +
                     rz * obj->mat[2][1] + obj->mat[3][1];
    ((f32*)use)[2] = rx * obj->mat[0][2] + ry * obj->mat[1][2] +
                     rz * obj->mat[2][2] + obj->mat[3][2];
    return idx;
}
#pragma opt_lifetimes reset
#pragma opt_propagation reset

/* 0x800CD254 - one shared position per frame (from node origin) */
#pragma opt_lifetimes on
#pragma opt_propagation off
static s32 getNewPosFrame(Psys* p, MBObject* node) {
    PsysModuleGlobals* globals = &lbl_80128710;
    s32 cached;

    cached = globals->posSlot;
    if (cached >= 0) {
        return cached;
    }
    {
        u16 max;
        s32 idx;
        f32* slot;

        max = (u16)p->pos_max;
        idx = (s32)p->pos_next;
        if (idx == p->pos_last) {
            idx = -1;
        } else if (idx == 0) {
            p->pos_next = max - 1;
        } else {
            p->pos_next = idx - 1;
        }
        if (idx < 0) {
            return idx;
        }
        slot = p->init_pos_lst[idx];
        slot[0] = node->mat[3][0];
        slot[1] = node->mat[3][1];
        slot[2] = node->mat[3][2];
        globals->posSlot = (s16)idx;
        return idx;
    }
}
#pragma opt_lifetimes reset
#pragma opt_propagation reset

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
static f32 getSinCos(f32 ang, f32* sinOut) {
#ifdef __MWERKS__
    asm {
        lfd f0, lbl_803491C8
        fsub f5, f0, f1
        fcmpo cr0, f1, f0
        frsp f5, f5
        ble folded
        lfd f0, lbl_803491D0
        fsub f1, f0, f1
        frsp f1, f1
    folded:
        fmuls f6, f5, f5
        lfs f3, lbl_803491D8
        fmuls f7, f1, f1
        lfs f2, lbl_803491DC
        lfs f0, lbl_803491E0
        fmuls f4, f6, f3
        fmuls f8, f7, f3
        fadds f3, f2, f4
        fadds f2, f2, f8
        fmuls f3, f6, f3
        fmuls f4, f7, f2
        fadds f2, f0, f3
        fadds f0, f0, f4
        fmuls f2, f6, f2
        fmuls f3, f7, f0
        fmadds f0, f5, f2, f5
        fmadds f1, f1, f3, f1
        stfs f0, 0(r3)
    }
#else
    f32 one;
    f32 negative;
    f32 coefficient;
    f32 st;
    f32 s;
    f32 s2;
    f32 a2;
    f32 at;

    s = (f32)(0.5 - ang);
    if (ang > 0.5) {
        ang = (f32)(1.0 - ang);
    }
    s2 = s * s;
    a2 = ang * ang;
    one = 1.0f;
    negative = -0.16f;
    coefficient = 0.008f;
    st = s2 * coefficient;
    at = a2 * coefficient;
    coefficient = negative + st;
    negative = negative + at;
    coefficient = s2 * coefficient;
    st = a2 * negative;
    negative = one + coefficient;
    one = one + st;
    negative = s2 * negative;
    coefficient = a2 * one;
    *sinOut = s * negative + s;
    return ang * coefficient + ang;
#endif
}

/* 0x800CD330 - cycling (shared) cone direction */
#pragma opt_lifetimes on
#pragma opt_propagation off
static s32 getNewDirConeShare(Psys* p, MBObject* node, s32 z) {
    f32 ux[3], uy[3], dir[3];
    u8 unused[4];
    f32 sn2, sn;
    f32 cs, cs2;
    f32 arg;
    f32* slot;
    s32 idx = p->dir_next;
    u16 max = p->dir_max;
    f32 angle;

    if (idx == p->dir_last) {
        idx = -1;
    } else if (idx == 0) {
        p->dir_next = max - 1;
    } else {
        p->dir_next = idx - 1;
    }
    if (idx < 0) {
        return idx;
    }
    getCurrentDir(p, node, dir);
    getOrthoVecs(ux, uy, dir);
    slot = p->init_dir_lst[idx];
    angle = p->e_angle;
    arg = (f32)(3.051850947599719e-05 * (f64)angle * (f64)(f32)(pbRand() & 0x7fff));
    cs = getSinCos(arg, &sn);
    arg = (f32)(9.587672783631622e-05 * (f64)(f32)(pbRand() & 0x7fff));
    cs2 = getSinCos(arg, &sn2);
    if (pbRand() & 4) {
        cs2 = -cs2;
    }
    cs2 = cs2 * cs;
    sn2 = sn2 * cs;
    slot[0] = sn * dir[0] + sn2 * ux[0] + cs2 * uy[0];
    slot[1] = sn * dir[1] + sn2 * ux[1] + cs2 * uy[1];
    slot[2] = sn * dir[2] + sn2 * ux[2] + cs2 * uy[2];
    return idx;
}
#pragma opt_lifetimes reset
#pragma opt_propagation reset

/* 0x800CD4DC - unique (free-slot) cone direction */
#pragma opt_lifetimes off
static s32 getNewDirConeUnique(Psys* p, MBObject* node, s32 z) {
    u8 unused[8];
    f32 ux[3], uy[3], dir[3];
    f32 sn2, sn;
    f32 cs, cs2;
    f32 arg;
    register Psys* psys = p;
    register s32 idx;
    register s32 count = p->dir_max;
    register u8* use = p->dir_use_lst;
    register MBObject* obj = node;
    s32 first;
    s32 used;
    f32 angle;

    idx = (s32)(3.051850947599719e-05 * (f64)(f32)count *
                (f64)(f32)(pbRand() & 0x7fff));
    if (idx >= count) {
        idx = count - 1;
    }
    used = use[idx];
    if (used == 0xff) {
        first = idx;
        do {
            if (idx-- == 0) {
                idx = count - 1;
            }
            if (idx == first) {
                idx = 0;
                goto found;
            }
            used = use[idx];
        } while (used == 0xff);
    }
    use[idx] = used + 1;
found:
    if (idx < 0) {
        return idx;
    }
    if (psys->dir_use_lst[idx] > 1) {
        return idx;
    }
    getCurrentDir(psys, obj, dir);
    getOrthoVecs(ux, uy, dir);
    use = (u8*)psys->init_dir_lst[idx];
    angle = psys->e_angle;
    arg = (f32)(3.051850947599719e-05 * (f64)angle * (f64)(f32)(pbRand() & 0x7fff));
    cs = getSinCos(arg, &sn);
    arg = (f32)(9.587672783631622e-05 * (f64)(f32)(pbRand() & 0x7fff));
    cs2 = getSinCos(arg, &sn2);
    if (pbRand() & 4) {
        cs2 = -cs2;
    }
    cs2 = cs2 * cs;
    sn2 = sn2 * cs;
    ((f32*)use)[0] = sn * dir[0] + sn2 * ux[0] + cs2 * uy[0];
    ((f32*)use)[1] = sn * dir[1] + sn2 * ux[1] + cs2 * uy[1];
    ((f32*)use)[2] = sn * dir[2] + sn2 * ux[2] + cs2 * uy[2];
    return idx;
}
#pragma opt_lifetimes reset

/* 0x800CD718 - random spherical direction (unit cube rejection + normalize) */
#pragma opt_lifetimes off
static s32 getNewDirSphere(Psys* p, MBObject* node, s32 z) {
    u8 unused[8];
    f32 dx, dy, dz, scale;
    f64 len;
    register Psys* psys = p;
    register s32 idx;
    register u8* use = p->dir_use_lst;
    register s32 count = p->dir_max;
    s32 first;
    s32 used;

    idx = (s32)(3.051850947599719e-05 * (f64)(f32)count *
                (f64)(f32)(pbRand() & 0x7fff));
    if (idx >= count) {
        idx = count - 1;
    }
    used = use[idx];
    if (used == 0xff) {
        first = idx;
        do {
            if (idx-- == 0) {
                idx = count - 1;
            }
            if (idx == first) {
                idx = 0;
                goto foundSphere;
            }
            used = use[idx];
        } while (used == 0xff);
    }
    use[idx] = used + 1;
foundSphere:
    if (idx < 0) {
        return idx;
    }
    if (psys->dir_use_lst[idx] > 1) {
        return idx;
    }
    use = (u8*)psys->init_dir_lst[idx];
    dx = (f32)(6.103701986151684e-06 * (f64)(f32)(pbRand() & 0x7fff) - 0.1);
    dy = (f32)(6.103701986151684e-06 * (f64)(f32)(pbRand() & 0x7fff) - 0.1);
    dz = (f32)(6.103701986151684e-06 * (f64)(f32)(pbRand() & 0x7fff) - 0.1);
    len = fqdist(fqdist(dx, dz), dy);
    if (len > 0.0) {
        scale = (f32)(1.0 / len);
        ((f32*)use)[2] = dz * scale;
        ((f32*)use)[1] = dy * scale;
        ((f32*)use)[0] = dx * scale;
    } else {
        ((f32*)use)[2] = dz;
        ((f32*)use)[1] = dy;
        ((f32*)use)[0] = dx;
    }
    return idx;
}
#pragma opt_lifetimes reset

/* 0x800CD90C - cycling (frame) direction */
#pragma opt_lifetimes on
#pragma opt_propagation off
static s32 getNewDirFrame(Psys* p, MBObject* node) {
    PsysModuleGlobals* globals = &lbl_80128710;
    u8 unused[8];
    s32 cached = globals->dirSlot;

    if (cached >= 0) {
        return cached;
    }
    {
        u16 max = p->dir_max;
        s32 idx = p->dir_next;

        if (idx == p->dir_last) {
            idx = -1;
        } else if (idx == 0) {
            p->dir_next = max - 1;
        } else {
            p->dir_next = idx - 1;
        }
        if (idx < 0) {
            return idx;
        }
        getCurrentDir(p, node, p->init_dir_lst[idx]);
        globals->dirSlot = (s16)idx;
        return idx;
    }
}
#pragma opt_lifetimes reset
#pragma opt_propagation reset

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
    lbl_80128710.dirSlot = -1;
    lbl_80128710.posSlot = -1;
    if (phase == 0) {
        setupParms(p);
        setupNewPMode_800CDCE4(f1, f2, f3, f4, f5, f6, f7, 0.0, p);
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
    u8 unused[8];
    s32 vis;
    if (p->e_phase >= 6) {
        vis = 1;
    } else {
        void* bounds = (u8*)draw + 0x30;
        vis = MBWorldSphereVisible3((f64)p->max_dist, bounds);
    }
    p->e_isvis = vis;
    if (vis == 0 && p->p_nactive != 0) {
        vis = 1;
    }
    if (vis != 0) {
        return 1;
    }
    return 0;
}

/* 0x800CDC5C - MBTraversePsys visitor: guard non-psys / filtered nodes */
s32 MBTraversePsys(MBObject* node, void* fn) {
    Psys* p = (Psys*)node->data.psys;
    u8* globals = (u8*)&lbl_80128710;
    if (p == NULL || *(s32*)(globals + 0x58) != 0) {
        if (p == NULL) {
            ErrorPrintf("MBTraversePsys: PSYS node with psys=0");
            return 1;
        }
        if (*(s32*)(globals + 0x58) != p->id) {
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
static void setupNewPMode_800CDCE4(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6,
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
        buf = AllocMem(ringBytes);
        (void)0; /*
                       0, 0, 0, 0, 0, 0); */
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
 * Walks the blob's fields_used bitfield, applying each present attribute (emit
 * time, counts, lifetime, colour envelope, speed, gravity, textures, flags)
 * with the same clamping the individual MBPsysSet* setters use.
 * The blob layout here is the packed WORLDPSYS wire format (distinct from
 * PsysDescrip, which MBNewPsysDescrip consumes). */
typedef struct WorldPsys {
    /* 0x00 */ u32 pversion;
    /* 0x04 */ s16 preset;
    /* 0x06 */ s16 pad06;
    /* 0x08 */ u32 flagval;
    /* 0x0C */ u32 flagmask;
    /* 0x10 */ u32 used;
    /* 0x14 */ s32 max_particles;
    /* 0x18 */ s32 max_directions;
    /* 0x1C */ s32 max_positions;
    /* 0x20 */ f32 e_lifefade[2];
    /* 0x28 */ f32 p_lifefade[2];
    /* 0x30 */ f32 pad30[2];
    /* 0x38 */ f32 e_angle;
    /* 0x3C */ s32 texcnt;
    /* 0x40 */ char texname[32];
    /* 0x60 */ f32 e_dir[3];
    /* 0x6C */ f32 e_vol[3];
    /* 0x78 */ f32 e_rate[4];
    /* 0x88 */ f32 e_rate_rand;
    /* 0x8C */ f32 gravity;
    /* 0x90 */ f32 drag;
    /* 0x94 */ f32 speed;
    /* 0x98 */ u32 rgba[4];   /* packed per-key RGBA colour envelope       */
    /* 0xA8 */ f32 width[4];
    /* 0xB8 */ f32 e_delay;
} WorldPsys;

static void setWorldParms(MBObject* node, Psys* p, PsysDescrip* wpd, f32* over) {
    WorldPsys* wp = (WorldPsys*)wpd;
    f32* tbl;
    f32 t0, t1, t2, t3;
    f64 v, mn, mx, sc;
    u32 used;
    u32 mask;
    u16 fl;
    u32 nfl;

    if (wp->pversion < 0x100 ||
        (s32)((u8*)&wp->preset - (u8*)wp) != 4 ||
        (s32)((u8*)&wp->flagval - (u8*)wp) != 8 ||
        (s32)((u8*)&wp->flagmask - (u8*)wp) != 0xc ||
        (s32)((u8*)&wp->used - (u8*)wp) != 0x10 ||
        (s32)((u8*)&wp->e_delay - (u8*)wp) != 0xb8) {
        ErrorPrintf("setWorldParms: WORLDPSYS type is bad");
        return;
    }

    if (wp->used & 0x1) {   /* inherit from a built-in preset first */
        u8* base = (u8*)psysInfo;
        s32 i = 0;
        for (;;) {
            s16 id = *(s16*)(base + i + 0x130);
            u8* pre = base + i + 0x12c;
            if (id == -1) {
                break;
            }
            if (wp->preset == id) {
                setWorldParms(node, p, (PsysDescrip*)pre, 0);
                break;
            }
            i += 0x138;
        }
    }
    if (wp->used & 0x2) {
        p->p_max = (u16)wp->max_particles;
    }
    if (wp->used & 0x8) {
        p->pos_max = (u16)wp->max_positions;
    }
    if (wp->used & 0x4) {
        p->dir_max = (u16)wp->max_directions;
    }
    if (wp->used & 0x10) {
        v = 30.0 * wp->e_lifefade[0];
        p->e_life = (u16)(s32)((v < 1.0) ? 1.0 : (v > 65535.0) ? 65535.0 : v);
        v = 30.0 * wp->e_lifefade[1];
        p->e_fade = (u16)(s32)((v < 0.0) ? 0.0 : (v > 65535.0) ? 65535.0 : v);
        if (wp->e_lifefade[0] < 0.0f) {
            p->e_life = 0xffff;
            p->flags |= 2;
        } else if (wp->e_lifefade[1] < 0.0f) {
            p->e_fade = 0xffff;
            p->flags |= 2;
        }
    }
    if (wp->used & 0x20) {
        v = 30.0 * wp->p_lifefade[0];
        p->p_life = (u8)(s32)((v < 1.0) ? 1.0 : (v > 255.0) ? 255.0 : v);
        v = 30.0 * wp->p_lifefade[1];
        p->p_fade = (u8)(s32)((v < 0.0) ? 0.0 : (v > 255.0) ? 255.0 : v);
    }
    if (wp->used & 0x40) {
        f32 a = wp->e_angle;
        if (a < 0.0f) {
            p->e_angle = -1.0f;
        } else if (a < 1.0f) {
            p->e_angle = 0.0f;
        } else if (a < 359.0f) {
            p->e_angle = (f32)((3.141592654 * a) / 360.0);
        } else {
            p->e_angle = -1.0f;
        }
    }
    if (wp->used & 0x80000) {
        v = 30.0 * wp->e_delay;
        p->e_delay = (u16)(s32)((v <= 0.0) ? 0.0 : v);
    }
    if (wp->used & 0x80) {
        p->e_dir[0] = wp->e_dir[0];
        p->e_dir[1] = wp->e_dir[1];
        p->e_dir[2] = wp->e_dir[2];
    } else {
        if (over == 0 ||
            (over[0] == 0.0f && over[1] == 0.0f && over[2] == 0.0f)) {
            p->e_dir[0] = 0.0f;
            p->e_dir[1] = 1.0f;
            p->e_dir[2] = 0.0f;
        } else {
            p->e_dir[0] = over[0];
            p->e_dir[1] = over[1];
            p->e_dir[2] = over[2];
        }
    }
    if (wp->used & 0x100) {
        p->e_vol[0] = wp->e_vol[0];
        p->e_vol[1] = wp->e_vol[1];
        p->e_vol[2] = wp->e_vol[2];
    }
    if (wp->used & 0x200) {
        p->e_rate.i.life_start = (f32)((1.0 / 30.0) * wp->e_rate[0]);
        p->e_rate.i.life_end   = (f32)((1.0 / 30.0) * wp->e_rate[1]);
        p->e_rate.i.fade_start = (f32)((1.0 / 30.0) * wp->e_rate[2]);
        p->e_rate.i.fade_end   = (f32)((1.0 / 30.0) * wp->e_rate[3]);
    }
    if (wp->used & 0x400) {
        p->e_rate_rand = (f32)(0.01 * wp->e_rate_rand);
    }
    if (wp->used & 0x800) {
        p->p_gravity = (f32)((-32.0 / 900.0) * wp->gravity);
    }
    if (wp->used & 0x1000) {
        p->p_drag = (f32)(-1.0 * wp->drag);
    }
    if (wp->used & 0x2000) {
        p->p_speed = (f32)((1.0 / 30.0) * wp->speed);
    }
    if (wp->used & 0x10000) {  /* packed r/g/b colour envelopes */
        Psys* q;
        /* red (lane 16) */
        q = (Psys*)node->data.psys;
        t3 = (f32)((1.0 / 255.0) * (f32)((wp->rgba[3] >> 16) & 0xff));
        t2 = (f32)((1.0 / 255.0) * (f32)((wp->rgba[2] >> 16) & 0xff));
        t1 = (f32)((1.0 / 255.0) * (f32)((wp->rgba[1] >> 16) & 0xff));
        if (q->e_phase > 1) {
            ErrorPrintf("Setting PSYS attribute after draw begins");
        } else {
            f32 sc, mn, mx, fv;
            tbl = (f32*)((u8*)&lbl_80128710 + 0x9c + 2 * 0x10);
            sc = tbl[0]; mn = tbl[1]; mx = tbl[2];
            t0 = (f32)((1.0 / 255.0) * (f32)((wp->rgba[0] >> 16) & 0xff));
            fv = t0 * sc;
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[2].i.life_start = fv;
            fv = t1 * sc;
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[2].i.life_end = fv;
            fv = t2 * sc;
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[2].i.fade_start = fv;
            fv = t3 * sc;
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[2].i.fade_end = fv;
        }
        /* green (lane 8) */
        q = (Psys*)node->data.psys;
        t3 = (f32)((1.0 / 255.0) * (f32)((wp->rgba[3] >> 8) & 0xff));
        t2 = (f32)((1.0 / 255.0) * (f32)((wp->rgba[2] >> 8) & 0xff));
        t1 = (f32)((1.0 / 255.0) * (f32)((wp->rgba[1] >> 8) & 0xff));
        if (q->e_phase > 1) {
            ErrorPrintf("Setting PSYS attribute after draw begins");
        } else {
            f32 sc, mn, mx, fv;
            tbl = (f32*)((u8*)&lbl_80128710 + 0x9c + 1 * 0x10);
            sc = tbl[0]; mn = tbl[1]; mx = tbl[2];
            t0 = (f32)((1.0 / 255.0) * (f32)((wp->rgba[0] >> 8) & 0xff));
            fv = t0 * sc;
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[1].i.life_start = fv;
            fv = t1 * sc;
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[1].i.life_end = fv;
            fv = t2 * sc;
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[1].i.fade_start = fv;
            fv = t3 * sc;
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[1].i.fade_end = fv;
        }
        /* blue (lane 0) */
        q = (Psys*)node->data.psys;
        t3 = (f32)((1.0 / 255.0) * (f32)(wp->rgba[3] & 0xff));
        t2 = (f32)((1.0 / 255.0) * (f32)(wp->rgba[2] & 0xff));
        t1 = (f32)((1.0 / 255.0) * (f32)(wp->rgba[1] & 0xff));
        if (q->e_phase > 1) {
            ErrorPrintf("Setting PSYS attribute after draw begins");
        } else {
            f32 sc, mn, mx, fv;
            tbl = (f32*)((u8*)&lbl_80128710 + 0x9c);
            sc = tbl[0]; mn = tbl[1]; mx = tbl[2];
            t0 = (f32)((1.0 / 255.0) * (f32)(wp->rgba[0] & 0xff));
            fv = t0 * sc;
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[0].i.life_start = fv;
            fv = t1 * sc;
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[0].i.life_end = fv;
            fv = t2 * sc;
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[0].i.fade_start = fv;
            fv = t3 * sc;
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[0].i.fade_end = fv;
        }
    }
    if (wp->used & 0x20000) {  /* alpha / intensity envelope (lane 24) */
        Psys* q = (Psys*)node->data.psys;
        t3 = (f32)((1.0 / 255.0) * (f32)(wp->rgba[3] >> 24));
        t2 = (f32)((1.0 / 255.0) * (f32)(wp->rgba[2] >> 24));
        t1 = (f32)((1.0 / 255.0) * (f32)(wp->rgba[1] >> 24));
        if (q->e_phase > 1) {
            ErrorPrintf("Setting PSYS attribute after draw begins");
        } else {
            f32 sc, mn, mx, fv;
            tbl = (f32*)((u8*)&lbl_80128710 + 0x9c + 3 * 0x10);
            sc = tbl[0]; mn = tbl[1]; mx = tbl[2];
            t0 = (f32)((1.0 / 255.0) * (f32)(wp->rgba[0] >> 24));
            fv = t0 * sc;
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[3].i.life_start = fv;
            fv = t1 * sc;
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[3].i.life_end = fv;
            fv = t2 * sc;
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[3].i.fade_start = fv;
            fv = t3 * sc;
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[3].i.fade_end = fv;
        }
    }
    if (wp->used & 0x40000) {  /* particle width envelope */
        Psys* q = (Psys*)node->data.psys;
        f32 w3 = wp->width[3];
        f32 w2 = wp->width[2];
        if (q->e_phase > 1) {
            ErrorPrintf("Setting PSYS attribute after draw begins");
        } else {
            f32 mn, mx, fv;
            tbl = (f32*)((u8*)&lbl_80128710 + 0x9c + 4 * 0x10);
            sc = tbl[0]; mn = tbl[1]; mx = tbl[2];
            fv = (f32)(wp->width[0] * sc);
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[4].i.life_start = fv;
            fv = (f32)(wp->width[1] * sc);
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[4].i.life_end = fv;
            fv = (f32)(w2 * sc);
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[4].i.fade_start = fv;
            fv = (f32)(w3 * sc);
            if (fv < mn) fv = mn; else if (mx < fv) fv = mx;
            q->p_parms[4].i.fade_end = fv;
        }
    }
    if (wp->used & 0x4000) {   /* bind particle texture by name */
        struct TexPageEnt {
            u32 f0;
            u8* obj;
            u32 f8;
            u32 fC;
        };
        u32 tex = MBOX_FindTexture(wp->texname, 0);
        u8* g = (u8*)gWinGlobals;
        Psys* q = (Psys*)node->data.psys;
        struct TexPageEnt* pages;
        q->p_texidx = tex;
        pages = *(struct TexPageEnt**)(g + 48);
        q->p_tex = (struct ROMTEX*)(*(u8**)(pages[(tex >> 16) & 0xFFFF].obj + 88) +
                                    (tex & 0xFFFF) * 16);
    }
    if (wp->used & 0x8000) {
        p->texcnt = wp->texcnt;
    }
    if ((wp->flagval & 8) && 0.0 == p->p_gravity) {
        p->p_gravity = -0.035555556f;
    }
    if ((wp->flagval & 0x10) && 0.0 == p->p_drag) {
        p->p_drag = -1.0f;
    }
    mask = wp->flagmask;
    fl = p->flags;
    if (mask & 1) {
        fl = (fl & 0xff3f) | ((wp->flagval & 1) ? 0xc0 : 0);
    }
    if (mask & 2) {
        fl = (fl & 0xfffe) | ((wp->flagval & 2) != 0);
    }
    if (mask & 4) {
        fl = (fl & 0xfffd) | ((wp->flagval & 4) ? 2 : 0);
    }
    if (mask & 0x40) {
        fl = (fl & 0xffdf) | ((wp->flagval & 0x40) ? 0x20 : 0);
    }
    if (mask & 0x20) {
        fl = (fl & 0xffef) | ((wp->flagval & 0x20) ? 0x10 : 0);
    }
    p->flags = fl;
    mask = wp->flagmask;
    nfl = node->flags;
    if (mask & 0x80) {
        nfl = (nfl & 0xff7fffff) | ((wp->flagval & 0x80) ? 0x800000 : 0);
    }
    if (mask & 0x100) {
        nfl = (nfl & 0xbfffffff) | ((wp->flagval & 0x100) ? 0x40000000 : 0);
    }
    if (mask & 0x200) {
        nfl = (nfl & 0xfffff7ff) | ((wp->flagval & 0x200) ? 0x800 : 0);
    }
    if (mask & 0x400) {
        nfl = (nfl & 0xffffffbf) | ((wp->flagval & 0x400) ? 0x40 : 0);
    }
    if (mask & 0x800) {
        nfl = (nfl & 0xffffff7f) | ((wp->flagval & 0x800) ? 0x80 : 0);
    }
    node->flags = nfl;
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
extern s8 lbl_803451B8;
extern s32 lbl_803451B4;

MBObject* MBNewWorldPsys(s32 a, s32 b, PsysDescrip* wp, s32 d, char* name,
                         f32* over) {
    Psys* p;
    MBObject* node;

    if (lbl_803451B8 == 0) {
        lbl_803451B4 = 0;
        lbl_803451B8 = 1;
    }
    node = createPsysNode(a, b, (name != NULL) ? 1 : 0, d);
    if (node == NULL) {
        return NULL;
    }
    p = (Psys*)node->data.psys;
    if (name != NULL && *name != 0) {
        p->worldname = name;
    } else {
        p->worldname = NULL;
    }
    if (wp != NULL) {
        setWorldParms(node, p, wp, over);
    }
    if (name != NULL) {
        p->flags |= 2;
    }
    return node;
}

/* 0x800CFB38 - build a psys node from a preset descriptor */
MBObject* MBNewPsysDescrip(s32 a, s32 b, s32 c, void* cfg) {
    MBObject* node = createPsysNode(a, b, 0, 1);
    if (node != NULL && cfg != NULL) {
        setWorldParms(node, (Psys*)node->data.psys, (PsysDescrip*)cfg, 0);
    }
    return node;
}

extern f64 lbl_80349298;        /* firework rate divisor */
extern f64 lbl_80349210;        /* firework power scale */

/* 0x800CFA84 - firework preset (deferred build through MBNewPsysDescrip) */
#pragma dont_inline on
MBObject* MBPsysFirework(s32 a, s32 b, s32 count, s32 m0, s32 m1, s32 m2,
                         f32 rate, f32 power, f32 sc0, f32 sc1, f32 sc2) {
    u8* pi = (u8*)psysInfo;
    MBObject* node;

    *(f32*)(pi + 3324) = sc0;
    *(f32*)(pi + 3328) = sc1;
    *(f32*)(pi + 3332) = sc1;
    *(f32*)(pi + 3336) = sc2;
    *(s32*)(pi + 3308) = m0;
    *(s32*)(pi + 3312) = m1;
    *(s32*)(pi + 3316) = m1;
    *(s32*)(pi + 3320) = m2;
    *(f32*)(pi + 3252) = (f32)(rate / lbl_80349298);
    *(u8*)(pi + 3117) = 0;
    *(u8*)(pi + 3118) = 0;
    if (count == 0) {
        *(s32*)(pi + 3148) = 100;
    } else {
        *(s32*)(pi + 3148) = count;
    }
    node = MBNewPsysDescrip(a, b, 0, pi + 3108);
    if (node != NULL) {
        *(u16*)(*(u8**)((u8*)node + 112) + 56) = (s32)(lbl_80349210 * power);
    }
    return node;
}
#pragma dont_inline off

/* 0x800CF8EC - flame preset */
MBObject* MBPsysFlame(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6, f64 f7,
                      f64 f8, s32 a, s32 tex, f32* verts, s32 b, s32 c, s32 d,
                      s32 e, s32 g) {
    return MBNewPsysDescrip((s32)f1, tex, 0, verts);
}

/* 0x800D079C - default psys node (no descriptor), stores render flags */
MBObject* MBNewPsysDefault(f64 f1, f64 f2, f64 f3, f64 f4, f64 f5, f64 f6,
                           f64 f7, f64 f8, f32* verts, f32 depth, s32 flags,
                           s32 tex, s32 a, s32 b, s32 c, s32 d) {
    MBObject* node;
    if (depth == 0.0f) {
        depth = 1.0f;   /* default draw depth (0x80344eb8) */
    }
    node = createPsysNode((s32)f1, tex, 0, 0);
    if (node != NULL) {
        node->flags = flags;
    }
    return node;
}

/* 0x800D07FC - create a scene node + attach a fresh psys descriptor */
extern f32 lbl_80349220;        /* psys default rate */

MBObject* createPsysNode(s32 a, s32 b, s32 c, s32 d) {
    u8* globals = (u8*)&lbl_80128710;
    MBObject* node;
    Psys* p;

    if (*(s32*)(globals + 88) == -1) {
        return NULL;
    }
    node = MBNewNode(b, (void*)a, 14);
    if (node == NULL) {
        return NULL;
    }
    p = allocPsys(c);
    if (p == NULL) {
        *(u32*)((u8*)node + 112) = 0;
        MBRemoveNode(node, 1);
        return NULL;
    }
    *(Psys**)((u8*)node + 112) = p;
    *(MBObject**)((u8*)p + 40) = node;
    if (d != 0) {
        if (d == -1) {
            *(u16*)((u8*)p + 44) |= 128;
        } else if (d == -2) {
            *(u16*)((u8*)p + 44) |= 64;
        } else {
            *(u16*)((u8*)p + 44) |= 192;
        }
    }
    *(s32*)((u8*)p + 144) = *(s32*)(globals + 20);
    *(u8*)((u8*)p + 55) = 0;
    *(f32*)((u8*)p + 164) = lbl_80349220;
    return node;
}

/* 0x800D08FC - allocate + default-init a psys descriptor (0x130 bytes) */
static Psys* allocPsys(s32 fromArena) {
    f64 f1 = 0.0, f2 = 0.0, f3 = 0.0, f4 = 0.0, f5 = 0.0, f6 = 0.0, f7 = 0.0,
        f8 = 0.0;
    Psys* p;
    s32 id = gPsysIdCounter++;
    if (fromArena == 0) {
        p = (Psys*)allocPsysMem(0x130, gPsysIdCounter);
    } else {
        p = (Psys*)AllocMem(0x130);
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
    struct TexPageEnt {
        u32 f0;
        u8* obj;
        u32 f8;
        u32 fC;
    };
    u8* g = (u8*)gWinGlobals;
    Psys* p = (Psys*)node->data.psys;
    struct TexPageEnt* pages;

    p->p_texidx = tex;
    pages = *(struct TexPageEnt**)(g + 48);
    p->p_tex = (struct ROMTEX*)(*(u8**)(pages[(tex >> 16) & 0xFFFF].obj + 88) +
                                (tex & 0xFFFF) * 16);
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
        freePsys(node);
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
    u32 cur;
    s32* link = (s32*)(base + 4);
    while ((cur = *link) != 0 && cur != (u32)id) {
        link = (s32*)(cur + 0x24);
    }
    return link;
}

/* 0x800D138C - freePsys: release a psys node's buffers back to the pool.
 * Documented reconstruction (NonMatching). */
#pragma opt_common_subs off
static void freePsys(MBObject* node) {
    if (*(MBObject**)((u8*)node + 40) != NULL) {
        *(void**)((u8*)*(MBObject**)((u8*)node + 40) + 112) = NULL;
        MBRemoveNode(*(MBObject**)((u8*)node + 40), 1);
        *(MBObject**)((u8*)node + 40) = NULL;
    }
    if (*((u32*)node + 1) == 0) {   /* not world-owned */
        if (*(Psys**)((u8*)node + 8) != NULL) {
            freePsysMem(*(Psys**)((u8*)node + 8));
            *(Psys**)((u8*)node + 8) = NULL;
        }
        freePsysMem(node);
    }
}
#pragma opt_common_subs reset

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
#pragma opt_lifetimes off
static void freePsysMem(void* mem) {
    PsysMemBlock* block;
    s32 nextBytes;
    s32 prevBytes;
    PsysMemBlock* next;
    PsysMemPool* pool;
    s32 bytes;
    PsysMemBlock* prev;
    PsysMemBlock* mergeNext;
    s32 error;

    block = (PsysMemBlock*)mem - 1;
    nextBytes = 0;
    prevBytes = 0;
    next = block->next;
    mem = (u8*)&lbl_80128710;
    pool = (PsysMemPool*)((u8*)mem + 0x24);
    bytes = -block->bytes;
    prev = block->prev;
    mergeNext = next;

    if (next == NULL) {
        if (pool->last != block) {
            error = 1;
            goto bad_block;
        }
        if ((u8*)block + bytes != (u8*)pool->addr + pool->pool_bytes) {
            error = 11;
            goto bad_block;
        }
    } else {
        if (next->prev != block) {
            error = 2;
            goto bad_block;
        }
        if ((u8*)next - (u8*)block != bytes) {
            error = 3;
            goto bad_block;
        }
        nextBytes = next->bytes;
        if (nextBytes <= 0) {
            if (nextBytes == 0) {
                error = 10;
                goto bad_block;
            }
            mergeNext = NULL;
        }
    }

    if (prev == NULL) {
        if (pool->frst != block) {
            error = 4;
            goto bad_block;
        }
        prevBytes = 0;
    } else {
        if (prev->next != block) {
            error = 5;
            goto bad_block;
        }
        prevBytes = prev->bytes;
        if (prevBytes <= 0) {
            if (prevBytes == 0) {
                error = 9;
                goto bad_block;
            }
            prevBytes = -prevBytes;
            prev = NULL;
        } else if ((u8*)block - (u8*)prev != prevBytes) {
            error = 6;
            goto bad_block;
        }
    }

    pool->free_bytes += bytes;
    pool->free_cnt++;
    pool->alloc_cnt--;

    if (mergeNext != NULL) {
        block->next = mergeNext->next;
        pool->next = block;
        if (block->next != NULL) {
            block->next->prev = block;
        } else {
            pool->last = block;
        }
        bytes += nextBytes;
        pool->free_cnt--;
    }

    if (prev != NULL) {
        prev->next = block->next;
        block = prev;
        pool->next = prev;
        if (prev->next != NULL) {
            prev->next->prev = prev;
        } else {
            pool->last = prev;
        }
        bytes += prevBytes;
        pool->free_cnt--;
    }

    block->bytes = bytes;
    block->id = -1;
    return;

bad_block:
    FatalErrorf(lbl_80116F30, error);
}
#pragma opt_lifetimes reset

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
extern f32 lbl_80349154;
extern u8 lbl_802C9D30[];

#pragma dont_inline on
void MBInitPsys(void) {
    u8* pi = (u8*)psysInfo;
    s32* psize = (s32*)(pi + 108);
    u8* m = pi + 64;
    s32 size;
    s32* q;

    *(s32*)(pi + 68) = 0;
    *(s32*)(pi + 64) = 0;
    *(s32*)(pi + 76) = 0;
    *(s32*)(pi + 72) = 0;
    *(s32*)(pi + 80) = 0;
    *(s32*)(pi + 92) = 0;
    *(s32*)(pi + 96) = 0;
    *(s32*)(pi + 84) = 0;
    *(f32*)(pi + 88) = lbl_80349154;
    *(s32*)(lbl_802C9D30 + 20) = 0;
    *(s32*)(pi + 100) = 0;
    size = 120000;
    *(s32*)(pi + 104) = size;
    *psize = size;
    *(s32*)(pi + 112) = 1;
    *(s32*)(pi + 116) = 0;
    *(s32*)(pi + 120) = (s32)AllocMem(size);
    *(s32*)(pi + 132) = *(s32*)(pi + 120);
    *(s32*)(pi + 124) = *(s32*)(pi + 120);
    *(s32*)(pi + 128) = *(s32*)(pi + 120);
    q = (s32*)(pi + 132);
    q = *(s32**)q;
    q[0] = *psize;
    q[1] = 0;
    q[2] = 0;
    q[3] = 0;
    if (*(s32*)(m + 72) == 0) {
        *(s32*)(m + 72) = 1023;
    }
    size = *(s32*)(m + 72);
    *(s32*)(m + 76) = (s32)AllocMem(size * 12);
    *(s32*)(m + 80) = (s32)AllocMem(size);
    memset(*(void**)(m + 80), 0, size);
    initPresetList();
}
#pragma dont_inline off
