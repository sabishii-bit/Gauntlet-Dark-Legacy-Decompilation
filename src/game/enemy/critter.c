/*
 * critter.c -- GCN CRITTER.OBJ.
 *
 * The critter behavior/load object between CONTROLS.OBJ and the sound-manager
 * object.  Critters are the large, scripted, multi-part creatures (golems,
 * bosses, generals) distinct from the swarm-style Enemy record.
 *
 * Bodies are transcribed from the GC (GUNE5D) DOL asm (tools/gdl/fnasm.py) with
 * Ghidra structure hints; function names are recovered from the Xbox
 * CRITTER.OBJ roster where cross-version behavior makes the mapping clear.
 * Every target function now has a translated body; remaining work is compiler
 * matching and replacing raw field offsets with recovered structures.
 *
 * .text       0x80034CFC..0x8004229C
 * extab       0x80005CE0..0x80005F28
 * extabindex  0x800093A0..0x8000970C
 */
#include "types.h"
#include "game/critter.h"
#include "game/effect.h"
#include "game/enemy.h"
#include "game/player.h"

/* -- module-local BigState siblings (bss, pooled off gBig) -- */
typedef struct CritterBigState {
    f32 scratch[4];
    f32 safeRockTimers[16];
    s32 safeRockIndices[16];
    u8 _pad090[0x1A4];
    Critter pool[16];
} CritterBigState;

typedef struct CritterHitNode {
    void *descriptor;
    void *active;
    u8 _pad08[0x34];
    f32 position[3];
    u8 _pad48[0x0C];
    f32 activeUntil;
    f32 activeFrom;
} CritterHitNode;

typedef struct CritterPattern {
    u8 _pad00[0x10];
    s16 flags;
    u8 _pad12[2];
    f32 cooldown;
    u8 _pad18[8];
    s16 move;
    s16 sequence[7];
    u8 _pad30[0x20];
} CritterPattern;

extern CritterBigState gBig;
extern void *lbl_80241060[4];         /* 0x80241060 loaded-file handle table    */
extern u8    lbl_80241070[4][0x50];   /* 0x80241070 per-type header buffers      */
extern Player gPlayers[4];        /* 0x80275AE0 player records (gPlayerRecords) */

/* -- module-local sbss variables -- */
extern void *lbl_80344648;            /* 0x80344648 pending callback context     */
extern s32   lbl_80344644;            /* 0x80344644 pending callback flag        */
extern s32   lbl_8034465C;            /* 0x8034465C active-player count           */
extern s16   lbl_80344664;            /* 0x80344664 rolling tick counter          */
extern s32   lbl_80344660;            /* 0x80344660 loaded-type count             */
extern s32   lbl_8034466C;            /* 0x8034466C active critter count (gNumCritters) */
extern s32   lbl_80344650;            /* 0x80344650 safe-rock collection flags    */
extern s32   lbl_80344654;            /* 0x80344654 selected safe-rock slot        */
extern s32   lbl_80344658;            /* 0x80344658 collected safe-rock count      */
extern f32   gClockFrameStep;         /* 0x80344590 frame delta                     */
extern f32   lbl_803447D8;            /* boss/player damage scaling gate             */
extern s32   sMusicTrackHi;
extern void *lbl_80344EB4;
extern f32   lbl_80343BEC;            /* 0x80343BEC tunable float (10.0)             */
extern volatile f32 sMusicFadeBase;   /* 0x80344594 shared game-time / fade base   */
extern f32   lbl_80346480;
extern f32   lbl_80346470;
extern f64   lbl_80346478;
extern f64   lbl_80346488;
extern f64   lbl_80346490;
extern f64   lbl_80346498;
extern f64   lbl_803464A0;
extern f64   lbl_803464B0;
extern f32   lbl_803464B8;
extern f32   lbl_803464BC;
extern f64   lbl_803464F8;
extern f64   lbl_803464C8;
extern f64   lbl_803464D0;
extern f64   lbl_803464D8;
extern f64   lbl_803464E0;
extern f64   lbl_80346500;
extern f32   lbl_80346590;
extern f32   lbl_80346594;
extern f32   lbl_80346598;
extern f64   lbl_803465A0;
extern f64   lbl_803465A8;
extern f64   lbl_803465B0;
extern f64   lbl_80346550;
extern f32   lbl_803464C0;
extern f32   lbl_803465F8;
extern f32   lbl_80346508;
extern f64   lbl_80346558;
extern f64   lbl_80346510;
extern f32   lbl_80346518;
extern f32   lbl_8034651C;
extern f32   lbl_80346520;
extern f32   lbl_80346524;
extern f64   lbl_80346528;
extern f64   lbl_80346530;
extern f64   lbl_80346538;
extern f64   lbl_80346540;
extern f32   lbl_803465F4;
extern f32   lbl_80346548;
extern f32   lbl_8034654C;
extern f32   lbl_80346560;
extern f64   lbl_80346568;
extern const char lbl_80346574[];
extern f64   lbl_80346580;
extern f64   lbl_80346600;
extern f64   lbl_80346630;
extern f32   lbl_8034663C;
extern f32   lbl_80346638;
extern f64   lbl_80346620;
extern f32   lbl_80346628;
extern f32   lbl_8034662C;
extern f32   lbl_803464A8;
extern f32   lbl_803464E8;
extern f32   gClockTime;
extern char  lbl_801121D4[];
extern char  lbl_80112174[];
extern Effect Effects[];
extern void  MBPsysSetEVolume(void *psys, f32 a, f32 b);
extern void  MBPsysSetPParm(void *psys, s32 n, f32 a, f32 b, f32 c, f32 d);

/* -- external helpers -- */
extern void *AllocFile(const char *wad, const char *name);
extern void *NextWaypoint(void *player);
extern void  AddExp(s32 player, s32 amount, s32 kind);
extern void  HealthMeterUpdate(f32 value, s32 meter);
extern void *memset(void *dst, int c, u32 n);
extern void *memcpy(void *dst, const void *src, u32 n);
extern void  ErrorPrintf(const char *fmt, ...);
extern void  FatalError(const char *msg, int code);
extern void  MBRemoveNode(void *node, s32 kind);
extern void  MBSetObject(void *node, s32 object);
extern s32   GetWorldMat(void *node, f32 *matrix, f32 *offset);
extern void  GetYawPitch(const f32 *vector, f32 *yaw, f32 *pitch);
extern void  ExtractPYR(void *matrix, f32 *angles);
extern void  CreatePYRMatrix(void *matrix, const f32 *angles);
extern s32   HealthMeterStart(void *header, s32 x, s32 y, s32 width,
                              s32 height, s32 style, f32 health);
extern void *AtreeMatch(void *header, const char *name, s32 report);
extern void *AtreeInit(void *header, void *tree, s32 flags, s32 size);
extern void  MBNodeSetParent(void *node, void *parent);
extern void  MBTreeSetFlags(void *node, u32 flags, s32 mode);
extern void  MBTreeSetAltTex(void *node, u32 mask, u32 texture, s32 mode);
extern void  MBTreeSetAmbientAdd(void *node, s32 value, s32 mode);
extern void  MBTreeSetZsortAdd(void *node, s32 value, s32 mode);
extern void *AtreeFindNode(void *tree, const char *name, s32 length);
extern s32   AtreeFindNodeIdx(void *tree, s32 count, const char *name,
                              s32 length);
extern void  AtreeNodeSetParent(void *node, void *parent, void *root,
                                s32 reparent);
extern void  SfxDeleteParented(void *sfx, s32 a, s32 b);
extern void  BossDeath(void);
extern void  del_target(void *mtx);
extern void  AtreeDelete(void *handle);
extern s32   CollectSafeRocks(s32 *out, s32 max, s32 flags);
extern void  SafeRockActivate(s32 index);
extern u32   RandInt(u32 limit);
extern s32   NextGridEnemy(void);
extern void  StartEnemyGrid(f32 *position, f32 radius);
extern s32   fn_8005D5C8(Critter *c, u8 *item);
extern void *FindClosestWaypoint(f64 maxDist, f32 *pos, s32 all);
extern f32   fn_8005F0F4(void *item, f32 *nodepos, f32 *center, f32 *out,
                         f32 radius, f32 height);
extern f32   fn_8005C1DC(u8 *item, s32 a, s32 b, void *hdr, f32 damage);
extern s32   NextGridItem(void);
extern void  StartItemGrid(f32 radius, f32 *position);
extern void  MulVecMat3(const f32 *vector, f32 *out, const f32 *matrix);
extern void  MulVecMat4(const f32 *vector, f32 *out, const f32 *matrix);
extern void  MulBodyVecMat4(const f32 *vector, f32 *out, const f32 *matrix);
extern s32   LineCylinderCollide(f32 *p1, f32 a, f32 b, f32 *p2, f32 *dest,
                                 f32 *contact, s32 flag);
extern void  damage_enemy(void *enemy, s32 a, s32 b, f32 radius, void *p1,
                          void *p2, s32 c);
extern s32   lbl_803447DC;
extern s32   damage_player(s32 player, f32 damage, s32 mode, u32 flags,
                           f32 *direction);
extern f32   NormalVector(f32 *vector);
extern f32   NormalVector2D(f32 *vector);
extern u32   WorldObjGetAllFlags(void *object);
extern f32   SlowNormalVector(f32 *vector);
extern f32   fqdist(f32 x, f32 z);
extern s32   fn_8005FB48(f32 radius, f32 *from, f32 *to,
                         f32 *limitPosition, s32 stopAtFirst);
extern void  ProcessSkinFX(f32 *state, void *root, void *node);
extern void *MBNewNode(void *parent, const f32 *matrix, s32 mode);
extern void  CopyMat3(const f32 *src, f32 *dst);
extern void  CopyMat4(const f32 *src, f32 *dst);
extern void  MulVec4Mat3(const f32 *src, f32 *dst, const f32 *matrix);
extern void  UnparentMatrix(void *node, f32 *matrix);
extern void *lbl_8034473C;
extern s32   gBossType;
extern f32   lbl_8011AEAC[];
extern s32   gFrameTicks;
extern u32   lbl_80344BF8;
extern u8    lbl_802411B0[0x540];
extern s32   lbl_80344668;
extern void *crit_load_desc;
extern s32  *lbl_80344640;
extern s32   lbl_80344630;
extern s32   lbl_80344634;
extern s32   lbl_80344638;
extern char  lbl_801120E0[];          /* 0x801120E0 rodata format-string anchor    */
extern s32  *lbl_8025776C[8];         /* 0x8025776C item/def pointer table          */
DECL_SECT(".sdata2") extern const char lbl_8034664C[]; /* 0x8034664C wad name       */
extern void *gWorldData;              /* 0x80344838 world data record                */
extern s32   FileSize(char *name, const char *wad);
extern s32  *StartFileRead(char *name, const char *wad, s32 mode, s32 size,
                           s32 arg, void *callback);
extern void  fn_8001267C(s32 handle, s32 index, s32 flag);
extern void  InitTexMods(s32 handle, s32 index);
extern s32   LoadModel(char *name, void *out, s32 a, s32 b);
extern s32   fn_8005A1EC(char *name, void *out);
extern s32   MBOX_BGLoadModelDone(void);
extern void  MBOX_BGLoadModelStart(const char *name, s32 model);
extern s32   AtreeHeaderFindSeq(void *atree, const char *name);
extern s32   FindTexMod(void *atree, const char *name, void *unused);
extern s32   AudioFindSound(const char *name, s32 bank, s32 global);
extern s32   MBOX_FindTexture(const char *name, void *unused);
extern s32   MBOX_FindTexture_Sub(const char *name, void *unused,
                                  s32 model, s32 fallback, s32 mode);
extern s32   AtreeModel(void *atree);
extern s32   InitCustomEffect(void *atree, char *name, s32 zmod, s32 alpha);
extern s32   sprintf(char *dst, const char *fmt, ...);
extern f32   atan2(f32 y, f32 x);
extern void  YawMat3(f32 angle, f32 *matrix);
extern void  WPitchMat3(f32 *matrix, f32 angle);
extern void  YawVec3(f32 *source, f32 *out, f32 angle);
extern void  PitchVec3(f32 *source, f32 *out, f32 angle);
extern f32   Random(f32 range);
extern void  CalcTargetDir(f32 *velocity, f32 targetScale, f32 speed,
                           f32 gravity, f32 lift);
extern void  PlaceEffectOnFloor(s32 effect, f32 *matrix);
extern void  SfxSetHit(s32 effect, s32 hitEffect, s32 hitAudio,
                       s32 wallSound);
extern void  SfxSetMorph(f32 time, s32 effect, s32 morph1, s32 morph2);
extern void  fn_80093E50(s32 effect, f32 *velocity, f32 *angularVelocity,
                         f32 weight, f32 radius);
extern void  DmgFxAdd(s32 effect);
extern u16   AnimateATree(void *tree, s32 sequence, s32 transition);
extern s32   AnimDone(void *animation);
extern void  MBTreeClearFlags(void *node, u32 flags, s32 mode);
extern s32   StartFXSub(s32 effect, f32 *position, u32 flags,
                        u32 treeFlags, f32 time);
extern void  SfxSetOwner(s32 effect, s32 owner);
extern void  SfxSetParent(s32 effect, void *parent);
extern void  SfxSetMat(s32 effect, f32 *matrix, void *unused);
extern void  MBTreeSetColor(void *node, s32 color, s32 mode);
extern void  MBTreeSetScale(f32 x, f32 y, f32 z, void *node);
extern void *MBNewPsysDefault(const f32 *matrix, void *parent,
                              s32 localSpace, s32 active);
extern void  MBPsysSetPTex(void *psys, s32 texture);
extern void  MBPsysSetERate4(f32 a, f32 b, f32 c, f32 d, void *psys);
extern void  MBPsysSetETime(f32 life, f32 variance, void *psys);
extern void  MBPsysSetPSpeed(void *psys, f32 speed);
extern void *PlaceItem(s32 type, s32 subtype, const char *name, f32 *position);
extern void  AddItemSub(void *item);
extern void  fn_800920E0(f32 *position, void *item, f32 scale);
extern char  lbl_803465EC;
extern void  msgPost(s32 message, s32 target, s32 value);
extern char *fn_80057ACC(s32 slot);
extern s32   toupper(s32 c);
extern void  DoTexMods(void *atree);
extern s32   MBSetupWad(s32 *wad, s32 base);
extern s32   MBGetFromWad(s32 *wad, s32 key, s32 *sizeOut);
extern u8   *sItems;
extern void *lbl_80241020[16];
extern s32   SafeRockActive(void *rock);
extern void *ItemGetNode(void *rock);
extern s32   PlayerAttacking(s32 player, s32 mode);
extern s32   player_can_be_damaged(void *player);
extern void  GetPlayerColPos(s32 i, f32 *out);
extern f64   __fabs(f64 x);
extern char  lbl_8011221C[];          /* 0x8011221C critter-overflow message      */
extern const char lbl_80112238[];
extern char  lbl_801122F0[];
extern f32   gIdentityMatrix[12];
DECL_SECT(".sdata2") extern const char lbl_80346644[];
extern void *gCurLevel;               /* current level record (->0xAC hp scale)   */
extern char  lbl_8011219C[];          /* move-type lookup failure message          */
extern void *MBOX_ReallyFindObject(const char *name, s32 type1, s32 type2,
                                    s32 exact);
extern void *MBNewObject(void *object, f32 *matrix, void *parent, u32 flags);
extern void *FloorCollide(f32 *pos, s32 a, s32 b, s32 mode, f32 x, f32 y,
                          f32 z);
extern u8    gFloorCollisionResult[]; /* 0x8023CAE0 world-collide result, mtx+Y   */
extern f32   lbl_8023CA98[];
extern void *EnemyWallCollide(f32 radius, f32 *from, f32 *to, f32 *normal);
extern s32   SlideAlongWall(f32 radius, f32 *pos, f32 *vel, f32 *wallpt,
                            f32 *normal);
extern char *lbl_8011AEA0[3];         /* 0x8011AEA0 shadow model-name table        */
extern f32   lbl_80346588;
extern f32   lbl_8034658C;
extern f32   lbl_80346618;
extern f32   lbl_80346640;
extern void  BossActivate(void *obj, s32 flag);
extern s32   gTriggerCameraState;
extern void  MBTreeSetAlpha(void *node, s32 alpha, s32 propagate);
extern f32   lbl_803464EC;
extern f64   lbl_803465C0;
extern u64   gControllerButtons;      /* 0x803445C8 (low word aliases sFlags)     */
extern void  SetSkinFX(void *fx, s32 base, s32 frames, s32 loops, f32 rate);
extern void  AudioPlay3DSel(s32 sound, s32 volume, f32 *position, s32 selector);
extern void  ShakeCamera(s32 type, s32 count, s32 delay, f32 radius,
                         s32 priority);
extern void  SafeRockSetup(void);
extern s32   lbl_802897B8[];          /* 0x802897B8 skinfx palette table          */
extern char  lbl_801121C0[];          /* 0x801121C0 killfx overflow message       */
extern f32   lbl_80346570;
extern f32   lbl_8034464C;
extern u32   sFlags;
extern s32   gBossDead;
extern s32   gGameOptions[];
extern f64   lbl_803465C8;
extern f64   lbl_803465D0;
extern f64   lbl_803465D8;
extern const char lbl_803465E0[];
extern const char lbl_803465E4[];
extern const char lbl_803465E8[];
extern char  lbl_80112104[];
extern char  lbl_8011213C[];
extern char *strcpy(char *dst, const char *src);
extern f32   acosf(f32 value);
extern void  camera_request_change(s32 value, s32 mode);
extern s32   DoAnimateTreeFrame(void *tree, s32 sequence, s32 frame,
                                s32 recurse);
extern void  DrawText(s32 x, s32 y, s32 font, u32 color,
                      const char *format, ...);
extern void  ModifyDamage(f32 armor, f32 *damage, u32 *damageType,
                          u32 shield);
extern void  do_heal_players(void *player, f32 *matrix, f32 amount);
extern s32   fn_800945D0(f32 *position, f32 *matrix, s32 damageType,
                         s32 alternate, s32 kind, f32 scale);
extern void  BossDying(void);
extern f32   lbl_8011AEC0[];

/* -- CRITTER.OBJ internal roster (forward declarations) -- */
struct CritterDamageDef;
s32  CritterCollideEnemies();
s32 CritterCollideItems(Critter *c, f32 *delta, s32 hits);
s32 CritterCollidePlayers(Critter *c, f32 *delta, s32 hits);
u32 CritterCollideWorld();
void CritterWorldDamage(Critter *c, void *surface, f32 *origin,
                        f32 *contact);
s32 CritterNodeEnemyCollide(Critter *c, void *damageDef);
s32  SafeRockNearestTarget(s32 player);
void CritterLookAtPlayer(Critter *c, CritterMove *move);
void NodeLookAtPos(void *node, f32 *target, f32 a, f32 b, f32 *yaw, f32 c,
                   f32 d, f32 *pitch);
void CritterFirePlayerCollide(Critter *c, struct CritterDamageDef *damage);
s32 CritterNodePlayerCollide(Critter *c, struct CritterDamageDef *damage,
                              s32 enabled);
void CritterAwardExp(s32 who, f32 amount);
struct CritterDamageDef;
void CritterDamagePlayer(Player *player, Critter *c,
                         struct CritterDamageDef *damageDef, u32 flags,
                         f32 *direction, s32 playSfx);
void CritterSetFxHitTime(s32 slot, s32 id, f32 amount);
s32  CritterGetTarget(Critter *c, f32 *out);
s32  CritterGetTargetSub(Critter *c, f32 *target, s32 mode);
f32  CritterReCalcTarget(Critter *c, f32 *moveTarget, s32 target);
void CritterGetSingleTargetPlayer(Critter *c);
void CritterResolveMultipleTargets(Critter *c);
void CritterGetTargetPlayers(Critter *c);
typedef struct CritterTargetRecord {
    u32 words00[3];
    f32 distance;
    u32 words10[5];
} CritterTargetRecord;

typedef struct CritterTargetState {
    u8 _pad000[0x12A];
    s16 count;
    CritterTargetRecord records[4];
} CritterTargetState;
void CritterInsertTarget(struct CritterTargetState *state,
                         struct CritterTargetRecord *target);
f32  CritterCalcTarget(Critter *c, f32 *moveTarget, f32 *target,
                       struct CritterTargetRecord *record);
void *CritterMoveNodeCol(f32 radius, f32 height, f32 *origin,
                         f32 *destination, f32 *contact, s32 ignore,
                         s32 mode);
s32  CritterMoveNodeColSub(Critter *c, f32 radius, f32 height,
                           f32 *offset, f32 *lineStart, f32 *contact,
                           s32 first);
s32  CritterExpNodeColSub(Critter *c, f32 radius, f32 squaredExpand,
                          f32 height, f32 *origin, f32 *destination,
                          f32 *contact, s32 mode);
Critter *CritterExpCollide(f32 *origin, f32 *forward, f32 radius,
                           f32 dot, f32 *contact, s32 timedId);
s32  CritterLineNodeColSub(Critter *c, f32 *origin, f32 *forward,
                           f32 *delta, f32 radius, f32 dotThreshold);
void CritterCollideStart(s32 unused, void *ctx);
s32  CritterNoHit(Critter *c, s32 id);
s32  CritterNoHitSub(Critter *c, s32 id);
void fn_80037ED0(f32 add, Critter *c, s32 id);
Critter *CritterLineCollide(f32 dotThresh, f32 limit, f32 *origin,
                            f32 *forward, f32 *out, f32 *score);
f32  CritterLineRootColSub(Critter *c, f32 *origin, f32 *forward, f32 *out,
                           f32 dotThresh, f32 limit);
s32 CritterDamage(f32 damage, Critter *c, s32 player, u32 flags,
                  f32 *hitPosition, f32 *direction, s32 source);
s32  ProcessCritter(Critter *c);
s32  ProcessCritterList(void);
void CritterDoKnockback(Critter *c);
void CritterUpdateCounters(Critter *c);
 s32 CritterGolemAI(Critter *c);
s32  CritterBossAI(Critter *c);
void CritterProcessSafeRocks(void);
void CritterDropItem(Critter *c);
s32  CritterTranslate(Critter *c, CritterMove *move);
void CritterRotate(Critter *c, CritterMove *move);
s32  CritterMoveSetup(Critter *c, CritterMove *move);
void CritterActivate(Critter *c, CritterMove *move, s32 frame);
void CritterGetNextMove(Critter *c);
void CritterLookForReady(Critter *c);
void CritterChildCriticalMove(Critter *c);
void CritterLookForCriticalMove(Critter *c);
void CritterChildGetPattern(Critter *c);
void CritterGetDoAction(Critter *c);
u32  CritterCopyAnim(Critter *c, CritterMove *move, s32 frame);
void CritterAnimate(Critter *c);
void CritterMoveDone(Critter *c, s32 moveIndex);
extern s32 lbl_8034489C;
extern s32 lbl_80344628;
extern f64 lbl_80346608;
extern f32 lbl_80346470;
s32  CritterGetDmove(CritterMove *a, CritterMove *b);
s32  CritterFindMoveType(Critter *c, s32 type, s32 mode);
void CritterAnimInterrupt(Critter *c, s32 action, s32 phase, s32 active);
s32 CritterDoTexmodNode(Critter *c, s32 action, s32 local,
                         f32 *position);
s32  CritterDoSfx(Critter *c, s32 sfx, void *parent, s32 arg3, s32 arg4);
s32  CritterDoSfxSub(Critter *c, u8 *sfx, f32 *position,
                     s32 parented, u32 flags);
void CritterDoParticle(Critter *c, void *sfx, s32 node);
void DmgFxNodeUpdate(void *node, s32 absolute, f32 rx, f32 rz, f32 rotp, f32 roty);
Critter *CritterNewInst(s32 type, s32 subtype, void *object);
void CritterInitGeo(Critter *c, void *object, s32 subtype);
void CritterAddHealthMeter(Critter *c);
void CritterInitInst(Critter *c, struct CritterHeader *hdr);
Critter *CritterEmptyInst(void);
void CritterDelInst(Critter *c);
void CritterUpdateSkinfx(Critter *c);
struct CritterColnode;
void CritterRemoveColnodeSub(Critter *c, struct CritterColnode *node, s32 mode);
void CritterInitColnodes(Critter *c);
void CritterAddAnimInsts(Critter *c, f32 *matrix);
s32  CritterLoadFile(const char *wad, const char *name);
/* Background type-file loader callbacks. */
s32 CritterLoadDone(s32 maxBytes);
void CritterBGLoadFile(s32 *loader);
s32 CritterLoadStartNext(void);
void CritterLoadAllTypes(s32 arg);
struct CritterHeader *CritterTypeLoaded(s32 type, s32 subtype);
void CritterAllocType(void *hdr, void *move, s32 arg);
void CritterLoadFinish(u8 *header);
void CritterInitAllMoves(void);
void CritterInitMoves(u8 *header);
void CritterInitSfx(void *file, s32 index, void *atreeHeader);
void CritterInitHeader(void *hdr, void *file);

/* ==================================================================== */

/* 0x80034CFC -- stop or deflect this frame's translation when it overlaps
 * a live swarm enemy. */
s32 CritterCollideEnemies(c, delta)
Critter *c;
f32 *delta;
{
    u8 unusedHigh[16];
    f32 contact[3];
    u8 unusedMid[4];
    f32 center[3];
    f32 bestContact[3];
    u8 unusedLow[8];
    u8 *enemy;
    s32 bestIndex;
    f32 *cpos;
    f32 best;
    f32 radius;
    f32 height;
    f32 combinedRadius;
    f32 combinedHeight;
    f32 distance;
    s32 index;
    s32 hit;

    cpos = c->pos;
    best = lbl_80346470;
    radius = *(f32 *)((u8 *)c->hdr + 0x7C);
    height = *(f32 *)((u8 *)c->hdr + 0x78);
    center[0] = cpos[0] + delta[0];
    bestIndex = -1;
    center[1] = cpos[1] + delta[1];
    center[2] = cpos[2] + delta[2];
    StartItemGrid(radius, center);
    while ((index = NextGridItem()) >= 0) {
        enemy = (u8 *)&gEnemies[index];
        if (*(s32 *)(enemy + 0xB4) != 1 &&
            *(s32 *)(enemy + 0xB4) != 6 &&
            (*(s32 *)(enemy + 0xB4) != 8 || lbl_803447DC == 0)) {
            continue;
        }
        if (*(s32 *)enemy == 31) {
            continue;
        }
        combinedRadius = radius + *(f32 *)(enemy + 0x238);
        combinedHeight = height + *(f32 *)(enemy + 0x23C);
        if ((*(u32 *)((u8 *)c->hdr + 0x5C) & 0x100) != 0) {
            hit = CritterMoveNodeColSub(c, combinedRadius, combinedHeight, delta,
                    (f32 *)(enemy + 0x54), contact, 1);
        } else {
            contact[0] = *(f32 *)(enemy + 0x54) - center[0];
            contact[1] = *(f32 *)(enemy + 0x58) - center[1];
            contact[2] = *(f32 *)(enemy + 0x5C) - center[2];
            if (contact[0] * contact[0] + contact[2] * contact[2] >
                combinedRadius * combinedRadius) {
                continue;
            }
            hit = LineCylinderCollide((f32 *)(enemy + 0x54), combinedRadius,
                    combinedHeight, cpos, center,
                    contact, 1);
        }
        if (hit == 0) {
            continue;
        }
        distance = fqdist(contact[0] - center[0],
                          contact[2] - center[2]);
        if (bestIndex < 0 || distance < best) {
            best = distance;
            bestIndex = index;
            bestContact[0] = contact[0];
            bestContact[1] = contact[1];
            bestContact[2] = contact[2];
        }
    }
    if (bestIndex >= 0) {
        if (*(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x20) == 3) {
            enemy = (u8 *)&gEnemies[bestIndex];
            if ((f64)*(f32 *)(enemy + 0x23C) <= lbl_80346478) {
                damage_enemy(enemy, -1, 0,
                             *(f32 *)((u8 *)c->hdr + 0xB8) *
                             *(f32 *)((u8 *)gCurLevel + 0xBC),
                             bestContact, NULL, 1);
                return 0;
            }
        }
        distance = lbl_80346470;
        delta[2] = distance;
        delta[0] = distance;
        return 1;
    }
    return 0;
}

/* 0x80034F60 -- stop translation against collidable item records returned by
 * the item grid. */
s32 CritterCollideItems(Critter *c, f32 *delta, s32 hits)
{
    f32 center[3];
    f32 out[3];
    u8 *item;
    u8 *node;
    u8 *desc;
    f64 dzero;
    f32 result;
    f32 radius;
    f32 height;
    f32 zerof;
    f32 damage;
    s32 index;
    s32 type;
    s32 hit;
    s32 j;
    f32 *cpos;

    cpos = c->pos;
    radius = *(f32 *)((u8 *)c->hdr + 0x7C);
    height = *(f32 *)((u8 *)c->hdr + 0x78);
    result = lbl_80346480;
    center[0] = c->pos[0] + delta[0];
    center[1] = c->pos[1] + delta[1];
    center[2] = c->pos[2] + delta[2];
    StartEnemyGrid(center, radius);
    dzero = lbl_80346488;
    zerof = lbl_80346470;
    while ((index = NextGridEnemy()) >= 0) {
        item = sItems + index * 0xF0;
        type = fn_8005D5C8(c, item);
        if (type == 0) {
            continue;
        }
        if ((*(u32 *)((u8 *)c->hdr + 0x5C) & 0x100) != 0) {
            for (j = 0; j < *(s16 *)((u8 *)c->hdr + 0x118); j++) {
                node = (u8 *)c + 0x4F8 + j * 0x5C;
                if (*(void **)(node + 4) == NULL) {
                    continue;
                }
                if (*(f32 *)(node + 0x58) >= *(f32 *)(node + 0x54)) {
                    continue;
                }
                desc = *(u8 **)node;
                if ((*(s16 *)(desc + 0x10) & 8) == 0) {
                    continue;
                }
                center[0] = *(f32 *)(node + 0x3C) + delta[0];
                center[1] = *(f32 *)(node + 0x40) + delta[1];
                center[2] = *(f32 *)(node + 0x44) + delta[2];
                desc = *(u8 **)node;
                result = fn_8005F0F4(item, (f32 *)(node + 0x3C), center, out,
                                     *(f32 *)(desc + 0x2C),
                                     *(f32 *)(desc + 0x2C));
                if (result >= dzero) {
                    break;
                }
            }
        } else {
            center[0] = cpos[0] + delta[0];
            center[1] = cpos[1] + delta[1];
            center[2] = cpos[2] + delta[2];
            result = fn_8005F0F4(item, cpos, center, out, radius, height);
        }
        hit = 0;
        if (result >= dzero) {
            if (type != 2) {
                if (type == 3) {
                    damage = *(f32 *)((u8 *)c->hdr + 0xB8) *
                             *(f32 *)((u8 *)gCurLevel + 0xBC);
                    if (fn_8005C1DC(item, 0, -1, c->hdr, damage) != zerof) {
                        hit = 1;
                    }
                } else {
                    hit = 1;
                }
            }
        }
        if (hit) {
            delta[2] = zerof;
            delta[0] = zerof;
        }
    }
    return 0;
}

/* 0x800351B0 -- separate active players from a translating critter and feed
 * the displacement into their push vectors. */
s32 CritterCollidePlayers(Critter *c, f32 *delta, s32 hits)
{
    Player *player;
    f32 dest[3];
    f32 contact[3];
    f32 sep[3];
    f32 radiusX;
    f32 radiusZ;
    f32 combined;
    f32 combinedZ;
    f32 length;
    f32 penetration;
    f32 scale;
    f64 maxPen;
    f64 minPen;
    f64 pushScale;
    s32 result;
    s32 count;
    s32 i;

    radiusX = *(f32 *)((u8 *)c->hdr + 0x7C);
    radiusZ = *(f32 *)((u8 *)c->hdr + 0x78);
    dest[0] = c->pos[0] + delta[0];
    dest[1] = c->pos[1] + delta[1];
    dest[2] = c->pos[2] + delta[2];
    result = 0;
    count = 0;
    maxPen = lbl_80346498;
    minPen = lbl_80346490;
    pushScale = lbl_80346478;
    for (i = 0; i < 4; i++) {
        player = &gPlayers[i];
        if (player->state != 1 && player->state != 4) {
            continue;
        }
        if ((*(s16 *)((u8 *)player + 0x964) & 0x20) != 0) {
            continue;
        }
        combined = radiusX + *(f32 *)((u8 *)player + 0x850);
        combinedZ = radiusZ + *(f32 *)((u8 *)player + 0x854);
        if ((*(s32 *)((u8 *)c->hdr + 0x5C) & 0x100) != 0) {
            result = CritterMoveNodeColSub(
                c, *(f32 *)((u8 *)player + 0x850),
                *(f32 *)((u8 *)player + 0x854), delta,
                (f32 *)((u8 *)player + 100), contact, 0);
            if (result != 0) {
                sep[0] = *(f32 *)((u8 *)player + 100) -
                         *(f32 *)((u8 *)c + result * 92 + 1240);
                sep[1] = *(f32 *)((u8 *)player + 104) -
                         *(f32 *)((u8 *)c + result * 92 + 1244);
                sep[2] = *(f32 *)((u8 *)player + 108) -
                         *(f32 *)((u8 *)c + result * 92 + 1248);
            }
        } else {
            result = LineCylinderCollide((f32 *)((u8 *)player + 100), combined,
                                         combinedZ, &c->pos[0], dest, contact, 1);
            if (result != 0) {
                sep[0] = *(f32 *)((u8 *)player + 100) - dest[0];
                sep[1] = *(f32 *)((u8 *)player + 104) - dest[1];
                sep[2] = *(f32 *)((u8 *)player + 108) - dest[2];
            }
        }
        if (result != 0) {
            count++;
            length = NormalVector(sep);
            penetration = combined - length;
            if (penetration < minPen) {
                penetration = minPen;
            } else if (penetration > maxPen) {
                penetration = maxPen;
            }
            scale = (f32)penetration;
            sep[0] = sep[0] * scale;
            sep[1] = sep[1] * scale;
            sep[2] = sep[2] * scale;
            *(f32 *)((u8 *)player + 0x870) =
                (f32)(pushScale * sep[0] + *(f32 *)((u8 *)player + 0x870));
            *(f32 *)((u8 *)player + 0x874) =
                (f32)(pushScale * sep[1] + *(f32 *)((u8 *)player + 0x874));
            *(f32 *)((u8 *)player + 0x878) =
                (f32)(pushScale * sep[2] + *(f32 *)((u8 *)player + 0x878));
        }
    }
    if (result != 0) {
        delta[2] = lbl_80346470;
        delta[0] = lbl_80346470;
    }
    return count;
}

/* 0x80035408 -- integrate the world-contact portion of a movement delta and
 * cache a floor point/status for item drops and shadows. */
typedef struct CritterWorldHeader {
    u8 _pad000[0x5C];
    u32 flags;
    u8 _pad060[0x18];
    f32 radius;
    f32 wallRadius;
    u8 _pad080[0x30];
    f32 floorOffset;
    u8 _pad0B4[0x64];
    s16 hitNodeCount;
} CritterWorldHeader;

u32 CritterCollideWorld(c, delta)
Critter *c;
f32 *delta;
{
    f32 probe[3];
    f32 direction[3];
    u8 unusedMid[12];
    f32 floorResult[15];
    f32 contact[3];
    u8 unusedLow[4];
    void *surface;
    f32 *from;
    f32 wallRadius;
    f32 radius;
    f32 bottom;
    f32 baseY;
    f32 floorY;
    f32 minRise;
    f32 reachLimit;
    f32 reach;
    f32 length;
    f32 difference;
    u32 result;
    s32 i;
    s32 offset;
    s32 grounded;

    from = c->pos;
    surface = NULL;
    minRise = (f32)(lbl_803464A0 * (f64)gClockFrameStep);
    wallRadius = ((CritterWorldHeader *)c->hdr)->wallRadius;
    radius = ((CritterWorldHeader *)c->hdr)->radius;
    if ((((CritterWorldHeader *)c->hdr)->flags & 0x100) != 0) {
        offset = 0;
        for (i = 0; i < ((CritterWorldHeader *)c->hdr)->hitNodeCount;
             i++, offset += sizeof(CritterHitNode)) {
            CritterHitNode *hitNode =
                (CritterHitNode *)((u8 *)c->hitnodes + offset);
            if (hitNode->active == NULL) {
                continue;
            }
            if (hitNode->activeFrom >= hitNode->activeUntil) {
                continue;
            }
            if ((*(s16 *)((u8 *)hitNode->descriptor + 0x10) & 8) == 0) {
                continue;
            }
            from = (f32 *)((u8 *)hitNode + 0x3C);
            probe[0] = from[0] + delta[0];
            probe[1] = from[1] + delta[1];
            probe[2] = from[2] + delta[2];
            surface = EnemyWallCollide(
                *(f32 *)((u8 *)hitNode->descriptor + 0x2C), from, probe,
                contact);
            if (surface != NULL) {
                break;
            }
        }
    } else {
        probe[0] = from[0] + delta[0];
        probe[1] = from[1] + delta[1];
        probe[2] = from[2] + delta[2];
        surface = EnemyWallCollide(wallRadius, from, probe, contact);
    }

    result = 0;
    if (surface != NULL) {
        CritterWorldDamage(c, surface, c->pos, contact);
        if ((*(u32 *)((u8 *)surface + 0x10) & 0x38) == 0) {
            if (SlideAlongWall(wallRadius, from, delta, contact,
                               lbl_8023CA98 + 4) < 0) {
                delta[2] = delta[0] = lbl_80346470;
                result = 2;
            }
        }
    }

    direction[0] = delta[0];
    direction[1] = delta[1];
    direction[2] = delta[2];
    length = NormalVector(direction);
    reach = wallRadius + length;
    reachLimit = (f32)(lbl_80346478 * (f64)reach);
    probe[0] = c->pos[0] + direction[0] * reach;
    probe[1] = c->pos[1] + direction[1] * reach;
    probe[2] = c->pos[2] + direction[2] * reach;
    bottom = (f32)(-radius - lbl_80346498);
    grounded = 0;
    if ((surface = FloorCollide(probe, (s32)floorResult, 0, 2,
                                lbl_803464A8, radius, bottom)) != NULL) {
        CritterWorldDamage(c, surface, c->pos, floorResult + 12);
        grounded = 1;
        baseY = c->pos[1] - ((CritterWorldHeader *)c->hdr)->floorOffset;
        *(f32 *)((u8 *)c + 0x438) = floorResult[12];
        *(f32 *)((u8 *)c + 0x43C) = floorResult[13];
        *(f32 *)((u8 *)c + 0x440) = floorResult[14];
        floorY = floorResult[13];
        difference = floorY - baseY;
        if (difference < lbl_80346470) {
            difference = -difference;
        }
        if (difference > reachLimit) {
            grounded = 0;
        } else if ((f64)length > lbl_80346488 &&
                   difference > (f32)(lbl_803464B0 * (f64)length)) {
            probe[0] = c->pos[0] + delta[0];
            probe[1] = c->pos[1] + delta[1];
            probe[2] = c->pos[2] + delta[2];
            surface = FloorCollide(probe, (s32)floorResult, 0, 2,
                                   lbl_803464A8, radius, bottom);
            if (surface == NULL) {
                grounded = 0;
            } else {
                *(f32 *)((u8 *)c + 0x438) = floorResult[12];
                *(f32 *)((u8 *)c + 0x43C) = floorResult[13];
                *(f32 *)((u8 *)c + 0x440) = floorResult[14];
                floorY = floorResult[13];
            }
        }
        if (grounded == 0) {
            surface = FloorCollide(c->pos, (s32)floorResult, 0, 2,
                                   lbl_803464A8, radius, bottom);
            if (surface == NULL) {
                floorY = baseY;
            } else {
                floorY = floorResult[13];
                *(f32 *)((u8 *)c + 0x438) = floorResult[12];
                *(f32 *)((u8 *)c + 0x43C) = floorResult[13];
                *(f32 *)((u8 *)c + 0x440) = floorResult[14];
            }
        }
        difference = floorY - baseY;
        if (difference < minRise) {
            difference = minRise;
        }
        delta[1] += difference;
    }
    if (grounded == 0) {
        result |= 0x10;
        delta[2] = delta[0] = lbl_80346470;
    }
    *(u32 *)((u8 *)c + 0x448) = result;
    if (surface != NULL) {
        if (*(void **)((u8 *)surface + 0x28) != NULL &&
            (*(u32 *)((u8 *)surface + 0x10) & 0x1000) != 0) {
            MBNodeSetParent(c->mbnode, *(void **)((u8 *)surface + 0x28));
        } else {
            MBNodeSetParent(c->mbnode, lbl_8034473C);
        }
        if (c->shadow != NULL) {
            CopyMat3(floorResult, (f32 *)c->shadow);
            *(f32 *)((u8 *)c->shadow + 0x30) = c->pos[0];
            *(f32 *)((u8 *)c->shadow + 0x34) = c->pos[1];
            *(f32 *)((u8 *)c->shadow + 0x38) = c->pos[2];
            *(f32 *)((u8 *)c->shadow + 0x34) =
                (f32)(lbl_803464B0 + (f64)floorResult[13]);
        }
    }
    return result;
}

/* 0x800358B0 -- translate collision material flags into a critter damage
 * class and direction. */
void CritterWorldDamage(Critter *c, void *surface, f32 *origin,
                        f32 *contact)
{
    u32 allFlags;
    u32 material;
    u32 flags;
    f32 direction[3];
    f32 damage;

    flags = 0;
    damage = lbl_80346470;
    if (((allFlags = WorldObjGetAllFlags(surface)) & 0xF0000) == 0) {
        return;
    }
    if ((allFlags & 0x02000000) != 0 &&
        (allFlags & 0x08000000) == 0) {
        return;
    }
    direction[0] = origin[0] - contact[0];
    direction[1] = lbl_80346470;
    direction[2] = origin[2] - contact[2];
    NormalVector2D(direction);
    material = allFlags & 0xF0000;
    switch (material) {
    case 0x10000:
        damage = lbl_803464B8;
        break;
    case 0x20000:
        damage = lbl_803464B8;
        flags = 0x10;
        break;
    case 0x30000:
    case 0x40000:
    case 0x50000:
        damage = lbl_803464BC;
        flags = 0x20;
        break;
    case 0x60000:
        break;
    }
    if (damage > lbl_80346488) {
        CritterDamage(damage, c, -1, flags, contact, direction, 1);
    }
}

/* 0x800359F0 -- damage swarm enemies intersecting an active critter node. */
#pragma dont_inline on
s32 CritterNodeEnemyCollide(Critter *c, void *damageDef)
{
    u8 unusedHigh[8];
    u8 *dmg = (u8 *)damageDef;
    f32 pos[3];
    u8 unusedMid[20];
    f32 out[3];
    u8 unusedLow[4];
    f32 delta[3];
    f64 zero;
    f32 bx;
    f32 by;
    f32 bz;
    f32 radius;
    f32 f26v;
    f64 k;
    s32 count;
    s32 idx;
    u8 *e;

    radius = *(f32 *)(dmg + 0x2C) * *(f32 *)((u8 *)gCurLevel + 0xBC);
    f26v = *(f32 *)(dmg + 0x0C);
    count = 0;
    MulVecMat3((f32 *)(dmg + 0x20), out, c->worldMoveMatrix);
    bx = c->moveMatrix[0] + out[0];
    by = c->moveMatrix[1] + out[1];
    bz = c->moveMatrix[2] + out[2];
    pos[0] = c->moveOrigin[0] + out[0];
    pos[1] = c->moveOrigin[1] + out[1];
    pos[2] = c->moveOrigin[2] + out[2];
    StartItemGrid(f26v, pos);
    k = lbl_80346478;
    zero = lbl_80346488;
    while ((idx = NextGridItem()) >= 0) {
        s32 state;
        e = (u8 *)&gEnemies[idx];
        state = *(s32 *)(e + 0xB4);
        if (state != 1 && state != 6 && (state != 8 || lbl_803447DC == 0)) {
            continue;
        }
        if (*(s32 *)e == 31) {
            continue;
        }
        if (radius > zero && sMusicFadeBase < *(f32 *)(e + 0x2B4)) {
            continue;
        }
        if (LineCylinderCollide((f32 *)(e + 0x54), *(f32 *)(e + 0x238) + f26v,
                                *(f32 *)(e + 0x23C) + f26v, pos, pos, out, 0)) {
            delta[0] = pos[0] - bx;
            delta[1] = pos[1] - by;
            delta[2] = pos[2] - bz;
            delta[0] = (f32)(k * delta[0]);
            delta[1] = (f32)(k * delta[1]);
            delta[2] = (f32)(k * delta[2]);
            damage_enemy(e, -1, 0, radius, out, delta, 1);
            count++;
        }
    }
    return count;
}
#pragma dont_inline off

/* 0x80035BC8 -- choose an available safe rock, or the available rock nearest
 * a requested player. */
s32 SafeRockNearestTarget(s32 player)
{
    f32 matrix[16];
    u8 unused[40];
    f32 playerX;
    f32 playerZ;
    f32 distance;
    f32 best;
    s32 i;
    s32 bestIndex;
    void *node;

    bestIndex = -1;
    best = lbl_803464C0;
    if (player < 0) {
        s32 sum;
        for (i = 0; i < lbl_80344658; i++) {
            node = lbl_80241020[i];
            sum = *(volatile s32 *)&lbl_80344654 + i;
            sum++;
            bestIndex = sum % lbl_80344658;
            if (SafeRockActive(node) == 0) {
                return bestIndex;
            }
        }
        return -1;
    }
    playerX = *(f32 *)((u8 *)&gPlayers[player] + 0x44);
    playerZ = *(f32 *)((u8 *)&gPlayers[player] + 0x4C);
    for (i = 0; i < lbl_80344658; i++) {
        if (SafeRockActive(lbl_80241020[i]) != 0) {
            continue;
        }
        node = ItemGetNode(lbl_80241020[i]);
        if (node == NULL) {
            continue;
        }
        GetWorldMat(node, matrix, NULL);
        distance = fqdist(matrix[12] - playerX, matrix[14] - playerZ);
        if (distance < best) {
            bestIndex = i;
            best = distance;
        }
    }
    return bestIndex;
}

/* 0x80035D08 -- aim the two optional look-at nodes at the selected player. */
void CritterLookAtPlayer(Critter *c, CritterMove *move)
{
    f32 target[3];
    struct CritterHeader *hdr;
    f32 *targetPtr;
    s32 look;

    look = 0;
    hdr = c->hdr;
    if (move != NULL && (move->type == 16 || move->type == 0)) {
        return;
    }
    if (move != NULL) {
        if (move->type == 17 || (move->flags & 1)) {
            look = 1;
        }
    }
    if (c->pausecnt > 0) {
        look = 1;
    }
    if (c->unkAC6 > 0) {
        look = 1;
        c->unkAC6 -= gFrameTicks;
    }
    if (look == 0 && c->unk124 >= 0) {
        Player *p = &gPlayers[c->unk124];
        target[0] = *(f32 *)((u8 *)p + 0x54);
        target[1] = *(f32 *)((u8 *)p + 0x58);
        target[2] = *(f32 *)((u8 *)p + 0x5C);
        targetPtr = target;
    } else {
        targetPtr = NULL;
    }
    if (c->hitnode0 != NULL) {
        NodeLookAtPos(c->hitnode0, targetPtr,
                      *(f32 *)((u8 *)hdr + 0x60), lbl_80346470,
                      (f32 *)((u8 *)c + 0x100),
                      *(f32 *)((u8 *)hdr + 0x68), *(f32 *)((u8 *)hdr + 0x70),
                      (f32 *)((u8 *)c + 0x108));
    }
    if (c->hitnode1 != NULL) {
        NodeLookAtPos(c->hitnode1, targetPtr,
                      *(f32 *)((u8 *)hdr + 0x64), lbl_80346470,
                      (f32 *)((u8 *)c + 0x104),
                      *(f32 *)((u8 *)hdr + 0x6C), *(f32 *)((u8 *)hdr + 0x74),
                      (f32 *)((u8 *)c + 0x10C));
    }
}

/* 0x80035E48 -- smoothly yaw and pitch a scene node toward a world point. */
void NodeLookAtPos(void *node, f32 *target, f32 a, f32 b, f32 *yaw, f32 c,
                   f32 d, f32 *pitch)
{
    union { f64 _align; f32 v[3]; } pyrU;
    f32 delta[3];
    f32 nodeYaw;
    f32 nodePitch;
    f32 yawv;
    f32 pitchv;
    f32 matrix[16];
    f32 dd;
    f64 nd;
    f32 r;
    f32 step;
#define pyr (pyrU.v)

    if (target != NULL) {
        GetWorldMat(node, matrix, NULL);
        GetYawPitch(&matrix[8], &nodeYaw, &nodePitch);
        delta[0] = target[0] - matrix[12];
        delta[1] = target[1] - matrix[13];
        delta[2] = target[2] - matrix[14];
        GetYawPitch(delta, &yawv, &pitchv);
        yawv = yawv - nodeYaw;
        pitchv = pitchv - nodePitch;
        yawv = yawv + b;
        pitchv = pitchv + d;
    } else {
        yawv = lbl_80346470;
        pitchv = lbl_80346470;
    }

    {
        dd = yawv - *yaw;
        if (dd > lbl_803464C8) {
            nd = dd - lbl_803464D0;
        } else if (dd <= lbl_803464D8) {
            nd = lbl_803464D0 + dd;
        } else {
            nd = dd;
        }
        r = (f32)nd;
        step = (f32)(lbl_803464E0 * gClockFrameStep);
        if (r > step) {
            r = step;
        }
        if (r < -step) {
            r = -step;
        }
        yawv = *yaw + r;
        *yaw = yawv;
    }

    {
        f64 nd;
        f32 r;
        f32 step;
        f32 dd = pitchv - *pitch;
        if (dd > lbl_803464C8) {
            nd = dd - lbl_803464D0;
        } else if (dd <= lbl_803464D8) {
            nd = lbl_803464D0 + dd;
        } else {
            nd = dd;
        }
        r = (f32)nd;
        step = (f32)(lbl_803464E0 * gClockFrameStep);
        if (r > step) {
            r = step;
        }
        if (r < -step) {
            r = -step;
        }
        pitchv = *pitch + r;
        *pitch = pitchv;
    }

    ExtractPYR(node, pyr);
    {
        f32 *pyrYaw = &pyr[1];
        nd = yawv - *pyrYaw;
        if (nd > lbl_803464C8) {
            nd = nd - lbl_803464D0;
        } else if (nd <= lbl_803464D8) {
            nd = lbl_803464D0 + nd;
        }
        r = (f32)nd;
        if (r > a) {
            r = a;
        }
        if (r < -a) {
            r = -a;
        }
        *pyrYaw += r;
    }

    {
        f32 *pyrPitch = &pyr[0];
        nd = pitchv - *pyrPitch;
        if (nd > lbl_803464C8) {
            nd = nd - lbl_803464D0;
        } else if (nd <= lbl_803464D8) {
            nd = lbl_803464D0 + nd;
        }
        r = (f32)nd;
        if (r > c) {
            r = c;
        }
        if (r < -c) {
            r = -c;
        }
        *pyrPitch += r;
    }
    CreatePYRMatrix(node, pyr);
#undef pyr
}

static inline void CritterDamagePlayerInline(Player *player, Critter *c,
                                              u8 *damageDef, u32 flags,
                                              f32 *direction, s32 playSfx,
                                              f64 damageGate, f64 damageScale,
                                              f32 zero, f64 hitTimeBase)
{
    u32 damageFlags;
    s32 playerIndex;
    f32 damage;
    u8 *descriptor;
    u8 *counter;
    u8 *hit;

    damageFlags = *(u32 *)(damageDef + 4) | flags;
    playerIndex = player->index;
    damage = *(f32 *)(damageDef + 0x2C) *
             *(f32 *)((u8 *)gCurLevel + 0xBC);
    if (playSfx != 0 && *(s16 *)(damageDef + 0x42) >= 0) {
        CritterDoSfx(c, *(s16 *)(damageDef + 0x42), &player->pos[0], 0, -1);
        damageFlags |= 0x01000000;
    }
    descriptor = *(u8 **)((u8 *)c->hdr + 0x120);
    if (*(s16 *)(descriptor + 0x20) != 4 &&
        (f64)lbl_803447D8 < damageGate) {
        damage = (f32)((f64)damage * damageScale);
    }
    damage_player(playerIndex, damage, 1, damageFlags, direction);
    hit = (u8 *)gPlayers + playerIndex * sizeof(Player);
    ((Player *)hit)->bossdamage = zero;
    ((Player *)hit)->fxhittime =
        (f32)(hitTimeBase + (f64)sMusicFadeBase);
    counter = (u8 *)c + playerIndex * 0x10;
    *(f32 *)(counter + 0x1BC) += damage;
    *(f32 *)(counter + 0x1C0) = sMusicFadeBase;
}

static inline void CritterDamagePlayerInlineNode(Player *player, Critter *c,
                                                  u8 *damageDef, u32 flags,
                                                  f32 *direction, s32 playSfx,
                                                  f64 damageGate,
                                                  f64 damageScale, f32 zero,
                                                  f64 hitTimeBase,
                                                  f32 *damage)
{
    u32 damageFlags;
    s32 playerIndex;
    u8 *descriptor;
    u8 *counter;
    u8 *hit;

    damageFlags = *(u32 *)(damageDef + 4) | flags;
    playerIndex = player->index;
    *damage = *(f32 *)(damageDef + 0x2C) *
              *(f32 *)((u8 *)gCurLevel + 0xBC);
    if (playSfx != 0 && *(s16 *)(damageDef + 0x42) >= 0) {
        CritterDoSfx(c, *(s16 *)(damageDef + 0x42), &player->pos[0], 0, -1);
        damageFlags |= 0x01000000;
    }
    descriptor = *(u8 **)((u8 *)c->hdr + 0x120);
    if (*(s16 *)(descriptor + 0x20) != 4 &&
        (f64)lbl_803447D8 < damageGate) {
        *damage = (f32)((f64)*damage * damageScale);
    }
    damage_player(playerIndex, *damage, 1, damageFlags, direction);
    hit = (u8 *)gPlayers + playerIndex * sizeof(Player);
    ((Player *)hit)->bossdamage = zero;
    ((Player *)hit)->fxhittime =
        (f32)(hitTimeBase + (f64)sMusicFadeBase);
    counter = (u8 *)c + playerIndex * 0x10;
    *(f32 *)(counter + 0x1BC) += *damage;
    *(f32 *)(counter + 0x1C0) = sMusicFadeBase;
}

/* 0x80036138 -- test the critter's forward fire segment against players. */
void CritterFirePlayerCollide(Critter *c, struct CritterDamageDef *damage)
{
    u8 *dmg = (u8 *)damage;
    u8 framePad[8];
    f32 start[3];
    u8 startPad[4];
    f32 end[3];
    u8 endPad[4];
    f32 delta[3];
    u8 deltaPad[4];
    f32 transformed[3];
    u8 transformedPad[4];
    f32 playerPos[3];
    u8 unused[16];
    Player *player;
    f32 maxDistance;
    f32 radius;
    f32 minDistance;
    f32 distance;
    f32 zeroForCollision;
    f64 hitTimeBase;
    f32 zeroForDamage;
    f64 damageScale;
    f64 damageGate;
    s32 i;

    maxDistance = *(f32 *)(dmg + 0x0C);
    radius = *(f32 *)(dmg + 0x08);
    minDistance = *(f32 *)(dmg + 0x10);
    delta[0] = *(f32 *)((u8 *)c + 0x3B8);
    delta[1] = *(f32 *)((u8 *)c + 0x3BC);
    delta[2] = *(f32 *)((u8 *)c + 0x3C0);
    NormalVector(delta);
    YawVec3(delta, delta, *(f32 *)(dmg + 0x14));
    PitchVec3(delta, delta, *(f32 *)(dmg + 0x1C));
    MulVecMat3((f32 *)(dmg + 0x20), transformed,
               (f32 *)((u8 *)c + 0x398));
    start[0] = *(f32 *)((u8 *)c + 0x3C8) + transformed[0];
    start[1] = *(f32 *)((u8 *)c + 0x3CC) + transformed[1];
    start[2] = *(f32 *)((u8 *)c + 0x3D0) + transformed[2];
    end[0] = delta[0] * maxDistance + start[0];
    end[1] = delta[1] * maxDistance + start[1];
    end[2] = delta[2] * maxDistance + start[2];
    damageGate = *(volatile f64 *)&lbl_80346490;
    damageScale = *(volatile f64 *)&lbl_803464F8;
    zeroForCollision = *(volatile f32 *)&lbl_80346470;
    zeroForDamage = *(volatile f32 *)&lbl_80346470;
    hitTimeBase = *(volatile f64 *)&lbl_80346500;

    for (i = 0; i < 4; i++) {
        player = &gPlayers[i];
        if (player->state != 1 || sMusicFadeBase < player->fxhittime) {
            continue;
        }
        playerPos[0] = *(f32 *)((u8 *)player + 0x64);
        playerPos[1] = *(f32 *)((u8 *)player + 0x68);
        playerPos[2] = *(f32 *)((u8 *)player + 0x6C);
        delta[0] = playerPos[0] - start[0];
        delta[1] = playerPos[1] - start[1];
        delta[2] = playerPos[2] - start[2];
        distance = fqdist(delta[0], delta[2]);
        if (distance < minDistance || distance > maxDistance) {
            continue;
        }
        if (!LineCylinderCollide(playerPos,
                                 *(f32 *)((u8 *)player + 0x850) + radius,
                                 *(f32 *)((u8 *)player + 0x854) + radius,
                                 start, end, transformed, 0)) {
            continue;
        }
        if (fn_8005FB48(zeroForCollision, start, playerPos, playerPos, 1) >= 0) {
            continue;
        }
        delta[0] = end[0] - start[0];
        delta[1] = end[1] - start[1];
        delta[2] = end[2] - start[2];
        NormalVector2D(delta);
        CritterDamagePlayerInline(player, c, dmg, 0, delta, 1, damageGate,
                                  damageScale, zeroForDamage, hitTimeBase);
    }
}

/* 0x80036424 -- test an expanded critter node/body volume against players. */
s32 CritterNodePlayerCollide(Critter *c, struct CritterDamageDef *damage,
                              s32 enabled)
{
    u8 *dmg = (u8 *)damage;
    u8 framePad[16];
    f32 start[3];
    u8 startGap[20];
    f32 transformed[3];
    u8 transformedPad[4];
    f32 deltaFromNode[3];
    u8 nodePad[4];
    f32 deltaFromCritter[3];
    u8 critterPad[4];
    f32 playerPos[3];
    u8 unused[12];
    Player *player;
    f64 hitTimeBase;
    f32 zeroDamage;
    f64 damageScale;
    f64 damageGate;
    f64 half;
    f32 zeroY;
    f64 zeroRadius;
    f32 playerDamage;
    f32 distance;
    f32 nodeX;
    f32 nodeY;
    f32 nodeZ;
    f32 radius;
    f32 expansion;
    f32 bestDistance;
    s32 i;
    s32 bestPlayer;

    radius = (f32)(enabled != 0
                       ? (f64)(*(f32 *)(dmg + 0x2C) *
                               *(f32 *)((u8 *)gCurLevel + 0xBC))
                       : lbl_80346488);
    expansion = *(f32 *)(dmg + 0x0C);
    bestDistance = lbl_80346508;
    bestPlayer = -1;
    MulVecMat3((f32 *)(dmg + 0x20), transformed,
               (f32 *)((u8 *)c + 0x398));
    nodeX = *(f32 *)((u8 *)c + 0x428) + transformed[0];
    nodeY = *(f32 *)((u8 *)c + 0x42C) + transformed[1];
    nodeZ = *(f32 *)((u8 *)c + 0x430) + transformed[2];
    start[0] = *(f32 *)((u8 *)c + 0x3C8) + transformed[0];
    start[1] = *(f32 *)((u8 *)c + 0x3CC) + transformed[1];
    start[2] = *(f32 *)((u8 *)c + 0x3D0) + transformed[2];
    half = *(volatile f64 *)&lbl_80346478;
    damageGate = *(volatile f64 *)&lbl_80346490;
    damageScale = *(volatile f64 *)&lbl_803464F8;
    zeroY = *(volatile f32 *)&lbl_80346470;
    zeroRadius = *(volatile f64 *)&lbl_80346488;
    zeroDamage = *(volatile f32 *)&lbl_80346470;
    hitTimeBase = *(volatile f64 *)&lbl_80346500;

    for (i = 0; i < 4; i++) {
        player = &gPlayers[i];
        if (player->state != 1 || player_can_be_damaged(player) == 0) {
            continue;
        }
        if (radius > zeroRadius && sMusicFadeBase < player->fxhittime) {
            continue;
        }
        playerPos[0] = *(f32 *)((u8 *)player + 0x64);
        playerPos[1] = *(f32 *)((u8 *)player + 0x68);
        playerPos[2] = *(f32 *)((u8 *)player + 0x6C);
        if (!LineCylinderCollide(playerPos,
                                 *(f32 *)((u8 *)player + 0x850) + expansion,
                                 *(f32 *)((u8 *)player + 0x854) + expansion,
                                 start, start, transformed, 0)) {
            continue;
        }
        deltaFromNode[0] = start[0] - nodeX;
        deltaFromNode[1] = start[1] - nodeY;
        deltaFromNode[2] = start[2] - nodeZ;
        deltaFromCritter[0] = playerPos[0] - c->pos[0];
        *(volatile f32 *)&deltaFromCritter[1] =
            playerPos[1] - c->pos[1];
        deltaFromCritter[2] = playerPos[2] - c->pos[2];
        deltaFromCritter[1] = zeroY;
        NormalVector(deltaFromNode);
        distance = NormalVector(deltaFromCritter);
        if (distance < bestDistance) {
            bestPlayer = i;
            bestDistance = distance;
        }
        if (radius > zeroRadius) {
            transformed[0] = deltaFromNode[0] + deltaFromCritter[0];
            transformed[1] = deltaFromNode[1] + deltaFromCritter[1];
            transformed[2] = deltaFromNode[2] + deltaFromCritter[2];
            transformed[0] = (f32)(half * (f64)transformed[0]);
            transformed[1] = (f32)(half * (f64)transformed[1]);
            transformed[2] = (f32)(half * (f64)transformed[2]);
            CritterDamagePlayerInlineNode(player, c, dmg, 0, transformed, 1,
                                          damageGate, damageScale, zeroDamage,
                                          hitTimeBase, &playerDamage);
        }
    }
    return bestPlayer;
}
/* 0x80036740 -- award experience to one player (who >= 0) or all four active
 * players (who < 0), by the integer part of `amount`. */
void CritterAwardExp(s32 who, f32 amount)
{
    s32 end;
    Player *player;

    if (who >= 0) {
        end = who + 1;
    } else {
        who = 0;
        end = 4;
    }
    player = &gPlayers[who];
    for (; who < end; who++, player++) {
        if (player->state == 1) {
            AddExp(who, (s32)amount, 0);
        }
    }
}
typedef struct CritterDamageDef {
    u32 unk00;
    u32 flags;
    u8 _pad08[0x24];
    f32 damage;
    u8 _pad30[0x12];
    s16 sfx;
} CritterDamageDef;

/* 0x800367CC -- apply one critter damage event to a player and update both
 * the player's feedback timers and the critter's per-player hit counters. */
void CritterDamagePlayer(Player *player, Critter *c,
                         CritterDamageDef *damageDef, u32 flags,
                         f32 *direction, s32 playSfx)
{
    u32 damageFlags;
    s32 playerIndex;
    f32 damage;
    u8 *descriptor;

    damageFlags = damageDef->flags | flags;
    playerIndex = player->index;
    damage = damageDef->damage * *(f32 *)((u8 *)gCurLevel + 0xBC);

    if (playSfx != 0 && damageDef->sfx >= 0) {
        CritterDoSfx(c, damageDef->sfx, &player->pos[0], 0, -1);
        damageFlags |= 0x01000000;
    }

    descriptor = *(u8 **)((u8 *)c->hdr + 0x120);
    if (*(s16 *)(descriptor + 0x20) != 4 &&
        (f64)lbl_803447D8 < lbl_80346490) {
        damage = (f32)((f64)damage * lbl_803464F8);
    }

    damage_player(playerIndex, damage, 1, damageFlags, direction);

    {
        u8 *hit;
        u8 *counter;
        hit = (u8 *)(playerIndex * sizeof(Player));
        hit = (u8 *)gPlayers + (u32)hit;
        ((Player *)hit)->bossdamage = lbl_80346470;
        counter = (u8 *)c + playerIndex * 0x10;
        ((Player *)hit)->fxhittime =
            (f32)(lbl_80346500 + (f64)sMusicFadeBase);
        *(f32 *)(counter + 0x1BC) += damage;
        *(f32 *)(counter + 0x1C0) = sMusicFadeBase;
    }
}

/* 0x800368DC -- add `amount` to a per-limb counter of the critter whose id
 * matches `id`, then stamp the companion slot with the current game time. */
void CritterSetFxHitTime(s32 slot, s32 id, f32 amount)
{
    s32 i;
    Critter *c;
    CritterBigState *big;

    big = &gBig;
    for (i = 0; i < lbl_8034466C; i++) {
        c = &big->pool[i];
        if (c->hdr != NULL && id == c->id) {
            break;
        }
    }
    if (i >= lbl_8034466C) {
        return;
    }
    big->pool[i].unk1BC[slot][0] += amount;
    big->pool[i].unk1BC[slot][1] = sMusicFadeBase;
}

/* 0x80036958 -- resolve a critter target position from either its selected
 * player or the current waypoint chain. */
s32 CritterGetTarget(Critter *c, f32 *out)
{
    u8 unused[16];
    void *waypoint;
    f64 minimum_distance;
    s32 result;

    if (*(s16 *)((u8 *)c + 0x12A) <= 0) {
        goto init_waypoint_search;
    } else {
        s32 player = *(s32 *)((u8 *)c + 0x12C);
        u8 *record = (u8 *)&gPlayers[player];

        out[0] = *(f32 *)(record + 0x64);
        out[1] = *(f32 *)(record + 0x68);
        out[2] = *(f32 *)(record + 0x6C);
        result = 1;
        goto done;
    }

waypoint_body:
    {
        f32 dx;
        f32 dy;
        f32 x;
        f32 dz;
        f32 distance;

        dy = *(f32 *)((u8 *)waypoint + 0x34) - c->vel[1];
        dx = (x = *(f32 *)((u8 *)waypoint + 0x30)) - c->vel[0];
        dz = *(f32 *)((u8 *)waypoint + 0x38) - c->vel[2];
        distance = dx * dx + dy * dy;
        distance = dz * dz + distance;

        if ((f64)distance < minimum_distance) {
            c->particle = NextWaypoint(waypoint);
            goto waypoint_test;
        } else {
            out[0] = x;
            out[1] = *(f32 *)((u8 *)c->particle + 0x34);
            out[2] = *(f32 *)((u8 *)c->particle + 0x38);
            result = 1;
            goto done;
        }
    }

init_waypoint_search:
    minimum_distance = lbl_80346490;
waypoint_test:
    waypoint = c->particle;
    if (waypoint != NULL) {
        goto waypoint_body;
    }

    out[0] = 0.0f;
    out[1] = 0.0f;
    out[2] = 0.0f;
    result = 0;
done:
    return result;
}
#pragma dont_inline on
#pragma opt_propagation off
/* 0x80036A58 */ s32 CritterGetTargetSub(Critter *c, f32 *target, s32 mode)
{
    s32 i;
    s32 best;
    f32 bestScore;

    best = -1;
    bestScore = lbl_80346508;
    if (target != NULL && (f64)target[6] > lbl_80346488 &&
        c->unk4AC > target[6]) {
        return -1;
    }

    for (i = 0; i < c->targetCount; i++) {
        f32 score = CritterReCalcTarget(c, target, i);
        if (best < 0 || score < bestScore) {
            best = i;
            bestScore = score;
        }
    }
    if (mode == 0 && (f64)bestScore >= lbl_80346510) {
        best = -1;
    }
    if (best >= 0) {
        u32 address = (u32)c + best * 0x24;
        best = *(s32 *)(address + 0x12C);
    } else if (mode != 0 && c->parent != NULL) {
        best = CritterGetTargetSub(c->parent, target, mode);
    }
    return best;
}
#pragma opt_propagation reset
#pragma dont_inline off
/* 0x80036B5C -- score one entry in the critter's target list against the
 * optional move targeting constraints. */
f32 CritterReCalcTarget(Critter *c, f32 *moveTarget, s32 target)
{
    f32 *entry;
    f32 forward[3];
    f32 dot;
    f32 range;

    entry = (f32 *)((u8 *)c + 0x12C + target * 0x24);
    if (moveTarget != NULL) {
        if (*(f32 *)((u8 *)c + 0x110) < moveTarget[4]) {
            return lbl_80346518;
        }
        if (moveTarget[5] > moveTarget[4] &&
            *(f32 *)((u8 *)c + 0x110) >= moveTarget[5]) {
            return lbl_80346518;
        }
    }

    range = entry[2];
    if (moveTarget != NULL) {
        if (range < moveTarget[0]) {
            return lbl_8034651C;
        }
        if (moveTarget[1] > lbl_80346488 && range > moveTarget[1]) {
            return lbl_80346520;
        }
        YawVec3((f32 *)((u8 *)c + 0x2C), forward, -moveTarget[2]);
        forward[1] = lbl_80346470;
        SlowNormalVector(forward);
        dot = entry[5] * forward[0] + entry[7] * forward[2];
        if (dot < moveTarget[3]) {
            return lbl_80346524;
        }
    }
    range = range * entry[4];
    return range;
}

/* 0x80036C70 -- choose the single best live player target. */
void CritterGetSingleTargetPlayer(Critter *c)
{
    f32 targetpos[3];
    CritterTargetRecord candidate;
    Player *player;
    s32 i;
    f32 score;
    f32 one;
    f64 thousand;
    f64 zero;

    c->targetCount = 0;
    if (c->health <= 0.0f) {
        return;
    }
    one = lbl_803464A8;
    thousand = lbl_80346528;
    zero = lbl_80346488;
    player = gPlayers;
    for (i = 0; i < 4; i++, player++) {
        if (player->state != 1 || (*(u32 *)((u8 *)player + 0x124) & 4) != 0) {
            continue;
        }
        targetpos[0] = *(f32 *)((u8 *)player + 0x64);
        targetpos[1] = *(f32 *)((u8 *)player + 0x68);
        targetpos[2] = *(f32 *)((u8 *)player + 0x6C);
        score = CritterCalcTarget(c, (f32 *)((u8 *)c->hdr + 0x80),
                                  targetpos,
                                  &candidate);
        if (c->particle != NULL && c->unkAD0 > zero && score > c->unkAD0) {
            continue;
        }
        if (sMusicFadeBase < player->fxhittime) {
            score = score * thousand;
        }
        if (c->targetCount == 0 ||
            score < ((CritterTargetState *)c)->records[0].distance) {
            c->targetCount = 1;
            candidate.distance = score;
            candidate.words00[0] = i;
            *(f32 *)&candidate.words10[0] = one;
            ((CritterTargetState *)c)->records[0] = candidate;
        }
    }
    if (c->targetCount != 0) {
        c->particle = NULL;
        gBig.scratch[*(s32 *)((u8 *)c + 0x12C)] += lbl_803464A8;
    }
}

/* 0x80036E00 -- distribute over-subscribed player targets among a root
 * critter and its child chain. */
void CritterResolveMultipleTargets(Critter *c)
{
    s32 threshold;
    s32 i;
    s32 outerOffset;
    s32 player;
    f32 decrement;

    if (c->alivecnt <= 0) {
        return;
    }
    if (c->unk11C >= 0) {
        return;
    }
    decrement = lbl_803464A8;
    outerOffset = 0;
    for (i = 0; i < c->targetCount; i++, outerOffset += 0x24) {
        CritterTargetRecord *record = (CritterTargetRecord *)
            ((u8 *)c + 0x12C + outerOffset);
        player = (s32)record->words00[0];
        if (*(f32 *)&record->words10[0] > lbl_80346490) {
            threshold = 2;
        } else if (*(f32 *)&record->words10[0] > lbl_80346530) {
            threshold = 3;
        } else {
            threshold = 4;
        }
        while (gBig.scratch[player] > threshold) {
            Critter *child;
            Critter *owner;
            s32 selected;
            s32 j;
            f32 best;

            owner = NULL;
            best = 0.0f;
            selected = 0;
            for (child = c->next; child != NULL; child = child->next) {
                if (child->unk11C >= 0) {
                    continue;
                }
                for (j = 0; j < child->targetCount; j++) {
                    CritterTargetRecord *entry;
                    entry = &((CritterTargetState *)child)->records[j];
                    if ((s32)entry->words00[0] == player &&
                        (owner == NULL || entry->distance > best)) {
                        owner = child;
                        best = entry->distance;
                        selected = j;
                        break;
                    }
                }
            }
            if (owner == NULL) {
                break;
            }
            for (j = selected; j < owner->targetCount - 1; j++) {
                ((CritterTargetState *)owner)->records[j] =
                    ((CritterTargetState *)owner)->records[j + 1];
            }
            owner->targetCount--;
            gBig.scratch[player] -= decrement;
        }
        record++;
    }
}

/* 0x80036FBC -- collect and distance-sort all eligible player targets. */
void CritterGetTargetPlayers(Critter *c)
{
    f32 targetpos[3];
    CritterTargetRecord record;
    Player *player;
    s32 i;
    f32 score;
    f32 damage;
    f32 base;
    f32 ratio;
    f32 thr;
    f32 result;
    f64 clamped;
    f64 pt01;
    f64 one;
    f64 huge;
    f64 thousand;
    f64 zero;

    c->targetCount = 0;
    if (c->health <= 0.0f) {
        return;
    }
    pt01 = lbl_80346540;
    one = lbl_80346490;
    thousand = lbl_80346528;
    zero = lbl_80346488;
    huge = lbl_80346510;
    player = gPlayers;
    for (i = 0; i < 4; i++, player++) {
        if (player->state != 1) {
            continue;
        }
        if ((player->flags & 4) && c->state != 0) {
            if (*(s16 *)((u8 *)*(void **)((u8 *)c->hdr + 0x120) + 0x20) != 4) {
                continue;
            }
        }
        targetpos[0] = *(f32 *)((u8 *)player + 0x64);
        targetpos[1] = *(f32 *)((u8 *)player + 0x68);
        targetpos[2] = *(f32 *)((u8 *)player + 0x6C);
        score = CritterCalcTarget(c, (f32 *)((u8 *)c->hdr + 0x80), targetpos,
                                  &record);
        if (c->particle != NULL) {
            thr = c->unkAD0;
            if (thr > zero && score > thr) {
                continue;
            }
        }
        if (sMusicFadeBase < player->fxhittime) {
            record.distance = record.distance * thousand;
        }
        if (score < huge) {
            record.words00[0] = i;
            damage = c->unk1BC[i][2];
            base = c->unk1BC[i][0];
            if (damage < one) {
                result = one + lbl_80343BEC;
            } else {
                ratio = base / damage;
                if (ratio < pt01) {
                    clamped = pt01;
                } else {
                    clamped = lbl_80343BEC;
                    if (ratio <= lbl_80343BEC) {
                        clamped = ratio;
                    }
                }
                result = clamped;
            }
            *(f32 *)&record.words10[0] = result;
            record.distance = record.distance * *(f32 *)&record.words10[0];
            CritterInsertTarget((CritterTargetState *)c, &record);
        }
    }
    for (i = 0; i < c->targetCount; i++) {
        s32 index = *(s32 *)((u8 *)c + 0x12C + i * 0x24);
        if (index >= 0) {
            gBig.scratch[index] += lbl_803464A8;
        }
    }
}
/* 0x800371BC -- insert a target record into the four-entry distance-sorted
 * target list. */
void CritterInsertTarget(CritterTargetState *state, CritterTargetRecord *target)
{
    s32 count;
    s32 insert;
    s32 shift;
    f32 distance;

    count = state->count;
    insert = 0;
    distance = target->distance;

    while (insert < count) {
        if (distance < state->records[insert].distance) {
            for (shift = count; shift > insert; shift--) {
                if (shift < 4) {
                    state->records[shift] = state->records[shift - 1];
                }
            }
            break;
        }
        insert++;
    }

    if (state->count < 4) {
        state->count++;
    }
    if (insert < 4) {
        state->records[insert] = *target;
    }
}

static inline f32 CritterCalcTargetScore(f32 distance, f32 dot, f32 *absolute)
{
    if (dot > lbl_803464F8) {
        *absolute = dot;
        *(u32 *)absolute &= 0x7FFFFFFF;
        return distance / *absolute;
    }
    return lbl_8034654C * distance;
}

/* 0x800372A0 -- calculate range, facing and score for a world-space target. */
f32 CritterCalcTarget(Critter *c, f32 *moveTarget, f32 *target,
                      CritterTargetRecord *record)
{
    f32 forward[3];
    u8 vectorGap[4];
    f32 delta[3];
    f32 distance;
    f32 vertical;
    f32 dot;
    f32 score;
    f32 absdot;
    f32 absdot2;
    u8 unused[16];

    if (moveTarget != NULL) {
        if (*(f32 *)((u8 *)c + 0x110) < moveTarget[4]) {
            return lbl_80346518;
        }
        if (moveTarget[5] > lbl_80346488 &&
            *(f32 *)((u8 *)c + 0x110) >= moveTarget[5]) {
            return lbl_80346518;
        }
    }

    delta[0] = target[0] - c->pos[0];
    delta[1] = target[1] - c->pos[1];
    delta[2] = target[2] - c->pos[2];
    vertical = delta[1];
    delta[1] = lbl_80346470;
    distance = SlowNormalVector(delta);

    if (moveTarget != NULL) {
        if (distance < moveTarget[0]) {
            return lbl_8034651C;
        }
        if (moveTarget[1] > lbl_80346488 && distance > moveTarget[1]) {
            return lbl_80346520;
        }
        if (vertical < lbl_80346470) {
            vertical = -vertical;
        }
        if (moveTarget[7] > *(volatile f64 *)&lbl_80346488 &&
            vertical > moveTarget[7]) {
            return lbl_80346548;
        }
        YawVec3((f32 *)((u8 *)c + 0x2C), forward, -moveTarget[2]);
        forward[1] = lbl_80346470;
        SlowNormalVector(forward);
        dot = delta[0] * forward[0] + delta[2] * forward[2];
        if (dot < moveTarget[3]) {
            return lbl_80346524;
        }
        score = CritterCalcTargetScore(distance, dot, &absdot);
    } else {
        forward[0] = c->mtx[2][0];
        forward[1] = lbl_80346470;
        forward[2] = c->mtx[2][2];
        SlowNormalVector(forward);
        dot = delta[0] * forward[0] + delta[2] * forward[2];
        score = CritterCalcTargetScore(distance, dot, &absdot2);
    }
    if (record != NULL) {
        f32 *out = (f32 *)record;
        out[1] = dot;
        out[2] = distance;
        out[3] = score;
        out[5] = delta[0];
        out[6] = delta[1];
        out[7] = delta[2];
    }
    return score;
}
static inline s32 CritterMoveNoHit(Critter *c, s32 id)
{
    Critter *relative;

    if (CritterNoHitSub(c, id)) {
        return 1;
    }
    relative = c->parent;
    if (relative != NULL) {
        if (CritterNoHitSub(relative, id)) {
            return 1;
        }
    } else {
        relative = c->next;
        while (relative != NULL) {
            if (CritterNoHitSub(relative, id)) {
                return 1;
            }
            relative = relative->next;
        }
    }
    return 0;
}

/* 0x800374FC -- sweep a movement segment against every other live critter. */
void *CritterMoveNodeCol(f32 radius, f32 height, f32 *origin,
                         f32 *destination, f32 *contact, s32 ignore,
                         s32 mode)
{
    Critter *c;
    f32 dz;
    f32 dx;
    f32 dy;
    f32 horizontalSquared;
    f32 verticalSquared;
    u8 unused[8];
    s32 result;

    dz = destination[2] - origin[2];
    dx = destination[0] - origin[0];
    dy = destination[1] - origin[1];
    horizontalSquared = dx * dx + dz * dz;
    verticalSquared = dy * dy;
    result = 0;
    while (lbl_80344644 < lbl_8034466C) {
        c = &gCritterPool[lbl_80344644++];
        if (c->hdr == NULL || c == lbl_80344648) {
            continue;
        }
        if (ignore >= 0) {
            if (CritterMoveNoHit(c, ignore)) {
                continue;
            }
        }
        if ((*(u32 *)((u8 *)c->hdr + 0x5C) & 2) != 0) {
            result = CritterExpNodeColSub(c, radius, horizontalSquared,
                                          verticalSquared, origin, destination,
                                          contact, mode);
        }
        if (!result) {
            f32 verticalRadius;
            f32 horizontalRadius;
            s32 collided;

            horizontalRadius = radius + *(f32 *)((u8 *)c->hdr + 0x7C);
            dz = c->pos[2] - destination[2];
            verticalRadius = radius + *(f32 *)((u8 *)c->hdr + 0x78);
            dx = c->pos[0] - destination[0];
            if (dx * dx + dz * dz >
                horizontalRadius * horizontalRadius + horizontalSquared) {
                collided = 0;
            } else if (c->pos[1] - destination[1] >
                       verticalRadius + verticalSquared) {
                collided = 0;
            } else if (LineCylinderCollide(c->pos, horizontalRadius,
                                           verticalRadius, origin, destination,
                                           contact, mode)) {
                collided = 1;
            } else {
                collided = 0;
            }
            result = collided;
        }
        if (result) {
            return c;
        }
    }
    return NULL;
}

/* 0x80037734 -- sweep against one critter's active collision nodes and keep
 * either the first or nearest contact. */
s32 CritterMoveNodeColSub(Critter *c, f32 radius, f32 height,
                          f32 *offsetVec, f32 *lineStart, f32 *contact,
                          s32 first)
{
    f32 nodePosition[3];
    u8 positionPad[20];
    f32 hit[3];
    f32 best;
    f32 distance;
    f32 nodeRadius;
    u8 *base;
    u8 *node;
    u8 *nodeDef;
    s32 byteOffset;
    s32 i;
    s32 resultIndex;

    resultIndex = -1;
    i = 0;
    byteOffset = 0;
    best = lbl_80346480;
    while (i < *(s16 *)((u8 *)c->hdr + 0x118)) {
        base = (u8 *)c + byteOffset;
        node = base + 0x4F8;
        if (*(void **)(base + 0x4FC) == NULL ||
            *(f32 *)(node + 0x58) >= *(f32 *)(node + 0x54) ||
            (*(s16 *)(*(u8 **)node + 0x10) & 8) == 0) {
            goto next;
        }
        nodeDef = *(u8 **)node;
        nodeRadius = *(f32 *)(nodeDef + 0x2C);
        nodePosition[0] = *(f32 *)(node += 0x3C) + offsetVec[0];
        nodePosition[1] = *(f32 *)(node + 4) + offsetVec[1];
        nodePosition[2] = *(f32 *)(node + 8) + offsetVec[2];
        if (LineCylinderCollide(lineStart, radius + nodeRadius,
                                height + nodeRadius, (f32 *)node,
                                nodePosition, hit, 1)) {
            if (first) {
                contact[0] = hit[0];
                contact[1] = hit[1];
                contact[2] = hit[2];
                return i + 1;
            }
            distance = fqdist(lineStart[0] - nodePosition[0],
                              lineStart[2] - nodePosition[2]);
            if (resultIndex < 0 || distance < best) {
                best = distance;
                resultIndex = i;
                contact[0] = hit[0];
                contact[1] = hit[1];
                contact[2] = hit[2];
            }
        }
next:
        i++;
        byteOffset += 0x5C;
    }
    if (resultIndex >= 0) {
        return resultIndex + 1;
    }
    return 0;
}

/* 0x800378C8 -- test an expanded sphere/capsule against one critter's active
 * hit nodes. */
s32 CritterExpNodeColSub(Critter *c, f32 radius, f32 squaredExpand,
                         f32 height, f32 *origin, f32 *destination,
                         f32 *contact, s32 mode)
{
    f32 dx;
    f32 dy;
    f32 dz;
    f32 nodeRadius;
    u8 *base;
    u8 *node;
    s32 i;
    s32 offset;

    offset = 0;
    i = 0;
    while (offset < *(s16 *)((u8 *)c->hdr + 0x118)) {
        base = (u8 *)c + i;
        node = base + 0x4F8;
        if (*(void **)(base + 0x4FC) == NULL) {
            goto next;
        }
        if (*(f32 *)(node + 0x58) >= *(f32 *)(node + 0x54)) {
            goto next;
        }
        if (mode != 2 || (*(s16 *)(*(u8 **)node + 0x10) & 8) != 0) {
            dz = *(f32 *)(node + 0x44) - destination[2];
            nodeRadius = radius + *(f32 *)(*(u8 **)node + 0x2C);
            dx = *(f32 *)(node + 0x3C) - destination[0];
            if (dx * dx + dz * dz >
                nodeRadius * nodeRadius + squaredExpand) {
                goto next;
            }
            dy = *(f32 *)(node + 0x40) - destination[1];
            if (dy > nodeRadius + height) {
                goto next;
            }
            if (LineCylinderCollide((f32 *)(node + 0x3C), nodeRadius,
                                    nodeRadius, origin, destination,
                                    contact, mode)) {
                c->unkAB8 = (s16)offset;
                return 1;
            }
        }
    next:
        offset++;
        i += 0x5C;
    }
    c->unkAB8 = -1;
    return 0;
}

/* 0x80037A10 -- expanded collision query across all critter roots, excluding
 * families that already own the supplied timed hit id. */
Critter *CritterExpCollide(f32 *origin, f32 *forward, f32 radius,
                           f32 dot, f32 *contact, s32 timedId)
{
    Critter *c;
    u8 unused2[8];
    f32 delta[3];
    u8 unused[16];
    f32 bodyRadius;
    f32 distance;
    s32 result;

    result = 0;
    if (contact == NULL) {
        contact = delta;
    }
    while (lbl_80344644 < lbl_8034466C) {
        c = &gCritterPool[lbl_80344644++];
        if (c->hdr == NULL || c == lbl_80344648) {
            continue;
        }
        if (CritterMoveNoHit(c, timedId)) {
            continue;
        }
        if ((*(u32 *)((u8 *)c->hdr + 0x5C) & 2) != 0) {
            result = CritterLineNodeColSub(c, origin, forward, contact,
                                           radius, dot);
        }
        if (!result) {
            s32 collided;

            contact[0] = c->pos[0] - origin[0];
            contact[1] = c->pos[1] - origin[1];
            contact[2] = c->pos[2] - origin[2];
            distance = NormalVector2D(contact);
            bodyRadius = radius + *(f32 *)((u8 *)c->hdr + 0x7C);
            if (distance > bodyRadius) {
                collided = 0;
            } else if (dot > 0.0 &&
                       contact[0] * forward[0] +
                       contact[2] * forward[2] < dot) {
                collided = 0;
            } else {
                collided = 1;
            }
            result = collided;
        }
        if (result) {
            return c;
        }
    }
    return NULL;
}
/* 0x80037C08 -- find an active collision node within `radius` and, when
 * requested, inside the caller's forward-facing half-space. */
s32 CritterLineNodeColSub(Critter *c, f32 *origin, f32 *forward,
                          f32 *delta, f32 radius, f32 dotThreshold)
{
    s32 offset;
    s32 i;
    CritterHitNode *node;
    f64 zero;
    f32 distance;
    u8 *record;
    u8 unused[16];

    i = 0;
    offset = 0;
    zero = lbl_80346550;
    while (i < *(s16 *)((u8 *)c->hdr + 0x118)) {
        record = (u8 *)c + offset;
        node = (CritterHitNode *)(record + 0x4F8);
        if (*(void **)(record + 0x4FC) == NULL) {
            goto next;
        }
        if (node->activeFrom >= node->activeUntil) {
            goto next;
        }

        delta[0] = node->position[0] - origin[0];
        delta[1] = node->position[1] - origin[1];
        delta[2] = node->position[2] - origin[2];
        distance = NormalVector2D(delta);
        if (distance > radius + *(f32 *)((u8 *)node->descriptor + 0x2C)) {
            goto next;
        }
        if ((f64)dotThreshold > zero &&
            delta[0] * forward[0] + delta[2] * forward[2] < dotThreshold) {
            goto next;
        }
        c->unkAB8 = (s16)i;
        return 1;

    next:
        i++;
        offset += sizeof(CritterHitNode);
    }

    c->unkAB8 = -1;
    return 0;
}
/* 0x80037D34 */
void CritterCollideStart(s32 unused, void *ctx)
{
    lbl_80344648 = ctx;
    lbl_80344644 = 0;
}

static inline s32 CritterTimedSlotActive(Critter *c, s32 id)
{
    s32 i;

    for (i = 0; i < 4; i++) {
        if (c->unk4E0[i] == id) {
            if (sMusicFadeBase < c->timed[i]) {
                return 1;
            }
        }
    }
    return 0;
}

/* 0x80037D44 -- search this critter and its immediate family for an active
 * timed slot.  Children are searched only for a root critter. */
s32 CritterNoHit(Critter *c, s32 id)
{
    Critter *relative;

    if (CritterTimedSlotActive(c, id)) {
        return 1;
    }
    relative = c->parent;
    if (relative != NULL) {
        if (CritterTimedSlotActive(relative, id)) {
            return 1;
        }
    } else {
        relative = c->next;
        while (relative != NULL) {
            if (CritterTimedSlotActive(relative, id)) {
                return 1;
            }
            relative = relative->next;
        }
    }
    return 0;
}
/* 0x80037E80 -- test whether a timed per-player slot is active for `id`. */
s32 CritterNoHitSub(Critter *c, s32 id)
{
    s32 i;

    for (i = 0; i < 4; i++) {
        if (c->unk4E0[i] == id) {
            if (sMusicFadeBase < c->timed[i]) {
                return 1;
            }
        }
    }
    return 0;
}
/* 0x80037ED0 -- allocate or replace one of the four timed id slots. */
void fn_80037ED0(f32 add, Critter *c, s32 id)
{
    s32 i;
    s32 oldest;
    f32 oldest_time;
    s32 offset;

    oldest = -1;
    oldest_time = lbl_80346480;
    if ((f64)add <= lbl_80346488) {
        return;
    }

    for (i = 0, offset = 0; i < 4; i++, offset += sizeof(f32)) {
        if (sMusicFadeBase > c->timed[i]) {
            c->unk4E0[i] = id;
            c->timed[i] = sMusicFadeBase + add;
            return;
        }
        if ((f64)oldest_time < lbl_80346488 ||
            c->timed[i] < oldest_time) {
            oldest_time = c->timed[i];
            oldest = i;
        }
    }
    if (oldest < 0) {
        return;
    }
    c->unk4E0[oldest] = id;
    c->timed[oldest] = sMusicFadeBase + add;
}
/* 0x80037F84 -- find the nearest live critter intersected by a directed
 * safe-rock query, considering both roots and their child chains. */
Critter *CritterLineCollide(f32 dotThresh, f32 limit, f32 *origin,
                            f32 *forward, f32 *out, f32 *score)
{
    u8 unused[8];
    f32 contact[3];
    Critter *pool;
    Critter *cur;
    Critter *bestC;
    s32 count;
    s32 i;
    f32 cx;
    f32 cy;
    f32 cz;
    f32 best;
    f32 d;
    u8 pad24[20];

    best = lbl_80346508;
    pool = gCritterPool;
    cur = NULL;
    bestC = NULL;
    i = -1;
    while ((count = lbl_8034466C) >= 0) {
        if (cur != NULL && cur->next != NULL) {
            cur = cur->next;
        } else {
            i++;
            for (; i < count; i++) {
                cur = &pool[i];
                if (cur->state >= 2 && cur->parent == NULL) {
                    break;
                }
            }
            if (i >= count) {
                break;
            }
        }
        d = CritterLineRootColSub(cur, origin, forward, contact, dotThresh,
                                  limit);
        if (d < best) {
            best = d;
            cx = contact[0];
            cy = contact[1];
            bestC = cur;
            cz = contact[2];
        }
    }
    if (out != NULL) {
        if (best >= lbl_80346558) {
            if (out != NULL) {
                out[0] = forward[0];
                out[1] = forward[1];
                out[2] = forward[2];
            }
        } else {
            out[0] = cx;
            out[1] = cy;
            out[2] = cz;
        }
    }
    if (score != NULL) {
        *score = best;
    }
    return bestC;
}

/* 0x800380F0 -- score a swept point against a critter's active hit nodes,
 * falling back to its body radius when it has no qualifying node. */
f32 CritterLineRootColSub(Critter *c, f32 *origin, f32 *forward, f32 *out,
                          f32 dotThresh, f32 limit)
{
    f32 delta[3];
    s32 off;
    u8 *hdr;
    s32 i;
    u8 *node;
    u8 *row;
    u8 *nodeDef;
    f32 best;
    f32 bestScore;
    f32 slope;
    f32 nd;
    f32 dr;
    f32 dist;
    f32 q;
    f32 dot;
    f32 thresh;
    f32 score;
    f32 nodeMax;
    f64 eps2;

    best = *(volatile f32 *)&lbl_80346508;
    hdr = (u8 *)c->hdr;
    bestScore = best;
    if (limit > *(volatile f64 *)&lbl_80346488) {
        slope = (lbl_80346490 - dotThresh) / limit;
    } else {
        slope = lbl_80346470;
    }
    if (c->health <= lbl_80346470) {
        return lbl_80346508;
    }
    if (c->state < 2) {
        return lbl_80346508;
    }
    if ((*(u32 *)(hdr + 92) & 2) != 0) {
        eps2 = lbl_80346488;
        for (i = 0, off = 0; i < *(s16 *)(hdr + 280); i++, off += 92) {
            row = (u8 *)c + off;
            node = row + 1272;
            if (*(void **)(row + 1276) == NULL) {
                continue;
            }
            if (*(f32 *)(node + 88) >= *(f32 *)(node + 84)) {
                continue;
            }
            delta[0] = *(f32 *)(node + 60) - origin[0];
            delta[1] = *(f32 *)(node + 64) - origin[1];
            delta[2] = *(f32 *)(node + 68) - origin[2];
            dist = NormalVector(delta);
            if (limit > eps2 && dist > limit) {
                continue;
            }
            nodeDef = *(u8 **)node;
            nodeMax = *(f32 *)(nodeDef + 24);
            if (nodeMax > eps2 && dist > nodeMax) {
                continue;
            }
            nd = dist - *(f32 *)(nodeDef + 44);
            q = fqdist(delta[0], delta[2]);
            dot = delta[0] * forward[0] + delta[2] * forward[2];
            if (slope > eps2) {
                thresh = q * (nd * slope + dotThresh);
            } else {
                thresh = dotThresh;
            }
            if (dot <= thresh) {
                continue;
            }
            score = nd / (*(f32 *)(*(u8 **)node + 28) * (dot - thresh));
            if (score < bestScore) {
                out[0] = delta[0];
                bestScore = score;
                best = nd;
                out[1] = delta[1];
                out[2] = delta[2];
            }
        }
        if (best < lbl_80346558) {
            return best;
        }
    }
    delta[0] = c->pos[0] - origin[0];
    delta[1] = c->pos[1] - origin[1];
    delta[2] = c->pos[2] - origin[2];
    dist = NormalVector(delta);
    if (limit > *(volatile f64 *)&lbl_80346488 && dist > limit) {
        return lbl_80346508;
    }
    dr = dist - *(f32 *)((u8 *)c->hdr + 124);
    q = fqdist(delta[0], delta[2]);
    dot = delta[0] * forward[0] + delta[2] * forward[2];
    thresh = q * (dr * slope + dotThresh);
    if (dot <= thresh) {
        return lbl_80346508;
    }
    out[0] = delta[0];
    out[1] = delta[1];
    out[2] = delta[2];
    return dr;
}

/* 0x800383A8 -- apply damage to a critter/hit node, accumulate combat
 * bookkeeping and transition a depleted critter into its death state. */
#pragma dont_inline on
s32 CritterDamage(f32 damage, Critter *c, s32 player, u32 flags,
                  f32 *hitPosition, f32 *direction, s32 source)
{
    u8 *header;
    u8 *descriptor;
    u8 *hitNode;
    CritterMove *move;
    Critter *child;
    Critter *parent;
    Player *playerData;
    f32 creditedDamage;
    f32 ratio;
    f32 livingChildren;
    f32 childDamage;
    f32 damageScale;
    s32 critterClass;
    s32 experience;
    s32 firstPlayer;
    s32 lastPlayer;
    s32 i;

    if (c->hdr == NULL) {
        return -1;
    }
    if (c->state < 2) {
        return -1;
    }

    header = (u8 *)c->hdr;
    move = &(*(CritterMove **)(header + 0x124))[c->curmove];
    if (move->type == 35) {
        damage = (f32)((f64)damage * lbl_80346500);
        flags &= ~0x130;
        if (*(s16 *)((u8 *)move + 0x5A) >= 1000 &&
            (c->moveSfxFlags & 1) == 0 &&
            *(s16 *)((u8 *)move + 0x58) >= 0) {
            c->moveSfxFlags |= 1;
            CritterDoSfx(c, *(s16 *)((u8 *)move + 0x58), NULL, 1, -1);
        }
    }

    ModifyDamage(*(f32 *)(header + 0xBC), &damage, &flags,
                 *(u32 *)(header + 0xE0));
    descriptor = *(u8 **)(header + 0x120);
    critterClass = *(s16 *)(descriptor + 0x20);

    if (gGameOptions[0] == 3 && player >= 0) {
        damage = lbl_80346560;
    }
    if (critterClass != 4 &&
        (f64)lbl_803447D8 < lbl_80346490) {
        damage = (f32)((f64)damage * lbl_80346478);
    }

    c->counterValue += damage;
    if (critterClass == 4 &&
        (lbl_8034489C < 1 || lbl_8034489C > 4)) {
        damage *= lbl_8011AEC0[lbl_8034465C];
    }

    if (player >= 0) {
        f32 maximumHealth;

        maximumHealth = *(f32 *)(header + 0xE4) *
                        *(f32 *)((u8 *)gCurLevel + 0xAC);
        creditedDamage = lbl_80346470;
        if (damage >= creditedDamage) {
            creditedDamage = c->health;
            if (damage <= creditedDamage) {
                creditedDamage = damage;
            }
        }

        ratio = (f32)((f64)creditedDamage /
                      (lbl_80346490 + (f64)maximumHealth));
        if ((f64)ratio > lbl_80346490) {
            ratio = lbl_803464A8;
        }
        experience = (s32)(ratio * *(f32 *)(header + 0xE8));
        if (critterClass == 4) {
            experience *= lbl_8034465C;
        }

        firstPlayer = player;
        if (player >= 0) {
            lastPlayer = player + 1;
        } else {
            firstPlayer = 0;
            lastPlayer = 4;
        }
        playerData = &gPlayers[firstPlayer];
        for (i = firstPlayer; i < lastPlayer; i++, playerData++) {
            if (playerData->state == 1) {
                AddExp(i, experience, 0);
            }
        }

        c->unk1BC[player][2] += creditedDamage;
        c->unk1BC[player][3] = sMusicFadeBase;
        if (flags & 0x00800000) {
            do_heal_players(&gPlayers[player], &c->mtx[0][0],
                            creditedDamage);
        }

        if (critterClass != 4 &&
            *(f32 *)((u8 *)gCurLevel + 0x9C) > lbl_80346470) {
            s32 level;

            playerData = &gPlayers[player];
            level = *(s32 *)((u8 *)playerData + 0x3324);
            damageScale = lbl_803464A8;
            if ((f32)level < *(f32 *)((u8 *)gCurLevel + 0x9C)) {
                damageScale = (f32)(lbl_80346490 -
                    lbl_80346568 *
                    (f64)(*(f32 *)((u8 *)gCurLevel + 0x9C) -
                          (f32)level));
            }
            if ((f64)damageScale < lbl_803464B0) {
                damageScale = lbl_80346570;
            }
            damage *= damageScale;
        }
    }

    if ((flags & 0x00100320) == 0 && c->unkAB8 >= 0) {
        hitNode = (u8 *)c + 0x4F8 + c->unkAB8 * 0x5C;
        if (*(f32 *)(hitNode + 0x58) >= *(f32 *)(hitNode + 0x54)) {
            damage = lbl_80346470;
        } else {
            u8 *hitDescriptor;

            hitDescriptor = *(u8 **)hitNode;
            damage *= *(f32 *)(hitDescriptor + 0x40);
            if (*(f32 *)(hitNode + 0x58) + damage >
                *(f32 *)(hitNode + 0x54)) {
                damage = *(f32 *)(hitNode + 0x54) -
                         *(f32 *)(hitNode + 0x58);
                if (*(s16 *)(hitDescriptor + 0x10) & 2) {
                    char objectName[40];
                    s32 object;

                    if (*(s16 *)(hitDescriptor + 0x12) >= 0) {
                        CritterDoTexmodNode(c,
                            *(s16 *)(hitDescriptor + 0x12), 0,
                            (f32 *)(hitNode + 0x3C));
                    }
                    sprintf(objectName, lbl_80346574,
                            *(u8 **)(header + 0x120) + 0x10,
                            hitDescriptor);
                    object = (s32)MBOX_ReallyFindObject(objectName,
                            *(s16 *)(descriptor + 0x22),
                            *(s16 *)(descriptor + 0x22), 1);
                    if (*(void **)(hitNode + 4) != NULL) {
                        if (object >= 0) {
                            MBSetObject(*(void **)(hitNode + 4), object);
                        }
                        for (i = 0; i < *(s32 *)((u8 *)c + 0xB0); i++) {
                            u8 *anode;

                            anode = *(u8 **)((u8 *)c + 0xB4) + i * 0x28;
                            if (*(void **)anode == *(void **)(hitNode + 4)) {
                                s32 j;

                                *(s32 *)(anode + 0x20) = 0;
                                *(void **)anode = NULL;
                                for (j = 0; j < *(s16 *)(header + 0x110); j++) {
                                    if ((*(CritterMove **)(header + 0x124))[j].node == i) {
                                        (*(CritterMove **)(header + 0x124))[j].node = -1;
                                    }
                                }
                            }
                        }
                        if ((*(s16 *)(hitDescriptor + 0x10) & 4) &&
                            *(void **)((u8 *)*(void **)(hitNode + 4) + 0x78) != NULL) {
                            CritterRemoveColnodeSub(c,
                                *(struct CritterColnode **)
                                    ((u8 *)*(void **)(hitNode + 4) + 0x78), 2);
                        }
                    }
                    if (*(void **)(hitNode + 0x4C) != NULL) {
                        MBRemoveNode(*(void **)(hitNode + 0x4C), 1);
                        *(void **)(hitNode + 0x4C) = NULL;
                    }
                }
            }
            *(f32 *)(hitNode + 0x58) += damage;
        }
    }

    if ((f64)damage <= lbl_80346488) {
        return 0;
    }

    c->counterState |= flags;
    if (direction != NULL) {
        c->knockbackInput[0] += direction[0];
        c->knockbackInput[1] += direction[1];
        c->knockbackInput[2] += direction[2];
    }
    c->counterTime = sMusicFadeBase;
    if (hitPosition == NULL) {
        hitPosition = c->movevec;
    }
    c->health -= damage;

#define CRITTER_DIE(victim)                                                   \
    do {                                                                       \
        Critter *deathChild;                                                   \
        (victim)->state = 1;                                                   \
        CritterAwardExp(-1, (f32)(lbl_80346580 *                             \
                                  (f64)*(f32 *)((u8 *)(victim)->hdr + 0xE8))); \
        if ((victim)->parent == NULL) {                                        \
            for (deathChild = (victim)->next; deathChild != NULL;              \
                 deathChild = deathChild->next) {                              \
                deathChild->health = lbl_803464A8;                             \
            }                                                                  \
        }                                                                      \
        if (*(s16 *)(*(u8 **)((u8 *)(victim)->hdr + 0x120) + 0x20) == 4 &&    \
            (victim)->parent == NULL) {                                        \
            BossDying();                                                       \
        }                                                                      \
    } while (0)

    if ((f64)c->health <= lbl_80346488) {
        if (c->state != 1) {
            CRITTER_DIE(c);
        }
        if (player >= 0) {
            playerData = &gPlayers[player];
            (*(s32 *)((u8 *)playerData + 0xC10 +
                      playerData->character * 0x1C))++;
        }
        return 1;
    }

    parent = c->parent;
    if (parent != NULL) {
        parent->health -= damage;
        if ((f64)parent->health <= lbl_80346488) {
            if (parent->state != 1) {
                CRITTER_DIE(parent);
            }
            return 1;
        }
    } else if (c->childcnt > 0) {
        livingChildren = lbl_80346470;
        for (child = c->next; child != NULL; child = child->next) {
            if (child->state >= 2) {
                livingChildren = (f32)((f64)livingChildren +
                                       lbl_80346490);
            }
        }
        if ((f64)livingChildren > lbl_80346488) {
            childDamage = (f32)(lbl_803464F8 *
                                (f64)(damage / livingChildren));
            for (child = c->next; child != NULL; child = child->next) {
                if (child->state >= 2) {
                    child->health -= childDamage;
                    if ((f64)child->health <= lbl_80346488 &&
                        child->state != 1) {
                        CRITTER_DIE(child);
                    }
                }
            }
        }
    }

#undef CRITTER_DIE

    if ((flags & 0x01000000) == 0) {
        if ((flags & 0xF) == 0) {
            if (source == 2 && *(s16 *)(header + 0xF6) >= 0) {
                CritterDoSfx(c, *(s16 *)(header + 0xF6), hitPosition, 0, -1);
            } else {
                CritterDoSfx(c, *(s16 *)(header + 0xF4), hitPosition, 0, -1);
            }
        } else {
            fn_800945D0(hitPosition, &c->mtx[0][0], flags, 0,
                        critterClass, *(f32 *)(header + 0x78));
        }

        if (flags & 0x00100320) {
            c->unkABC = 2;
        } else if (c->unkAB8 >= 0) {
            hitNode = (u8 *)c + 0x4F8 + c->unkAB8 * 0x5C;
            *(s32 *)(hitNode + 0x50) = 2;
        }
    }
    if (c->particle != NULL) {
        c->particle = NULL;
    }
    return 0;
}
#pragma dont_inline off
/* 0x80038D18 -- per-frame critter list step: reset per-player scratch, count
 * active players, then process every live critter, summing their results. */
s32 ProcessCritterList(void)
{
    Player *player;
    s32 activePlayers;
    s32 i;
    s32 total;

    activePlayers = 0;
    total = 0;
    lbl_80344664++;
    for (player = gPlayers, i = 0; i < 4; i++, player++) {
        if (player->state == 1) {
            activePlayers++;
        }
        gBig.scratch[i] = 0.0f;
    }
    lbl_8034465C = activePlayers;

    for (i = 0; i < lbl_8034466C; i++) {
        if (gCritterPool[i].hdr != NULL) {
            total += ProcessCritter(&gCritterPool[i]);
        }
    }
    return total;
}
#pragma dont_inline on
/* 0x80038DDC -- update one root critter and its child chain, including world
 * transforms, hit nodes, AI, animation, skin effects and render matrices. */
s32 ProcessCritter(Critter *c)
{
    Critter *child;
    u8 *node;
    CritterMove *move;
    s32 i;
    s32 type;
    s32 alive;
    s32 allDead;
    f32 scale;

    if (c->parent != NULL) {
        return 0;
    }
    alive = 0;
    for (child = c->next; child != NULL; child = child->next) {
        if (child->health > 0.0f) {
            alive++;
        }
    }
    c->alivecnt = (s8)alive;

    GetWorldMat(c->mbnode, &c->mtx[0][0], NULL);
    c->movevec[0] = c->vel[0];
    c->movevec[1] = c->vel[1] + *(f32 *)((u8 *)c->hdr + 0xB4);
    c->movevec[2] = c->vel[2];
    MulVec4Mat3((f32 *)((u8 *)c->hdr + 0xC0), c->pos, &c->mtx[0][0]);
    c->pos[0] += c->vel[0];
    c->pos[1] += c->vel[1];
    c->pos[2] += c->vel[2];

    for (i = 0; i < *(s16 *)((u8 *)c->hdr + 0x118); i++) {
        node = (u8 *)c + 0x4F8 + i * 0x5C;
        if (*(void **)(node + 4) == NULL) {
            CopyMat4(&c->mtx[0][0], (f32 *)(node + 0x0C));
        } else {
            GetWorldMat(*(void **)(node + 4), (f32 *)(node + 0x0C),
                        (f32 *)(*(u8 **)node + 0x20));
        }
    }
    CritterDoKnockback(c);
    CritterUpdateCounters(c);
    if (c->healthmtr >= 0) {
        HealthMeterUpdate(c->health, c->healthmtr);
    }
    if (c->damageflash != NULL) {
        scale = c->health /
                (*(f32 *)((u8 *)c->hdr + 0xE4) *
                 *(f32 *)((u8 *)gCurLevel + 0xAC));
        if ((f64)c->health <= lbl_80346488) {
            AtreeDelete(&c->healthbar[0]);
            c->damageflash = NULL;
        } else {
            MBTreeSetScale(scale, lbl_803464A8, lbl_803464A8,
                           c->damageflash);
        }
    }

    for (child = c->next; child != NULL; child = child->next) {
        CopyMat4(&c->mtx[0][0], &child->mtx[0][0]);
        child->movevec[0] = c->movevec[0];
        child->movevec[1] = c->movevec[1];
        child->movevec[2] = c->movevec[2];
        child->pos[0] = c->pos[0];
        child->pos[1] = c->pos[1];
        child->pos[2] = c->pos[2];
        if (child->obj_d0 != NULL) {
            GetWorldMat(child->obj_d0, child->worldMoveMatrix, NULL);
            child->vel[0] = child->moveOrigin[0];
            child->vel[1] = child->moveOrigin[1];
            child->vel[2] = child->moveOrigin[2];
        } else {
            CopyMat4(&c->mtx[0][0], child->worldMoveMatrix);
        }
        for (i = 0; i < *(s16 *)((u8 *)child->hdr + 0x118); i++) {
            node = (u8 *)child + 0x4F8 + i * 0x5C;
            if (*(void **)(node + 4) == NULL) {
                CopyMat4(&child->mtx[0][0], (f32 *)(node + 0x0C));
            } else {
                GetWorldMat(*(void **)(node + 4), (f32 *)(node + 0x0C),
                            (f32 *)(*(u8 **)node + 0x20));
            }
        }
        CritterUpdateCounters(child);
        if (child->healthmtr >= 0) {
            HealthMeterUpdate(child->health, child->healthmtr);
        }
        if (child->damageflash != NULL) {
            scale = child->health /
                    (*(f32 *)((u8 *)child->hdr + 0xE4) *
                     *(f32 *)((u8 *)gCurLevel + 0xAC));
            if ((f64)child->health <= lbl_80346488) {
                AtreeDelete(&child->healthbar[0]);
                child->damageflash = NULL;
            } else {
                MBTreeSetScale(scale, lbl_803464A8, lbl_803464A8,
                               child->damageflash);
            }
        }
    }

    if (c->state == 3) {
        allDead = 1;
        for (child = c->next; child != NULL; child = child->next) {
            if (allDead > 0 && (f64)child->health <= lbl_80346488) {
                allDead++;
            } else {
                allDead = 0;
            }
        }
        if (allDead > 1) {
            c->health = lbl_80346480;
        }
        if ((f64)c->health <= lbl_80346488 && c->state != 1) {
            c->state = 1;
            CritterAwardExp(
                -1, (f32)(lbl_80346580 *
                          (f64)*(f32 *)((u8 *)c->hdr + 0xE8)));
            if (c->parent == NULL) {
                for (child = c->next; child != NULL; child = child->next) {
                    child->health = lbl_803464A8;
                }
            }
            type = *(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x20);
            if (type == 4 && c->parent == NULL) {
                BossDying();
            }
        }
    }

    type = *(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x20);
    switch (type) {
    case 4:
        if (!CritterBossAI(c)) {
            return 0;
        }
        break;
    case 3:
    case 7:
    case 8:
        if (!CritterGolemAI(c)) {
            return 0;
        }
        break;
    case 0:
    case 1:
    case 2:
    case 5:
    case 6:
        CritterAnimate(c);
        if (FloorCollide(c->vel, 0, 0, 2, lbl_803464B8,
                         lbl_80346588, lbl_8034658C) != NULL) {
            c->vel[1] =
                *(f32 *)(gFloorCollisionResult + 0x34) +
                *(f32 *)((u8 *)c->hdr + 0xB0);
            if (c->shadow != NULL) {
                CopyMat3((f32 *)gFloorCollisionResult, (f32 *)c->shadow);
                *(f32 *)((u8 *)c->shadow + 0x30) = c->vel[0];
                *(f32 *)((u8 *)c->shadow + 0x34) = c->vel[1];
                *(f32 *)((u8 *)c->shadow + 0x38) = c->vel[2];
                *(f32 *)((u8 *)c->shadow + 0x34) =
                    (f32)(lbl_803464B0 +
                          (f64)*(f32 *)(gFloorCollisionResult + 0x34));
            }
        }
        break;
    }

    move = &(*(CritterMove **)((u8 *)c->hdr + 0x124))[c->curmove];
    if ((move->flags & 8) != 0) {
        if ((*(u32 *)((u8 *)c->anim + 0x60) & 0x40) == 0) {
            MBTreeSetFlags(c->anim, 0x40, 1);
        }
    } else if ((*(u32 *)((u8 *)c->anim + 0x60) & 0x40) != 0) {
        MBTreeClearFlags(c->anim, 0x40, 1);
    }
    CritterUpdateSkinfx(c);
    for (child = c->next; child != NULL; child = child->next) {
        CritterUpdateSkinfx(child);
    }
    if (*(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x26) !=
        lbl_80344664) {
        void *atree = *(void **)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x28);
        if (atree != NULL) {
            DoTexMods(atree);
        }
        *(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x26) =
            lbl_80344664;
    }
    CopyMat4(&c->mtx[0][0], (f32 *)c->mbnode);
    UnparentMatrix(c->mbnode, *(f32 **)((u8 *)c->mbnode + 0x74));

    c->movevec[0] = c->vel[0];
    c->movevec[1] = c->vel[1] + *(f32 *)((u8 *)c->hdr + 0xB4);
    c->movevec[2] = c->vel[2];
    MulVec4Mat3((f32 *)((u8 *)c->hdr + 0xC0), c->pos,
                &c->mtx[0][0]);
    c->pos[0] += c->vel[0];
    c->pos[1] += c->vel[1];
    c->pos[2] += c->vel[2];
    return 1;
}
#pragma dont_inline off
/* 0x8003946C -- consume a critter's pending knockback vector, applying the
 * damage-class scale and clamping the accumulated velocity. */
void CritterDoKnockback(Critter *c)
{
    s16 type;
    f32 scale;
    f32 lengthSquared;
    f64 clampScale;

    scale = 0.0f;
    type = *(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x20);
    if (type == 4) {
        return;
    }

    if ((f64)c->health <= 0.0) {
        scale = lbl_80346590;
    } else if ((c->counterState & 0x10140) != 0) {
        scale = lbl_80346594;
    } else if ((c->counterState & 0x20) != 0) {
        scale = lbl_80346598;
    } else if ((c->counterState & 0x10) != 0) {
        scale = lbl_803464B8;
    }

    if (type == 3) {
        scale = (f32)((f64)scale - lbl_803465A0);
    }
    if ((f64)scale > 0.0) {
        c->knockbackVelocity[0] += c->knockbackInput[0] * scale;
        c->knockbackVelocity[1] += c->knockbackInput[1] * scale;
        c->knockbackVelocity[2] += c->knockbackInput[2] * scale;

        lengthSquared =
            c->knockbackVelocity[0] * c->knockbackVelocity[0] +
            c->knockbackVelocity[1] * c->knockbackVelocity[1] +
            c->knockbackVelocity[2] * c->knockbackVelocity[2];
        if ((f64)lengthSquared > lbl_803465A8) {
            NormalVector(c->knockbackVelocity);
            clampScale = lbl_803465B0;
            c->knockbackVelocity[0] =
                (f32)(clampScale * (f64)c->knockbackVelocity[0]);
            c->knockbackVelocity[1] =
                (f32)(clampScale * (f64)c->knockbackVelocity[1]);
            c->knockbackVelocity[2] =
                (f32)(clampScale * (f64)c->knockbackVelocity[2]);
        }

        c->knockbackInput[0] = 0.0f;
        c->knockbackInput[1] = 0.0f;
        c->knockbackInput[2] = 0.0f;
    }
}
/* 0x800395C8 -- expire the critter's transient move counter and both timed
 * counter pairs for each of its four effect slots. */
void CritterUpdateCounters(Critter *c)
{
    s32 i;
    s32 moveType;
    f32 *counterTime;
    u8 *base;
    f64 zero;
    f64 timeout;
    f32 clear;
    f32 current;

    moveType = *(s32 *)(*(u8 **)((u8 *)c->hdr + 0x124) + c->curmove * 0x90);
    if ((((f64)c->counterTime > 0.0) &&
         ((f64)(sMusicFadeBase - c->counterTime) > 3.0)) ||
        moveType == 0x22 || (moveType >= 0x40 && moveType < 0x7F)) {
        c->counterValue = 0.0f;
        c->counterState = 0;
        c->counterTime = 0.0f;
    }

    zero = 0.0;
    timeout = 15.0;
    clear = 0.0f;
    for (i = 0; i < 4; i++) {
        base = (u8 *)c + i * 0x10;
        counterTime = (f32 *)(base + 0x1C0);
        current = *counterTime;
        if ((f64)current > zero &&
            (f64)(sMusicFadeBase - current) > timeout) {
            *(f32 *)(base + 0x1BC) = clear;
            *counterTime = clear;
        }
        current = *(counterTime = (f32 *)(base + 0x1C8));
        if ((f64)current > zero &&
            (f64)(sMusicFadeBase - current) > timeout) {
            *(f32 *)(base + 0x1C4) = clear;
            *counterTime = clear;
        }
    }
}
/* 0x800396A4 -- run the compact golem/general AI path. */
s32 CritterGolemAI(Critter *c)
{
    CritterMove *move0;
    CritterMove *move;
    CritterMove *nm;
    Critter *child;
    s32 mt;
    s32 i;
    f32 speed;
    f32 ratio;
    f32 best;
    s32 anim32;
    f64 one;
    u8 unused[8];

    CritterGetSingleTargetPlayer(c);
    one = lbl_80346490;
    ratio = c->health /
            (one + *(f32 *)((u8 *)c->hdr + 0xE4) *
                       *(f32 *)((u8 *)gCurLevel + 0xAC));
    speed = one - ratio;
    speed = lbl_803464E8 + speed * lbl_803464EC;
    c->invRateScale = one / speed;
    c->rateScale = speed;

    if (c->state == 0) {
        if (c->particle == NULL) {
            best = lbl_80346470;
            for (i = 0; i < c->targetCount; i++) {
                f32 v = *(f32 *)((u8 *)c + 0x134 + i * 0x24);
                if (v > best) {
                    best = v;
                }
            }
        }
        c->state = 3;
        for (child = c->next; child != NULL; child = child->next) {
            child->state = 3;
        }
        if (*(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x20) == 4) {
            BossActivate(c, 1);
        }
    }

    move0 = &(*(CritterMove **)((u8 *)c->hdr + 0x124))[
                c->curmove < 0 ? 0 : c->curmove];
    mt = -1;
    c->nextmove = mt;
    c->unk11E = mt;
    c->unk126 = mt;
    CritterGetDoAction(c);

    if (gTriggerCameraState != 0) {
        if (c->nextmove < 0) {
            if (c->rateScale < lbl_803465C0) {
                mt = CritterFindMoveType(c, 0x21, 0);
            }
            if (mt < 0) {
                mt = CritterFindMoveType(c, 0x20, 1);
            }
            c->nextmove = (s16)mt;
        }
    } else if (lbl_803447DC == 0) {
        if (c->nextmove < 0) {
            CritterLookForCriticalMove(c);
        }
        if (c->nextmove < 0) {
            CritterChildCriticalMove(c);
        }
        if (c->nextmove < 0) {
            CritterLookForReady(c);
        }
        if (c->nextmove < 0) {
            mt = -1;
            if (c->rateScale < lbl_803465C0) {
                mt = CritterFindMoveType(c, 0x21, 0);
            }
            if (mt < 0) {
                mt = CritterFindMoveType(c, 0x20, 1);
            }
            c->nextmove = (s16)mt;
        }
    }

    if (c->nextmove < 0) {
        c->nextmove = c->curmove;
    }

    nm = &(*(CritterMove **)((u8 *)c->hdr + 0x124))[c->nextmove];
    if (lbl_803447DC == 0 || c->curmove < 0 ||
        move0->type == 0x11 || nm->type == 0x11 ||
        move0->type == 0x10 || nm->type == 0x10) {
        CritterAnimate(c);
    }

    if (c->curmove < 0) {
        c->curmove = 0;
    }
    anim32 = (s32)*(f32 *)((u8 *)c + 0x90);
    move = &(*(CritterMove **)((u8 *)c->hdr + 0x124))[c->curmove];
    if (move->type == 0x11) {
        if (AnimDone(c->sound)) {
            CritterDropItem(c);
            CritterDelInst(c);
            return 0;
        }
        {
            s32 dur = *(s32 *)((u8 *)move + 0x40);
            if (dur > 0) {
                s32 elapsed = anim32 - dur;
                s32 total = *(s16 *)((u8 *)c + 0x88) - dur;
                if (elapsed > 0 && total > 0) {
                    MBTreeSetAlpha(c->anim, 255 - elapsed * 255 / total, 1);
                }
            }
        }
    }

    if (move->type == 0x11 || lbl_803447DC == 0) {
        CritterMoveSetup(c, move);
        CritterActivate(c, move, anim32);
        if (!CritterTranslate(c, move)) {
            CritterRotate(c, move);
        }
        CritterLookAtPlayer(c, move);
    }

    if (lbl_80346490 != lbl_803447D8) {
        if (c->mbnode != NULL) {
            MBTreeSetScale(lbl_803447D8, lbl_803447D8, lbl_803447D8, c->mbnode);
        }
        if (c->shadow != NULL) {
            MBTreeSetScale(lbl_803447D8, lbl_803447D8, lbl_803447D8, c->shadow);
        }
    } else {
        if (c->mbnode != NULL) {
            MBTreeClearFlags(c->mbnode, 8, 0);
        }
        if (c->shadow != NULL) {
            MBTreeClearFlags(c->shadow, 8, 0);
        }
    }
    return 1;
}

/* 0x80039AD8 -- run boss target distribution, pattern selection and the
 * coordinated root/child animation pass. */
s32 CritterBossAI(Critter *c)
{
    char moveName[32];
    Critter *child;
    CritterMove *move;
    CritterMove *childMove;
    u8 *header;
    u8 *surface;
    f32 best;
    f32 speed;
    f32 ratio;
    f32 duration;
    f32 angle;
    f32 distance;
    f32 dot;
    f32 clamped;
    s32 frame;
    s32 childFrame;
    s32 moveIndex;
    s32 moveType;
    s32 selected;
    s32 linkedChildren;
    s32 done;
    s32 i;
    f64 one;

    CritterGetTargetPlayers(c);
    header = (u8 *)c->hdr;
    one = lbl_80346490;
    ratio = c->health /
            (one + *(f32 *)(header + 0xE4) *
                       *(f32 *)((u8 *)gCurLevel + 0xAC));
    speed = one - ratio;
    speed = lbl_803464E8 + speed * lbl_803464EC;
    c->invRateScale = one / speed;
    c->rateScale = speed;

    for (child = c->next; child != NULL; child = child->next) {
        CritterGetTargetPlayers(child);
        header = (u8 *)child->hdr;
        ratio = child->health /
                (one + *(f32 *)(header + 0xE4) *
                           *(f32 *)((u8 *)gCurLevel + 0xAC));
        speed = one - ratio;
        speed = lbl_803464E8 + speed * lbl_803464EC;
        child->invRateScale = one / speed;
        child->rateScale = speed;
    }

    header = (u8 *)c->hdr;
    CritterResolveMultipleTargets(c);
    CritterProcessSafeRocks();

    if (c->state == 0) {
        if ((f64)lbl_8034464C == lbl_80346488) {
            if ((*(u32 *)(header + 0x5C) & 0x80) == 0) {
                lbl_8034464C = (f32)(lbl_80346478 + (f64)sMusicFadeBase);
            }
        } else if ((f64)sMusicFadeBase >= (f64)lbl_8034464C) {
            best = lbl_80346470;
            for (i = 0; i < c->targetCount; i++) {
                f32 candidate = *(f32 *)((u8 *)c + 0x134 + i * 0x24);
                if (candidate > best) {
                    best = candidate;
                }
            }
            if ((f64)best <= lbl_80346488) {
                best = lbl_803464C0;
            }
            distance = *(f32 *)(header + 0xEC);
            if ((f64)distance <= lbl_80346488 || best < distance) {
                c->state = 3;
                for (child = c->next; child != NULL; child = child->next) {
                    child->state = 3;
                }
                if (*(s16 *)(*(u8 **)(header + 0x120) + 0x20) == 4) {
                    BossActivate(c, 1);
                }
            }
        }
    }

    if (c->unk128 >= 0) {
        camera_request_change(30, 2);
    }

    moveIndex = c->curmove;
    if (moveIndex < 0) {
        moveIndex = 0;
    }
    header = (u8 *)c->hdr;
    move = (CritterMove *)(*(u8 **)(header + 0x124) + moveIndex * 0x90);

    if ((sFlags & 0x80) != 0) {
        CritterGetNextMove(c);
    } else {
        c->nextmove = -1;
        c->unk11E = -1;
        c->unk126 = -1;
        CritterGetDoAction(c);
        if (c->nextmove < 0) {
            CritterLookForCriticalMove(c);
        }
        if (c->nextmove < 0) {
            CritterChildCriticalMove(c);
            linkedChildren = 0;
            for (child = c->next; child != NULL; child = child->next) {
                child->nextmove = -1;
                child->unk11E = -1;
                child->unk126 = -1;
                if (c->unk11C >= 0) {
                    if (c->unk11C < *(s16 *)((u8 *)child->hdr + 0x114)) {
                        child->unk11C = c->unk11C;
                        child->unk120 = c->unk120;
                        child->nextmove = *(s16 *)(
                            *(u8 **)((u8 *)child->hdr + 0x128) +
                            child->unk11C * 0x50 + child->unk120 * 2 + 0x20);
                    }
                } else if (c->nextmove < 0) {
                    CritterChildGetPattern(child);
                    if (child->nextmove < 0) {
                        CritterChildCriticalMove(child);
                    }
                    selected = child->nextmove;
                    if (selected < 0) {
                        selected = -1;
                        if ((f64)child->rateScale < lbl_803465C0) {
                            selected = CritterFindMoveType(child, 0x21, 0);
                        }
                        if (selected < 0) {
                            selected = CritterFindMoveType(child, 0x20, 1);
                        }
                        child->nextmove = (s16)selected;
                    } else if (child->unk11C >= 0) {
                        linkedChildren++;
                    } else {
                        childMove = (CritterMove *)(
                            *(u8 **)((u8 *)child->hdr + 0x124) +
                            selected * 0x90);
                        if (childMove->type >= 0x7F) {
                            linkedChildren++;
                        }
                    }
                }
            }
            if (linkedChildren != 0) {
                c->nextmove = (s16)CritterFindMoveType(c, 1, 1);
            }
        }
        if (c->nextmove < 0 && move->type < 0x7F) {
            CritterLookForReady(c);
        }
        if (c->nextmove < 0) {
            selected = -1;
            if ((f64)c->rateScale < lbl_803465C0) {
                selected = CritterFindMoveType(c, 0x21, 0);
            }
            if (selected < 0) {
                selected = CritterFindMoveType(c, 0x20, 1);
            }
            c->nextmove = (s16)selected;
        }
    }

    CritterAnimate(c);
    if (c->curmove < 0) {
        c->curmove = 0;
    }
    move = (CritterMove *)(*(u8 **)((u8 *)c->hdr + 0x124) +
                           c->curmove * 0x90);
    frame = (s32)*(f32 *)((u8 *)c + 0x90);

    for (child = c->next; child != NULL; child = child->next) {
        if (child->state == 1) {
            child->nextmove = (s16)CritterFindMoveType(child, 0x11, 1);
            CritterAnimate(child);
        } else if ((move->type == 1 || c->unk11C >= 0) &&
                   (lbl_8034489C < 2 || lbl_8034489C > 3) &&
                   (child->curmove >= 0 || child->nextmove >= 0)) {
            CritterAnimate(child);
        } else {
            DoAnimateTreeFrame((u8 *)child + 0x74, *(s16 *)((u8 *)c + 0x86),
                               frame, 1);
            child->movedone = c->movedone;
            child->curmove = -1;
            child->unk11C = -1;
            child->unk120 = -1;
            if (move->type == 1) {
                child->nextmove = 0;
            }
        }
    }

    duration = *(f32 *)((u8 *)move + 0x8C);
    done = AnimDone(c->sound);
    if ((f64)duration > lbl_80346488) {
        if (move->type == 0x11) {
            if (done == 0) {
                c->rate = sMusicFadeBase + duration;
            } else if ((f64)sMusicFadeBase >= (f64)c->rate &&
                       (sFlags & 0x80) == 0) {
                CritterDelInst(c);
                return 0;
            } else if ((sFlags & 0x80) == 0) {
                f32 remaining = c->rate - sMusicFadeBase;
                gBossDead = 1;
                if ((f64)remaining < lbl_803464F8) {
                    s32 fade = 255 -
                        (s32)(lbl_80346478 * (lbl_803465C8 * remaining));
                    MBTreeSetAlpha(c->anim, fade, 1);
                }
            }
        } else if (done == 0 || (f64)c->rate == lbl_80346488) {
            c->rate = sMusicFadeBase + duration;
        }
    } else {
        c->rate = lbl_80346470;
    }

    moveType = move->type;
    if (moveType == 0x10) {
        if (move->link < 0 && done != 0 && lbl_8034489C == 1) {
            lbl_8034489C = 2;
        }
    } else if (moveType == 0x22) {
        if (done != 0 && lbl_8034489C == 3) {
            lbl_8034489C = 4;
        }
    }
    if (lbl_8034489C >= 2 && lbl_8034489C < 4) {
        c->unkABE = 0xFF;
    }

    CritterMoveSetup(c, move);
    CritterActivate(c, move, frame);
    CritterTranslate(c, move);
    CritterRotate(c, move);
    CritterLookAtPlayer(c, move);

    for (child = c->next; child != NULL; child = child->next) {
        childMove = NULL;
        if (child->curmove >= 0) {
            childFrame = (s32)*(f32 *)((u8 *)child + 0x90);
            childMove = (CritterMove *)(
                *(u8 **)((u8 *)child->hdr + 0x124) + child->curmove * 0x90);
            CritterMoveSetup(child, childMove);
            CritterActivate(child, childMove, childFrame);
            CritterTranslate(child, childMove);
            CritterRotate(child, childMove);
        }
        CritterLookAtPlayer(child, childMove);
    }

    if (FloorCollide(c->vel, 0, 0, 2, lbl_803464B8,
                     lbl_80346588, lbl_8034658C) != NULL) {
        c->vel[1] = *(f32 *)(gFloorCollisionResult + 0x34) +
                    *(f32 *)((u8 *)c->hdr + 0xB0);
        if (c->state == 0 && (f64)lbl_8034464C == lbl_80346488 &&
            (*(u32 *)((u8 *)c->hdr + 0x5C) & 0x80) != 0) {
            s32 surfaceFlags = 0;
            surface = *(u8 **)(gFloorCollisionResult + 0x44);
            if (surface != NULL) {
                surfaceFlags = (s8)surface[0x16];
                if (*(u8 **)(surface + 0x18) != NULL) {
                    surfaceFlags |= (s8)(*(u8 **)(surface + 0x18))[0x16];
                }
            }
            if ((surfaceFlags & 0x10) != 0) {
                lbl_8034464C = (f32)(lbl_80346478 + (f64)sMusicFadeBase);
                BossActivate(c, 0);
            }
        }
        if (c->shadow != NULL) {
            CopyMat3((f32 *)gFloorCollisionResult, c->shadow);
            *(f32 *)((u8 *)c->shadow + 0x30) = c->vel[0];
            *(f32 *)((u8 *)c->shadow + 0x34) = c->vel[1];
            *(f32 *)((u8 *)c->shadow + 0x38) = c->vel[2];
            *(f32 *)((u8 *)c->shadow + 0x34) =
                (f32)(lbl_803464B0 +
                      (f64)*(f32 *)(gFloorCollisionResult + 0x34));
        }
    }

    if ((sFlags & 0x10) != 0 && gGameOptions[8] != 0) {
        distance = lbl_80346480;
        angle = distance;
        if (c->targetCount > 0) {
            angle = *(f32 *)((u8 *)c + 0x134);
            if ((f64)angle >= lbl_80346510) {
                angle = distance;
            }
            dot = *(f32 *)((u8 *)c + 0x130);
            if ((f64)dot < lbl_80346550) {
                clamped = (f32)lbl_80346550;
            } else if ((f64)dot > lbl_80346490) {
                clamped = (f32)lbl_80346490;
            } else {
                clamped = dot;
            }
            distance = (f32)(lbl_803465D0 *
                             (lbl_803465D8 * (f64)acosf(clamped)));
        }
        if (c->unk11C >= 0) {
            sprintf(moveName, lbl_803465E0, c->unk11C);
        } else {
            strcpy(moveName, lbl_803465E4);
        }
        DrawText(8, 214, 0, 0xFFFFFF, lbl_80112104, moveName,
                 (u8 *)move + 0x10, (s32)c->health,
                 (s32)(lbl_80346594 * c->rateScale),
                 (s32)(lbl_803464F8 + *(f32 *)((u8 *)c + 0x90)),
                 c->unk124, (s32)(lbl_803464F8 + angle),
                 (s32)(lbl_803464F8 + distance));

        i = 0;
        for (child = c->next; child != NULL; child = child->next, i++) {
            distance = lbl_80346480;
            angle = distance;
            if (child->targetCount > 0) {
                angle = *(f32 *)((u8 *)child + 0x134);
                if ((f64)angle >= lbl_80346510) {
                    angle = distance;
                }
                dot = *(f32 *)((u8 *)child + 0x130);
                if ((f64)dot < lbl_80346550) {
                    dot = (f32)lbl_80346550;
                } else if ((f64)dot > lbl_80346490) {
                    dot = (f32)lbl_80346490;
                }
                distance = (f32)(lbl_803465D0 *
                                 (lbl_803465D8 * (f64)acosf(dot)));
            }
            if (child->unk11C >= 0) {
                sprintf(moveName, lbl_803465E0, child->unk11C);
            } else {
                strcpy(moveName, lbl_803465E4);
            }
            childMove = NULL;
            childFrame = -1;
            if (child->curmove >= 0) {
                childMove = (CritterMove *)(
                    *(u8 **)((u8 *)child->hdr + 0x124) +
                    child->curmove * 0x90);
                childFrame = (s32)*(f32 *)((u8 *)child + 0x90);
            }
            DrawText(8, 224 + i * 10, 0, 0xFFFFFF, lbl_8011213C, i,
                     moveName,
                     childMove != NULL ? (char *)childMove + 0x10
                                       : (char *)lbl_803465E8,
                     (s32)child->health,
                     (s32)(lbl_80346594 * child->rateScale), childFrame,
                     child->unk124, (s32)(lbl_803464F8 + angle),
                     (s32)(lbl_803464F8 + distance));
        }
    }
    return 1;
}
/* 0x8003A73C -- collect the level's safe rocks once, then count down each
 * reactivation timer and restore the corresponding item when it expires. */
void CritterProcessSafeRocks(void)
{
    CritterBigState *big;
    s32 i;
    s32 count;

    big = &gBig;
    if (lbl_80344658 == 0) {
        lbl_80344658 =
            CollectSafeRocks(big->safeRockIndices, 16, lbl_80344650);
        count = lbl_80344658;
        for (i = 0; i < count; i++) {
            gBig.safeRockTimers[i] = 0.0f;
        }
        if (count <= 0) {
            lbl_80344658 = -1;
        } else {
            lbl_80344654 = RandInt(count);
        }
    } else if (lbl_80344658 > 0) {
        for (i = 0; i < lbl_80344658; i++) {
            if ((f64)gBig.safeRockTimers[i] > 0.0) {
                gBig.safeRockTimers[i] -= gClockFrameStep;
                if ((f64)gBig.safeRockTimers[i] <= 0.0) {
                    SafeRockActivate(gBig.safeRockIndices[i]);
                }
            }
        }
    }
}
/* 0x8003A838 -- release or spawn the item carried by a critter and place it
 * at the cached floor contact point. */
void CritterDropItem(Critter *c)
{
    void *item;
    char name[32];
    s32 type;

    item = NULL;
    type = 0;
    if (*(void **)((u8 *)c + 0xACC) != NULL) {
        item = *(void **)((u8 *)c + 0xACC);
        *(void **)c->_blkACC = NULL;
        type = 1;
    } else {
        switch (*(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x20)) {
        case 7: {
            char *p;

            sprintf(name, &lbl_803465EC, fn_80057ACC(0x20));
            p = name;
            while (*p != '\0') {
                *p = (char)toupper(*p);
                p++;
            }
            item = PlaceItem(1, 0x10, name, NULL);
            type = 2;
            break;
        }
        }
    }
    if (item == NULL) {
        return;
    }
    switch (*(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x20)) {
    case 8:
        msgPost(0x86, -1, 0);
        break;
    case 7:
        msgPost(0x8A, -1, 0);
        break;
    }
    if (type != 0) {
        *(u8 *)((u8 *)item + 0xCD) = 10;
        fn_800920E0((f32 *)((u8 *)c + 0x438), item, lbl_80346470);
        return;
    }

    *(u8 *)((u8 *)item + 0xCD) = 0;
    MBTreeClearFlags(*(void **)((u8 *)item + 0x64), 2, 0);
    if (**(s32 **)((u8 *)item + 0x0) == 1) {
        *(s16 *)((u8 *)item + 0xEC) = 60;
    }
    *(f32 *)((u8 *)item + 0x34) = *(f32 *)((u8 *)c + 0x438);
    *(f32 *)((u8 *)item + 0x38) = *(f32 *)((u8 *)c + 0x43C);
    *(f32 *)((u8 *)item + 0x3C) = *(f32 *)((u8 *)c + 0x440);
    AddItemSub(item);
}

/* 0x8003A9C4 -- integrate scripted translation and knockback, then clamp the
 * result through world and critter collision. */
s32 CritterTranslate(Critter *c, CritterMove *move)
{
    u8 pad16[16];
    f32 delta[3];
    f32 t0;
    f32 t1;
    f32 t2;
    u8 pad4[4];
    f32 contact[3];
    f32 dest[3];
    register f32 zero;
    f32 speed;
    f32 spd;
    f32 dx;
    f32 dz;
    f32 fx;
    f32 fz;
    f32 ax;
    f32 az;
    f32 bz;
    f32 bx;
    f32 tx;
    f32 tz;
    f32 gz;
    f64 kdamp;
    f32 nx;
    f32 ny;
    f32 nz;
    f32 dist;
    f32 scale;
    s32 result = 0;
    s32 hits;
    s32 pr;
    s32 tmpr;
    f32 rad;
    u8 pad24[24];

    speed = *(f32 *)((u8 *)c->hdr + 0xAC);
    spd = *(f32 *)((u8 *)gCurLevel + 0xB0) *
          (move->readyDistance * gClockFrameStep);
    if (speed <= 0.0f) {
        return 0;
    }
    if (move->type == 0x38) {
        delta[0] = c->targetPos[0] - c->vel[0];
        delta[1] = c->targetPos[1] - c->vel[1];
        delta[2] = c->targetPos[2] - c->vel[2];
        tx = delta[0];
        tz = delta[2];
        dist = fqdist(tx, tz);
        if (dist > lbl_80346488) {
            scale = lbl_80346490 / dist;
            tx *= scale;
            tz *= scale;
        }
        dx = tx * spd;
        dz = tz * spd;
    } else {
        if ((*(u32 *)((u8 *)c->hdr + 0x5C) & 0x40) != 0) {
            ax = *(f32 *)((u8 *)c + 1016);
            az = *(f32 *)((u8 *)c + 1024);
            dist = fqdist(ax, az);
            if (dist > lbl_80346488) {
                scale = lbl_80346490 / dist;
                ax *= scale;
                az *= scale;
            }
            fx = ax;
            fz = az;
        } else {
            bx = c->mtx[2][0];
            bz = c->mtx[2][2];
            dist = fqdist(bx, bz);
            if (dist > lbl_80346488) {
                scale = lbl_80346490 / dist;
                bx *= scale;
                bz *= scale;
            }
            fx = bx;
            fz = bz;
        }
        switch (move->type) {
        default:
            dx = spd * fx;
            dz = spd * fz;
            break;
        case 0x35:
            dx = -spd * fx;
            dz = -spd * fz;
            break;
        case 0x32:
            dz = spd * fx;
            dx = -spd * fz;
            break;
        case 0x33:
            dx = spd * fz;
            dz = -spd * fx;
            break;
        case 0x36:
            dx = spd * fx;
            dz = spd * fz;
            dz = dz + dx;
            dx = -spd * fz + dx;
            break;
        case 0x37:
            dx = spd * fx;
            dz = spd * fz;
            break;
        }
    }
    delta[0] = dx;
    zero = lbl_80346470;
    delta[1] = zero;
    delta[2] = dz;
    delta[0] += c->knockbackVelocity[0] * gClockFrameStep;
    delta[1] += c->knockbackVelocity[1] * gClockFrameStep;
    delta[2] += c->knockbackVelocity[2] * gClockFrameStep;
    kdamp = lbl_803465C0;
    nx = c->vel[0] + delta[0];
    ny = c->vel[1] + delta[1];
    nz = c->vel[2] + delta[2];
    c->knockbackVelocity[0] = (f32)(kdamp * c->knockbackVelocity[0]);
    c->knockbackVelocity[1] = (f32)(kdamp * c->knockbackVelocity[1]);
    c->knockbackVelocity[2] = (f32)(kdamp * c->knockbackVelocity[2]);
    t0 = c->knockbackVelocity[0];
    *(u32 *)&t0 &= 0x7FFFFFFF;
    if (t0 < lbl_80346540) {
        c->knockbackVelocity[0] = zero;
    }
    t1 = c->knockbackVelocity[1];
    *(u32 *)&t1 &= 0x7FFFFFFF;
    if (t1 < lbl_80346540) {
        c->knockbackVelocity[1] = lbl_80346470;
    }
    t2 = c->knockbackVelocity[2];
    *(u32 *)&t2 &= 0x7FFFFFFF;
    if (t2 < lbl_80346540) {
        c->knockbackVelocity[2] = lbl_80346470;
    }
    gz = lbl_80346470;
    if (c->knockbackVelocity[1] > gz) {
        c->knockbackVelocity[1] =
            c->knockbackVelocity[1] - lbl_803465F4 * gClockFrameStep;
        if (c->knockbackVelocity[1] < gz) {
            c->knockbackVelocity[1] = gz;
        }
    }
    if (*(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x20) == 4) {
        delta[0] = nx - c->movePathPos[0];
        delta[1] = ny - c->movePathPos[1];
        delta[2] = nz - c->movePathPos[2];
        if (c->state != 1) {
            dist = fqdist(delta[0], delta[2]);
            c->unk4AC = dist;
            if ((*(u32 *)((u8 *)c->hdr + 0x5C) & 0x20) != 0) {
                if (delta[0] > speed) {
                    delta[0] = speed;
                }
                if (delta[0] < -speed) {
                    delta[0] = -speed;
                }
                if (delta[2] > speed) {
                    delta[2] = speed;
                }
                if (delta[2] < -speed) {
                    delta[2] = -speed;
                }
            } else if (dist > speed) {
                scale = speed / dist;
                delta[0] *= scale;
                delta[2] *= scale;
            }
        }
        c->vel[0] = c->movePathPos[0] + delta[0];
        c->vel[1] = c->movePathPos[1] + delta[1];
        c->vel[2] = c->movePathPos[2] + delta[2];
    } else {
        delta[0] = nx - c->vel[0];
        delta[1] = ny - c->vel[1];
        delta[2] = nz - c->vel[2];
        hits = CritterCollideWorld(c, delta, 0);
        hits += CritterCollideItems(c, delta, hits);
        tmpr = CritterCollidePlayers(c, delta, hits);
        hits = hits + tmpr;
        pr = tmpr;
        CritterCollideEnemies(c, delta, hits);
        rad = *(f32 *)((u8 *)c->hdr + 0x7C);
        dest[0] = c->pos[0] + delta[0];
        dest[1] = c->pos[1] + delta[1];
        dest[2] = c->pos[2] + delta[2];
        lbl_80344644 = 0;
        lbl_80344648 = c;
        if (CritterMoveNodeCol(rad, lbl_80346470,
                               &c->pos[0], dest, contact, -1, 1) != NULL) {
            delta[2] = lbl_80346470;
            delta[0] = lbl_80346470;
        }
        c->vel[0] = c->vel[0] + delta[0];
        c->vel[1] = c->vel[1] + delta[1];
        c->vel[2] = c->vel[2] + delta[2];
        if (pr != 0) {
            result = 1;
        }
    }
    return result;
}
/* 0x8003AF4C -- turn the critter toward the move-selected player, waypoint
 * or explicit facing, clamping angular speed by the move and header rates. */
void CritterRotate(Critter *c, CritterMove *move)
{
    f32 delta;
    f32 turn;
    f32 limit;
    volatile f64 highPad;
    f32 target[3];
    u8 unused[8];

    turn = *(f32 *)((u8 *)move + 0x88);
    limit = *(f32 *)((u8 *)c->hdr + 0xCC);
    if ((move->flags & 0x20) != 0) {
        delta = *(f32 *)((u8 *)c + 0xF8) - *(f32 *)((u8 *)c + 0xFC);
    } else if (c->unk128 >= 0) {
        delta = lbl_80346470;
    } else {
        if (c->unk124 >= 0 && (f64)turn > 0.0) {
            GetPlayerColPos(c->unk124, target);
            target[0] -= c->vel[0];
            target[1] -= c->vel[1];
            target[2] -= c->vel[2];
            if ((*(u32 *)((u8 *)c->hdr + 0x5C) & 0x400) != 0) {
                register f32 z = target[2];
                delta = atan2(target[0], z) - *(f32 *)((u8 *)c + 0xFC);
            } else {
                register f32 z = target[2];
                f32 angle = atan2(target[0], z) - *(f32 *)((u8 *)c + 0xF8);
                f64 wideAngle;
                f32 clamped;
                if ((f64)angle > 3.141592654) {
                    wideAngle = (f64)angle - 6.283185308;
                } else if ((f64)angle <= -3.141592654) {
                    wideAngle = 6.283185308 + (f64)angle;
                } else {
                    wideAngle = angle;
                }
                angle = (f32)wideAngle;
                clamped = angle < -limit ? -limit :
                          angle > limit ? limit : angle;
                delta = (*(f32 *)((u8 *)c + 0xF8) + clamped) -
                        *(f32 *)((u8 *)c + 0xFC);
            }
        } else if (c->particle != NULL && c->targetCount == 0) {
            target[0] = *(f32 *)((u8 *)c->particle + 0x30) - c->vel[0];
            target[1] = *(f32 *)((u8 *)c->particle + 0x34) - c->vel[1];
            target[2] = *(f32 *)((u8 *)c->particle + 0x38) - c->vel[2];
            {
                register f32 z = target[2];
                delta = atan2(target[0], z) - *(f32 *)((u8 *)c + 0xFC);
            }
        } else {
            delta = lbl_80346470;
        }
    }

    if ((f64)delta != 0.0) {
        f64 wideDelta = delta;
        f32 wrapped;
        f32 turnStep;
        if (wideDelta > 3.141592654) {
            wideDelta -= 6.283185308;
        } else if (wideDelta <= -3.141592654) {
            wideDelta = 6.283185308 + wideDelta;
        }
        wrapped = (f32)wideDelta;
        turnStep = turn * gClockFrameStep;
        if (c->unkAC6 > 0) {
            turnStep *= 0.1;
        }
        if (wrapped < -turnStep) {
            delta = -turnStep;
        } else {
            if (wrapped > turnStep) {
                wrapped = turnStep;
            }
            delta = wrapped;
        }
        *(f32 *)((u8 *)c + 0xFC) =
            *(f32 *)((u8 *)c + 0xFC) + delta;
        CopyMat3((f32 *)gIdentityMatrix, &c->mtx[0][0]);
        YawMat3(*(f32 *)((u32)c + 0xFC), &c->mtx[0][0]);
    }
}
/* 0x8003B1CC -- select the critter's current target/node and refresh the
 * world-space movement matrix used by the active move. */
s32 CritterMoveSetup(Critter *c, CritterMove *move)
{
    f32 *target;
    void *node;
    void *candidate;
    s32 nodeIndex;
    s16 currentMove;
    s16 queuedTarget;

    target = NULL;
    currentMove = c->curmove;
    if (currentMove >= 0) {
        target = (f32 *)(*(u8 **)((u8 *)c->hdr + 0x124) +
                         currentMove * sizeof(CritterMove) + 0x60);
    }

    if (c->unk124 < 0 || c->movedone != 0) {
        if (c->unkAC6 > 0) {
            c->unk124 = -1;
        } else if ((queuedTarget = c->unk126) >= 0) {
            c->unk124 = queuedTarget;
        } else {
            c->unk124 = CritterGetTargetSub(c, target, 1);
        }
    }

    if (c->movedone != 0) {
        c->moveFlags = 0;
        c->moveSfxFlags = 0;
        if (c->emitter != NULL) {
            MBRemoveNode(c->emitter, 1);
            c->emitter = NULL;
        }

        nodeIndex = move->node;
        node = c->anim;
        if (nodeIndex < 0) {
            goto store_move_node;
        }
        candidate = ((void **)((u8 *)c->anodes +
                               nodeIndex * 0x28))[0];
        if (candidate == NULL) {
            candidate = node;
        }
        node = candidate;
store_move_node:
        c->obj_d0 = node;
    }

    c->moveMatrix[0] = c->moveOrigin[0];
    c->moveMatrix[1] = c->moveOrigin[1];
    c->moveMatrix[2] = c->moveOrigin[2];
    return GetWorldMat(c->obj_d0, c->worldMoveMatrix, NULL);
}
/* Displacement overlay for the per-move sound/particle trigger fields
 * (0x58..0x5E within CritterMove); a struct-member view keeps MWCC emitting
 * direct base+disp loads instead of a hoisted address register. */
typedef struct CritterMoveFx {
    u8  _padFx[0x58];
    s16 sfx;        /* 0x58 */
    s16 sfxFrame;   /* 0x5A */
    s16 sfx2;       /* 0x5C */
    s16 sfx2Frame;  /* 0x5E */
} CritterMoveFx;

/* 0x8003B300 -- activate frame-gated move actions, sounds and particles. */
void CritterActivate(Critter *c, CritterMove *move, s32 frame)
{
    CritterMoveFx *fx = (CritterMoveFx *)move;
    u32 events;
    s16 oldFlags;
    u8 *entry;

    if (c->emitter != NULL) {
        DmgFxNodeUpdate(c->emitter, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    if (*(s32 *)((u8 *)move + 0x40) >= 0) {
        events = CritterCopyAnim(c, move, frame);
        oldFlags = c->moveFlags;
        if ((events & 1) != 0) {
            if (move->type != 0x85) {
                c->moveFlags = oldFlags | 1;
            }
            if (*(s16 *)((u8 *)move + 0x48) >= 0) {
                CritterAnimInterrupt(c, *(s16 *)((u8 *)move + 0x48), 1,
                                     !(oldFlags & 1));
            }
        }
        if ((events & 2) != 0) {
            if (move->type != 0x85) {
                c->moveFlags |= 2;
            }
            if (*(s16 *)((u8 *)move + 0x4A) >= 0) {
                CritterAnimInterrupt(c, *(s16 *)((u8 *)move + 0x4A), 2,
                                     !(oldFlags & 2));
            }
        }
    }
    if (*(s16 *)((u8 *)move + 0x48) >= 0) {
        entry = *(u8 **)(*(u8 **)((u8 *)c->hdr + 0x130) + 0x44) +
                *(s16 *)((u8 *)move + 0x48) * 0x50;
        if ((*(s16 *)(entry + 2) & 0x4000) && c->unkAC8 > lbl_80346488 &&
            *(s16 *)(entry + 0) != 1) {
            return;
        }
    }
    if ((c->moveSfxFlags & 1) == 0 && fx->sfx >= 0 && frame >= fx->sfxFrame) {
        c->moveSfxFlags |= 1;
        CritterDoSfx(c, fx->sfx, NULL, 1, -1);
    }
    if ((c->moveSfxFlags & 2) == 0 && fx->sfx2 >= 0 && frame >= fx->sfx2Frame) {
        c->moveSfxFlags |= 2;
        CritterDoSfx(c, fx->sfx2, NULL, 1, -1);
    }
}

/* 0x8003B4CC -- follow an explicit move link or advance to the next legal
 * move while skipping pattern-marker entries. */
void CritterGetNextMove(Critter *c)
{
    s16 count;
    CritterMove *moves;
    CritterMove *move;
    Critter *child;
    s16 linked;
    s32 childrenDone;

    moves = *(CritterMove **)((u8 *)c->hdr + 0x124);
    count = *(s16 *)((u8 *)c->hdr + 0x110);
    move = &moves[c->curmove];
    if (c->curmove < 0) {
        c->nextmove = 0;
        return;
    }
    linked = move->link;
    if (linked >= 0) {
        c->nextmove = linked;
        return;
    }

    if (move->type == 1) {
        childrenDone = 1;
        for (child = c->next; child != NULL; child = child->next) {
            if (child->curmove >= 0 || child->nextmove >= 0) {
                child->nextmove = child->curmove + 1;
                if (child->nextmove < *(s16 *)((u8 *)child->hdr + 0x110)) {
                    childrenDone = 0;
                } else {
                    child->nextmove = -1;
                    if (child->curmove >= 0) {
                        childrenDone = 0;
                    }
                }
            }
        }
        if (!childrenDone) {
            c->nextmove = c->curmove;
            return;
        }
    }

    c->nextmove = c->curmove + 1;
    for (;;) {
        move = &moves[c->nextmove];
        if (c->nextmove >= count) {
            c->nextmove = 0;
            continue;
        }
        if (move->type == 0xF0) {
            c->nextmove++;
            continue;
        }
        if (move->link == c->curmove) {
            c->nextmove++;
            continue;
        }
        break;
    }
    for (child = c->next; child != NULL; child = child->next) {
        child->nextmove = -1;
    }
    if (c->curmove >= 0 && moves[c->curmove].type == 0x11) {
        MBTreeClearFlags(c->anim, 1, 0);
        MBTreeClearFlags(*(void **)((u8 *)c->anim + 0x78), 2, 2);
    }
}
/* 0x8003B67C -- choose the closest ready move in the 0x30..0x39 family. */
void CritterLookForReady(Critter *c)
{
    s32 timeOffset;
    s32 moveOffset;
    s32 i;
    CritterMove *moves;
    s32 result;
    s32 moveCount;
    CritterMove *move;
    s32 type;
    f64 zeroDouble;
    f32 zeroFloat;
    f32 best;
    f32 distance;

    moveCount = *(s16 *)((u8 *)c->hdr + 0x110);
    moves = *(CritterMove **)((u8 *)c->hdr + 0x124);
    result = -1;
    best = lbl_803464C0;

    if ((*(u32 *)((u8 *)c->hdr + 0x5C) & 0x10000) == 0) {
        return;
    }
    if (CritterGetTarget(c, c->targetPos) == 0) {
        return;
    }

    zeroFloat = lbl_80346470;
    i = 0;
    zeroDouble = lbl_80346488;
    timeOffset = 0;
    moveOffset = 0;
    while (i < moveCount) {
        move = (CritterMove *)((u8 *)moves + moveOffset);
        type = move->type;
        if (type < 0x30 || type > 0x39) {
            goto next;
        }
        if (type == 0x38 && c->unk124 < 0) {
            goto next;
        }
        if ((move->flags & 4) != 0) {
            goto next;
        }

        if (c->targetCount == 0 && c->particle != NULL) {
            if (c->targetCount == 0 &&
                move->readyDistance > zeroFloat) {
                result = i;
                break;
            }
        }

        if ((f64)move->cooldown > zeroDouble &&
            sMusicFadeBase <
                *(f32 *)((u8 *)c + 0x218 + timeOffset) +
                    move->cooldown) {
            goto next;
        }

        distance = CritterCalcTarget(c, (f32 *)((u8 *)move + 0x60),
                                     c->targetPos, 0);
        if (distance < best) {
            result = i;
            best = distance;
        }

    next:
        i++;
        timeOffset += 4;
        moveOffset += sizeof(CritterMove);
    }

    if (result >= 0) {
        c->nextmove = (s16)result;
    }
}
/* 0x8003B7D8 -- continue the current child-pattern sequence or choose the
 * oldest ready child pattern / critical move and its target. */
void CritterChildCriticalMove(Critter *c)
{
    CritterPattern *patterns;
    CritterPattern *pattern;
    CritterMove *moves;
    CritterMove *move;
    f32 *time;
    s32 patternChoice;
    s32 moveChoice;
    s32 playerChoice;
    s32 player;
    s32 i;
    s32 timeOffset;
    s32 recordOffset;
    s32 type;
    u32 flags;
    f64 zero;
    f32 best;

    patternChoice = -1;
    moveChoice = -1;
    playerChoice = -1;
    best = lbl_803465F8;

    if (c->unk11C >= 0 && c->unk120 + 1 < 8) {
        patterns = *(CritterPattern **)((u8 *)c->hdr + 0x128);
        c->nextmove =
            patterns[c->unk11C].sequence[c->unk120];
        if (c->nextmove >= 0) {
            c->unk11E = c->unk11C;
            return;
        }
    }

    i = 0;
    patterns = *(CritterPattern **)((u8 *)c->hdr + 0x128);
    timeOffset = 0;
    recordOffset = 0;
    while (i < *(s16 *)((u8 *)c->hdr + 0x114)) {
        pattern = (CritterPattern *)((u8 *)patterns + recordOffset);
        if (i == c->unk11C) {
            goto next_pattern;
        }
        if ((pattern->flags & 2) != 0 &&
            c->childcnt != c->alivecnt) {
            goto next_pattern;
        }
        if ((pattern->flags & 0x1000) != 0) {
            goto next_pattern;
        }
        time = (f32 *)((u8 *)c + 0x318 + timeOffset);
        if (sMusicFadeBase < *time + pattern->cooldown) {
            goto next_pattern;
        }
        player = CritterGetTargetSub(c, (f32 *)((u8 *)pattern + 0x30), 0);
        if (player >= 0 && *time < best) {
            patternChoice = i;
            playerChoice = player;
            best = *time;
        }

    next_pattern:
        i++;
        timeOffset += 4;
        recordOffset += sizeof(CritterPattern);
    }

    moves = *(CritterMove **)((u8 *)c->hdr + 0x124);
    i = 0;
    zero = lbl_80346488;
    timeOffset = 0;
    recordOffset = 0;
    while (i < *(s16 *)((u8 *)c->hdr + 0x110)) {
        if (i == c->curmove) {
            goto next_move;
        }
        move = (CritterMove *)((u8 *)moves + recordOffset);
        type = move->type;
        if (type < 0x7F || type >= 0xF0) {
            goto next_move;
        }
        flags = move->flags;
        if ((flags & 4) != 0) {
            goto next_move;
        }
        if ((flags & 2) != 0 &&
            c->childcnt != c->alivecnt) {
            goto next_move;
        }
        if ((flags & 0x10) != 0) {
            if (move->node < 0) {
                goto next_move;
            }
            if (move->link >= 0 && moves[move->link].node < 0) {
                goto next_move;
            }
        }

        if (c->unk128 >= 0 && type == 0x81) {
            patternChoice = -1;
            moveChoice = i;
            playerChoice = c->unk128;
            break;
        }

        if ((f64)move->cooldown > zero &&
            sMusicFadeBase <
                *(f32 *)((u8 *)c + 0x218 + timeOffset) +
                    move->cooldown) {
            goto next_move;
        }
        player = CritterGetTargetSub(c, (f32 *)((u8 *)move + 0x60), 0);
        if (player >= 0) {
            time = (f32 *)((u8 *)c + 0x218 + timeOffset);
            if (*time < best) {
                best = *time;
                patternChoice = -1;
                moveChoice = i;
                playerChoice = player;
            } else {
                if (patternChoice < 0 &&
                    (moveChoice < 0 ||
                     CritterGetDmove(&moves[moveChoice], move) > 1)) {
                    best = *time;
                    patternChoice = -1;
                    moveChoice = i;
                    playerChoice = player;
                }
            }
        }

    next_move:
        i++;
        timeOffset += 4;
        recordOffset += sizeof(CritterMove);
    }

    if (patternChoice >= 0) {
        c->unk11E = (s16)patternChoice;
        c->unk126 = (s16)playerChoice;
        pattern = *(CritterPattern **)((u8 *)c->hdr + 0x128);
        c->nextmove = pattern[patternChoice].move;
    } else if (moveChoice >= 0) {
        c->nextmove = (s16)moveChoice;
        c->unk126 = (s16)playerChoice;
    }
}
/* 0x8003BAFC -- select a ready critical move when its target player is
 * currently attacking. */
void CritterLookForCriticalMove(Critter *c)
{
    s32 i;
    CritterMove *moves;
    s32 moveOffset;
    s32 timeOffset;
    CritterMove *move;
    u32 flags;
    s32 player;
    f64 zero;

    i = 0;
    timeOffset = 0;
    moveOffset = 0;
    moves = *(CritterMove **)((u8 *)c->hdr + 0x124);
    zero = lbl_80346488;

    while (i < *(s16 *)((u8 *)c->hdr + 0x110)) {
        move = (CritterMove *)((u8 *)moves + moveOffset);
        if (move->type != 0x23) {
            goto next;
        }
        flags = move->flags;
        if ((flags & 4) != 0) {
            goto next;
        }
        if ((flags & 2) != 0 && c->childcnt != c->alivecnt) {
            goto next;
        }
        if ((flags & 0x10) != 0) {
            if (move->node < 0) {
                goto next;
            }
            if (move->link >= 0 && moves[move->link].node < 0) {
                goto next;
            }
        }
        if ((f64)move->cooldown > zero &&
            sMusicFadeBase <
                c->moveTimes[i] + move->cooldown) {
            goto next;
        }
        player = CritterGetTargetSub(c, (f32 *)((u8 *)move + 0x60), 0);
        if (player < 0 || PlayerAttacking(player, 1) == 0) {
            goto next;
        }
        c->nextmove = (s16)i;

    next:
        i++;
        timeOffset += 4;
        moveOffset += sizeof(CritterMove);
    }
}
/* 0x8003BC28 -- select a child-pattern move, with damage reactions taking
 * precedence over the ordinary linked continuation. */
void CritterChildGetPattern(Critter *c)
{
    CritterMove *move;

    move = &(*(CritterMove **)((u8 *)c->hdr + 0x124))[c->curmove];
    if (c->state == 1) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x11, 1);
    } else if (lbl_8034489C >= 3 && lbl_8034489C <= 5 && gBossType == 35) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x20, 0);
    } else if (move->link >= 0) {
        c->nextmove = move->link;
    }
    if (c->nextmove < 0 && (c->counterState & 0x120) != 0) {
        if ((c->counterState & 0x100) != 0) {
            c->nextmove = (s16)CritterFindMoveType(c, 0x42, 0);
        }
        if (c->nextmove < 0) {
            c->nextmove = (s16)CritterFindMoveType(c, 0x41, 0);
        }
    }
    if (c->nextmove < 0 &&
        c->counterValue >=
            (f32)(s32)(lbl_80346600 * lbl_8011AEAC[lbl_8034465C])) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x22, 0);
    }
    if (c->nextmove < 0 && (c->counterState & 0x10) != 0) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x40, 0);
    }
    if (c->nextmove >= 0) {
        c->unk11E = -2;
    }
}

/* 0x8003BDF4 -- choose the root critter's next action from lifecycle,
 * explicit links, AI mode and pending damage reactions. */
void CritterGetDoAction(Critter *c)
{
    CritterMove *move;
    Critter *child;
    s32 aiType;

    move = &(*(CritterMove **)((u8 *)c->hdr + 0x124))[c->curmove];
    aiType = *(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x20);
    if (c->curmove < 0 || c->state == 0) {
        c->nextmove = (s16)CritterFindMoveType(c, 0, 1);
    } else if (c->state == 2) {
        c->nextmove = (s16)CritterFindMoveType(c, 0, 1);
    } else if (move->type == 0) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x10, 0);
    } else if (c->state == 1) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x11, 1);
    } else if (move->link >= 0) {
        c->nextmove = move->link;
    } else if ((u32)(lbl_8034489C - 1) <= 1) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x20, 1);
    } else if (lbl_8034489C == 3 && move->type != 0x22) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x22, 1);
    } else if (lbl_8034489C >= 3 && lbl_8034489C <= 5 && gBossType == 0x23) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x20, 0);
    } else if (move->type == 0x10 && aiType == 4) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x20, 0);
    }

    if (c->nextmove < 0 && (c->counterState & 0x120) != 0) {
        if ((c->counterState & 0x100) != 0) {
            c->nextmove = (s16)CritterFindMoveType(c, 0x42, 0);
        }
        if (c->nextmove < 0) {
            c->nextmove = (s16)CritterFindMoveType(c, 0x41, 0);
        }
    }
    if (c->nextmove < 0 &&
        c->counterValue >=
            (f32)(s32)(lbl_80346600 * lbl_8011AEAC[lbl_8034465C])) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x22, 0);
    }
    if (c->nextmove < 0 && (c->counterState & 0x10) != 0) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x40, 0);
    }
    if (c->nextmove <= 1 && c->nextmove >= 0) {
        lbl_80344628++;
    }
    if (c->nextmove >= 0 && c->unk11C >= 0) {
        c->unk11C = -1;
        c->unk120 = -1;
        for (child = c->next; child != NULL; child = child->next) {
            child->unk11C = -1;
            child->unk120 = -1;
        }
    }
    c->counterState &= ~0x130;
}

static f32 CritterAnimMod(s32 delta, f32 period)
{
    f64 absPeriod;
    f32 t = (f32)delta;
    absPeriod = __fabs(period);
    if (absPeriod > __fabs(t)) {
        return t;
    }
    return t - period * (f32)(s64)(t / period);
}

/* 0x8003C11C -- convert move frame windows into the two activation edges
 * consumed by CritterActivate. */
u32 CritterCopyAnim(Critter *c, CritterMove *move, s32 frame)
{
    u32 result;
    u8 unused[16];

    result = 0;
    switch (move->type) {
    case 0x80:
    case 0x83:
    case 0x86: {
        s32 second;
        if (frame >= *(s32 *)((u8 *)move + 0x40) &&
            frame <= *(s16 *)((u8 *)move + 0x50)) {
            result |= 1;
        }
        second = *(s32 *)((u8 *)move + 0x44);
        if (second >= 0 && frame >= second &&
            frame <= *(s16 *)((u8 *)move + 0x52)) {
            result |= 2;
        }
        break;
    }
    case 0x81: {
        s32 second;
        if (frame >= *(s32 *)((u8 *)move + 0x40) &&
            frame <= *(s16 *)((u8 *)move + 0x50)) {
            result |= 1;
        }
        second = *(s32 *)((u8 *)move + 0x44);
        if (second >= 0 && (c->moveFlags & 2) == 0 && frame >= second) {
            result |= 2;
        }
        break;
    }
    case 0x84: {
        s16 flags = c->moveFlags;
        s32 second;
        if ((flags & 1) == 0 && frame >= *(s32 *)((u8 *)move + 0x40)) {
            result |= 1;
        }
        second = *(s32 *)((u8 *)move + 0x44);
        if (second >= 0 && (flags & 2) == 0 && frame >= second) {
            result |= 2;
        }
        break;
    }
    case 0x85: {
        s32 first = *(s32 *)((u8 *)move + 0x40);
        s32 second;
        f32 period;
        if (frame >= first && frame <= *(s16 *)((u8 *)move + 0x50)) {
            period = *(f32 *)((u8 *)move + 0x4C);
            if (period <= lbl_80346488 ||
                (s32)CritterAnimMod(frame - first, period) == 0) {
                result |= 1;
            }
        }
        second = *(s32 *)((u8 *)move + 0x44);
        if (second >= 0 && frame >= second &&
            frame <= *(s16 *)((u8 *)move + 0x52)) {
            period = *(f32 *)((u8 *)move + 0x4C);
            if (period <= lbl_80346488 ||
                (s32)CritterAnimMod(frame - second, period) == 0) {
                result |= 2;
            }
        }
        break;
    }
    case 0x88: {
        s16 idx;
        s16 flags;
        s32 second;
        if ((c->moveFlags & 1) == 0 && frame >= *(s32 *)((u8 *)move + 0x40) &&
            (idx = c->unk124) >= 0) {
            GetPlayerColPos(idx, c->targetPos);
            result |= 1;
        }
        second = *(s32 *)((u8 *)move + 0x44);
        if (second >= 0 && ((flags = c->moveFlags) & 1) != 0 &&
            (flags & 2) == 0 && frame >= second) {
            result |= 2;
        }
        break;
    }
    default: {
        s16 flags = c->moveFlags;
        s32 second;
        if ((flags & 1) == 0 && frame >= *(s32 *)((u8 *)move + 0x40)) {
            result |= 1;
        }
        second = *(s32 *)((u8 *)move + 0x44);
        if (second >= 0 && (flags & 2) == 0 && frame >= second) {
            result |= 2;
        }
        break;
    }
    }
    return result;
}

/* 0x8003C40C -- select/blend the active sequence, animate auxiliary trees,
 * and hand completed moves to CritterMoveDone. */
typedef struct CritterAnimPatternRow {
    u8 _pad00[0x22];
    s16 sequence[8];
    u8 _pad32[0x1E];
} CritterAnimPatternRow;

typedef struct CritterAnimateHeader {
    u8 _pad000[0x124];
    CritterMove *moves;
    CritterAnimPatternRow *patterns;
} CritterAnimateHeader;

#pragma opt_propagation off
void CritterAnimate(Critter *c)
{
    CritterMove *current;
    CritterMove *next;
    u8 *subnode;
    s32 currentIndex;
    s32 nextIndex;
    s32 selectedSequence;
    s32 sequence;
    s32 transition;
    register s32 doneResult;
    u16 done;
    u64 controllerFlag;
    s32 candidate;

    current = NULL;
    next = NULL;
    currentIndex = c->curmove;
    if (c->unk11E < 0 || c->unk120 < 0 || c->unk120 >= 8) {
        goto requested_move;
    }
    candidate = ((CritterAnimateHeader *)c->hdr)
                    ->patterns[c->unk11E]
                    .sequence[c->unk120];
    if (candidate < 0) {
requested_move:
        candidate = c->nextmove;
    }
    nextIndex = candidate;
    if (currentIndex >= 0) {
        current = &((CritterAnimateHeader *)c->hdr)->moves[currentIndex];
    }
    if (candidate >= 0) {
        next = &((CritterAnimateHeader *)c->hdr)->moves[candidate];
    }

    if (next == NULL) {
        sequence = current->anim;
        transition = 0;
    } else {
        controllerFlag = gControllerButtons & 0x80;
        if (controllerFlag == 0 && next != current && next->unk08 >= 0xF00 &&
            (current == NULL || current->unk56 != 0)) {
            sequence = next->anim;
            transition = 3;
        } else if (c->rate > sMusicFadeBase) {
            sequence = current != NULL ? current->anim : -1;
            nextIndex = currentIndex;
            transition = 0;
        } else {
            selectedSequence = 0;
            if (controllerFlag != (u64)(u32)selectedSequence) {
                if (next != NULL) {
                    selectedSequence = next->anim;
                }
                sequence = selectedSequence;
                if (current == NULL) {
                    transition = 3;
                } else if (current != next && current->type == 1) {
                    transition = 3;
                } else {
                    transition = 1;
                }
            } else {
                sequence = next->anim;
                if (current != NULL) {
                    transition = CritterGetDmove(current, next);
                } else {
                    transition = 3;
                }
            }
        }
    }

    if (sequence < 0) {
        FatalError(lbl_80112174, 0x800000);
    }
    if (current != next && transition == 0 && sMusicFadeBase > c->rate &&
        AnimDone(&c->sound[0])) {
        transition = 1;
    }

    if (c->pausecnt > 0) {
        c->animtimer += gClockFrameStep;
        done = 0;
    } else {
        done = AnimateATree(&c->colhandle, sequence, transition);
    }
    if (current != NULL && current == next && current->type == 0) {
        done = 0;
    }
    for (subnode = (u8 *)c->subnodes; subnode != NULL;
         subnode = *(u8 **)(subnode + 0x50)) {
        if (sequence >= *(s16 *)(subnode + 0x10)) {
            sequence = 0;
        }
        AnimateATree(subnode, sequence, transition);
    }

    done &= 3;
    doneResult = (s16)done;
    c->movedone = (s16)doneResult;
    if ((s16)doneResult != 0) {
        CritterMoveDone(c, nextIndex);
    } else if (nextIndex < 0 && AnimDone(&c->sound[0])) {
        c->curmove = -1;
    }
}
#pragma opt_propagation reset

/* 0x8003C6FC -- record cooldown/pattern progress and install the move that
 * just completed its blend. */
void CritterMoveDone(Critter *c, s32 moveIndex)
{
    u8* m;
    Critter* child;
    s32 n;
    s32 fx;
    f32* cf = (f32*)c;

    fx = *(s16*)((u8*)c + 280);
    m = 0;
    if (fx >= 0) {
        m = *(u8**)(*(u8**)((u8*)c + 4) + 292) + fx * 144;
    }
    if (m != 0) {
        switch (*(s32*)m) {
        case 16:
            if (*(s16*)(m + 84) < 0) {
                if (lbl_8034489C == 1) {
                    lbl_8034489C = 2;
                }
            }
            break;
        case 34:
            if (lbl_8034489C == 3) {
                lbl_8034489C = 4;
            }
            break;
        }
    }
    if (*(s16*)((u8*)c + 284) >= 0) {
        child = *(Critter**)((u8*)c + 2780);
        if (child != 0 && *(s16*)((u8*)child + 284) >= 0) {
        } else {
            *(s16*)((u8*)c + 288) = *(s16*)((u8*)c + 288) + 1;
            n = *(s16*)((u8*)c + 284);
            fx = *(s16*)((u8*)c + 288);
            if (fx < 8 &&
                (&((s16*)(*(u8**)(*(u8**)((u8*)c + 4) + 296) +
                          n * 80))[fx])[16] >= 0) {
            } else {
                *(s16*)((u8*)c + 284) = -1;
                *(s16*)((u8*)c + 288) = -1;
                child = *(Critter**)((u8*)c + 2776);
                while (child != 0) {
                    *(s16*)((u8*)child + 284) = -1;
                    *(s16*)((u8*)child + 288) = -1;
                    child = *(Critter**)((u8*)child + 2776);
                }
            }
        }
    } else {
        n = *(s16*)((u8*)c + 286);
        if (n >= 0) {
            (&cf[n])[198] = sMusicFadeBase;
            *(s16*)((u8*)c + 284) = *(s16*)((u8*)c + 286);
            *(s16*)((u8*)c + 288) = 0;
        } else {
            (&cf[moveIndex])[134] =
                (f32)(lbl_80346608 * (f32)(*(s16*)((u8*)c + 136) - 2) +
                      sMusicFadeBase);
        }
    }
    fx = *(s16*)((u8*)c + 2746);
    if (fx >= 0) {
        *(s16*)((u8*)c + 2746) = DeleteEffect(fx, 1);
    }
    *(s16*)((u8*)c + 280) = moveIndex;
    *(f32*)((u8*)c + 532) = lbl_80346470;
}

/* 0x8003C8D4 -- classify two critters' facing/positions into a 0/1/2 code by
 * the relation encoded in a->curmove (0x56). */
s32 CritterGetDmove(CritterMove *a, CritterMove *b)
{
    s32 av;
    s32 bv;
    s32 result;

    result = 1;
    av = a->unk08;
    bv = b->unk08;
    switch (a->unk56) {
    case 0:
        result = 0;
        break;
    case 20:
        if ((bv & ~0xFF) > (av & ~0xFF)) {
            result = 2;
        }
        break;
    case 40:
    default:
        if (bv > av) {
            result = 2;
        }
        break;
    case 60:
        if (bv >= av) {
            result = 2;
        }
        break;
    case 80:
        if (bv > 0) {
            result = 2;
        }
        break;
    case 90:
        result = 2;
        break;
    }
    return result;
}

/* 0x8003C988 -- select an available move of the requested type, preferring
 * the candidate whose cooldown expires first. */
#pragma opt_propagation off
s32 CritterFindMoveType(Critter *c, s32 type, s32 mode)
{
    s32 timeOffset;
    s32 moveOffset;
    CritterMove *move;
    u8 *hdr;
    s32 i;
    s32 result;
    f32 best;
    f32 remaining;

    hdr = (u8 *)c->hdr;
    i = 0;
    timeOffset = 0;
    moveOffset = 0;
    result = -1;
    best = lbl_80346470;

    for (; i < *(s16 *)(hdr + 0x110);
         i++, timeOffset += 4, moveOffset += sizeof(CritterMove)) {
        move = (CritterMove *)(*(u8 **)(hdr + 0x124) + moveOffset);
        if ((move->flags & 4) == 0 && move->type == type) {
            if ((f64)move->cooldown > lbl_80346488) {
                remaining =
                    *(f32 *)((u8 *)c + 0x218 + timeOffset) +
                    move->cooldown - sMusicFadeBase;
            } else {
                remaining = *(volatile f32 *)&lbl_80346470;
            }
            if ((f64)remaining <= lbl_80346488 || mode != 0) {
                if (result < 0 || remaining < best) {
                    best = remaining;
                    result = i;
                    if (mode == 2) {
                        break;
                    }
                }
            }
        }
    }

    if (result < 0 && mode != 0) {
        ErrorPrintf(lbl_8011219C, type, mode);
        result = CritterFindMoveType(c, 0x20, 1);
    }
    return result;
}
#pragma opt_propagation on
/* -- externs used by CritterAnimInterrupt -- */
extern void *SfxGetNode(s32 node);
extern void  PlayerSetParent(Player *p, void *node, f32 *offset);
extern void  PlayerUnsetParent(Player *p);
extern void  DmgFxCircleUpdate(void *fx, f32 radius, s32 flag);
extern void *DmgFxCircleAdd(void *emitter, f32 a, f32 b, f32 c, f32 *v, s32 z);
extern void  DmgFxConeUpdate(void *fx, f32 a, f32 b, f32 c, f32 d, s32 flag);
extern void *DmgFxConeAdd(void *emitter, f32 a, f32 b, f32 c, f32 d, f32 *v,
                          s32 z);
extern void  BossSpewCoins(f32 *origin, f32 *dir, f32 angle);
extern f32   acosf(f32 x);
extern s32   gGameOptions[];
extern f32   lbl_80127D00[];
extern f64   lbl_80346610;
extern f32   lbl_803464F0;
/* 0x8003CA98 -- dispatch one move action descriptor on activation or release. */
void CritterAnimInterrupt(Critter *c, s32 action, s32 phase, s32 active)
{
    CritterBigState *big = &gBig;
    u8 *desc;
    s16 type;
    s32 i;
    s32 node;
    Player *pp;
    f32 v[3];
    f32 dir[3];
    u8 unused[16];

    desc = *(u8 **)(*(u8 **)((u8 *)c->hdr + 0x130) + 0x44) + action * 0x50;
    type = *(s16 *)desc;
    switch (type) {
    case 5:
        if (active) {
            for (i = 0; i < lbl_80344658; i++) {
                node = CritterDoTexmodNode(c, action, 0, lbl_80127D00);
                if (node >= 0) {
                    u8 *row = (u8 *)big + i * 4;
                    MBNodeSetParent(SfxGetNode(node),
                                    ItemGetNode((void *)*(u32 *)(row + 0x50)));
                }
            }
        }
        break;
    case 6:
        if (active) {
            if (lbl_80344658 != 0) {
                lbl_80344654 = SafeRockNearestTarget(c->unk124);
                if (lbl_80344654 >= 0) {
                    node = CritterDoTexmodNode(c, action, 0, lbl_80127D00);
                    if (node >= 0) {
                        s32 frames;
                        MBNodeSetParent(
                            SfxGetNode(node),
                            ItemGetNode(
                                (void *)big->safeRockIndices[lbl_80344654]));
                        frames = -1;
                        if (node >= 0) {
                            frames = *(s16 *)(Effects + node * 240 + 0x2C);
                        }
                        frames = frames - 1;
                        big->safeRockTimers[lbl_80344654] =
                            (f32)(lbl_80346610 * (f64)frames);
                    }
                }
            }
        }
        break;
    case 8:
        if (active) {
            CritterDoTexmodNode(c, action, 0, c->targetPos);
        }
        break;
    case 0:
        CritterNodePlayerCollide(c, (CritterDamageDef *)desc, 1);
        CritterNodeEnemyCollide(c, desc);
        if (active) {
            CritterDoTexmodNode(c, action, 1, NULL);
        }
        if (c->emitter != NULL) {
            DmgFxCircleUpdate(c->emitter, *(f32 *)(desc + 0x0C), 1);
        } else if ((gControllerButtons & 0x10) && gGameOptions[8]) {
            c->emitter = DmgFxCircleAdd(c->obj_d0, *(f32 *)(desc + 0x0C),
                                        *(f32 *)(desc + 0x1C),
                                        *(f32 *)(desc + 0x14),
                                        (f32 *)(desc + 0x20), 0);
        }
        break;
    case 4:
        CritterFirePlayerCollide(c, (CritterDamageDef *)desc);
        if (active) {
            CritterDoTexmodNode(c, action, 1, NULL);
        }
        if (c->emitter != NULL) {
            DmgFxConeUpdate(c->emitter, *(f32 *)(desc + 0x08),
                            *(f32 *)(desc + 0x0C), *(f32 *)(desc + 0x1C),
                            *(f32 *)(desc + 0x14), 1);
        } else if ((gControllerButtons & 0x10) && gGameOptions[8]) {
            c->emitter = DmgFxConeAdd(c->obj_d0, *(f32 *)(desc + 0x08),
                                      *(f32 *)(desc + 0x0C),
                                      *(f32 *)(desc + 0x1C),
                                      *(f32 *)(desc + 0x14),
                                      (f32 *)(desc + 0x20), 0);
        }
        break;
    case 1:
        if (active) {
            CritterDoTexmodNode(c, action, 0, c->moveOrigin);
        }
        break;
    case 7:
        if (phase == 1) {
            if (c->unk128 < 0) {
                node = CritterNodePlayerCollide(c, (CritterDamageDef *)desc, 0);
                if (node >= 0) {
                    pp = &gPlayers[node];
                    PlayerSetParent(pp, c->obj_d0, (f32 *)(desc + 0x20));
                    c->unk128 = (s16)node;
                    if (*(s16 *)(desc + 0x42) >= 0) {
                        SfxSetParent(
                            CritterDoSfx(c, *(s16 *)(desc + 0x42), NULL, 0, -1),
                            *(void **)((u8 *)pp + 0x74));
                    }
                }
            }
            if (active) {
                CritterDoTexmodNode(c, action, 0, c->moveOrigin);
            }
            if (c->emitter != NULL) {
                DmgFxCircleUpdate(c->emitter, *(f32 *)(desc + 0x0C), 1);
            } else if ((gControllerButtons & 0x10) && gGameOptions[8]) {
                c->emitter = DmgFxCircleAdd(c->obj_d0, *(f32 *)(desc + 0x0C),
                                            *(f32 *)(desc + 0x1C),
                                            *(f32 *)(desc + 0x14),
                                            (f32 *)(desc + 0x20), 0);
            }
        } else if (phase == 2) {
            if (c->unk128 >= 0) {
                pp = &gPlayers[c->unk128];
                PlayerUnsetParent(pp);
                dir[0] = *(f32 *)((u8 *)c->mbnode + 0x20);
                dir[1] = *(f32 *)((u8 *)c->mbnode + 0x24);
                dir[2] = *(f32 *)((u8 *)c->mbnode + 0x28);
                dir[1] = lbl_803464F0;
                NormalVector(dir);
                dir[0] = dir[0] * *(f32 *)(desc + 0x30);
                dir[1] = dir[1] * *(f32 *)(desc + 0x30);
                dir[2] = dir[2] * *(f32 *)(desc + 0x30);
                CritterDamagePlayer(pp, c, (CritterDamageDef *)desc, 0x8050,
                                    dir, 0);
                c->unk128 = -1;
            }
        }
        break;
    case 2:
    case 3:
        if (active) {
            CritterDoTexmodNode(c, action, 1, NULL);
        }
        break;
    case 9:
        if (active) {
            f32 angle = acosf(*(f32 *)(desc + 0x18));
            v[0] = *(f32 *)((u8 *)c + 0x3F8);
            v[1] = *(f32 *)((u8 *)c + 0x3FC);
            v[2] = *(f32 *)((u8 *)c + 0x400);
            YawVec3(v, v, *(f32 *)(desc + 0x14));
            PitchVec3(v, v, *(f32 *)(desc + 0x1C));
            v[0] = v[0] * *(f32 *)(desc + 0x30);
            v[1] = v[1] * *(f32 *)(desc + 0x30);
            v[2] = v[2] * *(f32 *)(desc + 0x30);
            BossSpewCoins(c->moveOrigin, v, angle);
        }
        break;
    default:
        if (active) {
            CritterDoTexmodNode(c, action, 1, NULL);
        }
        break;
    }
}

/* 0x8003D0A4 -- execute the visual/sound payload attached to an action
 * descriptor at either a supplied world position or the critter node. */
s32 CritterDoTexmodNode(Critter *c, s32 action, s32 local, f32 *position)
{
    u8 unused[8];
    f32 world[3];
    u8 worldPad[4];
    f32 velocity[3];
    u8 velocityPad[4];
    f32 offset[3];
    f32 angularVelocity[3];
    u8 *container;
    u8 *desc;
    u8 *sfxDesc;
    u8 *hitDesc;
    u8 *morphDesc;
    u32 flags;
    s32 result;
    s32 morph;
    s32 morphTarget;
    f32 scale;
    f32 radius;
    f32 damage;
    f32 damageRadius;
    f32 speed;
    f32 yaw;

    container = *(u8 **)((u8 *)c->hdr + 0x130);
    desc = *(u8 **)(container + 0x44) + action * 0x50;
    if ((*(s16 *)(desc + 2) & 0x4000) != 0 &&
        (f64)c->unkAC8 > lbl_80346488 && *(s16 *)desc != 1) {
        return -1;
    }
    if (c->mbnode != NULL &&
        (*(u32 *)((u8 *)c->mbnode + 0x60) & 8) != 0) {
        scale = *(f32 *)((u8 *)c->mbnode + 0x44);
    } else {
        scale = lbl_803464A8;
    }
    offset[0] = *(f32 *)(desc + 0x20) * scale;
    offset[1] = *(f32 *)(desc + 0x24) * scale;
    offset[2] = *(f32 *)(desc + 0x28) * scale;
    if (local) {
        world[0] = offset[0];
        world[1] = offset[1];
        world[2] = offset[2];
        if (position != NULL) {
            MulBodyVecMat4(position, position, &c->mtx[0][0]);
            world[0] = position[0] + world[0];
            world[1] = position[1] + world[1];
            world[2] = position[2] + world[2];
        }
    } else if (position != NULL) {
        MulVecMat3(offset, world, &c->mtx[0][0]);
        world[0] = position[0] + world[0];
        world[1] = position[1] + world[1];
        world[2] = position[2] + world[2];
    } else {
        MulVecMat4(offset, world, &c->mtx[0][0]);
    }

    result = CritterDoSfx(c, *(s16 *)(desc + 0x40), world, local, -1);
    if (result < 0) {
        goto done;
    }

    flags = 0x801;
    sfxDesc = *(u8 **)(container + 0x4C) + *(s16 *)(desc + 0x40) * 0x50;
    radius = *(f32 *)(desc + 0x2C);
    if (*(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x20) != 4) {
        flags |= 8;
        fn_80037ED0(lbl_80346618, c, Effects[result].id);
    }

    switch (*(s16 *)desc) {
    case 1:
        flags |= 6;
        if ((*(s16 *)(desc + 2) & 0x4000) != 0 &&
            (f64)c->unkAC8 > lbl_80346488) {
            Effects[result].endtime = gClockTime + c->unkAC8;
        }
        break;
    case 2:
        flags |= 0x30;
        break;
    case 3:
    case 5:
    case 6:
        flags |= 0x20;
        break;
    case 8:
        flags |= 0x20;
        break;
    case 4:
        radius = lbl_80346480;
        flags = 0;
        break;
    }

    if ((*(u32 *)(desc + 4) & 0x00020000) != 0) {
        flags |= 0x00020000;
    }
    if ((*(s16 *)(desc + 2) & 0x40) != 0) {
        flags &= ~6;
    }
    if ((*(s16 *)(desc + 2) & 0x1000) != 0) {
        flags &= ~1;
    }
    if ((*(s16 *)(desc + 2) & 0x2000) != 0) {
        flags |= 0x400;
    }
    Effects[result].flags |= flags;

    if ((*(u32 *)sfxDesc & 0x10) != 0 && *(s16 *)(desc + 0x42) < 0) {
        PlaceEffectOnFloor(result, (f32 *)Effects[result].node);
    }
    if (*(s16 *)desc != 1) {
        if (lbl_80346470 != *(f32 *)(desc + 0x14)) {
            YawMat3(*(f32 *)(desc + 0x14), (f32 *)Effects[result].node);
        }
        if (lbl_80346470 != *(f32 *)(desc + 0x1C)) {
            WPitchMat3((f32 *)Effects[result].node, *(f32 *)(desc + 0x1C));
        }
    }

    if (radius >= lbl_80346470) {
        damage = radius * *(f32 *)((u8 *)gCurLevel + 0xBC);
        damageRadius = *(f32 *)(desc + 0x0C) * scale;
        radius = *(f32 *)(desc + 8) * scale;
        Effects[result].damage = damage;
        Effects[result].mindp = *(f32 *)(desc + 0x18);
        Effects[result].damageradius = damageRadius;
        Effects[result].damagetype = *(DMG_TYPE *)(desc + 4);

        if (*(s16 *)(desc + 0x42) >= 0) {
            hitDesc = *(u8 **)(container + 0x4C) +
                      *(s16 *)(desc + 0x42) * 0x50;
            SfxSetHit(result, *(s32 *)(hitDesc + 8), *(s32 *)(hitDesc + 0xC),
                      *(s32 *)(hitDesc + 0xC));
            if ((*(u32 *)hitDesc & 0x10) != 0) {
                Effects[result].flags |= 0x200000;
            }
        }

        if (*(s16 *)(desc + 0x44) >= 0) {
            morphDesc = *(u8 **)(container + 0x4C) + 8;
            morphTarget = *(s32 *)(morphDesc +
                                    *(s16 *)(desc + 0x44) * 0x50);
            morph = 0;
            if (*(s16 *)(desc + 0x46) >= 0) {
                morph = *(s32 *)(morphDesc +
                                  *(s16 *)(desc + 0x46) * 0x50);
            }
            speed = *(f32 *)(desc + 0x3C);
            if ((f64)speed <= lbl_80346488) {
                speed = lbl_803464BC;
            }
            SfxSetMorph(speed, result, morphTarget, morph);
            if ((*(s16 *)(desc + 2) & 0x800) != 0) {
                Effects[result].flags |= 0x8000;
            }
            if ((*(u32 *)(desc + 4) & 0x04000000) != 0) {
                Effects[result].webtime = speed;
                Effects[result].flags &= ~0x20;
            }
        }

        if (*(f32 *)(desc + 0x30) > lbl_80346470) {
            speed = (f32)(((f64)c->rateScale < lbl_803464F8)
                              ? lbl_803464F8
                              : ((f64)c->rateScale > lbl_80346620)
                                    ? lbl_80346620
                                    : (f64)c->rateScale);
            speed = (f32)(lbl_80346530 *
                          ((f64)speed -
                           *(volatile f64 *)&lbl_803464F8)) *
                        (*(f32 *)(desc + 0x34) - *(f32 *)(desc + 0x30)) +
                    *(f32 *)(desc + 0x30);

            if ((*(s16 *)(desc + 2) & 4) != 0) {
                velocity[0] = *(f32 *)((u8 *)c + 0x2C);
                velocity[1] = *(f32 *)((u8 *)c + 0x30);
                velocity[2] = *(f32 *)((u8 *)c + 0x34);
            } else if ((*(s16 *)(desc + 2) & 1) != 0 && c->unk124 >= 0) {
                GetPlayerColPos(c->unk124, velocity);
                velocity[0] -= *(f32 *)((u8 *)Effects[result].node + 0x30);
                velocity[1] -= *(f32 *)((u8 *)Effects[result].node + 0x34);
                velocity[2] -= *(f32 *)((u8 *)Effects[result].node + 0x38);
            } else {
                velocity[0] = *(f32 *)((u8 *)c + 0x2C);
                velocity[1] = *(f32 *)((u8 *)c + 0x30);
                velocity[2] = *(f32 *)((u8 *)c + 0x34);
                velocity[1] = lbl_80346628;
            }

            if ((*(s16 *)(desc + 2) & 8) == 0) {
                CalcTargetDir(velocity, speed,
                              (f32)(lbl_80346490 / (f64)speed),
                              *(f32 *)(desc + 0x38),
                              lbl_80346470);
            } else {
                NormalVector(velocity);
            }

            if (*(s16 *)desc == 1) {
                if (lbl_80346470 != *(f32 *)(desc + 0x14) ||
                    lbl_80346470 != *(f32 *)(desc + 0x48)) {
                    yaw = *(f32 *)(desc + 0x14);
                    if (*(f32 *)(desc + 0x48) >
                        *(volatile f32 *)&lbl_80346470) {
                        yaw = (f32)((f64)yaw +
                                    lbl_803464F8 *
                                        -(f64)*(f32 *)(desc + 0x48) +
                                    (f64)Random(*(f32 *)(desc + 0x48)));
                    }
                    YawVec3(velocity, velocity, yaw);
                }
                if ((*(s16 *)(desc + 2) & 8) != 0 &&
                    lbl_80346470 != *(f32 *)(desc + 0x1C)) {
                    PitchVec3(velocity, velocity, *(f32 *)(desc + 0x1C));
                }
            }
            velocity[0] *= speed;
            velocity[1] *= speed;
            velocity[2] *= speed;

            if ((*(u32 *)sfxDesc & 8) != 0) {
                angularVelocity[0] = Random(lbl_8034662C);
                angularVelocity[1] = lbl_80346470;
                angularVelocity[2] = Random(lbl_8034662C);
                fn_80093E50(result, velocity, angularVelocity,
                            *(f32 *)(desc + 0x38), radius);
            } else {
                fn_80093E50(result, velocity, NULL, *(f32 *)(desc + 0x38),
                            radius);
            }
        } else {
            fn_80093E50(result, NULL, NULL, *(f32 *)(desc + 0x38), radius);
        }

        if ((gControllerButtons & 0x10) != 0 && gGameOptions[8] != 0) {
            DmgFxAdd(result);
        }
    }
done:
    return result;
}
/* 0x8003D7E0 */
s32 CritterDoSfx(Critter *c, s32 sfx, void *parent, s32 arg3, s32 arg4)
{
    u8 *entry;
    s32 result;
    u32 flags;
    f32 mtxTmp[16];
    u32 unusedHigh;
    f32 world[3];
    u32 unusedLow;
    f32 color[3];
    f32 scale;
    f32 skinValue;
    s32 nodeCount;
    s32 audio;
    s16 skinParam;

    result = -1;
    if (sfx < 0) {
        return -1;
    }
    entry = *(u8 **)(*(u8 **)((u8 *)c->hdr + 0x130) + 0x4C) + sfx * 0x50;
    flags = *(u32 *)entry;

    if ((flags & 0x400) != 0) {
        if ((gControllerButtons & 0x80) != 0) {
            return -1;
        }
        MBTreeSetFlags(c->anim, 1, 0);
        MBTreeSetFlags(*(void **)((u8 *)c->anim + 0x78), 2, 2);
    }

    if (c->mbnode != NULL && (*(u32 *)((u8 *)c->mbnode + 0x60) & 8) != 0) {
        scale = *(f32 *)((u8 *)c->mbnode + 0x44);
    } else {
        scale = lbl_803464A8;
    }
    color[0] = *(f32 *)(entry + 0x30) * scale;
    color[1] = *(f32 *)(entry + 0x34) * scale;
    color[2] = *(f32 *)(entry + 0x38) * scale;

    if ((flags & 0x0F000000) != 0) {
        CritterDoParticle(c, entry, arg4);
    } else if ((flags & 0x100) != 0) {
        skinValue = *(f32 *)(entry + 0x40);
        nodeCount = (s32)(lbl_80346630 * *(f32 *)(entry + 0x3C));
        skinParam = *(s16 *)(entry + 0x44);
        if (*(s32 *)(entry + 8) >= 0) {
            SetSkinFX((u8 *)c + 0xE0, *(s32 *)(entry + 8),
                      nodeCount, skinParam, skinValue);
        } else {
            SetSkinFX((u8 *)c + 0xE0, lbl_802897B8[c->counterState & 0xF], 10,
                      0, lbl_803464E8);
        }
    } else if ((flags & 0x200) != 0) {
        nodeCount = 0;
        arg4 = 0;
        for (; nodeCount < *(s16 *)((u8 *)c->hdr + 0x118);
             nodeCount++, arg4 += 0x5C) {
            u8 *node = (u8 *)c + 0x4F8 + arg4;
            if ((*(s16 *)(*(u8 **)node + 0x10) & 1) == 0) {
                world[0] = *(f32 *)(node + 0x3C) + color[0];
                world[1] = *(f32 *)(node + 0x40) + color[1];
                world[2] = *(f32 *)(node + 0x44) + color[2];
                result = CritterDoSfxSub(c, entry, world, 0, flags);
            }
        }
    } else if (*(s32 *)(entry + 8) >= 0) {
        if ((flags & 0x801) != 0) {
            arg3 = 1;
            world[0] = color[0];
            world[1] = color[1];
            world[2] = color[2];
        } else if ((flags & 0x80) != 0) {
            arg3 = 0;
            world[0] = *(f32 *)((u8 *)c + 0x418) + color[0];
            world[1] = *(f32 *)((u8 *)c + 0x41C) + color[1];
            world[2] = *(f32 *)((u8 *)c + 0x420) + color[2];
        } else if ((flags & 0x40) != 0) {
            if (c->obj_d0 != NULL) {
                GetWorldMat(c->obj_d0, mtxTmp, color);
                world[0] = mtxTmp[12];
                world[1] = mtxTmp[13];
                world[2] = mtxTmp[14];
            } else {
                world[0] = c->vel[0];
                world[1] = c->vel[1];
                world[2] = c->vel[2];
            }
            if (parent != NULL) {
                world[0] = ((f32 *)parent)[0] + world[0];
                world[1] = ((f32 *)parent)[1] + world[1];
                world[2] = ((f32 *)parent)[2] + world[2];
            }
            arg3 = 0;
        } else if (parent != NULL) {
            world[0] = ((f32 *)parent)[0] + color[0];
            world[1] = ((f32 *)parent)[1] + color[1];
            world[2] = ((f32 *)parent)[2] + color[2];
        } else {
            world[0] = color[0];
            world[1] = color[1];
            world[2] = color[2];
        }
        result = CritterDoSfxSub(c, entry, world, arg3, flags);
    } else {
        result = -1;
    }

    audio = *(s32 *)(entry + 0xC);
    if (audio >= 0) {
        if (c->curmove >= 0 &&
            (*(CritterMove **)((u8 *)c->hdr + 0x124))[c->curmove].type == 17) {
            AudioPlay3DSel(audio, 224, c->vel, 0);
        } else {
            AudioPlay3DSel(audio, 224, c->vel, 1);
        }
    }
    if ((flags & 2) != 0) {
        ShakeCamera(0, 0, 90, lbl_80346570, 100);
    }
    if ((flags & 0x20) != 0) {
        SafeRockSetup();
    }
    if ((*(u32 *)entry & 0x40000) != 0) {
        if (c->unkABA >= 0) {
            ErrorPrintf(lbl_801121C0);
        } else {
            c->unkABA = (s16)result;
        }
    }
    if (*(s32 *)(entry + 4) >= 0) {
        CritterDoSfx(c, *(s32 *)(entry + 4), parent, arg3, result);
    }
    return result;
}

/* 0x8003DC64 -- create one ordinary effect and apply owner, parent, color
 * and scale properties encoded by the critter sound descriptor. */
s32 CritterDoSfxSub(Critter *c, u8 *sfx, f32 *position,
                    s32 parented, u32 flags)
{
    u32 color;
    u32 treeFlags;
    u32 effectFlags;
    s32 useSceneRoot;
    s32 result;
    s32 effect;
    void *parent;
    u8 *effectData;
    f32 scale;

    effect = *(s32 *)(sfx + 8);
    if (effect < 0) goto fail;
    treeFlags = 0x800;
    effectFlags = 0;
    if ((flags & 4) != 0) treeFlags |= 0x80080;
    if ((flags & 0x100000) != 0) treeFlags |= 0x80040;
    if ((flags & 0x1000) != 0) treeFlags |= 0x801000;
    if ((flags & 0x80000) != 0) treeFlags |= 0x40000000;
    useSceneRoot = flags & 0x2000;
    if (useSceneRoot != 0) treeFlags &= ~0x800;
    if ((flags & 0x10) != 0) effectFlags |= 0x200000;
    if ((flags & 0x8000) != 0) {
        effectFlags |= sMusicTrackHi == 8 ? 0x08000000 : 0x10000;
    }
    if ((flags & 0x10000) != 0) effectFlags |= 0x400000;
    if ((flags & 0x20000) != 0) effectFlags |= 0x02000000;
    if ((flags & 0x400000) != 0) effectFlags |= 0x04000000;
    if ((flags & 0x800000) != 0) effectFlags |= 0x20000000;

    result = StartFXSub(effect, position, effectFlags, treeFlags,
                        *(f32 *)(sfx + 0x3C));
    SfxSetOwner(result, c->id | 0x1000);
    if (useSceneRoot != 0) {
        SfxSetParent(result, lbl_80344EB4);
    } else if ((flags & 0x40) != 0) {
        SfxSetMat(result, (f32 *)c->mbnode, NULL);
    } else if (parented) {
        if ((flags & 0x800) != 0) {
            parent = *(void **)((u8 *)c->anim + 0x74);
        } else if ((flags & 1) != 0) {
            parent = c->anim;
        } else {
            parent = c->obj_d0;
        }
        SfxSetParent(result, parent);
    }
    color = *(u32 *)(sfx + 0x48);
    if (color != 0xFFFFFFFF) {
        effectData = (u8 *)Effects;
        effectData += result * sizeof(Effect);
        MBTreeSetColor(**(void ***)(effectData += 0x18), color, 1);
    }
    scale = *(f32 *)(sfx + 0x4C);
    if (c->mbnode != NULL &&
        (*(u32 *)((u8 *)c->mbnode + 0x60) & 8) != 0) {
        scale *= *(f32 *)((u8 *)c->mbnode + 0x44);
    }
    if (scale != 1.0) {
        MBTreeSetScale(scale, scale, scale, Effects[result].node);
    }
    goto done;

fail:
    result = -1;
done:
    return result;
}

/* 0x8003DE70 -- create and configure a particle system from one descriptor. */
void CritterDoParticle(Critter *c, void *sfx, s32 node)
{
    u8 *s = (u8 *)sfx;
    f32 rate;
    f32 speed;
    f32 etime;
    s32 tex;
    u32 flags;
    u32 kind;
    void *parent;
    void *psys;

    rate = (f32)(lbl_80346630 * *(f32 *)(s + 0x40));
    flags = *(u32 *)s;
    tex = *(s32 *)(s + 8);
    etime = *(f32 *)(s + 0x3C);
    speed = (f32)(lbl_80346540 * (f64)*(s16 *)(s + 0x46));
    kind = flags & 0x0F000000;
    if ((flags & 0x4000) && node >= 0) {
        parent = Effects[node].node;
    } else if (c->obj_d0 != NULL) {
        parent = c->obj_d0;
    } else {
        parent = c->mbnode;
    }
    switch (kind) {
    case 0x02000000:
        psys = MBNewPsysDefault(gIdentityMatrix, parent, 0, 1);
        if (psys != NULL) {
            MBPsysSetEVolume(psys, lbl_80346638, lbl_80346638);
            MBTreeSetFlags(psys, 0x880, 1);
            MBPsysSetPParm(psys, 3, lbl_803464A8, lbl_803464A8, lbl_803464A8,
                           lbl_80346470);
        }
        break;
    case 0x01000000:
    default:
        psys = MBNewPsysDefault(gIdentityMatrix, parent, 0, 1);
        MBPsysSetEVolume(psys, lbl_803464E8, lbl_803464E8);
        break;
    }
    if (psys == NULL) {
        ErrorPrintf(lbl_801121D4);
    } else {
        *(f32 *)((u8 *)psys + 0x30) = *(f32 *)(s + 0x30);
        *(f32 *)((u8 *)psys + 0x34) = *(f32 *)(s + 0x34);
        *(f32 *)((u8 *)psys + 0x38) = *(f32 *)(s + 0x38);
        MBPsysSetPTex(psys, tex);
        MBPsysSetERate4(rate, rate, rate, rate, psys);
        MBPsysSetETime(etime, lbl_8034663C, psys);
        MBPsysSetPSpeed(psys, speed);
    }
}
/* 0x8003E048 -- allocate and initialize a root critter and the child chain
 * described by its loaded type header. */
typedef struct CritterChildLinks {
    u32 words[18];
} CritterChildLinks;

Critter *CritterNewInst(s32 type, s32 subtype, void *object)
{
    u8 *childDef;
    u8 *header;
    Critter *root;
    Critter *tail;
    u8 *childHeader;
    Critter *child;
    void *node;
    u8 *geo;
    s32 nodeIndex;
    s32 childIndex;

    header = (u8 *)gCritterHeaders[type][subtype];
    if (header == NULL) {
        ErrorPrintf("No Critter type %d subtype %d loaded\n", type, subtype);
        return NULL;
    }
    if (*(void **)(header + 0x138) == NULL) {
        return NULL;
    }

    root = CritterEmptyInst();
    if (root == NULL) {
        return NULL;
    }
    CritterInitInst(root, (struct CritterHeader *)header);
    CritterInitGeo(root, object, subtype);
    CritterAddAnimInsts(root, &root->mtx[0][0]);
    CritterInitColnodes(root);
    CritterAddHealthMeter(root);

    childIndex = *(s16 *)(header + 0x11C);
    root->childcnt = 0;
    tail = root;
    while (childIndex >= 0) {
        childHeader = *(u8 **)(*(u8 **)(header + 0x130) + 0x14) +
                      childIndex * 0x140;
        child = CritterEmptyInst();
        CritterInitInst(child, (struct CritterHeader *)childHeader);
        childDef = (u8 *)child->hdr;
        geo = *(u8 **)((u8 *)root->hdr + 0x138);
        *(CritterChildLinks *)&child->colhandle =
            *(CritterChildLinks *)&root->colhandle;

        nodeIndex = AtreeFindNodeIdx(*(void **)(geo + 0x0C),
                                     *(s32 *)(geo + 0x10),
                                     (char *)child->hdr + 0x10, 0x10);
        child->colhandle = (u8 *)root->anodes + nodeIndex * 0x28;
        AtreeNodeSetParent(child->colhandle, NULL, NULL, 0);
        child->anim = *(void **)child->colhandle;

        nodeIndex = *(s16 *)(childDef + 0x56);
        if (nodeIndex < 0) {
            node = NULL;
        } else {
            node = *(void **)((u8 *)child->anodes + nodeIndex * 0x28);
            if (node == NULL) {
                node = NULL;
            }
        }
        child->hitnode0 = node;
        if ((*(u32 *)(childDef + 0x5C) & 0x10) != 0 &&
            child->hitnode0 != NULL &&
            *(void **)((u8 *)child->hitnode0 + 0x74) != NULL) {
            child->hitnode0 = *(void **)((u8 *)child->hitnode0 + 0x74);
        }

        nodeIndex = *(s16 *)(childDef + 0x58);
        if (nodeIndex < 0) {
            node = NULL;
        } else {
            node = *(void **)((u8 *)child->anodes + nodeIndex * 0x28);
            if (node == NULL) {
                node = NULL;
            }
        }
        child->hitnode1 = node;

        nodeIndex = *(s16 *)(childDef + 0x5A);
        if (nodeIndex < 0) {
            node = NULL;
        } else {
            node = *(void **)((u8 *)child->anodes + nodeIndex * 0x28);
            if (node == NULL) {
                node = NULL;
            }
        }
        child->hitnode2 = node;

        CritterInitColnodes(child);
        CritterAddHealthMeter(child);

        tail->next = child;
        tail = child;
        child->parent = root;
        childIndex = *(s16 *)(childHeader + 0x11C);
        root->childcnt++;
    }
    switch (*(s16 *)(*(u8 **)((u8 *)root->hdr + 0x120) + 0x20)) {
    case 8:
        root->particle = FindClosestWaypoint(lbl_80346594,
                                             (f32 *)((u8 *)root + 0x3C), 0);
        break;
    default:
        break;
    }
    return root;
}
/* 0x8003E2E8 -- reserve the first free critter pool slot, wipe it, and stamp
 * it with a fresh index + rolling unique id. */
#pragma opt_propagation off
#pragma opt_common_subs off
Critter *CritterEmptyInst(void)
{
    u8 *c;
    s32 byte_offset;
    s32 i;
    s32 count;
    s32 scan_offset;
    u8 *scan;
    CritterBigState *big;

    big = &gBig;
    count = lbl_8034466C;
    for (i = 0, scan_offset = 0; i < count;
         i++, scan_offset += sizeof(Critter)) {
        scan = (u8 *)big + scan_offset;
        if (*(void **)(scan + 0x238) == NULL) {
            break;
        }
    }
    if (i >= 16) {
        ErrorPrintf(lbl_8011221C, i, count);
        return NULL;
    }
    if (i == count) {
        lbl_8034466C++;
        if (lbl_8034466C > gCritterCountMax) {
            gCritterCountMax = lbl_8034466C;
        }
    }
    byte_offset = i * sizeof(Critter);
    c = (u8 *)big + byte_offset;
    c += 0x234;
    memset(c, 0, sizeof(Critter));
    *(s16 *)c = (s16)i;
    scan = (u8 *)big + byte_offset;
    *(s16 *)(scan + 0x236) = gCritterNextID;
    if ((u16)(gCritterNextID = gCritterNextID + 1) > 4095) {
        gCritterNextID = 1;
    }
    return (Critter *)c;
}
#pragma opt_common_subs reset
#pragma opt_propagation reset
/* 0x8003E3E8 -- instantiate the model/animation tree and cache the principal
 * scene nodes and world-space transforms used by movement and collision. */
void CritterInitGeo(Critter *c, void *object, s32 subtype)
{
    u8 *header;
    s32 atreeFlags;
    void *node;
    void *n;
    s32 idx;
    f32 *gid;

    gid = gIdentityMatrix;
    header = (u8 *)c->hdr;
    c->mbnode = MBNewNode(lbl_8034473C, gid, 1);
    *(f32 *)((u8 *)c + 0xF8) =
        atan2(*(f32 *)((u8 *)object + 0x20), *(f32 *)((u8 *)object + 0x28));
    *(f32 *)((u8 *)c + 0xFC) = *(f32 *)((u8 *)c + 0xF8);
    CopyMat3(gid, &c->mtx[0][0]);
    c->vel[0] = *(f32 *)((u8 *)object + 0x30);
    c->vel[1] = *(f32 *)((u8 *)object + 0x34);
    c->vel[2] = *(f32 *)((u8 *)object + 0x38);
    YawMat3(*(f32 *)((u8 *)c + 0xFC), &c->mtx[0][0]);

    atreeFlags = 0;
    if ((*(u32 *)(header + 0x5C) & 0x1000) == 0) {
        atreeFlags |= 0x800;
    }
    c->colhandle = AtreeInit(*(void **)(header + 0x138), &c->colhandle, 0,
                             atreeFlags);
    c->anim = *(void **)c->colhandle;
    MBNodeSetParent(*(void **)c->colhandle, c->mbnode);

    if ((*(u32 *)(header + 0x5C) & 1) != 0) {
        s16 shadowType = *(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x22);
        s32 shadowIdx = subtype > 2 ? 1 : subtype;
        node = MBOX_ReallyFindObject(lbl_8011AEA0[shadowIdx], shadowType,
                                     shadowType, 1);
        c->shadow = MBNewObject(node, gIdentityMatrix, NULL, 0x880);
        *(f32 *)((u8 *)c->shadow + 0x30) = c->vel[0];
        *(f32 *)((u8 *)c->shadow + 0x34) = c->vel[1];
        *(f32 *)((u8 *)c->shadow + 0x38) = c->vel[2];
        *(f32 *)((u8 *)c->shadow + 0x54) = lbl_80346640;
        *(s16 *)((u8 *)c->shadow + 0x68) = -32;
    }

    idx = *(s16 *)(header + 0x56);
    if (idx < 0) {
        node = NULL;
    } else {
        n = *(void **)((u8 *)c->anodes + idx * 0x28);
        node = n ? n : NULL;
    }
    c->hitnode0 = node;
    if ((*(u32 *)(header + 0x5C) & 0x10) != 0 && c->hitnode0 != NULL &&
        *(void **)((u8 *)c->hitnode0 + 0x74) != NULL) {
        c->hitnode0 = *(void **)((u8 *)c->hitnode0 + 0x74);
    }
    idx = *(s16 *)(header + 0x58);
    if (idx < 0) {
        node = NULL;
    } else {
        n = *(void **)((u8 *)c->anodes + idx * 0x28);
        node = n ? n : NULL;
    }
    c->hitnode1 = node;
    idx = *(s16 *)(header + 0x5A);
    if (idx < 0) {
        node = NULL;
    } else {
        n = *(void **)((u8 *)c->anodes + idx * 0x28);
        node = n ? n : NULL;
    }
    c->hitnode2 = node;

    if (FloorCollide(c->vel, 0, 0, 2, lbl_803464B8, lbl_80346588,
                     lbl_8034658C) != 0) {
        c->vel[1] = *(f32 *)(gFloorCollisionResult + 0x34) +
                    *(f32 *)(header + 0xB0);
        if (c->shadow != NULL) {
            CopyMat3((f32 *)gFloorCollisionResult, c->shadow);
            *(f32 *)((u8 *)c->shadow + 0x30) = c->vel[0];
            *(f32 *)((u8 *)c->shadow + 0x34) = c->vel[1];
            *(f32 *)((u8 *)c->shadow + 0x38) = c->vel[2];
            *(f32 *)((u8 *)c->shadow + 0x34) =
                *(f32 *)(gFloorCollisionResult + 0x34);
        }
    } else {
        c->vel[1] = c->vel[1] + *(f32 *)(header + 0xB0);
    }

    CopyMat4(&c->mtx[0][0], c->mbnode);
    UnparentMatrix(c->mbnode, *(f32 **)((u8 *)c->mbnode + 0x74));
    CopyMat3(&c->mtx[0][0], (f32 *)((u8 *)c + 0x3D8));
    *(f32 *)((u8 *)c + 0x418) = c->vel[0];
    *(f32 *)((u8 *)c + 0x41C) = c->vel[1];
    *(f32 *)((u8 *)c + 0x420) = c->vel[2];
    MulVec4Mat3((f32 *)(header + 0xC0), c->pos, &c->mtx[0][0]);
    c->pos[0] = c->vel[0] + c->pos[0];
    c->pos[1] = c->vel[1] + c->pos[1];
    c->pos[2] = c->vel[2] + c->pos[2];
    c->movevec[0] = c->vel[0];
    c->movevec[1] = c->vel[1] + *(f32 *)(header + 0xB4);
    c->movevec[2] = c->vel[2];
    c->obj_d0 = c->anim;
    GetWorldMat(c->obj_d0, c->worldMoveMatrix, NULL);

    if (*(f32 *)((u8 *)c->hdr + 0xA4) < lbl_80346618) {
        *(f32 *)((u8 *)c + 0x49C) = *(f32 *)((u8 *)c->hdr + 0xA0);
        *(f32 *)((u8 *)c + 0x4A0) = *(f32 *)((u8 *)c->hdr + 0xA4);
        *(f32 *)((u8 *)c + 0x4A4) = *(f32 *)((u8 *)c->hdr + 0xA8);
    } else {
        *(f32 *)((u8 *)c + 0x49C) = *(f32 *)((u8 *)c + 0x418);
        *(f32 *)((u8 *)c + 0x4A0) = *(f32 *)((u8 *)c + 0x41C);
        *(f32 *)((u8 *)c + 0x4A4) = *(f32 *)((u8 *)c + 0x420);
    }
}
/* 0x8003E7D0 -- create the optional HUD meter and attach the optional
 * in-world red health-fill geometry described by the critter header. */
void CritterAddHealthMeter(Critter *c)
{
    u8 *header;
    s32 meter;
    s32 style;
    void *match;
    void *root;

    header = (u8 *)c->hdr;
    meter = -1;
    if ((*(u32 *)(header + 0x5C) & 4) != 0) {
        style = (*(u32 *)(header + 0x5C) & 8) != 0 ? 1 : 0;
        meter = HealthMeterStart(
            header, *(s16 *)(header + 0xF8), *(s16 *)(header + 0xFA),
            *(s16 *)(header + 0xFC), *(s16 *)(header + 0xFE),
            style, c->health);
    }
    c->healthmtr = (s16)meter;

    if ((*(u32 *)(header + 0x5C) & 0x800) != 0) {
        match = AtreeMatch(
            *(void **)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x28),
            lbl_80346644, 1);
        if (match != NULL) {
            *(void **)&c->healthbar[0] =
                AtreeInit(match, &c->healthbar[0], 0, 0x800);
            MBNodeSetParent(**(void ***)&c->healthbar[0], c->mbnode);
            MBTreeSetFlags(**(void ***)&c->healthbar[0], 0x02000000, 0);

            root = **(void ***)&c->healthbar[0];
            *(f32 *)((u8 *)root + 0x30) =
                *(f32 *)((u8 *)root + 0x30) +
                *(f32 *)((u8 *)c->hdr + 0x100);
            *(f32 *)((u8 *)**(void ***)&c->healthbar[0] + 0x34) +=
                *(f32 *)((u8 *)c->hdr + 0x104);
            *(f32 *)((u8 *)**(void ***)&c->healthbar[0] + 0x38) +=
                *(f32 *)((u8 *)c->hdr + 0x108);

            c->damageflash =
                AtreeFindNode(&c->healthbar[0], lbl_80112238, 9);
        }
    }
}
/* 0x8003E920 -- bind a fresh critter instance to its loaded header: reset move
 * bookkeeping, scale starting health by the level, and zero the per-move and
 * hit-node scratch tables. */
void CritterInitInst(Critter *c, struct CritterHeader *hdr)
{
    s32 i;
    u8 *h;

    h = (u8 *)hdr;
    c->hdr = hdr;
    c->state = 0;
    c->curmove = -1;
    c->nextmove = 0;
    c->unk11C = -1;
    c->unk11E = -1;
    c->unk120 = -1;
    c->unk124 = -1;
    c->unk126 = -1;
    c->unk128 = -1;
    c->unkABA = -1;
    c->unkABC = -1;
    c->unkABE = 0;
    c->unkAC0 = -1;
    c->pausecnt = 0;
    c->unkAC6 = 0;
    c->unkAC8 = 0.0f;
    c->unk4AC = 0.0f;
    c->health = *(f32 *)(h + 228) * *(f32 *)((u8 *)gCurLevel + 172);
    for (i = 0; i < 4; i++) {
        c->unk1BC[i][0] = 0.0f;
        c->unk1BC[i][1] = 0.0f;
        c->unk1BC[i][2] = 0.0f;
        c->unk1BC[i][3] = 0.0f;
    }
    for (i = 0; i < 4; i++) {
        c->unk4E0[i] = -1;
    }
    if ((s16)*(s16 *)(h + 272) > 0) {
        memset(c->moveTimes, 0, *(s16 *)(h + 272) * 4);
    }
    if ((s16)*(s16 *)(h + 276) > 0) {
        memset(c->patternTimes, 0, *(s16 *)(h + 276) * 4);
    }
    if ((s16)*(s16 *)(h + 280) > 0) {
        memset(c->hitnodes, 0, *(s16 *)(h + 280) * 92);
    }
}
/* 0x8003EA4C -- tear down a critter instance: detach scene nodes, kill sfx,
 * recurse into linked children, free colnode list, then clear the slot. */
typedef struct CritterSubnode {
    void *atree;
    u8 _pad04[68];
    void *mbnode;
    u8 _pad4C[4];
    struct CritterSubnode *next;
} CritterSubnode;

void CritterDelInst(Critter *c)
{
    CritterSubnode *node;

    if (*(s16 *)((u8 *)*(void **)((u8 *)c->hdr + 288) + 32) == 4) {
        del_target(c->mtx);
        if (c->parent == NULL) {
            BossDeath();
        }
    }
    if (c->next != NULL) {
        CritterDelInst(c->next);
        c->next = NULL;
    }
    if (c->emitter != NULL) {
        MBRemoveNode(c->emitter, 1);
    }
    if (c->shadow != NULL) {
        MBRemoveNode(c->shadow, 0);
    }
    SfxDeleteParented(c->anim, 1, -1);
    if (*(u32 *)((u8 *)c + 216) != 0) {
        MBRemoveNode(c->emitter, 2);
    }
    if (c->colhandle != NULL) {
        AtreeDelete(&c->colhandle);
    }
    if (c->mbnode != NULL) {
        MBRemoveNode(c->mbnode, 0);
    }
    c->anim = NULL;
    while ((node = c->subnodes) != NULL) {
        if (node->atree != NULL) {
            AtreeDelete(node);
        }
        if (node->mbnode != NULL) {
            MBRemoveNode(node->mbnode, 1);
        }
        node->mbnode = NULL;
        c->subnodes = *(void **)((u8 *)c->subnodes + 80);
    }
    c->hdr = NULL;
    c->state = 0;
}
/* 0x8003EB8C -- step skin effects and service the per-node flash/fade
 * counters without leaving temporary texture flags on the attachment node. */
void CritterUpdateSkinfx(Critter *c)
{
    s32 offset;
    s32 i;
    u8 *node;
    u32 savedFlags;

    savedFlags = 0;
    ProcessSkinFX((f32 *)((u8 *)c + 0xE0), c->anim, c->hitnode2);
    if (c->hitnode2 != NULL) {
        u32 *flags = (u32 *)c->hitnode2;
        savedFlags = *(flags += 0x18);
        *flags = savedFlags | 0x10;
    }

    for (i = 0, offset = 0; i < *(s16 *)((u8 *)c->hdr + 0x118);
        i++, offset += 0x5C) {
        u8 *base = (u8 *)c + offset;
        s32 counter = *(s32 *)(base + 0x548);
        node = base + 0x4F8;
        if (counter > 0) {
            MBTreeSetAltTex(*(void **)(node + 8), 0xFFFFFFFC,
                            lbl_80344BF8, 1);
            MBTreeSetAmbientAdd(*(void **)(node + 8), 0xFF, 1);
            (*(s32 *)(node + 0x50))--;
        } else if (*(s32 *)(node + 0x50) == 0) {
            MBTreeSetAltTex(*(void **)(node + 8), 0xFFFFFFFF, 0, 1);
            MBTreeSetAmbientAdd(*(void **)(node + 8), 0, 1);
            *(s32 *)(node + 0x50) = -1;
        }
    }

    if (c->unkABC > 0) {
        MBTreeSetAltTex(c->anim, 0xFFFFFFFC, lbl_80344BF8, 1);
        MBTreeSetAmbientAdd(c->anim, 0xFF, 1);
        c->unkABC--;
    } else if (c->unkABC == 0) {
        MBTreeSetAltTex(c->anim, 0xFFFFFFFF, 0, 1);
        MBTreeSetAmbientAdd(c->anim, 0, 1);
        c->unkABC = -1;
    }
    if (c->pausecnt > 0) {
        c->pausecnt -= gFrameTicks;
        if (c->pausecnt <= 0) {
            MBTreeSetAltTex(c->anim, 0xFFFFFFFF, 0, 1);
            c->pausecnt = 0;
        } else if (c->pausecnt < 180 && (c->pausecnt & 8) != 0) {
            MBTreeSetAltTex(c->anim, 0xFFFFFFFF, 0, 1);
        } else {
            MBTreeSetAltTex(c->anim, 0xFFFFFFFC, (u32)c->unkAC0, 1);
        }
    }
    if (c->unkABE > 0) {
        s32 ambient = c->unkABE;
        c->unkABE -= 16;
        if (c->unkABE < 0) {
            c->unkABE = 0;
            ambient = 0;
        }
        MBTreeSetAmbientAdd(c->anim, ambient, 1);
    }
    if (c->hitnode2 != NULL) {
        *(u32 *)((u8 *)c->hitnode2 + 0x60) = savedFlags;
    }
}
typedef struct CritterColnode {
    u8 _pad00[0x78];
    struct CritterColnode *child;
    struct CritterColnode *next;
} CritterColnode;

typedef struct CritterAnimNode {
    CritterColnode *node;
    u8 _pad04[0x1C];
    void *attachment;
    u8 _pad24[4];
} CritterAnimNode;

/* 0x8003EDC4 -- recursively remove a collision-node chain and clear every
 * animation/move/hit-node reference that pointed at the removed nodes. */
void CritterRemoveColnodeSub(Critter *c, CritterColnode *node, s32 mode)
{
    CritterColnode *next;
    s32 animOffset;
    s32 moveOffset;
    s32 hitOffset;
    s32 i;
    s32 j;

    while (node != NULL) {
        if (node->child != NULL) {
            CritterRemoveColnodeSub(c, node->child, 2);
        }
        next = node->next;
        MBRemoveNode(node, 0);

        for (i = 0, animOffset = 0; i < c->anodeCount;
             i++, animOffset += sizeof(CritterAnimNode)) {
            if (*(CritterColnode **)((u8 *)c->anodes + animOffset) == node) {
                j = 0;
                *(void **)((u8 *)c->anodes + animOffset + 0x20) = NULL;
                *(CritterColnode **)((u8 *)c->anodes + animOffset) = NULL;
                moveOffset = j;
                while (j < *(s16 *)((u8 *)c->hdr + 0x110)) {
                    if (*(s16 *)(*(u8 **)((u8 *)c->hdr + 0x124) +
                                 moveOffset + 0x0E) == i) {
                        *(s16 *)(*(u8 **)((u8 *)c->hdr + 0x124) +
                                 moveOffset + 0x0E) = -1;
                    }
                    j++;
                    moveOffset += sizeof(CritterMove);
                }
            }
        }

        for (i = 0, hitOffset = 0;
             i < *(s16 *)((u8 *)c->hdr + 0x118);
             i++, hitOffset += 0x5C) {
            u8 *hitRecord = (u8 *)c + hitOffset;
            if (*(void **)(hitRecord + 0x4FC) == node) {
                *(void **)(hitRecord + 0x4FC) = NULL;
            }
        }

        if (mode != 2) {
            break;
        }
        node = next;
    }
}
static inline void *CritterColnodeAnimNode(Critter *c, s32 index)
{
    void *node = c->anim;
    void *candidate;

    if (index < 0) {
        return node;
    }
    candidate = *(void **)((u8 *)c->anodes + index * 0x28);
    if (candidate == NULL) {
        candidate = node;
    }
    return candidate;
}
/* 0x8003EEF8 -- bind each collision descriptor to its animation node and
 * initialize its health, flash and optional damage-effect state. */
void CritterInitColnodes(Critter *c)
{
    u8 *header;
    s32 i;
    u8 *descriptorBase;
    u8 *descriptor;
    u8 *record;
    char *name;
    s8 ch;
    s32 nodeIndex;
    f32 zerof;
    u8 unused[8];

    header = (u8 *)c->hdr;
    if (*(s16 *)(header + 0x118) <= 0) {
        return;
    }
    descriptorBase = *(u8 **)(*(u8 **)(header + 0x130) + 0x3C) +
                     *(s16 *)(header + 0x11A) * 0x50;
    c->unkAB8 = -1;
    zerof = lbl_80346470;
    for (i = 0; i < *(s16 *)((u8 *)c->hdr + 0x118); i++) {
        record = (u8 *)c + 0x4F8 + i * 0x5C;
        *(u8 **)record = descriptorBase + i * 0x50;
        nodeIndex = *(s16 *)(*(u8 **)record + 0x14);
        if ((s16)nodeIndex >= 0) {
            *(void **)(record + 4) = CritterColnodeAnimNode(c, nodeIndex);
        } else {
            *(void **)(record + 4) = NULL;
        }
        *(void **)(record + 8) = *(void **)(record + 4);
        if (*(void **)(record + 4) != NULL) {
            descriptor = *(u8 **)record;
            ch = *(s8 *)(name = (char *)descriptor + 0x30);
            if (ch != 0) {
                if (ch == '-') {
                    s32 n = *(s8 *)(*(u8 **)record + 0x31) - '0';
                    while (n > 0) {
                        *(void **)(record + 8) =
                            *(void **)(*(u8 **)(record + 8) + 0x74);
                        n--;
                    }
                } else if (ch == '+') {
                    s32 n = *(s8 *)(*(u8 **)record + 0x31) - '0';
                    while (n > 0) {
                        *(void **)(record + 8) =
                            *(void **)(*(u8 **)(record + 8) + 0x78);
                        n--;
                    }
                } else {
                    s32 idx = -1;
                    void *atc = *(void **)((u8 *)c->hdr + 0x138);
                    if (atc != NULL && name != NULL && ch != 0 &&
                        name[1] != 0) {
                        idx = AtreeFindNodeIdx(*(void **)((u8 *)atc + 0xC),
                                               *(s32 *)((u8 *)atc + 0x10),
                                               name, 0x10);
                    }
                    *(void **)(record + 8) = CritterColnodeAnimNode(c, idx);
                }
            }
        }
        MBTreeSetZsortAdd(*(void **)(record + 4),
                          *(s16 *)(*(u8 **)record + 0x16), 1);
        *(s32 *)(record + 0x50) = -1;
        *(f32 *)(record + 0x58) = zerof;
        *(f32 *)(record + 0x54) =
            *(f32 *)(*(u8 **)record + 0x44) * c->health;
        {
            u8 *psys;
            s32 sfxidx = *(s16 *)(*(u8 **)record + 0x12);
            void *hdr130 = *(void **)((u8 *)c->hdr + 0x130);
            void *sfxparam =
                *(void **)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x28);
            if (sfxidx >= 0) {
                psys = *(u8 **)((u8 *)hdr130 + 0x44) + sfxidx * 0x50;
                CritterInitSfx(hdr130, *(s16 *)(psys + 0x40), sfxparam);
                CritterInitSfx(hdr130, *(s16 *)(psys + 0x44), sfxparam);
                CritterInitSfx(hdr130, *(s16 *)(psys + 0x46), sfxparam);
                CritterInitSfx(hdr130, *(s16 *)(psys + 0x42), sfxparam);
                if (*(s16 *)psys == 6) {
                    lbl_80344650 = 1;
                }
            }
        }
        if ((gControllerButtons & 0x10) && gGameOptions[8]) {
            *(void **)(record + 0x4C) = DmgFxCircleAdd(
                *(void **)(record + 4), *(f32 *)(*(u8 **)record + 0x2C),
                lbl_80346470, lbl_80346470,
                (f32 *)(*(u8 **)record + 0x20), 127);
        }
    }
}

/* 0x8003F1F0 -- instantiate every auxiliary animation tree attached to a
 * critter type and link the allocated records into the instance. */
static CritterSubnode *CritterNewAnimInst(void)
{
    s32 i;
    s32 total = lbl_80344668;

    for (i = 0; i < total; i++) {
        if (((CritterSubnode *)(lbl_802411B0 + i * 0x54))->mbnode == NULL) {
            break;
        }
    }
    if (i >= 1) {
        ErrorPrintf("Too many Critter Anim Insts: %d", i);
        return NULL;
    }
    if (i == total) {
        lbl_80344668 = lbl_80344668 + 1;
    }
    return (CritterSubnode *)(lbl_802411B0 + i * 0x54);
}

void CritterAddAnimInsts(Critter *c, f32 *matrix)
{
    u8 *node;
    CritterSubnode *record;
    CritterSubnode *tail;
    void *parent;

    node = *(u8 **)((u8 *)c->hdr + 0x134);
    while (node != NULL) {
        record = CritterNewAnimInst();
        if (record != NULL) {
            if (c->subnodes != NULL) {
                tail = (CritterSubnode *)c->subnodes;
                while (tail->next != NULL) {
                    tail = tail->next;
                }
                tail->next = record;
            } else {
                c->subnodes = record;
            }
            if (*(void **)(node + 4) != NULL) {
                parent = lbl_8034473C;
                if ((*(s16 *)(node + 2) & 1) != 0) {
                    if (*(s8 *)(node + 0x18) != 0) {
                        parent = AtreeFindNode(&c->colhandle,
                                               (char *)(node + 0x18), 8);
                        if (parent == NULL) {
                            parent = c->anim;
                        }
                    } else {
                        parent = c->anim;
                    }
                }
                record->mbnode = MBNewNode(parent, matrix, 1);
                *(f32 *)((u8 *)record->mbnode + 0x30) = *(f32 *)(node + 0x20);
                *(f32 *)((u8 *)record->mbnode + 0x34) = *(f32 *)(node + 0x24);
                *(f32 *)((u8 *)record->mbnode + 0x38) = *(f32 *)(node + 0x28);
                record->atree =
                    AtreeInit(*(void **)(node + 4), record, 0, 0x800);
                MBNodeSetParent(*(void **)record->atree, record->mbnode);
            } else {
                ErrorPrintf("Bad critter anim inst: %s", (char *)(node + 0x10));
            }
        }
        node = *(u8 **)(node + 8);
    }
}

/* 0x8003F3AC -- allocate a load slot, read the file, and build its header. */
s32 CritterLoadFile(const char *wad, const char *name)
{
    s32 idx;
    idx = lbl_80344660++;
    lbl_80241060[idx] = AllocFile(wad, name);
    CritterInitHeader(&lbl_80241070[idx], lbl_80241060[idx]);
    return idx;
}

/* 0x8003F414 -- poll the active background model request and advance it to
 * the texture/model-finalization stage. */
s32 CritterLoadDone(s32 maxBytes)
{
    char buf[36];
    u8 *desc;
    char *fmtbase;
    s32 result;
    s32 *handle;
    s32 size;
    s32 i;

    result = 0;
    fmtbase = lbl_801120E0;
    desc = (u8 *)crit_load_desc;
    if (*(s16 *)(desc + 0x24) == 1) {
        if (MBOX_BGLoadModelDone() != 0) {
            *(s16 *)(desc + 0x24) = 2;
            switch (*(s16 *)(desc + 0x20)) {
            case 3:
            case 8:
                sprintf(buf, &fmtbase[416], desc, (u8 *)gWorldData + 4);
                break;
            case 7:
                for (i = 0; i < 32; i += 4) {
                    s32 *entry = *(s32 **)((u8 *)lbl_8025776C + i);
                    if (*entry == 32) {
                        sprintf(buf, &fmtbase[432], desc, (u8 *)entry + 16);
                        break;
                    }
                }
                break;
            default:
                sprintf(buf, &fmtbase[448], desc);
                break;
            }
            size = FileSize(buf, lbl_8034664C);
            if (maxBytes != 0 && size > maxBytes) {
                size = maxBytes;
            }
            lbl_80344640 = StartFileRead(buf, lbl_8034664C, 0, size,
                                         *(s32 *)(desc + 0x28),
                                         (void *)CritterBGLoadFile);
        }
    } else {
        handle = lbl_80344640;
        if (handle != NULL) {
            if (*(handle += 4) != 0) {
                *handle = -1;
                *(s16 *)(desc + 0x24) = 3;
                fn_8001267C(*(s32 *)(desc + 0x28), *(s16 *)(desc + 0x22), -1);
                InitTexMods(*(s32 *)(desc + 0x28), *(s16 *)(desc + 0x22));
                result = 1;
            }
        } else {
            result = 1;
        }
    }
    return result;
}

/* 0x8003F5B4 -- advance a background loader unless it has finished (state 2). */
void CritterBGLoadFile(s32 *loader)
{
    if (loader[4] == 2) {
        return;
    }
    loader[1] += loader[2];
}

/* 0x8003F5D4 -- find the next unloaded critter resource and start its model
 * request.  Returns through the module globals consumed by CritterLoadDone. */
s32 CritterLoadStartNext(void)
{
    char buf[32];
    u8 *fmtbase;
    u8 *tableBase;
    s32 i;
    s32 j;
    u8 *entry;
    u8 *sub;
    u8 *desc;
    s32 offset;

    fmtbase = (u8 *)lbl_801120E0;
    tableBase = (u8 *)lbl_80241070;
    for (i = 0; i < lbl_80344660; i++) {
        entry = tableBase + i * 80;
        if (*(s32 *)entry != 1) {
            continue;
        }
        for (j = 0; j < *(s32 *)(entry + 0x10); j++) {
            sub = *(u8 **)(entry + 0x14) + j * 320;
            desc = *(u8 **)(sub + 0x120);
            if (desc == NULL) {
                continue;
            }
            switch (*(s16 *)(desc + 0x24)) {
            case 0:
                break;
            case 1:
                switch (*(s16 *)(desc + 0x20)) {
                case 3:
                case 8:
                    sprintf(buf, (char *)&fmtbase[416], desc,
                            (u8 *)gWorldData + 4);
                    break;
                case 7:
                    for (offset = 0; offset < 32; offset += 4) {
                        s32 *e2 =
                            *(s32 **)((u8 *)lbl_8025776C + offset);
                        if (*e2 == 32) {
                            sprintf(buf, (char *)&fmtbase[432], desc,
                                    (u8 *)e2 + 16);
                            break;
                        }
                    }
                    break;
                default:
                    sprintf(buf, (char *)&fmtbase[448], desc);
                    break;
                }
                MBOX_BGLoadModelStart(buf, *(s16 *)(desc + 0x22));
                crit_load_desc = desc;
                lbl_80344640 = NULL;
                return 1;
            case 2:
                return 1;
            case 3:
                CritterLoadFinish(sub);
                break;
            case 4:
                break;
            default:
                break;
            }
        }
        if (j == *(s32 *)(entry + 0x10)) {
            *(s32 *)entry = 2;
        }
    }
    return 0;
}

/* 0x8003F784 -- for every loaded type/subtype header, register each of its
 * moves via CritterAllocType. */
void CritterLoadAllTypes(s32 arg)
{
    s32 type;
    s32 sub;
    u8 *hdr;

    for (type = 0; type < lbl_80344660; type++) {
        hdr = lbl_80241070[type];
        if (*(s32 *)hdr != 0) {
            for (sub = 0; sub < *(s32 *)(hdr + 16); sub++) {
                CritterAllocType(hdr, *(u8 **)(hdr + 20) + sub * 320, arg);
            }
        }
    }
}

/* 0x8003F81C */
struct CritterHeader *CritterTypeLoaded(s32 type, s32 subtype)
{
    return gCritterHeaders[type][subtype];
}

/* 0x8003F83C -- bind one type header to its container resource and publish
 * it in the type/subtype lookup table. */
void CritterAllocType(void *hdr, void *move, s32 arg)
{
    char buf[32];
    u8 *desc;
    u8 *fmtbase;
    s32 k;
#define m ((u8 *)move)

    *(void **)(m + 0x130) = hdr;
    fmtbase = (u8 *)lbl_801120E0;
    desc = *(u8 **)((u8 *)hdr + 0x1C) + *(s16 *)(m + 0x50) * 48;
    *(u8 **)(m + 0x120) = desc;
    if (*(s16 *)(desc + 0x22) < 0) {
        switch (*(s16 *)(desc + 0x20)) {
        case 3:
        case 8:
            sprintf(buf, (char *)&fmtbase[416], desc, (u8 *)gWorldData + 4);
            break;
        case 7:
            for (k = 0; k < 8; k++) {
                s32 *e2 = lbl_8025776C[k];
                if (*e2 == 32) {
                    sprintf(buf, (char *)&fmtbase[432], desc, (u8 *)e2 + 16);
                    break;
                }
            }
            break;
        default:
            sprintf(buf, (char *)&fmtbase[448], desc);
            break;
        }
        if (arg != 0) {
            *(s16 *)(desc + 0x22) = LoadModel(buf, (u8 *)desc + 0x28, 0, -1);
            *(s16 *)(desc + 0x24) = 2;
            if (*(u8 **)(desc + 0x28) != NULL) {
                InitTexMods(*(s32 *)(desc + 0x28), *(s16 *)(desc + 0x22));
                *(s16 *)(desc + 0x24) = 3;
            }
        } else {
            *(s16 *)(desc + 0x22) = fn_8005A1EC(buf, (u8 *)desc + 0x28);
            *(s16 *)(desc + 0x24) = 1;
        }
        *(s16 *)(desc + 0x26) = lbl_80344664;
    }
    if (arg != 0) {
        CritterLoadFinish(m);
    } else {
        *(s32 *)(m + 0x138) = 0;
    }
    if (*(s16 *)(m + 0x52) >= 0) {
        gCritterHeaders[*(s16 *)(desc + 0x20)][*(s16 *)(m + 0x52)] =
            (struct CritterHeader *)m;
    }
#undef m
}

/* 0x8003F9F4 -- resolve the animation tree and named attachment nodes for a
 * type after its model resource has loaded. */
void CritterLoadFinish(u8 *header)
{
    u8 *parent;
    u8 *attachment;
    void *atree;
    s32 index;
    f64 name[4];
    u8 unused[8];

    if (*(void **)(header + 0x138) != NULL) {
        return;
    }
    if (*(s16 *)(header + 0x11E) < 0) {
        sprintf((char *)name, "%s%s",
                (char *)(*(u8 **)(header + 0x120) + 0x10), header);
        *(void **)(header + 0x138) =
            AtreeMatch(*(void **)(*(u8 **)(header + 0x120) + 0x28),
                       (char *)name, 0);
        if (*(void **)(header + 0x138) == NULL) {
            ErrorPrintf("Critter can not find atree %s", (char *)name);
        }
    } else {
        parent = *(u8 **)(*(u8 **)(header + 0x130) + 0x14) +
                 *(s16 *)(header + 0x11E) * 0x140;
        *(void **)(header + 0x138) = *(void **)(parent + 0x138);
        if (*(void **)(parent + 0x138) == NULL) {
            FatalError("Child critter defined before parent", 0x800000);
        }
    }

    for (attachment = *(u8 **)(header + 0x134);
         attachment != NULL;
         attachment = *(u8 **)(attachment + 8)) {
        *(void **)(attachment + 4) =
            AtreeMatch(*(void **)(*(u8 **)(header + 0x120) + 0x28),
                       (char *)(attachment + 0x10), 1);
    }

    atree = *(void **)(header + 0x138);
    index = -1;
    if (atree != NULL && (header + 0x20) != NULL &&
        *(char *)(header + 0x20) != '\0' &&
        *(char *)(header + 0x21) != '\0') {
        index = AtreeFindNodeIdx(*(void **)((u8 *)atree + 0x0C),
                                 *(s32 *)((u8 *)atree + 0x10),
                                 (char *)(header + 0x20), 0x10);
    }
    *(s16 *)(header + 0x56) = (s16)index;
    index = -1;
    atree = *(void **)(header + 0x138);
    if (atree != NULL && (header + 0x30) != NULL &&
        *(char *)(header + 0x30) != '\0' &&
        *(char *)(header + 0x31) != '\0') {
        index = AtreeFindNodeIdx(*(void **)((u8 *)atree + 0x0C),
                                 *(s32 *)((u8 *)atree + 0x10),
                                 (char *)(header + 0x30), 0x10);
    }
    *(s16 *)(header + 0x58) = (s16)index;
    index = -1;
    atree = *(void **)(header + 0x138);
    if (atree != NULL && (header + 0x40) != NULL &&
        *(char *)(header + 0x40) != '\0' &&
        *(char *)(header + 0x41) != '\0') {
        index = AtreeFindNodeIdx(*(void **)((u8 *)atree + 0x0C),
                                 *(s32 *)((u8 *)atree + 0x10),
                                 (char *)(header + 0x40), 0x10);
    }
    *(s16 *)(header + 0x5A) = (s16)index;
}

/* 0x8003FBD0 -- initialize the move tables of every loaded type/subtype. */
void CritterInitAllMoves(void)
{
    s32 type;
    s32 sub;
    u8 *hdr;

    for (type = 0; type < lbl_80344660; type++) {
        hdr = lbl_80241070[type];
        for (sub = 0; sub < *(s32 *)(hdr + 16); sub++) {
            CritterInitMoves(*(u8 **)(hdr + 20) + sub * 320);
        }
    }
}

/* 0x8003FC4C -- resolve animation/node names and every sound/particle
 * dependency referenced by a loaded type's move and collision tables. */
void CritterInitMoves(u8 *header)
{
    volatile u8 unused[8];
    u8 *container;
    void *atree;
    u8 *moves;
    u8 *colnodes;
    u8 *entry;
    s32 i;
    s32 index;

    atree = *(void **)(header + 0x138);
    container = *(u8 **)(header + 0x130);
    moves = *(u8 **)(container + 0x2C) +
            *(s16 *)(header + 0x112) * 0x90;
    colnodes = *(u8 **)(container + 0x3C) +
               *(s16 *)(header + 0x11A) * 0x50;
    if (atree == NULL) {
        return;
    }
    if (*(s16 *)(header + 0x110) > lbl_80344630) {
        lbl_80344630 = *(s16 *)(header + 0x110);
    }
    if (*(s16 *)(header + 0x114) > lbl_80344634) {
        lbl_80344634 = *(s16 *)(header + 0x114);
    }
    if (*(s16 *)(header + 0x118) > lbl_80344638) {
        lbl_80344638 = *(s16 *)(header + 0x118);
    }

    i = 0;
    while (i < *(s16 *)(header + 0x110)) {
        entry = moves + i * 0x90;
        if (*(s32 *)entry >= 0x30 && *(s32 *)entry <= 0x39) {
            *(u32 *)(header + 0x5C) |= 0x10000;
        }
        if (*(s16 *)(entry + 0x0C) >= 0) {
            goto next_move;
        }
        if (*(char *)(entry + 0x20) != '\0') {
            index = AtreeHeaderFindSeq(atree, (char *)(entry + 0x20));
            *(s16 *)(entry + 0x0C) = (s16)index;
            if (*(s16 *)(entry + 0x0C) < 0) {
                ErrorPrintf(lbl_801122F0,
                            (char *)(*(u8 **)(header + 0x120) + 0x10),
                            (char *)(entry + 0x20));
                *(s16 *)(entry + 0x0C) = 0;
            }
        }
        index = -1;
        {
            void *lookupAtree = *(void **)(header + 0x138);
        if (lookupAtree != NULL && (entry + 0x30) != NULL &&
            *(char *)(entry + 0x30) != '\0' &&
            *(char *)(entry + 0x31) != '\0') {
            index = AtreeFindNodeIdx(*(void **)((u8 *)lookupAtree + 0x0C),
                                     *(s32 *)((u8 *)lookupAtree + 0x10),
                                     (char *)(entry + 0x30), 0x10);
        }
        }
        *(s16 *)(entry + 0x0E) = (s16)index;

        index = *(s16 *)(entry + 0x48);
        {
            u8 *sfx;
            void *sfxHeader;
            sfxHeader = *(void **)(*(u8 **)(header + 0x120) + 0x28);
        if (index >= 0) {
            sfx = *(u8 **)(container + 0x44) + index * 0x50;
            CritterInitSfx(container, *(s16 *)(sfx + 0x40), sfxHeader);
            CritterInitSfx(container, *(s16 *)(sfx + 0x44), sfxHeader);
            CritterInitSfx(container, *(s16 *)(sfx + 0x46), sfxHeader);
            CritterInitSfx(container, *(s16 *)(sfx + 0x42), sfxHeader);
            if (*(s16 *)sfx == 6) {
                lbl_80344650 = 1;
            }
        }
        }
        {
            u8 *sfx;
            void *sfxHeader;
            index = *(s16 *)(entry + 0x4A);
            sfxHeader = *(void **)(*(u8 **)(header + 0x120) + 0x28);
        if (index >= 0) {
            sfx = *(u8 **)(container + 0x44) + index * 0x50;
            CritterInitSfx(container, *(s16 *)(sfx + 0x40), sfxHeader);
            CritterInitSfx(container, *(s16 *)(sfx + 0x44), sfxHeader);
            CritterInitSfx(container, *(s16 *)(sfx + 0x46), sfxHeader);
            CritterInitSfx(container, *(s16 *)(sfx + 0x42), sfxHeader);
            if (*(s16 *)sfx == 6) {
                lbl_80344650 = 1;
            }
        }
        }
        CritterInitSfx(container, *(s16 *)(entry + 0x58),
                       *(void **)(*(u8 **)(header + 0x120) + 0x28));
        CritterInitSfx(container, *(s16 *)(entry + 0x5C),
                       *(void **)(*(u8 **)(header + 0x120) + 0x28));
next_move:
        i++;
    }

    {
    s32 colIndex = 0;
    while (colIndex < *(s16 *)(header + 0x118)) {
        u8 *colnode;
        void *lookupAtree;
        colnode = colnodes + colIndex * 0x50;
        index = -1;
        lookupAtree = *(void **)(header + 0x138);
        if (lookupAtree != NULL && colnode != NULL &&
            *(char *)colnode != '\0' && *(char *)(colnode + 1) != '\0') {
            index = AtreeFindNodeIdx(*(void **)((u8 *)lookupAtree + 0x0C),
                                     *(s32 *)((u8 *)lookupAtree + 0x10),
                                     (char *)colnode, 0x10);
        }
        *(s16 *)(colnode + 0x14) = (s16)index;
        colIndex++;
    }
    }

    CritterInitSfx(container, *(s16 *)(header + 0xF4),
                   *(void **)(*(u8 **)(header + 0x120) + 0x28));
    CritterInitSfx(container, *(s16 *)(header + 0xF6),
                   *(void **)(*(u8 **)(header + 0x120) + 0x28));
    *(u8 **)(header + 0x124) = moves;
    *(u8 **)(header + 0x12C) = colnodes;
    *(u8 **)(header + 0x128) =
        *(u8 **)(container + 0x34) + *(s16 *)(header + 0x116) * 0x50;
}

/* 0x8003FF98 -- lazily resolve one sound/particle descriptor and recursively
 * initialize its linked descriptor. */
void CritterInitSfx(void *file, s32 index, void *atreeHeader)
{
    u8 *entry;
    s32 model;
    char name[32];

    if (index < 0) {
        return;
    }
    entry = *(u8 **)((u8 *)file + 0x4C) + index * 0x50;
    if (*(s32 *)(entry + 8) < 0) {
        if ((*(u32 *)entry & 0x0F000100) != 0) {
            if (*(char *)(entry + 0x10) != '\0') {
                *(s32 *)(entry + 8) =
                    FindTexMod(atreeHeader, (char *)(entry + 0x10), NULL);
                if (*(s32 *)(entry + 8) <= 0) {
                    model = AtreeModel(atreeHeader);
                    *(s32 *)(entry + 8) = MBOX_FindTexture_Sub(
                        (char *)(entry + 0x10), NULL, model, model, -1);
                }
                if (*(s32 *)(entry + 8) <= 0) {
                    *(s32 *)(entry + 8) =
                        MBOX_FindTexture((char *)(entry + 0x10), NULL);
                }
            } else {
                *(s32 *)(entry + 8) = -1;
            }
        } else {
            *(s32 *)(entry + 8) = InitCustomEffect(
                atreeHeader, (char *)(entry + 0x10),
                *(s16 *)(entry + 0x44), *(s16 *)(entry + 0x46));
        }
    }
    if (*(s32 *)(entry + 0x0C) < 0) {
        if (*(char *)(entry + 0x20) != '\0') {
            sprintf(name, (char *)(entry + 0x20),
                    *(s8 *)((u8 *)gCurLevel + 8));
            *(s32 *)(entry + 0x0C) = AudioFindSound(name, 0, 1);
        } else {
            *(s32 *)(entry + 0x0C) = -1;
        }
    }
    CritterInitSfx(file, *(s32 *)(entry + 4), atreeHeader);
}
typedef struct CritterFileHeader {
    s32 state;
    s32 wad[3];
    s32 typeCount;
    u8 *types;
    s32 descriptorCount;
    u8 *descriptors;
    s32 addAnimCount;
    u8 *addAnims;
    s32 moveCount;
    u8 *moves;
    s32 patternCount;
    u8 *patterns;
    s32 nodeCount;
    u8 *nodes;
    s32 damageCount;
    u8 *damage;
    s32 sfxCount;
    u8 *sfx;
} CritterFileHeader;

extern char lbl_8034665C[8]; /* "SFXX" */
extern char lbl_80346664[8]; /* "DAMG" */
extern char lbl_8034666C[8]; /* "MOVE" */
extern char lbl_80346674[8]; /* "PTRN" */
extern char lbl_8034667C[8]; /* "NODE" */
extern char lbl_80346684[8]; /* "DESC" */
extern char lbl_8034668C[8]; /* "TYPE" */
extern char lbl_80346694[8]; /* "ADDA" */
static inline s32 CritterWadTag(char *s)
{
    return (s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3];
}

#define CRITTER_SFX_TAG(s) \
    (((s)[0] << 24) | ((s)[1] << 16) | ((s)[2] << 8) | (s)[3])

static inline u16 CritterSwap16(u16 v)
{
    u8 *p = (u8 *)&v;
    return (u16)(p[0] | (p[1] << 8));
}

static inline u32 CritterSwap32(u32 v)
{
    u32 r;
    u8 *s = (u8 *)&v;
    u8 *d = (u8 *)&r;
    d[0] = s[3];
    d[1] = s[2];
    d[2] = s[1];
    d[3] = s[0];
    return r;
}

static inline f32 CritterSwapF(f32 v)
{
    u32 r = CritterSwap32(*(u32 *)&v);
    return *(f32 *)&r;
}

/* 0x800400F0 -- open all CRITTER WAD sections, convert little-endian
 * serialized records in place, reset runtime-only fields, and build each
 * type's linked auxiliary-animation list. */
void CritterInitHeader(void *hdr, void *file)
{
    CritterFileHeader *header;
    s32 *wad;
    s32 swapped;
    s32 i;
    u8 *p;
    s32 j;
    u8 *type;
    u8 *tail;
    s32 typeIndex;

    header = (CritterFileHeader *)hdr;
    swapped = 0;
    if (header->state == 0) {
        wad = header->wad;
        swapped = MBSetupWad(wad, (s32)file);
        header->sfx = (u8 *)MBGetFromWad(wad,
                                         CRITTER_SFX_TAG(lbl_8034665C),
                                         &header->sfxCount);
        header->damage = (u8 *)MBGetFromWad(wad,
                                            CritterWadTag(lbl_80346664),
                                            &header->damageCount);
        header->moves = (u8 *)MBGetFromWad(wad,
                                           CritterWadTag(lbl_8034666C),
                                           &header->moveCount);
        header->patterns = (u8 *)MBGetFromWad(wad,
                                              CritterWadTag(lbl_80346674),
                                              &header->patternCount);
        header->nodes = (u8 *)MBGetFromWad(wad,
                                           CritterWadTag(lbl_8034667C),
                                           &header->nodeCount);
        header->descriptors = (u8 *)MBGetFromWad(wad,
                                                 CritterWadTag(lbl_80346684),
                                                 &header->descriptorCount);
        header->types = (u8 *)MBGetFromWad(wad,
                                           CritterWadTag(lbl_8034668C),
                                           &header->typeCount);
        header->addAnims = (u8 *)MBGetFromWad(wad,
                                              CritterWadTag(lbl_80346694),
                                              &header->addAnimCount);
        if (header->types == NULL) {
            FatalError("Critter Header has no types", 0x800000);
        }
        header->state = 1;
    }

    if ((u8)swapped) {
        for (i = 0; i < header->sfxCount; i++) {
            p = header->sfx + i * 0x50;
            *(u16 *)(p + 0x44) = CritterSwap16(*(u16 *)(p + 0x44));
            *(u16 *)(p + 0x46) = CritterSwap16(*(u16 *)(p + 0x46));
            *(u32 *)(p + 0x00) = CritterSwap32(*(u32 *)(p + 0x00));
            *(u32 *)(p + 0x04) = CritterSwap32(*(u32 *)(p + 0x04));
            *(u32 *)(p + 0x08) = CritterSwap32(*(u32 *)(p + 0x08));
            *(u32 *)(p + 0x0C) = CritterSwap32(*(u32 *)(p + 0x0C));
            *(f32 *)(p + 0x3C) = CritterSwapF(*(f32 *)(p + 0x3C));
            *(f32 *)(p + 0x40) = CritterSwapF(*(f32 *)(p + 0x40));
            *(u32 *)(p + 0x48) = CritterSwap32(*(u32 *)(p + 0x48));
            *(f32 *)(p + 0x4C) = CritterSwapF(*(f32 *)(p + 0x4C));
            for (j = 0; j < 3; j++) {
                *(f32 *)(p + 0x30 + j * 4) =
                    CritterSwapF(*(f32 *)(p + 0x30 + j * 4));
            }
        }

        for (i = 0; i < header->damageCount; i++) {
            p = header->damage + i * 0x50;
            *(u16 *)(p + 0x00) = CritterSwap16(*(u16 *)(p + 0x00));
            *(u16 *)(p + 0x02) = CritterSwap16(*(u16 *)(p + 0x02));
            *(u16 *)(p + 0x40) = CritterSwap16(*(u16 *)(p + 0x40));
            *(u16 *)(p + 0x42) = CritterSwap16(*(u16 *)(p + 0x42));
            *(u16 *)(p + 0x44) = CritterSwap16(*(u16 *)(p + 0x44));
            *(u16 *)(p + 0x46) = CritterSwap16(*(u16 *)(p + 0x46));
            *(f32 *)(p + 0x08) = CritterSwapF(*(f32 *)(p + 0x08));
            *(f32 *)(p + 0x0C) = CritterSwapF(*(f32 *)(p + 0x0C));
            *(f32 *)(p + 0x10) = CritterSwapF(*(f32 *)(p + 0x10));
            *(f32 *)(p + 0x14) = CritterSwapF(*(f32 *)(p + 0x14));
            *(f32 *)(p + 0x18) = CritterSwapF(*(f32 *)(p + 0x18));
            *(f32 *)(p + 0x1C) = CritterSwapF(*(f32 *)(p + 0x1C));
            *(f32 *)(p + 0x2C) = CritterSwapF(*(f32 *)(p + 0x2C));
            *(f32 *)(p + 0x30) = CritterSwapF(*(f32 *)(p + 0x30));
            *(f32 *)(p + 0x34) = CritterSwapF(*(f32 *)(p + 0x34));
            *(f32 *)(p + 0x38) = CritterSwapF(*(f32 *)(p + 0x38));
            *(f32 *)(p + 0x3C) = CritterSwapF(*(f32 *)(p + 0x3C));
            *(f32 *)(p + 0x48) = CritterSwapF(*(f32 *)(p + 0x48));
            *(u32 *)(p + 0x04) = CritterSwap32(*(u32 *)(p + 0x04));
            for (j = 0; j < 3; j++) {
                *(f32 *)(p + 0x20 + j * 4) =
                    CritterSwapF(*(f32 *)(p + 0x20 + j * 4));
            }
        }

        for (i = 0; i < header->moveCount; i++) {
            p = header->moves + i * 0x90;
            *(u16 *)(p + 0x0C) = CritterSwap16(*(u16 *)(p + 0x0C));
            *(u16 *)(p + 0x0E) = CritterSwap16(*(u16 *)(p + 0x0E));
            *(u16 *)(p + 0x48) = CritterSwap16(*(u16 *)(p + 0x48));
            *(u16 *)(p + 0x4A) = CritterSwap16(*(u16 *)(p + 0x4A));
            *(u16 *)(p + 0x50) = CritterSwap16(*(u16 *)(p + 0x50));
            *(u16 *)(p + 0x52) = CritterSwap16(*(u16 *)(p + 0x52));
            *(u16 *)(p + 0x54) = CritterSwap16(*(u16 *)(p + 0x54));
            *(u16 *)(p + 0x56) = CritterSwap16(*(u16 *)(p + 0x56));
            *(u16 *)(p + 0x58) = CritterSwap16(*(u16 *)(p + 0x58));
            *(u16 *)(p + 0x5A) = CritterSwap16(*(u16 *)(p + 0x5A));
            *(u16 *)(p + 0x5C) = CritterSwap16(*(u16 *)(p + 0x5C));
            *(u16 *)(p + 0x5E) = CritterSwap16(*(u16 *)(p + 0x5E));
            *(u32 *)(p + 0x04) = CritterSwap32(*(u32 *)(p + 0x04));
            *(u32 *)(p + 0x08) = CritterSwap32(*(u32 *)(p + 0x08));
            *(u32 *)(p + 0x40) = CritterSwap32(*(u32 *)(p + 0x40));
            *(u32 *)(p + 0x44) = CritterSwap32(*(u32 *)(p + 0x44));
            *(f32 *)(p + 0x4C) = CritterSwapF(*(f32 *)(p + 0x4C));
            *(f32 *)(p + 0x80) = CritterSwapF(*(f32 *)(p + 0x80));
            *(f32 *)(p + 0x84) = CritterSwapF(*(f32 *)(p + 0x84));
            *(f32 *)(p + 0x88) = CritterSwapF(*(f32 *)(p + 0x88));
            *(f32 *)(p + 0x8C) = CritterSwapF(*(f32 *)(p + 0x8C));
            *(u32 *)(p + 0x00) = CritterSwap32(*(u32 *)(p + 0x00));
            *(f32 *)(p + 0x60) = CritterSwapF(*(f32 *)(p + 0x60));
            *(f32 *)(p + 0x64) = CritterSwapF(*(f32 *)(p + 0x64));
            *(f32 *)(p + 0x68) = CritterSwapF(*(f32 *)(p + 0x68));
            *(f32 *)(p + 0x6C) = CritterSwapF(*(f32 *)(p + 0x6C));
            *(f32 *)(p + 0x70) = CritterSwapF(*(f32 *)(p + 0x70));
            *(f32 *)(p + 0x74) = CritterSwapF(*(f32 *)(p + 0x74));
            *(f32 *)(p + 0x78) = CritterSwapF(*(f32 *)(p + 0x78));
            *(f32 *)(p + 0x7C) = CritterSwapF(*(f32 *)(p + 0x7C));
        }

        for (i = 0; i < header->patternCount; i++) {
            p = header->patterns + i * 0x50;
            *(u16 *)(p + 0x10) = CritterSwap16(*(u16 *)(p + 0x10));
            *(u16 *)(p + 0x12) = CritterSwap16(*(u16 *)(p + 0x12));
            *(f32 *)(p + 0x14) = CritterSwapF(*(f32 *)(p + 0x14));
            *(f32 *)(p + 0x30) = CritterSwapF(*(f32 *)(p + 0x30));
            *(f32 *)(p + 0x34) = CritterSwapF(*(f32 *)(p + 0x34));
            *(f32 *)(p + 0x38) = CritterSwapF(*(f32 *)(p + 0x38));
            *(f32 *)(p + 0x3C) = CritterSwapF(*(f32 *)(p + 0x3C));
            *(f32 *)(p + 0x40) = CritterSwapF(*(f32 *)(p + 0x40));
            *(f32 *)(p + 0x44) = CritterSwapF(*(f32 *)(p + 0x44));
            *(f32 *)(p + 0x48) = CritterSwapF(*(f32 *)(p + 0x48));
            *(f32 *)(p + 0x4C) = CritterSwapF(*(f32 *)(p + 0x4C));
            for (j = 0; j < 8; j++) {
                *(u16 *)(p + 0x20 + j * 2) =
                    CritterSwap16(*(u16 *)(p + 0x20 + j * 2));
            }
        }

        for (i = 0; i < header->nodeCount; i++) {
            p = header->nodes + i * 0x50;
            *(u16 *)(p + 0x10) = CritterSwap16(*(u16 *)(p + 0x10));
            *(u16 *)(p + 0x12) = CritterSwap16(*(u16 *)(p + 0x12));
            *(u16 *)(p + 0x14) = CritterSwap16(*(u16 *)(p + 0x14));
            *(u16 *)(p + 0x16) = CritterSwap16(*(u16 *)(p + 0x16));
            *(f32 *)(p + 0x18) = CritterSwapF(*(f32 *)(p + 0x18));
            *(f32 *)(p + 0x1C) = CritterSwapF(*(f32 *)(p + 0x1C));
            *(f32 *)(p + 0x2C) = CritterSwapF(*(f32 *)(p + 0x2C));
            *(f32 *)(p + 0x40) = CritterSwapF(*(f32 *)(p + 0x40));
            *(f32 *)(p + 0x44) = CritterSwapF(*(f32 *)(p + 0x44));
            for (j = 0; j < 3; j++) {
                *(f32 *)(p + 0x20 + j * 4) =
                    CritterSwapF(*(f32 *)(p + 0x20 + j * 4));
            }
        }

        for (i = 0; i < header->descriptorCount; i++) {
            p = header->descriptors + i * 0x30;
            *(u16 *)(p + 0x20) = CritterSwap16(*(u16 *)(p + 0x20));
            *(u16 *)(p + 0x24) = CritterSwap16(*(u16 *)(p + 0x24));
            *(u16 *)(p + 0x26) = CritterSwap16(*(u16 *)(p + 0x26));
            *(u32 *)(p + 0x28) = CritterSwap32(*(u32 *)(p + 0x28));
        }

        for (i = 0; i < header->typeCount; i++) {
            p = header->types + i * 0x140;
            *(u16 *)(p + 0x50) = CritterSwap16(*(u16 *)(p + 0x50));
            *(u16 *)(p + 0x52) = CritterSwap16(*(u16 *)(p + 0x52));
            *(u16 *)(p + 0x56) = CritterSwap16(*(u16 *)(p + 0x56));
            *(u16 *)(p + 0x58) = CritterSwap16(*(u16 *)(p + 0x58));
            *(u16 *)(p + 0x5A) = CritterSwap16(*(u16 *)(p + 0x5A));
            *(u16 *)(p + 0xF4) = CritterSwap16(*(u16 *)(p + 0xF4));
            *(u16 *)(p + 0xF6) = CritterSwap16(*(u16 *)(p + 0xF6));
            *(u16 *)(p + 0xF8) = CritterSwap16(*(u16 *)(p + 0xF8));
            *(u16 *)(p + 0xFA) = CritterSwap16(*(u16 *)(p + 0xFA));
            *(u16 *)(p + 0xFC) = CritterSwap16(*(u16 *)(p + 0xFC));
            *(u16 *)(p + 0xFE) = CritterSwap16(*(u16 *)(p + 0xFE));
            *(u16 *)(p + 0x110) = CritterSwap16(*(u16 *)(p + 0x110));
            *(u16 *)(p + 0x112) = CritterSwap16(*(u16 *)(p + 0x112));
            *(u16 *)(p + 0x114) = CritterSwap16(*(u16 *)(p + 0x114));
            *(u16 *)(p + 0x116) = CritterSwap16(*(u16 *)(p + 0x116));
            *(u16 *)(p + 0x118) = CritterSwap16(*(u16 *)(p + 0x118));
            *(u16 *)(p + 0x11A) = CritterSwap16(*(u16 *)(p + 0x11A));
            *(u16 *)(p + 0x11C) = CritterSwap16(*(u16 *)(p + 0x11C));
            *(u16 *)(p + 0x11E) = CritterSwap16(*(u16 *)(p + 0x11E));
            *(u32 *)(p + 0x5C) = CritterSwap32(*(u32 *)(p + 0x5C));
            *(f32 *)(p + 0x60) = CritterSwapF(*(f32 *)(p + 0x60));
            *(f32 *)(p + 0x64) = CritterSwapF(*(f32 *)(p + 0x64));
            *(f32 *)(p + 0x68) = CritterSwapF(*(f32 *)(p + 0x68));
            *(f32 *)(p + 0x6C) = CritterSwapF(*(f32 *)(p + 0x6C));
            *(f32 *)(p + 0x70) = CritterSwapF(*(f32 *)(p + 0x70));
            *(f32 *)(p + 0x74) = CritterSwapF(*(f32 *)(p + 0x74));
            *(f32 *)(p + 0x78) = CritterSwapF(*(f32 *)(p + 0x78));
            *(f32 *)(p + 0x7C) = CritterSwapF(*(f32 *)(p + 0x7C));
            *(f32 *)(p + 0xAC) = CritterSwapF(*(f32 *)(p + 0xAC));
            *(f32 *)(p + 0xB0) = CritterSwapF(*(f32 *)(p + 0xB0));
            *(f32 *)(p + 0xB4) = CritterSwapF(*(f32 *)(p + 0xB4));
            *(f32 *)(p + 0xB8) = CritterSwapF(*(f32 *)(p + 0xB8));
            *(f32 *)(p + 0xBC) = CritterSwapF(*(f32 *)(p + 0xBC));
            *(f32 *)(p + 0xCC) = CritterSwapF(*(f32 *)(p + 0xCC));
            *(f32 *)(p + 0xDC) = CritterSwapF(*(f32 *)(p + 0xDC));
            *(u32 *)(p + 0xE0) = CritterSwap32(*(u32 *)(p + 0xE0));
            *(f32 *)(p + 0xE4) = CritterSwapF(*(f32 *)(p + 0xE4));
            *(f32 *)(p + 0xE8) = CritterSwapF(*(f32 *)(p + 0xE8));
            *(f32 *)(p + 0xEC) = CritterSwapF(*(f32 *)(p + 0xEC));
            *(f32 *)(p + 0xF0) = CritterSwapF(*(f32 *)(p + 0xF0));
            *(u32 *)(p + 0x120) = CritterSwap32(*(u32 *)(p + 0x120));
            *(u32 *)(p + 0x124) = CritterSwap32(*(u32 *)(p + 0x124));
            *(u32 *)(p + 0x128) = CritterSwap32(*(u32 *)(p + 0x128));
            *(u32 *)(p + 0x12C) = CritterSwap32(*(u32 *)(p + 0x12C));
            *(u32 *)(p + 0x130) = CritterSwap32(*(u32 *)(p + 0x130));
            *(u32 *)(p + 0x138) = CritterSwap32(*(u32 *)(p + 0x138));
            *(f32 *)(p + 0x80) = CritterSwapF(*(f32 *)(p + 0x80));
            *(f32 *)(p + 0x84) = CritterSwapF(*(f32 *)(p + 0x84));
            *(f32 *)(p + 0x88) = CritterSwapF(*(f32 *)(p + 0x88));
            *(f32 *)(p + 0x8C) = CritterSwapF(*(f32 *)(p + 0x8C));
            *(f32 *)(p + 0x90) = CritterSwapF(*(f32 *)(p + 0x90));
            *(f32 *)(p + 0x94) = CritterSwapF(*(f32 *)(p + 0x94));
            *(f32 *)(p + 0x98) = CritterSwapF(*(f32 *)(p + 0x98));
            *(f32 *)(p + 0x9C) = CritterSwapF(*(f32 *)(p + 0x9C));
            for (j = 0; j < 3; j++) {
                *(f32 *)(p + 0xA0 + j * 4) =
                    CritterSwapF(*(f32 *)(p + 0xA0 + j * 4));
                *(f32 *)(p + 0xC0 + j * 4) =
                    CritterSwapF(*(f32 *)(p + 0xC0 + j * 4));
                *(f32 *)(p + 0xD0 + j * 4) =
                    CritterSwapF(*(f32 *)(p + 0xD0 + j * 4));
                *(f32 *)(p + 0x100 + j * 4) =
                    CritterSwapF(*(f32 *)(p + 0x100 + j * 4));
            }
        }

        for (i = 0; i < header->addAnimCount; i++) {
            p = header->addAnims + i * 0x30;
            *(u16 *)(p + 0x00) = CritterSwap16(*(u16 *)(p + 0x00));
            *(u16 *)(p + 0x02) = CritterSwap16(*(u16 *)(p + 0x02));
            *(u32 *)(p + 0x04) = CritterSwap32(*(u32 *)(p + 0x04));
            *(u32 *)(p + 0x08) = CritterSwap32(*(u32 *)(p + 0x08));
            for (j = 0; j < 3; j++) {
                *(f32 *)(p + 0x20 + j * 4) =
                    CritterSwapF(*(f32 *)(p + 0x20 + j * 4));
            }
        }
    }

    for (i = 0; i < header->descriptorCount; i++) {
        *(s16 *)(header->descriptors + i * 0x30 + 0x22) = -1;
    }
    for (i = 0; i < header->typeCount; i++) {
        *(u8 **)(header->types + i * 0x140 + 0x134) = NULL;
    }
    for (i = 0; i < header->addAnimCount; i++) {
        p = header->addAnims + i * 0x30;
        if ((typeIndex = *(s16 *)p) > header->typeCount) {
            ErrorPrintf("CRITTER: AddAnim has addto idx %d > max %d",
                        typeIndex, header->typeCount);
        } else {
            type = header->types + typeIndex * 0x140;
            if (*(u8 **)(type + 0x134) != NULL) {
                tail = *(u8 **)(type + 0x134);
                while (*(u8 **)(tail + 8) != NULL) {
                    tail = *(u8 **)(tail + 8);
                }
                *(u8 **)(tail + 8) = p;
            } else {
                *(u8 **)(type + 0x134) = p;
            }
        }
    }
}
