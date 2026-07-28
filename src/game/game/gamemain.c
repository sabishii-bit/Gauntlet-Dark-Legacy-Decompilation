#include "types.h"

/*
 * game/game/gamemain.c -- the top-level game-flow TU (a slice of it).
 *
 * This is the GameCube build's master "game" translation unit: the main
 * game-loop state machine (game_main), the between-level flow (load a
 * tower / select, transition screens), the end-of-level FINAL STATS
 * display, the on-screen thermometer/wave-timer HUD gadgets, per-world
 * enemy-type setup and boss / good-wizard orchestration.
 *
 * On Xbox this code was split across several .OBJ files (GAMEMAIN.OBJ,
 * gauntworld.obj, GAMEDEFS.OBJ); the GameCube/Midway build merged them
 * into one very large TU whose private constant pool is contiguous in
 * .data9 (0x80346810..0x80346CB4) and whose switch jump tables are
 * contiguous in .data (0x8011C16C..0x8011C7xx).  That single TU starts
 * before this file's window (>=0x8004C3E4) and continues past it
 * (ResolveWorldData @0x80058078 is a 0x1C3C-byte function in the same
 * unit; see game/world/gauntworld.c), so THIS file is only the
 * [0x80050054, 0x80058078) slice of the real TU.
 *
 * Frontier game code, no reference source: wired NonMatching.  The bytes
 * are supplied from the DOL (asm) -- this source documents the function
 * identities recovered from the Xbox PDB (GAMEMAIN.OBJ / gauntworld.obj
 * rosters), on-target strings and the call graph.
 */

/* ------------------------------------------------------------------ */
/* Recovered function identities (HIGH confidence, see symbols.txt).   */
/*                                                                      */
/*   0x80054230  game_main          -- top game-loop state machine;    */
/*                                      jumptable @0x8011C398; dispatches*/
/*                                      init/do_mapscreen, init/do_     */
/*                                      gamemovie, do_stats_display,    */
/*                                      pbDiagDrawMenu, ...             */
/*   0x800522E8  do_stats_display   -- "FINAL STATS" end-of-level tally*/
/*                                      (GENERATORS/TREASURES/... rows); */
/*                                      jumptable @0x8011C37C.           */
/*   0x80053B88  LoadTowerAndSelect -- waits for the async tower load   */
/*                                      (FatalError "LoadTowerAndSelect */
/*                                      Timeout" on stall), loads the   */
/*                                      "shopatt9" font, then loads the */
/*                                      level via init_next_level.      */
/*   0x8005638C  init_next_level    -- builds the "levels/level%s" path */
/*                                      and loads a level (calls        */
/*                                      GetEnemyTypes).                  */
/*   0x80055898  init_thermometer   -- creates the two HUD thermometer  */
/*                                      display objects and binds the   */
/*                                      "THERMBASE"/"THERMCOL" models.  */
/*   0x8005773C  GetEnemyTypes      -- fills the per-level 6-slot enemy */
/*                                      type table @0x80257680, printing*/
/*                                      "Enemy type %d has bad subtype  */
/*                                      %d" on a bad descriptor.        */
/*                                                                      */
/* Additional identities recovered this pass (behaviour + Xbox PDB      */
/* GAMEMAIN.OBJ / gauntworld.obj rosters + on-target strings + call     */
/* graph; cross-file refs retrofitted in main.c / sounds.c / attract.c  */
/* / auxscreen.c):                                                       */
/*                                                                      */
/*   0x80053420  game_init_data     -- second-stage game/audio init;    */
/*                                      main() calls game_init_once then */
/*                                      this; prints "Initializing       */
/*                                      Audio.../Loading Audio.../        */
/*                                      Loading Game." around AudioInit. */
/*   0x80054D18  next_world         -- advance to the next world; picks  */
/*                                      the next enabled world entry and  */
/*                                      re-resolves (NextWorldLevel +     */
/*                                      ResolveWorldData).                */
/*   0x8005674C  world_update       -- per-frame world update dispatched  */
/*                                      from game_main: boss state,       */
/*                                      DoGoodWizard, ProcessSpewItems,   */
/*                                      world animation / effects.        */
/*   0x800575CC  PrintWorldMemSizes -- debug dump of per-category memory  */
/*                                      (OTHER/WORLD/WORLDNODES/ITEMS/    */
/*                                      WEAPONS/ENEMIES/TOTAL) via         */
/*                                      mlmMemUsed.                        */
/*   0x80057978  GetEnemySubtype    -- switch(type) -> subtype code;      */
/*                                      jumptable @0x8011C794; called by   */
/*                                      GetEnemyTypes.                     */
/*   0x800579E0  InLevel            -- compares a 4-char level tag        */
/*                                      against the current level id;      */
/*                                      returns bool (utility, called      */
/*                                      widely).                           */
/*   0x80057A6C  LevelLetter        -- returns the world/level display     */
/*                                      letter ('T'->'G' remap); sounds.c  */
/*                                      builds "MAP_%c"/"S_MAP_%c" banks.  */
/*   0x80057C14  NextAttractWave    -- advance the attract-mode wave/      */
/*                                      world (called from attract.c).     */
/*   0x80057D94  PrevWorldLevel     -- previous level matching a wave      */
/*                                      mask, wrapping to the prev world.  */
/*   0x80057E6C  NextWorldLevel     -- next level matching a wave mask,    */
/*                                      wrapping to the next world.        */
/*                                                                      */
/* Parked giants (documented, not yet decompiled):                      */
/*   0x80054230  game_main          0xAAC  top state machine.            */
/*   0x800522E8  do_stats_display   0x10FC FINAL STATS (tally/disp        */
/*                                         helpers inlined here).          */
/*   0x8005351C  fn_8005351C        0x4F4  world/level entry orchestrator */
/*                                         (only caller: game_main).       */
/*   0x80057024  fn_80057024        0x5A8  world initializer (enemies/    */
/*                                         effects/critters; attract +     */
/*                                         level start).                   */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/* Cross-TU externs (types chosen to reproduce the emitted access form) */
/* ------------------------------------------------------------------ */

/* Active level / world-data records (SDA-relative pointers). */
extern u8*  gCurLevel;             /* 0x8034483C */
extern u8*  gWorldData;            /* 0x80344838 */

/* 44-byte per-realm world-data descriptor table (0x8011... via ADDR16). */
typedef struct WorldDataType {
    s32 type;        /* 0x00 realm type id            */
    s32 _a[4];       /* 0x04 (byte 15 = display letter)*/
    s32 f20;         /* 0x14 associated world value    */
    s32 _b[5];       /* 0x18                          */
} WorldDataType;                   /* size 0x2C (44) */
extern WorldDataType sWorldDataTypes[];
extern s32  sCurWorldIndex;        /* 0x80344844 */

/* Flat views of the big module globals (avoids header coupling). */
extern s32  gEnemies[];            /* 0x80251C18 Enemy[25], stride 0x394 */
extern s32  gWorldInfo[];          /* 0x8028CA8C world info block        */
extern s32  lbl_80250E00[];        /* enemy/texmod pool base             */
extern s32  lbl_802511FC[];        /* per-index sign-flip table          */
typedef struct Row36 {
    s32 f0;      /* 0x00 key   */
    s32 f4;      /* 0x04       */
    s32 _a[3];   /* 0x08       */
    s32 f14;     /* 0x14       */
    s32 _b[3];   /* 0x18       */
} Row36;                           /* size 0x24 (36) */
extern Row36 lbl_8011AF48[];       /* 44-entry, stride 36 lookup table   */
extern s32  lbl_80257640[];        /* 4-entry threshold table            */
extern s32  gGameOptions[];        /* prefs/config block                 */
extern s32  lbl_802577CC[];        /* 8 keys                             */
extern s8*  lbl_8025776C[];        /* 8 parallel object pointers         */

/* SDA-relative scalars (all in .sdata/.sbss). */
extern s32   lbl_80343C0C;
extern s32   lbl_80344A2C;
extern s32   lbl_8034476C;
extern s32   lbl_80344768;
extern s32   gNumPlayers;
extern s32   lbl_80344760;
extern s32   lbl_80343C10;
extern s32   lbl_80343DD4;
extern s32   lbl_80343B38;
extern s32   lbl_803448AC;
extern s32   lbl_803448A8;
extern s32   lbl_8034471C;
extern s32   lbl_80344738;
extern void* lbl_803447B0;
extern s32   gBossType;            /* 0x8034439C */
extern s32   gGameMode;
extern s32   lbl_80344740;
extern s32   lbl_80344748;
extern s32   lbl_80344750;
extern s32   lbl_8034474C;
extern s32   lbl_8034473C;
extern s32   gSceneRoot;
extern s32   gNumEnemies;          /* 0x80344744 */
extern f32   lbl_80346820;
extern s32   gIdentityMatrix[];       /* node template (ADDR16) */

static char sBossGenName[] = "BOSSGEN";   /* 0x80346AA4 (.sdata) */

extern s32   stricmp(const char* a, const char* b);
extern s32   toupper(s32 c);
extern void  AtreeInitLists(s32 a, s32 b);
extern void* MBNewNode(s32 parent, void* tmpl, s32 arg2);

/* game_init_data externs. */
extern char  lbl_80112538[];       /* audio/init message string table */
extern s32   lbl_80344800;
extern s32   lbl_803447C0;
extern s32   lbl_803443E8;
extern s32   lbl_80344830;
extern s32   lbl_8034482C;
extern s32   lbl_80344828;
extern s32   lbl_80344758;
extern s32   lbl_80344B84;
extern s32   alpha;
extern s32   lbl_80344784;
extern s32   lbl_80344DA4;
extern s32   lbl_80344DA0;
extern void  InitPlayerControls(void);
extern void  ControlsUpdate(void);
extern void  AnimInit(void);
extern void  fn_80010DF4(s32 arg0);
extern void  AudioRegisterMenu(void);
extern void  AudioResetInput(void);
extern void  InitLighting(s32 arg0);
extern void  reset_sel_menu(void);
extern void  reset_attract_mode(void);
extern void  bulletproof_printf(const char* fmt, ...);
extern void  AudioInit(void);
extern void  fn_80067B0C(s32 arg0);
extern s32   AudioSysUpdate(s32 arg0);
extern void  ResolveWorldData(s32 worldlevel);

/* World-load / blit / fx externs. */
extern s32   sWorldDataConst;      /* 0x80344848 */
extern s32   sMusicTrackHi;        /* 0x803448D8 */
extern s32   lbl_803447A4;
extern s32   lbl_803447E4;
extern s32   lbl_803447EC;
extern s32   lbl_803447F0;
extern s32   lbl_803447F4;
extern f32   lbl_80346AF4;
extern f32   lbl_80346AF8;
extern f32   lbl_80346BE0;
extern f32   lbl_80346BE4;
extern f32   lbl_80346BE8;
extern f32   lbl_80346BEC;
extern f32   lbl_80346BF0;
extern f32   lbl_80346BF4;
extern char  lbl_801125D0[];
extern s32   BytesFree(void);
extern void  fn_80057F44(s32 a, s32 b);
extern void  NewWorld(void);
extern void  init_players(void);
extern void  ResetItems(void);
extern void  ClearControls(void);
extern void  BossInit(void);
extern void  GameCameraInit(void);
extern void  BossCameraInit(void);
extern void* MBOX_FindTexture(const char* name, s32 arg1);
extern void* MBCreateBlit(s32 a, void* tex, s32 c, s32 d, s32 e, s32 f);
extern void  mbBlitProject(void* blit, s32 a, s32 b);
extern void  mbBlitCvtCoord(void* blit, f32 c);
extern void  MBBlitSetAlpha(void* blit, s32 a);
extern s32   fn_80093BC0(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g, f32 h);
extern void  SfxSetDamage(s32 a, s32 b, s32 c, f32 d, f32 e, f32 f);
extern void  ScaleFX(s32 a, f32 b, f32 c, f32 d);
extern void  fn_8009C8A0(s32 a);

typedef struct EffectInfoEntry {
    void* f0;
    s32 f4;
    s32 f8;
} EffectInfoEntry;                 /* size 12 */
extern EffectInfoEntry EffectInfo[];

/* PrintWorldMemSizes / fn_80050FB0 externs. */
extern char  lbl_80112788[];       /* debug format-string table */
extern s32   lbl_80257680[];       /* per-level enemy type table */
typedef struct WorldMemTable {
    u8  _0[140];
    s32 sizes[8];     /* 0x8C */
    u8  _1[160];
    s32 typeids[8];   /* 0x14C */
} WorldMemTable;
extern s32   lbl_8011B578[];       /* category name-string table */
extern s32   lbl_802577AC[];
extern char  lbl_801124EC[];
extern s32   lbl_80344850;
extern s32   lbl_80344854;
extern s32   lbl_80344858;
extern s32   lbl_8034485C;
extern s32   lbl_80344D80;
extern s32   lbl_80344D84;
extern s32   lbl_80344D88;
extern s32   dbgTextEnable;
extern s32   mlmMemUsed;
extern void  ErrorPrintf(const char* fmt, ...);

/* fn_800521E8 / fn_8005412C externs. */
extern s32   gGameBusy;
extern s32   lbl_80344774;
extern s32   gFrameTicks;
extern s32   lbl_80344778;
extern s32   lbl_803441F8;
extern f32   lbl_80346AB8;
extern void  fn_8009FB00(void);
extern void  SetDrawStringScale(f32 s);
extern void* DrawStringText(s32 a, s32 b, s32 c, s32 d, s32 e, ...);
extern void  RestoreDrawStringScale(void);
extern void  init_attract_mode(s32 mode);
extern u8    gPlayers[];       /* world descriptor array, stride 13148 */
extern f32   lbl_803447D4;
extern f32   lbl_803447D8;
extern s32   lbl_803447DC;
extern s32   lbl_803447E0;
extern f32   lbl_80346AF0;
extern f64   lbl_80346B00;

/* Called helpers (signatures picked to reproduce the argument setup). */
extern void  fn_80030BA0(s32 idx);
extern void  mbBlitInit3414(void* blit, s32 hide);
extern void  reset_players(void);
extern void  LoadPdataFile(void);
extern void  setup_player_models(void);
extern void  UnloadWeaponsPowerups(void);
extern void  LoadPowerups(s32 arg0);
extern void  LoadWeapons(void);
extern void  LoadWorldData(void);
extern void  MBOX_LockModels(void);
extern void  fn_80010E84(s32 arg0);
extern void  AudioStopSelect(void);
extern void  init_prefs(void);
extern void  InitTexMods(void* tex, s32 arg1);
extern void  MBTreeSetFlags(void* node, s32 flags, s32 arg2);

/* Forward decls for same-TU functions referenced before definition. */
void game_main(void);
void do_stats_display(void);
void LoadTowerAndSelect(s32 arg0);
s32  init_next_level(s32 arg0);
void init_thermometer(void);
void GetEnemyTypes(void);
void game_init_data(void);
s32  next_world(void);
void world_update(void);
void PrintWorldMemSizes(void);
s32  GetEnemySubtype(s32 type);
s32  InLevel(const char* tag);
s32  LevelLetter(s32 arg0);
s32  NextAttractWave(s32 level);
s32  PrevWorldLevel(s32 waveMask);
s32  NextWorldLevel(s32 waveMask);
s32  fn_80055E60(s32 arg0);
s32  fn_80055F68(s32 arg0, s32 arg1);
s32  fn_80056698(s32 arg0, s32 arg1);
void fn_800510A4(void);

/* ================================================================== */
/* Function bodies (address order).                                    */
/* ================================================================== */

/* 0x800508A0 -- re-init texture-mod state for every active pool entry. */
void fn_800508A0(void)
{
    s32 i;
    s32 idx;
    u8 unused[8];

    for (i = 0; i < lbl_8034471C; i++) {
        idx = lbl_80250E00[8 + i];
        if ((void*)lbl_80250E00[345 + idx] != 0) {
            InitTexMods((void*)lbl_80250E00[345 + idx], lbl_80250E00[300 + idx]);
        }
    }
}

/* 0x80050910 -- negate one entry of the sign-flip table, then notify. */
void fn_80050910(s32 arg0)
{
    lbl_802511FC[arg0] = -lbl_802511FC[arg0];
    fn_80030BA0(arg0);
}

/* 0x80051164 -- clear the parallel per-slot tables of the pool. */
void fn_80051164(void)
{
    s32* p = lbl_80250E00;
    s32 i;

    for (i = 0; i < 45; i++) {
        p[345 + i] = 0;
        p[300 + i] = -1;
        p[255 + i] = 0;
    }
    for (i = 0; i < 8; i++) {
        p[8 + i] = -1;
    }
    lbl_8034471C = 0;
    lbl_80344738 = -1;
}

/* 0x80051F64 -- table lookup by id, return &entry.f20 (or NULL). */
void* fn_80051F64(s32 id)
{
    s32 i;

    for (i = 0; i < 44; i++) {
        if (lbl_8011AF48[i].f0 == id) {
            return &lbl_8011AF48[i].f14;
        }
    }
    return 0;
}

/* 0x80051FA0 -- table lookup by id, return &entry.f4 (or NULL). */
void* fn_80051FA0(s32 id)
{
    s32 i;

    for (i = 0; i < 44; i++) {
        if (lbl_8011AF48[i].f0 == id) {
            return &lbl_8011AF48[i].f4;
        }
    }
    return 0;
}

/* 0x8005207C -- store clamped indices + resolve one cached selection. */
#pragma dont_inline on
void fn_8005207C(s32 arg0, s32 arg1, s32 arg2)
{
    lbl_8034476C = arg0;
    if (arg1 < 0) {
        arg1 = 0;
    } else if (arg1 > 4) {
        arg1 = 4;
    }
    lbl_80344768 = arg1;
    if (gGameOptions[3] == 0) {
        gNumPlayers = arg0;
    } else {
        gNumPlayers = gGameOptions[3];
    }
    lbl_80344760 = arg2;
}
#pragma dont_inline off

/* 0x800520C8 -- empty. */
void fn_800520C8(void)
{
}

/* 0x800520CC -- reset the prefs/config block, then load prefs. */
void fn_800520CC(void)
{
    gGameOptions[0] = 0;
    gGameOptions[1] = 0;
    gGameOptions[2] = 3;
    gGameOptions[6] = 0;
    gGameOptions[7] = 1;
    gGameOptions[8] = 0;
    gGameOptions[3] = 0;
    gGameOptions[4] = 0;
    gGameOptions[5] = 0;
    gGameOptions[9] = 512;
    gGameOptions[10] = 0;
    gGameOptions[11] = 0;
    init_prefs();
}

/* 0x800533E4 -- reload player data / models / weapons / world. */
void fn_800533E4(void)
{
    reset_players();
    LoadPdataFile();
    setup_player_models();
    UnloadWeaponsPowerups();
    LoadPowerups(0);
    LoadWeapons();
    LoadWorldData();
}

/* 0x80053A10 -- clear two per-enemy fields for all 25 enemy records. */
void fn_80053A10(void)
{
    s32* e = gEnemies;
    s32 i;

    for (i = 0; i < 25; i++) {
        e[45] = 0;   /* +0xB4 */
        e[25] = 0;   /* +0x64 */
        e += 229;    /* stride 0x394 */
    }
}

/* 0x80053A38 -- hide the blit at lbl_803447B0 if present. */
void fn_80053A38(void)
{
    if (lbl_803447B0 != 0) {
        mbBlitInit3414(lbl_803447B0, 1);
    }
}

/* 0x80053B20 -- reset the selection/flow globals and stop select audio. */
void fn_80053B20(void)
{
    lbl_80343C10 = -1;
    lbl_80343DD4 = -1;
    lbl_80343B38 = -1;
    AudioStopSelect();
    lbl_803448AC = -1;
    lbl_803448A8 = -1;
}

/* 0x80053B60 -- start something at slot 1. */
void fn_80053B60(void)
{
    fn_80055F68(1, 0);
}

/* 0x80054CDC -- flag if any of the 4 thresholds exceeds 120. */
s32 fn_80054CDC(void)
{
    s32 i;

    for (i = 0; i < 4; i++) {
        if (lbl_80257640[i] > 120) {
            lbl_80344A2C = 1;
        }
    }
    return lbl_80344A2C;
}

/* 0x80054E68 -- set-and-return the previous value of lbl_80343C0C. */
s32 fn_80054E68(s32 arg0)
{
    s32 old = lbl_80343C0C;
    lbl_80343C0C = arg0;
    return old;
}

/* 0x80055CB8 -- find the worldanim bound to a given world object. */
void* fn_80055CB8(void* wobj)
{
    s32 idx = ((s32)wobj - gWorldInfo[1]) / 60;   /* wobjs @0x04, stride 60 */
    u8* wa = (u8*)gWorldInfo[35];                  /* worldanims @0x8C       */
    s32 i;

    for (i = 0; i < gWorldInfo[36]; i++) {         /* nworldanims @0x90      */
        if (idx == *(s16*)(wa + i * 16)) {
            return wa + i * 16;
        }
    }
    return 0;
}

/* 0x80055E04 -- run fn_80055E60, then flag a scene-node subtree. */
void fn_80055E04(void* node, s32 arg1)
{
    s32* n = (s32*)node;

    fn_80055E60(arg1);
    while (n != 0) {
        MBTreeSetFlags((void*)n[10], 2, 0);   /* +0x28 */
        n[4] |= 0x10000000;                   /* +0x10 */
        n = (s32*)n[6];                       /* +0x18 next */
    }
}

/* 0x80057ACC -- lookup a live object by key, else default to gWorldData+4. */
void* fn_80057ACC(s32 key)
{
    s32 i;

    for (i = 0; i < 8; i++) {
        if (lbl_802577CC[i] == key) {
            s8* p = lbl_8025776C[i];
            if (p != 0 && *(p += 16) != 0) {
                return p;
            }
        }
    }
    return gWorldData + 4;
}

/* 0x80057AB4 -- accessor: current-level record + 8. */
void* LevelItemDesc(void)
{
    return gCurLevel + 8;
}

/* 0x80057AC0 -- accessor: world-data record + 4. */
void* WorldItemDesc(void)
{
    return gWorldData + 4;
}

/* 0x80057BC8 -- realm-type descriptor's f20 for a given type id. */
s32 fn_80057BC8(s32 type)
{
    s32 i;

    for (i = 0; i < 14; i++) {
        if (sWorldDataTypes[i].type == type) {
            break;
        }
    }
    return sWorldDataTypes[i].f20;
}

/* 0x8005403C -- lock the model box, then run fn_80010E84. */
void fn_8005403C(s32 arg0)
{
    MBOX_LockModels();
    fn_80010E84(arg0);
}

/* 0x8005636C -- advance a two-field counter unless it is parked at 2. */
void fn_8005636C(s32* s)
{
    if (s[4] == 2) {
        return;
    }
    s[1] += s[2];
}

/* 0x800579E0 -- does the given 4-char tag match the current level id? */
s32 InLevel(const char* tag)
{
    char* lvl = (char*)gCurLevel;

    if (lvl[8] == tag[0] &&
        (lvl[9] == 0 || lvl[9] == tag[1]) &&
        (lvl[10] == 0 || lvl[10] == tag[2]) &&
        (lvl[11] == 0 || lvl[11] == tag[3])) {
        return 1;
    }
    return 0;
}

/* 0x80057A6C -- level display letter ('T' realm remaps to 'G'). */
s32 LevelLetter(s32 arg0)
{
    s32 idx = sCurWorldIndex;
    s32 c;

    if (idx < 0) {
        c = 'A';
    } else {
        c = ((u8*)&sWorldDataTypes[idx])[15];
    }
    if ((s8)c == 'T' && arg0 == 0) {
        c = 'G';
    }
    return c;
}

/* 0x80053420 -- second-stage game/audio init (main() calls this). */
void game_init_data(void)
{
    char* msgs = lbl_80112538;
    u8 unused[8];

    InitPlayerControls();
    ControlsUpdate();
    AnimInit();
    fn_80010DF4(0);
    AudioRegisterMenu();
    AudioResetInput();
    AudioRegisterMenu();
    lbl_80344800 = 0;
    lbl_803447C0 = 0;
    lbl_803443E8 = 0;
    lbl_80344830 = 99999;
    lbl_8034482C = 0;
    lbl_80344828 = 1;
    ControlsUpdate();
    InitLighting(0);
    lbl_80344A2C = 0;
    gGameMode = 0x8002;
    lbl_80344758 = 0;
    lbl_80344B84 = -1;
    reset_sel_menu();
    alpha = 0;
    reset_attract_mode();
    ControlsUpdate();
    bulletproof_printf(msgs + 92);   /* "Initializing Audio..." */
    AudioInit();
    bulletproof_printf(msgs + 116);  /* "Loading Audio..."      */
    while (AudioSysUpdate(1) != 0) {
        fn_80067B0C(-1);
    }
    bulletproof_printf(msgs + 136);  /* "Loading Game."         */
    ControlsUpdate();
    lbl_80344784 = 0;
}

/* 0x80053A68 -- create/refresh the loading-screen blit. */
void fn_80053A68(s32 arg0)
{
    f32 coord;
    s32 sz;

    if (arg0 != 0) {
        coord = lbl_80346AF4;
        sz = 384;
    } else {
        sz = 320;
        coord = lbl_80346AF8;
    }
    if (lbl_803447B0 == 0) {
        void* tex = MBOX_FindTexture(lbl_801125D0, 0);
        lbl_803447B0 = MBCreateBlit(0, tex, 0, 0, 512, sz);
    }
    mbBlitProject(lbl_803447B0, 0, sz);
    mbBlitCvtCoord(lbl_803447B0, coord);
    MBBlitSetAlpha(lbl_803447B0, 0);
    mbBlitInit3414(lbl_803447B0, 0);
}

/* 0x80054070 -- load a world/level, measuring its heap usage. */
s32 fn_80054070(s32 arg0, s32 arg1, s32 arg2)
{
    lbl_803447A4 = BytesFree();
    sMusicTrackHi = -1;
    if (arg0 == sWorldDataConst) {
        fn_80057F44(sWorldDataConst, 0);
    } else if (arg0 >= 0) {
        arg2 = fn_80056698(arg0, arg2);
    }
    lbl_803447E4 = 0;
    lbl_803447A4 = lbl_803447A4 - BytesFree();
    lbl_803447EC = 0;
    lbl_803447F0 = 0;
    lbl_803447F4 = 0;
    NewWorld();
    init_players();
    fn_800510A4();
    ResetItems();
    ClearControls();
    BossInit();
    GameCameraInit();
    BossCameraInit();
    return arg2;
}

/* 0x80055E60 -- spawn the level-transition effect for the current music. */
s32 fn_80055E60(s32 arg0)
{
    f32 f30;
    f32 f31;
    s32 fxid;
    s32 dmg;
    s32 result;

    f30 = lbl_80346BE0;
    switch (sMusicTrackHi) {
    case 9:
    case 11:
        f31 = lbl_80346BE4;
        fxid = 94;
        dmg = 2048;
        break;
    default:
        f31 = lbl_80346BE8;
        fxid = -1;
        f30 = lbl_80346BEC;
        dmg = 0;
        break;
    }
    if (fxid < 0 || EffectInfo[fxid].f0 == 0) {
        fxid = 22;
        dmg = 33;
    }
    result = fn_80093BC0(fxid, arg0, 0, 43, 0, 4, 0, lbl_80346BF0);
    SfxSetDamage(result, dmg, 0, lbl_80346BF4, f31, lbl_80346BF0);
    ScaleFX(result, f30, lbl_80346BE0, f30);
    fn_8009C8A0(arg0);
    return result;
}

/* 0x800521E8 -- animate the loading-timer HUD, arm attract on timeout. */
void fn_800521E8(void)
{
    s32 flag = gGameBusy;
    s32 oldTimer = lbl_80344774;
    s32 newTimer;
    s32 idx;
    void* txt;
    u8 unused[16];

    lbl_80344774 = oldTimer + gFrameTicks;
    newTimer = lbl_80344774;
    idx = (newTimer - 60) >> 3;
    if (oldTimer < 60 && newTimer >= 60) {
        fn_8009FB00();
    }
    if (idx < 0) {
        idx = 0;
    } else if (idx > 9) {
        idx = 9;
    }
    SetDrawStringScale(lbl_80346AB8);
    txt = DrawStringText(-256, 120, 6, 0xFFFFFF, 169, 0);
    RestoreDrawStringScale();
    ((u8*)((void**)txt)[4])[idx] = 0;   /* ((u8*)txt->f16)[idx] = 0 */
    if (flag != 0) {
        return;
    }
    {
        s32 remaining = lbl_80344778 - gFrameTicks;
        lbl_80344778 = remaining;
        if (remaining > 0) {
            return;
        }
    }
    lbl_80343C10 = -1;
    lbl_80343DD4 = -1;
    lbl_80343B38 = -1;
    AudioStopSelect();
    lbl_803448AC = -1;
    lbl_803448A8 = -1;
    lbl_803441F8 = 1;
    init_attract_mode(0x8002);
}

/* 0x8005412C -- categorise the loaded worlds and update the flow globals. */
void fn_8005412C(void)
{
    u8* base = gPlayers;
    s32 count1 = 0;
    s32 count2 = 0;
    s32 count3 = 0;
    s32 i;

    lbl_803447D4 = lbl_803447D8;
    lbl_803447DC = 0;
    lbl_803447D8 = lbl_80346AF0;
    lbl_803447E0 = 0;
    for (i = 0; i < 4; i++) {
        u8* e = base + i * 13148;
        s32 type = *(s32*)(e + 232);
        if (type != 0) {
            count1++;
            if (type != 2 && type != 3) {
                count2++;
            }
        }
        if (type == 1 || type == 5) {
            lbl_803447E0 |= (1 << i);
            count3++;
        }
        if (type == 1) {
            s32 f292 = *(s32*)(e + 292);
            if (f292 & 0x8) {
                lbl_803447DC = 1;
            }
            if (gBossType < 0 && (f292 & 0x200)) {
                lbl_803447D8 = lbl_803447D8 * lbl_80346B00;
            }
        }
        *(s16*)(e + 2406) = 0;
    }
    fn_8005207C(count1, count2, count3);
}

/* 0x80050FB0 -- resolve a type id through the override tables. */
s32 fn_80050FB0(s32 arg0, s32 arg1)
{
    s32 result = arg0;

    if (arg0 == 3) {
        result = lbl_802577AC[1];
    }
    if ((u32)(arg0 - 4) <= 1) {
        if (arg1 >= 4 && lbl_802577AC[4] >= 0) {
            result = lbl_802577AC[4];
        } else if (lbl_802577AC[2] >= 0) {
            result = lbl_802577AC[2];
        } else if (lbl_802577AC[3] >= 0) {
            result = lbl_802577AC[3];
        }
    }
    if (result == -1) {
        char* name = 0;
        s32 i;
        for (i = 0; i < 44; i++) {
            if (lbl_8011AF48[i].f0 == arg0) {
                name = (char*)&lbl_8011AF48[i].f4;
                break;
            }
        }
        ErrorPrintf(lbl_801124EC, name);
    }
    return result;
}

/* 0x800575CC -- debug dump of the per-category memory usage. */
void PrintWorldMemSizes(void)
{
    char* fmt = lbl_80112788;
    WorldMemTable* t = (WorldMemTable*)lbl_80257680;
    s32 sum;
    u8 unused[8];
    s32 i;

    if (lbl_80344850 == 0) {
        return;
    }
    sum = 0;
    if (dbgTextEnable == 0) {
        lbl_80344850 = 0;
    }
    bulletproof_printf(fmt + 368, lbl_80344854);
    bulletproof_printf(fmt + 388, lbl_80344D88);
    bulletproof_printf(fmt + 408, lbl_803447A4);
    bulletproof_printf(fmt + 428, lbl_80344D80);
    bulletproof_printf(fmt + 448, lbl_80344D84);
    bulletproof_printf(fmt + 468, lbl_8034485C);
    bulletproof_printf(fmt + 488, lbl_80344858);
    for (i = 0; i < 8; i++) {
        sum += t->sizes[i];
    }
    bulletproof_printf(fmt + 508, sum);
    for (i = 0; i < 8; i++) {
        if (t->typeids[i] >= 0 && t->sizes[i] >= 0) {
            bulletproof_printf(fmt + 528, lbl_8011B578[t->typeids[i]]);
        }
    }
    bulletproof_printf(fmt + 540);
    bulletproof_printf(fmt + 568, mlmMemUsed);
    lbl_80344850 = 0;
}

/* 0x80056698 -- resolve a world/level then tally its memory footprint. */
s32 fn_80056698(s32 arg0, s32 arg1)
{
    s32* p;
    s32* q;
    s32 total = 0;

    ResolveWorldData(arg0);
    if (arg1 < 0) {
        init_next_level(arg0);
        while (fn_80055F68(0, 0) == 0) {
            fn_80067B0C(-1);
        }
    }
    p = (s32*)lbl_80344DA4;
    if (p != 0) {
        total = p[0] + p[20] * 3;   /* +0x50 */
        total += p[22];             /* +0x58 */
    }
    q = (s32*)lbl_80344DA0;
    if (q != 0) {
        total += q[0];
        total += q[20] * 2;
        total += q[22];
    }
    return total;
}

/* 0x800510A4 -- (re)initialise the enemy pool and per-slot link tables. */
void fn_800510A4(void)
{
    s32* pool = lbl_80250E00;
    s32* e = (s32*)((u8*)pool + 3608);   /* = gEnemies */
    f32 fv = lbl_80346820;
    s32 i;

    for (i = 0; i < 25; i++) {
        e[45] = 0;                       /* +0xB4 */
        e[25] = 0;                       /* +0x64 */
        e[26] = 2;                       /* +0x68 */
        *(f32*)(e + 135) = fv;           /* +0x21C */
        e[27] = 0;                       /* +0x6C */
        e[119] = 0;                      /* +0x1DC */
        e += 229;
    }
    lbl_8034473C = (s32)MBNewNode(gSceneRoot, gIdentityMatrix, 1);
    gNumEnemies = *(s16*)((u8*)gCurLevel + 142);
    lbl_80344740 = 0;
    lbl_80344748 = -1;
    lbl_80344750 = -1;
    lbl_8034474C = 0;
    for (i = 0; i < 256; i++) {
        pool[390 + i] = 0;   /* +0x618 */
        pool[646 + i] = 0;   /* +0xA18 */
    }
}

/* 0x80051FDC -- resolve a level/enemy name to its type id (-1 on miss). */
s32 fn_80051FDC(const char* name)
{
    s32 i;

    if (stricmp(name, sBossGenName) == 0) {
        s32 t = lbl_802577CC[0];
        if (t == gBossType) {
            t = -1;
        }
        return t;
    }
    for (i = 0; i < 44; i++) {
        if (stricmp(name, (char*)&lbl_8011AF48[i].f4) == 0) {
            return lbl_8011AF48[i].f0;
        }
    }
    return -1;
}

/* 0x80053C70 -- pick Atree list sizes from the current game-mode id. */
void fn_80053C70(void)
{
    switch (gGameMode) {
    case 0x8002:
    case 0x400F:
    case 0x8004:
        AtreeInitLists(64, 64);
        break;
    case 0x4012:
    case 0x8009:
        AtreeInitLists(3584, 3072);
        break;
    default:
        AtreeInitLists(-1, -1);
        break;
    }
}

/* 0x80057B30 -- parse a "<letter><digit>" level tag to (realm<<8)|index. */
s32 fn_80057B30(const char* s)
{
    s32 realm = -1;
    s8 letter = toupper(s[0]);
    s32 i;

    for (i = 0; i < 14; i++) {
        if (letter == ((s8*)&sWorldDataTypes[i])[15]) {
            realm = sWorldDataTypes[i].type;
            break;
        }
    }
    if (realm < 0) {
        return -1;
    }
    return (realm << 8) | (u8)(s[1] - '1');
}
