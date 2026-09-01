#include "types.h"

#ifndef offsetof
#define offsetof(type, memb) ((u32) & ((type*)0)->memb)
#endif

/* Boss subsystem (GCN BOSS.OBJ region, 0x8001A960-0x8001BC88). Names are the
 * real ones from the Xbox build's BOSS.OBJ (shell3D.pdb). The GameCube MWCC
 * build emits this TU's functions in the reverse of the Xbox link order, so
 * BossSpewCoins (last in the PDB) is first here and BossInit (first in the PDB)
 * is last.
 *
 * Byte-exact (modulo cosmetic reloc names that link-resolve identically):
 * BossInit / BossDying / HealthMeterInit / BossActivate / AddBoss /
 * StartSpewItem.  The .bss anchor GATE is resolved: BossSpewCoins is
 * reconstructed and the unreferenced boss_bss_order() helper pins the .bss
 * block to declaration order (HealthMeterBG @ .bss+0, gSpewItems @ +0x6c), so
 * gSpewItems / gBossPos / HealthMeter* now pool off HealthMeterBG.  The coin
 * names + meter/error format strings are reconstructed in-TU as sBossStr so
 * MWCC restores .rodata sibling-pooling (extern would kill it).
 *
 * Reconstructed with real bodies but carrying register-renumber / FP-form
 * residuals (parked, see PARKED.txt): BossSpewCoins, ProcessSpewItems,
 * HealthMeterStart, HealthMeterUpdate, BossDeath.  BossGenerateEnemy is still
 * a stub. */

/* ------------------------------------------------------------------ */
/* boss-owned small data (.sbss)                                       */
/* ------------------------------------------------------------------ */
int   gBossActive;      /* 0x80344378 */
int   gBossDead;        /* 0x8034437C */
int   gBossDying;       /* 0x80344380 */
int   HealthMeterNum;   /* 0x80344384 */
int   HealthMeterX;     /* 0x80344388 */
int   HealthMeterNextX; /* 0x8034438C */
int   gNumSpewItems;    /* 0x80344390 */
int   gBossKeyBlit;     /* 0x80344394 */
int   gBoss398;         /* 0x80344398 */
int   gBossType;        /* 0x8034439C */
void* gBossObj;         /* 0x803443A0 (8-byte record: ptr + unk) */
int   gBoss3A4;         /* 0x803443A4 */

/* ------------------------------------------------------------------ */
/* boss-owned bss (addresses in comments).                              */
/*                                                                      */
/* .bss ANCHOR RESIDUAL (blocks AddBoss/StartSpewItem/ProcessSpewItems/ */
/* HealthMeterUpdate/HealthMeterStart/BossDeath): the target lays this   */
/* block out in pure DECLARATION order (HealthMeterBG @ .bss+0,          */
/* gSpewItems @ +0x6c, gBossPos @ +0x26c=620) and MWCC -O4 addresses the */
/* whole block from HealthMeterBG as the base register -- e.g. AddBoss   */
/* stores gBossPos at HealthMeterBG+620.  With -common off MWCC allocates */
/* .bss by FIRST-REFERENCE order (referenced globals first in use order, */
/* unreferenced last in reverse decl order).  Because BossSpewCoins (fn  */
/* #1) is still a stub, the first real .bss touch is gSpewItems in        */
/* ProcessSpewItems, so gSpewItems lands at offset 0 and HealthMeterBG    */
/* later -- wrong base, wrong offsets.  The GATE is reconstructing        */
/* BossSpewCoins: its target references HealthMeterBG (ADDR16) at +0x30,  */
/* BEFORE any other .bss global, which is what pins HealthMeterBG to      */
/* offset 0 and pools the block in decl order.  Until BossSpewCoins is    */
/* byte-matched, none of the gSpewItems/gBossPos-relative fns can match.  */
/* ------------------------------------------------------------------ */
typedef struct SpewItem {
    /* 0x0 */ void* obj;
    /* 0x4 */ f32 vx;
    /* 0x8 */ f32 vy;
    /* 0xC */ f32 vz;
} SpewItem;

void* HealthMeterBG[3][2];      /* 0x8023E608 */
void* HealthMeterFG[3][2];      /* 0x8023E620 */
int   HealthMeterNPieces[3];    /* 0x8023E638 */
f32   HealthMeterValue[3];      /* 0x8023E644 */
f32   HealthMeterMaxValue[3];   /* 0x8023E650 */
int   HealthMeter65C[3];        /* 0x8023E65C */
int   HealthMeter668[3];        /* 0x8023E668 */
SpewItem gSpewItems[32];        /* 0x8023E674, 0x200 */
f32   gBossPos[3];              /* 0x8023E874 */

/* ------------------------------------------------------------------ */
/* shared externs (owned by other TUs)                                */
/* ------------------------------------------------------------------ */
extern int lbl_803444E0;   /* 0x803444E0 */
extern int sMusicSubIndex;                /* 0x803448EC */
extern int sMusicSubState;                /* 0x803448E8 */

extern void add_target(int obj);
extern void* CritterTypeLoaded(int a, int b);
extern void* CritterNewInst(int a, int b, void* obj);
extern void* CopyMat4(void* a, void* b);
extern void* PlaceItem(s32 a, s32 b, char* name, void* mat);
extern void YawVec3(void* axis, f32* out, f32 angle);
extern f32  Random(f32 a);
extern s32  StartFXSub(s32 type, f32* pos, u32 fla, u32 flb, f32 time);
extern void SfxSetDamage(f32 damage, f32 radius, f32 delay, s32 idx, s32 type, s32 owner);

/* BOSS.OBJ-external data pooled into other TUs (referenced by ADDR16). */
typedef struct BossType42Entry {
    /* 0x00 */ char name[16];
    /* 0x10 */ s32  a;
    /* 0x14 */ s32  b;
    /* 0x18 */ f32  c;
} BossType42Entry;              /* 0x1c */

typedef struct BossSpewData {
    /* 0x000 */ s32 counts[14][3];        /* per-world bronze/silver/gold */
    /* 0x0a8 */ BossType42Entry entries[4];
} BossSpewData;                 /* 0x118 */

extern BossSpewData lbl_801189E0;         /* 0x801189E0 spew tables */
extern f32         gIdentityMatrix[16];      /* item matrix template */
extern s32         lbl_8034476C;          /* coin count multiplier */
extern s32         sMusicTrackHi;         /* 0x803448D8 world index */
extern f32         gClockFrameStep;       /* physics timestep */
extern f32         lbl_80344880;          /* ground-probe height */

extern f32  FloorPos(f32 a, f32 b, void* c, s32 d);   /* ground/collision query */
extern void UpdateObjWorldMat(void* p);

extern int   sprintf(char* buf, const char* fmt, ...);
extern void  ErrorPrintf(const char* fmt, ...);
extern void* MBNewBlit(char* name, int x, int flags);
extern void  MBBlitSetAlpha(void* blit, int a);
extern char  lbl_80345B60;                /* meter-name suffix (named) */
extern char  lbl_80345B64;                /* meter-name suffix (blank) */
extern int   lbl_80343B70;                /* blit layer/flags */

extern void StartGoodWizard(void);
extern int  GetWorldOrder(int worldIdx);
extern void PlayerGiveRune(int i, int worldOrder);
extern int  InitCustomEffect(int buf, char* name, int c, int d);
extern int  SfxSetMorph(f32 a, int blit, int e2, int d);
extern void fn_8009F340(f32* pos);
extern void mbBlitInit3414(void* blit, int a);
typedef struct BossPlayerView {
    u8 _0[232];
    s32 state;
    u8 _ec[0x335c - 236];
} BossPlayerView;
extern BossPlayerView gPlayers[];     /* per-slot item/rune records (0x335c) */
extern int  sItemFile1Buf;                /* 0x80344974 */
extern char str_BOSSKEY;                  /* .sdata2 effect name */
extern char str_BOSSKEY2[];               /* .rodata effect name */

extern void mbBlitProject(void* blit, int alpha, int c);
extern void mbBlitSetupVerts(void* blit, f32 a, f32 b, f32 c, f32 d);
extern void MBBlitSetColor(void* blit, u32 color);
extern u32  gFrameTicks;                 /* meter approach rate */

extern u8  gEnemies[];                   /* enemy records (stride 0x394) */
extern f32 lbl_80343B74[2];              /* add-spawn yaw angles */
extern f64 lbl_80345B78;                 /* add-spawn spacing scale */
extern f32 lbl_80345AD0;                 /* add move speed */
extern f32 lbl_80345B10;                 /* generate_enemy angle arg */
extern s32 generate_enemy(f32* pos, s32 type, s32 level, f32* dir, s32 spew,
                          void* gen, s32 imp, f32 ang);
extern f32 atan2(f32 y, f32 x);

/* ------------------------------------------------------------------ */
/* forward decls (address order)                                      */
/* ------------------------------------------------------------------ */
void BossSpewCoins(f32 v, f32* pos, f32* dir);
void ProcessSpewItems(void);
int  StartSpewItem(f32 a, int b, int c, char* name, int e, int f, void* pos, void* vel);
void HealthMeterUpdate(f32 v, int meter);
int  HealthMeterStart(f32 v, char* name, int n, int p, int x, int y, int flag);
void HealthMeterInit(void);
void BossActivate(void* obj, int flag);
void BossDeath(void);
void BossDying(void);
struct BossObjView {
    u8  _0[0x20];
    f32 dir[3];   /* 0x20 */
    u8  _2c[4];
    f32 pos[3];   /* 0x30 */
};

/* Local view of the placed-item object pointed to by gSpewItems[i].obj
 * (a `struct item` per research/xbox_symbols/misc.h Id=3252, GC-verified
 * layout in include/game/item.h Item, sizeof 0xf0).  Kept as a file-local
 * partial view (not #include "game/item.h") because item.h's PlaceItem
 * prototype returns Item* while this TU's own extern declares it as void*;
 * only the fields this TU touches are named.  `data` is the 0x14-byte
 * anim/oanim/texmod/psys union (Item.data) whose runtime sub-layout is not
 * individually verified -- offsets within it stay raw. */
typedef struct SpewObjView {
    u8  _0[0x4];           /* info ptr */
    u8  objgrp[0x30];      /* 0x04 embedded OBJGRP (worldmat rows 0-2)   */
    f32 worldPos[3];      /* 0x34 objgrp.worldmat[3][0..2] world position */
    u8  _40[0xc4 - 0x40];
    s16 active;            /* 0xc4 */
    u8  _c6[0xdc - 0xc6];
    u8  data[0x14];         /* 0xdc union: anim/oanim/texmod/psys + runtime */
} SpewObjView;

/* Local view of the tail of struct Enemy (include/game/enemy.h, sizeof
 * 0x394) touched by BossGenerateEnemy.  Not #include "game/enemy.h" -
 * this TU's own extern declares gEnemies as a raw u8[] (stride-only,
 * comment-verified), and enemy.h's own `extern Enemy gEnemies[25];` would
 * conflict with that declaration's type. */
typedef struct EnemyTailView {
    u8  _0[0x240];
    f32 pyr[3];         /* 0x240 pitch/yaw/roll   */
    f32 ang;            /* 0x24c facing angle     */
    f32 angbak;         /* 0x250                  */
    u8  _254[0x2ec - 0x254];
    f32 birth_pos[3];   /* 0x2ec spawn position    */
} EnemyTailView;

/* Local view of struct Critter (include/game/critter.h, sizeof 0xae0)
 * touched by BossDeath/HealthMeterUpdate via gBossObj.  Not #include
 * "game/critter.h" for the same file-local-view uniformity as above;
 * hdr's own target (CritterHeader) has no verified fields at +0xd0/+0xd4/
 * +0xd8, so those stay raw offsets off the resolved hdr pointer. */
typedef struct BossCritterView {
    u8    _0[0x4];
    void* hdr;                /* 0x004 CritterHeader* (see critter.h)     */
    u8    _8[0xc - 0x8];
    f32   mtx[3][4];          /* 0x00c world transform (3x4 Mtx)          */
    u8    _3c[0x418 - 0x3c];
    f32   prevMovePathPos[3]; /* 0x418 prior-frame movePathPos snapshot   */
    u8    _424[0xac4 - 0x424];
    s16   pausecnt;           /* 0xac4 anim pause counter                 */
} BossCritterView;

void BossGenerateEnemy(struct BossObjView* o);
void AddBoss(void* obj);
void BossInit(void);

/* boss.c-owned .rodata string pool (0x60 bytes; dtk auto-split as
 * lbl_80111748).  Kept as ONE object so MWCC pools the coin names and the
 * meter/error format strings off a single base register (extern would kill
 * that pooling and re-materialize each address). */
static const struct {
    char coin[3][12];
    char errFmt[28];
    char bgFmt[16];
    char fgFmt[16];
} sBossStr = {
    { "COIN_BRONZE", "COIN_SILVER", "COIN_GOLD" },
    "Too many health meters: %d",
    "%s%sMETER_BG%d",
    "%s%sMETER_FG%d",
};

/* Pin the .bss block to declaration order: touch every array in address
 * order BEFORE the real functions so first-use order == decl order.  This
 * keeps HealthMeterBG at .bss+0 (mwld strips this unreferenced helper). */
static void boss_bss_order(void) {
    HealthMeterBG[0][0] = 0;
    HealthMeterFG[0][0] = 0;
    HealthMeterNPieces[0] = 0;
    HealthMeterValue[0] = 0.0f;
    HealthMeterMaxValue[0] = 0.0f;
    HealthMeter65C[0] = 0;
    HealthMeter668[0] = 0;
    gSpewItems[0].obj = 0;
    gBossPos[0] = 0.0f;
}

/* ================================================================== */
/* best-effort (not byte-matched) bodies for the FP-heavy routines     */
/* ================================================================== */
void BossSpewCoins(f32 v, f32* pos, f32* dir) {
    f32 vec[3];
    f32 step;
    f32 angle;
    int i;
    int n;
    int h;

    h = StartFXSub(0, pos, 34, 0, 5.0f);
    SfxSetDamage(1000.0f, 1000.0f, 0.0f, h, 0, 0);

    if (gBossType == 42) {
        f32 lvec[3];
        step = 2.0f * v * 0.25f;
        angle = -v + 0.5 * step;
        for (i = 0; i < 4; i++) {
            YawVec3(dir, lvec, angle);
            StartSpewItem(lbl_801189E0.entries[i].c, 1, 9,
                          lbl_801189E0.entries[i].name,
                          lbl_801189E0.entries[i].a, lbl_801189E0.entries[i].b,
                          pos, lvec);
            angle += step;
        }
    } else if (lbl_8034476C != 0) {
        n = lbl_8034476C * lbl_801189E0.counts[sMusicTrackHi][0];
        if (n != 0) {
            f32 mat[16];
            step = 2.0f * v / (f32)n;
            angle = -v + 0.5 * step;
            for (i = 0; i < n; i++) {
                vec[0] = dir[0] * (0.85 + Random(0.1f));
                vec[1] = dir[1] * (0.85 + Random(0.1f));
                vec[2] = dir[2] * (0.85 + Random(0.1f));
                YawVec3(dir, vec, angle);
                if (gNumSpewItems < 32) {
                    CopyMat4(gIdentityMatrix, mat);
                    mat[12] = pos[0];
                    mat[13] = pos[1];
                    mat[14] = pos[2];
                    gSpewItems[gNumSpewItems].obj =
                        PlaceItem(1, 1, (char*)sBossStr.coin[0], mat);
                    if (gSpewItems[gNumSpewItems].obj != 0) {
                        gSpewItems[gNumSpewItems].vx = vec[0];
                        gSpewItems[gNumSpewItems].vy = vec[1];
                        gSpewItems[gNumSpewItems].vz = vec[2];
                        *(s32*)((char*)gSpewItems[gNumSpewItems].obj + offsetof(SpewObjView, data) + 4) = 500;
                        *(s32*)((char*)gSpewItems[gNumSpewItems].obj + offsetof(SpewObjView, data)) = 0;
                        *(f32*)((char*)gSpewItems[gNumSpewItems].obj + offsetof(SpewObjView, data) + 8) = 0.0f;
                        *(s16*)((char*)gSpewItems[gNumSpewItems].obj + offsetof(SpewObjView, data) + 0x10) = 120;
                        gNumSpewItems++;
                    }
                }
                angle += step;
            }
        }
        n = lbl_8034476C * lbl_801189E0.counts[sMusicTrackHi][1];
        if (n != 0) {
            f32 mat[16];
            step = 2.0f * v / (f32)n;
            angle = -v + 0.5 * step;
            for (i = 0; i < n; i++) {
                vec[0] = dir[0] * (0.8 + Random(0.1f));
                vec[1] = dir[1] * (0.8 + Random(0.1f));
                vec[2] = dir[2] * (0.8 + Random(0.1f));
                YawVec3(dir, vec, angle);
                if (gNumSpewItems < 32) {
                    CopyMat4(gIdentityMatrix, mat);
                    mat[12] = pos[0];
                    mat[13] = pos[1];
                    mat[14] = pos[2];
                    gSpewItems[gNumSpewItems].obj =
                        PlaceItem(1, 1, (char*)sBossStr.coin[1], mat);
                    if (gSpewItems[gNumSpewItems].obj != 0) {
                        gSpewItems[gNumSpewItems].vx = vec[0];
                        gSpewItems[gNumSpewItems].vy = vec[1];
                        gSpewItems[gNumSpewItems].vz = vec[2];
                        *(s32*)((char*)gSpewItems[gNumSpewItems].obj + offsetof(SpewObjView, data) + 4) = 1000;
                        *(s32*)((char*)gSpewItems[gNumSpewItems].obj + offsetof(SpewObjView, data)) = 0;
                        *(f32*)((char*)gSpewItems[gNumSpewItems].obj + offsetof(SpewObjView, data) + 8) = 0.0f;
                        *(s16*)((char*)gSpewItems[gNumSpewItems].obj + offsetof(SpewObjView, data) + 0x10) = 120;
                        gNumSpewItems++;
                    }
                }
                angle += step;
            }
        }
        n = lbl_8034476C * lbl_801189E0.counts[sMusicTrackHi][2];
        if (n != 0) {
            f32 mat[16];
            step = 2.0f * v / (f32)n;
            angle = -v + 0.5 * step;
            for (i = 0; i < n; i++) {
                vec[0] = dir[0] * (0.75 + Random(0.1f));
                vec[1] = dir[1] * (0.75 + Random(0.1f));
                vec[2] = dir[2] * (0.75 + Random(0.1f));
                YawVec3(vec, vec, angle);
                if (gNumSpewItems < 32) {
                    CopyMat4(gIdentityMatrix, mat);
                    mat[12] = pos[0];
                    mat[13] = pos[1];
                    mat[14] = pos[2];
                    gSpewItems[gNumSpewItems].obj =
                        PlaceItem(1, 1, (char*)sBossStr.coin[2], mat);
                    if (gSpewItems[gNumSpewItems].obj != 0) {
                        gSpewItems[gNumSpewItems].vx = vec[0];
                        gSpewItems[gNumSpewItems].vy = vec[1];
                        gSpewItems[gNumSpewItems].vz = vec[2];
                        *(s32*)((char*)gSpewItems[gNumSpewItems].obj + offsetof(SpewObjView, data) + 4) = 5000;
                        *(s32*)((char*)gSpewItems[gNumSpewItems].obj + offsetof(SpewObjView, data)) = 0;
                        *(f32*)((char*)gSpewItems[gNumSpewItems].obj + offsetof(SpewObjView, data) + 8) = 0.0f;
                        *(s16*)((char*)gSpewItems[gNumSpewItems].obj + offsetof(SpewObjView, data) + 0x10) = 120;
                        gNumSpewItems++;
                    }
                }
                angle += step;
            }
        }
    }
}

static void clampAxis(f32* p, f32 lim) {
    if (*p <= lim) {
        if (-lim <= *p) {
            *p = 0.0f;
        } else {
            *p = -(f32)(lim * *p - *p);
        }
    } else {
        *p = -(f32)(lim * *p - *p);
    }
}

void ProcessSpewItems(void) {
    int i;
    void* obj;
    f32* pvx;
    f32* pvy;
    f32* pvz;
    f32 lim;
    f32 h;

    for (i = 0; i < gNumSpewItems; i++) {
        SpewItem* it = &gSpewItems[i];
        obj = it->obj;
        if (obj == 0) {
            continue;
        }
        if ((*(s16*)((char*)obj + offsetof(SpewObjView, active)) & 0x100) != 0) {
            it->obj = 0;
            continue;
        }
        pvx = &it->vx;
        pvy = &it->vy;
        pvz = &it->vz;
        *(f32*)((char*)obj + offsetof(SpewObjView, worldPos[0])) =
            gClockFrameStep * it->vx + *(f32*)((char*)obj + offsetof(SpewObjView, worldPos[0]));
        *(f32*)((char*)obj + offsetof(SpewObjView, worldPos[1])) =
            gClockFrameStep * it->vy + *(f32*)((char*)obj + offsetof(SpewObjView, worldPos[1]));
        *(f32*)((char*)obj + offsetof(SpewObjView, worldPos[2])) =
            gClockFrameStep * it->vz + *(f32*)((char*)obj + offsetof(SpewObjView, worldPos[2]));

        h = 10.0f;
        lim = 0.5 * gClockFrameStep;
        if (it->vy <= 0.0f) {
            f64 g = FloorPos(lbl_80344880, 1.0f, (char*)obj + offsetof(SpewObjView, worldPos[0]), 0);
            h = *(f32*)((char*)obj + offsetof(SpewObjView, worldPos[1])) - (f32)(1.0 + g);
            if (h < 0.1) {
                *pvy = (f32)(0.4 * -*pvy);
                if (*pvy < 0.1f) {
                    *pvy = 0.0f;
                }
                *(f32*)((char*)obj + offsetof(SpewObjView, worldPos[1])) = (f32)(1.0 + g);
                lim = 4.0 * gClockFrameStep;
            }
        }
        if (0.1 <= h) {
            *pvy = -(f32)(8.0 * gClockFrameStep - *pvy);
        }
        clampAxis(pvx, lim);
        clampAxis(pvz, lim);
        UpdateObjWorldMat((char*)it->obj + offsetof(SpewObjView, objgrp));
    }
}

int StartSpewItem(f32 a, int b, int c, char* name, int e, int f, void* pos, void* vel) {
    f32 mat[16];

    if (gNumSpewItems >= 32) {
        return -1;
    }
    CopyMat4(gIdentityMatrix, mat);
    mat[12] = ((f32*)pos)[0];
    mat[13] = ((f32*)pos)[1];
    mat[14] = ((f32*)pos)[2];
    gSpewItems[gNumSpewItems].obj = PlaceItem(b, c, name, mat);
    if (gSpewItems[gNumSpewItems].obj == 0) {
        return -1;
    }
    gSpewItems[gNumSpewItems].vx = ((f32*)vel)[0];
    gSpewItems[gNumSpewItems].vy = ((f32*)vel)[1];
    gSpewItems[gNumSpewItems].vz = ((f32*)vel)[2];
    if (b == 1) {
        *(s32*)((char*)gSpewItems[gNumSpewItems].obj + offsetof(SpewObjView, data) + 4) = e;
        *(s32*)((char*)gSpewItems[gNumSpewItems].obj + offsetof(SpewObjView, data)) = f;
        *(f32*)((char*)gSpewItems[gNumSpewItems].obj + offsetof(SpewObjView, data) + 8) = a;
        *(s16*)((char*)gSpewItems[gNumSpewItems].obj + offsetof(SpewObjView, data) + 0x10) = 120;
    }
    gNumSpewItems++;
    return gNumSpewItems - 1;
}

void HealthMeterUpdate(f32 v, int meter) {
    int i;
    int flags;
    int a1;
    int a2;
    f32 pct;
    f32 cv;

    if (v < 0.0f) {
        v = 0.0f;
    }
    if (HealthMeterValue[meter] > v) {
        f32 nv = HealthMeterValue[meter] - (f32)(gFrameTicks * 3);
        HealthMeterValue[meter] = nv;
        if (nv < v) {
            HealthMeterValue[meter] = v;
        }
    } else if (HealthMeterValue[meter] < v) {
        f32 nv = HealthMeterValue[meter] + (f32)(gFrameTicks * 3);
        HealthMeterValue[meter] = nv;
        if (v < nv) {
            HealthMeterValue[meter] = v;
        }
    }
    flags = HealthMeter65C[meter];
    if (HealthMeterNPieces[meter] == 2) {
        if (HealthMeterFG[meter][0] == 0) {
            return;
        }
        if (HealthMeterFG[meter][1] == 0) {
            return;
        }
        pct = (f32)(2.0 * (HealthMeterValue[meter] / HealthMeterMaxValue[meter]));
        cv = -1.0f;
        if (pct < 1.0) {
            cv = (f32)(pct * (f32)(256 - flags) + (f32)flags);
        }
        a1 = (int)cv;
        cv = 0.0f;
        if (0.0 < (f32)(pct - 1.0)) {
            cv = (f32)((f32)(pct - 1.0) * (f32)(256 - HealthMeter668[meter]));
        }
        a2 = (int)cv;
        mbBlitProject(HealthMeterFG[meter][0], a1, 0);
        if (a1 < 0) {
            mbBlitSetupVerts(HealthMeterFG[meter][0], -1.0f, 1.0f, -1.0f, -1.0f);
        } else {
            mbBlitSetupVerts(HealthMeterFG[meter][0], -1.0f,
                             (f32)(0.00390625 * (f32)a1), -1.0f, -1.0f);
        }
        mbBlitProject(HealthMeterFG[meter][1], a2, 0);
        mbBlitSetupVerts(HealthMeterFG[meter][1], -1.0f,
                         (f32)(0.00390625 * (f32)a2), -1.0f, -1.0f);
    } else {
        if (HealthMeterFG[meter][0] == 0) {
            return;
        }
        cv = 0.0f;
        if (0.0 < HealthMeterValue[meter] / HealthMeterMaxValue[meter]) {
            cv = (f32)((HealthMeterValue[meter] / HealthMeterMaxValue[meter]) *
                       (f32)(256 - (flags + HealthMeter668[meter])));
        }
        mbBlitProject(HealthMeterFG[meter][0], (int)cv, 0);
        mbBlitSetupVerts(HealthMeterFG[meter][0], -1.0f,
                         (f32)(0.00390625 * (f32)(int)cv), -1.0f, -1.0f);
    }
    for (i = 0; i < HealthMeterNPieces[meter]; i++) {
        void* blit = HealthMeterBG[meter][i];
        if (blit != 0) {
            if (*(s16*)((char*)gBossObj + offsetof(BossCritterView, pausecnt)) < 1) {
                MBBlitSetColor(blit, 0xffffffff);
            } else {
                MBBlitSetColor(blit, 0xff8080ff);
            }
        }
    }
}

int HealthMeterStart(f32 v, char* name, int n, int p, int x, int y, int flag) {
    int num;
    int i;
    char buf[40];

    num = HealthMeterNum;
    if (num >= 3) {
        ErrorPrintf(sBossStr.errFmt, HealthMeterNum);
        return -1;
    }
    HealthMeterNum = num + 1;
    if (flag != 0) {
        HealthMeterX = HealthMeterNextX;
        HealthMeterNextX = HealthMeterNextX + p;
        for (i = 0; i < n; i++) {
            sprintf(buf, sBossStr.bgFmt, name,
                    (*name == 0) ? &lbl_80345B64 : &lbl_80345B60, i + 1);
            HealthMeterBG[num][i] = MBNewBlit(buf, HealthMeterX + i * 256, lbl_80343B70);
            MBBlitSetAlpha(HealthMeterBG[num][i], 112);
        }
    }
    for (i = 0; i < n; i++) {
        sprintf(buf, sBossStr.fgFmt, name,
                (*name == 0) ? &lbl_80345B64 : &lbl_80345B60, i + 1);
        HealthMeterFG[num][i] = MBNewBlit(buf, HealthMeterX + i * 256, lbl_80343B70);
        MBBlitSetAlpha(HealthMeterFG[num][i], 112);
    }
    HealthMeterNPieces[num] = n;
    HealthMeterMaxValue[num] = v;
    HealthMeterValue[num] = v;
    HealthMeter65C[num] = x;
    HealthMeter668[num] = y;
    return num;
}

void BossDeath(void) {
    int i;
    int off;
    int col;
    int coff;
    void* obj;
    int e1;
    int e2;
    f32 pos[3];
    u8 unused[40];

    StartGoodWizard();
    gBossDead = 1;
    for (i = 0; i < 4; i++) {
        int state = gPlayers[i].state;
        if (state == 1 || state == 8) {
            PlayerGiveRune(i, GetWorldOrder(sMusicTrackHi));
        }
    }
    obj = gBossObj;
    pos[0] = *(f32*)((char*)obj + offsetof(BossCritterView, prevMovePathPos[0]));
    pos[1] = *(f32*)((char*)obj + offsetof(BossCritterView, prevMovePathPos[1]));
    pos[2] = *(f32*)((char*)obj + offsetof(BossCritterView, prevMovePathPos[2]));
    pos[0] = *(f32*)((char*)*(void**)((char*)obj + offsetof(BossCritterView, hdr)) + 0xd0) + pos[0];
    pos[1] = *(f32*)((char*)*(void**)((char*)obj + offsetof(BossCritterView, hdr)) + 0xd4) + pos[1];
    pos[2] = *(f32*)((char*)*(void**)((char*)obj + offsetof(BossCritterView, hdr)) + 0xd8) + pos[2];
    if (gBossType < 42) {
        e1 = InitCustomEffect(sItemFile1Buf, &str_BOSSKEY, 0, 0);
        e2 = InitCustomEffect(sItemFile1Buf, str_BOSSKEY2, 0, 0);
        gBossKeyBlit = StartFXSub(e1, pos, 0x80000040, 0x880, 0.0f);
        SfxSetMorph(30.0f, gBossKeyBlit, e2, 0);
        fn_8009F340(pos);
    }
    for (i = 0, off = 0; i < 3; i++, off += 8) {
        for (col = 0, coff = 0; col < 2; col++, coff += 4) {
            if (HealthMeterBG[i][col] != 0) {
                mbBlitInit3414(HealthMeterBG[i][col], 1);
            }
            if (HealthMeterFG[i][col] != 0) {
                mbBlitInit3414(HealthMeterFG[i][col], 1);
            }
        }
    }
}

/* Boss type 44: spawn the two escort adds beside the boss. */
void BossGenerateEnemy(struct BossObjView* o) {
    f32 pos[3];
    f32 vec[3];
    s32 i;
    u8* e;
    u8 unused[4];

    switch (gBossType) {
    case 44: {
        f64 scale = lbl_80345B78;
        f32 speed = lbl_80345AD0;

        for (i = 0; i < 2; i++) {
            YawVec3(o->dir, vec, lbl_80343B74[i]);
            pos[0] = scale * vec[0] + o->pos[0];
            pos[1] = scale * vec[1] + o->pos[1];
            pos[2] = scale * vec[2] + o->pos[2];
            e = (u8*)gEnemies +
                generate_enemy(pos, 27, 3, vec, -1, 0, 1, lbl_80345B10) * 916;
            {
                f32 vz = vec[2];
                *(f32*)(e + offsetof(EnemyTailView, ang)) = atan2(vec[0], vz);
            }
            *(f32*)(e + offsetof(EnemyTailView, angbak)) = *(f32*)(e + offsetof(EnemyTailView, ang));
            *(f32*)(e + offsetof(EnemyTailView, pyr[0])) = speed;
            *(f32*)(e + offsetof(EnemyTailView, pyr[1])) = *(f32*)(e + offsetof(EnemyTailView, ang));
            *(f32*)(e + offsetof(EnemyTailView, pyr[2])) = speed;
            *(f32*)(e + offsetof(EnemyTailView, birth_pos[0])) = o->pos[0];
            *(f32*)(e + offsetof(EnemyTailView, birth_pos[1])) = o->pos[1];
            *(f32*)(e + offsetof(EnemyTailView, birth_pos[2])) = o->pos[2];
        }
        break;
    }
    }
}

void AddBoss(void* obj) {
    gBossObj = 0;
    gBossPos[0] = *(f32*)((int)obj + offsetof(struct BossObjView, pos[0]));
    gBossPos[1] = *(f32*)((int)obj + offsetof(struct BossObjView, pos[1]));
    gBossPos[2] = *(f32*)((int)obj + offsetof(struct BossObjView, pos[2]));
    if (CritterTypeLoaded(4, 0)) {
        gBossObj = CritterNewInst(4, 0, obj);
    }
}

/* ================================================================== */
/* fully reconstructed boss-state setters                              */
/* ================================================================== */
void HealthMeterInit(void) {
    HealthMeterNum = 0;
    HealthMeterX = 0;
    HealthMeterNextX = 0;
}

void BossActivate(void* obj, int flag) {
    gBossActive = 1;
    if (lbl_803444E0 == 0) {
        lbl_803444E0 = 1;
        add_target((int)obj + offsetof(BossCritterView, mtx));
    }
    if (flag != 0) {
        sMusicSubIndex = 1;
        sMusicSubState = 1;
    }
}

void BossDying(void) {
    gBossDying = 1;
}

void BossInit(void) {
    int i, j;

    gBossActive = 0;
    gBossObj = 0;
    gBossDead = 0;
    gBossDying = 0;
    gBossKeyBlit = -1;
    gNumSpewItems = 0;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 2; j++) {
            HealthMeterBG[i][j] = 0;
            HealthMeterFG[i][j] = 0;
        }
    }
}
