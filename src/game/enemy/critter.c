/*
 * critter.c -- GCN CRITTER.OBJ.
 *
 * The critter behavior/load object between CONTROLS.OBJ and the sound-manager
 * object.  Critters are the large, scripted, multi-part creatures (golems,
 * bosses, generals) distinct from the swarm-style Enemy record.
 *
 * Bodies are transcribed from the GC (GUNE5D) DOL asm (tools/gdl/fnasm.py) with
 * Ghidra structure hints; function names are the pre-mapped CRITTER.OBJ roster.
 * Functions not yet reconstructed are documented empty skeletons with real
 * signatures (they keep the TU compiling; NonMatching does not link).
 *
 * .text       0x80034CFC..0x8004229C
 * extab       0x80005CE0..0x80005F28
 * extabindex  0x800093A0..0x8000970C
 */
#include "types.h"
#include "game/critter.h"
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
extern f32   lbl_80344590;            /* 0x80344590 frame delta                     */
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

/* -- external helpers -- */
extern void *AllocFile(const char *wad, const char *name);
extern void *NextWaypoint(void *player);
extern void  AddExp(s32 player, s32 amount, s32 kind);
extern void  HealthMeterUpdate(void *meter, f32 cur, f32 max);
extern void *memset(void *dst, int c, u32 n);
extern void  ErrorPrintf(const char *fmt, ...);
extern void  MBRemoveNode(void *node, s32 kind);
extern s32   GetWorldMat(void *node, f32 *matrix, f32 *offset);
extern s32   HealthMeterStart(void *header, s32 x, s32 y, s32 width,
                              s32 height, s32 style, f32 health);
extern void *AtreeMatch(void *header, const char *name, s32 report);
extern void *AtreeInit(void *header, void *tree, s32 flags, s32 size);
extern void  MBNodeSetParent(void *node, void *parent);
extern void  MBTreeSetFlags(void *node, u32 flags, s32 mode);
extern void *AtreeFindNode(void *tree, const char *name, s32 length);
extern void  SfxDeleteParented(void *sfx, s32 a, s32 b);
extern void  BossDeath(void);
extern void  fn_8002C49C(void *mtx);
extern void  AtreeDelete(void *handle);
extern s32   CollectSafeRocks(s32 *out, s32 max, s32 flags);
extern void  SafeRockActivate(s32 index);
extern u32   RandInt(u32 limit);
extern void  damage_player(s32 player, f32 damage, s32 mode, u32 flags,
                           f32 *direction);
extern f32   NormalVector(f32 *vector);
extern f32   NormalVector2D(f32 *vector);
extern s32   PlayerAttacking(s32 player, s32 mode);
extern char  lbl_8011221C[];          /* 0x8011221C critter-overflow message      */
extern const char lbl_80112238[];
DECL_SECT(".sdata2") extern const char lbl_80346644[];
extern void *gCurLevel;               /* current level record (->0xAC hp scale)   */
extern char  lbl_8011219C[];          /* move-type lookup failure message          */

/* -- CRITTER.OBJ internal roster (forward declarations) -- */
void fn_80034CFC(void);
void fn_80034F60(void);
void fn_800351B0(void);
void fn_80035408(void);
void fn_800358B0(void);
void fn_800359F0(void);
void fn_80035BC8(void);
void fn_80035D08(void);
void fn_80035E48(void);
void fn_80036138(void);
void fn_80036424(void);
void CritterAwardExp(s32 who, f32 amount);
struct CritterDamageDef;
void CritterDamagePlayer(Player *player, Critter *c,
                         struct CritterDamageDef *damageDef, u32 flags,
                         f32 *direction, s32 playSfx);
void CritterSetFxHitTime(s32 slot, s32 id, f32 amount);
s32  CritterGetTarget(Critter *c, f32 *out);
s32  CritterGetTargetSub(Critter *c, f32 *target, s32 mode);
void fn_80036B5C(void);
void fn_80036C70(void);
void fn_80036E00(void);
void fn_80036FBC(void);
struct CritterTargetState;
struct CritterTargetRecord;
void CritterInsertTarget(struct CritterTargetState *state,
                         struct CritterTargetRecord *target);
f32  fn_800372A0(Critter *c, f32 *moveTarget, f32 *target, s32 mode);
void fn_800374FC(void);
void fn_80037734(void);
void fn_800378C8(void);
void fn_80037A10(void);
s32  CritterLineNodeColSub(Critter *c, f32 *origin, f32 *forward,
                           f32 *delta, f32 radius, f32 dotThreshold);
void fn_80037D34(s32 unused, void *ctx);
s32  fn_80037D44(Critter *c, s32 id);
s32  fn_80037E80(Critter *c, s32 id);
void fn_80037ED0(f32 add, Critter *c, s32 id);
void fn_80037F84(void);
void fn_800380F0(void);
void CritterDamage(void);
s32  ProcessCritter(Critter *c);
s32  ProcessCritterList(void);
void CritterDoKnockback(Critter *c);
void CritterUpdateCounters(Critter *c);
void CritterGolemAI(void);
void CritterBossAI(void);
void CritterProcessSafeRocks(void);
void fn_8003A838(void);
void fn_8003A9C4(void);
void CritterRotate(void);
s32  fn_8003B1CC(Critter *c, CritterMove *move);
void fn_8003B300(void);
void CritterGetNextMove(void);
void CritterLookForReady(Critter *c);
void CritterChildCriticalMove(Critter *c);
void CritterLookForCriticalMove(Critter *c);
void fn_8003BC28(void);
void fn_8003BDF4(void);
void fn_8003C11C(void);
void CritterAnimate(void);
void CritterMoveDone(void);
s32  fn_8003C8D4(CritterMove *a, CritterMove *b);
s32  CritterFindMoveType(Critter *c, s32 type, s32 mode);
void fn_8003CA98(void);
void fn_8003D0A4(void);
void CritterDoSfx(Critter *c, s32 sfx, void *parent, s32 arg3, s32 arg4);
void CritterDoSfxSub(void);
void CritterDoParticle(void);
void CritterNewInst(void);
void CritterInitGeo(void);
void CritterAddHealthMeter(Critter *c);
void CritterInitInst(Critter *c, struct CritterHeader *hdr);
Critter *CritterEmptyInst(void);
void CritterDelInst(Critter *c);
void CritterUpdateSkinfx(void);
struct CritterColnode;
void CritterRemoveColnodeSub(Critter *c, struct CritterColnode *node, s32 mode);
void CritterInitColnodes(void);
void CritterAddAnimInsts(void);
s32  CritterLoadFile(const char *wad, const char *name);
void CritterLoadDone(void);
void CritterBGLoadFile(s32 *loader);
void CritterLoadStartNext(void);
void CritterLoadAllTypes(s32 arg);
struct CritterHeader *CritterTypeLoaded(s32 type, s32 subtype);
void CritterAllocType(void *hdr, void *move, s32 arg);
void CritterLoadFinish(void);
void CritterInitAllMoves(void);
void CritterInitMoves(void *move);
void CritterInitSfx(void);
void CritterInitHeader(void *hdr, void *file);

/* ==================================================================== */

/* 0x80034CFC */ void fn_80034CFC(void) {}
/* 0x80034F60 */ void fn_80034F60(void) {}
/* 0x800351B0 */ void fn_800351B0(void) {}
/* 0x80035408 */ void fn_80035408(void) {}
/* 0x800358B0 */ void fn_800358B0(void) {}
/* 0x800359F0 */ void fn_800359F0(void) {}
/* 0x80035BC8 */ void fn_80035BC8(void) {}
/* 0x80035D08 */ void fn_80035D08(void) {}
/* 0x80035E48 */ void fn_80035E48(void) {}
/* 0x80036138 */ void fn_80036138(void) {}
/* 0x80036424 */ void fn_80036424(void) {}
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

    *(f32 *)((u8 *)&gPlayers[playerIndex] + 0x914) = 0.0f;
    *(f32 *)((u8 *)&gPlayers[playerIndex] + 0x8E8) =
        (f32)(lbl_80346500 + (f64)sMusicFadeBase);

    *(f32 *)((u8 *)c + playerIndex * 0x10 + 0x1BC) += damage;
    *(f32 *)((u8 *)c + playerIndex * 0x10 + 0x1C0) = sMusicFadeBase;
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
        f32 dy;
        f32 x;
        f32 dx;
        f32 dz;
        f32 distance;

        dy = *(f32 *)((u8 *)waypoint + 0x34) - c->vel[1];
        x = *(f32 *)((u8 *)waypoint + 0x30);
        dx = x - c->vel[0];
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
    return 0;
}
#pragma dont_inline off
/* 0x80036B5C */ void fn_80036B5C(void) {}
/* 0x80036C70 */ void fn_80036C70(void) {}
/* 0x80036E00 */ void fn_80036E00(void) {}
/* 0x80036FBC */ void fn_80036FBC(void) {}
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
/* 0x800372A0 */ f32 fn_800372A0(Critter *c, f32 *moveTarget,
                                 f32 *target, s32 mode)
{
    return 0.0f;
}
#pragma dont_inline off
/* 0x800374FC */ void fn_800374FC(void) {}
/* 0x80037734 */ void fn_80037734(void) {}
/* 0x800378C8 */ void fn_800378C8(void) {}
/* 0x80037A10 */ void fn_80037A10(void) {}
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
void fn_80037D34(s32 unused, void *ctx)
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
s32 fn_80037D44(Critter *c, s32 id)
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
s32 fn_80037E80(Critter *c, s32 id)
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
/* 0x80037F84 */ void fn_80037F84(void) {}
/* 0x800380F0 */ void fn_800380F0(void) {}
/* 0x800383A8 */ void CritterDamage(void) {}
/* 0x80038D18 -- per-frame critter list step: reset per-player scratch, count
 * active players, then process every live critter, summing their results. */
s32 ProcessCritterList(void)
{
    Player *player = gPlayers;
    s32 activePlayers;
    s32 i;
    s32 total;

    activePlayers = 0;
    total = 0;
    lbl_80344664++;
    for (i = 0; i < 4; i++, player++) {
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
/* 0x80038DDC */ s32 ProcessCritter(Critter *c) { return 0; }
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
/* 0x800396A4 */ void CritterGolemAI(void) {}
/* 0x80039AD8 */ void CritterBossAI(void) {}
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
                big->safeRockTimers[i] -= lbl_80344590;
                if ((f64)big->safeRockTimers[i] <= 0.0) {
                    SafeRockActivate(big->safeRockIndices[i]);
                }
            }
        }
    }
}
/* 0x8003A838 */ void fn_8003A838(void) {}
/* 0x8003A9C4 */ void fn_8003A9C4(void) {}
/* 0x8003AF4C */ void CritterRotate(void) {}
/* 0x8003B1CC -- select the critter's current target/node and refresh the
 * world-space movement matrix used by the active move. */
s32 fn_8003B1CC(Critter *c, CritterMove *move)
{
    f32 *target;
    void *node;
    void *candidate;
    s32 nodeIndex;

    target = NULL;
    if (c->curmove >= 0) {
        target = (f32 *)(*(u8 **)((u8 *)c->hdr + 0x124) +
                         c->curmove * sizeof(CritterMove) + 0x60);
    }

    if (c->unk124 < 0 || c->movedone != 0) {
        if (c->unkAC6 > 0) {
            c->unk124 = -1;
        } else if (c->unk126 >= 0) {
            c->unk124 = c->unk126;
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
        } else {
            candidate = ((void **)((u8 *)c->anodes +
                                   nodeIndex * 0x28))[0];
            if (candidate == NULL) {
                candidate = node;
            }
            node = candidate;
        }
        c->obj_d0 = node;
    }

    c->moveMatrix[0] = c->moveOrigin[0];
    c->moveMatrix[1] = c->moveOrigin[1];
    c->moveMatrix[2] = c->moveOrigin[2];
    return GetWorldMat(c->obj_d0, c->worldMoveMatrix, NULL);
}
/* 0x8003B300 */ void fn_8003B300(void) {}
/* 0x8003B4CC */ void CritterGetNextMove(void) {}
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

        distance = fn_800372A0(c, (f32 *)((u8 *)move + 0x60),
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

    patterns = *(CritterPattern **)((u8 *)c->hdr + 0x128);
    i = 0;
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
                     fn_8003C8D4(&moves[moveChoice], move) > 1)) {
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
/* 0x8003BC28 */ void fn_8003BC28(void) {}
/* 0x8003BDF4 */ void fn_8003BDF4(void) {}
/* 0x8003C11C */ void fn_8003C11C(void) {}
/* 0x8003C40C */ void CritterAnimate(void) {}
/* 0x8003C6FC */ void CritterMoveDone(void) {}

/* 0x8003C8D4 -- classify two critters' facing/positions into a 0/1/2 code by
 * the relation encoded in a->curmove (0x56). */
s32 fn_8003C8D4(CritterMove *a, CritterMove *b)
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
    CritterMove *move;
    s32 timeOffset;
    s32 moveOffset;
    s32 i;
    s32 result;
    f32 best;
    f32 remaining;

    i = 0;
    timeOffset = 0;
    moveOffset = 0;
    result = -1;
    best = 0.0f;

    for (; i < *(s16 *)((u8 *)c->hdr + 0x110);
         i++, timeOffset += 4, moveOffset += sizeof(CritterMove)) {
        move = (CritterMove *)(*(u8 **)((u8 *)c->hdr + 0x124) + moveOffset);
        if ((move->flags & 4) == 0 && move->type == type) {
            if ((f64)move->cooldown > 0.0) {
                remaining =
                    *(f32 *)((u8 *)c + 0x218 + timeOffset) +
                    move->cooldown - sMusicFadeBase;
            } else {
                remaining = 0.0f;
            }
            if ((f64)remaining <= 0.0 || mode != 0) {
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
/* 0x8003CA98 */ void fn_8003CA98(void) {}
/* 0x8003D0A4 */ void fn_8003D0A4(void) {}
/* 0x8003D7E0 */
void CritterDoSfx(Critter *c, s32 sfx, void *parent, s32 arg3, s32 arg4) {}
/* 0x8003DC64 */ void CritterDoSfxSub(void) {}
/* 0x8003DE70 */ void CritterDoParticle(void) {}
/* 0x8003E048 */ void CritterNewInst(void) {}
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
/* 0x8003E3E8 */ void CritterInitGeo(void) {}
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
        fn_8002C49C(c->mtx);
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
/* 0x8003EB8C */ void CritterUpdateSkinfx(void) {}
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
/* 0x8003EEF8 */ void CritterInitColnodes(void) {}
/* 0x8003F1F0 */ void CritterAddAnimInsts(void) {}

/* 0x8003F3AC -- allocate a load slot, read the file, and build its header. */
s32 CritterLoadFile(const char *wad, const char *name)
{
    s32 idx;
    idx = lbl_80344660++;
    lbl_80241060[idx] = AllocFile(wad, name);
    CritterInitHeader(&lbl_80241070[idx], lbl_80241060[idx]);
    return idx;
}

/* 0x8003F414 */ void CritterLoadDone(void) {}

/* 0x8003F5B4 -- advance a background loader unless it has finished (state 2). */
void CritterBGLoadFile(s32 *loader)
{
    if (loader[4] == 2) {
        return;
    }
    loader[1] += loader[2];
}

/* 0x8003F5D4 */ void CritterLoadStartNext(void) {}

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

/* 0x8003F83C */ void CritterAllocType(void *hdr, void *move, s32 arg) {}
/* 0x8003F9F4 */ void CritterLoadFinish(void) {}

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

/* 0x8003FC4C */ void CritterInitMoves(void *move) {}
/* 0x8003FF98 */ void CritterInitSfx(void) {}
/* 0x800400F0 */ void CritterInitHeader(void *hdr, void *file) {}
