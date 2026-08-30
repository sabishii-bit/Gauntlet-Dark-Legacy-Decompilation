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
void SfxDeleteParented(void* parent, s32 a, s32 b);
void dbgTextPrintfCol(int x, int y, const char* fmt, ...);
s32 AnimateATree(void* node, s32 seq, s32 mode);
u32 DoAnimateTree(f32 t, void* node, s32 seq, u32 frame, s32 mode, s32 e);
s32 StartEnemyAtkFX(void* a, s32 b);
void MBTreeSetFlags(void* obj, u32 flags, s32 recurse);
void MBTreeClearFlags(void* obj, u32 flags, s32 recurse);

extern u32 sFlags;      /* 0x803445CC anim debug flags */
extern u64 gControllerButtons; /* 0x803445C8 config-word pair */
extern u8* gCurLevel;   /* 0x8034483C current level record */
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
    f32* ef = (f32*)enemy;
    u8* e70 = (u8*)e + 0x70; /* interruptible flag at +0x34, walk rate at +0x10 */
    s32* defs = e + 0x35; /* ACTIONDEF[34] at +0xD4 */
    s32 next = e[0x34];   /* +0xD0 requested action */
    s32* node;            /* atree node at +0x6C */
    s32 act;
    s32 cur;
    s32 mode = 2;
    s32 interruptible = 1;
    s32 sw;
    s32 seq;
    s32 result;
    s32 type;

    node = e + 0x1B;
    cur = e[0x33];        /* +0xCC current action */
    act = next;
    if (act >= 0x1C) {
        sw = 0;
    } else {
        sw = cur;
    }
    switch (sw) {
    case 1:
        type = e[0];
        mode = 0;
        interruptible = 0;
        if (type == 1 || type == 4 || type == 10 || type == 7) {
            if (defs[3 * 2] >= 0) {
                act = 3;
            } else {
                act = 4;
            }
        }
        break;
    case 0:
        if (e[0] == 0x1D) {
            if (next >= 0x1C) {
                mode = 2;
            } else {
                mode = 0;
            }
            interruptible = 0;
        }
        if (e[0] == 0x1B) {
            mode = 2;
        } else if (next == 3 && defs[9 * 2] >= 0) {
            act = 9;
            mode = 0;
        } else if (next == 4 && defs[11 * 2] >= 0) {
            act = 0xB;
            mode = 0;
        } else if (next == 0xC || next == 0xE || next == 0x10) {
            mode = 0;
        }
        break;
    case 0xB:
        if (next >= 0x1C) {
            mode = 2;
        } else {
            mode = 0;
            interruptible = 0;
            if (defs[4 * 2] >= 0) {
                act = 4;
            } else {
                act = 3;
            }
        }
        break;
    case 9:
        if (next >= 0x1C) {
            mode = 2;
        } else {
            mode = 0;
            interruptible = 0;
            if (defs[3 * 2] >= 0) {
                act = 3;
            } else {
                act = 4;
            }
        }
        break;
    case 8:
    case 10:
        if (next >= 0x1C) {
            mode = 2;
        } else {
            mode = 0;
            interruptible = 0;
            act = 0;
        }
        break;
    case 3:
    case 5:
    case 6:
    case 7:
        if (e[0] == 0x1D && next == 0) {
            mode = 0;
            interruptible = 0;
            act = 0;
        }
        if (next == 0) {
            if (defs[8 * 2] >= 0) {
                act = 8;
            }
            mode = 0;
        }
        break;
    case 4:
        if (next == 0 && defs[10 * 2] >= 0) {
            act = 10;
            mode = 0;
        }
        break;
    case 0x1C:
        mode = 0;
        interruptible = 0;
        break;
    case 0x1D:
        mode = 0;
        interruptible = 0;
        if (defs[0x1F * 2] >= 0) {
            act = 0x1F;
        }
        break;
    case 0x1E:
        mode = 0;
        interruptible = 0;
        break;
    case 0x20:
        mode = 0;
        interruptible = 0;
        break;
    case 0x1F:
        mode = 0;
        interruptible = 0;
        break;
    case 2:
        mode = 0;
        interruptible = 0;
        break;
    case 0xC:
        mode = 0;
        interruptible = 0;
        if (next >= 0x1C) {
            mode = 2;
        } else {
            act = 0xD;
        }
        break;
    case 0xD:
        mode = 0;
        interruptible = 0;
        if (next >= 0x1C) {
            mode = 2;
        } else {
            if (defs[14 * 2] >= 0) {
                act = 0xE;
            } else {
                if (next == 0xC) {
                    act = 0xC;
                }
            }
        }
        break;
    case 0xE:
        mode = 0;
        interruptible = 0;
        if (next >= 0x1C) {
            mode = 2;
        } else {
            act = 0xF;
        }
        break;
    case 0xF:
        mode = 0;
        interruptible = 0;
        if (next >= 0x1C) {
            mode = 2;
        } else {
            if (next == 0xC) {
                act = 0xC;
            }
        }
        break;
    case 0x10:
        mode = 0;
        interruptible = 0;
        if (next >= 0x1C) {
            mode = 2;
        } else {
            act = 0x11;
        }
        break;
    case 0x11:
        mode = 0;
        interruptible = 0;
        if (next >= 0x1C) {
            mode = 2;
        }
        break;
    case 0x12:
        mode = 0;
        interruptible = 0;
        if (next >= 0x1C) {
            mode = 2;
        } else {
            act = 0x13;
        }
        break;
    case 0x13:
        if (next == 0 && e[0xA1] >= 0) {
            act = 0xC;
        }
        break;
    case 0x14:
        mode = 0;
        interruptible = 0;
        if (next >= 0x1C) {
            mode = 2;
        } else {
            act = 0x15;
        }
        break;
    case 0x15:
        if (next == 0 && e[0xA1] >= 0) {
            act = 0xC;
        }
        break;
    case 0x16:
        mode = 0;
        interruptible = 0;
        if (next >= 0x1C) {
            mode = 2;
        } else {
            if (next == 0x16) {
                act = 0x17;
            }
        }
        break;
    case 0x17:
        mode = 0;
        interruptible = 0;
        if (next >= 0x1C) {
            mode = 2;
        } else {
            if (next == 0x16) {
                act = 0x16;
            }
        }
        break;
    case 0x18:
        if (defs[0x18 * 2] < 0) {
            act = 0x19;
            cur = 0x19;
        }
        /* fallthrough */
    case 0x19:
        mode = 0;
        interruptible = 0;
        if (next >= 0x1C) {
            mode = 2;
        } else {
            act = 0x1A;
        }
        break;
    case 0x1A:
        mode = 0;
        interruptible = 0;
        if (next >= 0x1C) {
            mode = 2;
        } else {
            if (next == 0x18) {
                act = 0x19;
            } else if (next == 0) {
                act = 0x1B;
            }
        }
        break;
    case 0x1B:
        mode = 0;
        interruptible = 0;
        act = 0;
        if (next >= 0x1C) {
            mode = 2;
        }
        break;
    case 0x21:
        break;
    }

    /* missing-sequence fallbacks */
    switch (act) {
    case 0xE:
    case 0x10:
    case 0x12:
    case 0x14:
        if (defs[act * 2] < 0) {
            act = 0xC;
        }
        break;
    case 3:
    case 0xB:
        if (defs[act * 2] < 0) {
            act = 4;
        }
        break;
    case 4:
    case 9:
        if (defs[act * 2] < 0) {
            act = 3;
        }
        break;
    case 8:
    case 10:
    case 0x1B:
        if (defs[act * 2] < 0) {
            act = 0;
        }
        break;
    case 0x19:
        if (defs[act * 2] < 0) {
            act = 0x18;
        }
        break;
    }

    seq = defs[act * 2];
    if (seq < 0 && act == 0x20) {
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
    *(s16*)(e70 + 0x34) = interruptible;
    result = AnimateATree(node, seq, mode);

    if (result != 0) {
        switch (cur) {
        case 0:
            break;
        case 0xC:
        case 0xE:
        case 0x12:
        case 0x14:
            if (act == 0xD || act == 0xF || act == 0x13 || act == 0x15) {
                e[0xB4] |= 1;
            }
            break;
        case 0x10:
            if (act == 0x11) {
                e[0xB4] |= 2;
            }
            break;
        case 0x18:
        case 0x19:
            if (act == 0x1A) {
                e[0xB4] |= 0x10;
            }
            break;
        case 0x16:
            if (act == 0x17) {
                e[0xB4] |= 0x10;
            }
            break;
        }
        switch (act) {
        case 0xC:
        case 0xE:
            if (e[0] == 0x1B) {
                SfxSetParent((void*)StartEnemyAtkFX(0, 0), (void*)e[0x19]);
            }
            break;
        case 0x10:
            if (e[0] == 0x1B) {
                SfxSetParent((void*)StartEnemyAtkFX(0, 1), (void*)e[0x19]);
            }
            break;
        case 0xD:
        case 0xF:
        case 0x11:
        case 0x1D:
        case 0x20:
            break;
        default:
            if (e[0] == 0x1B) {
                SfxDeleteParented((void*)e[0x19], 0, -1);
            }
            break;
        }
    }

    if (result != 0) {
        f32 dur = 0.0f;
        f32 accum = 0.0f;

        if (act >= 0x18 && act <= 0x1A) {
            dur = ef[0xDE] * *(f32*)(gCurLevel + 0xC0) + ef[0xE0];
        }
        if (dur > 0.0) {
            while (dur > 1.0) {
                accum += 1.0;
                dur = (f32)(dur - 1.0);
            }
            ef[0xE0] = dur;
            ef[0xDF] = accum;
            if (ef[0xDF] >= 1.0) {
                ef[0xDF] = (f32)(0.0333333333 * (s32)*(s16*)(e70 + 0x10) +
                                 ef[0xDF]);
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
    f32* pf = (f32*)player;
    f32* atree = (f32*)((u8*)player + 0x80);
    void* node = (u8*)player + 0x7C;
    ACTIONDEF* defs = (ACTIONDEF*)((u8*)player + 0x210);
    char** action_names = lbl_80126C68;
    s32 rpt = 0;
    s32 next;
    s32 cur;
    s32 atkNext;
    s32 atkCur;
    s32 atkD;
    f32 speed;
    s32 mode = 0;
    s32 didt = 0;
    u32 frame = 0;
    s32 d;
    s32 act;
    s32 seq;
    s32 combo;
    s32 flags;
    void* mbobj;
    s32 adv;
    f32 ang;
    s32 dance;
    u8 unused[8];

    next = p[0x83];
    cur = p[0x82];
    *((u8*)p + 0x93) |= 2;
    atkNext = PlayerAttackType(next);
    atkCur = PlayerAttackType(cur);
    speed = 0.0f;
    d = cur;
    if (next >= 0x73 && atkCur != 0 && atkCur < 0xB) {
        d = 0;
    }
    if (next >= 0x1D && next < 0x20) {
        mode = 2;
    }
    if (next >= 0x83 && next <= 0x94) {
        mode = 2;
        if (next == 0x94) {
            rpt = 2;
        } else {
            rpt = 1;
        }
    }
    atkD = PlayerAttackType(d);
    if (atkD != 0 && atkD < 0xB && atkNext >= 11) {
        d = 0;
        mode = 2;
    }
    if ((atkD < 2 || atkD > 6) && atkD != 8) {
        p[0x242] = 0;
    }
    p[0x201] = 0;
    act = next;
    dance = 0;
    switch (d) {
    case 0x7D:
        mode = 2;
        break;
    case 0:
        didt = 1;
        mode = 2;
        if (next == 0 && p[0x20D] == 0) {
            s32 hi, lo;
            if ((gControllerButtons & 0x10) != 0) {
                hi = 0xB4;
            } else {
                hi = 0x708;
            }
            if (gControllerButtons != 0) {
                lo = 0x3C;
            } else {
                lo = 600;
            }
            if (*(s16*)((u8*)p + 0x200) > hi) {
                mode = 1;
                act = 1;
            } else if (*(s16*)((u8*)p + 0x202) > lo) {
                act = 2;
                mode = 1;
            }
        }
        break;
    case 1:
        didt = 0;
        mode = 2;
        if (next == 0) {
            mode = 1;
            act = 0;
        }
        break;
    case 2:
        didt = 0;
        mode = 2;
        if (next == 0) {
            mode = 1;
            act = 3;
        }
        break;
    case 3:
        didt = 1;
        mode = 2;
        if (next == 0) {
            mode = 0;
            act = 3;
        }
        break;
    case 0x15:
        didt = 1;
        mode = 2;
        break;
    case 4:
    case 5:
    case 6:
    case 7:
    case 0x77:
    case 0x78:
        mode = 1;
        if (d == 0x77) {
            act = 0x78;
        } else {
            act = 0x79;
        }
        break;
    case 9:
    case 0xB:
        if (next >= 0x20 || atkNext != 0) {
            mode = 2;
        }
        if (next == 9) {
            act = 10;
        }
        if (next == 0xB) {
            act = 0xC;
        }
        break;
    case 10:
    case 0xC:
        if (next >= 0x20 || atkNext != 0) {
            mode = 2;
        }
        if (next == 9) {
            act = 9;
        }
        if (next == 0xB) {
            act = 0xB;
        }
        break;
    case 0xD:
    case 0xF:
        if (next >= 0x20 || atkNext != 0) {
            mode = 2;
        }
        if (next == 0xD) {
            act = 0xE;
        }
        if (next == 0xF) {
            act = 0x10;
        }
        break;
    case 0xE:
    case 0x10:
        if (next >= 0x20 || atkNext != 0) {
            mode = 2;
        }
        if (next == 0xD) {
            act = 0xD;
        }
        if (next == 0xF) {
            act = 0xF;
        }
        break;
    case 0x47:
    case 0x49:
        if (next == 0x47) {
            act = 0x48;
        }
        if (next == 0x49) {
            act = 0x4A;
        }
        mbobj = *(void**)((u8*)p + 0x6E0);
        if (mbobj != 0 && (p[2] & 3) != 2 && p[2] != 3) {
            if (atree[6] < 2.0f) {
                MBTreeSetFlags(mbobj, 2, 0);
            } else {
                MBTreeClearFlags(mbobj, 2, 0);
            }
        }
        break;
    case 0x48:
    case 0x4A:
        if (next == 0x47) {
            act = 0x47;
        }
        if (next == 0x49) {
            act = 0x49;
        }
        mbobj = *(void**)((u8*)p + 0x6E0);
        if (mbobj != 0 && (p[2] & 3) != 2 && p[2] != 3) {
            if (atree[6] < 2.0f) {
                MBTreeSetFlags(mbobj, 2, 0);
            } else {
                MBTreeClearFlags(mbobj, 2, 0);
            }
        }
        break;
    case 0x4B:
    case 0x4D:
        if (next == 0x4B) {
            act = 0x4C;
        }
        if (next == 0x4D) {
            act = 0x4E;
        }
        mbobj = *(void**)((u8*)p + 0x6E0);
        if (mbobj != 0 && (p[2] & 3) != 2 && p[2] != 3) {
            if (atree[6] < 2.0f) {
                MBTreeSetFlags(mbobj, 2, 0);
            } else {
                MBTreeClearFlags(mbobj, 2, 0);
            }
        }
        break;
    case 0x4C:
    case 0x4E:
        if (next == 0x4B) {
            act = 0x4B;
        }
        if (next == 0x4D) {
            act = 0x4D;
        }
        mbobj = *(void**)((u8*)p + 0x6E0);
        if (mbobj != 0 && (p[2] & 3) != 2 && p[2] != 3) {
            if (atree[6] < 2.0f) {
                MBTreeSetFlags(mbobj, 2, 0);
            } else {
                MBTreeClearFlags(mbobj, 2, 0);
            }
        }
        break;
    case 0x17:
    case 0x18:
        mode = 2;
        break;
    case 0x11:
        if (next >= 0x1B || atkNext != 0) {
            mode = 2;
        }
        if (next == 0x11) {
            act = 0x12;
        }
        break;
    case 0x12:
        if (next >= 0x1B || atkNext != 0) {
            mode = 2;
        }
        if (next == 0x11) {
            act = 0x11;
        }
        break;
    case 0x13:
        if (next >= 0x1B || atkNext != 0) {
            mode = 2;
        }
        if (next == 0x13) {
            act = 0x14;
        }
        break;
    case 0x14:
        if (next == 0x1B || atkNext != 0) {
            mode = 2;
        }
        if (next == 0x13) {
            act = 0x13;
        }
        break;
    case 0x16:
        if (next >= 0x20 || atkNext != 0) {
            mode = 2;
        }
        break;
    case 8:
        didt = 1;
        if (next >= 0x20) {
            mode = 2;
        }
        break;
    case 0x20:
        if (atkNext >= 0xB || (u32)(atkNext - 9) <= 1) {
            mode = 2;
            frame = (s32)(0.5 + atree[6]);
        } else {
            act = 0x21;
        }
        break;
    case 0x21:
        p[0x201] = 1;
        act = 0x22;
        break;
    case 0x22:
        if (atkNext == 1) {
            mode = 2;
        }
        break;
    case 0x27:
    case 0x28:
    case 0x29:
        flags = p[0x23E];
        if ((flags & 0x400U) != 0 && (combo = p[0x242]) != 0) {
            if (combo == 1) {
                act = 0x23;
            } else if (combo == 2) {
                act = 0x25;
            } else {
                act = 0x3C;
            }
        } else if (flags != 0 || p[0x23D] != 0) {
            if ((p[0x243] & 8U) != 0) {
                act = cur == 0x28 ? 0x2A : 0x2B;
            } else if ((p[0x243] & 4U) != 0) {
                act = cur == 0x28 ? 0x40 : 0x3F;
            } else {
                act = cur == 0x28 ? 0x29 : 0x28;
            }
        } else {
            act = cur == 0x28 ? 0x2A : 0x2B;
        }
        break;
    case 0x2A:
    case 0x2B:
        flags = p[0x23E];
        if ((flags & 0x400U) != 0 && (combo = p[0x242]) != 0) {
            if (combo == 1) {
                act = 0x23;
            } else if (combo == 2) {
                act = 0x25;
            } else {
                act = 0x3C;
            }
        } else if (flags != 0 && atree[6] <= 2.0 &&
                   (p[0x243] & 1U) != 0) {
            act = cur == 0x2A ? 0x29 : 0x28;
            mode = 2;
        }
        if (atkNext == 1) {
            mode = 2;
        }
        break;
    case 0x3E:
    case 0x3F:
    case 0x40:
        flags = p[0x23E];
        if ((flags & 0x400U) != 0 && (combo = p[0x242]) != 0) {
            if (combo == 1) {
                act = 0x23;
            } else if (combo == 2) {
                act = 0x25;
            } else {
                act = 0x3C;
            }
        } else if (flags != 0 || p[0x23D] != 0) {
            if ((p[0x243] & 8U) != 0) {
                s32 gapAct;

                if (cur == 0x3F) {
                    gapAct = 0x41;
                } else {
                    gapAct = 0x42;
                }
                act = gapAct;
            } else if ((p[0x243] & 4U) != 0) {
                s32 gapAct;

                if (cur == 0x3F) {
                    gapAct = 0x40;
                } else {
                    gapAct = 0x3F;
                }
                act = gapAct;
            } else {
                s32 gapAct;

                if (cur == 0x3F) {
                    gapAct = 0x29;
                } else {
                    gapAct = 0x28;
                }
                act = gapAct;
            }
        } else {
            s32 paired_action;

            if (cur == 0x3F) {
                paired_action = 0x41;
            } else {
                paired_action = 0x42;
            }
            act = paired_action;
        }
        break;
    case 0x41:
    case 0x42:
        if ((p[0x23E] & 0x400U) != 0 && (combo = p[0x242]) != 0) {
            if (combo == 1) {
                act = 0x23;
            } else if (combo == 2) {
                act = 0x25;
            } else {
                act = 0x3C;
            }
        } else if (atkNext == 1) {
            mode = 2;
        }
        break;
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x46:
        if (atkNext == 1) {
            mode = 2;
        }
        break;
    case 0x2C:
        act = 0x2E;
        break;
    case 0x2D:
        act = 0x2F;
        break;
    case 0x30:
        act = 0x32;
        break;
    case 0x31:
        act = 0x33;
        break;
    case 0x34:
        act = 0x36;
        break;
    case 0x35:
        act = 0x37;
        break;
    case 0x38:
        act = 0x3A;
        break;
    case 0x39:
        act = 0x3B;
        break;
    case 0x2E:
    case 0x2F:
    case 0x32:
    case 0x33:
    case 0x36:
    case 0x37:
    case 0x3A:
    case 0x3B:
        if (atkNext == 1) {
            mode = 2;
        }
        break;
    case 0x3C:
        p[0x201] = 1;
        if ((p[0x23E] & 0x400U) != 0 && p[0x242] != 0) {
            act = 0x25;
        } else {
            act = 0x3D;
        }
        break;
    case 0x3D:
        if (atkNext == 1) {
            mode = 2;
        }
        break;
    case 0x54:
        if (rpt < 2) {
            mode = 0;
        }
        act = 0x55;
        break;
    case 0x23:
        if (rpt < 2) {
            mode = 0;
        }
        act = 0x24;
        break;
    case 0x25:
        p[0x201] = 1;
        if (rpt < 2) {
            mode = 0;
        }
        act = 0x26;
        break;
    case 0x63:
        if (rpt < 2) {
            mode = 0;
        }
        act = 0x64;
        break;
    case 0x56:
        if (rpt < 2) {
            mode = 0;
        }
        break;
    case 0x57:
        if (rpt < 2) {
            mode = 0;
        }
        break;
    case 0x58:
        mode = 1;
        if (p[0x136] >= 0) {
            act = 0x59;
        } else if (p[0x138] >= 0) {
            act = 0x5A;
        } else if (rpt < 2) {
            mode = 0;
        }
        break;
    case 0x5A:
    case 0x88:
    case 0x8A:
    case 0x8B:
    case 0x8C:
    case 0x8D:
    case 0x8E:
    case 0x90:
    case 0x91:
    case 0x92:
    case 0x93:
        if (rpt < 2) {
            mode = 0;
        }
        break;
    case 0x59:
    case 0x89:
    case 0x8F:
        didt = 1;
        if (next != d) {
            if (p[d * 2 + 0x86] >= 0) {
                act = d + 1;
            }
            mode = 2;
        } else {
            mode = 0;
        }
        break;
    case 0x24:
    case 0x26:
    case 0x55:
    case 0x64:
        if (rpt == 0) {
            mode = 0;
        }
        break;
    case 0x4F:
    case 0x50:
        if (next == 0x4F) {
            if (cur == 0x4F) {
                act = 0x50;
            } else {
                act = 0x4F;
            }
        } else {
            act = 0x51;
        }
        break;
    case 0x51:
        if (atkNext == 1) {
            mode = 2;
        } else if (next == 0x4F) {
            act = 0x50;
            if (p[0x124] < 0) {
                act = 0x4F;
            }
        } else {
            mode = 1;
        }
        break;
    case 0x52:
        if ((p[0x23E] & 0x400U) != 0 && p[0x242] != 0) {
            act = 0x54;
        } else {
            act = 0x53;
        }
        break;
    case 0x53:
        if ((p[0x23E] & 0x400U) != 0 && p[0x242] != 0) {
            act = 0x54;
        } else {
            if (atkNext == 1) {
                mode = 2;
            } else {
                mode = 1;
            }
        }
        break;
    case 0x5D:
    case 0x5E:
        dance = 1;
        /* fallthrough */
    case 0x5B:
    case 0x5C:
        if (atkNext > 1 && atkNext != 9 && atkNext != 10) {
            mode = 2;
            frame = (s32)(0.5 + atree[6]);
        } else if (atkNext == 7) {
            mode = 2;
            frame = (s32)(0.5 + atree[6]);
        } else if (cur == 0x73 || cur == 0x75) {
            mode = 2;
        } else if (cur == 0x65) {
            mode = 2;
            frame = (s32)(0.5 + atree[6]);
        } else {
            if (cur != 0x5B && atree[6] >= 2.0f) {
                mode = 2;
                act = dance ? 0x60 : 0x5F;
            } else {
                act = dance ? 0x60 : 0x5F;
            }
        }
        break;
    case 0x5F:
    case 0x60:
        if (cur == 0x5F) {
            act = 0x61;
        } else {
            act = 0x62;
        }
        break;
    case 0x61:
    case 0x62:
        if (atkNext == 1) {
            mode = 2;
        }
        break;
    case 0x65:
    case 0x66:
        if (atkNext > 1 && atkNext != 9 && atkNext != 10) {
            mode = 2;
            frame = (s32)(0.5 + atree[6]);
        } else if (atkNext == 7) {
            mode = 2;
            frame = (s32)(0.5 + atree[6]);
        } else if (cur == 0x73 || cur == 0x75) {
            mode = 2;
        } else if (cur == 0x65) {
            if (next == 0x65) {
                act = 0x66;
            } else {
                act = 0x65;
            }
        }
        break;
    case 0x6B:
    case 0x6C:
        if (next == 0x6B) {
            didt = 1;
            act = 0x6C;
        } else if (atkNext != 0) {
            mode = 1;
        } else {
            act = 0x6D;
        }
        break;
    case 0x6E:
        if (rpt == 0) {
            mode = 0;
        }
        act = 0x6F;
        break;
    case 0x70:
        if (rpt == 0) {
            mode = 0;
        }
        act = 0x71;
        break;
    case 0x67:
        if (rpt == 0) {
            mode = 0;
        }
        act = 0x69;
        break;
    case 0x68:
        if (rpt == 0) {
            mode = 0;
        }
        act = 0x6A;
        break;
    case 0x73:
        if (next == 0x75) {
            mode = 2;
            act = 0x75;
        } else if ((*(s16*)((u8*)p + 0x956) & 4) == 0) {
            mode = 0;
            act = 0x75;
        } else {
            act = 0x74;
        }
        break;
    case 0x75:
        act = 0x76;
        break;
    case 0x7A:
        didt = 1;
        if (cur == 0 ||
            (atree[6] < 10.0f && *(u16*)((u8*)atree + 0x36) == 0)) {
            mode = 0;
        } else {
            mode = 2;
        }
        break;
    case 0x7B:
        if (rpt == 0) {
            mode = 0;
        }
        didt = 1;
        break;
    case 0x19:
    case 0x1A:
        if (next != 0) {
            mode = 2;
            didt = 1;
        }
        break;
    case 0x7C:
        mode = 0;
        break;
    case 0x7E:
    case 0x84:
    case 0x86:
        mode = 0;
        if (next == cur) {
            act = 0;
        }
        break;
    case 0x1C:
        if (next == cur) {
            act = 0;
        }
        break;
    case 0x1D:
    case 0x1E:
        if (next == 0x1D) {
            didt = 1;
            act = 0x1E;
        } else {
            act = 0x1F;
        }
        break;
    case 0x1B:
        if (act >= 0x82) {
            mode = 3;
        } else {
            mode = 2;
        }
        if (cur == 0 || cur == 0x11 || cur == 0x13) {
            mode = 0;
        }
        break;
    case 0x80:
        didt = 1;
        if (next != 0) {
            mode = 2;
        }
        break;
    case 0x7F:
    case 0x81:
    case 0x82:
        mode = 0;
        if (next >= 0x83) {
            mode = 3;
        }
        if (next == cur) {
            act = 0;
        }
        break;
    case 0x83:
        mode = 0;
        if (next != 0x83) {
            mode = 1;
            act = 0x84;
        }
        break;
    case 0x85:
        mode = 0;
        if (next != 0x85) {
            mode = 1;
            act = 0x86;
        }
        break;
    case 0x87:
        mode = 0;
        if (next != 0x87) {
            mode = 1;
            act = 0x84;
        }
        break;
    case 0x94:
        mode = 2;
        didt = 1;
        break;
    case 0x95:
        break;
    }

    /* direction / follow-up refinement of the chosen action */
    switch (act) {
    case 0:
        if ((p[0x48] & 0x620000U) != 0) {
            act = 0x15;
        }
        if (cur != 0 && (cur < 0x56 || cur > 0x93) && cur != 0x1B &&
            (u32)(cur - 0x81) > 1 && (p[2] != 3 || cur != 0x2A)) {
            speed = 0.0666667f;
        }
        break;
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
        if ((p[0x48] & 0x620000U) != 0) {
            act = 0x16;
            if (cur == 0x16) {
                mode = 0;
            }
            didt = 1;
        }
        break;
    case 0x27:
    case 0x29:
        ang = pf[0x241];
        if (ang > 2.3561944905) {
            act = 0x34;
        } else if (ang < -2.3561944905) {
            act = 0x38;
        } else if (ang > 1.0471975513333334) {
            act = 0x2C;
        } else if (ang < -1.0471975513333334) {
            act = 0x30;
        }
        break;
    case 0x28:
        ang = pf[0x241];
        if (ang > 2.3561944905) {
            act = 0x35;
        } else if (ang < -2.3561944905) {
            act = 0x39;
        } else if (ang > 1.0471975513333334) {
            act = 0x2D;
        } else if (ang < -1.0471975513333334) {
            act = 0x31;
        }
        break;
    case 0x5B:
        if (cur == 0x11 || cur == 0x13) {
            act = 0x5D;
        }
        break;
    case 0x5C:
        if (cur == 0x11 || cur == 0x13) {
            act = 0x5E;
        }
        break;
    case 0x65:
        if (cur == 0x11 || cur == 0x13) {
            act = 0x66;
        }
        break;
    case 0x3E:
        if (cur == 0x12 || cur == 0x14) {
            act = 0x45;
        } else if (cur == 0x27 || cur == 0x29) {
            act = 0x43;
        }
        /* fallthrough */
    case 0x40:
        ang = pf[0x241];
        if (ang > 2.3561944905) {
            act = 0x34;
        } else if (ang < -2.3561944905) {
            act = 0x38;
        } else if (ang > 1.0471975513333334) {
            act = 0x2C;
        } else if (ang < -1.0471975513333334) {
            act = 0x30;
        }
        break;
    case 0x3F:
        ang = pf[0x241];
        if (ang > 2.3561944905) {
            act = 0x35;
        } else if (ang < -2.3561944905) {
            act = 0x39;
        } else if (ang > 1.0471975513333334) {
            act = 0x2D;
        } else if (ang < -1.0471975513333334) {
            act = 0x31;
        }
        break;
    case 0x23:
        if ((p[0x243] & 2U) != 0) {
            act = 0x54;
        }
        break;
    }

    /* resolve the sequence, falling back on 0x23/0x24 for missing dances */
    d = act;
    if (act < 0x55) {
        if (act >= 0x54 && defs[act].seq < 0) {
            d = 0x23;
        }
    } else if (act == 0x55) {
        if (defs[act].seq < 0) {
            d = 0x24;
        }
    }
    *(s16*)((u8*)atree + 0x34) = (s16)didt;
    seq = defs[d].seq;
    if (seq < 0) {
        seq = 0;
    }

    /* per-action animation speed */
    if ((*(s16*)((u8*)p + 0x964) & 0xD0) != 0 || atkNext >= 0xB ||
        atkNext == 1) {
        atree[10] = 1.0f;
    } else if ((act >= 0x58 && act <= 0x5A) ||
               (act >= 0x88 && act <= 0x93)) {
        atree[10] = 1.0f;
    } else if ((p[0x235] & 0x8000U) != 0 && act >= 0x82) {
        atree[10] = 2.0f;
    } else if ((p[0x47] & 0x20000000U) != 0 &&
               (u32)(atkNext - 9) <= 1) {
        atree[10] = 0.75f;
    } else if (act == 0x78) {
        atree[10] = (f32)(0.2 * pf[0x42]);
        if (atree[10] < 0.25) {
            atree[10] = 0.25f;
        }
    } else if ((p[0x49] & 0x10000U) != 0) {
        atree[10] = 0.75f;
    } else {
        atree[10] = 1.0f;
    }

    adv = DoAnimateTree(speed, node, seq, frame, mode, 1);
    if (adv == 0) {
        p[0x23C] = PlayerAttackType(cur);
    } else {
        p[0x23C] = PlayerAttackType(act);
    }

    if (adv != 0) {
        pf[0x296] = -1.0f;
        switch (cur) {
        case 1:
            *(s16*)((u8*)p + 0x200) = 0;
            *(s16*)((u8*)p + 0x202) = 1;
            break;
        case 3:
            if (act != 3) {
                *(s16*)((u8*)p + 0x200) = 0;
                *(s16*)((u8*)p + 0x202) = 0;
            }
            break;
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2C:
        case 0x2D:
        case 0x30:
        case 0x31:
        case 0x34:
        case 0x35:
        case 0x38:
        case 0x39:
        case 0x3C:
        case 0x4F:
        case 0x50:
            p[0x240] |= 2;
            break;
        case 0x21:
        case 0x3E:
        case 0x3F:
        case 0x40:
        case 0x43:
        case 0x45:
            p[0x240] |= 4;
            break;
        case 0x52:
            p[0x240] |= 8;
            break;
        case 0x54:
            p[0x240] |= 0x10;
            break;
        case 0x23:
        case 0x25:
            if (p[2] != 6) {
                p[0x240] |= 0x10;
            }
            break;
        case 0x63:
            if (p[2] != 6 || p[0x20D] >= 2) {
                p[0x240] |= 0x1000;
            }
            break;
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
        case 0x4C:
        case 0x4D:
        case 0x4E:
        case 0x5F:
        case 0x60:
        case 0x65:
        case 0x66:
            p[0x240] |= 0x100;
            break;
        case 0x61:
        case 0x62:
        case 0x64:
            if (*(void**)((u8*)p + 0x6E0) != 0 &&
                (p[2] & 3) != 2 && p[2] != 3) {
                MBTreeClearFlags(*(void**)((u8*)p + 0x6E0), 2, 0);
            }
            break;
        case 0x6B:
        case 0x6C:
            if ((u32)(act - 0x6C) <= 1) {
                p[0x240] |= 0x800;
            }
            break;
        case 0x67:
            if (act == 0x69) {
                p[0x240] |= 0x2000;
            }
            break;
        case 0x68:
            if (act == 0x6A) {
                p[0x240] |= 0x4000;
            }
            break;
        case 8:
        case 0x11:
        case 0x13:
        case 0x16:
            *(s16*)((u8*)p + 0x962) |= 1;
            break;
        case 0x12:
        case 0x14:
            *(s16*)((u8*)p + 0x962) |= 2;
            break;
        }

        *(s16*)((u8*)p + 0x964) &= ~0xC702;
        switch (act) {
        case 0x20:
        case 0x21:
        case 0x27:
        case 0x28:
        case 0x29:
        case 0x2C:
        case 0x2D:
        case 0x30:
        case 0x31:
        case 0x34:
        case 0x35:
        case 0x38:
        case 0x39:
        case 0x3C:
        case 0x3E:
        case 0x3F:
        case 0x40:
        case 0x43:
        case 0x45:
        case 0x4F:
        case 0x50:
        case 0x52:
            p[0x240] |= 1;
            if (p[0x23E] != 0) {
                p[0x242] = p[0x242] + 1;
            } else {
                p[0x242] = 0;
            }
            p[0x23E] = 0;
            break;
        case 0x5B:
        case 0x5C:
        case 0x5D:
        case 0x5E:
        case 0x65:
        case 0x66:
            p[0x240] |= 1;
            p[0x23E] = 0;
            break;
        case 0x61:
        case 0x62:
            if (*(void**)((u8*)p + 0x6E0) != 0 &&
                (p[2] & 3) != 2 && p[2] != 3) {
                MBTreeSetFlags(*(void**)((u8*)p + 0x6E0), 2, 0);
            }
            break;
        case 0x6B:
        case 0x6C:
            p[0x240] |= 1;
            p[0x23E] = 0;
            break;
        case 0x64:
            if (*(void**)((u8*)p + 0x6E0) != 0 &&
                (p[2] & 3) != 2 && p[2] != 3) {
                MBTreeSetFlags(*(void**)((u8*)p + 0x6E0), 2, 0);
            }
            break;
        case 0x73:
            p[0x240] |= 0x10000;
            p[0x23E] = 0;
            break;
        case 0x74:
            p[0x240] |= 0x20000;
            p[0x23E] = 0;
            break;
        case 0x75:
            p[0x240] |= 0x10000;
            p[0x23E] = 0;
            *(s16*)((u8*)p + 0x958) = 0;
            break;
        case 0x76:
            p[0x240] |= 0x40000;
            p[0x23E] = 0;
            break;
        case 0x6E:
            p[0x240] |= 0x1000000;
            p[0x23E] = 0;
            break;
        case 0x71:
            p[0x240] |= 0x2000000;
            p[0x23E] = 0;
            break;
        case 0x7B:
            *(s16*)((u8*)p + 0x964) |= 0x800;
            /* fallthrough */
        case 0x7E:
        case 0x83:
        case 0x84:
        case 0x85:
        case 0x86:
        case 0x87:
            *(s16*)((u8*)p + 0x964) |= 2;
            break;
        case 0x77:
            *(s16*)((u8*)p + 0x964) |= 0x100;
            break;
        case 4:
        case 5:
        case 6:
        case 7:
        case 0x78:
            *(s16*)((u8*)p + 0x964) |= 0x200;
            break;
        case 9:
        case 10:
        case 0xB:
        case 0xC:
        case 0xD:
        case 0xE:
        case 0xF:
        case 0x10:
            *(s16*)((u8*)p + 0x964) |= 0x4000;
            break;
        case 0x47:
        case 0x48:
        case 0x49:
        case 0x4A:
        case 0x4B:
        case 0x4C:
        case 0x4D:
        case 0x4E:
            *(s16*)((u8*)p + 0x964) |= 0x8000;
            p[0x240] |= 1;
            p[0x23E] = 0;
            break;
        case 8:
            *(s16*)((u8*)p + 0x964) |= 0x400;
            p[0x23E] = 0;
            break;
        case 0x22:
        case 0x2A:
        case 0x2B:
        case 0x2E:
        case 0x2F:
        case 0x32:
        case 0x33:
        case 0x36:
        case 0x37:
        case 0x3A:
        case 0x3B:
        case 0x3D:
        case 0x41:
        case 0x42:
        case 0x44:
        case 0x46:
        case 0x51:
        case 0x53:
            break;
        default:
            p[0x23E] = 0;
            break;
        }
    }

    /* per-action move / turn scales */
    pf[0x295] = 1.0f;
    pf[0x294] = 1.0f;
    if (cur >= 0x20 && cur < 0x72) {
        if (cur >= 0x6B) {
            pf[0x292] = 0.0f;
            pf[0x293] = 0.5f;
            pf[0x295] = 0.0f;
        } else if (cur >= 0x67) {
            pf[0x292] = 0.25f;
            pf[0x293] = 1.0f;
        } else if (cur >= 0x65) {
            pf[0x292] = 1.0f;
            pf[0x293] = 1.0f;
        } else if (cur >= 0x63) {
            pf[0x292] = 0.25f;
            pf[0x293] = 1.0f;
        } else if (cur >= 0x5B) {
            pf[0x292] = 0.0f;
            pf[0x293] = 0.5f;
        } else if (cur >= 0x58) {
            pf[0x292] = 0.0f;
            pf[0x293] = 0.0f;
            pf[0x295] = 0.0f;
        } else if (cur >= 0x57) {
            if (p[2] == 6 && atree[6] > 11.0f) {
                pf[0x293] = 0.0f;
                pf[0x292] = 0.0f;
            } else {
                pf[0x293] = 0.25f;
                pf[0x292] = 0.0f;
            }
            pf[0x295] = 0.0f;
        } else if (cur >= 0x56) {
            pf[0x292] = 0.0f;
            pf[0x293] = 1.0f;
            pf[0x295] = 0.0f;
        } else if (cur >= 0x54) {
            pf[0x293] = 1.0f;
            pf[0x292] = 0.25f;
            pf[0x295] = 0.0f;
        } else if (cur >= 0x4F) {
            pf[0x292] = 1.0f;
            pf[0x293] = 1.0f;
        } else if (cur >= 0x47) {
            pf[0x292] = 0.667f;
            pf[0x293] = 1.0f;
        } else if (cur >= 0x3E) {
            pf[0x292] = 1.0f;
            pf[0x293] = 0.25f;
        } else if (cur >= 0x3C) {
            pf[0x292] = 0.5f;
            pf[0x293] = 1.0f;
        } else if (cur >= 0x34) {
            pf[0x292] = 1.0f;
            pf[0x293] = 1.0f;
        } else if (cur >= 0x2C) {
            pf[0x292] = 1.0f;
            pf[0x293] = 1.0f;
        } else if (cur >= 0x27) {
            pf[0x292] = 0.25f;
            pf[0x293] = 0.0f;
        } else if (cur == 0x25) {
            if (p[2] == 7 || p[2] == 6) {
                pf[0x292] = 0.0f;
                pf[0x293] = 0.0f;
            } else if ((u32)(p[2] - 2) <= 1) {
                pf[0x292] = 0.25f;
                pf[0x293] = 1.0f;
            } else {
                pf[0x292] = 0.5f;
                pf[0x293] = 1.0f;
            }
            pf[0x295] = 0.0f;
        } else if (cur >= 0x23) {
            if (p[2] == 5 || p[2] == 6) {
                pf[0x292] = 0.0f;
                pf[0x293] = 0.0f;
            } else if (p[2] == 2) {
                pf[0x292] = 0.25f;
                pf[0x293] = 1.0f;
            } else {
                pf[0x292] = 0.5f;
                pf[0x293] = 1.0f;
            }
            pf[0x295] = 0.0f;
        } else {
            pf[0x292] = 0.0f;
            pf[0x293] = 1.0f;
        }
    } else {
        if (cur == 0x80) {
            pf[0x292] = 0.4f;
            pf[0x293] = 0.5f;
        } else if ((u32)(cur - 0x13) <= 1 || cur == 0x16) {
            pf[0x292] = 1.3f;
            pf[0x293] = 1.0f;
        } else if (cur == 0x8F) {
            pf[0x292] = 1.5f;
            pf[0x293] = 0.5f;
        } else if (cur == 8) {
            pf[0x292] = 1.5f;
            pf[0x293] = 1.0f;
        } else if (cur >= 0x77 && cur <= 0x78) {
            pf[0x292] = 0.0f;
            pf[0x293] = 0.0f;
        } else if (cur >= 9 && cur <= 0x10) {
            pf[0x292] = 0.667f;
            pf[0x293] = 1.0f;
        } else if (cur == 0x7B) {
            pf[0x292] = 1.0f;
            pf[0x293] = 1.0f;
        } else if (cur > 0x72) {
            pf[0x294] = 0.0f;
            pf[0x292] = 1.0f;
            pf[0x293] = 1.0f;
        } else {
            pf[0x292] = 1.0f;
            pf[0x293] = 1.0f;
        }
    }

    if ((gControllerButtons & 1) != 0 &&
        (gControllerButtons & 8) != 0 && p[0] == 0) {
        dbgTextPrintfCol(1, 0x1C,
                         "ACTION:%s NEXT:%s D:%s INT:%d RPT:%d DIDT:%d",
                         action_names[cur], action_names[act],
                         action_names[next], mode, rpt, didt);
        dbgTextPrintfCol(1, 0x1D, "  SEQ:%s  frame:%.1f/%d",
                         (char*)(*(s32*)atree + *(s16*)((u8*)atree + 0xE) * 0x30),
                         atree[6], (s32)*(s16*)((u8*)atree + 0x10));
    }

    if (adv != 0) {
        p[0x82] = act;
    } else {
        p[0x82] = cur;
    }
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
