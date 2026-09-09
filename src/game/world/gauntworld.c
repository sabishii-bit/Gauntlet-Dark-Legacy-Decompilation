/* gauntworld.obj: world-data (WAD) loading and resolution, level flow, world
 * animation and the per-frame world update (.text 0x80055CB8..0x8005A1EC).
 * Xbox shell3D.pdb module gauntworld.obj lists this TU's source order; the
 * GameCube text order below is the linked order. Names still spelled
 * fn_XXXXXXXX are unnamed in symbols.txt. */
#include "types.h"
#include "game/critter.h"
#include "game/effect.h"
#include "game/enemy.h"
#include "game/gamemode.h"
#include "game/worldinfo.h"
#include "game/leveldata.h"
#include "game/mbnode.h"
#include "game/player.h"
#include "game/worldobj.h"

#ifndef offsetof
#define offsetof(type, memb) ((u32) & ((type*)0)->memb)
#endif

/* --- On-disk / in-memory world-data blob header (base = gWorldData) --------
 * All multi-byte scalars are byte-swapped in place by ResolveWorldData for the
 * big-endian GameCube.  Section pointers (0x1C.. ) are filled by MBGetFromWad. */
typedef struct WorldLevel WorldLevel;

typedef struct WorldData {
    /* 0x00 */ u32   id;         /* magic/id, swapped                        */
    /* 0x04 */ u8    _pad04[0x10];
    /* 0x14 */ s16   field14;    /* swapped short                            */
    /* 0x16 */ s16   curLevel;   /* set to the resolved level index          */
    /* 0x18 */ s16   numLevels;  /* level count (WorldLevel array length)    */
    /* 0x1A */ s16   numSounds;  /* sound-table entry count (stride 0x18)    */
    /* 0x1C */ WorldLevel* levels;   /* level array base (stride 0x10C)      */
    /* 0x20 */ void* section20;
    /* 0x24 */ u8*   cameras;    /* camera array   (stride 0x6C)             */
    /* 0x28 */ u8*   audio;      /* audio array    (stride 0x3C)             */
    /* 0x2C */ u8*   sounds;     /* sound table    (stride 0x18)             */
    /* 0x30 */ u8*   section30;  /* array          (stride 0x48)             */
    /* 0x34 */ u8*   section34;  /* array          (stride 0x54)             */
} WorldData;

/* --- One level record inside WorldData.levels (stride 0x10C) ---------------
 * This is the SAME byte layout as struct level_data (game/leveldata.h,
 * Xbox shell3D.pdb misc.h Id=3267): every field/offset/size below lines up
 * exactly with level_data's (flags/name/bosstype/camidx../camera../
 * bosscamidx, then the fog+float tail this local view leaves padded).
 * ResolveWorldDataPointers is byte-exact MATCHED using these locally-named
 * fields (some, e.g. flags2/resolved, predate and are not yet GC-verified
 * against the Xbox enabled/setup spelling), so this view struct is kept
 * as-is rather than replaced by a literal `typedef level_data WorldLevel`
 * alias, to avoid adopting unverified names into a matched region for no
 * matching benefit (see claim.gcurlevel-is-level-data). gCurLevel itself
 * (the field this TU's de-fakematch pass actually needed named access to)
 * is declared as level_data* directly below instead, matching the other
 * TUs (enemy.c, items.c) that already reference the same global that way. */
struct WorldLevel {
    /* 0x00 */ u32   flags;      /* bit0 cleared during resolve              */
    /* 0x04 */ s16   flags2;     /* bit0 => level owns cameras               */
    /* 0x06 */ s16   resolved;   /* set to 1 once floats normalised          */
    /* 0x08 */ char  name[0x3C]; /* used to build the ambient-track name     */
    /* 0x44 */ u32   bossType;   /* copied to gBossType on activate          */
    /* 0x48 */ u8    _pad48[0x10];
    /* 0x58 */ s16   cameraIdx;  /* -> cameraPtr (WorldData.cameras[idx])    */
    /* 0x5A */ s16   audioIdx;   /* -> audioPtr  (WorldData.audio[idx])      */
    /* 0x5C */ s16   sec30Idx;   /* -> sec30Ptr  (WorldData.section30[idx])  */
    /* 0x5E */ u8    _pad5E[2];
    /* 0x60 */ u8*   cameraPtr;
    /* 0x64 */ u8*   audioPtr;
    /* 0x68 */ u8*   sec30Ptr;
    /* 0x6C */ u8*   sec34Ptr;
    /* 0x70 */ u8    _pad70[0x1C];
    /* 0x8C */ s16   sec34Idx;   /* -> sec34Ptr  (WorldData.section34[idx])  */
    /* 0x8E */ u8    _pad8E[0x7E]; /* = level_data's 0x8E..0x10C float tail;
                                      accessed via gCurLevel (level_data*)   */
    /* audio volume / range floats live at 0xA8..0xDC and are normalised     */
};

/* --- camera_data / audio_data / map_data / bosscam_data --------------------
 * level_data (game/leveldata.h) forward-declares these four as opaque
 * pointee types for its camera/audio/mapdata/bosscam fields (0x60/0x64/0x68/
 * 0x6C); this TU is where the bodies are needed, to name ResolveWorldData's
 * big-endian fix-up loops over the WAD's CAMS/AUDS/MAPS/BCAM section arrays
 * (WorldData.cameras/audio/section30/section34).
 *
 * GC OFFSET VERIFICATION: every field below is independently confirmed by
 * ResolveWorldData's own pre-existing byte-swap loop, which swaps EXACTLY
 * the byte ranges the Xbox layout marks numeric (s16/s32/f32) and skips
 * exactly the ranges it marks char - a complete field-by-field match for
 * all 26 camera_data fields, all 8 audio_data fields, both map_data fields
 * and all 13 bosscam_data fields (claim.law.swap-loop-is-record-layout-
 * ground-truth). camera_data extends newcam.c's prior 5-field verification
 * (minpitch/min/max/minrad/maxrad, Xbox misc.h Id=3269) to the full struct;
 * audio_data is Xbox audio.h Id=3270; map_data (misc.h Id=3272) resolves
 * auxscreen.c's previously-blocked init_mapscreen residual (init_mapscreen's
 * `route + 4` maps onto offsetof(map_data, offset) + 4 = offset[1]) - the
 * struct body was missing from the tsv struct index but present via a
 * direct grep, same class of miss as PBFRAMEBUF's; bosscam_data is misc.h
 * Id=3273. Xbox field spelling/order kept verbatim (matches leveldata.h's
 * own convention for level_data's fields). */
struct camera_data {
    /* 0x00 */ s16 dir;
    /* 0x02 */ s16 pitch_dir;
    /* 0x04 */ f32 dp;
    /* 0x08 */ f32 minpitch;
    /* 0x0C */ f32 min[3];
    /* 0x18 */ f32 max[3];
    /* 0x24 */ char limits;
    /* 0x25 */ char start_event;
    /* 0x26 */ s16 attcam;
    /* 0x28 */ f32 att_data;
    /* 0x2C */ f32 minrad;
    /* 0x30 */ f32 maxrad;
    /* 0x34 */ s16 enemax;
    /* 0x36 */ s16 special_radius;
    /* 0x38 */ f32 maxpitch;
    /* 0x3C */ f32 pitchsub;
    /* 0x40 */ f32 pitchmul;
    /* 0x44 */ f32 pitchadd;
    /* 0x48 */ f32 distmuladd;
    /* 0x4C */ f32 distmulfac;
    /* 0x50 */ f32 distmulmin;
    /* 0x54 */ f32 distmulmax;
    /* 0x58 */ f32 smooth;
    /* 0x5C */ f32 minyaw;
    /* 0x60 */ f32 maxyaw;
    /* 0x64 */ f32 bossminrad;
    /* 0x68 */ f32 bossmaxrad;
};                                          /* size 0x6C == WorldData.cameras stride */

struct audio_data {
    /* 0x00 */ char bank[16];
    /* 0x10 */ s16  entersnd;
    /* 0x12 */ s16  hitsnd;
    /* 0x14 */ s32  namesnd;
    /* 0x18 */ char stream[16];
    /* 0x28 */ s16  nareas;
    /* 0x2A */ s16  stereo;
    /* 0x2C */ s16  nparts[8];
};                                          /* size 0x3C == WorldData.audio stride */

struct map_data {
    /* 0x00 */ f32 offset[2];
    /* 0x08 */ f32 dash[8][2];
};                                          /* size 0x48 == WorldData.section30 stride */

struct bosscam_data {
    /* 0x00 */ s32 flags;
    /* 0x04 */ f32 maxyaw;
    /* 0x08 */ f32 cosmaxyaw;
    /* 0x0C */ f32 mindist;
    /* 0x10 */ f32 minpdist;
    /* 0x14 */ f32 maxdist;
    /* 0x18 */ f32 maxpdist;
    /* 0x1C */ f32 minpitch;
    /* 0x20 */ f32 maxpitch;
    /* 0x24 */ f32 minattn[3];
    /* 0x30 */ f32 maxattn[3];
    /* 0x3C */ f32 keyattn[3];
    /* 0x48 */ f32 wizattn[3];
};                                          /* size 0x54 == WorldData.section34 stride */

/* level_data.fog is declared as a raw u8[0x1C] blob (game/leveldata.h); the
 * Xbox layout underneath it is misc.h fog_data (Id=3297, size 0x1C), whose
 * fields are GC-verified below by the SAME levels-array swap loop this
 * struct is used to name (type/color[3] unswapped, 6 floats swapped at the
 * exact relative offsets fog_data lists). */
struct fog_data {
    /* 0x00 */ u8  type;
    /* 0x01 */ u8  color[3];
    /* 0x04 */ f32 intensity;
    /* 0x08 */ f32 density;
    /* 0x0C */ f32 min;
    /* 0x10 */ f32 max;
    /* 0x14 */ f32 nearw;
    /* 0x18 */ f32 farw;
};                                          /* size 0x1C == level_data.fog size */

/* WorldData.sounds (WAD tag "SNDS") is Xbox sound_data (audio.h Id=3271,
 * size 0x18 == the 24-byte sounds stride): desc[16]/idx/vol/pri. This
 * file-local view predates that identification and names the idx field
 * soundHandle (still accurate - AudioFindSound's return value IS a sound
 * index/handle); the sounds swap loop originally left vol/pri as a
 * `_pad14[4]` guess, but both are independently GC-verified real fields
 * (both individually byte-swapped) per
 * claim.law.swap-loop-is-record-layout-ground-truth. */
typedef struct WorldSoundView {
    char name[16];
    s32  soundHandle;
    s16  vol;
    s16  pri;
} WorldSoundView;

/* ---- real callees (names already resolved in config/GUNE5D/symbols.txt) --- */
extern s32   MBSetupWad(void* ctx, void* wadData);
extern void* MBGetFromWad(void* ctx, s32 tag, s32* outLen);
extern int   MBSetObject();
extern int   AudioFindSound(const char* name, int a, int b);
extern int   FileExists(const char* dev, const char* path);
extern int   FileSize(const char* dev, const char* path);
extern void* AllocMem(int size);
extern void  MLMReadFile(const char* dev, const char* path, int size, void* dst);
extern int   FatalErrorf(const char* fmt, ...);  /* ErrorPrintf-style logger  */
/* fn_80057F44 (world-registry hook) is defined in this TU below; its
 * prototype sits with the 0x80055CB8 block's declarations. */
extern int   sprintf(char* buf, const char* fmt, ...);
extern int   ErrorPrintf(const char* fmt, ...);
extern void  InitTexMods(void* data, s32 model);
extern void  fn_8001267C(void* data, s32 model, s32 index);
extern void  DoTexMods(void* data);
extern s32   gGameBusy;
extern s32   gGameplayPauseTimer;
extern s32   gGameMode;
extern s64   gControllerButtons;
extern Player gPlayers[4];
extern void  MBTreeSetFlags(void* node, s32 flags, s32 value);
extern WorldObj* FindWORLDOBJ(char* name);

/* ---- module data (real names from symbols.txt) --------------------------- */
extern s32   sWorldLevelTable[]; /* 0x8011C3C0 base; adjacent world tables    */
/* sWorldDataTypes (0x8011C4A8) is declared with its real 0x2C-stride record
 * type below, beside the block that reads its fields. */
extern s32   sCurWorldType;      /* 0x80343C28 cached resolved realm type     */
extern s32   sFirstWorldId;      /* 0x80343C2C first loaded realm id (<<8)    */
extern s32   sCurWorldIndex;     /* 0x80344844 index into type table          */
extern s32   sLastWorldLevel;    /* 0x80344820 last resolved worldlevel id    */
extern s32   sCurLevelHasCameras;/* 0x803448C0 first level index with cameras */
extern s32   sMusicTrackLo;      /* 0x803448D4 current level within realm     */
extern s32   sMusicTrackHi;      /* 0x803448D8 current realm data type        */
extern s32   sWorldDataConst;    /* 0x80344848 = 0xD00                        */
extern s32   gBossType;          /* 0x8034439C boss id of the active level    */
extern WorldData* gWorldData;    /* 0x80344838 active world-data header        */
extern level_data* gCurLevel;    /* 0x8034483C active level record            */

/* forward decl of the static BE fix-up pass */
static void ResolveWorldDataPointers(void);

/* ---- WAD section tags (.sdata2 4-char strings) --------------------------- */
extern char lbl_80346CB8[8]; /* "WRLD" - world header                        */
extern char lbl_80346CC0[8]; /* "LEVL" - level array                         */
extern char lbl_80346CC8[8]; /* "ENMY" - enemy section                       */
extern char lbl_80346CD0[8]; /* "CAMS" - camera array                        */
extern char lbl_80346CD8[8]; /* "AUDS" - audio array                         */
extern char lbl_80346CE0[8]; /* "SNDS" - sound table                         */
extern char lbl_80346CE8[8]; /* "MAPS" - map section                         */
extern char lbl_80346CF0[8]; /* "BCAM" - boss-camera section                 */

extern char  lbl_802576C0[]; /* WAD context buffer                           */
extern char  lbl_80112788[]; /* world-data string block                      */
extern s32   lbl_8034487C;   /* "WRLD" section length                        */
extern char* lbl_80344888;   /* active realm secondary name                  */
extern WorldLevel* lbl_80344840; /* next camera-owning level record          */
extern s32   lbl_803448A0;
extern s32   lbl_803448A4;
extern s32   lbl_803448B8;   /* active realm == 12                           */

/* ---- WAD section tag + in-place big-endian fixup helpers (same idioms as
 * critter.c's matched CritterWadTag/CritterSwap16/CritterSwap32) ---------- */
static inline s32 WorldWadTag(char* s)
{
    return (s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3];
}

static inline u16 WorldSwap16(u16 v)
{
    u8* p = (u8*)&v;
    return (u16)(p[0] | (p[1] << 8));
}

static inline u32 WorldSwap32(u32 v)
{
    u32 r;
    u8* s = (u8*)&v;
    u8* d = (u8*)&r;
    d[0] = s[3];
    d[1] = s[2];
    d[2] = s[1];
    d[3] = s[0];
    return r;
}

static inline f32 WorldSwapF(f32 v)
{
    u32 r = WorldSwap32(*(u32*)&v);
    return *(f32*)&r;
}

#define WSWAP16(p, off) *(u16*)((p) + (off)) = WorldSwap16(*(u16*)((p) + (off)))
#define WSWAP32(p, off) *(u32*)((p) + (off)) = WorldSwap32(*(u32*)((p) + (off)))
#define WSWAPF(p, off)  *(f32*)((p) + (off)) = WorldSwapF(*(f32*)((p) + (off)))

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

extern s32  lbl_802511FC[];        /* per-index sign-flip table          */
/* gGameOptions is a byte-offset view in this TU; the block below uses the
 * same *(s32*)(gGameOptions + N) form as the rest of the file. */
extern u8    gGameOptions[];
extern s32  lbl_802577CC[];        /* 8 keys                             */
extern s8*  lbl_8025776C[];        /* 8 parallel object pointers         */

extern s32   lbl_803441B0;
extern s32   lbl_803441B4;
extern s32   lbl_803441B8;
extern s32   lbl_803443BC;
extern s32   lbl_80344738;
extern void* lbl_803447B0;
extern s32   lbl_802512B0[];
extern s32   DoWorldAnimSub(void* track, void* animdata, void* animBase);
extern s32   lbl_80344DA4;
extern s32   lbl_80344DA0;
extern void  bulletproof_printf(const char* fmt, ...);
extern void  serve_busy(s32 arg0);
extern void  LoadItems(void);
extern void  MBCompVertScaleAddUV(s32 a, s32 b, f32 x, f32 y, f32 z,
                                  f32 u, f32 v);
extern void  InitItems(void);
extern s32   good_wiz_state;
extern s32   lbl_803447A4;
extern f32   lbl_80346BE4;
extern f32   lbl_80346BE8;
extern f32   lbl_80346BEC;
extern f32   lbl_80346BF0;
extern f32   lbl_80346BF4;
extern s32   BytesFree(void);
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

extern char  lbl_80257680[];       /* per-level enemy type table; also the
                                    * path scratch buffer further down */
typedef struct WorldMemTable {
    u8  _0[140];
    s32 sizes[8];     /* 0x8C */
    u8  _1[160];
    s32 typeids[8];   /* 0x14C */
} WorldMemTable;
extern s32   lbl_80344850;
extern s32   lbl_80344854;
extern s32   lbl_80344858;
extern s32   lbl_8034485C;
extern s32   lbl_80343C30;
extern s32   lbl_80344870;
extern u32   lbl_80344874;
extern s32*  lbl_80344878;
extern s32   lbl_80344D80;
extern s32   lbl_80344D84;
extern s32   lbl_80344D88;
extern s32   dbgTextEnable;
extern s32   mlmMemUsed;
extern void  WorldLoadModelDone(void* world);
extern s32   WorldLoadModelStart(void);
extern s32   StartWorldLoad(s32 arg0);
extern s32   StartLoadWorldAnim(void* world);
extern s32   FinishLoadWorldAnim(void);
extern void  MBOX_BGLoadModelStart(char* name, void* model);
extern s32   MBOX_BGLoadModelDone(void);
extern s32*  StartFileRead(char* wad, const char* name, s32 mode, s32 size,
                           void* dest, void* callback);
extern s32   CritterLoadStartNext(void);
extern s32   CritterLoadDone(s32 maxBytes);
extern void  LoadWeapons(void);
extern s32   lbl_80344768;
extern void* gWadAtreeHeaders[];
extern u32   pbLoad;
extern s32   lbl_803447B8;
extern s32   gScriptedCameraState;
extern char* lbl_8011B578[];       /* category name-string table */
extern void  CreateDynobjGrid(void);
extern void  SetupDynGrid(void);
extern void  MBTreeClearFlags(void* node, s32 flags, s32 arg2);
extern void  MBTreeSetAlpha(void* node, s32 alpha, s32 arg2);
extern s32   fn_80057F44(s32 code, s32 mask);
void ResolveWorldData(int worldlevel);   /* defined below in this TU */
s32  LoadModel(const char* name, void** outData, s32 initTexMods, s32 model);
void FatalError(const char* msg, int code);

/* Forward declarations inside this block (target .text order puts some
 * callers ahead of their callees). */
void* fn_80057ACC(s32 key);
void  PrintWorldMemSizes(void);
s32   GetEnemySubtype(s32 type);
s32   InLevel(const char* tag);
s32   NextAttractWave(s32 level);
s32   PrevWorldLevel(s32 waveMask);
s32   NextWorldLevel(s32 waveMask);
s32   WorldExplosion(s32 arg0);
s32   fn_80055F68(s32 arg0, s32 arg1);
s32   fn_80056698(s32 arg0, s32 arg1);
void  fn_8005636C(s32* s);
void  fn_80057024(void);
s32   fn_80057BC8(s32 type);
void* LevelItemDesc(void);
void* WorldItemDesc(void);
void  GetEnemyTypes(void);
void  DoWorldAnimation(void);
void  WorldObjectExplode(void* node, s32 arg1);
s32   init_next_level_8005638C(s32 arg0);

/* Functions that stayed in game/game/gamemain.c (GAMEMAIN.OBJ) and are
 * called from this block. */
extern void  AllocEnemy(s32 id, s32 model);
extern void  LoadEnemy(s32 id, s32 model);
extern void  fn_800508A0(void);
extern void  fn_80050910(s32 arg0);
extern void  fn_80050DD8(char* buf, s32 id, s32 qty);
extern void  LockModels(s32 arg0);
extern void  TransitionBlitShow(s32 arg0);

/* 0x8005638C -- build the "levels/level%s" path and load a level. */
extern u32  lbl_803448D0;
extern s32  lbl_803448CC;
extern s32  lbl_803448C8;
extern s32  lbl_803448C4;
extern s32  LoadWorldDone(void* name);
extern void CritterLoadFile(const char* wad, const char* name);
extern void CritterLoadAllTypes(s32 arg0);

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
extern s32  lbl_8034484C;
extern s32  lbl_8034486C;
extern s32  lbl_80344884;
extern s32  lbl_8023E558[];
extern f32  gDefaultPlayerPosition[3];
extern s32  towerGetRuneNearStat(s32 player, s32 world);
extern void WorldSaveInitState(void);
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
extern s32  lbl_803447B4;
extern Critter* gBossObj;
extern s32  gBossDead;
extern f32  gClockTime;
extern Effect Effects[]; /* live fx pool, stride 0xF0 (game/effect.h) */
extern void DoGoodWizard(void);
extern void ProcessSpewItems(void);
extern s32  CamGetPlayerAvgPos(f32* pos, s32 mode);
extern void StartEnterFX(f32* pos);
extern void fn_8009D288(f32* pos);
extern void fn_80067AE0(f32 a, f32 b);
extern s32  sndFxQueUpdate(void);
extern void* MBOX_FindObject(char* name);
extern s32  DeleteEffect(s32 idx, s32 mode);
extern void fn_8009C9DC(s32 mode, void* pos);

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

extern char lbl_801129F8[];   /* "couldn't find world data" fatal fmt */

void world_update(void);

/* The .text run 0x80055CB8..0x80058078 (DoWorldAnimation .. fn_80057F44) is
 * gauntworld.obj's, not GAMEMAIN.OBJ's: contiguous text, extab and extabindex,
 * .rodata reads at 0x80112788 and above, and an .sdata2 pool interleaved with
 * ResolveWorldDataPointers' (measured 2026-09-04). Functions follow target
 * .text order. */

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

/* Complete the worldinfo.h forward declarations with the layouts already
 * used by world.c. GC allocation/swap loops confirm the 0x10/0xA0 strides;
 * the track's keyframe-data pointer is at +0x0C. */
struct worldanim {
    /* 0x00 */ s16   objidx;   /* index into gWorldInfo.wobjs                */
    /* 0x02 */ s16   nframes;  /* frame count                                */
    /* 0x04 */ u16   fixed;    /* 0x8000 = data offset already relocated     */
    /* 0x06 */ s16   state;    /* run-time state/direction flag bits         */
    /* 0x08 */ f32   curframe; /* current frame position (advanced by 30*dt) */
    /* 0x0C */ void* data;     /* keyframe stream (little-endian in file)    */
};
struct animdata {
    /* 0x00 */ s32 seq;
    /* 0x04 */ s32 used;
    /* 0x08 */ s16 pidx;
    /* 0x0A */ s16 nidx;
    /* 0x0C */ s32 keycount;
    /* 0x10 */ f32 ppyr[4];
    /* 0x20 */ f32 npyr[4];
    /* 0x30 */ f32 xpyr[4];
    /* 0x40 */ f32 ppos[4];
    /* 0x50 */ f32 npos[4];
    /* 0x60 */ f32 xpos[4];
    /* 0x70 */ f32 pscale[4];
    /* 0x80 */ f32 nscale[4];
    /* 0x90 */ f32 xscale[4];
};

/* Advance every active world-object animation track. */
void DoWorldAnimation(void)
{
    s32* count;
    s32* header;
    s32 i;
    u8* anim_base;

    if ((void*)gWorldInfo.atreelist != NULL) {
        DoTexMods((void*)gWorldInfo.atreelist);
    }
    if ((lbl_80344768 > 0 || (gGameMode & MODE_GROUP_ATTRACT) != 0) &&
        (lbl_803443BC <= 10 || lbl_803443BC >= 100000) &&
        gWorldInfo.nworldanims != 0 && (void*)gWorldInfo.animheader != NULL) {
        count = &gWorldInfo.nworldanims;
        header = (s32*)gWorldInfo.animheader;
        anim_base = (u8*)header[3];
        i = 0;
        lbl_803441B8 = header[0];
        lbl_803441B4 = header[1];
        lbl_803441B0 = header[2];
        while (i < *count) {
            struct worldanim* track = &gWorldInfo.worldanims[i];
            if (track->data != NULL) {
                DoWorldAnimSub(track, &gWorldInfo.animdata[i], anim_base);
            }
            i++;
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

/* 0x80055F68 -- asynchronous world/model/atree/critter load state machine. */
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
            size = FileSize(name, "anim");
            lbl_80344878 = StartFileRead(name, "anim", 0, size,
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
        fn_8001267C(gWadAtreeHeaders[type], lbl_802512B0[type], -1);
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

/* 0x8005636C -- advance a two-field counter unless it is parked at 2. */
void fn_8005636C(s32* s)
{
    if (s[4] == 2) {
        return;
    }
    s[1] += s[2];
}

#pragma opt_common_subs off
s32 init_next_level_8005638C(s32 arg0)
{
    s32 off;
    s32 flag;
    u8* w;
    s32 id;
    s32 t;
    u8* tbl = (u8*)lbl_80257680;
    u8* q;
    s32 i;
    s32 result;

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
    sprintf((char*)(tbl + 172), "levels/level%s", gCurLevel->name);
    lbl_80343C30 = 0;
    lbl_80344854 = mlmMemUsed;
    result = LoadWorldDone(tbl + 172);
    GetEnemyTypes();

    if (*(s32*)(gGameOptions + 8) < 2 && gBossType < 0 && arg0 != sWorldDataConst &&
        lbl_80344738 < 0) {
        lbl_80344738 = LoadModel("gen", 0, 0, -1);
    }

    for (i = 0, off = 0; i < 8; i++, off += 4) {
        q = tbl + off;
        t = *(s32*)(q += 332);

        w = tbl + off;
        *(s32*)(w += 140) = 0;
        id = t;
        flag = 1;
        switch (t) {
        case E_DRAGON:
            CritterLoadFile("critter", "dragon.wad");
            break;
        case E_CHIMERA:
            CritterLoadFile("critter", "chimera.wad");
            break;
        case E_DJINN:
            CritterLoadFile("critter", "djinn.wad");
            break;
        case E_DRIDER:
            CritterLoadFile("critter", "drider.wad");
            break;
        case E_PBOSS:
            CritterLoadFile("critter", "pboss.wad");
            break;
        case E_YETI:
            CritterLoadFile("critter", "yeti.wad");
            break;
        case E_LICH:
            CritterLoadFile("critter", "lich.wad");
            break;
        case E_WRAITH:
            CritterLoadFile("critter", "wraith.wad");
            break;
        case E_SKORNE1:
            CritterLoadFile("critter", "skorne1.wad");
            break;
        case E_SKORNE2:
            CritterLoadFile("critter", "skorne2.wad");
            break;
        case E_GARM:
            CritterLoadFile("critter", "garm.wad");
            break;
        case E_GOLEM:
            if (sMusicTrackHi == 9) {
                CritterLoadFile("critter", "golemI.wad");
            } else if (sMusicTrackHi == 6) {
                CritterLoadFile("critter", "golemF.wad");
            } else {
                CritterLoadFile("critter", "golem.wad");
            }
            break;
        case E_GENERAL:
            CritterLoadFile("critter", "general.wad");
            break;
        case E_GARGOYLE: {
            char buf[16];
            u8* e = tbl + off;
            sprintf(buf, "gar_%s.wad", *(char**)(e + 236) + 16);
            CritterLoadFile("critter", buf);
            break;
        }
        default:
            flag = 0;
            if (id >= 0) {
                u8* e;
                *(s32*)w = BytesFree();
                e = tbl + off;
                AllocEnemy(id, *(s32*)(e + 268));
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

/* 0x800579E0 -- does the given 4-char tag match the current level id?
 * OUT OF .text ORDER ON PURPOSE: the target INLINES this function twice into
 * fn_80057024 (0x80057024), which .text places 0x9BC bytes BEFORE it, and
 * MWCC only inlines a function already defined.  Emitting the block in .text
 * order therefore costs fn_80057024 its two inline expansions (measured
 * 2026-09-04: 92.4062% -> 73.8923%, two `bl` where the target has 26
 * instructions of lbz/extsb/cmpw/bne).  So the real TU's SOURCE order is not
 * its .text order for this function, and this definition is hoisted here to
 * keep the expansions.  Falsify with
 * `python tools/gdl/fndiff.py game/world/gauntworld fn_80057024 --ops`. */
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
    if (gBossType >= 0 && gGameMode == MG_PLAY) {
        if (good_wiz_state) {
            DoGoodWizard();
        }
        ProcessSpewItems();
    }
    if (gGameMode == MG_PLAY || gGameMode == MG_ROUND_START) {
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
                        col, hdr[0], *(f32*)(hdr + offsetof(FogData, min)), *(f32*)(hdr + offsetof(FogData, max)),
                        (f32)(lbl_80346C10 * *(f32*)(hdr + offsetof(FogData, nearw))),
                        (f32)(lbl_80346C10 * *(f32*)(hdr + offsetof(FogData, farw))),
                        (f32)(lbl_80346C38 * *(f32*)(hdr + offsetof(FogData, density))));
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
                    col, hdr[0], *(f32*)(hdr + offsetof(FogData, min)), *(f32*)(hdr + offsetof(FogData, max)),
                    (f32)(lbl_80346C10 * *(f32*)(hdr + offsetof(FogData, nearw))),
                    (f32)(lbl_80346C10 * *(f32*)(hdr + offsetof(FogData, farw))),
                    (f32)(lbl_80346C38 * *(f32*)(hdr + offsetof(FogData, density))));
                lbl_80344868 = 1;
            }
        }
    }
    {
        u8* lv = (u8*)gCurLevel;

        if ((s8)lv[8] == (s8)"C5"[0] &&
            ((s8)lv[9] == 0 || (s8)lv[9] == (s8)"C5"[1]) &&
            ((s8)lv[10] == 0 || (s8)lv[10] == (s8)"C5"[2]) &&
            ((s8)lv[11] == 0 || (s8)lv[11] == (s8)"C5"[3])) {
            cond = 1;
        } else {
            cond = 0;
        }
    }
    if (cond && gGameMode == MG_PLAY && gBossObj != NULL &&
        gBossObj->state != 0) {
        {
            WorldObj* w = FindWORLDOBJ(strs + 0xd0);

            if (w != 0 && w->nodeptr != 0) {
                w->nodeptr->flags |= 2;
            } else {
                ErrorPrintf(strs + 0xdc);
            }
        }
        {
            WorldObj* w = FindWORLDOBJ(strs + 0xfc);

            if (w != 0 && w->nodeptr != 0) {
                w->nodeptr->flags |= 2;
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
            WorldObj* wo = *(WorldObj**)(row + 0x4c);
            u8* node;
            f32* timer;

            if (wo == 0) {
                continue;
            }
            node = (u8*)wo->nodeptr;
            if (node == 0) {
                continue;
            }
            if (*(s32*)(node + 0x60) & 2) {
                timer = (f32*)(row + 0x5c);
                if (sMusicFadeBase >= *timer) {
                    fn_80067AE0(lbl_80346C4C, lbl_80346C50);
                    MBTreeClearFlags(wo->nodeptr, 2, 0);
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
    if (gGameMode == MG_PLAY && lbl_803447B4 != 0) {
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
                gBossObj->unkAC8 = lbl_80346BF0;
            }
            break;
        case 0x2a:
            if ((f32)(lbl_80346C88 - d) <= lbl_80346C70) {
                kill = 1;
                gBossObj->unkAC8 = lbl_80346BF0;
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
                mbnode* o = (mbnode*)gBossObj->hitnode1;

                if (o != 0 && o->child != 0) {
                    MBSetObject(o->child, found);
                }
                gBossObj->unkAC6 = 0;
                lbl_8034489C = 6;
            }
            break;
        }
        if (kill) {
            if (lbl_80344894 >= 0) {
                lbl_80344894 = DeleteEffect(lbl_80344894, 1);
                fn_8009C9DC(3, gBossObj->movevec);
                fn_8009C9DC(4, gBossObj->movevec);
            }
            lbl_8034489C = 6;
        }
        if (lbl_80344890 >= 0) {
            u8* e = (u8*)&Effects[lbl_80344890];
            f32 dt = *(f32*)(e + 0x68) - gClockTime;

            if (dt < lbl_80346C40) {
                if (!(*(s32*)(e + 0x64) & 0x4020)) {
                    mbnode* o = *(mbnode**)(e + 0x14);

                    if (o != 0) {
                        s32 al = (s32)(lbl_80346C90 * dt);

                        while (al > 255) {
                            al -= 255;
                        }
                        MBTreeSetAlpha(
                            o->child->child, al, 2);
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
                    ((Player*)q)->quest_state = 0;
                } else if (towerGetRuneNearStat(i, sMusicTrackHi) != 0) {
                    ((Player*)q)->quest_state = 1;
                    lbl_8034489C = 1;
                    lbl_80344898 = z;
                } else {
                    ((Player*)q)->quest_state = 0;
                }
            }
        }
    }

    WorldSaveInitState();
    lbl_80344880 = (f32)(gWorldInfo.worldmin[1] - lbl_80346C98);
    GetEnemyTypes();

    if (*(s32*)(gGameOptions + 8) < 2 && gBossType < 0 && sMusicTrackHi != 13 &&
        lbl_80344738 < 0) {
        lbl_80344738 = LoadModel("gen", 0, 0, -1);
    }

    {
        s32* pool = lbl_802511FC;
        for (off = 0, i = 0; i < 8; i++, off += 4) {
            s32 raw = *(s32*)(tbl + off + 332);
            s32 t = raw;
            if (raw < E_GARGOYLE) {
                if (raw == E_GOLEM) {
                    continue;
                }
            } else {
                if (raw >= E_MAXTYPES) {
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
        if (!(gGameMode & MODE_GROUP_ATTRACT)) {
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

    if (InLevel("A5")) {
        lbl_8034484C = 4;
        strcpy((char*)(tbl + 108), fmt + 312);
    } else if (InLevel("F2")) {
        lbl_8034484C = 4;
        strcpy((char*)(tbl + 108), fmt + 328);
    } else {
        lbl_8034484C = 0;
    }

    {
        f64 kOff = lbl_80346C60;
        for (i = 0, off = 0; i < lbl_8034484C; i++, off += 4) {
            sprintf((char*)tbl, "%s%d", tbl + 108, i + 1);
            *(s32*)(tbl + off + 76) = (s32)FindWORLDOBJ((char*)tbl);
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
    if (gGameMode == MG_PLAY || gGameMode == MG_ROUND_START) {
        MBCompVertScaleAddUV(lbl_8034486C, 0, lbl_80346C90, lbl_80346C90,
                             lbl_80346C30, lbl_80346BF0, lbl_80346BF0);
    } else {
        MBCompVertScaleAddUV(0, 0, lbl_80346BF0, lbl_80346BF0, lbl_80346BF0,
                             lbl_80346BF0, lbl_80346BF0);
        sLevelAmbientScale = lbl_80346BE0;
    }
    if (!(gGameMode & MODE_GROUP_ATTRACT)) {
        lbl_80344850 = 1;
    }
    mini_inventory_setup();
    AppendBigapePowerupsToScene();
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
    if (*(s32*)(gGameOptions + 8) < 2) {
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
                if (value >= 0 && value < E_NTYPES) {
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
    case E_SCORP:
    case E_RAT:
    case E_SNAKE:
    case E_SPIDER:
    case E_MAGGOT:
    case E_WOLF:
    case E_DOG:
    case E_ACID:
    case E_HAND:
        subtype = 1;
        break;
    case E_TROLL:
    case E_GRUNT:
    case E_SORCERER:
    case E_LIZARDMAN:
    case E_ZOMBIE:
    case E_SKELETON:
    case E_IMP:
        subtype = 3;
        break;
    case E_DEMON:
    case E_KNIGHT:
    case E_MUMMY:
    case E_TREEFOLK:
    case E_PLAGUE:
    case E_ICE:
    case E_WORM:
    case E_GHOST:
    case E_WARLOCK:
    case E_SKY:
    case E_GARM2:
        subtype = 4;
        break;
    case E_GOLEM:
    case E_GENERAL:
        subtype = 5;
        break;
    case E_DEATH:
        subtype = 6;
        break;
    case E_IT:
        subtype = 7;
        break;
    case E_GARGOYLE:
        subtype = 8;
        break;
    case E_DRAGON:
    case E_CHIMERA:
    case E_DJINN:
    case E_DRIDER:
    case E_PBOSS:
    case E_YETI:
    case E_WRAITH:
    case E_LICH:
    case E_SKORNE1:
    case E_SKORNE2:
    case E_GARM:
        subtype = 9;
        break;
    }
    return subtype;
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
            WorldLevel* levels;
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
        return *(s32*)(gGameOptions + 36);
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
        return *(s32*)(gGameOptions + 36);
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

/* --------------------------------------------------------------------------
 * ResolveWorldData(worldlevel)  0x80058078
 *
 * Activate a realm+level: find the realm's loaded WAD, pull its sections with
 * MBSetupWad/MBGetFromWad, byte-swap every packed structure for the big-endian
 * GameCube, run the pointer fix-up pass, then cache the current world/level
 * globals (music track, boss type, first camera level, ...).
 *
 *   worldlevel = (realmType << 8) | levelIndex
 *
 * The realm records live 232 bytes past sWorldLevelTable (the adjacent
 * sWorldDataTypes object), stride 0x2C; the per-realm WAD pointers are another
 * 848 bytes past sWorldLevelTable, stride 4.  Array section counts are derived
 * from the gap between consecutive section pointers. */
void ResolveWorldData(int worldlevel)
{
    int level;
    int realm;
    u32 i;
    u8* rec;
    int off;
    u8* wt;
    char* strs;
    void* ctx;
    s32 setup;
    int n;
    int j;
    int k;
    int count;
    u8* p;
    u8* q;
    u8* q2;
    u8* q3;

    wt = (u8*)sWorldLevelTable;
    strs = lbl_80112788;
    if (worldlevel < 0) {
        return;
    }
    level = worldlevel & 0xFF;

    if (sCurWorldType != (realm = worldlevel >> 8)) {
        off = 0;
        for (i = 0; i < 14; i++, off += 44) {
            rec = wt + off;
            if (realm != *(s32*)(rec += 232)) {
                continue;
            }
            if (*(s32*)(rec + 16) != 0) {
                ctx = lbl_802576C0;
                {
                    void** wadtab = (void**)(wt + i * 4);
                    setup = MBSetupWad(ctx, wadtab[212]);
                }
                gWorldData = (WorldData*)MBGetFromWad(ctx,
                                                      WorldWadTag(lbl_80346CB8),
                                                      &lbl_8034487C);
                if ((u8)setup) {
                    gWorldData->id = WorldSwap32(gWorldData->id);
                    WSWAP16((u8*)gWorldData, 20);
                    WSWAP16((u8*)gWorldData, 22);
                    WSWAP16((u8*)gWorldData, 24);
                    WSWAP16((u8*)gWorldData, 26);
                }
                if (lbl_8034487C == 0) {
                    FatalErrorf(strs + 640, i);
                }
                gWorldData->levels = (WorldLevel*)MBGetFromWad(ctx,
                                                               WorldWadTag(lbl_80346CC0), 0);
                gWorldData->section20 = MBGetFromWad(ctx,
                                                     WorldWadTag(lbl_80346CC8), 0);
                gWorldData->cameras = (u8*)MBGetFromWad(ctx,
                                                        WorldWadTag(lbl_80346CD0), 0);
                gWorldData->audio = (u8*)MBGetFromWad(ctx,
                                                      WorldWadTag(lbl_80346CD8), 0);
                gWorldData->sounds = (u8*)MBGetFromWad(ctx,
                                                       WorldWadTag(lbl_80346CE0), 0);
                gWorldData->section30 = (u8*)MBGetFromWad(ctx,
                                                          WorldWadTag(lbl_80346CE8), 0);
                gWorldData->section34 = (u8*)MBGetFromWad(ctx,
                                                          WorldWadTag(lbl_80346CF0), 0);
                sCurWorldType  = realm;
                sCurWorldIndex = i;
                if ((u8)setup) {
                    n = 0;
                    off = 0;
                    while (n < gWorldData->numLevels) {
                        p = (u8*)gWorldData->levels + off;
                        WSWAP32(p, offsetof(level_data, camera));
                        WSWAP32(p, offsetof(level_data, audio));
                        WSWAP32(p, offsetof(level_data, mapdata));
                        WSWAP32(p, offsetof(level_data, bosscam));
                        WSWAP32(p, offsetof(level_data, flags));
                        WSWAP16(p, offsetof(level_data, enabled));
                        WSWAP16(p, offsetof(level_data, setup));
                        WSWAP16(p, offsetof(level_data, wavetime));
                        WSWAP16(p, offsetof(level_data, dummy));
                        WSWAP32(p, offsetof(level_data, bosstype));
                        WSWAP32(p, offsetof(level_data, earlyenemies));
                        WSWAP16(p, offsetof(level_data, camidx));
                        WSWAP16(p, offsetof(level_data, audidx));
                        WSWAP16(p, offsetof(level_data, bosscamidx));
                        WSWAP16(p, offsetof(level_data, maxenemies));
                        WSWAP16(p, offsetof(level_data, rune));
                        WSWAP16(p, offsetof(level_data, legend));
                        WSWAPF(p, offsetof(level_data, musicvol));
                        WSWAPF(p, offsetof(level_data, soundvol));
                        WSWAPF(p, offsetof(level_data, plevel));
                        WSWAPF(p, offsetof(level_data, xpmul));
                        WSWAPF(p, offsetof(level_data, damagemul));
                        WSWAPF(p, offsetof(level_data, difficulty));
                        WSWAPF(p, offsetof(level_data, ene_health));
                        WSWAPF(p, offsetof(level_data, ene_speed));
                        WSWAPF(p, offsetof(level_data, ene_visrad));
                        WSWAPF(p, offsetof(level_data, ene_attack));
                        WSWAPF(p, offsetof(level_data, ene_damage));
                        WSWAPF(p, offsetof(level_data, ene_mrate));
                        WSWAPF(p, offsetof(level_data, ene_mspeed));
                        WSWAPF(p, offsetof(level_data, ene_macc));
                        WSWAPF(p, offsetof(level_data, gen_health));
                        WSWAPF(p, offsetof(level_data, gen_rate));
                        WSWAPF(p, offsetof(level_data, gen_max));
                        WSWAPF(p, offsetof(level_data, trap_rate));
                        WSWAPF(p, offsetof(level_data, trap_damage));
                        WSWAP32(p, offsetof(level_data, shop_maxgold));
                        WSWAP32(p, offsetof(level_data, shop_maxkills));
                        WSWAP32(p, offsetof(level_data, shop_maxexp));
                        WSWAPF(p, offsetof(level_data, ambient));
                        WSWAPF(p, offsetof(level_data, lightinten));
                        WSWAP16(p, offsetof(level_data, mapidx));
                        for (j = 0; j < 3; j++) {
                            q = p + j * 4;
                            WSWAPF(q, offsetof(level_data, lightcolor_fp));
                            q += offsetof(level_data, lightdir);
                            *(f32*)q = WorldSwapF(*(f32*)q);
                        }
                        for (j = 0; j < 6; j++) {
                            q = p + j * 2;
                            q2 = q + offsetof(level_data, enemytype);
                            WSWAP16(q2, 0);
                        }
                        WSWAPF(p, offsetof(level_data, fog) + offsetof(struct fog_data, intensity));
                        WSWAPF(p, offsetof(level_data, fog) + offsetof(struct fog_data, density));
                        WSWAPF(p, offsetof(level_data, fog) + offsetof(struct fog_data, min));
                        WSWAPF(p, offsetof(level_data, fog) + offsetof(struct fog_data, max));
                        WSWAPF(p, offsetof(level_data, fog) + offsetof(struct fog_data, nearw));
                        WSWAPF(p, offsetof(level_data, fog) + offsetof(struct fog_data, farw));
                        n++;
                        off += sizeof(level_data);
                    }
                    n = 0;
                    off = 0;
                    while (n < gWorldData->numSounds) {
                        p = (u8*)gWorldData->sounds + off;
                        WSWAP32(p, offsetof(WorldSoundView, soundHandle));
                        WSWAP16(p, offsetof(WorldSoundView, vol));
                        WSWAP16(p, offsetof(WorldSoundView, pri));
                        n++;
                        off += sizeof(WorldSoundView);
                    }
                    count = (u32)((u8*)gWorldData->section34 -
                                  (u8*)gWorldData->section20) / 24;
                    off = 0;
                    while (count > 0) {
                        p = (u8*)gWorldData->section20 + off;
                        WSWAP32(p, 0);
                        WSWAP32(p, 4);
                        off += 24;
                        count--;
                    }
                    count = (u32)((u8*)gWorldData->sounds -
                                  (u8*)gWorldData->cameras) / sizeof(struct camera_data);
                    n = 0;
                    off = 0;
                    while (n < count) {
                        p = (u8*)gWorldData->cameras + off;
                        WSWAP16(p, offsetof(struct camera_data, dir));
                        WSWAP16(p, offsetof(struct camera_data, pitch_dir));
                        WSWAPF(p, offsetof(struct camera_data, dp));
                        WSWAPF(p, offsetof(struct camera_data, minpitch));
                        WSWAP16(p, offsetof(struct camera_data, attcam));
                        WSWAPF(p, offsetof(struct camera_data, att_data));
                        WSWAPF(p, offsetof(struct camera_data, minrad));
                        WSWAPF(p, offsetof(struct camera_data, maxrad));
                        WSWAP16(p, offsetof(struct camera_data, enemax));
                        WSWAP16(p, offsetof(struct camera_data, special_radius));
                        WSWAPF(p, offsetof(struct camera_data, maxpitch));
                        WSWAPF(p, offsetof(struct camera_data, pitchsub));
                        WSWAPF(p, offsetof(struct camera_data, pitchmul));
                        WSWAPF(p, offsetof(struct camera_data, pitchadd));
                        WSWAPF(p, offsetof(struct camera_data, distmuladd));
                        WSWAPF(p, offsetof(struct camera_data, distmulfac));
                        WSWAPF(p, offsetof(struct camera_data, distmulmin));
                        WSWAPF(p, offsetof(struct camera_data, distmulmax));
                        WSWAPF(p, offsetof(struct camera_data, smooth));
                        WSWAPF(p, offsetof(struct camera_data, minyaw));
                        WSWAPF(p, offsetof(struct camera_data, maxyaw));
                        WSWAPF(p, offsetof(struct camera_data, bossminrad));
                        WSWAPF(p, offsetof(struct camera_data, bossmaxrad));
                        for (j = 0; j < 3; j++) {
                            q = p + j * 4;
                            q2 = q + offsetof(struct camera_data, min);
                            q3 = q + offsetof(struct camera_data, max);
                            WSWAPF(q2, 0);
                            WSWAPF(q3, 0);
                        }
                        n++;
                        off += sizeof(struct camera_data);
                    }
                    count = (u32)((u8*)gWorldData->section30 -
                                  (u8*)gWorldData->audio) / sizeof(struct audio_data);
                    n = 0;
                    off = 0;
                    while (n < count) {
                        p = (u8*)gWorldData->audio + off;
                        WSWAP16(p, offsetof(struct audio_data, entersnd));
                        WSWAP16(p, offsetof(struct audio_data, hitsnd));
                        WSWAP32(p, offsetof(struct audio_data, namesnd));
                        WSWAP16(p, offsetof(struct audio_data, nareas));
                        WSWAP16(p, offsetof(struct audio_data, stereo));
                        for (j = 0; j < 8; j++) {
                            q = p + j * 2;
                            q2 = q + offsetof(struct audio_data, nparts);
                            WSWAP16(q2, 0);
                        }
                        n++;
                        off += sizeof(struct audio_data);
                    }
                    count = (u32)((u8*)gWorldData->levels -
                                  (u8*)gWorldData->section30) / sizeof(struct map_data);
                    n = 0;
                    off = 0;
                    while (n < count) {
                        p = (u8*)gWorldData->section30 + off;
                        for (j = 0; j < 2; j++) {
                            WSWAPF(p, offsetof(struct map_data, offset) + j * 4);
                        }
                        for (k = 0; k < 8; k++) {
                            q = p + k * 8;
                            for (j = 0; j < 2; j++) {
                                WSWAPF(q, offsetof(struct map_data, dash) + j * 4);
                            }
                        }
                        n++;
                        off += sizeof(struct map_data);
                    }
                    count = (u32)((u8*)gWorldData->cameras -
                                  (u8*)gWorldData->section34) / sizeof(struct bosscam_data);
                    n = 0;
                    off = 0;
                    while (n < count) {
                        p = (u8*)gWorldData->section34 + off;
                        WSWAP32(p, offsetof(struct bosscam_data, flags));
                        WSWAPF(p, offsetof(struct bosscam_data, maxyaw));
                        WSWAPF(p, offsetof(struct bosscam_data, cosmaxyaw));
                        WSWAPF(p, offsetof(struct bosscam_data, mindist));
                        WSWAPF(p, offsetof(struct bosscam_data, minpdist));
                        WSWAPF(p, offsetof(struct bosscam_data, maxdist));
                        WSWAPF(p, offsetof(struct bosscam_data, maxpdist));
                        WSWAPF(p, offsetof(struct bosscam_data, minpitch));
                        WSWAPF(p, offsetof(struct bosscam_data, maxpitch));
                        for (j = 0; j < 3; j++) {
                            q = p + j * 4;
                            WSWAPF(q, offsetof(struct bosscam_data, minattn));
                            WSWAPF(q, offsetof(struct bosscam_data, maxattn));
                            WSWAPF(q, offsetof(struct bosscam_data, keyattn));
                            q += offsetof(struct bosscam_data, wizattn);
                            *(f32*)q = WorldSwapF(*(f32*)q);
                        }
                        n++;
                        off += sizeof(struct bosscam_data);
                    }
                }
                p = wt + off;
                if (*(s32*)(p += 252) < 0) {
                    *(s32*)p = gWorldData->numLevels;
                }
                ResolveWorldDataPointers();
                lbl_80344888 = (char*)(rec + 28);
                break;
            }
            FatalErrorf(strs + 672, rec + 4);
        }
        if (i == 14) {
            FatalErrorf(strs + 692, realm);
        }
    }

    if (level < 0 || level >= gWorldData->numLevels) {
        level = 0;
    }
    gWorldData->curLevel = (s16)level;
    gCurLevel = (level_data*)&gWorldData->levels[level];
    sMusicTrackLo  = level;
    sMusicTrackHi  = realm;
    lbl_803448B8 = (realm == 12);
    sLastWorldLevel = worldlevel;
    gBossType      = gCurLevel->bosstype;

    /* first level (from the current one) that owns cameras */
    count = gWorldData->numLevels;
    level++;
    n = level * sizeof(level_data);
    goto camera_check;
camera_next:
    level++;
    n += sizeof(level_data);
camera_check:
    if (level < count &&
        !(*(s16*)((u8*)gWorldData->levels + n + offsetof(level_data, enabled)) & 1)) {
        goto camera_next;
    }
    if (level < count) {
        lbl_80344840 = &gWorldData->levels[level];
    }
    lbl_803448A4 = 9;
    lbl_803448A0 = 9;
}

/* --------------------------------------------------------------------------
 * ResolveWorldDataPointers()  0x80059CB4   (static)
 *
 * Called only by ResolveWorldData.  Turns the packed per-level section indices
 * into absolute pointers, clamps negatives, normalises the audio volume/range
 * floats and resolves ambient-track sound handles via AudioFindSound. */
extern char optionsAudioAndPrefs30[]; /* 0x80274E80 options block          */
typedef struct OptsView {
    u8  _pad[20];
    s32 vol; /* +20: volume option index */
} OptsView;
typedef struct WorldAudioView {
    u8 _pad00[20];
    s32 soundHandle;
    u8 _pad18[36];
} WorldAudioView;
extern f32  lbl_8011C748[3];           /* volume gain table                 */

static void ResolveWorldDataPointers(void)
{
    char* strs = lbl_80112788;
    level_data* lvl;
    WorldLevel* level;
    s32 i;
    f32 one;
    f64 sent;
    f32 d;
    f32* gp;
    f32 gain;
    char nameBuf[12];

    if (gWorldData->cameras == 0) {
        FatalErrorf(strs + 716, nameBuf);
    }
    if (gWorldData->audio == 0) {
        FatalErrorf(strs + 748, nameBuf);
    }
    sCurLevelHasCameras = -1;
    sent = lbl_80346C70;
    one = lbl_80346BE0;

    for (i = 0; i < gWorldData->numLevels; i++) {
        lvl = (level_data*)&gWorldData->levels[i];
        level = (WorldLevel*)lvl;

        if (level->cameraIdx < 0) {
            level->cameraIdx = 0;
        }
        level->cameraPtr = gWorldData->cameras + level->cameraIdx * 108;
        {
            s16 v34 = level->sec34Idx;
            if (v34 < 0) {
                level->sec34Ptr = 0;
            } else {
                level->sec34Ptr = gWorldData->section34 + v34 * 84;
            }
        }
        if (level->audioIdx < 0) {
            level->audioIdx = 0;
        }
        level->audioPtr = gWorldData->audio + level->audioIdx * 60;
        {
            s16 v30 = level->sec30Idx;
            if (v30 < 0) {
                level->sec30Ptr = 0;
            } else {
                level->sec30Ptr = gWorldData->section30 + v30 * 72;
            }
        }

        if (level->flags2 != 0 && sCurLevelHasCameras < 0) {
            sCurLevelHasCameras = i;
        }

        if (level->flags2 != 0 && sMusicTrackHi != 12) {
            sprintf(nameBuf, strs + 776, level->name);
            ((WorldAudioView*)level->audioPtr)->soundHandle =
                AudioFindSound(nameBuf, 0, 1);
        } else {
            ((WorldAudioView*)level->audioPtr)->soundHandle = -1;
        }

        level->flags &= ~1u;

        if (level->resolved != 0) {
            continue;
        }
        level->resolved = 1;
        {
            /* per-level float tuning block, level_data 0xA8..0xDC
             * (difficulty..trap_damage) - stride/order confirmed by this
             * loop's exact offsets against every field in that span. */
            if (sent == lvl->difficulty) {
                lvl->difficulty = one;
            }
            d = lvl->difficulty;
            if (sent == lvl->ene_health) {
                lvl->ene_health = d;
            }
            if (sent == lvl->ene_speed) {
                lvl->ene_speed = d;
            }
            if (sent == lvl->ene_visrad) {
                lvl->ene_visrad = d;
            }
            if (sent == lvl->ene_attack) {
                lvl->ene_attack = d;
            }
            if (sent == lvl->ene_damage) {
                lvl->ene_damage = d;
            }
            if (sent == lvl->ene_mrate) {
                lvl->ene_mrate = d;
            }
            if (sent == lvl->ene_mspeed) {
                lvl->ene_mspeed = one;
            }
            if (sent == lvl->ene_macc) {
                lvl->ene_macc = d;
            }
            if (sent == lvl->gen_health) {
                lvl->gen_health = d;
            }
            if (sent == lvl->gen_rate) {
                lvl->gen_rate = d;
            }
            if (sent == lvl->gen_max) {
                lvl->gen_max = d;
            }
            if (sent == lvl->trap_rate) {
                lvl->trap_rate = d;
            }
            if (sent == lvl->trap_damage) {
                lvl->trap_damage = d;
            }
            {
                gp = (f32*)((u8*)lbl_8011C748 +
                            ((OptsView*)optionsAudioAndPrefs30)->vol * 4);
                gain = *gp;
                lvl->difficulty *= gain;
                lvl->ene_speed *= gain;
                lvl->ene_visrad *= gain;
                lvl->ene_attack *= gain;
                lvl->ene_mrate *= gain;
                lvl->ene_macc *= gain;
                lvl->gen_rate *= gain;
                lvl->gen_max *= gain;
                lvl->trap_rate *= gain;
                lvl->trap_damage *= gain;
            }
            lvl->ene_attack =
                one / lvl->ene_attack;
            lvl->ene_macc =
                one / lvl->ene_macc;
            lvl->ene_mrate =
                one / lvl->ene_mrate;
            lvl->trap_rate =
                one / lvl->trap_rate;
        }
    }

    for (i = 0; i < gWorldData->numSounds; i++) {
        WorldSoundView* snd = &((WorldSoundView*)gWorldData->sounds)[i];
        snd->soundHandle = AudioFindSound(snd->name, 0, 1);
    }
}

/* --------------------------------------------------------------------------
 * LoadWorldData()  0x8005A098
 *
 * Load every realm's world-data WAD into its table slot.  Iterates the 14
 * world/level entries in sWorldLevelTable; for each it builds "<name>.wad",
 * checks it exists on the "wdata" device, records the size, allocates the blob
 * (once), reads it in, and remembers the first realm id.  A missing file logs
 * "No world data file: %s" and clears the slot.  Finally the world-state
 * globals are reset and the world-registry hook (fn_80057F44) is primed. */
extern char lbl_80112A9C[];        /* "No world data file: %s\n" */

void LoadWorldData(void)
{
    u8* entry;
    u32 i;
    int size;
    s32* ids;
    u8* table;
    char* path;

    table = (u8*)sWorldLevelTable;
    path = lbl_80257680;

    for (i = 0; i < 14; i++) {
        entry = table + i * 44;
        ids = (s32*)(entry += 232);
        sprintf(path, "%s.wad", (u8*)ids + 4);
        if (FileExists("wdata", path)) {
            size = FileSize("wdata", path);
            ids[4] = 1;
            if (sFirstWorldId < 0) {
                sFirstWorldId = ids[0] << 8;
            }
            entry = table + i * 4;
            if (*(void**)(entry += 848) == 0) {
                *(void**)entry = AllocMem(size);
            }
            MLMReadFile("wdata", path, size, *(void**)entry);
        } else {
            ErrorPrintf(lbl_80112A9C, path);
            entry = table + i * 4;
            *(void**)(entry += 848) = 0;
        }
    }

    sCurWorldType  = -1;
    sCurWorldIndex = -1;
    sMusicTrackHi  = -1;
    sMusicTrackLo  = -1;
    sCurLevelHasCameras = -1;
    gWorldData = 0;
    gCurLevel  = 0;
    ids = (s32*)(gGameOptions + 36);
    *ids = fn_80057F44(*ids, 1);
    sWorldDataConst = 0xD00;
}

extern void MBTreeSetFlags(void* node, s32 flags, s32 value);

extern void  MBTreeClearFlags(void* node, s32 flags, s32 value);
extern void  MBTreeSetAlpha(void* node, s32 alpha, s32 value);
extern f32   Random(f32 scale);

extern char* strcpy(char* d, const char* s);

void FatalError(const char* msg, int code);

s16* FindWobjWanim(void* wobj);

extern void MBTreeSetAlpha(void* node, s32 alpha, s32 mode);
extern void MBTreeClearFlags(void* node, s32 flags, s32 value);

