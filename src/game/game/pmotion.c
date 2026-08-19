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
        u8 pad[56];
        s32 flag;
    } control;
} ControlState;
extern ControlState lbl_80240E30[]; /* control-pad state array, stride 15 f32 */
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

static f32 fabsf_param(f32 x) {
    *(u32*)&x &= 0x7FFFFFFF;
    return x;
}

/* ================================================================== */

#define STUB(address, name) void name(void) {}

/* get_player_pos spawn view: kills the PF() address-CSE on the rotation
 * triple and the floor-object word (L7 -- struct-displacement view). */
typedef struct PSpawnView {
    u8  _000[0xC4];
    f32 rot[3];          /* 0xC4 euler rotation */
    u8  _0D0[0x7F0];
    u32 floor_flags;     /* 0x8C0 */
    u32 floor_obj;       /* 0x8C4 */
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
    u8* ctx = lbl_80282850;
    f32* spread = lbl_80120BF0;

    if (sMusicTrackHi < 0) {
        return;
    }
    p = &gPlayers[playerIdx];
    *(volatile u32*)&SV(p)->floor_obj = 0;
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
            PF(other, 0x8B4, f32) = other->pos[1];
            CopyMat4(other->mat, p->mat);
            SV(p)->rot[0] = SV(other)->rot[0];
            SV(p)->rot[1] = SV(other)->rot[1];
            SV(p)->rot[2] = SV(other)->rot[2];
            pos[0] = other->col_pos[0];
            pos[1] = other->col_pos[1];
            pos[2] = other->col_pos[2];
            r = 0.5 + PF(other, 0x850, f32);
            sx = r * (spread[8 + playerIdx * 2] - spread[8 + i * 2]);
            sz = r * (spread[9 + playerIdx * 2] - spread[9 + i * 2]);
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
            *(volatile u32*)&SV(other)->floor_obj = FloorCollide(lbl_80347B10, lbl_80347B14,
                lbl_80347B18, pos2, (f32*)(ctx + 24), 1, 1);
            if (*(void**)(ctx + 92) != NULL) {
                PF(other, 0x8C0, u32) = PF(*(void**)(ctx + 92), 0x10, u32);
            } else {
                PF(other, 0x8C0, u32) = 0;
            }
            PF(other, 0x8B4, f32) = *(f32*)(ctx + 76);
            CopyMat3(other->mat, p->mat);
            p->pos[0] = pos2[0];
            p->pos[1] = pos2[1];
            p->pos[2] = pos2[2];
            SV(p)->rot[0] = SV(other)->rot[0];
            SV(p)->rot[1] = SV(other)->rot[1];
            SV(p)->rot[2] = SV(other)->rot[2];
            r = half + PF(other, 0x850, f32);
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
                *(volatile u32*)&SV(p)->floor_obj = *(volatile u32*)&SV(other)->floor_obj;
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
                PF(p, 0x8B4, f32) = FloorPos(p->pos[1], lbl_80347B10, p->pos, 1);
                p->pos[1] = PF(p, 0x8B4, f32);
                MBNodeSetParent(p->node, *(void**)(other->node + 0x74));
                *(volatile u32*)&SV(p)->floor_obj = *(volatile u32*)&SV(other)->floor_obj;
                ErrorPrintf(lbl_80114220);
            }
        } else {
            CopyMat4((f32*)gIdentityMatrix, mat);
            YawMat3(mat, gPlayerStartYaw);
            r = 0.5 + PF(p, 0x850, f32);
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
            PF(p, 0x8B4, f32) = y;
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
    f32 radius = PF(p, 0x850, f32);
    f32 height = PF(p, 0x854, f32);
    s32 floorFlags = (*(u32*)(motion + 0x8C0)) & 0x38;

    collidePosition[0] = *(f32*)(motion + 0x64);
    collidePosition[1] = *(f32*)(motion + 0x68);
    collidePosition[2] = *(f32*)(motion + 0x6C);

    if (findFloor != 0) {
        screen[0] = FloorPos(lbl_80344880, (f32)(lbl_80347B00 * radius), position, 1);
        if (*(void**)((u8*)gFloorCollisionResult + 0x44) == NULL) {
            return 0;
        }
        if (*(void**)(motion + 0x8C4) != NULL &&
            ((*(u32*)(*(u8**)(motion + 0x8C4) + 0x10) & 0x1000) != 0) &&
            *(void**)(motion + 0x8C4) != *(void**)((u8*)gFloorCollisionResult + 0x44)) {
            return 0;
        }
        delta = screen[0] - *(f32*)(motion + 0x8B4);
        *(u32*)&delta &= 0x7FFFFFFF;
        if (delta > lbl_80347B38 ||
            (delta > lbl_80347B28 && floorFlags == 0)) {
            return 0;
        }
        position[1] = screen[0];
    } else {
        position[1] = *(f32*)(motion + 0x8B4);
    }

    p->pos[0] = position[0];
    p->pos[1] = position[1];
    p->pos[2] = position[2];
    fn_8005A404(&p->mat[0], (f32*)((u8*)p + 0x844), (f32*)((u8*)p + 0x838));

    if (gGameMode != 0x400C && lbl_80344500 == 0 && lbl_803443A8 == 0) {
        get_actual_screen_pos(0, (f32*)&screen[1], (f32*)&screen[0], p->col_pos);
        if (screen[1] < (f32)(lbl_80344520 + 30) ||
            screen[1] > (f32)(lbl_8034451C - 30) ||
            screen[0] > (f32)(lbl_80344518 - 20) ||
            screen[0] < (f32)(lbl_80344514 + 40)) {
            return 0;
        }
    }

    position[1] += PF(p, 0x848, f32);
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

    slope = PF(p, 0x8BC, f32);
    if (slope > lbl_80347D00) {
        slope = lbl_80347D08;
    }
    if ((PF(p, 0x8C0, u32) & 8) != 0 && slope > lbl_80347B30 &&
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
        PF(p, 0x864, f32) += gClockFrameReciprocal * dx;
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
        PF(p, 0x86C, f32) += gClockFrameReciprocal * (to[2] - from[2]);
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
        fn_8009C850((u8*)p + 0x64);
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
    if (PF(p, 0x204, s32) >= 31) {
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
u32 PlayerKnockback(f32 angle, Player* p, f32* out) {
    u32 result = 0;
    u32 prevFlags;
    u32 flags;

    if (PF(p, 0x8E0, f32) > lbl_80347B08) {
        PF(p, 0x8E0, f32) = lbl_80347B30;
    }
    prevFlags = PF(p, 0x8D8, u32);
    PF(p, 0x8D8, u32) = PF(p, 0x8D4, u32);
    flags = PF(p, 0x8D4, u32);

    if ((flags & 0x4000) != 0) {
        PF(p, 0x8D4, u32) = flags & 0x4000;
        return 300;
    }
    if ((flags & 0x8000) != 0) {
        PF(p, 0x8D4, u32) = flags & 0x8000;
        if ((prevFlags & 0x8000) == 0) {
            PF(p, 0x870, f32) = PF(p, 0x8DC, f32);
            PF(p, 0x874, f32) = PF(p, 0x8E0, f32);
            PF(p, 0x878, f32) = PF(p, 0x8E4, f32);
        }
        return 301;
    }
    if ((prevFlags & 0x8000) != 0) {
        damage_player(p->index, PF(p, 0x8D0, f32), 1, 0, NULL);
        PF(p, 0x8D0, f32) = lbl_80347B30;
    }
    if ((PF(p, 0x120, u32) & 0x10000) != 0) {
        PF(p, 0x8D0, f32) = lbl_80347B30;
        PF(p, 0x8D4, u32) = 0;
        return 0;
    }

    if ((flags & 0x4000000) != 0) {
        result = 200;
    }
    if (PF(p, 0x8D0, f32) > lbl_80347B40) {
#define KNOCK_IMPULSE(sc)                                              \
        PF(p, 0x870, f32) = PF(p, 0x8DC, f32) * (sc) + PF(p, 0x870, f32); \
        PF(p, 0x874, f32) = PF(p, 0x8E0, f32) * (sc) + PF(p, 0x874, f32); \
        PF(p, 0x878, f32) = PF(p, 0x8E4, f32) * (sc) + PF(p, 0x878, f32)

        if ((flags & 0x10000) != 0) {
            f32 sc = lbl_80347C50;
            result = 30;
            KNOCK_IMPULSE(sc);
        } else if ((flags & 0x40) != 0) {
            f32 sc = lbl_80347C50;
            result = 20;
            KNOCK_IMPULSE(sc);
        } else if ((flags & 0x120) != 0) {
            f32 sc = (f32)((PF(p, 0x124, u32) & 0x400) != 0 ? lbl_80347D38
                                                            : lbl_80347D40);
            result = 20;
            KNOCK_IMPULSE(sc);
        } else if ((flags & 0x10) != 0) {
            f32 sc = lbl_80347D48;
            result = 10;
            KNOCK_IMPULSE(sc);
        } else if ((flags & 0x2000) != 0) {
            result = 3;
        } else if ((flags & 0x80) != 0) {
            result = 2;
        } else {
            result = 1;
        }
#undef KNOCK_IMPULSE
        if (result >= 10) {
            {
                f32 a = atan2(PF(p, 0x8DC, f32), PF(p, 0x8E4, f32));
                f32 d = a - angle;
                if (d > lbl_80347B50) {
                    d = (f32)(d - lbl_80347B60);
                } else if (d <= lbl_80347B68) {
                    d = (f32)(lbl_80347B60 + d);
                }
                if (fabsf_(d) > lbl_80347C38) {
                    result++;
                    a = (f32)(a + lbl_80347B50);
                }
                if (a > lbl_80347B50) {
                    a = (f32)(a - lbl_80347B60);
                } else if (a <= lbl_80347B68) {
                    a = (f32)(lbl_80347B60 + a);
                }
                *out = a;
            }
        }
        if ((flags & 0x1000000) == 0) {
            fn_80094164((u8*)p + 0x54, flags, 0);
        }
        SetSkinFX((u8*)p + 0x7DC, lbl_80344BF8, 1, 1, lbl_80347B40);
    } else if ((flags & 0x80) != 0) {
        result = 2;
    }

    PF(p, 0x8DC, f32) = lbl_80347B30;
    PF(p, 0x8E0, f32) = lbl_80347B30;
    PF(p, 0x8E4, f32) = lbl_80347B30;
    PF(p, 0x8D0, f32) = lbl_80347B30;
    PF(p, 0x8D4, u32) = 0;
    return result;
}
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
/* 0x80086924 - resolve a melee hit against `target`: run the hit test, and on
 * a connect apply damage and raise the hit-something flag (type 3 = solid,
 * type 10 = openable, which also posts a "hit chest" message). */
void PlayerMotion_HitTarget(Player* p, void* target, s32 arg, f32 range) {
    f32 hitpos[3];
    f64 priority;

    if (lbl_80347B30 == range) {
        range = PF(p, 0x104, f32);
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
        dmg = PF(p, 0x104, f32);
    }
    if ((PF(p, 0x124, u32) & 0x100) != 0) {
        dmg = (f32)(dmg * lbl_80347C28);
    }
    dir[0] = PF(p, 0x34, f32);
    dir[1] = PF(p, 0x38, f32);
    dir[2] = PF(p, 0x3C, f32);
    dir[1] = (f32)(lbl_80347D50 * dmg);
    if (dir[1] > lbl_80347C28) {
        dir[1] = lbl_80347B98;
    }

    if (enemy != NULL) {
        s32 estateRaw = PF(enemy, 0xB4, s32);
        s32 estate = estateRaw;
        if (PF(enemy, 0x200, f32) > lbl_80347B08 &&
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
STUB(0x80086C78, PlayerGetTarget)
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
            local[0] = PF(p, 0x944, f32);
            local[1] = PF(p, 0x948, f32);
            local[2] = PF(p, 0x94C, f32);
            FloorCollide(a, 4.0f, -10.0f, local, NULL, 0, 1);
            out[0] = local[0] - pos[0];
            out[2] = local[2] - pos[2];
            out[1] = gFloorCollisionResult[13] - PF(p, 0x48, f32);
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
                PF(p, 0x944, f32) = local[0];
                PF(p, 0x948, f32) = local[1];
                PF(p, 0x94C, f32) = local[2];
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
        PF(p, 0x1F2, s16) = 0;
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
                s8 sub = PF(item, 0xC8, s8);
                if ((sub != 2 && sub != 4) ||
                    (PF(item, 0xC4, s16) & 1) == 0) {
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
        PF(p, 0x8A8, u8*) = last;
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
    f32 hit[3];
    s32 closest = -1;
    f32 best = lbl_80347B30;
    s32 i;

    for (i = 0; i < 4; i++) {
        Player* op = &gPlayerRecords[i];
        f32 dot;
        f32 d;

        if (i == p->index) {
            continue;
        }
        if (op->state != 1 && op->state != 4) {
            continue;
        }
        if ((PF(op, 0x964, s16) & 0x20) != 0) {
            continue;
        }
        dot = (PF(op, 0x64, f32) - from[0]) * (to[0] - from[0]) +
              (PF(op, 0x6C, f32) - from[2]) * (to[2] - from[2]);
        if (dot < lbl_80347B30) {
            continue;
        }
        if (LineCylinderCollide((f32*)((u8*)op + 0x64),
                                range + PF(op, 0x850, f32), p3,
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
        Player* cp = &gPlayerRecords[closest];
        f32 ex = to[0] - PF(cp, 0x64, f32);
        f32 ez = to[2] - PF(cp, 0x6C, f32);
        f32 dist = fqdist(ex, ez);

        if (dist > lbl_80347D68) {
            f32 scale = (range + PF(cp, 0x850, f32) - dist) / dist;
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
        state = *(s32*)(item + 0xB4);
        if (state != 1 && state != 6 &&
            (state != 8 || lbl_803447DC == 0)) {
            goto item_test;
        }
        if (*(s32*)item == 0x1F) {
            goto item_test;
        }

        collisionRange = range + *(f32*)(item + 0x238);
        collisionHeight = height + *(f32*)(item + 0x23C);
        dx = *(f32*)(item + 0x54) - to[0];
        dy = *(f32*)(item + 0x58) - to[1];
        dz = *(f32*)(item + 0x5C) - to[2];
        if (dx * dx + dz * dz < collisionRange * collisionRange &&
            fabsf_(dy) < *(f32*)(item + 0x23C) &&
            LineCylinderCollide((f32*)(item + 0x54), collisionRange,
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
        if (FastWallCollide(from, (f32*)(item + 0x54), 0, 0) != 0) {
            closest = -1;
        }
    }

    CritterCollideStart(range, to, 0);
    {
        object = CritterMoveNodeCol(range, lbl_80347B30, from, to,
                                    localHit, -1, 2);
        if (object != 0 &&
            *(s16*)(*(u8**)(*(u8**)(object + 4) + 0x120) + 0x20) == 4 &&
            (PF(p, 0x8D4, u32) & 0x8000) != 0) {
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
                dx = to[0] - *(f32*)(critter + 0x5C);
                dz = to[2] - *(f32*)(critter + 0x64);
                radius = (s32)*(f32*)(*(u8**)(critter + 4) + 0x7C);
            } else {
                u8* item = gEnemies + closest * 0x394;
                zero = lbl_80347B30;
                dx = to[0] - *(f32*)(item + 0x54);
                dz = to[2] - *(f32*)(item + 0x5C);
                radius = (s32)*(f32*)(item + 0x238);
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
        {
            f32 t1 = m->fwd[0] * PF(p, 0x38, f32) - m->fwd[1] * PF(p, 0x34, f32);
            f32 t2 = m->fwd[1] * PF(p, 0x3C, f32) - m->fwd[2] * PF(p, 0x38, f32);
            PF(p, 0x8BC, f32) = t1 * m->fwd[0] - t2 * m->fwd[2];
        }
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

int PlayerCheckMovingFloor_80088688(Player* p) {
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

/* 0x80088714 - collide the motion segment pos->pos+dpos against the world
 * walls, slide `dpos` along the hit wall (or project it for one-way walls),
 * then re-test; if the second wall opposes the first (wedged), stop the move.
 * Returns 0 (no wall), 1 (slid / exit wall on the right anim), or 2. */
s32 fn_80088714(f32 range, Player* p, f32* pos, f32* dpos) {
    f32 to[3];
    s32 result = 0;
    WorldObj* wall;
    u8* ctx = lbl_80282850;
    f32* wn = (f32*)&lbl_8023CA98[0x10];

    to[0] = pos[0] + dpos[0];
    to[1] = pos[1] + dpos[1];
    to[2] = pos[2] + dpos[2];
    lbl_80344B30 = PlayerWallCollide(pos, to, ctx, range);
    wall = (WorldObj*)lbl_80344B30;
    if (wall == NULL) {
        return result;
    }

    *(f32*)(ctx + 12) = wn[0];
    result = 1;
    *(f32*)(ctx + 16) = wn[1];
    *(f32*)(ctx + 20) = wn[2];

    if ((wall->flags & 0x38) != 0) {
        return p->anim_208 == 0x8F ? 1 : 2;
    }

    if ((wall->flags & 0x1000) != 0) {
        f32 nx = *(f32*)(ctx + 12);
        f32 ny = *(f32*)(ctx + 16);
        f32 nz = *(f32*)(ctx + 20);
        f32 d = -(dpos[2] * nz + dpos[0] * nx + dpos[1] * ny);
        dpos[0] = nx * d + dpos[0];
        dpos[1] = ny * d + dpos[1];
        dpos[2] = nz * d + dpos[2];
    } else {
        SlideAlongWall(pos, dpos, ctx, (f32*)(ctx + 12), range);
    }

    to[0] = pos[0] + dpos[0];
    to[1] = pos[1] + dpos[1];
    to[2] = pos[2] + dpos[2];
    (*(u8**)&gWorldInfo[0x5C])[lbl_80344180]++;
    wall = (WorldObj*)PlayerWallCollide(pos, to, ctx, (f32)(lbl_80347D78 * range));
    if (wall == NULL) {
        return result;
    }
    lbl_80344B30 = wall;
    if (wn[1] * *(f32*)(ctx + 16) + wn[0] * *(f32*)(ctx + 12) +
            wn[2] * *(f32*)(ctx + 20) < lbl_80347B00) {
        dpos[0] = 0.0f;
        dpos[1] = 0.0f;
        dpos[2] = 0.0f;
    }
    return result;
}
STUB(0x80088938, fn_80088938)
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

    if (PF(p, 0x6B8, u32) != 0 || PF(p, 0x6BC, u32) != 0 ||
        PF(p, 0x6DC, u32) == 0) {
        return -1;
    }
    if ((PF(p, 0x124, u32) & 0x400) != 0) {
        return -1;
    }
    if (p->state != 1) {
        return -1;
    }
    if (PF(p, 0x828, f32) < lbl_80347BB8) {
        return -1;
    }
    floor = (WorldObj*)PF(p, 0x8C4, u32);
    if (floor == NULL || (floor->flags & 0x1000) != 0) {
        return -1;
    }

    face[0] = PF(p, 0x34, f32);
    face[1] = PF(p, 0x38, f32);
    face[2] = PF(p, 0x3C, f32);
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
        if (PF(op, 0x6B8, u32) != 0) {
            continue;
        }
        if ((PF(op, 0x964, s16) & 0x50) != 0) {
            continue;
        }
        anim = op->anim_208;
        if ((anim >= 0x54 && anim < 0x5B) || anim >= 0x6B) {
            continue;
        }
        if ((PF(op, 0x124, u32) & 0x400) != 0) {
            continue;
        }
        if (p->quest_state != 0 && gBossType >= 0) {
            continue;
        }
        floor = (WorldObj*)PF(op, 0x8C4, u32);
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
