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
void  pbBlitSetDrawRegs(u32 a, u32 b, u32 c);                   /* set blend/tev mode */
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
extern f32   gCameraMtx[16];
extern f32*  lbl_80344EE8;   /* active view/frustum basis */
extern s32   lbl_80345188;   /* quads drawn this frame */
extern f32   lbl_803451D4;   /* screen cull min x */
extern f32   lbl_803451D8;   /* screen cull max x */
extern f32   lbl_803451DC;   /* screen cull min y */
extern f32   lbl_803451E0;   /* screen cull max y */
extern f32   lbl_803451E4;   /* min quad width */
extern f32   lbl_803451E8;   /* min quad height */
extern f32 lbl_80349150;
extern const f32 lbl_8034915C;
extern f32 lbl_80349158;
extern f32 lbl_80349160;
extern const f64 lbl_80349170;
extern f32 lbl_80349178;
extern f32 lbl_8034917C;
extern f32 lbl_80349180;
extern const f32 lbl_80349184;
extern f32 lbl_80349188;
extern s32 lbl_803451C4;
extern s32 lbl_803451C8;
extern s32 lbl_803451CC;
extern s32 lbl_803451D0;
extern f32 lbl_803451EC;
extern f32 lbl_803451F0;

extern f32   psysInfo[];     /* per-parm scale/min/max config table */
extern char  lbl_80116F30[]; /* "freePsysMem: bad free block..." */
extern char  lbl_80116D70[]; /* TU string block base */

typedef struct PsysModuleGlobals {
    u8 pad00[0x54];
    s16 dirSlot;
    s16 posSlot;
} PsysModuleGlobals;

extern PsysModuleGlobals lbl_80128710; /* retail module-global data block */
typedef struct PsysPresetRecord {
    u32 id;
    u8 payload[304];
    u32 checksum;
} PsysPresetRecord;
extern PsysPresetRecord psysPresetTable[9];
extern s32 lbl_80345194;
extern s8 lbl_80345198;

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
extern f32 lbl_80349154;
extern const f64 lbl_80349168;
extern const f64 lbl_803491A0;
extern const f64 lbl_803491F8;
static f32  getSinCos(f32 ang, f32* sinOut);
static void DrawPsysSub(f32* pos, u32 color, s32 c, s32 sx, s32 sy, f32 size);
static void setupNewPMode_800CDCE4(Psys* p);
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
MBObject* MBPsysFlame(f32 f1, f32 f2, f32 f3, s32 a, s32 tex, f32* verts);

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

    last = p->pos_last;
    psys = p;
    max = p->pos_max;
    obj = node;
    idx = p->pos_next;

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
    Psys* psys;
    s32 idx;
    s32 count;
    u8* use;
    MBObject* obj;
    s32 first;
    s32 used;

    psys = p;
    use = p->pos_use_lst;
    obj = node;
    count = p->pos_max;
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
    {
        f32* slot = psys->init_pos_lst[idx];
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
    }
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
    negative = -0.16f;
    coefficient = 0.008f;
    st = s2 * coefficient;
    at = a2 * coefficient;
    coefficient = negative + st;
    negative = negative + at;
    coefficient = s2 * coefficient;
    st = a2 * negative;
    negative = 1.0f + coefficient;
    coefficient = a2 * (1.0f + st);
    negative = s2 * negative;
    *sinOut = s * negative + s;
    return ang * coefficient + ang;
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
static s32 getNewDirConeUnique(register Psys* p, register MBObject* node, s32 z) {
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
static void DrawPsysSub(f32* pos, u32 color, s32 c, s32 sx, s32 sy, f32 size) {
    f32 v[3];
    f32 corner[4];
    f32 out1[4];
    f32 out2[4];
    s32 ix, iy;
    u8 rgba[4];
    u32 cw;
    u8* win;
    f32* row;
    f32 nhs;
    f32 x1, y1;
    f32 x2, y2, z1;
    f32 d;
    f32 u0, u1, v0, v1;
    u32 a;
    u8 rr, gg, bb;
    s32 sa, sb;

    win = (u8*)gWinGlobals;
    row = lbl_80344EE8 + 25;
    if (lbl_80345188 > 2048) {
        return;
    }

    a = (color >> 24) & 0xFE;
    rr = color >> 16;
    gg = color >> 8;
    bb = color;
    if (a > 255) {
        a = 255;
    }
    rgba[0] = rr;
    rgba[1] = gg;
    rgba[2] = bb;
    rgba[3] = a;
    cw = *(u32*)rgba;
    GXSetChanMatColor(4, &cw);

    size *= 0.5;
    nhs = -size;
    v[0] = row[0] * nhs + pos[0];
    v[1] = row[1] * nhs + pos[1];
    v[2] = row[2] * nhs + pos[2];
    v[0] = row[4] * nhs + v[0];
    v[1] = row[5] * nhs + v[1];
    v[2] = row[6] * nhs + v[2];
    __as__4vec3FRC4vec3((u32)corner, (u32)v);
    sceSamp0MultVec(out1, gCameraMtx, corner);

    z1 = out1[2];
    if (z1 <= (*(f32**)(win + 4))[22]) {
        return;
    }
    if (z1 >= (*(f32**)(win + 4))[23]) {
        return;
    }
    x1 = (out1[0] * (*(f32**)(win + 4))[32]) / z1;
    y1 = (out1[1] * (*(f32**)(win + 4))[37]) / z1;
    if (x1 < lbl_803451D4) {
        return;
    }
    if (x1 >= lbl_803451D8) {
        return;
    }
    if (y1 < lbl_803451DC) {
        return;
    }
    if (y1 >= lbl_803451E0) {
        return;
    }

    v[0] = row[0] * size + pos[0];
    v[1] = row[1] * size + pos[1];
    v[2] = row[2] * size + pos[2];
    v[0] = row[4] * size + v[0];
    v[1] = row[5] * size + v[1];
    v[2] = row[6] * size + v[2];
    __as__4vec3FRC4vec3((u32)corner, (u32)v);
    sceSamp0MultVec(out2, gCameraMtx, corner);

    x2 = (out2[0] * (*(f32**)(win + 4))[32]) / out2[2];
    y2 = (out2[1] * (*(f32**)(win + 4))[37]) / out2[2];
    if (x2 < lbl_803451D4) {
        return;
    }
    if (x2 >= lbl_803451D8) {
        return;
    }
    if (y2 < lbl_803451DC) {
        return;
    }
    if (y2 >= lbl_803451E0) {
        return;
    }

    d = x2 - x1;
    if (d < 0.0f) {
        d = -d;
    }
    if (d < lbl_803451E4) {
        return;
    }
    d = y2 - y1;
    if (d < 0.0f) {
        d = -d;
    }
    if (d < lbl_803451E8) {
        return;
    }

    out1[2] = -1.0f * out1[2];
    fn_800C7914(&ix, &iy);

    sa = (s16)(s32)(sx * 16.0 + 0.5) - 8;
    sb = (s16)(s32)(sy * 16.0 + 0.5) - 8;
    u0 = (8.0f / (f32)ix) * 0.0625f;
    u1 = ((f32)sa / (f32)ix) * 0.0625f;
    v0 = (8.0f / (f32)iy) * 0.0625f;
    v1 = ((f32)sb / (f32)iy) * 0.0625f;

    GXBegin(0x98, 0, 4);
    x2 = out1[2];
    y2 = out1[1];
    z1 = out1[0];
    d = out2[0];
    nhs = out2[1];
    *(volatile f32*)0xCC008000 = z1;
    *(volatile f32*)0xCC008000 = y2;
    *(volatile f32*)0xCC008000 = x2;
    *(volatile f32*)0xCC008000 = u0;
    *(volatile f32*)0xCC008000 = v0;
    *(volatile f32*)0xCC008000 = z1;
    *(volatile f32*)0xCC008000 = nhs;
    *(volatile f32*)0xCC008000 = x2;
    *(volatile f32*)0xCC008000 = u0;
    *(volatile f32*)0xCC008000 = v1;
    *(volatile f32*)0xCC008000 = d;
    *(volatile f32*)0xCC008000 = y2;
    *(volatile f32*)0xCC008000 = x2;
    *(volatile f32*)0xCC008000 = u1;
    *(volatile f32*)0xCC008000 = v0;
    *(volatile f32*)0xCC008000 = d;
    *(volatile f32*)0xCC008000 = nhs;
    *(volatile f32*)0xCC008000 = x2;
    *(volatile f32*)0xCC008000 = u1;
    *(volatile f32*)0xCC008000 = v1;
    lbl_80345188 = lbl_80345188 + 1;
}

/* 0x800CBC4C - per-frame emitter state machine + particle draw pass.
 * Documented flow: run the emitter phase machine (psys->e_phase: 0 delay .. 8
 * dead), emit new particles via pos_func/dir_func, age the ring through
 * ppos_func and draw each live particle with DrawPsysSub. Giant (NonMatching). */
s32 MBDrawPsys(MBObject* node, void* arg) {
    typedef struct PSlot {
        f32 cur;
        f32 start;
        f32 slope;
        struct PSlot* next;
    } PSlot;
    u8* pi = (u8*)psysInfo;
    Psys* p = (Psys*)node->data.psys;
    f32 rate = lbl_80349150;
    u32 dt;
    u32 age;
    u32 ageTmp;
    f32 agef;
    u16* ringbase;
    u16* ringend;
    u16* oldest;
    u16* newest;
    u16* emitStart;
    u16* cursor;
    u16* stop;
    u16 entry;
    u32 agemask;
    s32 age_bits, dir_bits, shiftDA;
    s32 plsum;
    s32 over;
    u32 color;
    u32 cmask;
    PSlot* list;
    f32 pos[4];
    PSlot slots[5];
    f32 budget;
    s32 texw, texh;
    PsysPPosFunc ppfn;
    u16 dmask;
    u8* blk;

    *(s16*)(pi + 148) = -1;
    blk = pi + 64;
    *(s16*)(pi + 150) = -1;
    list = NULL;
    dt = *(u32*)(pi + 84) - p->e_last_time;
    if (dt > 15) {
        dt = 1;
    }
    ageTmp = p->e_age + dt;
    p->e_last_time = *(u32*)(blk + 20);
    agef = (f32)ageTmp;
    age = ageTmp;

    switch (p->e_phase) {
    case 0:
    {
        u32 d = p->e_delay;
        if (d != 0) {
            if (age <= d) {
                p->e_age = age;
                return 0;
            }
            age -= d;
            dt = age;
        }
        p->e_phase = 1;
    }
    case 1:
        setupParms(p);
        setupNewPMode_800CDCE4(p);
        p->p_oldest_ptr = NULL;
        p->p_oldest_age = 0;
        p->p_newest_age = 0;
        p->p_newest_ptr = (u16*)((u8*)p->p_lst + p->p_max * 2 - 2);
        p->p_save_cnt = lbl_80349154;
        p->nearest_z = lbl_80349158;
        if (p->p_lst == NULL) {
            s32* h = listFindHandle((s32)p, (s32)(pi + 64));
            *(s32*)(pi + 64) = *(s32*)(pi + 64) - 1;
            *h = (s32)p->next;
            p->next = NULL;
            *(s32*)(pi + 72) = *(s32*)(pi + 72) + 1;
            p->next = *(Psys**)(pi + 76);
            *(Psys**)(pi + 76) = p;
            p->flags |= 0x8000;
            p->e_phase = 8;
            p->e_phase = 8;
            return 0;
        }
        if (dt == 0) {
            dt = 1;
            age += 1;
            agef = agef + lbl_8034915C;
        }
        p->e_phase = 2;
        if (p->flags & 1) {
            rate = lbl_80349160;
            dt = 1;
            p->e_phase = 6;
            age = 1;
            goto phaseD;
        }
        if ((p->flags & 2) && p->e_life == 0xFFFF) {
            p->e_phase = 4;
    case 4:
            rate = p->e_rate.o.life_start;
            goto phaseD;
        }
    case 2:
        if (age <= p->e_life) {
            rate = p->e_rate.o.life_slope * agef + p->e_rate.o.life_start;
            goto phaseD;
        }
        if ((p->flags & 2) && p->e_fade == 0xFFFF) {
            p->e_phase = 5;
    case 5:
            rate = p->e_rate.o.fade_start;
            goto phaseD;
        }
        p->e_phase = 3;
    case 3:
        if (age <= (u32)p->e_life + p->e_fade) {
            rate = p->e_rate.o.fade_slope * agef + p->e_rate.o.fade_start;
            goto phaseD;
        }
        if (p->flags & 2) {
            p->e_phase = 2;
            age = 0;
            rate = p->e_rate.o.life_start;
            goto phaseD;
        }
        p->e_phase = 6;
    case 6:
        p->p_save_cnt = rate = lbl_80349154;
        if (p->p_oldest_ptr == NULL) {
    case 7:
        {
            s32* h = listFindHandle((s32)p, (s32)(pi + 64));
            *(s32*)(pi + 64) = *(s32*)(pi + 64) - 1;
            *h = (s32)p->next;
            p->next = NULL;
            *(s32*)(pi + 72) = *(s32*)(pi + 72) + 1;
            p->next = *(Psys**)(pi + 76);
            *(Psys**)(pi + 76) = p;
            p->flags |= 0x8000;
            p->e_phase = 8;
            p->e_phase = 8;
        }
    case 8:
            return 0;
        }
        break;
    }
phaseD:
    if (p->e_isvis == 0 || (node->flags & 0x200000)) {
        rate = lbl_80349154;
    }
    p->e_age = age;
    ringbase = p->p_lst;
    ringend = ringbase + p->p_max;
    if (rate > lbl_80349154) {
        f32 rr = p->e_rate_rand;
        if (rr > lbl_80349154) {
            u32 r = pbRand() & 0x7FFF;
            rate = (f32)(rate * (rr * (lbl_80349170 * (f32)r - 1.0) + 1.0));
        }
    }
    {
        u32 nfl = node->flags;
        u32 m;
        if (p->p_tex != NULL) {
            pbBlitSetTexture(p->p_texidx);
            texw = *(u16*)((u8*)p->p_tex + 10);
            texh = *(u16*)((u8*)p->p_tex + 12);
        } else {
            texw = 16;
            texh = 16;
        }
        m = 0;
        if (p->flags & 0x20) {
            m |= 0x100000;
        } else if (p->flags & 0x10) {
            m |= 0x200000;
        }
        pbBlitSetDrawRegs(m, nfl, 0);
    }
    plsum = (u32)p->p_life + p->p_fade;
    age_bits = p->age_bits;
    dir_bits = p->dir_bits;
    dmask = (1 << dir_bits) - 1;
    agemask = (1 << age_bits) - 1;
    shiftDA = dir_bits + age_bits;
    newest = p->p_newest_ptr;
    oldest = p->p_oldest_ptr;
    over = (s32)(dt + p->p_oldest_age) - plsum;
    if (over >= 0 && oldest != NULL) {
        entry = *oldest;
        stop = (newest >= oldest) ? newest : ringend - 1;
        do {
            if (p->pos_use_lst != NULL) {
                p->pos_use_lst[(s32)entry >> shiftDA] -= 1;
            }
            if (p->dir_use_lst != NULL) {
                p->dir_use_lst[dmask & ((s32)entry >> age_bits)] -= 1;
            }
            if (oldest == stop) {
                if (oldest == newest) {
                    newest = ringend - 1;
                    oldest = NULL;
                    break;
                }
                oldest = ringbase - 1;
                stop = newest;
            }
            oldest += 1;
            entry = *oldest;
            over -= (agemask & 0xFFFF) & entry;
        } while (over >= 0);
        if (oldest != NULL) {
            p->pos_last = (s32)entry >> shiftDA;
            p->dir_last = dmask & ((s32)entry >> age_bits);
        } else {
            p->pos_last = 0xFFFF;
            p->dir_last = 0xFFFF;
        }
    }
    p->p_oldest_age = plsum + over;
    budget = rate * (f32)dt + p->p_save_cnt;
    stop = (oldest > newest) ? oldest : ringend;
    emitStart = (newest == ringend - 1) ? ringbase : newest + 1;
    cursor = emitStart;
    if (cursor == oldest) {
        p->p_save_cnt = lbl_80349154;
        p->p_newest_age = p->p_newest_age + dt;
    } else {
        f32 startBudget = budget;
        u8 unused[8];
        f32 zero = lbl_80349154;
        while (budget > zero) {
            s32 posIdx;
            s32 dirIdx;
            if ((posIdx = ((s32(*)(Psys*,void*,s32))p->pos_func)(p, arg, 0)) < 0) {
                break;
            }
            dirIdx = ((s32(*)(Psys*,void*,s32))p->dir_func)(p, arg, 0);
            if (dirIdx < 0) {
                if (p->pos_use_lst != NULL) {
                    p->pos_use_lst[dirIdx] -= 1;
                }
                break;
            }
            budget = (f32)(budget - 1.0);
            *cursor = (posIdx << shiftDA) | (dirIdx << age_bits);
            cursor += 1;
            if (cursor == stop) {
                if (cursor != ringend) {
                    break;
                }
                if (oldest == NULL) {
                    oldest = emitStart;
                }
                if (oldest == ringbase) {
                    break;
                }
                cursor = ringbase;
                stop = oldest;
            }
        }
        p->p_save_cnt = (budget > lbl_80349154) ? lbl_80349154 : budget;
        if (budget == startBudget) {
            p->p_newest_age = p->p_newest_age + dt;
        } else {
            u16* scan;
            u16* limit;
            s32 filled = 0;
            f32 acc;
            u32 am16;
            if (oldest == NULL) {
                oldest = emitStart;
            }
            p->p_newest_ptr = (cursor == ringbase) ? ringend - 1 : cursor - 1;
            acc = rate;
            scan = cursor;
            limit = (emitStart < cursor) ? emitStart : ringbase - 1;
            am16 = agemask & 0xFFFF;
            while (1) {
                scan -= 1;
                acc = acc - lbl_8034915C;
                if (scan == limit) {
                    if (scan == emitStart) {
                        break;
                    }
                    scan = ringend - 1;
                    limit = emitStart;
                    if (scan == emitStart) {
                        break;
                    }
                }
                if (acc <= lbl_80349154) {
                    s32 n = 0;
                    do {
                        acc = acc + rate;
                        n += 1;
                        filled += 1;
                    } while (acc <= lbl_80349154 && n != (s32)am16);
                    *scan = *scan | n;
                }
            }
            if (oldest == emitStart) {
                p->p_oldest_age = filled;
                p->p_newest_age = 0;
            } else {
                s32 carry;
                u32 e;
                u16* lim2;
                carry = p->p_newest_age;
                p->p_newest_age = 0;
                carry = (carry + dt) - filled;
                e = *scan;
                lim2 = (cursor <= scan) ? ringend : cursor;
                if (carry > (s32)am16) {
                    do {
                        *scan = e | am16;
                        scan += 1;
                        if (scan == lim2) {
                            if (scan == cursor) {
                                e = 0;
                                carry = am16 & *scan;
                                p->p_newest_age = carry;
                                break;
                            }
                            lim2 = cursor;
                            scan = ringbase;
                        }
                        e = *scan;
                        carry = carry + ((am16 & e) - am16);
                    } while (carry > (s32)am16);
                    e = e & ~am16;
                }
                *scan = e | (u16)carry;
            }
        }
    }
    p->p_oldest_ptr = oldest;
    {
        u16* prevOld = oldest - 1;
        f32 plf = (f32)(u32)p->p_life;
        f32 page;
        u32 e8;
        u16* wrapStop;
        color = 0xFFFFFFFF;
        cmask = 0;
        page = (f32)(u32)p->p_newest_age;
        cursor = p->p_newest_ptr;
        if (oldest > cursor) {
            wrapStop = ringbase - 1;
        } else {
            wrapStop = prevOld;
        }
        e8 = *cursor;
        if (page < plf) {
            PSlot* sl = &slots[4];
            f32* src = (f32*)&p->p_parms[4];
            list = NULL;
            cmask = 0;
            do {
                if (*(s32*)(src + 1) == 0) {
                    sl->cur = src[0];
                } else {
                    sl->start = src[0];
                    sl->slope = src[1];
                    sl->next = list;
                    list = sl;
                    if (sl <= &slots[3]) {
                        cmask = cmask | (0xFF << (sl - slots) * 8);
                    }
                }
                src -= 4;
            } while (sl-- != &slots[0]);
            color = ((s32)(p->p_parms[1].k.life_value) << 8) |
                    (s32)(p->p_parms[0].k.life_value) |
                    ((s32)(p->p_parms[2].k.life_value) << 16) |
                    ((s32)(p->p_parms[3].k.life_value) << 24);
        }
        ppfn = p->ppos_func;
        {
            u8* g = (u8*)gWinGlobals;
            u8* w = *(u8**)(g + 56);
            lbl_803451C4 = *(s32*)(w + 8) - 256;
            lbl_803451C8 = *(s32*)(*(u8**)(g + 56) + 8) +
                           *(s32*)(*(u8**)(g + 56) + 16) + 256;
            lbl_803451CC = *(s32*)(*(u8**)(g + 56) + 12) - 256;
            lbl_803451D0 = *(s32*)(*(u8**)(g + 56) + 12) +
                           *(s32*)(*(u8**)(g + 56) + 20) - 256;
            lbl_803451EC = lbl_80349178 /
                           ((f32)(*(s32*)(*(u8**)(g + 56) + 16)) * lbl_8034917C *
                            lbl_80349180);
            lbl_803451E4 = lbl_803451EC * lbl_8034917C;
            lbl_803451D4 = lbl_80349184 - lbl_803451EC;
            lbl_803451D8 = lbl_8034915C + lbl_803451EC;
            lbl_803451F0 = lbl_80349178 /
                           ((f32)(*(s32*)(*(u8**)(g + 56) + 20)) * lbl_8034917C *
                            lbl_80349180);
            lbl_803451E8 = lbl_803451F0 * lbl_8034917C;
            lbl_803451DC = lbl_80349184 - lbl_803451F0;
            lbl_803451E0 = lbl_8034915C + lbl_803451F0;
        }
        p->p_nactive = 0;
        if (oldest != NULL) {
            do {
                u32 ent = e8 & 0xFFFF;
                s32 dIdx = (s32)ent >> age_bits;
                s32 pIdx = (s32)ent >> shiftDA;
                u32 di = dmask & dIdx;
                if (page >= plf) {
                    PSlot* sl = &slots[4];
                    s32* src = (s32*)&p->p_parms[4];
                    plf = lbl_80349188;
                    cmask = 0;
                    list = NULL;
                    do {
                        if (src[3] == 0) {
                            sl->cur = *(f32*)(src + 2);
                        } else {
                            sl->start = *(f32*)(src + 2);
                            sl->slope = *(f32*)(src + 3);
                            sl->next = list;
                            list = sl;
                            if (sl <= &slots[3]) {
                                cmask = cmask | (0xFF << (sl - slots) * 8);
                            }
                        }
                        src -= 4;
                    } while (sl-- != &slots[0]);
                    color = ((s32)(p->p_parms[1].k.fade_value) << 8) |
                            (s32)(p->p_parms[0].k.fade_value) |
                            ((s32)(p->p_parms[2].k.fade_value) << 16) |
                            ((s32)(p->p_parms[3].k.fade_value) << 24);
                }
                ppfn(p, pos, p->init_dir_lst[di], p->init_pos_lst[pIdx], page);
                pos[3] = lbl_8034915C;
                SetMultiPassTextureParams(0);
                SetVertexFormat(2);
                SetCullMode(0);
                SetPerspectiveMode(1);
                SetViewportHeight(gVpScaleY);
                {
                    PSlot* sl = list;
                    if (cmask != 0) {
                        color = color & ~cmask;
                        while (sl != NULL && sl <= &slots[3]) {
                            s32 v = (s32)(sl->slope * page + sl->start);
                            color = color | (v << (sl - slots) * 8);
                            sl = sl->next;
                        }
                    }
                    for (; sl != NULL; sl = sl->next) {
                        sl->cur = sl->slope * page + sl->start;
                    }
                }
                p->p_nactive = p->p_nactive + 1;
                DrawPsysSub(pos, color, p->p_texidx, texw, texh,
                            slots[4].cur / *(f32*)(pi + 284) * node->scale[1]);
                cursor -= 1;
                if (cursor == wrapStop) {
                    if (cursor == prevOld) {
                        break;
                    }
                    cursor = ringend - 1;
                    wrapStop = prevOld;
                }
                e8 = *cursor;
                page = page + (f32)(s32)(agemask & 0xFFFF & ent);
            } while (1);
        }
    }
    return 0;
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

#pragma opt_propagation off
static void setupParms(Psys* p) {
    f32 lifeStart;
    f32 lifeEnd;
    f32 fadeStart;
    f32 fadeEnd;
    f32 fadeRate;
    f32 emitterLifeRate;
    f32 emitterFadeRate;
    f32 fadeProduct;
    f32 maxWidth;
    u8* win;
    u8* globals;
    PsysParm* parm;

    win = (u8*)gWinGlobals;
    if (p->e_life == 0) {
        p->e_life = 1;
    }

    if (p->e_fade == 0) {
        fadeRate = lbl_80349154;
    } else {
        fadeRate = (f32)(lbl_80349168 / (f64)p->e_fade);
    }
    emitterFadeRate = fadeRate;

    lifeStart = p->e_rate.i.life_start;
    lifeEnd = p->e_rate.i.life_end;
    fadeStart = p->e_rate.i.fade_start;
    fadeEnd = p->e_rate.i.fade_end;
    emitterLifeRate = (f32)(lbl_80349168 / (f64)p->e_life);
    fadeProduct = (f32)p->e_life * emitterFadeRate;
    p->e_rate.o.life_start = lifeStart;
    p->e_rate.o.life_slope = emitterLifeRate * (lifeEnd - lifeStart);

    if (lbl_803491F8 == emitterFadeRate) {
        p->e_rate.o.fade_start = fadeStart;
        p->e_rate.o.fade_slope = 0.0f;
    } else {
        p->e_rate.o.fade_start = fadeProduct * (fadeStart - fadeEnd) + fadeStart;
        p->e_rate.o.fade_slope = emitterFadeRate * (fadeEnd - fadeStart);
    }

    maxWidth = p->p_parms[4].i.life_start;
    lifeEnd = p->p_parms[4].i.life_end;
    maxWidth = lifeEnd > maxWidth ? lifeEnd : maxWidth;
    fadeStart = p->p_parms[4].i.fade_start;
    fadeStart = fadeStart > maxWidth ? fadeStart : maxWidth;
    fadeEnd = p->p_parms[4].i.fade_end;
    fadeEnd = fadeEnd > fadeStart ? fadeEnd : fadeStart;
    p->max_width = (f32)(lbl_803491A0 * fadeEnd);

    {
        f32 pLifeStart;
        f32 pLifeEnd;
        f32 pLifeRate;
        f32 pFadeRate;
        f32 pFadeProduct;
        f32 pFadeStart;
        f32 pFadeEnd;

        if (p->p_life == 0) {
            p->p_life = 1;
        }

        if (p->p_fade == 0) {
            pFadeRate = lbl_80349154;
        } else {
            pFadeRate = (f32)(lbl_80349168 / (f64)p->p_fade);
        }
        pLifeRate = (f32)(lbl_80349168 / (f64)p->p_life);
        pFadeProduct = (f32)p->p_life * pFadeRate;

        parm = &p->p_parms[5];
        while (parm-- != &p->p_parms[0]) {
            pLifeStart = parm->i.life_start;
            pLifeEnd = parm->i.life_end;
            pFadeStart = parm->i.fade_start;
            pFadeEnd = parm->i.fade_end;

            if (lbl_803491F8 == pLifeRate || pLifeStart == pLifeEnd) {
                parm->o.life_start = pLifeStart;
                parm->k.life_anim = 0;
            } else {
                parm->o.life_start = pLifeStart;
                parm->o.life_slope = pLifeRate * (pLifeEnd - pLifeStart);
            }

            if (lbl_803491F8 == pFadeRate || pFadeStart == pFadeEnd) {
                parm->o.fade_start = pFadeStart;
                parm->k.fade_anim = 0;
            } else {
                parm->o.fade_start = pFadeProduct * (pFadeStart - pFadeEnd) + pFadeStart;
                parm->o.fade_slope = pFadeRate * (pFadeEnd - pFadeStart);
            }
        }
    }

    if (p->p_tex == NULL) {
        globals = (u8*)&lbl_80128710;
        p->p_tex = *(struct ROMTEX**)(globals + 0x1c);
        if ((((MBObject*)p->node)->flags & 0x00800000) != 0 &&
            *(struct ROMTEX**)(globals + 0x20) != NULL) {
            p->p_tex = *(struct ROMTEX**)(globals + 0x20);
        }
        if (p->p_tex == NULL) {
            p->p_tex = *(struct ROMTEX**)(*(u8**)(*(u8**)(win + 0x30) + 4) + 0x58);
            p->flags |= 0x30;
        }
    }
}
#pragma opt_propagation reset

/* 0x800CDCE4 - choose spawn generators + size the ring/index/usage buffers.
 * Wires dir_func/pos_func/ppos_func based on the emit distribution and
 * animation flags, then carves the per-psys buffers out of the block pool (or
 * the world arena). Giant (NonMatching); documented flow. */
static void setupNewPMode_800CDCE4(Psys* p) {
    u8 unused[88];
    f32 c0;
    f32 d0;
    f32 a0;
    f32 b0;
    f32 est = lbl_80349154;
    f32 elife;
    f32 efade;
    f32 pl = lbl_80349154;
    f32 el;
    char* strs = (char*)lbl_80116D70;
    s32*  pi = (s32*)psysInfo;
    u16 fl = p->flags;
    s32 n;
    s32 posUse, hasDir, dirUse;
    s32 nPos, nDir;
    s32 ringHalf, total;
    u8* buf;
    s32 posCap, dirCap;
    void* dirFn;
    void* posFn;
    s32 posMode, dirMode;
    s32 mode0, flag40, r8;
    s32 vf, va;
    s32 bits;
    u8* nxt;
    s32 b;
    s32 one;
    u16 pm;
    u16 dm;
    s32 ab;

    if ((fl & 1) || p->p_max != 0) {
        if ((u32)(n = p->p_max) == 0) {
            n = (s32)(30.0 * p->e_rate.o.life_start);
        }
    } else {
        f32 part1;
        s32 n32;
        s32 limE;

        pl = (f32)p->p_life + (f32)p->p_fade;
        el = (f32)p->e_life + (f32)p->e_fade;
        elife = (f32)p->e_life;
        efade = (f32)p->e_fade;
        a0 = p->e_rate.o.life_start;
        b0 = p->e_rate.o.fade_start;
        c0 = p->e_rate.o.life_slope;
        d0 = p->e_rate.o.fade_slope;
        part1 = el;
        if (el > pl) {
            if (c0 <= 0.0f && d0 < 0.0f) {
                if (pl > elife) {
                    f32 rem = pl - elife;
                    f32 t0 = c0 * elife;
                    f32 t1 = d0 * rem;
                    f32 q0 = (f32)(0.5 * t0 + a0);
                    f32 q1 = (f32)(0.5 * t1 + b0);
                    q1 = q1 * rem;
                    est = (q0 * elife + q1) / pl;
                } else {
                    f32 t0 = c0 * pl;
                    est = (f32)(0.5 * t0 + a0);
                }
            } else if (c0 <= 0.0f) {
                if (pl > elife) {
                    f32 rem = pl - elife;
                    f32 t0 = c0 * elife;
                    f32 t1 = d0 * rem;
                    f32 q0 = (f32)(0.5 * t0 + a0);
                    f32 q1 = (f32)(0.5 * t1 + b0);
                    q1 = q1 * rem;
                    part1 = (q0 * elife + q1) / pl;
                } else {
                    f32 t0 = c0 * pl;
                    part1 = (f32)(0.5 * t0 + a0);
                }
                if (pl > efade) {
                    f32 rem = pl - efade;
                    f32 nc = -c0;
                    f32 t1 = d0 * efade;
                    f32 mid = c0 * elife + a0;
                    f32 t2 = nc * rem;
                    f32 q1 = (f32)(0.5 * t1 + b0);
                    f32 q2 = (f32)(0.5 * t2 + mid);
                    q1 = q1 * efade;
                    est = (q2 * rem + q1) / pl;
                } else {
                    f32 nd = -d0;
                    f32 mid = d0 * efade + b0;
                    nd = nd * pl;
                    est = (f32)(0.5 * nd + mid);
                }
                if (part1 > est) {
                    est = part1;
                }
            } else if (d0 < 0.0f) {
                f32 up;
                f32 dn;
                f32 x = d0 / b0;
                x = -x;
                up = pl * x;
                dn = pl - up;
                if (up > elife) {
                    up = elife;
                    dn = pl - elife;
                } else if (dn > efade) {
                    dn = efade;
                    up = pl - efade;
                }
                {
                    f32 nc = -c0;
                    f32 t1 = d0 * dn;
                    f32 mid = c0 * elife + a0;
                    f32 t2 = nc * up;
                    f32 q1 = (f32)(0.5 * t1 + b0);
                    f32 q2 = (f32)(0.5 * t2 + mid);
                    q1 = q1 * dn;
                    est = (q2 * up + q1) / pl;
                }
            } else {
                if (pl > efade) {
                    f32 rem = pl - efade;
                    f32 nc = -c0;
                    f32 t1 = d0 * efade;
                    f32 mid = c0 * elife + a0;
                    f32 t2 = nc * rem;
                    f32 q1 = (f32)(0.5 * t1 + b0);
                    f32 q2 = (f32)(0.5 * t2 + mid);
                    q1 = q1 * efade;
                    est = (q2 * rem + q1) / pl;
                } else {
                    f32 nd = -d0;
                    f32 mid = d0 * efade + b0;
                    nd = nd * pl;
                    est = (f32)(0.5 * nd + mid);
                }
            }
        } else {
            f32 t1 = d0 * efade;
            f32 t0 = c0 * elife;
            pl = part1;
            {
                f32 q1 = (f32)(0.5 * t1 + b0);
                f32 q0 = (f32)(0.5 * t0 + a0);
                q1 = q1 * efade;
                q0 = q0 * elife + q1;
                est = q0 / part1;
            }
        }
        n32 = (s32)(est * pl);
        limE = p->p_max;
        if ((u32)limE == 0) {
            limE = 300;
        }
        if (n32 < limE) {
            limE = n32;
        }
        n = limE;
    }
    if (n < 1) {
        n = 1;
    }

    mode0 = (fl & 0x80) ? 1 : 0;
    if (0.0 == p->e_angle) {
        dirMode = 0;
    } else if (-1.0 == p->e_angle) {
        dirMode = 2;
    } else {
        dirMode = 4;
    }
    vf = 1;
    va = vf;
    dirMode |= mode0;
    if (0.0 == p->e_vol[0] && 0.0 == p->e_vol[1]) {
        va = 0;
    }
    if ((u8)va == 0) {
        if (0.0 == p->e_vol[2]) {
            vf = 0;
        }
    }
    r8 = (u8)vf;
    flag40 = (fl & 0x40) ? 1 : 0;
    posMode = (r8 != 0) ? 2 : 0;
    posMode |= flag40;
    if (fl & 1) {
        s32 dm2;
        if (0.0 == p->e_angle) {
            dm2 = 0;
        } else if (-1.0 == p->e_angle) {
            dm2 = 3;
        } else {
            dm2 = 5;
        }
        dirMode = dm2;
        if (r8 != 0) {
            dm2 = 3;
        } else {
            dm2 = 0;
        }
        posMode = dm2;
    }

    pm = p->pos_max;
    posUse = 0;
    posCap = 100;
    if (pm != 0) {
        posCap = pm;
    }
    if (posCap > n) {
        posCap = n;
    }
    switch (posMode) {
    case 0:
    {
        void* t = (void*)getNewPosSingle1;
        p->flags |= 4;
        posFn = t;
        nPos = 1;
        break;
    }
    case 1:
        nPos = (s32)pl;
        p->flags |= 4;
        posFn = (void*)getNewPosFrame;
        break;
    case 2:
    case 4:
    {
        s32 t2;
        if (pl < 10.0f) {
            t2 = n;
        } else {
            t2 = (s32)(10.0f * est);
        }
        if (t2 < 8) {
            t2 = 8;
        }
        if (t2 < 1) {
            nPos = 1;
        } else if (t2 > n) {
            nPos = n;
        } else {
            nPos = t2;
        }
        if (pm != 0) {
            nPos = pm;
        }
    }
        p->flags |= 4;
        posFn = (void*)getNewPosRectUnique;
        posUse = 1;
        break;
    case 3:
    case 5:
    default:
        p->flags |= 4;
        posFn = (void*)getNewPosRectShare;
        nPos = n;
        break;
    }
    if (nPos > posCap) {
        nPos = posCap;
    }

    dm = p->dir_max;
    hasDir = 1;
    dirUse = 0;
    dirCap = 31;
    if (dm != 0) {
        dirCap = dm;
    }
    if (dirCap > n) {
        dirCap = n;
    }
    switch (dirMode) {
    case 0:
        p->flags |= 8;
        dirFn = (void*)getNewDirSingle1;
        nDir = 1;
        break;
    case 1:
        nDir = (s32)pl;
        p->flags |= 8;
        dirFn = (void*)getNewDirFrame;
        break;
    case 2:
    case 3:
        nDir = pi[34];
        dirFn = (void*)getNewDirSphere;
        p->init_dir_lst = (f32(*)[3])pi[35];
        hasDir = 0;
        dirCap = nDir;
        p->dir_use_lst = (u8*)pi[36];
        break;
    case 4:
    {
        s32 t3;
        if (pl < 10.0f) {
            t3 = n;
        } else {
            t3 = (s32)(10.0f * est);
        }
        if (t3 < 8) {
            t3 = 8;
        }
        if (t3 < 1) {
            nDir = 1;
        } else if (t3 > n) {
            nDir = n;
        } else {
            nDir = t3;
        }
    }
        if (dm != 0) {
            nDir = dm;
        }
        p->flags |= 8;
        dirFn = (void*)getNewDirConeUnique;
        dirUse = 1;
        break;
    case 5:
    default:
        p->flags |= 8;
        dirFn = (void*)getNewDirConeShare;
        nDir = n;
        break;
    }
    if (nDir > dirCap) {
        nDir = dirCap;
    }
    if (nDir < 1) {
        nDir = 1;
    }
    if (nDir > 31) {
        nDir = 31;
    }

    {
        u16 pm2 = p->pos_max;
        s32 lim2;
        if (pm2 == 0 && nPos > 100) {
            nPos = 100;
        }
        if (pm2 == 0 && nPos > 100) {
            nPos = 100;
        }
        lim2 = 100;
        if (pm2 != 0) {
            lim2 = pm2;
        }
        if (nPos >= lim2) {
            nPos = lim2;
        }
    }

    bits = 0;
    if (p->init_dir_lst == (f32(*)[3])pi[35]) {
        bits |= 1;
    }
    if (0.0 != p->p_drag) {
        bits |= 2;
    }
    if (0.0 != p->p_gravity) {
        bits |= 4;
    }
    switch (bits) {
    case 0: p->ppos_func = getPPosLinear;    break;
    case 1: p->ppos_func = getPPosSpeed;     break;
    case 2: p->ppos_func = getPPosLinear;    break;
    case 3: p->ppos_func = getPPosSpeed;     break;
    case 4: p->ppos_func = getPPosGrav;      break;
    case 5: p->ppos_func = getPPosSpeedGrav; break;
    case 6: p->ppos_func = getPPosGrav;      break;
    case 7: p->ppos_func = getPPosSpeedGrav; break;
    }

    p->p_max = n;
    ringHalf = (n + 1) & ~1;
    p->pos_max = nPos;
    p->dir_max = nDir;
    p->pos_func = (PsysPosFunc)posFn;
    p->dir_func = (PsysDirFunc)dirFn;
    if (hasDir != 0) {
        hasDir = nDir;
    }
    if (posUse != 0) {
        posUse = nPos;
    }
    if (dirUse != 0) {
        dirUse = nDir;
    }
    total = ringHalf * 2;
    total += hasDir * 12;
    total += nPos * 12;
    total += dirUse;
    total += posUse;
    if (p->worldname != NULL) {
        buf = AllocMem(total);
        pi[25] += total;
    } else {
        buf = allocPsysMem(total, p->id);
    }
    if (buf == NULL) {
        if (p->worldname != NULL) {
            ErrorPrintf(strs + 176, p->worldname);
        } else {
            ErrorPrintf(strs + 204, p->id);
        }
        p->p_lst = NULL;
        return;
    }
    memset(buf, 0, total);
    p->p_lst = (u16*)buf;
    nxt = (u8*)p->p_lst + ringHalf * 2;
    if (hasDir != 0) {
        p->init_dir_lst = (f32(*)[3])nxt;
        nxt = (u8*)p->init_dir_lst + (u32)hasDir * 12;
    }
    if (nPos != 0) {
        p->init_pos_lst = (f32(*)[3])nxt;
        nxt = (u8*)p->init_pos_lst + (u32)nPos * 12;
    }
    if (dirUse != 0) {
        p->dir_use_lst = nxt;
        nxt = p->dir_use_lst + dirUse;
    }
    if (posUse != 0) {
        p->pos_use_lst = nxt;
    }
    one = 1;
    for (b = 0; (one << b) < nDir + 1; b++) {
    }
    p->dir_bits = b;
    for (b = 0; (one << b) < nPos + 1; b++) {
    }
    p->pos_bits = b;
    ab = 16 - p->dir_bits - p->pos_bits;
    p->age_bits = ab;
    if (ab < 1) {
        ErrorPrintf(strs + 228, p->pos_bits, p->dir_bits,
                    p->dir_bits + p->pos_bits + 1, 16);
        {
            Psys* q = (Psys*)((MBObject*)p->node)->data.psys;
            if (q->e_phase < 7) {
                q->e_phase = 7;
            }
        }
    } else {
        p->dir_next = nDir - 1;
        p->pos_next = nPos - 1;
        p->dir_last = 0xFFFF;
        p->pos_last = 0xFFFF;
    }
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

extern const f64 lbl_80349260;   /* 1/255 double */

static void setWorldParms(MBObject* node, Psys* p, PsysDescrip* wpd, f32* over) {
    WorldPsys* wp = (WorldPsys*)wpd;
    char* strs = (char*)lbl_80116D70;
    f32* pif = (f32*)psysInfo;
    f32 t0, t1, t2, t3;
    f64 v;
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
        ErrorPrintf(strs + 276);
        return;
    }

    if (wp->used & 0x1) {   /* inherit from a built-in preset first */
        s32 i;
        s16 id;
        u8* pre;
        for (i = 0; (pre = (u8*)pif + i), (id = *(s16*)(pre + 304)),
                    (pre += 300), id != -1; i += 312) {
            if (wp->preset == id) {
                setWorldParms(node, p, (PsysDescrip*)pre, 0);
                break;
            }
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
        p->e_delay = (u16)(s32)((v > 0.0) ? v : 0.0);
    }
    if (wp->used & 0x80) {
        p->e_dir[0] = wp->e_dir[0];
        p->e_dir[1] = wp->e_dir[1];
        p->e_dir[2] = wp->e_dir[2];
    } else if (over != 0 &&
               (over[0] != 0.0f || over[1] != 0.0f || over[2] != 0.0f)) {
        p->e_dir[0] = over[0];
        p->e_dir[1] = over[1];
        p->e_dir[2] = over[2];
    } else {
        p->e_dir[0] = 0.0f;
        p->e_dir[1] = 1.0f;
        p->e_dir[2] = 0.0f;
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
        t3 = (f32)(lbl_80349260 * (f32)(wp->rgba[3] >> 16 & 0xff));
        t2 = (f32)(lbl_80349260 * (f32)(wp->rgba[2] >> 16 & 0xff));
        t1 = (f32)(lbl_80349260 * (f32)(wp->rgba[1] >> 16 & 0xff));
        t0 = (f32)(lbl_80349260 * (f32)(wp->rgba[0] >> 16 & 0xff));
        if (q->e_phase > 1) {
            ErrorPrintf(strs + 316);
        } else {
            f32 sc = pif[63];
            f32 lo = pif[64];
            f32 hi = pif[65];
            {
                f32 fv = t0 * sc;
                if (fv < lo) {
                    fv = lo;
                } else if (fv > hi) {
                    fv = hi;
                }
                q->p_parms[2].i.life_start = fv;
            }
            {
                f32 fv = t1 * sc;
                if (fv < lo) {
                    fv = lo;
                } else if (fv > hi) {
                    fv = hi;
                }
                q->p_parms[2].i.life_end = fv;
            }
            {
                f32 fv = t2 * sc;
                if (fv < lo) {
                    fv = lo;
                } else if (fv > hi) {
                    fv = hi;
                }
                q->p_parms[2].i.fade_start = fv;
            }
            t3 = t3 * sc;
            if (!(t3 < lo)) {
                if (!(t3 > hi)) {
                    hi = t3;
                }
                lo = hi;
            }
            q->p_parms[2].i.fade_end = lo;
        }
        /* green (lane 8) */
        q = (Psys*)node->data.psys;
        t3 = (f32)(lbl_80349260 * (f32)(wp->rgba[3] >> 8 & 0xff));
        t2 = (f32)(lbl_80349260 * (f32)(wp->rgba[2] >> 8 & 0xff));
        t1 = (f32)(lbl_80349260 * (f32)(wp->rgba[1] >> 8 & 0xff));
        t0 = (f32)(lbl_80349260 * (f32)(wp->rgba[0] >> 8 & 0xff));
        if (q->e_phase > 1) {
            ErrorPrintf(strs + 316);
        } else {
            f32 sc = pif[59];
            f32 lo = pif[60];
            f32 hi = pif[61];
            {
                f32 fv = t0 * sc;
                if (fv < lo) {
                    fv = lo;
                } else if (fv > hi) {
                    fv = hi;
                }
                q->p_parms[1].i.life_start = fv;
            }
            {
                f32 fv = t1 * sc;
                if (fv < lo) {
                    fv = lo;
                } else if (fv > hi) {
                    fv = hi;
                }
                q->p_parms[1].i.life_end = fv;
            }
            {
                f32 fv = t2 * sc;
                if (fv < lo) {
                    fv = lo;
                } else if (fv > hi) {
                    fv = hi;
                }
                q->p_parms[1].i.fade_start = fv;
            }
            t3 = t3 * sc;
            if (!(t3 < lo)) {
                if (!(t3 > hi)) {
                    hi = t3;
                }
                lo = hi;
            }
            q->p_parms[1].i.fade_end = lo;
        }
        /* blue (lane 0) */
        q = (Psys*)node->data.psys;
        t3 = (f32)(lbl_80349260 * (f32)(wp->rgba[3] & 0xff));
        t2 = (f32)(lbl_80349260 * (f32)(wp->rgba[2] & 0xff));
        t1 = (f32)(lbl_80349260 * (f32)(wp->rgba[1] & 0xff));
        t0 = (f32)(lbl_80349260 * (f32)(wp->rgba[0] & 0xff));
        if (q->e_phase > 1) {
            ErrorPrintf(strs + 316);
        } else {
            f32 sc = pif[55];
            f32 lo = pif[56];
            f32 hi = pif[57];
            {
                f32 fv = t0 * sc;
                if (fv < lo) {
                    fv = lo;
                } else if (fv > hi) {
                    fv = hi;
                }
                q->p_parms[0].i.life_start = fv;
            }
            {
                f32 fv = t1 * sc;
                if (fv < lo) {
                    fv = lo;
                } else if (fv > hi) {
                    fv = hi;
                }
                q->p_parms[0].i.life_end = fv;
            }
            {
                f32 fv = t2 * sc;
                if (fv < lo) {
                    fv = lo;
                } else if (fv > hi) {
                    fv = hi;
                }
                q->p_parms[0].i.fade_start = fv;
            }
            t3 = t3 * sc;
            if (!(t3 < lo)) {
                if (!(t3 > hi)) {
                    hi = t3;
                }
                lo = hi;
            }
            q->p_parms[0].i.fade_end = lo;
        }
    }
    if (wp->used & 0x20000) {  /* alpha / intensity envelope (lane 24) */
        Psys* q;
        q = (Psys*)node->data.psys;
        t3 = (f32)(lbl_80349260 * (f32)(wp->rgba[3] >> 24));
        t2 = (f32)(lbl_80349260 * (f32)(wp->rgba[2] >> 24));
        t1 = (f32)(lbl_80349260 * (f32)(wp->rgba[1] >> 24));
        t0 = (f32)(lbl_80349260 * (f32)(wp->rgba[0] >> 24));
        if (q->e_phase > 1) {
            ErrorPrintf(strs + 316);
        } else {
            f32 sc = pif[67];
            f32 lo = pif[68];
            f32 hi = pif[69];
            {
                f32 fv = t0 * sc;
                if (fv < lo) {
                    fv = lo;
                } else if (fv > hi) {
                    fv = hi;
                }
                q->p_parms[3].i.life_start = fv;
            }
            {
                f32 fv = t1 * sc;
                if (fv < lo) {
                    fv = lo;
                } else if (fv > hi) {
                    fv = hi;
                }
                q->p_parms[3].i.life_end = fv;
            }
            {
                f32 fv = t2 * sc;
                if (fv < lo) {
                    fv = lo;
                } else if (fv > hi) {
                    fv = hi;
                }
                q->p_parms[3].i.fade_start = fv;
            }
            t3 = t3 * sc;
            if (!(t3 < lo)) {
                if (!(t3 > hi)) {
                    hi = t3;
                }
                lo = hi;
            }
            q->p_parms[3].i.fade_end = lo;
        }
    }
    if (wp->used & 0x40000) {  /* particle width envelope */
        Psys* q;
        q = (Psys*)node->data.psys;
        t3 = wp->width[3];
        t2 = wp->width[2];
        t1 = wp->width[1];
        t0 = wp->width[0];
        if (q->e_phase > 1) {
            ErrorPrintf(strs + 316);
        } else {
            f32 sc = pif[71];
            f32 lo = pif[72];
            f32 hi = pif[73];
            {
                f32 fv = t0 * sc;
                if (fv < lo) {
                    fv = lo;
                } else if (fv > hi) {
                    fv = hi;
                }
                q->p_parms[4].i.life_start = fv;
            }
            {
                f32 fv = t1 * sc;
                if (fv < lo) {
                    fv = lo;
                } else if (fv > hi) {
                    fv = hi;
                }
                q->p_parms[4].i.life_end = fv;
            }
            {
                f32 fv = t2 * sc;
                if (fv < lo) {
                    fv = lo;
                } else if (fv > hi) {
                    fv = hi;
                }
                q->p_parms[4].i.fade_start = fv;
            }
            t3 = t3 * sc;
            if (!(t3 < lo)) {
                if (!(t3 > hi)) {
                    hi = t3;
                }
                lo = hi;
            }
            q->p_parms[4].i.fade_end = lo;
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
        fl = (fl & ~0xc0) | ((wp->flagval & 1) ? 0xc0 : 0);
    }
    if (mask & 2) {
        fl = (fl & ~1) | ((wp->flagval & 2) ? 1 : 0);
    }
    if (mask & 4) {
        fl = (fl & ~2) | ((wp->flagval & 4) ? 2 : 0);
    }
    if (mask & 0x40) {
        fl = (fl & ~0x20) | ((wp->flagval & 0x40) ? 0x20 : 0);
    }
    if (mask & 0x20) {
        fl = (fl & ~0x10) | ((wp->flagval & 0x20) ? 0x10 : 0);
    }
    p->flags = fl;
    mask = wp->flagmask;
    nfl = node->flags;
    if (mask & 0x80) {
        nfl = (nfl & ~0x800000) | ((wp->flagval & 0x80) ? 0x800000 : 0);
    }
    if (mask & 0x100) {
        nfl = (nfl & ~0x40000000) | ((wp->flagval & 0x100) ? 0x40000000 : 0);
    }
    if (mask & 0x200) {
        nfl = (nfl & ~0x800) | ((wp->flagval & 0x200) ? 0x800 : 0);
    }
    if (mask & 0x400) {
        nfl = (nfl & ~0x40) | ((wp->flagval & 0x400) ? 0x40 : 0);
    }
    if (mask & 0x800) {
        nfl = (nfl & ~0x80) | ((wp->flagval & 0x800) ? 0x80 : 0);
    }
    node->flags = nfl;
}

/* ======================================================================= *
 *  Node stack / traversal registration                                    *
 * ======================================================================= */

/* 0x800CEA18 - push/remove/query the "current draw node" stack (<=100).
 * NOTE: PDB name MBPsysSetDebugNode is inferred, not string-anchored. */
typedef struct NodeStackOverlay {
    u8 pad[264];
    s32 stack[100];
} NodeStackOverlay;

static NodeStackOverlay gNodeState; /* 0x802c9c48 */
static s32 gNodeStackTop;          /* 0x803451bc */
static s8  gNodeStackInit;         /* 0x803451c0 */
static s32 gNodeStackDirty;        /* 0x80345190 */

#pragma opt_propagation off
s32 MBPsysSetDebugNode(u32 node, s32 remove) {
    NodeStackOverlay* dst;
    s32 v;

    if (gNodeStackInit == 0) {
        gNodeStackTop = 0;
        gNodeStackInit = 1;
    }
    if (node != 0) {
        if (remove != 0) {
            s32 count;
            s32 i;
            NodeStackOverlay* src;
            s32 out;

            count = gNodeStackTop;
            i = 0;
            out = 0;
            while (i < count) {
                src = (NodeStackOverlay*)((u8*)&gNodeState + (i << 2));
                dst = (NodeStackOverlay*)((u8*)&gNodeState + (out << 2));

                v = src->stack[0];
                dst->stack[0] = v;
                if (src->stack[0] == node) {
                    out--;
                }
                i++;
                out++;
            }
            gNodeStackTop = out;
        } else {
            s32 top = gNodeStackTop;

            if (top > 99) {
                gNodeState.stack[0] = node;
            } else {
                gNodeStackTop = top + 1;
                gNodeState.stack[top] = node;
            }
            gNodeStackDirty = 1;
        }
    }
    if (gNodeStackTop > 0) {
        return gNodeState.stack[gNodeStackTop - 1];
    }
    return 0;
}
#pragma opt_propagation reset

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
extern const f64 lbl_80349218;   /* -1.0 */
extern const f64 lbl_80349248;   /* 1/30 */
extern const f64 lbl_80349258;   /* -0.0355555... gravity scale */
extern const f64 lbl_80349260;   /* 1/255 double */
extern const f32 lbl_803492A0;   /* 1/255 */

MBObject* MBNewPsysDescrip(s32 a, s32 b, s32 c, void* cfg) {
    PsysDescrip* wp = (PsysDescrip*)cfg;
    char* strs = (char*)lbl_80116D70;
    f32* pif = (f32*)psysInfo;
    Psys* p;
    MBObject* node;
    Psys* q;
    u32 used;

    used = wp->fields_used;
    if (used & 8) {
        b = (s32)wp->parent;
    }
    if ((u32)b == 0) {
        return NULL;
    }
    if ((node = createPsysNode(a, b, 0, 1)) == NULL) {
        return NULL;
    }
    p = (Psys*)node->data.psys;

    if (wp->fields_used & 1) {
        p->p_max = wp->max_particles;
    }
    if (wp->fields_used & 2) {
        p->dir_max = wp->max_directions;
    }
    if (wp->fields_used & 4) {
        p->pos_max = wp->max_positions;
    }
    if (wp->fields_used & 0x20) {
        f32 lf0;
        f32 lf1;
        q = (Psys*)node->data.psys;
        lf1 = wp->e_lifefade[1];
        lf0 = wp->e_lifefade[0];
        if (q->e_phase > 1) {
            ErrorPrintf(strs + 316);
        } else {
            f64 v;
            v = 30.0 * lf0;
            q->e_life = (u16)(s32)((v < 1.0) ? 1.0 : (v > 65535.0) ? 65535.0 : v);
            v = 30.0 * lf1;
            q->e_fade = (u16)(s32)((v < 0.0) ? 0.0 : (v > 65535.0) ? 65535.0 : v);
            q->flags &= ~1;
            if (lf0 < lbl_80349154) {
                q->e_life = 0xFFFF;
                q->flags |= 2;
            } else if (lf1 < lbl_80349154) {
                q->e_fade = 0xFFFF;
                q->flags |= 2;
            }
        }
    }
    if (wp->fields_used & 0x40) {
        f32 lf0;
        f32 lf1;
        q = (Psys*)node->data.psys;
        lf1 = wp->p_lifefade[1];
        lf0 = wp->p_lifefade[0];
        if (q->e_phase > 1) {
            ErrorPrintf(strs + 316);
        } else {
            f64 v;
            v = 30.0 * lf0;
            q->p_life = (u8)(s32)((v < 1.0) ? 1.0 : (v > 255.0) ? 255.0 : v);
            v = 30.0 * lf1;
            q->p_fade = (u8)(s32)((v < 0.0) ? 0.0 : (v > 255.0) ? 255.0 : v);
        }
    }
    if (wp->fields_used & 0x100) {
        f32 d0;
        f32 d1;
        f32 d2;
        q = (Psys*)node->data.psys;
        d2 = wp->e_dir[2];
        d1 = wp->e_dir[1];
        d0 = wp->e_dir[0];
        if (q->e_phase > 1) {
            ErrorPrintf(strs + 316);
        } else {
            q->e_dir[0] = d0;
            q->e_dir[1] = d1;
            q->e_dir[2] = d2;
        }
    }
    if (wp->fields_used & 0x200) {
        f32 ang;
        q = (Psys*)node->data.psys;
        ang = wp->e_angle;
        if (q->e_phase > 1) {
            ErrorPrintf(strs + 316);
        } else {
            setPTimeVal(ang, q);
        }
    }
    if (wp->fields_used & 0x400) {
        f32 v0;
        f32 v1;
        f32 v2;
        q = (Psys*)node->data.psys;
        v2 = wp->e_vol[2];
        v1 = wp->e_vol[1];
        v0 = wp->e_vol[0];
        if (q->e_phase > 1) {
            ErrorPrintf(strs + 316);
        } else {
            q->e_vol[0] = v0;
            q->e_vol[1] = v1;
            q->e_vol[2] = v2;
        }
    }
    if (wp->fields_used & 0x800) {
        f32 r3v;
        f32 r2v;
        f32 r1v;
        f32 r0v;
        q = (Psys*)node->data.psys;
        r3v = wp->e_rate[3];
        r2v = wp->e_rate[2];
        r1v = wp->e_rate[1];
        r0v = wp->e_rate[0];
        if (q->e_phase > 1) {
            ErrorPrintf(strs + 316);
        } else {
            q->e_rate.o.life_start = (f32)(lbl_80349248 * r0v);
            q->e_rate.o.life_slope = (f32)(lbl_80349248 * r1v);
            q->e_rate.o.fade_start = (f32)(lbl_80349248 * r2v);
            q->e_rate.o.fade_slope = (f32)(lbl_80349248 * r3v);
        }
    }
    if (wp->fields_used & 0x1000) {
        p->p_gravity = (f32)(lbl_80349258 * wp->p_gravity);
    }
    if (wp->fields_used & 0x2000) {
        p->p_drag = (f32)(lbl_80349218 * wp->p_drag);
    }
    if (wp->fields_used & 0x4000) {
        f32 sp;
        q = (Psys*)node->data.psys;
        sp = wp->p_speed;
        if (q->e_phase > 1) {
            ErrorPrintf(strs + 316);
        } else {
            q->p_speed = (f32)(lbl_80349248 * sp);
        }
    }
    if (wp->fields_used & 0x8000) {
        s32 tex = MBOX_FindTexture(wp->p_texname1, 0);
        u8* g = (u8*)gWinGlobals;
        q = (Psys*)node->data.psys;
        q->p_texidx = tex;
        {
            u8* t1 = *(u8**)(g + 48);
            t1 += ((u32)tex >> 16) << 4;
            q->p_tex = (struct ROMTEX*)(*(s32*)(*(u8**)(t1 + 4) + 88) +
                                        (((u32)tex & 0xFFFF) << 4));
        }
    }
    if (wp->fields_used & 0x10000) {
        s32 tex = MBOX_FindTexture(wp->p_texname2, 0);
        u8* g = (u8*)gWinGlobals;
        q = (Psys*)node->data.psys;
        q->p_texidx = tex;
        {
            u8* t1 = *(u8**)(g + 48);
            t1 += ((u32)tex >> 16) << 4;
            q->p_tex = (struct ROMTEX*)(*(s32*)(*(u8**)(t1 + 4) + 88) +
                                        (((u32)tex & 0xFFFF) << 4));
        }
    }
    if (wp->fields_used & 0x20000) {
        u32 tex = wp->p_texidx;
        u8* g = (u8*)gWinGlobals;
        Psys* texPsys = (Psys*)node->data.psys;
        texPsys->p_texidx = tex;
        {
            u8* t1 = *(u8**)(g + 48);
            t1 += (tex >> 16) << 4;
            texPsys->p_tex = (struct ROMTEX*)(*(s32*)(*(u8**)(t1 + 4) + 88) +
                                              ((tex & 0xFFFF) << 4));
        }
    }
    if (wp->fields_used & 0x40000) {
        q = (Psys*)node->data.psys;
        q->p_texidx = wp->p_texidx;
        q = (Psys*)node->data.psys;
        q->p_tex = wp->p_romtex;
    }
    if (wp->fields_used & 0x80000) {
        f32 x3, x2, x1, x0;
        {
            q = (Psys*)node->data.psys;
            x3 = lbl_803492A0 * (f32)(wp->p_rgba[3] >> 24);
            x2 = lbl_803492A0 * (f32)(wp->p_rgba[2] >> 24);
            x1 = lbl_803492A0 * (f32)(wp->p_rgba[1] >> 24);
            x0 = lbl_803492A0 * (f32)(wp->p_rgba[0] >> 24);
            if (q->e_phase > 1) {
                ErrorPrintf(strs + 316);
            } else {
                f32 sc = pif[67];
                f32 lo = pif[68];
                f32 hi = pif[69];
                {
                    f32 v = x0 * sc;
                    if (v < lo) {
                        v = lo;
                    } else if (v > hi) {
                        v = hi;
                    }
                    q->p_parms[3].o.life_start = v;
                }
                {
                    f32 v = x1 * sc;
                    if (v < lo) {
                        v = lo;
                    } else if (v > hi) {
                        v = hi;
                    }
                    q->p_parms[3].o.life_slope = v;
                }
                {
                    f32 v = x2 * sc;
                    if (v < lo) {
                        v = lo;
                    } else if (v > hi) {
                        v = hi;
                    }
                    q->p_parms[3].o.fade_start = v;
                }
                x3 = x3 * sc;
                if (!(x3 < lo)) {
                    if (!(x3 > hi)) {
                        hi = x3;
                    }
                    lo = hi;
                }
                q->p_parms[3].o.fade_slope = lo;
            }
        }
        {
            q = (Psys*)node->data.psys;
            x3 = lbl_803492A0 * (f32)(wp->p_rgba[3] >> 16 & 0xFF);
            x2 = lbl_803492A0 * (f32)(wp->p_rgba[2] >> 16 & 0xFF);
            x1 = lbl_803492A0 * (f32)(wp->p_rgba[1] >> 16 & 0xFF);
            x0 = lbl_803492A0 * (f32)(wp->p_rgba[0] >> 16 & 0xFF);
            if (q->e_phase > 1) {
                ErrorPrintf(strs + 316);
            } else {
                f32 sc = pif[63];
                f32 lo = pif[64];
                f32 hi = pif[65];
                {
                    f32 v = x0 * sc;
                    if (v < lo) {
                        v = lo;
                    } else if (v > hi) {
                        v = hi;
                    }
                    q->p_parms[2].o.life_start = v;
                }
                {
                    f32 v = x1 * sc;
                    if (v < lo) {
                        v = lo;
                    } else if (v > hi) {
                        v = hi;
                    }
                    q->p_parms[2].o.life_slope = v;
                }
                {
                    f32 v = x2 * sc;
                    if (v < lo) {
                        v = lo;
                    } else if (v > hi) {
                        v = hi;
                    }
                    q->p_parms[2].o.fade_start = v;
                }
                x3 = x3 * sc;
                if (!(x3 < lo)) {
                    if (!(x3 > hi)) {
                        hi = x3;
                    }
                    lo = hi;
                }
                q->p_parms[2].o.fade_slope = lo;
            }
        }
        {
            q = (Psys*)node->data.psys;
            x3 = lbl_803492A0 * (f32)(wp->p_rgba[3] >> 8 & 0xFF);
            x2 = lbl_803492A0 * (f32)(wp->p_rgba[2] >> 8 & 0xFF);
            x1 = lbl_803492A0 * (f32)(wp->p_rgba[1] >> 8 & 0xFF);
            x0 = lbl_803492A0 * (f32)(wp->p_rgba[0] >> 8 & 0xFF);
            if (q->e_phase > 1) {
                ErrorPrintf(strs + 316);
            } else {
                f32 sc = pif[59];
                f32 lo = pif[60];
                f32 hi = pif[61];
                {
                    f32 v = x0 * sc;
                    if (v < lo) {
                        v = lo;
                    } else if (v > hi) {
                        v = hi;
                    }
                    q->p_parms[1].o.life_start = v;
                }
                {
                    f32 v = x1 * sc;
                    if (v < lo) {
                        v = lo;
                    } else if (v > hi) {
                        v = hi;
                    }
                    q->p_parms[1].o.life_slope = v;
                }
                {
                    f32 v = x2 * sc;
                    if (v < lo) {
                        v = lo;
                    } else if (v > hi) {
                        v = hi;
                    }
                    q->p_parms[1].o.fade_start = v;
                }
                x3 = x3 * sc;
                if (!(x3 < lo)) {
                    if (!(x3 > hi)) {
                        hi = x3;
                    }
                    lo = hi;
                }
                q->p_parms[1].o.fade_slope = lo;
            }
        }
        {
            q = (Psys*)node->data.psys;
            x3 = lbl_803492A0 * (f32)(wp->p_rgba[3] & 0xFF);
            x2 = lbl_803492A0 * (f32)(wp->p_rgba[2] & 0xFF);
            x1 = lbl_803492A0 * (f32)(wp->p_rgba[1] & 0xFF);
            x0 = lbl_803492A0 * (f32)(wp->p_rgba[0] & 0xFF);
            if (q->e_phase > 1) {
                ErrorPrintf(strs + 316);
            } else {
                f32 sc = pif[55];
                f32 lo = pif[56];
                f32 hi = pif[57];
                {
                    f32 v = x0 * sc;
                    if (v < lo) {
                        v = lo;
                    } else if (v > hi) {
                        v = hi;
                    }
                    q->p_parms[0].o.life_start = v;
                }
                {
                    f32 v = x1 * sc;
                    if (v < lo) {
                        v = lo;
                    } else if (v > hi) {
                        v = hi;
                    }
                    q->p_parms[0].o.life_slope = v;
                }
                {
                    f32 v = x2 * sc;
                    if (v < lo) {
                        v = lo;
                    } else if (v > hi) {
                        v = hi;
                    }
                    q->p_parms[0].o.fade_start = v;
                }
                x3 = x3 * sc;
                if (!(x3 < lo)) {
                    if (!(x3 > hi)) {
                        hi = x3;
                    }
                    lo = hi;
                }
                q->p_parms[0].o.fade_slope = lo;
            }
        }
    }
    if (wp->fields_used & 0x100000) {
        f32 x3, x2, x1, x0;
        q = (Psys*)node->data.psys;
        x3 = wp->p_width[3];
        x2 = wp->p_width[2];
        x1 = wp->p_width[1];
        x0 = wp->p_width[0];
        if (q->e_phase > 1) {
            ErrorPrintf(strs + 316);
        } else {
            f32 sc = pif[71];
            f32 lo = pif[72];
            f32 hi = pif[73];
            {
                f32 v = x0 * sc;
                if (v < lo) {
                    v = lo;
                } else if (v > hi) {
                    v = hi;
                }
                q->p_parms[4].o.life_start = v;
            }
            {
                f32 v = x1 * sc;
                if (v < lo) {
                    v = lo;
                } else if (v > hi) {
                    v = hi;
                }
                q->p_parms[4].o.life_slope = v;
            }
            {
                f32 v = x2 * sc;
                if (v < lo) {
                    v = lo;
                } else if (v > hi) {
                    v = hi;
                }
                q->p_parms[4].o.fade_start = v;
            }
            x3 = x3 * sc;
            if (!(x3 < lo)) {
                if (!(x3 > hi)) {
                    hi = x3;
                }
                lo = hi;
            }
            q->p_parms[4].o.fade_slope = lo;
        }
    }
    {
        s8 d = wp->dynamic;
        if (d != 0) {
            switch (d) {
            case -1:
                p->flags &= ~0xC0;
                break;
            case 2:
                p->flags |= 0x80;
                break;
            case 3:
                p->flags |= 0x40;
                break;
            case 1:
            default:
                p->flags |= 0xC0;
                break;
            }
        }
    }
    if (wp->oneshot > 0) {
        u16 keep;
        q = (Psys*)node->data.psys;
        keep = p->p_max;
        if (q->e_phase > 1) {
            ErrorPrintf(strs + 316);
        } else {
            q->flags |= 1;
            q->p_max = keep;
        }
    }
    if (wp->forever > 0) {
        q = (Psys*)node->data.psys;
        if (q->e_phase > 1) {
            ErrorPrintf(strs + 316);
        } else {
            q->flags |= 2;
        }
    }
    if (wp->notex_rgb > 0) {
        p->flags |= 0x10;
    }
    if (wp->notex_a > 0) {
        p->flags |= 0x20;
    }
    if (wp->fbadd > 0) {
        node->flags |= 0x800000;
    }
    if (wp->fbmul > 0) {
        node->flags |= 0x40000000;
    }
    if (wp->sort > 0) {
        node->flags |= 0x800;
    }
    if (wp->nozcompare > 0) {
        node->flags |= 0x40;
    }
    if (wp->nozwrite > 0) {
        node->flags |= 0x80;
    }
    return node;
}

extern const f64 lbl_80349248;   /* 1/30 */
extern const f64 lbl_80349258;   /* -0.0355555... gravity scale */
extern const f64 lbl_80349260;   /* 1/255 double */
extern const f32 lbl_803492A0;   /* 1/255 */
extern f64 lbl_80349298;        /* firework rate divisor */
extern f64 lbl_80349210;        /* firework power scale */
extern MBObject* lbl_80344EBC;
extern MBObject* gSceneRoot;

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
extern f32 lbl_80349154;         /* 0.0f  */
extern f32 lbl_80349220;         /* 10.0f */
extern const f32 lbl_8034915C;   /* 1.0f  */
extern const f32 lbl_80349184;   /* -1.0f */
extern const f32 lbl_8034926C;   /* 0.3f  */
extern const f32 lbl_80349270;   /* 0.15f */
extern const f32 lbl_80349280;   /* 0.13f */
extern const f64 lbl_803491A0;   /* 0.5   */
extern const f64 lbl_803491F0;   /* 0.1   */
extern const f64 lbl_80349278;   /* 0.2   */
extern const f64 lbl_80349288;   /* 0.94  */
extern const f64 lbl_80349290;   /* 1.06  */

#pragma dont_inline on
MBObject* MBPsysFlame(f32 f1, f32 f2, f32 f3, s32 a, s32 tex, f32* verts) {
    u8* pi = (u8*)psysInfo;
    f32 w;

    if (f3 <= lbl_80349154) {
        *(f32*)(pi + 3408) = lbl_8034926C;
        *(f32*)(pi + 3412) = lbl_80349270;
    } else {
        *(f32*)(pi + 3408) = (f32)(lbl_80349278 * f3);
        *(f32*)(pi + 3412) = (f32)(lbl_803491F0 * f3);
    }
    w = (f32)(f2 * lbl_803491A0);
    if (w <= lbl_80349154) {
        *(f32*)(pi + 3444) = lbl_80349280;
        *(f32*)(pi + 3448) = lbl_80349280;
        *(f32*)(pi + 3452) = lbl_80349280;
    } else {
        *(f32*)(pi + 3444) = w;
        *(f32*)(pi + 3448) = w;
        *(f32*)(pi + 3452) = w;
    }
    if (f1 <= lbl_80349154) {
        *(f32*)(pi + 3400) = lbl_80349220;
        *(f32*)(pi + 3404) = lbl_8034915C;
    } else {
        *(f32*)(pi + 3400) = f1;
        *(f32*)(pi + 3404) = (f32)(lbl_803491F0 * f1);
    }
    if (verts != NULL) {
        f32 vx = verts[0];
        f32 vy = verts[1];
        f32 vz = verts[2];
        f32 mag;
        mag = vy * vy;
        mag = vx * vx + mag;
        mag = vz * vz + mag;
        if (lbl_80349154 == mag) {
            vz = lbl_80349154;
            vy = lbl_80349184;
            vx = (vz = vz);
        } else if (mag < lbl_80349288 || mag > lbl_80349290) {
            mag = mbInvSqrtLookup(mag);
            vx = vx * mag;
            vy = vy * mag;
            vz = vz * mag;
        }
        *(f32*)(pi + 3428) = vx;
        *(f32*)(pi + 3432) = vy;
        *(f32*)(pi + 3436) = vz;
    } else {
        f32 z = lbl_80349154;
        *(f32*)(pi + 3428) = z;
        *(f32*)(pi + 3432) = lbl_80349184;
        *(f32*)(pi + 3436) = z;
    }
    return MBNewPsysDescrip(a, tex, 0, (void*)(pi + 3340));
}
#pragma dont_inline off

/* 0x800D079C - default psys node (no descriptor), stores render flags */
MBObject* MBNewPsysDefault(void* matrix, MBObject* parent, s32 flags,
                           s32 arena) {
    MBObject* node;
    u8 unused[8];

    (void)unused;
    if (parent == NULL) {
        if ((flags & 0x2000) != 0) {
            parent = lbl_80344EBC;
        } else {
            parent = gSceneRoot;
        }
    }
    node = createPsysNode((s32)matrix, (s32)parent, 0, arena);
    if (node == NULL) {
        return NULL;
    }
    node->flags = flags;
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
extern const f32 lbl_803492A4;   /* 0.06f */
extern char lbl_80116ED8[];      /* "particle2_a"  */
extern char lbl_80116EE4[];      /* "particle2_xp" */

struct TexPageEnt {
    u32 f0;
    u8* obj;
    u32 f8;
    u32 fC;
};

static Psys* allocPsys(s32 fromArena) {
    u8 unused[8];
    u8* pi = (u8*)psysInfo;
    u8* gw = (u8*)gWinGlobals;
    u8* g = pi + 64;
    Psys* p;
    s32 off;
    s32 k;
    f32 one;
    f32 zero;

    *(s32*)(pi + 80) += 1;
    if (fromArena != 0) {
        p = (Psys*)AllocMem(304);
        *(s32*)(pi + 100) += 304;
    } else {
        p = (Psys*)allocPsysMem(304, *(s32*)(g + 16));
    }
    if (p == NULL) {
        return NULL;
    }
    memset(p, 0, 304);
    *(s32*)(pi + 64) += 1;
    *(void**)((u8*)p + 36) = *(void**)(pi + 68);
    *(Psys**)(pi + 68) = p;
    *(u16*)((u8*)p + 44) |= 0x4000;
    *(s32*)((u8*)p + 0) = *(s32*)(g + 16);
    *(u16*)((u8*)p + 58) = 300;
    *(u16*)((u8*)p + 60) = 300;
    one = lbl_8034915C;
    zero = lbl_80349154;
    *(f32*)((u8*)p + 68) = zero;
    *(f32*)((u8*)p + 72) = one;
    *(f32*)((u8*)p + 76) = zero;
    *(f32*)((u8*)p + 64) = lbl_80349184;
    *(f32*)((u8*)p + 80) = zero;
    *(f32*)((u8*)p + 84) = zero;
    *(f32*)((u8*)p + 88) = zero;
    *(f32*)((u8*)p + 204) = zero;
    *(f32*)((u8*)p + 208) = one;
    *(f32*)((u8*)p + 212) = one;
    *(f32*)((u8*)p + 216) = one;
    *(f32*)((u8*)p + 220) = one;
    *(u8*)((u8*)p + 96) = 20;
    *(u8*)((u8*)p + 97) = 10;
    *(f32*)((u8*)p + 132) = lbl_803492A4;
    off = 0;
    for (k = 0; k < 5; k++) {
        f32* src = (f32*)(pi + off);
        f32* dst = (f32*)((u8*)p + off);
        dst[56] = src[58];
        dst[57] = src[58];
        dst[58] = src[58];
        dst[59] = src[58];
        off += 16;
    }
    if (*(u32*)(g + 32) == 0 || *(u32*)(g + 28) == 0) {
        s32 texA = MBOX_FindTexture(lbl_80116ED8, 0);
        s32 texXp = MBOX_FindTexture(lbl_80116EE4, 0);
        if (texA != 0) {
            *(u8**)(g + 28) =
                *(u8**)((*(struct TexPageEnt**)(gw + 48))[texA >> 16].obj + 88) +
                (texA & 0xFFFF) * 16;
            *(u8**)(g + 32) =
                *(u8**)((*(struct TexPageEnt**)(gw + 48))[texXp >> 16].obj + 88) +
                (texXp & 0xFFFF) * 16;
        } else {
            *(u8**)(g + 28) =
                *(u8**)((*(struct TexPageEnt**)(gw + 48))[0].obj + 88);
            *(s32*)(g + 32) = 0;
        }
    }
    *(s32*)((u8*)p + 136) = 0;
    *(s32*)((u8*)p + 140) = 0;
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

/* 0x800D1074 - advance the clock, free queued psys, spawn deferred effects. */
extern s8  lbl_803451A8;
extern s32 lbl_803451A4;
extern s8  lbl_803451B0;
extern s32 lbl_803451AC;
extern s32 gClockElapsedTime;
extern s32 lbl_80345188;
extern s32 lbl_8034519C;
extern s32 lbl_803451A0;
extern s32 lbl_8034518C;
extern const f32 lbl_803492A8;   /* 10000000.0f */
extern const f32 lbl_80343FC8;   /* 2.0f  */
extern const f32 lbl_80343FCC;   /* 3.0f  */
extern s32 lbl_80343FD0;         /* 255   */
extern s32 lbl_80343FD4;
extern s32 lbl_80343FD8;
extern s32 lbl_80343FDC;
extern const f32 lbl_80343FE0;   /* 0.01f */
extern const f32 lbl_80343FE4;   /* 0.5f  */
extern const f32 lbl_80343FE8;   /* 0.1f  */
extern const f32 lbl_80343FEC;   /* 0.28f */
extern const f32 lbl_80343FF0;   /* 2.0f  */

#pragma dont_inline on
void MBPsysStartFrame(void) {
    u8 unused[8];
    u8* pi = (u8*)psysInfo;
    u8* g = pi + 64;
    u32 clock;
    MBObject* node;
    MBObject* next;
    u8* g2;
    u8* g3;
    u32 dbg;

    if (lbl_803451A8 == 0) {
        lbl_803451A4 = 0;
        lbl_803451A8 = 1;
    }
    if (lbl_803451B0 == 0) {
        lbl_803451AC = 0;
        lbl_803451B0 = 1;
    }
    clock = gClockElapsedTime + 5000000;
    lbl_80345188 = 0;
    if (clock > 150000000) {
        *(s32*)(g + 20) += 1;
        *(f32*)(g + 24) = lbl_80349154;
        if (lbl_803451AC <= 15) {
            lbl_803451AC = 15;
        }
    } else if (lbl_803451AC != 0) {
        *(s32*)(g + 20) += 1;
        *(f32*)(g + 24) = lbl_80349154;
        if (lbl_803451AC <= 15) {
            lbl_803451AC -= 1;
        }
    } else {
        *(f32*)(g + 24) = (f32)(u32)clock / lbl_803492A8;
        *(s32*)(g + 20) += (s32)*(f32*)(g + 24);
        *(f32*)(g + 24) = *(f32*)(g + 24) - (f32)(s32)*(f32*)(g + 24);
    }

    node = *(MBObject**)(pi + 76);
    g2 = pi + 64;
    while (node != NULL) {
        next = *(MBObject**)((u8*)node + 36);
        freePsys(node);
        node = next;
    }
    *(s32*)(g2 + 12) = 0;
    *(s32*)(g2 + 8) = 0;

    if ((next = *(MBObject**)(g + 100)) != NULL) {
        g3 = pi + 64;
        if ((dbg = *(u32*)(pi + 168)) == 0) {
            dbg = MBPsysSetDebugNode(0, 0);
        }
        *(s32*)(g3 + 108) = (s32)MBNewPsysDescrip(0, dbg, 0, next);
        *(s32*)(g + 100) = 0;
    }

    if (lbl_8034519C != 0) {
        if (lbl_8034519C == 1) {
            f32 rate;
            f32 power;

            dbg = MBPsysSetDebugNode(0, 0);
            rate = lbl_80343FC8;
            power = lbl_80343FCC;
            lbl_803451A0 = (s32)MBPsysFirework(0, dbg, lbl_80343FD0, lbl_80343FD4,
                                               lbl_80343FD8, lbl_80343FDC,
                                               rate, power, lbl_80343FE0,
                                               lbl_80343FE4, lbl_80343FE8);
        } else if (lbl_8034519C == 2) {
            lbl_803451A0 = (s32)MBPsysFlame(lbl_80349220, lbl_80343FEC,
                                            lbl_80343FF0, 0,
                                            (s32)MBPsysSetDebugNode(0, 0), 0);
        }
        lbl_8034519C = 0;
    }

    if (lbl_8034518C != 0) {
        lbl_8034518C = 0;
    }
}
#pragma dont_inline off

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

/* 0x800D1404 - allocPsysMem: first-fit split allocator over the block pool. */
static void* allocPsysMem(s32 size, s32 tag) {
    PsysMemPool* pool = (PsysMemPool*)((u8*)&lbl_80128710 + 0x24);
    PsysMemBlock* b;
    u32 need;
    PsysMemBlock* first;

    if (size <= 0 || size > (s32)pool->free_bytes) {
        return NULL;
    }
    need = (size + 0x1f) & 0xfffffff0;
    first = pool->next;
    b = first;
    for (;;) {
        if (b->bytes >= (s32)need) {
            break;
        }
        b = b->next;
        if (b == NULL) {
            b = pool->frst;
        }
        if (b == first) {
            return NULL;
        }
    }

    b->id = tag;
    if ((u32)(b->bytes - need) > 0x130) {
        PsysMemBlock* next = b->next;
        PsysMemBlock* split = (PsysMemBlock*)((u8*)b + need);

        b->next = split;
        split->prev = b;
        split->next = next;
        if (next != NULL) {
            next->prev = split;
        } else {
            pool->last = split;
        }
        pool->next = split;
        split->bytes = b->bytes - need;
        b->bytes = -(s32)need;
        pool->alloc_cnt++;
        pool->free_bytes -= need;
        return b + 1;
    }

    if (b->next == NULL) {
        pool->next = pool->frst;
    } else {
        pool->next = b->next;
    }
    {
        s32 oldBytes = b->bytes;
        b->bytes = -oldBytes;
        pool->alloc_cnt++;
        pool->free_cnt--;
        pool->free_bytes -= oldBytes;
    }
    return b + 1;
}

/* 0x800D1530 - freePsysMem: return a block, coalescing neighbours.
 * Documented reconstruction (NonMatching). */
#pragma opt_lifetimes off
static void freePsysMem(void* mem) {
    PsysMemPool* pool;
    PsysMemBlock* next;
    PsysMemBlock* prev;
    s32 error;
    PsysMemBlock* mergeNext;
    s32 nextBytes;
    s32 bytes;
    PsysMemBlock* block;
    s32 prevBytes;

    block = (PsysMemBlock*)mem - 1;
    nextBytes = 0;
    prevBytes = 0;
    next = block->next;
    pool = (PsysMemPool*)((u8*)&lbl_80128710 + 0x24);
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
        s32 nextValue;

        if (next->prev != block) {
            error = 2;
            goto bad_block;
        }
        if ((u8*)next - (u8*)block != bytes) {
            error = 3;
            goto bad_block;
        }
        nextValue = next->bytes;
        nextBytes = nextValue;
        if (nextValue <= 0) {
            if (nextValue == 0) {
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

/* 0x800D1724 - checksum + validate the built-in preset table. */
static void initPresetList(void) {
    u8* byteStart;
    u8* byteEnd;
    PsysPresetRecord* preset;
    u32 sum;
    u8* cursor;
    s32 bytes;
    s32 i;

    if (lbl_80345198 == 0) {
        lbl_80345194 = 0;
        lbl_80345198 = 1;
    }
    if (lbl_80345194 != 0) {
        return;
    }

    for (i = 8; i >= 0; i--) {
        if ((preset = &psysPresetTable[i])->id < 0x101) {
            preset->checksum = 0;
        } else {
            cursor = (u8*)preset;
            byteStart = (u8*)preset + 64;
            byteEnd = (u8*)preset + 96;
            sum = 0;
            bytes = 312;
            while ((bytes -= 4) != 0) {
                if (cursor >= byteStart && cursor <= byteEnd) {
                    sum += cursor[0];
                    sum += cursor[1];
                    sum += cursor[2];
                    sum += cursor[3];
                    cursor += 4;
                } else {
                    sum += *(u32*)cursor;
                    cursor += 4;
                }
            }
            preset->checksum = sum ^ 0xAAAA5555;
        }
    }
    lbl_80345194 = 1;
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
