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

/* Active level / world-data records (SDA-relative pointers). */
extern u8*  gCurLevel;             /* 0x8034483C */
extern u8*  gWorldData;            /* 0x80344838 */

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
extern void* lbl_80257630[];       /* two thermometer blits at [1],[2]   */
extern s32  gGameOptions[];        /* prefs/config block                 */
extern s32  lbl_802577CC[];        /* 8 keys                             */
extern s8*  lbl_8025776C[];        /* 8 parallel object pointers         */

/* SDA-relative scalars (all in .sdata/.sbss). */
extern s32   lbl_80343C0C;
extern u64   gControllerButtons;
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
extern s32   gIdentityMatrix[];       /* node template (ADDR16) */
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
extern void  fn_80053D08(s32 a, s32 b, s32 c);
extern void  SelectLoadStart(void);
extern s32   SelectLoadDone(void);
extern void  FontInitSpecial(void* def, s32 font);
extern void  ShopLoadData(void);
extern void  LoadItems(void);

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
extern void* MBCreateBlit(s32 a, void* tex, s32 c, s32 d, s32 e, s32 f);
extern void  mbBlitProject(void* blit, s32 a, s32 b);
extern void  mbBlitCvtCoord(void* blit, f32 c);
extern void  mbBlitSetupVerts(void* blit, f32 a, f32 b, f32 c, f32 d);
extern void  mbBlitCalcY(void* blit, s32 y);
extern s32   Round(f32 value);
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
extern s32   lbl_80344D80;
extern s32   lbl_80344D84;
extern s32   lbl_80344D88;
extern s32   dbgTextEnable;
extern s32   mlmMemUsed;
extern void  ErrorPrintf(const char* fmt, ...);

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
extern u8    gPlayers[];       /* world descriptor array, stride 13148 */
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
extern void  LoadPowerups(s32 arg0);
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
void do_stats_display(void);
void LoadTowerAndSelect(void);
s32  init_next_level_8005638C(s32 arg0);
s32  init_next_level_8005638C(s32 arg0);
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

/* Xbox PDB: format_brain -- initialize a newly allocated enemy's AI state. */
void format_brain(s32 index)
{
    u8* enemy = (u8*)gEnemies + index * 916;

    *(f32*)(enemy + 764) = lbl_80346A7C;
    *(u32*)(enemy + 816) = 0;

    switch (*(s16*)(enemy + 784)) {
    case 0:
        *(s32*)(enemy + 852) = 1;
        *(s16*)(enemy + 868) = 0;
        break;
    case 3:
        *(s32*)(enemy + 852) = 1;
        *(s16*)(enemy + 868) = 0;
        *(s32*)(enemy + 804) = 0;
        *(s32*)(enemy + 808) = -1;
        *(s32*)(enemy + 800) = 0;
        break;
    case 2:
    case 4:
        *(s32*)(enemy + 884) = 0;
        *(u32*)(enemy + 816) |= 1;
        break;
    case 5:
    case 6:
        *(u32*)(enemy + 816) |= 1;
        break;
    case 7:
        *(s32*)(enemy + 852) = 0;
        *(s16*)(enemy + 868) = 0;
        *(s16*)(enemy + 870) = 0;
        break;
    case 8:
        *(s32*)(enemy + 852) = 0;
        *(s16*)(enemy + 868) = 0;
        *(s16*)(enemy + 870) = 0;
        *(s32*)(enemy + 828) = 0;
        *(s32*)(enemy + 832) = -1;
        *(f32*)(enemy + 836) = lbl_803468B0;
        break;
    case 9:
        *(s32*)(enemy + 208) = 0;
        *(u32*)(enemy + 816) |= 5;
        break;
    case 10:
        *(s32*)(enemy + 852) = 0;
        *(s16*)(enemy + 868) = 0;
        *(s16*)(enemy + 870) = 0;
        break;
    case 11:
        *(u32*)(enemy + 816) |= 7;
        break;
    case 12: {
        f32 zero = lbl_80346820;
        *(f32*)(enemy + 736) = zero;
        *(f32*)(enemy + 740) = zero;
        *(f32*)(enemy + 744) = zero;
        *(s32*)(enemy + 852) = 0;
        *(s16*)(enemy + 868) = 0;
        *(s16*)(enemy + 870) = 0;
        break;
    }
    case 13:
        *(s32*)(enemy + 852) = 0;
        *(s16*)(enemy + 868) = 0;
        *(s16*)(enemy + 870) = 0;
        break;
    case 14:
        *(s32*)(enemy + 852) = 0;
        *(s16*)(enemy + 868) = 0;
        *(s16*)(enemy + 870) = 0;
        *(s32*)(enemy + 804) = 0;
        break;
    case 16:
    case 23:
        *(s32*)(enemy + 800) = RandInt(10);
        *(s32*)(enemy + 804) = 0;
        break;
    case 17:
    case 26:
        *(s32*)(enemy + 800) = RandInt(10);
        *(s32*)(enemy + 804) = 0;
        break;
    case 20:
        *(s32*)(enemy + 852) = 0;
        *(s16*)(enemy + 868) = 0;
        *(s16*)(enemy + 870) = 0;
        break;
    case 21:
        *(s32*)(enemy + 804) = 0;
        break;
    case 24:
        *(s32*)(enemy + 804) = 0;
        break;
    case 27:
        *(s32*)(enemy + 852) = 1;
        *(s16*)(enemy + 868) = 0;
        break;
    case 28:
    case 29:
    case 31:
        *(s32*)(enemy + 800) = RandInt(30);
        *(s32*)(enemy + 804) = 0;
        *(s32*)(enemy + 808) = 0;
        break;
    case 30:
        *(s32*)(enemy + 852) = 1;
        *(s16*)(enemy + 868) = 0;
        *(s32*)(enemy + 804) = RandInt(60) + 60;
        break;
    }

    *(s32*)(enemy + 860) = 0;
    *(s32*)(enemy + 856) = 0;
    *(s32*)(enemy + 840) = -1;
    *(s32*)(enemy + 844) = 0;
    *(s32*)(enemy + 848) = 4;
    *(s16*)(enemy + 726) = 0;
    *(s32*)(enemy + 880) = PlayersAverageLevel();
    *(s32*)(enemy + 872) = 8;
    *(s32*)(enemy + 876) = RandInt(*(u32*)(enemy + 872));
}

/* Rebuild an enemy's animation tree, render object, actions, and shadow. */
void SetEnemyObj(u8* enemy, s32 type, s32 level, s32 unused)
{
    void* object;
    s32 shadowObject;
    s32 shadowLevel;

    if (*(s32*)enemy == 27) {
        SfxDeleteParented(*(void**)(enemy + 100), 0, -1);
    }
    if (*(void**)(enemy + 108) != 0) {
        AtreeDelete(enemy + 108);
    }
    if (*(void**)(enemy + 100) != 0) {
        lbl_80344734 = 1;
        MBRemoveNode(*(void**)(enemy + 100), 0);
        lbl_80344734 = 0;
    }
    *(void**)(enemy + 108) = 0;
    *(void**)(enemy + 100) = 0;
    *(f32*)(enemy + 540) = lbl_80346820;

    if (type == 31) {
        *(void**)(enemy + 108) = (void*)fn_80011BBC(
            sGoodWizObj, lbl_80346770, enemy + 108, lbl_80346770, 2048);
        *(f32*)(enemy + 540) = lbl_80346A80;
    } else if (gWadAtreeHeaders[type] != 0) {
        char* name = fn_80051E1C(type, level, 0);
        *(void**)(enemy + 108) = (void*)fn_80011BBC(
            gWadAtreeHeaders[type], name, enemy + 108, name, 2048);
    }

    if (*(void**)(enemy + 108) != 0) {
        *(void**)(enemy + 100) = MBNewNode(lbl_8034473C,
                                            (void*)gIdentityMatrix, 1);
        MBNodeSetParent(**(void***)(enemy + 108), *(void**)(enemy + 100));
        InitActions(enemy + 108, enemy + 212, lbl_80126EC0);
    } else {
        InitActions(0, enemy + 212, lbl_80126EC0);
    }

    if (*(void**)(enemy + 100) == 0) {
        char* name = fn_80051E1C(type, level, 1);
        *(void**)(enemy + 100) = MBOX_NewObject(name, (f32*)gIdentityMatrix,
                                                (void*)lbl_8034473C, 0);
        MBTreeSetFlags(*(void**)(enemy + 100), 2048, 0);
    }

    if (*(void**)(enemy + 476) != 0) {
        MBRemoveNode(*(void**)(enemy + 476), 0);
        *(void**)(enemy + 476) = 0;
    }

    if (*(s32*)enemy != 30 && *(s32*)enemy != 0 &&
        *(s32*)enemy != 31 && *(s32*)enemy != 21) {
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
        *(void**)(enemy + 476) = MBNewObject(object, (f32*)gIdentityMatrix,
                                             0, 2176);
        *(f32*)(*(u8**)(enemy + 476) + 84) = lbl_80346A80;
        *(s16*)(*(u8**)(enemy + 476) + 104) = -32;
    }
}

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
        fn_8005A1EC(buf, (void**)((u8*)pool + id + 1380));
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
        LoadModel(buf, (void**)((u8*)pool + offset + 1380), 0, -1);
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
    s32* e = gEnemies;
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
    s32 idx = ((s32)wobj - gWorldInfo[1]) / 60;   /* wobjs @0x04, stride 60 */
    u8* wa = (u8*)gWorldInfo[35];                  /* worldanims @0x8C       */
    s32 i;

    for (i = 0; i < gWorldInfo[36]; i++) {         /* nworldanims @0x90      */
        if (worldAnimIndexMatches(*(s16*)(wa + i * 16), idx)) {
            return (s16*)(wa + i * 16);
        }
    }
    return 0;
}

/* Advance every active world-object animation track. */
void DoWorldAnimation(void)
{
    s32* wi;
    s32* count;
    s32* header;
    u8* data_off;
    u8* track_off;
    s32 i;
    u8* anim_base;

    if ((void*)gWorldInfo[32] != NULL) {
        DoTexMods((void*)gWorldInfo[32]);
    }
    if ((lbl_80344768 > 0 || (gGameMode & 0x8000) != 0) &&
        (lbl_803443BC <= 10 || lbl_803443BC >= 100000) &&
        gWorldInfo[36] != 0 && (void*)gWorldInfo[37] != NULL) {
        data_off = NULL;
        track_off = NULL;
        wi = gWorldInfo;
        count = &wi[36];
        header = (s32*)wi[37];
        anim_base = (u8*)header[3];
        i = 0;
        lbl_803441B8 = header[0];
        lbl_803441B4 = header[1];
        lbl_803441B0 = header[2];
        while (i < *count) {
            u8* track = (u8*)wi[35] + (u32)track_off;
            if (*(u32*)(track + 12) != 0) {
                DoWorldAnimSub(track, (u8*)wi[38] + (u32)data_off, anim_base);
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
        if (*(s32*)e == world) {
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

/* 0x8005403C -- lock the model box, then run AtreeListLock. */
void LockModels(s32 arg0)
{
    MBOX_LockModels();
    AtreeListLock(arg0);
}

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

void GetEnemyTypes(void)
{
    u8* tbl = (u8*)lbl_80257680;
    u8* etab = *(u8**)(gWorldData + 0x20);
    s32 seen1e = 0;
    s32 i;

    AudioClearActiveTracks();
    for (i = 0; i < 8; i++) {
        u8* slot = tbl + i * 4;
        s32 type;
        s32 t14c;

        if (i < 6) {
            type = *(s16*)(gCurLevel + i * 2 + 0x4C);
        } else {
            type = -1;
        }
        if (type >= 0) {
            u8* ent = etab + type * 0x18;

            *(s32*)(slot + 0x14C) = *(s32*)ent;
            *(s32*)(slot + 0x10C) = *(s32*)(ent + 0x4);
            if (*(s32*)(ent + 0x4) != 9 && *(s32*)(ent + 0x4) != 5) {
                AudioSetupBossStreams(i, ent + 0x8);
            }
            *(u8**)(slot + 0xEC) = ent;
        } else if (*(s32*)(gCurLevel + 0x44) < 0 && *(s32*)gWorldData != 0xD &&
                   seen1e == 0) {
            *(s32*)(slot + 0x14C) = 0x1E;
            *(s32*)(slot + 0x10C) = 0;
            *(s32*)(slot + 0xEC) = 0;
        } else {
            *(s32*)(slot + 0x14C) = -1;
            *(s32*)(slot + 0x10C) = 0;
            *(s32*)(slot + 0xEC) = 0;
        }

        t14c = *(s32*)(slot + 0x14C);
        if (t14c == 0x1E) {
            seen1e = 1;
        }
        if (t14c >= 0) {
            if (*(s32*)(slot + 0x10C) == 0) {
                *(s32*)(slot + 0x10C) = GetEnemySubtype(t14c);
            }
            if (*(s32*)(slot + 0x10C) <= 0) {
                ErrorPrintf(lbl_801129D4, t14c);
            }
        }
    }

    for (i = 0; i < 8; i++) {
        *(s32*)(tbl + i * 4 + 0x12C) = -1;
    }
    for (i = 0; i < 8; i++) {
        s32 idx = *(s32*)(tbl + i * 4 + 0x10C);

        if (idx < 6) {
            *(s32*)(tbl + idx * 4 + 0x12C) = *(s32*)(tbl + i * 4 + 0x14C);
        }
    }
    if (gGameOptions[2] < 2) {
        for (i = 0; i < 8; i++) {
            if (*(s32*)(tbl + i * 4 + 0x10C) == 2) {
                *(s32*)(tbl + i * 4 + 0x10C) = 4;
            } else {
                *(s32*)(tbl + i * 4 + 0x14C) = -1;
            }
        }
    }
}

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

/* 0x80054D18 -- choose and resolve the next world/level selection. */
s32 next_world(void)
{
    u8 unused[8];
    s32 world;
    s32 forced = 0;
    s32 state = lbl_8034481C;
    s32 transitioning = 0;

    if (state >= 13 && state < 0x10000) {
        transitioning = 1;
    }
    if (state >= 2) {
        forced = 1;
    }
    if (transitioning) {
        world = lbl_80344B84;
        forced = 1;
    } else if (sLastWorldLevel < 0) {
        world = gGameOptions[9];
        if ((world >> 8) >= 14) {
            world = sFirstWorldId;
        }
        lbl_8034481C = world + 0x10000;
        forced = 1;
    } else {
        s32 i;
        s32 offset;

        world = -1;
        for (i = 0, offset = 0; i < 4; i++, offset += 13148) {
            s32 state = *(s32*)(gPlayers + offset + 232);
            if (state != 0 && state != 2 &&
                world < *(s32*)(gPlayers + offset + 2096)) {
                world = *(s32*)(gPlayers + offset + 2096);
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
    if (!forced && ((*(s16*)(gCurLevel + 4) & 1) == 0)) {
        world = NextWorldLevel(1);
        ResolveWorldData(world);
    }
    return world;
}

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
    fn_8009C8A0(arg0);
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
                         (f32*)((u8*)lbl_8034479C + 0x30));
            for (i = 0; i < 3; i++) {
                *(f32*)((u8*)lbl_8034479C + 0x40 + i * 4) = lbl_80343C18;
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
    u8* base = gPlayers;
    s32 offset = 0;
    s32 bossType = gBossType;
    s32 count1 = 0;
    s32 count2 = 0;
    s32 count3 = 0;
    u8* e;
    s32 i;
    s32 type;
    s32 f292;

    lbl_803447D4 = lbl_803447D8;
    lbl_803447DC = offset;
    lbl_803447D8 = lbl_80346AF0;
    lbl_803447E0 = offset;
    for (i = 0; i < 4; i++, offset += 13148) {
        e = base + offset;
        type = *(s32*)(e + 232);
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
            f292 = *(s32*)(e + 292);
            if (f292 & 0x8) {
                lbl_803447DC = 1;
            }
            if (bossType < 0 && (f292 & 0x200)) {
                lbl_803447D8 = lbl_803447D8 * lbl_80346B00;
            }
        }
        *(s16*)(e + 2406) = 0;
    }
    fn_8005207C(count1, count2, count3);
}
#pragma opt_propagation reset

/* 0x80050FB0 -- resolve a type id through the override tables. */
s32 GetEnemyType(s32 arg0, s32 arg1)
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

/* 0x80051480 -- return the milestone nearest to a world-space position. */
s32 fn_80051480(f32* pos)
{
    u8 unused[16];
    s32 best_idx = -1;
    register f32 best_dist = 100000.0f;
    u8* node = sMilestones;
    s32 i;

    for (i = 0; i < sNumMilestones; i++) {
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
            best_dist = d;
            best_idx = i;
        }
        node += 104;
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

typedef struct WorldLevelNav {
    u32 flags;
    s16 flags2;
    u8 _06[0x106];
} WorldLevelNav;

typedef struct WorldDataNav {
    u8 _00[0x16];
    s16 curLevel;
    s16 numLevels;
    u8 _1A[2];
    WorldLevelNav* levels;
} WorldDataNav;

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
    s32 startIndex;
    s32 tableOffset;
    s32 level;
    s32 worldBits;
    u8* worldTable = sWorldLevelTable;

    for (worldIndex = 0; worldIndex < 14; worldIndex++) {
        if (worldType == *(s32*)(worldTable + worldIndex * 44 + 232)) {
            break;
        }
    }
    if (worldIndex == 14) {
        worldIndex = 0;
    }
    startIndex = worldIndex;

    do {
        do {
            worldIndex++;
            if ((u32)worldIndex >= 14) {
                worldIndex = 0;
            }
            tableOffset = worldIndex * 44;
        } while (*(s32*)(worldTable + tableOffset + 248) == 0 &&
                 worldIndex != startIndex);

        level = *(s32*)(worldTable + tableOffset + 272);
        if (level >= *(s32*)(worldTable + tableOffset + 252)) {
            level = 0;
        }
        worldBits = *(s32*)(worldTable + tableOffset + 232) << 8;
        ResolveWorldData(worldBits | (level & 0xFF));

        if ((gControllerButtons & 0x10) == 0) {
            s32 originalLevel = level;
            s32 numLevels = *(s32*)(worldTable + tableOffset + 252);
            WorldLevelNav* levels = ((WorldDataNav*)gWorldData)->levels;

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
        worldIndex = worldBits | (level & 0xFF);
        ResolveWorldData(worldIndex);
        level++;
        if (level >= *(s32*)(worldTable + tableOffset + 252)) {
            level = 0;
        }
        *(s32*)(worldTable + tableOffset + 272) = level;
        return worldIndex;
    } while (1);
}

/* 0x80057D94 -- move backward to a level accepted by waveMask, wrapping
 * through the loaded-world table when the current world is exhausted. */
#pragma opt_propagation off
#ifdef __MWERKS__
asm s32 PrevWorldLevel(s32 waveMask)
{
    nofralloc
    lwz r7, gWorldData(r0)
    lis r4, sWorldLevelTable@ha
    lwz r8, sCurWorldIndex(r0)
    addi r6, r4, sWorldLevelTable@l
    cmplwi r7, 0
    addi r9, r8, 0
    bne prev_have_world
    lis r3, gGameOptions@ha
    addi r3, r3, gGameOptions@l
    lwz r3, 36(r3)
    blr
prev_have_world:
    cmpwi r3, -1
    bne prev_get_level
    li r10, -1
    b prev_check_wrap
prev_get_level:
    lha r4, 22(r7)
    cmpwi r3, 0
    addi r10, r4, -1
    beq prev_check_wrap
    mulli r4, r10, 268
    b prev_wave_test
prev_wave_next:
    addi r10, r10, -1
    addi r4, r4, -268
prev_wave_test:
    cmpwi r10, 0
    blt prev_check_wrap
    lwz r5, 28(r7)
    addi r0, r4, 4
    lhax r0, r5, r0
    and. r0, r3, r0
    beq prev_wave_next
prev_check_wrap:
    cmpwi r10, 0
    bge prev_return
    li r10, 0
prev_world_loop:
    addic. r9, r9, -1
    bge prev_world_valid
    li r9, 13
prev_world_valid:
    mulli r4, r9, 44
    add r3, r6, r4
    lwz r0, 248(r3)
    cmpwi r0, 0
    bne prev_world_found
    cmpw r9, r8
    bne prev_world_loop
prev_world_found:
    add r3, r6, r4
    lwz r3, 252(r3)
    cmpwi r3, 0
    blt prev_return
    addi r10, r3, -1
prev_return:
    mulli r0, r9, 44
    add r3, r6, r0
    lwz r0, 232(r3)
    slwi r3, r0, 8
    rlwimi r3, r10, 0, 24, 31
    blr
}
#else
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
        level = ((WorldDataNav*)gWorldData)->curLevel - 1;
        if (waveMask != 0) {
            while (level >= 0 &&
                   (waveMask & ((WorldDataNav*)gWorldData)->levels[level].flags2) == 0) {
                level--;
            }
        }
    }

    if (level < 0) {
        level = 0;
        for (;;) {
            s32 entryOffset;
            WorldLevelTableNav* entry;

            worldIndex--;
            if (worldIndex < 0) {
                worldIndex = 13;
            }
            entryOffset = worldIndex * 44;
            entry = (WorldLevelTableNav*)((u8*)worldTable + entryOffset);
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
#endif
#pragma opt_propagation on

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

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
        level = ((WorldDataNav*)gWorldData)->curLevel + 1;
        if (waveMask != 0) {
            while (level < ((WorldDataNav*)gWorldData)->numLevels &&
                   (waveMask & ((WorldDataNav*)gWorldData)->levels[level].flags2) == 0) {
                level++;
            }
        }
    }

    if (level >= ((WorldDataNav*)gWorldData)->numLevels) {
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
    char* fatal;
    u32 i;

    fatal = (char*)lbl_801129F8;
    for (;;) {
        wt = code >> 8;
        sub = code & 0xFF;
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
                FatalError(fatal, 0x800000);
            }
        } else {
            if (gWorldData != 0) {
                if (sub >= *(s16*)(gWorldData + 24)) {
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
        if (*(s16*)(gCurLevel + 4) & mask) {
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

void fn_80051C78(void)
{
    u8* pool = (u8*)lbl_80250E00;
    s32 best = -1;
    s32 cur;
    s32 i;

    for (i = 0; i < 128; i++) {
        *(s32*)(pool + i * 4 + 0xF4) = -1;
    }
    lbl_80344724 = 0;

    {
        f32 bestDist = lbl_803468B0;
        u8* m = sMilestones;

        for (i = 0; i < sNumMilestones; i++, m += 0x68) {
            f32 dy = gDefaultPlayerPosition[1] - *(f32*)(m + 0x34);
            f32 dx = gDefaultPlayerPosition[0] - *(f32*)(m + 0x30);
            f32 dz = gDefaultPlayerPosition[2] - *(f32*)(m + 0x38);
            f32 d2 = dy * dy + dx * dx + dz * dz;

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
        *(s32*)(pool + count * 4 + 0xF4) = cur;
        cur = fn_800511D0(cur, lbl_80346984);
        count = lbl_80344724;
        for (k = 0; k < count; k++) {
            if (*(s32*)(pool + k * 4 + 0xF4) == cur) {
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

/* 0x80051E1C - format a world/level display name (uppercased) */
extern char lbl_80346A90[8];    /* "%s" fmt */
extern char lbl_80346A98[8];    /* "%s %c" fmt */
extern char lbl_80346AA0[8];    /* suffix */
extern char lbl_80343BF8[5];    /* level letter table */

char* fn_80051E1C(s32 world, s32 lvl, s32 flag)
{
    char* buf = (char*)lbl_80250E00;
    s32 n;
    u32 i;

    n = lvl;
    if (lvl == 0) {
        n = 1;
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
extern u8* sItems;
extern f64 lbl_80346A88;        /* health floor */
extern f32 lbl_803468B0;        /* big initial distance */
extern f32 lbl_803469D4;        /* grid radius */
extern f32 lbl_80346820;        /* 0.0f */
extern f64 lbl_80346830;        /* 0.5 */
extern f64 lbl_803468B8;        /* 3.0 */
extern f64 __frsqrte(f64 x);
extern void StartEnemyGrid(f32* pos, f32 radius);
extern s32 NextGridEnemy(void);

void fn_80051568(s32 index)
{
    u8* e = (u8*)gEnemies + index * 916;
    s32 i;
    s32 off;
    u8* it;
    u8* hdr;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 dist2;
    f64 kHalf;
    f32 kZero;
    f64 kThree;
    u8 _spare[24];

    if (*(s16*)(e + 628) >= 0 && *(f32*)(e + 636) <= lbl_80346A88) {
        *(s32*)(e + 828) = 0;
        *(s32*)(e + 832) = -1;
        *(f32*)(e + 836) = lbl_803468B0;
        return;
    }
    if (*(s32*)(e + 828) != 0) {
        return;
    }
    StartEnemyGrid((f32*)(e + 52), lbl_803469D4);
    kZero = lbl_80346820;
    kHalf = lbl_80346830;
    kThree = lbl_803468B8;
    while ((i = NextGridEnemy()) >= 0) {
        off = i * 240;
        it = (u8*)sItems + off;
        hdr = *(u8**)it;
        if (*(s16*)(it + 196) == -1) {
            continue;
        }
        if (*(s32*)hdr != 2) {
            continue;
        }
        if (*(s8*)(it + 205) != 0) {
            continue;
        }
        dx = *(f32*)(it + 52) - *(f32*)(e + 52);
        dy = *(f32*)(it + 56) - *(f32*)(e + 56);
        dz = *(f32*)(it + 60) - *(f32*)(e + 60);
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
        if (dist2 < *(f32*)(e + 836)) {
            *(f32*)(e + 836) = dist2;
            *(s32*)(e + 832) = i;
            *(s32*)(e + 828) = 1;
        }
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
        if (*(s32*)((u8*)gPlayers + i * 13148 + 232) == 1) {
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
extern u32 sSpecialItem10;
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
extern s32 mbBlitReset33F8(void* blit);
extern void fn_8009FF54(f32* pos);
extern void fn_8009FFA4(f32* pos);

/* 0x80055678 -- update the special-item proximity meter blits from the
 * distance between the two given points. */
void fn_80055678(f32* a, f32* b)
{
    f32 d;
    f32 v;
    f64 t;
    f32 lvl;
    f32 f6;
    f32 x;
    f64 lvl2;
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
            volatile f32 tmp;
            f64 y = __frsqrte(d);
            y = lbl_80346B90 * y * (lbl_80346B98 - y * y * d);
            y = lbl_80346B90 * y * (lbl_80346B98 - y * y * d);
            y = lbl_80346B90 * y * (lbl_80346B98 - y * y * d);
            d = (f32)(d * (lbl_80346B90 * y * (lbl_80346B98 - y * y * d)));
            tmp = d;
            d = tmp;
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
        t = lbl_80346BA8 - lvl;
        f6 = (f32)t;
        lvl2 = lbl_80346BC8 * f6;
        mbBlitSetupVerts((void*)lbl_803447A8[1], lbl_80346B20, lbl_80346B20,
                         (f32)((lbl_80346BC0 - lvl2) * lbl_80346B38),
                         lbl_80346B20);
        mbBlitProject((void*)lbl_803447A8[1], 0, Round((f32)lvl2) + 27);
        mbBlitCalcY((void*)lbl_803447A8[1], 102 - Round((f32)lvl2));
    }
}
