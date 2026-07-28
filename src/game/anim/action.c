/*
 * action.c - enemy/player action state machines (ACTION.OBJ).
 *
 * .text 0x800AB8E0-0x800ADE30 (the former "WORLD TU2" map gap between
 * world.c and g3dMath3D.cpp).  Identified as ACTION.OBJ via the Xbox PDB:
 * the GC emission is the exact reverse of the Xbox source order
 * (RequestEnemyAction, InitActions, PlayerAttackType, DoPlayerAction,
 * DoEnemyAction), sizes align (DoPlayerAction 0x1B94 vs Xbox 0x1A44,
 * DoEnemyAction 0x788 vs 0x64D), callers confirm (enemy.c -> DoEnemyAction/
 * RequestEnemyAction, do_players/PlayerMotion -> DoPlayerAction, gamemain ->
 * InitActions), and e_actpri (the PDB-named priority table) sits at
 * 0x80126F48 directly before the TU's seven jumptables.
 *
 * extab/extabindex gaps between world.c and g3dMath3D = exactly the TU's
 * three LR-saving fns (DoEnemyAction/DoPlayerAction/InitActions).  Own
 * .sdata2 pool 0x803489A0-0x80348A40 (0.0f first, shared by DoEnemyAction/
 * DoPlayerAction/RequestEnemyAction - single-TU proof).  The anim-debug
 * overlay strings ("ACTION:%s NEXT:%s D:%s INT:%d RPT:%d DIDT:%d",
 * "  SEQ:%s  frame:%.1f/%d", "Stop Watch #N" @0x80115810+) live in
 * DoPlayerAction's GC-only dbgTextPrintfCol debug block; the action-name
 * pointer table lbl_80126C68 it prints from is owned by an earlier TU.
 *
 * Status: NonMatching.  InitActions / RequestEnemyAction / PlayerAttackType
 * are full translations; the two giant dispatchers are documented skeletons
 * (DoEnemyAction: 34-case switch on enemy action via jumptable_801270EC +
 * three inner jumptables, SFX attach/detach via SfxSetParent/
 * SfxDeleteParented, gCurLevel checks; DoPlayerAction: player anim-action
 * sequencer over p->seq/p->nextSeq with PlayerAttackType classification,
 * three jumptables 0x80127174/80127384/80127540, atree frame stepping via
 * fn_80011134, mb_tree node color pokes MBTreeClearFlags/MBTreeSetFlags).
 */
#include "types.h"

/* atree (animation tree): +0x04 sequence table, 48-byte entries with the
 * frame count at +0x20 of each entry. */
typedef struct ATREE {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ char* seqs;
} ATREE;

/* per-action init record filled by InitActions */
typedef struct ACTIONDEF {
    /* 0x00 */ s32 seq;    /* atree sequence index, -1 = missing */
    /* 0x04 */ s32 frames; /* sequence frame count */
} ACTIONDEF;

/* minimal enemy view (full struct: include/game/Enemy.h, stride 0x394) */
typedef struct ENEMYACT {
    /* 0x000 */ u8 _pad0[0xD0];
    /* 0x0D0 */ s32 action;    /* current action id (e_actpri index) */
    /* 0x0D4 */ u8 _pad1[0x2A8];
    /* 0x37C */ f32 actTimer;  /* in-progress timer; >0 = uninterruptible */
} ENEMYACT;

s32 AtreeFindSeq(ATREE* atree, char* name);
void SfxSetParent(void* sfx, void* parent);
void SfxDeleteParented(void* parent);
void dbgTextPrintfCol(int x, int y, u32 col, const char* fmt, ...);

void DoEnemyAction(void* enemy);
void DoPlayerAction(void* player);
s32 PlayerAttackType(s32 seq);
void InitActions(ATREE* atree, ACTIONDEF* defs, char** names);
void RequestEnemyAction(ENEMYACT* e, s32 action);

/* 0x80126F48  action priority table (Xbox PDB: e_actpri).  A new action is
 * accepted only when its priority exceeds the current action's. */
s32 e_actpri[33] = {
    100, 900, 900, 200, 200, 200, 200, 200, 200, 200, 200,
    200, 300, 300, 300, 300, 300, 300, 300, 300, 300, 300,
    300, 300, 300, 300, 300, 150, 400, 450, 460, 900, 999,
};

/* 0x800AB8E0  enemy action dispatcher: switch on e->action (0..33, ids >= 28
 * only while below the 28-cap; jumptable_801270EC + inner jumptables
 * 0x80126FCC/80127020/80127088).  Sequences enemy anim actions, attaches /
 * detaches action SFX (SfxSetParent / SfxDeleteParented / the sfx starter
 * fn_80091D50), steps animation via fn_80011104, special-cases gCurLevel.
 * SKELETON - full reconstruction pending. */
void DoEnemyAction(void* enemy)
{
    (void)enemy;
}

/* 0x800AC068  player action sequencer (giant, 0x1B94): advances the player
 * action state (p+0x208 current seq, p+0x20C next seq, PlayerAttackType of
 * both), drives the atree animation (fn_80011134), pokes display-tree state
 * (MBTreeClearFlags / MBTreeSetFlags), and draws the GC-only anim debug overlay
 * ("ACTION:%s NEXT:%s D:%s INT:%d RPT:%d DIDT:%d" via dbgTextPrintfCol,
 * name table lbl_80126C68).  Jumptables 0x80127174/0x80127384/0x80127540.
 * SKELETON - full reconstruction pending. */
void DoPlayerAction(void* player)
{
    (void)player;
}

/* 0x800ADBFC  classify an atree sequence index into a player attack type
 * (1 = quick attacks, 2..12 = melee/turbo family bands, 0 = not an attack). */
s32 PlayerAttackType(s32 seq)
{
    if (seq >= 32 && seq < 114) {
        if (seq < 39) {
            return 2;
        }
        if (seq < 44) {
            return 3;
        }
        if (seq < 60) {
            return 4;
        }
        if (seq < 62) {
            return 5;
        }
        if (seq < 71) {
            return 6;
        }
        if (seq < 79) {
            return 7;
        }
        if (seq < 86) {
            return 8;
        }
        if (seq < 88) {
            return 11;
        }
        if (seq < 91) {
            return 12;
        }
        if (seq < 99) {
            return 9;
        }
        if (seq < 107) {
            return 10;
        }
        return 11;
    }
    if (seq >= 136 && seq <= 147) {
        return 12;
    }
    if (seq >= 119 && seq <= 121) {
        return 1;
    }
    if (seq >= 4 && seq <= 7) {
        return 1;
    }
    if (seq == 8) {
        return 1;
    }
    return 0;
}

/* 0x800ADD24  resolve a NULL-terminated action-name list against an atree:
 * defs[i] = { AtreeFindSeq(atree, names[i]), frame count } (-1/0 when the
 * atree is missing or the sequence is not found). */
void InitActions(ATREE* atree, ACTIONDEF* defs, char** names)
{
    s32 i;
    s32 seq;

    for (i = 0; names[i] != 0; i++) {
        if (atree != 0) {
            seq = AtreeFindSeq(atree, names[i]);
        } else {
            seq = -1;
        }
        defs[i].seq = seq;
        if (seq >= 0) {
            defs[i].frames = *(s16*)(atree->seqs + seq * 48 + 32);
        } else {
            defs[i].frames = 0;
        }
    }
}

/* 0x800ADDBC  request a new enemy action: refused while an interrupt-locked
 * action band (12..20, 24..26) still has time on its timer, or when the
 * current action's priority is not lower than the request's. */
void RequestEnemyAction(ENEMYACT* e, s32 action)
{
    if (action >= 12 && action <= 20 && e->actTimer > 0.0f) {
        return;
    }
    if (action >= 24 && action <= 26 && e->actTimer > 0.0f) {
        return;
    }
    if (e_actpri[e->action] >= e_actpri[action]) {
        return;
    }
    e->action = action;
}
