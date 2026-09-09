#include "types.h"
#include "game/controls.h"
#include "game/critter.h"
#include "game/enemy.h"
#include "game/gamemode.h"
#include "game/item.h"
#include "game/effect.h"
#include "game/leveldata.h"
#include "game/mbobject.h"
#include "game/worldinfo.h"
#include "game/player.h"
#include "game/camera.h"

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

/* MWCC deferred inlining emits these definitions in reverse order. The
 * reversed public definitions and deferred BSS declarations recover retail
 * text order, the ten BSS objects, and the complete 0x130-byte numeric pool.
 * The string region is reconstructed below as typed objects and literals.
 * game_main's adjacent instruction-order pair remains nonexact; fallback stays
 * selected until complete source-linked code/data/EH verification succeeds. */

extern level_data* gCurLevel;
extern s32 gGameOptions[];
extern s32 lbl_80257640[];
extern void* lbl_80257630[4];

/* These names describe verified uses, not recovered original identifiers.
 * The renderer accepts packed RGB; FatalError records its second argument as
 * an error code. next_world adds WORLD_OVERRIDE_BASE to an explicit selection,
 * distinguishing it from the ordinary exit-reason range below that value. */
enum {
    TEXT_RGB_WHITE = 0xFFFFFF,
    WORLD_LOAD_TIMEOUT_ERROR = 0x8000,
    WORLD_OVERRIDE_BASE = 0x10000,
    WORLD_RANDOM_SEED = 0x12D687
};

/* SDA-relative scalars (all in .sdata/.sbss). */
extern s32   lbl_80343C0C;
extern u64   gControllerButtons;
extern s32   lbl_80344A2C;
extern s32   lbl_80343C10;
extern s32   lbl_80343DD4;
extern s32   lbl_80343B38;
extern s32   lbl_803448AC;
extern s32   lbl_803448A8;
extern void* lbl_803447B0;
extern s32   gBossType;            /* 0x8034439C */

/* Game-mode ids held in gGameMode: enum e_mode, now in game/gamemode.h
 * (included above) so consumer TUs can reference the names too. */

extern s32   gGameMode;
/* Xbox MILESTONE is an OBJGRP wrapper (misc.h, size 0x68). GC confirms
 * stride 104 and node at +96 in fn_80055AFC; ShowMilestones supplies the
 * matrix to add_arrow and stores that returned node at the same offset.
 * Keep this declaration local: no shared-header layout change is needed. */
typedef struct MILESTONE {
    OBJGRP objgrp;
} MILESTONE;
extern MILESTONE sMilestones[];
extern s32   sNumMilestones;
extern f64   __frsqrte(f64 x);
extern f32   gIdentityMatrix[];       /* identity matrix */
DECL_SECT(".sdata2") extern const char lbl_80346770[];

extern void  AtreeAlloc(s32 a, s32 b);
extern void* MBOX_NewObject(const char* name, f32* matrix, void* parent, u32 flags);

/* game_init_data externs. */
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
extern s32   lbl_8034481C;
extern s32   sLastWorldLevel;
extern s32   sFirstWorldId;
extern u32   pbLoad;
extern s32   gDemoMode;
extern s32   opt_force_player;
extern void* lbl_8034479C;
extern s32   options_state;
extern s32   optionsAudioAndPrefs30[];
extern s16   lbl_80343C14[2];
extern f32   lbl_80343C18;
extern f32   lbl_80343C1C;
extern s32   lbl_80343C20;
/* Verified partial view of mb_window.c's MBWINDOW/MBCamNode.
 * The camera basis starts at +0x64; position follows its twelve floats. */
typedef struct GamemainWindowCamera {
    f32 mat[12];
    f32 pos[3];
} GamemainWindowCamera;
typedef struct GamemainWindowView {
    f32 xscale;
    f32 yscale;
    f32 xcenter;
    f32 ycenter;
    f32 unk10;
    f32 width;
    f32 height;
    f32 ang;
    f32 hang;
    f32 tanAng;
    f32 tanHang;
    f32 cotAng;
    f32 cotHang;
    f32 cosAng;
    f32 cosHang;
    f32 rect1[4];
    f32 rect2[4];
    f32 nearZ;
    f32 farZ;
    GamemainWindowCamera cam;
} GamemainWindowView;
extern GamemainWindowView* lbl_80344EE8;
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
extern void  SelectLoadStart(void);
extern s32   SelectLoadDone(void);
extern void  FontInitSpecial(void* def, s32 font);
extern void  ShopLoadData(void);
extern void  LoadItems(void);
extern void  EndFireScroll(void);
extern void  DeleteOptionBlits(void);
extern void  SumnerEnd(void);
extern void  Randomize(u32 seed);
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
/* World IDs from WAVETYPE in the Xbox symbols, independently confirmed by
 * the little-endian WRLD.type in all 14 Gauntlet/WDATA/*.WAD files. The
 * GC ResolveWorldData path stores worldlevel >> 8 in sMusicTrackHi: despite
 * that provisional symbol name, these comparisons select a world, not music.
 * MOUNT.WAD is MOUNTAIN; TEMPLE.WAD is BOSSWAVE. */
enum WAVETYPE {
    TESTWAVE = 0,
    CASTLE = 1,
    MOUNTAIN = 2,
    DESERT = 3,
    FOREST = 4,
    BOSSWAVE = 5,
    HELL = 6,
    TOWN = 7,
    BATTLE = 8,
    ICE = 9,
    DREAM = 10,
    SKY = 11,
    SECRET = 12,
    TOWER = 13,
    NUMWORLDS = 14
};
extern s32   sMusicTrackHi;        /* 0x803448D8: current WAVETYPE, or -1 */
extern s32   lbl_803447A4;
extern s32   lbl_803447E4;
extern s32   lbl_803447EC;
extern s32   lbl_803447F0;
extern s32   lbl_803447F4;
extern s32   lbl_80344810;
extern f32   lbl_80344814;
extern f32   lbl_80344818;
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

extern s32 MBOX_BGLoadModelDone(void);

/* fn_800521E8 / SetPlayerVars externs. */
extern s32   gGameBusy;
extern s32   lbl_80344774;
extern s32   gFrameTicks;
extern s32   lbl_80344778;
extern s32   lbl_803441F8;
extern void  fn_8009FB00(void);
extern f32   SetDrawStringScale(f32 s);
/* Local mirror of mb_font.c's queued message, returned through the opaque
 * DrawStringText API. GC MBDrawText uses a 44-byte stride and initializes
 * every field below; its +0x10 pointer addresses the copied character data.
 * fn_800521E8 truncates that copy, not the original string resource. */
typedef struct MBTextMsg {
    u32 flags;
    s32 x;
    s32 y;
    f32 z;
    char* text;
    f32 xspace;
    f32 xscale;
    f32 yspace;
    f32 yscale;
    s16 font;
    s16 seq;
    u32 color;
} MBTextMsg;
/* DrawStringText's GC va_list starts after six GPR arguments; the message
 * index is fixed, not the first variadic argument. */
extern void* DrawStringText(s32 x, s32 y, u32 flags, u32 color,
                            s32 message, s32 index, ...);
extern f32   RestoreDrawStringScale(void);
extern void  init_attract_mode(s32 mode);
extern Player gPlayers[];      /* gPlayerRecords[4], stride 13148 (0x335C) */
extern f32   lbl_803447D4;
extern f32   lbl_803447D8;
extern s32   lbl_803447DC;
extern s32   lbl_803447E0;

/* Called helpers; pointer/FPR argument order follows the owning APIs. */
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
extern void  MBOX_LockModels(s32 slot);
extern void  AtreeListLock(s32 arg0);
extern void  AudioStopSelect(void);
extern void  init_prefs(void);
extern void  MBTreeSetFlags(void* node, s32 flags, s32 arg2);
extern void  MBTreeClearFlags(void* node, s32 flags, s32 arg2);
extern void  MBTreeSetAlpha(void* node, s32 alpha, s32 arg2);
extern void  MBWindowTo3D(s16* screen, GamemainWindowCamera* camera,
                          f32* out, f32 depth);

/* Other game entry points. */
extern s32 init_next_level_8005638C(s32 arg0);   /* gauntworld.obj */
void GetEnemyTypes(void);
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
s32 fn_80051480(f32* pos);
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

extern void* lbl_80344EA8;

/* Shared controller record, as reconstructed by controls.c (0x3C stride).
 * The tally helpers read held buttons, not a character descriptor. */

extern void DrawTextKeepScale(f32 scale, s32 x, s32 y, s32 flags, s32 color,
                              const char* fmt);
extern s32  DrawNormalText(f32 scale, char* s, s32 flags);
extern void WritePlayerInfo(s32 player);
extern void fn_8009FCA8(s32 arg0);
extern void AudioStopMusicA(void);
extern s32  strcmp(const char* a, const char* b);
struct MBBLIT;
extern int MBRemoveBlit(struct MBBLIT* blit);

/* Independent GAMEMAIN statics: GC references establish the element widths
 * and layout; Xbox GAMEMAIN.OBJ corroborates these names and array bounds. */
static int screenblitxy[4][2] = {{0, 359}, {256, 359}, {0, 103}, {256, 103}};
/* The statistics string objects occupy retail .rodata+0..0x5B. Names are
 * descriptive, not recovered identifiers. The last two have no recovered
 * caller; the Xbox PDB also names an unrecovered statistics initializer.
 * Retain the known data without inventing that missing function's body.
 * Array lengths follow the strings, with alignment left to the compiler. */
static const char stats_title[] = "FINAL STATS";
static const char stats_generators_label[] = "GENERATORS";
static const char stats_treasures_label[] = "TREASURES";
static const char stats_playtime_label[] = "PLAYTIME";
static const char stats_playtime_format[] = "%3d:%02d:%02d";
static const char stats_boss_texture[] = "bosstats";
static const char stats_tally_texture_format[] = "TALBOARD%02d";

static void* stats_bg_blit[4];
static int tbuf_treasures[4];
static int tbuf_enemies[4];
static int tbuf_generators[4];
static int tbuf_playtime[4];
static int tbuf_timer[4];
static int tbuf_step[4];
/* SandglassBlit[4], soft_reset[4] and restore_pos[4][3] in the Xbox PDB.
 * GC references confirm their respective 16-, 16- and 48-byte extents. */
f32 lbl_80257650[4][3];
s32 lbl_80257640[4];
void* lbl_80257630[4];
static int stat_lx[4] = {0, 0, 256, 256};
static int stat_cx[4] = {128, 128, 384, 384};
static int stat_rx[4] = {245, 245, 501, 501};
static int stat_ty[4] = {33, 177, 33, 177};
static int stat_yoff[7] = {4, 26, 46, 66, 86, 106, 126};

/* GC small-data initializers. The compass position is two signed shorts,
 * confirmed by both MBWindowTo3D's reads and Xbox's CompassPos[2]. Its
 * scale, depth and alpha are separate scalars, not fields in a packed blob.
 * The final four zero bytes of the claimed range are alignment slack. */
s32 lbl_80343C00 = -1;
s32 lbl_80343C04 = -1;
f32 lbl_80343C08 = 1.0f;
s32 lbl_80343C0C = 30;
s32 lbl_80343C10 = -1;
s16 lbl_80343C14[2] = {64, 128};
f32 lbl_80343C18 = 1.5f;
f32 lbl_80343C1C = 10.0f;
s32 lbl_80343C20 = 128;

#define CHAR_STAT(p) ((p)->save.stats[(p)->character])

extern char lbl_80112370[];        /* format-string blob */
int sprintf(char* s, const char* fmt, ...);
void* fn_80057ACC(s32 key);

extern s32 lbl_803447CC;
extern s32 lbl_803447E8;
extern s32 lbl_80344780;
extern s32 ShowMilestones(s32 idx);
extern s32 msgPost(s32 message, s32 player, char* position);

extern void* lbl_803447A8[2];        /* meter blit handles */
extern Item* sSpecialItem10;
extern s32 lbl_80344790;
extern s32 lbl_8034478C;
extern f32 lbl_80343C08;
extern s32 mbBlitReset33F8(void* blit);
extern s32 PlayerHasShard(s32 player, s32 shard);
extern s32 PlayerHasRune(s32 player, s32 rune);
extern s32 GetWorldOrder(s32 world);
extern void fn_8009FF54(f32* pos);
extern void fn_8009FFA4(f32* pos);

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
extern void player_store_in_save(void* pl);
extern void PlayerRestoreState(s32 player);
extern void EnterTower(void);
extern void InitCamera(s32 mode);
extern u32  lbl_80344824;
extern void load_player(s32 player);
extern void add_target(void* mat);
extern void LoadPlyrData(s32 player, s32 pad, s32 mode);
extern void CopyMat3(f32* src, f32* dst);
extern f32  lbl_80257650[4][3];
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

extern s32  lbl_803447B8;
extern s32  gGameplayPauseTimer;
extern f32  gClockFrameStep;
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
extern s32  init_mapscreen(s32 a, s32 b);
extern void towerRecordLevelBeaten(u32 a, s32 b);
extern s32  pbDiagDrawMenu(void);

/* Same-TU entry points; deferred inlining also sees later definitions. */
void default_options(void);
void StartCompass(void);
void fn_800521E8(void);
s32 do_stats_display(void);
void ResetModels(void);
void init_moving_objects(void);
void TransitionBlitHide(void);
void EndTower(void);
void ShowLoading(void);
s32 SetMaxFPS(s32 arg0);
void LockModels(s32 arg0);
void game_init_data(void);
void TransitionBlitShow(s32 arg0);
void LoadTowerAndSelect(void);
s32 fn_80053D08(s32 wave, s32 mode, s32 loadResult);
s32 next_world(void);
void fn_800552A4(f32 total, f32 current);
s32 fn_80054CDC(void);
void fn_800553B4(void);
s32 fn_80054070(s32 arg0, s32 arg1, s32 arg2);
void fn_80052134(void);
void SetPlayerVars(void);
void fn_80053C70(void);
void fn_80055AFC(void);
void fn_80055678(f32* a, f32* b);
void init_thermometer(void);
void fn_8005351C(void);
void game_main(void);
void fn_80054E78(void);


static inline int tally_treasures(Player* pp)
{
    int amount = tbuf_step[pp->index];

    if (gGameBusy != 0) {
        return 0;
    }
    if (PlayerControl[pp->index].levels & 0x0F000000) {
        amount *= 6;
    }
    tbuf_treasures[pp->index] += amount;
    if (tbuf_treasures[pp->index] < CHAR_STAT(pp).gold_found) {
        return 0;
    }
    tbuf_treasures[pp->index] = CHAR_STAT(pp).gold_found;
    return 1;
}


static inline int tally_enemies(Player* pp)
{
    int amount = tbuf_step[pp->index];

    if (gGameBusy != 0) {
        return 0;
    }
    if (PlayerControl[pp->index].levels & 0x0F000000) {
        amount *= 6;
    }
    tbuf_enemies[pp->index] += amount;
    if (tbuf_enemies[pp->index] < CHAR_STAT(pp).enemies_killed) {
        return 0;
    }
    tbuf_enemies[pp->index] = CHAR_STAT(pp).enemies_killed;
    return 1;
}


static inline int tally_generators(Player* pp)
{
    int amount = tbuf_step[pp->index];

    if (gGameBusy != 0) {
        return 0;
    }
    if (PlayerControl[pp->index].levels & 0x0F000000) {
        amount *= 6;
    }
    tbuf_generators[pp->index] += amount;
    if (tbuf_generators[pp->index] < CHAR_STAT(pp).generators_destroyed) {
        return 0;
    }
    tbuf_generators[pp->index] = CHAR_STAT(pp).generators_destroyed;
    return 1;
}


static inline int tally_playtime(Player* pp)
{
    int amount = tbuf_step[pp->index];

    if (gGameBusy != 0) {
        return 0;
    }
    if (PlayerControl[pp->index].levels & 0x0F000000) {
        amount *= 6;
    }
    tbuf_playtime[pp->index] += amount;
    if (tbuf_playtime[pp->index] < CHAR_STAT(pp).total_playtime) {
        return 0;
    }
    tbuf_playtime[pp->index] = CHAR_STAT(pp).total_playtime;
    return 1;
}


static inline void disp_pname(Player* pp)
{
    char buf[16];

    sprintf(buf, "%s", pp->save.name);
    if (strcmp(buf, "___") == 0) {
        strcpy(buf, "NO NAME");
    }
    DrawTextKeepScale(0.6f, -stat_cx[pp->index],
                      stat_yoff[0] + stat_ty[pp->index], 7, TEXT_RGB_WHITE, buf);
}


static inline void disp_enemies(Player* pp)
{
    char buf[16];
    int width;

    DrawTextKeepScale(0.5f, stat_lx[pp->index] + 7,
                      stat_yoff[1] + stat_ty[pp->index], 7, TEXT_RGB_WHITE, "ENEMIES");
    sprintf(buf, "%d", tbuf_enemies[pp->index]);
    width = DrawNormalText(0.5f, buf, 7);
    DrawTextKeepScale(0.5f, stat_rx[pp->index] - width,
                      stat_yoff[1] + stat_ty[pp->index], 7, TEXT_RGB_WHITE, buf);
}


static inline void disp_generators(Player* pp)
{
    char buf[16];
    int width;

    DrawTextKeepScale(0.5f, stat_lx[pp->index] + 7,
                      stat_yoff[2] + stat_ty[pp->index], 7, TEXT_RGB_WHITE, stats_generators_label);
    sprintf(buf, "%d", tbuf_generators[pp->index]);
    width = DrawNormalText(0.5f, buf, 7);
    DrawTextKeepScale(0.5f, stat_rx[pp->index] - width,
                      stat_yoff[2] + stat_ty[pp->index], 7, TEXT_RGB_WHITE, buf);
}


static inline void disp_treasures(Player* pp)
{
    char buf[16];
    int width;

    DrawTextKeepScale(0.5f, stat_lx[pp->index] + 7,
                      stat_yoff[3] + stat_ty[pp->index], 7, TEXT_RGB_WHITE, stats_treasures_label);
    sprintf(buf, "%d", tbuf_treasures[pp->index]);
    width = DrawNormalText(0.5f, buf, 7);
    DrawTextKeepScale(0.5f, stat_rx[pp->index] - width,
                      stat_yoff[3] + stat_ty[pp->index], 7, TEXT_RGB_WHITE, buf);
}


static inline void disp_playtime(Player* pp)
{
    char buf[16];
    int width;
    int time = tbuf_playtime[pp->index] / 60;
    int seconds = time % 60;
    int minutes;
    time /= 60;
    minutes = time % 60;
    time /= 60;

    DrawTextKeepScale(0.5f, stat_lx[pp->index] + 7,
                      stat_yoff[5] + stat_ty[pp->index], 7, TEXT_RGB_WHITE, stats_playtime_label);
    sprintf(buf, stats_playtime_format, time, minutes, seconds);
    width = DrawNormalText(0.5f, buf, 7);
    DrawTextKeepScale(0.5f, stat_rx[pp->index] - width,
                      stat_yoff[5] + stat_ty[pp->index], 7, TEXT_RGB_WHITE, buf);
}

/* This exit-reason test recurs in next_world, level entry and game_main.
 * The local name describes its use; the original helper name is unknown. */
static inline s32 is_level_transition(s32 state)
{
    s32 transitioning = 0;
    if (state >= 13 && state < WORLD_OVERRIDE_BASE) {
        transitioning = 1;
    }
    return transitioning;
}

/* SDK-style four-step square root (MSL math_ppc.h, sqrtf_accurate).
 * Positive inputs round through float storage; other inputs pass through.
 * Both GC proximity-meter routines embed it; no standalone copy is needed. */
static inline f32 sqrtf_accurate(f32 value)
{
    // lint-allow-next-line FM003: Four Newton steps end with the GC-observed float store/reload, as in MSL math_ppc.h; rounded is used, not padding.
    volatile f32 rounded;
    if (value > 0.0f) {
        f64 guess = __frsqrte(value);
        guess = 0.5 * guess * (3.0 - guess * guess * value);
        guess = 0.5 * guess * (3.0 - guess * guess * value);
        guess = 0.5 * guess * (3.0 - guess * guess * value);
        guess = 0.5 * guess * (3.0 - guess * guess * value);
        rounded = (f32)(value * guess);
        value = rounded;
    }
    return value;
}

/* The game-over and good-wizard timers use this decrement/store/test shape.
 * Return the stored value, without reloading the timer. This descriptive
 * inline's original name is unknown; no extra timer storage is needed. */
static inline s32 countdown_ticks(s32* timer, s32 ticks)
{
    return *timer -= ticks;
}

/* Public definitions: reverse of their emitted order. */
/* 0x80055AFC -- milestone blink cycle: flash the milestone markers while the
 * party is idle at a boss gate. */
void fn_80055AFC(void)
{
    // lint-allow-next-line FM003: User-approved compatibility reservation (2026-09-08): retail reserves 32 otherwise unaccessed bytes beyond the recovered locals; their original source identity remains unknown. No runtime reads or writes are added.
    u8 unrecovered_stack[32];
    s32 i;
    MILESTONE* ms;
    s32 n;
    s32 limit;

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
            MILESTONE* mp = &ms[i];
            MBTreeClearFlags(mp->objgrp.node, 2, 0);
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
            MILESTONE* mp = &ms[i];
            MBTreeSetFlags(mp->objgrp.node, 2, 0);
            i++;
        }
    }
    lbl_80344780 = lbl_803447EC;
    lbl_803447EC = 1;
    lbl_803447F4 = lbl_803447F4 - limit;
}


void init_thermometer(void)
{
    void** blits;
    s32 player;
    s32 enabled;
    u32 texture;
    f32 length;
    f32 x;
    f32 yCoord;
    f32 z;

    enabled = 1;
    lbl_8034478C = lbl_80344790 = 0;
    if ((gGameMode & MODE_GROUP_GAME) != 0 && sSpecialItem10 != 0) {
        player = 0;
        do {
            Player* playerData = &gPlayers[player];
            if (PlayerHasShard(player, sSpecialItem10->info->item.value) != 0) {
                enabled = 1;
                break;
            }
            if (sMusicTrackHi == BATTLE) {
                s32 charIdx = playerData->character;
                if ((playerData->save.waves[charIdx][BATTLE] & 4) != 0) {
                    enabled = 0;
                }
            } else if (PlayerHasRune(player, GetWorldOrder(5)) != 0) {
                enabled = 0;
            }
            player++;
        } while (player < 4);
    }

    lbl_803447A8[0] = MBCreateBlit(0, 0, 392, -1, -1, -1);
    *(blits = &lbl_803447A8[1]) = MBCreateBlit(0, 0, 392, -1, -1, -1);
    mbBlitCvtCoord(lbl_803447A8[0], 63912.0f);
    mbBlitCvtCoord(*blits, 63911.0f);
    mbBlitInit3414(lbl_803447A8[0], enabled);
    mbBlitInit3414(*blits, enabled);
    texture = MBOX_FindTexture_Err("THERMBASE", 0, 1);
    mbInitBlitEntry(lbl_803447A8[0], texture, 0);
    mbInitBlitEntry(*blits, MBOX_FindTexture_Err("THERMCOL", 0, 1), 0);
    mbBlitSetupVerts(*blits, -1.0f, -1.0f,
                     0.7890625f, 1.0f);
    mbBlitProject(*blits, 0, 27);

    x = gWorldInfo.worldsize[0];
    yCoord = gWorldInfo.worldsize[1];
    z = gWorldInfo.worldsize[2];
    length = sqrtf_accurate(x * x + yCoord * yCoord + z * z);
    lbl_80343C08 = (f32)(1.0 / (0.7 * length));
}

/* 0x80055678 -- update the special-item proximity meter blits from the
 * distance between the two given points. */
void fn_80055678(f32* a, f32* b)
{
    f32 d;
    f32 dx;
    f32 dy;
    f32 dz;

    if (mbBlitReset33F8(lbl_803447A8[0]) != 0 || sSpecialItem10 == 0) {
        mbBlitInit3414(lbl_803447A8[0], 1);
        mbBlitInit3414(lbl_803447A8[1], 1);
    } else {
        dx = a[0] - b[0];
        dy = a[1] - b[1];
        dz = a[2] - b[2];
        d = sqrtf_accurate(dx * dx + dy * dy + dz * dz);
        d = (f32)(d - 8.0);
        d = d * lbl_80343C08;
        {
            f64 clamped = d < 0.0f ? 0.0 : d > 1.0 ? 1.0 : d;
            d = (f32)clamped;
        }
        d = (f32)(1.0 - (1.0 - d) * (1.0 - d));
        if (lbl_80344790 == 0 && d < 0.25) {
            lbl_80344790 = 1;
            fn_8009FF54(b);
        } else if (lbl_8034478C == 0 && d < 0.025) {
            lbl_8034478C = 1;
            fn_8009FFA4(b);
        }
        d = (f32)(1.0 - d);
        mbBlitSetupVerts(lbl_803447A8[1], -1.0f, -1.0f,
                         (f32)((101.0 - 73.0 * d) / 128.0),
                         -1.0f);
        mbBlitProject(lbl_803447A8[1], 0, Round((f32)(73.0 * d)) + 27);
        mbBlitCalcY(lbl_803447A8[1], 102 - Round((f32)(73.0 * d)));
    }
}

/* 0x800553B4 -- initialize the four timer/thermometer HUD blits. */
void fn_800553B4(void)
{
    s32 i;
    s32 hide;
    s32 texture;

    if ((gCurLevel->flags & 4) != 0) {
        if ((gControllerButtons & 0x10) != 0) {
            lbl_80344814 = 100.99f;
        } else {
            lbl_80344814 =
                (f32)(0.99 + (f64)gCurLevel->wavetime);
        }
        lbl_80344818 = lbl_80344814;
    }

    lbl_80344810 = 0;
    lbl_80257630[0] = MBCreateBlit(0, 0, 1, 1, -1, -1);
    lbl_80257630[1] = MBCreateBlit(0, 0, 1, 24, -1, -1);
    lbl_80257630[2] = MBCreateBlit(0, 0, 1, 106, -1, -1);
    lbl_80257630[3] = MBCreateBlit(0, 0, 63, 58, -1, -1);

    mbBlitCvtCoord(lbl_80257630[0], 63913.0f);
    mbBlitCvtCoord(lbl_80257630[1], 63911.0f);
    mbBlitCvtCoord(lbl_80257630[2], 63911.0f);
    mbBlitCvtCoord(lbl_80257630[3], 63912.0f);

    if ((gControllerButtons & 0x10) == 0) {
        if ((gCurLevel->flags & 4) != 0) {
            hide = 0;
        } else {
            hide = 1;
        }
    } else {
        hide = 1;
    }
    for (i = 0; i < 4; i++) {
        mbBlitInit3414(lbl_80257630[i], hide);
    }
    mbBlitInit3414(lbl_80257630[3], 1);

    texture = MBOX_FindTexture_Err("TIMER", 0, 1);
    mbInitBlitEntry(lbl_80257630[0], texture, 0);
    mbInitBlitEntry(lbl_80257630[1],
                    MBOX_FindTexture_Err("TIMER_SAND", 0, 1), 0);
    mbInitBlitEntry(lbl_80257630[2],
                    MBOX_FindTexture_Err("TIMER_SAND", 0, 1), 0);
    mbInitBlitEntry(lbl_80257630[3],
                    MBOX_FindTexture_Err("SAND_ANIM", 0, 1), 0);

    mbBlitSetupVerts(lbl_80257630[1], -1.0f, -1.0f,
                     0.1796875f, 0.5f);
    mbBlitProject(lbl_80257630[1], 0, 41);
    mbBlitSetupVerts(lbl_80257630[2], -1.0f, -1.0f,
                     0.8203125f, 1.0f);
    mbBlitProject(lbl_80257630[2], 0, 23);
}

/* 0x800552A4 -- animate the two halves of the loading thermometer. */
void fn_800552A4(f32 total, f32 current)
{
    f32 remaining = total - current;
    f32 progress = remaining / total;
    mbBlitSetupVerts(lbl_80257630[1], -1.0f, -1.0f,
                     (f32)((41.0 * progress + 23.0) / 128.0), -1.0f);
    mbBlitProject(lbl_80257630[1], 0, 41 - Round((f32)(39.0 * progress)));
    mbBlitCalcY(lbl_80257630[1], Round((f32)(39.0 * progress)) + 24);
    mbBlitSetupVerts(lbl_80257630[2], -1.0f, -1.0f,
                     (f32)((105.0 - 38.0 * progress) / 128.0), -1.0f);
    mbBlitProject(lbl_80257630[2], 0, Round((f32)(38.0 * progress)) + 23);
    mbBlitCalcY(lbl_80257630[2], 106 - Round((f32)(38.0 * progress)));
}


void fn_80054E78(void)
{
    s32 active;
    s32 i;

    if (lbl_803447B8 != 0) {
        active = 0;
    } else {
        active = 1;
    }

    if ((gControllerButtons & 0x10) == 0) {
        if (active != 0 && (gCurLevel->flags & 4) &&
            lbl_80257630[3] != 0) {
            mbBlitInit3414(lbl_80257630[3], 0);
        }
        if (lbl_80344818 >
            1.0f + (f32)gCurLevel->wavetime) {
            lbl_80344814 = 5.0f;
            lbl_80344818 = 5.0f;
        }
    }

    if ((gCurLevel->flags & 4) &&
        (gGameBusy | gGameplayPauseTimer) == 0 &&
        (gControllerButtons & 4) == 0 && active != 0) {
        f32 nt;
        s32 oldi;

        oldi = (s32)lbl_80344818;
        lbl_80344818 -= gClockFrameStep;
        nt = lbl_80344818;
        if (nt <= 0.0) {
            for (i = 0; i < 4; i++) {
                if (lbl_80257630[i] != 0) {
                    MBRemoveBlit(lbl_80257630[i]);
                    lbl_80257630[i] = 0;
                }
            }
            lbl_80344818 = 0.0f;
            active = 0;
            if ((gControllerButtons & 0x10) != 0 &&
                (gGameOptions[9] >> 8) == 12) {
                s32 player_index;
                Player* player;

                lbl_8034481C = 2;
                player = gPlayers;
                for (player_index = 0; player_index < 4;
                     player_index++, player++) {
                    if (player->state != INACTIVE) {
                        lbl_80257650[player_index][0] = player->pos[0];
                        lbl_80257650[player_index][1] = player->pos[1];
                        lbl_80257650[player_index][2] = player->pos[2];
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
            fn_800552A4((f32)(60.0 * lbl_80344814),
                         (f32)(60.0 * lbl_80344818));

            if ((gControllerButtons & 0x10) != 0) {
                DrawText(-256, 8, 6, TEXT_RGB_WHITE, "%.1f",
                         lbl_80344814 - lbl_80344818);
            } else if ((gControllerButtons & 0x10) != 0) {
                DrawText(-256, 8, 6, TEXT_RGB_WHITE, "%.1f", lbl_80344818);
            }
        }
    }
}

/* 0x80054E68 -- set-and-return the previous value of lbl_80343C0C. */
s32 SetMaxFPS(s32 arg0)
{
    s32 old = lbl_80343C0C;
    lbl_80343C0C = arg0;
    return old;
}


s32 next_world(void)
{
    s32 world;
    s32 forced;
    s32 transitioning;
    s32 state = lbl_8034481C;
    s32 t2;
    register s32 selected;

    transitioning = is_level_transition(state);
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
        selected = gGameOptions[9];
        world = selected;
        if ((selected >> 8) >= NUMWORLDS) {
            world = sFirstWorldId;
        }
        lbl_8034481C = world + WORLD_OVERRIDE_BASE;
        forced = 1;
    } else {
        s32 i;

        world = -1;
        for (i = 0; i < 4; i++) {
            Player* player;
            state = (player = &gPlayers[i])->state;
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
        if ((world >> 8) >= NUMWORLDS) {
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

/* User-approved compatibility reconstruction (2026-09-08). Xbox STATS.OBJ
 * identifies a one-byte HistHero(int) no-op. Its original GameCube linkage
 * and caller identity remain unproven. Inlining this disabled statistics hook
 * preserves GC's otherwise unused player-mask loop without volatile accesses
 * or generated-code rewriting; it is not a recovered original definition. */
static inline void HistHero(int hero) {}

/* 0x80054230 - top-level per-frame game mode dispatcher. */
void game_main(void)
{
    s32 i;
    s32 v;
    s32 reset_player;
    s32 cond;
    s32 flag;
    s32 lvl;
    s32 next;
    s32 all;
    s32 c;

    lbl_80344800++;
    if (gGameMode & MODE_GROUP_GAME) {
        for (i = 0; i < 4; i++) {
            if (lbl_80257640[i] > 120) {
                lbl_80344A2C = 1;
            }
        }
    }
    if (opt_quit_request && OptionsDone()) {
        opt_quit_request = 0;
        gGameMode = MG_OVER;
        gGameBusy = 0;
        AudioStopSelect();
        AudioSelectReset();
        fn_8009D34C();
        lbl_80344774 = 0;
        lbl_80344778 = 240;
        for (i = 0; i < 4; i++) {
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
        bulletproof_printf("Audio Stop\n");
        AudioStopSelect();
        AudioSelectReset();
        bulletproof_printf("Reinit stuff\n");
        alpha = 0;
        fn_800520C8();
        AudioClearInputFlag();
        init_targets();
        lbl_80344A2C = 0;
        options_state = 0;
        end_all_optmenus();
        FireScrollReset();
        TriggerCameraEnd();
        TowerInit();
        for (reset_player = 0; reset_player < 4; reset_player++) {
            abort_player(reset_player);
        }
        bulletproof_printf("Reset Models\n");
        fn_80053D08(-1, 0, -1);
        bulletproof_printf("Init attract\n");
        lbl_80343C10 = -1;
        lbl_80343DD4 = -1;
        lbl_80343B38 = -1;
        AudioStopSelect();
        lbl_803448AC = -1;
        lbl_803448A8 = -1;
        while (!MBOX_BGLoadModelDone()) {
        }
        init_attract_mode(MA_TITLESCREEN);
    }
    cond = is_level_transition(c = lbl_8034481C);
    flag = cond ? 1 : 0;
    SetPlayerVars();
    if (lbl_803447D8 > lbl_803447D4 && gGameMode == MG_PLAY) {
        fn_8009D530();
    }
    c = gGameMode;
    switch (c) {
    case MA_CREDITS:
    case MA_TITLEMOVIE:
    case MA_MOVIE:
    case MA_INSTRUCT:
    case MA_SCREEN2D:
    case MA_CONTEST:
    case MA_DEMO:
    case MA_HSTABLE:
    case MA_FLYBY:
    case MA_TITLESCREEN:
        {
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
        /* See the explicitly approved HistHero reconstruction above. */
        {
            Player* player = gPlayers;
            for (i = 0; i < 4; i++, player++) {
                if (lbl_80344824 & (1 << i)) {
                    HistHero(player->character);
                }
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
                if (countdown_ticks(&good_wiz_exit_timer, gFrameTicks) <= 0) {
                    lbl_80344808 = 1;
                }
            }
        }
        fn_80054E78();
        fn_80055678(lbl_8025EA04, gCameras[0].attn);
        if (!lbl_803447B8) {
            StartCompass();
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
            gGameMode = MG_OVER;
            gGameBusy = 0;
            AudioStopSelect();
            AudioSelectReset();
            fn_8009D34C();
            lbl_80344774 = 0;
            lbl_80344778 = 240;
            for (i = 0; i < 4; i++) {
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
            s32 flag2;
            lvl = (lbl_803448D0 << 8) | (lbl_803448CC & 0xFF);
            if (!lbl_80344824) {
                gGameMode = MG_OVER;
                gGameBusy = 0;
                AudioStopSelect();
                AudioSelectReset();
                fn_8009D34C();
                lbl_80344774 = 0;
                lbl_80344778 = 240;
                for (i = 0; i < 4; i++) {
                    abort_player(i);
                }
                lbl_80344824 = 0;
                TriggerCameraEnd();
                break;
            }
            fn_8009D610(2, 0);
            if (lvl == sWorldDataConst) {
                lbl_80343C10 = -1;
                lbl_80343DD4 = -1;
                lbl_80343B38 = -1;
                AudioStopSelect();
                lbl_803448AC = -1;
                lbl_803448A8 = -1;
                lbl_80343C04 = next_world();
                ResolveWorldData(lbl_80343C04);
                lbl_80343C00 = init_mapscreen(120, 0);
                break;
            }
            next = -1;
            c = lbl_8034481C;
            cond = is_level_transition(c);
            /* GC normalizes this predicate again. Use it for the destination
             * decision, retaining the map-screen argument across later calls. */
            cond = cond ? 1 : 0;
            flag2 = cond;
            all = 1;
            for (i = 0; i < 4; i++) {
                v = gPlayers[i].state;
                if (v != 0 && v != 11) {
                    all = 0;
                }
            }
            if (all) {
                next = sWorldDataConst;
            } else if (cond) {
                next = lbl_80344B84;
            } else if (opt_restart_request) {
                next = sWorldDataConst;
            } else if (c == 0 || c == 12) {
                if (gBossType == E_GARM) {
                    init_gamemovie(E_GARM);
                    sLastWorldLevel = sWorldDataConst;
                    next = -2;
                } else if (gBossType == E_SKORNE2) {
                    init_gamemovie(E_SKORNE2);
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
            init_gamemovie(E_GARM);
        } else {
            do_players();
        }
        break;
    case MA_VIEWMENU:
        if (pbDiagDrawMenu() == 2) {
            gGameMode = lbl_80344788;
        }
        break;
    default:
        break;
    }
}

/* 0x8005412C -- categorise the loaded worlds and update the flow globals. */
void SetPlayerVars(void)
{
    s32 bossType = gBossType;
    s32 count1 = 0;
    s32 count2 = 0;
    s32 count3 = 0;
    Player* e;
    s32 i;
    s32 type;
    s32 playerFlags;

    lbl_803447D4 = lbl_803447D8;
    lbl_803447DC = 0;
    lbl_803447D8 = 1.0f;
    lbl_803447E0 = 0;
    for (i = 0; i < 4; i++) {
        type = (e = &gPlayers[i])->state;
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
            playerFlags = e->flags;
            if (playerFlags & 0x8) {
                lbl_803447DC = 1;
            }
            if (bossType < 0 && (playerFlags & 0x200)) {
                lbl_803447D8 *= 0.667;
            }
        }
        e->hud_flags2 = 0;
    }
    fn_8005207C(count1, count2, count3);
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

/* 0x8005403C -- lock the model box, then run AtreeListLock. */
void LockModels(s32 arg0)
{
    MBOX_LockModels(arg0);
    AtreeListLock(arg0);
}

/* 0x80053D08 -- tear down the current front-end/world state and load a wave. */
s32 fn_80053D08(s32 wave, s32 mode, s32 loadResult)
{
    s32 result;
    f32 zero;

    EndFireScroll();
    DeleteOptionBlits();
    lbl_8034479C = 0;
    SumnerEnd();
    AudioStopSelect();
    good_wiz_enabled = 0;
    Randomize(WORLD_RANDOM_SEED);
    good_wiz_state = 0;
    ResetPlayerMissiles();
    ClearAllPlyrData();
    InitializeClockIRQ();
    vibrators_off();

    if ((gControllerButtons & 0x10) != 0) {
        LoadWorldData();
    }

    if (lbl_80343C10 >= 0) {
        WorldRestoreInitState();
        MBOX_ResetUnlockedModels(2);
        AtreeInitLists(2);
        ResetTexmods();
        result = fn_80054070(wave, mode, loadResult);
    } else if (loadResult < 0) {
        bulletproof_printf("Starting wave %d... MEM=%d\n", wave, BytesFree());
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

        zero = 0.0f;
        MBCompVertScaleAddUV(0, 0, zero, zero, zero, zero, zero);
        ResetTexmods();
        bulletproof_printf("  Worlds... MEM=%d\n", BytesFree());
        ResetWorlds();
        bulletproof_printf("  Enemies... MEM=%d\n", BytesFree());
        fn_80051164();
        bulletproof_printf("  Items... MEM=%d\n", BytesFree());
        InitItems();
        bulletproof_printf("  Critters... MEM=%d\n", BytesFree());
        sndSysInit();
        bulletproof_printf("  Game... MEM=%d\n", BytesFree());
        result = fn_80054070(wave, mode, -1);
        bulletproof_printf("  Done. MEM=%d\n", BytesFree());
    } else {
        MBOX_ResetUnlockedModels(2);
        AtreeInitLists(2);
        zero = 0.0f;
        MBCompVertScaleAddUV(0, 0, zero, zero, zero, zero, zero);
        ResetTexmods();
        result = fn_80054070(wave, mode, loadResult);
    }

    lbl_803447B0 = 0;
    lbl_803447A0 = MBOX_FindTexture("AAAWHITE", 0);
    lbl_803443E4 = MBOX_FindTexture("FONT32_GLOW", 0);
    lbl_80344E48 = MBOX_FindTexture("BUTTON_X", 0);
    lbl_80344E44 = MBOX_FindTexture("BUTTON_TRI", 0);
    lbl_80344E40 = MBOX_FindTexture("BUTTON_SQ", 0);
    lbl_80344E3C = MBOX_FindTexture("BUTTON_O", 0);
    lbl_80344E38 = MBOX_FindTexture("BUTTON_L", 0);
    lbl_80344E34 = MBOX_FindTexture("BUTTON_R", 0);
    lbl_80344E30 = MBOX_FindTexture("BUTTON_U", 0);
    lbl_80344E2C = MBOX_FindTexture("BUTTON_D", 0);
    AudioRegisterMenu();
    InitLighting(0);
    return result;
}

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

/* 0x80053B88 -- finish async tower loading, then initialize the selected level. */
void LoadTowerAndSelect(void)
{
    u32 timeout = pbLoad + 900;

    if (lbl_80343DD4 < 0) {
        fn_80053D08(-1, 0, -1);
        SelectLoadStart();
        while (SelectLoadDone() == 0) {
            if (pbLoad > timeout) {
                FatalError("LoadTowerAndSelect Timeout", WORLD_LOAD_TIMEOUT_ERROR);
            }
        }
    }
    if (lbl_80343C10 < 0) {
        fn_80053D08(sWorldDataConst, 1, -1);
        FontInitSpecial("shopatt9", 8);
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

/* 0x80053B60 -- start something at slot 1. */
void ShowLoading(void)
{
    fn_80055F68(1, 0);
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

/* 0x80053A68 -- create/refresh the loading-screen blit. */
void TransitionBlitShow(s32 arg0)
{
    f32 coord;
    s32 sz;

    if (arg0 != 0) {
        coord = 100.0f;
        sz = 384;
    } else {
        sz = 320;
        coord = 64100.0f;
    }
    if (lbl_803447B0 == 0) {
        void* tex = MBOX_FindTexture("TRANSITION_SCREEN", 0);
        lbl_803447B0 = MBCreateBlit(0, tex, 0, 0, 512, sz);
    }
    mbBlitProject(lbl_803447B0, 0, sz);
    mbBlitCvtCoord(lbl_803447B0, coord);
    MBBlitSetAlpha(lbl_803447B0, 0);
    mbBlitInit3414(lbl_803447B0, 0);
}

/* 0x80053A38 -- hide the blit at lbl_803447B0 if present. */
void TransitionBlitHide(void)
{
    if (lbl_803447B0 != 0) {
        mbBlitInit3414(lbl_803447B0, 1);
    }
}

/* 0x80053A10 -- clear two per-enemy fields for all 25 enemy records. */
void init_moving_objects(void)
{
    Enemy* e = gEnemies;
    s32 i;

    for (i = 0; i < 25; i++, e++) {
        e->state = INACTIVE;
        e->objgrp.node = 0;
    }
}


void fn_8005351C(void)
{
    s32 t;
    s32 state = lbl_8034481C;
    s32 inTower;
    s32 isSelect;
    s32 i;
    Player* p;

    t = is_level_transition(state);
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
    lbl_803447D4 = 1.0f;
    lbl_803447D8 = 1.0f;
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
        for (i = 0, p = gPlayers; i < 4; i++, p++) {
            Player* player = p;
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
        for (i = 0, p = gPlayers; i < 4; i++, p++) {
            p->exit_dest = sLastWorldLevel;
            p->node = 0;
            p->platform = 0;
            if ((lbl_80344824 & (1 << i)) && p->state != INTOWER) {
                Player* player = p;
                player->state = ACTIVE;
                load_player(i);
                add_target(player->mat);
                LoadPlyrData(i, player->character, 1);
                if (isSelect != 0) {
                    f32* v;
                    CopyMat3(gIdentityMatrix, player->mat);
                    v = lbl_80257650[i];
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
        for (i = 0; i < 4; i++) {
            Player* player = &gPlayers[i];
            if (player->state != INACTIVE) {
                if (sMusicTrackHi == 13) {
                    PlayerSaveState(i, 0);
                } else if (sMusicTrackHi != 12) {
                    PlayerSaveState(i, 1);
                }
                {
                    s32* experience;
                    /* Preserve GC's field-address-before-test idiom. The
                     * pointer names the real exp member, then updates it;
                     * its original local spelling is not known. */
                    if (*(experience = &player->exp) == 0) {
                        *experience = 1;
                        player->save.saved = 0;
                    }
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
            for (i = 0, p = gPlayers; i < 4; i++, p++) {
                if (p->state == ACTIVE) {
                    p->save_backup = p->save;
                }
            }
        }
    }
}

/* 0x80053420 -- second-stage game/audio init (main() calls this). */
void game_init_data(void)
{
    // lint-allow-next-line FM003: User-approved compatibility reservation (2026-09-08): retail reserves 8 otherwise unaccessed bytes beyond the recovered save area; their original source identity remains unknown. No runtime reads or writes are added.
    u8 unrecovered_stack[8];

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
    bulletproof_printf("Initializing Audio...\n");   /* "Initializing Audio..." */
    AudioInit();
    bulletproof_printf("Loading Audio...\n");  /* "Loading Audio..."      */
    while (AudioSysUpdate(1) != 0) {
        serve_busy(-1);
    }
    bulletproof_printf("Loading Game.\n");  /* "Loading Game."         */
    ControlsUpdate();
    lbl_80344784 = 0;
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


s32 do_stats_display(void)
{
    Player* p;
    int i;
    int stalled = 0;
    int done = 1;

    DrawTextKeepScale(0.75f, -256, 0, 7, TEXT_RGB_WHITE, stats_title);
    for (i = 0, p = gPlayers; i < 4; i++, p++) {
        if (p->state != 1 && p->state != 5 && p->state != 4) {
            continue;
        }
        disp_pname(p);

        switch (p->field_A64) {
        case 0:
            tbuf_enemies[i] = tbuf_generators[i] = tbuf_treasures[i] =
                (tbuf_playtime[i] == 0);
            tbuf_timer[i] = 480;
            p->field_A64++;
            tbuf_step[i] = CHAR_STAT(p).enemies_killed / 60;
            if (tbuf_step[i] < 1) {
                tbuf_step[i] = 1;
            }
        case 1:
            done = 0;
            if (tally_enemies(p)) {
                p->field_A64++;
                tbuf_step[i] = CHAR_STAT(p).generators_destroyed / 60;
                if (tbuf_step[i] < 1) {
                    tbuf_step[i] = 1;
                }
            } else {
                stalled = 1;
            }
            disp_enemies(p);
            break;
        case 2:
            done = 0;
            disp_enemies(p);
            if (tally_generators(p)) {
                p->field_A64++;
                tbuf_step[i] = CHAR_STAT(p).gold_found / 60;
                if (tbuf_step[i] < 1) {
                    tbuf_step[i] = 1;
                }
            } else {
                stalled = 1;
            }
            disp_generators(p);
            break;
        case 3:
            done = 0;
            disp_enemies(p);
            disp_generators(p);
            if (tally_treasures(p)) {
                p->field_A64++;
                tbuf_step[i] = (int)(CHAR_STAT(p).total_playtime / 60.0f);
                if (tbuf_step[i] < 60) {
                    tbuf_step[i] = 60;
                }
                if (tbuf_step[i] < 1) {
                    tbuf_step[i] = 1;
                }
            } else {
                stalled = 1;
            }
            disp_treasures(p);
            break;
        case 4:
            p->field_A64++;
        case 5:
            done = 0;
            disp_enemies(p);
            disp_generators(p);
            disp_treasures(p);
            if (tally_playtime(p)) {
                p->field_A64++;
            } else {
                stalled = 1;
            }
            disp_playtime(p);
            break;
        case 6:
            done = 0;
            disp_enemies(p);
            disp_generators(p);
            disp_treasures(p);
            disp_playtime(p);
            if ((tbuf_timer[i] -= gFrameTicks) <= 0) {
                p->field_A64++;
            }
            break;
        default:
            disp_enemies(p);
            disp_generators(p);
            disp_treasures(p);
            disp_playtime(p);
            break;
        }
    }
    WritePlayerInfo(-1);
    fn_8009FCA8(stalled);
    if (done) {
        for (i = 0; i < 4; i++) {
            MBRemoveBlit(stats_bg_blit[i]);
        }
        AudioStopMusicA();
    }
    return done;
}

/* 0x800521E8 -- animate the loading-timer HUD, arm attract on timeout. */
void fn_800521E8(void)
{
    s32 idx;
    s32 flag = gGameBusy;
    s32 oldTimer = lbl_80344774;
    s32 newTimer;
    MBTextMsg* txt;
    char* textData;

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
    SetDrawStringScale(2.0f);
    txt = (MBTextMsg*)DrawStringText(-256, 120, 6, TEXT_RGB_WHITE, 169, 0);
    RestoreDrawStringScale();
    textData = txt->text;
    textData[idx] = 0;
    if (flag != 0) {
        return;
    }
    {
        s32 remaining = countdown_ticks(&lbl_80344778, gFrameTicks);
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
    init_attract_mode(MA_MOVIE);
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
            MBWindowTo3D(lbl_80343C14, &lbl_80344EE8->cam,
                         ((MBObject*)lbl_8034479C)->mat[3], lbl_80343C1C);
            for (i = 0; i < 3; i++) {
                ((MBObject*)lbl_8034479C)->scale[i] = lbl_80343C18;
            }
            MBTreeSetAlpha(lbl_8034479C, lbl_80343C20, 0);
        }
    }
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

/* Xbox exposes StartCompass as a standalone function; GC embeds this
 * same creation sequence in game_main. A native whole-link control verifies
 * that mwld discards the unused outlined body and its exception records. */
void StartCompass(void)
{
    if (lbl_8034479C == 0) {
        lbl_8034479C = MBOX_NewObject("COMPASS", 0, lbl_80344EA8, 8);
    }
}
