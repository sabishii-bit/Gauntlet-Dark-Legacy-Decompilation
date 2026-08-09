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
        arc *= lbl_803480B8;
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
 * random spin, then attach the item to spawn on completion. */
s32 fn_800920E0(f32* pos, struct item* item, f32 scale)
{
    EffectPage* page = (EffectPage*)EffectInfo;
    s32 ret;
    s32 ro;
    volatile f32 pyr[3];
    volatile f32 v[3];
    u8 _spare[36];

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
        Effect* e = &page->fx[ret];
        f32 yaw = atan2(v[0], v[2]);
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

/* Spawn the combo sphere for a player class and the optional colored combo
 * burst.  The class tables share the packed magic/effect definition page. */
s32 StartComboFX(f32* pos, s32 color, s32 playerClass)
{
    u8* comboTable = (u8*)lbl_80122088;
    EffectPage* page = (EffectPage*)EffectInfo;
    s32 result = -1;

    if (playerClass >= 0) {
        s32 type = *(s32*)(comboTable + 3576);
        EffectHeader* header;

        result = -1;
        if (type < 0 || type >= MAXEFFECTTYPES) {
            ErrorPrintf("Bad Effect type: %d", type);
            result = -1;
        } else {
            header = &page->info[type];
            if (header->atree != NULL) {
                result = StartFXTree(header->atree, pos, 0, 0x80980, 0.0f);
                if (result >= 0) {
                    s32 scaled = result * 240;
                    u8* nodeField = (u8*)page + scaled;

                    MBTreeSetZsortAdd(*(struct mbnode**)(nodeField += 2996),
                                      header->zmod, 1);
                    MBTreeSetAlpha(*(struct mbnode**)nodeField, header->alpha, 1);
                    *(s32*)((u8*)page + scaled + 3072) = type;
                }
            }
        }
        if (result >= 0) {
            s32 scaled = result * 240;
            u8* nodeField = (u8*)page + scaled;
            s32 colorIndex;
            f32* lightColor;

            MBTreeSetColor(*(struct mbnode**)(nodeField += 2996),
                           lbl_8011A178[playerClass], 1);
            MBTreeSetAmbientAdd(*(struct mbnode**)nodeField, 0x1FF, 1);
            colorIndex = *(s32*)(comboTable + 108 + playerClass * 4);
            lightColor = (f32*)(comboTable + 24 + colorIndex * 12);
            if (result >= 0) {
                *(f32*)((u8*)page + scaled + 2992) = lbl_803480EC;
                if (lightColor != NULL) {
                    *(f32*)((u8*)page + scaled + 2976) = lightColor[0];
                    *(f32*)((u8*)page + scaled + 2980) = lightColor[1];
                    *(f32*)((u8*)page + scaled + 2984) = lightColor[2];
                } else {
                    *(f32*)((u8*)page + scaled + 2976) = light_color[0];
                    *(f32*)((u8*)page + scaled + 2980) = light_color[1];
                    *(f32*)((u8*)page + scaled + 2984) = light_color[2];
                }
            }
        }
    }
    if (color >= 0) {
        s32 type = *(s32*)(comboTable + 3560 + color * 4);
        s32 index = -1;
        EffectHeader* header;

        if (type < 0 || type >= MAXEFFECTTYPES) {
            ErrorPrintf("Bad Effect type: %d", type);
            index = -1;
        } else {
            header = &page->info[type];
            if (header->atree != NULL) {
                index = StartFXTree(header->atree, pos, 0, 0x400880, 0.0f);
                if (index >= 0) {
                    s32 scaled = index * 240;
                    u8* nodeField = (u8*)page + scaled;

                    MBTreeSetZsortAdd(*(struct mbnode**)(nodeField += 2996),
                                      header->zmod, 1);
                    MBTreeSetAlpha(*(struct mbnode**)nodeField, header->alpha, 1);
                    *(s32*)((u8*)page + scaled + 3072) = type;
                }
            }
        }
        result = index;
        if (result >= 0) {
            u8* effectBase = (u8*)page + result * 240;

            MBTreeSetAmbientAdd(*(struct mbnode**)(effectBase + 2996), 0x1FF, 1);
        }
    }
    return result;
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
    u8 _p3a[3236];
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

/* 0x80092B58 StartThrowMagicFX -- spawn a thrown-magic projectile: def id
 * from the magic table, launch velocity/yaw from vel, damage + light setup. */
s32 StartThrowMagicFX(f32* pos, f32* vel, s32 type, s32 player, s32 snd,
                      f32 weight, f32 dmg, f32 size)
{
    MagicView* tbl = (MagicView*)lbl_80122088;
    EffectPage* page = (EffectPage*)EffectInfo;
    s32 tid;
    s32 t4;
    s32 ret;
    s32 cp;
    f32* cp3;
    f32* pv;
    f32 rad;
    s32 fxh;
    u32 fl;
    s32 ro;

    rad = (f32)(lbl_80348128 * size);
    if (rad > lbl_80348118) {
        rad = lbl_803480A0;
    }
    tid = tbl->throwid[type & 0xF];
    t4 = type & 0xF;
    ret = -1;
    if (tid < 0 || tid >= 218) {
        ErrorPrintf(lbl_80114790, tid);
        ret = -1;
    } else {
        EffectHeader* hdr = &page->info[tid];
        if (hdr->atree != NULL) {
            ret = StartFXTree(hdr->atree, pos, 0x20010E, 0x800, lbl_80348130);
            if (ret >= 0) {
                ro = ret * 240;
                MBTreeSetZsortAdd(page->fx[ret].node, hdr->zmod, 1);
                MBTreeSetAlpha(page->fx[ret].node, hdr->alpha, 1);
                page->fx[ret].type = (fx_type)tid;
            }
        }
    }
    if (ret >= 0) {
        Effect* e = &page->fx[ret];
        if (vel != NULL) {
            f32 yaw = atan2(vel[0], vel[2]);
            e->vel[0] = vel[0];
            e->vel[1] = vel[1];
            e->vel[2] = vel[2];
            if (e->node != NULL) {
                YawMat3(e->node, yaw);
            }
        }
        pv = tbl->pyr;
        if (pv != NULL) {
            e->pyrvel[0] = pv[0];
            e->pyrvel[1] = pv[1];
            e->pyrvel[2] = pv[2];
        }
        if (weight >= lbl_80348078) {
            e->weight = weight;
        }
        e->colrad = lbl_80348134;
    }
    fxh = tbl->magicid[t4];
    if (ret >= 0) {
        page->fx[ret].fxhit = fxh;
        page->fx[ret].hit_audio = snd;
        page->fx[ret].wall_sound = snd;
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
    page->fx[ret].hitscale = rad;
    cp = tbl->coloridx[t4];
    cp3 = tbl->colors[cp];
    if (ret >= 0) {
        u8* e4 = (u8*)page + ret * 240;
        *(f32*)(e4 + 2992) = (f32)(lbl_80348120 * size);
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
        MagicFxView* fx = (MagicFxView*)((u32)base + idx * 240 + 2976);
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
 * smoke, flash, plus a deathmatch-mode variant. */
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
        fl = 2048;
        SetMagicParams((u8*)page, a, fl, dmg, lbl_80348138, -1);
        ro = a * 240;
        nd = page->fx[a].node;
        y = *(f32*)((u32)nd + 52);
        *(f32*)((u32)nd + 52) = (f32)(y + lbl_803480A8);
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
        fl = 1057;
        SetMagicParams((u8*)page, ret, fl, dmg, lbl_80348144, -1);
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
            u8* e4 = (u8*)page + ro;
            f32* c = tbl->colors[0];
            *(f32*)(e4 + 2992) = (f32)(lbl_80348150 * lbl_803480EC);
            if (c != NULL) {
                e3->lightcolor[0] = c[0];
                *(f32*)(e4 + 2980) = c[1];
                *(f32*)(e4 + 2984) = c[2];
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

extern f64 lbl_803480F0;        /* death launch velocity factor */
extern f32 lbl_803480F8;        /* death timer preset */
extern f32 lbl_803480FC;        /* death power preset */
extern void MBTreeSetZsortAdd(struct mbnode* node, s32 v, s32 a);
extern void MBTreeSetAlpha(struct mbnode* node, s32 v, s32 a);

s32 StartEnemyDeathFX(u8* en)
{
    u8* base = (u8*)EffectInfo;
    u8* hdr = base + 1056;
    s32 idx;
    s32 off;
    u8* fxp;
    u8* q;
    struct mbnode* node;
    f32 yaw;
    f32 v[3];
    f32* vp = v;

    idx = -1;
    if (*(void**)(base + 1056) == 0) {
        goto done;
    }
    idx = -1;
    if (*(void**)(base + 1056) != 0 &&
        (idx = StartFXTree(*(struct atreeheader**)(base + 1056), (f32*)(en + 48),
                           0x8C01, 0x20800, 0.0f)) >= 0) {
        off = idx * 240;
        fxp = base + off;
        node = *(struct mbnode**)(fxp += 2996);
        MBTreeSetZsortAdd(node, *(s32*)(hdr + 4), 1);
        MBTreeSetAlpha(*(struct mbnode**)fxp, *(s32*)(hdr + 8), 1);
        *(s32*)(base + off + 3072) = 88;
    }
    v[0] = (f32)(lbl_803480F0 * *(f32*)(en + 32));
    v[1] = (f32)(lbl_803480F0 * *(f32*)(en + 36));
    v[2] = (f32)(lbl_803480F0 * *(f32*)(en + 40));
    if (idx >= 0) {
        q = base + idx * 240 + 2976;
        yaw = atan2(vp[0], vp[2]);
        *(f32*)(q + 128) = vp[0];
        *(f32*)(q + 132) = vp[1];
        *(f32*)(q + 136) = vp[2];
        if (*(struct mbnode**)(q + 20) != 0) {
            YawMat3(*(struct mbnode**)(q + 20), yaw);
        }
        *(f32*)(q + 160) = 0.0f;
        *(f32*)(q + 152) = lbl_803480F8;
    }
    SetMagicParams(base, idx, 0x100020, lbl_803480FC, 0.0f, -1);
    if (idx >= 0) {
        u8* r = base + idx * 240;
        *(u16*)(r + 3168) = 89;
        *(u16*)(r + 3170) = -1;
        *(u32*)(r + 3076) = *(u32*)(r + 3076) | 0x4000;
        *(f32*)(r + 3092) = lbl_803480F8;
    }
done:
    return idx;
}


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
 * radius/morph setup, light, then deathmatch debris flames. */
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
            u8* ep = (u8*)page + ro;
            Effect* e = (Effect*)(ep + 2976);
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
            f32 y;
            r2 = -1;
            y = *(f32*)((u32)n2 + 52);
            *(f32*)((u32)n2 + 52) = (f32)(y + lbl_80348160);
            if (hdr->atree != NULL) {
                r2 = StartFXTree(hdr->atree, (f32*)(en + 48), 0, 0,
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
                Effect* e = &page->fx[r2];
                if (en != NULL) {
                    CopyMat3((f32*)en, e->node);
                }
                if ((f32*)(en + 48) != NULL) {
                    f32* pv = (f32*)(en + 48);
                    *(f32*)((u8*)e->node + 48) = pv[0];
                    *(f32*)((u8*)e->node + 52) = pv[1];
                    *(f32*)((u8*)e->node + 56) = pv[2];
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
            u8* ep = (u8*)page + ro;
            Effect* e = (Effect*)(ep + 2976);
            if ((nd = *(struct mbnode**)(ep + 2996)) != NULL) {
                f32 k1;
                MBTreeSetFlags(nd, 8, 0);
                k1 = lbl_80348168;
                *(f32*)((u8*)e->node + 64) = k1;
                *(f32*)((u8*)e->node + 68) = lbl_803480A0;
                *(f32*)((u8*)e->node + 72) = k1;
            }
        }
        nd = *(struct mbnode**)((u8*)page + ro + 2996);
        y = *(f32*)((u32)nd + 52);
        *(f32*)((u32)nd + 52) = (f32)(y + lbl_80348150);
        break;
    }
    default: {
        ro = ret * 240;
        rad = lbl_80348144;
        fl = 1057;
        {
            u8* ep = (u8*)page + ro;
            Effect* e = (Effect*)(ep + 2976);
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
            Effect* e = (Effect*)((u8*)page + ro + 2976);
            e->fxmorph = 26;
            e->fxmorph2 = 27;
            e->flags |= 0x4000;
            e->morphtime = lbl_80348170;
        }
        {
            u8* ep = (u8*)page + ro;
            Effect* e = (Effect*)(ep + 2976);
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
        f32* c = tbl->colors[0];
        *(f32*)(e4 + 2992) = (f32)(lbl_80348150 * rad);
        if (c != NULL) {
            *(f32*)(e4 + 2976) = c[0];
            *(f32*)(e4 + 2980) = c[1];
            *(f32*)(e4 + 2984) = c[2];
        } else {
            *(f32*)(e4 + 2976) = light_color[0];
            *(f32*)(e4 + 2980) = light_color[1];
            *(f32*)(e4 + 2984) = light_color[2];
        }
    }
    SetMagicParams((u8*)page, ret, fl, dmg, rad, -1);
    if (sMusicTrackHi == 10 && type == 24) {
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
                page->fx[d].damageradius = lbl_80348144;
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
        u32 v = lbl_80344BD0;
        lbl_80344BD0 = v + 1;
        tid = (v & 1) + 6;
    }
    if (tid == 5) {
        u32 v = lbl_80344BD0;
        lbl_80344BD0 = v + 1;
        tid = (v & 1) + 8;
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
        Effect* e = &page->fx[ret];
        if (mat != NULL) {
            CopyMat3(mat, e->node);
        }
    }
    {
        u8* ep = (u8*)page + ret * 240;
        struct mbnode* nd;
        Effect* e = (Effect*)(ep + 2976);
        if ((nd = *(struct mbnode**)(ep + 2996)) != NULL) {
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
