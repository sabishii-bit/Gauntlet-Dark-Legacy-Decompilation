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
extern volatile f32 sMusicFadeBase;   /* 0x80344594 shared game-time / fade base   */
extern f32   lbl_80346480;
extern f32   lbl_80346470;
extern f64   lbl_80346488;
extern f64   lbl_80346490;
extern f32   lbl_803464B8;
extern f64   lbl_803464F8;
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
extern f64   lbl_80346510;
extern f32   lbl_80346518;
extern f32   lbl_8034651C;
extern f32   lbl_80346520;
extern f32   lbl_80346524;
extern f64   lbl_80346528;
extern f64   lbl_80346530;
extern f64   lbl_80346538;
extern f64   lbl_80346540;
extern f32   lbl_8034654C;

/* -- external helpers -- */
extern void *AllocFile(const char *wad, const char *name);
extern void *NextWaypoint(void *player);
extern void  AddExp(s32 player, s32 amount, s32 kind);
extern void  HealthMeterUpdate(void *meter, f32 cur, f32 max);
extern void *memset(void *dst, int c, u32 n);
extern void *memcpy(void *dst, const void *src, u32 n);
extern void  ErrorPrintf(const char *fmt, ...);
extern void  MBRemoveNode(void *node, s32 kind);
extern s32   GetWorldMat(void *node, f32 *matrix, f32 *offset);
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
extern void  SfxDeleteParented(void *sfx, s32 a, s32 b);
extern void  BossDeath(void);
extern void  del_target(void *mtx);
extern void  AtreeDelete(void *handle);
extern s32   CollectSafeRocks(s32 *out, s32 max, s32 flags);
extern void  SafeRockActivate(s32 index);
extern u32   RandInt(u32 limit);
extern s32   NextGridEnemy(void);
extern void  StartEnemyGrid(f32 *position, f32 radius);
extern s32   NextGridItem(void);
extern void  StartItemGrid(f32 radius, f32 *position);
extern void  damage_player(s32 player, f32 damage, s32 mode, u32 flags,
                           f32 *direction);
extern f32   NormalVector(f32 *vector);
extern f32   NormalVector2D(f32 *vector);
extern f32   SlowNormalVector(f32 *vector);
extern void  ProcessSkinFX(f32 *state, void *root, void *node);
extern void *MBNewNode(void *parent, const f32 *matrix, s32 mode);
extern void  CopyMat3(const f32 *src, f32 *dst);
extern void  CopyMat4(const f32 *src, f32 *dst);
extern void  MulVec4Mat3(const f32 *src, f32 *dst, const f32 *matrix);
extern void  UnparentMatrix(void *node, f32 *matrix);
extern void *lbl_8034473C;
extern s32   gFrameTicks;
extern u32   lbl_80344BF8;
extern u8    lbl_802411B0[0x540];
extern s32   lbl_80344668;
extern void *crit_load_desc;
extern s32  *lbl_80344640;
extern s32   lbl_80344630;
extern s32   lbl_80344634;
extern s32   lbl_80344638;
extern s32   MBOX_BGLoadModelDone(void);
extern void  MBOX_BGLoadModelStart(const char *name, s32 model, void *arg);
extern s32   AtreeHeaderFindSeq(void *atree, const char *name);
extern s32   FindTexMod(void *atree, const char *name, void *unused);
extern s32   AudioFindSound(const char *name, s32 bank, s32 global);
extern s32   MBOX_FindTexture(const char *name, void *unused);
extern s32   MBOX_FindTexture_Sub(const char *name, void *unused,
                                  s32 model, s32 fallback, s32 mode);
extern s32   AtreeModel(void *atree);
extern s32   InitCustomEffect(void *atree, char *name, s32 zmod, s32 alpha);
extern s32   sprintf(char *dst, const char *fmt, ...);
extern f64   atan2(f64 y, f64 x);
extern void  YawMat3(f32 angle, f32 *matrix);
extern u16   AnimateATree(void *tree, s32 sequence, s32 transition);
extern s32   AnimDone(void *animation);
extern void  MBTreeClearFlags(void *node, u32 flags, s32 mode);
extern s32   StartFXSub(s32 effect, f32 *position, u32 flags,
                        u32 treeFlags, u32 effectFlags, s32 owner);
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
extern void  MBPsysSetPSpeed(f32 speed, void *psys);
extern void *PlaceItem(s32 type, s32 subtype, const char *name, f32 *position);
extern void  AddItemSub(void *item);
extern void  fn_800920E0(f32 *position, void *item);
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
extern char  lbl_8011221C[];          /* 0x8011221C critter-overflow message      */
extern const char lbl_80112238[];
extern f32   gIdentityMatrix[12];
DECL_SECT(".sdata2") extern const char lbl_80346644[];
extern void *gCurLevel;               /* current level record (->0xAC hp scale)   */
extern char  lbl_8011219C[];          /* move-type lookup failure message          */

/* -- CRITTER.OBJ internal roster (forward declarations) -- */
struct CritterDamageDef;
void CritterCollideEnemies(Critter *c, f32 *delta);
void CritterCollideItems(Critter *c, f32 *delta);
void CritterCollidePlayers(Critter *c, f32 *delta);
void CritterCollideWorld(Critter *c, f32 *delta);
void CritterWorldDamage(Critter *c, void *surface, f32 *origin,
                        f32 *contact);
void CritterNodeEnemyCollide(Critter *c, void *damageDef);
s32  SafeRockNearestTarget(s32 player);
void CritterLookAtPlayer(Critter *c, void *move);
void NodeLookAtPos(void *node, f32 *target, f32 *yaw, f32 *pitch);
void CritterFirePlayerCollide(Critter *c, struct CritterDamageDef *damage);
void CritterNodePlayerCollide(Critter *c, struct CritterDamageDef *damage,
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
 s32 CritterMoveNodeCol(f32 *origin, f32 *destination, f32 *contact,
                        s32 ignore, s32 mode);
s32  CritterMoveNodeColSub(Critter *c, f32 *origin, f32 *destination,
                           f32 *point, f32 *contact, s32 first);
s32  CritterExpNodeColSub(Critter *c, f32 *origin, f32 radius,
                          f32 height, f32 *contact, s32 mode);
s32  CritterExpCollide(f32 *origin, f32 *forward, f32 radius,
                       f32 dot, f32 *contact, s32 timedId);
s32  CritterLineNodeColSub(Critter *c, f32 *origin, f32 *forward,
                           f32 *delta, f32 radius, f32 dotThreshold);
void CritterCollideStart(s32 unused, void *ctx);
s32  CritterNoHit(Critter *c, s32 id);
s32  CritterNoHitSub(Critter *c, s32 id);
void fn_80037ED0(f32 add, Critter *c, s32 id);
void CritterLineCollide(Critter *skip, f32 *origin, f32 *forward,
                        f32 radius, f32 *out, f32 *score);
f32  CritterLineRootColSub(Critter *c, f32 *origin, f32 *forward,
                           f32 radius, f32 *out);
void CritterDamage(f32 damage, Critter *c, s32 player, u32 flags,
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
extern f64 lbl_80346608;
extern f32 lbl_80346470;
s32  CritterGetDmove(CritterMove *a, CritterMove *b);
s32  CritterFindMoveType(Critter *c, s32 type, s32 mode);
void CritterAnimInterrupt(Critter *c, s32 action, s32 phase, s32 active);
void CritterDoTexmodNode(Critter *c, s32 action, s32 local,
                         f32 *position);
s32  CritterDoSfx(Critter *c, s32 sfx, void *parent, s32 arg3, s32 arg4);
s32  CritterDoSfxSub(Critter *c, void *sfx, f32 *position,
                     s32 parented, u32 flags);
void CritterDoParticle(Critter *c, void *sfx, s32 node);
Critter *CritterNewInst(s32 type, s32 subtype, void *object);
void CritterInitGeo(Critter *c, void *object);
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
void CritterLoadStartNext(void);
void CritterLoadAllTypes(s32 arg);
struct CritterHeader *CritterTypeLoaded(s32 type, s32 subtype);
void CritterAllocType(void *hdr, void *move, s32 arg);
void CritterLoadFinish(void *typeHeader);
void CritterInitAllMoves(void);
void CritterInitMoves(void *move);
void CritterInitSfx(void *file, s32 index, void *atreeHeader);
void CritterInitHeader(void *hdr, void *file);

/* ==================================================================== */

/* 0x80034CFC -- stop or deflect this frame's translation when it overlaps
 * a live swarm enemy. */
void CritterCollideEnemies(Critter *c, f32 *delta)
{
    Enemy *enemy;
    f32 dx;
    f32 dz;
    f32 radius;
    f32 distance2;
    s32 index;

    radius = *(f32 *)((u8 *)c->hdr + 0x7C);
    StartItemGrid(radius, c->pos);
    while ((index = NextGridItem()) >= 0) {
        enemy = &gEnemies[index];
        if (enemy->state != ACTIVE && enemy->state != ON_EXIT) {
            continue;
        }
        dx = enemy->trans[0] - (c->pos[0] + delta[0]);
        dz = enemy->trans[2] - (c->pos[2] + delta[2]);
        distance2 = dx * dx + dz * dz;
        if (distance2 <= (radius + enemy->rad) * (radius + enemy->rad)) {
            delta[0] = 0.0f;
            delta[2] = 0.0f;
            enemy->pushed[0] += dx * -0.5f;
            enemy->pushed[2] += dz * -0.5f;
        }
    }
}

/* 0x80034F60 -- stop translation against collidable item records returned by
 * the item grid. */
void CritterCollideItems(Critter *c, f32 *delta)
{
    u8 *item;
    f32 dx;
    f32 dz;
    f32 radius;
    s32 index;

    if (sItems == NULL) {
        return;
    }
    radius = *(f32 *)((u8 *)c->hdr + 0x7C);
    StartEnemyGrid(c->pos, radius);
    while ((index = NextGridEnemy()) >= 0) {
        item = sItems + index * 0xF0;
        if (*(s32 *)item == 0) {
            continue;
        }
        dx = *(f32 *)(item + 0x34) - (c->pos[0] + delta[0]);
        dz = *(f32 *)(item + 0x3C) - (c->pos[2] + delta[2]);
        if (dx * dx + dz * dz <= radius * radius) {
            delta[0] = 0.0f;
            delta[2] = 0.0f;
        }
    }
}

/* 0x800351B0 -- separate active players from a translating critter and feed
 * the displacement into their push vectors. */
void CritterCollidePlayers(Critter *c, f32 *delta)
{
    Player *player;
    f32 separation[3];
    f32 distance;
    f32 penetration;
    f32 radius;
    s32 i;

    radius = *(f32 *)((u8 *)c->hdr + 0x7C);
    for (i = 0; i < 4; i++) {
        player = &gPlayers[i];
        if ((player->state != 1 && player->state != 4) ||
            (*(u16 *)((u8 *)player + 0x964) & 0x20) != 0) {
            continue;
        }
        separation[0] = *(f32 *)((u8 *)player + 0x64) -
                        (c->pos[0] + delta[0]);
        separation[1] = 0.0f;
        separation[2] = *(f32 *)((u8 *)player + 0x6C) -
                        (c->pos[2] + delta[2]);
        distance = separation[0] * separation[0] +
                   separation[2] * separation[2];
        if (distance > 0.0f) {
            distance = (f32)NormalVector2D(separation);
        }
        penetration = radius - distance;
        if (penetration > 0.0f) {
            *(f32 *)((u8 *)player + 0x870) +=
                separation[0] * penetration * 0.5f;
            *(f32 *)((u8 *)player + 0x878) +=
                separation[2] * penetration * 0.5f;
            delta[0] = 0.0f;
            delta[2] = 0.0f;
        }
    }
}

/* 0x80035408 -- integrate the world-contact portion of a movement delta and
 * cache a floor point/status for item drops and shadows. */
void CritterCollideWorld(Critter *c, f32 *delta)
{
    f32 nextY;
    f32 floorY;

    nextY = c->pos[1] + delta[1];
    floorY = *(f32 *)((u8 *)c + 0x43C);
    if (nextY < floorY) {
        delta[1] += floorY - nextY;
        if (c->knockbackVelocity[1] < 0.0f) {
            c->knockbackVelocity[1] = 0.0f;
        }
        *(u32 *)((u8 *)c + 0x448) |= 0x10;
    } else {
        *(u32 *)((u8 *)c + 0x448) &= ~0x10;
    }
    *(f32 *)((u8 *)c + 0x438) = c->pos[0] + delta[0];
    *(f32 *)((u8 *)c + 0x440) = c->pos[2] + delta[2];
}

/* 0x800358B0 -- translate collision material flags into a critter damage
 * class and direction. */
void CritterWorldDamage(Critter *c, void *surface, f32 *origin,
                        f32 *contact)
{
    u32 material;
    u32 flags;
    f32 direction[3];
    f32 damage;

    if (surface == NULL) {
        return;
    }
    material = *(u32 *)((u8 *)surface + 0x10) & 0xF0000;
    flags = 0;
    damage = 0.0f;
    if (material == 0x10000 || material == 0x20000) {
        damage = 1.0f;
        flags = material == 0x20000 ? 0x10 : 0;
    } else if (material >= 0x30000 && material <= 0x50000) {
        damage = 2.0f;
        flags = 0x20;
    }
    if (damage <= 0.0f) {
        return;
    }
    direction[0] = origin[0] - contact[0];
    direction[1] = 0.0f;
    direction[2] = origin[2] - contact[2];
    NormalVector(direction);
    CritterDamage(damage, c, -1, flags, contact, direction, 1);
}

/* 0x800359F0 -- damage swarm enemies intersecting an active critter node. */
void CritterNodeEnemyCollide(Critter *c, void *damageDef)
{
    Enemy *enemy;
    f32 delta[3];
    f32 radius;
    s32 i;

    if (damageDef == NULL) {
        return;
    }
    radius = *(f32 *)((u8 *)damageDef + 0x0C);
    for (i = 0; i < gNumEnemies; i++) {
        enemy = &gEnemies[i];
        if (enemy->state != ACTIVE) {
            continue;
        }
        delta[0] = enemy->trans[0] - c->moveOrigin[0];
        delta[1] = enemy->trans[1] - c->moveOrigin[1];
        delta[2] = enemy->trans[2] - c->moveOrigin[2];
        if (NormalVector(delta) <= radius + enemy->rad) {
            enemy->pushed[0] += delta[0] * *(f32 *)((u8 *)damageDef + 0x2C);
            enemy->pushed[1] += delta[1] * *(f32 *)((u8 *)damageDef + 0x2C);
            enemy->pushed[2] += delta[2] * *(f32 *)((u8 *)damageDef + 0x2C);
        }
    }
}

/* 0x80035BC8 -- choose an available safe rock, or the available rock nearest
 * a requested player. */
s32 SafeRockNearestTarget(s32 player)
{
    f32 matrix[12];
    f32 dx;
    f32 dz;
    f32 best;
    s32 bestIndex;
    s32 i;
    void *node;

    bestIndex = -1;
    best = lbl_803464C0;
    for (i = 0; i < lbl_80344658; i++) {
        if (SafeRockActive(lbl_80241020[i]) != 0) {
            continue;
        }
        if (player < 0) {
            return i;
        }
        node = ItemGetNode(lbl_80241020[i]);
        if (node == NULL) {
            continue;
        }
        GetWorldMat(node, matrix, NULL);
        dx = matrix[9] - *(f32 *)((u8 *)&gPlayers[player] + 0x64);
        dz = matrix[11] - *(f32 *)((u8 *)&gPlayers[player] + 0x6C);
        if (dx * dx + dz * dz < best) {
            best = dx * dx + dz * dz;
            bestIndex = i;
        }
    }
    return bestIndex;
}

/* 0x80035D08 -- aim the two optional look-at nodes at the selected player. */
void CritterLookAtPlayer(Critter *c, void *move)
{
    f32 target[3];
    f32 *targetPtr;

    targetPtr = NULL;
    if (c->unk124 >= 0 && c->unk124 < 4) {
        memcpy(target, (u8 *)&gPlayers[c->unk124] + 0x64, sizeof(target));
        targetPtr = target;
    }
    if (c->hitnode0 != NULL) {
        NodeLookAtPos(c->hitnode0, targetPtr,
                      (f32 *)((u8 *)c + 0x100),
                      (f32 *)((u8 *)c + 0x108));
    }
    if (c->hitnode1 != NULL) {
        NodeLookAtPos(c->hitnode1, targetPtr,
                      (f32 *)((u8 *)c + 0x104),
                      (f32 *)((u8 *)c + 0x10C));
    }
    (void)move;
}

/* 0x80035E48 -- smoothly yaw and pitch a scene node toward a world point. */
void NodeLookAtPos(void *node, f32 *target, f32 *yaw, f32 *pitch)
{
    f32 matrix[12];
    f32 dx;
    f32 dy;
    f32 dz;
    f32 wantedYaw;
    f32 wantedPitch;
    f32 yawDelta;
    f32 step;

    wantedYaw = 0.0f;
    wantedPitch = 0.0f;
    if (node != NULL && target != NULL) {
        GetWorldMat(node, matrix, NULL);
        dx = target[0] - matrix[9];
        dy = target[1] - matrix[10];
        dz = target[2] - matrix[11];
        wantedYaw = (f32)atan2(dx, dz);
        wantedPitch = (f32)atan2(dy, dx * dx + dz * dz);
    }
    step = 0.08f * gClockFrameStep;
    yawDelta = wantedYaw - *yaw;
    while (yawDelta > 3.1415927f) {
        yawDelta -= 6.2831855f;
    }
    while (yawDelta < -3.1415927f) {
        yawDelta += 6.2831855f;
    }
    if (yawDelta > step) {
        *yaw += step;
    } else if (yawDelta < -step) {
        *yaw -= step;
    } else {
        *yaw = wantedYaw;
    }
    if (wantedPitch - *pitch > step) {
        *pitch += step;
    } else if (wantedPitch - *pitch < -step) {
        *pitch -= step;
    } else {
        *pitch = wantedPitch;
    }
}

/* 0x80036138 -- test the critter's forward fire segment against players. */
void CritterFirePlayerCollide(Critter *c, struct CritterDamageDef *damage)
{
    f32 delta[3];
    Player *player;
    s32 i;

    for (i = 0; i < 4; i++) {
        player = &gPlayers[i];
        if (player->state != 1) {
            continue;
        }
        delta[0] = *(f32 *)((u8 *)player + 0x64) - c->moveOrigin[0];
        delta[1] = *(f32 *)((u8 *)player + 0x68) - c->moveOrigin[1];
        delta[2] = *(f32 *)((u8 *)player + 0x6C) - c->moveOrigin[2];
        if (NormalVector(delta) <= *(f32 *)((u8 *)damage + 0x0C)) {
            CritterDamagePlayer(player, c, damage, 0, delta, 1);
        }
    }
}

/* 0x80036424 -- test an expanded critter node/body volume against players. */
void CritterNodePlayerCollide(Critter *c, struct CritterDamageDef *damage,
                              s32 enabled)
{
    f32 delta[3];
    Player *player;
    f32 radius;
    s32 i;

    radius = enabled ? *(f32 *)((u8 *)damage + 0x2C) : 0.0f;
    for (i = 0; i < 4; i++) {
        player = &gPlayers[i];
        if (player->state != 1 || !PlayerAttacking(i, 0)) {
            continue;
        }
        delta[0] = *(f32 *)((u8 *)player + 0x64) - c->pos[0];
        delta[1] = *(f32 *)((u8 *)player + 0x68) - c->pos[1];
        delta[2] = *(f32 *)((u8 *)player + 0x6C) - c->pos[2];
        if (NormalVector(delta) <=
            radius + *(f32 *)((u8 *)c->hdr + 0x7C)) {
            CritterDamagePlayer(player, c, damage, 0, delta, 1);
        }
    }
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
/* 0x80036A58 */ s32 CritterGetTargetSub(Critter *c, f32 *target, s32 mode)
{
    s32 i;
    s32 best;
    f32 bestScore;

    if (target != NULL &&
        (target[6] <= 0.0f || c->unk4AC <= target[6])) {
        return -1;
    }

    best = -1;
    bestScore = lbl_80346508;
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
    if (best < 0) {
        if (mode != 0 && c->parent != NULL) {
            return CritterGetTargetSub(c->parent, target, mode);
        }
        return -1;
    }
    return *(s32 *)((u8 *)c + 0x12C + best * 0x24);
}
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
        if (*(f32 *)((u8 *)c + 0x110) < moveTarget[4] ||
            (moveTarget[4] < moveTarget[5] &&
             moveTarget[5] <= *(f32 *)((u8 *)c + 0x110))) {
            return lbl_80346518;
        }
    }

    range = entry[2];
    if (moveTarget != NULL) {
        if (range < moveTarget[0]) {
            return lbl_8034651C;
        }
        if (moveTarget[1] > 0.0f && moveTarget[1] < range) {
            return lbl_80346520;
        }
        forward[0] = c->mtx[2][0];
        forward[1] = 0.0f;
        forward[2] = c->mtx[2][2];
        SlowNormalVector(forward);
        dot = entry[5] * forward[0] + entry[7] * forward[2];
        if (dot < moveTarget[3]) {
            return lbl_80346524;
        }
    }
    return range * entry[4];
}

/* 0x80036C70 -- choose the single best live player target. */
void CritterGetSingleTargetPlayer(Critter *c)
{
    CritterTargetRecord candidate;
    Player *player;
    s32 i;
    f32 score;

    c->targetCount = 0;
    if (c->health <= 0.0f) {
        return;
    }
    for (i = 0; i < 4; i++) {
        player = &gPlayers[i];
        if (player->state != 1 || (*(u32 *)((u8 *)player + 0x124) & 4) != 0) {
            continue;
        }
        memset(&candidate, 0, sizeof(candidate));
        candidate.words00[0] = i;
        score = CritterCalcTarget(c, (f32 *)((u8 *)c->hdr + 0x80),
                                  (f32 *)((u8 *)player + 0x64),
                                  &candidate);
        if (c->targetCount == 0 ||
            score < ((CritterTargetState *)c)->records[0].distance) {
            candidate.distance = score;
            c->targetCount = 1;
            ((CritterTargetState *)c)->records[0] = candidate;
        }
    }
    if (c->targetCount != 0) {
        *(s32 *)((u8 *)c + 0xAD4) = 0;
    }
}

/* 0x80036E00 -- distribute over-subscribed player targets among a root
 * critter and its child chain. */
void CritterResolveMultipleTargets(Critter *c)
{
    Critter *child;
    Critter *owner;
    s32 i;
    s32 j;
    s32 player;
    f32 best;

    if (c->alivecnt <= 0 || c->unk11C >= 0) {
        return;
    }
    for (i = 0; i < c->targetCount; i++) {
        player = *(s32 *)((u8 *)c + 0x12C + i * 0x24);
        while (gBig.scratch[player] > 4.0f) {
            owner = NULL;
            best = 0.0f;
            for (child = c->next; child != NULL; child = child->next) {
                if (child->unk11C >= 0) {
                    continue;
                }
                for (j = 0; j < child->targetCount; j++) {
                    u8 *entry = (u8 *)child + 0x12C + j * 0x24;
                    if (*(s32 *)entry == player &&
                        (owner == NULL || *(f32 *)(entry + 0x0C) > best)) {
                        owner = child;
                        best = *(f32 *)(entry + 0x0C);
                        break;
                    }
                }
            }
            if (owner == NULL) {
                break;
            }
            for (j = 0; j + 1 < owner->targetCount; j++) {
                u8 *dst = (u8 *)owner + 0x12C + j * 0x24;
                u8 *src = dst + 0x24;
                if (*(s32 *)dst == player) {
                    memcpy(dst, src, 0x24);
                }
            }
            owner->targetCount--;
            gBig.scratch[player] -= 1.0f;
        }
    }
}

/* 0x80036FBC -- collect and distance-sort all eligible player targets. */
void CritterGetTargetPlayers(Critter *c)
{
    CritterTargetRecord target;
    Player *player;
    s32 i;
    f32 score;
    f32 damage;

    c->targetCount = 0;
    if (c->health <= 0.0f) {
        return;
    }
    for (i = 0; i < 4; i++) {
        player = &gPlayers[i];
        if (player->state != 1) {
            continue;
        }
        memset(&target, 0, sizeof(target));
        target.words00[0] = i;
        score = CritterCalcTarget(c, (f32 *)((u8 *)c->hdr + 0x80),
                                  (f32 *)((u8 *)player + 0x64), &target);
        if (score >= (f32)lbl_80346510) {
            continue;
        }
        damage = c->unk1BC[i][2];
        if (damage > 0.0f) {
            target.words10[0] = (u32)(c->unk1BC[i][0] / damage);
        }
        target.distance = score;
        CritterInsertTarget((CritterTargetState *)c, &target);
    }
    for (i = 0; i < c->targetCount; i++) {
        s32 index = *(s32 *)((u8 *)c + 0x12C + i * 0x24);
        if (index >= 0) {
            gBig.scratch[index] += 1.0f;
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
#pragma dont_inline on
/* 0x800372A0 -- calculate range, facing and score for a world-space target. */
f32 CritterCalcTarget(Critter *c, f32 *moveTarget, f32 *target,
                      CritterTargetRecord *record)
{
    f32 delta[3];
    f32 forward[3];
    f32 distance;
    f32 vertical;
    f32 dot;
    f32 score;

    if (moveTarget != NULL &&
        (*(f32 *)((u8 *)c + 0x110) < moveTarget[4] ||
         (moveTarget[5] > 0.0f &&
          moveTarget[5] <= *(f32 *)((u8 *)c + 0x110)))) {
        return lbl_80346508;
    }

    delta[0] = target[0] - c->pos[0];
    delta[1] = 0.0f;
    delta[2] = target[2] - c->pos[2];
    vertical = target[1] - c->pos[1];
    distance = SlowNormalVector(delta);
    forward[0] = c->mtx[2][0];
    forward[1] = 0.0f;
    forward[2] = c->mtx[2][2];
    SlowNormalVector(forward);
    dot = delta[0] * forward[0] + delta[2] * forward[2];

    if (moveTarget != NULL) {
        f32 height = vertical < 0.0f ? -vertical : vertical;
        if (distance < moveTarget[0] ||
            (moveTarget[1] > 0.0f && moveTarget[1] < distance) ||
            (moveTarget[7] > 0.0f && moveTarget[7] < height) ||
            dot < moveTarget[3]) {
            return lbl_80346508;
        }
    }
    score = dot <= 0.0f ? lbl_8034654C * distance : distance / dot;
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
#pragma dont_inline off
/* 0x800374FC -- sweep a movement segment against every other live critter. */
s32 CritterMoveNodeCol(f32 *origin, f32 *destination, f32 *contact,
                       s32 ignore, s32 mode)
{
    Critter *c;
    s32 i;
    s32 result;

    result = 0;
    for (i = 0; i < lbl_8034466C; i++) {
        c = &gCritterPool[i];
        if (c->hdr == NULL || c == lbl_80344648 || i == ignore) {
            continue;
        }
        if ((*(u32 *)((u8 *)c->hdr + 0x5C) & 2) != 0) {
            result = CritterMoveNodeColSub(c, origin, destination,
                                           destination, contact, mode);
        }
        if (!result) {
            f32 delta[3];
            f32 radius;
            delta[0] = c->pos[0] - destination[0];
            delta[1] = c->pos[1] - destination[1];
            delta[2] = c->pos[2] - destination[2];
            radius = *(f32 *)((u8 *)c->hdr + 0x7C);
            if (NormalVector(delta) <= radius) {
                memcpy(contact, delta, sizeof(delta));
                result = 1;
            }
        }
        if (result) {
            break;
        }
    }
    return result;
}

/* 0x80037734 -- sweep against one critter's active collision nodes and keep
 * either the first or nearest contact. */
s32 CritterMoveNodeColSub(Critter *c, f32 *origin, f32 *destination,
                          f32 *point, f32 *contact, s32 first)
{
    f32 delta[3];
    f32 best;
    f32 distance;
    f32 radius;
    u8 *node;
    s32 i;
    s32 result;

    result = 0;
    best = lbl_80346480;
    for (i = 0; i < *(s16 *)((u8 *)c->hdr + 0x118); i++) {
        node = (u8 *)c + 0x4F8 + i * 0x5C;
        if (*(void **)(node + 4) == NULL ||
            *(f32 *)(node + 0x58) >= *(f32 *)(node + 0x54) ||
            (*(u16 *)(*(u8 **)node + 0x10) & 8) == 0) {
            continue;
        }
        delta[0] = point[0] - *(f32 *)(node + 0x3C);
        delta[1] = point[1] - *(f32 *)(node + 0x40);
        delta[2] = point[2] - *(f32 *)(node + 0x44);
        distance = NormalVector(delta);
        radius = *(f32 *)(*(u8 **)node + 0x2C);
        if (distance <= radius && (!result || distance < best)) {
            memcpy(contact, delta, sizeof(delta));
            best = distance;
            result = 1;
            if (first) {
                break;
            }
        }
    }
    (void)origin;
    (void)destination;
    return result;
}

/* 0x800378C8 -- test an expanded sphere/capsule against one critter's active
 * hit nodes. */
s32 CritterExpNodeColSub(Critter *c, f32 *origin, f32 radius,
                         f32 height, f32 *contact, s32 mode)
{
    f32 delta[3];
    f32 nodeRadius;
    u8 *node;
    s32 i;

    for (i = 0; i < *(s16 *)((u8 *)c->hdr + 0x118); i++) {
        node = (u8 *)c + 0x4F8 + i * 0x5C;
        if (*(void **)(node + 4) == NULL ||
            *(f32 *)(node + 0x58) >= *(f32 *)(node + 0x54)) {
            continue;
        }
        if (mode == 2 && (*(u16 *)(*(u8 **)node + 0x10) & 8) == 0) {
            continue;
        }
        delta[0] = *(f32 *)(node + 0x3C) - origin[0];
        delta[1] = *(f32 *)(node + 0x40) - origin[1];
        delta[2] = *(f32 *)(node + 0x44) - origin[2];
        nodeRadius = radius + *(f32 *)(*(u8 **)node + 0x2C);
        if (delta[0] * delta[0] + delta[2] * delta[2] <=
                nodeRadius * nodeRadius &&
            delta[1] <= nodeRadius + height) {
            memcpy(contact, delta, sizeof(delta));
            c->unkAB8 = (s16)i;
            return 1;
        }
    }
    c->unkAB8 = -1;
    return 0;
}

/* 0x80037A10 -- expanded collision query across all critter roots, excluding
 * families that already own the supplied timed hit id. */
s32 CritterExpCollide(f32 *origin, f32 *forward, f32 radius,
                      f32 dot, f32 *contact, s32 timedId)
{
    Critter *c;
    f32 delta[3];
    f32 bodyRadius;
    f32 distance;
    s32 i;

    for (i = 0; i < lbl_8034466C; i++) {
        c = &gCritterPool[i];
        if (c->hdr == NULL || c == lbl_80344648 ||
            CritterNoHit(c, timedId)) {
            continue;
        }
        if ((*(u32 *)((u8 *)c->hdr + 0x5C) & 2) != 0 &&
            CritterExpNodeColSub(c, origin, radius, 0.0f, contact, 1)) {
            return 1;
        }
        delta[0] = c->pos[0] - origin[0];
        delta[1] = c->pos[1] - origin[1];
        delta[2] = c->pos[2] - origin[2];
        distance = NormalVector(delta);
        bodyRadius = radius + *(f32 *)((u8 *)c->hdr + 0x7C);
        if (distance <= bodyRadius &&
            (dot <= 0.0f ||
             delta[0] * forward[0] + delta[2] * forward[2] >= dot)) {
            memcpy(contact, delta, sizeof(delta));
            return 1;
        }
    }
    return 0;
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
void CritterLineCollide(Critter *skip, f32 *origin, f32 *forward,
                        f32 radius, f32 *out, f32 *score)
{
    Critter *root;
    Critter *candidate;
    f32 delta[3];
    f32 best;
    f32 value;
    s32 i;

    best = lbl_80346508;
    for (i = 0; i < lbl_8034466C; i++) {
        root = &gCritterPool[i];
        if (root->hdr == NULL || root->state <= 1 || root->parent != NULL) {
            continue;
        }
        for (candidate = root; candidate != NULL; candidate = candidate->next) {
            if (candidate == skip) {
                continue;
            }
            value = CritterLineRootColSub(candidate, origin, forward,
                                          radius, delta);
            if (value < best) {
                best = value;
                if (out != NULL) {
                    memcpy(out, delta, sizeof(delta));
                }
            }
        }
    }
    if (out != NULL && best >= 1000000.0f) {
        memcpy(out, forward, sizeof(delta));
    }
    if (score != NULL) {
        *score = best;
    }
}

/* 0x800380F0 -- score a swept point against a critter's active hit nodes,
 * falling back to its body radius when it has no qualifying node. */
f32 CritterLineRootColSub(Critter *c, f32 *origin, f32 *forward,
                          f32 radius, f32 *out)
{
    f32 delta[3];
    f32 best;
    f32 distance;
    f32 along;
    f32 nodeRadius;
    u8 *node;
    s32 i;

    best = lbl_80346508;
    if (c->health <= 0.0f || c->state <= 1) {
        return best;
    }
    if ((*(u32 *)((u8 *)c->hdr + 0x5C) & 2) != 0) {
        for (i = 0; i < *(s16 *)((u8 *)c->hdr + 0x118); i++) {
            node = (u8 *)c + 0x4F8 + i * 0x5C;
            if (*(void **)(node + 4) == NULL ||
                *(f32 *)(node + 0x58) >= *(f32 *)(node + 0x54)) {
                continue;
            }
            delta[0] = *(f32 *)(node + 0x3C) - origin[0];
            delta[1] = *(f32 *)(node + 0x40) - origin[1];
            delta[2] = *(f32 *)(node + 0x44) - origin[2];
            distance = NormalVector(delta);
            nodeRadius = *(f32 *)(*(u8 **)node + 0x2C);
            if (radius > 0.0f && distance > radius) {
                continue;
            }
            along = delta[0] * forward[0] + delta[2] * forward[2];
            if (along > 0.0f && distance - nodeRadius < best) {
                best = distance - nodeRadius;
                memcpy(out, delta, sizeof(delta));
            }
        }
    }
    if (best >= lbl_80346508) {
        delta[0] = c->pos[0] - origin[0];
        delta[1] = c->pos[1] - origin[1];
        delta[2] = c->pos[2] - origin[2];
        distance = NormalVector(delta);
        nodeRadius = *(f32 *)((u8 *)c->hdr + 0x7C);
        along = delta[0] * forward[0] + delta[2] * forward[2];
        if ((radius <= 0.0f || distance <= radius) && along > 0.0f) {
            best = distance - nodeRadius;
            memcpy(out, delta, sizeof(delta));
        }
    }
    return best;
}

/* 0x800383A8 -- apply damage to a critter/hit node, accumulate combat
 * bookkeeping and transition a depleted critter into its death state. */
void CritterDamage(f32 damage, Critter *c, s32 player, u32 flags,
                   f32 *hitPosition, f32 *direction, s32 source)
{
    f32 applied;
    f32 maximum;
    u8 *node;
    s32 i;

    if (c == NULL || c->hdr == NULL || c->state <= 1 || damage <= 0.0f) {
        return;
    }
    applied = damage;
    if (c->curmove >= 0) {
        CritterMove *move =
            &(*(CritterMove **)((u8 *)c->hdr + 0x124))[c->curmove];
        if (move->type == 0x23) {
            applied *= 0.5f;
            flags &= ~0x130;
        }
    }

    if (c->unkAB8 >= 0 && (flags & 0x100320) == 0) {
        node = (u8 *)c + 0x4F8 + c->unkAB8 * 0x5C;
        if (*(f32 *)(node + 0x58) < *(f32 *)(node + 0x54)) {
            applied *= *(f32 *)(*(u8 **)node + 0x40);
            if (*(f32 *)(node + 0x58) + applied >
                *(f32 *)(node + 0x54)) {
                applied = *(f32 *)(node + 0x54) -
                          *(f32 *)(node + 0x58);
            }
            *(f32 *)(node + 0x58) += applied;
            if (*(f32 *)(node + 0x58) >= *(f32 *)(node + 0x54)) {
                *(s32 *)(node + 0x50) = 8;
            }
        }
    }
    if (applied <= 0.0f) {
        return;
    }

    c->counterState |= (s32)flags;
    c->counterValue += applied;
    c->counterTime = sMusicFadeBase;
    if (direction != NULL) {
        c->knockbackInput[0] += direction[0];
        c->knockbackInput[1] += direction[1];
        c->knockbackInput[2] += direction[2];
    }
    if (hitPosition == NULL) {
        hitPosition = c->movevec;
    }
    c->health -= applied;
    if (player >= 0 && player < 4) {
        c->unk1BC[player][2] += applied;
        c->unk1BC[player][3] = sMusicFadeBase;
    }
    if (source >= 0) {
        c->unkABC = 8;
    }
    maximum = *(f32 *)((u8 *)c->hdr + 0xE4) *
              *(f32 *)((u8 *)gCurLevel + 0xAC);
    if (c->health < 0.0f) {
        c->health = 0.0f;
    } else if (c->health > maximum) {
        c->health = maximum;
    }
    if (c->health <= 0.0f) {
        c->state = 3;
        for (i = 0; i < 4; i++) {
            if (c->unk1BC[i][2] > 0.0f) {
                CritterAwardExp(i, c->unk1BC[i][2]);
            }
        }
    }
}
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
/* 0x80038DDC -- update one root critter and its child chain, including world
 * transforms, hit nodes, AI, animation, skin effects and render matrices. */
s32 ProcessCritter(Critter *c)
{
    Critter *child;
    u8 *node;
    s32 i;
    s32 type;

    if (c->parent != NULL) {
        return 0;
    }
    c->alivecnt = 0;
    for (child = c->next; child != NULL; child = child->next) {
        if (child->health > 0.0f) {
            c->alivecnt++;
        }
    }

    if (c->mbnode != NULL) {
        GetWorldMat(c->mbnode, &c->mtx[0][0], NULL);
    }
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
        HealthMeterUpdate((void *)(s32)c->healthmtr, c->health,
                          *(f32 *)((u8 *)c->hdr + 0xE4));
    }
    if (c->damageflash != NULL) {
        f32 maximum = *(f32 *)((u8 *)c->hdr + 0xE4) *
                      *(f32 *)((u8 *)gCurLevel + 0xAC);
        f32 scale = maximum > 0.0f ? c->health / maximum : 0.0f;
        if (scale > 0.0f) {
            MBTreeSetScale(scale, 1.0f, 1.0f, c->damageflash);
        } else {
            AtreeDelete(&c->healthbar[0]);
            c->damageflash = NULL;
        }
    }

    for (child = c->next; child != NULL; child = child->next) {
        CopyMat4(&c->mtx[0][0], &child->mtx[0][0]);
        memcpy(child->movevec, c->movevec, sizeof(c->movevec));
        memcpy(child->pos, c->pos, sizeof(c->pos));
        if (child->obj_d0 != NULL) {
            GetWorldMat(child->obj_d0, child->worldMoveMatrix, NULL);
            child->vel[0] = child->moveOrigin[0];
            child->vel[1] = child->moveOrigin[1];
            child->vel[2] = child->moveOrigin[2];
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
    }

    if (c->state == 3 && c->health <= 0.0f) {
        c->state = 1;
        CritterAwardExp(-1, *(f32 *)((u8 *)c->hdr + 0xE8));
        for (child = c->next; child != NULL; child = child->next) {
            child->health = 1.0f;
        }
    }

    type = *(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x20);
    if (type == 4) {
        if (!CritterBossAI(c)) {
            return 0;
        }
    } else if (type == 3 || type == 7 || type == 8) {
        if (!CritterGolemAI(c)) {
            return 0;
        }
    } else {
        CritterAnimate(c);
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
    if (c->mbnode != NULL) {
        CopyMat4(&c->mtx[0][0], (f32 *)c->mbnode);
        UnparentMatrix(c->mbnode, &c->mtx[0][0]);
    }
    return 1;
}
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
    CritterMove *move;
    Critter *child;

    CritterGetTargetPlayers(c);
    CritterResolveMultipleTargets(c);
    if (c->state == 0 && c->targetCount != 0) {
        c->state = 3;
    }
    if (c->nextmove < 0) {
        CritterGetDoAction(c);
    }
    if (c->nextmove < 0) {
        CritterLookForCriticalMove(c);
    }
    if (c->nextmove < 0) {
        CritterLookForReady(c);
    }
    if (c->nextmove < 0) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x20, 1);
    }

    CritterAnimate(c);
    move = c->curmove >= 0
               ? &(*(CritterMove **)((u8 *)c->hdr + 0x124))[c->curmove]
               : NULL;
    if (move != NULL) {
        CritterMoveSetup(c, move);
        CritterActivate(c, move, (s32)c->animtimer);
        if (!CritterTranslate(c, move)) {
            CritterRotate(c, move);
        }
    }
    for (child = c->next; child != NULL; child = child->next) {
        if (child->nextmove < 0) {
            CritterChildGetPattern(child);
        }
        CritterAnimate(child);
    }
    return 1;
}

/* 0x80039AD8 -- run boss target distribution, pattern selection and the
 * coordinated root/child animation pass. */
s32 CritterBossAI(Critter *c)
{
    Critter *child;
    f32 maximum;
    f32 speed;

    CritterGetTargetPlayers(c);
    maximum = 1.0f + *(f32 *)((u8 *)c->hdr + 0xE4) *
                       *(f32 *)((u8 *)gCurLevel + 0xAC);
    speed = 1.0f - c->health / maximum;
    speed = 0.8f + speed * 0.4f;
    *(f32 *)((u8 *)c + 0x110) = speed;
    *(f32 *)((u8 *)c + 0x114) = 1.0f / speed;
    for (child = c->next; child != NULL; child = child->next) {
        CritterGetTargetPlayers(child);
        *(f32 *)((u8 *)child + 0x110) = speed;
        *(f32 *)((u8 *)child + 0x114) = 1.0f / speed;
    }
    CritterResolveMultipleTargets(c);
    if (c->state == 0 && c->targetCount != 0) {
        c->state = 3;
        for (child = c->next; child != NULL; child = child->next) {
            child->state = 3;
        }
    }
    return CritterGolemAI(c);
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
            big->safeRockTimers[i] = 0.0f;
        }
        if (count <= 0) {
            lbl_80344658 = -1;
        } else {
            lbl_80344654 = RandInt(count);
        }
    } else if (lbl_80344658 > 0) {
        for (i = 0; i < lbl_80344658; i++) {
            if ((f64)big->safeRockTimers[i] > 0.0) {
                big->safeRockTimers[i] -= gClockFrameStep;
                if ((f64)big->safeRockTimers[i] <= 0.0) {
                    SafeRockActivate(big->safeRockIndices[i]);
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
    char name[40];
    char *source;
    s32 i;
    s32 type;

    item = *(void **)((u8 *)c + 0xACC);
    type = 0;
    if (item != NULL) {
        *(void **)((u8 *)c + 0xACC) = NULL;
        type = 1;
    } else if (*(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x20) == 7) {
        source = fn_80057ACC(0x20);
        for (i = 0; i < (s32)sizeof(name) - 1 && source[i] != '\0'; i++) {
            name[i] = (char)toupper(source[i]);
        }
        name[i] = '\0';
        item = PlaceItem(1, 0x10, name, NULL);
        type = 2;
    }
    if (item == NULL) {
        return;
    }
    if (*(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x20) == 8) {
        msgPost(0x86, -1, 0);
    } else if (*(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x20) == 7) {
        msgPost(0x8A, -1, 0);
    }
    if (type == 0) {
        memcpy((u8 *)item + 0x34, (u8 *)c + 0x438, 12);
        AddItemSub(item);
    } else {
        fn_800920E0((f32 *)((u8 *)c + 0x438), item);
    }
}

/* 0x8003A9C4 -- integrate scripted translation and knockback, then clamp the
 * result through world and critter collision. */
s32 CritterTranslate(Critter *c, CritterMove *move)
{
    f32 forward[3];
    f32 delta[3];
    f32 speed;
    f32 length;

    speed = *(f32 *)((u8 *)c->hdr + 0xAC);
    if (speed <= 0.0f) {
        return 0;
    }
    speed *= *(f32 *)((u8 *)gCurLevel + 0xB0) *
             move->readyDistance * gClockFrameStep;
    if (move->type == 0x38) {
        forward[0] = c->targetPos[0] - c->vel[0];
        forward[1] = c->targetPos[1] - c->vel[1];
        forward[2] = c->targetPos[2] - c->vel[2];
        NormalVector2D(forward);
    } else if ((*(u32 *)((u8 *)c->hdr + 0x5C) & 0x40) != 0) {
        forward[0] = c->worldMoveMatrix[8];
        forward[1] = 0.0f;
        forward[2] = c->worldMoveMatrix[10];
        NormalVector2D(forward);
    } else {
        forward[0] = c->mtx[2][0];
        forward[1] = 0.0f;
        forward[2] = c->mtx[2][2];
        NormalVector2D(forward);
    }

    switch (move->type) {
    case 0x32:
        delta[0] = -forward[2] * speed;
        delta[2] = forward[0] * speed;
        break;
    case 0x33:
        delta[0] = forward[2] * speed;
        delta[2] = -forward[0] * speed;
        break;
    case 0x35:
        delta[0] = -forward[0] * speed;
        delta[2] = -forward[2] * speed;
        break;
    case 0x36:
        delta[0] = (forward[0] - forward[2]) * speed;
        delta[2] = (forward[0] + forward[2]) * speed;
        break;
    default:
        delta[0] = forward[0] * speed;
        delta[2] = forward[2] * speed;
        break;
    }
    delta[1] = 0.0f;
    delta[0] += c->knockbackVelocity[0] * gClockFrameStep;
    delta[1] += c->knockbackVelocity[1] * gClockFrameStep;
    delta[2] += c->knockbackVelocity[2] * gClockFrameStep;
    c->knockbackVelocity[0] *= 0.8f;
    c->knockbackVelocity[1] *= 0.8f;
    c->knockbackVelocity[2] *= 0.8f;
    length = NormalVector(delta);
    if (length > speed * 2.0f && speed > 0.0f) {
        delta[0] *= speed * 2.0f;
        delta[1] *= speed * 2.0f;
        delta[2] *= speed * 2.0f;
    } else {
        delta[0] *= length;
        delta[1] *= length;
        delta[2] *= length;
    }
    c->vel[0] += delta[0];
    c->vel[1] += delta[1];
    c->vel[2] += delta[2];
    return 0;
}
/* 0x8003AF4C -- turn the critter toward the move-selected player, waypoint
 * or explicit facing, clamping angular speed by the move and header rates. */
void CritterRotate(Critter *c, CritterMove *move)
{
    f32 wanted;
    f32 current;
    f32 delta;
    f32 step;
    f32 target[3];

    current = *(f32 *)((u8 *)c + 0xFC);
    wanted = current;
    if ((move->flags & 0x20) != 0) {
        wanted = *(f32 *)((u8 *)c + 0xF8);
    } else if (c->unk128 < 0) {
        if (c->unk124 >= 0 && *(f32 *)((u8 *)move + 0x88) > 0.0f) {
            memcpy(target, (u8 *)&gPlayers[c->unk124] + 0x64,
                   sizeof(target));
            wanted = (f32)atan2(target[0] - c->vel[0],
                                target[2] - c->vel[2]);
        } else if (c->particle != NULL && c->targetCount == 0) {
            target[0] = *(f32 *)((u8 *)c->particle + 0x30) - c->vel[0];
            target[2] = *(f32 *)((u8 *)c->particle + 0x38) - c->vel[2];
            wanted = (f32)atan2(target[0], target[2]);
        }
    }

    delta = wanted - current;
    while (delta > 3.1415927f) {
        delta -= 6.2831855f;
    }
    while (delta < -3.1415927f) {
        delta += 6.2831855f;
    }
    step = *(f32 *)((u8 *)move + 0x88) * gClockFrameStep;
    if (c->unkAC6 > 0) {
        step *= 0.5f;
    }
    if (delta > step) {
        delta = step;
    } else if (delta < -step) {
        delta = -step;
    }
    *(f32 *)((u8 *)c + 0xFC) = current + delta;
    CopyMat3((f32 *)gIdentityMatrix, &c->mtx[0][0]);
    YawMat3(*(f32 *)((u8 *)c + 0xFC), &c->mtx[0][0]);
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
            asm { b store_move_node }
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
/* 0x8003B300 -- activate frame-gated move actions, sounds and particles. */
void CritterActivate(Critter *c, CritterMove *move, s32 frame)
{
    u32 events;
    u16 oldFlags;

    oldFlags = (u16)c->moveFlags;
    events = CritterCopyAnim(c, move, frame);
    if ((events & 1) != 0) {
        if (move->type != 0x85) {
            c->moveFlags |= 1;
        }
        if (*(s16 *)((u8 *)move + 0x48) >= 0) {
            CritterAnimInterrupt(c, *(s16 *)((u8 *)move + 0x48), 1,
                                 (oldFlags & 1) == 0);
        }
    }
    if ((events & 2) != 0) {
        if (move->type != 0x85) {
            c->moveFlags |= 2;
        }
        if (*(s16 *)((u8 *)move + 0x4A) >= 0) {
            CritterAnimInterrupt(c, *(s16 *)((u8 *)move + 0x4A), 2,
                                 (oldFlags & 2) == 0);
        }
    }
    if ((c->moveSfxFlags & 1) == 0 &&
        *(s16 *)((u8 *)move + 0x58) >= 0 &&
        frame >= *(s16 *)((u8 *)move + 0x5A)) {
        c->moveSfxFlags |= 1;
        CritterDoSfx(c, *(s16 *)((u8 *)move + 0x58), NULL, 1, -1);
    }
    if ((c->moveSfxFlags & 2) == 0 &&
        *(s16 *)((u8 *)move + 0x5C) >= 0 &&
        frame >= *(s16 *)((u8 *)move + 0x5E)) {
        c->moveSfxFlags |= 2;
        CritterDoSfx(c, *(s16 *)((u8 *)move + 0x5C), NULL, 1, -1);
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

    c->nextmove = -1;
    if (c->state == 1) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x11, 1);
    } else if (c->curmove >= 0) {
        move = &(*(CritterMove **)((u8 *)c->hdr + 0x124))[c->curmove];
        if (move->link >= 0) {
            c->nextmove = move->link;
        }
    }
    if (c->nextmove < 0 && (c->counterState & 0x120) != 0) {
        if ((c->counterState & 0x100) != 0) {
            c->nextmove = (s16)CritterFindMoveType(c, 0x42, 0);
        }
        if (c->nextmove < 0) {
            c->nextmove = (s16)CritterFindMoveType(c, 0x41, 0);
        }
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

    aiType = *(s16 *)(*(u8 **)((u8 *)c->hdr + 0x120) + 0x20);
    move = c->curmove >= 0
               ? &(*(CritterMove **)((u8 *)c->hdr + 0x124))[c->curmove]
               : NULL;
    c->nextmove = -1;
    if (c->curmove < 0 || c->state == 0 || c->state == 2) {
        c->nextmove = (s16)CritterFindMoveType(c, 0, 1);
    } else if (move->type == 0) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x10, 0);
    } else if (c->state == 1) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x11, 1);
    } else if (move->link >= 0) {
        c->nextmove = move->link;
    } else if (aiType == 4 && move->type == 0x10) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x20, 0);
    } else {
        c->nextmove = (s16)CritterFindMoveType(c, 0x20, 1);
    }

    if (c->nextmove < 0 && (c->counterState & 0x120) != 0) {
        if ((c->counterState & 0x100) != 0) {
            c->nextmove = (s16)CritterFindMoveType(c, 0x42, 0);
        }
        if (c->nextmove < 0) {
            c->nextmove = (s16)CritterFindMoveType(c, 0x41, 0);
        }
    }
    if (c->nextmove < 0 && (c->counterState & 0x10) != 0) {
        c->nextmove = (s16)CritterFindMoveType(c, 0x40, 0);
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

/* 0x8003C11C -- convert move frame windows into the two activation edges
 * consumed by CritterActivate. */
u32 CritterCopyAnim(Critter *c, CritterMove *move, s32 frame)
{
    u32 result;
    s32 first;
    s32 second;

    result = 0;
    first = *(s32 *)((u8 *)move + 0x40);
    second = *(s32 *)((u8 *)move + 0x44);
    if (first >= 0 && frame >= first &&
        frame <= *(s16 *)((u8 *)move + 0x50)) {
        result |= 1;
    }
    if (second >= 0 && frame >= second &&
        frame <= *(s16 *)((u8 *)move + 0x52)) {
        result |= 2;
    }
    if (move->type == 0x88 && (c->moveFlags & 1) == 0 &&
        frame >= first && c->unk124 >= 0) {
        memcpy(c->targetPos, (u8 *)&gPlayers[c->unk124] + 0x64,
               sizeof(c->targetPos));
        result |= 1;
    }
    return result;
}

/* 0x8003C40C -- select/blend the active sequence, animate auxiliary trees,
 * and hand completed moves to CritterMoveDone. */
void CritterAnimate(Critter *c)
{
    CritterMove *moves;
    CritterMove *current;
    CritterMove *next;
    u8 *subnode;
    s32 nextIndex;
    s32 sequence;
    s32 transition;
    u16 done;

    moves = *(CritterMove **)((u8 *)c->hdr + 0x124);
    nextIndex = c->nextmove;
    current = c->curmove >= 0 ? &moves[c->curmove] : NULL;
    next = nextIndex >= 0 ? &moves[nextIndex] : NULL;
    if (next == NULL) {
        next = current;
        nextIndex = c->curmove;
    }
    if (next == NULL) {
        return;
    }
    sequence = next->anim;
    transition = current == NULL ? 3 : CritterGetDmove(current, next);
    if (c->rate > sMusicFadeBase && current != NULL) {
        sequence = current->anim;
        transition = 0;
        nextIndex = c->curmove;
    }

    if (c->pausecnt <= 0) {
        done = AnimateATree(&c->colhandle, sequence, transition);
    } else {
        c->animtimer += gClockFrameStep;
        done = 0;
    }
    for (subnode = (u8 *)c->subnodes;
         subnode != NULL; subnode = *(u8 **)(subnode + 0x50)) {
        AnimateATree(subnode, sequence, transition);
    }
    c->movedone = (s16)(done & 3);
    if ((done & 3) != 0) {
        CritterMoveDone(c, nextIndex);
    } else if (nextIndex < 0 && AnimDone(&c->sound[0])) {
        c->curmove = -1;
    }
}

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
s32 CritterFindMoveType(Critter *c, s32 type, s32 mode)
{
    u8 *hdr;
    s32 timeOffset;
    s32 moveOffset;
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
        CritterMove *move = (CritterMove *)(*(u8 **)(hdr + 0x124) + moveOffset);
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
/* 0x8003CA98 -- dispatch one move action descriptor on activation or release. */
void CritterAnimInterrupt(Critter *c, s32 action, s32 phase, s32 active)
{
    u8 *desc;
    s16 type;

    if (action < 0) {
        return;
    }
    desc = *(u8 **)(*(u8 **)((u8 *)c->hdr + 0x130) + 0x44) +
           action * 0x50;
    type = *(s16 *)desc;
    switch (type) {
    case 0:
    case 4:
        if (active) {
            CritterDoTexmodNode(c, action, 1, NULL);
        }
        break;
    case 1:
        if (active) {
            CritterDoTexmodNode(c, action, 0, c->moveOrigin);
        }
        break;
    case 2:
    case 3:
        if (active) {
            CritterDoTexmodNode(c, action, 1, NULL);
        }
        break;
    case 7:
        if (phase == 1 && c->unk128 < 0) {
            c->unk128 = (s16)CritterGetTargetSub(c, (f32 *)(desc + 0x20), 0);
            if (active) {
                CritterDoTexmodNode(c, action, 0, c->moveOrigin);
            }
        } else if (phase == 2) {
            c->unk128 = -1;
        }
        break;
    case 8:
        if (active) {
            CritterDoTexmodNode(c, action, 0, c->targetPos);
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
void CritterDoTexmodNode(Critter *c, s32 action, s32 local, f32 *position)
{
    u8 *container;
    u8 *desc;
    f32 offset[3];
    f32 world[3];
    s32 sfx;

    container = *(u8 **)((u8 *)c->hdr + 0x130);
    desc = *(u8 **)(container + 0x44) + action * 0x50;
    offset[0] = *(f32 *)(desc + 0x20);
    offset[1] = *(f32 *)(desc + 0x24);
    offset[2] = *(f32 *)(desc + 0x28);
    if (local) {
        MulVec4Mat3(offset, world, &c->mtx[0][0]);
        if (position != NULL) {
            world[0] += position[0];
            world[1] += position[1];
            world[2] += position[2];
        } else {
            world[0] += c->vel[0];
            world[1] += c->vel[1];
            world[2] += c->vel[2];
        }
    } else {
        world[0] = offset[0] + (position != NULL ? position[0] : c->vel[0]);
        world[1] = offset[1] + (position != NULL ? position[1] : c->vel[1]);
        world[2] = offset[2] + (position != NULL ? position[2] : c->vel[2]);
    }
    sfx = *(s16 *)(desc + 0x40);
    if (sfx >= 0) {
        CritterDoSfx(c, sfx, world, local, -1);
    }
    if (*(s16 *)(desc + 0x42) >= 0) {
        CritterDoSfx(c, *(s16 *)(desc + 0x42), world, local, -1);
    }
}
/* 0x8003D7E0 */
s32 CritterDoSfx(Critter *c, s32 sfx, void *parent, s32 arg3, s32 arg4)
{
    u8 *entry;
    u32 flags;
    f32 position[3];
    s32 result;
    s32 i;

    if (sfx < 0) {
        return -1;
    }
    entry = *(u8 **)(*(u8 **)((u8 *)c->hdr + 0x130) + 0x4C) +
            sfx * 0x50;
    flags = *(u32 *)entry;
    result = -1;
    position[0] = *(f32 *)(entry + 0x30);
    position[1] = *(f32 *)(entry + 0x34);
    position[2] = *(f32 *)(entry + 0x38);

    if ((flags & 0x0F000000) != 0) {
        CritterDoParticle(c, entry, arg4);
    } else if ((flags & 0x100) != 0) {
        ProcessSkinFX((f32 *)((u8 *)c + 0xE0), c->anim,
                      *(void **)(entry + 8));
    } else if ((flags & 0x200) != 0) {
        for (i = 0; i < *(s16 *)((u8 *)c->hdr + 0x118); i++) {
            u8 *node = (u8 *)c + 0x4F8 + i * 0x5C;
            if ((*(u16 *)(*(u8 **)node + 0x10) & 1) == 0) {
                f32 nodePos[3];
                nodePos[0] = *(f32 *)(node + 0x3C) + position[0];
                nodePos[1] = *(f32 *)(node + 0x40) + position[1];
                nodePos[2] = *(f32 *)(node + 0x44) + position[2];
                result = CritterDoSfxSub(c, entry, nodePos, 0, flags);
            }
        }
    } else {
        f32 *world = parent != NULL ? (f32 *)parent : c->vel;
        position[0] += world[0];
        position[1] += world[1];
        position[2] += world[2];
        result = CritterDoSfxSub(c, entry, position, arg3, flags);
    }

    if ((flags & 0x40000) != 0) {
        if (c->unkABA < 0) {
            c->unkABA = (s16)result;
        } else {
            ErrorPrintf("Critter -> 1 killfx\n");
        }
    }
    if (*(s32 *)(entry + 4) >= 0) {
        CritterDoSfx(c, *(s32 *)(entry + 4), parent, arg3, result);
    }
    return result;
}

/* 0x8003DC64 -- create one ordinary effect and apply owner, parent, color
 * and scale properties encoded by the critter sound descriptor. */
s32 CritterDoSfxSub(Critter *c, void *sfx, f32 *position,
                    s32 parented, u32 flags)
{
    u8 *entry;
    u32 treeFlags;
    u32 effectFlags;
    s32 result;
    void *parent;

    entry = (u8 *)sfx;
    if (*(s32 *)(entry + 8) < 0) {
        return -1;
    }
    treeFlags = 0x800;
    effectFlags = 0;
    if ((flags & 4) != 0) {
        treeFlags = 0x80880;
    }
    if ((flags & 0x100000) != 0) treeFlags |= 0x80040;
    if ((flags & 0x1000) != 0) treeFlags |= 0x801000;
    if ((flags & 0x80000) != 0) treeFlags |= 0x40000000;
    if ((flags & 0x10) != 0) effectFlags |= 0x200000;
    if ((flags & 0x10000) != 0) effectFlags |= 0x400000;
    if ((flags & 0x20000) != 0) effectFlags |= 0x02000000;

    result = StartFXSub(*(s32 *)(entry + 8), position, effectFlags,
                        treeFlags, effectFlags, c->id);
    if (result < 0) {
        return result;
    }
    SfxSetOwner(result, (u16)c->id | 0x1000);
    if ((flags & 0x40) != 0) {
        SfxSetMat(result, (f32 *)c->mbnode, NULL);
    } else if (parented) {
        parent = (flags & 1) != 0 ? c->anim : c->obj_d0;
        if ((flags & 0x800) != 0 && c->anim != NULL) {
            parent = *(void **)((u8 *)c->anim + 0x74);
        }
        SfxSetParent(result, parent);
    }
    if (*(s32 *)(entry + 0x48) != -1 && c->mbnode != NULL) {
        MBTreeSetColor(c->mbnode, *(s32 *)(entry + 0x48), 1);
    }
    if (*(f32 *)(entry + 0x4C) != 1.0f && c->mbnode != NULL) {
        f32 scale = *(f32 *)(entry + 0x4C);
        MBTreeSetScale(scale, scale, scale, c->mbnode);
    }
    return result;
}

/* 0x8003DE70 -- create and configure a particle system from one descriptor. */
void CritterDoParticle(Critter *c, void *sfx, s32 node)
{
    u8 *entry;
    void *parent;
    void *psys;
    f32 matrix[12];
    f32 rate;

    entry = (u8 *)sfx;
    parent = c->obj_d0 != NULL ? c->obj_d0 : c->mbnode;
    if ((*(u32 *)entry & 0x4000) != 0 && node >= 0 &&
        node < *(s16 *)((u8 *)c->hdr + 0x118)) {
        parent = *(void **)((u8 *)c + 0x4FC + node * 0x5C);
    }
    memcpy(matrix, c->mtx, sizeof(matrix));
    psys = MBNewPsysDefault(matrix, parent, 0, 1);
    if (psys == NULL) {
        ErrorPrintf("Critter unable to generate psys\n");
        return;
    }
    MBPsysSetPTex(psys, *(s32 *)(entry + 8));
    rate = *(f32 *)(entry + 0x40) * 0.01f;
    MBPsysSetERate4(rate, rate, rate, rate, psys);
    MBPsysSetETime(*(f32 *)(entry + 0x3C), 0.01f, psys);
    MBPsysSetPSpeed(*(s16 *)(entry + 0x46) * 0.01f, psys);
}
/* 0x8003E048 -- allocate and initialize a root critter and the child chain
 * described by its loaded type header. */
Critter *CritterNewInst(s32 type, s32 subtype, void *object)
{
    Critter *root;
    Critter *tail;
    Critter *child;
    u8 *header;
    u8 *childHeader;
    s16 childIndex;

    if (type < 0 || type >= 9 || subtype < 0 || subtype >= 6) {
        ErrorPrintf("No Critter type %d subtype %d loaded\n", type, subtype);
        return NULL;
    }
    header = (u8 *)gCritterHeaders[type][subtype];
    if (header == NULL || *(void **)(header + 0x138) == NULL) {
        return NULL;
    }

    root = CritterEmptyInst();
    if (root == NULL) {
        return NULL;
    }
    CritterInitInst(root, (struct CritterHeader *)header);
    CritterInitGeo(root, object);
    CritterAddAnimInsts(root, &root->mtx[0][0]);
    CritterInitColnodes(root);
    CritterAddHealthMeter(root);

    root->alivecnt = 0;
    tail = root;
    childIndex = *(s16 *)(header + 0x11C);
    while (childIndex >= 0) {
        childHeader = *(u8 **)(*(u8 **)(header + 0x130) + 0x14) +
                      childIndex * 0x140;
        child = CritterEmptyInst();
        if (child == NULL) {
            break;
        }
        CritterInitInst(child, (struct CritterHeader *)childHeader);
        memcpy(child->mtx, root->mtx, sizeof(root->mtx));
        CritterInitGeo(child, NULL);
        CritterAddAnimInsts(child, &child->mtx[0][0]);
        CritterInitColnodes(child);
        CritterAddHealthMeter(child);

        child->parent = root;
        tail->next = child;
        tail = child;
        root->alivecnt++;
        childIndex = *(s16 *)(childHeader + 0x11C);
    }
    if (*(s16 *)(*(u8 **)(header + 0x120) + 0x20) == 8) {
        root->particle = NULL;
    }
    return root;
}
/* 0x8003E2E8 -- reserve the first free critter pool slot, wipe it, and stamp
 * it with a fresh index + rolling unique id. */
Critter *CritterEmptyInst(void)
{
    s32 i;
    s32 byte_offset;
    Critter *c;
    CritterBigState *big;

    big = &gBig;
    for (i = 0; i < lbl_8034466C; i++) {
        if (big->pool[i].hdr == NULL) {
            break;
        }
    }
    if (i >= 16) {
        ErrorPrintf(lbl_8011221C, i, lbl_8034466C);
        return NULL;
    }
    if (i == lbl_8034466C) {
        lbl_8034466C++;
        if (lbl_8034466C > gCritterCountMax) {
            gCritterCountMax = lbl_8034466C;
        }
    }
    byte_offset = i * sizeof(Critter);
    c = (Critter *)((u8 *)big + 0x234 + byte_offset);
    memset(c, 0, sizeof(Critter));
    c->index = (s16)i;
    *(s16 *)((u8 *)big + 0x236 + byte_offset) = gCritterNextID;
    if ((u16)(gCritterNextID = gCritterNextID + 1) > 4095) {
        gCritterNextID = 1;
    }
    return c;
}
/* 0x8003E3E8 -- instantiate the model/animation tree and cache the principal
 * scene nodes and world-space transforms used by movement and collision. */
void CritterInitGeo(Critter *c, void *object)
{
    u8 *header;
    void *atree;
    s16 nodeIndex;

    header = (u8 *)c->hdr;
    c->mbnode = MBNewNode(lbl_8034473C, &c->mtx[0][0], 1);
    c->animtimer = 0.0f;
    *(f32 *)((u8 *)c + 0xFC) = 0.0f;

    atree = AtreeInit(*(void **)(header + 0x138), &c->colhandle, 0,
                      (*(u32 *)(header + 0x5C) & 0x1000) == 0 ? 0x800 : 0);
    c->colhandle = atree;
    c->anim = atree != NULL ? *(void **)atree : NULL;
    if (c->anim != NULL) {
        MBNodeSetParent(c->anim, c->mbnode);
    }

    c->hitnode0 = NULL;
    nodeIndex = *(s16 *)(header + 0x56);
    if (nodeIndex >= 0 && c->anodes != NULL) {
        c->hitnode0 = *(void **)((u8 *)c->anodes + nodeIndex * 0x28);
    }
    c->hitnode1 = NULL;
    nodeIndex = *(s16 *)(header + 0x58);
    if (nodeIndex >= 0 && c->anodes != NULL) {
        c->hitnode1 = *(void **)((u8 *)c->anodes + nodeIndex * 0x28);
    }
    c->hitnode2 = NULL;
    nodeIndex = *(s16 *)(header + 0x5A);
    if (nodeIndex >= 0 && c->anodes != NULL) {
        c->hitnode2 = *(void **)((u8 *)c->anodes + nodeIndex * 0x28);
    }

    if (object != NULL) {
        memcpy(c->pos, (u8 *)object + 0x30, sizeof(c->pos));
    } else {
        c->pos[0] = c->mtx[0][3];
        c->pos[1] = c->mtx[1][3];
        c->pos[2] = c->mtx[2][3];
    }
    c->movevec[0] = c->pos[0];
    c->movevec[1] = c->pos[1] + *(f32 *)(header + 0xB4);
    c->movevec[2] = c->pos[2];
    c->obj_d0 = c->anim;
    if (c->obj_d0 != NULL) {
        GetWorldMat(c->obj_d0, c->worldMoveMatrix, NULL);
    } else {
        memcpy(c->worldMoveMatrix, c->mtx, sizeof(c->worldMoveMatrix));
    }
    memcpy(c->moveOrigin, c->pos, sizeof(c->moveOrigin));
    memcpy(c->moveMatrix, c->pos, sizeof(c->moveMatrix));
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
    s32 i;
    u8 *node;
    u32 savedFlags;

    savedFlags = 0;
    ProcessSkinFX((f32 *)((u8 *)c + 0xE0), c->anim, c->hitnode2);
    if (c->hitnode2 != NULL) {
        savedFlags = *(u32 *)((u8 *)c->hitnode2 + 0x60);
        *(u32 *)((u8 *)c->hitnode2 + 0x60) = savedFlags | 0x10;
    }

    for (i = 0; i < *(s16 *)((u8 *)c->hdr + 0x118); i++) {
        node = (u8 *)c + 0x4F8 + i * 0x5C;
        if (*(s32 *)(node + 0x50) > 0) {
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
        c->unkABC--;
        MBTreeSetAltTex(c->anim, 0xFFFFFFFC, lbl_80344BF8, 1);
        MBTreeSetAmbientAdd(c->anim, 0xFF, 1);
    } else if (c->unkABC == 0) {
        MBTreeSetAltTex(c->anim, 0xFFFFFFFF, 0, 1);
        MBTreeSetAmbientAdd(c->anim, 0, 1);
        c->unkABC = -1;
    }
    if (c->pausecnt > 0) {
        c->pausecnt -= (s16)gFrameTicks;
        if (c->pausecnt <= 0) {
            c->pausecnt = 0;
            MBTreeSetAltTex(c->anim, 0xFFFFFFFF, 0, 1);
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
    s32 i;
    s32 j;
    s32 animOffset;
    s32 moveOffset;
    s32 hitOffset;

    while (node != NULL) {
        if (node->child != NULL) {
            CritterRemoveColnodeSub(c, node->child, 2);
        }
        next = node->next;
        MBRemoveNode(node, 0);

        for (i = 0, animOffset = 0; i < c->anodeCount;
             i++, animOffset += sizeof(CritterAnimNode)) {
            if (*(CritterColnode **)((u8 *)c->anodes + animOffset) == node) {
                *(void **)((u8 *)c->anodes + animOffset + 0x20) = NULL;
                *(CritterColnode **)((u8 *)c->anodes + animOffset) = NULL;
                for (j = 0, moveOffset = 0;
                     j < *(s16 *)((u8 *)c->hdr + 0x110);
                     j++, moveOffset += sizeof(CritterMove)) {
                    if (*(s16 *)(*(u8 **)((u8 *)c->hdr + 0x124) +
                                 moveOffset + 0x0E) == i) {
                        *(s16 *)(*(u8 **)((u8 *)c->hdr + 0x124) +
                                 moveOffset + 0x0E) = -1;
                    }
                }
            }
        }

        for (i = 0, hitOffset = 0;
             i < *(s16 *)((u8 *)c->hdr + 0x118);
             i++, hitOffset += 0x5C) {
            if (*(void **)((u8 *)c + 0x4FC + hitOffset) == node) {
                *(void **)((u8 *)c + 0x4FC + hitOffset) = NULL;
            }
        }

        if (mode != 2) {
            break;
        }
        node = next;
    }
}
/* 0x8003EEF8 -- bind each collision descriptor to its animation node and
 * initialize its health, flash and optional damage-effect state. */
void CritterInitColnodes(Critter *c)
{
    u8 *header;
    u8 *descriptor;
    u8 *record;
    s32 i;
    s16 nodeIndex;

    header = (u8 *)c->hdr;
    if (*(s16 *)(header + 0x118) <= 0) {
        return;
    }
    descriptor = *(u8 **)(*(u8 **)(header + 0x130) + 0x3C) +
                 *(s16 *)(header + 0x11A) * 0x50;
    c->unkAB8 = -1;
    for (i = 0; i < *(s16 *)(header + 0x118); i++, descriptor += 0x50) {
        record = (u8 *)c + 0x4F8 + i * 0x5C;
        *(u8 **)record = descriptor;
        nodeIndex = *(s16 *)(descriptor + 0x14);
        if (nodeIndex < 0) {
            *(void **)(record + 4) = NULL;
        } else {
            *(void **)(record + 4) =
                *(void **)((u8 *)c->anodes + nodeIndex * 0x28);
            if (*(void **)(record + 4) == NULL) {
                *(void **)(record + 4) = c->anim;
            }
        }
        *(void **)(record + 8) = *(void **)(record + 4);
        MBTreeSetZsortAdd(*(void **)(record + 4),
                          *(s16 *)(descriptor + 0x16), 1);
        *(s32 *)(record + 0x50) = -1;
        *(f32 *)(record + 0x54) =
            *(f32 *)(descriptor + 0x44) * c->health;
        *(f32 *)(record + 0x58) = 0.0f;
    }
}

/* 0x8003F1F0 -- instantiate every auxiliary animation tree attached to a
 * critter type and link the allocated records into the instance. */
void CritterAddAnimInsts(Critter *c, f32 *matrix)
{
    u8 *definition;
    CritterSubnode *tail;
    CritterSubnode *record;
    void *parent;
    s32 slot;

    definition = *(u8 **)((u8 *)c->hdr + 0x134);
    while (definition != NULL) {
        record = NULL;
        for (slot = 0; slot < lbl_80344668; slot++) {
            CritterSubnode *candidate =
                (CritterSubnode *)(lbl_802411B0 + slot * 0x54);
            if (candidate->mbnode == NULL) {
                record = candidate;
                break;
            }
        }
        if (record == NULL && slot < 16) {
            record = (CritterSubnode *)(lbl_802411B0 + slot * 0x54);
            lbl_80344668 = slot + 1;
        }
        if (record == NULL) {
            ErrorPrintf("Too many Critter Anim Insts\n");
            return;
        }
        memset(record, 0, 0x54);
        if (c->subnodes == NULL) {
            c->subnodes = record;
        } else {
            tail = (CritterSubnode *)c->subnodes;
            while (tail->next != NULL) {
                tail = tail->next;
            }
            tail->next = record;
        }

        parent = c->anim;
        if ((*(u16 *)(definition + 2) & 1) != 0 &&
            *(char *)(definition + 0x18) != '\0') {
            void *found = AtreeFindNode(&c->colhandle,
                                        (char *)(definition + 0x18), 8);
            if (found != NULL) {
                parent = found;
            }
        }
        record->mbnode = MBNewNode(parent, matrix, 1);
        record->atree = AtreeInit(*(void **)(definition + 4), record, 0,
                                  0x800);
        if (record->atree != NULL) {
            MBNodeSetParent(*(void **)record->atree, record->mbnode);
        }
        definition = *(u8 **)(definition + 8);
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
    u8 *resource;

    resource = (u8 *)crit_load_desc;
    if (resource == NULL) {
        return 1;
    }
    if (*(s16 *)(resource + 0x24) != 1) {
        if (lbl_80344640 == NULL) {
            return 1;
        }
        if (lbl_80344640[4] == 0) {
            return 0;
        }
        lbl_80344640[4] = -1;
        *(s16 *)(resource + 0x24) = 3;
        return 1;
    }
    if (!MBOX_BGLoadModelDone()) {
        return 0;
    }
    *(s16 *)(resource + 0x24) = 2;
    /* The original begins the companion texture read here.  Keeping the
     * loader context live lets CritterBGLoadFile advance the same request. */
    if (maxBytes < 0) {
        maxBytes = 0;
    }
    (void)maxBytes;
    return 0;
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
void CritterLoadStartNext(void)
{
    s32 file;
    s32 type;
    u8 *container;
    u8 *header;
    u8 *resource;

    for (file = 0; file < lbl_80344660; file++) {
        container = lbl_80241070[file];
        if (*(s32 *)container != 1) {
            continue;
        }
        header = *(u8 **)(container + 0x14);
        for (type = 0; type < *(s32 *)(container + 0x10);
             type++, header += 0x140) {
            resource = *(u8 **)(header + 0x120);
            if (resource == NULL) {
                continue;
            }
            if (*(s16 *)(resource + 0x24) == 2) {
                return;
            }
            if (*(s16 *)(resource + 0x24) == 3) {
                CritterLoadFinish(header);
                continue;
            }
            if (*(s16 *)(resource + 0x24) == 0) {
                MBOX_BGLoadModelStart((char *)(resource + 0x10),
                                      *(s16 *)(resource + 0x22), resource);
                lbl_80344640 = NULL;
                crit_load_desc = resource;
                *(s16 *)(resource + 0x24) = 1;
                return;
            }
        }
        *(s32 *)container = 2;
    }
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
    u8 *container;
    u8 *typeHeader;
    u8 *resource;
    s16 type;
    s16 subtype;

    container = (u8 *)hdr;
    typeHeader = (u8 *)move;
    *(u8 **)(typeHeader + 0x130) = container;
    resource = *(u8 **)(container + 0x1C) +
               *(s16 *)(typeHeader + 0x50) * 0x30;
    *(u8 **)(typeHeader + 0x120) = resource;

    if (*(s16 *)(resource + 0x22) < 0) {
        if (arg == 0) {
            *(void **)(typeHeader + 0x138) = NULL;
        } else {
            CritterLoadFinish(typeHeader);
        }
    } else if (arg != 0) {
        CritterLoadFinish(typeHeader);
    }

    type = *(s16 *)(resource + 0x20);
    subtype = *(s16 *)(typeHeader + 0x52);
    if (type >= 0 && type < 9 && subtype >= 0 && subtype < 6) {
        gCritterHeaders[type][subtype] =
            (struct CritterHeader *)typeHeader;
    }
}

/* 0x8003F9F4 -- resolve the animation tree and named attachment nodes for a
 * type after its model resource has loaded. */
void CritterLoadFinish(void *typeHeaderPtr)
{
    u8 *header;
    u8 *resource;
    u8 *parent;
    u8 *attachment;
    void *atree;
    s32 index;

    header = (u8 *)typeHeaderPtr;
    if (*(void **)(header + 0x138) != NULL) {
        return;
    }
    resource = *(u8 **)(header + 0x120);
    if (*(s16 *)(header + 0x11E) < 0) {
        *(void **)(header + 0x138) =
            AtreeMatch(*(void **)(resource + 0x28),
                       (char *)(resource + 0x10), 0);
        if (*(void **)(header + 0x138) == NULL) {
            ErrorPrintf("Critter can not find atree %s\n",
                        (char *)(resource + 0x10));
        }
    } else {
        parent = *(u8 **)(*(u8 **)(header + 0x130) + 0x14) +
                 *(s16 *)(header + 0x11E) * 0x140;
        *(void **)(header + 0x138) = *(void **)(parent + 0x138);
    }

    for (attachment = *(u8 **)(header + 0x134);
         attachment != NULL;
         attachment = *(u8 **)(attachment + 8)) {
        *(void **)(attachment + 4) =
            AtreeMatch(*(void **)(resource + 0x28),
                       (char *)(attachment + 0x10), 1);
    }

    atree = *(void **)(header + 0x138);
    index = -1;
    if (atree != NULL && *(char *)(header + 0x20) != '\0') {
        index = AtreeFindNodeIdx(atree, *(s32 *)((u8 *)atree + 0x10),
                                 (char *)(header + 0x20), 0x10);
    }
    *(s16 *)(header + 0x56) = (s16)index;
    index = -1;
    if (atree != NULL && *(char *)(header + 0x30) != '\0') {
        index = AtreeFindNodeIdx(atree, *(s32 *)((u8 *)atree + 0x10),
                                 (char *)(header + 0x30), 0x10);
    }
    *(s16 *)(header + 0x58) = (s16)index;
    index = -1;
    if (atree != NULL && *(char *)(header + 0x40) != '\0') {
        index = AtreeFindNodeIdx(atree, *(s32 *)((u8 *)atree + 0x10),
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
void CritterInitMoves(void *move)
{
    u8 *header;
    u8 *container;
    u8 *entry;
    u8 *colnode;
    void *atree;
    s32 i;
    s32 index;

    header = (u8 *)move;
    atree = *(void **)(header + 0x138);
    container = *(u8 **)(header + 0x130);
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

    entry = *(u8 **)(container + 0x2C) +
            *(s16 *)(header + 0x112) * 0x90;
    for (i = 0; i < *(s16 *)(header + 0x110); i++, entry += 0x90) {
        if (*(s16 *)(entry + 0x0C) < 0 && *(char *)(entry + 0x20) != '\0') {
            index = AtreeHeaderFindSeq(atree, (char *)(entry + 0x20));
            *(s16 *)(entry + 0x0C) = (s16)(index < 0 ? 0 : index);
        }
        if (*(s16 *)(entry + 0x0E) < 0 &&
            *(char *)(entry + 0x30) != '\0') {
            *(s16 *)(entry + 0x0E) =
                (s16)AtreeFindNodeIdx(atree,
                                      *(s32 *)((u8 *)atree + 0x10),
                                      (char *)(entry + 0x30), 0x10);
        }
        CritterInitSfx(container, *(s16 *)(entry + 0x58),
                       *(void **)(*(u8 **)(header + 0x120) + 0x28));
        CritterInitSfx(container, *(s16 *)(entry + 0x5C),
                       *(void **)(*(u8 **)(header + 0x120) + 0x28));
    }

    colnode = *(u8 **)(container + 0x3C) +
              *(s16 *)(header + 0x11A) * 0x50;
    for (i = 0; i < *(s16 *)(header + 0x118);
         i++, colnode += 0x50) {
        index = -1;
        if (*(char *)colnode != '\0') {
            index = AtreeFindNodeIdx(atree,
                                     *(s32 *)((u8 *)atree + 0x10),
                                     (char *)colnode, 0x10);
        }
        *(s16 *)(colnode + 0x14) = (s16)index;
    }
    *(u8 **)(header + 0x124) =
        *(u8 **)(container + 0x2C) + *(s16 *)(header + 0x112) * 0x90;
    *(u8 **)(header + 0x12C) = colnode -
        *(s16 *)(header + 0x118) * 0x50;
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
extern void FatalError(const char *msg, int code);

static inline s32 CritterWadTag(char *s)
{
    return (s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3];
}

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
        swapped = MBSetupWad(wad = header->wad, (s32)file);
        header->sfx = (u8 *)MBGetFromWad(wad,
                                         CritterWadTag(lbl_8034665C),
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
            for (j = 0; j < 12; j += 4) {
                *(f32 *)(p + 0x30 + j) = CritterSwapF(*(f32 *)(p + 0x30 + j));
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
            for (j = 0; j < 12; j += 4) {
                *(f32 *)(p + 0x20 + j) = CritterSwapF(*(f32 *)(p + 0x20 + j));
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
            for (j = 0; j < 16; j += 2) {
                *(u16 *)(p + 0x20 + j) = CritterSwap16(*(u16 *)(p + 0x20 + j));
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
            for (j = 0; j < 12; j += 4) {
                *(f32 *)(p + 0x20 + j) = CritterSwapF(*(f32 *)(p + 0x20 + j));
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
            for (j = 0; j < 12; j += 4) {
                *(f32 *)(p + 0xA0 + j) = CritterSwapF(*(f32 *)(p + 0xA0 + j));
                *(f32 *)(p + 0xC0 + j) = CritterSwapF(*(f32 *)(p + 0xC0 + j));
                *(f32 *)(p + 0xD0 + j) = CritterSwapF(*(f32 *)(p + 0xD0 + j));
                *(f32 *)(p + 0x100 + j) = CritterSwapF(*(f32 *)(p + 0x100 + j));
            }
        }

        for (i = 0; i < header->addAnimCount; i++) {
            p = header->addAnims + i * 0x30;
            *(u16 *)(p + 0x00) = CritterSwap16(*(u16 *)(p + 0x00));
            *(u16 *)(p + 0x02) = CritterSwap16(*(u16 *)(p + 0x02));
            *(u32 *)(p + 0x04) = CritterSwap32(*(u32 *)(p + 0x04));
            *(u32 *)(p + 0x08) = CritterSwap32(*(u32 *)(p + 0x08));
            for (j = 0; j < 12; j += 4) {
                *(f32 *)(p + 0x20 + j) = CritterSwapF(*(f32 *)(p + 0x20 + j));
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
