#include "types.h"
#include "game/critter.h"
#include "game/enemy.h"
#include "game/gamemode.h"
#include "game/item.h"
#include "game/effect.h"
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
 * The current GameCube split is [0x800520CC, 0x80055CB8): the
 * GAMEDEFS prefix followed by the main game-flow functions. Enemy helpers
 * before this range belong to enemy.c; the world-loader block after it
 * belongs to gauntworld.c. Retail string-pool bases and exception-table
 * boundaries corroborate those module cuts independently of the Xbox names.
 * This TU remains NonMatching until all code and owned data match.

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

/* Fog block embedded in level_data at +0x70. include/game/leveldata.h
 * reserves exactly `u8 fog[0x1C]` there, and the Xbox PDB's fog_data
 * (misc.h) is exactly 0x1C with no pad gaps; every offset this TU reads
 * raw is a field start of the matching width. Kept file-local rather than
 * widening the shared header. */
typedef struct FogData {
    u8  type;         /* 0x00 fog mode                     */
    u8  color[3];     /* 0x01 packed RGB                   */
    f32 intensity;    /* 0x04                              */
    f32 density;      /* 0x08                              */
    f32 min;          /* 0x0C                              */
    f32 max;          /* 0x10 (also reached as gCurLevel+0x80) */
    f32 nearw;        /* 0x14                              */
    f32 farw;         /* 0x18                              */
} FogData;            /* 0x1C */

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

/* Game-mode ids held in gGameMode: enum e_mode, now in game/gamemode.h
 * (included above) so consumer TUs can reference the names too. */

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
/* Milestone table record (stride 0x68). Layout adopted from the recovered
 * MilestoneParam in src/game/world/items.c, which walks the same table:
 * items.c's GetMilestonePos reads matrix[12..14] as the world position,
 * exactly the m+48/52/56 triple this TU reads raw. Declared file-locally
 * (never added to a shared header) per the whole-TU cascade law. */
typedef struct MilestoneParam {
    f32 matrix[16];   /* 0x00 node transform; [8]/[10] give facing, [12..14] position */
    f32 pos[3];       /* 0x40 */
    u8  _pad4C[4];
    f32 saved_pos[3]; /* 0x50 */
    u8  _pad5C[4];
    s32 handle;       /* 0x60 */
    s32 active;       /* 0x64 */
} MilestoneParam;     /* 0x68 */

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

/* Byte offsets of the individual string literals inside the pooled .rodata
 * constant object lbl_80112788 (0x80112788, size 0x24C).  Each entry is a
 * NUL-terminated literal padded to a 4-byte boundary; the values below are
 * read from the retail image.  Only the boss .wad names consumed by
 * init_next_level_8005638C are named here. */
enum {
    STR_LEVELS_LEVEL_S = 0,    /* "levels/level%s" */
    STR_DRAGON_WAD     = 16,   /* "dragon.wad"  */
    STR_CHIMERA_WAD    = 28,   /* "chimera.wad" */
    STR_DJINN_WAD      = 40,   /* "djinn.wad"   */
    STR_DRIDER_WAD     = 52,   /* "drider.wad"  */
    STR_PBOSS_WAD      = 64,   /* "pboss.wad"   */
    STR_YETI_WAD       = 76,   /* "yeti.wad"    */
    STR_LICH_WAD       = 88,   /* "lich.wad"    */
    STR_WRAITH_WAD     = 100,  /* "wraith.wad"  */
    STR_SKORNE1_WAD    = 112,  /* "skorne1.wad" */
    STR_SKORNE2_WAD    = 124,  /* "skorne2.wad" */
    STR_GARM_WAD       = 136,  /* "garm.wad"    */
    STR_GOLEMI_WAD     = 148,  /* "golemI.wad"  */
    STR_GOLEMF_WAD     = 160,  /* "golemF.wad"  */
    STR_GOLEM_WAD      = 172,  /* "golem.wad"   */
    STR_GENERAL_WAD    = 184,  /* "general.wad" */
    STR_GAR_S_WAD      = 196   /* "gar_%s.wad"  */
};

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
extern s32 init_next_level_8005638C(s32 arg0);   /* gauntworld.obj */
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


/* Entry points now owned by enemy.c. */
void init_enemy_vars(s32 slot, s32 spew, f32 scale);
void format_brain(s32 index);
void SetEnemyObj(Enemy* enemy, s32 type, s32 level);
void fn_800508A0(void);
void fn_80050910(s32 arg0);
void AllocEnemy(s32 id, s32 model);
void LoadEnemy(s32 id, s32 model);
void fn_80050DD8(char* buf, s32 id, s32 qty);
s32 GetEnemyType(s32 w, s32 l);
void fn_800510A4(void);
void fn_80051164(void);
s32 fn_800511D0(s32 arg0, f32 arg1);
s32 fn_80051480(f32* pos);
void fn_80051568(s32 index);
void fn_800516F8(s32 slot);
void fn_80051C78(void);
char* fn_80051E1C(s32 world, s32 lvl, s32 flag);
void* EnemyTypePrefix(s32 id);
void* EnemyTypeDesc(s32 id);
s32 EnemyDescType(const char* name);
void fn_8005207C(s32 arg0, s32 arg1, s32 arg2);
void fn_800520C8(void);
/* ================================================================== */
/* Function bodies (existing source order; not yet target text order). */
/* ================================================================== */

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


extern char lbl_80112370[];        /* format-string blob */
int sprintf(char* s, const char* fmt, ...);
void* fn_80057ACC(s32 key);

/* 0x8005403C -- lock the model box, then run AtreeListLock. */
#pragma dont_inline on
void LockModels(s32 arg0)
{
    MBOX_LockModels();
    AtreeListLock(arg0);
}
#pragma dont_inline off

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
    gGameMode = MA_MOVIE;
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
        if (wave == sWorldDataConst && (gGameMode & MODE_GROUP_ATTRACT) == 0) {
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

    texture = MBOX_FindTexture_Err("TIMER", 0, 1);
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

/* 0x80053C70 -- pick Atree list sizes from the current game-mode id. */
void fn_80053C70(void)
{
    switch (gGameMode) {
    case MA_MOVIE:
    case MG_MAPSCREEN:
    case MA_SCREEN2D:
        AtreeAlloc(64, 64);
        break;
    case MG_SHOP:
    case MA_TITLESCREEN:
        AtreeAlloc(3584, 3072);
        break;
    default:
        AtreeAlloc(-1, -1);
        break;
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
        if (gPlayers[i].state == ACTIVE) {
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
    lbl_8034478C = lbl_80344790 = playerOffset;
    if ((gGameMode & MODE_GROUP_GAME) != 0 && sSpecialItem10 != 0) {
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
    gGameMode = MG_ROUND_START;
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
            if ((lbl_80344824 & (1 << i)) && ((Player*)p)->state != INTOWER) {
                Player* player = (Player*)p;
                player->state = ACTIVE;
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
            if (player->state != INACTIVE) {
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
                if (((Player*)p)->state == ACTIVE) {
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
extern void MBRemoveBlit(s32 blit);
extern void AudioFootstep(s32 n);
extern void fn_8009FA84(void);
extern void fn_8009FCA8(s32 n);
extern void DoAudioTallySFX(s32 sel);
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
    if (gGameMode & MODE_GROUP_GAME) {
        for (i = 0; i < 4; i++) {
            if (lbl_80257640[i] > 120) {
                lbl_80344A2C = 1;
            }
        }
    }
    if (opt_quit_request && OptionsDone()) {
        i = 0;
        opt_quit_request = i;
        gGameMode = MG_OVER;
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
        if (gGameMode == MG_PLAY && sMusicTrackHi != 13) {
            if (OptionsDone()) {
                for (i = 0; i < 4; i++) {
                    kill_player(i);
                }
            }
        } else {
            opt_restart_request = 0;
        }
    }
    if (gGameMode & MODE_GROUP_ATTRACT) {
        if (gControllerButtons & 4) {
            if (!assigned_controller(0)) {
                assign_controller(0);
            }
        }
    }
    if (lbl_80344A2C && gGameMode != MG_GAMEMOVIE) {
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
    if (lbl_803447D8 > lbl_803447D4 && gGameMode == MG_PLAY) {
        fn_8009D530();
    }
    c = gGameMode;
    switch (c) {
    default:
        if (c >= 0x8000) {
            switch (c) {
            default:
            case MA_CREDITS:
            case MA_HSTABLE:
                do_credits();
                goto attract_tail;
            case MA_SCREEN2D:
            case MA_CONTEST:
                if (!lbl_80344798) {
                    lbl_80344798 = 1;
                    check_prefs_loaded();
                }
                do_screen2d();
                goto attract_tail;
            case MA_TITLEMOVIE:
            case MA_MOVIE:
                do_movie();
                goto attract_tail;
            case MA_TITLESCREEN:
                do_titlescreen();
                goto attract_tail;
            case MA_INSTRUCT:
            case MA_DEMO:
            case MA_FLYBY:
                world_update();
                fn_8005B988();
                do_enemies();
                enemy_update();
                do_flyby();
            attract_tail:
        if (gGameMode != MA_TITLESCREEN && gGameMode != MG_PLAYER_SELECT && lbl_803441FC > 1) {
            v = new_start(-1);
            if (gGameMode & MODE_GROUP_ATTRACT) {
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
    case MG_PLAYER_SELECT:
        if (!(gGameOptions[11] & 1)) {
            WritePlayerInfo(-1);
        }
        if (gGameBusy) {
            break;
        }
        do_players();
        if (gGameMode == MA_TITLESCREEN) {
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
    case MG_MAPSCREEN:
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
    case MG_GAMEMOVIE:
        c = do_gamemovie();
        if (c == 2) {
            init_shop(0);
        } else if (c) {
            fn_8005351C();
        }
        break;
    case MG_ROUND_START:
        gGameMode = MG_PLAY;
        WritePlayerInfo(-1);
        AudioMusicVolUpdate();
        if (!gGameBusy && sMusicTrackHi != 12) {
            lbl_803447CC += gFrameTicks;
        }
        SetPlayerWindows(0);
        break;
    case MG_PLAY:
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
            lbl_8034479C = MBOX_NewObject("COMPASS", 0, lbl_80344EA8, 8);
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
            gGameMode = MG_OVER;
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
                gGameMode = MG_OVER;
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
    case MG_SHOP:
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
    case MG_OVER:
        world_update();
        fn_8005B988();
        do_enemies();
        do_players();
        fn_800521E8();
        break;
    case MG_STATS:
        if (do_stats_display()) {
            init_gamemovie(0x2c);
        } else {
            do_players();
        }
        break;
    case MA_VIEWMENU:
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
                    if (player->state != INACTIVE) {
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
                    DoAudioTallySFX(n);
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
                DrawText(-256, 8, 6, 0xFFFFFF, "%.1f",
                         lbl_80344814 - lbl_80344818);
            } else if ((gControllerButtons & 0x10) != 0) {
                DrawText(-256, 8, 6, 0xFFFFFF, "%.1f", lbl_80344818);
            }
        }
    }
}

/* 0x800522E8 -- "FINAL STATS" end-of-level tally and display. */
extern s32  lbl_8011C300[];        /* per-class stats screen layout table */
extern u8   lbl_80240E30[];        /* per-class 60-byte descriptor table  */
DECL_SECT(".sdata2") extern const f32  lbl_80346ABC;
DECL_SECT(".sdata2") extern const f32  lbl_80346AD0;
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
        {                                                                               u8* v_ = state + *(s32*)p * 4;                                              sprintf(buf_, "%d", *(s32*)(v_ + (valoff)));                    }                                                                   \
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

/* The active character's per-character stat record.  This is the block that
 * used to be addressed here as raw `Player + 3088 + character*28` with purely
 * positional labels; it is now modelled as Player.char_stats[16] in
 * include/game/player.h (the Xbox P_SAVE_STATS analogue -- see that header for
 * the identification evidence).  `p` walks gPlayers as a u8*, so the cast is
 * what the loop shape requires, not an invented one. */
#define CHAR_STAT(p) (((Player*)(p))->char_stats[((Player*)(p))->character])

#define STAT_TALLY(accOff, tgtField, ok)                                        {                                                                               s32 c_ = *(s32*)p;                                                          u8* b_ = state + c_ * 4;                                                    s32 amt_ = *(s32*)(b_ + 96);                                                if (gGameBusy != 0) {                                                           ok = 0;                                                                 } else {                                                                        u8* a_;                                                                     if (*(s32*)(lbl_80240E30 + c_ * 60 + 4) & 0x0F000000) {                         amt_ *= 6;                                                              }                                                                           *(s32*)(b_ + (accOff)) = *(s32*)(b_ + (accOff)) + amt_;                     a_ = state + *(s32*)p * 4;                                                  if (*(s32*)(a_ += (accOff)) <                                                   CHAR_STAT(p).tgtField) {                        ok = 0;                                                                 } else {                                                                        *(s32*)a_ =                                                                     CHAR_STAT(p).tgtField;                      ok = 1;                                                                 }                                                                       }                                                                       }

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
        sprintf(nbuf, "%s", p + 2688);
        if (strcmp(nbuf, "___") == 0) {
            strcpy(nbuf, "NO NAME");
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
            *t96 = CHAR_STAT(p).enemies_killed / 60;
            if (*t96 < 1) {
                *t96 = 1;
            }
        }
        case 1: {
            s32 ok;
            done = 0;
            STAT_TALLY(32, enemies_killed, ok);
            if (ok != 0) {
                s32* sp2 = (s32*)(state + off);
                (*(s32*)(p + offsetof(Player, field_A64)))++;
                sp2[24] = CHAR_STAT(p).generators_destroyed / 60;
                if (*(sp2 += 24) < 1) {
                    *sp2 = 1;
                }
            } else {
                stalled = 1;
            }
            STAT_ROW(col1, "ENEMIES", 32);
            break;
        }
        case 2: {
            s32 ok;
            done = 0;
            STAT_ROW(col1, "ENEMIES", 32);
            STAT_TALLY(48, generators_destroyed, ok);
            if (ok != 0) {
                s32* sp2 = (s32*)(state + off);
                (*(s32*)(p + offsetof(Player, field_A64)))++;
                sp2[24] = CHAR_STAT(p).gold_found / 60;
                if (*(sp2 += 24) < 1) {
                    *sp2 = 1;
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
            STAT_ROW(col1, "ENEMIES", 32);
            STAT_ROW(col2, msgs + 12, 48);
            STAT_TALLY(16, gold_found, ok);
            if (ok != 0) {
                s32* sp2 = (s32*)(state + off);
                (*(s32*)(p + offsetof(Player, field_A64)))++;
                sp2[24] = (s32)(CHAR_STAT(p).total_playtime / k60);
                if (*(sp2 += 24) < 60) {
                    *sp2 = 60;
                }
                if (*sp2 < 1) {
                    *sp2 = 1;
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
            STAT_ROW(col1, "ENEMIES", 32);
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
                    tgt = CHAR_STAT(p).total_playtime;
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
            STAT_ROW(col1, "ENEMIES", 32);
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
            STAT_ROW(col1, "ENEMIES", 32);
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
