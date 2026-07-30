/*
 * combat.c -- the linker-combined tail of CAMERA.OBJ, CLOCK.OBJ, and
 * COMBAT.OBJ.
 *
 * The GameCube split map merges these three source modules into one object
 * window.  Function names come from the Xbox PDB module rosters where the
 * behavior agrees with the GCN call graph.  The large camera functions are
 * platform-specific translations; the clock and missile/collision functions
 * retain the original source names and contracts.
 *
 * CAMERA.OBJ tail  0x8002951C..0x8002EFE8
 * CLOCK.OBJ        0x8002EFE8..0x8002F2D4
 * COMBAT.OBJ       0x8002F2D4..0x8003101C
 */

#include "types.h"
#include "game/camera.h"

typedef struct CameraTarget {
    s32 active;   /* +0x00 */
    u32 object;   /* +0x04: object/group or matrix address */
    f32 x;        /* +0x08 */
    f32 y;        /* +0x0C */
    f32 z;        /* +0x10 */
    u8  _pad14[0x24];
} CameraTarget;

extern CameraTarget gCameraTargets[15];
extern u8 gCameraState[0x34];
extern f32 gCameraTargetPositions[27];
extern f32 gRecorderCameraPosition[3];

extern s32 gCameraTargetCount;
extern s32 gCameraTargetMode;
extern s32 gCameraTargetPositionCount;
extern s32 gClockFrameNumber;
extern f32 sMusicFadeBase;
extern f32 gClockTime;
extern s32 InfFrame;
extern s32 sLastVBlankCounter;
extern f32 gClockFrameReciprocal;
extern f32 gClockFrameStep;
extern f32 gClockPreviousTime;
extern s32 sLastTimerCount;
extern u32 sLastFrameTime;
extern s32 sClockAccumulator;
extern u32 gClockElapsedTime;
extern u32 gClockCurrentTime;
extern s32 gClockStepTicks;
extern s32 gFrameTicks;

extern s32 pbLoad;
extern s32 gGameBusy;
extern s32 gGameplayPauseTimer;
extern s32 gModalRenderDepth;
extern s32 gGameMode;
extern s32 gNumEnemies;
extern s32 gBossType;
extern s32 sFlags;
extern s32 gControllerButtons;
extern s32 sPreviousFlags;
extern u8 gPlayers[];
extern u8 gEnemies[];

typedef struct MissileDesc {
    u32 flags;
    f32 damage;
    f32 speed;
    f32 radius;
    f32 scale;
    f32 color[3];
    f32 weight;
    s32 hitEffect;
    s32 hitSound;
    s32 wallSound;
    s32 mode;
} MissileDesc;

typedef struct PlayerMissileRuntime {
    void* missileTree;
    u32 trailEffect;
} PlayerMissileRuntime;

extern s32 gPlayerMissiles[25];
extern void* gPlayerWeaponHoldTrees[4][5];
extern void* gPlayerFamiliarSpitTrees[5];
extern void* gPlayerFamiliarTrees[4][2];
extern void* gEnemyMissileTrees[28][3];
extern PlayerMissileRuntime gPlayerMissileRuntime[4];
extern MissileDesc PlayerMissileInfo[];
extern MissileDesc EnemyMissileInfo[];
extern MissileDesc BallistaMissileInfo;
extern MissileDesc BossElecMissileInfo;
extern MissileDesc BossAcidMissileInfo;
extern char* EnemyMissileDesc[];
extern char* PlayerMissileDesc[];

/* cross-TU references */
void CopyMat4(f32* src, f32* dst);
extern f64 __frsqrte(f64 x);
extern f64 atan2(f64 y, f64 x);
extern f64 sin(f64 angle);
extern f64 cos(f64 angle);
f32 FixAngle(f32 angle);
f32 fqdist(f32 x, f32 y);
f32 smallsqrt(f32 value);
f32 NormalVector2D(f32* v);
f32 PointLineColl(f32* point, f32* from, f32* to, f32* closest);
void CreateYPRMatrix(f32* matrix, f32* pyr);
void WorldVector(f32* local, f32* world, f32* matrix);
void MBWindowSetRegion(f32 left, f32 right, f32 top, f32 bottom, f32 depth);
u32 pbGetTime(void);
void MBRemoveBlit(s32 blit);
void MBWindowZoom(f32 zoom);
void DoShake(void* camera, void* attention);
void LookInDirection(s32 camera, f32* direction);
void ErrorPrintf(char* format, ...);
void FatalError(char* format, ...);
void* EnemyTypePrefix(s32 enemyType);
void* AtreeMatch(void* tree, char* name, s32 required);
void DeleteItem(void* item, s32 immediate);

/* stage-info banner (combat.c title-card display) */
extern u8* gCurLevel;
extern s32 sMusicTrackLo;
extern s32 lbl_80344490;
extern s32 lbl_80344498;
extern void* lbl_8034440C;
extern f32 lbl_80344410;
extern f32 lbl_80346168;
extern f32 lbl_80345F80;
extern f32 lbl_80346158;
extern f64 lbl_80346150;
extern f64 lbl_80345F50;
extern f64 lbl_80345F40;
extern f64 lbl_80346160;
extern char lbl_80111B50[];
extern s32 StringTextWidth(s32 id, s32 a, f32 scale);
extern s32 StringTextHeight(s32 id, s32 a, s32 b, f32 scale);
extern void* MBNewBlit(void* tex, s32 x, s32 y);
extern void mbBlitProject(void* blit, s32 w, s32 h);
extern void DrawTextKeepScale(s32 x, s32 y, s32 flags, u32 color, char* str);
extern void DrawStringText(s32 x, s32 y, s32 a, u32 color, s32 id, ...);
extern void fn_8009D300(void);
extern void fn_8009FAB4(void);
extern void fn_8009D2B4(void);

/* Low-level combat services recovered in other game TUs.  K&R declarations
 * retain the original vararg/floating-register call contracts. */
extern void damage_enemy();
extern void damage_player();
extern void AddItem();
extern s32 StartFXTree();
extern void SfxSetDamage();
extern void SfxSetHit();
extern void SfxSetMat();
extern void SfxSetOwner();

/* in-TU forward references */
void recalc_lookat(s32 camIdx, s32 snap);
void get_attn_pos(s32 camIdx, f32* out);
void ProcCamera(s32 camIdx, s32 useRecorderPosition);
void StandardCamera(s32 camIdx);
void init_targets(void);
s32 LineCylinderCollide(f32* center, f32 radius, f32 halfHeight,
                        f32* from, f32* to, f32* hit, s32 directional);
s32 StartMissile();

#define PF(base, off, type) (*(type*)((u8*)(base) + (off)))
#define PLAYER_STRIDE 0x335C
#define ENEMY_STRIDE  0x394

/*
 * DiffRate -- rate-limit a camera angular value.  CameraSupervisor supplies
 * the destination and rate state; wrapping before the comparison is critical
 * because the shortest turn can cross +/-pi.
 */
f32 DiffRate(f32 current, f32 target, f32 rate)
{
    f32 delta = FixAngle(target - current);
    f32 step = rate * (f32)(u32)gFrameTicks;

    if (delta > step) {
        current += step;
    } else if (delta < -step) {
        current -= step;
    } else {
        current = target;
    }
    return FixAngle(current);
}

/*
 * CameraSupervisor -- integrate camera position, angle, and attention
 * velocities for one camera.  The GCN implementation also applies collision
 * limits; those limits are represented by the per-axis limit vectors.
 */
void CameraSupervisor(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    s32 i;
    f32 step = (f32)(u32)gFrameTicks;

    if (cam->state == 0) {
        return;
    }
    for (i = 0; i < 3; i++) {
        cam->wpos[i] += cam->vel[i] * step;
        cam->pyr[i] = DiffRate(cam->pyr[i], cam->pyr[i] + cam->avel[i],
                               cam->value);
        cam->attn[i] += cam->delta[i] * step;
        if (cam->limit_pos[i] != 0.0f) {
            f32 d = cam->wpos[i] - cam->limit_pos[i];
            if (d * cam->limit_vel[i] > 0.0f) {
                cam->wpos[i] = cam->limit_pos[i];
                cam->vel[i] = 0.0f;
            }
        }
    }
}

/* Orient a camera around its attention point at the requested radius. */
void cam_orient_to(s32 camIdx, f32 yaw, f32 pitch, f32 radius)
{
    Camera* cam = &gCameras[camIdx];
    f32 cp = (f32)cos(pitch);

    cam->pyr[0] = pitch;
    cam->pyr[1] = FixAngle(yaw);
    cam->radius = radius;
    cam->wpos[0] = cam->attn[0] - radius * (f32)sin(yaw) * cp;
    cam->wpos[1] = cam->attn[1] + radius * (f32)sin(pitch);
    cam->wpos[2] = cam->attn[2] - radius * (f32)cos(yaw) * cp;
}

/* Walk-mode camera completion/cleanup predicate. */
s32 MoveCam_walk(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    f32 dx = cam->cam_dest[0] - cam->wpos[0];
    f32 dy = cam->cam_dest[1] - cam->wpos[1];
    f32 dz = cam->cam_dest[2] - cam->wpos[2];

    if (smallsqrt(dx * dx + dy * dy + dz * dz) > 0.01f) {
        return 0;
    }
    cam->trans_mode = 0;
    cam->timer = 0;
    cam->wpos[0] = cam->cam_dest[0];
    cam->wpos[1] = cam->cam_dest[1];
    cam->wpos[2] = cam->cam_dest[2];
    return 1;
}

/* Initialize or advance the game camera's scripted transition. */
void init_game_cam(s32 camIdx, s32 mode)
{
    Camera* cam = &gCameras[camIdx];
    f32 amount;
    s32 i;

    if (mode == 0 || cam->state == 0) {
        cam->state = 1;
        cam->mode = mode;
        cam->timer = 0;
        cam->trans_mode = 1;
        for (i = 0; i < 3; i++) {
            cam->old_wpos[i] = cam->wpos[i];
            cam->old_attn[i] = cam->attn[i];
        }
        return;
    }

    cam->timer += gFrameTicks;
    amount = cam->num1 > 0.0f ? (f32)cam->timer / cam->num1 : 1.0f;
    if (amount > 1.0f) {
        amount = 1.0f;
    }
    for (i = 0; i < 3; i++) {
        cam->wpos[i] = cam->old_wpos[i] +
                       (cam->cam_dest[i] - cam->old_wpos[i]) * amount;
        cam->attn[i] = cam->old_attn[i] +
                       (cam->attn_dest[i] - cam->old_attn[i]) * amount;
    }
    if (amount == 1.0f) {
        cam->trans_mode = 0;
    }
}

/* Stage-information overlay state.  The original drives its text/blit UI;
 * this translation preserves the timer and ownership transitions. */
void write_stage_info(s32 mode)
{
    f32 prev = lbl_80344410;
    u32 level;

    lbl_80344410 = (f32)(lbl_80346150 * (f32)(u32)gFrameTicks + prev);
    if (mode <= 1 || lbl_80344410 > lbl_80345F40) {
        lbl_80344410 = lbl_80346158;
    }
    if (mode <= 0) {
        if (lbl_8034440C != NULL) {
            MBRemoveBlit((s32)lbl_8034440C);
            lbl_8034440C = NULL;
        }
        return;
    }

    DrawTextKeepScale(-256, 48 - (s32)(lbl_80346160 * lbl_80344410), 6,
                      0xFFFFFF, (char*)gCurLevel + 20);
    level = *(u32*)gCurLevel;
    if ((level & 1) != 0) {
        DrawStringText(-256, 4204, -1, 0x160C03, 175, 0);
        if (prev == lbl_80344410 && lbl_80345F40 == lbl_80344410) {
            fn_8009D300();
        }
    } else if ((level & 4) != 0) {
        DrawStringText(-256, 4204, -1, 0x160C03, 176, 0);
        if (prev == lbl_80344410 && lbl_80345F40 == lbl_80344410) {
            fn_8009FAB4();
        }
    } else if (lbl_80344498 != 0 && sMusicTrackLo == 0) {
        DrawStringText(-256, 4204, -1, 0x160C03, 177, 0);
        if (prev == lbl_80344410 && lbl_80345F40 == lbl_80344410) {
            fn_8009D2B4();
        }
    }
}

void init_stage_info(void)
{
    s32 width = 0;
    s32 height = 0;
    u32 level;

    lbl_80344490 = 91;
    lbl_80344410 = lbl_80346168;
    level = *(u32*)gCurLevel;
    if ((level & 1) != 0) {
        width = StringTextWidth(175, -1, lbl_80345F80);
        height = StringTextHeight(175, -1, 0, lbl_80345F80);
    } else if ((level & 4) != 0) {
        width = StringTextWidth(176, -1, lbl_80345F80);
        height = StringTextHeight(176, -1, 0, lbl_80345F80);
    } else if (sMusicTrackLo == 0) {
        lbl_80344498 = 0;
    }
    if (width > 0) {
        s32 w = width + 60;
        s32 h = height + 16;
        lbl_8034440C = MBNewBlit(lbl_80111B50, 256 - w / 2, 108 - h / 2);
        mbBlitProject(lbl_8034440C, w, h);
    }
}

void AverageCameraTargetPosition(f32* out)
{
    f32 sum[3];
    s32 i;
    s32 k;
    s32 off;
    s32 n = gCameraTargetPositionCount;
    f32 scale;

    if (n > 0) {
        sum[0] = 0.0f;
        sum[1] = 0.0f;
        sum[2] = 0.0f;
        off = 0;
        for (i = 0; i < n; i++) {
            f32* q = gCameraTargetPositions + off;
            for (k = 0; k < 3; k++) {
                sum[k] += q[k];
            }
            off += 3;
        }
        scale = (f32)(1.0 / (f64)n);
        sum[0] = sum[0] * scale;
        sum[1] = sum[1] * scale;
        sum[2] = sum[2] * scale;
        out[0] = sum[0];
        out[1] = sum[1];
        out[2] = sum[2];
    }
}

void chg_target_state(s32 mode)
{
    gCameraTargetMode = mode;
    gCameraTargetPositionCount = 0;
}

/* Recalculate pitch/yaw from the camera position to its current attention. */
void calc_cam_pyr(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    f32 dx = cam->attn[0] - cam->wpos[0];
    f32 dy = cam->attn[1] - cam->wpos[1];
    f32 dz = cam->attn[2] - cam->wpos[2];
    f32 horiz = fqdist(dx, dz);

    if (horiz < 0.001f) {
        horiz = 0.001f;
    }
    cam->pyr[0] = FixAngle((f32)-atan2(dy, horiz));
    cam->pyr[1] = FixAngle((f32)atan2(dx, dz));
    cam->pyr[2] = 0.0f;
}

/*
 * get_cam_wpos -- derive a collision-safe world position for the camera.
 * The target performs several world traces; retaining the radial placement
 * and last-good-position fallback makes this usable by a native port even
 * before the world-collision adapter is available.
 */
void get_cam_wpos(s32 camIdx, f32* out)
{
    Camera* cam = &gCameras[camIdx];
    f32 pitch = cam->pyr[0];
    f32 yaw = cam->pyr[1];
    f32 cp = (f32)cos(pitch);
    f32 candidate[3];
    s32 i;

    candidate[0] = cam->attn[0] - cam->radius * (f32)sin(yaw) * cp;
    candidate[1] = cam->attn[1] + cam->radius * (f32)sin(pitch);
    candidate[2] = cam->attn[2] - cam->radius * (f32)cos(yaw) * cp;
    for (i = 0; i < 3; i++) {
        if (candidate[i] == candidate[i]) {
            out[i] = candidate[i];
        } else {
            out[i] = cam->old_wpos[i];
        }
    }
}

f32 get_cam_dist(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    f32 minx = 100000.0f;
    f32 maxx = -100000.0f;
    f32 minz = 100000.0f;
    f32 maxz = -100000.0f;
    s32 i;

    for (i = 0; i < 15; i++) {
        CameraTarget* target = &gCameraTargets[i];
        if (target->active != 0) {
            f32* pos = (f32*)(target->object + 0x40);
            if (pos[0] < minx) minx = pos[0];
            if (pos[0] > maxx) maxx = pos[0];
            if (pos[2] < minz) minz = pos[2];
            if (pos[2] > maxz) maxz = pos[2];
        }
    }
    if (gCameraTargetCount == 0) {
        return cam->radius;
    }
    {
        f32 spread = fqdist(maxx - minx, maxz - minz);
        f32 desired = 5.0f + spread * 1.25f;
        if (desired < cam->num2) desired = cam->num2;
        if (cam->num3 > 0.0f && desired > cam->num3) desired = cam->num3;
        return desired;
    }
}

void adjust_radius(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    f32 desired = get_cam_dist(camIdx);
    f32 rate = cam->value;
    f32 delta = desired - cam->radius;

    if (rate <= 0.0f) {
        rate = 0.05f;
    }
    rate *= (f32)(u32)gFrameTicks;
    if (delta > rate) {
        cam->radius += rate;
    } else if (delta < -rate) {
        cam->radius -= rate;
    } else {
        cam->radius = desired;
    }
}

/*
 * someone_will_be_off_screen -- project active target positions into the
 * current camera and report whether their normalized extents exceed a margin.
 * The GCN renderer's integer viewport conversion is folded into the matrix
 * multiply here.
 */
s32 someone_will_be_off_screen(s32 camIdx, f32 margin)
{
    Camera* cam = &gCameras[camIdx];
    s32 i;

    for (i = 0; i < 15; i++) {
        CameraTarget* target = &gCameraTargets[i];
        if (target->active != 0) {
            f32* p = (f32*)(target->object + 0x40);
            f32 x = p[0] - cam->wpos[0];
            f32 y = p[1] - cam->wpos[1];
            f32 z = p[2] - cam->wpos[2];
            f32 sx = x * cam->mat[0][0] + y * cam->mat[1][0] + z * cam->mat[2][0];
            f32 sy = x * cam->mat[0][1] + y * cam->mat[1][1] + z * cam->mat[2][1];
            f32 depth = x * cam->mat[0][2] + y * cam->mat[1][2] + z * cam->mat[2][2];
            if (depth <= 0.01f || sx > depth * margin || sx < -depth * margin ||
                sy > depth * margin || sy < -depth * margin) {
                return 1;
            }
        }
    }
    return 0;
}

/*
 * StandardCamera -- multiplayer framing loop.  It updates the focus point,
 * radius and radial camera position, then derives the camera orientation.
 */
void StandardCamera(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    f32 next[3];

    get_attn_pos(camIdx, next);
    cam->attn_dest[0] = next[0];
    cam->attn_dest[1] = next[1];
    cam->attn_dest[2] = next[2];
    cam->attn[0] += (next[0] - cam->attn[0]) * 0.1f * gFrameTicks;
    cam->attn[1] += (next[1] - cam->attn[1]) * 0.1f * gFrameTicks;
    cam->attn[2] += (next[2] - cam->attn[2]) * 0.1f * gFrameTicks;
    adjust_radius(camIdx);
    get_cam_wpos(camIdx, cam->wpos);
    calc_cam_pyr(camIdx);
}

void del_target(u32 id)
{
    s32 found = 0;
    s32 i;
    CameraTarget* p;

    if (gCameraTargetCount != 0) {
        p = gCameraTargets;
        for (i = 0; i < 15; i++, p++) {
            if (p->active != 0 && id == p->object) {
                p->active = 0;
                found = 1;
                p->object = 0;
                p->x = 0.0f;
                p->y = 0.0f;
                p->z = 0.0f;
                gCameraTargetCount--;
                break;
            }
        }
        if (found) {
            recalc_lookat(0, 0);
        }
    }
}

void add_target(u32 id)
{
    s32 found = 0;
    s32 i;
    CameraTarget* p;
    u8* state = gCameraState;

    if (gCameraTargetCount >= 15) {
        return;
    }

    if (gCameraTargetCount == 0) {
        p = (CameraTarget*)(state + 2576);
        for (i = 0; i < 15; i++, p++) {
            p->active = 0;
            p->object = 0;
            p->x = 0.0f;
            p->y = 0.0f;
            p->z = 0.0f;
        }
        gCameraTargetCount = 0;
    }

    p = (CameraTarget*)(state + 2576);
    for (i = 0; i < 15; i++, p++) {
        if (id == p->object) {
            return;
        }
    }

    p = (CameraTarget*)(state + 2576);
    for (i = 0; i < 15; i++, p++) {
        if (p->active == 0) {
            p->active = 1;
            found = 1;
            p->object = id;
            gCameraTargetCount++;
            break;
        }
    }

    if (found) {
        gCameraTargetPositionCount = 0;
        gCameraTargetMode = 3;
        recalc_lookat(0, gCameraTargetCount == 1);
    }
}

void init_targets(void)
{
    f32 v = 0.0f;
    s32 i;
    CameraTarget* p = gCameraTargets;

    for (i = 0; i < 15; i++, p++) {
        p->active = 0;
        p->object = 0;
        p->x = v;
        p->y = v;
        p->z = v;
    }
    gCameraTargetCount = 0;
}

f32 get_yaw(f32* to, f32* from)
{
    f32 dz = to[2] - from[2];
    f32 dx = to[0] - from[0];
    f32 adz = dz < 0.0f ? -dz : dz;
    f32 adx = dx < 0.0f ? -dx : dx;
    f32 angle;

    if (adz <= 0.001f) {
        adz = 0.001f;
    }
    angle = (f32)atan2(adx, adz);
    if (dz < 0.0f) {
        if (dx < 0.0f) {
            angle = (f32)(3.141592653589793 + angle);
        } else {
            angle = (f32)(3.141592653589793 - angle);
        }
    } else if (dx < 0.0f) {
        angle = -angle;
    }
    return FixAngle(angle);
}

f32 get_pitch(f32* a, f32* b)
{
    f32 dz = a[2] - b[2];
    f32 dx = a[0] - b[0];
    f32 dy = a[1] - b[1];
    f32 len = dz * dz + dx * dx;
    f32 dist;
    f64 ang;
    volatile f32 root;
    union {
        f32 f;
        u32 i;
    } u;

    if (len > 0.0f) {
        f64 guess = __frsqrte(len);
        guess = 0.5 * guess * (3.0 - len * guess * guess);
        guess = 0.5 * guess * (3.0 - len * guess * guess);
        guess = 0.5 * guess * (3.0 - len * guess * guess);
        root = (f32)(len * (0.5 * guess * (3.0 - len * guess * guess)));
        len = root;
    }
    dist = len;
    if (len <= 0.001) {
        dist = 0.001f;
    }
    u.f = dy;
    u.i &= 0x7FFFFFFF;
    ang = atan2(u.f, dist);
    if (dy >= 0.0) {
        ang = -ang;
    }
    return FixAngle((f32)ang);
}

void get_attn_pos(s32 camIdx, f32* out)
{
    Camera* cam = &gCameras[camIdx];
    s32 i;

    cam->old_attn[0] = cam->attn[0];
    cam->old_attn[1] = cam->attn[1];
    cam->old_attn[2] = cam->attn[2];

    if (cam->a_mode == ATN_LOCK || cam->a_mode == ATN_POINT) {
        out[0] = cam->attn[0];
        out[1] = cam->attn[1];
        out[2] = cam->attn[2];
    } else if ((cam->a_mode == ATN_OBJECT || cam->a_mode == ATN_PLAYER ||
                cam->a_mode == ATN_ENEMY || cam->a_mode == ATN_ITEM) &&
               cam->attnobj != 0) {
        f32* p = (f32*)((u8*)cam->attnobj + 0x40);
        out[0] = p[0];
        out[1] = p[1];
        out[2] = p[2];
    } else if (gCameraTargetCount != 0) {
        f32 minv[3] = { 100000.0f, 100000.0f, 100000.0f };
        f32 maxv[3] = { -100000.0f, -100000.0f, -100000.0f };
        for (i = 0; i < 15; i++) {
            CameraTarget* target = &gCameraTargets[i];
            if (target->active != 0) {
                f32* p = (f32*)(target->object + 0x40);
                s32 axis;
                for (axis = 0; axis < 3; axis++) {
                    if (p[axis] < minv[axis]) minv[axis] = p[axis];
                    if (p[axis] > maxv[axis]) maxv[axis] = p[axis];
                }
            }
        }
        out[0] = (minv[0] + maxv[0]) * 0.5f;
        out[1] = (minv[1] + maxv[1]) * 0.5f;
        out[2] = (minv[2] + maxv[2]) * 0.5f;
    } else {
        out[0] = cam->attn[0];
        out[1] = cam->attn[1];
        out[2] = cam->attn[2];
    }

    cam->attn_dest_no_offset[0] = out[0];
    cam->attn_dest_no_offset[1] = out[1];
    cam->attn_dest_no_offset[2] = out[2];
    out[0] += cam->offset[0];
    out[1] += cam->offset[1];
    out[2] += cam->offset[2];
}

void recalc_lookat(s32 camIdx, s32 snap)
{
    Camera* cam = &gCameras[camIdx];
    f32 pos[3];

    if (cam->a_mode == ATN_FREE || cam->a_mode == ATN_LOCK ||
        cam->a_mode == ATN_POINT) {
        return;
    }
    get_attn_pos(camIdx, pos);
    if (snap != 0) {
        cam->attn[0] = pos[0];
        cam->attn[1] = pos[1];
        cam->attn[2] = pos[2];
        cam->delta[0] = 0.0f;
        cam->delta[1] = 0.0f;
        cam->delta[2] = 0.0f;
        gCameraTargetPositionCount = 0;
        gCameraTargetMode = ATN_TARGET;
    }
    if (snap != 0) {
        f32 dx = cam->wpos[0] - pos[0];
        f32 dy = cam->wpos[1] - pos[1];
        f32 dz = cam->wpos[2] - pos[2];
        cam->radius = smallsqrt(dx * dx + dy * dy + dz * dz);
    }
}

void InitCamera(s32 resetAll)
{
    s32 first = resetAll != 0 ? 0 : 1;
    s32 i;

    for (i = first; i < 6; i++) {
        Camera* cam = &gCameras[i];
        s32 k;
        cam->state = i == 0 ? 1 : 0;
        cam->radius = 10.0f;
        cam->mode = 0;
        cam->flags = 0;
        cam->trans_mode = 0;
        cam->timer = 0;
        cam->c_mode = CAM_GAME;
        cam->pc_mode = CAM_GAME;
        cam->a_mode = ATN_TARGET;
        cam->pa_mode = ATN_TARGET;
        cam->camobj = 0;
        cam->attnobj = 0;
        cam->pn = cam->en = cam->gn = cam->mn = cam->ln = cam->cn = -1;
        for (k = 0; k < 4; k++) {
            cam->wpos[k] = 0.0f;
            cam->old_wpos[k] = 0.0f;
            cam->vel[k] = 0.0f;
            cam->avel[k] = 0.0f;
            cam->pyr[k] = 0.0f;
            cam->pyr_delta[k] = 0.0f;
            cam->attn[k] = 0.0f;
            cam->old_attn[k] = 0.0f;
            cam->delta[k] = 0.0f;
            cam->offset[k] = 0.0f;
        }
        cam->mat[0][0] = cam->mat[1][1] = cam->mat[2][2] = cam->mat[3][3] = 1.0f;
    }
    init_targets();
    if (gCameras[0].state != 0) {
        StandardCamera(0);
        ProcCamera(0, 0);
    }
}

extern s32 gCameraWindowLeftLimit;
extern s32 gCameraWindowRightLimit;
extern s32 gCameraWindowTopLimit;
extern s32 gCameraWindowBottomLimit;
extern f32 gCameraWindowScaleX;
extern f32 gCameraWindowScaleY;

void ChangeWindow(void)
{
    f32 centerX = (f32)(gCameraWindowLeftLimit + gCameraWindowRightLimit) * 0.5f;
    f32 centerY = (f32)(gCameraWindowTopLimit + gCameraWindowBottomLimit) * 0.5f;
    f32 halfX = (f32)(gCameraWindowRightLimit - gCameraWindowLeftLimit) *
                0.5f * gCameraWindowScaleX;
    f32 halfY = (f32)(gCameraWindowTopLimit - gCameraWindowBottomLimit) *
                0.5f * gCameraWindowScaleY;

    MBWindowSetRegion(centerX - halfX, centerX + halfX,
                      centerY + halfY, centerY - halfY, 1.0f);
}

void CopyCam(u8* src, u8* dst)
{
    CopyMat4((f32*)(src + 4), (f32*)(dst + 4));
#define CF(o) *(f32*)(dst + (o)) = *(f32*)(src + (o))
#define CI(o) *(u32*)(dst + (o)) = *(u32*)(src + (o))
    CF(0x64); CF(0x68); CF(0x6C); CF(0x74); CF(0x78); CF(0x7C);
    CF(0x84); CF(0x88); CF(0x8C); CF(0x94); CF(0x98); CF(0x9C);
    CF(0xA4); CF(0xA8); CF(0xAC); CF(0xB4); CF(0xB8); CF(0xBC);
    CI(0xD8); CF(0xC4); CI(0xC8); CI(0xCC); CI(0xD0); CI(0xD4);
    CF(0xDC); CF(0xE0); CF(0xE4); CF(0xE8);
    CI(0xEC); CI(0xF0); CI(0xF4); CI(0xF8); CI(0xFC); CI(0x100);
    CI(0x104); CI(0x108); CI(0x10C); CI(0x110); CI(0x114); CI(0x118);
    CF(0x11C); CF(0x120); CF(0x124); CF(0x12C); CF(0x130); CF(0x134);
    CF(0x13C); CF(0x140); CF(0x144); CF(0x14C); CF(0x150); CF(0x154);
    CF(0x15C); CF(0x160); CF(0x164); CF(0x16C); CF(0x170); CF(0x174);
    CF(0x17C); CF(0x180); CF(0x184);
#undef CF
#undef CI
}

void ProcCamera(s32 camIdx, s32 useRecorderPosition)
{
    Camera* cam = &gCameras[camIdx];
    f32 offset[3];

    if (cam->state == 0) {
        return;
    }
    if (cam->a_mode == ATN_FREE) {
        CreateYPRMatrix(&cam->mat[0][0], cam->pyr);
    }
    if (gGameBusy == 0 && gGameplayPauseTimer == 0) {
        WorldVector(cam->vel, offset, &cam->mat[0][0]);
        cam->wpos[0] += offset[0];
        cam->wpos[1] += offset[1];
        cam->wpos[2] += offset[2];
        WorldVector(cam->avel, offset, &cam->mat[0][0]);
        cam->attn[0] += offset[0];
        cam->attn[1] += offset[1];
        cam->attn[2] += offset[2];
    }
    if (useRecorderPosition != 0 && camIdx == 0 && cam->c_mode == CAM_GAME) {
        cam->mat[3][0] = gRecorderCameraPosition[0];
        cam->mat[3][1] = gRecorderCameraPosition[1];
        cam->mat[3][2] = gRecorderCameraPosition[2];
    } else {
        cam->mat[3][0] = cam->wpos[0];
        cam->mat[3][1] = cam->wpos[1];
        cam->mat[3][2] = cam->wpos[2];
    }
}

/*
 * screen_limitation -- keep the active camera inside its configured world
 * limits.  The debug build also printed selectable object information here;
 * clamping is the runtime-relevant part of the routine.
 */
void screen_limitation(s32 camIdx)
{
    Camera* cam = &gCameras[camIdx];
    s32 i;

    if (cam->state == 0) {
        return;
    }
    for (i = 0; i < 3; i++) {
        f32 limit = cam->limit_pos[i];
        f32 range = cam->limit_vel[i];
        if (range > 0.0f) {
            if (cam->attn[i] < limit - range) cam->attn[i] = limit - range;
            if (cam->attn[i] > limit + range) cam->attn[i] = limit + range;
        }
    }
}

void ResetClock(void)
{
    gClockFrameNumber = 0;
    sMusicFadeBase = 0.0f;
    gClockTime = 0.0f;
    InfFrame = 0;
    sLastVBlankCounter = 0;
    gClockFrameReciprocal = 0.0f;
    gClockFrameStep = 0.0f;
    gClockPreviousTime = 0.0f;
}

void InitializeClockIRQ(void)
{
    gClockFrameNumber = 0;
    sMusicFadeBase = 0.0f;
    gClockTime = 0.0f;
    InfFrame = 0;
    sLastVBlankCounter = 0;
    gClockFrameReciprocal = 0.0f;
    gClockFrameStep = 0.0f;
    gClockPreviousTime = 0.0f;
}

void ClockOncePerFrame(void)
{
    s32 freeze = 0;
    s32 resumed = 0;

    if ((sFlags & 8) != 0) {
        freeze = 1;
        if ((sPreviousFlags & 8) != 0) {
            sClockAccumulator = 0;
            freeze = 0;
            resumed = 1;
        } else if ((gControllerButtons & 8) != 0) {
            if (sClockAccumulator < 60) {
                sClockAccumulator += gClockStepTicks;
            } else {
                sClockAccumulator -= 4;
                freeze = 0;
            }
        }
    }
    if ((sFlags & 4) != 0 && gGameMode == 0x4010) {
        freeze = 1;
    }

    gClockStepTicks = pbLoad - sLastTimerCount;
    sLastTimerCount = pbLoad;
    gFrameTicks = gClockStepTicks;
    gClockCurrentTime = pbGetTime();
    gClockElapsedTime = gClockCurrentTime - sLastFrameTime;
    sLastFrameTime = gClockCurrentTime;

    if (gGameBusy != 0 || gModalRenderDepth != 0 || (freeze && !resumed)) {
        gFrameTicks = 0;
        if (gGameBusy != 100) {
            gClockElapsedTime = 0;
        }
        gClockFrameStep = 0.0f;
        gClockFrameReciprocal = 1.0f;
    } else if (resumed || gFrameTicks > 60 || gFrameTicks == 0) {
        gFrameTicks = 2;
        gClockElapsedTime = 10000000;
        gClockFrameStep = 2.0f;
        gClockFrameReciprocal = 0.5f;
    } else {
        gClockFrameStep = (f32)gFrameTicks / 2.0f;
        gClockFrameReciprocal = 1.0f / gClockFrameStep;
    }
    if (gClockElapsedTime > 300000000) {
        gClockElapsedTime = 10000000;
    }
    if (!freeze && gGameBusy == 0) {
        sMusicFadeBase += gClockFrameStep;
        if (sMusicFadeBase > 65536.0f) {
            sMusicFadeBase -= 65536.0f;
        }
        InfFrame++;
        gClockTime = sMusicFadeBase;
        gClockFrameNumber = (s32)(60.0f * sMusicFadeBase + 0.5f);
    }
    gClockPreviousTime = sMusicFadeBase;
}

void PlayerDamagedEnemy(void* player, void* enemy, s32 enemyState,
                        s32 damage, s32 magic, u32 flags, f32* direction)
{
    s32 enemyType = PF(enemy, 0x00, s32);

    if ((enemyType != PF(gPlayers, 0x00, s32) || gGameBusy != 0) &&
        (enemyState == 1 || enemyState == 6)) {
        if (enemyType == -2) {
            enemyType = 1;
        } else if (enemyType == -3) {
            enemyType = 2;
        } else if (enemyType < 0) {
            enemyType = 0;
        }
        damage_enemy(PF(player, 0x00, s32), enemy, damage, magic,
                     flags, direction, enemyType);
    }
}

void PlayerDamagedItem(void* player, void* item, s32 exact)
{
    void* object = *(void**)item;
    s32 type;

    if (object == 0) {
        return;
    }
    type = PF(object, 0x00, s32);
    if (type == 1 && PF(item, 0xC6, s16) < 1 &&
        PF(object, 0x04, s32) == 4) {
        f32 hitPos[3];
        hitPos[0] = PF(item, 0x44, f32);
        hitPos[1] = PF(item, 0x48, f32);
        hitPos[2] = PF(item, 0x4C, f32);
        AddItem(PF(player, 0x00, s32), hitPos, PF(object, 0x3C, s32), 0);
        DeleteItem(item, 1);
    } else if (type == 3) {
        s32 damageType = PF(item, 0xDC, s16);
        if (exact != 0) {
            PF(player, 0x918, s32) = 0;
            PF(player, PF(player, 0x0C, s32) * 0x1C + 0xC20, s32)++;
        }
        if (damageType == -2) {
            damageType = 1;
        } else if (damageType == -3) {
            damageType = 2;
        } else if (damageType < 0) {
            damageType = 0;
        }
        damage_enemy(PF(player, 0x00, s32), item,
                     exact ? 5 : 1, 0, damageType, 0);
    }
}

void ModifyDamage(f32 armor, f32* damage, u32* damageType, u32 shield)
{
    f32 value = *damage;
    u32 type = *damageType;
    f32 weak;
    f32 resist;
    f32 strong;
    u32 color;

    if ((shield & 0x100000) != 0) {
        if ((f64)value <= 1.0) {
            *damage = 0.0f;
        } else {
            *damage = (f32)(0.1 * -(f64)value);
        }
        return;
    }
    if ((shield & 0x10000) != 0 ||
        ((shield & 0x1000) != 0 && (type & 0x200) != 0) ||
        ((shield & 0x2000) != 0 && (type & 0x800) != 0)) {
        *damage = 0.0f;
        return;
    }

    if (gBossType >= 0) {
        weak = 0.75f;
        resist = 1.25f;
        strong = 1.5f;
    } else {
        weak = 0.5f;
        resist = 1.5f;
        strong = 2.0f;
    }
    if ((shield & 0x40000) != 0) {
        *damageType &= 0xFFFEFE8F;
    }
    if ((shield & 0x10) != 0 && (type & 0x200) != 0) {
        value *= weak;
    }

    if ((type & 0xA00) == 0) {
        f32 original = value;
        value = 0.0f;
        if (original < 0.0f) {
            value = -original;
        } else if (original > armor) {
            value = original - armor;
        }
    }
    if ((f64)value > 0.0) {
        color = type & 0xF;
        switch (color) {
        case 1:
            if ((shield & 1) != 0) {
                value *= weak;
            } else if ((shield & 0x100) != 0) {
                value = 0.0f;
            } else if ((shield & 2) == 0 && (shield & 0x200) == 0) {
                value *= resist;
            } else {
                value *= strong;
            }
            break;
        case 2:
            if ((shield & 2) != 0) {
                value *= weak;
            } else if ((shield & 0x200) != 0) {
                value = 0.0f;
            } else if ((shield & 1) == 0 && (shield & 0x100) == 0) {
                value *= resist;
            } else {
                value *= strong;
            }
            break;
        case 3:
            if ((shield & 4) != 0) {
                value *= weak;
            } else if ((shield & 0x400) != 0) {
                value = 0.0f;
            } else if ((shield & 8) == 0 && (shield & 0x800) == 0) {
                value *= resist;
            } else {
                value *= strong;
            }
            break;
        case 4:
            if ((shield & 8) != 0) {
                value *= weak;
            } else if ((shield & 0x800) != 0) {
                value = 0.0f;
            } else if ((shield & 4) == 0 && (shield & 0x400) == 0) {
                value *= resist;
            } else {
                value *= strong;
            }
            break;
        }
    }
    *damage = value;
}

s32 DamageColor(s32 type)
{
    switch (type & 0xF) {
    default:
        return -1;
    case 1:
        return 2;
    case 2:
        return 1;
    case 3:
        return 0;
    case 4:
        return 3;
    }
}

s32 LineCylinderCollide(f32* center, f32 radius, f32 halfHeight,
                        f32* from, f32* to, f32* hit, s32 directional)
{
    f32 closest[3];
    f32 delta[3];
    f32 fromDir[3];
    f32 centerDir[3];
    f32 distance;

    distance = PointLineColl(center, from, to, closest);
    if (distance > radius + halfHeight) {
        return 0;
    }
    delta[0] = closest[0] - center[0];
    delta[1] = closest[1] - center[1];
    delta[2] = closest[2] - center[2];
    if (fqdist(delta[0], delta[2]) > radius ||
        (delta[1] < 0.0f ? -delta[1] : delta[1]) > halfHeight) {
        return 0;
    }

    if (directional != 0) {
        fromDir[0] = center[0] - from[0];
        fromDir[1] = 0.0f;
        fromDir[2] = center[2] - from[2];
        distance = NormalVector2D(fromDir);
        if (distance <= radius) {
            centerDir[0] = to[0] - from[0];
            centerDir[1] = 0.0f;
            centerDir[2] = to[2] - from[2];
            {
                f32 lineLength = NormalVector2D(centerDir);
                if (distance < 0.001f) {
                    if (lineLength < 0.001f) {
                        if (hit != 0) {
                            hit[0] = closest[0];
                            hit[1] = closest[1];
                            hit[2] = closest[2];
                        }
                        return 1;
                    }
                    return 0;
                }
                if (centerDir[0] * fromDir[0] +
                    centerDir[2] * fromDir[2] < -0.01f) {
                    return 0;
                }
            }
        }
    }
    if (hit != 0) {
        hit[0] = closest[0];
        hit[1] = closest[1];
        hit[2] = closest[2];
    }
    return 1;
}

s32 MissileCollideEnemy(f32 radius, f32* from, f32* to, f32* hit,
                        s32 cooldownSlot, s32 respectCooldown, s32 firstEnemy)
{
    f32 segX = to[0] - from[0];
    f32 segY = to[1] - from[1];
    f32 segZ = to[2] - from[2];
    f32 horizontalLen2 = segX * segX + segZ * segZ;
    f32 verticalLen2 = segY * segY;
    s32 i;

    for (i = firstEnemy; i < 25; i++) {
        u8* enemy = gEnemies + i * ENEMY_STRIDE;
        s32 state = PF(enemy, 0xB4, s32);
        if ((state == 1 || state == 6) &&
            (!respectCooldown ||
             PF(enemy, 0x2B4 + cooldownSlot * 4, f32) <= sMusicFadeBase)) {
            f32 enemyRadius = radius + PF(enemy, 0x238, f32);
            f32 enemyHeight = radius + PF(enemy, 0x23C, f32);
            f32 dx = PF(enemy, 0x54, f32) - to[0];
            f32 dy = PF(enemy, 0x58, f32) - to[1];
            f32 dz = PF(enemy, 0x5C, f32) - to[2];
            if (dx * dx + dz * dz <= enemyRadius * enemyRadius + horizontalLen2 &&
                dy <= verticalLen2 + enemyHeight &&
                LineCylinderCollide((f32*)(enemy + 0x54), enemyRadius,
                                    enemyHeight, from, to, hit, 0)) {
                return i;
            }
        }
    }
    return -1;
}

void* MissileCollidePlayer(f32 radius, f32* from, f32* to, f32* hit)
{
    u8 framePad[8];
    s32 i;

    for (i = 0; i < 4; i++) {
        u8* player = gPlayers + i * PLAYER_STRIDE;
        if (PF(player, 0xE8, s32) == 1) {
            f32 halfHeight = radius + PF(player, 0x854, f32);
            f32 cylinderRadius = radius + PF(player, 0x850, f32);
            if (LineCylinderCollide((f32*)(player + 0x64),
                                    cylinderRadius, halfHeight,
                                    from, to, hit, 0)) {
                return player;
            }
        }
    }
    return 0;
}

/*
 * StartMissile -- common effect allocator/configurator shared by enemy and
 * player launch paths.  MissileDesc is the PDB-described runtime definition;
 * the effect system owns movement after these attributes are installed.
 */
s32 StartMissile(s32 owner, f32* position, f32* velocity, u32 damageType,
                 MissileDesc* desc, void* missileTree, s32 variant,
                 u32 extraFlags)
{
    s32 effect;

    if (missileTree == 0) {
        ErrorPrintf("ERROR: missile has no animation tree");
        return -1;
    }
    if (velocity[0] * velocity[0] + velocity[1] * velocity[1] +
        velocity[2] * velocity[2] < 0.001f) {
        FatalError("ERROR: ZERO LENGTH MISSILE VEL");
        return -1;
    }

    effect = StartFXTree(missileTree, position, 0,
                         extraFlags | 0x1000000, 0.0f);
    if (effect < 0) {
        return effect;
    }
    SfxSetDamage(desc->damage, desc->radius, desc->scale,
                 effect, damageType | desc->flags, owner);
    SfxSetHit(effect, desc->hitEffect, desc->hitSound, desc->wallSound);
    SfxSetMat(effect, 0, position);
    SfxSetOwner(effect, owner);
    return effect;
}

void CalcTargetDir(f32* velocity, f32 targetScale, f32 speed,
                   f32 gravity, f32 lift)
{
    u8 framePad[8];
    f32 horizontal = fqdist(velocity[0], velocity[2]);
    f64 inverse;

    if ((f64)horizontal > 0.001) {
        inverse = 1.0 / (f64)horizontal;
    } else {
        inverse = 1.0;
    }
    {
        f32 norm = (f32)inverse;
        velocity[0] = velocity[0] * norm;
        velocity[2] = velocity[2] * norm;
        velocity[1] =
            (f32)((f64)speed *
                  (0.5 * (f64)gravity *
                       (f64)(f32)(horizontal * speed) +
                   (f64)((velocity[1] + lift) * (targetScale * norm))));
    }
}

s32 EnemyStartMissile(void* enemy, f32* target, s32 missileType, s32 slot)
{
    s32 enemyType = PF(enemy, 0x00, s32);
    MissileDesc* desc = &EnemyMissileInfo[enemyType * 3 + slot];
    void* tree = gEnemyMissileTrees[enemyType][slot];
    f32 position[3];
    f32 velocity[3];
    f32 speed;

    if (tree == 0) {
        ErrorPrintf("ENEMY %d HAS NO MISSILE TYPE %d", enemyType, slot);
        return -1;
    }
    position[0] = PF(enemy, 0x54, f32);
    position[1] = PF(enemy, 0x58, f32) + desc->scale;
    position[2] = PF(enemy, 0x5C, f32);
    velocity[0] = target[0] - position[0];
    velocity[1] = target[1] - position[1];
    velocity[2] = target[2] - position[2];
    speed = desc->speed * PF(enemy, 0x310, f32);
    if (slot == 2) {
        f32 length = smallsqrt(velocity[0] * velocity[0] +
                               velocity[1] * velocity[1] +
                               velocity[2] * velocity[2]);
        if (length > 0.001f) {
            velocity[0] /= length;
            velocity[1] /= length;
            velocity[2] /= length;
        }
    } else {
        CalcTargetDir(velocity, 1.0f, 1.0f / speed, desc->weight, -2.5f);
    }
    return StartMissile(enemyType, position, velocity,
                        desc->flags, desc, tree, slot, 0);
}

s32 PlayerStartMissile(void* player, f32* direction, u32 damageType, s32 slot)
{
    s32 playerIndex = PF(player, 0x00, s32);
    MissileDesc* desc = &PlayerMissileInfo[PF(player, 0x0C, s32) * 5 +
                                           (damageType & 0xF)];
    f32 position[3];
    f32 velocity[3];
    s32 effect;

    position[0] = PF(player, 0x64, f32) + direction[0] * -3.0f;
    position[1] = PF(player, 0x68, f32) + direction[1] * -3.0f;
    position[2] = PF(player, 0x6C, f32) + direction[2] * -3.0f;
    velocity[0] = direction[0] * desc->speed;
    velocity[1] = direction[1] * desc->speed;
    velocity[2] = direction[2] * desc->speed;

    if ((damageType & 0x100000) == 0) {
        CalcTargetDir(velocity, 1.0f, 1.0f / desc->speed,
                      desc->weight, -0.5f);
    }
    effect = StartMissile(playerIndex + 1, position, velocity, damageType,
                          desc, gPlayerMissileRuntime[playerIndex].missileTree,
                          slot, (damageType & 0x100000) ? 0x10000 : 0);
    if (slot >= 0 && slot < 5) {
        gPlayerMissiles[playerIndex * 5 + slot] = effect;
    }
    return effect;
}

void InitEnemyMissiles(s32 enemyType)
{
    s32 slot;
    void* enemyTree = EnemyTypePrefix(enemyType);

    for (slot = 0; slot < 3; slot++) {
        char* name = EnemyMissileDesc[enemyType * 3 + slot];
        if (enemyTree == 0 || name == 0 || *name == '\0') {
            gEnemyMissileTrees[enemyType][slot] = 0;
        } else {
            gEnemyMissileTrees[enemyType][slot] =
                AtreeMatch(enemyTree, name, 0);
        }
    }
}

void InitPlayerMissiles(void* player)
{
    s32 playerIndex = PF(player, 0x00, s32);
    s32 i;
    void* playerTree = PF(player, 0x74, void*);

    for (i = 0; i < 5; i++) {
        gPlayerMissiles[playerIndex * 5 + i] = -1;
        gPlayerWeaponHoldTrees[playerIndex][i] = 0;
        if (playerTree != 0 && PlayerMissileDesc[i] != 0) {
            gPlayerWeaponHoldTrees[playerIndex][i] =
                AtreeMatch(playerTree, PlayerMissileDesc[i], 1);
        }
    }
    gPlayerFamiliarTrees[playerIndex][0] = 0;
    gPlayerFamiliarTrees[playerIndex][1] = 0;
    gPlayerFamiliarSpitTrees[playerIndex] = 0;
    gPlayerMissileRuntime[playerIndex].missileTree =
        playerTree != 0 ? AtreeMatch(playerTree, "THROW1", 0) : 0;
    gPlayerMissileRuntime[playerIndex].trailEffect = 0;
    if (gPlayerMissileRuntime[playerIndex].missileTree == 0) {
        ErrorPrintf("Player Missile not found");
    }
}
