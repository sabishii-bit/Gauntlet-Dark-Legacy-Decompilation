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

/* ------------------------------------------------------------------ */
/* extern globals (.sbss/.sdata runtime state)                         */
/* ------------------------------------------------------------------ */

extern s32 gGameMode;   /* game state (0x4010 = in-game) */
extern f32 lbl_8034458C;   /* frame delta (float) */
extern s32 gFrameTicks;   /* frame delta (int) */
extern s32 lbl_803447B8;   /* pause/menu depth */
extern s32 lbl_803447E4;   /* hit-something flag */
extern void* lbl_80344B2C; /* world root node */
extern s32 lbl_8034481C;
extern s32 lbl_80344804;
extern s32 lbl_80344808;
extern s32 lbl_803443B4;
extern s32 gBossType;
extern s32 lbl_80344768;
extern s32 lbl_803447B4;
extern f32 lbl_80240E30[]; /* control-pad state array, stride 15 f32 */
extern f32 sMusicFadeBase; /* sMusicFadeBase */

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
extern void fn_8009C850(void* p);
extern void damage_player(s32 i, f32 dmg, s32 mode, u32 flags, f32* dir);
extern void MBTreeSetAlpha(void* node, s32 alpha, s32 mode);
extern void* fn_8005B8B0(Player* p);
extern s32 PointVisible(f32 y, f32* pos);
extern void fn_8009C98C(f32* pos);
extern f32 gFloorCollisionResult[]; /* transporter table (0x34 = target height) */

/* Player-motion transform context (arg to PlayerNewFloor / collision fns):
 * a 3x3-ish orient block at 0x10 and the current floor WorldObj* at 0x44. */
typedef struct PMotionCtx {
    u8         _p00[0x10];
    f32        fwd[3];    /* 0x10, 0x14, 0x18 */
    u8         _p1c[0x28];
    WorldObj*  floor;     /* 0x44 */
} PMotionCtx;

/* float magnitude via sign-bit clear (matches the inline fabs codegen). */
static f32 fabsf_(f32 x) {
    f32 slots[3];

    slots[2] = x;
    *(u32*)&slots[2] &= 0x7FFFFFFF;
    return slots[2];
}

/* ================================================================== */

#define STUB(address, name) void name(void) {}

STUB(0x8008091C, get_player_pos)
STUB(0x80081104, try_location)
void PlayerMotion_SetAnimState(Player* p) {
    u8 unused[32];
    if (lbl_803447B8 >= 2) {
        MBTreeClearFlags(p->node, 2, 0);
        if (PF(p, 0x6C8, void*) != NULL) {
            MBTreeClearFlags(PF(p, 0x6C8, void*), 2, 0);
        }
        if (p->anim_208 == 0x7C) {
            PF(p, 0x964, s16) |= 0x1000;
        }
        if ((PF(p, 0x964, s16) & 0x1000) == 0) {
            p->anim_20C = 0x7C;
        } else {
            p->anim_20C = 0;
        }
    } else {
        p->anim_20C = 0;
        MBTreeSetFlags(p->node, 2, 0);
        if (PF(p, 0x6C8, void*) != NULL) {
            MBTreeSetFlags(PF(p, 0x6C8, void*), 2, 0);
        }
    }
    DoPlayerAction(p);
}
/*
 * PlayerMotion  0x80081504  (0x4A9C bytes -- the giant per-frame driver).
 *
 * SKELETON ONLY (semantics notes; full reconstruction deferred).
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
void PlayerMotion(void) {
}
STUB(0x80085FA0, ModifyPlayerDpos)

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
        PF(p, 0x864, f32) += lbl_8034458C * dx;
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
        PF(p, 0x86C, f32) += lbl_8034458C * (to[2] - from[2]);
        dpos[2] = 0.0f;
    }

    return count;
}

/* NOTE: correct body; not yet byte-exact (0x8EC address-CSE + flags-mask CSE
 * + switch lowering residuals -- parked per light-touch cap). */
void PlayerMotion_FloorFX(Player* p, WorldObj* obj, f32* v1, f32* v2) {
    f32 dv[3];
    u32 flags = WorldObjGetAllFlags(obj);

    if ((flags & 0xF0000) == 0) {
        return;
    }
    if ((flags & 0x2000000) != 0 && (flags & 0x8000000) == 0) {
        return;
    }
    if (PF(p, 0x204, s32) >= 31) {
        return;
    }
    if (sMusicFadeBase < PF(p, 0x8EC, f32)) {
        return;
    }

    dv[0] = v1[0] - v2[0];
    dv[1] = 0.0f;
    dv[2] = v1[2] - v2[2];
    NormalVector2D(dv);

    switch (flags & 0xF0000) {
    case 0x20000:
        damage_player(p->index, 10.0f, 1, 16, dv);
        PF(p, 0x8EC, f32) = 1.0 + sMusicFadeBase;
        break;
    case 0x30000:
    case 0x40000:
    case 0x50000:
        PF(p, 0x8EC, f32) = 1.0 + sMusicFadeBase;
        damage_player(p->index, 15.0f, 1, 32, dv);
        fn_8009C850((u8*)p + 0x64);
        break;
    default:
        PF(p, 0x8EC, f32) = 1.0 + sMusicFadeBase;
        damage_player(p->index, 5.0f, 1, 0, NULL);
        break;
    }
}
STUB(0x80086470, PlayerKnockback)
void PlayerMotion_FindClosestPlayer(Player* p, f32* dir, u32 flags, f32 dmg) {
    u8 unused[8];
    f32 dvec[3];
    f32 best = 2.0 + (f64)PF(p, 0x850, f32);
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
        adj = len - PF(op, 0x850, f32);
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
STUB(0x80086924, PlayerMotion_HitTarget)
STUB(0x80086A24, PlayerMotion_DamageTarget)
STUB(0x80086C78, PlayerGetTarget)
/* NOTE: correct body; not yet byte-exact (far-field PF address-CSE parks an
 * extra nonvolatile -- needs a 0x93C..0x94C struct overlay; light-touch cap). */
s32 DoTransporter(Player* p, f32* pos, f32* out, f32 a) {
    s32 timer = PF(p, 0x940, s32);

    if (timer > 0) {
        s32 t = timer - gFrameTicks * 2;
        PF(p, 0x940, s32) = t;
        if (t < 0) {
            PF(p, 0x940, s32) = 0;
        }
        t = PF(p, 0x940, s32);
        if (t < 30) {
            MBTreeSetAlpha(p->node, t * 255 / 29, 1);
        } else {
            MBTreeSetAlpha(p->node, 255 - (t - 30) * 255 / 30, 1);
        }
        if (timer >= 30 && PF(p, 0x940, s32) < 30) {
            f32 local[3];
            local[0] = PF(p, 0x944, f32);
            local[1] = PF(p, 0x948, f32);
            local[2] = PF(p, 0x94C, f32);
            FloorCollide(a, 4.0f, -10.0f, local, NULL, 0, 1);
            out[0] = local[0] - pos[0];
            out[2] = local[2] - pos[2];
            out[1] = gFloorCollisionResult[13] - PF(p, 0x48, f32);
            PF(p, 0x93C, s32) = 1;
            msgPost(9, p->index, (u32)&p->col_pos);
            return 2;
        }
        return 1;
    } else {
        u8* tp = (u8*)fn_8005B8B0(p);
        if (tp == NULL) {
            if (PF(p, 0x93C, s32) > 0) {
                PF(p, 0x93C, s32) = PF(p, 0x93C, s32) - 1;
            }
        } else if (PF(p, 0x93C, s32) < 1) {
            f32 local[3];
            local[0] = PF(tp, 0x34, f32);
            local[1] = PF(tp, 0x38, f32);
            local[2] = PF(tp, 0x3C, f32);
            PF(p, 0x944, f32) = local[0];
            PF(p, 0x948, f32) = local[1];
            PF(p, 0x94C, f32) = local[2];
            if (PointVisible(-a, local) != 0) {
                if (FloorCollide(a, 4.0f, -10.0f, local, NULL, 0, 1) != 0) {
                    fn_8009C98C(local);
                    PF(p, 0x940, s32) = 60;
                }
                return 1;
            }
        }
        return 0;
    }
}
void DoExit(Player* p) {
    s32 exiting;

    if (lbl_8034481C != 0 && (lbl_80344804 != 0 || lbl_80344808 != 0)) {
        p->state = 4;
        PF(p, 0x1F2, s16) = 0;
        exiting = 1;
    } else {
        exiting = 0;
    }

    if (!exiting && sumnerSpeechActive() == 0 && lbl_803443B4 == 0) {
        if (lbl_80344808 != 0) {
            p->idle_timer += gFrameTicks;
        } else if (fn_8005B8FC(p) != 0) {
            if (lbl_80344804 != 0 ||
                0.0 == (f64)lbl_80240E30[p->index * 15 + 8]) {
                p->idle_timer += gFrameTicks;
            }
        } else {
            p->idle_timer = 0;
        }

        if (p->idle_timer >= 6) {
            p->state = 4;
            PF(p, 0x1F2, s16) = 0;
            if (gBossType < 0 && lbl_80344768 > 1 && lbl_803447B4 == 0 &&
                lbl_8034481C < 3) {
                msgPost(11, p->index, (u32)&p->col_pos);
            }
        } else if (p->state == 4) {
            p->state = 1;
        }
    }
}
STUB(0x8008760C, PlayerCollideEnemies)
STUB(0x80087830, PlayerCollidePlayers)
STUB(0x80087A20, PlayerCollideItems)
int PlayerNewFloor(PMotionCtx* m, Player* p, f32* dpos) {
    u8 unused[8];
    WorldObj* mf = (WorldObj*)PF(p, 0x8C4, u32);
    s32 result;

    if (mf != NULL && (mf->flags & 0xC000000) != 0 &&
        (mf->flags & 0x20000000) != 0 && mf != m->floor) {
        dpos[0] = 0.0f;
        dpos[1] = 0.0f;
        dpos[2] = 0.0f;
        return 0;
    }

    CopyMat3((f32*)m, (f32*)PF(p, 0x6C8, u32));
    result = PlayerCheckFloor(p, m->floor, dpos);

    if (m->floor != NULL && (m->floor->flags & 8) != 0) {
        f32 d1 = fqdist(dpos[0], dpos[2]);
        f32 d2 = fqdist(d1, dpos[1]);
        if (d1 > 0.01 && d2 > 0.01 &&
            ((PF(p, 0x8C0, u32) & 8) == 0 || fabsf_(dpos[1]) > 0.01)) {
            PF(p, 0x8BC, f32) = dpos[1] / d2;
        }
    } else {
        PF(p, 0x8BC, f32) =
            (m->fwd[0] * PF(p, 0x38, f32) - m->fwd[1] * PF(p, 0x34, f32)) * m->fwd[0] -
            (m->fwd[1] * PF(p, 0x3C, f32) - m->fwd[2] * PF(p, 0x38, f32)) * m->fwd[2];
    }

    PF(p, 0x8C0, u32) = m->floor != NULL ? m->floor->flags : 0;
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
        cur = (WorldObj*)p->floor_name2;
    } else {
        cur = obj;
    }
    if (cur == NULL || (cur->flags & 0x1000) == 0) {
        MBNodeSetParent(p->node, lbl_80344B2C);
    }

    if (obj != (WorldObj*)p->floor_name2) {
        PF(p, 0x964, s16) |= 1;
    }
    p->floor_name2 = (char*)obj;

    if (result != 0) {
        dpos[0] = 0.0f;
        dpos[2] = 0.0f;
    }
    return result;
}

STUB(0x80088068, PlayerCollideFloor)

int PlayerCheckMovingFloor(Player* p) {
    f32 drop = -(3.0 + (f64)PF(p, 0x854, f32));
    if (gGameMode == 0x4010) {
        PF(p, 0x8C4, u32) = FloorCollide(PF(p, 0x850, f32), 0.0f, drop,
            (f32*)((u8*)p + 0x44), NULL, 1, 0);
        PF(p, 0x964, s16) |= 1;
    }
    if (PF(p, 0x8C4, u32) != 0) {
        return 1;
    }
    return 0;
}

STUB(0x80088714, fn_80088714)
STUB(0x80088938, fn_80088938)
STUB(0x80088EF4, fn_80088EF4)

#undef STUB
