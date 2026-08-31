#include "types.h"
#include "game/critter.h"
#include "game/enemy.h"
#include "game/item.h"
#include "game/leveldata.h"
#include "game/mbobject.h"
#include "game/worldinfo.h"
#include "game/player.h"

#define offsetof(type, member) ((u32)&((type*)0)->member)

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
/*                                      level via init_next_level_8005638C.      */
/*   0x8005638C  init_next_level_8005638C    -- builds the "levels/level%s" path */
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

/* Per-level navigation record reached through gWorldData->levels. */
typedef struct WorldLevelNav {
    u32 flags;
    s16 flags2;
    u8 _06[0x106];
} WorldLevelNav;

/* Head of the loaded world-data blob: level cursor plus the level array. */
typedef struct WorldDataNav {
    u8 _00[0x16];
    s16 curLevel;
    s16 numLevels;
    u8 _1A[2];
    WorldLevelNav* levels;
} WorldDataNav;

/* Active level / world-data records (SDA-relative pointers). */
extern level_data* gCurLevel;      /* 0x8034483C */
extern WorldDataNav* gWorldData;   /* 0x80344838 */

/* 44-byte per-realm world-data descriptor table (0x8011... via ADDR16). */
typedef struct WorldDataType {
    s32 type;        /* 0x00 realm type id            */
    u8  _04[11];
    u8  letter;      /* 0x0F display letter           */
    s32 available;   /* 0x10 world data is loaded     */
    s32 f20;         /* 0x14 associated world value    */
    s32 _b[4];       /* 0x18                          */
    s32 attractLevel;/* 0x28 attract-mode level index */
} WorldDataType;                   /* size 0x2C (44) */
extern WorldDataType sWorldDataTypes[];
extern u8 sWorldLevelTable[];
extern s32  sCurWorldIndex;        /* 0x80344844 */

/* Flat views of the big module globals (avoids header coupling). */
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
extern void* lbl_80257630[];       /* two thermometer blits at [1],[2]   */
extern u8    lbl_802575C0[];
extern s32  gGameOptions[];        /* prefs/config block                 */
extern s32  lbl_802577CC[];        /* 8 keys                             */
extern s8*  lbl_8025776C[];        /* 8 parallel object pointers         */

/* SDA-relative scalars (all in .sdata/.sbss). */
extern s32   lbl_80343C0C;
extern u64   gControllerButtons;
extern s32   sFlags;
extern s32   lbl_80344A2C;
extern s32   lbl_8034476C;
extern s32   lbl_80344768;
extern s32   lbl_803441B0;
extern s32   lbl_803441B4;
extern s32   lbl_803441B8;
extern s32   lbl_803443BC;
extern s32   gNumPlayers;
extern s32   lbl_80344760;
extern s32   lbl_80343C10;
extern s32   lbl_80343DD4;
extern s32   lbl_80343B38;
extern s32   lbl_803448AC;
extern s32   lbl_803448A8;
extern s32   lbl_8034471C;
extern s32   lbl_80344738;
extern s32   lbl_80344734;
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
extern f32   lbl_803468B0;
extern f32   lbl_80346A80;
extern u8    sMilestones[];
extern s32   sNumMilestones;
extern f64   __frsqrte(f64 x);
extern f32   gIdentityMatrix[];       /* identity matrix */
extern f32   lbl_80346A7C;
extern u32   RandInt(u32 limit);
extern void* sGoodWizObj;
extern void* gWadAtreeHeaders[];
extern s32   lbl_802512B0[];
extern char* lbl_8011BFF8[];
extern u8    lbl_80126EC0[];
DECL_SECT(".sdata2") extern const char lbl_80346770[];

static char sBossGenName[] = "BOSSGEN";   /* 0x80346AA4 (.sdata) */

extern s32   stricmp(const char* a, const char* b);
extern s32   toupper(s32 c);
extern void  AtreeAlloc(s32 a, s32 b);
extern void  DoTexMods(void* data);
extern s32   DoWorldAnimSub(void* track, void* animdata, void* animBase);
extern void* MBNewNode(s32 parent, void* tmpl, s32 arg2);
extern void  SfxDeleteParented(void* node, s32 arg1, s32 player);
extern void  AtreeDelete(void* atree);
extern void  MBRemoveNode(void* node, s32 recursive);
extern s32   fn_80011BBC(void* model, const char* name, void* atreeOut,
                         const char* work, s32 workSize);
extern void  InitActions(void* atree, void* actionList, void* actionTable);
extern void* MBOX_NewObject(const char* name, f32* matrix, void* parent, u32 flags);
extern void* MBOX_ReallyFindObject(const char* name, s32 type1, s32 type2, s32 exact);
extern void* MBNewObject(void* object, f32* matrix, void* parent, u32 flags);
extern void  MBNodeSetParent(void* node, void* parent);

/* game_init_data externs. */
extern char  lbl_80112538[];       /* audio/init message string table */
extern s32   lbl_80344800;
extern s32   lbl_803447C0;
extern s32   gLanguageId;
extern s32   lbl_80344830;
extern s32   lbl_8034482C;
extern s32   lbl_80344828;
extern s32   lbl_80344758;
extern s32   lbl_80344B84;
extern s32   alpha;
extern s32   lbl_80344784;
extern s32   lbl_80344DA4;
extern s32   lbl_80344DA0;
extern s32   lbl_8034481C;
extern s32   sLastWorldLevel;
extern s32   sFirstWorldId;
extern u32   pbLoad;
extern s32   gDemoMode;
extern s32   opt_force_player;
extern void* lbl_8034479C;
extern s32   options_state;
extern u8    optionsAudioAndPrefs[];
extern s32   optionsAudioAndPrefs30[];
extern s16   lbl_80343C14;
extern f32   lbl_80343C18;
extern f32   lbl_80343C1C;
extern s32   lbl_80343C20;
extern u8*   lbl_80344EE8;
extern void  InitPlayerControls(void);
extern void  ControlsUpdate(void);
extern void  AnimInit(void);
extern void  AtreeInitLists(s32 arg0);
extern void  AudioRegisterMenu(void);
extern void  AudioResetInput(void);
extern void  InitLighting(s32 arg0);
extern void  reset_sel_menu(void);
extern void  reset_attract_mode(void);
extern void  bulletproof_printf(const char* fmt, ...);
extern void  AudioInit(void);
extern void  serve_busy(s32 arg0);
extern s32   AudioSysUpdate(s32 arg0);
extern void  ResolveWorldData(s32 worldlevel);
extern void  FatalError(const char* msg, s32 code);
s32          fn_80053D08(s32 a, s32 b, s32 c);
extern void  SelectLoadStart(void);
extern s32   SelectLoadDone(void);
extern void  FontInitSpecial(void* def, s32 font);
extern void  ShopLoadData(void);
extern void  LoadItems(void);
extern void  EndFireScroll(void);
extern void  DeleteOptionBlits(void);
extern void  SumnerEnd(void);
extern void  Randomize(s32 seed);
extern void  ResetPlayerMissiles(void);
extern void  ClearAllPlyrData(void);
extern void  InitializeClockIRQ(void);
extern void  vibrators_off(void);
extern void  WorldRestoreInitState(void);
extern void  MBOX_ResetUnlockedModels(s32 mode);
extern void  ResetTexmods(void);
extern void  MBCompVertScaleAddUV(s32 a, s32 b, f32 x, f32 y, f32 z,
                                  f32 u, f32 v);
extern void  ResetWorlds(void);
extern void  InitItems(void);
extern void  sndSysInit(void);
extern s32   good_wiz_enabled;
extern s32   good_wiz_state;
extern void* lbl_803447A0;
extern void* lbl_803443E4;
extern void* lbl_80344E48;
extern void* lbl_80344E44;
extern void* lbl_80344E40;
extern void* lbl_80344E3C;
extern void* lbl_80344E38;
extern void* lbl_80344E34;
extern void* lbl_80344E30;
extern void* lbl_80344E2C;

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
extern f32   lbl_80346AFC;
extern s32   lbl_80344810;
extern f32   lbl_80344814;
extern f32   lbl_80344818;
extern f32   lbl_80346B60;
extern f64   lbl_80346B68;
extern f32   lbl_80346B20;
extern f32   lbl_80346B70;
extern f32   lbl_80346B74;
extern f32   lbl_80346B78;
DECL_SECT(".sdata2") extern const char lbl_80346B7C[];
extern f32   lbl_80346B84;
extern f32   lbl_80346B88;
extern f64   lbl_80346AE8;
extern f32   lbl_80346AD4;
extern f32   lbl_80346BE0;
extern f32   lbl_80346BE4;
extern f32   lbl_80346BE8;
extern f32   lbl_80346BEC;
extern f32   lbl_80346BF0;
extern f32   lbl_80346BF4;
extern char  lbl_801125D0[];
extern char  lbl_801125E4[];
extern char  lbl_80112600[];
extern s32   BytesFree(void);
extern s32   fn_80057F44(s32 code, s32 mask);
extern void  NewWorld(s32 arg0);
extern void  init_players(void);
extern void  ResetItems(void);
extern void  ClearControls(void);
extern void  BossInit(void);
extern void  GameCameraInit(void);
extern void  BossCameraInit(void);
extern void* MBOX_FindTexture(const char* name, s32 arg1);
extern s32   MBOX_FindTexture_Err(const char* name, void** out, s32 flag);
extern void* MBCreateBlit(s32 a, void* tex, s32 c, s32 d, s32 e, s32 f);
extern void  mbInitBlitEntry(void* blit, u32 texture, s32 frame);
extern void  mbBlitProject(void* blit, s32 a, s32 b);
extern void  mbBlitCvtCoord(void* blit, f32 c);
extern void  mbBlitSetupVerts(void* blit, f32 a, f32 b, f32 c, f32 d);
extern void  mbBlitCalcY(void* blit, s32 y);
extern s32   Round(f32 value);
extern void  MBBlitSetAlpha(void* blit, s32 a);
extern s32   fn_80093BC0(s32 a, s32 b, s32 c, s32 d, s32 e, s32 f, s32 g, f32 h);
extern void  SfxSetDamage(s32 a, s32 b, s32 c, f32 d, f32 e, f32 f);
extern void  ScaleFX(s32 a, f32 b, f32 c, f32 d);
extern void  AudioWorldExplosion(s32 a);

typedef struct EffectInfoEntry {
    void* f0;
    s32 f4;
    s32 f8;
} EffectInfoEntry;                 /* size 12 */
extern EffectInfoEntry EffectInfo[];

/* PrintWorldMemSizes / GetEnemyType externs. */
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
extern s32   lbl_80343C30;
extern s32   lbl_80344870;
extern u32   lbl_80344874;
extern s32*  lbl_80344878;
extern s32   lbl_8025778C[];
DECL_SECT(".sdata2") extern const char lbl_80346BF8[];
extern s32   lbl_80344D80;
extern s32   lbl_80344D84;
extern s32   lbl_80344D88;
extern s32   dbgTextEnable;
extern s32   mlmMemUsed;
extern void  ErrorPrintf(const char* fmt, ...);
extern void  WorldLoadModelDone(void* world);
extern s32   WorldLoadModelStart(void);
extern s32   StartWorldLoad(s32 arg0);
extern s32   StartLoadWorldAnim(void* world);
extern s32   FinishLoadWorldAnim(void);
extern void  MBOX_BGLoadModelStart(char* name, void* model);
extern s32   MBOX_BGLoadModelDone(void);
extern s32   FileSize(char* wad, const char* name);
extern s32*  StartFileRead(char* wad, const char* name, s32 mode, s32 size,
                           void* dest, void* callback);
extern s32   CritterLoadStartNext(void);
extern s32   CritterLoadDone(s32 maxBytes);
extern void  fn_8001267C(void* header, void* object, s32 arg2);

/* fn_800521E8 / SetPlayerVars externs. */
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
extern Player gPlayers[];      /* gPlayerRecords[4], stride 13148 (0x335C) */
extern f32   lbl_803447D4;
extern f32   lbl_803447D8;
extern s32   lbl_803447DC;
extern s32   lbl_803447E0;
extern f32   lbl_80346AF0;
extern f64   lbl_80346B00;

/* Called helpers (signatures picked to reproduce the argument setup). */
extern void  InitEnemyMissiles(s32 idx);
extern s32   fn_8005A1EC(const char* name, void** outData);
extern s32   LoadModel(const char* name, void** outData, s32 initTexMods, s32 model);
extern void  FatalErrorf(const char* fmt, ...);
extern char  lbl_80112370[];
extern int   sprintf(char* s, const char* fmt, ...);
extern void* fn_80057ACC(s32 key);
extern void  mbBlitInit3414(void* blit, s32 hide);
extern void  reset_players(void);
extern void  LoadPdataFile(void);
extern void  setup_player_models(void);
extern void  UnloadWeaponsPowerups(void);
extern void  LoadWeapons(void);
extern void  LoadWorldData(void);
extern void  MBOX_LockModels(void);
extern void  AtreeListLock(s32 arg0);
extern void  AudioStopSelect(void);
extern void  init_prefs(void);
extern void  InitTexMods(void* tex, s32 arg1);
extern void  MBTreeSetFlags(void* node, s32 flags, s32 arg2);
extern void  MBTreeClearFlags(void* node, s32 flags, s32 arg2);
extern void  MBTreeSetAlpha(void* node, s32 alpha, s32 arg2);
extern void  MBWindowTo3D(f32 depth, s16* screen, f32* camera, f32* out);

/* Forward decls for same-TU functions referenced before definition. */
void game_main(void);
s32  do_stats_display(void);
void LoadTowerAndSelect(void);
static s32 init_next_level_8005638C(s32 arg0);
s32  fn_80054070(s32 arg0, s32 arg1, s32 arg2);
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
s32  WorldExplosion(s32 arg0);
s32  fn_80055F68(s32 arg0, s32 arg1);
s32  fn_80056698(s32 arg0, s32 arg1);
void fn_800510A4(void);
void fn_80052134(void);
s32 fn_80051480(f32* pos);
void fn_800552A4(f32 total, f32 current);
char* fn_80051E1C(s32 world, s32 lvl, s32 flag);

/* ================================================================== */
/* Function bodies (address order).                                    */
/* ================================================================== */

static s32 PlayersAverageLevel(void)
{
    s32* player = (s32*)gPlayers;
    s32 activePlayers = 0;
    s32 totalLevel = 0;
    s32 i;

    for (i = 0; i < 4; i++) {
        if (player[58] == 1) {
            activePlayers++;
            totalLevel += player[3273];
        }
        player += 3287;
    }
    if (activePlayers == 0) {
        return 1;
    }
    return totalLevel / activePlayers;
}

extern f64 lbl_80346A70;
extern f64 lbl_80346830;
extern f32 lbl_803468F0;
extern f64 lbl_80346A30;
extern f64 lbl_80346A28;
extern f32 lbl_80346A78;
extern f64 lbl_80346810;
void format_brain();

/* Xbox PDB: init_enemy_vars -- reset a spawned enemy's combat variables. */
void init_enemy_vars(s32 slot, s32 spew, f32 scale)
{
    u8* e;
    Enemy* enemy;
    u8* tbl;
    s32 i4;
    f32 z;
    f32 z2;
    f32 t;
    f32 hi;
    f32 lo;
    f32 spd;
    f32 ht;
    f32 t2;
    f32 hi2;
    f32 lo2;
    s32 tier;
    s32 ty;
    u8* row;
    f32 fv;
    s16 sv;

    e = (u8*)gEnemies + slot * 916;
    enemy = (Enemy*)e;
    tbl = (u8*)lbl_8011AF48;
    z = lbl_80346820;
    enemy->skinfx.nframes = z;
    enemy->next_enemy = -1;
    enemy->prev_enemy = -1;
    enemy->action = 0;
    enemy->attack_timer = 0;
    enemy->stun_timer = 0;
    enemy->prev_closest = -1;
    enemy->closest = -1;
    enemy->sight = (f32)(lbl_80346A70 * gCurLevel->ene_visrad);
    fv = lbl_803468F0;
    enemy->close_dist = fv;
    enemy->actual_dist = fv;
    row = tbl + *(s32*)e * 4;
    enemy->hht = (f32)(lbl_80346830 * ((f32*)row)[452]);
    row = tbl + *(s32*)e * 4;
    enemy->rad = ((f32*)row)[486];
    enemy->area = 0;
    enemy->coll_pnum = -1;
    enemy->coll_enenum = -1;
    enemy->coll_ip = NULL;
    enemy->floor_wobj = NULL;
    enemy->count = 0;
    enemy->moved = 0;
    enemy->stopped = 0;
    enemy->attack_index = -1;
    enemy->attack_count = 0;
    enemy->attack_flag = 0;
    enemy->damage = z;
    enemy->damagetype = 0;
    for (i4 = 0; i4 < 20; i4 += 4) {
        *(f32*)(e + i4 + 692) = z;
    }
    z2 = lbl_80346820;
    enemy->damagedir[0] = z2;
    enemy->damagedir[1] = z2;
    enemy->damagedir[2] = z2;
    enemy->pushed[0] = z2;
    enemy->pushed[1] = z2;
    enemy->pushed[2] = z2;
    enemy->pushang = z2;
    enemy->push_cnt = 0;
    enemy->generator = NULL;
    enemy->anim_done = -1;
    enemy->idle_time = lbl_803468F0;
    enemy->idle_secs = z2;
    enemy->idle_frac = z2;
    enemy->damage_count = 0;
    row = tbl + *(s32*)e * 4;
    t = gCurLevel->ene_health * ((f32*)row)[690];
    hi = (f32)(lbl_80346A30 * t);
    lo = (f32)(lbl_80346A28 * t);
    tier = 0;
    if (scale > hi) {
        tier = 3;
    } else if (scale > lo) {
        tier = 2;
    } else if (scale > z2) {
        tier = 1;
    }
    enemy->org_lvl = (s16)tier;
    enemy->mode2 = 0;
    enemy->mode1 = 0;
    enemy->flag2 = 0;
    enemy->flag1 = 0;
    enemy->counter2 = 0;
    enemy->counter1 = 0;
    enemy->recognized = 0;
    enemy->skip_itemcol = 0;
    enemy->prev_dir = lbl_80346A78;
    fv = lbl_80346820;
    enemy->zspd = fv;
    enemy->xspd = fv;
    enemy->visible = 1;
    enemy->visactive = 1;
    enemy->gotitem = NULL;
    enemy->specialfx = -1;
    enemy->alpha = 0;
    if (spew == 1) {
        spew = 0;
    }
    if (spew == 10) {
        spew = 7;
    }
    if (spew < 0 || spew > 31) {
        row = tbl + *(s32*)e * 4;
        enemy->algorithm = (s16)((s32*)row)[894];
    } else {
        enemy->algorithm = (s16)spew;
    }
    sv = enemy->algorithm;
    enemy->old_ai = sv;
    enemy->prev_ai = sv;
    format_brain();
    row = tbl + *(s32*)e * 4;
    enemy->atts.invspeed = (f32)(lbl_80346810 / ((f32*)row)[588]);
    ty = *(s32*)e;
    row = tbl + ty * 4;
    t2 = gCurLevel->ene_health * ((f32*)row)[690];
    ht = enemy->health;
    hi2 = (f32)(lbl_80346A30 * t2);
    lo2 = (f32)(lbl_80346A28 * t2);
    spd = gCurLevel->ene_damage * ((f32*)row)[622];
    if (!(ht > hi2)) {
        if (ty != 30) {
            if (ht > lo2) {
                spd = (f32)(lbl_80346A30 * spd);
            } else {
                spd = (f32)(lbl_80346A28 * spd);
            }
        }
    }
    enemy->atts.fight = spd;
    row = tbl + *(s32*)e * 4;
    enemy->atts.armor = ((f32*)row)[656];
    row = tbl + *(s32*)e * 4;
    enemy->atts.damagetype = ((s32*)row)[928];
    row = tbl + *(s32*)e * 4;
    enemy->atts.armortype = ((s32*)row)[962];
}

/* Xbox PDB: format_brain -- initialize a newly allocated enemy's AI state. */
void format_brain(s32 index)
{
    Enemy* enemy = &gEnemies[index];

    enemy->view = lbl_80346A7C;
    enemy->ai_flags = 0;

    switch (enemy->algorithm) {
    case 0:
        enemy->route = 1;
        enemy->collided = 0;
        break;
    case 3:
        enemy->route = 1;
        enemy->collided = 0;
        enemy->counter1 = 0;
        enemy->counter2 = -1;
        enemy->flag2 = 0;
        break;
    case 2:
    case 4:
        enemy->play = 0;
        enemy->ai_flags |= 1;
        break;
    case 5:
    case 6:
        enemy->ai_flags |= 1;
        break;
    case 7:
        enemy->route = 0;
        enemy->collided = 0;
        enemy->stuck_count = 0;
        break;
    case 8:
        enemy->route = 0;
        enemy->collided = 0;
        enemy->stuck_count = 0;
        enemy->guard_mode = 0;
        enemy->guard_closest = -1;
        enemy->guard_dist = lbl_803468B0;
        break;
    case 9:
        enemy->daction = 0;
        enemy->ai_flags |= 5;
        break;
    case 10:
        enemy->route = 0;
        enemy->collided = 0;
        enemy->stuck_count = 0;
        break;
    case 11:
        enemy->ai_flags |= 7;
        break;
    case 12: {
        f32 zero = lbl_80346820;
        enemy->dest[0] = zero;
        enemy->dest[1] = zero;
        enemy->dest[2] = zero;
        enemy->route = 0;
        enemy->collided = 0;
        enemy->stuck_count = 0;
        break;
    }
    case 13:
        enemy->route = 0;
        enemy->collided = 0;
        enemy->stuck_count = 0;
        break;
    case 14:
        enemy->route = 0;
        enemy->collided = 0;
        enemy->stuck_count = 0;
        enemy->counter1 = 0;
        break;
    case 16:
    case 23:
        enemy->flag2 = RandInt(10);
        enemy->counter1 = 0;
        break;
    case 17:
    case 26:
        enemy->flag2 = RandInt(10);
        enemy->counter1 = 0;
        break;
    case 20:
        enemy->route = 0;
        enemy->collided = 0;
        enemy->stuck_count = 0;
        break;
    case 21:
        enemy->counter1 = 0;
        break;
    case 24:
        enemy->counter1 = 0;
        break;
    case 27:
        enemy->route = 1;
        enemy->collided = 0;
        break;
    case 28:
    case 29:
    case 31:
        enemy->flag2 = RandInt(30);
        enemy->counter1 = 0;
        enemy->counter2 = 0;
        break;
    case 30:
        enemy->route = 1;
        enemy->collided = 0;
        enemy->counter1 = RandInt(60) + 60;
        break;
    }

    enemy->count = 0;
    enemy->dead_end = 0;
    enemy->plr_ms = -1;
    enemy->ms_idx = 0;
    enemy->max_msidx = 4;
    enemy->watchdog = 0;
    enemy->lv = PlayersAverageLevel();
    enemy->operation_speed = 8;
    enemy->operation_count = RandInt(enemy->operation_speed);
}

/* Rebuild an enemy's animation tree, render object, actions, and shadow. */
void SetEnemyObj(Enemy* enemy, s32 type, s32 level, s32 unused)
{
    void* object;
    s32 shadowObject;
    s32 shadowLevel;

    if (enemy->type == 27) {
        SfxDeleteParented(enemy->objgrp.node, 0, -1);
    }
    if (enemy->atree.root != 0) {
        AtreeDelete(&enemy->atree.root);
    }
    if (enemy->objgrp.node != 0) {
        lbl_80344734 = 1;
        MBRemoveNode(enemy->objgrp.node, 0);
        lbl_80344734 = 0;
    }
    enemy->atree.root = 0;
    enemy->objgrp.node = 0;
    enemy->flooroffset = lbl_80346820;

    if (type == 31) {
        enemy->atree.root = (void*)fn_80011BBC(
            sGoodWizObj, lbl_80346770, &enemy->atree.root, lbl_80346770, 2048);
        enemy->flooroffset = lbl_80346A80;
    } else if (gWadAtreeHeaders[type] != 0) {
        char* name = fn_80051E1C(type, level, 0);
        enemy->atree.root = (void*)fn_80011BBC(
            gWadAtreeHeaders[type], name, &enemy->atree.root, name, 2048);
    }

    if (enemy->atree.root != 0) {
        enemy->objgrp.node = MBNewNode(lbl_8034473C,
                                      (void*)gIdentityMatrix, 1);
        MBNodeSetParent(*(void**)enemy->atree.root, enemy->objgrp.node);
        InitActions(&enemy->atree.root, enemy->actionlist, lbl_80126EC0);
    } else {
        InitActions(0, enemy->actionlist, lbl_80126EC0);
    }

    if (enemy->objgrp.node == 0) {
        char* name = fn_80051E1C(type, level, 1);
        enemy->objgrp.node = MBOX_NewObject(name, (f32*)gIdentityMatrix,
                                            (void*)lbl_8034473C, 0);
        MBTreeSetFlags(enemy->objgrp.node, 2048, 0);
    }

    if (enemy->shadow != 0) {
        MBRemoveNode(enemy->shadow, 0);
        enemy->shadow = 0;
    }

    if (enemy->type != 30 && enemy->type != 0 &&
        enemy->type != 31 && enemy->type != 21) {
        shadowObject = lbl_802512B0[type];
        if (level < 1) {
            shadowLevel = 1;
        } else if (level > 3) {
            shadowLevel = 3;
        } else {
            shadowLevel = level;
        }
        level = shadowLevel - 1;
        object = MBOX_ReallyFindObject(lbl_8011BFF8[level],
                                       shadowObject, shadowObject, 1);
        enemy->shadow = MBNewObject(object, (f32*)gIdentityMatrix, 0, 2176);
        ((MBObject*)enemy->shadow)->zsort_add = lbl_80346A80;
        ((MBObject*)enemy->shadow)->zmod = -32;
    }
}

/* 0x800508A0 -- re-init texture-mod state for every active pool entry. */
#pragma dont_inline on
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
#pragma dont_inline off

/* 0x80050910 -- negate one entry of the sign-flip table, then notify. */
void fn_80050910(s32 arg0)
{
    lbl_802511FC[arg0] = -lbl_802511FC[arg0];
    InitEnemyMissiles(arg0);
}

/* Allocate an enemy model slot without loading the model contents. */
void AllocEnemy(s32 id, s32 model)
{
    char buf[68];
    u8 unused[4];
    char* fmt = lbl_80112370;
    Row36* tbl = lbl_8011AF48;
    s32* pool = lbl_80250E00;
    char* name;
    s32 i;

    lbl_8034471C++;
    if (lbl_8034471C > 8) {
        FatalErrorf(fmt + 284, lbl_8034471C, 8);
    }
    pool[7 + lbl_8034471C] = id;

    if (id == 29 || id == 33) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].f0) {
                name = (char*)&tbl[i].f4;
                goto alloc_fmt1;
            }
        }
        name = 0;
alloc_fmt1:
        sprintf(buf, fmt + 304, name, fn_80057ACC(id));
    } else if (id == 32) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].f0) {
                name = (char*)&tbl[i].f4;
                goto alloc_fmt2;
            }
        }
        name = 0;
alloc_fmt2:
        sprintf(buf, fmt + 320, name, fn_80057ACC(id));
    } else if (model == 4) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].f0) {
                name = (char*)&tbl[i].f4;
                goto alloc_fmt3;
            }
        }
        name = 0;
alloc_fmt3:
        sprintf(buf, fmt + 336, name);
    } else if (model > 10) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].f0) {
                name = (char*)&tbl[i].f4;
                goto alloc_fmt4;
            }
        }
        name = 0;
alloc_fmt4:
        sprintf(buf, fmt + 352, name, model - 10);
    } else {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].f0) {
                name = (char*)&tbl[i].f4;
                goto alloc_fmt5;
            }
        }
        name = 0;
alloc_fmt5:
        sprintf(buf, fmt + 368, name);
    }

    id <<= 2;
    *(s32*)((u8*)&pool[300] + id) =
        fn_8005A1EC(buf, (void**)((u8*)&pool[345] + id));
    *(s32*)((u8*)&pool[255] + id) = -model;
}

/* Load an enemy model and finish its per-type missile setup. */
void LoadEnemy(s32 id, s32 model)
{
    char buf[68];
    u8 unused[4];
    char* fmt = lbl_80112370;
    Row36* tbl = lbl_8011AF48;
    s32* pool = lbl_80250E00;
    char* name;
    s32 i;
    s32 offset;

    lbl_8034471C++;
    if (lbl_8034471C > 8) {
        FatalErrorf(fmt + 284, lbl_8034471C, 8);
    }
    pool[7 + lbl_8034471C] = id;

    if (id == 29 || id == 33) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].f0) {
                name = (char*)&tbl[i].f4;
                goto load_fmt1;
            }
        }
        name = 0;
load_fmt1:
        sprintf(buf, fmt + 304, name, fn_80057ACC(id));
    } else if (id == 32) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].f0) {
                name = (char*)&tbl[i].f4;
                goto load_fmt2;
            }
        }
        name = 0;
load_fmt2:
        sprintf(buf, fmt + 320, name, fn_80057ACC(id));
    } else if (model == 4) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].f0) {
                name = (char*)&tbl[i].f4;
                goto load_fmt3;
            }
        }
        name = 0;
load_fmt3:
        sprintf(buf, fmt + 336, name);
    } else if (model > 10) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].f0) {
                name = (char*)&tbl[i].f4;
                goto load_fmt4;
            }
        }
        name = 0;
load_fmt4:
        sprintf(buf, fmt + 352, name, model - 10);
    } else {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].f0) {
                name = (char*)&tbl[i].f4;
                goto load_fmt5;
            }
        }
        name = 0;
load_fmt5:
        sprintf(buf, fmt + 368, name);
    }

    offset = id << 2;
    *(s32*)((u8*)&pool[300] + offset) =
        LoadModel(buf, ((void**)&pool[345]) + id, 0, -1);
    *(s32*)((u8*)&pool[255] + offset) = model;
    InitEnemyMissiles(id);
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
void* EnemyTypePrefix(s32 id)
{
    s32 i;

    for (i = 0; i < 44; i++) {
        if (lbl_8011AF48[i].f0 == id) {
            return &lbl_8011AF48[i].f14;
        }
    }
    return 0;
}

/* 0x80051FA0 -- table lookup by id, return &entry.f4 (the enemy descriptor). */
void* EnemyTypeDesc(s32 id)
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

/* 0x800520CC -- restore default options, then load saved preferences. */
void default_options(void)
{
    s32* options;
    s32 zero;

    zero = 0;
    options = &gGameOptions[zero];
    options[0] = zero;
    options[1] = zero;
    options[2] = 3;
    options[6] = zero;
    options[7] = 1;
    options[8] = zero;
    options[3] = zero;
    options[4] = zero;
    options[5] = zero;
    options[9] = 512;
    options[10] = zero;
    options[11] = zero;
    init_prefs();
}

/* 0x800533E4 -- reload player data / models / weapons / world. */
void ResetModels(void)
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
void init_moving_objects(void)
{
    s32* e = (s32*)gEnemies;
    s32 i;

    for (i = 0; i < 25; i++) {
        e[45] = 0;   /* +0xB4 */
        e[25] = 0;   /* +0x64 */
        e += 229;    /* stride 0x394 */
    }
}

/* 0x80053A38 -- hide the blit at lbl_803447B0 if present. */
void TransitionBlitHide(void)
{
    if (lbl_803447B0 != 0) {
        mbBlitInit3414(lbl_803447B0, 1);
    }
}

/* 0x80053B20 -- reset the selection/flow globals and stop select audio. */
void EndTower(void)
{
    lbl_80343C10 = -1;
    lbl_80343DD4 = -1;
    lbl_80343B38 = -1;
    AudioStopSelect();
    lbl_803448AC = -1;
    lbl_803448A8 = -1;
}

/* 0x80053B60 -- start something at slot 1. */
void ShowLoading(void)
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
s32 SetMaxFPS(s32 arg0)
{
    s32 old = lbl_80343C0C;
    lbl_80343C0C = arg0;
    return old;
}

static s32 worldAnimIndexMatches(s32 entry, s32 index)
{
    return entry == index;
}

/* 0x80055CB8 -- find the worldanim bound to a given world object. */
s16* FindWobjWanim(void* wobj)
{
    s32 idx = ((s32)wobj - (s32)gWorldInfo.wobjs) / 60;   /* stride 60 */
    u8* wa = (u8*)gWorldInfo.worldanims;

    s32 i;

    for (i = 0; i < gWorldInfo.nworldanims; i++) {
        if (worldAnimIndexMatches(*(s16*)(wa + i * 16), idx)) {
            return (s16*)(wa + i * 16);
        }
    }
    return 0;
}

/* Advance every active world-object animation track. */
void DoWorldAnimation(void)
{
    u8* data_off;
    u8* track_off;
    s32* count;
    s32* header;
    s32 i;
    u8* anim_base;

    if ((void*)gWorldInfo.atreelist != NULL) {
        DoTexMods((void*)gWorldInfo.atreelist);
    }
    if ((lbl_80344768 > 0 || (gGameMode & 0x8000) != 0) &&
        (lbl_803443BC <= 10 || lbl_803443BC >= 100000) &&
        gWorldInfo.nworldanims != 0 && (void*)gWorldInfo.animheader != NULL) {
        count = &gWorldInfo.nworldanims;
        header = (s32*)gWorldInfo.animheader;
        anim_base = (u8*)header[3];
        i = 0;
        data_off = NULL;
        track_off = NULL;
        lbl_803441B8 = header[0];
        lbl_803441B4 = header[1];
        lbl_803441B0 = header[2];
        while (i < *count) {
            u8* track = (u8*)gWorldInfo.worldanims + (u32)track_off;
            if (*(u32*)(track + 12) != 0) {
                DoWorldAnimSub(track, (u8*)gWorldInfo.animdata + (u32)data_off, anim_base);
            }
            i++;
            data_off += 160;
            track_off += 16;
        }
    }
}

/* 0x80055E04 -- run WorldExplosion, then flag a scene-node subtree. */
void WorldObjectExplode(void* node, s32 arg1)
{
    s32* n = (s32*)node;

    WorldExplosion(arg1);
    while (n != 0) {
        MBTreeSetFlags((void*)n[10], 2, 0);   /* +0x28 */
        n[4] |= 0x10000000;                   /* +0x10 */
        n = (s32*)n[6];                       /* +0x18 next */
    }
}

static char* findWorldName(s32 world)
{
    s32 off = 0;
    s32 i;

    for (i = 0; i < 44; i++) {
        u8* e = (u8*)lbl_8011AF48 + off;
        if (world == *(s32*)e) {
            return (char*)(e + 4);
        }
        off += 36;
    }
    return 0;
}

extern char lbl_80112370[];        /* format-string blob */
int sprintf(char* s, const char* fmt, ...);
void* fn_80057ACC(s32 key);

/* 0x80050DD8 -- format the pickup/status message line for an id/qty. */
void fn_80050DD8(char* buf, s32 id, s32 qty)
{
    char* fmt = lbl_80112370;
    Row36* tbl = lbl_8011AF48;
    char* name;
    s32 i;

    if (id == 29 || id == 33) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].f0) {
                name = (char*)&tbl[i].f4;
                goto f1;
            }
        }
        name = 0;
f1:
        sprintf(buf, fmt + 304, name, fn_80057ACC(id));
    } else if (id == 32) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].f0) {
                name = (char*)&tbl[i].f4;
                goto f2;
            }
        }
        name = 0;
f2:
        sprintf(buf, fmt + 320, name, fn_80057ACC(id));
    } else if (qty == 4) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].f0) {
                name = (char*)&tbl[i].f4;
                goto f3;
            }
        }
        name = 0;
f3:
        sprintf(buf, fmt + 336, name);
    } else if (qty > 10) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].f0) {
                name = (char*)&tbl[i].f4;
                goto f4;
            }
        }
        name = 0;
f4:
        sprintf(buf, fmt + 352, name, qty - 10);
    } else {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].f0) {
                name = (char*)&tbl[i].f4;
                goto f5;
            }
        }
        name = 0;
f5:
        sprintf(buf, fmt + 368, name);
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
    return (u8*)gWorldData + 4;
}

/* 0x80057AB4 -- accessor: current-level record + 8. */
void* LevelItemDesc(void)
{
    return gCurLevel->name;
}

/* 0x80057AC0 -- accessor: world-data record + 4. */
void* WorldItemDesc(void)
{
    return (u8*)gWorldData + 4;
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

/* 0x8005403C -- lock the model box, then run AtreeListLock. */
#pragma dont_inline on
void LockModels(s32 arg0)
{
    MBOX_LockModels();
    AtreeListLock(arg0);
}
#pragma dont_inline off

/* 0x8005636C -- advance a two-field counter unless it is parked at 2. */
void fn_8005636C(s32* s)
{
    if (s[4] == 2) {
        return;
    }
    s[1] += s[2];
}

/* 0x8005773C - build the per-level enemy-type track table from the current
 * level's 6 type slots: resolve each to its world-data entry (0x18 stride),
 * arm its audio streams, synthesise a boss slot (0x1e) when the level calls
 * for one, resolve missing subtypes, build the subtype->type reverse map, and
 * apply the easy-difficulty (< 2) type substitutions. */
extern void AudioClearActiveTracks(void);
extern void AudioSetupBossStreams(s32 idx, void* data);
extern char lbl_801129D4[];

typedef struct EnemyTypeRow {
    u8 pad0[0xEC];
    u8* ent;
    u8 pad1[0x1C];
    s32 subtype;
    u8 pad2[0x1C];
    s32 reverse;
    u8 pad3[0x1C];
    s32 type;
} EnemyTypeRow;

#pragma opt_lifetimes off
void GetEnemyTypes(void)
{
    u8* tbl = (u8*)lbl_80257680;
    s32 i;
    s32 seen1e = 0;
    u8* etab = *(u8**)((u8*)gWorldData + 0x20);
    s32 off;
    s32 levelOff;

    AudioClearActiveTracks();
    i = 0;
    off = 0;
    levelOff = 0;
    for (; i < 8; i++, off += 4, levelOff += 2) {
        s32 type;
        s32 t14c;
        u8* ent;
        u8* subWords;
        u8* typeWords;

        if (i < 6) {
            type = *(s16*)((u8*)gCurLevel + levelOff + offsetof(level_data, enemytype));
        } else {
            type = -1;
        }
        if (type >= 0) {
            ent = etab + type * 0x18;

            {
                EnemyTypeRow* slot = (EnemyTypeRow*)tbl;
                slot = (EnemyTypeRow*)((u8*)slot + off);
                slot->type = *(s32*)ent;
                slot->subtype = *(s32*)(ent + 0x4);
            }
            if (*(s32*)(ent + 0x4) != 9 && *(s32*)(ent + 0x4) != 5) {
                AudioSetupBossStreams(i, ent + 0x8);
            }
            {
                EnemyTypeRow* slot = (EnemyTypeRow*)tbl;
                slot = (EnemyTypeRow*)((u8*)slot + off);
                slot->ent = ent;
            }
        } else if (gCurLevel->bosstype < 0 && *(s32*)gWorldData != 0xD &&
                   seen1e == 0) {
            *(s32*)(tbl + off + offsetof(EnemyTypeRow, type)) = 0x1E;
            *(s32*)(tbl + off + offsetof(EnemyTypeRow, subtype)) = 0;
            *(s32*)(tbl + off + offsetof(EnemyTypeRow, ent)) = 0;
        } else {
            *(s32*)(tbl + off + offsetof(EnemyTypeRow, type)) = -1;
            *(s32*)(tbl + off + offsetof(EnemyTypeRow, subtype)) = 0;
            *(s32*)(tbl + off + offsetof(EnemyTypeRow, ent)) = 0;
        }

        typeWords = tbl;
        typeWords += off;
        t14c = *(s32*)(typeWords += offsetof(EnemyTypeRow, type));
        if (t14c == 0x1E) {
            seen1e = 1;
        }
        if (t14c >= 0) {
            subWords = tbl;
            subWords += off;
            if (*(s32*)(subWords += offsetof(EnemyTypeRow, subtype)) == 0) {
                *(s32*)subWords = GetEnemySubtype(t14c);
            }
            if (*(s32*)subWords <= 0) {
                ErrorPrintf(lbl_801129D4, *(s32*)typeWords, *(s32*)subWords);
            }
        }
    }

    for (i = 0; i < 8; i++) {
        EnemyTypeRow* row = (EnemyTypeRow*)tbl;
        row = (EnemyTypeRow*)((u8*)row + i * 4);
        row->reverse = -1;
    }
    for (i = 0; i < 8; i++) {
        EnemyTypeRow* row = (EnemyTypeRow*)tbl;
        s32 idx;
        row = (EnemyTypeRow*)((u8*)row + i * 4);
        idx = row->subtype;

        if (idx < 6) {
            s32 rowType = row->type;
            row = (EnemyTypeRow*)tbl;
            row = (EnemyTypeRow*)((u8*)row + idx * 4);
            row->reverse = rowType;
        }
    }
    if (gGameOptions[2] < 2) {
        for (i = 0; i < 8; i++) {
            u8* words;
            words = tbl;
            words += i * 4;
            if (*(s32*)(words += offsetof(EnemyTypeRow, subtype)) == 2) {
                *(s32*)words = 4;
            } else {
                s32 value;
                words = tbl;
                words += i * 4;
                value = *(s32*)(words += offsetof(EnemyTypeRow, type));
                if (value >= 0 && value < 28) {
                    *(s32*)words = -1;
                } else {
                    *(volatile s32*)words = -1;
                }
            }
        }
    }
}
#pragma opt_lifetimes reset
/* 0x80057978 -- map an enemy type id to its shared subtype class. */
s32 GetEnemySubtype(s32 type)
{
    s32 subtype = 0;

    switch (type) {
    case 0:
    case 3:
    case 6:
    case 9:
    case 12:
    case 15:
    case 18:
    case 21:
    case 22:
        subtype = 1;
        break;
    case 1:
    case 4:
    case 7:
    case 10:
    case 13:
    case 19:
    case 23:
        subtype = 3;
        break;
    case 2:
    case 5:
    case 8:
    case 11:
    case 14:
    case 16:
    case 17:
    case 20:
    case 24:
    case 25:
    case 27:
        subtype = 4;
        break;
    case 29:
    case 33:
        subtype = 5;
        break;
    case 30:
        subtype = 6;
        break;
    case 31:
        subtype = 7;
        break;
    case 32:
        subtype = 8;
        break;
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
        subtype = 9;
        break;
    }
    return subtype;
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
        c = sWorldDataTypes[idx].letter;
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
    AtreeInitLists(0);
    AudioRegisterMenu();
    AudioResetInput();
    AudioRegisterMenu();
    lbl_80344800 = 0;
    lbl_803447C0 = 0;
    gLanguageId = 0;
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
        serve_busy(-1);
    }
    bulletproof_printf(msgs + 136);  /* "Loading Game."         */
    ControlsUpdate();
    lbl_80344784 = 0;
}

/* 0x80053A68 -- create/refresh the loading-screen blit. */
void TransitionBlitShow(s32 arg0)
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

/* 0x80053B88 -- finish async tower loading, then initialize the selected level. */
void LoadTowerAndSelect(void)
{
    u32 timeout = pbLoad + 900;

    if (lbl_80343DD4 < 0) {
        fn_80053D08(-1, 0, -1);
        SelectLoadStart();
        while (SelectLoadDone() == 0) {
            if (pbLoad > timeout) {
                FatalError(lbl_801125E4, 0x8000);
            }
        }
    }
    if (lbl_80343C10 < 0) {
        fn_80053D08(sWorldDataConst, 1, -1);
        FontInitSpecial(lbl_80112600, 8);
        ShopLoadData();
        LoadItems();
        lbl_80343C10 = init_next_level_8005638C(sWorldDataConst);
        if (gDemoMode == 0) {
            opt_force_player = 0;
        }
    } else {
        fn_80053D08(-1, 1, -1);
    }
}

/* 0x80053D08 -- tear down the current front-end/world state and load a wave. */
#pragma opt_propagation off
#pragma opt_lifetimes off
s32 fn_80053D08(s32 wave, s32 mode, s32 loadResult)
{
    char* strings = lbl_80112538;
    s32 result;
    f32 zero;
    u32 buttons;
    s32 flagMask;
    s32 flags;

    EndFireScroll();
    DeleteOptionBlits();
    lbl_8034479C = (void*)(result = 0);
    SumnerEnd();
    AudioStopSelect();
    good_wiz_enabled = result;
    Randomize(0x12D687);
    good_wiz_state = result;
    ResetPlayerMissiles();
    ClearAllPlyrData();
    InitializeClockIRQ();
    vibrators_off();

    buttons = *(u32*)&gControllerButtons;
    flagMask = 0x10;
    flags = sFlags;
    buttons &= result;
    flagMask = flags & flagMask;
    flagMask ^= result;
    buttons ^= result;
    if ((flagMask | buttons) != 0) {
        LoadWorldData();
    }

    if (lbl_80343C10 >= 0) {
        WorldRestoreInitState();
        MBOX_ResetUnlockedModels(2);
        AtreeInitLists(2);
        ResetTexmods();
        result = fn_80054070(wave, mode, loadResult);
    } else if (loadResult < 0) {
        bulletproof_printf(strings + 212, wave, BytesFree());
        if (wave >= 0 && wave != sWorldDataConst) {
            result = -1;
            lbl_80343C10 = result;
            lbl_80343DD4 = result;
            lbl_80343B38 = result;
            AudioStopSelect();
            lbl_803448AC = result;
            lbl_803448A8 = result;
        }

        fn_800BC418(2, -1);
        if (wave == sWorldDataConst && (gGameMode & 0x8000) == 0) {
            MBOX_ResetUnlockedModels(2);
            AtreeInitLists(2);
        } else {
            MBOX_ResetUnlockedModels(1);
            AtreeInitLists(1);
        }

        zero = lbl_80346AFC;
        MBCompVertScaleAddUV(0, 0, zero, zero, zero, zero, zero);
        ResetTexmods();
        bulletproof_printf(strings + 240, BytesFree());
        ResetWorlds();
        bulletproof_printf(strings + 260, BytesFree());
        fn_80051164();
        bulletproof_printf(strings + 284, BytesFree());
        InitItems();
        bulletproof_printf(strings + 304, BytesFree());
        sndSysInit();
        bulletproof_printf(strings + 328, BytesFree());
        result = fn_80054070(wave, mode, -1);
        bulletproof_printf(strings + 348, BytesFree());
    } else {
        MBOX_ResetUnlockedModels(2);
        AtreeInitLists(2);
        zero = lbl_80346AFC;
        MBCompVertScaleAddUV(0, 0, zero, zero, zero, zero, zero);
        ResetTexmods();
        result = fn_80054070(wave, mode, loadResult);
    }

    lbl_803447B0 = 0;
    lbl_803447A0 = MBOX_FindTexture(strings + 364, 0);
    lbl_803443E4 = MBOX_FindTexture(strings + 376, 0);
    lbl_80344E48 = MBOX_FindTexture(strings + 388, 0);
    lbl_80344E44 = MBOX_FindTexture(strings + 400, 0);
    lbl_80344E40 = MBOX_FindTexture(strings + 412, 0);
    lbl_80344E3C = MBOX_FindTexture(strings + 424, 0);
    lbl_80344E38 = MBOX_FindTexture(strings + 436, 0);
    lbl_80344E34 = MBOX_FindTexture(strings + 448, 0);
    lbl_80344E30 = MBOX_FindTexture(strings + 460, 0);
    lbl_80344E2C = MBOX_FindTexture(strings + 472, 0);
    AudioRegisterMenu();
    InitLighting(0);
    return result;
}
#pragma opt_lifetimes reset
#pragma opt_propagation reset

/* 0x80054D18 -- choose and resolve the next world/level selection. */
#pragma opt_propagation off
static inline s32 load_world_option(s32* options)
{
    return options[9];
}

s32 next_world(void)
{
    u8 unused[8];
    s32 world;
    s32 forced;
    s32 transitioning = 0;
    s32 state = lbl_8034481C;
    s32 t2;
    register s32 selected;

    if (state >= 13 && state < 0x10000) {
        transitioning = 1;
    }
    if (transitioning != 0) {
        t2 = 1;
    } else {
        t2 = 0;
    }
    forced = 0;
    if (state >= 2) {
        forced = 1;
    }
    if (t2) {
        world = lbl_80344B84;
        forced = 1;
    } else if (sLastWorldLevel < 0) {
        selected = load_world_option(gGameOptions);
        world = selected;
        if ((selected >> 8) >= 14) {
            world = sFirstWorldId;
        }
        lbl_8034481C = world + 0x10000;
        forced = 1;
    } else {
        s32 i;

        world = -1;
        for (i = 0, transitioning = 0; i < 4; i++, transitioning += 13148) {
            Player* player = (Player*)((u8*)gPlayers + transitioning);
            state = player->state;
            if (state != 0 && state != 2) {
                state = player->exit_dest;
                if (world < state) {
                    world = state;
                }
            }
        }
        if (world < 0) {
            world = sWorldDataConst;
        }
        if ((world >> 8) >= 14) {
            world = sFirstWorldId;
        }
    }
    ResolveWorldData(world);
    if (!forced && ((gCurLevel->enabled & 1) == 0)) {
        world = NextWorldLevel(1);
        ResolveWorldData(world);
    }
    return world;
}
#pragma opt_propagation reset

/* 0x800552A4 -- animate the two halves of the loading thermometer. */
void fn_800552A4(f32 total, f32 current)
{
    u8 unused[8];
    f64 offset;
    f64 vertex;
    f32 progress = (total - current) / total;

    vertex = 41.0 * progress + 23.0;
    vertex *= 0.0078125;
    mbBlitSetupVerts(lbl_80257630[1], -1.0f, -1.0f,
                     (f32)vertex, -1.0f);
    offset = 39.0 * progress;
    mbBlitProject(lbl_80257630[1], 0, 41 - Round((f32)offset));
    mbBlitCalcY(lbl_80257630[1], Round((f32)offset) + 24);

    offset = 38.0 * progress;
    vertex = 105.0 - offset;
    vertex *= 0.0078125;
    mbBlitSetupVerts(lbl_80257630[2], -1.0f, -1.0f,
                     (f32)vertex, -1.0f);
    mbBlitProject(lbl_80257630[2], 0, Round((f32)offset) + 23);
    mbBlitCalcY(lbl_80257630[2], 106 - Round((f32)offset));
}

/* 0x800553B4 -- initialize the four timer/thermometer HUD blits. */
void fn_800553B4(void)
{
    char* strings = lbl_80112538;
    u8* state = lbl_802575C0;
    void** blit1;
    void** blit2;
    void** blit3;
    s32 offset;
    s32 i;
    s32 hide;
    s32 texture;

    if ((gCurLevel->flags & 4) != 0) {
        if ((gControllerButtons & 0x10) != 0) {
            lbl_80344814 = lbl_80346B60;
        } else {
            lbl_80344814 =
                (f32)(lbl_80346B68 + (f64)gCurLevel->wavetime);
        }
        lbl_80344818 = lbl_80344814;
    }

    lbl_80344810 = 0;
    *(void**)(state + 112) = MBCreateBlit(0, 0, 1, 1, -1, -1);
    *(blit1 = (void**)(state + 116)) = MBCreateBlit(0, 0, 1, 24, -1, -1);
    *(blit2 = (void**)(state + 120)) = MBCreateBlit(0, 0, 1, 106, -1, -1);
    *(blit3 = (void**)(state + 124)) = MBCreateBlit(0, 0, 63, 58, -1, -1);

    mbBlitCvtCoord(*(void**)(state + 112), lbl_80346B70);
    mbBlitCvtCoord(*blit1, lbl_80346B74);
    mbBlitCvtCoord(*blit2, lbl_80346B74);
    mbBlitCvtCoord(*blit3, lbl_80346B78);

    if ((gControllerButtons & 0x10) == 0) {
        if ((gCurLevel->flags & 4) != 0) {
            hide = 0;
        } else {
            hide = 1;
        }
    } else {
        hide = 1;
    }
    for (i = 0, offset = 0; i < 4; i++, offset += 4) {
        void** entry = (void**)(state + offset);
        mbBlitInit3414(entry[28], hide);
    }
    mbBlitInit3414(*blit3, 1);

    texture = MBOX_FindTexture_Err(lbl_80346B7C, 0, 1);
    mbInitBlitEntry(*(void**)(state + 112), texture, 0);
    mbInitBlitEntry(*blit1,
                    MBOX_FindTexture_Err(strings + 544, 0, 1), 0);
    mbInitBlitEntry(*blit2,
                    MBOX_FindTexture_Err(strings + 544, 0, 1), 0);
    mbInitBlitEntry(*blit3,
                    MBOX_FindTexture_Err(strings + 556, 0, 1), 0);

    mbBlitSetupVerts(*blit1, lbl_80346B20, lbl_80346B20,
                     lbl_80346B84, lbl_80346AD4);
    mbBlitProject(*blit1, 0, 41);
    mbBlitSetupVerts(*blit2, lbl_80346B20, lbl_80346B20,
                     lbl_80346B88, lbl_80346AF0);
    mbBlitProject(*blit2, 0, 23);
}

/* 0x80054070 -- load a world/level, measuring its heap usage. */
s32 fn_80054070(s32 arg0, s32 arg1, s32 arg2)
{
    s32 free;

    lbl_803447A4 = BytesFree();
    sMusicTrackHi = -1;
    if (arg0 == sWorldDataConst) {
        fn_80057F44(sWorldDataConst, 0);
    } else if (arg0 >= 0) {
        arg2 = fn_80056698(arg0, arg2);
    }
    free = BytesFree();
    lbl_803447A4 = lbl_803447A4 - free;
    lbl_803447E4 = 0;
    lbl_803447EC = 0;
    lbl_803447F0 = 0;
    lbl_803447F4 = 0;
    NewWorld(0);
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
s32 WorldExplosion(s32 arg0)
{
    f32 f31;
    f32 f30;
    s32 fxid;
    s32 result;
    s32 dmg;

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
    AudioWorldExplosion(arg0);
    return result;
}

/* 0x80052134 -- update the option marker's visibility and screen position. */
void fn_80052134(void)
{
    s32 i;

    if (lbl_8034479C != 0) {
        if (options_state != 0 ||
            optionsAudioAndPrefs30[6] == 0) {
            MBTreeSetFlags(lbl_8034479C, 1, 0);
        } else {
            MBTreeClearFlags(lbl_8034479C, 1, 0);
            MBWindowTo3D(lbl_80343C1C, &lbl_80343C14,
                         (f32*)(lbl_80344EE8 + 100),
                         ((MBObject*)lbl_8034479C)->mat[3]);
            for (i = 0; i < 3; i++) {
                ((MBObject*)lbl_8034479C)->scale[i] = lbl_80343C18;
            }
            MBTreeSetAlpha(lbl_8034479C, lbl_80343C20, 0);
        }
    }
}

/* 0x800521E8 -- animate the loading-timer HUD, arm attract on timeout. */
void fn_800521E8(void)
{
    s32 idx;
    s32 flag = gGameBusy;
    s32 oldTimer = lbl_80344774;
    s32 newTimer;
    void* txt;
    u8* textData;
    u8 unused[8];

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
    textData = ((void**)txt)[4];
    textData[idx] = 0;
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
#pragma opt_propagation off
void SetPlayerVars(void)
{
    u8* base = (u8*)gPlayers;
    s32 offset = 0;
    s32 bossType = gBossType;
    s32 count1 = 0;
    s32 count2 = 0;
    s32 count3 = 0;
    Player* e;
    s32 i;
    s32 type;
    s32 f292;

    lbl_803447D4 = lbl_803447D8;
    lbl_803447DC = offset;
    lbl_803447D8 = lbl_80346AF0;
    lbl_803447E0 = offset;
    for (i = 0; i < 4; i++, offset += 13148) {
        e = (Player*)(base + offset);
        type = e->state;
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
            f292 = e->flags;
            if (f292 & 0x8) {
                lbl_803447DC = 1;
            }
            if (bossType < 0 && (f292 & 0x200)) {
                lbl_803447D8 = lbl_803447D8 * lbl_80346B00;
            }
        }
        e->hud_flags2 = 0;
    }
    fn_8005207C(count1, count2, count3);
}
#pragma opt_propagation reset

/* 0x80050FB0 -- resolve a type id through the override tables. */
s32 GetEnemyType(s32 w, s32 l)
{
    s32 result = w;

    if (w == 3) {
        result = lbl_802577AC[1];
    }
    if ((u32)(w - 4) <= 1) {
        if (l >= 4 && lbl_802577AC[4] >= 0) {
            result = lbl_802577AC[4];
        } else if (lbl_802577AC[2] >= 0) {
            result = lbl_802577AC[2];
        } else if (lbl_802577AC[3] >= 0) {
            result = lbl_802577AC[3];
        }
    }
    if (result == -1) {
        char* name;
        s32 i;
        for (i = 0; i < 44; i++) {
            if (w == lbl_8011AF48[i].f0) {
                name = (char*)&lbl_8011AF48[i].f4;
                goto have_name;
            }
        }
        name = 0;
have_name:
        ErrorPrintf(lbl_801124EC, name);
    }
    return result;
}

/* 0x800575CC -- debug dump of the per-category memory usage. */
#pragma opt_propagation off
void PrintWorldMemSizes(void)
{
    char* fmt = lbl_80112788;
    WorldMemTable* t = (WorldMemTable*)lbl_80257680;
    s32 sum;
    u8 unused[8];
    s32 i;
    WorldMemTable* entry;

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
        entry = (WorldMemTable*)((u8*)t + i * 4);
        sum += *(volatile s32*)&entry->sizes[0];
    }
    bulletproof_printf(fmt + 508, sum);
    for (i = 0; i < 8; i++) {
        entry = (WorldMemTable*)((u8*)t + i * 4);
        if (entry->typeids[0] >= 0 && entry->sizes[0] >= 0) {
            bulletproof_printf(fmt + 528,
                               lbl_8011B578[entry->typeids[0]],
                               entry->sizes[0]);
        }
    }
    bulletproof_printf(fmt + 540);
    bulletproof_printf(fmt + 568, mlmMemUsed);
    lbl_80344850 = 0;
}
#pragma opt_propagation reset

/* 0x80055F68 -- asynchronous world/model/atree/critter load state machine. */
#pragma dont_inline on
s32 fn_80055F68(s32 arg0, s32 arg1)
{
    register u8* table = (u8*)lbl_80257680;
    char name[264];
    volatile u8 unused[4];
    s32 type;
    s32 qty;
    s32 size;

    if (lbl_80343C30 < 0) {
        return -1;
    }
    if (arg1 < 0) {
        if (lbl_80343C30 != 0) {
            return 0;
        }
        return 1;
    }

    if (lbl_80343C30 == 12) goto state12;
    if (lbl_80343C30 >= 12) goto dispatch_high;
    if (lbl_80343C30 == 4) goto state4;
    if (lbl_80343C30 >= 4) goto dispatch_mid;
    if (lbl_80343C30 == 1) goto state1;
    if (lbl_80343C30 >= 1) goto dispatch_low;
    if (lbl_80343C30 >= 0) goto state0;
    goto invalid;

dispatch_low:
    if (lbl_80343C30 >= 3) goto state3;
    goto state2;

dispatch_mid:
    if (lbl_80343C30 == 10) goto state10;
    if (lbl_80343C30 >= 10) goto state11;
    if (lbl_80343C30 >= 6) goto invalid;
    goto state5;

dispatch_high:
    if (lbl_80343C30 == 21) goto state21;
    if (lbl_80343C30 >= 21) goto dispatch_very_high;
    if (lbl_80343C30 == 14) goto state14;
    if (lbl_80343C30 < 14) goto state13;
    if (lbl_80343C30 >= 20) goto state20;
    goto invalid;

dispatch_very_high:
    if (lbl_80343C30 == 100) goto invalid;
    if (lbl_80343C30 >= 100) goto invalid;
    if (lbl_80343C30 == 30) goto state30;
    goto invalid;

state0:
    WorldLoadModelDone(table + 0xAC);
    lbl_80343C30 = 1;
    goto done;

state1:
    if (WorldLoadModelStart() != 0) {
        lbl_80343C30 = arg1 != 0 ? 100 : 2;
        lbl_80344874 = pbLoad;
    }
    goto done;

state2:
    if (StartWorldLoad(arg1) != 0) {
        lbl_80343C30 = arg1 != 0 ? 100 : 3;
        lbl_80344874 = pbLoad;
    }
    goto done;

state3:
    if (StartLoadWorldAnim(table + 0xAC) != 0) {
        lbl_80343C30 = 4;
    } else {
        lbl_80343C30 = 5;
    }
    goto done;

state4:
    if (FinishLoadWorldAnim() != 0) {
        lbl_80343C30 = 5;
        lbl_80344874 = pbLoad;
    }
    goto done;

state5:
    lbl_80343C30 = 10;
state10:
    type = *(s32*)(table + offsetof(EnemyTypeRow, type) + lbl_80344870 * 4);
    arg1 = type;
    if (type >= 0) {
        qty = *(s32*)(table + offsetof(EnemyTypeRow, subtype) +
                      lbl_80344870 * 4);
        fn_80050DD8(name, arg1, qty);
        MBOX_BGLoadModelStart(name, (void*)lbl_802512B0[arg1]);
        lbl_80343C30 = 11;
    } else {
        lbl_80343C30 = 14;
    }
    goto done;

state11:
    if (MBOX_BGLoadModelDone() != 0) {
        lbl_80343C30 = arg1 != 0 ? 100 : 12;
        lbl_80344874 = pbLoad;
    }
    goto done;

state12:
    {
        type = *(s32*)(table + offsetof(EnemyTypeRow, type) +
                       lbl_80344870 * 4);
        if (gWadAtreeHeaders[type] != 0) {
            qty = *(s32*)(table + offsetof(EnemyTypeRow, subtype) +
                          lbl_80344870 * 4);
            fn_80050DD8(name, type, qty);
            size = FileSize(name, lbl_80346BF8);
            lbl_80344878 = StartFileRead(name, lbl_80346BF8, 0, size,
                                         gWadAtreeHeaders[type], fn_8005636C);
            lbl_80343C30 = 13;
        } else {
            lbl_80343C30 = 14;
        }
    }
    goto done;

state13:
    if (lbl_80344878[4] != 0) {
        EnemyTypeRow* entry;

        lbl_80344878[4] = -1;
        entry = (EnemyTypeRow*)table;
        entry = (EnemyTypeRow*)((u8*)entry + lbl_80344870 * 4);
        type = entry->type;
        fn_8001267C(gWadAtreeHeaders[type], (void*)lbl_802512B0[type], -1);
        lbl_80343C30 = arg1 != 0 ? 100 : 14;
        lbl_80344874 = pbLoad;
    }
    goto done;

state14:
    {
        s32 next;
        EnemyTypeRow* entry = (EnemyTypeRow*)table;

        entry = (EnemyTypeRow*)((u8*)entry + lbl_80344870 * 4);
        type = entry->type;
    if (type >= 0) {
        fn_80050910(type);
    }
        next = lbl_80344870 + 1;
        lbl_80344870 = next;
    if (next >= 8) {
        lbl_80343C30 = 20;
    } else {
        lbl_80343C30 = 10;
    }
    }
    goto done;

state20:
    if (CritterLoadStartNext() != 0) {
        lbl_80343C30 = 21;
    } else {
        lbl_80343C30 = 30;
    }
    goto done;

state21:
    if (CritterLoadDone(0) != 0) {
        lbl_80343C30 = arg1 != 0 ? 100 : 20;
        lbl_80344874 = pbLoad;
    }
    goto done;

state30:
    lbl_80343C30 = 100;
    goto done;

invalid:
    lbl_80343C30 = -1;
    return 1;
done:
    return 0;
}
#pragma dont_inline reset

/* 0x80056698 -- resolve a world/level then tally its memory footprint. */
s32 fn_80056698(s32 arg0, s32 arg1)
{
    s32* p;
    s32* q;
    s32 total = 0;

    ResolveWorldData(arg0);
    if (arg1 < 0) {
        init_next_level_8005638C(arg0);
        while (fn_80055F68(0, 0) == 0) {
            serve_busy(-1);
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
    Enemy* e = (Enemy*)((u8*)pool + 3608);   /* = gEnemies */
    f32 fv = lbl_80346820;
    s32 i;

    for (i = 0; i < 25; i++) {
        e->state = 0;
        e->objgrp.node = 0;
        e->objgrp.flags = 2;
        e->flooroffset = fv;
        e->atree.root = 0;
        e->shadow = 0;
        e++;
    }
    lbl_8034473C = (s32)MBNewNode(gSceneRoot, gIdentityMatrix, 1);
    gNumEnemies = gCurLevel->maxenemies;
    lbl_80344740 = 0;
    lbl_80344748 = -1;
    lbl_80344750 = -1;
    lbl_8034474C = 0;
    for (i = 0; i < 256; i++) {
        pool[390 + i] = 0;   /* +0x618 */
        pool[646 + i] = 0;   /* +0xA18 */
    }
}

/* 0x80051480 -- return the milestone nearest to a world-space position. */
s32 fn_80051480(f32* pos)
{
    u8 unused[16];
    s32 best_idx = -1;
    register f32 best_dist = 100000.0f;
    u8* node = sMilestones;
    s32 i;

    for (i = 0; i < sNumMilestones; i++, node += 104) {
        f32 d;
        f32 dx;
        f32 dy;
        f32 dz;

        dy = pos[1] - *(f32*)(node + 0x34);
        dx = pos[0] - *(f32*)(node + 0x30);
        dz = pos[2] - *(f32*)(node + 0x38);
        d = dx * dx + dy * dy;
        d = dz * dz + d;

        if (d > 0.0f) {
            volatile f32 tmp;
            f64 y = __frsqrte(d);
            y = 0.5 * y * (3.0 - y * y * d);
            y = 0.5 * y * (3.0 - y * y * d);
            y = 0.5 * y * (3.0 - y * y * d);
            tmp = (f32)(d * (0.5 * y * (3.0 - y * y * d)));
            d = tmp;
        }
        if (d < best_dist) {
            best_idx = i;
            best_dist = d;
        }
    }
    return best_idx;
}

/* 0x80051FDC -- resolve a level/enemy name to its type id (-1 on miss). */
s32 EnemyDescType(const char* name)
{
    u32 i;

    if (stricmp(name, sBossGenName) == 0) {
        s32 t = lbl_802577CC[0];
        if (t != gBossType) {
            return t;
        }
        return -1;
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
        AtreeAlloc(64, 64);
        break;
    case 0x4012:
    case 0x8009:
        AtreeAlloc(3584, 3072);
        break;
    default:
        AtreeAlloc(-1, -1);
        break;
    }
}

/* 0x80057B30 -- parse a "<letter><digit>" level tag to (realm<<8)|index. */
u32 FindWave(const s8* s)
{
    s32 realm = -1;
    s8 letter = toupper(s[0]);
    s32 i;

    for (i = 0; i < 14; i++) {
        if (letter == (s8)sWorldDataTypes[i].letter) {
            realm = sWorldDataTypes[i].type;
            break;
        }
    }
    if (realm < 0) {
        return -1;
    }
    return (realm << 8) | ((u32)((s32)(s8)s[1] - '1') & 0xFF);
}

typedef struct WorldTypeNav {
    s32 worldId;
    u8 _04[12];
    s32 loaded;
    s32 numLevels;
    u8 _18[16];
    s32 nextLevel;
} WorldTypeNav;

typedef struct WorldLevelTableNav {
    u8 _000[232];
    WorldTypeNav worlds[15];
} WorldLevelTableNav;

/* 0x80057C14 -- advance attract mode to the next loaded, playable wave. */
s32 NextAttractWave(s32 worldLevel)
{
    s32 worldType = worldLevel >> 8;
    s32 worldIndex;
    s32 tableOffset;
    s32 level;
    s32 worldId;
    s32 worldBits;
    s32 loaded;
    WorldLevelTableNav* worldTable = (WorldLevelTableNav*)sWorldLevelTable;

    for (worldIndex = 0; worldIndex < 14; worldIndex++) {
        if (worldType == worldTable->worlds[worldIndex].worldId) {
            break;
        }
    }
    if ((u32)worldIndex == 14) {
        worldIndex = 0;
    }
    do {
        s32 startIndex = worldIndex;

        do {
            worldIndex++;
            if ((u32)worldIndex >= 14) {
                worldIndex = 0;
            }
            tableOffset = worldIndex * 44;
            loaded = worldTable->worlds[worldIndex].loaded;
        } while (loaded == 0 && worldIndex != startIndex);

        level = worldTable->worlds[worldIndex].nextLevel;
        worldId = worldTable->worlds[worldIndex].worldId;
        if (level >= worldTable->worlds[worldIndex].numLevels) {
            level = 0;
        }
        worldBits = worldId << 8;
        ResolveWorldData((level & 0xFF) | worldBits);

        if ((gControllerButtons & 0x10) == 0) {
            s32 numLevels;
            WorldLevelNav* levels;
            s32 originalLevel;

            originalLevel = level;
            numLevels = worldTable->worlds[worldIndex].numLevels;
            levels = gWorldData->levels;

            while ((levels[level].flags2 & 2) == 0) {
                level++;
                if (level >= numLevels) {
                    level = 0;
                }
                if (level == originalLevel) {
                    break;
                }
            }
            if ((levels[level].flags2 & 2) == 0) {
                continue;
            }
        }
        worldIndex = (level & 0xFF) | worldBits;
        ResolveWorldData(worldIndex);
        level++;
        if (level >= *(s32*)((u8*)worldTable + tableOffset + 252)) {
            level = 0;
        }
        *(s32*)((u8*)worldTable + tableOffset + 272) = level;
        return worldIndex;
    } while (1);
}

/* 0x80057D94 -- move backward to a level accepted by waveMask, wrapping
 * through the loaded-world table when the current world is exhausted. */
static inline WorldLevelTableNav* PrevWorldEntry(WorldLevelTableNav* table,
                                                 s32 offset)
{
    return (WorldLevelTableNav*)((u8*)table + offset);
}

#pragma opt_propagation off
s32 PrevWorldLevel(s32 waveMask)
{
    register s32 currentWorld;
    WorldLevelTableNav* worldTable = (WorldLevelTableNav*)sWorldLevelTable;
    s32 worldIndex;
    s32 level;

    currentWorld = sCurWorldIndex;
    worldIndex = currentWorld;
    if (gWorldData == 0) {
        return gGameOptions[9];
    }
    if (waveMask == -1) {
        level = -1;
    } else {
        level = gWorldData->curLevel - 1;
        if (waveMask != 0) {
            while (level >= 0 &&
                   (waveMask & gWorldData->levels[level].flags2) == 0) {
                level--;
            }
        }
    }

    if (level < 0) {
        level = 0;
        for (;;) {
            register WorldLevelTableNav* entry;
            register s32 offset;

            worldIndex--;
            if (worldIndex < 0) {
                worldIndex = 13;
            }
            offset = worldIndex * 44;
            entry = PrevWorldEntry(worldTable, offset);
            if (entry->worlds[0].loaded != 0 || worldIndex == currentWorld) {
                break;
            }
        }
        if (worldTable->worlds[worldIndex].numLevels >= 0) {
            level = worldTable->worlds[worldIndex].numLevels - 1;
        }
    }
    return (worldTable->worlds[worldIndex].worldId << 8) |
           (level & 0xFF);
}
#pragma opt_propagation on

/* 0x80057E6C -- move forward to a level accepted by waveMask, wrapping
 * through the loaded-world table when the current world is exhausted. */
#pragma opt_propagation off
s32 NextWorldLevel(s32 waveMask)
{
    register s32 currentWorld;
    s32 worldIndex;
    s32 level;

    currentWorld = sCurWorldIndex;
    worldIndex = currentWorld;
    if (gWorldData == 0) {
        return gGameOptions[9];
    }
    if (waveMask == -1) {
        level = 99;
    } else {
        level = gWorldData->curLevel + 1;
        if (waveMask != 0) {
            while (level < gWorldData->numLevels &&
                   (waveMask & gWorldData->levels[level].flags2) == 0) {
                level++;
            }
        }
    }

    if (level >= gWorldData->numLevels) {
        level = 0;
        do {
            worldIndex++;
            if ((u32)worldIndex >= 14) {
                worldIndex = 0;
            }
        } while (sWorldDataTypes[worldIndex].available == 0 &&
                 worldIndex != currentWorld);
    }
    return (sWorldDataTypes[worldIndex].type << 8) | (level & 0xFF);
}
#pragma opt_propagation reset

/* 0x80057F44 - validate/normalize a world+level code, resolve it, and walk
 * forward until a level matching the wave mask is found */
extern s32 sCurWorldType;
extern char lbl_801129F8[];   /* "couldn't find world data" fatal fmt */

s32 fn_80057F44(s32 code, s32 mask)
{
    WorldDataType* types = sWorldDataTypes;
    s32 wt;
    s32 sub;
    u32 i;

    for (;;) {
        sub = code & 0xFF;
        wt = code;
        wt >>= 8;
        if (sCurWorldType != wt) {
            for (i = 0; i < 14; i++) {
                if (types[i].type == wt && types[i].available != 0) {
                    break;
                }
            }
            if (i == 14) {
                for (i = 0; i < 14; i++) {
                    if (types[i].available != 0) {
                        wt = types[i].type;
                        break;
                    }
                }
            }
            if (i == 14) {
                FatalError(lbl_801129F8, 0x800000);
            }
        } else {
            if (gWorldData != 0) {
                if (sub >= gWorldData->numLevels) {
                    sub = 0;
                }
            } else {
                sub = 0;
            }
        }
        sub = (u8)sub;
        sub = (sub & 0xFF) | (wt << 8);
        ResolveWorldData(sub);
        if (mask == 0) {
            break;
        }
        if (gCurLevel->enabled & mask) {
            break;
        }
        code = NextWorldLevel(mask);
    }
    return sub;
}

/* 0x80051C78 - rebuild the enemy path-milestone chain: clear the per-slot
 * milestone table, find the milestone closest to the default player start,
 * then walk fn_800511D0 from there appending each hop until it revisits a
 * listed milestone or stops advancing. */
extern s32 lbl_80344724;          /* count of milestones in the chain */
extern f32 lbl_803468B0;          /* big initial distance */
extern f64 __frsqrte(f64 x);
extern f32 lbl_80346984;
extern f32 gDefaultPlayerPosition[3];
extern s32 fn_800511D0(s32 milestone, f32 param);

typedef struct MilestonePool {
    u8 _000[0xF4];
    s32 slots[128];
} MilestonePool;

void fn_80051C78(void)
{
    MilestonePool* mp = (MilestonePool*)lbl_80250E00;
    u8 unused[16];
    s32 best;
    s32 cur;
    s32 i;

    for (i = 0; i < 128; i++) {
        mp->slots[i] = -1;
    }
    lbl_80344724 = 0;
    best = -1;

    {
        f32 bestDist = lbl_803468B0;
        u8* m = sMilestones;

        for (i = 0; i < sNumMilestones; i++, m += 0x68) {
            f32 dx = gDefaultPlayerPosition[0] - *(f32*)(m + 0x30);
            f32 dy = gDefaultPlayerPosition[1] - *(f32*)(m + 0x34);
            f32 dz = gDefaultPlayerPosition[2] - *(f32*)(m + 0x38);
            f32 d2 = dx * dx + dy * dy;

            d2 = dz * dz + d2;
            if (d2 > 0.0f) {
                volatile f32 tmp;
                f64 y = __frsqrte(d2);
                y = 0.5 * y * (3.0 - y * y * d2);
                y = 0.5 * y * (3.0 - y * y * d2);
                y = 0.5 * y * (3.0 - y * y * d2);
                tmp = (f32)(d2 * (0.5 * y * (3.0 - y * y * d2)));
                d2 = tmp;
            }
            if (d2 < bestDist) {
                best = i;
                bestDist = d2;
            }
        }
    }

    cur = best;
    for (;;) {
        s32 count = lbl_80344724;
        s32 prev = cur;
        s32 k;

        lbl_80344724 = count + 1;
        mp->slots[count] = cur;
        cur = fn_800511D0(cur, lbl_80346984);
        count = lbl_80344724;
        for (k = 0; k < count; k++) {
            if (mp->slots[k] == cur) {
                break;
            }
        }
        if (k < count) {
            break;
        }
        if (prev == cur) {
            break;
        }
    }
}

/* 0x800511D0 - from a given milestone, pick the next milestone whose
 * direction (relative to the source's facing) is within the tolerance,
 * preferring the nearest one (with a second-best fallback by |dy|). */
extern f32  atan2(f32 y, f32 x);
extern f64  lbl_80346840;          /* pi   */
extern f64  lbl_80346848;          /* 2pi  */
extern f64  lbl_80346850;          /* -pi  */
extern char lbl_80112518[];        /* "no next milestone" error fmt */
extern f32  get_yaw(f32* to, f32* from);
extern f64  lbl_803468B8;          /* 3.0 */

#pragma opt_common_subs off
#pragma opt_propagation off
s32 fn_800511D0(s32 arg0, f32 arg1)
{
    u8 unusedHi[12];
    f32 pos[3];
    f32 ad;
    volatile f32 tmp;
    f32 t1;
    f32 t2;
    f32 bestDist;
    f32 secondDist;
    f32 bestDy;
    f32 secondDy;
    f32 tolerance;
    f64 kThree;
    f64 kHalf;
    f32 kZero;
    f64 k2Pi;
    f64 kNegPi;
    f64 kPi;
    f32 base;
    s32 second;
    s32 milestone;
    u8* m;
    s32 i;
    s32 best;
    u8 unusedLo[28];

    bestDist = lbl_803468B0;
    best = -1;
    secondDist = bestDist;
    bestDy = bestDist;
    second = -1;
    secondDy = bestDist;
    tolerance = arg1;
    milestone = arg0;
    if (milestone < 0) {
        return milestone;
    }

    m = sMilestones + milestone * 104;
    pos[0] = *(f32*)(m + 48);
    pos[1] = *(f32*)(m + 52);
    pos[2] = *(f32*)(m + 56);
    {
        f32 x = *(f32*)(m + 40);
        f32 r = atan2(*(f32*)(m + 32), x);
        f64 p = lbl_80346840;
        f32 a = (f32)(p + r);
        f64 t;
        if (a > p) {
            t = a - lbl_80346848;
        } else if (a <= lbl_80346850) {
            t = lbl_80346848 + a;
        } else {
            t = a;
        }
        base = (f32)t;
    }

    kZero = lbl_80346820;
    kHalf = lbl_80346830;
    kThree = lbl_803468B8;
    kNegPi = lbl_80346850;
    k2Pi = lbl_80346848;
    kPi = lbl_80346840;
    {
        u8* m0 = sMilestones;
        m = m0;
    }
    for (i = 0; i < sNumMilestones; i++, m += 104) {
        f32 d;
        f64 nd;
        f32 dx;
        f32 dy;
        f32 dz;
        f32 dist;

        if (i == milestone) {
            continue;
        }
        d = get_yaw((f32*)(m + 48), pos) - base;
        if (d > kPi) {
            nd = d - k2Pi;
        } else if (d <= kNegPi) {
            nd = k2Pi + d;
        } else {
            nd = d;
        }
        ad = (f32)nd;
        *(u32*)&ad &= 0x7FFFFFFF;
        if (ad <= tolerance) {
            dy = *(f32*)(m + 52) - pos[1];
            dx = *(f32*)(m + 48) - pos[0];
            dz = *(f32*)(m + 56) - pos[2];
            dist = dx * dx + dy * dy;
            dist = dz * dz + dist;
            if (dist > kZero) {
                f64 y = __frsqrte(dist);
                y = kHalf * y * (kThree - y * y * dist);
                y = kHalf * y * (kThree - y * y * dist);
                y = kHalf * y * (kThree - y * y * dist);
                tmp = (f32)(dist * (kHalf * y * (kThree - y * y * dist)));
                dist = tmp;
            }
            if (dist < bestDist) {
                t1 = dy;
                secondDist = bestDist;
                second = best;
                secondDy = bestDy;
                *(u32*)&t1 &= 0x7FFFFFFF;
                bestDist = dist;
                best = i;
                bestDy = t1;
            } else if (dist < secondDist) {
                t2 = dy;
                secondDist = dist;
                second = i;
                *(u32*)&t2 &= 0x7FFFFFFF;
                secondDy = t2;
            }
        }
    }

    if (bestDy > secondDy) {
        best = second;
    }
    if (best < 0) {
        if ((gControllerButtons & 0x10) != 0) {
            ErrorPrintf(lbl_80112518, milestone);
        }
        best = milestone;
    }
    return best;
}
#pragma opt_propagation reset
#pragma opt_common_subs on

/* 0x80051E1C - format a world/level display name (uppercased) */
extern char lbl_80346A90[8];    /* "%s" fmt */
extern char lbl_80346A98[8];    /* "%s %c" fmt */
extern char lbl_80346AA0[8];    /* suffix */
extern char lbl_80343BF8[5];    /* level letter table */

char* fn_80051E1C(s32 world, s32 lvl, s32 flag)
{
    s32 n;
    u32 i;
    char* buf;

    buf = (char*)lbl_80250E00;
    if (lvl == 0) {
        n = 1;
    } else {
        n = lvl;
    }
    if (lvl >= 4) {
        goto chk8;
    }
    goto plain;
chk8:
    if (lvl >= 8) {
        goto plain;
    }
    goto lettered;
plain:
    sprintf(buf, lbl_80346A90, findWorldName(world));
    goto suffix;
lettered:
    sprintf(buf, lbl_80346A98, findWorldName(world), (&lbl_80343BF8[n])[-4]);
suffix:
    if (flag != 0) {
        strcat(buf, lbl_80346AA0);
    }
    for (i = 0; i < strlen(buf); i++) {
        buf[i] = toupper(buf[i]);
    }
    return buf;
}

/* 0x80051568 - find the nearest live idle item for an enemy (grid scan) */
extern f64 lbl_80346A88;        /* health floor */
extern f32 lbl_803468B0;        /* big initial distance */
extern f32 lbl_803469D4;        /* grid radius */
extern f32 lbl_80346820;        /* 0.0f */
extern f64 lbl_803468B8;        /* 3.0 */
extern f64 __frsqrte(f64 x);
extern void StartEnemyGrid(f32* pos, f32 radius);
extern s32 NextGridEnemy(void);

void fn_80051568(s32 index)
{
    u8* e = (u8*)gEnemies + index * 916;
    s32 i;
    Item* it;
    iteminfo* hdr;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 dist2;
    f64 kHalf;
    f32 kZero;
    f64 kThree;
    u8 _spare[24];
    u8 unused[8];

    if (*(s16*)(e + offsetof(Enemy, closest)) >= 0 &&
        *(f32*)(e + offsetof(Enemy, actual_dist)) <= lbl_80346A88) {
        *(s32*)(e + offsetof(Enemy, guard_mode)) = 0;
        *(s32*)(e + offsetof(Enemy, guard_closest)) = -1;
        *(f32*)(e + offsetof(Enemy, guard_dist)) = lbl_803468B0;
        return;
    }
    if (*(s32*)(e + offsetof(Enemy, guard_mode)) != 0) {
        return;
    }
    StartEnemyGrid((f32*)(e + offsetof(Enemy, objgrp.worldmat[3])), lbl_803469D4);
    kZero = lbl_80346820;
    kHalf = lbl_80346830;
    kThree = lbl_803468B8;
    while ((i = NextGridEnemy()) >= 0) {
        it = &sItems[i];
        hdr = it->info;
        if (it->active == -1) {
            continue;
        }
        if (hdr->type != 2) {
            continue;
        }
        if (it->minoff != 0) {
            continue;
        }
        dx = it->objgrp.worldmat[3][0] - *(f32*)(e + offsetof(Enemy, objgrp.worldmat[3][0]));
        dy = it->objgrp.worldmat[3][1] - *(f32*)(e + offsetof(Enemy, objgrp.worldmat[3][1]));
        dz = it->objgrp.worldmat[3][2] - *(f32*)(e + offsetof(Enemy, objgrp.worldmat[3][2]));
        dist2 = dx * dx + dy * dy + dz * dz;
        if (dist2 > kZero) {
            volatile f32 tmp;
            f64 y = __frsqrte(dist2);
            y = kHalf * y * (kThree - y * y * dist2);
            y = kHalf * y * (kThree - y * y * dist2);
            y = kHalf * y * (kThree - y * y * dist2);
            dist2 = (f32)(dist2 * (kHalf * y * (kThree - y * y * dist2)));
            tmp = dist2;
            dist2 = tmp;
        }
        if (dist2 < *(f32*)(e + offsetof(Enemy, guard_dist))) {
            *(f32*)(e + offsetof(Enemy, guard_dist)) = dist2;
            *(s32*)(e + offsetof(Enemy, guard_closest)) = i;
            *(s32*)(e + offsetof(Enemy, guard_mode)) = 1;
        }
    }
}

/* 0x800516F8 -- pick the closest eligible player target for an enemy. */
extern s32 lbl_80344B24;           /* forced "it" player index */
extern f64 lbl_80346870;
extern f64 lbl_80346868;

#define DIST3(dst, av, bv, kZ, kH, kT)     {         f32 dy_ = (av)[1] - (bv)[1];         f32 dx_ = (av)[0] - (bv)[0];         f32 dz_ = (av)[2] - (bv)[2];         (dst) = dx_ * dx_ + dy_ * dy_;         (dst) = dz_ * dz_ + (dst);         if ((dst) > (kZ)) {             volatile f32 tmp_;             f64 y_ = __frsqrte((dst));             y_ = (kH) * y_ * ((kT) - y_ * y_ * (dst));             y_ = (kH) * y_ * ((kT) - y_ * y_ * (dst));             y_ = (kH) * y_ * ((kT) - y_ * y_ * (dst));             tmp_ = (f32)((dst) * ((kH) * y_ * ((kT) - y_ * y_ * (dst))));             (dst) = tmp_;         }     }

void fn_800516F8(s32 slot)
{
    u8 unused[44];
    f32 ad;
    u8* p;
    u8* e;
    s32 i;
    s32 t;
    f64 kK;
    f64 kThree;
    f64 kHalf;
    f32 kZero;
    f32 dist;
    f64 kPi;
    f32 range;
    f32 bestSpecial;
    u8 padLo[4];

    e = (u8*)gEnemies + slot * 916;
    bestSpecial = lbl_803468B0;

    for (i = 0, p = (u8*)gPlayers; i < 4; i++, p += 13148) {
        if (*(s32*)(p + offsetof(Player, state)) == 1) {
            break;
        }
    }
    if (i >= 4) {
        *(s16*)(e + offsetof(Enemy, recognized)) = 0;
    }

    t = lbl_80344B24;
    if (t >= 0 && gPlayers[t].state == 1 &&
        !(gPlayers[t].flags & 4) &&
        !(*(s32*)e == 30 && (gPlayers[t].shield_flags & 0x80000))) {
        u8* q;
        *(s16*)(e + offsetof(Enemy, prev_closest)) = *(s16*)(e + offsetof(Enemy, closest));
        *(s16*)(e + offsetof(Enemy, closest)) = (s16)lbl_80344B24;
        q = (u8*)gPlayers + lbl_80344B24 * 13148;
        {
            f32 fd;
            if (*(s16*)(q + offsetof(Player, field_A1C)) > 2) {
                DIST3(fd, (f32*)(e + offsetof(Enemy, objgrp) + offsetof(OBJGRP, coll_pos)), (f32*)(q + 2564),
                      lbl_80346820, lbl_80346830, lbl_803468B8);
            } else {
                DIST3(fd, (f32*)(e + offsetof(Enemy, objgrp) + offsetof(OBJGRP, coll_pos)), (f32*)(q + offsetof(Player, effectpos)),
                      lbl_80346820, lbl_80346830, lbl_803468B8);
            }
            *(f32*)(e + offsetof(Enemy, actual_dist)) = fd;
        }
        *(f32*)(e + offsetof(Enemy, close_dist)) = *(f32*)(e + offsetof(Enemy, actual_dist)) +
                           *(f32*)((u8*)gPlayers + lbl_80344B24 * 13148 + 2600);
    } else {
        s32 go = 1;
        s32 cur;
        if ((lbl_80344800 & 7) != (slot & 7) && *(s16*)(e + offsetof(Enemy, closest)) >= 0) {
            go = 0;
        }
        cur = *(s16*)(e + offsetof(Enemy, closest));
        if ((s16)cur >= 0 &&
            gPlayers[cur].state != 1) {
            go = -1;
        }
        if (go != 0) {
            f32 big;
            *(s16*)(e + offsetof(Enemy, prev_closest)) = (s16)cur;
            *(s16*)(e + offsetof(Enemy, closest)) = -1;
            big = lbl_803468B0;
            *(f32*)(e + offsetof(Enemy, close_dist)) = big;
            *(f32*)(e + offsetof(Enemy, actual_dist)) = big;
            if (*(s32*)e == 30) {
                *(s32*)(e + offsetof(Enemy, counter2)) = -1;
            }
            kPi = lbl_80346840;
            kK = lbl_80346870;
            kZero = lbl_80346820;
            kHalf = lbl_80346830;
            kThree = lbl_803468B8;
            {
                for (; i < 4; i++, p += 13148) {
                    if (*(s32*)(p + offsetof(Player, state)) != 1) {
                        continue;
                    }
                    if (*(u32*)(p + offsetof(Player, flags)) & 4) {
                        continue;
                    }
                    if (*(s16*)(p + offsetof(Player, field_A1C)) > 2) {
                        DIST3(dist, (f32*)(e + offsetof(Enemy, objgrp) + offsetof(OBJGRP, coll_pos)), (f32*)(p + 2564),
                              kZero, kHalf, kThree);
                    } else {
                        DIST3(dist, (f32*)(e + offsetof(Enemy, objgrp) + offsetof(OBJGRP, coll_pos)), (f32*)(p + offsetof(Player, effectpos)),
                              kZero, kHalf, kThree);
                    }
                    range = dist;
                    if (range > *(f32*)(e + offsetof(Enemy, sight))) {
                        continue;
                    }
                    if (*(s32*)e == 30 && (*(u32*)(p + offsetof(Player, shield_flags)) & 0x80000)) {
                        if (range < bestSpecial) {
                            bestSpecial = range;
                            *(s32*)(e + offsetof(Enemy, counter2)) = i;
                        }
                        continue;
                    }
                    if (range > kK * *(f32*)(e + offsetof(Enemy, rad))) {
                        range += *(f32*)(p + 2600);
                    }
                    if (!(range < *(f32*)(e + offsetof(Enemy, close_dist)))) {
                        continue;
                    }
                    if (*(f32*)(e + offsetof(Enemy, view)) < kPi) {
                        ad = get_yaw((f32*)(p + offsetof(Player, effectpos)), (f32*)(e + offsetof(Enemy, objgrp) + offsetof(OBJGRP, coll_pos))) -
                             *(f32*)(e + offsetof(Enemy, pyr) + 4);
                        *(u32*)&ad &= 0x7FFFFFFF;
                        if (ad > *(f32*)(e + offsetof(Enemy, view))) {
                            continue;
                        }
                    }
                    *(f32*)(e + offsetof(Enemy, close_dist)) = range;
                    *(f32*)(e + offsetof(Enemy, actual_dist)) = dist;
                    *(s16*)(e + offsetof(Enemy, closest)) = (s16)i;
                }
            }
        }
    }

    if (*(s16*)(e + offsetof(Enemy, closest)) >= 0) {
        if (*(f32*)(e + offsetof(Enemy, actual_dist)) <= *(f32*)(e + offsetof(Enemy, sight))) {
            Player* base;
            *(s16*)(e + offsetof(Enemy, recognized)) = 1;
            base = gPlayers;
            (*(s32*)((u8*)base + *(s16*)(e + offsetof(Enemy, closest)) * 13148 + 2596))++;
            {
                u8* r = (u8*)base + *(s16*)(e + offsetof(Enemy, closest)) * 13148;
                *(f32*)(r + 2600) = (f32)(*(f32*)(r + 2600) + lbl_80346868);
            }
        }
    } else {
        f32 big = lbl_803468B0;
        *(f32*)(e + offsetof(Enemy, actual_dist)) = big;
        *(f32*)(e + offsetof(Enemy, close_dist)) = big;
    }
}

extern s32 lbl_803447CC;
extern s32 lbl_803447E8;
extern s32 lbl_80344780;
extern s32 ShowMilestones(s32 idx);
extern s32 msgPost();

/* 0x80055AFC -- milestone blink cycle: flash the milestone markers while the
 * party is idle at a boss gate. */
void fn_80055AFC(void)
{
    s32 i;
    u8* ms;
    s32 n;
    s32 limit;
    u8 _spare[32];

    if (ShowMilestones(-1) != 0) {
        return;
    }
    if (lbl_803447CC < 1200) {
        return;
    }
    switch (lbl_803447E8) {
    case 0:
        limit = 600;
        break;
    default:
        limit = 420;
        break;
    }
    n = 0;
    for (i = 0; i < 4; i++) {
        if (gPlayers[i].state == 1) {
            break;
        }
        n++;
    }
    if (n == 4) {
        lbl_803447F4 = limit;
        lbl_803447E4 = 1;
    }
    if (gGameBusy != 0) {
        return;
    }
    if (gBossType >= 0) {
        return;
    }
    if (lbl_803447E4 != 0) {
        lbl_803447EC = 0;
    }
    n = lbl_803447F4 + gFrameTicks;
    lbl_803447F4 = n;
    if (n < limit) {
        return;
    }
    if (lbl_803447EC != 0) {
        ms = sMilestones;
        i = 0;
        while (i < sNumMilestones) {
            u8* mp = ms + i * 104;
            MBTreeClearFlags(*(void**)(mp + 96), 2, 0);
            i++;
        }
        if (sNumMilestones > 0 && lbl_80344780 == 0) {
            msgPost(29, -1, 0);
        }
        lbl_803447E8 = lbl_803447E8 + 1;
    } else {
        ms = sMilestones;
        i = 0;
        while (i < sNumMilestones) {
            u8* mp = ms + i * 104;
            MBTreeSetFlags(*(void**)(mp + 96), 2, 0);
            i++;
        }
    }
    lbl_80344780 = lbl_803447EC;
    lbl_803447EC = 1;
    lbl_803447F4 = lbl_803447F4 - limit;
}

extern s32 lbl_803447A8[2];        /* meter blit handles */
extern Item* sSpecialItem10;
extern s32 lbl_80344790;
extern s32 lbl_8034478C;
extern f32 lbl_80346AFC;           /* 0.0f */
extern f64 lbl_80346B90;           /* 0.5 */
extern f64 lbl_80346B98;           /* 3.0 */
extern f64 lbl_80346BA0;
extern f32 lbl_80343C08;
extern f64 lbl_80346B10;
extern f64 lbl_80346BA8;
extern f64 lbl_80346BB0;
extern f64 lbl_80346BB8;
extern f32 lbl_80346B20;
extern f64 lbl_80346BC8;
extern f64 lbl_80346BC0;
extern f64 lbl_80346B38;
extern f32 lbl_80346BD0;
extern f64 lbl_80346BD8;
extern char lbl_80112770[];
extern char lbl_8011277C[];
extern s32 mbBlitReset33F8(void* blit);
extern s32 PlayerHasShard(s32 player, s32 shard);
extern s32 PlayerHasRune(s32 player, s32 rune);
extern s32 GetWorldOrder(s32 world);
extern void fn_8009FF54(f32* pos);
extern void fn_8009FFA4(f32* pos);

/* 0x80055678 -- update the special-item proximity meter blits from the
 * distance between the two given points. */
#pragma opt_propagation off
void fn_80055678(f32* a, f32* b)
{
    f32 d;
    f32 v;
    f64 lvl2;
    f32 lvl;
    f64 scale;
    f64 base;
    f32 f6;
    f32 x;
    f64 t;
    f32 dx;
    f32 dy;
    f32 dz;

    if (mbBlitReset33F8((void*)lbl_803447A8[0]) != 0 || sSpecialItem10 == 0) {
        mbBlitInit3414((void*)lbl_803447A8[0], 1);
        mbBlitInit3414((void*)lbl_803447A8[1], 1);
    } else {
        dx = a[0] - b[0];
        dy = a[1] - b[1];
        dz = a[2] - b[2];
        d = dx * dx + dy * dy + dz * dz;
        if (d > lbl_80346AFC) {
            volatile f32 tmp[3];
            f64 y = __frsqrte(d);
            y = lbl_80346B90 * y * (lbl_80346B98 - y * y * d);
            y = lbl_80346B90 * y * (lbl_80346B98 - y * y * d);
            y = lbl_80346B90 * y * (lbl_80346B98 - y * y * d);
            d = (f32)(d * (lbl_80346B90 * y * (lbl_80346B98 - y * y * d)));
            tmp[0] = d;
            d = tmp[0];
        }
        v = (f32)(d - lbl_80346BA0);
        v = v * lbl_80343C08;
        if (v < lbl_80346AFC) {
            t = lbl_80346B10;
        } else if (v > lbl_80346BA8) {
            t = lbl_80346BA8;
        } else {
            t = v;
        }
        x = (f32)t;
        lvl = (f32)(lbl_80346BA8 - (lbl_80346BA8 - x) * (lbl_80346BA8 - x));
        if (lbl_80344790 == 0 && lvl < lbl_80346BB0) {
            lbl_80344790 = 1;
            fn_8009FF54(b);
        } else if (lbl_8034478C == 0 && lvl < lbl_80346BB8) {
            lbl_8034478C = 1;
            fn_8009FFA4(b);
        }
        scale = lbl_80346BC8;
        base = lbl_80346BC0;
        lvl2 = scale * (f64)(f6 = (f32)(lbl_80346BA8 - lvl));
        t = base - lvl2;
        mbBlitSetupVerts((void*)lbl_803447A8[1], lbl_80346B20, lbl_80346B20,
                         (f32)(t * lbl_80346B38),
                         lbl_80346B20);
        mbBlitProject((void*)lbl_803447A8[1], 0, Round((f32)lvl2) + 27);
        mbBlitCalcY((void*)lbl_803447A8[1], 102 - Round((f32)lvl2));
    }
}
#pragma opt_propagation on

#pragma opt_propagation off
void init_thermometer(void)
{
    s32 playerOffset;
    s32* blits;
    s32 player;
    u8* playerData;
    s32 enabled;
    u8* players;
    u32 texture;
    f32 length;
    f32 x;
    f32 yCoord;
    f32 z;
    volatile f32 tmp[3];

    enabled = 1;
    playerOffset = 0;
    lbl_80344790 = lbl_8034478C = playerOffset;
    if ((gGameMode & 0x4000) != 0 && sSpecialItem10 != 0) {
        players = (u8*)gPlayers;
        player = 0;
        do {
            playerData = players + playerOffset;
            if (PlayerHasShard(player, sSpecialItem10->info->item.value) != 0) {
                enabled = 1;
                break;
            }
            if (sMusicTrackHi == 8) {
                s32 charIdx = ((Player*)playerData)->character;
                if ((*(u8*)(playerData + 7384 + charIdx * 14) & 4) != 0) {
                    enabled = 0;
                }
            } else if (PlayerHasRune(player, GetWorldOrder(5)) != 0) {
                enabled = 0;
            }
            player++;
            playerOffset += 13148;
        } while (player < 4);
    }

    lbl_803447A8[0] = (s32)MBCreateBlit(0, 0, 392, -1, -1, -1);
    *(blits = &lbl_803447A8[1]) = (s32)MBCreateBlit(0, 0, 392, -1, -1, -1);
    mbBlitCvtCoord((void*)lbl_803447A8[0], lbl_80346B78);
    mbBlitCvtCoord((void*)*blits, lbl_80346B74);
    mbBlitInit3414((void*)lbl_803447A8[0], enabled);
    mbBlitInit3414((void*)*blits, enabled);
    texture = MBOX_FindTexture_Err(lbl_80112770, 0, 1);
    mbInitBlitEntry((void*)lbl_803447A8[0], texture, 0);
    mbInitBlitEntry((void*)*blits, MBOX_FindTexture_Err(lbl_8011277C, 0, 1), 0);
    mbBlitSetupVerts((void*)*blits, lbl_80346B20, lbl_80346B20,
                     lbl_80346BD0, lbl_80346AF0);
    mbBlitProject((void*)*blits, 0, 27);

    x = gWorldInfo.worldsize[0];
    yCoord = gWorldInfo.worldsize[1];
    z = gWorldInfo.worldsize[2];
    length = x * x + yCoord * yCoord + z * z;
    if (length > lbl_80346AFC) {
        f64 y = __frsqrte(length);
        y = lbl_80346B90 * y * (lbl_80346B98 - y * y * length);
        y = lbl_80346B90 * y * (lbl_80346B98 - y * y * length);
        y = lbl_80346B90 * y * (lbl_80346B98 - y * y * length);
        length = (f32)(length * (lbl_80346B90 * y * (lbl_80346B98 - y * y * length)));
        tmp[0] = length;
        length = tmp[0];
    }
    lbl_80343C08 = (f32)(lbl_80346BA8 / (lbl_80346BD8 * length));
}
#pragma opt_propagation on

/* 0x8005351C -- world/level entry orchestrator (only caller: game_main). */
extern s32  opt_restart_request;
extern void AudioReset(s32 force);
extern void fn_800BC4E4(void);
extern s32  good_wiz_exit_timer;
extern s32  lbl_80344808;
extern s32  lbl_80344804;
extern s32  lbl_803447C8;
extern s32  lbl_803447C4;
extern s32  lbl_80343C04;
extern s32  lbl_80343C00;
extern s32  sMusicTrackLo;
extern s32  lbl_803448B4;
extern s32  lbl_803448B0;
extern void SetScrollLevelMsgList(s32 mode, void* list);
extern void sumnerUpdatePresence(void);
void fn_80057024(void);
extern void SetupDynGrid(void);
extern void CreateDynobjGrid(void);
extern void player_store_in_save(u8* pl);
extern void PlayerRestoreState(s32 player);
extern void EnterTower(void);
extern void InitCamera(s32 mode);
extern u32  lbl_80344824;
extern void load_player(s32 player);
extern void add_target(void* mat);
extern void LoadPlyrData(s32 player, s32 pad, s32 mode);
extern void CopyMat3(f32* src, f32* dst);
extern f32  lbl_80257650[];
extern void UpdatePlayerWorldMat(void* player, s32 force);
extern void setup_player_display(s32 player);
extern void PlayerSaveState(s32 player, s32 mode);
extern void camera_mode_level(s32 mode);
extern s32  lbl_803447D0;
extern void LoadAllRecords(void);
extern void BGMusicStart(void);
extern void SetPlayerWindows(s32 mode);
extern void fn_8006F16C(s32 arg0);
extern void fn_8005B988(void);
extern void do_enemies(void);
extern void AudioMusicVolUpdate(void);
extern s32  welcome_timer;

typedef struct PlayerSaveBlk {
    s32 w[1293];                    /* 5172 bytes */
} PlayerSaveBlk;

void fn_8005351C(void)
{
    u8 unused[8];
    s32 t = 0;
    s32 state = lbl_8034481C;
    s32 inTower;
    s32 isSelect;
    s32 off;
    u8* tbl;
    f32* idmat;
    s32 i;
    u8* p;

    if (state >= 13 && state < 0x10000) {
        t = 1;
    }
    if (t != 0) {
        inTower = 1;
    } else {
        inTower = 0;
    }
    if (state == 2) {
        isSelect = 1;
    } else {
        isSelect = 0;
    }

    opt_restart_request = 0;
    AudioReset(0);
    fn_800BC4E4();
    gGameMode = 0x400C;
    gGameBusy = 0;
    good_wiz_exit_timer = 0;
    lbl_80344808 = 0;
    lbl_80344804 = 0;
    lbl_803447E8 = 0;
    lbl_80344780 = 0;
    lbl_803447D4 = lbl_80346AF0;
    lbl_803447D8 = lbl_80346AF0;
    lbl_803447C8 = 0;
    lbl_803447C4 = 0;

    if (lbl_80343C10 < 0) {
        next_world();
        if (lbl_80343C04 != sLastWorldLevel) {
            lbl_80343C00 = -1;
        }
        lbl_8034481C = 0;
        fn_80053D08(sLastWorldLevel, 1, lbl_80343C00);
        fn_80053C70();
    } else {
        fn_80053D08(sWorldDataConst, 1, lbl_80343C10);
        fn_80053C70();
    }

    InitLighting(1);
    lbl_803448B4 = sMusicTrackHi;
    lbl_803448B0 = sMusicTrackLo;
    lbl_80343C00 = -1;
    SetScrollLevelMsgList(0, gCurLevel->name);
    {
        Enemy* e = gEnemies;
        for (i = 0; i < 25; i++, e++) {
            e->state = 0;
            e->objgrp.node = 0;
        }
    }
    sumnerUpdatePresence();
    fn_80057024();
    SetupDynGrid();
    CreateDynobjGrid();

    if (sMusicTrackHi == 13) {
        s32 one = 1;
        for (i = 0, p = (u8*)gPlayers; i < 4; i++, p += 13148) {
            Player* player = (Player*)p;
            s32 st = player->state;
            if (st == 1) {
                player_store_in_save(p);
            } else if (st == 11) {
                PlayerRestoreState(i);
                player->state = one;
            }
        }
        EnterTower();
    }

    if (sMusicTrackHi != 12 && inTower == 0) {
        lbl_803447CC = 0;
    }

    InitCamera(0);
    {
        idmat = (f32*)gIdentityMatrix;
        tbl = (u8*)lbl_80257650;
        for (i = 0, off = 0, p = (u8*)gPlayers; i < 4; i++, off += 12, p += 13148) {
            ((Player*)p)->exit_dest = sLastWorldLevel;
            ((Player*)p)->node = 0;
            ((Player*)p)->platform = 0;
            if ((lbl_80344824 & (1 << i)) && ((Player*)p)->state != 11) {
                Player* player = (Player*)p;
                player->state = 1;
                load_player(i);
                add_target(player->mat);
                LoadPlyrData(i, player->character, 1);
                if (isSelect != 0) {
                    f32* v;
                    CopyMat3(idmat, player->mat);
                    v = (f32*)(tbl + off);
                    player->pos[0] = v[0];
                    player->pos[1] = v[1];
                    player->pos[2] = v[2];
                    UpdatePlayerWorldMat(player, 0);
                }
            }
            setup_player_display(i);
        }
    }

    if (inTower == 0) {
        s32 off2;
        u8* base = (u8*)gPlayers;
        for (i = 0, off2 = 0; i < 4; i++, off2 += 13148) {
            u8* q = base + off2;
            Player* player = (Player*)q;
            if (player->state != 0) {
                if (sMusicTrackHi == 13) {
                    PlayerSaveState(i, 0);
                } else if (sMusicTrackHi != 12) {
                    PlayerSaveState(i, 1);
                }
                if (player->exp == 0) {
                    player->exp = 1;
                    *(s8*)(q + 2699) = 0;
                }
            }
        }
        camera_mode_level(0);
        lbl_803447D0 = 0;
        fn_80051C78();
    } else {
        LoadAllRecords();
        lbl_80344B84 = -1;
    }

    BGMusicStart();
    SetPlayerWindows(0);
    fn_8006F16C(inTower);
    fn_8005B988();
    do_enemies();
    fn_800553B4();
    init_thermometer();
    AudioMusicVolUpdate();
    {
        s32 mt = sMusicTrackHi;
        if (mt != 12) {
            welcome_timer = 300;
        }
        if (mt == 13) {
            for (i = 0, p = (u8*)gPlayers; i < 4; i++, p += 13148) {
                if (((Player*)p)->state == 1) {
                    *(PlayerSaveBlk*)(p + 7884) = *(PlayerSaveBlk*)(p + 2688);
                }
            }
        }
    }
}

/* 0x80054E78 -- per-frame level-timer / thermometer HUD update. */
extern s32  lbl_803447B8;
extern s32  gGameplayPauseTimer;
extern f32  gClockFrameStep;
extern f32  lbl_80346B08;
extern f64  lbl_80346B18;
extern f64  lbl_80346B28;
extern f64  lbl_80346B30;
extern f64  lbl_80346B40;
extern f64  lbl_80346B48;
extern f64  lbl_80346B50;
DECL_SECT(".sdata2") extern const char lbl_80346B58[];
extern void MBRemoveBlit(s32 blit);
extern void AudioFootstep(s32 n);
extern void fn_8009FA84(void);
extern void fn_8009FCA8(s32 n);
extern void init_got_it(void);
extern void DrawText(s32 x, s32 y, s32 flags, s32 color, ...);

extern s32  opt_quit_request;
extern s32  lbl_803441FC;
extern s32  lbl_80344794;
extern s32  lbl_80344C18;
extern s32  gScriptedCameraState;
extern f32  gCameras[];
extern f32  lbl_8025EA04[];
extern void* lbl_80344EA8;
extern char lbl_80346AB0[8];
extern s32  lbl_80344788;
extern u8   lbl_80344798;
extern u32  lbl_803448D0;
extern s32  lbl_803448CC;

extern s32  OptionsDone(void);
extern void AudioSelectReset(void);
extern void fn_8009D34C(void);
extern void TriggerCameraEnd(void);
extern void AudioClearInputFlag(void);
extern void init_targets(void);
extern void end_all_optmenus(void);
extern void FireScrollReset(void);
extern void TowerInit(void);
extern void fn_800520C8(void);
extern void enemy_update(void);
extern void do_flyby(void);
extern void do_credits(void);
extern void do_screen2d(void);
extern void do_movie(void);
extern void do_titlescreen(void);
extern void check_prefs_loaded(void);
extern void init_titlescreen(void);
extern void ProcessEffects(void);
extern void fn_80055AFC(void);
extern void SetPlayerVars(void);
extern void fn_8009D530(void);
extern void abort_player(s32 player);
extern void kill_player(s32 player);
extern void init_player_select(s32 mode);
extern void init_shop(s32 mode);
extern s32  init_gamemovie(s32 mode);
extern void WritePlayerInfo(s32 player);
extern s32  fn_80055F68(s32 a, s32 b);
extern s32  FileSystemBusy(void);
extern s32  new_start(s32 a);
extern s32  assigned_controller(s32 a);
extern void assign_controller(s32 a);
extern s32  ExitAttract(void);
extern s32  do_players(void);
extern s32  do_player_select(void);
extern s32  do_mapscreen(s32 mode);
extern s32  do_gamemovie(void);
extern s32  do_shop(void);
extern s32  check_active_players(void);
extern s32  sndFxUpdate(s32 a);
extern void fn_8009D610(s32 a, s32 b);
extern s32  next_world(void);
extern s32  init_mapscreen(s32 a, s32 b);
extern void towerRecordLevelBeaten(u32 a, s32 b);
extern s32  pbDiagDrawMenu(void);
extern void fn_80054E78(void);

/* 0x80054230 - top-level per-frame game mode dispatcher. */
#pragma dont_inline on
void game_main(void)
{
    u8 unused[8];
    u8 unused2[8];
    s32 i;
    s32 reset_player;
    s32 cond;
    s32 flag;
    s32 flag2;
    s32 lvl;
    s32 next;
    s32 all;
    s32 v;
    s32 c;
    char* strs = lbl_80112538;

    lbl_80344800++;
    if (gGameMode & 0x4000) {
        for (i = 0; i < 4; i++) {
            if (lbl_80257640[i] > 120) {
                lbl_80344A2C = 1;
            }
        }
    }
    if (opt_quit_request && OptionsDone()) {
        i = 0;
        opt_quit_request = i;
        gGameMode = 0x4014;
        gGameBusy = i;
        AudioStopSelect();
        AudioSelectReset();
        fn_8009D34C();
        lbl_80344774 = i;
        lbl_80344778 = 240;
        for (; i < 4; i++) {
            abort_player(i);
        }
        lbl_80344824 = 0;
        TriggerCameraEnd();
    }
    if (opt_restart_request) {
        if (gGameMode == 0x4010 && sMusicTrackHi != 13) {
            if (OptionsDone()) {
                for (i = 0; i < 4; i++) {
                    kill_player(i);
                }
            }
        } else {
            opt_restart_request = 0;
        }
    }
    if (gGameMode & 0x8000) {
        if (gControllerButtons & 4) {
            if (!assigned_controller(0)) {
                assign_controller(0);
            }
        }
    }
    if (lbl_80344A2C && gGameMode != 0x400e) {
        while (!fn_80055F68(0, 1)) {
            serve_busy(-1);
        }
        while (!MBOX_BGLoadModelDone()) {
            serve_busy(-1);
        }
        while (FileSystemBusy()) {
            serve_busy(-1);
        }
        while (AudioSysUpdate(10)) {
            serve_busy(-1);
        }
        while (FileSystemBusy()) {
            serve_busy(-1);
        }
        bulletproof_printf(strs + 0x1e4);
        AudioStopSelect();
        AudioSelectReset();
        bulletproof_printf(strs + 0x1f0);
        v = 0;
        alpha = v;
        fn_800520C8();
        AudioClearInputFlag();
        init_targets();
        lbl_80344A2C = v;
        options_state = v;
        end_all_optmenus();
        FireScrollReset();
        TriggerCameraEnd();
        TowerInit();
        for (reset_player = 0; reset_player < 4; reset_player++) {
            abort_player(reset_player);
        }
        bulletproof_printf(strs + 0x200);
        fn_80053D08(-1, 0, -1);
        bulletproof_printf(strs + 0x210);
        i = -1;
        lbl_80343C10 = i;
        lbl_80343DD4 = i;
        lbl_80343B38 = i;
        AudioStopSelect();
        lbl_803448AC = i;
        lbl_803448A8 = i;
        while (!MBOX_BGLoadModelDone()) {
        }
        init_attract_mode(0x8009);
    }
    cond = 0;
    c = lbl_8034481C;
    if (c >= 13 && c < 0x10000) {
        cond = 1;
    }
    flag = cond ? 1 : 0;
    SetPlayerVars();
    if (lbl_803447D8 > lbl_803447D4 && gGameMode == 0x4010) {
        fn_8009D530();
    }
    c = gGameMode;
    switch (c) {
    default:
        if (c >= 0x8000) {
            switch (c) {
            default:
            case 0x8000:
            case 0x8007:
                do_credits();
                goto attract_tail;
            case 0x8004:
            case 0x8005:
                if (!lbl_80344798) {
                    lbl_80344798 = 1;
                    check_prefs_loaded();
                }
                do_screen2d();
                goto attract_tail;
            case 0x8001:
            case 0x8002:
                do_movie();
                goto attract_tail;
            case 0x8009:
                do_titlescreen();
                goto attract_tail;
            case 0x8003:
            case 0x8006:
            case 0x8008:
                world_update();
                fn_8005B988();
                do_enemies();
                enemy_update();
                do_flyby();
            attract_tail:
        if (gGameMode != 0x8009 && gGameMode != 0x400b && lbl_803441FC > 1) {
            v = new_start(-1);
            if (gGameMode & 0x8000) {
                if (gControllerButtons & 4) {
                    if (assigned_controller(0)) {
                        v = 1;
                    }
                }
            }
            if (v) {
                lbl_80344794 = 1;
            }
        }
                if (lbl_80344794) {
                    if (ExitAttract()) {
                        lbl_80344794 = 0;
                        init_titlescreen();
                    }
                }
                break;
            }
        }
        break;
    case 0x400b:
        if (!(gGameOptions[11] & 1)) {
            WritePlayerInfo(-1);
        }
        if (gGameBusy) {
            break;
        }
        do_players();
        if (gGameMode == 0x8009) {
            break;
        }
        if (!do_player_select()) {
            break;
        }
        if (AudioSysUpdate(100000)) {
            break;
        }
        {
            u32 player_mask = *(volatile u32*)&lbl_80344824;
            for (i = 0; i < 4; i++) {
                !!(player_mask & (1 << i));
            }
        }
        fn_8005351C();
        break;
    case 0x400f:
        WritePlayerInfo(-1);
        if (do_mapscreen(flag) && !AudioSysUpdate(100000)) {
            ResolveWorldData(lbl_80343C04);
            if (!init_gamemovie(flag)) {
                fn_8005351C();
            }
        } else {
            do_players();
        }
        break;
    case 0x400e:
        c = do_gamemovie();
        if (c == 2) {
            init_shop(0);
        } else if (c) {
            fn_8005351C();
        }
        break;
    case 0x400c:
        gGameMode = 0x4010;
        WritePlayerInfo(-1);
        AudioMusicVolUpdate();
        if (!gGameBusy && sMusicTrackHi != 12) {
            lbl_803447CC += gFrameTicks;
        }
        SetPlayerWindows(0);
        break;
    case 0x4010:
        lbl_803447E4 = 0;
        if (!(gGameBusy | gGameplayPauseTimer | gScriptedCameraState)) {
            if (good_wiz_exit_timer > 0) {
                if ((good_wiz_exit_timer -= gFrameTicks) <= 0) {
                    lbl_80344808 = 1;
                }
            }
        }
        fn_80054E78();
        fn_80055678(lbl_8025EA04, gCameras + 75);
        if (!lbl_803447B8 && lbl_8034479C == 0) {
            lbl_8034479C = MBOX_NewObject(lbl_80346AB0, 0, lbl_80344EA8, 8);
        }
        if (sMusicTrackHi == 13 && !options_state && !lbl_803447B8) {
            if (check_active_players()) {
                init_player_select(1);
                break;
            }
        }
        AudioMusicVolUpdate();
        if (!gGameBusy && sMusicTrackHi != 12) {
            lbl_803447CC += gFrameTicks;
        }
        if (!lbl_80344824) {
            i = 0;
            gGameMode = 0x4014;
            gGameBusy = i;
            AudioStopSelect();
            AudioSelectReset();
            fn_8009D34C();
            lbl_80344774 = i;
            lbl_80344778 = 240;
            for (; i < 4; i++) {
                abort_player(i);
            }
            lbl_80344824 = 0;
            TriggerCameraEnd();
            break;
        }
        world_update();
        fn_8005B988();
        do_enemies();
        enemy_update();
        if (do_players() && !sndFxUpdate(1)) {
            lvl = (lbl_803448D0 << 8) | (lbl_803448CC & 0xFF);
            if (!lbl_80344824) {
                i = 0;
                gGameMode = 0x4014;
                gGameBusy = i;
                AudioStopSelect();
                AudioSelectReset();
                fn_8009D34C();
                lbl_80344774 = i;
                lbl_80344778 = 240;
                for (; i < 4; i++) {
                    abort_player(i);
                }
                lbl_80344824 = 0;
                TriggerCameraEnd();
                break;
            }
            fn_8009D610(2, 0);
            if (lvl == sWorldDataConst) {
                i = -1;
                lbl_80343C10 = i;
                lbl_80343DD4 = i;
                lbl_80343B38 = i;
                AudioStopSelect();
                lbl_803448AC = i;
                lbl_803448A8 = i;
                lbl_80343C04 = next_world();
                ResolveWorldData(lbl_80343C04);
                lbl_80343C00 = init_mapscreen(120, 0);
                break;
            }
            next = -1;
            cond = 0;
            c = lbl_8034481C;
            if (c >= 13 && c < 0x10000) {
                cond = 1;
            }
            flag2 = cond ? 1 : 0;
            all = 1;
            for (i = 0; i < 4; i++) {
                v = gPlayers[i].state;
                if (v != 0 && v != 11) {
                    all = 0;
                }
            }
            if (all) {
                next = sWorldDataConst;
            } else if (flag2) {
                next = lbl_80344B84;
            } else if (opt_restart_request) {
                next = sWorldDataConst;
            } else if (c == 0 || c == 12) {
                if (gBossType == 0x2c) {
                    init_gamemovie(0x2c);
                    sLastWorldLevel = sWorldDataConst;
                    next = -2;
                } else if (gBossType == 0x2b) {
                    init_gamemovie(0x2b);
                    sLastWorldLevel = sWorldDataConst;
                    next = -2;
                } else if ((s32)lbl_803448D0 == 12) {
                    next = lvl + 1;
                } else if ((s32)lbl_803448D0 == 5 && lbl_803448CC == 0) {
                    next = lvl + 1;
                } else if ((s32)lbl_803448D0 == 6 && lbl_803448CC == 0) {
                    next = lvl + 1;
                } else {
                    next = sWorldDataConst;
                }
            }
            if (!all) {
                towerRecordLevelBeaten(lbl_803448D0, lbl_803448CC);
            }
            if (next != -2) {
                if (next == sWorldDataConst) {
                    init_shop(0);
                } else {
                    if (next >= 0) {
                        lbl_80343C04 = next;
                        ResolveWorldData(lbl_80343C04);
                    } else {
                        lbl_80343C04 = sLastWorldLevel;
                    }
                    lbl_80343C00 = init_mapscreen(120, flag2);
                }
            }
        } else {
            ProcessEffects();
            fn_80055AFC();
        }
        break;
    case 0x4012:
        c = do_shop();
        if (c) {
            if (c == 2 && !lbl_80344C18) {
                init_player_select(2);
            } else {
                fn_8005351C();
            }
        } else {
            do_players();
        }
        break;
    case 0x4014:
        world_update();
        fn_8005B988();
        do_enemies();
        do_players();
        fn_800521E8();
        break;
    case 0x4016:
        if (do_stats_display()) {
            init_gamemovie(0x2c);
        } else {
            do_players();
        }
        break;
    case 0x800A:
        if (pbDiagDrawMenu() == 2) {
            gGameMode = lbl_80344788;
        }
        break;
    }
}
#pragma dont_inline reset

void fn_80054E78(void)
{
    u8* state = (u8*)lbl_802575C0;
    s32 active;
    s32 off;
    u8* q;
    s32 i;
    void** b;

    if (lbl_803447B8 != 0) {
        active = 0;
    } else {
        active = 1;
    }

    if ((gControllerButtons & 0x10) == 0) {
        if (active != 0 && (gCurLevel->flags & 4) &&
            *(void**)(state + 124) != 0) {
            mbBlitInit3414(*(void**)(state + 124), 0);
        }
        if (lbl_80344818 >
            lbl_80346AF0 + (f32)gCurLevel->wavetime) {
            lbl_80344814 = lbl_80346B08;
            lbl_80344818 = lbl_80346B08;
        }
    }

    if ((gCurLevel->flags & 4) &&
        (gGameBusy | gGameplayPauseTimer) == 0 &&
        (gControllerButtons & 4) == 0 && active != 0) {
        f32 t;
        f32 nt;
        s32 oldi;

        t = lbl_80344818;
        oldi = (s32)t;
        lbl_80344818 = t - gClockFrameStep;
        nt = lbl_80344818;
        if (nt <= lbl_80346B10) {
            for (i = 0, off = 0; i < 4; i++, off += 4) {
                u32 v;

                q = state + off;
                v = *(u32*)(q += 112);
                if (v != 0) {
                    MBRemoveBlit(v);
                    *(u32*)q = 0;
                }
            }
            lbl_80344818 = lbl_80346AFC;
            active = 0;
            if ((gControllerButtons & 0x10) != 0 &&
                (gGameOptions[9] >> 8) == 12) {
                s32 player_off;
                u8* row;
                u8* p;

                lbl_8034481C = 2;
                p = (u8*)gPlayers;
                for (player_off = 0; player_off < 48;
                     player_off += 12, p += 13148) {
                    Player* player = (Player*)p;
                    if (player->state != 0) {
                        row = state + player_off;
                        *(f32*)(row + 144) = player->pos[0];
                        *(f32*)(row + 148) = player->pos[1];
                        *(f32*)(row + 152) = player->pos[2];
                    }
                }
            } else {
                lbl_8034481C = 13;
            }
            init_got_it();
        } else if (lbl_80344810 == 0) {
            s32 n = (s32)nt;
            if (oldi != (s32)nt) {
                AudioFootstep(n);
                if (n == 8) {
                    fn_8009FA84();
                } else if (n <= 5 && lbl_8034481C == 0) {
                    fn_8009FCA8(n);
                }
            }
        }

        if (active != 0) {
            f32 total = (f32)(lbl_80346B18 * lbl_80344814);
            f32 curv = (f32)(lbl_80346B18 * lbl_80344818);
            f32 frac = (total - curv) / total;
            f64 v1;
            f64 v2;

            b = (void**)(state + 116);
            mbBlitSetupVerts(*b, lbl_80346B20, lbl_80346B20,
                             (f32)((lbl_80346B30 * frac + lbl_80346B28) *
                                   lbl_80346B38),
                             lbl_80346B20);
            v1 = lbl_80346B40 * frac;
            mbBlitProject(*b, 0, 41 - Round((f32)v1));
            mbBlitCalcY(*b, Round((f32)v1) + 24);

            b = (void**)(state + 120);
            v2 = lbl_80346B50 * frac;
            mbBlitSetupVerts(*b, lbl_80346B20, lbl_80346B20,
                             (f32)((lbl_80346B48 - v2) * lbl_80346B38),
                             lbl_80346B20);
            mbBlitProject(*b, 0, Round((f32)v2) + 23);
            mbBlitCalcY(*b, 106 - Round((f32)v2));

            if ((gControllerButtons & 0x10) != 0) {
                DrawText(-256, 8, 6, 0xFFFFFF, lbl_80346B58,
                         lbl_80344814 - lbl_80344818);
            } else if ((gControllerButtons & 0x10) != 0) {
                DrawText(-256, 8, 6, 0xFFFFFF, lbl_80346B58, lbl_80344818);
            }
        }
    }
}

/* 0x80057024 -- world initializer (enemies/effects/critters; level start). */
extern f32  sMusicFadeBase;
extern f32  lbl_80346C94;
extern f64  lbl_80346C98;
extern f64  lbl_80346C60;
extern f32  lbl_80346C68;
extern f32  lbl_80346C90;
extern f32  lbl_80346C30;
extern f32  lbl_80346CB0;
extern f32  lbl_80346CB4;
extern f32  lbl_80346BF0;
extern f32  lbl_80346BE0;
extern f32  gPlayerStartYaw;
extern f32  sLevelAmbientScale;
extern s32  gBoss398;
extern s32  gNumType7Items;
extern f32  lbl_80344860;
extern f32  lbl_80344864;
extern s32  lbl_80344868;
extern f32  lbl_80344880;
extern s32  lbl_803447FC;
extern s32  lbl_803447F8;
extern s32  lbl_8034489C;
extern f32  lbl_80344898;
extern s32  lbl_80344894;
extern s32  lbl_80344890;
extern s32  lbl_8034488C;
extern s32  lbl_80344888;
extern s32  lbl_8034484C;
extern s32  lbl_8034486C;
extern s32  lbl_80344884;
extern s32  lbl_8023E558[];
extern f32  gDefaultPlayerPosition[3];
DECL_SECT(".sdata2") extern const char lbl_80346C00[];
DECL_SECT(".sdata2") extern const char lbl_80346CA0[];
DECL_SECT(".sdata2") extern const char lbl_80346CA4[];
DECL_SECT(".sdata2") extern const char lbl_80346CA8[];
extern s32  towerGetRuneNearStat(s32 player, s32 world);
extern void WorldSaveInitState(void);
extern s32  FindWORLDOBJ(char* name);
extern char* strcpy(char* dst, const char* src);
extern f32  Random(f32 range);
extern void AddItemInstList(void);
extern void AddLocatorInstList(void);
extern void InitDynGrid(f32 a, f32 b);
extern void fn_8005D04C(void);
extern void SumnerInit(void);
extern void mini_inventory_setup(void);
extern void AppendBigapePowerupsToScene(void);
extern void SetupWeaponPowerupTexMods(void);
extern void SetupItemTexMods(void);
extern void DoPlayerTexMods(s32 idx);
extern void InitEffects(void);
extern void InitItemInfoData(void);
extern void CritterInitAllMoves(void);

extern f64  lbl_80346C10;
extern f64  lbl_80346C38;
extern f64  lbl_80346C18;
extern f64  lbl_80346C20;
extern f64  lbl_80346C28;
extern f64  lbl_80346C40;
extern f64  lbl_80346C58;
extern f64  lbl_80346C70;
extern f64  lbl_80346C78;
extern f64  lbl_80346C88;
extern f32  lbl_80346C4C;
extern f32  lbl_80346C50;
extern f32  lbl_80346C80;
extern f32  lbl_80346C84;
extern char lbl_80346C48[8];
extern s32  lbl_803447B4;
extern void* lbl_803447B0;
extern u8*  gBossObj;
extern s32  gBossDead;
extern f32  gClockTime;
extern u8   Effects[];
extern void DoGoodWizard(void);
extern void ProcessSpewItems(void);
extern s32  CamGetPlayerAvgPos(f32* pos, s32 mode);
extern void StartEnterFX(f32* pos);
extern void fn_8009D288(f32* pos);
extern void fn_80067AE0(f32 a, f32 b);
extern s32  sndFxQueUpdate(void);
extern void* MBOX_FindObject(char* name);
extern void MBSetObject(void* a, void* b);
extern s32  DeleteEffect(s32 idx, s32 mode);
extern void fn_8009C9DC(s32 mode, void* pos);

/* 0x8005674C - per-frame world state update (fades, boss timers, wobjs). */
#pragma dont_inline on
void world_update(void)
{
    char* strs = lbl_80112788;
    u8* tbl = (u8*)lbl_80257680;
    s32 cond;
    s32 i;
    s32 off;
    s32 kill;
    f32 d;
    f32 pos[3];
    f32 a;

    cond = 1;
    if (lbl_803447B8 != 0 && gCurLevel->earlyenemies == 0) {
        cond = 0;
    }
    lbl_8034488C = cond ? 1 : 0;
    if (gGameBusy | gGameplayPauseTimer) {
        return;
    }
    DoWorldAnimation();
    SetupDynGrid();
    CreateDynobjGrid();
    if (gBossType >= 0 && gGameMode == 0x4010) {
        if (good_wiz_state) {
            DoGoodWizard();
        }
        ProcessSpewItems();
    }
    if (gGameMode == 0x4010 || gGameMode == 0x400c) {
        if (lbl_803447B8 != 0) {
            d = sMusicFadeBase - lbl_80344860;
            if (d < lbl_80346C10) {
                a = (f32)(lbl_80346C18 *
                          (lbl_80346C20 - (f32)(lbl_80346C28 * d)));
                MBCompVertScaleAddUV(lbl_8034486C, 0, a, a, lbl_80346C30,
                                     lbl_80346BF0, lbl_80346BF0);
            } else {
                if (!lbl_80344868) {
                    u8* lv = (u8*)gCurLevel;
                    u8* hdr = lv + 0x70;
                    s32 col = 0;

                    if (*(f32*)(lv + 0x80) > lbl_80346BF0) {
                        col = (hdr[1] << 16) | (hdr[2] << 8) | hdr[3];
                    }
                    MBCompVertScaleAddUV(
                        col, hdr[0], *(f32*)(hdr + 0xc), *(f32*)(hdr + 0x10),
                        (f32)(lbl_80346C10 * *(f32*)(hdr + 0x14)),
                        (f32)(lbl_80346C10 * *(f32*)(hdr + 0x18)),
                        (f32)(lbl_80346C38 * *(f32*)(hdr + 8)));
                    lbl_80344868 = 1;
                }
                if (lbl_803447B8 == 1) {
                    lbl_803447B8 = 2;
                    if (CamGetPlayerAvgPos(pos, 2)) {
                        StartEnterFX(pos);
                        fn_8009D288(pos);
                    } else {
                        StartEnterFX(gDefaultPlayerPosition);
                        fn_8009D288(gDefaultPlayerPosition);
                    }
                }
                if (d >= lbl_80346C40 && gScriptedCameraState > 1) {
                    gScriptedCameraState = 1;
                }
            }
        } else {
            if (!lbl_80344868) {
                u8* lv = (u8*)gCurLevel;
                u8* hdr = lv + 0x70;
                s32 col = 0;

                if (*(f32*)(lv + 0x80) > lbl_80346BF0) {
                    col = (hdr[1] << 16) | (hdr[2] << 8) | hdr[3];
                }
                MBCompVertScaleAddUV(
                    col, hdr[0], *(f32*)(hdr + 0xc), *(f32*)(hdr + 0x10),
                    (f32)(lbl_80346C10 * *(f32*)(hdr + 0x14)),
                    (f32)(lbl_80346C10 * *(f32*)(hdr + 0x18)),
                    (f32)(lbl_80346C38 * *(f32*)(hdr + 8)));
                lbl_80344868 = 1;
            }
        }
    }
    {
        u8* lv = (u8*)gCurLevel;

        if ((s8)lv[8] == (s8)lbl_80346C48[0] &&
            ((s8)lv[9] == 0 || (s8)lv[9] == (s8)lbl_80346C48[1]) &&
            ((s8)lv[10] == 0 || (s8)lv[10] == (s8)lbl_80346C48[2]) &&
            ((s8)lv[11] == 0 || (s8)lv[11] == (s8)lbl_80346C48[3])) {
            cond = 1;
        } else {
            cond = 0;
        }
    }
    if (cond && gGameMode == 0x4010 && gBossObj != NULL &&
        *(s32*)(gBossObj + offsetof(Critter, state)) != 0) {
        {
            u32 w = (u32)FindWORLDOBJ(strs + 0xd0);

            if (w != 0 && *(u32*)(w + 0x28) != 0) {
                *(s32*)(*(u32*)(w + 0x28) + 0x60) |= 2;
            } else {
                ErrorPrintf(strs + 0xdc);
            }
        }
        {
            u32 w = (u32)FindWORLDOBJ(strs + 0xfc);

            if (w != 0 && *(u32*)(w + 0x28) != 0) {
                *(s32*)(*(u32*)(w + 0x28) + 0x60) |= 2;
            } else {
                ErrorPrintf(strs + 0x108);
            }
        }
    }
    {
        f64 k2 = lbl_80346C60;
        f64 k1 = lbl_80346C58;

        for (i = 0, off = 0; i < lbl_8034484C; i++, off += 4) {
            u8* row = tbl + off;
            u8* wo = *(u8**)(row + 0x4c);
            u8* node;
            f32* timer;

            if (wo == 0) {
                continue;
            }
            node = *(u8**)(wo + 0x28);
            if (node == 0) {
                continue;
            }
            if (*(s32*)(node + 0x60) & 2) {
                timer = (f32*)(row + 0x5c);
                if (sMusicFadeBase >= *timer) {
                    fn_80067AE0(lbl_80346C4C, lbl_80346C50);
                    MBTreeClearFlags(*(void**)(wo + 0x28), 2, 0);
                    *timer = (f32)(k1 + sMusicFadeBase);
                }
            } else {
                timer = (f32*)(row + 0x5c);
                if (sMusicFadeBase >= *timer) {
                    MBTreeSetFlags(node, 2, 0);
                    *timer = (f32)(k2 + sMusicFadeBase +
                                   Random(lbl_80346C68));
                }
            }
        }
    }
    if (gGameMode == 0x4010 && lbl_803447B4 != 0) {
        if (lbl_80344864 == lbl_80346C70) {
            lbl_80344864 = sMusicFadeBase;
            if (gBossType != 0x2c) {
                TransitionBlitShow(0);
            }
            if (lbl_803447B0 != NULL) {
                MBBlitSetAlpha(lbl_803447B0, 255);
            }
            MBCompVertScaleAddUV(0, 0, lbl_80346BF0, lbl_80346BF0,
                                 lbl_80346BF0, lbl_80346BF0, lbl_80346BF0);
        } else {
            d = sMusicFadeBase - lbl_80344864;
            if (d < lbl_80346C78) {
                if (lbl_803447B0 != NULL) {
                    MBBlitSetAlpha(lbl_803447B0,
                                   (s32)(lbl_80346C18 *
                                         (lbl_80346C20 - lbl_80346C28 * d)));
                }
                sLevelAmbientScale = (f32)(lbl_80346C20 - d);
            } else {
                if (lbl_803447B0 != NULL) {
                    MBBlitSetAlpha(lbl_803447B0, 0);
                }
                sLevelAmbientScale = lbl_80346C80;
                if (!sndFxQueUpdate()) {
                    lbl_803447B4 = 2;
                }
            }
        }
    }
    PrintWorldMemSizes();
    if (gBossDead) {
        lbl_8034489C = 99;
    }
    switch (lbl_8034489C) {
    case 2:
        if (lbl_80344898 == lbl_80346C70) {
            switch (gBossType) {
            case 0x29:
            case 0x2a:
                lbl_80344898 = (f32)(lbl_80346C20 + sMusicFadeBase);
                break;
            case 0x23:
                lbl_80344898 = (f32)(lbl_80346C20 + sMusicFadeBase);
                break;
            default:
                lbl_80344898 = (f32)(lbl_80346C40 + sMusicFadeBase);
                break;
            }
        } else if (sMusicFadeBase >= lbl_80344898) {
            lbl_8034489C = 3;
            lbl_80344898 = lbl_80346BF0;
        }
        fn_80067AE0(lbl_80346C4C, lbl_80346C84);
        break;
    case 3:
        fn_80067AE0(lbl_80346C4C, lbl_80346C84);
        break;
    case 4:
        lbl_8034489C = 5;
        lbl_80344898 = sMusicFadeBase;
        break;
    case 5:
        kill = 0;
        d = sMusicFadeBase - lbl_80344898;
        switch (gBossType) {
        case 0x27:
        case 0x28:
            if ((f32)(lbl_80346C88 - d) <= lbl_80346C70) {
                kill = 1;
                *(f32*)(gBossObj + offsetof(Critter, unkAC8)) = lbl_80346BF0;
            }
            break;
        case 0x2a:
            if ((f32)(lbl_80346C88 - d) <= lbl_80346C70) {
                kill = 1;
                *(f32*)(gBossObj + offsetof(Critter, unkAC8)) = lbl_80346BF0;
            }
            break;
        case 0x24:
            if ((f32)(lbl_80346C88 - d) <= lbl_80346C70) {
                kill = 1;
            }
            break;
        case 0x26:
            if ((f32)(lbl_80346C88 - d) <= lbl_80346C70) {
                void* found = MBOX_FindObject(strs + 0x128);
                u32 o = *(u32*)(gBossObj + offsetof(Critter, hitnode1));

                if (o != 0 && *(u32*)(o + 0x78) != 0) {
                    MBSetObject((void*)*(s32*)(o + 0x78), found);
                }
                *(u16*)(gBossObj + offsetof(Critter, unkAC6)) = 0;
                lbl_8034489C = 6;
            }
            break;
        }
        if (kill) {
            if (lbl_80344894 >= 0) {
                lbl_80344894 = DeleteEffect(lbl_80344894, 1);
                fn_8009C9DC(3, gBossObj + offsetof(Critter, movevec));
                fn_8009C9DC(4, gBossObj + offsetof(Critter, movevec));
            }
            lbl_8034489C = 6;
        }
        if (lbl_80344890 >= 0) {
            u8* e = Effects + lbl_80344890 * 0xf0;
            f32 dt = *(f32*)(e + 0x68) - gClockTime;

            if (dt < lbl_80346C40) {
                if (!(*(s32*)(e + 0x64) & 0x4020)) {
                    u32 o = *(u32*)(e + 0x14);

                    if (o != 0) {
                        s32 al = (s32)(lbl_80346C90 * dt);

                        while (al > 255) {
                            al -= 255;
                        }
                        MBTreeSetAlpha(
                            (void*)*(s32*)(*(s32*)(o + 0x78) + 0x78), al, 2);
                    }
                }
            }
        }
        break;
    }
}

#pragma dont_inline reset

void fn_80057024(void)
{
    u8* tbl = (u8*)lbl_80257680;
    char* fmt = lbl_80112788;
    f32 z = lbl_80346BF0;
    s32 off;
    s32 i;
    u8* p;

    lbl_80344860 = sMusicFadeBase;
    lbl_80344864 = z;
    lbl_80344868 = 0;
    lbl_80344880 = lbl_80346C94;
    lbl_803447FC = 18000;
    lbl_803447F8 = 18000;
    gDefaultPlayerPosition[0] = z;
    gDefaultPlayerPosition[1] = z;
    gDefaultPlayerPosition[2] = z;
    gPlayerStartYaw = z;
    gBoss398 = -1;
    lbl_8034489C = 0;
    lbl_80344898 = z;
    lbl_80344894 = -1;
    lbl_80344890 = -1;
    gNumType7Items = 0;
    lbl_8034488C = 0;

    if (gBossType >= 0 && gBossType < 43) {
        lbl_8034489C = 0;
        for (i = 0, off = 0, p = (u8*)gPlayers; i < 4; i++, off += 13148) {
            u8* q = p + off;
            s32 st = *(s32*)(q + offsetof(Player, state));
            if (st == 1 || st == 5 || st == 3) {
                if (lbl_8034489C != 0) {
                    *(s32*)(q + offsetof(Player, quest_state)) = 0;
                } else if (towerGetRuneNearStat(i, sMusicTrackHi) != 0) {
                    *(s32*)(q + offsetof(Player, quest_state)) = 1;
                    lbl_8034489C = 1;
                    lbl_80344898 = z;
                } else {
                    *(s32*)(q + offsetof(Player, quest_state)) = 0;
                }
            }
        }
    }

    WorldSaveInitState();
    lbl_80344880 = (f32)(gWorldInfo.worldmin[1] - lbl_80346C98);
    GetEnemyTypes();

    if (gGameOptions[2] < 2 && gBossType < 0 && sMusicTrackHi != 13 &&
        lbl_80344738 < 0) {
        lbl_80344738 = LoadModel(lbl_80346C00, 0, 0, -1);
    }

    {
        s32* pool = lbl_802511FC;
        for (off = 0, i = 0; i < 8; i++, off += 4) {
            s32 raw = *(s32*)(tbl + off + 332);
            s32 t = raw;
            if (raw < 32) {
                if (raw == 29) {
                    continue;
                }
            } else {
                if (raw >= 45) {
                    goto chk;
                }
                continue;
            }
chk:
            if (t >= 0) {
                s32* pe = pool + t;
                if (pe[0] == 0) {
                    LoadEnemy(t, *(s32*)(tbl + off + 268));
                }
            }
        }
    }

    {
        s32 free0 = BytesFree();
        if (!(gGameMode & 0x8000)) {
            LoadWeapons();
        }
        LoadPowerups((char*)lbl_80344888);
        lbl_80344858 = lbl_80344858 + (free0 - BytesFree());
        free0 = BytesFree();
        LoadItems();
        lbl_8034485C = free0 - BytesFree();
    }

    if (gWorldInfo.atreelist != 0) {
        InitTexMods(gWorldInfo.atreelist, gWorldInfo.model);
    }
    fn_800508A0();
    SetupWeaponPowerupTexMods();
    SetupItemTexMods();
    i = 0;
    do {
        DoPlayerTexMods(i);
        i++;
    } while (i < 4);
    InitEffects();
    InitItemInfoData();
    CritterInitAllMoves();

    if (InLevel(lbl_80346CA0)) {
        lbl_8034484C = 4;
        strcpy((char*)(tbl + 108), fmt + 312);
    } else if (InLevel(lbl_80346CA4)) {
        lbl_8034484C = 4;
        strcpy((char*)(tbl + 108), fmt + 328);
    } else {
        lbl_8034484C = 0;
    }

    {
        f64 kOff = lbl_80346C60;
        for (i = 0, off = 0; i < lbl_8034484C; i++, off += 4) {
            sprintf((char*)tbl, lbl_80346CA8, tbl + 108, i + 1);
            *(s32*)(tbl + off + 76) = FindWORLDOBJ((char*)tbl);
            if (*(void**)(tbl + off + 76) != 0 &&
                *(void**)(*(u8**)(tbl + off + 76) + 40) != 0) {
                MBTreeSetFlags(*(void**)(*(u8**)(tbl + off + 76) + 40), 2, 0);
            } else {
                ErrorPrintf(fmt + 344, tbl);
            }
            *(f32*)(tbl + off + 92) =
                (f32)(kOff + sMusicFadeBase + Random(lbl_80346C68));
        }
    }

    world_update();
    AddItemInstList();
    AddLocatorInstList();
    if (gBossType >= 0) {
        InitDynGrid(lbl_80346C68, lbl_80346CB0);
    } else {
        InitDynGrid(lbl_80346CB4, lbl_80346CB0);
    }
    SetupDynGrid();
    CreateDynobjGrid();
    fn_8005D04C();
    good_wiz_state = 0;
    lbl_8023E558[24] = 0;
    lbl_80344884 = 0;
    if (sMusicTrackHi == 13) {
        SumnerInit();
    }
    if (gGameMode == 0x4010 || gGameMode == 0x400C) {
        MBCompVertScaleAddUV(lbl_8034486C, 0, lbl_80346C90, lbl_80346C90,
                             lbl_80346C30, lbl_80346BF0, lbl_80346BF0);
    } else {
        MBCompVertScaleAddUV(0, 0, lbl_80346BF0, lbl_80346BF0, lbl_80346BF0,
                             lbl_80346BF0, lbl_80346BF0);
        sLevelAmbientScale = lbl_80346BE0;
    }
    if (!(gGameMode & 0x8000)) {
        lbl_80344850 = 1;
    }
    mini_inventory_setup();
    AppendBigapePowerupsToScene();
}


/* 0x8005638C -- build the "levels/level%s" path and load a level. */
extern u32  lbl_803448D0;
extern s32  lbl_803448CC;
extern s32  lbl_803448C8;
extern s32  lbl_803448C4;
DECL_SECT(".sdata2") extern const char lbl_80346C04[];
extern s32  LoadWorldDone(void* name);
extern void CritterLoadFile(const char* wad, const char* name);
extern void CritterLoadAllTypes(s32 arg0);

#pragma opt_common_subs off
static s32 init_next_level_8005638C(s32 arg0)
{
    char* fmt = lbl_80112788;
    u8* tbl = (u8*)lbl_80257680;
    s32 result;
    s32 i;
    s32 off;

    lbl_80344874 = pbLoad;
    if (arg0 < 0) {
        LoadWorldDone(0);
        lbl_80343C30 = -1;
        return -1;
    }
    ResolveWorldData(arg0);
    {
        s32 hi = sMusicTrackHi;
        s32 lo = sMusicTrackLo;
        lbl_803448D0 = hi;
        lbl_803448CC = lo;
        if (hi != 13) {
            lbl_803448C8 = hi;
            lbl_803448C4 = lo;
        }
    }
    sprintf((char*)(tbl + 172), fmt, gCurLevel->name);
    lbl_80344854 = mlmMemUsed;
    lbl_80343C30 = 0;
    result = LoadWorldDone(tbl + 172);
    GetEnemyTypes();

    if (gGameOptions[2] < 2 && gBossType < 0 && arg0 != sWorldDataConst &&
        lbl_80344738 < 0) {
        lbl_80344738 = LoadModel(lbl_80346C00, 0, 0, -1);
    }

    for (i = 0, off = 0; i < 8; i++, off += 4) {
        u8* q = tbl + off;
        u8* w = tbl + off;
        s32 t = *(s32*)(q += 332);
        s32 flag;

        *(s32*)(w += 140) = 0;
        flag = 1;
        switch (t) {
        case 34:
            CritterLoadFile(lbl_80346C04, fmt + 16);
            break;
        case 35:
            CritterLoadFile(lbl_80346C04, fmt + 28);
            break;
        case 36:
            CritterLoadFile(lbl_80346C04, fmt + 40);
            break;
        case 37:
            CritterLoadFile(lbl_80346C04, fmt + 52);
            break;
        case 38:
            CritterLoadFile(lbl_80346C04, fmt + 64);
            break;
        case 39:
            CritterLoadFile(lbl_80346C04, fmt + 76);
            break;
        case 41:
            CritterLoadFile(lbl_80346C04, fmt + 88);
            break;
        case 40:
            CritterLoadFile(lbl_80346C04, fmt + 100);
            break;
        case 42:
            CritterLoadFile(lbl_80346C04, fmt + 112);
            break;
        case 43:
            CritterLoadFile(lbl_80346C04, fmt + 124);
            break;
        case 44:
            CritterLoadFile(lbl_80346C04, fmt + 136);
            break;
        case 29:
            if (sMusicTrackHi == 9) {
                CritterLoadFile(lbl_80346C04, fmt + 148);
            } else if (sMusicTrackHi == 6) {
                CritterLoadFile(lbl_80346C04, fmt + 160);
            } else {
                CritterLoadFile(lbl_80346C04, fmt + 172);
            }
            break;
        case 33:
            CritterLoadFile(lbl_80346C04, fmt + 184);
            break;
        case 32: {
            char buf[16];
            sprintf(buf, fmt + 196, *(char**)(tbl + off + 236) + 16);
            CritterLoadFile(lbl_80346C04, buf);
            break;
        }
        default:
            flag = 0;
            if (t >= 0) {
                *(s32*)w = BytesFree();
                AllocEnemy(t, *(s32*)(tbl + off + 268));
                *(s32*)w = *(s32*)w - BytesFree();
            }
            break;
        }
        if (flag != 0) {
            *(s32*)q = -1;
        }
    }

    CritterLoadAllTypes(0);
    lbl_80344870 = 0;
    if (arg0 != sWorldDataConst) {
        InitItems();
    }
    lbl_80344858 = 0;
    LockModels(2);
    return result;
}
#pragma opt_common_subs on

/* 0x800522E8 -- "FINAL STATS" end-of-level tally and display. */
extern s32  lbl_8011C300[];        /* per-class stats screen layout table */
extern u8   lbl_80240E30[];        /* per-class 60-byte descriptor table  */
DECL_SECT(".sdata2") extern const f32  lbl_80346ABC;
DECL_SECT(".sdata2") extern const char lbl_80346AC0[];
DECL_SECT(".sdata2") extern const char lbl_80346AC4[];
DECL_SECT(".sdata2") extern const char lbl_80346AC8[];
DECL_SECT(".sdata2") extern const f32  lbl_80346AD0;
DECL_SECT(".sdata2") extern const char lbl_80346AD8[];
DECL_SECT(".sdata2") extern const char lbl_80346AE0[];
extern f32  lbl_80346AE4;
extern void DrawTextKeepScale(f32 scale, s32 x, s32 y, s32 flags, s32 color,
                              const char* fmt);
extern s32  DrawNormalText(f32 scale, char* s, s32 flags);
extern void WritePlayerInfo(s32 player);
extern void fn_8009FCA8(s32 arg0);
extern void AudioStopMusicA(void);
extern s32  strcmp(const char* a, const char* b);

#define STAT_ROW(colp, lab, valoff)                                         \
    {                                                                       \
        s32 c_;                                                             \
        u8* row_;                                                           \
        s32 w_;                                                             \
        char buf_[12];                                                      \
        c_ = *(s32*)p;                                                      \
        row_ = (u8*)layout + c_ * 4;                                        \
        DrawTextKeepScale(lbl_80346AD4, *(s32*)(row_ + 32) + 7,             \
                          *(colp) + *(s32*)(row_ + 80), 7, 0xFFFFFF, lab);  \
        {                                                                               u8* v_ = state + *(s32*)p * 4;                                              sprintf(buf_, lbl_80346AE0, *(s32*)(v_ + (valoff)));                    }                                                                   \
        w_ = DrawNormalText(lbl_80346AD4, buf_, 7);                         \
        c_ = *(s32*)p;                                                      \
        row_ = (u8*)layout + c_ * 4;                                        \
        DrawTextKeepScale(lbl_80346AD4, *(s32*)(row_ + 64) - w_,            \
                          *(colp) + *(s32*)(row_ + 80), 7, 0xFFFFFF, buf_); \
    }

#define TIME_ROW(colp)                                                      \
    {                                                                       \
        s32 c_;                                                             \
        u8* row_;                                                           \
        s32 t_;                                                             \
        s32 sec_;                                                           \
        s32 min_;                                                           \
        s32 w_;                                                             \
        char buf_[12];                                                      \
        c_ = *(s32*)p;                                                      \
        row_ = state + c_ * 4;                                              \
        t_ = *(s32*)(row_ + 64) / 60;                                       \
        sec_ = t_ % 60;                                                     \
        t_ /= 60;                                                           \
        min_ = t_ % 60;                                                     \
        t_ /= 60;                                                           \
        row_ = (u8*)layout + c_ * 4;                                        \
        DrawTextKeepScale(lbl_80346AD4, *(s32*)(row_ + 32) + 7,             \
                          *(colp) + *(s32*)(row_ + 80), 7, 0xFFFFFF,        \
                          msgs + 36);                                       \
        sprintf(buf_, msgs + 48, t_, min_, sec_);                           \
        w_ = DrawNormalText(lbl_80346AD4, buf_, 7);                         \
        c_ = *(s32*)p;                                                      \
        row_ = (u8*)layout + c_ * 4;                                        \
        DrawTextKeepScale(lbl_80346AD4, *(s32*)(row_ + 64) - w_,            \
                          *(colp) + *(s32*)(row_ + 80), 7, 0xFFFFFF, buf_); \
    }

#define STAT_TALLY(accOff, tgtOff, ok)                                          {                                                                               s32 c_ = *(s32*)p;                                                          u8* b_ = state + c_ * 4;                                                    s32 amt_ = *(s32*)(b_ + 96);                                                if (gGameBusy != 0) {                                                           ok = 0;                                                                 } else {                                                                        u8* a_;                                                                     if (*(s32*)(lbl_80240E30 + c_ * 60 + 4) & 0x0F000000) {                         amt_ *= 6;                                                              }                                                                           *(s32*)(b_ + (accOff)) = *(s32*)(b_ + (accOff)) + amt_;                     a_ = state + *(s32*)p * 4;                                                  if (*(s32*)(a_ += (accOff)) <                                                   *(s32*)((u8*)p + *(s32*)(p + offsetof(Player, character)) * 28 + (tgtOff))) {                        ok = 0;                                                                 } else {                                                                        *(s32*)a_ =                                                                     *(s32*)((u8*)p + *(s32*)(p + offsetof(Player, character)) * 28 + (tgtOff));                      ok = 1;                                                                 }                                                                       }                                                                       }

s32 do_stats_display(void)
{
    char* msgs;
    u8* state = lbl_802575C0;
    s32* layout = lbl_8011C300;
    u8* p;
    s32* col1;
    s32* col2;
    s32* col3;
    s32* colT;
    s32 i;
    s32 off;
    s32 stalled = 0;
    s32 done = 1;
    f32 k60;

    msgs = lbl_80112538;
    DrawTextKeepScale(lbl_80346ABC, -256, 0, 7, 0xFFFFFF, msgs);
    k60 = lbl_80346AE4;
    col1 = (s32*)((u8*)layout + 100);
    col2 = (s32*)((u8*)layout + 104);
    col3 = (s32*)((u8*)layout + 108);
    colT = (s32*)((u8*)layout + 116);

    for (i = 0, off = 0, p = (u8*)gPlayers; i < 4; i++, off += 4, p += 13148) {
        s32 st = *(s32*)(p + offsetof(Player, state));
        char nbuf[12];

        if (st != 1 && st != 5 && st != 4) {
            continue;
        }
        sprintf(nbuf, lbl_80346AC0, p + 2688);
        if (strcmp(nbuf, lbl_80346AC4) == 0) {
            strcpy(nbuf, lbl_80346AC8);
        }
        {
            s32 c = *(s32*)p;
            u8* row = (u8*)layout + c * 4;
            DrawTextKeepScale(lbl_80346AD0, -*(s32*)(row + 48),
                              layout[24] + *(s32*)(row + 80),
                              7, 0xFFFFFF, nbuf);
        }

        switch (*(u32*)(p + offsetof(Player, field_A64))) {
        case 0: {
            u8* sp = state + off;
            s32 on = (*(s32*)(sp + 64) == 0);
            s32* t96;
            *(s32*)(sp + 16) = on;
            t96 = (s32*)(sp + 96);
            *(s32*)(sp + 48) = on;
            *(s32*)(sp + 32) = on;
            *(s32*)(sp + 80) = 480;
            (*(s32*)(p + offsetof(Player, field_A64)))++;
            *t96 = *(s32*)((u8*)p + *(s32*)(p + offsetof(Player, character)) * 28 + 3088) / 60;
            if (*t96 < 1) {
                *t96 = 1;
            }
        }
        case 1: {
            s32 ok;
            done = 0;
            STAT_TALLY(32, 3088, ok);
            if (ok != 0) {
                u8* sp2 = state + off;
                (*(s32*)(p + offsetof(Player, field_A64)))++;
                *(s32*)(sp2 + 96) =
                    *(s32*)((u8*)p + *(s32*)(p + offsetof(Player, character)) * 28 + 3104) / 60;
                if (*(s32*)(sp2 += 96) < 1) {
                    *(s32*)sp2 = 1;
                }
            } else {
                stalled = 1;
            }
            STAT_ROW(col1, lbl_80346AD8, 32);
            break;
        }
        case 2: {
            s32 ok;
            done = 0;
            STAT_ROW(col1, lbl_80346AD8, 32);
            STAT_TALLY(48, 3104, ok);
            if (ok != 0) {
                u8* sp2 = state + off;
                (*(s32*)(p + offsetof(Player, field_A64)))++;
                *(s32*)(sp2 + 96) =
                    *(s32*)((u8*)p + *(s32*)(p + offsetof(Player, character)) * 28 + 3108) / 60;
                if (*(s32*)(sp2 += 96) < 1) {
                    *(s32*)sp2 = 1;
                }
            } else {
                stalled = 1;
            }
            STAT_ROW(col2, msgs + 12, 48);
            break;
        }
        case 3: {
            s32 ok;
            done = 0;
            STAT_ROW(col1, lbl_80346AD8, 32);
            STAT_ROW(col2, msgs + 12, 48);
            STAT_TALLY(16, 3108, ok);
            if (ok != 0) {
                u8* sp2 = state + off;
                (*(s32*)(p + offsetof(Player, field_A64)))++;
                *(s32*)(sp2 + 96) =
                    (s32)(*(f32*)((u8*)p + *(s32*)(p + offsetof(Player, character)) * 28 + 3112) /
                          k60);
                if (*(s32*)(sp2 += 96) < 60) {
                    *(s32*)sp2 = 60;
                }
                if (*(s32*)sp2 < 1) {
                    *(s32*)sp2 = 1;
                }
            } else {
                stalled = 1;
            }
            STAT_ROW(col3, msgs + 24, 16);
            break;
        }
        case 4:
            (*(s32*)(p + offsetof(Player, field_A64)))++;
        case 5: {
            done = 0;
            STAT_ROW(col1, lbl_80346AD8, 32);
            STAT_ROW(col2, msgs + 12, 48);
            STAT_ROW(col3, msgs + 24, 16);
            {
                s32 c = *(s32*)p;
                u8* b = state + c * 4;
                s32 amt = *(s32*)(b + 96);
                s32 ok;
                if (gGameBusy != 0) {
                    ok = 0;
                } else {
                    u8* a;
                    f32 tgt;
                    if (*(s32*)(lbl_80240E30 + c * 60 + 4) & 0x0F000000) {
                        amt *= 6;
                    }
                    *(s32*)(b + 64) += amt;
                    a = state + *(s32*)p * 4;
                    tgt = *(f32*)((u8*)p + *(s32*)(p + offsetof(Player, character)) * 28 + 3112);
                    if ((f32)*(s32*)(a += 64) < tgt) {
                        ok = 0;
                    } else {
                        *(s32*)a = (s32)tgt;
                        ok = 1;
                    }
                }
                if (ok != 0) {
                    (*(s32*)(p + offsetof(Player, field_A64)))++;
                } else {
                    stalled = 1;
                }
            }
            TIME_ROW(colT);
            break;
        }
        case 6: {
            u8* sp;
            done = 0;
            STAT_ROW(col1, lbl_80346AD8, 32);
            STAT_ROW(col2, msgs + 12, 48);
            STAT_ROW(col3, msgs + 24, 16);
            TIME_ROW(colT);
            sp = state + off;
            st = *(s32*)(sp + 80) - gFrameTicks;
            *(s32*)(sp + 80) = st;
            if (st <= 0) {
                (*(s32*)(p + offsetof(Player, field_A64)))++;
            }
            break;
        }
        default:
            STAT_ROW(col1, lbl_80346AD8, 32);
            STAT_ROW(col2, msgs + 12, 48);
            STAT_ROW(col3, msgs + 24, 16);
            TIME_ROW(colT);
            break;
        }
    }

    WritePlayerInfo(-1);
    fn_8009FCA8(stalled);
    if (done != 0) {
        s32 j;
        for (j = 0; j < 4; j++) {
            MBRemoveBlit(*(s32*)(state + j * 4));
        }
        AudioStopMusicA();
    }
    return done;
}
