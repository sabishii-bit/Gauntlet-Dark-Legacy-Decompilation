#include "game/ml_text.h"
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
 * DoAnimateTree, mb_tree node color pokes MBTreeClearFlags/MBTreeSetFlags).
 */
#include "types.h"
#include "game/enemy.h"
#include "game/player.h"
#include "game/leveldata.h"

#ifndef offsetof
#define offsetof(type, memb) ((u32) & ((type*)0)->memb)
#endif

/* atree (animation tree): +0x04 sequence table, 48-byte entries with the
 * frame count at +0x20 of each entry (see ATREESEQ below).  DoEnemyAction's
 * e70 and DoPlayerAction's atree locals both point at an embedded
 * `animinfo` (include/game/enemy.h) - the enemy and player atree wrappers
 * share that same playback-state layout at the byte level (animseq +0x0E,
 * numframes +0x10, repeat +0x34, stage +0x36 all verified equal in both
 * functions' target asm). */
typedef struct ATREE {
    /* 0x00 */ s32 unk00;
    /* 0x04 */ char* seqs;
} ATREE;

/* one atreeseq sequence-table entry (48 bytes, InitActions' `seq * 48 + 32`).
 * Only the frame-count field this TU reads is named; the rest of the entry
 * is unresolved and kept as explicit padding. */
typedef struct ATREESEQ {
    /* 0x00 */ u8 _pad00[0x20];
    /* 0x20 */ s16 frames;
    /* 0x22 */ u8 _pad22[0x30 - 0x22];
} ATREESEQ;

/* per-action init record filled by InitActions */
typedef struct ACTIONDEF {
    /* 0x00 */ s32 seq;    /* atree sequence index, -1 = missing */
    /* 0x04 */ s32 frames; /* sequence frame count */
} ACTIONDEF;

/* minimal enemy view (full struct: include/game/enemy.h, stride 0x394) */
typedef struct ENEMYACT {
    /* 0x000 */ u8 _pad0[0xD0];
    /* 0x0D0 */ s32 action;    /* current action id (e_actpri index) */
    /* 0x0D4 */ u8 _pad1[0x2A8];
    /* 0x37C */ f32 actTimer;  /* in-progress timer; >0 = uninterruptible */
} ENEMYACT;

s32 AtreeFindSeq(ATREE* atree, char* name);
void SfxSetParent(void* sfx, void* parent);
void SfxDeleteParented(void* parent, s32 a, s32 b);
s32 AnimateATree(void* node, s32 seq, s32 mode);
u32 DoAnimateTree(f32 t, void* node, s32 seq, u32 frame, s32 mode, s32 e);
s32 StartEnemyAtkFX(void* a, s32 b);
void MBTreeSetFlags(void* obj, u32 flags, s32 recurse);
void MBTreeClearFlags(void* obj, u32 flags, s32 recurse);

extern u32 sFlags;      /* 0x803445CC anim debug flags */
extern u64 gControllerButtons; /* 0x803445C8 config-word pair */
extern level_data* gCurLevel;   /* game/leveldata.h; 0x8034483C */
extern char* lbl_80126C68[]; /* action-name table (owned by an earlier TU) */

s32 DoEnemyAction(void* enemy);
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

/* 0x800AB8E0  enemy action dispatcher: switch on the current action, pick the
 * next action id, substitute fallbacks when the sequence table has no entry,
 * step the atree via AnimateATree, then run the SFX attach/detach side
 * effects and the 0x18..0x1A walk-cycle timer. */
s32 DoEnemyAction(void* enemy)
{
    s32* e = (s32*)enemy;
    Enemy* en = (Enemy*)enemy;
    f32* ef = (f32*)enemy;
    animinfo* e70 = (animinfo*)((u8*)e + 0x70); /* == &enemy->atree.animinfo: repeat (interruptible
                                 flag) at +0x34, numframes at +0x10 */
    s32* defs = e + 0x35; /* ACTIONDEF[34] at +0xD4 */
    s32 next = en->daction;   /* +0xD0 requested action */
    s32* node;            /* atree node at +0x6C */
    s32 act;
    s32 cur;
    s32 mode = 2;
    s32 interruptible = 1;
    s32 sw;
    s32 seq;
    s32 result;
    s32 type;

    node = (s32*)&en->atree;
    cur = en->action;        /* +0xCC current action */
    act = next;
    if (act >= E_HIT_REACT1) {
        sw = E_READY;
    } else {
        sw = cur;
    }
    switch (sw) {
    case E_START:
        type = en->type;
        mode = 0;
        interruptible = 0;
        if (type == E_TROLL || type == E_GRUNT || type == E_LIZARDMAN || type == E_SORCERER) {
            if (defs[E_WALK * 2] >= 0) {
                act = E_WALK;
            } else {
                act = E_RUN;
            }
        }
        break;
    case E_READY:
        if (en->type == E_GOLEM) {
            if (next >= E_HIT_REACT1) {
                mode = 2;
            } else {
                mode = 0;
            }
            interruptible = 0;
        }
        if (en->type == E_GARM2) {
            mode = 2;
        } else if (next == E_WALK && defs[E_READYTOWALK * 2] >= 0) {
            act = E_READYTOWALK;
            mode = 0;
        } else if (next == E_RUN && defs[E_READYTORUN * 2] >= 0) {
            act = E_READYTORUN;
            mode = 0;
        } else if (next == E_ATTACK || next == E_ATTACK2 || next == E_ATTACK_PWR) {
            mode = 0;
        }
        break;
    case E_READYTORUN:
        if (next >= E_HIT_REACT1) {
            mode = 2;
        } else {
            mode = 0;
            interruptible = 0;
            if (defs[E_RUN * 2] >= 0) {
                act = E_RUN;
            } else {
                act = E_WALK;
            }
        }
        break;
    case E_READYTOWALK:
        if (next >= E_HIT_REACT1) {
            mode = 2;
        } else {
            mode = 0;
            interruptible = 0;
            if (defs[E_WALK * 2] >= 0) {
                act = E_WALK;
            } else {
                act = E_RUN;
            }
        }
        break;
    case E_WALKTOREADY:
    case E_RUNTOREADY:
        if (next >= E_HIT_REACT1) {
            mode = 2;
        } else {
            mode = 0;
            interruptible = 0;
            act = E_READY;
        }
        break;
    case E_WALK:
    case E_FLY:
    case E_HOVER:
    case E_LANDING:
        if (en->type == E_GOLEM && next == E_READY) {
            mode = 0;
            interruptible = 0;
            act = E_READY;
        }
        if (next == E_READY) {
            if (defs[E_WALKTOREADY * 2] >= 0) {
                act = E_WALKTOREADY;
            }
            mode = 0;
        }
        break;
    case E_RUN:
        if (next == E_READY && defs[E_RUNTOREADY * 2] >= 0) {
            act = E_RUNTOREADY;
            mode = 0;
        }
        break;
    case E_HIT_REACT1:
        mode = 0;
        interruptible = 0;
        break;
    case E_HIT_REACT2:
        mode = 0;
        interruptible = 0;
        if (defs[E_GETUP * 2] >= 0) {
            act = E_GETUP;
        }
        break;
    case E_HIT_REACT3:
        mode = 0;
        interruptible = 0;
        break;
    case E_DYING:
        mode = 0;
        interruptible = 0;
        break;
    case E_GETUP:
        mode = 0;
        interruptible = 0;
        break;
    case E_TAUNT:
        mode = 0;
        interruptible = 0;
        break;
    case E_ATTACK:
        mode = 0;
        interruptible = 0;
        if (next >= E_HIT_REACT1) {
            mode = 2;
        } else {
            act = E_ATTACK_R;
        }
        break;
    case E_ATTACK_R:
        mode = 0;
        interruptible = 0;
        if (next >= E_HIT_REACT1) {
            mode = 2;
        } else {
            if (defs[E_ATTACK2 * 2] >= 0) {
                act = E_ATTACK2;
            } else {
                if (next == E_ATTACK) {
                    act = E_ATTACK;
                }
            }
        }
        break;
    case E_ATTACK2:
        mode = 0;
        interruptible = 0;
        if (next >= E_HIT_REACT1) {
            mode = 2;
        } else {
            act = E_ATTACK2_R;
        }
        break;
    case E_ATTACK2_R:
        mode = 0;
        interruptible = 0;
        if (next >= E_HIT_REACT1) {
            mode = 2;
        } else {
            if (next == E_ATTACK) {
                act = E_ATTACK;
            }
        }
        break;
    case E_ATTACK_PWR:
        mode = 0;
        interruptible = 0;
        if (next >= E_HIT_REACT1) {
            mode = 2;
        } else {
            act = E_ATTACK_PWR_R;
        }
        break;
    case E_ATTACK_PWR_R:
        mode = 0;
        interruptible = 0;
        if (next >= E_HIT_REACT1) {
            mode = 2;
        }
        break;
    case E_ATTACK4:
        mode = 0;
        interruptible = 0;
        if (next >= E_HIT_REACT1) {
            mode = 2;
        } else {
            act = E_ATTACK4_R;
        }
        break;
    case E_ATTACK4_R:
        if (next == E_READY && en->coll_pnum >= 0) {
            act = E_ATTACK;
        }
        break;
    case E_ATTACK5:
        mode = 0;
        interruptible = 0;
        if (next >= E_HIT_REACT1) {
            mode = 2;
        } else {
            act = E_ATTACK5_R;
        }
        break;
    case E_ATTACK5_R:
        if (next == E_READY && en->coll_pnum >= 0) {
            act = E_ATTACK;
        }
        break;
    case E_RUNATTACK:
        mode = 0;
        interruptible = 0;
        if (next >= E_HIT_REACT1) {
            mode = 2;
        } else {
            if (next == E_RUNATTACK) {
                act = E_RUNATTACK2;
            }
        }
        break;
    case E_RUNATTACK2:
        mode = 0;
        interruptible = 0;
        if (next >= E_HIT_REACT1) {
            mode = 2;
        } else {
            if (next == E_RUNATTACK) {
                act = E_RUNATTACK;
            }
        }
        break;
    case E_THROW:
        if (defs[E_THROW * 2] < 0) {
            act = E_THROW2;
            cur = E_THROW2;
        }
        /* fallthrough */
    case E_THROW2:
        mode = 0;
        interruptible = 0;
        if (next >= E_HIT_REACT1) {
            mode = 2;
        } else {
            act = E_THROW_FINISH;
        }
        break;
    case E_THROW_FINISH:
        mode = 0;
        interruptible = 0;
        if (next >= E_HIT_REACT1) {
            mode = 2;
        } else {
            if (next == E_THROW) {
                act = E_THROW2;
            } else if (next == E_READY) {
                act = E_THROWTOREADY;
            }
        }
        break;
    case E_THROWTOREADY:
        mode = 0;
        interruptible = 0;
        act = E_READY;
        if (next >= E_HIT_REACT1) {
            mode = 2;
        }
        break;
    case E_NACTIONS:
        break;
    }

    /* missing-sequence fallbacks */
    switch (act) {
    case E_ATTACK2:
    case E_ATTACK_PWR:
    case E_ATTACK4:
    case E_ATTACK5:
        if (defs[act * 2] < 0) {
            act = E_ATTACK;
        }
        break;
    case E_WALK:
    case E_READYTORUN:
        if (defs[act * 2] < 0) {
            act = E_RUN;
        }
        break;
    case E_RUN:
    case E_READYTOWALK:
        if (defs[act * 2] < 0) {
            act = E_WALK;
        }
        break;
    case E_WALKTOREADY:
    case E_RUNTOREADY:
    case E_THROWTOREADY:
        if (defs[act * 2] < 0) {
            act = E_READY;
        }
        break;
    case E_THROW2:
        if (defs[act * 2] < 0) {
            act = E_THROW;
        }
        break;
    }

    seq = defs[act * 2];
    if (seq < 0 && act == E_DYING) {
        seq = defs[0x3A];
    }
    if (seq < 0) {
        if (defs[0] >= 0) {
            seq = defs[0];
        } else {
            seq = 0;
        }
        interruptible = 1;
        if (mode != 0) {
            mode = 2;
        }
    }
    e70->repeat = interruptible;
    result = AnimateATree(node, seq, mode);

    if (result != 0) {
        switch (cur) {
        case E_READY:
            break;
        case E_ATTACK:
        case E_ATTACK2:
        case E_ATTACK4:
        case E_ATTACK5:
            if (act == E_ATTACK_R || act == E_ATTACK2_R || act == E_ATTACK4_R || act == E_ATTACK5_R) {
                en->attack_flag |= 1;
            }
            break;
        case E_ATTACK_PWR:
            if (act == E_ATTACK_PWR_R) {
                en->attack_flag |= 2;
            }
            break;
        case E_THROW:
        case E_THROW2:
            if (act == E_THROW_FINISH) {
                en->attack_flag |= 0x10;
            }
            break;
        case E_RUNATTACK:
            if (act == E_RUNATTACK2) {
                en->attack_flag |= 0x10;
            }
            break;
        }
        switch (act) {
        case E_ATTACK:
        case E_ATTACK2:
            if (en->type == E_GARM2) {
                SfxSetParent((void*)StartEnemyAtkFX(0, 0), (void*)e[0x19]);
            }
            break;
        case E_ATTACK_PWR:
            if (en->type == E_GARM2) {
                SfxSetParent((void*)StartEnemyAtkFX(0, 1), (void*)e[0x19]);
            }
            break;
        case E_ATTACK_R:
        case E_ATTACK2_R:
        case E_ATTACK_PWR_R:
        case E_HIT_REACT2:
        case E_DYING:
            break;
        default:
            if (en->type == E_GARM2) {
                SfxDeleteParented((void*)e[0x19], 0, -1);
            }
            break;
        }
    }

    if (result != 0) {
        f32 dur = 0.0f;
        f32 accum = 0.0f;

        if (act >= E_THROW && act <= E_THROW_FINISH) {
            dur = en->idle_time * gCurLevel->ene_mrate + en->idle_frac;
        }
        if (dur > 0.0) {
            while (dur > 1.0) {
                accum += 1.0;
                dur = (f32)(dur - 1.0);
            }
            en->idle_frac = dur;
            en->idle_secs = accum;
            if (en->idle_secs >= 1.0) {
                en->idle_secs = (f32)(0.0333333333 *
                                 (s32)e70->numframes +
                                 en->idle_secs);
            }
        }
        return act;
    }
    return cur;
}

/* 0x800AC068  player action sequencer: advances the player action state
 * (p+0x208 current, p+0x20C requested, PlayerAttackType of both), drives the
 * atree animation (DoAnimateTree), pokes display-tree state (MBTreeClearFlags /
 * MBTreeSetFlags), and draws the GC-only anim debug overlay. */
void DoPlayerAction(void* player)
{
    s32* p = (s32*)player;
    Player* pl = (Player*)player;
    f32* pf = (f32*)player;
    animinfo* atree = (animinfo*)((u8*)player + 0x80);
    char** action_names = lbl_80126C68;
    void* node = (u8*)player + 0x7C;
    ACTIONDEF* defs = (ACTIONDEF*)((u8*)player + 0x210);
    s32 rpt = 0;
    s32 next;
    s32 cur;
    s32 atkNext;
    s32 atkCur;
    s32 atkD;
    f32 speed;
    s32 mode;
    s32 didt;
    u32 frame;
    s32 d;
    s32 act;
    s32 seq;
    s32 combo;
    s32 flags;
    s32 adv;
    f32 ang;
    s32 dance;
    u8 unused[8];

    act = pl->anim_20C;
    cur = pl->anim_208;
    next = act;
    *((u8*)p + 0x93) |= 2;
    atkNext = PlayerAttackType(next);
    atkCur = PlayerAttackType(cur);
    speed = 0.0f;
    d = cur;
    mode = 0;
    didt = 0;
    frame = 0;
    if (act >= P_USE_MAGIC && atkCur != 0 && atkCur < 0xB) {
        d = P_READY;
    }
    if (act >= P_DEATHGRAB && act < P_ATTACK_SLOW) {
        mode = 2;
    }
    if (act >= P_FALL_DOWN && act <= P_GRABBED) {
        s32 repeatCount;

        mode = 2;
        if (act == P_GRABBED) {
            repeatCount = 2;
        } else {
            repeatCount = 1;
        }
        rpt = repeatCount;
    }
    atkD = PlayerAttackType(d);
    if (atkD != 0 && atkD < 0xB && atkNext >= 11) {
        d = P_READY;
        mode = 2;
    }
    if ((atkD < 2 || atkD > 6) && atkD != 8) {
        pl->field_908 = 0;
    }
    p[0x201] = 0;
    dance = 0;
    switch (d) {
    case P_ACTION_INIT:
        mode = 2;
        break;
    case P_READY:
        didt = 1;
        mode = 2;
        if (next == P_READY && pl->quest_state == 0) {
            s32 hi, lo;
            if ((gControllerButtons & 0x10) != 0) {
                hi = 0xB4;
            } else {
                hi = 0x708;
            }
            if ((gControllerButtons & 0x10) != 0) {
                lo = 0x3C;
            } else {
                lo = 600;
            }
            if (pl->vibe_timer > hi) {
                mode = 1;
                act = P_IDLE1;
            } else if (pl->vibe_timer2 > lo) {
                act = P_IDLE2;
                mode = 1;
            }
        }
        break;
    case P_IDLE1:
        didt = 0;
        mode = 2;
        if (next == P_READY) {
            act = P_READY;
            mode = 1;
        }
        break;
    case P_IDLE2:
        didt = 0;
        mode = 2;
        if (next == P_READY) {
            act = P_IDLE2_LOOP;
            mode = 1;
        }
        break;
    case P_IDLE2_LOOP:
        didt = 1;
        mode = 2;
        if (next == P_READY) {
            act = P_IDLE2_LOOP;
            mode = 0;
        }
        break;
    case P_SHIELD_READY:
        didt = 1;
        mode = 2;
        break;
    case P_DEFENDL:
    case P_DEFENDR:
    case P_DEFENDB:
    case P_DEFENDF:
    case P_DEFEND1:
    case P_DEFEND2:
        mode = 1;
        if (d == P_DEFEND1) {
            act = P_DEFEND2;
        } else {
            act = P_DEFEND3;
        }
        break;
    case P_STRAFE_WLKF:
    case P_STRAFE_WLKB:
        if (next >= P_ATTACK_SLOW || atkNext != 0) {
            mode = 2;
        }
        if (next == P_STRAFE_WLKF) {
            act = P_STRAFE_WLKF2;
        }
        if (next == P_STRAFE_WLKB) {
            act = P_STRAFE_WLKB2;
        }
        break;
    case P_STRAFE_WLKF2:
    case P_STRAFE_WLKB2:
        if (next >= P_ATTACK_SLOW || atkNext != 0) {
            mode = 2;
        }
        if (next == P_STRAFE_WLKF) {
            act = P_STRAFE_WLKF;
        }
        if (next == P_STRAFE_WLKB) {
            act = P_STRAFE_WLKB;
        }
        break;
    case P_STRAFE_WLKL:
    case P_STRAFE_WLKR:
        if (next >= P_ATTACK_SLOW || atkNext != 0) {
            mode = 2;
        }
        if (next == P_STRAFE_WLKL) {
            act = P_STRAFE_WLKL2;
        }
        if (next == P_STRAFE_WLKR) {
            act = P_STRAFE_WLKR2;
        }
        break;
    case P_STRAFE_WLKL2:
    case P_STRAFE_WLKR2:
        if (next >= P_ATTACK_SLOW || atkNext != 0) {
            mode = 2;
        }
        if (next == P_STRAFE_WLKL) {
            act = P_STRAFE_WLKL;
        }
        if (next == P_STRAFE_WLKR) {
            act = P_STRAFE_WLKR;
        }
        break;
    case P_STRAFE_ATKF:
    case P_STRAFE_ATKB:
        if (next == P_STRAFE_ATKF) {
            act = P_STRAFE_ATKF2;
        }
        if (next == P_STRAFE_ATKB) {
            act = P_STRAFE_ATKB2;
        }
        if (pl->weaphold_node != 0 && (pl->char_type & 3) != 2 && pl->char_type != 3) {
            if (atree->frame < 2.0f) {
                MBTreeSetFlags(pl->weaphold_node, 2, 0);
            } else {
                MBTreeClearFlags(pl->weaphold_node, 2, 0);
            }
        }
        break;
    case P_STRAFE_ATKF2:
    case P_STRAFE_ATKB2:
        if (next == P_STRAFE_ATKF) {
            act = P_STRAFE_ATKF;
        }
        if (next == P_STRAFE_ATKB) {
            act = P_STRAFE_ATKB;
        }
        if (pl->weaphold_node != 0 && (pl->char_type & 3) != 2 && pl->char_type != 3) {
            if (atree->frame < 2.0f) {
                MBTreeSetFlags(pl->weaphold_node, 2, 0);
            } else {
                MBTreeClearFlags(pl->weaphold_node, 2, 0);
            }
        }
        break;
    case P_STRAFE_ATKL:
    case P_STRAFE_ATKR:
        if (next == P_STRAFE_ATKL) {
            act = P_STRAFE_ATKL2;
        }
        if (next == P_STRAFE_ATKR) {
            act = P_STRAFE_ATKR2;
        }
        if (pl->weaphold_node != 0 && (pl->char_type & 3) != 2 && pl->char_type != 3) {
            if (atree->frame < 2.0f) {
                MBTreeSetFlags(pl->weaphold_node, 2, 0);
            } else {
                MBTreeClearFlags(pl->weaphold_node, 2, 0);
            }
        }
        break;
    case P_STRAFE_ATKL2:
    case P_STRAFE_ATKR2:
        if (next == P_STRAFE_ATKL) {
            act = P_STRAFE_ATKL;
        }
        if (next == P_STRAFE_ATKR) {
            act = P_STRAFE_ATKR;
        }
        if (pl->weaphold_node != 0 && (pl->char_type & 3) != 2 && pl->char_type != 3) {
            if (atree->frame < 2.0f) {
                MBTreeSetFlags(pl->weaphold_node, 2, 0);
            } else {
                MBTreeClearFlags(pl->weaphold_node, 2, 0);
            }
        }
        break;
    case P_PIVOTL:
    case P_PIVOTR:
        mode = 2;
        break;
    case P_WALK:
        if (next >= P_HIT_REACT || atkNext != 0) {
            mode = 2;
        }
        if (next == P_WALK) {
            act = P_WALK2;
        }
        break;
    case P_WALK2:
        if (next >= P_HIT_REACT || atkNext != 0) {
            mode = 2;
        }
        if (next == P_WALK) {
            act = P_WALK;
        }
        break;
    case P_RUN:
        if (next >= P_HIT_REACT || atkNext != 0) {
            mode = 2;
        }
        if (next == P_RUN) {
            act = P_RUN2;
        }
        break;
    case P_RUN2:
        if (next == P_HIT_REACT || atkNext != 0) {
            mode = 2;
        }
        if (next == P_RUN) {
            act = P_RUN;
        }
        break;
    case P_SHIELD_RUN:
        if (next >= P_ATTACK_SLOW || atkNext != 0) {
            mode = 2;
        }
        break;
    case P_SHOVE:
        didt = 1;
        if (next >= P_ATTACK_SLOW) {
            mode = 2;
        }
        break;
    case P_ATTACK_SLOW:
        if (atkNext >= 0xB || (u32)(atkNext - 9) <= 1) {
            mode = 2;
            frame = (s32)(0.5 + atree->frame);
        } else {
            act = P_ATTACK_SLOW1;
        }
        break;
    case P_ATTACK_SLOW1:
        p[0x201] = 1;
        act = P_ATTACK_SLOW1_R;
        break;
    case P_ATTACK_SLOW1_R:
        if (atkNext == 1) {
            mode = 2;
        }
        break;
    case P_ATTACK_QUICK:
    case P_ATTACK_QUICK2:
    case P_ATTACK_QUICK3:
        flags = pl->field_8F8;
        if ((flags & 0x400U) != 0 && (combo = pl->field_908) != 0) {
            if (combo == 1) {
                act = P_ATTACK_PWRA_CLOSE;
            } else if (combo == 2) {
                act = P_ATTACK_PWRA_MED;
            } else {
                act = P_ATTACK_360;
            }
        } else if (flags != 0 || pl->field_8F4 != 0) {
            if ((pl->coll_flags & 8U) != 0) {
                act = cur == P_ATTACK_QUICK2 ? P_ATTACK_QUICK2_R : P_ATTACK_QUICK3_R;
            } else if ((pl->coll_flags & 4U) != 0) {
                act = cur == P_ATTACK_QUICK2 ? P_ATTACK_STEP3 : P_ATTACK_STEP2;
            } else {
                act = cur == P_ATTACK_QUICK2 ? P_ATTACK_QUICK3 : P_ATTACK_QUICK2;
            }
        } else {
            act = cur == P_ATTACK_QUICK2 ? P_ATTACK_QUICK2_R : P_ATTACK_QUICK3_R;
        }
        break;
    case P_ATTACK_QUICK2_R:
    case P_ATTACK_QUICK3_R:
        flags = pl->field_8F8;
        if ((flags & 0x400U) != 0 && (combo = pl->field_908) != 0) {
            if (combo == 1) {
                act = P_ATTACK_PWRA_CLOSE;
            } else if (combo == 2) {
                act = P_ATTACK_PWRA_MED;
            } else {
                act = P_ATTACK_360;
            }
        } else if (flags != 0 && atree->frame <= 2.0 &&
                   (pl->coll_flags & 1U) != 0) {
            act = cur == P_ATTACK_QUICK2_R ? P_ATTACK_QUICK3 : P_ATTACK_QUICK2;
            mode = 2;
        }
        if (atkNext == 1) {
            mode = 2;
        }
        break;
    case P_ATTACK_STEP:
    case P_ATTACK_STEP2:
    case P_ATTACK_STEP3:
        flags = pl->field_8F8;
        if ((flags & 0x400U) != 0 && (combo = pl->field_908) != 0) {
            if (combo == 1) {
                act = P_ATTACK_PWRA_CLOSE;
            } else if (combo == 2) {
                act = P_ATTACK_PWRA_MED;
            } else {
                act = P_ATTACK_360;
            }
        } else if (flags != 0 || pl->field_8F4 != 0) {
            if ((pl->coll_flags & 8U) != 0) {
                s32 gapAct;

                if (cur == P_ATTACK_STEP2) {
                    gapAct = 0x41;
                } else {
                    gapAct = 0x42;
                }
                act = gapAct;
            } else if ((pl->coll_flags & 4U) != 0) {
                s32 gapAct;

                if (cur == P_ATTACK_STEP2) {
                    gapAct = 0x40;
                } else {
                    gapAct = 0x3F;
                }
                act = gapAct;
            } else {
                s32 gapAct;

                if (cur == P_ATTACK_STEP2) {
                    gapAct = 0x29;
                } else {
                    gapAct = 0x28;
                }
                act = gapAct;
            }
        } else {
            s32 paired_action;

            if (cur == P_ATTACK_STEP2) {
                paired_action = 0x41;
            } else {
                paired_action = 0x42;
            }
            act = paired_action;
        }
        break;
    case P_ATTACK_STEP2_R:
    case P_ATTACK_STEP3_R:
        if ((pl->field_8F8 & 0x400U) != 0 && (combo = pl->field_908) != 0) {
            if (combo == 1) {
                act = P_ATTACK_PWRA_CLOSE;
            } else if (combo == 2) {
                act = P_ATTACK_PWRA_MED;
            } else {
                act = P_ATTACK_360;
            }
        } else if (atkNext == 1) {
            mode = 2;
        }
        break;
    case P_ATTACK_Q3TOSTEP1:
    case P_ATTACK_Q3TOSTEP1_R:
    case P_ATTACK_WALK2:
    case P_ATTACK_WALK2_R:
        if (atkNext == 1) {
            mode = 2;
        }
        break;
    case P_ATTACK_RIGHT:
        act = P_ATTACK_RIGHT_R;
        break;
    case P_ATTACK_RIGHT2:
        act = P_ATTACK_RIGHT2_R;
        break;
    case P_ATTACK_LEFT:
        act = P_ATTACK_LEFT_R;
        break;
    case P_ATTACK_LEFT2:
        act = P_ATTACK_LEFT2_R;
        break;
    case P_ATTACK_180:
        act = P_ATTACK_180_R;
        break;
    case P_ATTACK_1802:
        act = P_ATTACK_1802_R;
        break;
    case P_ATTACK_180L:
        act = P_ATTACK_180L_R;
        break;
    case P_ATTACK_180L2:
        act = P_ATTACK_180L2_R;
        break;
    case P_ATTACK_RIGHT_R:
    case P_ATTACK_RIGHT2_R:
    case P_ATTACK_LEFT_R:
    case P_ATTACK_LEFT2_R:
    case P_ATTACK_180_R:
    case P_ATTACK_1802_R:
    case P_ATTACK_180L_R:
    case P_ATTACK_180L2_R:
        if (atkNext == 1) {
            mode = 2;
        }
        break;
    case P_ATTACK_360:
        p[0x201] = 1;
        if ((pl->field_8F8 & 0x400U) != 0 && pl->field_908 != 0) {
            act = P_ATTACK_PWRA_MED;
        } else {
            act = P_ATTACK_360_R;
        }
        break;
    case P_ATTACK_360_R:
        if (atkNext == 1) {
            mode = 2;
        }
        break;
    case P_ATTACK_PWRA_LOW:
        if (rpt < 2) {
            mode = 0;
        }
        act = P_ATTACK_PWRA_LOW_R;
        break;
    case P_ATTACK_PWRA_CLOSE:
        if (rpt < 2) {
            mode = 0;
        }
        act = P_ATTACK_PWRA_CLOSE_R;
        break;
    case P_ATTACK_PWRA_MED:
        p[0x201] = 1;
        if (rpt < 2) {
            mode = 0;
        }
        act = P_ATTACK_PWRA_MED_R;
        break;
    case P_ATTACK_PWRA_THROW:
        if (rpt < 2) {
            mode = 0;
        }
        act = P_ATTACK_PWRA_THROW_R;
        break;
    case P_ATTACK_PWRB:
        if (rpt < 2) {
            mode = 0;
        }
        break;
    case P_ATTACK_PWRC:
        if (rpt < 2) {
            mode = 0;
        }
        break;
    case P_COMBO_ACTIVE1:
        mode = 1;
        if (defs[0x59].seq >= 0) {
            act = P_COMBO_ACTIVE2;
        } else if (defs[0x5A].seq >= 0) {
            act = P_COMBO_ACTIVE3;
        } else if (rpt < 2) {
            mode = 0;
        }
        break;
    case P_COMBO_ACTIVE3:
    case P_COMBO_WAR1:
    case P_COMBO_WAR3:
    case P_COMBO_VAL:
    case P_COMBO_WIZ:
    case P_COMBO_ARC:
    case P_COMBO_DWF1:
    case P_COMBO_DWF3:
    case P_COMBO_KNI:
    case P_COMBO_SOR:
    case P_COMBO_JES:
        if (rpt < 2) {
            mode = 0;
        }
        break;
    case P_COMBO_ACTIVE2:
    case P_COMBO_WAR2:
    case P_COMBO_DWF2:
        didt = 1;
        if (next != d) {
            if (defs[d + P_IDLE1].seq >= 0) {
                act = d + P_IDLE1;
            }
            mode = 2;
        } else {
            mode = 0;
        }
        break;
    case P_ATTACK_PWRA_CLOSE_R:
    case P_ATTACK_PWRA_MED_R:
    case P_ATTACK_PWRA_LOW_R:
    case P_ATTACK_PWRA_THROW_R:
        if (rpt == 0) {
            mode = 0;
        }
        break;
    case P_ATTACK_LOW:
    case P_ATTACK_LOW2:
        if (next == P_ATTACK_LOW) {
            s32 gapAct;

            if (cur == P_ATTACK_LOW) {
                gapAct = 0x50;
            } else {
                gapAct = 0x4F;
            }
            act = gapAct;
        } else {
            act = P_ATTACK_LOW_R;
        }
        break;
    case P_ATTACK_LOW_R:
        if (atkNext == 1) {
            mode = 2;
        } else if (next == P_ATTACK_LOW) {
            act = P_ATTACK_LOW2;
            if (defs[0x50].seq < 0) {
                act = P_ATTACK_LOW;
            }
        } else {
            mode = 1;
        }
        break;
    case P_ATTACK_KICK:
        if ((pl->field_8F8 & 0x400U) != 0 && pl->field_908 != 0) {
            act = P_ATTACK_PWRA_LOW;
        } else {
            act = P_ATTACK_KICK_R;
        }
        break;
    case P_ATTACK_KICK_R:
        if ((pl->field_8F8 & 0x400U) != 0 && pl->field_908 != 0) {
            act = P_ATTACK_PWRA_LOW;
        } else {
            if (atkNext == 1) {
                mode = 2;
            } else {
                mode = 1;
            }
        }
        break;
    case P_THROW2:
    case P_THROW2Q:
        dance = 1;
        /* fallthrough */
    case P_THROW:
    case P_THROWQ:
        if (atkNext > 1 && atkNext != 9 && atkNext != 10) {
            mode = 2;
            frame = (s32)(0.5 + atree->frame);
        } else if (atkNext == 7) {
            mode = 2;
            frame = (s32)(0.5 + atree->frame);
        } else if (next == P_USE_MAGIC || next == P_THROW_MAGIC) {
            mode = 2;
        } else if (next == P_THROW_STEP) {
            mode = 2;
            frame = (s32)(0.5 + atree->frame);
        } else {
            if (next != P_THROW && atree->frame >= 2.0f) {
                mode = 2;
                act = ((dance + P_IDLE1) == P_IDLE2) ? P_THROW2_RELEASE : P_THROW_RELEASE;
            } else {
                act = ((dance + P_IDLE1) == P_IDLE2) ? P_THROW2_RELEASE : P_THROW_RELEASE;
            }
        }
        break;
    case P_THROW_RELEASE:
    case P_THROW2_RELEASE: {
        s32 gapAct;

        if (cur == P_THROW_RELEASE) {
            gapAct = 0x61;
        } else {
            gapAct = 0x62;
        }
        act = gapAct;
        break;
    }
    case P_THROW_RECOVER:
    case P_THROW2_RECOVER:
        if (atkNext == 1) {
            mode = 2;
        }
        break;
    case P_THROW_STEP:
    case P_THROW_STEP2:
        if (atkNext > 1 && atkNext != 9 && atkNext != 10) {
            mode = 2;
            frame = (s32)(0.5 + atree->frame);
        } else if (atkNext == 7) {
            mode = 2;
            frame = (s32)(0.5 + atree->frame);
        } else if (next == P_USE_MAGIC || next == P_THROW_MAGIC) {
            mode = 2;
        } else if (next == P_THROW_STEP) {
            s32 gapAct;

            if (cur == P_THROW_STEP) {
                gapAct = 0x66;
            } else {
                gapAct = 0x65;
            }
            act = gapAct;
        }
        break;
    case P_SSHOT:
    case P_SSHOT2:
        if (next == P_SSHOT) {
            act = P_SSHOT2;
            didt = 1;
        } else if (atkNext != 0) {
            mode = 1;
        } else {
            act = P_SSHOT_R;
        }
        break;
    case P_BREATHE:
        if (rpt == 0) {
            mode = 0;
        }
        act = P_BREATHE_R;
        break;
    case P_HAMMER:
        if (rpt == 0) {
            mode = 0;
        }
        act = P_HAMMER_R;
        break;
    case P_FIREL:
        if (rpt == 0) {
            mode = 0;
        }
        act = P_FIREL_R;
        break;
    case P_FIRER:
        if (rpt == 0) {
            mode = 0;
        }
        act = P_FIRER_R;
        break;
    case P_USE_MAGIC:
        if (next == P_THROW_MAGIC) {
            act = P_THROW_MAGIC;
            mode = 2;
        } else if ((pl->field_956 & 4) == 0) {
            act = P_THROW_MAGIC;
            mode = 0;
        } else {
            act = P_MAGIC_RELEASE;
        }
        break;
    case P_THROW_MAGIC:
        act = P_THROW_MAGIC_RELEASE;
        break;
    case P_DEATH_REACT:
        didt = 1;
        if (next == P_READY ||
            (atree->frame < 10.0f && atree->stage == 0)) {
            mode = 0;
        } else {
            mode = 2;
        }
        break;
    case P_VICTORY:
        if (rpt == 0) {
            mode = 0;
        }
        didt = 1;
        break;
    case P_PUSH:
    case P_PUSHED:
        if (next != P_READY) {
            mode = 2;
            didt = 1;
        }
        break;
    case P_ACTION_START:
        mode = 0;
        break;
    case P_ACTION_DEATH:
    case P_GET_UP:
    case P_GET_UP_FWD:
        mode = 0;
        if (next == cur) {
            act = P_READY;
        }
        break;
    case P_PICKUP:
        if (next == cur) {
            act = P_READY;
        }
        break;
    case P_DEATHGRAB:
    case P_DEATHGRAB2:
        if (next == P_DEATHGRAB) {
            act = P_DEATHGRAB2;
            didt = 1;
        } else {
            act = P_DEATHGRAB_R;
        }
        break;
    case P_HIT_REACT:
        if (act >= P_KNOCKBACK) {
            mode = 3;
        } else {
            mode = 2;
        }
        if (next == P_READY || next == P_WALK || next == P_RUN) {
            mode = 0;
        }
        break;
    case P_STUCK:
        didt = 1;
        if (next != P_READY) {
            mode = 2;
        }
        break;
    case P_STUN_REACT:
    case P_SPIKE_HIT:
    case P_KNOCKBACK:
        mode = 0;
        if (next >= P_FALL_DOWN) {
            mode = 3;
        }
        if (next == cur) {
            act = P_READY;
        }
        break;
    case P_FALL_DOWN:
        mode = 0;
        if (next != P_FALL_DOWN) {
            act = P_GET_UP;
            mode = 1;
        }
        break;
    case P_FALL_DOWN_FWD:
        mode = 0;
        if (next != P_FALL_DOWN_FWD) {
            act = P_GET_UP_FWD;
            mode = 1;
        }
        break;
    case P_WHIRLWIND:
        mode = 0;
        if (next != P_WHIRLWIND) {
            act = P_GET_UP;
            mode = 1;
        }
        break;
    case P_GRABBED:
        mode = 2;
        didt = 1;
        break;
    case P_NACTIONS:
        break;
    }

    /* direction / follow-up refinement of the chosen action */
    switch (act) {
    case P_READY:
        if ((pl->shield_flags & 0x620000U) != 0) {
            act = P_SHIELD_READY;
        }
        if (cur != P_READY && (cur < P_ATTACK_PWRB || cur > P_COMBO_JES) && cur != P_HIT_REACT &&
            (u32)(cur - P_SPIKE_HIT) > 1 && (pl->char_type != 3 || cur != P_ATTACK_QUICK2_R)) {
            speed = 0.066667f;
        }
        break;
    case P_WALK:
    case P_WALK2:
    case P_RUN:
    case P_RUN2:
        if ((pl->shield_flags & 0x620000U) != 0) {
            act = P_SHIELD_RUN;
            if (cur == P_SHIELD_RUN) {
                mode = 0;
            }
            didt = 1;
        }
        break;
    case P_ATTACK_QUICK:
    case P_ATTACK_QUICK3:
        ang = pl->melee_yaw;
        if (ang > 2.3561944905) {
            act = P_ATTACK_180;
        } else if (ang < -2.3561944905) {
            act = P_ATTACK_180L;
        } else if (ang > 1.0471975513333334) {
            act = P_ATTACK_RIGHT;
        } else if (ang < -1.0471975513333334) {
            act = P_ATTACK_LEFT;
        }
        break;
    case P_ATTACK_QUICK2:
        ang = pl->melee_yaw;
        if (ang > 2.3561944905) {
            act = P_ATTACK_1802;
        } else if (ang < -2.3561944905) {
            act = P_ATTACK_180L2;
        } else if (ang > 1.0471975513333334) {
            act = P_ATTACK_RIGHT2;
        } else if (ang < -1.0471975513333334) {
            act = P_ATTACK_LEFT2;
        }
        break;
    case P_THROW:
        if (cur == P_WALK || cur == P_RUN) {
            act = P_THROW2;
        }
        break;
    case P_THROWQ:
        if (cur == P_WALK || cur == P_RUN) {
            act = P_THROW2Q;
        }
        break;
    case P_THROW_STEP:
        if (cur == P_WALK || cur == P_RUN) {
            act = P_THROW_STEP2;
        }
        break;
    case P_ATTACK_STEP:
        if (cur == P_WALK2 || cur == P_RUN2) {
            act = P_ATTACK_WALK2;
        } else if (cur == P_ATTACK_QUICK || cur == P_ATTACK_QUICK3) {
            act = P_ATTACK_Q3TOSTEP1;
        }
        /* fallthrough */
    case P_ATTACK_STEP3:
        ang = pl->melee_yaw;
        if (ang > 2.3561944905) {
            act = P_ATTACK_180;
        } else if (ang < -2.3561944905) {
            act = P_ATTACK_180L;
        } else if (ang > 1.0471975513333334) {
            act = P_ATTACK_RIGHT;
        } else if (ang < -1.0471975513333334) {
            act = P_ATTACK_LEFT;
        }
        break;
    case P_ATTACK_STEP2:
        ang = pl->melee_yaw;
        if (ang > 2.3561944905) {
            act = P_ATTACK_1802;
        } else if (ang < -2.3561944905) {
            act = P_ATTACK_180L2;
        } else if (ang > 1.0471975513333334) {
            act = P_ATTACK_RIGHT2;
        } else if (ang < -1.0471975513333334) {
            act = P_ATTACK_LEFT2;
        }
        break;
    case P_ATTACK_PWRA_CLOSE:
        if ((pl->coll_flags & 2U) != 0) {
            act = P_ATTACK_PWRA_LOW;
        }
        break;
    }

    /* resolve the sequence, falling back on 0x23/0x24 for missing dances */
    d = act;
    switch (act) {
    case P_ATTACK_PWRA_LOW:
        if (defs[act].seq < 0) {
            d = P_ATTACK_PWRA_CLOSE;
        }
        break;
    case P_ATTACK_PWRA_LOW_R:
        if (defs[act].seq < 0) {
            d = P_ATTACK_PWRA_CLOSE_R;
        }
        break;
    }
    atree->repeat = (s16)didt;
    {
        s32 rawSeq = defs[d].seq;
        seq = rawSeq;
        if (rawSeq < 0) {
            seq = 0;
        }
    }

    /* per-action animation speed */
    if ((pl->hud_flags & 0xD0) != 0 || atkNext >= 0xB ||
        atkNext == 1) {
        atree->animscale = 1.0f;
    } else if ((act >= P_COMBO_ACTIVE1 && act <= P_COMBO_ACTIVE3) ||
               (act >= P_COMBO_WAR1 && act <= P_COMBO_JES)) {
        atree->animscale = 1.0f;
    } else if ((pl->obj_flags & 0x8000U) != 0 && act >= P_KNOCKBACK) {
        atree->animscale = 2.0f;
    } else if ((pl->field_11C & 0x20000000U) != 0 &&
               (u32)(atkNext - 9) <= 1) {
        atree->animscale = 0.75f;
    } else if (act == P_DEFEND2) {
        atree->animscale = (f32)(0.2 * pl->stat_armor);
        if (atree->animscale < 0.25) {
            atree->animscale = 0.25f;
        }
    } else if ((pl->flags & 0x10000U) != 0) {
        atree->animscale = 0.75f;
    } else {
        atree->animscale = 1.0f;
    }

    adv = DoAnimateTree(speed, node, seq, frame, mode, 1);
    if (adv != 0) {
        seq = PlayerAttackType(act);
    } else {
        seq = PlayerAttackType(cur);
    }
    pl->action = seq;

    if (adv != 0) {
        pl->combo_cd = -1.0f;
        switch (cur) {
        case P_IDLE1:
            pl->vibe_timer = 0;
            pl->vibe_timer2 = 1;
            break;
        case P_IDLE2_LOOP:
            if (act != P_IDLE2_LOOP) {
                pl->vibe_timer = 0;
                pl->vibe_timer2 = 0;
            }
            break;
        case P_ATTACK_QUICK:
        case P_ATTACK_QUICK2:
        case P_ATTACK_QUICK3:
        case P_ATTACK_RIGHT:
        case P_ATTACK_RIGHT2:
        case P_ATTACK_LEFT:
        case P_ATTACK_LEFT2:
        case P_ATTACK_180:
        case P_ATTACK_1802:
        case P_ATTACK_180L:
        case P_ATTACK_180L2:
        case P_ATTACK_360:
        case P_ATTACK_LOW:
        case P_ATTACK_LOW2:
            pl->act_bits |= 2;
            break;
        case P_ATTACK_SLOW1:
        case P_ATTACK_STEP:
        case P_ATTACK_STEP2:
        case P_ATTACK_STEP3:
        case P_ATTACK_Q3TOSTEP1:
        case P_ATTACK_WALK2:
            pl->act_bits |= 4;
            break;
        case P_ATTACK_KICK:
            pl->act_bits |= 8;
            break;
        case P_ATTACK_PWRA_LOW:
            pl->act_bits |= 0x10;
            break;
        case P_ATTACK_PWRA_CLOSE:
        case P_ATTACK_PWRA_MED:
            if (pl->char_type != 6) {
                pl->act_bits |= 0x10;
            }
            break;
        case P_ATTACK_PWRA_THROW:
            if (pl->char_type != 6 || pl->quest_state >= 2) {
                pl->act_bits |= 0x1000;
            }
            break;
        case P_STRAFE_ATKF:
        case P_STRAFE_ATKF2:
        case P_STRAFE_ATKB:
        case P_STRAFE_ATKB2:
        case P_STRAFE_ATKL:
        case P_STRAFE_ATKL2:
        case P_STRAFE_ATKR:
        case P_STRAFE_ATKR2:
        case P_THROW_RELEASE:
        case P_THROW2_RELEASE:
        case P_THROW_STEP:
        case P_THROW_STEP2:
            pl->act_bits |= 0x100;
            break;
        case P_THROW_RECOVER:
        case P_THROW2_RECOVER:
        case P_ATTACK_PWRA_THROW_R:
            if (pl->weaphold_node != 0 &&
                (pl->char_type & 3) != 2 && pl->char_type != 3) {
                MBTreeClearFlags(pl->weaphold_node, 2, 0);
            }
            break;
        case P_SSHOT:
        case P_SSHOT2:
            if ((u32)(act - P_SSHOT2) <= 1) {
                pl->act_bits |= 0x800;
            }
            break;
        case P_FIREL:
            if (act == P_FIREL_R) {
                pl->act_bits |= 0x2000;
            }
            break;
        case P_FIRER:
            if (act == P_FIRER_R) {
                pl->act_bits |= 0x4000;
            }
            break;
        case P_SSHOT_R:
        case P_BREATHE:
            break;
        case P_SHOVE:
        case P_WALK:
        case P_RUN:
        case P_SHIELD_RUN:
            pl->grab_flags |= 1;
            break;
        case P_WALK2:
        case P_RUN2:
            pl->grab_flags |= 2;
            break;
        }

        pl->hud_flags &= ~0xC702;
        switch (act) {
        case P_ATTACK_SLOW:
        case P_ATTACK_SLOW1:
        case P_ATTACK_QUICK:
        case P_ATTACK_QUICK2:
        case P_ATTACK_QUICK3:
        case P_ATTACK_RIGHT:
        case P_ATTACK_RIGHT2:
        case P_ATTACK_LEFT:
        case P_ATTACK_LEFT2:
        case P_ATTACK_180:
        case P_ATTACK_1802:
        case P_ATTACK_180L:
        case P_ATTACK_180L2:
        case P_ATTACK_360:
        case P_ATTACK_STEP:
        case P_ATTACK_STEP2:
        case P_ATTACK_STEP3:
        case P_ATTACK_Q3TOSTEP1:
        case P_ATTACK_WALK2:
        case P_ATTACK_LOW:
        case P_ATTACK_LOW2:
        case P_ATTACK_KICK:
            pl->act_bits |= 1;
            if (pl->field_8F8 != 0) {
                pl->field_908 = pl->field_908 + 1;
            } else {
                pl->field_908 = 0;
            }
            pl->field_8F8 = 0;
            break;
        case P_THROW:
        case P_THROWQ:
        case P_THROW2:
        case P_THROW2Q:
        case P_THROW_STEP:
        case P_THROW_STEP2:
            pl->act_bits |= 1;
            pl->field_8F8 = 0;
            break;
        case P_THROW_RECOVER:
        case P_THROW2_RECOVER:
            if (pl->weaphold_node != 0 &&
                (pl->char_type & 3) != 2 && pl->char_type != 3) {
                MBTreeSetFlags(pl->weaphold_node, 2, 0);
            }
            break;
        case P_SSHOT:
        case P_SSHOT2:
            pl->act_bits |= 1;
            pl->field_8F8 = 0;
            break;
        case P_ATTACK_PWRA_THROW_R:
            if (pl->weaphold_node != 0 &&
                (pl->char_type & 3) != 2 && pl->char_type != 3) {
                MBTreeSetFlags(pl->weaphold_node, 2, 0);
            }
            break;
        case P_USE_MAGIC:
            pl->act_bits |= 0x10000;
            pl->field_8F8 = 0;
            break;
        case P_MAGIC_RELEASE:
            pl->act_bits |= 0x20000;
            pl->field_8F8 = 0;
            break;
        case P_THROW_MAGIC:
            pl->act_bits |= 0x10000;
            pl->field_8F8 = 0;
            pl->throw_str = 0;
            break;
        case P_THROW_MAGIC_RELEASE:
            pl->act_bits |= 0x40000;
            pl->field_8F8 = 0;
            break;
        case P_BREATHE:
            pl->act_bits |= 0x1000000;
            pl->field_8F8 = 0;
            break;
        case P_HAMMER_R:
            pl->act_bits |= 0x2000000;
            pl->field_8F8 = 0;
            break;
        case P_VICTORY:
            pl->hud_flags |= 0x800;
            /* fallthrough */
        case P_ACTION_DEATH:
        case P_FALL_DOWN:
        case P_GET_UP:
        case P_FALL_DOWN_FWD:
        case P_GET_UP_FWD:
        case P_WHIRLWIND:
            pl->hud_flags |= 2;
            break;
        case P_DEFEND1:
            pl->hud_flags |= 0x100;
            break;
        case P_DEFENDL:
        case P_DEFENDR:
        case P_DEFENDB:
        case P_DEFENDF:
        case P_DEFEND2:
            pl->hud_flags |= 0x200;
            break;
        case P_STRAFE_WLKF:
        case P_STRAFE_WLKF2:
        case P_STRAFE_WLKB:
        case P_STRAFE_WLKB2:
        case P_STRAFE_WLKL:
        case P_STRAFE_WLKL2:
        case P_STRAFE_WLKR:
        case P_STRAFE_WLKR2:
            pl->hud_flags |= 0x4000;
            break;
        case P_STRAFE_ATKF:
        case P_STRAFE_ATKF2:
        case P_STRAFE_ATKB:
        case P_STRAFE_ATKB2:
        case P_STRAFE_ATKL:
        case P_STRAFE_ATKL2:
        case P_STRAFE_ATKR:
        case P_STRAFE_ATKR2:
            pl->hud_flags |= 0x8000;
            pl->act_bits |= 1;
            pl->field_8F8 = 0;
            break;
        case P_SHOVE:
            pl->hud_flags |= 0x400;
            pl->field_8F8 = 0;
            break;
        case P_ATTACK_SLOW1_R:
        case P_ATTACK_QUICK2_R:
        case P_ATTACK_QUICK3_R:
        case P_ATTACK_RIGHT_R:
        case P_ATTACK_RIGHT2_R:
        case P_ATTACK_LEFT_R:
        case P_ATTACK_LEFT2_R:
        case P_ATTACK_180_R:
        case P_ATTACK_1802_R:
        case P_ATTACK_180L_R:
        case P_ATTACK_180L2_R:
        case P_ATTACK_360_R:
        case P_ATTACK_STEP2_R:
        case P_ATTACK_STEP3_R:
        case P_ATTACK_Q3TOSTEP1_R:
        case P_ATTACK_WALK2_R:
        case P_ATTACK_LOW_R:
        case P_ATTACK_KICK_R:
            break;
        default:
            pl->field_8F8 = 0;
            break;
        }
    }

    /* per-action move / turn scales */
    pl->field_A54 = 1.0f;
    pl->field_A50 = 1.0f;
    if (cur >= P_ATTACK_SLOW && cur < P_LAST_ATTACK) {
        if (cur >= P_SSHOT) {
            pl->field_A48 = 0.0f;
            pl->field_A4C = 0.5f;
            pl->field_A54 = 0.0f;
        } else if (cur >= P_FIREL) {
            pl->field_A48 = 0.25f;
            pl->field_A4C = 1.0f;
        } else if (cur >= P_THROW_STEP) {
            pl->field_A48 = 1.0f;
            pl->field_A4C = 1.0f;
        } else if (cur >= P_ATTACK_PWRA_THROW) {
            pl->field_A48 = 0.25f;
            pl->field_A4C = 1.0f;
        } else if (cur >= P_THROW) {
            pl->field_A48 = 0.0f;
            pl->field_A4C = 0.5f;
        } else if (cur >= P_COMBO_ACTIVE1) {
            pl->field_A48 = 0.0f;
            pl->field_A4C = 0.0f;
            pl->field_A54 = 0.0f;
        } else if (cur >= P_ATTACK_PWRC) {
            if (pl->char_type == 6 && atree->frame > 11.0f) {
                pl->field_A4C = 0.0f;
                pl->field_A48 = 0.0f;
            } else {
                pl->field_A4C = 0.25f;
                pl->field_A48 = 0.0f;
            }
            pl->field_A54 = 0.0f;
        } else if (cur >= P_ATTACK_PWRB) {
            pl->field_A48 = 0.0f;
            pl->field_A4C = 1.0f;
            pl->field_A54 = 0.0f;
        } else if (cur >= P_ATTACK_PWRA_LOW) {
            pl->field_A4C = 1.0f;
            pl->field_A48 = 0.25f;
            pl->field_A54 = 0.0f;
        } else if (cur >= P_ATTACK_LOW) {
            pl->field_A48 = 1.0f;
            pl->field_A4C = 1.0f;
        } else if (cur >= P_STRAFE_ATKF) {
            pl->field_A48 = 0.667f;
            pl->field_A4C = 1.0f;
        } else if (cur >= P_ATTACK_STEP) {
            pl->field_A48 = 1.0f;
            pl->field_A4C = 0.25f;
        } else if (cur >= P_ATTACK_360) {
            pl->field_A48 = 0.5f;
            pl->field_A4C = 1.0f;
        } else if (cur >= P_ATTACK_180) {
            pl->field_A48 = 1.0f;
            pl->field_A4C = 1.0f;
        } else if (cur >= P_ATTACK_RIGHT) {
            pl->field_A48 = 1.0f;
            pl->field_A4C = 1.0f;
        } else if (cur >= P_ATTACK_QUICK) {
            pl->field_A48 = 0.25f;
            pl->field_A4C = 0.0f;
        } else if (cur == P_ATTACK_PWRA_MED) {
            if (pl->char_type == 7 || pl->char_type == 6) {
                pl->field_A48 = 0.0f;
                pl->field_A4C = 0.0f;
            } else if ((u32)(pl->char_type - 2) <= 1) {
                pl->field_A48 = 0.25f;
                pl->field_A4C = 1.0f;
            } else {
                pl->field_A48 = 0.5f;
                pl->field_A4C = 1.0f;
            }
            pl->field_A54 = 0.0f;
        } else if (cur >= P_ATTACK_PWRA_CLOSE) {
            if (pl->char_type == 5 || pl->char_type == 6) {
                pl->field_A48 = 0.0f;
                pl->field_A4C = 0.0f;
            } else if (pl->char_type == 2) {
                pl->field_A48 = 0.25f;
                pl->field_A4C = 1.0f;
            } else {
                pl->field_A48 = 0.5f;
                pl->field_A4C = 1.0f;
            }
            pl->field_A54 = 0.0f;
        } else {
            pl->field_A48 = 0.0f;
            pl->field_A4C = 1.0f;
        }
    } else {
        if (cur == P_STUCK) {
            pl->field_A48 = 0.4f;
            pl->field_A4C = 0.5f;
        } else if ((u32)(cur - P_RUN) <= 1 || cur == P_SHIELD_RUN) {
            pl->field_A48 = 1.3f;
            pl->field_A4C = 1.0f;
        } else if (cur == P_COMBO_DWF2) {
            pl->field_A48 = 1.5f;
            pl->field_A4C = 0.5f;
        } else if (cur == P_SHOVE) {
            pl->field_A48 = 1.5f;
            pl->field_A4C = 1.0f;
        } else if (cur >= P_DEFEND1 && cur <= P_DEFENDF) {
            pl->field_A48 = 0.0f;
            pl->field_A4C = 0.0f;
        } else if (cur >= P_STRAFE_WLKF && cur <= P_STRAFE_WLKR2) {
            pl->field_A48 = 0.667f;
            pl->field_A4C = 1.0f;
        } else if (cur == P_VICTORY) {
            pl->field_A48 = 1.0f;
            pl->field_A4C = 1.0f;
        } else if (cur > P_LAST_ATTACK) {
            pl->field_A50 = 0.0f;
            pl->field_A48 = 1.0f;
            pl->field_A4C = 1.0f;
        } else {
            pl->field_A48 = 1.0f;
            pl->field_A4C = 1.0f;
        }
    }

    if ((gControllerButtons & 1) != 0 &&
        (gControllerButtons & 8) != 0 && pl->index == 0) {
        dbgTextPrintfCol(1, 0x1C,
                         "ACTION:%s NEXT:%s D:%s INT:%d RPT:%d DIDT:%d",
                         action_names[cur], action_names[act],
                         action_names[next], mode, didt, adv);
        dbgTextPrintfCol(1, 0x1D, "  SEQ:%s  frame:%.1f/%d      ",
                         (char*)((s32)atree->seqheader + atree->animseq * 0x30),
                         atree->frame, (s32)atree->numframes);
    }

    if (adv != 0) {
        pl->anim_208 = act;
    } else {
        pl->anim_208 = cur;
    }
}

/* 0x800ADBFC  classify an atree sequence index into a player attack type
 * (1 = quick attacks, 2..12 = melee/turbo family bands, 0 = not an attack). */
s32 PlayerAttackType(s32 seq)
{
    if (seq >= P_ATTACK_SLOW && seq < P_LAST_ATTACK) {
        if (seq < P_ATTACK_QUICK) {
            return 2;
        }
        if (seq < P_ATTACK_RIGHT) {
            return 3;
        }
        if (seq < P_ATTACK_360) {
            return 4;
        }
        if (seq < P_ATTACK_STEP) {
            return 5;
        }
        if (seq < P_STRAFE_ATKF) {
            return 6;
        }
        if (seq < P_ATTACK_LOW) {
            return 7;
        }
        if (seq < P_ATTACK_PWRB) {
            return 8;
        }
        if (seq < P_COMBO_ACTIVE1) {
            return 11;
        }
        if (seq < P_THROW) {
            return 12;
        }
        if (seq < P_ATTACK_PWRA_THROW) {
            return 9;
        }
        if (seq < P_SSHOT) {
            return 10;
        }
        return 11;
    }
    if (seq >= P_COMBO_WAR1 && seq <= P_COMBO_JES) {
        return 12;
    }
    if (seq >= P_DEFEND1 && seq <= P_DEFEND3) {
        return 1;
    }
    if (seq >= P_DEFENDL && seq <= P_DEFENDF) {
        return 1;
    }
    if (seq == P_SHOVE) {
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
            defs[i].frames = *(s16*)(atree->seqs + seq * sizeof(ATREESEQ) +
                                      offsetof(ATREESEQ, frames));
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
    if (action >= E_ATTACK && action <= E_ATTACK5 && e->actTimer > 0.0f) {
        return;
    }
    if (action >= E_THROW && action <= E_THROW_FINISH && e->actTimer > 0.0f) {
        return;
    }
    if (e_actpri[e->action] >= e_actpri[action]) {
        return;
    }
    e->action = action;
}
