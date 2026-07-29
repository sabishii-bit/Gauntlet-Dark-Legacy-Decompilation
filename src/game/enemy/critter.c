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
    u8 _pad010[0x224];
    Critter pool[16];
} CritterBigState;

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
extern volatile f32 sMusicFadeBase;   /* 0x80344594 shared game-time / fade base   */
extern f32   lbl_80346480;
extern f64   lbl_80346488;
extern f64   lbl_80346490;

/* -- external helpers -- */
extern void *AllocFile(const char *wad, const char *name);
extern void *NextWaypoint(void *player);
extern void  AddExp(s32 player, s32 amount, s32 kind);
extern void  HealthMeterUpdate(void *meter, f32 cur, f32 max);
extern void *memset(void *dst, int c, u32 n);
extern void  ErrorPrintf(const char *fmt, ...);
extern void  MBRemoveNode(void *node, s32 kind);
extern void  SfxDeleteParented(void *sfx, s32 a, s32 b);
extern void  BossDeath(void);
extern void  fn_8002C49C(void *mtx);
extern void  AtreeDelete(void *handle);
extern char  lbl_8011221C[];          /* 0x8011221C critter-overflow message      */
extern void *gCurLevel;               /* current level record (->0xAC hp scale)   */

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
void fn_80036740(s32 who, f32 amount);
void fn_800367CC(void);
void CritterSetFxHitTime(s32 slot, s32 id, f32 amount);
s32  CritterGetTarget(Critter *c, f32 *out);
void CritterGetTargetSub(void);
void fn_80036B5C(void);
void fn_80036C70(void);
void fn_80036E00(void);
void fn_80036FBC(void);
void fn_800371BC(void *critter, f32 *pos);
void fn_800372A0(void);
void fn_800374FC(void);
void fn_80037734(void);
void fn_800378C8(void);
void fn_80037A10(void);
void fn_80037C08(void);
void fn_80037D34(s32 unused, void *ctx);
s32  fn_80037D44(Critter *c, s32 id);
s32  fn_80037E80(Critter *c, s32 id);
void fn_80037ED0(f32 add, Critter *c, s32 id);
void fn_80037F84(void);
void fn_800380F0(void);
void CritterDamage(void);
s32  ProcessCritter(Critter *c);
s32  ProcessCritterList(void);
void CritterDoKnockback(void);
void CritterUpdateCounters(Critter *c);
void CritterGolemAI(void);
void CritterBossAI(void);
void fn_8003A73C(void);
void fn_8003A838(void);
void fn_8003A9C4(void);
void CritterRotate(void);
void fn_8003B1CC(void);
void fn_8003B300(void);
void CritterGetNextMove(void);
void fn_8003B67C(void);
void fn_8003B7D8(void);
void fn_8003BAFC(void);
void fn_8003BC28(void);
void fn_8003BDF4(void);
void fn_8003C11C(void);
void CritterAnimate(void);
void CritterMoveDone(void);
s32  fn_8003C8D4(Critter *a, Critter *b);
void CritterFindMoveType(void);
void fn_8003CA98(void);
void fn_8003D0A4(void);
void CritterDoSfx(void);
void CritterDoParticle(void);
void fn_8003DE70(void);
void CritterNewInst(void);
void CritterInitGeo(void);
void CritterAddHealthMeter(void);
void CritterInitInst(Critter *c, struct CritterHeader *hdr);
Critter *CritterEmptyInst(void);
void CritterDelInst(Critter *c);
void CritterUpdateSkinfx(void);
void CritterRemoveColnodeSub(void);
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
void fn_8003FF98(void);
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
void fn_80036740(s32 who, f32 amount)
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
/* 0x800367CC */ void fn_800367CC(void) {}

/* 0x800368DC -- add `amount` to a per-limb counter of the critter whose id
 * matches `id`, then stamp the companion slot with the current game time. */
void CritterSetFxHitTime(s32 slot, s32 id, f32 amount)
{
    s32 i;
    Critter *c;
    for (i = 0; i < lbl_8034466C; i++) {
        c = &gCritterPool[i];
        if (c->hdr != NULL && id == c->id) {
            break;
        }
    }
    if (i >= lbl_8034466C) {
        return;
    }
    c->unk1BC[slot][0] += amount;
    c->unk1BC[slot][1] = sMusicFadeBase;
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
/* 0x80036A58 */ void CritterGetTargetSub(void) {}
/* 0x80036B5C */ void fn_80036B5C(void) {}
/* 0x80036C70 */ void fn_80036C70(void) {}
/* 0x80036E00 */ void fn_80036E00(void) {}
/* 0x80036FBC */ void fn_80036FBC(void) {}
/* 0x800371BC */ void fn_800371BC(void *critter, f32 *pos) {}
/* 0x800372A0 */ void fn_800372A0(void) {}
/* 0x800374FC */ void fn_800374FC(void) {}
/* 0x80037734 */ void fn_80037734(void) {}
/* 0x800378C8 */ void fn_800378C8(void) {}
/* 0x80037A10 */ void fn_80037A10(void) {}
/* 0x80037C08 */ void fn_80037C08(void) {}
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
    s32 activePlayers;
    s32 i;
    s32 total;

    activePlayers = 0;
    total = 0;
    lbl_80344664++;
    for (i = 0; i < 4; i++) {
        if (gPlayers[i].state == 1) {
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
/* 0x8003946C */ void CritterDoKnockback(void) {}
/* 0x800395C8 */ void CritterUpdateCounters(Critter *c) {}
/* 0x800396A4 */ void CritterGolemAI(void) {}
/* 0x80039AD8 */ void CritterBossAI(void) {}
/* 0x8003A73C */ void fn_8003A73C(void) {}
/* 0x8003A838 */ void fn_8003A838(void) {}
/* 0x8003A9C4 */ void fn_8003A9C4(void) {}
/* 0x8003AF4C */ void CritterRotate(void) {}
/* 0x8003B1CC */ void fn_8003B1CC(void) {}
/* 0x8003B300 */ void fn_8003B300(void) {}
/* 0x8003B4CC */ void CritterGetNextMove(void) {}
/* 0x8003B67C */ void fn_8003B67C(void) {}
/* 0x8003B7D8 */ void fn_8003B7D8(void) {}
/* 0x8003BAFC */ void fn_8003BAFC(void) {}
/* 0x8003BC28 */ void fn_8003BC28(void) {}
/* 0x8003BDF4 */ void fn_8003BDF4(void) {}
/* 0x8003C11C */ void fn_8003C11C(void) {}
/* 0x8003C40C */ void CritterAnimate(void) {}
/* 0x8003C6FC */ void CritterMoveDone(void) {}

/* 0x8003C8D4 -- classify two critters' facing/positions into a 0/1/2 code by
 * the relation encoded in a->curmove (0x56). */
s32 fn_8003C8D4(Critter *a, Critter *b)
{
    s32 av;
    s32 bv;
    s32 result;

    result = 1;
    av = (s32)a->state;
    bv = (s32)b->state;
    switch ((s32)*(s16 *)((u8 *)a + 86)) {
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

/* 0x8003C988 */ void CritterFindMoveType(void) {}
/* 0x8003CA98 */ void fn_8003CA98(void) {}
/* 0x8003D0A4 */ void fn_8003D0A4(void) {}
/* 0x8003D7E0 */ void CritterDoSfx(void) {}
/* 0x8003DC64 */ void CritterDoParticle(void) {}
/* 0x8003DE70 */ void fn_8003DE70(void) {}
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
/* 0x8003E7D0 */ void CritterAddHealthMeter(void) {}
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
        memset(c->moveflags, 0, *(s16 *)(h + 272) * 4);
    }
    if ((s16)*(s16 *)(h + 276) > 0) {
        memset(c->movestate, 0, *(s16 *)(h + 276) * 4);
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
/* 0x8003EDC4 */ void CritterRemoveColnodeSub(void) {}
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
/* 0x8003FF98 */ void fn_8003FF98(void) {}
/* 0x800400F0 */ void CritterInitHeader(void *hdr, void *file) {}
