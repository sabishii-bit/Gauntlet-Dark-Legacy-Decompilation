#include "types.h"

/* Boss subsystem (GCN BOSS.OBJ region, 0x8001A960-0x8001BC88). Names are the
 * real ones from the Xbox build's BOSS.OBJ (shell3D.pdb). The GameCube MWCC
 * build emits this TU's functions in the reverse of the Xbox link order, so
 * BossSpewCoins (last in the PDB) is first here and BossInit (first in the PDB)
 * is last.
 *
 * Fully reconstructed (match target): the pure-integer boss-state setters
 * BossInit / BossDying / HealthMeterInit / BossActivate.  The large
 * FP-heavy routines (BossSpewCoins, ProcessSpewItems, StartSpewItem,
 * HealthMeterUpdate, HealthMeterStart, BossDeath, BossGenerateEnemy, AddBoss)
 * carry best-effort bodies that compile but are not byte-matched yet; their
 * .rodata string pool / .sdata2 FP pool are therefore left in the auto splits
 * until they are reconstructed. */

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
/* boss-owned bss (addresses in comments).  NOTE: the exact intra-.bss  */
/* ordering the original TU used is not yet reproduced (MWCC/-common      */
/* anchors gSpewItems at offset 0 here), which is the sole residual      */
/* keeping AddBoss from a byte match -- see AddBoss below.               */
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
extern int gBossActivateFlag;   /* 0x803444E0 */
extern int g8EC;                /* 0x803448EC */
extern int g8E8;                /* 0x803448E8 */

extern void fn_8002C53C(int obj);
extern int  fn_8003F81C(int a, int b);
extern void* fn_8003E048(int a, int b, void* obj);
extern void* fn_800BE8F4(void* a, void* b);
extern void* fn_80063928();
extern void fn_800BDE08(void);
extern f64  fn_800BCCE8(f64 a);

/* ------------------------------------------------------------------ */
/* forward decls (address order)                                      */
/* ------------------------------------------------------------------ */
void BossSpewCoins(void);
void ProcessSpewItems(void);
int  StartSpewItem(f32 v, void* pos, void* vel, int type, int amount);
void HealthMeterUpdate(f32 v, int meter);
int  HealthMeterStart(f32 v, char* name, int n, int p, int x, int y, int flag);
void HealthMeterInit(void);
void BossActivate(void* obj, int flag);
void BossDeath(void);
void BossDying(void);
void BossGenerateEnemy(void);
void AddBoss(void* obj);
void BossInit(void);

/* ================================================================== */
/* best-effort (not byte-matched) bodies for the FP-heavy routines     */
/* ================================================================== */
void BossSpewCoins(void) {
}

void ProcessSpewItems(void) {
    int i;
    for (i = 0; i < gNumSpewItems; i++) {
        if (gSpewItems[i].obj == 0) {
            gSpewItems[i].obj = 0;
        }
    }
}

int StartSpewItem(f32 v, void* pos, void* vel, int type, int amount) {
    (void)v; (void)pos; (void)vel; (void)type; (void)amount;
    if (gNumSpewItems >= 32) {
        return -1;
    }
    return gNumSpewItems;
}

void HealthMeterUpdate(f32 v, int meter) {
    (void)v; (void)meter;
}

int HealthMeterStart(f32 v, char* name, int n, int p, int x, int y, int flag) {
    (void)v; (void)name; (void)p; (void)x; (void)y; (void)flag;
    if (HealthMeterNum >= 3) {
        return -1;
    }
    return HealthMeterNum;
}

void BossDeath(void) {
    gBossActive = 1;
}

void BossGenerateEnemy(void) {
}

void AddBoss(void* obj) {
    gBossObj = 0;
    gBossPos[0] = *(f32*)((int)obj + 0x30);
    gBossPos[1] = *(f32*)((int)obj + 0x34);
    gBossPos[2] = *(f32*)((int)obj + 0x38);
    if (fn_8003F81C(4, 0)) {
        gBossObj = fn_8003E048(4, 0, obj);
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
    if (gBossActivateFlag == 0) {
        gBossActivateFlag = 1;
        fn_8002C53C((int)obj + 0xc);
    }
    if (flag != 0) {
        g8EC = 1;
        g8E8 = 1;
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
