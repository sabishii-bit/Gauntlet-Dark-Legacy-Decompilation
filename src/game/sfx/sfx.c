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
 *   0x80091F34 StartGemFX            Start* (gem family?)
 *   0x800920E0 fn_800920E0            Start* + Random + atan2 (firework-like)
 *   0x800922A0 StartEnterFX       (G) FX_ENTER, flb 0x80880                         [high] BODY (parked: renum)
 *   0x8009233C StartBlockFX       (G) FX_BLOCK + per-class frame + player reparent  [high] BODY (parked: 1-insn fold)
 *   0x80092464 StartComboFX       (G) combo sphere/color pair + class light color   [high]
 *   0x800926BC StartLevelUpFX     (G) FX_LEVELUP_* via color table 0x80122E60       [high] BODY (parked: renum)
 *   0x80092794 StartShieldFX            Start* via Sub + launch helper fn_80093E50
 *   0x800929C8 StartMagicHealFX   (G) FX_MAGICHEAL + scale/32 clamp                 [high] BODY (parked: renum)
 *   0x80092AC0 StartMagicPlayerFX (G) FX_START_MAGIC, flb 0x880                     [high] BODY (parked: renum)
 *   0x80092B58 StartThrowMagicFX            Start* + atan2
 *   0x80092DF4 StartMagicFX            Start* via Sub + fn_80093E50
 *   0x80092FC0 SuicideExplosion   (G) big multi-part explosion (enemy.c caller)      [med-high]
 *   0x800933BC StartExplosion     (G) largest Start*: MBPsysFlame + debris + Random  [high]
 *   0x80093918 fn_80093918            Start* + gPlayerRecords + atan2 (shield-like)
 *   0x80093B04 StartFXNoLoop            plain typed spawn (~StartFXNoLoop)            [high] BODY (parked: renum)
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
 *   gIdentityMatrix   .data              4x4 identity matrix (MB lib, shared)
 *   gFrameTicks   .sbss              integer frame delta (shared w/ camera.c etc)
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

/* The first sequence in an effect definition supplies the expansion time for
 * ordinary hit effects.  This is the Xbox/retail atree-header prefix; the
 * complete resource owns more fields after the sequence pointer. */
struct fxatreeseq {
    /* 0x00 */ char name[32];
    /* 0x20 */ s16 numframes;
    /* 0x22 */ s16 framerate;
};
struct fxatreeheader {
    /* 0x00 */ struct fxatreeseq* seq;
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
extern f32 gIdentityMatrix[16];                   /* identity matrix */
extern f32 gFloorCollisionResult[16];                   /* floor-probe result matrix */
extern u32 gFrameTicks;                       /* frame delta */
extern s32 lbl_80344BF0;                       /* skinfx frame base A */
extern s32 lbl_80344BF4;                       /* skinfx frame base B */

/* --- MB / engine externs (fn_ names = symbols.txt identities) --- */
extern void ErrorPrintf(const char* fmt, ...);
extern void CopyMat3(f32* src, void* dst);
extern void CopyMat4(f32* src, void* dst);
extern void MBTreeSetFlags(struct mbnode* node, u32 flags, s32 recurse);  /* show/flag set   */
extern void MBTreeClearFlags(struct mbnode* node, u32 flags, s32 recurse);  /* flag set var.   */
extern void MBTreeSetAmbientAdd(struct mbnode* node, s32 alpha, s32 recurse);  /* set alpha       */
extern void MBTreeSetAltTex(struct mbnode* node, s32 sel, s32 frame, s32 recurse); /* set frame */
extern void MBTreeSetAlpha(struct mbnode* node, s32 alpha, s32 recurse);  /* set base alpha  */
extern void MBTreeSetZsortAdd(struct mbnode* node, s32 zmod, s32 recurse);   /* set z-bias      */
extern void MBNodeSetParent(struct mbnode* node, struct mbnode* parent);   /* reparent        */
extern void WYawMat3(struct mbnode* node, f32 ang);                    /* rotate yaw      */
extern void WPitchMat3(struct mbnode* node, f32 ang);                  /* rotate pitch    */
extern void YawMat3(struct mbnode* node, f32 yaw);                     /* set yaw         */
extern f32 atan2(f32 y, f32 x); /* PS2-shim float-returning decl */
extern void MBRemovePolyInst(struct polyinst* p);
extern void MBPolyInstUpdateVerts(struct polyinst* p, s32 nverts, f32* verts);
extern struct polyinst* MBNewPoly(void* ctx, s32 type, s32 tex, f32* verts);
extern void MBPolyInstSetColorAlpha(struct polyinst* p, u32 color, s32 alpha);
extern void MBRemoveNode(struct mbnode* node, s32 flag);               /* node destroy    */
extern f32 NormalVector(f32* v);                                      /* normalize, ret len */
extern int msgPost(int idx, int param, char* str);
extern u32 FloorCollide(f32 rad1, f32 rad2, f32 drop, f32* pos, f32* outnrm, s32 a, s32 b); /* floor probe */
extern void AtreeDelete(void* atree);                                  /* atree release   */
extern struct anode* AtreeInit(struct atreeheader* hdr, void* atree, s32 a, s32 b); /* atree build */
extern struct anode* AtreeInitSub(struct atreeheader* hdr, void* atree, s32 a, u32 flb, s32 b); /* atree build (flags) */
extern struct mbnode* MBNewNode(struct mbnode* parent, f32* mat, s32 flag); /* new node under parent */
extern struct mbnode* MBOX_NewObject(const char* name, s32 p2, s32 p3, u32 p4); /* create MB object */
extern struct mbnode* lbl_80344EBC; /* fx scene root (flag 0x2000)     */
extern struct mbnode* lbl_80344BD4; /* fx scene root (flag 0x800)      */
extern struct mbnode* gSceneRoot; /* default fx scene root           */
extern void MBTreeSetColor(struct mbnode* node, s32 frame, s32 recurse); /* set anim frame  */
extern s32 lbl_80285B04[];  /* per-enemy hit-fx type table   (.bss)    */
extern s32 lbl_80285A50[];  /* per-enemy death-fx type table (.bss)    */
extern s32 lbl_80122E60[4]; /* levelup fx type by player color (.data) */
extern s32 lbl_8011A178[];  /* block-fx frame by player class  (.data) */
extern s32 lbl_80122088[];   /* combo/magic FX definition page (.data) */
extern f32 lbl_803480EC;     /* combo-sphere light radius               */
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
extern u8 gPlayers[];   /* player-record array, stride 0x335C      */
extern f32 gClockTime;      /* current game time (min-endtime gate)    */
extern s32 lbl_80343DF0;    /* running effect-id counter               */
extern s32 lbl_80344890;    /* tracked live-fx slot A (cleared on del) */
extern s32 lbl_80344894;    /* tracked live-fx slot B (cleared on del) */
extern s32 sItemFile1Handle;

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
        fx->frame += fx->rate * (f32)(gFrameTicks >> 1);
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
        MBTreeSetAltTex(node, sel, base + (s32)fx->frame, 1);
        MBTreeSetAmbientAdd(node, (s32)fx->alpha, 1);
    } else {
        MBTreeSetAltTex(node, -1, 0, 1);
        MBTreeSetAmbientAdd(node, 0, 1);
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
        MBTreeSetFlags(node, 1, 0);
    }
    if (alpha > 0) {
        MBTreeSetAlpha(node, alpha, 0);
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
        MBTreeSetFlags(node, 1, 0);
    }
    if (alpha > 0) {
        MBTreeSetAlpha(node, alpha, 0);
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
    rx *= 0.01;
    rz *= 0.01;
    node->scale[0] = rx;
    node->scale[1] = rx;
    node->scale[2] = rz;
    if (absolute != 0) {
        CopyMat3(gIdentityMatrix, node);
        sx = roty;
        WYawMat3(node, sx);
        sy = rotp;
        WPitchMat3(node, sy);
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
        MBTreeClearFlags(node, 1, 0);
    } else {
        MBTreeSetFlags(node, 1, 0);
    }
}

/* 0x80091700 DmgFxAdd - dmgdebug node: COLCIR sphere or COLARC fan */
extern Effect Effects[64];
extern f32 acosf(f32 x);
extern s32 Round(f32 x);
extern f64 lbl_803480A8;        /* -1.0 arc threshold */
extern f64 lbl_803480B8;        /* 180/pi factor */
extern f64 lbl_803480C0;        /* arc scale */
extern f64 lbl_803480C8;        /* 15.0 divisor */
extern f64 lbl_80348098;        /* radius scale */
extern f64 lbl_803480B0;        /* y squash */
extern f32 lbl_803480D0;        /* odd-start yaw */
extern f32 lbl_803480DC;        /* even-start yaw */
extern f64 lbl_803480E0;        /* yaw step */
extern f32 lbl_803480E8;        /* fixed radius */
extern char lbl_803480D4[7];    /* "COLARC" */
extern char lbl_8034808C[7];    /* "COLCIR" */

void DmgFxAdd(s32 idx)
{
    Effect* e = &Effects[idx];
    s32 cnt;
    s32 flags = 0x401808;
    f32 s;
    f32 yaw;
    f64 arc;
    f64 step;
    u8 _spare[8];

    if ((*(u32*)((u8*)e + 100) & 0x20) && *(f32*)((u8*)e + 156) > lbl_803480A8) {
        arc = lbl_803480C0 * acosf(*(f32*)((u8*)e + 156));
        arc = lbl_803480B8 * arc;
        arc /= lbl_803480C8;
        cnt = Round((f32)arc) << 1;
        s = (f32)(lbl_80348098 * *(f32*)((u8*)e + 152));
        if (cnt & 1) {
            yaw = lbl_803480D0;
            e->dmgdebug = MBOX_NewObject(lbl_803480D4, 0, (s32)e->node, flags);
            cnt = cnt - 1;
        } else {
            yaw = lbl_803480DC;
            e->dmgdebug = MBOX_NewObject(0, 0, (s32)e->node, flags);
        }
        e->dmgdebug->scale[0] = s;
        e->dmgdebug->scale[1] = (f32)(lbl_803480B0 * s);
        e->dmgdebug->scale[2] = s;
        step = lbl_803480E0;
        while (cnt != 0) {
            YawMat3(MBOX_NewObject(lbl_803480D4, 0,
                                                   (s32)e->dmgdebug, flags),
                    yaw);
            YawMat3(MBOX_NewObject(lbl_803480D4, 0,
                                                   (s32)e->dmgdebug, flags),
                    -yaw);
            yaw = (f32)(yaw + step);
            cnt = cnt - 2;
        }
    } else {
        struct mbnode* node;
        if (*(u32*)((u8*)e + 100) & 0x20) {
            s = lbl_803480E8;
        } else {
            s = (f32)(lbl_80348098 * *(f32*)((u8*)e + 152));
        }
        node = MBOX_NewObject(lbl_8034808C, 0,
                                              *(s32*)((u8*)e + 20), flags);
        node->scale[0] = s;
        node->scale[1] = s;
        node->scale[2] = s;
        e->dmgdebug = node;
    }
}

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
            Effect* effect = &Effects[idx];
            if (mat != NULL) {
                CopyMat3(mat, effect->node);
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
            MBTreeSetZsortAdd(page->fx[idx].node, h->zmod, 1);
            MBTreeSetAlpha(page->fx[idx].node, h->alpha, 1);
            page->fx[idx].type = (fx_type)type;
        }
    }
    if (idx >= 0) {
        Effect* e = &page->fx[idx];
        struct anode* root;

        MBNodeSetParent(e->node, parent);
        root = ATREE_ROOT(e);
        if (root != NULL) {
            MBTreeSetFlags(root->node, 0x10, 0);
        }
    }
    page->fx[idx].minendtime = 0.5 + gClockTime;
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
        MBTreeSetZsortAdd(page->fx[idx].node, h->zmod, 1);
        MBTreeSetAlpha(page->fx[idx].node, h->alpha, 1);
        page->fx[idx].type = (fx_type)type;
    }
    return idx;
}


/* page-explicit clone of StartFXSubGuts: taking the page as a param lets the
 * caller compute EffectInfo once in its prologue (target hoists it to r31). */
static s32 StartFXSubGutsP(EffectPage* page, s32 type, f32* pos, u32 fla, u32 flb, f32 time)
{
    s32 idx = -1;
    EffectHeader* h;

    if (type < 0 || type >= MAXEFFECTTYPES) {
        ErrorPrintf("Bad Effect type: %d", type);
        return -1;
    }
    h = &page->info[type];
    if (h->atree != NULL && (idx = StartFXTree(h->atree, pos, fla, flb, time)) >= 0) {
        MBTreeSetZsortAdd(page->fx[idx].node, h->zmod, 1);
        MBTreeSetAlpha(page->fx[idx].node, h->alpha, 1);
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
s32 StartGemFX(f32* pos, s32 sel)
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

extern f64 Random(f64 range);
extern f32 lbl_80127D20[3];     /* bag launch direction */
extern f64 lbl_80348078;
extern f32 lbl_80348068;
extern f32 lbl_803480F8;
extern f32 lbl_803480A0;
extern f32 lbl_80348100;
extern f32 lbl_80348104;
extern f64 lbl_803480E0;
extern f32 lbl_803480FC;

/* 0x800920E0 -- spawn the treasure-bag toss fx: scaled launch direction,
 * random spin, then attach the item to spawn on completion.
 * STRUCTURAL MATCH 112/112, opcode streams identical (multi-def u8* p
 * device keeps add(page,ret*240)+addi 2976). PARKED renum residual:
 * {pos=r26,hdr=r28,ret=r29,base=r27} vs ours {r28,r26,r27,r29} + f0/f2
 * const-home swap. Exhausted: f32 z zero-local (adds stack slot, still
 * f2), decl-order/param-alias homes (no effect). Do not re-run. */
s32 fn_800920E0(f32* pos, struct item* item, f32 scale)
{
    EffectPage* page = (EffectPage*)EffectInfo;
    s32 ret;
    s32 ro;
    u8 _hi[12];
    volatile f32 v[3];
    volatile f32 pyr[3];
    u8 _lo[24];

    if (scale <= lbl_80348078) {
        scale = lbl_80348100;
    }
    v[0] = lbl_80127D20[0] * scale;
    v[1] = lbl_80127D20[1] * scale;
    v[2] = lbl_80127D20[2] * scale;
    pyr[0] = (f32)(lbl_803480E0 + Random(lbl_80348104));
    pyr[1] = lbl_80348068;
    pyr[2] = lbl_80348068;
    ret = -1;
    {
        EffectHeader* hdr = &page->info[68];
        struct atreeheader* at;
        if ((at = hdr->atree) != NULL) {
            ret = StartFXTree(at, pos, 0x200004, 0, lbl_803480F8);
        if (ret >= 0) {
            ro = ret * 240;
            MBTreeSetZsortAdd(page->fx[ret].node, hdr->zmod, 1);
            MBTreeSetAlpha(page->fx[ret].node, hdr->alpha, 1);
            page->fx[ret].type = (fx_type)68;
            }
        }
    }
    if (ret >= 0) {
        page->fx[ret].fxhit = 69;
        page->fx[ret].hit_audio = 0;
        page->fx[ret].wall_sound = 0;
    }
    if (ret >= 0) {
        Effect* e;
        f32 yaw;
        f32 vz;
        u8* p = (u8*)page;

        p += ret * 240;
        e = (Effect*)(p + 2976);
        vz = v[2];
        yaw = atan2(v[0], vz);
        e->vel[0] = v[0];
        e->vel[1] = v[1];
        e->vel[2] = v[2];
        if (e->node != NULL) {
            YawMat3(e->node, yaw);
        }
        e->pyrvel[0] = pyr[0];
        e->pyrvel[1] = pyr[1];
        e->pyrvel[2] = pyr[2];
        if (lbl_803480FC >= lbl_80348078) {
            e->weight = lbl_803480FC;
        }
        if (lbl_803480A0 >= lbl_80348078) {
            e->colrad = lbl_803480A0;
        }
    }
    page->fx[ret].additem = item;
    return ret;
}

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
        MBTreeSetZsortAdd(page->fx[idx].node, h->zmod, 1);
        MBTreeSetAlpha(page->fx[idx].node, h->alpha, 1);
        page->fx[idx].type = FX_BLOCK;
    }
    if (idx >= 0) {
        MBTreeSetColor(page->fx[idx].node,
                    lbl_8011A178[*(s32*)(gPlayers + pnum * 0x335C + 4)], 1);
        MBTreeSetAlpha(page->fx[idx].node, 0x40, 1);
        if (idx >= 0) {
            Effect* e = &page->fx[idx];
            struct anode* root;

            MBNodeSetParent(e->node,
                        *(struct mbnode**)(gPlayers + pnum * 0x335C + 0x74));
            root = ATREE_ROOT(e);
            if (root != NULL) {
                MBTreeSetFlags(root->node, 0x10, 0);
            }
        }
    }
    return idx;
}

typedef struct ComboFxView {
    u8 _0000[24];
    f32 colors[7][3];
    s32 colorpick[4];
    u8 _007C[3436];
    s32 types[4];
    s32 mainType;
} ComboFxView;

/* Spawn the combo sphere for a player class and the optional colored combo
 * burst.  The class tables share the packed magic/effect definition page.
 * STRUCTURAL MATCH 150/150, opcode streams identical (GutsP inlines,
 * assignment-in-condition first guts, flags locals, multi-def p for the
 * colors address). PARKED: renum in the color-region mulli (r25 vs r24)
 * and second guts cluster (r22/r23/r26 rotation). Exhausted: type-local
 * staging (adds mr), plain-Guts (remat lis), open-coded hybrid (worse). */
s32 StartComboFX(f32* pos, s32 typeIndex, s32 colorIndex)
{
    ComboFxView* table = (ComboFxView*)lbl_80122088;
    EffectPage* page = (EffectPage*)EffectInfo;
    s32 ret = -1;

    if (colorIndex >= 0) {
        u32 flags = 0x80980;

        if ((ret = StartFXSubGutsP(page, table->mainType, pos, 0, flags, 0.0f)) >= 0) {
            f32* color;

            MBTreeSetColor(page->fx[ret].node, lbl_8011A178[colorIndex], 1);
            MBTreeSetAmbientAdd(page->fx[ret].node, 0x1FF, 1);
            {
                u8* p = (u8*)table;
                p += table->colorpick[colorIndex] * 12;
                color = (f32*)(p + 24);
            }
            if (ret >= 0) {
                page->fx[ret].lightrad = lbl_803480EC;
                if (color != NULL) {
                    page->fx[ret].lightcolor[0] = color[0];
                    page->fx[ret].lightcolor[1] = color[1];
                    page->fx[ret].lightcolor[2] = color[2];
                } else {
                    page->fx[ret].lightcolor[0] = light_color[0];
                    page->fx[ret].lightcolor[1] = light_color[1];
                    page->fx[ret].lightcolor[2] = light_color[2];
                }
            }
        }
    }
    if (typeIndex >= 0) {
        u32 flags = 0x400880;
        s32 subret = StartFXSubGutsP(page, table->types[typeIndex], pos, 0, flags, 0.0f);

        ret = subret;
        if (subret >= 0) {
            MBTreeSetAmbientAdd(page->fx[ret].node, 0x1FF, 1);
        }
    }
    return ret;
}

s32 StartLevelUpFX(f32* pos, s32 color)
{
    u8 unused[8];
    return StartFXSubGuts(lbl_80122E60[color], pos, 0, 0x880800, 0.0f);
}

extern f64 lbl_80348108;
/* typed view of the magic FX def table at lbl_80122088: MWCC emits the
 * member-array accesses as add(base,scaled-index) + member-offset
 * displacement, which raw byte math refuses to produce */
typedef struct MagicView {
    u8 _p0[24];
    f32 colors[1][3];   /* +24  color triplets, stride 12  */
    u8 _p1[72];
    s32 colorpick[1];   /* +108 per-player-class color idx */
    u8 _p2[12];
    s32 coloridx[1];    /* +124 per-magic-type color idx   */
    u8 _p3a[3216];
    s32 hitmorph[5];    /* +3344 default hit fx by damage type */
    s32 kindidA[1];     /* +3364 base fx id per element    */
    u8 _q0[16];
    s32 kindidB[1];     /* +3384 alt fx id per element     */
    u8 _q1[16];
    s32 kindidC[1];     /* +3404 base big fx id            */
    u8 _q2[16];
    s32 kindidD[1];     /* +3424 alt big fx id             */
    u8 _q3[16];
    s32 kindidE[1];     /* +3444 base beam fx id           */
    u8 _q4[16];
    s32 kindidF[1];     /* +3464 alt beam fx id            */
    u8 _q5[16];
    s32 magicid[1];     /* +3484 magic fx id per type      */
    u8 _p3b[16];
    s32 throwid[1];     /* +3504 throw fx id per type      */
    u8 _p3c[16];
    s32 shieldid[1];    /* +3524 shield fx id per type     */
    u8 _p4[52];
    s32 fxflags[1];     /* +3580 damage-type flags per fx  */
    u8 _p5[16];
    f32 pyr[3];         /* +3600 throw pyr velocity        */
} MagicView;
extern f32 lbl_803480F8;
extern f32 lbl_80348068;
extern f64 lbl_80348078;
extern f32 lbl_803480A0;
extern f64 lbl_80348110;
void fn_80093E50(s32 idx, f32* a, f32* b, f32 x, f32 y);
extern f64 lbl_80348118;
extern f64 lbl_80348120;

/* 0x80092794 StartShieldFX -- spawn the shield magic fx for a player and
 * scale its size/damage/light from the cast size. */
s32 StartShieldFX(f32* pos, s32 type, s32 player, f32 dmg, f32 size)
{
    s32 t4 = type & 0xF;
    MagicView* tbl = (MagicView*)lbl_80122088;
    EffectPage* page = (EffectPage*)EffectInfo;
    s32 ret;
    s32 cp;
    f32* cp3;
    f32 rad;
    f64 t;
    u32 fl;
    s32 ro;
    u8* ep;
    struct mbnode* nd;
    Effect* e;
    u8 _spare[24];

    rad = (f32)(lbl_80348108 * size);
    ret = StartFXSub(tbl->shieldid[type & 0xF], pos, 314, 0x800,
                     lbl_803480F8);
    if (ret < 0) {
        ret = -1;
    } else {
        fn_80093E50(ret, (f32*)0, (f32*)0, lbl_80348068, lbl_80348068);
        page->fx[ret].fxhit = 0;
        page->fx[ret].hit_audio = 0;
        page->fx[ret].wall_sound = 0;
    }
    if (ret >= 0) {
        Effect* e = &page->fx[ret];
        e->weight = lbl_80348068;
        if (size >= lbl_80348078) {
            e->colrad = size;
        }
    }
    fl = type;
    if (ret >= 0) {
        Effect* e = &page->fx[ret];
        if ((s32)(fl & 0xF) >= 5) {
            fl &= ~0xC;
        }
        e->damage = dmg;
        e->damagetype = (DMG_TYPE)fl;
        e->damageradius = size;
        e->damagedelay = lbl_80348068;
        e->owner = player + 1;
    }
    if (rad < lbl_80348110) {
        t = lbl_80348110;
    } else if (rad > lbl_80348118) {
        t = lbl_80348118;
    } else {
        t = rad;
    }
    rad = (f32)t;
    ro = ret * 240;
    ep = (u8*)page + ro;
    nd = *(struct mbnode**)(ep + 2996);
    e = (Effect*)(ep + 2976);
    if (nd != NULL) {
        MBTreeSetFlags(nd, 8, 0);
        *(f32*)((u8*)e->node + 64) = rad;
        *(f32*)((u8*)e->node + 68) = lbl_803480A0;
        *(f32*)((u8*)e->node + 72) = rad;
    }
    MBTreeSetFlags(*(struct mbnode**)((u32)page + ret * 240 + 2996), 0x90800, 1);
    cp = tbl->coloridx[t4];
    cp3 = tbl->colors[cp];
    if (ret >= 0) {
        u8* e4 = (u8*)page + ro;
        *(f32*)(e4 + 2992) = (f32)(lbl_80348120 * size);
        if (cp3 != NULL) {
            e->lightcolor[0] = cp3[0];
            *(f32*)(e4 + 2980) = cp3[1];
            *(f32*)(e4 + 2984) = cp3[2];
        } else {
            e->lightcolor[0] = light_color[0];
            *(f32*)(e4 + 2980) = light_color[1];
            *(f32*)(e4 + 2984) = light_color[2];
        }
    }
    return ret;
}


#pragma opt_lifetimes off
s32 StartMagicHealFX(f32 scale, f32* pos)
{
    EffectPage* page = (EffectPage*)EffectInfo;
    u8 unused[8];
    s32 idx;
    f32 s;
    Effect* e;
    struct mbnode* node;

    s = 0.03125 * scale;
    idx = StartFXSubGuts(FX_MAGICHEAL, pos, 0, 0x880, 0.0f);
    if (s > 1.0) {
        s = 1.0f;
    }
    e = (Effect*)&page->info[idx * 20];
    node = ((EffectPage*)e)->fx[0].node;

    e = (Effect*)((u8*)e + 2976);
    if (node != NULL) {
        MBTreeSetFlags(node, 8, 0);
        e->node->scale[0] = s;
        e->node->scale[1] = s;
        e->node->scale[2] = s;
    }
    return idx;
}
#pragma opt_lifetimes reset

s32 StartMagicPlayerFX(f32* pos)
{
    u8 unused[8];
    return StartFXSubGuts(FX_START_MAGIC, pos, 0, 0x880, 0.0f);
}

/* 0x80092DF4 StartMagicFX -- bodied below (older pass). */


extern char lbl_80114790[];     /* "Bad throw effect" fmt */
extern f64 lbl_80348128;
extern f64 lbl_80348118;
extern f64 lbl_80348120;
extern f32 lbl_80348130;
extern f32 lbl_80348134;

/* throw-specialized guts clone: time constant lives inside the body so its
 * load stays below the atree branch (arg-eval at an inline call site hoists
 * a global-load argument above the inlined bounds checks). */
static s32 StartThrowGutsX(EffectPage* page, s32 type, f32* pos)
{
    s32 idx = -1;
    EffectHeader* h;

    if (type < 0 || type >= MAXEFFECTTYPES) {
        ErrorPrintf("Bad Effect type: %d", type);
        return -1;
    }
    h = &page->info[type];
    if (h->atree != NULL && (idx = StartFXTree(h->atree, pos, 0x20010E, 0x800, lbl_80348130)) >= 0) {
        MBTreeSetZsortAdd(page->fx[idx].node, h->zmod, 1);
        MBTreeSetAlpha(page->fx[idx].node, h->alpha, 1);
        page->fx[idx].type = (fx_type)type;
    }
    return idx;
}

/* 0x80092B58 StartThrowMagicFX -- spawn a thrown-magic projectile: def id
 * from the magic table, launch velocity/yaw from vel, damage + light setup.
 * STRUCTURAL MATCH 167/167 via StartThrowGutsX clone (time const inside the
 * inlinee sinks its lfs below the atree branch; a GutsP time argument gets
 * hoisted at arg-eval), vz arg2-first atan2 temp, ep multi-def address
 * blocks. PARKED residual: one clrlwi (t4) 3 slots early + renum (rad/size
 * f30<->f31, e web r23 vs r3/r20, fxh r3/r4). Exhausted: t4 stmt placement
 * x4, cast-transit, assignment-in-index, sz param-copy, e/ep chain forms.
 * Do not re-run. */
s32 StartThrowMagicFX(f32* pos, f32* vel, s32 type, s32 player, s32 snd,
                      f32 weight, f32 dmg, f32 size)
{
    MagicView* tbl = (MagicView*)lbl_80122088;
    EffectPage* page = (EffectPage*)EffectInfo;
    f32 sz = size;
    s32 t4;
    s32 ret;
    s32 cp;
    f32* cp3;
    f32 rad;
    s32 fxh;
    u32 fl;
    Effect* e;
    f32 yaw;
    f32 vz;
    u8* ep;
    u8* e4;

    rad = (f32)(lbl_80348128 * sz);
    if (rad > lbl_80348118) {
        rad = lbl_803480A0;
    }
    t4 = type & 0xF;
    ret = StartThrowGutsX(page, tbl->throwid[type & 0xF], pos);
    if (ret >= 0) {
        ep = (u8*)page;
        ep += ret * 240;
        ep += 2976;
        e = (Effect*)ep;
        if (vel != NULL) {
            vz = vel[2];
            yaw = atan2(vel[0], vz);
            e->vel[0] = vel[0];
            e->vel[1] = vel[1];
            e->vel[2] = vel[2];
            if (e->node != NULL) {
                YawMat3(e->node, yaw);
            }
        }
        if (tbl->pyr != NULL) {
            e->pyrvel[0] = tbl->pyr[0];
            e->pyrvel[1] = tbl->pyr[1];
            e->pyrvel[2] = tbl->pyr[2];
        }
        if (weight >= lbl_80348078) {
            e->weight = weight;
        }
        e->colrad = lbl_80348134;
    }
    fxh = tbl->magicid[t4];
    if (ret >= 0) {
        e4 = (u8*)page;
        e4 += ret * 240;
        *(s16*)(e4 + 3166) = fxh;
        *(s32*)(e4 + 3180) = snd;
        *(s32*)(e4 + 3184) = snd;
    }
    fl = type;
    if (ret >= 0) {
        ep = (u8*)page;
        ep += ret * 240;
        e = (Effect*)(ep + 2976);
        if ((s32)(fl & 0xF) >= 5) {
            fl &= ~0xC;
        }
        e->damage = dmg;
        e->damagetype = (DMG_TYPE)fl;
        e->damageradius = sz;
        e->damagedelay = lbl_80348068;
        e->owner = player + 1;
    }
    page->fx[ret].hitscale = rad;
    cp = tbl->coloridx[t4];
    e4 = (u8*)tbl;
    e4 += cp * 12;
    cp3 = (f32*)(e4 + 24);
    if (ret >= 0) {
        e4 = (u8*)page + ret * 240;
        *(f32*)(e4 + 2992) = (f32)(lbl_80348120 * sz);
        if (cp3 != NULL) {
            *(f32*)(e4 + 2976) = cp3[0];
            *(f32*)(e4 + 2980) = cp3[1];
            *(f32*)(e4 + 2984) = cp3[2];
        } else {
            *(f32*)(e4 + 2976) = light_color[0];
            *(f32*)(e4 + 2980) = light_color[1];
            *(f32*)(e4 + 2984) = light_color[2];
        }
    }
    return ret;
}


/* pool-entry view for the magic-FX param block */
typedef struct MagicFxView {
    u8  _pad00[124];
    f32 timer;              /* 0x7C */
    u8  _pad80[44];
    f32 power;              /* 0xAC */
    u8  _padB0[4];
    s32 flags;              /* 0xB4 */
    f32 scale;              /* 0xB8 */
    u16 owner;              /* 0xBC */
} MagicFxView;

/* shared magic-FX param seeding (auto-inlined at each Start* site) */
static void SetMagicParams(u8* base, s32 idx, s32 tf, f32 power, f32 scale,
                           s32 owner)
{
    if (idx >= 0) {
        u8* mp = base;
        MagicFxView* fx;
        mp += idx * 240;
        fx = (MagicFxView*)(mp + 2976);
        if ((tf & 15) >= 5) {
            tf = tf & ~0xC;
        }
        fx->power = power;
        fx->flags = tf;
        fx->scale = scale;
        fx->timer = 0.0f;
        fx->owner = owner + 1;
    }
}

extern s32 sMusicTrackHi;
extern f32 lbl_80348134;
extern f32 lbl_80348138;
extern f32 lbl_8034813C;
extern f32 lbl_80348140;
extern f32 lbl_80348144;
extern f32 lbl_80348148;
extern f64 lbl_80348150;
extern f64 lbl_803480A8;
extern f32 lbl_803480EC;

/* 0x80092FC0 SuicideExplosion -- the potion/suicide blast: main explosion,
 * smoke, flash, plus a deathmatch-mode variant.
 * STRUCTURAL MATCH 255/255, opcode streams identical. KEY DEVICES: scoped
 * #pragma opt_propagation off around the fn keeps the inlined (tf&15)>=5
 * test on constant tf (folds otherwise); SetSuicideParamsA/B clones keep
 * the scale-constant lfs under the idx guard; cast-transit e4; direct
 * tbl->colors[0][i]; _spare[16] frame pad. PARKED: renum only (ret r29
 * vs r28, y-block r3/r5, r24/r26). */
/* per-branch SetMagicParams clones for SuicideExplosion: the scale constant
 * lives inside the body so its lfs stays under the idx guard (a global-load
 * argument is evaluated at the inline boundary), and the caller's scoped
 * opt_propagation off keeps the (tf&15)>=5 test on a constant tf. */
static void SetSuicideParamsA(u8* base, s32 idx, s32 tf, f32 power)
{
    if (idx >= 0) {
        u8* mp = base;
        MagicFxView* fx;
        mp += idx * 240;
        fx = (MagicFxView*)(mp + 2976);
        if ((tf & 15) >= 5) {
            tf = tf & ~0xC;
        }
        fx->power = power;
        fx->flags = tf;
        fx->scale = lbl_80348138;
        fx->timer = 0.0f;
        fx->owner = 0;
    }
}

static void SetSuicideParamsB(u8* base, s32 idx, s32 tf, f32 power)
{
    if (idx >= 0) {
        u8* mp = base;
        MagicFxView* fx;
        mp += idx * 240;
        fx = (MagicFxView*)(mp + 2976);
        if ((tf & 15) >= 5) {
            tf = tf & ~0xC;
        }
        fx->power = power;
        fx->flags = tf;
        fx->scale = lbl_80348144;
        fx->timer = 0.0f;
        fx->owner = 0;
    }
}

#pragma opt_propagation off
s32 SuicideExplosion(f32* pos, f32 dmg)
{
    MagicView* tbl = (MagicView*)lbl_80122088;
    EffectPage* page = (EffectPage*)EffectInfo;
    s32 ret;
    s32 a;
    s32 ro;
    s32 fl;
    struct mbnode* nd;
    u8* ha;
    f32 y;
    u8 _spare[16];

    if (sMusicTrackHi == 11 || sMusicTrackHi == 7) {
        a = StartFXSub(25, pos, 43, 0x800, lbl_80348068);
        if (a < 0) {
            a = -1;
        } else {
            fn_80093E50(a, (f32*)0, (f32*)0, lbl_80348068, lbl_80348068);
            page->fx[a].fxhit = 0;
            page->fx[a].hit_audio = 4;
            page->fx[a].wall_sound = 0;
        }
        ret = a;
        SetSuicideParamsA((u8*)page, a, 2048, dmg);
        ro = a * 240;
        nd = page->fx[a].node;
        {
            f32* yp = (f32*)nd;
            y = yp[13];
            yp[13] = (f32)(y + lbl_803480A8);
        }
        if (a >= 0) {
            *(s16*)((u8*)page + ro + 3168) = 26;
            *(s16*)((u8*)page + ro + 3170) = 27;
            *(u32*)((u8*)page + ro + 3076) |= 0x4000;
            *(f32*)((u8*)page + ro + 3092) = lbl_8034813C;
        }
        {
            u8* ep = (u8*)page + ro;
            struct mbnode* nd2;
            Effect* e = (Effect*)(ep + 2976);
            if ((nd2 = *(struct mbnode**)(ep + 2996)) != NULL) {
                f32 k1;
                MBTreeSetFlags(nd2, 8, 0);
                k1 = lbl_80348140;
                *(f32*)((u8*)e->node + 64) = k1;
                *(f32*)((u8*)e->node + 68) = lbl_803480A0;
                *(f32*)((u8*)e->node + 72) = k1;
            }
        }
    } else {
        EffectHeader* hdr = &page->info[22];
        struct atreeheader* at;
        Effect* e3;
        s32 sm;

        ret = -1;
        if ((at = hdr->atree) != NULL) {
            ret = StartFXTree(at, pos, 41, 0x880, lbl_80348068);
            if (ret >= 0) {
                MBTreeSetZsortAdd(page->fx[ret].node, hdr->zmod, 1);
                MBTreeSetAlpha(page->fx[ret].node, hdr->alpha, 1);
                page->fx[ret].type = (fx_type)22;
            }
        }
        if (ret < 0) {
            return -1;
        }
        ro = ret * 240;
        ha = (u8*)page + 3180;
        *(s32*)(ha + ro) = 4;
        SetSuicideParamsB((u8*)page, ret, 1057, dmg);
        {
            u8* ep = (u8*)page + ro;
            e3 = (Effect*)(ep + 2976);
            if ((nd = *(struct mbnode**)(ep + 2996)) != NULL) {
                f32 k1;
                MBTreeSetFlags(nd, 8, 0);
                k1 = lbl_803480A0;
                *(f32*)((u8*)e3->node + 64) = k1;
                *(f32*)((u8*)e3->node + 68) = k1;
                *(f32*)((u8*)e3->node + 72) = k1;
            }
        }
        sm = StartFXSub(28, pos, 0, 0x800, lbl_80348068);
        if (sm < 0) {
            sm = -1;
        } else {
            fn_80093E50(sm, (f32*)0, (f32*)0, lbl_80348068, lbl_80348068);
            page->fx[sm].fxhit = 0;
            *(s32*)(ha + sm * 240) = 0;
            page->fx[sm].wall_sound = 0;
        }
        {
            u8* ep = (u8*)page + sm * 240;
            struct mbnode* nd2;
            Effect* e = (Effect*)(ep + 2976);
            if ((nd2 = *(struct mbnode**)(ep + 2996)) != NULL) {
                f32 k1;
                MBTreeSetFlags(nd2, 8, 0);
                k1 = lbl_80348148;
                *(f32*)((u8*)e->node + 64) = k1;
                *(f32*)((u8*)e->node + 68) = k1;
                *(f32*)((u8*)e->node + 72) = k1;
            }
        }
        if (ret >= 0) {
            u8* e4 = (u8*)((u32)page + (u32)ro);
            *(f32*)(e4 + 2992) = (f32)(lbl_80348150 * lbl_803480EC);
            if (tbl->colors[0] != NULL) {
                e3->lightcolor[0] = tbl->colors[0][0];
                *(f32*)(e4 + 2980) = tbl->colors[0][1];
                *(f32*)(e4 + 2984) = tbl->colors[0][2];
            } else {
                e3->lightcolor[0] = light_color[0];
                *(f32*)(e4 + 2980) = light_color[1];
                *(f32*)(e4 + 2984) = light_color[2];
            }
        }
    }
    a = StartFXSub(80, pos, 0, 0x800, lbl_80348068);
    if (a < 0) {
        a = -1;
    } else {
        fn_80093E50(a, (f32*)0, (f32*)0, lbl_80348068, lbl_80348068);
        page->fx[a].fxhit = 0;
        page->fx[a].hit_audio = 0;
        page->fx[a].wall_sound = 0;
    }
    if (a >= 0) {
        page->fx[a].fxfade = lbl_80348134;
    }
    return ret;
}
#pragma opt_propagation reset

extern f64 lbl_803480F0;        /* death launch velocity factor */
extern f32 lbl_803480F8;        /* death timer preset */
extern f32 lbl_803480FC;        /* death power preset */
extern void MBTreeSetZsortAdd(struct mbnode* node, s32 v, s32 a);
extern void MBTreeSetAlpha(struct mbnode* node, s32 v, s32 a);

static void SetEnemyDeathParams(u8* base, s32 idx, s32 tf)
{
    if (idx >= 0) {
        MagicFxView* mp = (MagicFxView*)(base + idx * 240);
        mp = (MagicFxView*)((u8*)mp + 2976);
        if ((tf & 15) >= 5) {
            tf &= ~0xC;
        }
        mp->power = lbl_803480FC;
        mp->flags = tf;
        mp->scale = 0.0f;
        mp->timer = 0.0f;
        mp->owner = 0;
    }
}

#pragma opt_propagation off
s32 StartEnemyDeathFX(u8* en)
{
    u8* base = (u8*)EffectInfo;
    s32 idx;
    u8* ep;
    u8* q = base + 1056;
    u8* fxp;
    s32 off;
    struct mbnode* node;
    f32 yaw;
    f32 v[5];
    f32* vp = &v[2];
    u32 flags;

    ep = en;
    flags = 0x8C01;
    idx = -1;
    if (*(void**)q == 0) {
        goto done;
    }
    idx = -1;
    if (*(void**)q != 0 &&
        (idx = StartFXTree(*(struct atreeheader**)q, (f32*)(ep + 48),
                           flags, 0x20800, 0.0f)) >= 0) {
        off = idx * 240;
        fxp = base;
        fxp += off;
        node = *(struct mbnode**)(fxp += 2996);
        MBTreeSetZsortAdd(node, *(s32*)(q + 4), 1);
        MBTreeSetAlpha(*(struct mbnode**)fxp, *(s32*)(q + 8), 1);
        fxp = base + off;
        *(s32*)(fxp + 3072) = 88;
    }
    v[2] = (f32)(lbl_803480F0 * *(f32*)(ep + 32));
    v[3] = (f32)(lbl_803480F0 * *(f32*)(ep + 36));
    v[4] = (f32)(lbl_803480F0 * *(f32*)(ep + 40));
    if (idx >= 0) {
        q = base + idx * 240;
        q += 2976;
        yaw = vp[2];
        yaw = atan2(vp[0], yaw);
        *(f32*)(q + 128) = vp[0];
        *(f32*)(q + 132) = vp[1];
        *(f32*)(q + 136) = vp[2];
        if (*(struct mbnode**)(q + 20) != 0) {
            YawMat3(*(struct mbnode**)(q + 20), yaw);
        }
        *(f32*)(q + 160) = 0.0f;
        *(f32*)(q + 152) = lbl_803480F8;
    }
    SetEnemyDeathParams(base, idx, 0x100020);
    if (idx >= 0) {
        u8* r = base + idx * 240;
        *(u16*)(r + 3168) = 89;
        *(s16*)(r + 3170) = -1;
        *(u32*)(r + 3076) = *(u32*)(r + 3076) | 0x4000;
        *(f32*)(r + 3092) = lbl_803480F8;
    }
done:
    return idx;
}
#pragma opt_propagation reset


extern f64 lbl_80348128;        /* magic scale factor */
extern f64 lbl_80348118;        /* magic scale cap test */
extern f32 lbl_803480A0;        /* magic scale cap */
extern f64 lbl_80348120;        /* light radius factor */
void fn_80093E50(s32 idx, f32* a, f32* b, f32 x, f32 y);

s32 StartMagicFX(f32* pos, s32 tf, s32 owner, f32 power, f32 scale)
{
    u8* base = (u8*)EffectInfo;
    s32 idx;
    s32 low;
    s32* tab = lbl_80122088;
    f32 s;
    u8* e;
    Effect* ef;
    struct mbnode* node;
    f32* col;
    s32 ci;

    low = tf & 15;
    s = (f32)(lbl_80348128 * scale);
    idx = StartFXSub((&tab[tf & 15])[871], pos, 298, 2048, 0.0f);
    if (idx < 0) {
        idx = -1;
    } else {
        fn_80093E50(idx, 0, 0, 0.0f, 0.0f);
        e = base + idx * 240;
        *(u16*)(e + 3166) = 0;
        *(u32*)(e + 3180) = 0;
        *(u32*)(e + 3184) = 0;
    }
    SetMagicParams(base, idx, tf, power, scale, owner);
    if (s > lbl_80348118) {
        s = lbl_803480A0;
    }
    owner = idx * 240;
    ef = (Effect*)(base + owner);
    node = ((EffectPage*)ef)->fx[0].node;
    ef = (Effect*)((u8*)ef + 2976);
    if (node != NULL) {
        MBTreeSetFlags(node, 8, 0);
        ef->node->scale[0] = s;
        ef->node->scale[1] = s;
        ef->node->scale[2] = s;
    }
    ci = (&tab[low])[31];
    col = (f32*)&(&tab[ci * 3])[6];
    if (idx >= 0) {
        e = base + owner;
        *(f32*)(e + 2992) = (f32)(lbl_80348120 * scale);
        if (col != 0) {
            *(f32*)ef = col[0];
            *(f32*)(e + 2980) = col[1];
            *(f32*)(e + 2984) = col[2];
        } else {
            *(f32*)ef = light_color[0];
            *(f32*)(e + 2980) = light_color[1];
            *(f32*)(e + 2984) = light_color[2];
        }
    }
    return idx;
}

extern f32 lbl_80348158;
extern f64 lbl_80348160;
extern f32 lbl_80348168;
extern f32 lbl_8034816C;
extern f32 lbl_80348170;
extern f32 lbl_80348174;
extern f32 lbl_80348178;
extern f64 lbl_80348180;
extern f64 lbl_80348188;
extern f64 lbl_80348190;
extern f32 lbl_80348198;
extern f64 lbl_803481A0;
extern f32 lbl_803481A8;
extern f32 lbl_80127D40[3];
extern f32 lbl_80127D50[3];
extern void YawVec3(void* axis, f32* out, f32 angle);
extern void PitchVec3(f32* a, f32* b, f32 angle);
extern void MBPsysFlame(s32 a, struct mbnode* node, f32* dir, f32 t, f32 x,
                        f32 y);

/* 0x800933BC StartExplosion -- typed explosion at an object: per-type node
 * radius/morph setup, light, then deathmatch debris flames.
 * NEAR-STRUCTURAL 342/343 (was 27 op-diffs, now 14 micro-placements):
 * fixed via ep-multi-def blocks, at-temp atree read, yp[13] y-bump, dr
 * loop-hoist, direct tbl->colors[0][i], SetMagicParams mp-multi-def body.
 * PARKED: 4x e-addi placed before the nd lwz (nd-first stmt adds mr,
 * comma form spills), pv-copy lwz/lfs order, case25 shared-add CSE
 * (cast-transit no-op), flame-block addi CSE. */
s32 StartExplosion(u8* en, s32 type, f32 dmg)
{
    MagicView* tbl = (MagicView*)lbl_80122088;
    EffectPage* page = (EffectPage*)EffectInfo;
    s32 ret;
    s32 ro;
    u32 fl;
    f32 rad;
    struct mbnode* nd;
    s32 t;
    s32 i;
    f32 v[3];
    u8 _spare[8];

    t = type;
    if (type == 29) {
        t = 22;
    }
    ret = StartFXSub(t, (f32*)(en + 48), 43, 0x800, lbl_80348068);
    if (ret < 0) {
        ret = -1;
    } else {
        fn_80093E50(ret, (f32*)0, (f32*)0, lbl_80348068, lbl_80348068);
        page->fx[ret].fxhit = 0;
        page->fx[ret].hit_audio = 4;
        page->fx[ret].wall_sound = 0;
    }
    switch (type) {
    case 29: {
        u8* nb;
        u8* hdr2;
        s32 r2;
        ro = ret * 240;
        rad = lbl_80348158;
        fl = 1057;
        {
            u8* ep = (u8*)page;
            Effect* e;
            ep += ro;
            e = (Effect*)(ep + 2976);
            if ((nd = *(struct mbnode**)(ep + 2996)) != NULL) {
                f32 k1;
                MBTreeSetFlags(nd, 8, 0);
                k1 = lbl_80348140;
                *(f32*)((u8*)e->node + 64) = k1;
                *(f32*)((u8*)e->node + 68) = lbl_803480A0;
                *(f32*)((u8*)e->node + 72) = k1;
            }
        }
        nb = (u8*)page + 2996;
        {
            struct mbnode* n2 = *(struct mbnode**)(nb + ro);
            EffectHeader* hdr = &page->info[29];
            struct atreeheader* at;
            f32 y;
            r2 = -1;
            {
                f32* yp = (f32*)n2;
                y = yp[13];
                yp[13] = (f32)(y + lbl_80348160);
            }
            if ((at = page->info[29].atree) != NULL) {
                r2 = StartFXTree(at, (f32*)(en + 48), 0, 0,
                                 lbl_80348068);
                if (r2 >= 0) {
                    s32 ro2 = r2 * 240;
                    MBTreeSetZsortAdd(*(struct mbnode**)(nb + ro2),
                                      hdr->zmod, 1);
                    MBTreeSetAlpha(*(struct mbnode**)(nb + ro2), hdr->alpha,
                                   1);
                    page->fx[r2].type = (fx_type)29;
                }
            }
            if (r2 >= 0) {
                u8* mp2 = (u8*)page;
                Effect* e;
                mp2 += r2 * 240;
                e = (Effect*)(mp2 + 2976);
                if (en != NULL) {
                    CopyMat3((f32*)en, e->node);
                }
                if ((f32*)(en + 48) != NULL) {
                    f32* pv = (f32*)(en + 48);
                    f32 t0;
                    t0 = pv[0];
                    *(f32*)((u8*)e->node + 48) = t0;
                    t0 = pv[1];
                    *(f32*)((u8*)e->node + 52) = t0;
                    t0 = pv[2];
                    *(f32*)((u8*)e->node + 56) = t0;
                }
            }
        }
        break;
    }
    case 24: {
        f32 y;
        ro = ret * 240;
        rad = lbl_80348158;
        fl = 1057;
        {
            u8* ep = (u8*)page;
            Effect* e;
            ep += ro;
            e = (Effect*)(ep + 2976);
            if ((nd = *(struct mbnode**)(ep + 2996)) != NULL) {
                f32 k1;
                MBTreeSetFlags(nd, 8, 0);
                k1 = lbl_80348168;
                *(f32*)((u8*)e->node + 64) = k1;
                *(f32*)((u8*)e->node + 68) = lbl_803480A0;
                *(f32*)((u8*)e->node + 72) = k1;
            }
        }
        {
            u8* ep = (u8*)page;
            f32* yp;
            ep += ro;
            nd = *(struct mbnode**)(ep + 2996);
            yp = (f32*)nd;
            y = yp[13];
            yp[13] = (f32)(y + lbl_80348150);
        }
        break;
    }
    default: {
        ro = ret * 240;
        rad = lbl_80348144;
        fl = 1057;
        {
            u8* ep = (u8*)page;
            Effect* e;
            ep += ro;
            e = (Effect*)(ep + 2976);
            if ((nd = *(struct mbnode**)(ep + 2996)) != NULL) {
                f32 k1;
                MBTreeSetFlags(nd, 8, 0);
                k1 = lbl_8034816C;
                *(f32*)((u8*)e->node + 64) = k1;
                *(f32*)((u8*)e->node + 68) = lbl_803480A0;
                *(f32*)((u8*)e->node + 72) = k1;
            }
        }
        break;
    }
    case 23:
        rad = lbl_80348170;
        fl = 17;
        break;
    case 25: {
        ro = ret * 240;
        rad = lbl_80348174;
        fl = 2048;
        if (ret >= 0) {
            u8* mp = (u8*)page;
            Effect* e;
            mp += ro;
            e = (Effect*)(mp + 2976);
            e->fxmorph = 26;
            e->fxmorph2 = 27;
            e->flags |= 0x4000;
            e->morphtime = lbl_80348170;
        }
        {
            u8* ep = (u8*)page;
            Effect* e;
            ep += ro;
            e = (Effect*)(ep + 2976);
            if ((nd = *(struct mbnode**)(ep + 2996)) != NULL) {
                f32 k1;
                MBTreeSetFlags(nd, 8, 0);
                k1 = lbl_80348178;
                *(f32*)((u8*)e->node + 64) = k1;
                *(f32*)((u8*)e->node + 68) = lbl_803480A0;
                *(f32*)((u8*)e->node + 72) = k1;
            }
        }
        break;
    }
    }
    if ((fl & 1) && ret >= 0) {
        u8* e4 = (u8*)page + ret * 240;
        *(f32*)(e4 + 2992) = (f32)(lbl_80348150 * rad);
        if (tbl->colors[0] != NULL) {
            *(f32*)(e4 + 2976) = tbl->colors[0][0];
            *(f32*)(e4 + 2980) = tbl->colors[0][1];
            *(f32*)(e4 + 2984) = tbl->colors[0][2];
        } else {
            *(f32*)(e4 + 2976) = light_color[0];
            *(f32*)(e4 + 2980) = light_color[1];
            *(f32*)(e4 + 2984) = light_color[2];
        }
    }
    SetMagicParams((u8*)page, ret, fl, dmg, rad, -1);
    if (sMusicTrackHi == 10 && type == 24) {
        f32 dr = lbl_80348144;
        f64 kA = lbl_80348180;
        f64 kB = lbl_80348188;
        f64 kC = lbl_80348190;
        f64 kD = lbl_803481A0;
        for (i = 0; i < 3; i++) {
            f32 ft = (f32)(kA + Random(lbl_80348134));
            f32 fs = (f32)(kB + Random(lbl_803480F8));
            f32 fy = (f32)(kC + Random(lbl_80348198));
            f32 fp = (f32)(kD + Random(lbl_803481A8));
            s32 d;
            YawVec3(lbl_80127D40, v, fy);
            PitchVec3(v, v, -fp);
            v[0] = v[0] * fs;
            v[1] = v[1] * fs;
            v[2] = v[2] * fs;
            d = StartFXSub(0, (f32*)(en + 48), 0x10020000, 0x800, ft);
            if (d < 0) {
                d = -1;
            } else {
                fn_80093E50(d, v, (f32*)0, lbl_80348068, lbl_80348068);
                page->fx[d].fxhit = -1;
                page->fx[d].hit_audio = -1;
                page->fx[d].wall_sound = -1;
            }
            if (d >= 0) {
                page->fx[d].damageradius = dr;
                page->fx[d].hitcount = 180;
                MBPsysFlame(0, page->fx[d].node, lbl_80127D50, ft,
                            lbl_80348134, lbl_8034813C);
            }
        }
    }
    return ret;
}




extern struct atreeheader* FamiliarSpit[];
extern f64 lbl_80348078;
extern f32 lbl_80348068;

/* 0x80093918 -- spawn a familiar-spit style projectile fx for a player:
 * velocity from vec*scale, yaw from the velocity, damage/light from the
 * magic def table. */
s32 fn_80093918(s32 idx, s32 player, f32* pos, f32* vec, f32 scale, f32 spd,
                f32 h)
{
    MagicView* tbl = (MagicView*)lbl_80122088;
    u8* fx = (u8*)EffectInfo;
    s32 ret;
    u32 flags;
    s32 pi;
    s32 cp;
    f32* cp3;
    volatile f32 v[3];

    ret = StartFXTree(FamiliarSpit[idx], pos, 0x101000E, 0x880, lbl_803480F8);
    if (ret < 0) {
        return -1;
    }
    v[0] = vec[0] * scale;
    v[1] = vec[1] * scale;
    v[2] = vec[2] * scale;
    if (ret >= 0) {
        Effect* e = (Effect*)(&fx[ret * 240] + 2976);
        f32 yaw = atan2(v[0], v[2]);
        e->vel[0] = v[0];
        e->vel[1] = v[1];
        e->vel[2] = v[2];
        if (e->node != NULL) {
            YawMat3(e->node, yaw);
        }
        if (h >= lbl_80348078) {
            e->weight = h;
        }
        e->colrad = lbl_803480A0;
    }
    flags = tbl->fxflags[idx];
    if (ret >= 0) {
        Effect* e = (Effect*)(&fx[ret * 240] + 2976);
        f32 k;
        if ((s32)(flags & 0xF) >= 5) {
            flags &= ~0xC;
        }
        k = lbl_80348068;
        e->damage = spd;
        e->damagetype = (DMG_TYPE)flags;
        e->damageradius = k;
        e->damagedelay = k;
        e->owner = player + 1;
    }
    pi = *(s32*)((u8*)gPlayers + player * 13148 + 4);
    cp = tbl->colorpick[pi];
    cp3 = tbl->colors[cp];
    if (idx >= 0) {
        u8* e3 = fx + idx * 240;
        *(f32*)(e3 + 2992) = lbl_803480F8;
        if (cp3 != NULL) {
            *(f32*)(e3 + 2976) = cp3[0];
            *(f32*)(e3 + 2980) = cp3[1];
            *(f32*)(e3 + 2984) = cp3[2];
        } else {
            *(f32*)(e3 + 2976) = light_color[0];
            *(f32*)(e3 + 2980) = light_color[1];
            *(f32*)(e3 + 2984) = light_color[2];
        }
    }
    return ret;
}

/* plain typed spawn at a position (no orientation) */
s32 StartFXNoLoop(s32 type, f32* pos)
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
            YawMat3(e->node, ang);
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
            YawMat3(e->node, ang);
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
        MBNodeSetParent(Effects[idx].node, parent);
        if (ATREE_ROOT(&Effects[idx]) != NULL) {
            MBTreeSetFlags(ATREE_ROOT(&Effects[idx])->node, 0x10, 0);
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

extern s32 lbl_8034482C;

/* 0x80094164 -- start a table-selected magic fx: table picked by the
 * beam-mode global and `which` (four duplicated StartFXSubGutsP inlines).
 * STRUCTURAL MATCH, 183/183, opcode streams identical. PARKED residual:
 * duplicated-inline renum in arms 2-4 only (arm 1 byte-exact) -- target
 * colors type=r26/h=r27/base=r28/mul=r29/ret=r30 there, ours rotates to
 * type=r30/ret=r29/h=r26/mul=r28/base=r27. Exhausted: ret-var vs direct
 * returns (identical output, axis dead), caller-local type staging (adds
 * addi copies), ternary select (breaks inlining). Do not re-run. */
s32 fn_80094164(f32* pos, u32 idx, s32 which)
{
    MagicView* tbl = (MagicView*)lbl_80122088;
    EffectPage* page = (EffectPage*)EffectInfo;
    u32 m = idx & 0xF;
    s32 ret;

    if (lbl_8034482C != 0) {
        if (which != 0) {
            ret = StartFXSubGutsP(page, tbl->kindidB[m], pos, 0, 0x880, 0.0f);
        } else {
            ret = StartFXSubGutsP(page, tbl->kindidA[m], pos, 0, 0x880, 0.0f);
        }
    } else {
        if (which != 0) {
            ret = StartFXSubGutsP(page, tbl->kindidD[m], pos, 0, 0x880, 0.0f);
        } else {
            ret = StartFXSubGutsP(page, tbl->kindidC[m], pos, 0, 0x880, 0.0f);
        }
    }
    return ret;
}

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

extern f64 lbl_80348060;
extern f64 lbl_803480B0;
extern s32 lbl_8034482C;
extern s32 lbl_80344BD0;

/* 0x800945D0 -- spawn a per-element magic burst fx: pick the def id from
 * the element tables, then scale the node + zsort from the def. */
s32 fn_800945D0(f32* pos, f32* mat, s32 idx, s32 alt, s32 kind, f32 scale)
{
    MagicView* tbl = (MagicView*)lbl_80122088;
    EffectPage* page = (EffectPage*)EffectInfo;
    s32 ret;
    s32 t = idx & 0xF;
    s32 tid;
    u32 next;
    u8* ep;
    struct mbnode* nd;
    Effect* e;
    f32 rad;

    if (kind == 11 || kind == 21) {
        if (alt != 0) {
            tid = tbl->kindidF[t];
        } else {
            tid = tbl->kindidE[t];
        }
        rad = (f32)(lbl_803480B0 * scale);
    } else if (kind == 29 || kind == 5 || lbl_8034482C != 0) {
        if (alt != 0) {
            tid = tbl->kindidB[t];
        } else {
            tid = tbl->kindidA[t];
        }
        rad = lbl_803480A0;
    } else {
        if (alt != 0) {
            tid = tbl->kindidD[t];
        } else {
            tid = tbl->kindidC[t];
        }
        rad = (f32)(lbl_803480B0 * scale);
    }
    if (tid == 4) {
        next = lbl_80344BD0 + 1;
        tid = (lbl_80344BD0 & 1) + 6;
        lbl_80344BD0 = next;
    }
    if (tid == 5) {
        next = lbl_80344BD0 + 1;
        tid = (lbl_80344BD0 & 1) + 8;
        lbl_80344BD0 = next;
    }
    ret = -1;
    if (tid < 0 || tid >= 218) {
        ErrorPrintf(lbl_80114790, tid);
        ret = -1;
    } else {
        EffectHeader* hdr = &page->info[tid];
        if (hdr->atree != NULL) {
            ret = StartFXTree(hdr->atree, pos, 0, 0x880, lbl_80348068);
            if (ret >= 0) {
                s32 ro = ret * 240;
                MBTreeSetZsortAdd(page->fx[ret].node, hdr->zmod, 1);
                MBTreeSetAlpha(page->fx[ret].node, hdr->alpha, 1);
                page->fx[ret].type = (fx_type)tid;
            }
        }
    }
    if (ret < 0) {
        return ret;
    }
    if (mat != NULL && ret >= 0) {
        ep = (u8*)page;
        ep += ret * 240;
        e = (Effect*)(ep += 2976);
        if (mat != NULL) {
            CopyMat3(mat, e->node);
        }
    }
    {
        ep = (u8*)page;
        ep += ret * 240;
        e = (Effect*)(ep += 2976);
        if ((nd = e->node) != NULL) {
            MBTreeSetFlags(nd, 8, 0);
            *(f32*)((u8*)e->node + 64) = rad;
            *(f32*)((u8*)e->node + 68) = rad;
            *(f32*)((u8*)e->node + 72) = rad;
        }
        MBTreeSetZsortAdd(
            e->node,
            (s32)(lbl_803480B0 + rad * (f32)page->info[t].zmod), 1);
    }
    return ret;
}


void ScaleFX(s32 idx, f32 sx, f32 sy, f32 sz)
{
    Effect* e = &Effects[idx];

    if (e->node != NULL) {
        MBTreeSetFlags(e->node, 8, 0);
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
        MBTreeSetZsortAdd(page->fx[idx].node, h->zmod, 1);
        MBTreeSetAlpha(page->fx[idx].node, h->alpha, 1);
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
    ATREE_ROOT(e) = AtreeInitSub(hdr, &e->atree[0], 0, flb, 0);
    if (ATREE_ROOT(e) == NULL) {
        return -1;
    }

    if (flb & 0x2000) {
        parent = lbl_80344EBC;
    } else if (flb & 0x800) {
        parent = lbl_80344BD4;
    } else {
        parent = gSceneRoot;
    }
    e->node = MBNewNode(parent, gIdentityMatrix, 1);
    if (e->node == NULL) {
        AtreeDelete(&e->atree[0]);
        return -1;
    }
    MBNodeSetParent(ATREE_ROOT(e)->node, e->node);

    ai = (struct fxanim*)&e->atree[4];
    if (time > 0.0) {
        e->endtime = gClockTime + time;
        ai->oneshot = 1;
    } else {
        n = ai->def->nframes;
        if (n == 0) {
            ai->oneshot = 1;
            n = 30;
        }
        e->endtime = 0.00111111 * ((f32)n * (f32)ai->def->rate) + gClockTime;
    }
    if (fla & 0x20000000) {
        ai->oneshot = 0;
    }
    e->maxtime = e->endtime - gClockTime;
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

extern s32 gGameBusy;
extern s32 gGameplayPauseTimer;
extern f32 gClockFrameStep;
extern f32 lbl_80344880;
extern void GetWorldMat(struct mbnode* node, f32* matrix, f32* offset);
extern void CreateDirMatrix(f32* matrix, f32* direction, f32* up);
extern void PitchMat3(f32* matrix, f32 angle);
extern void RollMat3(f32* matrix, f32 angle);
extern void UnparentMatrix(struct mbnode* node, f32* matrix);
extern f32 NormalVector2D(f32* vector);
extern s32 AnimateATree(void* tree, s32 sequence, s32 transition);
extern s32 damage_player();
extern s32 damage_enemy();
extern s32 CritterDamage();
extern void CritterSetFxHitTime(f32 damage, s32 player, s32 owner);
extern void PlayerDamagedEnemy();
extern void StartEnemyGrid(f32* pos, f32 radius);
extern s32 NextGridEnemy(void);
extern void StartItemGrid(f32 radius, f32* pos);
extern s32 NextGridItem(void);
extern void CritterCollideStart(f32 radius, f32* pos, s32 unused);
extern void* CritterExpCollide();
extern void* CritterMoveNodeCol();
extern void* MissileCollidePlayer(f32 radius, f32* oldpos, f32* newpos,
                                  f32* hitpos);
extern s32 MissileCollideEnemy();
extern s32 fn_8005FB48(f32 radius, f32* from, f32* to,
                       f32* limitPosition, s32 stopAtFirst);
extern void* WeaponWallCollide(f32 radius, f32* oldpos, f32* newpos,
                               f32* hitpos);
extern u32 WorldObjGetAllFlags(void* object);
extern void WorldObjectExplode(void* object, f32* hitpos);
extern f32 FloorPos(f32 fallback, f32 radius, f32* position, s32 mode);
extern void ReflectVector(f32* vector, f32* normal, f32* out);
extern void BossGenerateEnemy(f32* matrix);
extern struct item* PlaceItem(s32 type, s32 subtype, char* name, f32* matrix);
extern void AddItemSub(struct item* item);
extern void fn_8009D5E0(f32* pos);
extern void fn_8009EF7C();
extern s32 fn_800C0ADC(f32* pos, f32* color, f32 radius, f32 intensity);
extern f32 fn_8005F0F4();
extern void fn_8005BA1C();
extern f32 fn_8005C1DC();
extern void* fn_8005ED44();
extern void PlayerDamagedItem();
extern void fn_80037ED0();
extern s32 RandInt(s32 limit);
extern void MBPsysFirework();
extern void DmgFxAdd(s32 idx);
extern s32 MBOX_FindTexture_Sub(char* name, s32 a, s32 b, s32 c, s32 flag);
extern void* MBOX_FindObject(char* name);
extern void MBSetObject(struct mbnode* node, void* object);
extern void fn_8009C9DC(s32 mode, f32* position);
extern u8 gEnemies[];
extern s32 lbl_8034466C;
extern f32 sMusicFadeBase;
extern f32 lbl_80348100;
extern f32 lbl_80348250;
extern f32 lbl_80343DF4;
extern f32 lbl_80343DF8;
extern f64 lbl_80348098;
extern f64 lbl_803480A8;
extern f64 lbl_803480B0;
extern f64 lbl_80348110;
extern f64 lbl_80348120;
extern f32 lbl_8034813C;
extern f64 lbl_80348150;
extern f64 lbl_80348160;
extern f64 lbl_803481C0;
extern f32 lbl_803481C8;
extern f64 lbl_803481D0;
extern f64 lbl_803481D8;
extern f32 lbl_803481E0;
extern f32 lbl_80348210;
extern f32 lbl_80348240;
extern f32 lbl_80348244;
extern u8 lbl_8023CA98[];
extern u8 lbl_8023CB28[];

struct fxworldobj {
    /* 0x00 */ u8 _00[0x10];
    /* 0x10 */ u32 flags;
};

struct fxcritterdesc {
    /* 0x00 */ u8 _00[0x20];
    /* 0x20 */ s16 type;
};
struct fxcritterheader {
    /* 0x000 */ u8 _000[0x120];
    /* 0x120 */ struct fxcritterdesc* desc;
};
struct fxcritter {
    /* 0x00 */ u8 _00[4];
    /* 0x04 */ struct fxcritterheader* header;
    /* 0x08 */ u8 _08[0x44];
    /* 0x4C */ f32 effectpos[3];
    /* 0x58 */ u8 _58[0x68];
    /* 0xC0 */ struct mbnode* node;
    /* 0xC4 */ u8 _c4[8];
    /* 0xCC */ struct fxcritterobject* object;
    /* 0xD0 */ u8 _d0[0x9F0];
    /* 0xAC0 */ s32 boss_texture;
    /* 0xAC4 */ s16 boss_timer_a;
    /* 0xAC6 */ s16 boss_timer_b;
};

struct fxcritterobject {
    /* 0x00 */ u8 _00[0x78];
    /* 0x78 */ struct mbnode* node;
};

struct fxenemy {
    /* 0x000 */ u8 _000[0x44];
    /* 0x044 */ f32 effectpos[4];
    /* 0x054 */ f32 pos[3];
    /* 0x060 */ u8 _060[0x54];
    /* 0x0B4 */ s32 state;
    /* 0x0B8 */ u8 _0b8[0x148];
    /* 0x200 */ f32 health;
    /* 0x204 */ u8 _204[0x34];
    /* 0x238 */ f32 radius;
    /* 0x23C */ u8 _23c[0x78];
    /* 0x2B4 */ f32 fxhittime[5];
    /* 0x2C8 */ s32 fxhitidx;
};

struct fxplayer {
    /* 0x000 */ s32 index;
    /* 0x004 */ u8 _004[0x50];
    /* 0x054 */ f32 col_pos[3];
    /* 0x060 */ u8 _060[4];
    /* 0x064 */ f32 effectpos[3];
    /* 0x070 */ u8 _070[0x78];
    /* 0x0E8 */ s32 state;
    /* 0x0EC */ u8 _0ec[0x34];
    /* 0x120 */ u32 shield_flags;
    /* 0x124 */ u8 _124[0x72c];
    /* 0x850 */ f32 radius;
    /* 0x854 */ f32 halfheight;
    /* 0x858 */ u8 _858[0x90];
    /* 0x8E8 */ f32 fxhittime;
};

/* The item pool is a fixed array of 0xF0-byte records.  Keep this local view
 * complete enough for ProcessEffects' direct item-grid collision pass and the
 * following SfxSkipItem policy helper. */
struct fxitemdef {
    /* 0x00 */ s32 type;
    /* 0x04 */ s32 subtype;
    /* 0x08 */ s16 coltype;
    /* 0x0A */ s16 colflags;
    /* 0x0C */ f32 radius;
    /* 0x10 */ u8 _10[0x40];
};
struct fxworldinfo {
    /* 0x00 */ u8 _00[0x68];
    /* 0x68 */ struct fxitemdef* itemdefs;
    /* 0x6C */ u8 _6c[0x38];
};
struct fxitem {
    /* 0x00 */ struct fxitemdef* def;
    /* 0x04 */ f32 worldmat[16];
    /* 0x44 */ f32 attn_pos[4];
    /* 0x54 */ f32 pos[4];
    /* 0x64 */ struct mbnode* node;
    /* 0x68 */ u32 objflags;
    /* 0x6C */ u8 atree[0x48];
    /* 0xB4 */ f32 coll_offset[3];
    /* 0xC0 */ s16 ctriidx;
    /* 0xC2 */ s16 nctris;
    /* 0xC4 */ s16 active;
    /* 0xC6 */ s16 activetime;
    /* 0xC8 */ s8 action;
    /* 0xC9 */ s8 paction;
    /* 0xCA */ s8 daction;
    /* 0xCB */ s8 opener;
    /* 0xCC */ s8 minplayers;
    /* 0xCD */ s8 minoff;
    /* 0xCE */ u8 playermask;
    /* 0xCF */ s8 armor;
    /* 0xD0 */ s16 health;
    /* 0xD2 */ s16 gridnext;
    /* 0xD4 */ f32 visrad;
    /* 0xD8 */ f32 fxhittime;
    /* 0xDC */ s16 data_type;
    /* 0xDE */ u8 data_mid[0xE];
    /* 0xEC */ s16 data_value;
    /* 0xEE */ u8 data_tail[2];
};
extern struct fxworldinfo gWorldInfo;
extern struct fxitem* sItems;

void ChangeEffect(s32 idx, s32 type, u32 newflags);
static void UpdateFXStreak(Effect* e, f32* pos);
static s32 SfxSkipItem_80096FF4(struct fxitem* item, u32 a, u32 b);

/* Advance every live effect.  The Xbox names for the principal locals are
 * `mat`, `omat`, `pos`, `opos`, `dir`, `hitpos`, `hit`, `moved`, and
 * `collision`.  The GC target keeps those same values in a 0x2f0-byte frame. */
void ProcessEffects(void)
{
    s32 i;
    u8 framePad[64];
    f32 mat[16];
    f32 targetmat[16];
    f32 oldpos[3];
    f32 pos[3];
    f32 dir[3];
    f32 hitpos[3];
    f32 normal[3];
    f32 radius;
    f32 remaining;
    f32 fade;
    f32 collisionDamage;

    (void)framePad;
    if (gGameBusy != 0 || gGameplayPauseTimer != 0) {
        return;
    }

    for (i = 0; i < NumEffects; i++) {
        Effect* e = &Effects[i];
        u32 flags;
        s32 moved;
        s32 hit;
        s32 mode;
        s32 j;
        s32 collision;
        s32 oldHitCount;
        s32 passThrough;
        s32 owner;
        struct fxplayer* ownerPlayer;
        f32 ageRadius;
        f32 damageScale;

        if (e->endtime <= 0.0 || e->node == NULL) {
            continue;
        }

        moved = 0;
        hit = 0;
        mode = 0;
        collision = 0;
        GetWorldMat(e->node, mat, NULL);
        oldpos[0] = mat[12];
        oldpos[1] = mat[13];
        oldpos[2] = mat[14];
        flags = e->flags;

        if ((flags & 0x40000000) && e->targetnode != NULL) {
            GetWorldMat(e->targetnode, targetmat, NULL);
            dir[0] = targetmat[12] - mat[12];
            dir[1] = targetmat[13] - mat[13];
            dir[2] = targetmat[14] - mat[14];
            NormalVector(dir);
            e->vel[0] = dir[0] * e->weight;
            e->vel[1] = dir[1] * e->weight;
            e->vel[2] = dir[2] * e->weight;
            pos[0] = oldpos[0] + e->vel[0] * gClockFrameStep;
            pos[1] = oldpos[1] + e->vel[1] * gClockFrameStep;
            pos[2] = oldpos[2] + e->vel[2] * gClockFrameStep;
            moved = 1;
        } else if ((flags & 0x4000) && (flags & 0x8000)) {
            pos[0] = oldpos[0];
            pos[1] = oldpos[1];
            pos[2] = oldpos[2];
        } else {
            if (e->vel[0] != 0.0f || e->vel[1] != 0.0f || e->vel[2] != 0.0f) {
                pos[0] = oldpos[0] + e->vel[0] * gClockFrameStep;
                pos[1] = oldpos[1] + e->vel[1] * gClockFrameStep;
                pos[2] = oldpos[2] + e->vel[2] * gClockFrameStep;
                moved = 1;
            } else {
                pos[0] = oldpos[0];
                pos[1] = oldpos[1];
                pos[2] = oldpos[2];
            }
            e->vel[0] -= e->dragx * gClockFrameStep;
            e->vel[1] -= e->weight * gClockFrameStep;
            e->vel[2] -= e->dragz * gClockFrameStep;
        }

        if (moved && pos[1] + 2.0 * e->colrad < lbl_80344880 - 25.0) {
            e->type = FX_NONE;
            DeleteEffect(i, 1);
            continue;
        }

        if (!(flags & 0x20000)) {
            if (e->pyrvel[0] != 0.0f) {
                PitchMat3(mat, -e->pyrvel[0] * gClockFrameStep);
                moved = 1;
            }
            if (e->pyrvel[1] != 0.0f) {
                YawMat3((struct mbnode*)mat, e->pyrvel[1] * gClockFrameStep);
                moved = 1;
            }
            if (e->pyrvel[2] != 0.0f) {
                RollMat3(mat, e->pyrvel[2] * gClockFrameStep);
                moved = 1;
            }
        } else {
            CreateDirMatrix(mat, e->vel, gCameras[0].mat[2]);
            moved = 1;
        }

        remaining = e->endtime - gClockTime;
        {
            f32 lightScale;

            if (flags & 0x20) {
                if (flags & 0x10) {
                    mode = 2;
                    ageRadius = remaining;
                    if (ageRadius > 1.0) {
                        ageRadius = 1.0;
                    }
                } else {
                    mode = 1;
                    ageRadius = (f32)(remaining + lbl_803481C0);
                }
            } else if (e->webtime > 0.0) {
                mode = 4;
                ageRadius = 0.0f;
            } else {
                mode = 0;
                ageRadius = lbl_803481C8;
            }

            passThrough = e->damagetype & DMG_SUPER;
            if (e->fxhit > 0 && EffectInfo[e->fxhit].atree != NULL &&
                !(flags & 0x20) && !passThrough) {
                struct fxatreeheader* hdr =
                    (struct fxatreeheader*)EffectInfo[e->fxhit].atree;
                if (hdr->seq->numframes > 0.0) {
                    ageRadius = (f32)(lbl_803481D0 *
                                      hdr->seq->numframes);
                }
            }

            if (flags & 0x800000) {
                fade = 0.0f;
            } else if (passThrough) {
                fade = (f32)(lbl_80348160 + ageRadius);
            } else if ((flags & 0x20) && ageRadius < 1.0) {
                fade = 1.0f;
            } else if (ageRadius < lbl_803481D8) {
                fade = lbl_803481E0;
            } else {
                fade = ageRadius;
            }

            if (mode == 2 || mode == 4) {
                lightScale = 1.0f;
                radius = e->damageradius;
                damageScale = 1.0f;
            } else if (mode == 0) {
                lightScale = 1.0f;
                radius = e->colrad;
                damageScale = 1.0f;
            } else {
                f32 damageTime = e->maxtime - e->damagedelay;
                f32 phase;

                if (remaining > damageTime) {
                    phase = 0.0f;
                } else if (damageTime <= lbl_803481D0) {
                    phase = 1.0f;
                } else {
                    phase = remaining / damageTime;
                }
                if (phase > 0.33) {
                    damageScale =
                        (f32)(lbl_80348120 * (phase - 0.33));
                    radius =
                        (f32)(e->damageradius *
                              (0.33 + (1.0 - phase)));
                } else {
                    radius = 0.0f;
                    damageScale = 0.0f;
                }

                if (damageTime <= lbl_803481D0) {
                    lightScale = 1.0f;
                } else if (remaining < damageTime) {
                    f32 phaseIn = (f32)(1.0 - remaining / damageTime);
                    if (phaseIn < lbl_803480B0) {
                        lightScale = lbl_8034813C * phaseIn;
                    } else {
                        lightScale =
                            (f32)(lbl_80348150 * (1.0 - phaseIn));
                    }
                } else {
                    lightScale = 0.0f;
                }
            }
            collisionDamage = e->damage * damageScale;

            if (e->lightrad > 0.0f) {
                f32 lightpos[3];
                lightpos[0] = pos[0];
                lightpos[1] = pos[1];
                lightpos[2] = pos[2];
                lightpos[1] += 1.0;
                fn_800C0ADC(lightpos, e->lightcolor,
                            lightScale * (e->lightrad * lbl_80343DF4),
                            lbl_80343DF8);
            }
        }
        if (e->dmgdebug != NULL && (flags & 0x20)) {
            f32 scale = (f32)(lbl_80348098 * radius);
            e->dmgdebug->scale[0] = scale;
            e->dmgdebug->scale[1] =
                e->mindp <= lbl_803480A8
                    ? scale
                    : (f32)(lbl_803480B0 * scale);
            e->dmgdebug->scale[2] = scale;
        }
        if (e->fxfade > 0.0 && e->fxhit <= 0 && remaining < e->fxfade) {
            MBTreeSetAlpha(
                e->node,
                (s32)(255.0 * (1.0 - remaining / e->fxfade)), 1);
        }
        if (flags & 0x08410000) {
            MBTreeClearFlags(e->node, 8, 0);
        }
        if (flags & 0x08000000) {
            f32 elapsed = e->maxtime - remaining;
            if (elapsed < 1.0 / e->maxtime) {
                f32 scale = 0.99 * elapsed * e->maxtime + 0.01;
                MBTreeSetFlags(e->node, 8, 0);
                e->node->scale[0] = scale;
                e->node->scale[1] = scale;
                e->node->scale[2] = scale;
            }
        } else if (flags & 0x10000) {
            f32 elapsed = e->maxtime - remaining;
            if (elapsed < 0.1) {
                f32 scale = 5.0 * elapsed + 0.5;
                MBTreeSetFlags(e->node, 8, 0);
                e->node->scale[0] = scale;
                e->node->scale[1] = scale;
                e->node->scale[2] = scale;
            }
        }
        if ((flags & 0x400000) && e->fxhit == 0 && remaining < lbl_803481D8) {
            f32 scale = 5.0 * remaining + 0.001;
            MBTreeSetFlags(e->node, 8, 0);
            e->node->scale[0] = scale;
            e->node->scale[1] = scale;
            e->node->scale[2] = scale;
        }
        if (e->streak != NULL) {
            UpdateFXStreak(e, pos);
        }

        dir[0] = mat[8];
        dir[1] = mat[9];
        dir[2] = mat[10];
        NormalVector2D(dir);
        owner = e->owner;
        if (owner < 0 || owner > 4) {
            owner = 0;
            ownerPlayer = NULL;
        } else {
            ownerPlayer =
                (struct fxplayer*)(gPlayers + (owner - 1) * 13148);
        }
        oldHitCount = e->hitcount;

        /* Direct player hits.  Expanding effects scan every active player;
         * point effects use the separate swept-missile state machine. */
        if ((flags & 1) && hit == 0) {
            if (mode != 0) {
                if (radius > 0.0 && !(flags & 0x200)) {
                    s32 damagetype = e->damagetype;

                    if (collisionDamage < lbl_80348210) {
                        damagetype = (damagetype & ~0x170) | DMG_NOHITFX;
                    }
                    for (j = 0; j < 4; j++) {
                        struct fxplayer* player =
                            (struct fxplayer*)(gPlayers + j * 13148);
                        union {
                            f32 value;
                            u32 bits;
                        } absdy;
                        f32 delta[3];
                        f32 dist;
                        f32 mindp;

                        if (player->state != 1 ||
                            sMusicFadeBase < player->fxhittime ||
                            player->index == e->owner - 1) {
                            continue;
                        }
                        delta[0] = player->effectpos[0] - pos[0];
                        delta[1] = player->effectpos[1] - pos[1];
                        delta[2] = player->effectpos[2] - pos[2];
                        absdy.value = delta[1];
                        absdy.bits &= 0x7fffffff;
                        if (absdy.value > player->halfheight + radius) {
                            continue;
                        }
                        dist = NormalVector2D(delta);
                        if (dist > radius + player->radius) {
                            continue;
                        }
                        if (dist > 10.0 &&
                            fn_8005FB48(0.1f, pos, player->effectpos,
                                       player->effectpos, 1) >= 0) {
                            continue;
                        }
                        if (e->mindp > -1.0) {
                            mindp = e->mindp;
                            if (dist < 0.3 * (radius + player->radius)) {
                                mindp *= 0.85;
                            }
                            if (delta[0] * dir[0] + delta[2] * dir[2] <
                                mindp) {
                                continue;
                            }
                        }
                        delta[0] *= 0.25;
                        delta[1] = 0.0f;
                        delta[2] *= 0.25;
                        damage_player(player->index, collisionDamage, 1,
                                      damagetype, delta);
                        if (e->owner >= 4096) {
                            CritterSetFxHitTime(collisionDamage, player->index,
                                               e->owner & 0xfff);
                        }
                        if (e->damagetype & 0x800) {
                            player->fxhittime = sMusicFadeBase + 0.5;
                        } else if (collisionDamage > lbl_8034813C) {
                            player->fxhittime = sMusicFadeBase + ageRadius;
                        }
                    }
                }
            } else {
                struct fxplayer* player =
                    (struct fxplayer*)MissileCollidePlayer(
                        (flags & 0x200) ? 0.5f : radius, oldpos, pos, hitpos);
                if (player != NULL && player->index != e->owner - 1) {
                    if ((e->flags & 0x200) == 0) {
                        if (player->shield_flags & 0x01020000) {
                            fn_8009EF7C(0, hitpos);
                            e->flags |= 8;
                            moved = 1;
                            e->flags &= ~0xc00;
                            e->vel[0] *= -1.0;
                            e->vel[1] *= -1.0;
                            e->vel[2] *= -1.0;
                            pos[0] += e->vel[0] * gClockFrameStep;
                            pos[1] += e->vel[1] * gClockFrameStep;
                            pos[2] += e->vel[2] * gClockFrameStep;
                            if (e->flags & 0x20000) {
                                CreateDirMatrix(mat, e->vel,
                                                gCameras[0].mat[2]);
                                moved = 1;
                            }
                            if (e->endtime > gClockTime + 10.0) {
                                e->endtime = gClockTime + 10.0;
                            } else {
                                e->endtime -= 1.0;
                            }
                            if (e->damage > lbl_80348240) {
                                e->damage = lbl_80348240;
                            }
                        } else {
                            s32 playerHit;

                            dir[0] = e->vel[0];
                            dir[1] = e->vel[1];
                            dir[2] = e->vel[2];
                            NormalVector(dir);
                            if (sMusicFadeBase >= player->fxhittime) {
                                if (e->flags & 0x2000) {
                                    playerHit = damage_player(
                                        player->index, 0.0f, 0,
                                        e->damagetype | DMG_STUN, dir);
                                } else {
                                    playerHit = damage_player(
                                        player->index, collisionDamage, 1,
                                        e->damagetype, dir);
                                }
                                if (e->owner >= 4096) {
                                    CritterSetFxHitTime(
                                        collisionDamage, player->index,
                                        e->owner & 0xfff);
                                }
                                if (collisionDamage > lbl_8034813C) {
                                    if (ownerPlayer != NULL &&
                                        passThrough != 0 &&
                                        ageRadius < 1.0) {
                                        ageRadius = 1.0f;
                                    }
                                    player->fxhittime =
                                        sMusicFadeBase + ageRadius;
                                }
                            } else {
                                playerHit = 0;
                            }
                            if (passThrough != 0) {
                                if ((e->damagetype & DMG_REFLECT) &&
                                    e->fxhit >= 0 &&
                                    collisionDamage > lbl_8034813C) {
                                    s32 impact = StartFXSubGuts(
                                        e->fxhit, player->col_pos, 0,
                                        0x880, 0.0f);

                                    if (e->flags & 0x00300000) {
                                        Effect* impactEffect =
                                            &Effects[impact];
                                        f32* impactMat =
                                            (f32*)impactEffect->node;

                                        if (FloorCollide(
                                                1.0 + impactEffect->colrad,
                                                5.0 + impactEffect->colrad,
                                                lbl_80348244, impactMat + 12,
                                                NULL, 1, 0)) {
                                            CopyMat4(gFloorCollisionResult,
                                                     impactMat);
                                            impactMat[13] += 0.1;
                                        } else {
                                            CopyMat3(gIdentityMatrix,
                                                     impactMat);
                                        }
                                    }
                                    e->damagetype &= ~DMG_SUPER;
                                }
                            } else {
                                pos[0] = hitpos[0];
                                pos[1] = hitpos[1];
                                pos[2] = hitpos[2];
                                moved = 1;
                            }
                            hit = playerHit == 0 ? 2 : 3;
                        }
                    } else if (e->maxtime - remaining > 0.1) {
                        hit = -1;
                    }
                } else if (player != NULL && e->maxtime - remaining > 0.2) {
                    hit = -1;
                }
                if (passThrough != 0) {
                    hit = 0;
                }
            }
        }

        if ((flags & 8) && hit == 0) {
            s32 enemyIndex;
            if (mode != 0 && radius > 0.0 && !(flags & 0x400)) {
                StartItemGrid(radius, pos);
                while ((enemyIndex = NextGridItem()) >= 0) {
                    struct fxenemy* enemy =
                        (struct fxenemy*)(gEnemies + enemyIndex * 916);
                    f32 enemyDelta[3];
                    f32 enemyDist;
                    f32 enemyMindp;
                    s32 enemyState;
                    s32 damage;

                    if (enemy->state != 1 && enemy->state != 6) {
                        continue;
                    }
                    if (enemy->state == 6 && (flags & 0x100)) {
                        continue;
                    }
                    if (!(flags & 0x01000000) &&
                        sMusicFadeBase < enemy->fxhittime[owner]) {
                        continue;
                    }
                    enemyDelta[0] = enemy->pos[0] - pos[0];
                    enemyDelta[1] = enemy->pos[1] - pos[1];
                    enemyDelta[2] = enemy->pos[2] - pos[2];
                    enemyDist = NormalVector2D(enemyDelta);
                    if (enemyDist > radius + enemy->radius) {
                        continue;
                    }
                    if (e->mindp > -1.0) {
                        enemyMindp = e->mindp;
                        if (enemyDist < 0.2 * (radius + enemy->radius)) {
                            enemyMindp *= 0.85;
                        }
                        if (enemyDelta[0] * dir[0] +
                                enemyDelta[2] * dir[2] <
                            enemyMindp) {
                            continue;
                        }
                    }
                    e->hitcount++;
                    enemyState = enemy->health <= 0.0 ? 0 : enemy->state;
                    damage = damage_enemy(
                        enemy, owner - 1, e->damagetype, 0, enemyDelta,
                        collisionDamage, 2);
                    if (damage >= 0) {
                        if (collisionDamage > lbl_8034813C &&
                            !(flags & 0x00800000)) {
                            enemy->fxhittime[owner] =
                                sMusicFadeBase + fade;
                        }
                        enemy->fxhitidx = i;
                        if (owner > 0) {
                            PlayerDamagedEnemy(
                                ownerPlayer, enemy,
                                enemyState, damage, 0);
                        }
                        if (e->fxhit >= 0) {
                            StartFXSub(e->fxhit, enemy->effectpos, 0, 0x880,
                                       0.0f);
                        }
                    }
                }
            } else if (mode == 0) {
                s32 start = 0;
                do {
                    s32 damage;
                    s32 enemyState;
                    struct fxenemy* enemy;

                    enemyIndex = MissileCollideEnemy(
                        radius, oldpos, pos, hitpos, owner,
                        (e->flags & 0x01000000) ? 0 : 1, start);
                    if (enemyIndex < 0) {
                        break;
                    }
                    enemy = (struct fxenemy*)(gEnemies + enemyIndex * 916);
                    if (flags & 0x400) {
                        if (e->maxtime - remaining > 0.0667) {
                            hit = -1;
                        }
                    } else {
                        dir[0] = e->vel[0];
                        dir[1] = e->vel[1];
                        dir[2] = e->vel[2];
                        e->hitcount++;
                        NormalVector(dir);
                        enemyState = enemy->health <= 0.0 ? 0 : enemy->state;
                        damage = damage_enemy(
                            enemy, owner - 1, e->damagetype, hitpos, dir,
                            collisionDamage, 2);
                        if (damage >= 0) {
                            if (collisionDamage > lbl_8034813C &&
                                !(flags & 0x00800000)) {
                                enemy->fxhittime[owner] =
                                    sMusicFadeBase + fade;
                                e->flags &= ~0x01000000;
                            }
                            if (owner > 0) {
                                PlayerDamagedEnemy(
                                    ownerPlayer, enemy,
                                    enemyState, damage, 0);
                            }
                            hit = damage == 0 ? 2 : 3;
                            if (passThrough == 0) {
                                pos[0] = hitpos[0];
                                pos[1] = hitpos[1];
                                pos[2] = hitpos[2];
                                moved = 1;
                            }
                        } else {
                            hit = 2;
                        }
                    }
                    start = enemyIndex + 1;
                } while (passThrough != 0);
                if (passThrough != 0) {
                    hit = 0;
                }
            }
        }

        if ((flags & 8) && hit == 0 && lbl_8034466C != 0) {
            struct fxcritter* critter;
            if (mode != 0 && radius > 0.0 && !(flags & 0x400)) {
                CritterCollideStart(radius, pos, 0);
                while ((critter = CritterExpCollide(
                            pos, normal, dir, radius, e->mindp, fade,
                            e->id)) != NULL) {
                    s32 damage;
                    fn_80037ED0(critter, e->id, fade);
                    if (critter->header->desc->type == 4 &&
                        (e->flags & 0x800)) {
                        continue;
                    }
                    e->hitcount++;
                    damage = CritterDamage(
                        critter, owner - 1, e->damagetype, 0, dir,
                        collisionDamage, 2);
                    if (damage >= 0 && e->fxhit >= 0) {
                        StartFXSub(e->fxhit, critter->effectpos, 0, 0x880,
                                   0.0f);
                    }
                }
            } else if (mode == 0) {
                CritterCollideStart(radius, pos, 0);
                critter = CritterMoveNodeCol(radius, fade, oldpos, pos,
                                             hitpos, e->id, 0);
                if (critter != NULL) {
                    fn_80037ED0(critter, e->id, fade);
                    if (critter->header->desc->type == 4) {
                        passThrough = 0;
                    }
                    if (!(e->flags & 0x400) &&
                        !(critter->header->desc->type == 4 &&
                          (e->flags & 0x800))) {
                        dir[0] = e->vel[0];
                        dir[1] = e->vel[1];
                        dir[2] = e->vel[2];
                        e->hitcount++;
                        NormalVector(dir);
                        hit = CritterDamage(
                            critter, owner - 1, e->damagetype, hitpos,
                            dir, collisionDamage, 2);
                        if (hit < 0) {
                            hit = 2;
                        } else if (hit == 0) {
                            hit = 2;
                        } else {
                            hit = 3;
                        }
                        if (passThrough == 0) {
                            pos[0] = hitpos[0];
                            pos[1] = hitpos[1];
                            pos[2] = hitpos[2];
                            moved = 1;
                        }

                        if ((u32)(lbl_8034489C - 2) <= 1) {
                            switch (gBossType) {
                            case 34:
                                critter->boss_texture = MBOX_FindTexture_Sub(
                                    lbl_80114790 + 20, 0, sItemFile1Handle,
                                    sItemFile1Handle, 1);
                                critter->boss_timer_a = 1200;
                                lbl_8034489C = 4;
                                break;
                            case 36:
                            {
                                EffectHeader* h = &EffectInfo[FX_LEGEND1];
                                f32 bosspos[3];
                                s32 effect = -1;

                                critter->boss_timer_b = 1800;
                                bosspos[0] = 0.0f;
                                bosspos[1] = lbl_80348144;
                                bosspos[2] = 0.0f;
                                if (h->atree != NULL) {
                                    effect = StartFXTree(h->atree, bosspos, 0,
                                                         0x400880,
                                                         lbl_80348250);
                                    if (effect >= 0) {
                                        Effect* spawned =
                                            EFFECTS_POOL_AT(effect);
                                        MBTreeSetZsortAdd(spawned->node,
                                                          h->zmod, 1);
                                        MBTreeSetAlpha(spawned->node,
                                                       h->alpha, 1);
                                        spawned->type = FX_LEGEND1;
                                    }
                                }
                                if (effect >= 0) {
                                    Effect* spawned = EFFECTS_POOL_AT(effect);
                                    MBNodeSetParent(spawned->node,
                                                    critter->node);
                                    if (ATREE_ROOT(spawned) != NULL) {
                                        MBTreeSetFlags(
                                            ATREE_ROOT(spawned)->node, 0x10,
                                            0);
                                    }
                                }
                                lbl_80344890 = effect;
                                lbl_8034489C = 4;
                                break;
                            }
                            case 38:
                                MBSetObject(
                                    critter->object->node,
                                    MBOX_FindObject(lbl_80114790 + 32));
                                critter->boss_timer_b = 18000;
                                break;
                            }
                            fn_8009C9DC(3, pos);
                            if (hit < 2) {
                                hit = 2;
                            }
                        }
                        if (lbl_8034489C == 5 && gBossType == 35) {
                            lbl_8034489C = 6;
                        }
                    } else {
                        if (e->maxtime - remaining > 0.0667) {
                            hit = -1;
                        }
                    }
                    if (passThrough != 0) {
                        hit = 0;
                    }
                }
            }
        }

        if ((flags & 2) && hit == 0) {
            struct fxitem* item;
            if (mode != 0) {
                f32 itemRadius = radius;
                s32 itemIndex;

                if (e->damagetype & DMG_EXPLODE) {
                    itemRadius -= 1.5;
                    if (itemRadius < 0.0f) {
                        itemRadius = 0.0f;
                    }
                }
                if (itemRadius > 0.0) {
                    StartEnemyGrid(pos, itemRadius);
                    while ((itemIndex = NextGridEnemy()) >= 0) {
                        f32 itemDist;
                        f32 itemHit;
                        f32 itemMindp;
                        s32 skip;
                        s32 noDmg;

                        item = &sItems[itemIndex];
                        if (item->def->type == 2 && item->data_type >= 0 &&
                            gWorldInfo.itemdefs[item->data_type].type == 4) {
                            skip = 0;
                        } else {
                            skip = SfxSkipItem_80096FF4(
                                item, e->flags, (u32)e->damagetype);
                        }
                        if (skip == 1) {
                            continue;
                        }
                        if (!(e->flags & DMG_NOHITFX) &&
                            sMusicFadeBase < item->fxhittime) {
                            continue;
                        }

                        normal[0] = item->pos[0] - pos[0];
                        normal[1] = item->pos[1] - pos[1];
                        normal[2] = item->pos[2] - pos[2];
                        itemDist = NormalVector2D(normal);
                        if (itemDist > itemRadius + item->def->radius) {
                            continue;
                        }
                        if (e->mindp > -1.0) {
                            itemMindp = e->mindp;
                            if (itemDist <
                                0.2 * (itemRadius + item->def->radius)) {
                                itemMindp *= 0.85;
                            }
                            if (normal[0] * dir[0] +
                                    normal[2] * dir[2] <
                                itemMindp) {
                                continue;
                            }
                        }
                        if (fn_8005F0F4(item, oldpos, pos, hitpos,
                                       itemRadius, itemRadius) < 0.0) {
                            continue;
                        }
                        if ((e->damagetype & DMG_HEAL) && owner > 0) {
                            fn_8005BA1C(item, ownerPlayer, damageScale);
                        }
                        if (skip != 0) {
                            continue;
                        }
                        e->hitcount++;
                        itemHit = fn_8005C1DC(item, collisionDamage,
                                             (u32)e->damagetype,
                                             owner - 1);
                        if (itemHit == 0.0) {
                            noDmg = 1;
                        } else {
                            noDmg = 0;
                        }
                        if (itemHit >= 0.0f && owner > 0) {
                            PlayerDamagedItem(ownerPlayer, item, noDmg);
                        }
                        if (collisionDamage > lbl_8034813C &&
                            !(e->flags & 0x00800000)) {
                            item->fxhittime =
                                sMusicFadeBase + ageRadius;
                        }
                    }
                }
            } else {
                item = (struct fxitem*)fn_8005ED44(
                    radius, oldpos, pos, hitpos, 1, owner - 1);
                if (item != NULL) {
                    s32 skip = SfxSkipItem_80096FF4(
                        item, e->flags, (u32)e->damagetype);
                    if (e->damagetype & DMG_POISONGAS) {
                        skip = 1;
                    }
                    if (skip == 0) {
                        f32 itemHit;
                        s32 noDmg;

                        if (passThrough == 0) {
                            pos[0] = hitpos[0];
                            pos[1] = hitpos[1];
                            pos[2] = hitpos[2];
                            moved = 1;
                        }
                        e->hitcount++;
                        itemHit = fn_8005C1DC(item, collisionDamage,
                                             (u32)e->damagetype,
                                             owner - 1);
                        if (itemHit == 0.0) {
                            noDmg = 1;
                        } else {
                            noDmg = 0;
                        }
                        if (itemHit >= 0.0f && owner > 0) {
                            PlayerDamagedItem(
                                gPlayers + (owner - 1) * 13148, item,
                                noDmg);
                        }
                        if (collisionDamage > lbl_8034813C &&
                            !(e->flags & 0x00800000)) {
                            item->fxhittime =
                                sMusicFadeBase + ageRadius;
                        }
                        if (itemHit < -1.0) {
                            hit = -1;
                        } else if (itemHit < 0.0) {
                            hit = 1;
                        } else {
                            hit = noDmg ? 3 : 2;
                        }
                    } else {
                        hit = 1;
                    }
                    if (passThrough != 0) {
                        if (item->def->type == 10 &&
                            item->data_type == 41 && item->health > 0) {
                            e->fxhit = -1;
                        } else {
                            hit = 0;
                        }
                    }
                }
            }
        }

        if ((flags & 4) && hit == 0) {
            void* wall = WeaponWallCollide(0.5 * radius, oldpos, pos,
                                           hitpos);
            if (wall != NULL) {
                u32 wallFlags = WorldObjGetAllFlags(wall);

                if (*(s32*)(lbl_8023CB28 + 68) != 0) {
                    collision = 1;
                }
                pos[0] = hitpos[0];
                pos[1] = hitpos[1];
                pos[2] = hitpos[2];
                moved = 1;
                if (((struct fxworldobj*)wall)->flags & 4) {
                    pos[1] += 2.0;
                }
                if ((wallFlags & 0xF0000) == 0x40000 ||
                    (wallFlags & 0xF0000) == 0x50000) {
                    WorldObjectExplode(wall, hitpos);
                }
                if (e->damagetype & DMG_REFLECT) {
                    if (e->wall_sound != 0 && owner > 0) {
                        fn_8009EF7C(0, hitpos);
                    }
                    ReflectVector(e->vel, (f32*)(lbl_8023CA98 + 16),
                                  e->vel);
                    if (e->vel[1] > 0.0) {
                        e->vel[1] *= 0.4;
                    }
                    if (e->flags & 0x20000) {
                        CreateDirMatrix(mat, e->vel, gCameras[0].mat[2]);
                        moved = 1;
                    }
                    if (e->endtime > gClockTime + 10.0) {
                        e->endtime = gClockTime + 10.0;
                    } else {
                        e->endtime -= 1.0;
                    }
                } else {
                    hit = 1;
                }
            }
        }

        if (hit == 0 && (flags & 0x40) && e->vel[1] < 0.1) {
            f32 floor = 0.1 + FloorPos(lbl_80344880, radius, pos, 0);
            if (pos[1] - floor < 0.1) {
                e->vel[0] = 0.0f;
                moved = 1;
                e->vel[1] = 0.0f;
                e->vel[2] = 0.0f;
                pos[1] = floor;
                e->weight = 0.0f;
            }
        }

        if (e->damageradius > 0.0 && !(e->flags & 0x4020) &&
            gClockTime >= e->endtime - 0.0667) {
            if (e->flags & 0x10000000) {
                s32 c0 = RandInt(6);
                s32 c1 = RandInt(6);
                s32 count = e->hitcount - (e->hitcount >> 2) +
                            RandInt(e->hitcount >> 1);
                struct mbnode* fireNode;
                (void)Random(lbl_80348134);
                fireNode = MBNewNode(lbl_80344BD4, mat, 1);
                MBPsysFirework(0, fireNode, count, lbl_80122088[c0],
                               lbl_80122088[c1], e->damageradius, 0.0f,
                               0.01f, 0.2f, 0.1f, 0xff000000);
                hit = -2;
            } else {
                hit = 1;
            }
        }
        if (passThrough != 0 && (e->damagetype & DMG_TURBO) &&
            e->hitcount > oldHitCount) {
            e->damage *= 0.5;
            if (e->damage < 1.0) {
                e->damagetype &= ~DMG_TURBO;
            }
        }
        if (hit == -1 && e->fxhit > 0 && e->damageradius > 0.0f) {
            hit = 1;
        }
        if (e->minendtime > 0.0 && gClockTime < e->minendtime) {
            hit = 0;
        }

        if (hit < 0) {
            e->endtime = gClockTime;
            ((struct fxanim*)&e->atree[4])->oneshot = 0;
        } else if (hit != 0) {
            MagicView* magic = (MagicView*)lbl_80122088;
            s32 morph;
            if (collision != 0) {
                fn_8009D5E0(hitpos);
            } else if (hit == 1) {
                if (e->wall_sound != 0) {
                    fn_8009DB24(e->wall_sound, hitpos);
                }
            } else if (e->hit_audio != 0) {
                fn_8009DB24(e->hit_audio, hitpos);
            }
            morph = e->fxhit;
            if (morph <= 0 && hit == 1) {
                morph = magic->hitmorph[e->damagetype & 0xf];
                e->flags &= ~0xf;
            }
            if (morph > 0 && morph < MAXEFFECTTYPES) {
                u32 newflags = 0;
                if (e->damageradius > 0.0) {
                    newflags |= 0x880;
                }
                ChangeEffect(i, morph, newflags);
                ZeroEffect(i);
                GetWorldMat(e->node, mat, NULL);
                if (e->hitscale != 1.0) {
                    MBTreeSetFlags(e->node, 8, 0);
                    e->node->scale[0] = e->hitscale;
                    e->node->scale[1] = e->hitscale;
                    e->node->scale[2] = e->hitscale;
                }
                if ((e->damagetype & DMG_STICKY) && e->webtime == 0.0) {
                    e->webtime = lbl_80348100;
                }
                if (e->webtime > 0.0) {
                    e->endtime = gClockTime + e->webtime;
                    e->maxtime = e->endtime - gClockTime;
                    ((struct fxanim*)&e->atree[4])->oneshot = 0;
                    e->flags &= 0xfe7dfbf9;
                    e->flags |= 0x00100000;
                } else {
                    struct fxanim* ai = (struct fxanim*)&e->atree[4];

                    e->endtime =
                        0.00111111 *
                            ((f32)ai->def->nframes * (f32)ai->def->rate) +
                        gClockTime;
                    e->maxtime = e->endtime - gClockTime;
                    ai->oneshot = 0;
                    if (e->damageradius > 0.0f) {
                        e->flags &= 0xf67cfbfb;
                        if (!(e->damagetype & DMG_MAGIC)) {
                            e->flags &= ~2;
                        }
                        e->flags |= 0x28;
                        if (owner != 0) {
                            e->flags |= 1;
                        }
                        if (e->dmgdebug != NULL) {
                            MBRemoveNode(e->dmgdebug, 1);
                            DmgFxAdd(i);
                        }
                    } else {
                        e->flags &= ~0xf;
                    }
                }

                if ((e->flags & 0x300000) != 0) {
                    mat[12] = pos[0];
                    mat[13] = pos[1];
                    mat[14] = pos[2];
                    if (FloorCollide(1.0 + e->colrad,
                                     5.0 + e->colrad, lbl_80348244,
                                     mat + 12, NULL, 1, 0)) {
                        CopyMat4(gFloorCollisionResult, mat);
                        mat[13] += 0.1;
                    } else {
                        CopyMat3(gIdentityMatrix, mat);
                    }
                    pos[0] = mat[12];
                    pos[1] = mat[13];
                    pos[2] = mat[14];
                    moved = 1;
                }
                if (e->flags & 0x04000000) {
                    BossGenerateEnemy(mat);
                    e->flags &= ~0x04000000;
                    e->flags |= 0x400;
                }
            } else {
                e->endtime = gClockTime;
                ((struct fxanim*)&e->atree[4])->oneshot = 0;
            }
        }

        if (gClockTime >= e->endtime - 0.03332 &&
            !(e->flags & 0x80000)) {
            if ((e->flags & 0x4000) && e->fxmorph > 0 &&
                e->fxmorph < MAXEFFECTTYPES) {
                ChangeEffect(i, e->fxmorph, 0);
                if (e->morphtime > 0.0) {
                    e->endtime = gClockTime + e->morphtime;
                } else {
                    struct fxanim* ai = (struct fxanim*)&e->atree[4];
                    e->endtime =
                        0.00111111 *
                            ((f32)ai->def->nframes * (f32)ai->def->rate) +
                        gClockTime;
                }
                e->maxtime = e->endtime - gClockTime;
                if (e->fxmorph2 > 0) {
                    e->fxmorph = e->fxmorph2;
                    e->fxmorph2 = 0;
                    e->morphtime = 0.0f;
                } else {
                    e->flags &= ~0x4000;
                    e->damageradius = 0.0f;
                }
                ((struct fxanim*)&e->atree[4])->oneshot = 1;
            } else {
                if (e->flags & 0x02000000) {
                    PlaceItem(3, 0, "BOSSGEN", mat);
                }
                if (e->flags & 0x04000000) {
                    BossGenerateEnemy(mat);
                }
                if (e->additem != NULL) {
                    struct fxitem* item = (struct fxitem*)e->additem;
                    struct fxitem* spawned;
                    item->minoff = 0;
                    spawned = item;
                    MBTreeClearFlags(item->node, 2, 0);
                    if (item->def->type == 1) {
                        item->data_value = 10;
                    }
                    item->worldmat[12] = mat[12];
                    item->worldmat[13] = mat[13];
                    item->worldmat[14] = mat[14];
                    AddItemSub((struct item*)spawned);
                }
                DeleteEffect(i, 1);
                continue;
            }
        }

        if (ATREE_ROOT(e) != NULL) {
            AnimateATree(&e->atree[0], 0, 0);
        }
        if (moved) {
            mat[12] = pos[0];
            mat[13] = pos[1];
            mat[14] = pos[2];
            CopyMat4(mat, e->node);
            UnparentMatrix(e->node, (f32*)e->node->parent);
        }
    }
}

static s32 SfxSkipItem_80096FF4(struct fxitem* item, u32 a, u32 b)
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

    t = e->maxtime - (e->endtime - gClockTime);
    side = (f32)(0.8 * e->streakscale);
    tail = (f32)(0.1 * e->streakscale);
    back = t;
    if (t > (f32)(0.4999999995 * e->streakscale)) {
        back = (f32)(0.4999999995 * e->streakscale);
    }

    v[0] = e->vel[0];
    v[1] = e->vel[1];
    v[2] = e->vel[2];
    NormalVector(v);

    w[0] = v[1] * gCameras[0].mat[2][2] - v[2] * gCameras[0].mat[2][1];
    w[1] = v[2] * gCameras[0].mat[2][0] - v[0] * gCameras[0].mat[2][2];
    w[2] = v[0] * gCameras[0].mat[2][1] - v[1] * gCameras[0].mat[2][0];
    len = NormalVector(w);

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
    if (FloorCollide(e->colrad + 1.0, e->colrad + 5.0, -10.0f, mat + 12, NULL, 1, 0)) {
        CopyMat4(gFloorCollisionResult, mat);
        mat[13] += 0.1;
    } else {
        CopyMat3(gIdentityMatrix, mat);
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
            AtreeDelete(&e->atree[0]);
            ATREE_ROOT(e) = AtreeInit(h->atree, &e->atree[0], 0, 0);
            MBNodeSetParent(ATREE_ROOT(e)->node, e->node);
            MBTreeSetZsortAdd(e->node, h->zmod, 1);
            MBTreeSetAlpha(e->node, h->alpha, 1);
            if (oldframe != 0) {
                MBTreeSetAmbientAdd(e->node, oldframe, 1);
            }
            MBTreeSetFlags(e->node, newflags, 1);
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
        if (e->minendtime > 0.0 && gClockTime < e->minendtime) {
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
                msgPost(18, e->owner - 1, (char*)(gPlayers + (e->owner - 1) * 13148 + 84));
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
        AtreeDelete(&e->atree[0]);
    }

    if ((n = e->node) != NULL) {
        if (n->child != NULL) {
            SfxDeleteParented(n, 0, -1);
        }
        MBRemoveNode(e->node, 1);
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
        CopyMat3(gIdentityMatrix, e->node);
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

extern u8 lbl_80122118[];        /* fx def script rows, stride 40 */
extern void MBNodeOrder(struct mbnode* a, struct mbnode* b);
extern struct mbnode* lbl_80344EB0;
extern s8 lbl_80348290;
extern s8 lbl_80348298;
extern s8 lbl_803482A0;
extern s8 lbl_803482A8;
extern s8 lbl_803482B0;
extern s8 lbl_803482B8;
extern s8 lbl_803482C0;
extern s8 lbl_803482C8;
extern s8 lbl_803482D0;
extern s8 lbl_803482D8;
extern s8 lbl_803482DC;
extern s8 lbl_803482E0;
extern u32 gWadAtreeHeaders[];
extern s32 lbl_80251148[];
extern s32 lbl_802511FC[];
extern s32 sGoodWizObjHack;
extern s32 InLevel(char* name);
extern void* FindTexMod(void* buf, char* name, s32 flag);
extern s32 MBOX_FindTexture_Sub(char* name, s32 a, s32 b, s32 c, s32 flag);
extern void* sPowerupsHandle;
extern void* sWeaponsHandle;
extern s32 lbl_80344BF8;
extern s32 lbl_80344BF4;
extern s32 lbl_80344BF0;
extern void* lbl_80344BEC;
extern s32 lbl_80344BE8;
extern void* lbl_80344BE4;
extern void* lbl_80344BE0;

/* 0x80097AA4 InitEffects - reset the live effect pool, resolve every fx def
 * row against the weapon/powerup wads, and seed the per-type skin fx. */
void InitEffects(void)
{
    char* strs = lbl_80114790;
    u8* ei = (u8*)EffectInfo;
    u8* tbl;
    u8* row;
    u8** hp;
    s32 i;
    s32 o40;
    s32 got;
    s32 v32;
    s32 v36;
    s32* q32;
    s32* q36;
    u32* p1008;
    u32* p1020;
    u32* p1032;
    u32* p1044;
    u32* p1056;
    u32* p960;
    u32* p1140;
    u32* p1152;
    u32* p972;
    u32* p1068;
    u32* p984;
    u32* p996;
    s32* p1128;
    s32* p1136;
    s32* p1132;
    u8 unused[352];

    for (i = 0; i < 64; i++) {
        *(struct mbnode**)(ei + i * 240 + 2996) = NULL;
        *(struct anode**)(ei + i * 240 + 3000) = NULL;
        *(void**)(ei + i * 240 + 3188) = NULL;
    }
    NumEffects = 0;
    lbl_80344BD8 = 0;
    lbl_80343DF0 = 256;
    lbl_80344BD4 = MBNewNode(0, gIdentityMatrix, 4);
    MBTreeSetFlags(lbl_80344BD4, 4, 0);
    MBNodeOrder(lbl_80344EB0, lbl_80344BD4);
    tbl = lbl_80122118;
    for (i = 0, o40 = 0; (u32)i < 80; i++, o40 += 40) {
        row = tbl + o40;
        v36 = *(s32*)(row + 36);
        q36 = (s32*)(row + 36);
        v32 = *(s32*)(row + 32);
        q32 = (s32*)(row + 32);
        if (sWeaponsBuf == NULL || row == NULL || *(s8*)row == 0) {
            ((EffectHeader*)ei)[i].atree = NULL;
        } else {
            ((EffectHeader*)ei)[i].atree =
                AtreeMatch(sWeaponsBuf, (char*)row, 0);
        }
        ((EffectHeader*)ei)[i].zmod = v32;
        ((EffectHeader*)ei)[i].alpha = v36;
        got = (((EffectHeader*)ei)[i].atree != NULL) ? 1 : 0;
        if (got == 0) {
            v36 = *q36;
            v32 = *q32;
            if (sPowerupsBuf == NULL || row == NULL || *(s8*)row == 0) {
                ((EffectHeader*)ei)[i].atree = NULL;
            } else {
                ((EffectHeader*)ei)[i].atree =
                    AtreeMatch(sPowerupsBuf, (char*)row, 0);
                if (((EffectHeader*)ei)[i].atree == NULL) {
                    ErrorPrintf(strs + 48, row);
                }
            }
            ((EffectHeader*)ei)[i].zmod = v32;
            ((EffectHeader*)ei)[i].alpha = v36;
        }
    }
    for (; i < 218; i++) {
        s32 off = i * 12;

        *(s32*)(ei + off) = 0;
        *(s32*)(ei + off + 4) = 0;
        *(s32*)(ei + off + 8) = 0;
    }
    p1140 = (u32*)(ei + 1140);
    p1152 = (u32*)(ei + 1152);
    p1032 = (u32*)(ei + 1032);
    p1044 = (u32*)(ei + 1044);
    p1056 = (u32*)(ei + 1056);
    p1068 = (u32*)(ei + 1068);
    p1008 = (u32*)(ei + 1008);
    p1020 = (u32*)(ei + 1020);
    p960 = (u32*)(ei + 960);
    p972 = (u32*)(ei + 972);
    p984 = (u32*)(ei + 984);
    p996 = (u32*)(ei + 996);
    for (i = 0; i < 45; i++) {
        *(s32*)((u8*)lbl_80251148 + i * 4) = 0;
        hp = (u8**)((u8*)gWadAtreeHeaders + i * 4);
        if (*hp != NULL) {
        if (*(s32*)((u8*)lbl_802511FC + i * 4) != 4 && *p960 == 0) {
            if (*hp == NULL || *(s8*)(strs + 68) == 0) {
                *p960 = 0;
            } else {
                *p960 = (s32)AtreeMatch(*hp, strs + 68, 0);
            }
            *(s32*)(ei + 964) = -512;
            *(s32*)(ei + 968) = 0;
        }
        if (*p972 == 0) {
            if (*hp == NULL || lbl_80348290 == 0) {
                *p972 = 0;
            } else {
                *p972 = (s32)AtreeMatch(*hp, (char*)&lbl_80348290, 0);
            }
            *(s32*)(ei + 976) = -512;
            *(s32*)(ei + 980) = 0;
            *(s32*)((u8*)lbl_80251148 + i * 4) = 1;
        }
        if (*p984 == 0) {
            if (*hp == NULL || lbl_80348298 == 0) {
                *p984 = 0;
            } else {
                *p984 = (s32)AtreeMatch(*hp, (char*)&lbl_80348298, 0);
            }
            *(s32*)(ei + 988) = -512;
            *(s32*)(ei + 992) = 0;
            *(s32*)((u8*)lbl_80251148 + i * 4) = 1;
        }
        if (*p996 == 0) {
            if (*hp == NULL || lbl_803482A0 == 0) {
                *p996 = 0;
            } else {
                *p996 = (s32)AtreeMatch(*hp, (char*)&lbl_803482A0, 0);
            }
            *(s32*)(ei + 1000) = -512;
            *(s32*)(ei + 1004) = 0;
            *(s32*)((u8*)lbl_80251148 + i * 4) = 1;
        }
        if (i == 11 || i == 21) {
            if (*hp == NULL || lbl_803482A8 == 0) {
                *p1008 = 0;
            } else {
                *p1008 = (s32)AtreeMatch(*hp, (char*)&lbl_803482A8, 0);
                if (*p1008 == 0) {
                    ErrorPrintf(strs + 48, &lbl_803482A8);
                }
            }
            *(s32*)(ei + 1012) = -512;
            *(s32*)(ei + 1016) = 0;
            if (*hp == NULL || lbl_803482B0 == 0) {
                *p1020 = 0;
            } else {
                *p1020 = (s32)AtreeMatch(*hp, (char*)&lbl_803482B0, 0);
                if (*p1020 == 0) {
                    ErrorPrintf(strs + 48, &lbl_803482B0);
                }
            }
            *(s32*)(ei + 1024) = -512;
            *(s32*)(ei + 1028) = 0;
        }
        if (i == 27) {
            if (*hp == NULL || lbl_803482B8 == 0) {
                *p1032 = 0;
            } else {
                *p1032 = (s32)AtreeMatch(*hp, (char*)&lbl_803482B8, 0);
                if (*p1032 == 0) {
                    ErrorPrintf(strs + 48, &lbl_803482B8);
                }
            }
            *(s32*)(ei + 1036) = -512;
            *(s32*)(ei + 1040) = 0;
            if (*hp == NULL || lbl_803482C0 == 0) {
                *p1044 = 0;
            } else {
                *p1044 = (s32)AtreeMatch(*hp, (char*)&lbl_803482C0, 0);
                if (*p1044 == 0) {
                    ErrorPrintf(strs + 48, &lbl_803482C0);
                }
            }
            *(s32*)(ei + 1048) = -512;
            *(s32*)(ei + 1052) = 0;
            if (*hp == NULL || *(s8*)(strs + 80) == 0) {
                *p1056 = 0;
            } else {
                *p1056 = (s32)AtreeMatch(*hp, strs + 80, 0);
                if (*p1056 == 0) {
                    ErrorPrintf(strs + 48, strs + 80);
                }
            }
            *(s32*)(ei + 1060) = -512;
            *(s32*)(ei + 1064) = 0;
            if (*hp == NULL || *(s8*)(strs + 92) == 0) {
                *p1068 = 0;
            } else {
                *p1068 = (s32)AtreeMatch(*hp, strs + 92, 0);
                if (*p1068 == 0) {
                    ErrorPrintf(strs + 48, strs + 92);
                }
            }
            *(s32*)(ei + 1072) = -512;
            *(s32*)(ei + 1076) = 0;
        }
        if (i == 30) {
            if (*hp == NULL || *(s8*)(strs + 104) == 0) {
                *p1140 = 0;
            } else {
                *p1140 = (s32)AtreeMatch(*hp, strs + 104, 0);
                if (*p1140 == 0) {
                    ErrorPrintf(strs + 48, strs + 104);
                }
            }
            *(s32*)(ei + 1144) = -512;
            *(s32*)(ei + 1148) = 0;
            if (*hp == NULL || *(s8*)(strs + 116) == 0) {
                *p1152 = 0;
            } else {
                *p1152 = (s32)AtreeMatch(*hp, strs + 116, 0);
                if (*p1152 == 0) {
                    ErrorPrintf(strs + 48, strs + 116);
                }
            }
            *(s32*)(ei + 1156) = -512;
            *(s32*)(ei + 1160) = 0;
        }
        *(s32*)(ei + i * 4 + 2796) =
            InitCustomEffectSub(*hp, (char*)&lbl_803482C8, 0, 0, 0);
        *(s32*)(ei + i * 4 + 2616) =
            InitCustomEffectSub(*hp, (char*)&lbl_803482D0, 0, 0, 0);
        } else {
            *(s32*)(ei + i * 4 + 2796) = -1;
            *(s32*)(ei + i * 4 + 2616) = -1;
        }
    }
    if ((u32)(sMusicTrackHi - 5) <= 1) {
        *(s32*)(ei + 2796) =
            InitCustomEffectSub(sGoodWizObj, (char*)&lbl_803482C8, 0, 0, 0);
        *(s32*)(ei + 2616) =
            InitCustomEffectSub(sGoodWizObj, (char*)&lbl_803482D0, 0, 0, 0);
    }
    if (gBossType >= 0) {
        if (sItemFile1Buf == NULL || *(s8*)(strs + 128) == 0) {
            *(s32*)(ei + 1080) = 0;
        } else {
            *(s32*)(ei + 1080) = (s32)AtreeMatch(sItemFile1Buf, strs + 128, 0);
        }
        *(s32*)(ei + 1084) = 0;
        *(s32*)(ei + 1088) = 0;
        if (sItemFile1Buf == NULL || *(s8*)(strs + 140) == 0) {
            *(s32*)(ei + 1092) = 0;
        } else {
            *(s32*)(ei + 1092) = (s32)AtreeMatch(sItemFile1Buf, strs + 140, 0);
        }
        *(s32*)(ei + 1096) = 0;
        *(s32*)(ei + 1100) = 0;
        if (sItemFile1Buf == NULL || *(s8*)(strs + 152) == 0) {
            *(s32*)(ei + 1116) = 0;
        } else {
            *(s32*)(ei + 1116) = (s32)AtreeMatch(sItemFile1Buf, strs + 152, 0);
        }
        *(s32*)(ei + 1120) = 0;
        *(s32*)(ei + 1124) = 0;
        if (sItemFile1Buf == NULL || *(s8*)(strs + 164) == 0) {
            *(s32*)(ei + 1104) = 0;
        } else {
            *(s32*)(ei + 1104) = (s32)AtreeMatch(sItemFile1Buf, strs + 164, 0);
        }
        *(s32*)(ei + 1108) = 0;
        *(s32*)(ei + 1112) = 0;
    }
    if (((EffectHeader*)ei)[82].atree == NULL) {
        ((EffectHeader*)ei)[82] = ((EffectHeader*)ei)[81];
    }
    if (((EffectHeader*)ei)[83].atree == NULL) {
        ((EffectHeader*)ei)[83] = ((EffectHeader*)ei)[81];
    }
    if (InLevel((char*)&lbl_803482D8) != 0 ||
        InLevel((char*)&lbl_803482DC) != 0) {
        if (sItemFile1Buf == NULL || *(s8*)(strs + 176) == 0) {
            *(s32*)(ei + 360) = 0;
        } else {
            *(s32*)(ei + 360) = (s32)AtreeMatch(sItemFile1Buf, strs + 176, 0);
            if (*(u32*)(ei + 360) == 0) {
                ErrorPrintf(strs + 48, strs + 176);
            }
        }
        *(s32*)(ei + 364) = 0;
        *(s32*)(ei + 368) = 0;
    }
    if (sGoodWizObj == NULL || *(s8*)(strs + 188) == 0) {
        *(s32*)(ei + 1128) = 0;
    } else {
        *(s32*)(ei + 1128) = (s32)AtreeMatch(sGoodWizObj, strs + 188, 0);
    }
    *(p1132 = (s32*)(ei + 1132)) = -512;
    *(p1136 = (s32*)(ei + 1136)) = 0;
    p1128 = (s32*)(ei + 1128);
    got = (*(u32*)p1128 != 0) ? 1 : 0;
    if (got == 0) {
        if (sItemFile1Buf == NULL || *(s8*)(strs + 188) == 0) {
            *p1128 = 0;
        } else {
            *p1128 = (s32)AtreeMatch(sItemFile1Buf, strs + 188, 0);
        }
        *p1132 = -512;
        *p1136 = 0;
    }
    lbl_80344BF8 = MBOX_FindTexture_Sub(strs + 200, 0, (s32)sPowerupsHandle,
                                        (s32)sPowerupsHandle, 1);
    lbl_80344BF4 = MBOX_FindTexture_Sub(strs + 212, 0, (s32)sWeaponsHandle,
                                        (s32)sWeaponsHandle, 1);
    lbl_80344BF0 = MBOX_FindTexture_Sub(strs + 228, 0, (s32)sWeaponsHandle,
                                        (s32)sWeaponsHandle, 1);
    lbl_80344BEC = FindTexMod(sWeaponsBuf, strs + 240, 0);
    lbl_80344BE8 = MBOX_FindTexture_Sub((char*)&lbl_803482E0, 0,
                                        (s32)sPowerupsHandle,
                                        (s32)sPowerupsHandle, 0);
    if (lbl_8034482C != 0) {
        *(s32*)(ei + 18336) = (s32)FindTexMod(sWeaponsBuf, strs + 252, 0);
    } else {
        *(s32*)(ei + 18336) = (s32)FindTexMod(sWeaponsBuf, strs + 264, 0);
    }
    *(s32*)(ei + 18340) = (s32)FindTexMod(sWeaponsBuf, strs + 276, 0);
    *(s32*)(ei + 18344) = (s32)FindTexMod(sWeaponsBuf, strs + 288, 0);
    *(s32*)(ei + 18348) = (s32)FindTexMod(sWeaponsBuf, strs + 240, 0);
    *(s32*)(ei + 18352) = (s32)FindTexMod(sWeaponsBuf, strs + 300, 0);
    if (gWadAtreeHeaders[29] != 0) {
        lbl_80344BE4 =
            FindTexMod((void*)gWadAtreeHeaders[29], strs + 312, 0);
    } else {
        lbl_80344BE4 = NULL;
    }
    if (gWadAtreeHeaders[11] != 0) {
        lbl_80344BE0 =
            FindTexMod((void*)gWadAtreeHeaders[11], strs + 324, 0);
    } else {
        if (gWadAtreeHeaders[5] != 0) {
            lbl_80344BE0 =
                FindTexMod((void*)gWadAtreeHeaders[5], strs + 324, 0);
        } else {
            lbl_80344BE0 = (void*)-1;
        }
    }
}

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
