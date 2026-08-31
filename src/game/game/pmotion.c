/*
 * pmotion.c -- GCN PMOTION.OBJ (player motion / collision / transporter).
 *
 * The object begins at get_player_pos immediately after PLAYER.OBJ.  The
 * boundary at 0x80089120 is the first PSFX.OBJ body; exception records also
 * change ownership there (0x80006BE8 / 0x8000AA2C).
 *
 * .text       0x8008091C..0x80089120
 * extab       0x80006B30..0x80006BE8
 * extabindex  0x8000A918..0x8000AA2C
 *
 * NonMatching TU: bodies are being filled from GC target asm + Ghidra.
 * Names anchored from the PMOTION.OBJ roster; unverified fns stay address
 * based.  Player struct: include/game/player.h (GC-verified offsets).
 */

#include "types.h"
#include "game/player.h"
#include "game/worldobj.h"

/* ------------------------------------------------------------------ */
/* player records                                                      */
/* ------------------------------------------------------------------ */

extern Player gPlayers[]; /* gPlayerRecords[4], stride 0x335C */
#define gPlayerRecords gPlayers
#define PREC_STRIDE 0x335C
#define PF(p, off, T) (*(T*)((u8*)(p) + (off)))
#ifndef offsetof
#define offsetof(type, memb) ((u32) & ((type*)0)->memb)
#endif
typedef struct PMotionCtx PMotionCtx;

/* Offset-only layout of Enemy fields read off an index-computed base (grid
 * scan, closest-target probes) throughout this TU (verified against
 * include/game/enemy.h's Enemy struct: type@0, objgrp.coll_pos@0x54,
 * state@0xB4, rad@0x238, hht@0x23C).  Never cast a live pointer to this type
 * -- it exists only to feed offsetof() so the raw walked/indexed base pointer
 * keeps its fused-immediate-displacement addressing (see
 * claim.law.multifield-alias-defeats-indexed-addressing / offsetof-fused-
 * immediate-counter): 3+ nearby fields are read off one index-computed base
 * in these call sites, and a typed alias measured worse in the sibling TUs
 * that hit this exact pattern. */
typedef struct PCollideEnemyLayout {
    s32 type;                /* 0x000 */
    u8  _004[0x50];
    f32 coll_pos[3];         /* 0x054 objgrp.coll_pos */
    u8  _060[0x54];
    s32 state;                /* 0x0B4 */
    u8  _0B8[0x148];
    f32 health;               /* 0x200 */
    u8  _204[0x34];
    f32 rad;                  /* 0x238 */
    f32 hht;                  /* 0x23C */
} PCollideEnemyLayout;

/* Same rationale as PCollideEnemyLayout, for the gCritterPool index-computed
 * scans in this TU (verified against include/game/critter.h's Critter
 * struct: hdr@4, pos@0x05C).  Never cast a live pointer to this type. */
typedef struct PCollideCritterLayout {
    u8   _000[4];
    void* hdr;                /* 0x004 */
    u8   _008[0x54];
    f32  pos[3];               /* 0x05C */
} PCollideCritterLayout;

/* Same rationale, for sItems index-computed scans in this TU (verified
 * against include/game/item.h's Item struct: active@0xC4, action@0xC8).
 * Never cast a live pointer to this type. */
typedef struct PCollideItemLayout {
    u8  _000[0xC4];
    s16 active;                /* 0x0C4 */
    s16 activetime;             /* 0x0C6 */
    s8  action;                  /* 0x0C8 */
} PCollideItemLayout;

/* ------------------------------------------------------------------ */
/* extern globals (.sbss/.sdata runtime state)                         */
/* ------------------------------------------------------------------ */

extern s32 gGameMode;   /* game state (0x4010 = in-game) */
extern f32 gClockFrameReciprocal; /* inverse frame delta */
extern s32 gFrameTicks;   /* frame delta (int) */
extern s32 lbl_803447B8;   /* pause/menu depth */
extern s32 lbl_803447E4;   /* hit-something flag */
extern void* lbl_80344B2C; /* world root node */
extern s32 lbl_8034481C;
extern s32 lbl_80344804;
extern s32 lbl_80344808;
extern s32 gTriggerCameraState;
extern s32 gBossType;
extern s32 lbl_80344768;
extern s32 lbl_803447B4;
typedef union ControlState {
    f32 values[15];
    struct {
        s32 ctl;
        u32 levels;
        u32 edges;
        u32 repedges;
        s32 spResult;
        s32 spLast;
        s32 spTimer;
        f32 lx;
        f32 ly;
        f32 rx;
        f32 ry;
        s32 scheme;
        s32 hasActuator;
        s32 unk34;
        s32 flag;
    } pad;
    struct {
        u8 pad[56];
        s32 flag;
    } control;
} ControlState;
typedef struct PlayerActionMotionView {
    u8 pad_000[0x8F4];
    s32 heldHistory;
    s32 edgeHistory;
    u8 pad_8FC[0x5A];
    s16 actionFlags;
    s16 actionTicks;
} PlayerActionMotionView;
extern ControlState lbl_80240E30[]; /* control-pad state array, stride 15 f32 */
extern f32 sMusicFadeBase; /* sMusicFadeBase */
extern s64 gControllerButtons;
extern s32 sFlags;
extern s32 gGameOptions[];
extern f32 gClockFrameStep;
extern u8* lbl_80344EE8;
extern s32 lbl_8034489C;
extern s32 gBossActive;
extern s32 lbl_80344B24;
extern u8* lbl_80282930[4];
extern f32 lbl_803447D8;
extern s32 lbl_803448B8;
extern s32 lbl_80344740;
extern s32 lbl_8034476C;
extern s32 gBoss398;
extern s32 good_wiz_state;
extern s32 gBossDead;
extern u8 Effects[];
extern s32 lbl_80344890;
extern s32 lbl_80344894;
extern s32 lbl_80344BE8;
extern s32 sPowerupsHandle;
extern f32 lbl_80127D00[];

/* ------------------------------------------------------------------ */
/* extern functions                                                    */
/* ------------------------------------------------------------------ */

extern u32 FloorCollide(f32 rad1, f32 rad2, f32 drop, f32* pos, f32* outnrm,
                        s32 a, s32 b);
extern void MBNodeSetParent(void* node, void* parent);
extern void MBTreeSetFlags(void* node, s32 flags, s32 mode);
extern void MBTreeClearFlags(void* node, s32 flags, s32 mode);
extern void DoPlayerAction(Player* p);
extern s32 OtherPlayerOnOtherMovingObject(s32 i, u8* obj);
extern s32 sumnerSpeechActive(void);
extern s32 fn_8005B8FC(void* p);
extern s32 msgPost(s32 code, s32 player, u32 arg);
extern u32 WorldObjGetAllFlags(WorldObj* obj);
extern f32 NormalVector2D(f32* vec);
extern f32 NormalVector(f32* vec);
extern void CopyMat3(f32* src, f32* dst);
extern f32 fqdist(f32 x, f32 y);
extern f32 smallsqrt(f32 v);
extern void AudioWorldHitPlyr(void* p);
extern s32 damage_player(s32 i, f32 dmg, s32 mode, u32 flags, f32* dir);
extern f64 fn_8005C1DC(void* target, s32 arg, s32 pidx, f64 range); /* hit test -> priority */
extern void PlayerDamagedItem(Player* p, void* target, s32 exact); /* apply melee hit */
extern f32 lbl_80347B30; /* 0.0f */
extern f64 lbl_80347B28; /* hit-point y offset */
extern f64 lbl_80347B08; /* hit-priority threshold */
extern f64 lbl_80347D68; /* min separation for push-out */
extern s32 LineCylinderCollide(f32* otherPos, f32 r, f32 p3, f32* from,
                               f32* to, f32* hitOut, s32 flag);
extern f64 lbl_80347B00; /* wedge dot threshold */
extern f64 lbl_80347D78; /* second-pass range scale */
extern u8  lbl_80282850[]; /* wall-collide context (normal copy @+12) */
extern u8  lbl_8023CA98[]; /* live wall-collide result (normal @+0x10) */
extern void* lbl_80344B30; /* last wall WorldObj hit */
extern s32 lbl_80344180;   /* per-cell wall-touch counter index */
extern u8  gWorldInfo[];   /* WorldInfo (cell touch buffer @+0x5C) */
extern void* PlayerWallCollide(f32* from, f32* to, void* ctx, f32 range);
extern void SlideAlongWall(f32* from, f32* dpos, void* ctx, f32* nrm, f32 range);
extern f64 lbl_80347D00; /* slope clamp threshold */
extern f32 lbl_80347D08; /* clamped slope */
extern f32 lbl_80347B10; /* flag-8 floor slope */
extern f64 lbl_80347D10; /* swim slope bias */
extern f64 lbl_80347D18; /* dot/steep-up threshold */
extern f64 lbl_80347D20; /* rising-dpos damp */
extern f32 lbl_80347B40; /* 1.0f (sqrt normalize) */
extern f64 lbl_80347D28; /* return coeff */
extern f64 lbl_80347BB0; /* return bias */
extern f64 lbl_80347BB8; /* min forward speed / vertical gate */
extern f64 lbl_80347C28; /* damage scale / force clamp */
extern f64 lbl_80347D50; /* knockback force scale */
extern f32 lbl_80347B98; /* knockback force cap */
extern u8  gCritterPool[]; /* critter records, stride 0xAE0 */
extern u8  gEnemies[];     /* enemy records, stride 0x394 */
extern s32 damage_enemy(void* enemy, s32 pidx, s32 a3, s32 a4, f32* dir,
                        s32 flag, f32 dmg);
extern s32 CritterDamage(void* critter, s32 pidx, s32 a3, s32 a4, f32* dir,
                         s32 flag, f32 dmg);
extern s32 CritterNoHit(void* critter, s32 slot);
extern void PlayerDamagedEnemy(Player* p, void* enemy, s32 state, s32 hit, s32 a5);
extern void fn_80037ED0(void* critter, s32 slot, f32 priority);
extern f32 lbl_80347C50; /* impulse scale (fire/heavy) */
extern f64 lbl_80347D38; /* impulse scale (potion, small) */
extern f64 lbl_80347D40; /* impulse scale (potion) */
extern f32 lbl_80347D48; /* impulse scale (light) */
extern f64 lbl_80347B50; /* +pi */
extern f64 lbl_80347B60; /* 2pi */
extern f64 lbl_80347B68; /* -pi */
extern f64 lbl_80347C00;
extern f64 lbl_80347C78;
extern f32 lbl_80347C6C;
extern f64 lbl_80347CE8;
extern f64 lbl_80347D80;
extern f64 lbl_80347D88;
extern f64 lbl_80347D90;
extern f64 lbl_80347C38; /* facing-flip threshold */
extern s32 lbl_80344BF8; /* skin-fx texture id */
extern f32 atan2(f32 y, f32 x);
extern void fn_80094164(void* pos, u32 flags, s32 a3);
extern void SetSkinFX(void* node, s32 tex, s32 a3, s32 a4, f32 dur);
extern void StartEnemyGrid(f32* pos, f32 range);
extern s32 NextGridEnemy(void);
extern void StartItemGrid(f32 radius, f32* position);
extern s32 NextGridItem(void);
extern s32 FastWallCollide(f32* from, f32* to, f32* normal, s32 mode);
extern void CritterCollideStart(f32 radius, f32* position, s32 unused);
extern void* CritterMoveNodeCol(f32 radius, f32 zero, f32* from, f32* to,
                                f32* hit, s32 ignore, s32 mode);
extern s32 lbl_803447DC;
extern f32 fn_8005F0F4(void* item, s32 a2, f32* pos, f32* hit, f32 range, f32 p2);
extern s32 fn_8005D730(Player* p, void* item);
extern u8* sItems;
extern void MBTreeSetAlpha(void* node, s32 alpha, s32 mode);
extern void* fn_8005B8B0(Player* p);
extern s32 PointVisible(f32 y, f32* pos);
extern void fn_8009C98C(f32* pos);
extern s32 fn_80088EF4(Player* p, f32 range, f32 minDot);
extern f32 gFloorCollisionResult[]; /* transporter table (0x34 = target height) */
extern f32 lbl_80344880;
extern f64 lbl_80347B38;
extern s32 lbl_803443A8;
extern s32 lbl_80344500;
extern s32 lbl_80344514;
extern s32 lbl_80344518;
extern s32 lbl_8034451C;
extern s32 lbl_80344520;
extern f32 FloorPos(f32 fallback, f32 radius, f32* position, s32 mode);
extern void fn_8005A404(f32* dst, f32* src1, f32* src2);
extern void get_actual_screen_pos(s32 camera, f32* x, f32* y, f32* position);
extern void* fn_8005EFAC(f32 radius, f32* from, f32* position, s32 a4, s32 a5);
extern f32 lbl_80347C88; /* 30.0f default target range */
extern f32 lbl_80347D58; /* 200.0f boss target range */
extern u8* gBossObj;
extern s32 optionsAudioAndPrefs30[];
extern f32 CritterLineRootColSub(f32 a, f32 b, void* critter, f32* pos,
                                 f32* dir, f32* out);
extern f32 closest_enemy(f32 a, f32 b, f32* pos, f32* dir, f32* out, s32* id,
                         s32 range);
extern u8* CritterLineCollide(f32 a, f32 b, f32* pos, f32* dir, f32* hit,
                              f32* dist);
extern f32 fn_8005B274(f32 a, f32 b, f32* pos, f32* dir, f32* hit, u8** obj);
extern void FatalError(const char* msg, s32 code);
extern s32 PlayerCollidePlayers(Player* p, f32 range, f32 height, f32* from,
                                f32* to, f32* hit, s32 stopFirst);
extern s32 sMusicTrackHi;
extern u8* CurTransmitter;
extern f32 lbl_80120BF0[]; /* spawn-spread table: per-player pairs at [8+2i],
                              direction ring pairs at [16+2i] */
extern f32 gDefaultPlayerPosition[];
extern f32 sPlayerStartPositions[];
extern f32 gPlayerStartYaw;
extern u8 gIdentityMatrix[];
extern void YawMat3(f32* mat, f32 yaw);
extern void CopyMat4(f32* src, f32* dst);
extern s32 RandInt(s32 limit);
extern void fn_8005A338(f32* mat, f32* fwd, f32* anchor);
extern void UpdatePlayerWorldMat(Player* p, s32 a2);
extern void ErrorPrintf(const char* fmt, ...);
extern char lbl_80114220[]; /* get_player_pos fallback format string */
extern f32 sin(f32 angle);
extern f32 cos(f32 angle);
extern f32 lbl_80347B14; /* 4.0f (FloorCollide rad2) */
extern f32 lbl_80347B18; /* -10.0f (FloorCollide drop) */
extern f32 lbl_80347B1C; /* 99999.0f (spawn-kill critter damage) */
extern f32 lbl_80347B20; /* 9999.0f (spawn-kill enemy damage) */
extern f64 lbl_80347B58;
extern f64 lbl_80347B70;
extern f64 lbl_80347B78;
extern f64 lbl_80347B80;
extern f64 lbl_80347B88;
extern f64 lbl_80347B90;
extern f64 lbl_80347BA0;
extern f64 lbl_80347BA8;
extern f64 lbl_80347BC0;
extern f64 lbl_80347BD0;
extern f64 lbl_80347BD8;
extern f64 lbl_80347BE0;
extern f64 lbl_80347BE8;
extern f64 lbl_80347BF0;
extern f32 lbl_80347BF8;
extern f64 lbl_80347C08;
extern f32 lbl_80347C10;
extern f64 lbl_80347C18;
extern f32 lbl_80347C20;
extern f64 lbl_80347C30;
extern f32 lbl_80347C40;
extern f64 lbl_80347C48;
extern f32 lbl_80347C54;
extern f32 lbl_80347C58;
extern u32 lbl_80344B38; /* last floor WorldObj hit (address word) */
extern f32 lbl_80344B34;

extern s32 fn_8005A730(f32* mat);
extern s32 fn_80088938(Player* p, f32 angle);
extern s32 PlayerCollideEnemies(Player* p, s32 a2, f32* pos, f32* out,
                                s32 a5, s32* out2, f32 range, f32 height);
extern int PlayerCollideWalls(Player* p, s32 unused, f32* dpos, f32* from,
                              f32* to);
extern void PlayerMotion_FloorFX(Player* p, WorldObj* obj, f32* v1, f32* v2);
extern s32 PlayerCollideFloor(u8* p, f32* pos, f32* dpos, s32 mode, f32 rad,
                              f32 height);
extern int PlayerNewFloor(PMotionCtx* m, Player* p, f32* dpos);
extern s32 fn_80088714(f32 range, Player* p, f32* pos, f32* dpos);
extern void PlayerMotion_HitTarget(Player* p, void* target, s32 arg, f32 range);
extern s32 PlayerMotion_DamageTarget(Player* p, s32 targetId, s32 a3, s32 a4,
                                     s32 a5, f32 dmg, f32 priority);
extern s32 DoTransporter(Player* p, f32* pos, f32* out, f32 range);
extern void DoExit(Player* p);
extern s32 CameraLimitPlayerDpos(s32 player, f32* dpos, s32 arg);
extern void PlyrSfxDoDamage(u8* p, s32 idx, u8* p2, u8* other, f32 t0, f32 t1);
extern void fn_8009DCB4(s32 pos);
extern u32 PlayerKnockback(f32 angle, Player* p, f32* out);
extern f32 PlayerGetTarget(Player* p, f32* pos, f32* dir, f32* out,
                           s32* outId, u8** outObj);
extern s32 StartFXNoLoop(s32 type, f32* pos);
extern void CreateDirMatrix(f32* matrix, f32* direction, f32* up);
extern s32 GetWorldMat(void* node, f32* matrix, f32* offset);
extern void AudioPlayEvt101IfIdle(s32 pos);
extern void fn_8009F158(s32 player);
extern void fn_8009F390(s32 player);
extern void PlayerMotion_FindClosestPlayer(Player* p, f32* direction,
                                            u32 flags, f32 damage);
extern s32 StartFXSub(s32 type, f32* position, u32 flagsA, u32 flagsB,
                      f32 time);
extern s32 DeleteEffect(s32 effect, s32 mode);
extern s32 StartComboFX(f32* position, s32 type, s32 color);
extern void fn_8009C9DC(s32 mode, f32* position);
extern void SfxSetParent(s32 effect, void* parent);
extern void MBTreeSetAmbientAdd(void* node, s32 value, s32 recurse);
extern void MBTreeSetAltTex(void* node, s32 index, s32 texture, s32 recurse);
extern s32 MBOX_FindTexture(const char* name, void** result);
extern void SfxSetMorph(f32 time, s32 effect, s32 morph1, s32 morph2);
extern void MBTreeSetColor(void* node, u32 color, s32 recurse);
extern void SfxSetPhysics(s32 effect, f32* velocity, f32* pyrVelocity,
                        f32 weight, f32 radius);
extern void SfxSetHitTarget(f32 speed, s32 effect, void* target);
extern void SfxSetDamage(f32 damage, f32 radius, f32 delay, s32 effect,
                         s32 type, s32 owner);
extern void* MBNewPsysDefault(f32* matrix, void* parent, s32 flags,
                              s32 arena);
extern void MBPsysSetEVolume(f32 base, f32 range, void* psys);
extern void MBPsysSetPParm(f32 a, f32 b, f32 c, f32 d, void* psys,
                           s32 parameter);
extern void MBPsysSetPTex(void* psys, s32 texture);
extern void MBPsysSetERate4(f32 a, f32 b, f32 c, f32 d, void* psys);
extern void MBPsysSetETime(f32 duration, f32 repeat, void* psys);
extern void MBPsysSetPSpeed(f32 speed, void* psys);
extern void CreateYPRMatrix(f32* matrix, f32* angles);
extern s32 player_get_powerup_state(f32 dt, void* p, s32 type, u32 mask);
extern void ShakeCamera(s32 type, s32 count, s32 delay, f32 radius,
                        s32 priority);
extern void fn_8009D4B0(s32 player);
extern void fn_8009F490(s32 player);
extern void fn_8009F450(s32 player);
extern void AudioTurboDefense(s32 player);
extern void AudioPlayerTurbo(s32 player, s32 a2, s32 a3);
extern s32 MBOX_ReallyFindObject(const char* name, s32 first, s32 last,
                                 s32 exact);
extern s32* AtreeFindMbidxNode(void* tree, s32 mbidx);
extern void MulVecMat4(f32* vector, f32* out, f32* matrix);
extern void CalcTargetDir(f32* velocity, f32 targetScale, f32 speed,
                          f32 gravity, f32 lift);
extern void fn_80093918(s32 effect, s32 player, f32* position, f32* velocity,
                        f32 scale, f32 speed, f32 arg);
extern s32 PlayerStartMissile(u8* player, f32* direction, u32 flags, s32 mode,
                              f32 speed, f32 scale);
extern void fn_8009F410(s32 player);
extern void fn_8009F3D0(s32 player);
extern void AudioPlayerEatSFX(s32 player);
extern s32 StartMagicPlayerFX(f32* position);
extern void start_magic(s32 player, f32* position, u32 flags, s32 mode,
                        f32 scale);
extern f32 ModifyPlayerDpos(Player* p, f32* from, f32* dpos, u32 flags,
                            s32 targetId, u32 target, f32 distance,
                            f32 scale);
extern void ReflectVector2D(const f32* vector, const f32* normal, f32* out);
extern f64 SlowNormalVector(f32* vector);
extern void PlayerSetGrabbed(Player* player, void* parent, f32* position);
extern void PlayerUnsetGrabbed(Player* player, s32 restore);
extern void fn_8009EFCC(s32 player, s32 variant, s32 kind);

extern f32 lbl_80347C5C;
extern f32 lbl_80347C60;
extern f32 lbl_80347C64;
extern f32 lbl_80347C68;
extern f64 lbl_80347C70;
extern f64 lbl_80347C80;
extern f32 lbl_80347C8C;
extern f32 lbl_80347C90;
extern f32 lbl_80347C94;
extern f64 lbl_80347C98;
extern f32 lbl_80347CA0;
extern f32 lbl_80347CA8;
extern f32 lbl_80347CAC;
extern f32 lbl_80347CB0;
extern f32 lbl_80347CB4;
extern f32 lbl_80347CB8;
extern f32 lbl_80347CBC;
extern f32 lbl_80347CC0;
extern f32 lbl_80347CC4;
extern f64 lbl_80347CC8;
extern f32 lbl_80347CD0;
extern f32 lbl_80347CD4;
extern f64 lbl_80347CD8;
extern f64 lbl_80347CE0;
extern f64 lbl_80347CF0;
extern f32 lbl_80347CF8;

/* Player-motion transform context (arg to PlayerNewFloor / collision fns):
 * a 3x3-ish orient block at 0x10 and the current floor WorldObj* at 0x44. */
struct PMotionCtx {
    u8         _p00[0x10];
    f32        fwd[3];    /* 0x10, 0x14, 0x18 */
    u8         _p1c[0x28];
    WorldObj*  floor;     /* 0x44 */
};

/* float magnitude via sign-bit clear (matches the inline fabs codegen). */
static f32 fabsf_(f32 x) {
    f32 slots[3];

    slots[2] = x;
    *(u32*)&slots[2] &= 0x7FFFFFFF;
    return slots[2];
}

static f32 fabsf_param(f32 x) {
    *(u32*)&x &= 0x7FFFFFFF;
    return x;
}

static inline f32 PlayerKnockbackFabs(f32 x) {
    f32 slots[4];

    slots[3] = x;
    *(u32*)&slots[3] &= 0x7FFFFFFF;
    return slots[3];
}

/* ================================================================== */

#define STUB(address, name) void name(void) {}

/* get_player_pos spawn view: kills the PF() address-CSE on the rotation
 * triple and the floor-object word (L7 -- struct-displacement view). */
typedef struct PSpawnView {
    u8  _000[0xC4];
    f32 rot[3];          /* 0xC4 euler rotation */
    u8  _0D0[0x7E4];
    f32 floor_y;         /* 0x8B4 last floor height */
    u8  _8B8[8];
    u32 floor_flags;     /* 0x8C0 */
    u32 floor_obj;       /* 0x8C4 */
    u8  _8C8[0x38];
    u32 act_bits;        /* 0x900 melee/effect pending bits */
} PSpawnView;
#define SV(p) ((PSpawnView*)(p))

/* 0x8008091C - compute the spawn position for player `playerIdx`: gate on the
 * music state, then (in attract/0x400C modes) first try to drop next to an
 * already-placed earlier player using the per-player spread pair; otherwise
 * rotate through the other players from a random start and try the 16-slot
 * direction ring around each candidate's collision position.  On total
 * failure fall back to the level's default start (ring-searched, then the
 * fixed start positions).  `mode` 1 forces the default-start fallback; mode 2
 * accepts a failed partner probe anyway. */
s32 try_location(u8* motion, Player* p, f32* position, f32* resultPosition,
                 s32* resultItem, s32 findFloor);
void get_player_pos(s32 playerIdx, s32 mode) {
    f32 pos2[3];
    f32 pos[3];
    f32 resultPos[3];
    u8 unused_74[12];
    f32 mat[16];
    s32 resultItem = -1;
    u8 unused_8[24];
    s32 partner = -1;
    s32 found = -1;
    Player* p;
    Player* other;
    f32 r;
    f32 sx;
    f32 sz;
    f32 s;
    f32 c;
    f32 ang;
    f32 y;
    f64 halfR;
    f64 thresh;
    s32 ok;
    s32 rand4;
    s32 i;
    s32 j;
    s32 idx;
    s32 k;
    f32* spreadz;
    u8* ctx = lbl_80282850;
    f32* spread = lbl_80120BF0;

    if (sMusicTrackHi < 0) {
        return;
    }
    p = &gPlayers[playerIdx];
    SV(p)->floor_obj = 0;
    if (p->state != 1 && p->state != 4) {
        return;
    }
    fn_8005A338(p->mat, p->anchor_fwd, p->anchor_pos);
    rand4 = RandInt(4);

    if (gGameMode == 0x400C || lbl_803447B8 != 0) {
        for (i = 0; i < 4; i++) {
            if (i >= playerIdx) {
                break;
            }
            other = &gPlayers[i];
            if (other->state == 1 || other->state == 4) {
                if ((other->hud_flags & 0x20) == 0) {
                    break;
                }
            }
        }
        if (mode == 1) {
            found = -2;
        }
        if (i != playerIdx && i < 4) {
            other = &gPlayers[i];
            other->floor_base = other->pos[1];
            CopyMat4(other->mat, p->mat);
            SV(p)->rot[0] = SV(other)->rot[0];
            SV(p)->rot[1] = SV(other)->rot[1];
            SV(p)->rot[2] = SV(other)->rot[2];
            r = 0.5 + other->col_radius;
            pos[0] = other->col_pos[0];
            pos[1] = other->col_pos[1];
            pos[2] = other->col_pos[2];
            spreadz = spread + 9;
            sx = r * (spread[8 + playerIdx * 2] - spread[8 + i * 2]);
            sz = r * (spreadz[playerIdx * 2] - spreadz[i * 2]);
            ang = CurTransmitter != NULL ? *(f32*)(CurTransmitter + 24) : 0.0;
            s = sin(ang);
            c = cos(ang);
            pos[0] += sx * c + sz * s;
            c = cos(ang);
            s = sin(ang);
            pos[2] += -sx * s + sz * c;
            if (try_location((u8*)other, p, pos, resultPos, &resultItem, 1) != 0) {
                found = i;
            } else if (mode == 2) {
                CopyMat4(other->mat, p->mat);
                found = i;
            }
        }
    }

    if (found == -1) {
        f64 half = lbl_80347B00;
        for (j = 0; j < 4; j++) {
            idx = (rand4 + j) % 4;
            if (idx == playerIdx) {
                continue;
            }
            other = &gPlayers[idx];
            if (other->state != 1 && other->state != 4 && other->state != 8) {
                continue;
            }
            if (other->node == NULL) {
                continue;
            }
            partner = idx;
            if ((other->hud_flags & 0x20) != 0) {
                pos2[0] = other->saved_pos[0];
                pos2[1] = other->saved_pos[1];
                pos2[2] = other->saved_pos[2];
            } else {
                pos2[0] = other->pos[0];
                pos2[1] = other->pos[1];
                pos2[2] = other->pos[2];
            }
            SV(other)->floor_obj = FloorCollide(lbl_80347B10, lbl_80347B14,
                lbl_80347B18, pos2, (f32*)(ctx + 24), 1, 1);
            SV(other)->floor_flags = (*(void**)(ctx + 92) != NULL)
                                         ? ((WorldObj*)*(void**)(ctx + 92))->flags
                                         : 0;
            other->floor_base = *(f32*)(ctx + 76);
            CopyMat3(other->mat, p->mat);
            p->pos[0] = pos2[0];
            p->pos[1] = pos2[1];
            p->pos[2] = pos2[2];
            SV(p)->rot[0] = SV(other)->rot[0];
            SV(p)->rot[1] = SV(other)->rot[1];
            SV(p)->rot[2] = SV(other)->rot[2];
            r = half + other->col_radius;
            k = 0;
            do {
                pos[0] = other->col_pos[0];
                pos[1] = other->col_pos[1];
                pos[2] = other->col_pos[2];
                pos[0] += r * spread[16 + k * 2];
                pos[2] += r * spread[17 + k * 2];
                if (try_location((u8*)other, p, pos, resultPos, &resultItem, 1) != 0) {
                    found = idx;
                    break;
                }
                k++;
            } while (k < 16);
            if (found >= 0) {
                MBNodeSetParent(p->node, *(void**)(other->node + 0x74));
                SV(p)->floor_obj = SV(other)->floor_obj;
                break;
            }
        }
    }

    if (found < 0) {
        if (partner >= 0) {
            other = &gPlayers[partner];
            CopyMat4(other->mat, p->mat);
            SV(p)->rot[0] = SV(other)->rot[0];
            SV(p)->rot[1] = SV(other)->rot[1];
            SV(p)->rot[2] = SV(other)->rot[2];
            if (resultItem >= 0) {
                p->pos[0] = resultPos[0];
                p->pos[1] = resultPos[1];
                p->pos[2] = resultPos[2];
                if (resultItem >= 0x10000) {
                    CritterDamage(gCritterPool + (resultItem & 0xFFFF) * 2784,
                                  -2, 0, 0, NULL, 1, lbl_80347B1C);
                } else {
                    damage_enemy(gEnemies + resultItem * 916,
                                 -2, 0, 0, NULL, 1, lbl_80347B20);
                }
            } else {
                p->floor_base = FloorPos(p->pos[1], lbl_80347B10, p->pos, 1);
                p->pos[1] = p->floor_base;
                MBNodeSetParent(p->node, *(void**)(other->node + 0x74));
                SV(p)->floor_obj = SV(other)->floor_obj;
                ErrorPrintf(lbl_80114220);
            }
        } else {
            CopyMat4((f32*)gIdentityMatrix, mat);
            YawMat3(mat, gPlayerStartYaw);
            r = 0.5 + p->col_radius;
            pos[0] = gDefaultPlayerPosition[0];
            pos[1] = gDefaultPlayerPosition[1];
            pos[2] = gDefaultPlayerPosition[2];
            halfR = 0.5 * r;
            y = FloorPos(lbl_80344880, halfR, pos, 1);
            if (*(void**)((u8*)gFloorCollisionResult + 0x44) == NULL) {
                ok = 0;
            } else {
                f32 d = y - pos[1];
                *(u32*)&d &= 0x7FFFFFFF;
                if (d > lbl_80347B28) {
                    ok = 0;
                } else {
                    pos[1] = y;
                    ok = 1;
                }
            }
            i = 0;
            thresh = lbl_80347B28;
            do {
                f32 d;
                if (ok != 0) {
                    break;
                }
                pos[0] = gDefaultPlayerPosition[0];
                pos[1] = gDefaultPlayerPosition[1];
                pos[2] = gDefaultPlayerPosition[2];
                pos[0] += r * spread[16 + i * 2];
                pos[2] += r * spread[17 + i * 2];
                y = FloorPos(lbl_80344880, halfR, pos, 1);
                if (*(void**)((u8*)gFloorCollisionResult + 0x44) == NULL) {
                    ok = 0;
                } else {
                    d = y - pos[1];
                    *(u32*)&d &= 0x7FFFFFFF;
                    if (d > thresh) {
                        ok = 0;
                    } else {
                        pos[1] = y;
                        ok = 1;
                    }
                }
                i++;
            } while (i < 16);
            if (ok == 0) {
                pos[0] = sPlayerStartPositions[0];
                pos[1] = sPlayerStartPositions[1];
                pos[2] = sPlayerStartPositions[2];
            }
            mat[12] = pos[0];
            mat[13] = pos[1];
            mat[14] = pos[2];
            y = FloorPos(pos[1], lbl_80347B10, &mat[12], 1);
            mat[13] = y;
            p->floor_base = y;
            CopyMat4(mat, p->mat);
            SV(p)->rot[0] = 0.0f;
            SV(p)->rot[1] = gPlayerStartYaw;
            SV(p)->rot[2] = 0.0f;
        }
    } else {
        if (gGameMode == 0x400C) {
            CopyMat4((f32*)gIdentityMatrix, mat);
            YawMat3(mat, gPlayerStartYaw);
            CopyMat3(mat, p->mat);
            SV(p)->rot[0] = 0.0f;
            SV(p)->rot[1] = gPlayerStartYaw;
            SV(p)->rot[2] = 0.0f;
        }
    }

    p->vibe_on = 0;
    UpdatePlayerWorldMat(p, 0);
}
s32 PlayerCollideItems(Player* p, f32 range, f32 height, f32* from, f32* to,
                       f32* hit);

s32 try_location(u8* motion, Player* p, f32* position, f32* resultPosition,
                 s32* resultItem, s32 findFloor) {
    f32 screen[2];
    u8 unused[8];
    f32 collidePosition[3];
    f32 hitPosition[3];
    f32 delta;
    u8 unusedTail[4];
    Player* pm = (Player*)motion;
    f32 radius = p->col_radius;
    f32 height = p->col_height;
    s32 floorFlags = SV(motion)->floor_flags & 0x38;

    collidePosition[0] = pm->effectpos[0];
    collidePosition[1] = pm->effectpos[1];
    collidePosition[2] = pm->effectpos[2];

    if (findFloor != 0) {
        screen[0] = FloorPos(lbl_80344880, (f32)(lbl_80347B00 * radius), position, 1);
        if (*(void**)((u8*)gFloorCollisionResult + 0x44) == NULL) {
            return 0;
        }
        if ((WorldObj*)SV(motion)->floor_obj != NULL &&
            ((((WorldObj*)SV(motion)->floor_obj)->flags & 0x1000) != 0) &&
            (WorldObj*)SV(motion)->floor_obj != *(void**)((u8*)gFloorCollisionResult + 0x44)) {
            return 0;
        }
        delta = screen[0] - SV(motion)->floor_y;
        *(u32*)&delta &= 0x7FFFFFFF;
        if (delta > lbl_80347B38 ||
            (delta > lbl_80347B28 && floorFlags == 0)) {
            return 0;
        }
        position[1] = screen[0];
    } else {
        position[1] = SV(motion)->floor_y;
    }

    p->pos[0] = position[0];
    p->pos[1] = position[1];
    p->pos[2] = position[2];
    fn_8005A404(&p->mat[0], p->anchor_fwd, p->anchor_pos);

    if (gGameMode != 0x400C && lbl_80344500 == 0 && lbl_803443A8 == 0) {
        get_actual_screen_pos(0, (f32*)&screen[1], (f32*)&screen[0], p->col_pos);
        if (screen[1] < (f32)(lbl_80344520 + 30) ||
            screen[1] > (f32)(lbl_8034451C - 30) ||
            screen[0] > (f32)(lbl_80344518 - 20) ||
            screen[0] < (f32)(lbl_80344514 + 40)) {
            return 0;
        }
    }

    position[1] += p->anchor_fwd[1];
    if (PlayerWallCollide(collidePosition, position, NULL, lbl_80347B40) != NULL) {
        return 0;
    }
    if (PlayerCollidePlayers(p, radius, height, position, position, hitPosition, 1) >= 0) {
        return 0;
    }
    p->collision_item = fn_8005EFAC(radius, collidePosition, position, 0, 0);
    if (p->collision_item != NULL && fn_8005D730(p, p->collision_item) != 0) {
        return 0;
    }
    {
        s32 item = PlayerCollideItems(p, radius, height, position, position, hitPosition);
        if (item >= 0) {
            resultPosition[0] = position[0];
            resultPosition[1] = position[1];
            resultPosition[2] = position[2];
            *resultItem = item;
            return 0;
        }
    }
    return 1;
}
void PlayerMotion_SetAnimState(Player* p) {
    u8 unused[32];
    if (lbl_803447B8 >= 2) {
        MBTreeClearFlags(p->node, 2, 0);
        if (p->mbnode != NULL) {
            MBTreeClearFlags(p->mbnode, 2, 0);
        }
        if (p->anim_208 == 0x7C) {
            p->hud_flags |= 0x1000;
        }
        if ((p->hud_flags & 0x1000) == 0) {
            p->anim_20C = 0x7C;
        } else {
            p->anim_20C = 0;
        }
    } else {
        p->anim_20C = 0;
        MBTreeSetFlags(p->node, 2, 0);
        if (p->mbnode != NULL) {
            MBTreeSetFlags(p->mbnode, 2, 0);
        }
    }
    DoPlayerAction(p);
}
/*
 * PlayerMotion  0x80081504  (0x4A9C bytes -- the giant per-frame driver).
 *
 * Portable reconstruction from the complete target control flow.
 *
 * The master player-motion routine, called once per active player per frame
 * from do_players().  It reads the pad-derived desired velocity, resolves it
 * against the world, and commits the new transform.  Orchestrates (in rough
 * order): GetWorldMat/MulVecMat4 to build the motion frame; ModifyPlayerDpos
 * to shape the raw dpos (slope/gravity); FastWallCollide + PlayerCollideWalls
 * for wall sliding; PlayerNewFloor/PlayerCollideFloor for floor snap + moving
 * platforms (PlayerSetGrabbed/PlayerUnsetGrabbed reparenting); the collision
 * sweeps PlayerCollideEnemies / PlayerCollidePlayers / PlayerCollideItems;
 * PlayerGetTarget + PlayerMotion_HitTarget / _DamageTarget / _FindClosestPlayer
 * for melee/attack resolution (SfxSetHitTarget/SfxSetDamage/SfxSetMorph,
 * CritterDamage); DoTransporter / DoExit for level portals; PlayerKnockback
 * and ShakeCamera / CameraLimitPlayerDpos for hit reactions; the MBPsys
 * and MBTree calls for footstep/trail particle + tint effects; and finally
 * DoPlayerAction (via PlayerMotion_SetAnimState) to advance the anim state.
 * ReflectVector2D/NormalVector(2D) do the vector math; AudioPlayEvt101IfIdle
 * handles idle SFX.  Jump/switch tables live in this TU .data section.
 */
static f32 PlayerMotion_WrapAngle(f32 angle) {
    f64 a = (f64)angle;
    f64 wrapped = (a > lbl_80347B50)  ? a - lbl_80347B60
                  : (a <= lbl_80347B68) ? lbl_80347B60 + a
                                        : a;
    return (f32)wrapped;
}

static s32 PlayerMotion_FpClassify(f32 value) {
    const s32 expMask = 0x7F800000;
    const s32 mantissaMask = 0x007FFFFF;

    switch ((*(s32*)&value) & expMask) {
    case 0x7F800000:
        return ((*(s32*)&value) & mantissaMask) != 0 ? 1 : 2;
    case 0:
        return ((*(s32*)&value) & mantissaMask) != 0 ? 5 : 3;
    default:
        return 4;
    }
}

static s32 PlayerMotion_SfxIndex(Player* p) {
    Player* other = p->grab_partner;
    s32 sfx;

    if (other != NULL) {
        sfx = *(s16*)(lbl_80282930[other->index] + 0x20);
    } else {
        sfx = -1;
    }
    return sfx;
}

void PlayerMotion(Player* p) {
    char* strings = lbl_80114220;
    u8* ctxbase = lbl_80282850;
    ControlState* ctl = &lbl_80240E30[p->index];
    s32 index = p->index;
    u8* motion = (u8*)p + 0x14;
    f32 radius = p->col_radius;
    f32 height = p->col_height;
    u8 unused[16];
    f32 oldpos[3];
    f32 dpos[3];
    u8 padgap[12];
    f32 to[3];
    f32 hit[3];
    f32 targetDir[3];
    f32 attackDir[3];
    f32 effectMatrix[16];
    s32 item = -1;
    u8* target = NULL;
    u8* firstEnemy;
    f32 reflection[3];
    f32 padvec[17];
    f32 effectVelocity[3];
    f32 bossColor[3];
    f32 localVector[3];
    f32 missileVelocity[3];
    s32 anim = p->anim_208;
    s32 motionType;
    s32 directionKind;
    s32 firstEnemyHits;
    s32 floorResult;
    s32 floorBlocked = 0;
    s32 transporter;
    s32 wallResult;
    s32 hitKind = 0;
    s32 specialCritter;
    f32 facing;
    f32 controlYaw;
    f32 heading;
    f32 movement;
    f32 speedScale;
    f32 moveLimit;
    f32 moveAmount;

    if (fn_8005A730((f32*)motion) == 0) {
        FatalError(strings + 36, 0x800000);
        get_player_pos(index, 0);
    }

    p->hud_flags &= ~1;
    facing = (f32)(atan2(*(f32*)(lbl_80344EE8 + 0x84),
                         *(f32*)(lbl_80344EE8 + 0x8C)) + lbl_80347B50);
    motionType = fn_80088938(p, facing);
    if (motionType == 15) {
        directionKind = 1;
    } else if (motionType == 16) {
        directionKind = 2;
    } else {
        directionKind = 0;
    }

    controlYaw = atan2(*(f32*)(motion + 0x20), *(f32*)(motion + 0x28));
    if (p->anim_208 == 143 && p->grab_partner != NULL &&
        lbl_80240E30[p->grab_partner->index].values[8] > 0.0f) {
        ControlState* otherCtl =
            &lbl_80240E30[p->grab_partner->index];
        movement = otherCtl->values[8];
        controlYaw = otherCtl->values[7];
    } else if (anim == 8) {
        if ((f64)ctl->values[8] < lbl_80347B58) {
            controlYaw = p->move_yaw - facing;
        } else {
            controlYaw = ctl->values[7];
        }
        movement = 1.0f;
    } else if (ctl->values[8] == 0.0f && p->char_type == 0 &&
               (anim == 35 || anim == 37)) {
        movement = lbl_80347B10;
        controlYaw = p->move_yaw - facing;
    } else {
        movement = ctl->values[8];
        controlYaw = ctl->values[7];
    }
    speedScale = movement * p->field_A50;

    if (lbl_8034489C == 2) {
        dpos[0] = *(f32*)(gBossObj + 0x3C) - *(f32*)(motion + 0x30);
        dpos[1] = *(f32*)(gBossObj + 0x40) - *(f32*)(motion + 0x34);
        dpos[2] = *(f32*)(gBossObj + 0x44) - *(f32*)(motion + 0x38);
        heading = atan2(dpos[0], dpos[2]);
        speedScale = 0.0f;
        facing = heading;
    } else if ((f64)ctl->values[10] > lbl_80347B08) {
        heading = ctl->values[9] + facing;
        facing = PlayerMotion_WrapAngle(controlYaw + facing);
    } else if ((motionType >= 2 && motionType <= 6) ||
               (motionType >= 9 && motionType <= 12) ||
               (motionType >= 17 && motionType <= 20)) {
        facing = PlayerMotion_WrapAngle(controlYaw + facing);
        heading = p->move_yaw;
    } else if ((f64)speedScale > lbl_80347B08) {
        heading = PlayerMotion_WrapAngle(controlYaw + facing);
        facing = heading;
    } else {
        heading = p->move_yaw;
        facing = heading;
        if (anim >= 62 && anim < 79) {
            speedScale = lbl_80347B10;
        }
    }

    if (p->action >= 11 &&
        (p->act_flags & 0x8000) == 0) {
        dpos[0] = 0.0f;
        dpos[1] = 0.0f;
        dpos[2] = 0.0f;
    } else {
        f32 angle;
        dpos[0] = p->vel[0] * gClockFrameStep;
        dpos[1] = p->vel[1] * gClockFrameStep;
        dpos[2] = p->vel[2] * gClockFrameStep;
        if (dpos[0] * dpos[0] + dpos[1] * dpos[1] +
                dpos[2] * dpos[2] >= lbl_80347B70) {
            angle = PlayerMotion_WrapAngle(atan2(dpos[0], dpos[2]) - facing);
            if ((f64)angle < lbl_80347B78 ||
                (f64)angle > lbl_80347B80) {
                speedScale = 0.0f;
            }
        }
        dpos[0] += p->light_vec[0] * gClockFrameStep;
        dpos[2] += p->light_vec[2] * gClockFrameStep;
    }

    if (PlayerMotion_FpClassify(dpos[0]) == 1 ||
        PlayerMotion_FpClassify(dpos[1]) == 1 ||
        PlayerMotion_FpClassify(dpos[2]) == 1) {
        p->light_vec[0] = 0.0f;
        p->light_vec[1] = 0.0f;
        p->light_vec[2] = 0.0f;
        p->light_vel[0] = 0.0f;
        p->light_vel[1] = 0.0f;
        p->light_vel[2] = 0.0f;
        dpos[0] = 0.0f;
        dpos[1] = 0.0f;
        dpos[2] = 0.0f;
    }

    moveLimit = (f32)(lbl_80347B88 *
                      (p->light_range * gClockFrameStep));
    if ((p->act_flags & 0x18160) != 0) {
        moveLimit = (f32)(lbl_80347B90 * gClockFrameStep);
    }
    if (lbl_8034489C >= 1 && lbl_8034489C <= 4 && gBossType == 35 &&
        gBossActive != 0) {
        moveLimit = lbl_80347B98 * gClockFrameStep;
    }
    dpos[1] = dpos[1] < 0.0f
                  ? 0.0f
                  : (dpos[1] > moveLimit ? moveLimit : dpos[1]);

    if (anim == 137) {
        moveAmount = (f32)(lbl_80347BA0 * gClockFrameStep);
        dpos[0] = moveAmount * sin(facing);
        dpos[2] = moveAmount * cos(facing);
    } else if (motionType == 29) {
        moveAmount = (f32)(lbl_80347BA8 *
                           (gClockFrameStep *
                            (p->light_range * speedScale)));
        dpos[0] = moveAmount * sin(facing);
        dpos[1] = speedScale != 0.0f
                      ? (f32)lbl_80347B08
                      : (f32)(lbl_80347BB0 * gClockFrameStep);
        dpos[2] = moveAmount * cos(facing);
    } else {
        moveAmount = p->field_A48 *
                     (gClockFrameStep * (p->light_range * speedScale));
        dpos[0] += moveAmount * sin(facing);
        dpos[2] += moveAmount * cos(facing);
        dpos[0] = dpos[0] < -moveLimit
                      ? -moveLimit
                      : (dpos[0] > moveLimit ? moveLimit : dpos[0]);
        dpos[2] = dpos[2] < -moveLimit
                      ? -moveLimit
                      : (dpos[2] > moveLimit ? moveLimit : dpos[2]);
    }

    if (*(f32*)(motion + 0x34) <= lbl_80344880) {
        get_player_pos(index, 0);
    }
    if (PlayerMotion_FpClassify(*(f32*)(motion + 0x24)) == 1) {
        FatalError(strings + 36, 0x800000);
        get_player_pos(index, 0);
    }

    p->dpos[0] = dpos[0];
    p->dpos[1] = dpos[1];
    p->dpos[2] = dpos[2];
    oldpos[0] = p->effectpos[0];
    oldpos[1] = p->effectpos[1];
    oldpos[2] = p->effectpos[2];

    if ((p->hud_flags & 0x20) != 0) {
        fn_8005A338(p->mat, (f32*)((u8*)p + 0x844),
                    (f32*)((u8*)p + 0x838));
        if (lbl_8034481C != 0 &&
            (lbl_80344804 != 0 || lbl_80344808 != 0)) {
            p->state = 4;
            p->field_1F2 = 0;
        }
        goto collision_done;
    }
    if (motionType == 29) {
        goto detach_floor;
    }

    to[0] = oldpos[0] + dpos[0];
    to[1] = oldpos[1] + dpos[1];
    to[2] = oldpos[2] + dpos[2];
    p->special_collision_item = 0;
    specialCritter = 0;
    firstEnemyHits = PlayerCollideEnemies(
        p, (s32)oldpos, to, to, 0, (s32*)&firstEnemy, radius, height);
    if (firstEnemyHits != 0) {
        u8* object = (u8*)firstEnemy;
        if ((s8)object[0xCF] >= 0 && *(s16*)(object + 0xD0) > 0 &&
            (directionKind != 0 || p->action != 0)) {
            dpos[0] = 0.0f;
            dpos[2] = 0.0f;
        } else {
            dpos[0] = to[0] - oldpos[0];
            dpos[2] = to[2] - oldpos[2];
        }
        if (anim == 137 || anim == 143) {
            s32 sfx;
            PlayerMotion_HitTarget(p, firstEnemy, 32,
                (f32)(anim == 137 ? lbl_80347BB8 : lbl_80347BC0));
            {
                s32 sfxProbe = PlayerMotion_SfxIndex(p);
                sfx = sfxProbe;
                if (sfxProbe >= 0) {
                    PlyrSfxDoDamage((u8*)p->grab_partner, sfx, (u8*)p,
                                (u8*)to, lbl_80347B30, lbl_80347B40);
                }
            }
            hitKind = 1;
        }
    }

    to[0] = oldpos[0] + dpos[0];
    to[1] = oldpos[1] + dpos[1];
    to[2] = oldpos[2] + dpos[2];
    {
        s32 otherIndex =
            PlayerCollidePlayers(p, radius, height, oldpos, to, to, 0);
        if (otherIndex >= 0 && anim == 137 &&
            p->grab_partner == &gPlayers[otherIndex] &&
            (f64)sMusicFadeBase <
                lbl_80347B88 + p->combo_fade) {
            otherIndex = -1;
        }
        if (otherIndex >= 0) {
            Player* op = &gPlayers[otherIndex];
            if (PF(op, 0x950, s16) == 0) {
                if ((PF(op, 0x964, s16) & 4) == 0) {
                    PF(op, 0x864, f32) = 0.0f;
                    PF(op, 0x868, f32) = 0.0f;
                    PF(op, 0x86C, f32) = 0.0f;
                }
                PF(op, 0x864, f32) = dpos[0] + PF(op, 0x864, f32);
                PF(op, 0x868, f32) = dpos[1] + PF(op, 0x868, f32);
                PF(op, 0x86C, f32) = dpos[2] + PF(op, 0x86C, f32);
                op->hud_flags = (s16)(op->hud_flags | 4);
            }
            dpos[0] = to[0] - oldpos[0];
            dpos[2] = to[2] - oldpos[2];
            if (anim == 137) {
                hitKind = 1;
            }
            if (PF(p, 0x954, u16) > 60) {
                lbl_80344B24 = otherIndex;
                gPlayers[otherIndex].speak_timer = 1;
                p->speak_timer = 0;
                msgPost(50, gPlayers[lbl_80344B24].index,
                        (u32)&gPlayers[lbl_80344B24].col_pos);
                fn_8009DCB4((s32)&gPlayers[lbl_80344B24].col_pos);
            }
        }
    }
    if (PF(p, 0x954, u16) != 0) {
        p->speak_timer = (u16)(PF(p, 0x954, u16) + gFrameTicks);
    }

    oldpos[1] = (f32)((f64)oldpos[1] + lbl_80347BD0);
    wallResult = fn_80088714(radius, p, oldpos, dpos);
    if (wallResult != 0) {
        p->floor_name = (WorldObj*)lbl_80344B30;
        PlayerMotion_FloorFX(p, (WorldObj*)lbl_80344B30, oldpos,
                             (f32*)ctxbase);
        if (anim == 137) {
            hitKind = 2;
            reflection[0] = *(f32*)(ctxbase + 12);
            reflection[1] = 0.0f;
            reflection[2] = *(f32*)(ctxbase + 20);
        }
    } else {
        p->floor_name = NULL;
    }

    oldpos[1] = (f32)((f64)oldpos[1] - lbl_80347BD0);
    floorResult = PlayerCollideFloor((u8*)p, oldpos, dpos, wallResult,
                                     radius, height);
    if (lbl_80344B38 != 0) {
        hit[0] = oldpos[0];
        hit[1] = oldpos[1];
        hit[2] = oldpos[2];
        hit[1] = p->floor_base;
        PlayerMotion_FloorFX(p, (WorldObj*)lbl_80344B38, oldpos, hit);
        p->floor_cur = lbl_80344B34;
    } else {
        p->floor_cur = p->floor_base;
    }
    if (SV(p)->floor_obj != 0) {
        PlayerMotion_FloorFX(p, (WorldObj*)SV(p)->floor_obj, oldpos,
                             (f32*)(ctxbase + 72));
    }
    if ((floorResult >= 1) ||
        (floorResult >= 0 && (PF(p, 0x8C4, WorldObj*) == NULL ||
         (PF(p, 0x8C4, WorldObj*)->flags & 0x1000) == 0)) ||
        (floorResult == -2 && (PF(p, 0x8C4, u32) == 0 ||
         (p->obj_flags & 0x8000) != 0))) {
        f32 rise = p->floor_base - *(f32*)(motion + 0x34);
        if ((f64)rise < lbl_80347BD8 * gClockFrameStep) {
            rise = (f32)(lbl_80347BD8 * gClockFrameStep);
        }
        dpos[1] += rise;
    }
    if (floorResult != 1) {
        wallResult = 1;
    }

    if ((p->act_flags & 0x8000) == 0) {
        s32 cameraArg = wallResult != 0 ? 0 : 1;
        s32 camLimit;
        p->camera_limit = CameraLimitPlayerDpos(index, dpos, cameraArg);
        camLimit = p->camera_limit;
        if (!(camLimit < 5 || camLimit > 5) && anim == 137) {
            hitKind = 1;
        }
        if (wallResult != 0) {
            p->camera_limit = -camLimit;
        }
    } else {
        p->camera_limit = 0;
    }

    if ((f64)dpos[1] > lbl_80347BE0 * moveAmount) {
        f32 horizontal = fqdist(moveAmount, dpos[1]);
        if ((f64)horizontal > lbl_80347BE8) {
            f32 scale = moveAmount / horizontal;
            dpos[0] *= scale;
            dpos[2] *= scale;
        }
    }
    if (floorResult > 0) {
        floorBlocked = PlayerNewFloor((PMotionCtx*)(ctxbase + 24),
                                      p, dpos);
    }

    if (floorBlocked == 0) {
        s32* savedFloor = p->speech_req;
        s32 collision;
        to[0] = oldpos[0] + dpos[0];
        to[1] = oldpos[1] + dpos[1];
        to[2] = oldpos[2] + dpos[2];
        collision = PlayerCollideEnemies(
            p, (s32)oldpos, to, hit, 1, (s32*)&firstEnemy,
            (f32)(lbl_80347BF0 * radius), height);
        if (collision != 0) {
            collision = 1;
            if (**(s32**)firstEnemy != 7) {
                f32 dot =
                    (PF(firstEnemy, 0x5C, f32) - oldpos[2]) * dpos[2] +
                    (PF(firstEnemy, 0x54, f32) - oldpos[0]) * dpos[0];
                if (dot < 0.0f) {
                    collision = 0;
                }
            }
        }
        if (collision != 0) {
            if (directionKind == 0 && p->action == 0 &&
                p->camera_limit == 0) {
                PlayerCollideWalls(p, (s32)oldpos, dpos, to, hit);
            } else {
                dpos[0] = 0.0f;
                dpos[2] = 0.0f;
            }
            if (anim == 137 || anim == 143) {
                s32 sfx;
                PlayerMotion_HitTarget(p, firstEnemy, 32,
                    (f32)(anim == 137 ? lbl_80347BB8 : lbl_80347BC0));
                {
                    s32 sfxProbe = PlayerMotion_SfxIndex(p);
                    sfx = sfxProbe;
                    if (sfxProbe >= 0) {
                        PlyrSfxDoDamage((u8*)p->grab_partner, sfx, (u8*)p,
                                    (u8*)hit, lbl_80347B30, lbl_80347B40);
                    }
                }
                hitKind = 1;
            }
        }
        if (firstEnemyHits != 0) {
            p->speech_req = savedFloor;
        }
    }

    transporter = DoTransporter(p, oldpos, dpos, radius);
    DoExit(p);
    if (floorBlocked == 0 && transporter != 1) {
        s32 otherIndex;
        if (transporter != 0) {
            oldpos[0] = p->effectpos[0] + dpos[0];
            oldpos[1] = p->effectpos[1] + dpos[1];
            oldpos[2] = p->effectpos[2] + dpos[2];
            to[0] = oldpos[0];
            to[1] = oldpos[1];
            to[2] = oldpos[2];
        } else {
            to[0] = oldpos[0] + dpos[0];
            to[1] = oldpos[1] + dpos[1];
            to[2] = oldpos[2] + dpos[2];
        }
        otherIndex = PlayerCollidePlayers(
            p, (f32)(lbl_80347BF0 * radius), height, oldpos, to, hit, 1);
        if (otherIndex >= 0) {
            if (transporter != 0) {
                dpos[0] = 0.0f;
                dpos[1] = 0.0f;
                dpos[2] = 0.0f;
            } else if (anim == 137) {
                if (sMusicFadeBase >
                    (f32)(lbl_80347B88 + p->combo_fade)) {
                    hitKind = 1;
                    PlayerCollideWalls(p, (s32)oldpos, dpos, to, hit);
                }
            } else {
                PlayerCollideWalls(p, (s32)oldpos, dpos, to, hit);
            }
            transporter = 0;
        }
    }

    if (floorBlocked == 0 && transporter != 1) {
        f32 itemRadius = radius;
        s32 critterIndex;
        s32 specialCritter;
        if (anim == 137) {
            itemRadius = lbl_80347B14;
        } else if (anim == 143) {
            itemRadius = lbl_80347BF8;
        }
        to[0] = oldpos[0] + dpos[0];
        to[1] = oldpos[1] + dpos[1];
        to[2] = oldpos[2] + dpos[2];
        item = PlayerCollideItems(p, itemRadius, height, oldpos, to, hit);
        if (item >= 0) {
            specialCritter = 0;
            if (item >= 0x10000) {
                u8* critter;
                critterIndex = item & 0xFFFF;
                critter = gCritterPool + critterIndex * 2784;
                if (*(s16*)(*(u8**)(*(u8**)(critter + 4) + 0x120) +
                            0x20) == 4) {
                    specialCritter = 1;
                }
            } else {
                critterIndex = -1;
            }
            if (transporter != 0) {
                PlayerMotion_DamageTarget(p, item, 0, 0, 0,
                                          lbl_80347B20, lbl_80347B30);
            } else if (anim == 8 && specialCritter == 0 &&
                       critterIndex < 0) {
                dpos[0] = (f32)(dpos[0] * lbl_80347C00);
                dpos[2] = (f32)(dpos[2] * lbl_80347C00);
            } else if (anim == 137 || anim == 143) {
                s32 sfx;
                s32 damaged = PlayerMotion_DamageTarget(
                    p, item, 32, (s32)hit, 0,
                    (f32)(anim == 137 ? lbl_80347BB8
                                  : lbl_80347C08),
                    lbl_80347B40);
                if (damaged >= 0) {
                    {
                        s32 sfxProbe = PlayerMotion_SfxIndex(p);
                        sfx = sfxProbe;
                        if (sfxProbe >= 0) {
                            PlyrSfxDoDamage((u8*)p->grab_partner, sfx,
                                        (u8*)p, (u8*)to,
                                        lbl_80347B30, lbl_80347B40);
                        }
                    }
                }
                if (specialCritter != 0 || critterIndex >= 0) {
                    dpos[0] = 0.0f;
                    dpos[2] = 0.0f;
                    hitKind = 1;
                }
            } else if (ctl->control.flag != 0 || wallResult != 0) {
                dpos[0] = 0.0f;
                dpos[2] = 0.0f;
            } else {
                PlayerCollideWalls(p, (s32)oldpos, dpos, to, hit);
            }
        }
    } else {
        item = -1;
    }

    if (transporter == 2) {
        p->floor_base = gFloorCollisionResult[13];
        if (lbl_80344B38 != 0) {
            p->floor_cur = lbl_80344B34;
        } else {
            p->floor_cur = SV(p)->floor_y;
        }
    }
    goto collision_done;

detach_floor:
    SV(p)->floor_obj = 0;
    MBNodeSetParent(p->node, lbl_80344B2C);
    p->hud_flags |= 1;
    p->floor_base = lbl_80344880;
    p->floor_cur = lbl_80344880;

collision_done:
    *(s32*)&p->coll_flags = 0;

    {
        f32 savedHeading;
        f32 targetAngle;
        f32 targetDistance;
        f32 contactRadius;
        f32 movingBias;
        s32 closeTarget;
        s32 reaction;
        s32 motionState = motionType;
        s32 critterIndex;
        s32 forceState;
        u8* enemy;

        if ((p->hud_flags & 0x20) != 0 &&
            (p->obj_flags & 0x4000) == 0) {
            contactRadius = 0.0f;
            movingBias = 0.0f;
            reaction = 0;
            targetDistance = lbl_80347C10;
            forceState = 0;
            motionState = 0;
            goto store_motion_state;
        }
        targetAngle = heading;
        targetDistance = lbl_80347C20;

        savedHeading = heading;
        {
            f64 decay = lbl_80347C18;
            p->vel[0] = (f32)(decay * p->vel[0]);
            p->vel[1] = (f32)(decay * p->vel[1]);
            p->vel[2] = (f32)(decay * p->vel[2]);
        }
        reaction = PlayerKnockback(controlYaw, p, &savedHeading);

        if (reaction < 300) {
            if (anim == 131) {
                reaction = 20;
            } else if (anim == 133) {
                reaction = 11;
            } else if (reaction < 10 && anim == 130) {
                reaction = 10;
            } else if (sMusicFadeBase < p->field_898 &&
                       motionType == 1) {
                reaction = 100;
            } else if (reaction < 1 && anim == 129) {
                reaction = 3;
            } else if (reaction < 1 && anim == 127) {
                reaction = 2;
            }
        }

        contactRadius = radius;
        if (directionKind != 0) {
            movingBias = 1.0f;
        } else {
            movingBias = 0.0f;
        }
        forceState = 0;
        to[0] = oldpos[0] + dpos[0];
        to[1] = oldpos[1] + dpos[1];
        to[2] = oldpos[2] + dpos[2];
        if (reaction >= 2) {
            attackDir[0] = *(f32*)(motion + 0x20);
            attackDir[1] = *(f32*)(motion + 0x24);
            attackDir[2] = *(f32*)(motion + 0x28);
        }

        if (reaction == 300) {
            motionState = 41;
            goto state_selected;
        }
        if (reaction == 301) {
            motionState = 35;
            goto state_selected;
        }
        if (reaction == 200) {
            if (motionType != 7) {
                motionState = 33;
            }
            goto state_selected;
        }
        if (reaction == 100) {
            motionState = 32;
            dpos[0] = 0.0f;
            dpos[1] = 0.0f;
            dpos[2] = 0.0f;
            goto state_selected;
        }
        if (reaction == 30 || reaction == 31) {
            heading = savedHeading;
            motionState = 37;
            goto state_selected;
        }
        if (reaction == 21) {
            heading = savedHeading;
            motionState = 35;
            goto state_selected;
        }
        if (reaction == 20) {
            heading = savedHeading;
            motionState = 36;
            goto state_selected;
        }
        if ((u32)(reaction - 10) <= 1) {
            heading = savedHeading;
            motionState = 34;
            goto state_selected;
        }
        if ((u32)(reaction - 2) <= 1) {
            dpos[0] = 0.0f;
            dpos[1] = 0.0f;
            dpos[2] = 0.0f;
            if (anim != 127 && anim != 129) {
                motionState = 31;
            } else {
                motionState = 1;
                reaction = 0;
            }
            goto state_selected;
        }

        targetDir[0] = sin(heading);
        targetDir[1] = 0.0f;
        targetDir[2] = cos(heading);
        target = p->collision_item;
        targetDistance = PlayerGetTarget(p, to, targetDir, attackDir,
                                         &item, &target);
        if (item >= 0x10000) {
            critterIndex = item & 0xFFFF;
            enemy = NULL;
        } else if (item >= 0) {
            enemy = gEnemies + item * 916;
            critterIndex = -1;
        } else {
            critterIndex = -1;
            enemy = NULL;
        }

        if ((p->hud_flags & 0xC000) != 0 ||
            (ctl->pad.levels & 0x5000) != 0 ||
            (ctl->pad.unk34 != 0 &&
             (f64)ctl->values[10] > lbl_80347B08)) {
            attackDir[0] = targetDir[0];
            attackDir[2] = targetDir[2];
        }
        targetAngle = atan2(attackDir[0], attackDir[2]);
        p->melee_yaw = targetAngle - controlYaw;
        p->melee_yaw =
            PlayerMotion_WrapAngle(p->melee_yaw);

        if (critterIndex >= 0) {
            u8* critter = gCritterPool + critterIndex * 2784;
            p->coll_flags |= 0x10;
            if (*(s16*)(*(u8**)(*(u8**)(critter + 4) + 0x120) + 0x20) ==
                4) {
                specialCritter = 1;
            }
        } else if (enemy != NULL) {
            p->coll_flags |= 0x10;
            if (((f64)PF(enemy, 0x23C, f32) <= lbl_80347C28 ||
                 (f64)lbl_803447D8 < lbl_80347BD0) &&
                (f64)targetDistance < lbl_80347C28 + radius) {
                p->coll_flags |= 2;
            }
        } else if (target != NULL) {
            p->coll_flags |= 0x20;
            if ((f64)*(f32*)(*(u8**)target + 0x10) <= lbl_80347C30 &&
                (f64)targetDistance < lbl_80347C28 + radius) {
                p->coll_flags |= 2;
            }
        }

        if ((gControllerButtons & 0x80) != 0) {
            p->coll_flags |= 2;
        }

        closeTarget = p->coll_flags & 2;
        if ((p->shield_flags & 0x80000) != 0 && enemy != NULL &&
            PF(enemy, 0, s32) == 30 &&
            (f64)fabsf_(p->melee_yaw) < lbl_80347C38) {
            heading = targetAngle;
            motionState = 1;
            damage_enemy(enemy, index, 0, 0, NULL, 1,
                         lbl_80347B40);
            if (p->speak_done == 0) {
                fn_8009F158(index);
                p->speak_done = 1;
            }
            AudioPlayEvt101IfIdle((s32)(enemy + 0x54));
            PF(p, 0x128, u32) |= 1;
            if (PF(enemy, 0x206, s16) == 2) {
                PF(p, 0x128, u32) |= 2;
            }
            p->speak_kind = 2;
            goto state_selected;
        }

        if ((f64)targetDistance < lbl_80347BD0 + radius) {
            if ((p->shield_flags & 0x200000) != 0 && item >= 0) {
                if (motionState == 1) {
                    motionState = 8;
                }
                if (motionState == 8 || motionState == 13) {
                    PlayerMotion_DamageTarget(p, item, 1, (s32)to, 0,
                                              lbl_80347BF8,
                                              lbl_80347B30);
                    goto state_selected;
                }
            }

            if ((p->shield_flags & 0x400000) != 0 && item >= 0) {
                s32 damaged = PlayerMotion_DamageTarget(
                    p, item, 34, (s32)to, 0, lbl_80347C40,
                    lbl_80347B40);
                if (damaged >= 0) {
                    s32 effect = StartFXNoLoop(55, NULL);
                    u8* effectRecord;
                    void* effectNode;
                    if (critterIndex >= 0) {
                        u8* critter =
                            gCritterPool + critterIndex * 2784;
                        hit[0] = PF(critter, 0x3C, f32) -
                                 *(f32*)(motion + 0x30);
                        hit[1] = PF(critter, 0x40, f32) -
                                 *(f32*)(motion + 0x34);
                        hit[2] = PF(critter, 0x44, f32) -
                                 *(f32*)(motion + 0x38);
                    } else if (enemy != NULL) {
                        hit[0] = PF(enemy, 0x34, f32) -
                                 *(f32*)(motion + 0x30);
                        hit[1] = PF(enemy, 0x38, f32) -
                                 *(f32*)(motion + 0x34);
                        hit[2] = PF(enemy, 0x3C, f32) -
                                 *(f32*)(motion + 0x38);
                    } else {
                        hit[0] = to[0] - *(f32*)(motion + 0x30);
                        hit[1] = to[1] - *(f32*)(motion + 0x34);
                        hit[2] = to[2] - *(f32*)(motion + 0x38);
                    }
                    effectRecord = Effects + effect * 240;
                    CreateDirMatrix(
                        (f32*)*(void**)(effectRecord += 0x14),
                        hit, NULL);
                    GetWorldMat(p->mbnode2, effectMatrix, NULL);
                    PF(*(void**)effectRecord, 0x30, f32) =
                        effectMatrix[12];
                    PF(*(void**)effectRecord, 0x34, f32) =
                        effectMatrix[13];
                    PF(*(void**)effectRecord, 0x38, f32) =
                        effectMatrix[14];
                    goto state_selected;
                }
            }

            if (anim == 8 && item >= 0 && specialCritter == 0) {
                PlayerMotion_DamageTarget(p, item, 32, (s32)to, 0,
                                          lbl_80347BF8,
                                          lbl_80347B40);
                goto state_selected;
            }

            if ((motionState == 8 || motionState == 13) &&
                p->field_8F8 == 0 &&
                (anim < 39 || anim > 114) && ctl->control.flag != 0) {
                if ((item >= 0 &&
                     (specialCritter == 0 || gBossType == 37 ||
                      gBossType == 41)) ||
                    (target != NULL && **(s32**)target == 3 &&
                     PF(target, 0xCF, s8) >= 0)) {
                    contactRadius = 0.0f;
                    movingBias = 0.0f;
                    motionState = 15;
                    forceState = 1;
                }
            }
        }

state_selected:
        if (motionState < 31) {
            if (p->speak_kind == 2) {
                p->speak_kind = 0;
                motionState = 27;
            } else if (p->speak_kind == 3) {
                p->speak_kind = 0;
                motionState = 32;
            } else if (p->speak_kind == 1 && lbl_803448B8 == 0) {
                p->speak_kind = 0;
                motionState = 14;
            }
        }

        if (p->action < 11) {
            if (anim == 8) {
                p->power_target =
                    (f32)(p->power_target -
                          lbl_80347C08 * gClockFrameStep);
                if ((f64)p->power_target < lbl_80347B08) {
                    p->power_target = 0.0f;
                }
            } else if (anim < 120 || anim >= 8) {
                if ((f64)p->power_target < lbl_80347C48) {
                    if ((gControllerButtons & 0x10) != 0) {
                        p->power_target =
                            (f32)(p->power_target +
                                  lbl_80347BB0 * gClockFrameStep);
                    } else {
                        p->power_target =
                            (f32)(p->power_target +
                                  lbl_80347C28 * gClockFrameStep);
                    }
                }
                if ((f64)p->power_target >= lbl_80347C48) {
                    if (lbl_80344740 >= 15) {
                        msgPost(110, index, (u32)&p->col_pos);
                    }
                    p->power_target = lbl_80347C50;
                } else if ((f64)p->power_target >= lbl_80347B90 &&
                           lbl_8034476C > 1 && lbl_80344740 >= 15) {
                    msgPost(111, index, (u32)&p->col_pos);
                }
            }
        }

        if (motionState == 8) {
            p->timer_89C += gClockFrameStep;
        } else if (motionState == 13) {
            if ((f64)p->timer_89C < lbl_80347BE0 &&
                gBossType < 0) {
                motionState = 8;
            }
            p->timer_89C += gClockFrameStep;
        } else {
            p->timer_89C = 0.0f;
        }

        if (gBossType >= 0 && gBoss398 >= 0 && good_wiz_state <= 1 &&
            gBossDead != 0 && (p->hud_flags & 0x800) == 0) {
            motionState = 28;
        }

store_motion_state:
        p->vibe_on = motionState;

        if (p->quest_state >= 2) {
            motionState = 1;
        }
        if (motionState == 21) {
            movingBias = 0.0f;
        }
        if ((gControllerButtons & 0x40) != 0) {
            if (movement != 0.0f) {
                targetDistance =
                    (f32)(lbl_80347BD0 + contactRadius + movingBias);
            } else {
                targetDistance = 0.0f;
            }
        }
        if ((f64)targetDistance <
            lbl_80347BD0 + contactRadius + movingBias) {
            p->coll_flags |= 1;
        } else if ((f64)targetDistance <
                   lbl_80347C28 + contactRadius + movingBias) {
            p->coll_flags |= 4;
        } else {
            p->coll_flags |= 8;
        }

        {
            s32 forcedAnim;
            if (forceState == 0 &&
                (p->flags & 0x1000) != 0) {
                forcedAnim = 110;
            } else if (forceState == 0 &&
                       (p->flags & 0x2000) != 0) {
                forcedAnim = 110;
            } else if (forceState == 0 &&
                       (p->flags & 0x8000) != 0) {
                forcedAnim = 103;
            } else if (forceState == 0 &&
                       (p->flags & 0x4000) != 0) {
                forcedAnim = 104;
            } else if (forceState == 0 &&
                       (p->field_11C & 0x100000) != 0) {
                forcedAnim = 107;
            } else if (forceState == 0 &&
                       (p->field_11C & 0x10000000) != 0) {
                forcedAnim = 112;
            } else if (forceState == 0 &&
                       (p->flags & 0x70) != 0) {
                forcedAnim = 110;
            } else {
                forcedAnim = 0;
            }

            switch (motionState) {
            case 28:
                p->anim_20C = 123;
                break;
            case 41:
                p->anim_20C = 148;
                break;
            case 33:
                p->anim_20C = 128;
                break;
            case 14:
                p->anim_20C = 28;
                break;
            case 27:
                p->anim_20C = 29;
                break;
            case 7:
                p->anim_20C = 8;
                break;
            case 2:
                p->anim_20C = 119;
                break;
            case 3:
                p->anim_20C = 4;
                break;
            case 4:
                p->anim_20C = 5;
                break;
            case 5:
                p->anim_20C = 6;
                break;
            case 6:
                p->anim_20C = 7;
                break;
            case 9:
                p->anim_20C = 9;
                break;
            case 10:
                p->anim_20C = 11;
                break;
            case 11:
                p->anim_20C = 13;
                break;
            case 12:
                p->anim_20C = 15;
                break;
            case 17:
                if (forcedAnim != 0) {
                    p->anim_20C = forcedAnim;
                } else {
                    p->anim_20C = 71;
                }
                break;
            case 18:
                if (forcedAnim != 0) {
                    p->anim_20C = forcedAnim;
                } else {
                    p->anim_20C = 73;
                }
                break;
            case 19:
                if (forcedAnim != 0) {
                    p->anim_20C = forcedAnim;
                } else {
                    p->anim_20C = 75;
                }
                break;
            case 20:
                if (forcedAnim != 0) {
                    p->anim_20C = forcedAnim;
                } else {
                    p->anim_20C = 77;
                }
                break;
            case 38:
                if (p->grab_partner == NULL) {
                    p->anim_20C = 0;
                } else {
                    switch (p->grab_partner->char_type) {
                    case 0: p->anim_20C = 136; break;
                    case 1: p->anim_20C = 139; break;
                    case 2: p->anim_20C = 140; break;
                    case 3: p->anim_20C = 141; break;
                    case 4: p->anim_20C = 142; break;
                    case 5: p->anim_20C = 145; break;
                    case 6: p->anim_20C = 146; break;
                    case 7: p->anim_20C = 147; break;
                    default: p->anim_20C = 0; break;
                    }
                }
                break;
            case 39:
                if (p->grab_partner == NULL) {
                    p->anim_20C = 0;
                } else {
                    switch (p->grab_partner->char_type) {
                    default: p->anim_20C = 0; break;
                    case 0: p->anim_20C = 137; break;
                    case 4: p->anim_20C = 143; break;
                    }
                }
                break;
            case 22:
                p->anim_20C = 88;
                p->coll_score = lbl_80347C54;
                break;
            case 23:
                p->anim_20C = 89;
                break;
            case 21:
                if ((p->flags & 0x400) != 0 &&
                    (f64)p->power_target >= lbl_80347B90) {
                    p->coll_score = lbl_80347C58;
                    p->anim_20C = 110;
                } else if ((f64)p->power_target >= lbl_80347C48) {
                    p->anim_20C = 87;
                    p->coll_score = lbl_80347C50;
                } else if ((f64)p->power_target >= lbl_80347B90) {
                    p->anim_20C = 86;
                    p->coll_score = lbl_80347C58;
                }
                break;
            case 16:
                if (forcedAnim != 0) {
                    p->anim_20C = forcedAnim;
                } else if (forceState == 0 &&
                           (p->field_11C & 0x480000) != 0) {
                    p->anim_20C = 92;
                } else if (p->field_908 != 0 && movement != 0.0f &&
                           (p->coll_flags & 5) != 0) {
                    p->anim_20C = 32;
                } else if ((p->coll_flags & 4) != 0 &&
                           !closeTarget && movement != 0.0f) {
                    p->melee_yaw = heading - controlYaw;
                    p->melee_yaw =
                        PlayerMotion_WrapAngle(p->melee_yaw);
                    p->anim_20C = 62;
                } else if ((p->coll_flags & 1) != 0) {
                    if (closeTarget) {
                        p->anim_20C = 79;
                    } else {
                        p->anim_20C = 32;
                    }
                } else if (forceState != 0) {
                    if (closeTarget) {
                        p->anim_20C = 79;
                    } else {
                        p->anim_20C = 32;
                    }
                } else {
                    p->anim_20C = 99;
                }
                break;
            case 15:
                if (forcedAnim != 0) {
                    p->anim_20C = forcedAnim;
                } else if ((p->coll_flags & 4) != 0 &&
                           !closeTarget && movement != 0.0f) {
                    p->melee_yaw = heading - controlYaw;
                    p->melee_yaw =
                        PlayerMotion_WrapAngle(p->melee_yaw);
                    p->anim_20C = 62;
                } else if ((p->coll_flags & 1) != 0) {
                    if (closeTarget) {
                        p->anim_20C = 82;
                    } else {
                        p->anim_20C = 39;
                    }
                } else if (forceState != 0) {
                    if (closeTarget) {
                        p->anim_20C = 82;
                    } else {
                        p->anim_20C = 39;
                    }
                } else {
                    p->anim_20C = 92;
                }
                break;
            case 24:
            case 25:
            case 26:
                if (p->item_body_hi != 0 ||
                    (gGameOptions[1] & 2) != 0) {
                    if (motionState == 25) {
                        p->anim_20C = 117;
                    } else if (motionState == 26) {
                        p->anim_20C = 115;
                        p->field_956 |= 2;
                    } else {
                        p->anim_20C = 115;
                    }
                    break;
                }
                msgPost(6, index, (u32)&p->col_pos);
            case 1:
            case 8:
            case 13:
                if ((f64)ctl->values[8] > lbl_80347C00) {
                    motionState = 13;
                } else if ((f64)ctl->values[8] > lbl_80347B08) {
                    motionState = 8;
                } else {
                    motionState = 1;
                }
                if ((f64)fqdist(dpos[0], dpos[2]) > lbl_80347BE8 &&
                    (p->hud_flags & 8) != 0) {
                    p->anim_20C = 26;
                    heading = atan2(dpos[0], dpos[2]);
                } else if (reaction == 3) {
                    p->anim_20C = 129;
                    heading = savedHeading;
                } else if (reaction != 0) {
                    p->anim_20C = 27;
                    heading = savedHeading;
                } else if (motionState == 13) {
                    p->anim_20C = 19;
                } else if (motionState == 8) {
                    p->anim_20C = 17;
                } else {
                    p->anim_20C = 0;
                }
                break;
            case 31:
                if (reaction == 3) {
                    p->anim_20C = 129;
                } else {
                    p->anim_20C = 127;
                }
                break;
            case 32:
                p->anim_20C = 122;
                break;
            case 34:
                p->anim_20C = 130;
                heading = savedHeading;
                break;
            case 35:
                p->anim_20C = 131;
                heading = savedHeading;
                break;
            case 36:
                p->anim_20C = 133;
                heading = savedHeading;
                break;
            case 37:
                p->anim_20C = 135;
                heading = savedHeading;
                break;
            default:
                p->anim_20C = 0;
                break;
            }
        }
        /* Target +0x2624: boss intro state and the action update. */
        if (p->quest_state != 0 && gBossType >= 0) {
            if (lbl_80344894 < 0 && p->quest_state < 4) {
                switch (gBossType) {
                case 34:
                case 35:
                case 36:
                case 37:
                case 38:
                case 39:
                    lbl_80344894 = StartFXSub(92, NULL, 0, 0x800,
                                              lbl_80347C10);
                    if (!(lbl_80344894 < 0)) {
                        MBTreeSetAmbientAdd(
                            *((void**)(Effects + 0x14) +
                              lbl_80344894 * 60),
                            0x1FF, 1);
                        SfxSetParent(lbl_80344894,
                                     p->hand_node);
                        p->shield_object =
                            *((void**)(Effects + 0x14) +
                              lbl_80344894 * 60);
                    }
                    break;
                default: {
                    f32 bossFxPos[3];
                    bossFxPos[0] = 0.0f;
                    bossFxPos[1] = lbl_80347C5C;
                    bossFxPos[2] = 0.0f;
                    lbl_80344894 = StartFXSub(92, bossFxPos, 0, 0x880,
                                              lbl_80347C10);
                    if (!(lbl_80344894 < 0)) {
                        MBTreeSetAmbientAdd(
                            *(void**)(Effects + lbl_80344894 * 240 + 0x14),
                            0x1FF, 1);
                        SfxSetParent(lbl_80344894,
                                     p->node);
                    }
                    break;
                }
                }
            }

            if (p->quest_state == 2) {
                s32 combo = StartComboFX(p->col_pos,
                                         PF(p, 4, s32), PF(p, 4, s32));
                if (combo >= 0) {
                    PF(Effects + combo * 240, 0x44, f32) =
                        lbl_80347C60;
                }
                p->quest_state = 3;
                p->timer_1FA = 60;
                fn_8009C9DC(0, p->col_pos);
            }
            if (p->quest_state == 3 && p->timer_1FA <= 0) {
                p->quest_state = 4;
                fn_8009C9DC(1, p->col_pos);
            }
            if (p->quest_state >= 4 &&
                (p->act_bits & ~1U) == 0) {
#pragma opt_common_subs off
                switch (gBossType) {
                case 36:
                case 37:
                    if (p->anim_208 != 107) {
                        p->anim_208 = 0;
                        p->anim_20C = 107;
                    }
                    break;
                case 34:
                case 35:
                case 38:
                case 39:
                    if (p->anim_208 != 99 &&
                        p->anim_208 != 100) {
                        p->anim_208 = 0;
                        p->anim_20C = 99;
                    }
                    break;
                default:
                    if (p->anim_208 != 115 &&
                        p->anim_208 != 116) {
                        p->anim_208 = 0;
                        p->anim_20C = 115;
                    }
                    break;
                }
            }
        }
#pragma opt_common_subs reset

        DoPlayerAction(p);

        /* Target +0x2870: completed-boss effects and damage table. */
        if (p->quest_state >= 4) {
            if (p->floor_cur > p->floor_base) {
                PF(p->mbnode, 0x30, f32) = *(f32*)(motion + 0x30);
                PF(p->mbnode, 0x34, f32) = *(f32*)(motion + 0x34);
                PF(p->mbnode, 0x38, f32) = *(f32*)(motion + 0x38);
                PF(p->mbnode, 0x34, f32) =
                    (f32)(lbl_80347BE0 + p->floor_cur);
                MBTreeSetAltTex(p->mbnode, -2,
                                lbl_80344BE8, 1);
                MBTreeClearFlags(p->mbnode, 2, 0);
            } else {
                PF(p->mbnode, 0x30, f32) = *(f32*)(motion + 0x30);
                PF(p->mbnode, 0x34, f32) = *(f32*)(motion + 0x34);
                PF(p->mbnode, 0x38, f32) = *(f32*)(motion + 0x38);
                PF(p->mbnode, 0x34, f32) =
                    (f32)(lbl_80347BE0 + p->floor_base);
                MBTreeSetAltTex(p->mbnode, -1, 0, 1);
                MBTreeClearFlags(p->mbnode, 2, 0);
            }

            if ((p->act_bits & ~1U) != 0) {
                f32 bossDamage = 0.0f;
                f32 damageScale = 0.0f;
                f32 weight = 0.0f;
                f32 effectRadius = lbl_80347B98;
                s32 effect = -1;
                s32 particleTexture = -1;
                u32 effectFlags = 0;
                void* hitNode = NULL;

                switch (gBossType) {
                case 34:
                case 35:
                case 36:
                case 37:
                case 38:
                case 39:
                case 41:
                    if (lbl_80344894 >= 0) {
                        lbl_80344894 = DeleteEffect(lbl_80344894, 1);
                        p->shield_object = 0;
                    }
                    break;
                default:
                    break;
                }
                p->quest_state = 0;
                p->act_bits = 0;
                fn_8009C9DC(2, p->col_pos);

                bossColor[0] = 0.0f;
                bossColor[1] = 0.0f;
                bossColor[2] = 0.0f;

                switch (gBossType) {
                case 35: {
                    u8* chain1 = PF(gBossObj, 0xAD8, u8*);
                    u8* chain2 = PF(chain1, 0xAD8, u8*);
                    hitNode = PF(chain2, 0xC8, void*);
                    while (PF(hitNode, 0x78, void*) != NULL) {
                        hitNode = PF(hitNode, 0x78, void*);
                    }
                    bossColor[0] = lbl_80347C64;
                    bossColor[1] = 0.0f;
                    bossColor[2] = lbl_80347C68;
                    bossDamage = lbl_80347C40;
                    damageScale =
                        (f32)(lbl_80347B88 * PF(chain2, 0x4B0, f32));
                    particleTexture =
                        MBOX_FindTexture(strings + 56, NULL);
                    break;
                }
                case 38:
                    hitNode = PF(gBossObj, 0xCC, void*);
                    effectFlags = 0x20000;
                    bossDamage = lbl_80347C40;
                    damageScale =
                        (f32)(lbl_80347BE0 * PF(gBossObj, 0x4B0, f32));
                    particleTexture =
                        MBOX_FindTexture(strings + 72, NULL);
                    break;
                case 34:
                    hitNode = PF(gBossObj, 0xC0, void*);
                    bossColor[0] = lbl_80347C64;
                    bossColor[1] = 0.0f;
                    bossColor[2] = lbl_80347C68;
                    bossDamage = lbl_80347C40;
                    damageScale =
                        (f32)(lbl_80347BE0 * PF(gBossObj, 0x4B0, f32));
                    particleTexture =
                        MBOX_FindTexture(strings + 72, NULL);
                    break;
                case 36:
                    weight = 5.0f;
                    hit[0] = PF(gBossObj, 0x4C, f32);
                    effectRadius = weight;
                    hit[1] = PF(gBossObj, 0x50, f32);
                    bossDamage = lbl_80347C40;
                    damageScale =
                        (f32)(lbl_80347BE0 * PF(gBossObj, 0x4B0, f32));
                    break;
                case 41:
                    hit[0] = PF(gBossObj, 0x418, f32);
                    hit[1] = PF(gBossObj, 0x41C, f32);
                    hit[2] = PF(gBossObj, 0x420, f32);
                    effect = StartFXSub(93, hit, 0, 0x1000000,
                                        0.0f);
                    if (effect >= 0) {
                        SfxSetMorph(5.0f, effect, 90, 0);
                    }
                    damageScale =
                        (f32)(lbl_80347B58 * PF(gBossObj, 0x4B0, f32));
                    CritterDamage(gBossObj, p->index, 0, 0, NULL, 0,
                                  damageScale);
                    break;
                case 39:
                    hit[0] = PF(gBossObj, 0x418, f32);
                    hit[1] = PF(gBossObj, 0x41C, f32);
                    hit[2] = PF(gBossObj, 0x420, f32);
                    hit[2] = (f32)(hit[2] + lbl_80347C70);
                    hit[1] = (f32)(hit[1] - lbl_80347C78);
                    hit[0] = (f32)(hit[0] - lbl_80347C80);
                    effect = StartFXSub(90, hit, 0, 0x80000,
                                        0.0f);
                    if (effect >= 0) {
                        SfxSetMorph(lbl_80347C88, effect, 91, 0);
                        MBTreeSetAmbientAdd(
                            *(void**)(Effects + effect * 240 + 0x14),
                            0x1FF, 1);
                        lbl_80344890 = effect;
                    }
                    damageScale =
                        (f32)(lbl_80347BE0 * PF(gBossObj, 0x4B0, f32));
                    CritterDamage(gBossObj, p->index, 0, 0, NULL, 0,
                                  damageScale);
                    PF(gBossObj, 0xAC8, f32) = lbl_80347B10;
                    hit[0] = PF(gBossObj, 0x418, f32);
                    hit[1] = PF(gBossObj, 0x41C, f32);
                    hit[2] = PF(gBossObj, 0x420, f32);
                    hit[1] = (f32)(hit[1] + lbl_80347BC0);
                    bossDamage = lbl_80347C40;
                    weight = lbl_80347C8C;
                    break;
                case 40:
                    hit[0] = PF(gBossObj, 0x418, f32);
                    hit[1] = PF(gBossObj, 0x41C, f32);
                    hit[2] = PF(gBossObj, 0x420, f32);
                    hit[2] = (f32)(hit[2] + lbl_80347BC0);
                    hit[1] = (f32)(hit[1] - lbl_80347C78);
                    effect = StartFXSub(93, hit, 0, 0x880,
                                        0.0f);
                    if (effect >= 0) {
                        SfxSetMorph(lbl_80347C88, effect, 90, 0);
                        lbl_80344890 = effect;
                    }
                    damageScale =
                        (f32)(lbl_80347BE0 * PF(gBossObj, 0x4B0, f32));
                    CritterDamage(gBossObj, p->index, 0, 0, NULL, 0,
                                  lbl_80347C90);
                    PF(gBossObj, 0xAC8, f32) = lbl_80347C94;
                    break;
                case 42:
                    hit[0] = PF(gBossObj, 0x418, f32);
                    hit[1] = PF(gBossObj, 0x41C, f32);
                    hit[2] = PF(gBossObj, 0x420, f32);
                    hit[2] = (f32)(hit[2] + lbl_80347B28);
                    hit[1] = (f32)(hit[1] + lbl_80347C98);
                    effect = StartFXSub(93, hit, 0, 0x880,
                                        0.0f);
                    if (effect >= 0) {
                        SfxSetMorph(lbl_80347C88, effect, 90, 0);
                        lbl_80344890 = effect;
                    }
                    damageScale =
                        (f32)(lbl_80347BE0 * PF(gBossObj, 0x4B0, f32));
                    CritterDamage(gBossObj, p->index, 0, 0, NULL, 0,
                                  damageScale);
                    PF(gBossObj, 0xAC8, f32) = lbl_80347CA0;
                    break;
                case 37:
                    hit[0] = 0.0f;
                    hit[1] = 0.0f;
                    hit[2] = 5.0f;
                    effect = StartFXSub(93, hit, 0, 0x8000880,
                                        0.0f);
                    if (effect >= 0) {
                        SfxSetParent(effect, p->node);
                        SfxSetMorph(lbl_80347BF8, effect, 90, 0);
                    }
                    MBTreeSetColor(PF(gBossObj, 0x6C, void*), 0xFF40FF40, 1);
                    MBTreeSetFlags(PF(gBossObj, 0x6C, void*), 8, 1);
                    PF(PF(gBossObj, 0x6C, void*), 0x40, f32) =
                        0.800000011920929f;
                    PF(PF(gBossObj, 0x6C, void*), 0x44, f32) =
                        0.800000011920929f;
                    PF(PF(gBossObj, 0x6C, void*), 0x48, f32) =
                        0.800000011920929f;
                    damageScale =
                        (f32)(lbl_80347BE0 * PF(gBossObj, 0x4B0, f32));
                    CritterDamage(gBossObj, p->index, 0, 0, NULL, 0,
                                  damageScale);
                    PF(gBossObj, 0xAC8, f32) = lbl_80347B10;
                    break;
                }

                if ((f64)bossDamage > lbl_80347B08) {
                    if (hitNode == NULL) {
                        effectVelocity[0] = hit[0] - p->col_pos[0];
                        effectVelocity[1] = hit[1] - p->col_pos[1];
                        effectVelocity[2] = hit[2] - p->col_pos[2];
                        NormalVector(effectVelocity);
                        hit[0] = (f32)((f64)effectVelocity[0] + p->col_pos[0]);
                        hit[1] = (f32)((f64)effectVelocity[1] + p->col_pos[1]);
                        hit[2] = (f32)((f64)effectVelocity[2] + p->col_pos[2]);
                        hit[1] = (f32)(hit[1] + lbl_80347C28);
                        effect = StartFXSub(93, hit, effectFlags | 8, 0,
                                            lbl_80347CA8);
                        effectVelocity[0] *= bossDamage;
                        effectVelocity[1] *= bossDamage;
                        effectVelocity[2] *= bossDamage;
                        SfxSetPhysics(effect, effectVelocity, bossColor,
                                    weight, effectRadius);
                    } else {
                        hit[0] = (f32)(p->mat[8] + (f64)p->col_pos[0]);
                        hit[1] = (f32)(p->mat[9] + (f64)p->col_pos[1]);
                        hit[2] = (f32)(p->mat[10] + (f64)p->col_pos[2]);
                        hit[1] = (f32)(hit[1] + lbl_80347C28);
                        effect = StartFXSub(93, hit, effectFlags | 8, 0,
                                            lbl_80347CA8);
                        SfxSetPhysics(effect, NULL, bossColor, 0.0f,
                                    effectRadius);
                        SfxSetHitTarget(bossDamage, effect, hitNode);
                    }
                    SfxSetDamage(damageScale, 0.0f, 0.0f, effect, 0,
                                 p->index);
                }

                if (effect >= 0) {
                    u8* fxRecord = Effects + effect * 240;
                    MBTreeSetAmbientAdd(
                        *(void**)(fxRecord += 0x14), 0x1FF, 1);
                    if (particleTexture >= 0) {
                        void* psys = MBNewPsysDefault(
                            (f32*)gIdentityMatrix,
                            *(void**)fxRecord, 0, 1);
                        if (psys != NULL) {
                            MBTreeSetFlags(psys, 0x880, 1);
                            MBPsysSetEVolume(1.0f, 1.0f, psys);
                            MBPsysSetPParm(1.0f, 1.0f, 1.0f, 0.0f,
                                           psys, 3);
                            MBPsysSetPParm(lbl_80347B98,
                                           lbl_80347B98,
                                           lbl_80347B98,
                                           lbl_80347B98, psys, 4);
                            MBPsysSetPTex(psys, particleTexture);
                            MBPsysSetERate4(lbl_80347C88,
                                           lbl_80347C88,
                                           lbl_80347C88,
                                           lbl_80347C88, psys);
                            MBPsysSetETime(lbl_80347BF8,
                                          lbl_80347CAC, psys);
                            MBPsysSetPSpeed(1.0f, psys);
                        }
                    }
                }
            }

            p->move_yaw = heading;
            PF(p, 0xC8, f32) = heading;
            CreateYPRMatrix((f32*)((u8*)p + 0x14),
                            (f32*)((u8*)p + 0xC4));
            p->hud_flags |= 1;
            goto player_motion_phase_exit;
        }

        /* Target +0x31D4: normal floor marker and powerup phases. */
        if ((p->hud_flags & 0x20) != 0) {
            if (p->floor_cur > p->floor_base) {
                PF(p->mbnode, 0x30, f32) = *(f32*)(motion + 0x30);
                PF(p->mbnode, 0x34, f32) = *(f32*)(motion + 0x34);
                PF(p->mbnode, 0x38, f32) = *(f32*)(motion + 0x38);
                PF(p->mbnode, 0x34, f32) =
                    (f32)(lbl_80347BE0 + p->floor_cur);
                MBTreeSetAltTex(p->mbnode, -2,
                                lbl_80344BE8, 1);
                MBTreeClearFlags(p->mbnode, 2, 0);
            } else {
                PF(p->mbnode, 0x30, f32) = *(f32*)(motion + 0x30);
                PF(p->mbnode, 0x34, f32) = *(f32*)(motion + 0x34);
                PF(p->mbnode, 0x38, f32) = *(f32*)(motion + 0x38);
                PF(p->mbnode, 0x34, f32) =
                    (f32)(lbl_80347BE0 + p->floor_base);
                MBTreeSetAltTex(p->mbnode, -1, 0, 1);
                MBTreeClearFlags(p->mbnode, 2, 0);
            }
            goto player_motion_phase_exit;
        }

        if (p->action > 0 && p->action < 11 &&
            p->action != 7 &&
            (ctl->pad.levels & 0x5000) == 0 && ctl->pad.unk34 != 0 &&
            lbl_80347B08 == speedScale &&
            lbl_80347B08 == ctl->values[10]) {
            heading = atan2(attackDir[0], attackDir[2]);
        }

        to[0] = oldpos[0] + dpos[0];
        to[1] = oldpos[1] + dpos[1];
        to[2] = oldpos[2] + dpos[2];

        if ((p->act_bits & 0x02000000) != 0) {
            s32 effect = StartFXSub(28, NULL, 42, 0x880, 0.0f);
            SfxSetParent(effect, p->node);
            SfxSetDamage(lbl_80347C50, lbl_80347CB0,
                         lbl_80347CA0, effect, 32, index + 1);
            SV(p)->act_bits = p->act_bits & ~0x02000000;
            player_get_powerup_state(1.0f, p, 5, 0x10000000);
            ShakeCamera(0, 0, 30, lbl_80347CB4, 200);
            fn_8009D4B0(p->index);
        }

        if ((p->act_bits & 0x01000000) != 0) {
            s32 effect = -1;
            if ((p->flags & 0x3000) != 0) {
                effect = StartFXSub(56, NULL, 42, 0x800, 0.0f);
                SfxSetDamage(lbl_80347C54, lbl_80347C40, 0.0f,
                             effect, 33, index + 1);
                if ((p->flags & 0x1000) != 0) {
                    fn_8009F490(index);
                } else {
                    fn_8009F450(index);
                }
            } else if ((p->flags & 0x410) != 0) {
                effect = StartFXSub(52, NULL, 42, 0x800, 0.0f);
                SfxSetDamage(lbl_80347C58, lbl_80347C40, 0.0f,
                             effect, 33, index + 1);
                AudioTurboDefense(index);
            } else if ((p->flags & 0x20) != 0) {
                effect = StartFXSub(53, NULL, 42, 0x800, 0.0f);
                SfxSetDamage(lbl_80347C58, lbl_80347C40, 0.0f,
                             effect, 36, index + 1);
                AudioTurboDefense(index);
            } else if ((p->flags & 0x40) != 0) {
                effect = StartFXSub(54, NULL, 42, 0x800, 0.0f);
                SfxSetDamage(lbl_80347C58, lbl_80347C40, 0.0f,
                             effect, 34, index + 1);
                AudioTurboDefense(index);
            }

            if (effect >= 0) {
                if ((p->flags & 0x400) != 0 &&
                    p->atree != NULL) {
                    s32 object = MBOX_ReallyFindObject(
                        strings + 84, sPowerupsHandle,
                        sPowerupsHandle, 1);
                    s32* found;
                    if ((found = AtreeFindMbidxNode(p->atree,
                                                    object)) != NULL) {
                        SfxSetParent(effect, (void*)*found);
                    }
                    p->power_target -= p->coll_score;
                    p->coll_score = 0.0f;
                    AudioPlayerTurbo(p->index, 0, 0);
                } else {
                    SfxSetParent(effect, p->weapon_node);
                }
                PF(Effects + effect * 240, 0x9C, f32) =
                    lbl_80347CB8;
                player_get_powerup_state(1.0f, p, 9, 0x70);
            }
            SV(p)->act_bits = p->act_bits & ~0x01000000;
        }

        targetDir[0] = sin(controlYaw);
        targetDir[1] = 0.0f;
        targetDir[2] = cos(controlYaw);

        /* Target +0x3600: weapon-node vector and one-shot projectile. */
        if ((p->act_bits & 0x10000000) != 0) {
            if ((p->flags & 0x80) != 0 ||
                PF(p, 0x748, void*) != NULL) {
                f32 projectileHeight;
                f32 adjusted;
                localVector[0] = PF(p->node, 0x40, f32) *
                                 PF(lbl_80282930[p->index], 0x170, f32);
                localVector[1] = PF(p->node, 0x44, f32) *
                                 PF(lbl_80282930[p->index], 0x174, f32);
                localVector[2] = PF(p->node, 0x48, f32) *
                                 PF(lbl_80282930[p->index], 0x178, f32);
                MulVecMat4(localVector, hit, (f32*)motion);
                localVector[0] = attackDir[0];
                localVector[1] = attackDir[1];
                localVector[2] = attackDir[2];

                if (gBossType >= 0) {
                    adjusted = ModifyPlayerDpos(
                        p, targetDir, localVector, p->field_11C,
                        item, (u32)target, targetDistance,
                        lbl_80347CBC);
                    projectileHeight = 0.0f;
                    missileVelocity[0] = localVector[0] * adjusted;
                    missileVelocity[1] = localVector[1] * adjusted;
                    missileVelocity[2] = localVector[2] * adjusted;
                    CalcTargetDir(missileVelocity, lbl_80347C54,
                                  lbl_80347CC0, projectileHeight,
                                  projectileHeight);
                } else {
                    adjusted = ModifyPlayerDpos(
                        p, targetDir, localVector, p->field_11C,
                        item, (u32)target, targetDistance,
                        lbl_80347CBC);
                    projectileHeight = lbl_80347C8C;
                    missileVelocity[0] = localVector[0] * adjusted;
                    missileVelocity[1] = localVector[1] * adjusted;
                    missileVelocity[2] = localVector[2] * adjusted;
                    CalcTargetDir(missileVelocity, lbl_80347C54,
                                  lbl_80347CC0, projectileHeight,
                                  lbl_80347CC4);
                }

                if ((p->flags & 0x80) != 0) {
                    fn_80093918(4, index, hit, missileVelocity,
                                lbl_80347CB0, lbl_80347C8C,
                                projectileHeight);
                } else {
                    f32 shotSpeed =
                        (f32)(lbl_80347BE0 *
                              (f32)(p->level - 25) +
                              lbl_80347CC8);
                    fn_80093918(index, index, hit,
                                missileVelocity, lbl_80347CB0,
                                shotSpeed, projectileHeight);
                }
            }
            SV(p)->act_bits = p->act_bits & ~0x10000000;
        }

        /* Target +0x3824: low-byte melee/target resolution. */
        if ((p->act_bits & 0xFF) != 0) {
            if ((p->act_bits & 0xFE) != 0) {
                f32 damage = p->stat_damage;
                u32 damageFlags = p->field_11C;
                f32 hitRange;

                if (p->collision_item == NULL && ctl->values[8] == 0.0f) {
                    hit[0] = (f32)(targetDir[0] *
                                   (lbl_80347C28 * p->col_radius) +
                                   oldpos[0]);
                    hit[1] = (f32)(targetDir[1] *
                                   (lbl_80347C28 * p->col_radius) +
                                   oldpos[1]);
                    hit[2] = (f32)(targetDir[2] *
                                   (lbl_80347C28 * p->col_radius) +
                                   oldpos[2]);
                    PlayerCollideEnemies(p, (s32)oldpos, hit, NULL, 0,
                                         NULL, radius, height);
                }

                targetDistance = PlayerGetTarget(
                    p, to, targetDir, attackDir, &item, &target);
                if (item >= 0x10000) {
                    critterIndex = item & 0xFFFF;
                    enemy = NULL;
                } else if (item >= 0) {
                    enemy = gEnemies + item * 916;
                    critterIndex = -1;
                } else {
                    critterIndex = -1;
                    enemy = NULL;
                }

                if ((p->act_bits & 0xF0) != 0) {
                    damage = (f32)(damage * lbl_80347B28);
                    p->power_target -= p->coll_score;
                    p->coll_score = 0.0f;
                    damageFlags |= 0x20;
                } else if ((p->act_bits & 4) != 0) {
                    damageFlags |= 0x10;
                    damage = (f32)(damage * lbl_80347C28);
                } else if ((p->act_bits & 8) != 0 &&
                           enemy != NULL &&
                           (f64)PF(enemy, 0x23C, f32) <= lbl_80347C28) {
                    damageFlags |= 0x20;
                }

                if ((p->act_bits & 2) != 0) {
                    hitRange = (f32)(lbl_80347BD0 +
                                     lbl_80347BD0 + radius);
                } else {
                    hitRange = (f32)(lbl_80347C28 + radius);
                }
                to[0] = oldpos[0] + targetDir[0] *
                                      (f32)(lbl_80347C28 + radius);
                to[1] = oldpos[1] + targetDir[1] *
                                      (f32)(lbl_80347C28 + radius);
                to[2] = oldpos[2] + targetDir[2] *
                                      (f32)(lbl_80347C28 + radius);

                if (targetDistance < hitRange) {
                    if (item >= 0) {
                        if (critterIndex >= 0) {
                            u8* critter =
                                gCritterPool + critterIndex * 2784;
                            hit[0] = PF(critter, 0x5C, f32);
                            hit[1] = PF(critter, 0x60, f32);
                            hit[2] = PF(critter, 0x64, f32);
                        } else {
                            hit[0] = PF(enemy, 0x54, f32);
                            hit[1] = PF(enemy, 0x58, f32);
                            hit[2] = PF(enemy, 0x5C, f32);
                        }
                        if (FastWallCollide(oldpos, hit, NULL, 0) == 0) {
                            s32 damaged = PlayerMotion_DamageTarget(
                                p, item, damageFlags, (s32)to, 1,
                                damage, 0.0f);
                            if (enemy == NULL ||
                                (f64)PF(enemy, 0x23C, f32) >
                                    lbl_80347C28) {
                                p->fall_frames +=
                                    damaged != 0 ? 3 : 1;
                                p->fall_time = sMusicFadeBase;
                            }
                        }
                    } else if (target != NULL) {
                        PlayerMotion_HitTarget(p, target, damageFlags,
                                               damage);
                    } else if (optionsAudioAndPrefs30[7] == 2) {
                        PlayerMotion_FindClosestPlayer(p, targetDir,
                                                       damageFlags,
                                                       damage);
                    }
                }
            }

            if ((p->act_bits & 1) != 0) {
                p->combo_fade = sMusicFadeBase;
                if ((p->flags & 0x400) != 0) {
                    SV(p)->act_bits = p->act_bits | 0x20000000;
                }
            }
            SV(p)->act_bits = p->act_bits & ~0xFFU;
        }

        /* Target +0x3B80: missile powerup byte. */
        if ((p->act_bits & 0xFF00) != 0) {
            u32 missileFlags = p->field_11C;
            s32 missileMode = 1;
            f32 missileDamage;
            f32 missileScale;

            if ((p->act_bits & 0x6000) != 0) {
                missileDamage = lbl_80347CD0;
                missileMode = 0;
                missileScale = 1.0f;
            } else if ((p->act_bits & 0x1000) != 0) {
                missileFlags |= 0x02000010;
                missileDamage = lbl_80347CD0;
                missileScale = lbl_80347B98;
                missileMode = 2;
            } else if ((p->act_bits & 0x800) != 0) {
                missileDamage = 0.0f;
                if (player_get_powerup_state(1.0f, p, 5,
                                             0x00100000) != 0) {
                    missileScale = gBossType >= 0
                                       ? lbl_80347CD4
                                       : lbl_80347B98;
                    missileFlags |= 0x00100000;
                } else {
                    missileScale = 1.0f;
                }
                missileMode = 0;
            } else {
                missileDamage =
                    (f32)((f64)(sMusicFadeBase - p->combo_fade) -
                          lbl_80347CD8);
                if ((f64)missileDamage < lbl_80347B08) {
                    missileDamage = 0.0f;
                }
                if ((f64)missileDamage > lbl_80347BE0) {
                    missileDamage = lbl_80347CA0;
                }
                missileScale = 1.0f;
            }

            {
                f32 adjusted = ModifyPlayerDpos(
                    p, targetDir, attackDir, missileFlags, item,
                    (u32)target, targetDistance, missileDamage);
                PlayerStartMissile((u8*)p, attackDir, missileFlags,
                                   missileMode, adjusted, missileScale);
            }
            if ((p->flags & 0x8000) != 0) {
                fn_8009F410(index);
            } else if ((p->flags & 0x4000) != 0) {
                fn_8009F3D0(index);
            } else {
                AudioPlayerEatSFX(index);
            }
            SV(p)->act_bits = p->act_bits & ~0xFF00U;
            SV(p)->act_bits = p->act_bits | 0x10000000;
        }

        /* Target +0x3CE4: magic-player and potion magic triggers. */
        if ((p->act_bits & 0x10000) != 0) {
            s32 effect;
            if ((effect = StartMagicPlayerFX(lbl_80127D00)) >= 0) {
                if (p->mbnode2 != NULL) {
                    MBNodeSetParent(
                        *(void**)(Effects + effect * 240 + 0x14),
                        p->mbnode2);
                }
                MBTreeSetFlags(*(void**)(Effects + effect * 240 + 0x14),
                               0x04000000, 0);
            }
            SV(p)->act_bits = p->act_bits & ~0x10000U;
        }

        if ((p->act_bits & 0x60000) != 0) {
            if (p->quest_state == 0 &&
                (lbl_8034489C <= 0 || lbl_8034489C >= 5)) {
                s32 magicMode;
                if ((p->act_bits & 0x40000) != 0) {
                    magicMode = (ctl->pad.levels & 0x10000) != 0 ? 3 : 2;
                } else if ((p->field_956 & 2) != 0) {
                    magicMode = 1;
                    fn_8009F390(p->index);
                } else {
                    magicMode = 0;
                }

                if (p->item_body_hi > 0) {
                    p->item_body_hi--;
                    start_magic(index, (f32*)(motion + 0x30),
                                PF(p, 0x3300 +
                                      p->item_body_hi * 4, u32),
                                magicMode, 1.0f);
                } else {
                    start_magic(index, (f32*)(motion + 0x30),
                                0, magicMode, 1.0f);
                }
                p->field_956 = 128;
            }
            SV(p)->act_bits = p->act_bits & ~0x60000U;
        }

player_motion_phase_exit:
        {
            f32 newYaw;
            f32 deltaYaw;
            f32 turnStep;
            f32 comboTime;
            s32 grabKind;

            p->prev_pos[0] = *(f32*)(motion + 0x30);
            p->prev_pos[1] = *(f32*)(motion + 0x34);
            p->prev_pos[2] = *(f32*)(motion + 0x38);
            *(f32*)(motion + 0x30) += dpos[0];
            *(f32*)(motion + 0x34) += dpos[1];
            *(f32*)(motion + 0x38) += dpos[2];

            if ((f64)(dpos[0] * dpos[0] + dpos[1] * dpos[1] +
                      dpos[2] * dpos[2]) > lbl_80347CE0) {
                p->hud_flags |= 1;
            }

            if (hitKind != 0 && p->timer_1FC <= 0) {
                if (hitKind == 2) {
                    NormalVector2D(reflection);
                    ReflectVector2D((f32*)((u8*)p + 0x34), reflection, hit);
                    newYaw = atan2(hit[0], hit[2]);
                } else {
                    newYaw = (f32)(lbl_80347CE8 + p->move_yaw);
                }
                newYaw = PlayerMotion_WrapAngle(newYaw);
                p->move_yaw = newYaw;
                p->timer_1FC = 10;
            } else {
                turnStep = (f32)(lbl_80347CF0 * gClockFrameStep *
                                 p->field_A4C);
                deltaYaw = PlayerMotion_WrapAngle(heading - controlYaw);
                if (deltaYaw > turnStep) {
                    newYaw = controlYaw + turnStep;
                } else if (deltaYaw < -turnStep) {
                    newYaw = controlYaw - turnStep;
                } else {
                    newYaw = heading;
                }
                if ((u32)(motionState - 34) <= 1) {
                    p->move_yaw = heading;
                } else {
                    p->move_yaw = newYaw;
                }
            }

            {
                f32 yawDiff = PF(p, 0xC8, f32) - newYaw;
                yawDiff = fabsf_param(yawDiff);
                if ((f64)yawDiff > lbl_80347CE0) {
                    p->hud_flags |= 1;
                }
            }
            PF(p, 0xC8, f32) = newYaw;
            CreateYPRMatrix((f32*)((u8*)p + 0x14),
                            (f32*)((u8*)p + 0xC4));

            {
                if (p->floor_cur > p->floor_base) {
                    PF(p->mbnode, 0x30, f32) = *(f32*)(motion + 0x30);
                    PF(p->mbnode, 0x34, f32) = *(f32*)(motion + 0x34);
                    PF(p->mbnode, 0x38, f32) = *(f32*)(motion + 0x38);
                    PF(p->mbnode, 0x34, f32) =
                        (f32)(lbl_80347BE0 + p->floor_cur);
                    MBTreeSetAltTex(p->mbnode, -2,
                                    lbl_80344BE8, 1);
                    MBTreeClearFlags(p->mbnode, 2, 0);
                } else {
                    PF(p->mbnode, 0x30, f32) = *(f32*)(motion + 0x30);
                    PF(p->mbnode, 0x34, f32) = *(f32*)(motion + 0x34);
                    PF(p->mbnode, 0x38, f32) = *(f32*)(motion + 0x38);
                    PF(p->mbnode, 0x34, f32) =
                        (f32)(lbl_80347BE0 + p->floor_base);
                    MBTreeSetAltTex(p->mbnode, -1, 0, 1);
                    MBTreeClearFlags(p->mbnode, 2, 0);
                }
            }

            if ((p->flags & 1) != 0) {
                u8* root = (u8*)p->platform;
                PF(PF(root, 0, void*), 0x34, f32) =
                    (f32)(lbl_80347B88 + PF(PF(root, 0x1C, u8*), 0x64, f32));
            } else {
                u8* root = (u8*)p->platform;
                PF(PF(root, 0, void*), 0x34, f32) =
                    PF(PF(root, 0x1C, u8*), 0x64, f32);
            }

            if ((p->grab_flags & 3) != 0) {
                s32 kind;
                s32 variant;
                if (p->floor_cur > p->floor_base) {
                    kind = 4;
                } else if ((p->shield_flags & 0x10000) != 0) {
                    kind = 3;
                } else if ((SV(p)->floor_flags & 8) != 0) {
                    kind = 2;
                } else {
                    kind = 0;
                }
                variant = (p->grab_flags & 2) ? 1 : 0;
                if ((p->flags & 1) == 0) {
                    fn_8009EFCC(index, variant, kind);
                }
                p->grab_flags &= ~3;
            }

            if ((p->flags & 0x400) != 0 &&
                p->grab_partner == NULL) {
                grabKind = -1;
            } else {
                grabKind = p->char_type;
            }
            comboTime = p->combo_time;

            switch (p->anim_208) {
            case 88:
            case 89:
                if (p->grab_pending != NULL) {
                    p->grab_partner = p->grab_pending;
                    PF(p->grab_partner, 0x964, s16) |= 0x10;
                    p->grab_partner->grab_partner = p;
                    p->grab_pending = NULL;
                    p->power_target -= p->coll_score;
                    p->coll_score = 0.0f;
                }
                break;
            default:
                p->grab_pending = NULL;
                break;
            }

            switch (grabKind) {
            case 2:
            case 5:
            case 6:
            case 7:
                switch (p->anim_208) {
                case 88:
                case 89:
                    if (p->grab_partner != NULL &&
                        (PF(p->grab_partner, 0x964, s16) & 0x20) == 0) {
                        PlayerSetGrabbed(p->grab_partner,
                                         p->grab_node, NULL);
                        goto player_motion_grab_done;
                    }
                    if (grabKind != 7) {
                        goto player_motion_grab_done;
                    }
                    break;
                default:
                    break;
                }
                if (p->grab_partner != NULL &&
                    (PF(p->grab_partner, 0x964, s16) & 0x10) != 0) {
                    if ((PF(p->grab_partner, 0x964, s16) & 0x20) != 0) {
                        PlayerUnsetGrabbed(p->grab_partner, 1);
                    }
                    PF(p->grab_partner, 0x964, s16) &= ~0x10;
                    PF(p->grab_partner, 0x6B8, Player*) = NULL;
                    p->grab_partner = NULL;
                }
                break;

            case 1:
            case 3:
                switch (p->anim_208) {
                case 88:
                case 89: {
                    Player* partner = p->grab_partner;
                    if (partner != NULL && (p->hud_flags & 0x20) == 0) {
                        f32 grabDir[3];
                        f32 partnerYaw;
                        f32 partnerFacing;
                        grabDir[0] = p->pos[0] - PF(partner, 0x44, f32);
                        grabDir[1] = p->pos[1] - PF(partner, 0x48, f32);
                        grabDir[2] = p->pos[2] - PF(partner, 0x4C, f32);
                        SlowNormalVector(grabDir);
                        partnerYaw = PlayerMotion_WrapAngle(
                            atan2(grabDir[0], grabDir[2]) + lbl_80347CF8);
                        partnerFacing =
                            atan2(PF(partner, 0x34, f32),
                                  PF(partner, 0x3C, f32));
                        YawMat3((f32*)((u8*)partner + 0x14),
                                PlayerMotion_WrapAngle(partnerYaw -
                                                       partnerFacing));
                        PF(partner, 0x894, f32) = partnerYaw;
                        PlayerSetGrabbed(p, p->grab_partner->grab_node,
                                         NULL);
                    }
                    break;
                }
                default:
                    if (p->grab_partner != NULL &&
                        (PF(p->grab_partner, 0x964, s16) & 0x10) != 0) {
                        if ((p->hud_flags & 0x20) != 0) {
                            PlayerUnsetGrabbed(p, 1);
                        }
                        PF(p->grab_partner, 0x964, s16) &= ~0x10;
                        PF(p->grab_partner, 0x6B8, Player*) = NULL;
                        p->grab_partner = NULL;
                    }
                    break;
                }
                break;

            case 0:
                switch (p->anim_208) {
                case 88:
                    if (comboTime < lbl_80347C88) {
                        if (p->grab_partner != NULL &&
                            (PF(p->grab_partner, 0x964, s16) & 0x20) == 0) {
                            PlayerSetGrabbed(p->grab_partner,
                                             p->grab_node, NULL);
                            PF(p->grab_partner, 0x8FC, f32) = sMusicFadeBase;
                        }
                        break;
                    }
                    /* fall through */
                case 89:
                default:
                    if (p->grab_partner != NULL) {
                        if ((PF(p->grab_partner, 0x964, s16) & 0x10) != 0) {
                            if ((PF(p->grab_partner, 0x964, s16) & 0x20) != 0) {
                                PlayerUnsetGrabbed(p->grab_partner, 0);
                            }
                            PF(p->grab_partner, 0x964, s16) &= ~0x10;
                            PF(p->grab_partner, 0x964, s16) |= 0x40;
                            p->timer_1FA = 240;
                        }
                        if ((PF(p->grab_partner, 0x964, s16) & 0x40) != 0 &&
                            p->timer_1FA <= 0) {
                            PF(p->grab_partner, 0x964, s16) &= ~0x40;
                            PF(p->grab_partner, 0x6B8, Player*) = NULL;
                            p->grab_partner = NULL;
                            p->hud_flags &= ~0x80;
                        }
                    }
                    break;
                }
                break;

            case 4:
                switch (p->anim_208) {
                case 88: {
                    Player* partner = p->grab_partner;
                    if (partner != NULL && (p->hud_flags & 0x20) == 0) {
                        f32 grabDir[3];
                        f32 partnerYaw;
                        f32 partnerFacing;
                        grabDir[0] = p->pos[0] - PF(partner, 0x44, f32);
                        grabDir[1] = p->pos[1] - PF(partner, 0x48, f32);
                        grabDir[2] = p->pos[2] - PF(partner, 0x4C, f32);
                        SlowNormalVector(grabDir);
                        partnerYaw = PlayerMotion_WrapAngle(
                            atan2(grabDir[0], grabDir[2]) + lbl_80347CF8);
                        partnerFacing =
                            atan2(PF(partner, 0x34, f32),
                                  PF(partner, 0x3C, f32));
                        YawMat3((f32*)((u8*)partner + 0x14),
                                PlayerMotion_WrapAngle(partnerYaw -
                                                       partnerFacing));
                        PF(partner, 0x894, f32) = partnerYaw;
                        PlayerSetGrabbed(p, p->grab_partner->grab_node,
                                         NULL);
                        p->hud_flags |= 0x80;
                        PF(p->grab_partner, 0x8FC, f32) = sMusicFadeBase;
                    }
                    p->timer_1FA = 240;
                    break;
                }
                default:
                    if (p->grab_partner != NULL) {
                        if ((PF(p->grab_partner, 0x964, s16) & 0x10) != 0) {
                            PF(p->grab_partner, 0x964, s16) &= ~0x10;
                            PF(p->grab_partner, 0x964, s16) |= 0x40;
                        } else if ((PF(p->grab_partner, 0x964, s16) & 0x40) != 0 &&
                                   p->timer_1FA <= 0) {
                            if ((p->hud_flags & 0x20) != 0) {
                                PlayerUnsetGrabbed(p, 0);
                            }
                            PF(p->grab_partner, 0x964, s16) &= ~0x40;
                            p->hud_flags &= ~0x80;
                            PF(p->grab_partner, 0x6B8, Player*) = NULL;
                            p->grab_partner = NULL;
                        }
                    }
                    break;
                }
                break;

            default:
                break;
            }

player_motion_grab_done:
            if (p->quest_state < 2) {
                s32 sfx1 = -1;
                s32 sfx2 = -1;
                s32 comboMode = 0;

                switch (p->anim_208) {
                case 60:
                    sfx1 = *(s16*)(lbl_80282930[index] + 18);
                    break;
                case 35:
                    sfx1 = *(s16*)(lbl_80282930[index] + 12);
                    break;
                case 37:
                    sfx1 = *(s16*)(lbl_80282930[index] + 16);
                    break;
                case 99:
                    sfx1 = *(s16*)(lbl_80282930[index] + 20);
                    break;
                case 84:
                    sfx1 = *(s16*)(lbl_80282930[index] + 14);
                    break;
                case 86:
                    sfx1 = *(s16*)(lbl_80282930[index] + 22);
                    break;
                case 87:
                    sfx1 = *(s16*)(lbl_80282930[index] + 24);
                    sfx2 = *(s16*)(lbl_80282930[index] + 26);
                    break;
                case 88:
                    sfx1 = *(s16*)(lbl_80282930[index] + 28);
                    comboMode = 1;
                    break;
                case 90:
                    sfx1 = *(s16*)(lbl_80282930[index] + 30);
                    break;
                case 123:
                    sfx1 = *(s16*)(lbl_80282930[index] + 34);
                    break;
                }

                if (comboMode == 1 && comboTime >= 0.0f &&
                    p->combo_cd < 0.0f) {
                    if (p->char_type == 4 || p->char_type == 7) {
                        f32 comboPos[3];
                        comboPos[0] = p->beacon_pos[0] + p->anchor_pos[0];
                        comboPos[1] = p->beacon_pos[1] + p->anchor_pos[1];
                        comboPos[2] = p->beacon_pos[2] + p->anchor_pos[2];
                        StartComboFX(comboPos, PF(p, 4, s32),
                                     PF(p->grab_partner, 4, s32));
                    } else {
                        StartComboFX(p->col_pos, PF(p, 4, s32),
                                     PF(p->grab_partner, 4, s32));
                    }
                } else if (comboMode >= 2 && comboTime >= 0.0f &&
                           p->combo_cd < 0.0f) {
                    f32 scale =
                        (f32)(lbl_80347BD0 + (f32)(comboMode - 2));
                    s32 effect = StartComboFX(p->col_pos, -1,
                                              PF(p, 4, s32));
                    PF(Effects + effect * 240, 0x64, u32) = 552;
                    SfxSetDamage(p->stat_damage * scale,
                                 (f32)(lbl_80347C28 * scale), 0.0f,
                                 effect, 32, p->index + 1);
                }

                if (sfx1 >= 0) {
                    PlyrSfxDoDamage((u8*)p, sfx1,
                                 (u8*)p->grab_partner, NULL,
                                 p->combo_cd, comboTime);
                }
                if (sfx2 >= 0) {
                    PlyrSfxDoDamage((u8*)p, sfx2,
                                 (u8*)p->grab_partner, NULL,
                                 p->combo_cd, comboTime);
                }
            }
            p->combo_cd = comboTime;
        }
        (void)hitKind;
    }
}
/* 0x80085FA0 - shape the raw horizontal dpos against the current floor slope:
 * clamp the slope, blend it into the vertical component (special-casing
 * flag-8 floors, swimming state 4, and steep/opposed motion), renormalize,
 * and return the frame's slope-scaled speed factor. */
f32 ModifyPlayerDpos(Player* p, f32* from, f32* dpos, u32 flags, s32 a5,
                     u32 a6, f32 arg7, f32 param) {
    f32 dot;
    f32 mag;
    f32 slope;

    dot = from[0] * dpos[0] + from[2] * dpos[2];
    mag = fqdist(dpos[0], dpos[2]);

    slope = p->field_8BC;
    if (slope > lbl_80347D00) {
        slope = lbl_80347D08;
    }
    if ((SV(p)->floor_flags & 8) != 0 && slope > lbl_80347B30 &&
        slope < lbl_80347B00) {
        slope = lbl_80347B10;
    }
    if (p->char_type == 4) {
        slope = (f32)(slope + lbl_80347D10);
    }

    if (dot < lbl_80347D18 * mag || (flags & 0x100000) != 0) {
        dpos[0] = from[0];
        dpos[2] = from[2];
        dpos[1] = slope;
    } else if (a5 >= 0 || a6 != 0) {
        if (dpos[1] > lbl_80347B30) {
            dpos[1] = (f32)(dpos[1] * lbl_80347D20);
        }
        if (fabsf_param(slope) > fabsf_param(dpos[1])) {
            dpos[1] = (f32)(lbl_80347B00 * (dpos[1] + slope));
        }
    } else {
        f32 s = smallsqrt(lbl_80347B40 - slope * slope);
        dpos[0] = dpos[0] * s;
        dpos[2] = dpos[2] * s;
        dpos[1] = slope;
    }

    if (dpos[1] < lbl_80347B30) {
        dpos[1] = (f32)(dpos[1] * lbl_80347B00);
    }
    NormalVector(dpos);
    if (dpos[1] > lbl_80347D18) {
        dpos[0] = from[0];
        dpos[1] = from[1];
        dpos[2] = from[2];
    }
    return (f32)(lbl_80347D28 * param + lbl_80347BB0);
}

int PlayerCollideWalls(Player* p, s32 unused, f32* dpos, f32* from, f32* to) {
    f32 dx = to[0] - from[0];
    f32 dz = to[2] - from[2];
    s32 count = 0;

    if (dx > 0.0f && dpos[0] < 0.0f) {
        dpos[0] += dx;
        if (dpos[0] > 0.0f) {
            dpos[0] = 0.0f;
        }
        count = 1;
    } else if (dx < 0.0f && dpos[0] > 0.0f) {
        dpos[0] += dx;
        if (dpos[0] < 0.0f) {
            dpos[0] = 0.0f;
        }
        count = 1;
    } else {
        p->light_vel[0] += gClockFrameReciprocal * dx;
        dpos[0] = 0.0f;
    }

    if (dz > 0.0f && dpos[2] < 0.0f) {
        dpos[2] += dz;
        if (dpos[2] > 0.0f) {
            dpos[2] = 0.0f;
        }
        count++;
    } else if (dz < 0.0f && dpos[2] > 0.0f) {
        dpos[2] += dz;
        if (dpos[2] < 0.0f) {
            dpos[2] = 0.0f;
        }
        count++;
    } else {
        p->light_vel[2] += gClockFrameReciprocal * (to[2] - from[2]);
        dpos[2] = 0.0f;
    }

    return count;
}

static inline void PlayerMotion_FloorFXDamage(Player* p, u32 flags, f32* dv)
{
    switch (((flags >> 16) & 0xF) << 16) {
    case 0x10000:
    case 0x60000:
    default:
        p->floor_fx_time = 1.0 + sMusicFadeBase;
        damage_player(p->index, 5.0f, 1, 0, NULL);
        break;
    case 0x20000:
        damage_player(p->index, 10.0f, 1, 16, dv);
        p->floor_fx_time = 1.0 + sMusicFadeBase;
        break;
    case 0x30000:
    case 0x40000:
    case 0x50000:
        p->floor_fx_time = 1.0 + sMusicFadeBase;
        damage_player(p->index, 15.0f, 1, 32, dv);
        AudioWorldHitPlyr((u8*)p + 0x64);
        break;
    }
}

void PlayerMotion_FloorFX(Player* p, WorldObj* obj, f32* v1, f32* v2) {
    f32 dv[3];
    u32 flags;

    flags = WorldObjGetAllFlags(obj);

    if ((flags & 0xF0000) == 0) {
        return;
    }
    if ((flags & 0x2000000) != 0 && (flags & 0x8000000) == 0) {
        return;
    }
    if (p->vibe_on >= 31) {
        return;
    }
    if (sMusicFadeBase < p->floor_fx_time) {
        return;
    }

    dv[0] = v1[0] - v2[0];
    dv[1] = 0.0f;
    dv[2] = v1[2] - v2[2];
    NormalVector2D(dv);
    PlayerMotion_FloorFXDamage(p, flags, dv);
}
/* 0x80086470 - advance the player's queued knockback: dispatch on the hit-type
 * flag bits to a reaction code + velocity impulse, retarget the facing angle
 * for the strong reactions, spawn the skin FX, then clear the queue.  Returns
 * the reaction code the motion driver acts on. */
#pragma opt_propagation off
#pragma opt_common_subs off
u32 PlayerKnockback(f32 angle, Player* p, f32* out) {
    s32 result = 0;
    u32 prevFlags;
    u32 initialFlags;
    u32 flags;

    if (p->hit_force[1] > lbl_80347B08) {
        p->hit_force[1] = lbl_80347B30;
    }
    prevFlags = p->act_flags;
    p->act_flags = p->obj_flags;
    initialFlags = p->obj_flags;

    if ((initialFlags & 0x4000) != 0) {
        p->obj_flags = initialFlags & 0x4000;
        return 300;
    }
    if ((initialFlags & 0x8000) != 0) {
        p->obj_flags = initialFlags & 0x8000;
        if ((prevFlags & 0x8000) == 0) {
            p->vel[0] = p->hit_force[0];
            p->vel[1] = p->hit_force[1];
            p->vel[2] = p->hit_force[2];
        }
        return 301;
    }
    if ((prevFlags & 0x8000) != 0) {
        damage_player(p->index, p->hit_damage, 1, 0, NULL);
        p->hit_damage = lbl_80347B30;
    }
    if ((p->shield_flags & 0x10000) != 0) {
        p->hit_damage = lbl_80347B30;
        p->obj_flags = 0;
        return 0;
    }

    flags = p->obj_flags;
    if ((flags & 0x4000000) != 0) {
        result = 200;
    }
    if (p->hit_damage > lbl_80347B40) {
#define KNOCK_IMPULSE(sc)                                              \
        p->vel[0] = p->hit_force[0] * (sc) + p->vel[0]; \
        p->vel[1] = p->hit_force[1] * (sc) + p->vel[1]; \
        p->vel[2] = p->hit_force[2] * (sc) + p->vel[2]
#define KNOCK_IMPULSE_X(sc, x)                                        \
        p->vel[0] = (x) * (sc) + p->vel[0];            \
        p->vel[1] = p->hit_force[1] * (sc) + p->vel[1]; \
        p->vel[2] = p->hit_force[2] * (sc) + p->vel[2]
        if ((flags & 0x10000) != 0) {
            f32 forceX;
            f32 sc;
            forceX = p->hit_force[0];
            result = 30;
            sc = lbl_80347C50;
            KNOCK_IMPULSE_X(sc, forceX);
        } else if ((flags & 0x40) != 0) {
            f32 forceX;
            f32 sc;
            forceX = p->hit_force[0];
            result = 20;
            sc = lbl_80347C50;
            KNOCK_IMPULSE_X(sc, forceX);
        } else if ((flags & 0x120) != 0) {
            f32 sc;
            result = 20;
            sc = (f32)((p->flags & 0x400) != 0 ? lbl_80347D38
                                                       : lbl_80347D40);
            KNOCK_IMPULSE(sc);
        } else if ((flags & 0x10) != 0) {
            f32 forceX;
            f32 sc;
            forceX = p->hit_force[0];
            result = 10;
            sc = lbl_80347D48;
            KNOCK_IMPULSE_X(sc, forceX);
        } else if ((flags & 0x2000) != 0) {
            result = 3;
        } else if ((flags & 0x80) != 0) {
            result = 2;
        } else {
            result = 1;
        }
#undef KNOCK_IMPULSE
#undef KNOCK_IMPULSE_X
        if (result >= 10) {
            {
                f32 atanX;
                f32 atanZ;
                f32 a;
                f64 d;
                f64 wrappedDelta;
                atanZ = p->hit_force[2];
                atanX = p->hit_force[0];
                a = atan2(atanX, atanZ);
                d = a - angle;
                if (d > lbl_80347B50) {
                    wrappedDelta = d - lbl_80347B60;
                } else if (d <= lbl_80347B68) {
                    wrappedDelta = lbl_80347B60 + d;
                } else {
                    wrappedDelta = d;
                }
                if (PlayerKnockbackFabs((f32)wrappedDelta) > lbl_80347C38) {
                    result++;
                    a = (f32)(a + lbl_80347B50);
                }
                d = a;
                if (d > lbl_80347B50) {
                    d -= lbl_80347B60;
                } else if (d <= lbl_80347B68) {
                    d = lbl_80347B60 + d;
                }
                *out = (f32)d;
            }
        }
        if ((p->obj_flags & 0x1000000) == 0) {
            fn_80094164((u8*)p->col_pos, p->obj_flags, 0);
        }
        SetSkinFX((u8*)p + 0x7DC, lbl_80344BF8, 1, 1, lbl_80347B40);
    } else if ((flags & 0x80) != 0) {
        result = 2;
    }

    {
        f32 zero = lbl_80347B30;
        p->hit_force[0] = zero;
        p->hit_force[1] = zero;
        p->hit_force[2] = zero;
        p->hit_damage = zero;
    }
    p->obj_flags = 0;
    return result;
}
#pragma opt_common_subs reset
#pragma opt_propagation reset
void PlayerMotion_FindClosestPlayer(Player* p, f32* dir, u32 flags, f32 dmg) {
    u8 unused[8];
    f32 dvec[3];
    f32 best = 2.0 + (f64)p->col_radius;
    s32 i;
    s32 closest = -1;

    for (i = 0; i < 4; i++) {
        Player* op = &gPlayerRecords[i];
        f32 len;
        f32 dot;
        f32 adj;
        if (op == p || op->state != 1) {
            continue;
        }
        dvec[0] = op->pos[0] - p->pos[0];
        dvec[1] = op->pos[1] - p->pos[1];
        dvec[2] = op->pos[2] - p->pos[2];
        len = NormalVector(dvec);
        dot = dvec[0] * dir[0] + dvec[1] * dir[1] + dvec[2] * dir[2];
        if (dot < 0.707) {
            continue;
        }
        adj = len - op->col_radius;
        if (adj >= best) {
            continue;
        }
        best = adj;
        closest = i;
    }

    if (closest >= 0) {
        damage_player(closest, dmg, 2, flags, dvec);
    }
}
/* 0x80086924 - resolve a melee hit against `target`: run the hit test, and on
 * a connect apply damage and raise the hit-something flag (type 3 = solid,
 * type 10 = openable, which also posts a "hit chest" message). */
void PlayerMotion_HitTarget(Player* p, void* target, s32 arg, f32 range) {
    f32 hitpos[3];
    f64 priority;

    if (lbl_80347B30 == range) {
        range = p->stat_damage;
    }
    if (target == NULL) {
        return;
    }
    hitpos[0] = PF(target, 0x44, f32);
    hitpos[1] = PF(target, 0x48, f32);
    hitpos[2] = PF(target, 0x4C, f32);
    hitpos[1] = (f32)(hitpos[1] + lbl_80347B28);

    priority = fn_8005C1DC(target, arg, p->index, range);
    {
        s32 exact = lbl_80347B08 == priority ? 1 : 0;
        s32 type;
        if (priority >= *(volatile f64*)&lbl_80347B08) {
            PlayerDamagedItem(p, target, exact);
            type = **(s32**)target;
            if (type == 3) {
                lbl_803447E4 = 1;
            } else if (type == 10) {
                if ((s8)(*(u8**)target)[0x28] == 0) {
                    msgPost(20, p->index, (u32)hitpos);
                }
                lbl_803447E4 = 1;
            }
        }
    }
}
typedef struct EnemyDamageView {
    u8 _000[0x2B8];
    f32 hitCooldown[4];
} EnemyDamageView;

/* 0x80086A24 - deal a melee hit to the enemy or critter identified by
 * `targetId` (>=0x10000 = critter pool, else enemy list): honor the per-player
 * hit cooldown, build the knockback direction from p's facing + damage, call
 * damage_enemy/CritterDamage, then run the follow-up reaction.  Returns the
 * hit result code, or -1. */
s32 PlayerMotion_DamageTarget(Player* p, s32 targetId, s32 a3, s32 a4, s32 a5,
                              f32 dmg, f32 priority) {
    u8 unused[8];
    f32 dir[3];
    u8* critter;
    EnemyDamageView* enemy;
    s32 result = -1;

    if (targetId >= 0x10000) {
        critter = &gCritterPool[(targetId & 0xFFFF) * 2784];
        enemy = NULL;
    } else if (targetId >= 0) {
        enemy = (EnemyDamageView*)&gEnemies[targetId * 916];
        critter = NULL;
    } else {
        return -1;
    }

    if (priority > lbl_80347B08) {
        if (enemy != NULL) {
            if (sMusicFadeBase < enemy->hitCooldown[p->index]) {
                return -1;
            }
        } else if (critter != NULL) {
            if (CritterNoHit(critter, p->index + 1) != 0) {
                return -1;
            }
        }
    }

    if (lbl_80347B30 == dmg) {
        dmg = p->stat_damage;
    }
    if ((p->flags & 0x100) != 0) {
        dmg = (f32)(dmg * lbl_80347C28);
    }
    dir[0] = p->mat[8];
    dir[1] = p->mat[9];
    dir[2] = p->mat[10];
    dir[1] = (f32)(lbl_80347D50 * dmg);
    if (dir[1] > lbl_80347C28) {
        dir[1] = lbl_80347B98;
    }

    if (enemy != NULL) {
        s32 estateRaw = PF(enemy, offsetof(PCollideEnemyLayout, state), s32);
        s32 estate = estateRaw;
        if (PF(enemy, offsetof(PCollideEnemyLayout, health), f32) > lbl_80347B08 &&
            (estateRaw == 1 || estateRaw == 6)) {
            result = damage_enemy(enemy, p->index, a3, a4, dir, 1, dmg);
        }
        if (result >= 0) {
            PlayerDamagedEnemy(p, enemy, estate, result, a5);
        }
        if (priority > lbl_80347B08) {
            enemy->hitCooldown[p->index] = sMusicFadeBase + priority;
        }
    } else if (critter != NULL) {
        result = CritterDamage(critter, p->index, a3, a4, dir, 1, dmg);
        if (priority > lbl_80347B08) {
            fn_80037ED0(critter, p->index + 1, priority);
        }
    }
    return result;
}
/* 0x80086C78 - pick p's current melee target: start from the incoming
 * *outId/*outObj, validate/replace it through the critter/enemy/boss
 * eligibility probes (range + facing dot), run the closest_enemy /
 * CritterLineCollide / fn_8005B274 sweeps, then the versus-mode player scan.
 * If nothing survived, fall back to the lunge-direction vector.  Returns the
 * winning distance (limit when no target).  The `flag` local is 0 on every
 * retail path; the boss-id block under it is dead but shipped. */
#pragma opt_lifetimes off
#pragma opt_common_subs off
f32 PlayerGetTarget(Player* p, f32* pos, f32* dir, f32* out, s32* outId,
                    u8** outObj) {
    f32 dist;
    u8 unused_40[16];
    f32 vec[3];
    s32 id = *outId;
    s32 flag = 0;
    u8* obj = outObj != NULL ? *outObj : NULL;
    u8 unused_8[12];
    f32 ty;
    f32 tz;
    f32 tx;
    f32 best;
    f32 limit;
    f32 dotThresh;
    f32 d;
    u8* enemy;
    Player* op;
    s32 i;

    best = limit = lbl_80347C88;

    if (gBossObj != NULL) {
        best = limit = lbl_80347D58;
    }

    if (id >= 0x10000) {
        u8* critter = &gCritterPool[(id & 0xFFFF) * 2784];
        tx = PF(critter, 0x5C, f32);
        ty = PF(critter, 0x60, f32);
        tz = PF(critter, 0x64, f32);
        best = CritterLineRootColSub(lbl_80347D08, lbl_80347B30, critter,
                                     pos, dir, out);
    } else if (id >= 0) {
        enemy = &gEnemies[id * 916];
        if (*(s32*)enemy != 31) {
            f32 dot;
            tx = PF(enemy, 0x54, f32);
            ty = PF(enemy, 0x58, f32);
            tz = PF(enemy, 0x5C, f32);
            out[0] = tx - pos[0];
            out[1] = ty - pos[1];
            out[2] = tz - pos[2];
            d = NormalVector(out);
            if ((best = d - PF(enemy, 0x238, f32)) > lbl_80347B30 ||
                *(s32*)enemy == 30) {
                dot = out[0] * dir[0] + out[1] * dir[1] + out[2] * dir[2];
                if (dot < lbl_80347B10) {
                    best = limit;
                } else {
                    flag = 0;
                }
            } else {
                flag = 0;
            }
        }
    }

    if (best >= limit) {
        if (obj != NULL && *(s8*)(obj + 0xCF) >= 0) {
            f32 dot;
            out[0] = PF(obj, 0x54, f32) - pos[0];
            out[1] = PF(obj, 0x58, f32) - pos[1];
            out[2] = PF(obj, 0x5C, f32) - pos[2];
            d = NormalVector2D(out);
            dot = out[0] * dir[0] + out[1] * dir[1] + out[2] * dir[2];
            best = d - PF(*(void**)obj, 0xC, f32);
            if (dot < lbl_80347D08) {
                best = limit;
            } else {
                flag = 0;
            }
        }
    }

    if (best >= limit) {
        u8* critter;
        if (optionsAudioAndPrefs30[5] >= 1) {
            dotThresh = lbl_80347D08;
        } else {
            dotThresh = lbl_80347B10;
        }
        best = closest_enemy(dotThresh, limit, pos, dir, out, &id,
                             (s32)PF(p, 0x108, f32));
        critter = CritterLineCollide(dotThresh, limit, pos, dir, vec, &dist);
        if (critter != NULL && dist < best) {
            if (*(void**)(critter + 4) == NULL) {
                FatalError("Ack!", 0x800000);
            }
            best = dist;
            out[0] = vec[0];
            out[1] = vec[1];
            out[2] = vec[2];
            id = *(s16*)critter | 0x10000;
        }
        dist = fn_8005B274(dotThresh, limit, pos, dir, vec, &obj);
        if (dist < best) {
            best = dist;
            out[0] = vec[0];
            out[1] = vec[1];
            out[2] = vec[2];
            id = -1;
            flag = 0;
        }
    }

    if (optionsAudioAndPrefs30[7] == 2 && best >= limit) {
        dotThresh = lbl_80347D08;
        enemy = (u8*)gPlayers;
        for (i = 0; i < 4; i++) {
            f32 dot;
            op = (Player*)(enemy + i * sizeof(Player));
            if (op == p) {
                continue;
            }
            if (op->state != 1) {
                continue;
            }
            out[0] = op->pos[0] - pos[0];
            out[1] = op->pos[1] - pos[1];
            out[2] = op->pos[2] - pos[2];
            dist = NormalVector(out);
            dot = out[0] * dir[0] + out[1] * dir[1] + out[2] * dir[2];
            if (dot < dotThresh) {
                continue;
            }
            dist = dist - PF(op, 0x850, f32);
            if (dist >= best) {
                continue;
            }
            best = dist;
        }
    }

    if (flag != 0) {
        if (id < 0) {
            id = *(s16*)gBossObj | 0x10000;
        }
    } else if (best >= limit) {
        f32 dx;
        f32 dz;
        f32 dy;
        id = -1;
        dx = pos[0] - PF(p, 0x64, f32);
        dz = pos[2] - PF(p, 0x6C, f32);
        dy = pos[1] - PF(p, 0x68, f32);
        d = fqdist(dx, dz);
        if (d < lbl_80347D50) {
            out[1] = lbl_80347B30;
        } else {
            out[1] = dy / d;
        }
        out[0] = sin(SV(p)->rot[1]);
        out[2] = cos(SV(p)->rot[1]);
        NormalVector(out);
    }

    if (id >= 0) {
        if (p->collision_item != NULL && **(s32**)p->collision_item == 7) {
            vec[0] = tx - pos[0];
            vec[1] = ty - pos[1];
            vec[2] = tz - pos[2];
            NormalVector(vec);
            vec[0] = vec[0] * (lbl_80347C28 * p->col_radius) + pos[0];
            vec[1] = vec[1] * (lbl_80347C28 * p->col_radius) + pos[1];
            vec[2] = vec[2] * (lbl_80347C28 * p->col_radius) + pos[2];
            dist = fn_8005F0F4((u8*)p->collision_item, (s32)pos, vec, 0,
                               p->col_radius, p->col_height);
            if (dist >= lbl_80347B08) {
                id = -1;
                best = limit;
            }
        }
    }

    if (outId != NULL) {
        *outId = id;
    }
    if (outObj != NULL) {
        *outObj = obj;
    }
    return best;
}
#pragma opt_common_subs reset
#pragma opt_lifetimes reset
/* NOTE: correct body; not yet byte-exact (far-field PF address-CSE parks an
 * extra nonvolatile -- needs a 0x93C..0x94C struct overlay; light-touch cap). */
typedef struct {
    u8  _pad[0x93C];
    s32 counter; /* 0x93C */
    s32 timer;   /* 0x940 */
} TransView;

s32 DoTransporter(Player* p, f32* pos, f32* out, f32 a) {
    TransView* tv = (TransView*)p;
    s32 timer = tv->timer;
    f32 local[3];
    f32 _pad[2];

    if (timer > 0) {
        s32 t = timer - gFrameTicks * 2;
        tv->timer = t;
        if (t < 0) {
            tv->timer = 0;
        }
        t = tv->timer;
        if (t >= 30) {
            MBTreeSetAlpha(p->node, 255 - (t - 30) * 255 / 30, 1);
        } else {
            MBTreeSetAlpha(p->node, t * 255 / 29, 1);
        }
        if (timer >= 30 && tv->timer < 30) {
            local[0] = p->transport_pos[0];
            local[1] = p->transport_pos[1];
            local[2] = p->transport_pos[2];
            FloorCollide(a, 4.0f, -10.0f, local, NULL, 0, 1);
            out[0] = local[0] - pos[0];
            out[2] = local[2] - pos[2];
            out[1] = gFloorCollisionResult[13] - p->pos[1];
            tv->counter = 1;
            msgPost(9, p->index, (u32)&p->col_pos);
            return 2;
        }
        return 1;
    } else {
        u8* tp = (u8*)fn_8005B8B0(p);
        if (tp != NULL) {
            if (tv->counter <= 0) {
                local[0] = PF(tp, 0x34, f32);
                local[1] = PF(tp, 0x38, f32);
                local[2] = PF(tp, 0x3C, f32);
                p->transport_pos[0] = local[0];
                p->transport_pos[1] = local[1];
                p->transport_pos[2] = local[2];
                if (PointVisible(-a, local) != 0) {
                    if (FloorCollide(a, 4.0f, -10.0f, local, NULL, 0, 1) != 0) {
                        fn_8009C98C(local);
                        tv->timer = 60;
                    }
                    return 1;
                }
            }
        } else {
            if (tv->counter > 0) {
                tv->counter = tv->counter - 1;
            }
        }
        return 0;
    }
}
void DoExit(Player* p) {
    s32 exiting;

    if (lbl_8034481C != 0 && (lbl_80344804 != 0 || lbl_80344808 != 0)) {
        p->state = 4;
        p->field_1F2 = 0;
        exiting = 1;
    } else {
        exiting = 0;
    }

    if (!exiting && sumnerSpeechActive() == 0) {
        switch (gTriggerCameraState) {
        case 0:
            break;
        default:
            return;
        }
        if (lbl_80344808 != 0) {
            p->idle_timer += gFrameTicks;
        } else if (fn_8005B8FC(p) != 0) {
            if (lbl_80344804 != 0 ||
                0.0 == (f64)lbl_80240E30[p->index].values[8]) {
                p->idle_timer += gFrameTicks;
            }
        } else {
            p->idle_timer = 0;
        }

        if (p->idle_timer >= 6) {
            p->state = 4;
            p->field_1F2 = 0;
            if (gBossType < 0 && lbl_80344768 > 1 && lbl_803447B4 == 0 &&
                lbl_8034481C < 3) {
                msgPost(11, p->index, (u32)&p->col_pos);
            }
        } else if (p->state == 4) {
            p->state = 1;
        }
    }
}
/* 0x8008760C - sweep the enemy grid around pos; for each eligible entry run
 * the collision test + resolve (fn_8005D730), track the best hit and the
 * caller's push-out, and remember the last blocking/opening entry on the
 * player.  Returns the number of hard collisions. */
s32 PlayerCollideEnemies(Player* p, s32 a2, f32* pos, f32* out, s32 a5,
                         s32* out2, f32 range, f32 p2) {
    f32 hit[3];
    f32 d;
    f32 best = lbl_80347B30;
    u8* item;
    u8* last = NULL;
    s32 lastResult = 0;
    s32 count = 0;
    s32 idx;

    if ((u32)(gBossType - 42) <= 1) {
        StartEnemyGrid(pos, (f32)(lbl_80347BB8 + range));
    } else {
        StartEnemyGrid(pos, range);
    }

    while ((idx = NextGridEnemy()) >= 0) {
        s32 result;

        item = sItems + idx * 240;

        {
            s32 skip = 0;
            switch (**(s32**)item) {
            case 1:
                if (PF(item, 0xE8, u32) != 0) {
                    skip = 1;
                }
                break;
            case 8: {
                s8 sub = *(s8*)(item + offsetof(PCollideItemLayout, action));
                if ((sub != 2 && sub != 4) ||
                    (*(s16*)(item + offsetof(PCollideItemLayout, active)) & 1) == 0) {
                    skip = 1;
                }
                break;
            }
            }
            if (skip) {
                continue;
            }
        }

        d = fn_8005F0F4(item, a2, pos, hit, range, p2);
        if (!(d >= 0.0)) {
            continue;
        }
        result = fn_8005D730(p, item);
        if (result != 0) {
            if (result == 1) {
                count++;
            }
            {
                switch (**(s32**)item) {
                case 9:
                case 11:
                    p->special_collision_item = item;
                    break;
                }
            }
        }
        if (last == NULL ||
            (result != 0 &&
             (d < best || lastResult == 0 || **(s32**)item == 10))) {
            best = d;
            last = item;
            lastResult = result;
            if (out != NULL) {
                out[0] = hit[0];
                out[1] = hit[1];
                out[2] = hit[2];
            }
            if (a5 != 0 && count != 0) {
                break;
            }
        }
    }

    if (a5 == 0) {
        p->collision_item = last;
    }
    if (out2 != NULL) {
        *out2 = (s32)last;
    }
    return count;
}
/* 0x80087830 - sweep the other three players along the movement segment
 * from->to; of those hit (and in front of the motion), pick the nearest and
 * push `out` back out of it.  Returns the collided player index or -1. */
s32 PlayerCollidePlayers(Player* p, f32 range, f32 p3, f32* from, f32* to,
                         f32* out, s32 stopFirst) {
    u8 unusedA[12];
    f32 hit[3];
    u8 unusedB[52];
    s32 i;
    s32 closest = -1;
    f32 best = 0.0f;
    Player* op;
    f32 dot;
    f32 d;
    f32 ex;
    f32 ez;
    f32 dist;
    f32 scale;

    for (i = 0; i < 4; i++) {
        op = &gPlayerRecords[i];
        if (i == p->index) {
            continue;
        }
        if (op->state != 1 && op->state != 4) {
            continue;
        }
        if ((op->hud_flags & 0x20) != 0) {
            continue;
        }
        dot = (op->effectpos[0] - from[0]) * (to[0] - from[0]) +
              (op->effectpos[2] - from[2]) * (to[2] - from[2]);
        if (dot < 0.0f) {
            continue;
        }
        if (LineCylinderCollide(op->effectpos,
                                range + op->col_radius, p3,
                                from, to, hit, 1) == 0) {
            continue;
        }
        d = fqdist(hit[0] - to[0], hit[2] - to[2]);
        if (closest < 0 || d < best) {
            closest = i;
            best = d;
        }
        if (stopFirst != 0) {
            break;
        }
    }

    if (closest >= 0) {
        op = &gPlayerRecords[closest];
        ex = to[0] - op->effectpos[0];
        ez = to[2] - op->effectpos[2];
        dist = fqdist(ex, ez);

        if (dist > lbl_80347D68) {
            scale = (range + op->col_radius - dist) / dist;
            out[0] = ex * scale + to[0];
            out[1] = lbl_80347B30 * scale + to[1];
            out[2] = ez * scale + to[2];
        } else {
            out[0] = from[0];
            out[1] = from[1];
            out[2] = from[2];
        }
    }
    return closest;
}
s32 PlayerCollideItems(Player* p, f32 range, f32 height, f32* from, f32* to,
                       f32* hit) {
    f32 localHit[12];
    volatile u8 unused[12];
    f32 best = lbl_80347B30;
    s32 closest = -1;
    s32 count = 0;
    s32 index;
    u8* object;

    StartItemGrid(range, to);
    {
    object = gEnemies;
    goto item_test;
item_body:
    {
        u8* item;
        s32 state;
        f32 collisionRange;
        f32 collisionHeight;
        f32 dx;
        f32 dy;
        f32 dz;
        f32 distance;

        item = object + index * 0x394;
        state = *(s32*)(item + offsetof(PCollideEnemyLayout, state));
        if (state != 1 && state != 6 &&
            (state != 8 || lbl_803447DC == 0)) {
            goto item_test;
        }
        if (*(s32*)(item + offsetof(PCollideEnemyLayout, type)) == 0x1F) {
            goto item_test;
        }

        collisionRange = range + *(f32*)(item + offsetof(PCollideEnemyLayout, rad));
        collisionHeight = height + *(f32*)(item + offsetof(PCollideEnemyLayout, hht));
        dx = *(f32*)(item + offsetof(PCollideEnemyLayout, coll_pos[0])) - to[0];
        dy = *(f32*)(item + offsetof(PCollideEnemyLayout, coll_pos[1])) - to[1];
        dz = *(f32*)(item + offsetof(PCollideEnemyLayout, coll_pos[2])) - to[2];
        if (dx * dx + dz * dz < collisionRange * collisionRange &&
            fabsf_(dy) < *(f32*)(item + offsetof(PCollideEnemyLayout, hht)) &&
            LineCylinderCollide((f32*)(item + offsetof(PCollideEnemyLayout, coll_pos)), collisionRange,
                                collisionHeight, from, to,
                                localHit, 1) != 0) {
                distance = fqdist(localHit[0] - to[0], localHit[2] - to[2]);
                if (closest < 0 || distance < best) {
                    best = distance;
                    closest = index;
                    hit[0] = localHit[0];
                    hit[1] = localHit[1];
                    hit[2] = localHit[2];
                }
                count++;
        }
    }
item_test:
    if ((index = NextGridItem()) >= 0) {
        goto item_body;
    }
    }

    if (closest >= 0) {
        u8* item = gEnemies + closest * 0x394;
        if (FastWallCollide(from, (f32*)(item + offsetof(PCollideEnemyLayout, coll_pos)), 0, 0) != 0) {
            closest = -1;
        }
    }

    CritterCollideStart(range, to, 0);
    {
        object = CritterMoveNodeCol(range, lbl_80347B30, from, to,
                                    localHit, -1, 2);
        if (object != 0 &&
            *(s16*)(*(u8**)(*(u8**)(object + 4) + 0x120) + 0x20) == 4 &&
            (p->obj_flags & 0x8000) != 0) {
            object = 0;
        }
        if (object != 0) {
            f32 distance = fqdist(localHit[0] - to[0], localHit[2] - to[2]);
            if (closest < 0 || distance < best) {
                closest = *(s16*)object | 0x10000;
                hit[0] = localHit[0];
                hit[1] = localHit[1];
                hit[2] = localHit[2];
            }
            count++;
        }
    }

    if (closest < 0) {
        hit[0] = from[0];
        hit[1] = from[1];
        hit[2] = from[2];
    } else if (lbl_80240E30[p->index].control.flag == 0) {
        if (count == 1) {
            f32 dx;
            f32 dz;
            f32 distance;
            f32 zero;
            s32 radius;

            if (closest >= 0x10000) {
                u8* critter = gCritterPool + (closest & 0xFFFF) * 0xAE0;
                zero = lbl_80347B30;
                dx = to[0] - *(f32*)(critter + offsetof(PCollideCritterLayout, pos[0]));
                dz = to[2] - *(f32*)(critter + offsetof(PCollideCritterLayout, pos[2]));
                radius = (s32)*(f32*)(*(u8**)(critter + offsetof(PCollideCritterLayout, hdr)) + 0x7C);
            } else {
                u8* item = gEnemies + closest * 0x394;
                zero = lbl_80347B30;
                dx = to[0] - *(f32*)(item + offsetof(PCollideEnemyLayout, coll_pos[0]));
                dz = to[2] - *(f32*)(item + offsetof(PCollideEnemyLayout, coll_pos[2]));
                radius = (s32)*(f32*)(item + offsetof(PCollideEnemyLayout, rad));
            }
            distance = fqdist(dx, dz);
            if (distance > lbl_80347D68) {
                f32 scale = (range + radius - distance) / distance;
                hit[0] = dx * scale + to[0];
                hit[1] = zero * scale + to[1];
                hit[2] = dz * scale + to[2];
            } else {
                hit[0] = from[0];
                hit[1] = from[1];
                hit[2] = from[2];
            }
        } else {
            hit[0] = from[0];
            hit[1] = from[1];
            hit[2] = from[2];
        }
    }
    return closest;
}
extern f32 lbl_80347B30; /* 0.0f (sdata2) */
extern f64 lbl_80347BE8; /* 0.01 (sdata2) */

int PlayerNewFloor(PMotionCtx* m, Player* p, f32* dpos) {
    WorldObj* mf = p->floor_name2;  /* floor-object cache; see PSpawnView.floor_obj */
    s32 result;

    if (mf != NULL && (mf->flags & 0xC000000) != 0 &&
        (mf->flags & 0x20000000) != 0 && mf != m->floor) {
        dpos[0] = 0.0f;
        dpos[1] = 0.0f;
        dpos[2] = 0.0f;
        return 0;
    }

    CopyMat3((f32*)m, (f32*)p->mbnode);
    result = PlayerCheckFloor(p, m->floor, dpos);

    if (m->floor != NULL && (m->floor->flags & 8) != 0) {
        f32 d1 = fqdist(dpos[0], dpos[2]);
        f32 d2 = fqdist(d1, dpos[1]);
        if (d1 > 0.01 && d2 > 0.01 &&
            ((SV(p)->floor_flags & 8) == 0 || fabsf_(dpos[1]) > 0.01)) {
            p->field_8BC = dpos[1] / d2;
        }
    } else {
        {
            f32 t1 = m->fwd[0] * p->mat[9] - m->fwd[1] * p->mat[8];
            f32 t2 = m->fwd[1] * p->mat[10] - m->fwd[2] * p->mat[9];
            p->field_8BC = t1 * m->fwd[0] - t2 * m->fwd[2];
        }
    }

    SV(p)->floor_flags = m->floor != NULL ? m->floor->flags : 0;
    return result;
}
int PlayerCheckFloor(Player* p, WorldObj* obj, f32* dpos) {
    WorldObj* cur;
    s32 result = 0;

    if (obj != NULL && obj->nodeptr != NULL && (obj->flags & 0x1000) != 0) {
        if (OtherPlayerOnOtherMovingObject(p->index, (u8*)obj) != 0) {
            result = 1;
        } else {
            MBNodeSetParent(p->node, obj->nodeptr);
        }
    }

    if (result != 0) {
        cur = p->floor_name2;
    } else {
        cur = obj;
    }
    if (cur == NULL || (cur->flags & 0x1000) == 0) {
        MBNodeSetParent(p->node, lbl_80344B2C);
    }

    if (obj != p->floor_name2) {
        p->hud_flags |= 1;
    }
    p->floor_name2 = obj;

    if (result != 0) {
        dpos[0] = 0.0f;
        dpos[2] = 0.0f;
    }
    return result;
}

extern f64 lbl_80347B28;
extern f32 lbl_80347B30;
extern f64 lbl_80347D68;
extern f64 lbl_80347D10;
extern f32 lbl_80347BF8;
extern f64 lbl_80347BD0;
extern f64 lbl_80347D70;
extern f64 lbl_80347BA8;
extern u32 lbl_80344B38; /* last floor WorldObj hit (address word) */
extern f32 lbl_80344B34;
extern u8  lbl_8023CB28[];

/* 0x80088068 - clamp a player displacement against the floor mesh: snap to
 * the hit height, slide on steep slopes, honour moving-floor ownership. */
s32 PlayerCollideFloor(u8* p, f32* pos, f32* dpos, s32 mode, f32 rad,
                       f32 height)
{
    f32 end[3];
    u8 padend[28];
    f32 nrm[3];
    f32 ts;
    f32 ts2;
    f32 tsa;
    u8* ctx;
    u8* fhp;
    f32 zoff;
    f32 dq;
    f32 fh;
    f32 dx;
    f32 dz;
    f32 d;
    f32 lim;
    u32 hit;
    s32 fA;
    s32 fB;
    f32 zv;
    Player* pd = (Player*)p;
    PSpawnView* sv = SV(p);  /* floor_obj cache @0x8C4, same slot get_player_pos clears */

    ctx = lbl_80282850;
    end[0] = pos[0] + dpos[0];
    zoff = (f32)-(lbl_80347B28 + height);
    end[1] = pos[1] + dpos[1];
    end[2] = pos[2] + dpos[2];
    hit = FloorCollide(rad, lbl_80347B30, zoff, end, (f32*)(ctx + 24), 1, 1);
    lbl_80344B38 = *(s32*)(lbl_8023CB28 + 68);
    lbl_80344B34 = *(f32*)(lbl_8023CB28 + 52);
    dq = fqdist(dpos[0], dpos[2]);
    {
        u32 fl = WorldObjGetAllFlags((WorldObj*)sv->floor_obj);
        fA = fl & 0x0C000000;
        fB = fl & 0x20000000;
    }
    if (fA != 0 && fB != 0 && sv->floor_obj != hit) {
        zv = lbl_80347B30;
        dpos[0] = zv;
        dpos[1] = zv;
        dpos[2] = zv;
        if (hit != 0) {
            return -1;
        }
        return -2;
    }
    if (hit == 0) {
        if ((f64)dq < lbl_80347D68) {
            sv->floor_obj = 0;
        }
        if (!(pd->obj_flags & 0x8000)) {
            zv = lbl_80347B30;
            dpos[0] = zv;
            dpos[2] = zv;
        }
        pd->floor_base = lbl_80344880;
        return -2;
    }
    fhp = ctx + 76;
    fh = *(f32*)(ctx + 76);
    ts = fh - pd->floor_base;
    *(u32*)&ts &= 0x7FFFFFFF;
    tsa = ts;
    if (pd->obj_flags & 0x8000) {
        if ((f64)(pd->pos[1] - fh) < lbl_80347D10) {
            pd->obj_flags &= ~0x8000;
        } else {
            return 0;
        }
    }
    if ((f64)dq < lbl_80347D68 && pd->prev_state == 1) {
        if ((f64)tsa > lbl_80347B28 || (*(u32*)(hit + 16) & 0x1000)) {
            pd->floor_base = fh;
        }
        if ((f64)tsa > lbl_80347D68) {
            return 1;
        }
        return 0;
    }
    lim = lbl_80347BF8;
    if (*(u32*)(ctx + 92) == sv->floor_obj) {
        lim = (f32)(lim + lbl_80347BD0);
    }
    if (tsa > lim) {
        pd->floor_base = pd->pos[1];
        zv = lbl_80347B30;
        dpos[0] = zv;
        dpos[2] = zv;
        return 0;
    }
    if (mode == 2 && hit != (u32)lbl_80344B30) {
        SlideAlongWall(pos, dpos, ctx, (f32*)(lbl_8023CA98 + 16), rad);
        end[0] = pos[0] + dpos[0];
        end[1] = pos[1] + dpos[1];
        end[2] = pos[2] + dpos[2];
        hit = FloorCollide(rad, lbl_80347B30, zoff, end, (f32*)(ctx + 24), 0,
                           1);
        if (fA != 0 && fB != 0 && sv->floor_obj != hit) {
            zv = lbl_80347B30;
            dpos[0] = zv;
            dpos[1] = zv;
            dpos[2] = zv;
            return -1;
        }
        if (hit != 0) {
            pd->floor_base = *(f32*)fhp;
            return 2;
        }
        zv = lbl_80347B30;
        dpos[0] = zv;
        dpos[2] = zv;
        return 0;
    }
    dx = end[0] - *(f32*)(ctx + 72);
    dz = end[2] - *(f32*)(ctx + 80);
    dq = dpos[1] * (end[1] - *(f32*)fhp) + dpos[0] * dx + dpos[2] * dz;
    d = fqdist(dx, dz);
    if ((f64)d < lbl_80347D68 && mode == 0) {
        nrm[0] = dpos[0];
        nrm[1] = dpos[1];
        nrm[2] = dpos[2];
        NormalVector(nrm);
        nrm[0] = nrm[0] * rad;
        nrm[1] = nrm[1] * rad;
        nrm[2] = nrm[2] * rad;
        end[0] = end[0] + nrm[0];
        end[1] = end[1] + nrm[1];
        end[2] = end[2] + nrm[2];
        hit = FloorCollide(rad, lbl_80347B30, zoff, end, 0, 1, 1);
        if (fA != 0 && fB != 0 && sv->floor_obj != hit) {
            zv = lbl_80347B30;
            dpos[0] = zv;
            dpos[1] = zv;
            dpos[2] = zv;
            return -1;
        }
        if (hit != 0) {
            ts2 = gFloorCollisionResult[13] - fh;
            *(u32*)&ts2 &= 0x7FFFFFFF;
            if ((*(u32*)(*(u32*)((u8*)gFloorCollisionResult + 68) + 16) & 8)
                && (f64)ts2 < lbl_80347BA8) {
                d = lbl_80347B30;
            } else {
                dx = end[0] - *(f32*)((u8*)gFloorCollisionResult + 48);
                dz = end[2] - *(f32*)((u8*)gFloorCollisionResult + 56);
                dq = dpos[1] *
                         (end[1] - gFloorCollisionResult[13]) +
                     dpos[0] * dx + dpos[2] * dz;
                d = fqdist(dx, dz);
            }
        } else {
            d = rad;
            dx = nrm[0];
            dz = nrm[2];
            dq = lbl_80347B30;
        }
    }
    if ((f64)d < lbl_80347D68 || dq < (zv = lbl_80347B30)) {
        pd->floor_base = fh;
        if ((f64)d < lbl_80347D68 || (f64)dq < lbl_80347D70) {
            return 1;
        }
        return 2;
    }
    dpos[0] = dpos[0] - dx;
    dpos[2] = dpos[2] - dz;
    end[0] = pos[0] + dpos[0];
    end[1] = pos[1] + dpos[1];
    end[2] = pos[2] + dpos[2];
    hit = FloorCollide(rad, zv, zoff, end, (f32*)(ctx + 24), 0, 1);
    if (fA != 0 && fB != 0 && sv->floor_obj != hit) {
        zv = lbl_80347B30;
        dpos[0] = zv;
        dpos[1] = zv;
        dpos[2] = zv;
        return -1;
    }
    if (hit != 0) {
        pd->floor_base = *(f32*)fhp;
        return 2;
    }
    zv = lbl_80347B30;
    dpos[0] = zv;
    dpos[2] = zv;
    return 0;
}

int PlayerCheckMovingFloor_80088688(Player* p) {
    f32 drop = -(3.0 + (f64)p->col_height);
    if (gGameMode == 0x4010) {
        p->floor_name2 = (WorldObj*)FloorCollide(p->col_radius, 0.0f, drop,
            p->pos, NULL, 1, 0);
        p->hud_flags |= 1;
    }
    if (p->floor_name2 != NULL) {
        return 1;
    }
    return 0;
}

/* 0x80088714 - collide the motion segment pos->pos+dpos against the world
 * walls, slide `dpos` along the hit wall (or project it for one-way walls),
 * then re-test; if the second wall opposes the first (wedged), stop the move.
 * Returns 0 (no wall), 1 (slid / exit wall on the right anim), or 2. */
s32 fn_80088714(f32 range, Player* p, f32* pos, f32* dpos) {
    f32 to[3];
    u8 unused[16];
    WorldObj* wall;
    u8* ctx = lbl_80282850;
    f32* wn;
    f32* ctxY;
    f32* ctxZ;
    f32* wnY;
    f32* wnX;
    f32* wnZ;
    s32 result = 0;

    to[0] = pos[0] + dpos[0];
    to[1] = pos[1] + dpos[1];
    to[2] = pos[2] + dpos[2];
    lbl_80344B30 = PlayerWallCollide(pos, to, ctx, range);
    wall = (WorldObj*)lbl_80344B30;
    if (wall != NULL) {
        wn = (f32*)&lbl_8023CA98[0x10];
        wnX = &wn[0];
        wnY = &wn[1];
        wnZ = &wn[2];
        ctxY = (f32*)(ctx + 16);
        ctxZ = (f32*)(ctx + 20);
        *(f32*)(ctx + 12) = *wnX;
        result = 1;
        *ctxY = *wnY;
        *ctxZ = *wnZ;

        if ((wall->flags & 0x38) != 0) {
            return p->anim_208 == 0x8F ? 1 : 2;
        } else {
            if ((wall->flags & 0x1000) != 0) {
                f32 d = -(dpos[1] * *ctxY +
                          dpos[0] * *(f32*)(ctx + 12) +
                          dpos[2] * *ctxZ);
                dpos[0] = *(f32*)(ctx + 12) * d + dpos[0];
                dpos[1] = *ctxY * d + dpos[1];
                dpos[2] = *ctxZ * d + dpos[2];
            } else {
                SlideAlongWall(pos, dpos, ctx, (f32*)(ctx + 12), range);
            }

            to[0] = pos[0] + dpos[0];
            to[1] = pos[1] + dpos[1];
            to[2] = pos[2] + dpos[2];
            (*(u8**)&gWorldInfo[0x5C])[lbl_80344180]++;
            wall = (WorldObj*)PlayerWallCollide(
                pos, to, ctx, (f32)(lbl_80347D78 * range));
            if (wall != NULL) {
                lbl_80344B30 = wall;
                if (*wnY * *ctxY + *wnX * *(f32*)(ctx + 12) +
                        *wnZ * *ctxZ < lbl_80347B00) {
                    dpos[0] = 0.0f;
                    dpos[1] = 0.0f;
                    dpos[2] = 0.0f;
                }
            }
        }
    }
    return result;
}
s32 fn_80088938(Player* p, f32 angle) {
    ControlState* ctl = &lbl_80240E30[p->index];
    PlayerActionMotionView* motion = (PlayerActionMotionView*)p;
    f64 wrapped;
    f32 facing;
    s32 action;

    angle = angle + ctl->pad.lx - p->move_yaw;
    if ((f64)angle > lbl_80347B50) {
        wrapped = (f64)angle - lbl_80347B60;
    } else if ((f64)angle <= lbl_80347B68) {
        wrapped = lbl_80347B60 + (f64)angle;
    } else {
        wrapped = (f64)angle;
    }
    facing = (f32)wrapped;

    if (p->quest_state != 0 && gBossType >= 0) {
        ctl->pad.edges &= ~0x900;
        ctl->pad.levels &= ~0x900;
    }

    if ((p->flags & 0x400) != 0 &&
        (ctl->pad.levels & 0x400) != 0) {
        ctl->pad.levels &= ~0x400;
        ctl->pad.levels |= 0x200;
    }

    if ((gControllerButtons & 0x10) != 0 || gGameOptions[6] != 0) {
        if ((ctl->pad.edges & 0x2000) != 0) {
            ctl->pad.edges &= ~0x2000;
            ctl->pad.edges |= 0x800;
        }
    }

    action = 0;
    if ((p->hud_flags & 0x40) != 0) {
        action = 0x27;
    } else if ((p->hud_flags & 0x10) != 0) {
        action = 0x26;
    } else if ((p->hud_flags & 0x80) != 0) {
        action = 0x17;
    } else if (((gControllerButtons & 0x10) != 0 || gGameOptions[6] != 0) &&
               (ctl->pad.levels & 0x08000000) != 0) {
        action = 0x1D;
    }

    if (action > 0) {
        goto final_action;
    }
    if ((ctl->pad.levels & 0x10000) != 0) {
            if ((motion->actionFlags & 0x80) == 0) {
            action = 0x19;
        }
    } else if ((ctl->pad.levels & 0x8000) != 0) {
        if ((motion->actionFlags & 0x80) == 0) {
            action = 0x1A;
        }
    } else if ((ctl->pad.levels & 0x100) != 0) {
        if ((motion->actionFlags & 0x80) == 0) {
            action = 0x18;
        }
    } else if (motion->actionFlags == 0x80) {
        motion->actionFlags = 0;
    }
    if (action > 0) {
        goto final_action;
    }
    if ((ctl->pad.levels & 0x20000) != 0 &&
        (f64)p->power_target >= lbl_80347BB8) {
        s32 target = fn_80088EF4(p, lbl_80347C6C, lbl_80347D08);
        if (target >= 0) {
            p->grab_pending = &gPlayerRecords[target];
            action = 0x16;
            goto final_action;
        }
    }

    if ((ctl->pad.levels & 0x800) != 0 &&
        (ctl->pad.edges & 0x200) != 0 &&
        (f64)p->power_target >= lbl_80347B08) {
        action = 0x15;
        goto final_action;
    }

    if ((ctl->pad.edges & 0x1000) != 0) {
        action = 2;
        goto final_action;
    } else if ((f64)ctl->pad.ry > lbl_80347B08) {
        if ((f64)ctl->pad.ly > lbl_80347B08) {
            f64 delta = ctl->pad.rx - ctl->pad.lx;
            f32 dir;
            if (delta > lbl_80347B50) {
                delta -= lbl_80347B60;
            } else if (delta <= lbl_80347B68) {
                delta = lbl_80347B60 + delta;
            }
            dir = (f32)delta;
            if ((f64)dir > lbl_80347CE8 || (f64)dir < lbl_80347D80) {
                action = 0x12;
            } else if ((f64)dir > lbl_80347D88) {
                action = 0x14;
            } else if ((f64)dir < lbl_80347D90) {
                action = 0x13;
            } else {
                action = 0x11;
            }
        } else if ((ctl->pad.levels & 0x400) != 0) {
            action = 0x10;
        } else {
            action = 0x0F;
        }
        goto selected;
    } else if ((ctl->pad.levels & 0x4000) != 0 &&
               (f64)ctl->pad.ly > lbl_80347B08) {
        if ((ctl->pad.levels & 0x600) != 0) {
            if ((f64)facing > lbl_80347CE8 || (f64)facing < lbl_80347D80) {
                action = 0x12;
            } else if ((f64)facing > lbl_80347D88) {
                action = 0x14;
            } else if ((f64)facing < lbl_80347D90) {
                action = 0x13;
            } else {
                action = 0x11;
            }
        } else {
            if ((f64)facing > lbl_80347CE8 || (f64)facing < lbl_80347D80) {
                action = 0x0A;
            } else if ((f64)facing > lbl_80347D88) {
                action = 0x0C;
            } else if ((f64)facing < lbl_80347D90) {
                action = 0x0B;
            } else {
                action = 9;
            }
        }
        goto selected;
    } else if ((ctl->pad.edges & 0x2000) != 0 &&
               (f64)p->power_target >= lbl_80347C78) {
        action = 7;
        goto selected;
    } else if ((ctl->pad.levels & 0x200) != 0) {
        action = 0x0F;
        goto selected;
    } else if ((ctl->pad.levels & 0x400) != 0) {
        action = 0x10;
    }

selected:
    if (action <= 0) {
        if ((f64)ctl->pad.ly > lbl_80347C00) {
            action = 0x0D;
        } else if ((f64)ctl->pad.ly > *(volatile f64*)&lbl_80347B08) {
            action = 8;
        } else {
            action = 1;
        }
    }
final_action:
    switch (p->anim_208) {
    case 0x73:
        if (action == 0x18) {
            if ((motion->actionFlags & 4) != 0) {
                motion->actionFlags |= 2;
            }
        } else {
            motion->actionFlags |= 4;
        }
        break;
    case 0x74:
        break;
    case 0x75:
        if ((u32)(action - 0x18) <= 1) {
            if ((motion->actionFlags & 8) == 0) {
                motion->actionTicks += gFrameTicks;
            }
        } else {
            motion->actionFlags |= 8;
        }
        break;
    }

    if ((ctl->pad.levels & 0x600) != 0) {
        motion->edgeHistory |= ctl->pad.levels ^ motion->heldHistory;
        motion->heldHistory |= ctl->pad.levels;
    } else if (motion->heldHistory != 0) {
        motion->heldHistory = 0;
    }
    return action;
}
/* 0x80088EF4 - find the nearest other player inside p's forward-facing cone
 * (within maxDist, dot >= minDot), gated by a stack of state/anim/floor
 * eligibility checks.  Returns that player's index, or -1 if none. */
s32 fn_80088EF4(Player* p, f32 range, f32 minDot) {
    f32 diff[3];
    f32 face[3];
    u8 unused[4];
    f32 maxDist = range;
    s32 i;
    s32 closest = -1;
    WorldObj* floor;

    if ((u32)p->grab_partner != 0 || (u32)p->grab_pending != 0 ||
        (u32)p->grab_node == 0) {
        return -1;
    }
    if ((p->flags & 0x400) != 0) {
        return -1;
    }
    if (p->state != 1) {
        return -1;
    }
    if (p->power_target < lbl_80347BB8) {
        return -1;
    }
    floor = (WorldObj*)SV(p)->floor_obj;
    if (floor == NULL || (floor->flags & 0x1000) != 0) {
        return -1;
    }

    face[0] = p->mat[8];
    face[1] = p->mat[9];
    face[2] = p->mat[10];
    NormalVector2D(face);

    for (i = 0; i < 4; i++) {
        Player* op = &gPlayerRecords[i];
        s32 anim;
        f32 d;

        if (i == p->index) {
            continue;
        }
        if (op->state != 1) {
            continue;
        }
        if ((u32)op->grab_partner != 0) {
            continue;
        }
        if ((op->hud_flags & 0x50) != 0) {
            continue;
        }
        anim = op->anim_208;
        if ((anim >= 0x54 && anim < 0x5B) || anim >= 0x6B) {
            continue;
        }
        if ((op->flags & 0x400) != 0) {
            continue;
        }
        if (p->quest_state != 0 && gBossType >= 0) {
            continue;
        }
        floor = (WorldObj*)SV(op)->floor_obj;
        if (floor == NULL || (floor->flags & 0x1000) != 0) {
            continue;
        }
        diff[0] = op->pos[0] - p->pos[0];
        diff[1] = op->pos[1] - p->pos[1];
        diff[2] = op->pos[2] - p->pos[2];
        if (fabsf_param(diff[1]) > 3.0) {
            continue;
        }
        d = NormalVector2D(diff);
        if (d > maxDist) {
            continue;
        }
        if (diff[0] * face[0] + diff[2] * face[2] < minDot) {
            continue;
        }
        closest = i;
        maxDist = d;
    }

    return closest;
}

#undef STUB
