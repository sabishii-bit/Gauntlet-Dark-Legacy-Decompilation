/* sfx.c -- GCN SFX.OBJ (shell3D.pdb module .\Release\SFX.OBJ), NonMatching.
 *
 * The effects engine: the Effects[64] live-fx pool (stride 0xF0, see
 * include/game/effect.h), the EffectInfo[218] def table, the Start* spawn
 * family, the SfxSet* property setters, the skin-FX flash system, the DmgFx*
 * collision-volume debug visualizers, and the per-frame driver ProcessEffects.
 *
 * TU boundary evidence:
 *   .text      0x800911C8..0x800988A4 (select.c ends at 0x800911C8; the shop
 *              TU starts at 0x800988A4 with do_shop)
 *   extab      0x80006D00..0x80006EB0 (flush after select.c's claim)
 *   extabindex 0x8000ABD0..0x8000AE58 (contiguous entries 0x91210..0x986A0)
 *   sdata2     pool 0x80348058..~0x80348328 (shared constants: 0x80348060
 *              conversion magic + 0x80348068 zero referenced from both ends
 *              of the range -> single TU)
 *
 * GC emits the file roughly in REVERSE of the Xbox source order with local
 * moves (skin-FX + DmgFx first, Start* family, StartFXSub/Tree, then
 * ProcessEffects and the management/init code) -- names below are mapped
 * BEHAVIORALLY (Ghidra decompile vs the PDB roster), not by position.
 *
 * NOTE: BATCH-5 had placed the name StartFXMat on 0x800911C8 (TU-start
 * position guess). Decompile shows 0x800911C8 packs the 6-float skin-FX
 * record used by CritterDoSfx/PlayerKnockback (= SetSkinFX), while
 * 0x800948E8 starts an effect from a matrix (StartFXSub + CopyMat3)
 * = the real StartFXMat. symbols.txt updated accordingly.
 *
 * Function map (GCN addr -> shell3D name; confidence):
 *   0x800911C8 SetSkinFX          (G) pack skinfx {frames,-rate,rate,base,loops,999} [high] BODY
 *   0x80091210 ProcessSkinFX      (G) advance skinfx timer (CritterUpdateSkinfx)     [high] BODY
 *   0x800912B4 DoProcessSkinFX    (G) apply skinfx frame+alpha to a node             [high] BODY
 *   0x800913A0 DmgFxConeAdd       (G) new "COLCYL" debug node                        [med]
 *   0x80091488 DmgFxCircleAdd     (G) new "COLCIR" debug node                        [med]
 *   0x80091570 DmgFxConeUpdate    (G) -> DmgFxNodeUpdate(n, r, h)                    [med]  BODY
 *   0x80091590 DmgFxCircleUpdate  (G) -> DmgFxNodeUpdate(n, r, r)                    [med]  BODY
 *   0x800915BC DmgFxNodeUpdate    (G) scale debug node (parent-chain compensated)    [high] BODY (parked: 6-insn order residual)
 *   0x80091700 DmgFxAdd           (G) dmgdebug node: COLCIR sphere or COLARC fan
 *                                     (arcs = (int)(acosf(mindp)*180/pi/15))         [high]
 *   0x800918AC SfxSetLight        (G) lightrad + lightcolor (default light_color)    [high] BODY
 *   0x8009190C fn_8009190C            typed spawn from matrix (attract caller)      [high] BODY (parked: sched tie 12)
 *   0x8009198C StartDeathFX       (G) FX_DEATH_HEALTH/EXP + reparent + minendtime   [high] BODY (parked: 1-insn fold)
 *   0x80091AC0 fn_80091AC0            per-enemy hit/death fx via page tables        [high] BODY (parked: assoc tie 6)
 *   0x80091B98 StartEnemyDeathFX  (G) FX_ENEDEATH1 + launch vel + morph->ENEDEATH2  [high]
 *   0x80091D50 StartEnemyAtkFX    (G) FX_ENEATK1+n                                  [high] BODY (parked: renum)
 *   0x80091E34 StartGenFX         (G) FX_GENFX1..3 (n=1..3)                         [high] BODY (parked: renum)
 *   0x80091F34 fn_80091F34            Start* (gem family?)
 *   0x800920E0 fn_800920E0            Start* + Random + atan2 (firework-like)
 *   0x800922A0 StartEnterFX       (G) FX_ENTER, flb 0x80880                         [high] BODY (parked: renum)
 *   0x8009233C StartBlockFX       (G) FX_BLOCK + per-class frame + player reparent  [high] BODY (parked: 1-insn fold)
 *   0x80092464 fn_80092464            Start* + frame-range select (fn_800BA42C)
 *   0x800926BC StartLevelUpFX     (G) FX_LEVELUP_* via color table 0x80122E60       [high] BODY (parked: renum)
 *   0x80092794 fn_80092794            Start* via Sub + launch helper fn_80093E50
 *   0x800929C8 StartMagicHealFX   (G) FX_MAGICHEAL + scale/32 clamp                 [high] BODY (parked: renum)
 *   0x80092AC0 StartMagicPlayerFX (G) FX_START_MAGIC, flb 0x880                     [high] BODY (parked: renum)
 *   0x80092B58 fn_80092B58            Start* + atan2
 *   0x80092DF4 fn_80092DF4            Start* via Sub + fn_80093E50
 *   0x80092FC0 SuicideExplosion   (G) big multi-part explosion (enemy.c caller)      [med-high]
 *   0x800933BC StartExplosion     (G) largest Start*: MBPsysFlame + debris + Random  [high]
 *   0x80093918 fn_80093918            Start* + gPlayerRecords + atan2 (shield-like)
 *   0x80093B04 fn_80093B04            plain typed spawn (~StartFXNoLoop)            [high] BODY (parked: renum)
 *   0x80093BC0 fn_80093BC0            Start* + atan2
 *   0x80093D08 SfxSetHitTarget    (G) targetnode + speed, flags|=0x40000000          [high] BODY
 *   0x80093D38 SfxSetOwner        (G) owner s16                                      [high] BODY
 *   0x80093D5C SfxSetMorph        (G) fxmorph/fxmorph2 + morphtime, flags|=0x4000    [high] BODY
 *   0x80093D98 fn_80093D98            streak attach (MBNewPoly+SetColorAlpha) ~SfxSetStreak
 *   0x80093E50 fn_80093E50            launch-velocity helper (atan2 + node yaw)
 *   0x80093F30 SfxSetDamage       (G) damage/radius/delay/type/owner                 [high] BODY
 *   0x80093F74 SfxSetHit          (G) fxhit + hit_audio + wall_sound                 [high] BODY
 *   0x80093FA0 SfxSetMat          (G) CopyMat3 into node + pos                       [high] BODY
 *   0x80094020 SfxSetParent       (G) reparent node, show atree root                 [high] BODY
 *   0x80094080 fn_80094080            Start* + sounds_evt call (fn_8009DB24)
 *   0x80094164 fn_80094164            Start* (big)
 *   0x80094440 fn_80094440            Start* w/ string-name def lookup (0x80122DAC)
 *   0x800945D0 fn_800945D0            Start* (big, CopyMat3)
 *   0x80094868 ScaleFX            (G) node flags|=8 + scale[3]                       [high] BODY
 *   0x800948E8 StartFXMat         (G) StartFXSub(type, mat+12, 0, 0x800) + CopyMat3  [high] BODY
 *   0x80094954 StartFXSub         (G) EffectInfo[type] -> StartFXTree + zmod/alpha   [high] BODY
 *   0x80094A04 StartFXTree        (G) core spawn: FindEffectIdx + atree build        [high] BODY
 *   0x80094BE0 ProcessEffects     (G) 0x2414 giant per-frame driver                        doc-only
 *   0x80096FF4 SfxSkipItem        (L) item-vs-effect skip policy switch(def->type)   [high] BODY
 *   0x8009716C UpdateFXStreak     (L) streak poly update                        [high] BODY (parked: quad sched tie)
 *   0x80097394 FindEffectIdx      (L) oldest-slot allocator                          [high] BODY
 *   0x80097474 SfxGetNode         (G) Effects[i].node                                [high] BODY
 *   0x8009748C PlaceEffectOnFloor (G) floor probe + snap matrix                      [high] BODY
 *   0x80097540 ChangeEffect       (G) swap live fx to a new def                      [high] BODY
 *   0x80097644 SfxDeleteParented  (G) delete fx parented under a node                [high] BODY
 *   0x800976C0 SfxDeleteParentedSub (L) recursive child/sibling walk                 [high] BODY
 *   0x80097790 DeleteEffect       (G) full teardown (msgPost, atree, childfx)   [high] BODY
 *   0x800979D4 ZeroEffect         (L) reset dynamics fields of a slot                [high] BODY
 *   0x80097AA4 InitEffects        (G) 0xBAC: parse fx defs, resolve atrees                 doc-only
 *   0x80098650 ClearCustomEffect  (G) EffectInfo[type].atree = 0                     [high] BODY
 *   0x8009867C InitCustomEffect   (G) -> InitCustomEffectSub(..., 1)                 [high] BODY
 *   0x800986A0 InitCustomEffectSub(L) register a custom fx def                  [high] BODY
 *
 * Xbox SFX.OBJ functions with no separate GC body (inlined or dropped):
 *   InitEffect, DmgFxNodeAdd (inlined into both Add twins), DmgFxUpdate,
 *   StartFXLoop/StartFXNoLoop/StartFXMissile (likely among the unnamed
 *   fn_ Start* bodies above -- kept fn_ until behaviorally pinned).
 *
 * Data:
 *   Effects[64]    .bss  0x80285BB8   live pool (effect.h verified)
 *   EffectInfo[218].bss  0x80285018   resolved def table (EffectHeader[218])
 *   NumEffects     .sbss 0x80344BDC   pool high-water mark
 *   light_color    .data 0x80127D10   {1,1,1,0} default SfxSetLight color (PDB name)
 *   lbl_80127D60   .data              4x4 identity matrix (MB lib, shared)
 *   lbl_8034457C   .sbss              integer frame delta (shared w/ camera.c etc)
 *   lbl_80344BF0/BF4 .sbss            skinfx special frame-base values (InitEffects)
 *   "COLCYL"/"COLCIR"/"COLARC"        debug object names in the sdata2 pool
 *   "Bad Effect type: %d" 0x80114790  StartFXSub/Start* error format
 */

#include "types.h"
#include "game/camera.h"
#include "game/effect.h"

/* --- partial MB scene-node view (offsets verified in this TU's asm) --- */
struct mbnode {
    /* 0x00 */ f32 mat[12];   /* 3x4 rotation rows                          */
    /* 0x30 */ f32 pos[3];
    /* 0x3C */ u8 _3c[4];
    /* 0x40 */ f32 scale[3];
    /* 0x4C */ u8 _4c[0x14];
    /* 0x60 */ u32 flags;
    /* 0x64 */ u8 _64[6];
    /* 0x6A */ s16 frame;
    /* 0x6C */ u8 _6c[8];
    /* 0x74 */ struct mbnode* parent;
    /* 0x78 */ struct mbnode* child;
    /* 0x7C */ struct mbnode* sibling;
};

/* skin-FX record packed by SetSkinFX, driven by ProcessSkinFX */
typedef struct SkinFx {
    /* 0x00 */ f32 endframe; /* frame count; <=0 = inactive          */
    /* 0x04 */ f32 frame;    /* current frame (starts at -rate)      */
    /* 0x08 */ f32 rate;     /* frames per vsync-pair                */
    /* 0x0C */ s32 base;     /* base frame selector                  */
    /* 0x10 */ s32 loops;    /* replay count                         */
    /* 0x14 */ f32 alpha;    /* applied alpha (999 = untouched)      */
} SkinFx;

/* anim-tree node: first word is the owning scene node */
struct anode {
    struct mbnode* node;
};

/* def record hanging off the inline atree's animinfo (frame counts) */
struct fxanimdef {
    /* 0x00 */ u8 _00[32];
    /* 0x20 */ s16 nframes;
    /* 0x22 */ s16 rate;
};

/* animinfo view of Effect.atree+4 (StartFXTree play-state fields) */
struct fxanim {
    /* 0x00 */ struct fxanimdef* def; /* = Effect+0x1C */
    /* 0x04 */ u8 _04[48];
    /* 0x34 */ s16 oneshot;           /* = Effect+0x50 */
    /* 0x36 */ s16 _36;
};

/* Some functions address the live pool THROUGH the def table (the original
 * emits EffectInfo+0xBA0-anchored code there): EffectInfo[248] is directly
 * followed by Effects[] in .bss, and those functions treat the pair as one
 * page struct. */
typedef struct EffectPage {
    EffectHeader info[248]; /* == EffectInfo            */
    Effect fx[64];          /* == Effects (at +0xBA0)   */
} EffectPage;
#define EFFECTS_POOL ((Effect*)&EffectInfo[248])

/* view of the fx .bss page with the per-enemy fx-type tables that sit
 * between the 218 def rows and the live pool (the original addresses both
 * tables and the pool as EffectInfo-relative field offsets) */
typedef struct EnemyFxPage {
    EffectHeader info[218]; /* 0x000                  */
    s32 deathfx[45];        /* +0xA38 (2616) per-enemy death fx type */
    s32 hitfx[45];          /* +0xAEC (2796) per-enemy hit fx type   */
    Effect fx[64];          /* +0xBA0 == Effects      */
} EnemyFxPage;
/* per-element form that keeps EffectInfo+idx*240 as the CSE base so the
 * +0xBA0 pool offset folds into each store displacement (target shape) */
#define EFFECTS_POOL_AT(i) ((Effect*)((u8*)EffectInfo + (i) * 240 + 2976))
/* fully-flat scalar view: keeps every +0xBA0-based constant inside the store
 * displacement (no CSE of the +2976 term) */
#define FX_POOL_F32(i, off) (*(f32*)((u8*)EffectInfo + (i) * 240 + (off)))

/* Effect.atree is stored inline (0x48 bytes); root anode* is its first word */
#define ATREE_ROOT(e) (*(struct anode**)&(e)->atree[0])

/* --- TU data --- */
extern Effect Effects[64];                     /* 0x80285BB8 */
extern EffectHeader EffectInfo[248];           /* 0x80285018 (0xBA0: 248 slots) */
extern s32 NumEffects;                         /* 0x80344BDC */
extern f32 light_color[4];                     /* 0x80127D10 */
extern f32 lbl_80127D60[16];                   /* identity matrix */
extern f32 lbl_8023CAE0[16];                   /* floor-probe result matrix */
extern u32 lbl_8034457C;                       /* frame delta */
extern s32 lbl_80344BF0;                       /* skinfx frame base A */
extern s32 lbl_80344BF4;                       /* skinfx frame base B */

/* --- MB / engine externs (fn_ names = symbols.txt identities) --- */
extern void ErrorPrintf(const char* fmt, ...);
extern void CopyMat3(f32* src, void* dst);
extern void CopyMat4(f32* src, void* dst);
extern void fn_800BA368(struct mbnode* node, u32 flags, s32 recurse);  /* show/flag set   */
extern void fn_800BA2C4(struct mbnode* node, u32 flags, s32 recurse);  /* flag set var.   */
extern void fn_800BA4D0(struct mbnode* node, s32 alpha, s32 recurse);  /* set alpha       */
extern void fn_800BA56C(struct mbnode* node, s32 sel, s32 frame, s32 recurse); /* set frame */
extern void fn_800BA6C0(struct mbnode* node, s32 alpha, s32 recurse);  /* set base alpha  */
extern void fn_800BA784(struct mbnode* node, s32 zmod, s32 recurse);   /* set z-bias      */
extern void fn_800BAD94(struct mbnode* node, struct mbnode* parent);   /* reparent        */
extern void fn_800BE6F4(struct mbnode* node, f32 ang);                 /* rotate yaw      */
extern void fn_800BE648(struct mbnode* node, f32 ang);                 /* rotate pitch    */
extern void fn_800BE4F4(struct mbnode* node, f32 yaw);                 /* set yaw         */
extern f32 atan2(f32 y, f32 x); /* PS2-shim float-returning decl */
extern void MBRemovePolyInst(struct polyinst* p);
extern void MBPolyInstUpdateVerts(struct polyinst* p, s32 nverts, f32* verts);
extern struct polyinst* MBNewPoly(void* ctx, s32 type, s32 tex, f32* verts);
extern void MBPolyInstSetColorAlpha(struct polyinst* p, u32 color, s32 alpha);
extern void fn_800BAEAC(struct mbnode* node, s32 flag);               /* node destroy    */
extern f32 fn_800BDA98(f32* v);                                       /* normalize, ret len */
extern int msgPost(int idx, int param, char* str);
extern u32 fn_8000D4B8(f32 rad1, f32 rad2, f32 drop, f32* pos, f32* outnrm, s32 a, s32 b); /* floor probe */
extern void fn_800115D0(void* atree);                                  /* atree release   */
extern struct anode* fn_80012F78(struct atreeheader* hdr, void* atree, s32 a, s32 b); /* atree build */
extern struct anode* fn_80012F9C(struct atreeheader* hdr, void* atree, s32 a, u32 flb, s32 b); /* atree build (flags) */
extern struct mbnode* fn_800BB29C(struct mbnode* parent, f32* mat, s32 flag); /* new node under parent */
extern struct mbnode* MBOX_NewObject(const char* name, s32 p2, s32 p3, u32 p4); /* create MB object */
extern struct mbnode* lbl_80344EBC; /* fx scene root (flag 0x2000)     */
extern struct mbnode* lbl_80344BD4; /* fx scene root (flag 0x800)      */
extern struct mbnode* lbl_80344EB8; /* default fx scene root           */
extern void fn_800BA42C(struct mbnode* node, s32 frame, s32 recurse); /* set anim frame  */
extern s32 lbl_80285B04[];  /* per-enemy hit-fx type table   (.bss)    */
extern s32 lbl_80285A50[];  /* per-enemy death-fx type table (.bss)    */
extern s32 lbl_80122E60[4]; /* levelup fx type by player color (.data) */
extern s32 lbl_8011A178[];  /* block-fx frame by player class  (.data) */
extern s32 lbl_80122D98[5]; /* fx type by index (.data)               */
extern s32 lbl_80122DAC[];  /* fx type table A by index (.data)       */
extern s32 lbl_80122DC0[];  /* fx type table B by index (.data)       */
extern void fn_8009DB24(s32 evt, f32* pos); /* sounds_evt             */
extern struct atreeheader* AtreeMatch(void* buf, char* name, s32 flag);
extern int strcmp(const char*, const char*);
extern int sprintf(char*, const char*, ...);

/* item-archive buffers searched by InitCustomEffectSub (owned by items.c) */
extern void* sWeaponsBuf;   /* 0x80344970 */
extern void* sPowerupsBuf;  /* 0x8034496C */
extern void* sGoodWizObj;   /* 0x80344978 */
extern void* sItemFile1Buf; /* 0x80344974 */
extern s32 lbl_8034489C;    /* in-world flag gating the boss rename    */
extern s32 gBossType;       /* current boss id (35 = STUMPL 'Q' skin)  */
extern s32 lbl_80344BD8;    /* count of registered custom fx defs      */
extern u8 lbl_80275AE0[];   /* player-record array, stride 0x335C      */
extern f32 lbl_80344584;    /* current game time (min-endtime gate)    */
extern s32 lbl_80343DF0;    /* running effect-id counter               */
extern s32 lbl_80344890;    /* tracked live-fx slot A (cleared on del) */
extern s32 lbl_80344894;    /* tracked live-fx slot B (cleared on del) */

/* forward decls (GC address order puts users first) */
void DoProcessSkinFX(SkinFx* fx, struct mbnode* node, struct mbnode* geo);
void DmgFxNodeUpdate(struct mbnode* node, s32 absolute, f32 a, f32 b, f32 c, f32 d);
s32 StartFXSub(s32 type, f32* pos, u32 fla, u32 flb, f32 time);
s32 StartFXTree(struct atreeheader* hdr, f32* pos, u32 fla, u32 flb, f32 time);
s32 DeleteEffect(s32 idx, s32 mode);
static s32 FindEffectIdx(void);
static void ZeroEffect(s32 idx);
static s32 SfxDeleteParentedSub(s32 idx, struct mbnode* node, s32 fxnum, s32 mode);
s32 InitCustomEffectSub(void* hdr, char* name, s32 zmod, s32 alpha, s32 err);

/* ======================================================================
 * skin FX -- 6-float record {endframe, frame, rate, base, loops, alpha}
 * ==================================================================== */

void SetSkinFX(SkinFx* fx, s32 base, s32 frames, s32 loops, f32 rate)
{
    fx->endframe = frames;
    fx->frame = -rate;
    fx->rate = rate;
    fx->base = base;
    fx->loops = loops;
    fx->alpha = 999.0f;
}

s32 ProcessSkinFX(SkinFx* fx, struct mbnode* node, struct mbnode* geo)
{
    if (fx->endframe > 0.0f) {
        fx->frame += fx->rate * (f32)(lbl_8034457C >> 1);
        if (fx->frame >= fx->endframe) {
            if (fx->loops != 0) {
                fx->frame = 0.0f;
                fx->loops--;
            } else {
                fx->endframe = 0.0f;
            }
        }
        DoProcessSkinFX(fx, node, geo);
        return 1;
    }
    return 0;
}

void DoProcessSkinFX(SkinFx* fx, struct mbnode* node, struct mbnode* geo)
{
    u32 saved = 0;

    if (geo != NULL) {
        saved = geo->flags;
        geo->flags |= 0x10;
    }

    if (fx->endframe > 0.0f) {
        s32 base = fx->base;
        s32 sel;

        if (base == lbl_80344BF4 || base == lbl_80344BF0) {
            sel = -3;
        } else {
            sel = -4;
        }
        fn_800BA56C(node, sel, base + (s32)fx->frame, 1);
        fn_800BA4D0(node, (s32)fx->alpha, 1);
    } else {
        fn_800BA56C(node, -1, 0, 1);
        fn_800BA4D0(node, 0, 1);
    }

    if (geo != NULL) {
        geo->flags = saved;
    }
}

/* ======================================================================
 * DmgFx -- collision-volume debug visualizers ("COLCYL"/"COLCIR"/"COLARC")
 * ==================================================================== */

/* new "COLCYL" debug cylinder node at pos, scaled/oriented by NodeUpdate */
struct mbnode* DmgFxConeAdd(s32 objid, f32* pos, s32 alpha, f32 rx, f32 rz, f32 rotp, f32 roty)
{
    struct mbnode* node;
    u32 flags = 0x401808;
    f32 sx;
    f32 sz;

    sz = rz;
    sx = rx;
    if (rx == 0.0) {
        sx = 0.01f;
        flags |= 1;
        sz = sx;
    }
    node = MBOX_NewObject("COLCYL", 0, objid, flags);
    node->pos[0] = pos[0];
    node->pos[1] = pos[1];
    node->pos[2] = pos[2];
    DmgFxNodeUpdate(node, 1, sx, sz, rotp, roty);
    if (flags & 1) {
        fn_800BA368(node, 1, 0);
    }
    if (alpha > 0) {
        fn_800BA6C0(node, alpha, 0);
    }
    return node;
}

/* new "COLCIR" debug circle node (uniform x/z radius) at pos */
struct mbnode* DmgFxCircleAdd(s32 objid, f32* pos, s32 alpha, f32 r, f32 rotp, f32 roty)
{
    struct mbnode* node;
    u32 flags = 0x401808;
    f32 sx;
    f32 sz;

    sz = r;
    sx = r;
    if (r == 0.0) {
        sx = 0.01f;
        flags |= 1;
        sz = sx;
    }
    node = MBOX_NewObject("COLCIR", 0, objid, flags);
    node->pos[0] = pos[0];
    node->pos[1] = pos[1];
    node->pos[2] = pos[2];
    DmgFxNodeUpdate(node, 1, sx, sz, rotp, roty);
    if (flags & 1) {
        fn_800BA368(node, 1, 0);
    }
    if (alpha > 0) {
        fn_800BA6C0(node, alpha, 0);
    }
    return node;
}

void DmgFxConeUpdate(struct mbnode* node, s32 absolute, f32 a, f32 b, f32 c, f32 d)
{
    DmgFxNodeUpdate(node, absolute, a, b, c, d);
}

void DmgFxCircleUpdate(struct mbnode* node, s32 absolute, f32 r)
{
    DmgFxNodeUpdate(node, absolute, r, r, 0.0f, 0.0f);
}

void DmgFxNodeUpdate(struct mbnode* node, s32 absolute, f32 rx, f32 rz, f32 rotp, f32 roty)
{
    u8 _pad[16];
    struct mbnode* p;
    f32 sx, sy, sz;

    if (node == NULL) {
        return;
    }
    /* PARKED at 6 residual insns (regalloc/order class, 3 attempts):
     * target emits fmul var,const (ours const,var) and fmr-before-mr call
     * setup; byte-different, semantically identical. */
    node->scale[0] = rx * 0.01;
    node->scale[1] = rx * 0.01;
    node->scale[2] = rz * 0.01;
    if (absolute != 0) {
        CopyMat3(lbl_80127D60, node);
        fn_800BE6F4(node, roty);
        fn_800BE648(node, rotp);
        sx = 1.0f;
        sy = 1.0f;
        sz = 1.0f;
        for (p = node->parent; p != NULL; p = p->parent) {
            if (p->flags & 8) {
                sx *= p->scale[0];
                sy *= p->scale[1];
                sz *= p->scale[2];
            }
        }
        if (sx != 0.0) {
            node->scale[0] /= sx;
        }
        if (sy != 0.0) {
            node->scale[1] /= sy;
        }
        if (sz != 0.0) {
            node->scale[2] /= sz;
        }
        fn_800BA2C4(node, 1, 0);
    } else {
        fn_800BA368(node, 1, 0);
    }
}

/* 0x80091700 DmgFxAdd -- doc-only: if !(flags&0x20) or mindp <= -1.0 create a
 * "COLCIR" node scaled by colrad*0.01 into e->dmgdebug; else build
 * (int)(acosf(mindp)*180/pi/15)*2 "COLARC" segments fanned +/- pi/12 steps. */

/* ====================================================================== */

void SfxSetLight(s32 idx, f32* color, f32 rad)
{
    u8* page = (u8*)EffectInfo;
    u8* e;

    if (idx < 0) {
        return;
    }
    idx *= 240;
    e = page + idx;
    *(f32*)(e + 0xBB0) = rad;
    if (color != NULL) {
        *(f32*)(e + 0xBA0) = color[0];
        *(f32*)(e + 0xBA4) = color[1];
        *(f32*)(e + 0xBA8) = color[2];
    } else {
        *(f32*)(e + 0xBA0) = light_color[0];
        *(f32*)(e + 0xBA4) = light_color[1];
        *(f32*)(e + 0xBA8) = light_color[2];
    }
}

/* ======================================================================
 * Start* spawn family (0x8009190C..0x80093BC0).  GC file order is the
 * REVERSE of the Xbox SFX.OBJ roster; names below are pinned by the fx_type
 * constants each body hardcodes (see the map above).  All funnel into
 * StartFXSub/StartFXTree.
 * ==================================================================== */

/* start an fx from a full matrix: spawn at mat row3, then orient the node */
s32 fn_8009190C(f32* mat, s32 type)
{
    s32 ret = -1;
    s32 idx;

    if (type >= 0) {
        idx = StartFXSub(type, mat + 12, 0, 0x800, 0.0f);
        ret = idx;
        if (idx >= 0) {
            if (mat != NULL) {
                CopyMat3(mat, Effects[idx].node);
            }
        }
    }
    return ret;
}

s32 StartDeathFX(struct mbnode* parent, s32 kind, u32 fla)
{
    EffectPage* page = (EffectPage*)EffectInfo;
    s32 idx;
    s32 type;
    s32 t;
    EffectHeader* h;

    if (kind == 2) {
        t = FX_DEATH_EXP;
    } else {
        t = FX_DEATH_HEALTH;
    }
    idx = -1;
    if (t < 0 || (type = t) >= MAXEFFECTTYPES) {
        ErrorPrintf("Bad Effect type: %d", type);
        idx = -1;
    } else {
        h = &page->info[type];
        if (h->atree != NULL && (idx = StartFXTree(h->atree, NULL, 0, fla | 0x800, 10.0f)) >= 0) {
            fn_800BA784(page->fx[idx].node, h->zmod, 1);
            fn_800BA6C0(page->fx[idx].node, h->alpha, 1);
            page->fx[idx].type = (fx_type)type;
        }
    }
    if (idx >= 0) {
        Effect* e = &page->fx[idx];
        struct anode* root;

        fn_800BAD94(e->node, parent);
        root = ATREE_ROOT(e);
        if (root != NULL) {
            fn_800BA368(root->node, 0x10, 0);
        }
    }
    page->fx[idx].minendtime = 0.5 + lbl_80344584;
    return idx;
}

/* generic hit/death fx for an enemy kind, from the per-enemy type tables */
s32 fn_80091AC0(f32* mat, s32 ene, s32 death)
{
    EnemyFxPage* page = (EnemyFxPage*)EffectInfo;
    s32 ret = -1;
    s32 type;
    s32 idx;

    if (ene < 0) {
        ene = 0;
    }
    if (death != 0) {
        s32 t = page->deathfx[ene];

        type = t;
        if (t < 0 && (ene < 0 || ene == 1 || ene == 4)) {
            type = FX_GENDEST;
        }
    } else {
        type = page->hitfx[ene];
    }
    if (type >= 0) {
        idx = StartFXSub(type, mat + 12, 0, 0x800, 0.0f);
        ret = idx;
        if (idx >= 0) {
            Effect* e = &page->fx[idx];

            if (mat != NULL) {
                CopyMat3(mat, e->node);
            }
        }
    }
    return ret;
}

/* 0x80091B98 StartEnemyDeathFX -- body below the small clones (bigger). */

/* Shared guts of the Start*FX family and StartFXSub. Defined before all of
 * them so -inline auto folds it into each caller (the standalone copy is
 * deadstripped by mwld). The inlinee's param/retval webs are what give the
 * clones their target register coloring; open-coding the body compiles to
 * the identical opcode stream but rotates the nonvolatile colors. */
static s32 StartFXSubGuts(s32 type, f32* pos, u32 fla, u32 flb, f32 time)
{
    EffectPage* page = (EffectPage*)EffectInfo;
    s32 idx = -1;
    EffectHeader* h;

    if (type < 0 || type >= MAXEFFECTTYPES) {
        ErrorPrintf("Bad Effect type: %d", type);
        return -1;
    }
    h = &page->info[type];
    if (h->atree != NULL && (idx = StartFXTree(h->atree, pos, fla, flb, time)) >= 0) {
        fn_800BA784(page->fx[idx].node, h->zmod, 1);
        fn_800BA6C0(page->fx[idx].node, h->alpha, 1);
        page->fx[idx].type = (fx_type)type;
    }
    return idx;
}

s32 StartEnemyAtkFX(f32* pos, s32 n)
{
    EffectPage* page = (EffectPage*)EffectInfo;

    if (page->info[FX_ENEATK1 + n].atree == NULL) {
        return -1;
    }
    return StartFXSubGuts(FX_ENEATK1 + n, pos, 0, 0x800, 0.0f);
}

s32 StartGenFX(f32* pos, s32 n)
{
    EffectPage* page = (EffectPage*)EffectInfo;
    s32 i;

    if (n < 1 || n > 3) {
        return -1;
    }
    i = n - 1;
    if (page->info[FX_GENFX1 + i].atree == NULL) {
        return -1;
    }
    return StartFXSubGuts(n + 80, pos, 0, 0x800, 0.0f);
}

/* gem/rune/garg pickup fx: special-cased constant types + generic default */
s32 fn_80091F34(f32* pos, s32 sel)
{
    s32 ret;

    if (sel == 0x400) {
        ret = StartFXSubGuts(FX_GET_RUNE, pos, 0, 0x80880, 0.0f);
    } else if (sel == 0x100) {
        ret = StartFXSubGuts(FX_GET_GARG, pos, 0, 0x80880, 0.0f);
    } else {
        ret = StartFXSubGuts(sel + 69, pos, 0, 0x80880, 0.0f);
    }
    return ret;
}

/* 0x800920E0 fn_800920E0 -- doc-only (bag, Random + atan2). */

s32 StartEnterFX(f32* pos)
{
    u32 flb = 0x80880;
    u8 unused[8];
    return StartFXSubGuts(FX_ENTER, pos, 0, flb, 0.0f);
}

s32 StartBlockFX(f32 time, s32 pnum)
{
    EffectPage* page = (EffectPage*)EffectInfo;
    EffectHeader* h = &page->info[FX_BLOCK];
    s32 idx = -1;
    u32 flb = 0x80980;
    u8 unused[8];

    if (h->atree != NULL && (idx = StartFXTree(h->atree, NULL, 0, flb, time)) >= 0) {
        fn_800BA784(page->fx[idx].node, h->zmod, 1);
        fn_800BA6C0(page->fx[idx].node, h->alpha, 1);
        page->fx[idx].type = FX_BLOCK;
    }
    if (idx >= 0) {
        fn_800BA42C(page->fx[idx].node,
                    lbl_8011A178[*(s32*)(lbl_80275AE0 + pnum * 0x335C + 4)], 1);
        fn_800BA6C0(page->fx[idx].node, 0x40, 1);
        if (idx >= 0) {
            Effect* e = &page->fx[idx];
            struct anode* root;

            fn_800BAD94(e->node,
                        *(struct mbnode**)(lbl_80275AE0 + pnum * 0x335C + 0x74));
            root = ATREE_ROOT(e);
            if (root != NULL) {
                fn_800BA368(root->node, 0x10, 0);
            }
        }
    }
    return idx;
}

/* 0x80092464 fn_80092464 -- doc-only (combo family). */

s32 StartLevelUpFX(f32* pos, s32 color)
{
    u8 unused[8];
    return StartFXSubGuts(lbl_80122E60[color], pos, 0, 0x880800, 0.0f);
}

/* 0x80092794 fn_80092794 -- doc-only (shield family). */

s32 StartMagicHealFX(f32 scale, f32* pos)
{
    EffectPage* page = (EffectPage*)EffectInfo;
    u8 unused[8];
    s32 idx;
    f32 s;

    s = 0.03125 * scale;
    idx = StartFXSubGuts(FX_MAGICHEAL, pos, 0, 0x880, 0.0f);
    if (s > 1.0) {
        s = 1.0f;
    }
    {
        Effect* e = (Effect*)&page->info[idx * 20];
        struct mbnode* node = ((EffectPage*)e)->fx[0].node;

        e = (Effect*)((u8*)e + 2976);
        if (node != NULL) {
            fn_800BA368(node, 8, 0);
            e->node->scale[0] = s;
            e->node->scale[1] = s;
            e->node->scale[2] = s;
        }
    }
    return idx;
}

s32 StartMagicPlayerFX(f32* pos)
{
    u8 unused[8];
    return StartFXSubGuts(FX_START_MAGIC, pos, 0, 0x880, 0.0f);
}

/* 0x80092B58 fn_80092B58 / 0x80092DF4 fn_80092DF4 -- doc-only
 * (throw-magic / magic family, atan2 launch). */

/* 0x80092FC0 SuicideExplosion / 0x800933BC StartExplosion /
 * 0x80093918 fn_80093918 -- doc-only giants this pass. */

/* plain typed spawn at a position (no orientation) */
s32 fn_80093B04(s32 type, f32* pos)
{
    return StartFXSubGuts(type, pos, 0, 0x800, 0.0f);
}

/* aimed-launch spawn: guts + velocity/yaw + hit-fx fields */
s32 fn_80093BC0(s32 type, f32* pos, f32* vel, u32 fla, s32 fxhit, s32 hit_audio, s32 wall_sound, f32 time)
{
    EffectPage* page = (EffectPage*)EffectInfo;
    u8 unused[8];
    s32 idx = StartFXSubGuts(type, pos, fla, 0x800, time);
    Effect* e;

    if (idx < 0) {
        return -1;
    }
    e = &page->fx[idx];
    if (vel != NULL) {
        f32 vz = vel[2];
        f32 ang = atan2(vel[0], vz);
        e->vel[0] = vel[0];
        e->vel[1] = vel[1];
        e->vel[2] = vel[2];
        if (e->node != NULL) {
            fn_800BE4F4(e->node, ang);
        }
    }
    e->weight = 0.0f;
    e->colrad = 0.0f;
    page->fx[idx].fxhit = fxhit;
    page->fx[idx].hit_audio = hit_audio;
    page->fx[idx].wall_sound = wall_sound;
    return idx;
}

void SfxSetHitTarget(f32 speed, s32 idx, struct mbnode* target)
{
    Effect* e;

    if (idx < 0) {
        return;
    }
    e = &Effects[idx];
    e->targetnode = target;
    e->weight = speed;
    e->flags |= 0x40000000;
}

void SfxSetOwner(s32 idx, s32 owner)
{
    Effect* e;

    if (idx < 0) {
        return;
    }
    e = &Effects[idx];
    e->owner = owner;
}

void SfxSetMorph(f32 time, s32 idx, s32 morph1, s32 morph2)
{
    Effect* e;

    if (idx < 0) {
        return;
    }
    e = &Effects[idx];
    e->fxmorph = morph1;
    e->fxmorph2 = morph2;
    e->flags |= 0x4000;
    e->morphtime = time;
}

/* attach a streak poly to an effect + set its color/alpha and streak params */
void fn_80093D98(s32 idx, s32 tex, u32 color, s32 alpha, f32 scale, f32 fwdmul)
{
    Effect* e;

    if (idx < 0) {
        return;
    }
    e = &Effects[idx];
    e->flags |= 0x40000;
    e->streak = MBNewPoly(NULL, 4, tex, NULL);
    if (e->streak != NULL) {
        MBPolyInstSetColorAlpha(e->streak, color, alpha);
    }
    e->streakfwdmul = fwdmul;
    if (scale > 0.0) {
        e->streakscale = scale;
    } else {
        e->streakscale = e->colrad;
    }
}

/* launch-velocity helper: set vel (+ yaw the node), pyrvel, weight, colrad */
void fn_80093E50(s32 idx, f32* vel, f32* pyrvel, f32 weight, f32 colrad)
{
    Effect* e;

    if (idx < 0) {
        return;
    }
    e = &Effects[idx];
    if (vel != NULL) {
        f32 vz = vel[2];
        f32 ang = atan2(vel[0], vz);
        e->vel[0] = vel[0];
        e->vel[1] = vel[1];
        e->vel[2] = vel[2];
        if (e->node != NULL) {
            fn_800BE4F4(e->node, ang);
        }
    }
    if (pyrvel != NULL) {
        e->pyrvel[0] = pyrvel[0];
        e->pyrvel[1] = pyrvel[1];
        e->pyrvel[2] = pyrvel[2];
    }
    if (weight >= 0.0) {
        e->weight = weight;
    }
    if (colrad >= 0.0) {
        e->colrad = colrad;
    }
}

void SfxSetDamage(f32 damage, f32 radius, f32 delay, s32 idx, s32 type, s32 owner)
{
    Effect* e;

    if (idx < 0) {
        return;
    }
    e = &Effects[idx];
    if ((type & 0xF) >= 5) {
        type &= ~0xC;
    }
    e->damage = damage;
    e->damagetype = (DMG_TYPE)type;
    e->damageradius = radius;
    e->damagedelay = delay;
    e->owner = owner;
}

void SfxSetHit(s32 idx, s32 fxhit, s32 hit_audio, s32 wall_sound)
{
    Effect* e;

    if (idx < 0) {
        return;
    }
    e = &Effects[idx];
    e->fxhit = fxhit;
    e->hit_audio = hit_audio;
    e->wall_sound = wall_sound;
}

void SfxSetMat(s32 idx, f32* mat, f32* pos)
{
    Effect* e;

    if (idx < 0) {
        return;
    }
    e = &Effects[idx];
    if (mat != NULL) {
        CopyMat3(mat, e->node);
    }
    if (pos != NULL) {
        e->node->pos[0] = pos[0];
        e->node->pos[1] = pos[1];
        e->node->pos[2] = pos[2];
    }
}

void SfxSetParent(s32 idx, struct mbnode* parent)
{
    if (idx >= 0) {
        fn_800BAD94(Effects[idx].node, parent);
        if (ATREE_ROOT(&Effects[idx]) != NULL) {
            fn_800BA368(ATREE_ROOT(&Effects[idx])->node, 0x10, 0);
        }
    }
}

/* start a table-selected fx at pos, then post its spawn sound event */
s32 fn_80094080(f32* pos, s32 index)
{
    s32 idx = StartFXSubGuts(lbl_80122D98[index & 0xF], pos, 0, 0x880, 0.0f);

    fn_8009DB24(5, pos);
    return idx;
}

/* 0x80094164 fn_80094164 -- doc-only (Start*, big). */

/* start a table-selected fx (table chosen by `which`) at pos.
 * STRUCTURAL: two duplicated StartFXSubGuts inlines; residual is a
 * duplicated-inline renum -- target GCSE-hoists EffectInfo to r31 before the
 * branch (shared by both guts copies) where ours re-materializes it inside the
 * fall-through, cascading the color assignment in both arms. mask-local hoist +
 * beq layout matched; open-coded shared-page form was worse (95 vs 72). */
s32 fn_80094440(f32* pos, u32 idx, s32 which)
{
    u32 m = idx & 0xF;
    s32 ret;

    if (which != 0) {
        ret = StartFXSubGuts(lbl_80122DC0[m], pos, 0, 0x880, 0.0f);
    } else {
        ret = StartFXSubGuts(lbl_80122DAC[m], pos, 0, 0x880, 0.0f);
    }
    return ret;
}

/* 0x800945D0 fn_800945D0 -- doc-only (Start*, big, CopyMat3). */

void ScaleFX(s32 idx, f32 sx, f32 sy, f32 sz)
{
    Effect* e = &Effects[idx];

    if (e->node != NULL) {
        fn_800BA368(e->node, 8, 0);
        e->node->scale[0] = sx;
        e->node->scale[1] = sy;
        e->node->scale[2] = sz;
    }
}

s32 StartFXMat(s32 type, f32* mat)
{
    u8 _pad[8];
    s32 idx = StartFXSub(type, mat + 12, 0, 0x800, 0.0f);

    if (idx >= 0) {
        Effect* e = &Effects[idx];

        if (mat != NULL) {
            CopyMat3(mat, e->node);
        }
    }
    return idx;
}

s32 StartFXSub(s32 type, f32* pos, u32 fla, u32 flb, f32 time)
{
    EffectPage* page = (EffectPage*)EffectInfo;
    s32 idx = -1;
    EffectHeader* h;

    if (type < 0 || type >= MAXEFFECTTYPES) {
        ErrorPrintf("Bad Effect type: %d", type);
        return -1;
    }
    h = &page->info[type];
    if (h->atree != NULL && (idx = StartFXTree(h->atree, pos, fla, flb, time)) >= 0) {
        fn_800BA784(page->fx[idx].node, h->zmod, 1);
        fn_800BA6C0(page->fx[idx].node, h->alpha, 1);
        page->fx[idx].type = (fx_type)type;
    }
    return idx;
}

/* core spawn: allocate a slot, build the atree, hang a node under the
 * selected scene root, then set lifetime from `time` or the def's frames */
s32 StartFXTree(struct atreeheader* hdr, f32* pos, u32 fla, u32 flb, f32 time)
{
    s32 idx;
    Effect* e;
    struct fxanim* ai;
    struct mbnode* parent;
    s32 n;

    if (hdr == NULL) {
        return -1;
    }
    idx = FindEffectIdx();
    e = &Effects[idx];
    ATREE_ROOT(e) = fn_80012F9C(hdr, &e->atree[0], 0, flb, 0);
    if (ATREE_ROOT(e) == NULL) {
        return -1;
    }

    if (flb & 0x2000) {
        parent = lbl_80344EBC;
    } else if (flb & 0x800) {
        parent = lbl_80344BD4;
    } else {
        parent = lbl_80344EB8;
    }
    e->node = fn_800BB29C(parent, lbl_80127D60, 1);
    if (e->node == NULL) {
        fn_800115D0(&e->atree[0]);
        return -1;
    }
    fn_800BAD94(ATREE_ROOT(e)->node, e->node);

    ai = (struct fxanim*)&e->atree[4];
    if (time > 0.0) {
        e->endtime = lbl_80344584 + time;
        ai->oneshot = 1;
    } else {
        n = ai->def->nframes;
        if (n == 0) {
            ai->oneshot = 1;
            n = 30;
        }
        e->endtime = 0.00111111 * ((f32)n * (f32)ai->def->rate) + lbl_80344584;
    }
    if (fla & 0x20000000) {
        ai->oneshot = 0;
    }
    e->maxtime = e->endtime - lbl_80344584;
    e->flags = fla;
    if (pos != NULL) {
        e->node->pos[0] = pos[0];
        e->node->pos[1] = pos[1];
        e->node->pos[2] = pos[2];
    }
    return idx;
}

/* 0x80094BE0 ProcessEffects -- 0x2414 doc-only giant: per-frame walk of
 * Effects[0..NumEffects): motion integration (vel/pyrvel/weight/drag), floor
 * + wall collision (wall_sound), damage application (damage_enemy /
 * CritterDamage / grids), morphs (ChangeEffect), streaks (UpdateFXStreak),
 * skinfx, child/parent bookkeeping, item spawning (PlaceItem), boss hooks. */

/* Minimal item view for SfxSkipItem (full layout: include/game/item.h).
 * def->type drives the skip policy; armor/active gate pickups. */
struct fxitemdef {
    /* 0x00 */ s32 type;
    /* 0x04 */ s32 subtype;
};
struct fxitem {
    /* 0x00 */ struct fxitemdef* def;
    /* 0x04 */ u8 _04[0xC0];
    /* 0xC4 */ s16 active;
    /* 0xC6 */ u8 _c6[9];
    /* 0xCF */ s8 armor;
};

static s32 SfxSkipItem(struct fxitem* item, u32 a, u32 b)
{
    struct fxitemdef* def = item->def;
    s32 sub = def->subtype;
    s32 ret = 0;

    switch (def->type) {
    case 3:
        if (a & 0x1000) {
            ret = 1;
        }
        break;
    case 10:
        if (sub != 41 && (a & 0x1000)) {
            ret = 1;
        }
        if (b & 0x200) {
            ret = 1;
        }
        if (item->armor == -1 && !(b & 0x400)) {
            ret = 1;
        }
        if (ret && (b & 0x800000)) {
            ret = 2;
        }
        break;
    case 2:
        if (sub == 43 && (a & 0x1000)) {
            ret = 1;
        }
        if (item->armor == -1 && !(b & 0x600)) {
            ret = 1;
        }
        break;
    case 1:
        if (a & 0x100) {
            if ((b & 0x800000) && (sub == 3 || sub == 1)) {
                ret = 2;
            } else {
                ret = 1;
            }
        }
        if (item->armor == -1 && !(b & 0x600)) {
            ret = 1;
        }
        break;
    case 8:
        if (b & 0x800000) {
            ret = 2;
        } else {
            ret = 1;
        }
        break;
    case 4:
        if (item->active & 1) {
            ret = 1;
            break;
        }
        if (b & 0x200) {
            break;
        }
        /* fallthrough */
    default:
        if (a & 0x100) {
            ret = 1;
        }
        if (item->armor == -1 && !(b & 0x600)) {
            ret = 1;
        }
        break;
    }
    return ret;
}

/* rebuild the streak quad (tail pair at -back, head pair at +fwd) and push
 * the 4 verts into the streak poly instance.
 * PARKED at a quad-block schedule tie (insns 138/138, frame + array layout
 * exact): target hoists the double fmadd p1 chain differently and colors
 * w[0]/w[1] reloads into f29/f28 where ours picks p1x/p1y. */
static void UpdateFXStreak(Effect* e, f32* pos)
{
    f32 quad[12];
    f32 v[3];
    f32 w[3];
    f64 unused[2];
    f32 side;
    f32 tail;
    f32 back;
    f32 t;
    f32 len;
    f64 fwd;
    f32 wx, wy, wz;
    f32 p0x, p0y, p0z;
    f32 p1x, p1y, p1z;

    t = e->maxtime - (e->endtime - lbl_80344584);
    side = (f32)(0.8 * e->streakscale);
    tail = (f32)(0.1 * e->streakscale);
    back = t;
    if (t > (f32)(0.4999999995 * e->streakscale)) {
        back = (f32)(0.4999999995 * e->streakscale);
    }

    v[0] = e->vel[0];
    v[1] = e->vel[1];
    v[2] = e->vel[2];
    fn_800BDA98(v);

    w[0] = v[1] * gCameras[0].mat[2][2] - v[2] * gCameras[0].mat[2][1];
    w[1] = v[2] * gCameras[0].mat[2][0] - v[0] * gCameras[0].mat[2][2];
    w[2] = v[0] * gCameras[0].mat[2][1] - v[1] * gCameras[0].mat[2][0];
    len = fn_800BDA98(w);

    if (len < 0.0f && len > -0.25) {
        len = -0.25f;
    } else if (len >= 0.0f && len < 0.25) {
        len = 0.25f;
    }
    w[0] *= len;
    w[1] *= len;
    w[2] *= len;

    wx = w[0];
    wy = w[1];
    wz = w[2];
    p0x = e->vel[0] * -back + pos[0];
    p0y = e->vel[1] * -back + pos[1];
    p0z = e->vel[2] * -back + pos[2];
    fwd = 0.0333333333 * e->streakfwdmul;
    p1x = e->vel[0] * fwd + pos[0];
    p1y = e->vel[1] * fwd + pos[1];
    p1z = e->vel[2] * fwd + pos[2];

    quad[0] = wx * tail + p0x;
    quad[1] = wy * tail + p0y;
    quad[2] = wz * tail + p0z;
    quad[3] = wx * side + p1x;
    quad[4] = wy * side + p1y;
    quad[5] = wz * side + p1z;
    quad[6] = wx * -side + p1x;
    quad[7] = wy * -side + p1y;
    quad[8] = wz * -side + p1z;
    quad[9] = wx * -tail + p0x;
    quad[10] = wy * -tail + p0y;
    quad[11] = wz * -tail + p0z;
    MBPolyInstUpdateVerts(e->streak, 4, quad);
}

static s32 FindEffectIdx(void)
{
    s32 best;
    s32 i;
    f32 bestt;

    best = 0;
    bestt = 0.0f;
    for (i = 0; i < NumEffects; i++) {
        if (Effects[i].endtime <= 0.0) {
            break;
        }
        if (!(Effects[i].flags & 0x80080000) && (bestt == 0.0 || Effects[i].endtime < bestt)) {
            bestt = Effects[i].endtime;
            best = i;
        }
    }
    if (i >= 64) {
        i = best;
    } else if (i == NumEffects) {
        NumEffects++;
    }
    Effects[i].type = FX_NONE;
    DeleteEffect(i, 1);
    return i;
}

struct mbnode* SfxGetNode(s32 idx)
{
    return Effects[idx].node;
}

void PlaceEffectOnFloor(s32 idx, f32* mat)
{
    Effect* e = &Effects[idx];

    if (mat == NULL) {
        mat = (f32*)e->node;
    }
    if (fn_8000D4B8(e->colrad + 1.0, e->colrad + 5.0, -10.0f, mat + 12, NULL, 1, 0)) {
        CopyMat4(lbl_8023CAE0, mat);
        mat[13] += 0.1;
    } else {
        CopyMat3(lbl_80127D60, mat);
    }
}

void ChangeEffect(s32 idx, s32 type, u32 newflags)
{
    EffectHeader* h = &EffectInfo[type];
    Effect* e = &Effects[idx];
    struct anode* root;
    struct mbnode* n;
    s32 oldframe;
    s16 cfx;

    if (h != NULL) {
        if (h->atree != NULL) {
            cfx = e->childfx;
            if (cfx >= 0) {
                DeleteEffect(cfx, 1);
                e->childfx = -1;
            }
            root = ATREE_ROOT(e);
            n = root->node;
            newflags |= n->flags & 0x890;
            oldframe = n->frame;
            fn_800115D0(&e->atree[0]);
            ATREE_ROOT(e) = fn_80012F78(h->atree, &e->atree[0], 0, 0);
            fn_800BAD94(ATREE_ROOT(e)->node, e->node);
            fn_800BA784(e->node, h->zmod, 1);
            fn_800BA6C0(e->node, h->alpha, 1);
            if (oldframe != 0) {
                fn_800BA4D0(e->node, oldframe, 1);
            }
            fn_800BA368(e->node, newflags, 1);
        }
    }
}

void SfxDeleteParented(struct mbnode* node, s32 mode, s32 fxnum)
{
    s32 i;

    for (i = 0; i < NumEffects; i++) {
        if (Effects[i].node != NULL) {
            SfxDeleteParentedSub(i, node, fxnum, mode);
        }
    }
}

static s32 SfxDeleteParentedSub(s32 idx, struct mbnode* node, s32 fxnum, s32 mode)
{
    u8 _pad[8];
    Effect* e = &Effects[idx];

    while (node != NULL) {
        if (e->node->parent == node || (fxnum >= 0 && fxnum == e->owner - 1)) {
            DeleteEffect(idx, 1);
            return 1;
        }
        if (mode != 0 && node->child != NULL &&
            SfxDeleteParentedSub(idx, node->child, fxnum, 2)) {
            return 1;
        }
        if (mode != 2) {
            break;
        }
        node = node->sibling;
    }
    return 0;
}

s32 DeleteEffect(s32 idx, s32 mode)
{
    Effect* e;
    struct mbnode* n;
    s16 cfx;

    if (idx < 0 || idx >= 64) {
        return -1;
    }
    e = &Effects[idx];

    if (mode == 0) {
        if (e->minendtime > 0.0 && lbl_80344584 < e->minendtime) {
            return idx;
        }
    }

    if (e->hitcount == 0) {
        switch (e->type) {
        case FX_MAGIC_FIRE:
        case FX_MAGIC_ELEC:
        case FX_MAGIC_LIGHT:
        case FX_MAGIC_ACID:
            if (e->owner >= 1 && e->owner <= 4) {
                msgPost(18, e->owner - 1, (char*)(lbl_80275AE0 + (e->owner - 1) * 13148 + 84));
            }
            break;
        }
    }

    if (idx == lbl_80344894) {
        lbl_80344894 = -1;
    }
    if (idx == lbl_80344890) {
        lbl_80344890 = -1;
    }

    if (e->endtime > 0.0f) {
        cfx = e->childfx;
        if (cfx >= 0) {
            DeleteEffect(cfx, 1);
            e->childfx = -1;
        }
    }

    if (ATREE_ROOT(e) != NULL) {
        fn_800115D0(&e->atree[0]);
    }

    if ((n = e->node) != NULL) {
        if (n->child != NULL) {
            SfxDeleteParented(n, 0, -1);
        }
        fn_800BAEAC(e->node, 1);
    }

    e->type = FX_NONE;
    e->hitcount = 0;
    e->node = NULL;
    e->childfx = -1;
    e->endtime = 0.0f;
    e->fxfade = 0.0f;
    e->morphtime = 0.0f;
    e->webtime = 0.0f;
    e->maxtime = 0.0f;
    e->flags = 0;
    e->damagetype = DMG_NORMAL;
    e->damageradius = 0.0f;
    e->damage = 0.0f;
    e->hitscale = 1.0f;
    e->mindp = -1.0f;
    e->owner = 0;
    e->dmgdebug = NULL;
    e->targetnode = NULL;
    e->additem = NULL;
    e->id = lbl_80343DF0++;
    if (lbl_80343DF0 >= 32512) {
        lbl_80343DF0 = 256;
    }
    e->lightrad = 0.0f;
    ZeroEffect(idx);
    return -1;
}

static void ZeroEffect(s32 idx)
{
    Effect* e = &Effects[idx];

    e->vel[0] = 0.0f;
    e->vel[1] = 0.0f;
    e->vel[2] = 0.0f;
    e->pyrvel[0] = 0.0f;
    e->pyrvel[1] = 0.0f;
    e->pyrvel[2] = 0.0f;
    if (e->node != NULL && !(e->flags & 0x20000)) {
        CopyMat3(lbl_80127D60, e->node);
    }
    e->colrad = 1.0f;
    e->weight = 0.0f;
    e->dragx = 0.0f;
    e->dragz = 0.0f;
    e->fxhit = 0;
    e->fxmorph = 0;
    e->fxmorph2 = 0;
    e->hit_audio = 0;
    e->wall_sound = 0;
    e->damagedelay = 0.0f;
    if (e->streak != NULL) {
        MBRemovePolyInst(e->streak);
    }
    e->streak = NULL;
    e->minendtime = 0.0f;
    e->childfx = -1;
}

/* 0x80097AA4 InitEffects -- doc-only 0xBAC giant: reads the fx def script,
 * resolves atree names (AtreeMatch/FindTexMod), fills EffectInfo[], stores
 * the skinfx frame bases (lbl_80344BE0..BF8) and weapon/powerup buffers. */

void ClearCustomEffect(s32 type)
{
    if (type < 0) {
        return;
    }
    if (type >= MAXEFFECTTYPES) {
        return;
    }
    EffectInfo[type].atree = NULL;
}

void InitCustomEffect(void* hdr, char* name, s32 zmod, s32 alpha)
{
    InitCustomEffectSub(hdr, name, zmod, alpha, 1);
}

/* register a custom fx def into a free FX_CUSTOM1..FX_CUSTOM_LAST slot */
s32 InitCustomEffectSub(void* hdr, char* name, s32 zmod, s32 alpha, s32 err)
{
    char buf[32];
    EffectHeader* page = EffectInfo;
    struct atreeheader* atree = NULL;
    s32 i;
    s32 n;
    s32 idx;

    if (name == NULL || name[0] == '\0') {
        return -1;
    }

    if (lbl_8034489C != 0 && gBossType >= 0) {
        switch (gBossType) {
        case 35:
            if (strcmp(name, "STUMPL") == 0) {
                sprintf(buf, "%sQ", name);
                name = buf;
            }
            break;
        }
    }

    if (hdr != NULL) {
        atree = AtreeMatch(hdr, name, 0);
    }
    if (atree == NULL && sWeaponsBuf != NULL) {
        atree = AtreeMatch(sWeaponsBuf, name, 0);
    }
    if (atree == NULL && sPowerupsBuf != NULL) {
        atree = AtreeMatch(sPowerupsBuf, name, 0);
    }
    if (atree == NULL && sGoodWizObj != NULL) {
        atree = AtreeMatch(sGoodWizObj, name, 0);
    }
    if (atree == NULL && sItemFile1Buf != NULL) {
        atree = AtreeMatch(sItemFile1Buf, name, 0);
    }
    if (atree == NULL) {
        if (err != 0) {
            ErrorPrintf("Unable to find effect '%s'", name);
        }
        return -1;
    }

    n = lbl_80344BD8;
    for (i = 0; i < n; i++) {
        if (*(struct atreeheader**)((u8*)page + i * 12 + 1164) == NULL) {
            break;
        }
    }
    if (i >= n) {
        if (n >= 121) {
            ErrorPrintf("> Max = %d custon effects", n);
            return -1;
        }
        lbl_80344BD8++;
    }
    idx = i + 97;
    page[idx].atree = atree;
    page[idx].zmod = zmod;
    page[idx].alpha = alpha;
    return idx;
}
