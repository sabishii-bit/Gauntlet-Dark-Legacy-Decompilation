#include "types.h"
#include "game/camera.h"
#include "game/dyngrid.h"
#include "game/effect.h"
#include "game/enemy.h"
#include "game/worldinfo.h"
#include "game/item.h"
#include "game/leveldata.h"
#include "game/mbobject.h"
#include "game/player.h"
#include "game/worldobj.h"

#ifndef offsetof
#define offsetof(type, memb) ((u32) & ((type*)0)->memb)
#endif

/* ==========================================================================
 * game/world/gauntworld.c  (NonMatching documentation slice)
 * .text 0x80058078 - 0x800631AC  (45 functions)
 *
 * This region is the GameCube "world runtime" block that begins with the
 * world-data (WAD) loader/resolver.  The first three functions are genuinely
 * the gauntworld world-data core and are reconstructed below with their real
 * Xbox-PDB symbol names, real callees (already named in symbols.txt) and the
 * discovered on-disk blob layout:
 *
 *   ResolveWorldData         0x80058078  size 0x1C3C  global  (9 callers)
 *   ResolveWorldDataPointers 0x80059CB4  size 0x03E4  local   (GCN-only BE fixup)
 *   LoadWorldData            0x8005A098  size 0x0154  global  (3 callers)
 *
 * NAMING NOTE (why the other 42 are still fn_):
 *   The Xbox PDB module gauntworld.obj lists 34 functions in source order
 *   (OldWaveToNew, ResolveWorldData, LoadWorldData, SetWorldNextLevel,
 *    NextWorldLevel, ... world_update, GauntWorldInit).  The GCN block here is
 *   a *linker-composite*: it does NOT follow that source order.  Proof:
 *   a gauntworld.obj function (GetEnemyTypes) is already mapped at 0x8005773C,
 *   i.e. BEFORE ResolveWorldData (0x80058078) - impossible within one obj if
 *   the block were pure gauntworld.  The block instead interleaves gauntworld
 *   with world-object / enemy-grid / item-spawn / debug-draw code from
 *   neighbouring TUs.  Because of that reordering, source-order mapping onto
 *   the Xbox names would produce WRONG names, and the strings referenced by
 *   these functions are error messages / item names / type tags - not symbol
 *   names - so behavioural evidence cannot pin a *specific* Xbox symbol either.
 *   Rather than pollute the authoritative symbol map with guesses (which the
 *   matching workflow explicitly warns against), the 42 helpers are left as
 *   fn_XXXXXXXX and fully catalogued in the INVENTORY table at the bottom of
 *   this file (address, size, real callees, referenced strings, role) so a
 *   later pass with Xbox call/xref data can name them safely.
 *
 * No include/game/worldinfo.h or worldobj.h exists in this tree (there is no
 * game/ include convention yet, and the Xbox struct dump's smworld_t is the
 * unrelated static-model geometry struct), so the world-data blob layout is
 * documented inline as WorldData / WorldLevel below.
 * ========================================================================== */

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
extern void* MBGetFromWad2();
extern int   AudioFindSound(const char* name, int a, int b);
extern int   FileExists(const char* dev, const char* path);
extern int   FileSize(const char* dev, const char* path);
extern void* AllocMem(int size);
extern void* AllocFile();
extern void  MLMReadFile(const char* dev, const char* path, int size, void* dst);
extern int   FatalErrorf(const char* fmt, ...);  /* ErrorPrintf-style logger  */
extern int   fn_80057F44();                      /* world-registry hook       */
extern int   sprintf(char* buf, const char* fmt, ...);
extern int   ErrorPrintf(const char* fmt, ...);
extern int   strcmp(const char* lhs, const char* rhs);
extern s32   MBOX_LoadModel(const char* name);
extern s32   MBOX_AllocModel(const char* name);
extern void  InitTexMods(void* data, s32 model);
extern void  fn_8001267C(void* data, s32 model, s32 index);
extern void  CopyMat4(f32* src, f32* dst);
extern int   GetWorldMat(void* node, f32* matrix, f32* offset);
extern void  UnparentMatrix(f32* matrix, void* node);
extern void  WorldVector(f32* src, f32* dst, f32* matrix);
extern int   msgPost(s32 code, s32 owner, char* data);
extern void  DoTexMods(void* data);
extern void  DoSpecialTexmods(void);
extern void  SetupPlayerTexMods(s32 player);
extern void  fn_800606FC(void);
extern s32   fn_8005D0C4(s32 id, f32* position);
extern f32   NormalVector(f32* vector);
extern f32   sNoDistance;
extern f32   sCameraVisibilityRadius;
extern u8    lbl_80237BA0[];
extern f32   gIdentityMatrix[16];
extern s32   sNumItems;
extern s32   lbl_8034481C;
extern s32   gGameBusy;
extern s32   gGameplayPauseTimer;
extern s32   gGameMode;
extern s64   gControllerButtons;
extern Player gPlayers[4];
extern char  lbl_80346D08[5];
extern char  lbl_80346D10[7];
extern s32   gDemoMode;
extern char  sObjectsFile[];
extern void  AtreeDelete(void* atree);
extern void  MBRemoveNode(void* node, s32 mode);
extern void  MBTreeSetFlags(void* node, s32 flags, s32 value);
extern s32   MBOX_NewObject(const char* name, void* matrix, s32 parent, u32 flags);
extern WorldObj* FindWORLDOBJ(char* name);
extern s32   towerGetLevelFlag(u8* record, s32 level);
extern s32   WorldOpen(s32 world);
extern s32   PlayerHasShard(s32 player, s32 shard);

/* ---- module data (real names from symbols.txt) --------------------------- */
extern s32   sWorldLevelTable[]; /* 0x8011C3C0 base; adjacent world tables    */
extern s32   sWorldDataTypes[];  /* 0x8011C4A8 per-realm world-data type ids  */
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
extern f64  lbl_80346C70;              /* -1.0 sentinel                     */
extern f32  lbl_80346BE0;              /* 1.0                                */
extern char lbl_80112788[];            /* string block (+716/+748/+776)     */

static void ResolveWorldDataPointers(void)
{
    char* strs = lbl_80112788;
    u8* lvl;
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
        lvl = (u8*)&gWorldData->levels[i];
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
            if (sent == *(f32*)(lvl + offsetof(level_data, difficulty))) {
                *(f32*)(lvl + offsetof(level_data, difficulty)) = one;
            }
            d = *(f32*)(lvl + offsetof(level_data, difficulty));
            if (sent == *(f32*)(lvl + offsetof(level_data, ene_health))) {
                *(f32*)(lvl + offsetof(level_data, ene_health)) = d;
            }
            if (sent == *(f32*)(lvl + offsetof(level_data, ene_speed))) {
                *(f32*)(lvl + offsetof(level_data, ene_speed)) = d;
            }
            if (sent == *(f32*)(lvl + offsetof(level_data, ene_visrad))) {
                *(f32*)(lvl + offsetof(level_data, ene_visrad)) = d;
            }
            if (sent == *(f32*)(lvl + offsetof(level_data, ene_attack))) {
                *(f32*)(lvl + offsetof(level_data, ene_attack)) = d;
            }
            if (sent == *(f32*)(lvl + offsetof(level_data, ene_damage))) {
                *(f32*)(lvl + offsetof(level_data, ene_damage)) = d;
            }
            if (sent == *(f32*)(lvl + offsetof(level_data, ene_mrate))) {
                *(f32*)(lvl + offsetof(level_data, ene_mrate)) = d;
            }
            if (sent == *(f32*)(lvl + offsetof(level_data, ene_mspeed))) {
                *(f32*)(lvl + offsetof(level_data, ene_mspeed)) = one;
            }
            if (sent == *(f32*)(lvl + offsetof(level_data, ene_macc))) {
                *(f32*)(lvl + offsetof(level_data, ene_macc)) = d;
            }
            if (sent == *(f32*)(lvl + offsetof(level_data, gen_health))) {
                *(f32*)(lvl + offsetof(level_data, gen_health)) = d;
            }
            if (sent == *(f32*)(lvl + offsetof(level_data, gen_rate))) {
                *(f32*)(lvl + offsetof(level_data, gen_rate)) = d;
            }
            if (sent == *(f32*)(lvl + offsetof(level_data, gen_max))) {
                *(f32*)(lvl + offsetof(level_data, gen_max)) = d;
            }
            if (sent == *(f32*)(lvl + offsetof(level_data, trap_rate))) {
                *(f32*)(lvl + offsetof(level_data, trap_rate)) = d;
            }
            if (sent == *(f32*)(lvl + offsetof(level_data, trap_damage))) {
                *(f32*)(lvl + offsetof(level_data, trap_damage)) = d;
            }
            {
                gp = (f32*)((u8*)lbl_8011C748 +
                            ((OptsView*)optionsAudioAndPrefs30)->vol * 4);
                gain = *gp;
                *(f32*)(lvl + offsetof(level_data, difficulty)) *= gain;
                *(f32*)(lvl + offsetof(level_data, ene_speed)) *= gain;
                *(f32*)(lvl + offsetof(level_data, ene_visrad)) *= gain;
                *(f32*)(lvl + offsetof(level_data, ene_attack)) *= gain;
                *(f32*)(lvl + offsetof(level_data, ene_mrate)) *= gain;
                *(f32*)(lvl + offsetof(level_data, ene_macc)) *= gain;
                *(f32*)(lvl + offsetof(level_data, gen_rate)) *= gain;
                *(f32*)(lvl + offsetof(level_data, gen_max)) *= gain;
                *(f32*)(lvl + offsetof(level_data, trap_rate)) *= gain;
                *(f32*)(lvl + offsetof(level_data, trap_damage)) *= gain;
            }
            *(f32*)(lvl + offsetof(level_data, ene_attack)) =
                one / *(f32*)(lvl + offsetof(level_data, ene_attack));
            *(f32*)(lvl + offsetof(level_data, ene_macc)) =
                one / *(f32*)(lvl + offsetof(level_data, ene_macc));
            *(f32*)(lvl + offsetof(level_data, ene_mrate)) =
                one / *(f32*)(lvl + offsetof(level_data, ene_mrate));
            *(f32*)(lvl + offsetof(level_data, trap_rate)) =
                one / *(f32*)(lvl + offsetof(level_data, trap_rate));
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
extern char lbl_80257680[];        /* path scratch buffer */
extern char lbl_80112A9C[];        /* "No world data file: %s\n" */
extern u8 gGameOptions[];

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

/* --------------------------------------------------------------------------
 * Model + animation loading boundary.
 *
 * These two helpers are the point where the world runtime acquires an MB
 * model and its optional "anim" sidecar.  Keeping the sidecar pointer visible
 * to callers lets the item subsystem initialize texture modifiers after the
 * model handle has been established.
 */

s32 fn_8005A1EC(const char* name, void** outData)
{
    s32 model = MBOX_AllocModel(name);

    if (outData != 0) {
        if (FileExists(name, lbl_80346D08)) {
            *outData = AllocMem(FileSize(name, lbl_80346D08));
        } else {
            *outData = 0;
        }
    }

    return model;
}

s32 LoadModel(const char* name, void** outData, s32 initTexMods, s32 model)
{
    s32 result;

    if (model < 0) {
        result = MBOX_LoadModel(name);
    } else {
        result = model;
    }

    if (outData != 0) {
        if (FileExists(name, lbl_80346D08)) {
            if (strcmp(name, lbl_80346D10) != 0) {
                *outData = AllocFile(name, lbl_80346D08);
            } else {
                *outData = lbl_80237BA0;
            }
            fn_8001267C(*outData, result, -1);
        } else {
            *outData = 0;
        }

        if (*outData != 0 && initTexMods != 0) {
            InitTexMods(*outData, result);
        }
    }

    return result;
}

/* --------------------------------------------------------------------------
 * Runtime object transforms.
 *
 * These helpers operate on the shared OBJGRP layout also used by Item.  Keeping
 * them here establishes the world-runtime -> MB scene-graph boundary without
 * duplicating the layout as anonymous byte offsets.
 */

void fn_8005A588(OBJGRP* group, f32* offset);
void fn_8005A65C(OBJGRP* group, f32* offset);

#pragma dont_inline on
void fn_8005A338(OBJGRP* group, f32* collOffset, f32* attnOffset)
{
    if (group != 0 && group->node != 0) {
        GetWorldMat(group->node, &group->worldmat[0][0], 0);
        fn_8005A65C(group, collOffset);
        fn_8005A588(group, attnOffset);
    } else if (group != 0) {
        CopyMat4(gIdentityMatrix, &group->worldmat[0][0]);
    }
}
#pragma dont_inline off

#pragma dont_inline on
void UpdateObjWorldMat(OBJGRP* group)
{
    if (group != 0 && group->node != 0) {
        CopyMat4(&group->worldmat[0][0], (f32*)group->node);
        UnparentMatrix((f32*)group->node,
                       ((MBObject*)group->node)->parent);
    }
}
#pragma dont_inline off

void fn_8005A404(OBJGRP* group, f32* collOffset, f32* attnOffset)
{
    f32 coll[3];
    f32 attn[3];

    if (collOffset == 0 || (group->flags & 1) != 0) {
        group->coll_pos[0] = group->worldmat[3][0];
        group->coll_pos[1] = group->worldmat[3][1];
        group->coll_pos[2] = group->worldmat[3][2];
    } else if ((group->flags & 2) != 0) {
        group->coll_pos[0] = group->worldmat[3][0] + collOffset[0];
        group->coll_pos[1] = group->worldmat[3][1] + collOffset[1];
        group->coll_pos[2] = group->worldmat[3][2] + collOffset[2];
    } else {
        WorldVector(collOffset, coll, &group->worldmat[0][0]);
        group->coll_pos[0] = group->worldmat[3][0] + coll[0];
        group->coll_pos[1] = group->worldmat[3][1] + coll[1];
        group->coll_pos[2] = group->worldmat[3][2] + coll[2];
    }

    if (attnOffset == 0 || (group->flags & 1) != 0) {
        group->attn_pos[0] = group->worldmat[3][0];
        group->attn_pos[1] = group->worldmat[3][1];
        group->attn_pos[2] = group->worldmat[3][2];
    } else if ((group->flags & 2) != 0) {
        group->attn_pos[0] = group->worldmat[3][0] + attnOffset[0];
        group->attn_pos[1] = group->worldmat[3][1] + attnOffset[1];
        group->attn_pos[2] = group->worldmat[3][2] + attnOffset[2];
    } else {
        WorldVector(attnOffset, attn, &group->worldmat[0][0]);
        group->attn_pos[0] = group->worldmat[3][0] + attn[0];
        group->attn_pos[1] = group->worldmat[3][1] + attn[1];
        group->attn_pos[2] = group->worldmat[3][2] + attn[2];
    }
}

void fn_8005A588(OBJGRP* group, f32* offset)
{
    f32 transformed[3];

    if (offset == 0 || (group->flags & 1) != 0) {
        group->attn_pos[0] = group->worldmat[3][0];
        group->attn_pos[1] = group->worldmat[3][1];
        group->attn_pos[2] = group->worldmat[3][2];
    } else if ((group->flags & 2) != 0) {
        group->attn_pos[0] = group->worldmat[3][0] + offset[0];
        group->attn_pos[1] = group->worldmat[3][1] + offset[1];
        group->attn_pos[2] = group->worldmat[3][2] + offset[2];
    } else {
        WorldVector(offset, transformed, &group->worldmat[0][0]);
        group->attn_pos[0] = group->worldmat[3][0] + transformed[0];
        group->attn_pos[1] = group->worldmat[3][1] + transformed[1];
        group->attn_pos[2] = group->worldmat[3][2] + transformed[2];
    }
}

void fn_8005A65C(OBJGRP* group, f32* offset)
{
    f32 transformed[3];

    if (offset == 0 || (group->flags & 1) != 0) {
        group->coll_pos[0] = group->worldmat[3][0];
        group->coll_pos[1] = group->worldmat[3][1];
        group->coll_pos[2] = group->worldmat[3][2];
    } else if ((group->flags & 2) != 0) {
        group->coll_pos[0] = group->worldmat[3][0] + offset[0];
        group->coll_pos[1] = group->worldmat[3][1] + offset[1];
        group->coll_pos[2] = group->worldmat[3][2] + offset[2];
    } else {
        WorldVector(offset, transformed, &group->worldmat[0][0]);
        group->coll_pos[0] = group->worldmat[3][0] + transformed[0];
        group->coll_pos[1] = group->worldmat[3][1] + transformed[1];
        group->coll_pos[2] = group->worldmat[3][2] + transformed[2];
    }
}

int fn_8005A730(void)
{
    return 1;
}

void fn_8005AC10(s32 player)
{
    Player* record = &gPlayers[player];
    s32 i;

    record->world_text_active = 0;
    record->world_name_len = 0;
    for (i = 0; i < 7; i++) {
        if (record->name[i] != 0) {
            record->world_name_len++;
        }
    }

    if (record->world_name_len >= 5) {
        record->world_name_len = 5;
        record->world_name_tail = (s8)record->name[record->world_name_len];
        record->name[record->world_name_len] = 0;
    } else {
        record->world_name_tail = 0x40;
    }

    if (gGameMode == 0x400B && (gControllerButtons & 4) != 0) {
        record->state = 1;
    }
}

/* gWorldInfo: game/worldinfo.h (WorldInfo, 0x8028CA8C, size 0xA4) */
extern f64 __fabs(f64 value);
extern u8 sItemRuntime[];
extern char* sArrowObjectNames[];
extern void get_screen_pos(s32 camera, s32* x, s32* y, void* position);
extern void DrawText(s32 x, s32 y, s32 font, u32 color,
                     const char* format, ...);

void fn_8005AF98(u8* record, s32* typeOut, s32* valueOut, s32* fieldOut,
                 s32* stateOut, char** nameOut);
f32 fn_8005B198(f32 radius, f32* position, Item** result);

static inline u8* world_record_at(u8** records, s16 index)
{
    return *records + index * 80;
}

void fn_8005ACE0(f32* position)
{
    char** names = sArrowObjectNames;
    char* runtime = (char*)sItemRuntime;
    Item* item;
    s32 type;
    s32 value;
    s32 field;
    s32 state;
    s32 x;
    s32 y;
    char* name;
    s32 screenX;
    s32 screenY;
    s32 displayType;

    fn_8005B198(20.0f, position, &item);
    if (item == 0) {
        return;
    }

    fn_8005AF98((u8*)item->info, &type, &value, &field, &state, &name);
    get_screen_pos(0, &x, &y, &item->objgrp.worldmat[3][0]);

    if (type == 2 && *(s16*)&item->data[0] >= 0) {
        fn_8005AF98((u8*)gWorldInfo.iteminfo +
                        *(s16*)&item->data[0] * 80,
                    &type, &value, &field, &state, &name);
    }

    screenX = x;
    if (screenX <= 32 || screenX >= 480 ||
        ((screenY = y), screenY < 0) || screenY >= 384) {
        return;
    }

    displayType = type;
    switch (displayType) {
    case 1:
        if ((u32)value >= 17) {
            return;
        }
        DrawText(-screenX, screenY, 0, 0xFFFFFF, "%s:%s:%d(%d)",
                 names[displayType + 43], names[value + 57], field,
                 item->minplayers);
        break;

    case 5:
    case 12:
        if (*(u32*)&item->data[0] == 0) {
            return;
        }
        if (displayType == 12) {
            sprintf(runtime + 3000, "ROT:");
        } else if (value == 20 || value == 22) {
            sprintf(runtime + 3000, "BRID:");
        } else if (value == 21 || value == 23) {
            sprintf(runtime + 3000, "DOOR:");
        } else if ((u32)(value - 25) <= 1) {
            sprintf(runtime + 3000, "ELEV:");
        } else if (value >= 27 && value <= 29) {
            sprintf(runtime + 3000, "LIFT:");
        } else {
            sprintf(runtime + 3000, "TRIG:");
        }
        DrawText(-x, y, 0, 0xFFFFFF, "%s:%s(%d)",
                 runtime + 3000,
                 *(char**)&item->data[0], item->minplayers);
        break;

    default:
        DrawText(-screenX, screenY, 0, 0xFFFFFF, "%s(%d)",
                 names[displayType + 43], item->minplayers);
        break;
    }
}

void fn_8005AF98(u8* record, s32* typeOut, s32* valueOut, s32* fieldOut,
                 s32* stateOut, char** nameOut)
{
    typedef struct WorldRecordView {
        s32 type;
        s32 valueOrCount;
        s16 links[16];
        char name[20];
        s32 state;
        s16 field;
    } WorldRecordView;
    WorldRecordView* view = (WorldRecordView*)record;
    u8 unusedHigh[8];
    s32 type;
    s32 value;
    s32 field;
    s32 state;
    s32 nextType;
    s32 nextValue;
    s32 nextField;
    s32 nextState;
    char* name;
    char* nextName;
    u8** worldRecords;
    s32 count;
    s32 i;
    u8 unusedLow[8];

    if (view->type == -1) {
        worldRecords = (u8**)&gWorldInfo.iteminfo;
        count = view->valueOrCount;
        fn_8005AF98(world_record_at(worldRecords, view->links[0]),
                    &type, &value, &field, &state, &name);
        for (i = 1; i < count; i++) {
            fn_8005AF98(*worldRecords +
                            view->links[i] * 80,
                        &nextType, &nextValue, &nextField, &nextState,
                        &nextName);
            if (nextType != type) {
                type = 0;
            }
            if (nextValue != value) {
                value = 0;
            }
            if (nextField != field) {
                field = 0;
            }
            if (nextState != state) {
                state = 0;
            }
            if (name != NULL && nextName != NULL) {
                if (strcmp(name, nextName) != 0) {
                    name = NULL;
                }
            } else {
                nextName = NULL;
                name = NULL;
            }
        }
        *typeOut = type;
        *valueOut = value;
        *fieldOut = field;
        *stateOut = state;
        *nameOut = name;
    } else {
        *typeOut = view->type;
        *valueOut = view->valueOrCount;
        *fieldOut = view->field;
        if (view->type != 1 || view->valueOrCount != 3) {
            *fieldOut = (s32)__fabs((f64)*fieldOut);
        }
        *stateOut = view->state;
        *nameOut = view->name;
    }
}

f32 fn_8005B198(f32 radius, f32* position, Item** result)
{
    f32 delta[3];
    f32 best_distance = -1.0f;
    Item* item;
    Item* best_item = 0;
    s32 index;

    StartEnemyGrid(position, radius);
    while ((index = NextGridEnemy()) >= 0) {
        item = &sItems[index];

        if (item->active != -1 && item->minoff == 0) {
            f32 distance;

            delta[0] = item->objgrp.coll_pos[0] - position[0];
            delta[1] = item->objgrp.coll_pos[1] - position[1];
            delta[2] = item->objgrp.coll_pos[2] - position[2];
            distance = NormalVector(delta);
            if (best_distance < 0.0f || distance < best_distance) {
                best_item = item;
                best_distance = distance;
            }
        }
    }

    *result = best_item;
    return best_distance;
}

extern f64 lbl_80346EE8;
extern f64 lbl_80346EF0;
extern f64 lbl_80346EF8;
extern f64 lbl_80346F00;
extern f64 lbl_80346F08;
extern f32 fqdist(f32 x, f32 y);

/* Find the closest eligible world item in front of a directed probe. */
f32 fn_8005B274(f32* position, f32 bias, f32 radius, f32* direction,
                f32* resultPosition, Item** resultItem)
{
    f32 delta[3];
    u8 unused[4];
    f32 dot;
    f32 weighted;
    f32 absY;
    f64 maxInset;
    f64 normalScale;
    f64 subtypeScale;
    f64 heightScale;
    f32 bestX;
    f32 bestY;
    f32 bestZ;
    f32 distance;
    f32 best;
    f32 scale;
    iteminfodata* sub;
    s32 index;
    s32 bestIndex;
    s32 type;
    s16 active;
    Item* item;
    iteminfo* info;

    scale = (f32)((lbl_80346EE8 - bias) / radius);
    best = radius;
    bestIndex = -1;
    StartEnemyGrid(position, radius);
    subtypeScale = lbl_80346EF8;
    normalScale = lbl_80346F00;
    maxInset = lbl_80346F08;
    heightScale = lbl_80346EF0;

    while ((index = NextGridEnemy()) >= 0) {
        item = &sItems[index];
        active = item->active;
        if (active == -1 || (active & 0x8100) != 0 ||
            item->armor == -1) {
            continue;
        }
        info = item->info;
        type = info->type;
        if (type == -1) {
            continue;
        }
        if (item->minoff != 0) {
            continue;
        }
        sub = &info->item;
        if ((active & 0x4000) == 0) {
            continue;
        }
        switch (type) {
        case 2:
        case 10:
            if (type == 2 && sub->subtype != 43 && sub->subtype != 44 &&
                sub->subtype != 45) {
                continue;
            }
            if (type == 10 && sub->subtype == 41) {
                continue;
            }
            if ((sub->subtype == 43 || sub->subtype == 44 ||
                 sub->subtype == 45) && item->action > 0) {
                continue;
            }
            goto check_type5;
        case 5:
check_type5:
            if (type == 5 && sub->subtype != 31) {
                continue;
            }
            break;
        case 3:
            break;
        default:
            continue;
        }

        delta[0] = item->objgrp.coll_pos[0] - position[0];
        delta[1] = item->objgrp.coll_pos[1] - position[1];
        delta[2] = item->objgrp.coll_pos[2] - position[2];
        absY = delta[1];
        *(u32*)&absY &= 0x7FFFFFFF;
        if (absY > heightScale * sub->height) {
            continue;
        }
        distance = NormalVector(delta);
        if (item->info->type != 3) {
            if (sub->subtype == 44) {
                distance *= subtypeScale;
            } else if (sub->subtype == 45) {
                distance *= subtypeScale;
            } else {
                distance *= normalScale;
            }
        }
        distance -=
            (((f64)sub->radius < maxInset) ? (f64)sub->radius : maxInset);
        if (distance > radius) {
            continue;
        }
        {
            f32 dotPart;
            f32 weightedBase;
            f32 q;

            q = fqdist(delta[0], delta[2]);
            weightedBase = distance * scale + bias;
            dotPart = delta[2] * direction[2];
            weighted = q * weightedBase;
            dot = delta[0] * direction[0] + dotPart;
            if (dot < weighted) {
                continue;
            }
        }
        if (distance < best) {
            best = distance;
            bestIndex = index;
            bestX = delta[0];
            bestY = delta[1];
            bestZ = delta[2];
        }
    }

    if (bestIndex >= 0) {
        resultPosition[0] = bestX;
        resultPosition[1] = bestY;
        resultPosition[2] = bestZ;
        if (resultItem != 0) {
            *resultItem = &sItems[bestIndex];
        }
    } else if (resultItem != 0) {
        *resultItem = 0;
    }
    return best;
}

/* Item-pool queries used by the world dispatcher and camera/UI code. */
Item* fn_8005B558(s32 id)
{
    s32 i;
    s32 count = sNumItems;

    for (i = 0; i < count; i++) {
        Item* item = &sItems[i];
        if (item->active != -1 && item->info->type == 9 &&
            id == *(s16*)&item->data[0]) {
            return item;
        }
    }
    return 0;
}

void fn_8005B5B8(void)
{
    u32 level_flags[14];
    char name[32];
    f32 matrix[16];
    char* strings = sObjectsFile;
    s32 player;
    s32 level;
    Item* item;
    s32 gate;
    s32 item_index;
    s32 world;
    s32 enable;
    s32 id;

    for (level = 0; level < 14; level++) {
        level_flags[level] = 0;
    }

    for (player = 0; player < 4; player++) {
        Player* record = &gPlayers[player];

        if (record->state != 0) {
            for (level = 0; level < 14; level++) {
                level_flags[level] |= towerGetLevelFlag((u8*)record, level);
            }
        }
    }

    for (item_index = 0; item_index < sNumItems; item_index++) {
        item = &sItems[item_index];
        enable = 0;

        if (item->active != -1 && item->info->type == 9) {
            id = *(s16*)&item->data[0];
            world = id >> 8;
            gate = id & 0xFF;

            if (gDemoMode != 0) {
                enable = 1;
                switch (world) {
                case 7:
                    if (gate == 0) {
                        enable = 0;
                    }
                    break;
                case 11:
                    if (gate == 3) {
                        enable = 0;
                    }
                    break;
                case 10:
                    if (gate == 5) {
                        enable = 0;
                    }
                    break;
                }
            } else {
                switch (world) {
                default:
                    if (gate - 1 >= 0 &&
                        ((1 << (gate - 1)) & level_flags[world]) == 0) {
                        enable = 1;
                    }
                    break;
                case 5:
                case 6:
                    if (WorldOpen(world) == 0) {
                        enable = 1;
                    }
                    break;
                case 8:
                    if (WorldOpen(world) == 0) {
                        enable = 1;
                    } else if (gate == 3) {
                        if (PlayerHasShard(-1, 0x1FFF) == 0) {
                            enable = 1;
                        }
                    } else if (gate - 1 >= 0 &&
                               ((1 << (gate - 1)) & level_flags[world]) == 0) {
                        enable = 1;
                    }
                    break;
                }
            }

            if (enable != 0) {
                WorldObj* world_object;
                s32 parent = (s32)((MBObject*)item->objgrp.node)->parent;

                CopyMat4((f32*)item->objgrp.node, matrix);
                if (*(void**)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                    *(s32*)&item->atree[0] = 0;
                }
                if (item->objgrp.node != 0) {
                    MBRemoveNode(item->objgrp.node, 0);
                    item->objgrp.node = 0;
                }
                item->objgrp.node = (struct mbnode*)MBOX_NewObject(
                    &strings[0x100], matrix, parent, 0);
                item->active = -0x8000;
                sprintf(name, &strings[0x10C], world + 0x40, gate + 1);
                world_object = FindWORLDOBJ(name);
                if (world_object != 0) {
                    MBTreeSetFlags(world_object->nodeptr, 2, 0);
                } else {
                    ErrorPrintf(&strings[0x120], name);
                }
            }
        }
    }
}

s32 fn_8005B8B0(void* owner)
{
    Item* item = (Item*)((Player*)owner)->special_collision_item;
    s32 result = 0;

    if (item != 0 && item->info->type == 0xB) {
        Item* linked = *(Item**)&item->data[8];
        s16 active = linked->active;

        result = (s32)linked;
        if (active == -1 || linked->minoff != 0 || (active & 0x4000) == 0) {
            result = 0;
        }
    }
    return result;
}

s32 fn_8005B8FC(void* owner)
{
    s32 result = 0;
    Item* item;

    if (lbl_8034481C != 0) {
        result = -1;
    }

    item = (Item*)((Player*)owner)->special_collision_item;
    if (item != 0 && item->info->type == 9 &&
        item->info->item.subtype != 0x32) {
        result = *(s16*)&item->data[0];
    }

    if ((gGameMode & 0x8000) != 0 && result != 0) {
        msgPost(0x61, *(s32*)owner, (char*)owner + 0x54);
    }
    return result;
}

void fn_8005B988(void)
{
    s32 i;

    if ((gGameBusy | gGameplayPauseTimer) == 0) {
        fn_800606FC();
        if (sGoodWizObj != 0) {
            DoTexMods(sGoodWizObj);
        }
        if (sItemFile1Buf != 0) {
            DoTexMods(sItemFile1Buf);
        }
        if (sWeaponsBuf != 0) {
            DoTexMods(sWeaponsBuf);
        }
        if (sPowerupsBuf != 0) {
            DoTexMods(sPowerupsBuf);
        }
        for (i = 0; i < 4; i++) {
            SetupPlayerTexMods(i);
        }
        DoSpecialTexmods();
    }
}

void fn_8005D04C(void)
{
    s32 i;
    Item* item;

    for (i = 0; i < sNumItems; i++) {
        item = &sItems[i];

        if (item->info != 0 && item->info->type == 4) {
            *(s16*)&item->data[0x12] =
                fn_8005D0C4(*(s16*)&item->data[0],
                             &item->objgrp.worldmat[3][0]);
        }
    }
}

/* forward decls for later-defined callees */
extern f32 fqdist(f32 x, f32 y);
extern void MBTreeSetFlags(void* node, s32 flags, s32 value);
extern int fn_8005EE18(Item* item, s32 arg);
extern f32 fn_8005F0F4(Item* item, f32* from, f32* pos, f32* out,
                       f32 radius, f32 height);
extern void fn_8009D91C(f32* pos);

s32 fn_8005D0C4(s32 id, f32* position)
{
    f32 dy;
    f32 best = sCameraVisibilityRadius;
    s32 best_idx = -1;
    s32 idx;
    u8 unused[16];
    struct {
        u8 pad[8];
        f32 value;
    } absWork;

    if (id != 33 && id != 32 && id != 29) {
        return -1;
    }

    StartEnemyGrid(position, sNoDistance);
    while ((idx = NextGridEnemy()) >= 0) {
        Item* item = &sItems[idx];
        f32 d;

        if (item->active == -1) {
            continue;
        }
        if (item->active & 0x8100) {
            continue;
        }
        if (item->minoff != 0) {
            continue;
        }
        if (item->info->type != 1) {
            continue;
        }

        {
            f32 dx = item->objgrp.worldmat[3][0] - position[0];
            f32 dz = item->objgrp.worldmat[3][2] - position[2];
            dy = item->objgrp.worldmat[3][1] - position[1];
            d = fqdist(dx, dz);
        }
        if (d < best) {
            absWork.value = dy;
            *(u32*)&absWork.value &= 0x7FFFFFFF;
            if (absWork.value < 3.0f) {
                best = d;
                best_idx = idx;
            }
        }
    }

    if (best_idx >= 0) {
        sItems[best_idx].minoff = 10;
        MBTreeSetFlags(sItems[best_idx].objgrp.node, 2, 0);
    }
    return best_idx;
}

extern u32 sItemRandSeed;
extern void* AtreeInit(void* hdr, void* atree, s32 a, s32 flags);
extern void MBNodeSetParent(void* node, void* parent);
extern void* sChestAtree;
extern s32 sDeathItemInfo;
extern char sKeyringName[8];
extern Item* NewItemPtr_800642C8(void);
extern void AddItemSub(Item* item);
extern f32 sArrowFloorRadius;
extern u8 gWadAtreeHeaders[];
extern u8 sEnemyDefaultAlgorithm[];
extern void fn_8009DAF8(void);

/* 0x8005E90C - resolve an item's world-record binding: reroll random slots,
 * spawn the linked pickup/chest contents, seed timers and child algorithm. */
void fn_8005E90C(Item* item, s32* inst)
{
    iteminfo** tblp;
    iteminfo* info;
    iteminfo* row;
    Item* child;
    Item* result;
    s32 idx;
    s32 delta;
    s32 i;
    u32 fl;
    s32 t;
    u8 unused[16];

    idx = *(s16*)&item->data[0];
    info = item->info;
    if ((s16)idx >= 0) {
        tblp = (iteminfo**)&gWorldInfo.iteminfo;
        row = *tblp + idx;
    } else {
        return;
    }
    delta = (s32)((u8*)item - (u8*)sItems);
    while (row->type == -1) {
        s32 n = row->item.subtype;
        s32 r;
        t = delta / 240;
        if (n != 0) {
            r = ((sItemRandSeed >> 5) + t) % (u32)n;
        } else {
            r = 0;
        }
        sItemRandSeed = sItemRandSeed + 439;
        *(s16*)&item->data[0] = *(s16*)((u8*)row + r * 2 + 8);
        row = *tblp + *(s16*)&item->data[0];
    }
    if (info->item.subtype == 48 && row->type == 1 && row->item.subtype == 1) {
        fl = 0;
        if (sChestAtree != NULL) {
            if (*(u32*)&item->atree[0] != 0) {
                AtreeDelete(item->atree);
            }
            fl |= 0x800;
            fl |= info->item.mbflags;
            *(void**)&item->atree[0] = AtreeInit(sChestAtree, item->atree, 0, fl);
            MBNodeSetParent(*(void**)*(void**)&item->atree[0], item->objgrp.node);
        }
        item->info = (iteminfo*)sDeathItemInfo;
        *(s32*)&item->data[4] = row->item.value;
        return;
    }
    if (info->item.subtype == 47) {
        *(s32*)&item->data[4] = row->item.value;
        return;
    }
    if (info->item.subtype == 44) {
        item->active |= 64;
        fn_8009DAF8();
        return;
    }
    if (row->type == 1) {
        switch (row->item.subtype) {
        case 2:
            if (*(s16*)&item->data[16] > 1) {
                iteminfo* q = *tblp;
                u8* world = (u8*)&gWorldInfo;
                for (i = 0; i < *(s32*)(world + 116); i++, q++) {
                    iteminfodata* qdata = &q->item;
                    if (strcmp(sKeyringName, qdata->desc) != 0) {
                        continue;
                    }
                    if (q->type != 1) {
                        continue;
                    }
                    if (qdata->subtype != 2) {
                        continue;
                    }
                    goto keyring_found;
                }
                i = -1;
keyring_found:
                row = *tblp + i;
            }
            break;
        }
    }
    if (row->type == 1 && *(u32*)&item->data[8] != 0) {
        f32 radius;

        child = NewItemPtr_800642C8();
        SetItem(child, NULL, row, gIdentityMatrix);
        result = child;
        MBNodeSetParent(child->objgrp.node, *(void**)&item->data[8]);
        *(Item**)&item->data[12] = child;
        *(Item**)&child->data[12] = item;
        child->opener = (s8)(inst != NULL ? *inst : -1);
        MBTreeSetFlags(result->objgrp.node, 8, 0);
        radius = sArrowFloorRadius;
        *(f32*)((u8*)child->objgrp.node + 64) = radius;
        *(f32*)((u8*)child->objgrp.node + 68) = radius;
        *(f32*)((u8*)child->objgrp.node + 72) = radius;
    } else {
        s32 subtype;

        child = NewItemPtr_800642C8();
        if (&item->objgrp != NULL) {
            SetItem(child, NULL, row, &item->objgrp.worldmat[0][0]);
            AddItemSub(child);
        } else {
            SetItem(child, NULL, row, gIdentityMatrix);
        }
        subtype = info->item.subtype;
        result = child;
        if (subtype == 43) {
            child->opener = (s8)(inst != NULL ? *inst : -2);
        } else {
            if (row->type == 1) {
                *(Item**)&child->data[12] = item;
            }
            child->opener = (s8)(inst != NULL ? *inst : -1);
        }
    }
    switch (row->type) {
    case 1:
        switch (row->item.subtype) {
        case 14:
            *(s32*)&result->data[4] = *(s16*)&item->data[16];
            break;
        case 2:
            *(s32*)&result->data[4] = *(s16*)&item->data[16];
            if (*(s32*)&result->data[4] < 1) {
                *(s32*)&result->data[4] = 1;
            }
            break;
        }
        *(s16*)&result->data[16] = 30;
        break;
    case 4:
        *(u32*)&result->data[8] |= 1;
        if (*(s16*)&result->data[0] == 30 && *(u32*)(gWadAtreeHeaders + 120) != 0) {
            MBTreeSetFlags(*(void**)*(void**)&result->atree[0], 2, 0);
            if (*(s16*)&item->data[16] != 0) {
                result->data[2] = 2;
                *(s8*)&result->data[3] = (s8)*(s32*)(sEnemyDefaultAlgorithm + 120);
            }
        }
        *(s16*)&result->data[18] = -1;
        break;
    }
}

Item* fn_8005ED44(f32 radius, s32 a2, f32* position, s32 a4, s32 a5, s32 a6)
{
    Item* item;
    f32 best_dist = 100000.0f;
    s32 idx;
    Item* best = 0;

    StartEnemyGrid(position, radius);
    while ((idx = NextGridEnemy()) >= 0) {
        item = &sItems[idx];
        if (fn_8005EE18(item, a6) != 0) {
            f32 d = fn_8005F0F4(item, (f32*)a2, position, (f32*)a4,
                                radius, radius);
            if (d >= 0.0 && d < best_dist) {
                best_dist = d;
                best = item;
                if (a5 == 0) {
                    break;
                }
            }
        }
    }
    return best;
}

int fn_8005EE18(Item* item, s32 arg)
{
    int result = 0;
    iteminfo* info = item->info;
    s32* sub = (s32*)info + 1;
    u8 _pad[8];

    switch (info->type) {
    case -1:
        break;
    case 1:
        if (item->activetime > 0) {
            result = 0;
        } else if (*(u32*)&item->data[0xC] != 0) {
            result = 0;
        } else {
            switch (*sub) {
            case 4:
                result = 1;
            }
        }
        break;
    case 2:
    case 7:
        result = 1;
        break;
    case 3: {
        s32 t;
        if (t = ((s8)item->data[6] ? 0 : 1)) {
            break;
        }
        result = 1;
        break;
    }
    case 4:
        result = 1;
        break;
    case 10:
        switch (*sub) {
        case 0x1E:
            break;
        case 0x29:
            if (*(s16*)&item->data[2] > 0) {
                result = 1;
            }
            break;
        case 0x34:
            if (arg >= 0 && (item->active & 1) == 0) {
                fn_8009D91C(&item->objgrp.worldmat[3][0]);
                item->active |= 1;
            }
            result = 0;
            break;
        case 0x28:
        case 0x31:
        case 0x33:
        case 0x35:
            result = 0;
            break;
        default:
            result = 1;
            break;
        }
        break;
    case 8:
        if (*sub == 5 &&
            (item->action == 1 || item->action == 2)) {
            item->daction = 3;
            result = 1;
        }
        break;
    case 5:
        if (*sub == 0x1F) {
            result = 1;
        }
        break;
    }
    return result;
}

Item* fn_8005EFAC(f32 radius, s32 a2, f32* position, s32 a4, s32 a5)
{
    Item* item;
    f32 best_dist = 100000.0f;
    f32 scaled = radius * 1.5;
    s32 idx;
    Item* best = 0;

    StartEnemyGrid(position, radius);
    while ((idx = NextGridEnemy()) >= 0) {
        s32 reject;
        item = &sItems[idx];
        reject = 0;
        switch (item->info->type) {
        case 13:
            reject = 1;
            break;
        case 1:
            if (*(u32*)&item->data[0xC] != 0) {
                reject = 1;
            }
            break;
        case 8:
            if ((item->action == 2 || item->action == 4) &&
                (item->active & 1)) {
            } else {
                reject = 1;
            }
            break;
        }
        if (reject != 0) {
            continue;
        }
        {
            f32 d = fn_8005F0F4(item, (f32*)a2, position, (f32*)a4,
                                radius, scaled);
            if (d >= 0.0 && d < best_dist) {
                best_dist = d;
                best = item;
                if (a5 == 0) {
                    break;
                }
            }
        }
    }
    return best;
}

Item* fn_80062FF0(f32 radius, f32* position, s32 type, f32* out1, f32* out2)
{
    u8 unused[40];
    Item* item;
    f32 min_flagged = 100000.0f;
    f32 min_all = 100000.0f;
    Item* best = 0;
    s32 idx;

    StartEnemyGrid(position, radius);
    while ((idx = NextGridEnemy()) >= 0) {
        iteminfo* info;
        s32 reject;
        s32 item_type;
        f32 d;

        item = &sItems[idx];
        if (item->active == -1) {
            continue;
        }
        if (item->active & 0x8100) {
            continue;
        }
        info = item->info;
        if (info->item.coltype == 0) {
            continue;
        }
        if (type != 0) {
            if (info->type != type) {
                continue;
            }
            if (type == 4 && (*(u32*)&item->data[8] & 1)) {
                continue;
            }
        }
        item_type = info->type;
        if (item_type == -1) {
            continue;
        }
        if (item->minoff != 0) {
            continue;
        }
        reject = 0;
        switch (item_type) {
        case 13:
            reject = 1;
            break;
        case 1:
            if (*(u32*)&item->data[0xC] != 0) {
                reject = 1;
            }
            break;
        case 8:
            if ((item->action == 2 || item->action == 4) &&
                (item->active & 1)) {
            } else {
                reject = 1;
            }
            break;
        }
        if (reject != 0) {
            continue;
        }
        d = fqdist(item->objgrp.coll_pos[0] - position[0],
                   item->objgrp.coll_pos[2] - position[2]);
        d = d - item->info->item.radius;
        if (d < min_all) {
            min_all = d;
        }
        if ((item->active & 0x40) || (item->active & 0x4000)) {
            if (d < min_flagged) {
                min_flagged = d;
                best = item;
            }
        }
    }

    if (out1 != 0) {
        *out1 = min_flagged;
    }
    if (out2 != 0) {
        *out2 = min_all;
    }
    return best;
}

s32 fn_800629B0(void)
{
    s32 result = 0;
    Item* item = sItems;
    s32 count = sNumItems;
    s32 i;

    for (i = 0; i < count; i++) {
        if (item->active != -1 && item->info->type == 1 &&
            item->info->item.subtype == 1) {
            result = 1;
        }
        item++;
    }
    return result;
}

/* --------------------------------------------------------------------------
 * Big world-object handlers (reconstructed from the DOL).
 *   fn_8005BA1C  0x07C0  gold-wizard reward pass over one item
 *   fn_8005C1DC  0x0E70  item/object spawn dispatcher
 *   fn_8005DE50  0x0ABC  big object state machine
 *   fn_8005F0F4  0x0A54  big per-object distance/worker
 *   fn_800606FC  0x22B4  per-frame world update dispatcher
 */

extern void* AtreeMatch(void* buf, const char* name, s32 mode);
extern void  AtreeDelete(void* atree);
extern void* AtreeInit(void* hdr, void* atree, s32 a, s32 flags);
extern void  MBNodeSetParent(void* node, void* parent);
extern s32   AnimateATree(void* atree, s32 action, s32 mode);
extern void  MBRemoveNode(void* node, s32 mode);
extern void* AtreeMatchAnyHeader(char* name, s32 mode);
extern void  fn_8009190C(OBJGRP* grp, s32 evt);
extern s32   gNextItemIdx;
/* gWorldInfo: game/worldinfo.h (WorldInfo, 0x8028CA8C, size 0xA4) */
extern const char lbl_80346F10[8];     /* "CHICKEN"  */
extern const char lbl_80346F18[6];     /* "APPLE"    */
extern char  lbl_80346F20[];     /* "TREAS_GOLD" (sdata2 copy)   */
extern char  lbl_80346F28[];     /* "TREAS_SILVER" (sdata2 copy) */
extern char  lbl_80346F34[];     /* "%s_D"     */
extern char  lbl_802583A8[];     /* scratch name buffer          */
extern char  sObjectsFile[];     /* +0x130 "TREAS_GOLD", +0x13C "TREAS_SILVER" */

f32 fn_8005C1DC(Item* item, f32 power, s32 flags, s32 owner);
extern Enemy gEnemies[25]; /* game/enemy.h; stride 0x394 */

/* 0x8005BA1C - apply a gold/silver-wizard reward to one item (swap the item's
 * atree to a treasure/food model, retarget generators, pop doors/walls). */
void fn_8005BA1C(Item* item, u8* player)
{
    char* objects = sObjectsFile;
    s32 evt = -1;                                 /* r25: fx event         */
    s32 msg = -1;                                 /* r24: message code     */
    iteminfo* info = item->info;
    s32* sub = (s32*)((u8*)info + 4);
    s32 rank = *(s32*)(player + 0x3324);          /* accumulated gold rank */
    s32 mode = *(u32*)(player + 8) & 3;
    void* hdr;
    s32 k;
    u8* world;
    u8** records;
    u8* rec;
    u8 unused[32];

    (void)unused;

    switch (info->type) {
    case 1:
        switch (*sub) {
        case 2:
            break;
        case 3:
            if (mode != 2) {
                break;
            }
            if (*(s32*)&item->data[4] <= -100) {
                if (rank < 0x32) {
                    break;
                }
                hdr = AtreeMatch(sPowerupsBuf, lbl_80346F10, 1);
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                }
                *(void**)&item->atree[0] = AtreeInit(hdr, item->atree, 0, 0x800);
                MBNodeSetParent(**(void***)&item->atree[0], item->objgrp.node);
                *(s32*)&item->data[4] = 100;
                evt = 0x2F;
                msg = 0x90;
            } else if (*(s32*)&item->data[4] < 0) {
                if (rank < 0x19) {
                    break;
                }
                hdr = AtreeMatch(sPowerupsBuf, lbl_80346F18, 1);
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                }
                *(void**)&item->atree[0] = AtreeInit(hdr, item->atree, 0, 0x800);
                MBNodeSetParent(**(void***)&item->atree[0], item->objgrp.node);
                *(s32*)&item->data[4] = 50;
                evt = 0x2F;
                msg = 0x8F;
            }
            break;
        case 1:
            if (mode != 0) {
                break;
            }
            if (*(s32*)&item->data[4] > 10) {
                break;
            }
            if (rank >= 0x32) {
                hdr = AtreeMatch(sPowerupsBuf, &objects[0x130], 1);
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                }
                *(void**)&item->atree[0] = AtreeInit(hdr, item->atree, 0, 0x800);
                MBNodeSetParent(**(void***)&item->atree[0], item->objgrp.node);
                *(s32*)&item->data[4] = 200;
                evt = 0x30;
                msg = 0x8C;
            } else if (rank >= 0x19) {
                hdr = AtreeMatch(sPowerupsBuf, &objects[0x13C], 1);
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                }
                *(void**)&item->atree[0] = AtreeInit(hdr, item->atree, 0, 0x800);
                MBNodeSetParent(**(void***)&item->atree[0], item->objgrp.node);
                *(s32*)&item->data[4] = 100;
                evt = 0x30;
                msg = 0x8B;
            }
            break;
        }
        break;

    case 2:
        if (*(s16*)&item->data[0] < 0) {
            break;
        }
        world = (u8*)&gWorldInfo;
        records = (u8**)(world + 0x68);
        rec = *records + *(s16*)&item->data[0] * 0x50;
        if (*(s32*)rec != 1) {
            break;
        }
        switch (*(s32*)(rec + 4)) {
        case 2:
            break;
        case 3:
            if (item->action > 0) {
                break;
            }
            if (mode != 2) {
                break;
            }
            if (*(s16*)(rec + 0x40) <= -100) {
                if (rank < 0x32) {
                    break;
                }
                rec = *records;
                for (k = 0; k < *(s32*)(world + 0x74); k++) {
                    if (strcmp(lbl_80346F10, (char*)(rec + 0x28)) == 0 &&
                        *(s32*)rec == 1 && *(s32*)(rec + 4) == 3) {
                        goto found_chicken;
                    }
                    rec += 0x50;
                }
                k = -1;
found_chicken:
                *(s16*)&item->data[0] = (s16)k;
                evt = 0x2F;
                msg = 0x90;
            } else if (*(s16*)(rec + 0x40) < 0) {
                if (rank < 0x19) {
                    break;
                }
                rec = *records;
                for (k = 0; k < *(s32*)(world + 0x74); k++) {
                    if (strcmp(lbl_80346F18, (char*)(rec + 0x28)) == 0 &&
                        *(s32*)rec == 1 && *(s32*)(rec + 4) == 3) {
                        goto found_apple;
                    }
                    rec += 0x50;
                }
                k = -1;
found_apple:
                *(s16*)&item->data[0] = (s16)k;
                evt = 0x2F;
                msg = 0x8F;
            }
            break;
        case 1:
            if (mode != 0) {
                break;
            }
            if (*(s16*)(rec + 0x40) > 10) {
                break;
            }
            if (rank >= 0x32) {
                if (*sub != 0x2B) {
                    hdr = AtreeMatch(sGoodWizObj, lbl_80346F20, 1);
                    if (*(u32*)&item->atree[0] != 0) {
                        AtreeDelete(item->atree);
                    }
                    *(void**)&item->atree[0] =
                        AtreeInit(hdr, item->atree, 0, 0x800);
                    MBNodeSetParent(**(void***)&item->atree[0],
                                    item->objgrp.node);
                }
                *(s16*)&item->data[0x10] = 200;
                if (item->action == 0) {
                    *(s16*)&item->data[0x10] = 200;
                    rec = *records;
                    for (k = 0; k < gWorldInfo.niteminfos; k++) {
                        if (strcmp(&objects[0x130],
                                   (char*)(rec + 0x28)) == 0 &&
                            *(s32*)rec == 1 && *(s32*)(rec + 4) == 1) {
                            goto found_gold;
                        }
                        rec += 0x50;
                    }
                    k = -1;
found_gold:
                    *(s16*)&item->data[0] = (s16)k;
                } else {
                    *(s32*)&item->data[4] = 200;
                }
                evt = 0x30;
                msg = 0x8C;
            } else if (rank >= 0x19) {
                if (*sub != 0x2B) {
                    hdr = AtreeMatch(sGoodWizObj, lbl_80346F28, 1);
                    if (*(u32*)&item->atree[0] != 0) {
                        AtreeDelete(item->atree);
                    }
                    *(void**)&item->atree[0] =
                        AtreeInit(hdr, item->atree, 0, 0x800);
                    MBNodeSetParent(**(void***)&item->atree[0],
                                    item->objgrp.node);
                }
                if (item->action == 0) {
                    *(s16*)&item->data[0x10] = 100;
                    rec = *records;
                    for (k = 0; k < gWorldInfo.niteminfos; k++) {
                        if (strcmp(&objects[0x13C],
                                   (char*)(rec + 0x28)) == 0 &&
                            *(s32*)rec == 1 && *(s32*)(rec + 4) == 1) {
                            goto found_silver;
                        }
                        rec += 0x50;
                    }
                    k = -1;
found_silver:
                    *(s16*)&item->data[0] = (s16)k;
                } else {
                    *(s32*)&item->data[4] = 100;
                }
                evt = 0x30;
                msg = 0x8B;
            }
            break;
        }
        break;

    case 10:
        if (*sub == 0x29 || *sub == 0x2B) {
            break;
        }
        if (mode != 3) {
            break;
        }
        if (rank >= 0x32) {
            fn_8005C1DC(item, 9999.0f, 0, *(s32*)player);
            msg = 0x92;
        } else {
            *(s16*)&item->data[4] = 4;
            msg = 0x91;
        }
        break;

    case 8:
        if (mode != 1) {
            break;
        }
        if (rank >= 0x32) {
            if (item->active == 0) {
                break;
            }
            sprintf(lbl_802583A8, lbl_80346F34, (char*)info + 0x28);
            hdr = AtreeMatchAnyHeader(lbl_802583A8, 0);
            if (hdr != 0) {
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                }
                *(void**)&item->atree[0] = AtreeInit(hdr, item->atree, 0, 0x800);
                MBNodeSetParent(**(void***)&item->atree[0], item->objgrp.node);
                item->action = 0;
                item->daction = 0;
                item->active = 0;
            } else {
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                    *(u32*)&item->atree[0] = 0;
                }
                if (item->objgrp.node != 0) {
                    MBRemoveNode(item->objgrp.node, 0);
                    item->objgrp.node = 0;
                }
                item->active = -1;
                k = ((u8*)item - (u8*)sItems) / 0xF0;
                if (k < gNextItemIdx) {
                    gNextItemIdx = k;
                }
            }
            msg = 0x8E;
            evt = 0x31;
        } else {
            if (item->activetime < 0x21C) {
                evt = 0x31;
            }
            item->action = 0;
            item->daction = 0;
            item->activetime = 0x258;
            AnimateATree(item->atree, item->daction, 3);
            msg = 0x8D;
        }
        break;
    }

    if (evt >= 0) {
        fn_8009190C(&item->objgrp, evt);
    }
    if (msg >= 0) {
        msgPost(msg, *(s32*)player, (char*)(player + 0x44));
    }
}

/* 0x8005F0F4 - collision/visibility probe of one item against the segment
 * from -> pos.  Returns the 2D distance to the item (negative = no hit); when
 * `out` is given it receives the pushed-out target position. */
extern void NormalVector2D(f32* v);
extern s32  towerAllPlayersMetBossReq(s32 level);
extern s32  lbl_80344768;
extern f64  sNewtonThree;
extern f64  sZeroDouble;
extern f32  sItemZero;
extern f64  lbl_80346EE8;
extern f64  lbl_80346EF0;
extern f32  lbl_80346F6C;
extern f32  fn_8005FDA8(u8* e, f32* a, f32* b, f32* outPos, f32* outNorm,
                        f32 margin);

/* float magnitude via sign-bit clear (inline fabs). */
static f32 wfabsf_(f32 x)
{
    *(u32*)&x &= 0x7FFFFFFF;
    return x;
}

f32 fn_8005F0F4(Item* item, f32* from, f32* pos, f32* out, f32 a, f32 b)
{
    iteminfo* info;
    iteminfodata* data;
    s32* sub;
    s32 coltype;
    s32 keep;
    s32 type;
    f32 R;
    f32 dist;
    f32 cx, cz;
    f32 nv[3];
    f32 mv[3];
    f32 hitpt[3];
    f32 norm[3];
    f32 f1, f2, f3, f4;
    f32 unused[12];

    (void)unused;

    if (item->active == -1) {
        return -1.0f;
    }
    if ((item->active & 0x8100) != 0) {
        return -1.0f;
    }
    info = item->info;
    if (info->type == -1) {
        return -1.0f;
    }
    if (item->minoff != 0) {
        return -1.0f;
    }
    data = &info->item;
    sub = &data->subtype;
    coltype = data->coltype;
    R = data->radius;
    if (coltype == 0) {
        return -1.0f;
    }
    if ((item->active & 0x40) == 0 && (item->active & 0x4000) == 0) {
        return -1.0f;
    }

    keep = 1;
    switch (info->type) {
    case 7:
        if (item->action >= 2 ||
            (item->action == 1 && item->activetime > 0x1E)) {
            keep = 0;
        }
        break;
    case 9:
        if (*sub != 0x32) {
            R = (f32)(R + ((f64)lbl_80344768 - lbl_80346EE8));
        }
        break;
    case 2:
        switch (*sub) {
        case 0x2B:
            if (item->action == 2) {
                keep = 0;
            }
            break;
        }
        break;
    case 3:
        if (item->armor < 0 && data->height <= sNewtonThree) {
            keep = 0;
        }
        break;
    case 4:
        if (*(f32*)&item->data[0xC] >= 0.0) {
            R = *(f32*)&item->data[0xC];
            coltype = 1;
        } else {
            R = lbl_80346F6C;
            coltype = 1;
        }
        break;
    case 10:
        switch (*sub) {
        case 0x2B:
        case 0x2C:
        case 0x2D:
            if (item->action > 0) {
                keep = 0;
            }
            break;
        }
        break;
    case 13:
        keep = 0;
        break;
    case 5:
        if ((item->active & 0x400) != 0) {
            keep = 0;
            break;
        }
        if (*(f32*)&item->data[0xC] > 0.0) {
            coltype = 1;
            R = *(f32*)&item->data[0xC];
        } else if (*sub == 0x1B) {
            coltype = 1;
            R = (f32)(R * lbl_80346EF0);
        }
        if ((*(s16*)&item->data[4] & 0x200) != 0) {
            keep = 0;
        } else if ((*(s16*)&item->data[4] & 0x40) != 0 &&
                   (s8)item->data[6] < 100 &&
                   towerAllPlayersMetBossReq((s8)item->data[6]) != 0) {
            R = (f32)(R * lbl_80346EF0);
        }
        break;
    }
    if (keep == 0) {
        return -1.0f;
    }

    R = (f32)(a + R);
    cx = item->objgrp.coll_pos[0];
    cz = item->objgrp.coll_pos[2];
    f1 = (f32)(cx - pos[0]);
    f2 = (f32)(cz - pos[2]);
    if (f1 * f1 + f2 * f2 > R * R) {
        return -1.0f;
    }

    nv[0] = (f32)(pos[0] - cx);
    nv[1] = pos[1] - item->objgrp.coll_pos[1];
    nv[2] = (f32)(pos[2] - cz);
    if (coltype != 2) {
        if (wfabsf_(nv[1]) > (f32)(data->height + b)) {
            return -1.0f;
        }
    }
    dist = fqdist(nv[0], nv[2]);
    if (dist > R) {
        return -1.0f;
    }

    switch (coltype) {
    case 1:
        break;
    case 2:
        if (fqdist(dist, nv[1]) > R) {
            keep = 0;
        }
        break;
    case 3:
        /* oriented box footprint */
        if (wfabsf_(nv[0] * item->objgrp.worldmat[0][0] +
                    nv[2] * item->objgrp.worldmat[0][2]) >
            (f32)(data->xdim + a)) {
            keep = 0;
        } else if (wfabsf_(nv[0] * item->objgrp.worldmat[2][0] +
                           nv[2] * item->objgrp.worldmat[2][2]) >
                   (f32)(data->zdim + a)) {
            keep = 0;
        }
        break;
    case 4:
        /* tri-list collision sweep */
        if (fn_8005FDA8((u8*)item, from, pos, hitpt, norm, a) < 0.0) {
            keep = 0;
        }
        break;
    default:
        keep = 0;
        break;
    }
    if (keep == 0) {
        return -1.0f;
    }

    dist -= R;
    if (dist < 0.0) {
        dist = 0.0f;
    }

    /* soft types accept immediately at the probe point */
    type = item->info->type;
    if (type != 10) {
        if (type < 10) {
            if (type == 5) {
                goto accept_at_pos;
            }
            if (type < 5) {
                goto los_check;
            }
            if (type >= 8) {
                goto accept_at_pos;
            }
        } else if (type < 0xC) {
accept_at_pos:
            if (out != 0) {
                out[0] = pos[0];
                out[1] = pos[1];
                out[2] = pos[2];
            }
            return dist;
        }
    }

los_check:
    /* line-of-sight check from the probe origin */
    keep = 0;
    nv[0] = (f32)(from[0] - cx);
    nv[1] = 0.0f;
    nv[2] = (f32)(from[2] - cz);
    switch (coltype) {
    case 3:
        f1 = nv[0] * item->objgrp.worldmat[0][0] +
             nv[2] * item->objgrp.worldmat[0][2];
        if (wfabsf_(f1) > (f32)(data->xdim + a)) {
            goto los_done;
        }
        f2 = nv[0] * item->objgrp.worldmat[2][0] +
             nv[2] * item->objgrp.worldmat[2][2];
        if (wfabsf_(f2) > (f32)(data->zdim + a)) {
            goto los_done;
        }
        nv[0] = (f32)(pos[0] - cx);
        nv[1] = 0.0f;
        nv[2] = (f32)(pos[2] - cz);
        f3 = nv[0] * item->objgrp.worldmat[0][0] +
             nv[2] * item->objgrp.worldmat[0][2];
        if (f1 < 0.0f) {
            if (f3 > f1) {
                goto los_done;
            }
        } else if (f1 > 0.0f && f3 < f1) {
            goto los_done;
        }
        f4 = nv[0] * item->objgrp.worldmat[2][0] +
             nv[2] * item->objgrp.worldmat[2][2];
        if (f2 < 0.0f) {
            if (f4 > f2) {
                goto los_done;
            }
        } else if (f2 > 0.0f && f4 < f2) {
            goto los_done;
        }
        keep = 1;
        break;
    case 1:
        keep = 1;
        break;
    case 4:
        break;
    default:
        if (fqdist(nv[0], nv[2]) > R) {
            break;
        }
        keep = 1;
        break;
    }

los_done:
    if (keep != 0) {
        nv[0] = pos[0] - from[0];
        nv[1] = 0.0f;
        nv[2] = pos[2] - from[2];
        mv[0] = (f32)(cx - from[0]);
        mv[1] = 0.0f;
        mv[2] = (f32)(cz - from[2]);
        NormalVector2D(nv);
        NormalVector2D(mv);
        if (nv[0] * mv[0] + nv[2] * mv[2] < 0.0f) {
            return -1.0f;
        }
    }

    if (out != 0) {
        switch (coltype) {
        case 3: {
            nv[0] = (f32)(pos[0] - cx);
            nv[1] = 0.0f;
            nv[2] = (f32)(pos[2] - cz);
            f3 = nv[0] * item->objgrp.worldmat[0][0] +
                 nv[2] * item->objgrp.worldmat[0][2];
            f4 = nv[0] * item->objgrp.worldmat[2][0] +
                 nv[2] * item->objgrp.worldmat[2][2];
            f2 = (f32)(data->xdim + a) - wfabsf_(f3);
            f1 = (f32)(data->zdim + a) - wfabsf_(f4);
            if (f2 > 0.0f || f1 > 0.0f) {
                if (f2 < f1 && f2 > 0.0f) {
                    if (f3 > 0.0f) {
                        out[0] = item->objgrp.worldmat[0][0] * f2 + pos[0];
                        out[1] = item->objgrp.worldmat[0][1] * f2 + pos[1];
                        out[2] = item->objgrp.worldmat[0][2] * f2 + pos[2];
                    } else {
                        f2 = -f2;
                        out[0] = item->objgrp.worldmat[0][0] * f2 + pos[0];
                        out[1] = item->objgrp.worldmat[0][1] * f2 + pos[1];
                        out[2] = item->objgrp.worldmat[0][2] * f2 + pos[2];
                    }
                } else if (f1 < f2 && f1 > 0.0f) {
                    if (f4 > 0.0f) {
                        out[0] = item->objgrp.worldmat[2][0] * f1 + pos[0];
                        out[1] = item->objgrp.worldmat[2][1] * f1 + pos[1];
                        out[2] = item->objgrp.worldmat[2][2] * f1 + pos[2];
                    } else {
                        f1 = -f1;
                        out[0] = item->objgrp.worldmat[2][0] * f1 + pos[0];
                        out[1] = item->objgrp.worldmat[2][1] * f1 + pos[1];
                        out[2] = item->objgrp.worldmat[2][2] * f1 + pos[2];
                    }
                } else {
                    out[0] = from[0];
                    out[1] = from[1];
                    out[2] = from[2];
                }
            } else {
                out[0] = from[0];
                out[1] = from[1];
                out[2] = from[2];
            }
            break;
        }
        case 4: {
            /* push the target out along the tri-list hit normal */
            nv[0] = hitpt[0] - pos[0];
            nv[1] = hitpt[1] - pos[1];
            nv[2] = hitpt[2] - pos[2];
            f1 = (f32)((nv[0] * norm[0] + nv[2] * norm[2]) + a);
            out[0] = pos[0];
            out[1] = pos[1];
            out[2] = pos[2];
            if (f1 > 0.0f) {
                out[0] = norm[0] * f1 + out[0];
                out[2] = norm[2] * f1 + out[2];
            }
            break;
        }
        case 1:
        default: {
        tangent_output:
            if (keep == 0) {
                nv[0] = pos[0] - from[0];
                nv[1] = 0.0f;
                nv[2] = pos[2] - from[2];
                mv[0] = (f32)(cx - from[0]);
                mv[1] = 0.0f;
                mv[2] = (f32)(cz - from[2]);
            }
            if (nv[2] * mv[0] - nv[0] * mv[2] > 0.0f) {
                f1 = -mv[2];
                mv[2] = mv[0];
                mv[0] = f1;
            } else {
                f1 = -mv[0];
                mv[0] = mv[2];
                mv[2] = f1;
            }
            NormalVector2D(mv);
            f1 = nv[0] * mv[0] + nv[2] * mv[2];
            out[0] = mv[0] * f1 + from[0];
            out[1] = mv[1] * f1 + from[1];
            out[2] = mv[2] * f1 + from[2];
            break;
        }
        }
    }
    return dist;
}

extern s32 LineCylinderCollide(f32* center, f32 radius, f32 halfHeight,
                               f32* from, f32* to, f32* hit,
                               s32 directional);
extern f32 lbl_80347004;
extern f64 sZeroDouble;

s32 fn_8005FB48(f32 radius, f32* from, f32* to, f32* limitPosition,
                 s32 stopAtFirst)
{
    Item* item;
    s32 itemIndex;
    s32 bestIndex;
    register s32 stop;
    s32 off;
    f32 delta[3];
    f32 zero;
    f64 zeroDouble;
    f32 limitDistance;
    f32 distance;
    s32 hit;
    iteminfo* info;
    s32 type;
    s32 subtype;

    stop = stopAtFirst;
    bestIndex = -1;
    if (limitPosition != 0) {
        delta[0] = limitPosition[0] - from[0];
        delta[1] = sItemZero;
        delta[2] = limitPosition[2] - from[2];
        limitDistance = fqdist(delta[0], delta[2]);
    } else {
        limitDistance = lbl_80347004;
    }

    off = 0;
    zeroDouble = sZeroDouble;
    itemIndex = 0;
    zero = sItemZero;
    for (; itemIndex < sNumItems;
         itemIndex++, off += sizeof(Item)) {
        item = (Item*)((u8*)sItems + off);
        if (item->active == -1) {
            continue;
        }

        info = item->info;
        type = info->type;
        subtype = info->item.subtype;
        switch (type) {
        case 2:
            switch (subtype) {
            case 43:
                if (item->action == 2) {
                    continue;
                }
                goto eligible_item;
            default:
                continue;
            }

        case 10:
            if (subtype == 49) {
                continue;
            }
            if (subtype >= 49) {
                goto high_subtype;
            }
            if (subtype == 41) {
                goto subtype_41;
            } else if (subtype >= 41) {
                goto eligible_item;
            } else if (subtype >= 40) {
                continue;
            }
            goto eligible_item;

        default:
            continue;
        }

high_subtype:
        if (subtype >= 54) {
            goto eligible_item;
        }
        if (subtype >= 51) {
            continue;
        }
        goto eligible_item;

subtype_41:
        if (item->health <= 0 || *(s16*)&item->data[2] <= 0) {
            continue;
        }

eligible_item:
        if (info->item.coltype == 4) {
            if ((f64)(s32)fn_8005FDA8((u8*)item, from, to, 0, 0,
                                       radius) >= zeroDouble) {
                hit = 1;
            } else {
                hit = 0;
            }
        } else {
            hit = LineCylinderCollide(&item->objgrp.coll_pos[0],
                                      info->item.radius + radius,
                                      info->item.height + radius,
                                      from, to, delta, 0);
        }

        if (hit != 0) {
            delta[0] = item->objgrp.coll_pos[0] - from[0];
            delta[1] = zero;
            delta[2] = item->objgrp.coll_pos[2] - from[2];
            distance = fqdist(delta[0], delta[2]);
            if (bestIndex >= 0 && distance >= zero) {
                continue;
            }
            if (limitDistance < distance) {
                continue;
            }
            bestIndex = itemIndex;
        }

        if (bestIndex >= 0 && stop != 0) {
            break;
        }
    }

    return bestIndex;
}

/* 0x8005C1DC - apply a hit of `power` to one world item (item/object damage
 * dispatcher).  Returns the remaining health as a float (-1 none, -2 heavy). */
extern void  DeleteItem(Item* item, s32 mode);
extern void  start_magic(s32 owner, f32* pos, s32 value, s32 mode, f32 radius);
extern void  StartFXMat(s32 fx, OBJGRP* grp);
extern s32   StartExplosion(OBJGRP* grp, s32 kind, f32 radius);
extern s32   MBOX_ReallyFindObject(char* name, s32 a, s32 b, s32 c);
extern void  MBTreeSetZsortAdd(void* node, s32 value, s32 mode);
extern s32   EnemyDescType(char* desc);
extern char* EnemyTypePrefix(s32 type);
extern void  AudioPlayEvt101(f32* pos);
extern void  AudioExplodeWall(f32* pos, s32 health);
void fn_8005E90C(Item* item, s32* inst);
extern void  fn_8009DA78(f32* pos);
extern void  fn_8009DA28(f32* pos);
extern void  fn_8009D9D8(f32* pos);
extern void  fn_8009EF4C(f32* pos);
extern void  AudioGeneratorDies(f32* pos, s32 gen);
extern void  AudioGeneratorDamaged(f32* pos, s32 gen);
extern void  StartGenHitFx(OBJGRP* grp, s32 gen, s32 off);
extern s32   fn_80094440(f32* pos, u32 flags, s32 destroyed);
extern void  AddItemWobj(Item* item);
extern s32   Round(f32 value);
extern s32   stricmp(const char* a, const char* b);
extern char* strcat(char* dst, const char* src);
extern Effect Effects[];   /* game/effect.h; stride 0xF0 */
extern s32   gNumEnemies;
extern f64   lbl_80346ED8;       /* 2^52 int-conv constant */
extern f64   lbl_80346EE8;
extern f64   lbl_80346EF0;
extern f64   lbl_80346F40;
extern f64   sItemFloorYOffset;
extern f32   sItemZero;
extern f32   sItemFloorRadius;
extern f32   lbl_80346F54;
extern const char lbl_80346F58[8]; /* "BADMEAT" */
extern const char lbl_80346F60[7]; /* "GAPPLE"  */
extern f32   lbl_80346F68;
extern f32   lbl_80346F6C;
extern const char lbl_80346F70[8];    /* "BOSSGEN" */
extern const char sLevelOneSuffix[3]; /* "L1"      */
extern const char sRootSuffix[5];     /* "ROOT"    */
extern f64   lbl_80346F88;
extern const char lbl_80346F90[8]; /* "BARPOI0" */
extern f64   lbl_80346F98;
extern const char lbl_80346FA0[8]; /* "BAREXP0" */
extern f32   lbl_80346FA8;

f32 fn_8005C1DC(Item* item, f32 power, s32 flags, s32 owner)
{
    s32 destroyed = 0;                        /* item died from this hit  */
    s32 alive;                                /* item tracks health       */
    s32 ret;                                  /* remaining health / code  */
    iteminfo* info = item->info;
    s32* sub = (s32*)((u8*)info + 4);
    char* objects = sObjectsFile;
    char buf[0x24];
    f32 v[4];
    void* hdr;
    u8* rec;
    s16* generator;
    s32 k;
    s32 thr;
    s32 state;

    /* scale generator hits by how far the player's gold exceeds the ramp */
    if (info->type == 3 && owner >= 0 &&
        gCurLevel->plevel > sItemZero) {
        f32 mult;
        f32 ramp = gCurLevel->plevel;
        f32 gold = (f32)*(s32*)((u8*)gPlayers + owner * 0x335C + 0x3324);

        if (gold < ramp) {
            mult = (f32)(lbl_80346EE8 - lbl_80346F40 * (ramp - gold));
        } else if (gold > ramp) {
            mult = (f32)(sItemFloorYOffset * (gold - ramp) + lbl_80346EE8);
        } else {
            mult = sItemFloorRadius;
        }
        power = power * mult;
        if (power < (f32)lbl_80346EE8) {
            power = sItemFloorRadius;
        }
    }

    if ((flags & 0x800) != 0) {
        if (power <= sCameraVisibilityRadius) {
            ret = -1;
        } else {
            ret = -2;
        }
        power = sItemZero;
        alive = 0;
    } else {
        if (item->armor >= 0) {
            power = power - (f32)item->armor;
            if (power <= sItemZero) {
                power = sItemFloorRadius;
            }
            item->health -= Round(power);
            if (item->health < 0) {
                item->health = 0;
            }
            if (item->health == 0) {
                destroyed = 1;
            } else {
                destroyed = 0;
            }
        }
        if ((s8)item->armor == -1) {
            ret = -1;
            alive = 0;
        } else {
            alive = 1;
            ret = (s32)(f32)item->health;
        }
    }

    v[1] = item->objgrp.coll_pos[0];
    v[2] = item->objgrp.coll_pos[1];
    v[3] = item->objgrp.coll_pos[2];
    v[2] = (f32)(v[2] + lbl_80346EF0);

    switch (info->type) {
    case 1:
        switch (*sub) {
        case 4:
            if (ret == 0) {
                start_magic(-1, item->objgrp.attn_pos,
                            *(s32*)((u8*)info + 0x3C), 0, lbl_80346F54);
                if (item->info->type == 1 && *(Item**)&item->data[0xC] != 0) {
                    DeleteItem(*(Item**)&item->data[0xC], 0);
                }
                if (item->info->type == 2 && *(Item**)&item->data[0xC] != 0) {
                    DeleteItem(*(Item**)&item->data[0xC], 0);
                }
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                    *(u32*)&item->atree[0] = 0;
                }
                if (item->objgrp.node != 0) {
                    MBRemoveNode(item->objgrp.node, 0);
                    item->objgrp.node = 0;
                }
                item->active = -1;
                k = ((u8*)item - (u8*)sItems) / 0xF0;
                if (k < gNextItemIdx) {
                    gNextItemIdx = k;
                }
            }
            break;

        case 2:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
            break;

        case 3:
            if (ret == -2) {
                /* good food goes bad on a heavy hit */
                if (item->health == 2) {
                    hdr = AtreeMatch(sPowerupsBuf, lbl_80346F58, 1);
                    if (*(u32*)&item->atree[0] != 0) {
                        AtreeDelete(item->atree);
                    }
                    *(void**)&item->atree[0] =
                        AtreeInit(hdr, item->atree, 0, 0x800);
                    MBNodeSetParent(**(void***)&item->atree[0],
                                    item->objgrp.node);
                    *(s32*)&item->data[4] = -100;
                } else {
                    hdr = AtreeMatch(sPowerupsBuf, lbl_80346F60, 1);
                    if (*(u32*)&item->atree[0] != 0) {
                        AtreeDelete(item->atree);
                    }
                    *(void**)&item->atree[0] =
                        AtreeInit(hdr, item->atree, 0, 0x800);
                    MBNodeSetParent(**(void***)&item->atree[0],
                                    item->objgrp.node);
                    *(s32*)&item->data[4] = -50;
                }
                destroyed = 1;
                msgPost(0x88, -1, 0);
            }
            /* fall through */

        default:
        /* generic destroy */
        if ((flags & 0x400) != 0 && power >= lbl_80346F68) {
            StartFXMat(0x20, &item->objgrp);
            StartFXMat(0x21, &item->objgrp);
            MBOX_NewObject(&objects[0x14C], item->objgrp.node,
                           *(s32*)((u8*)item->objgrp.node + 0x74), 0x80800);
            if (item->info->type == 1 && *(Item**)&item->data[0xC] != 0) {
                DeleteItem(*(Item**)&item->data[0xC], 0);
            }
            if (item->info->type == 2 && *(Item**)&item->data[0xC] != 0) {
                DeleteItem(*(Item**)&item->data[0xC], 0);
            }
            if (*(u32*)&item->atree[0] != 0) {
                AtreeDelete(item->atree);
                *(u32*)&item->atree[0] = 0;
            }
            if (item->objgrp.node != 0) {
                MBRemoveNode(item->objgrp.node, 0);
                item->objgrp.node = 0;
            }
            item->active = -1;
            k = ((u8*)item - (u8*)sItems) / 0xF0;
            if (k < gNextItemIdx) {
                gNextItemIdx = k;
            }
            msgPost(0x87, -1, 0);
        }
            break;

        case 1:
        /* food -> junk treasure */
        if ((flags & 0x400) != 0 && power >= lbl_80346F68) {
            StartFXMat(0x20, &item->objgrp);
            StartFXMat(0x21, &item->objgrp);
            hdr = AtreeMatch(sPowerupsBuf, &objects[0x158], 1);
            if (*(u32*)&item->atree[0] != 0) {
                AtreeDelete(item->atree);
            }
            *(void**)&item->atree[0] = AtreeInit(hdr, item->atree, 0, 0x800);
            MBNodeSetParent(**(void***)&item->atree[0], item->objgrp.node);
            *(s32*)&item->data[4] = 10;
        }
            break;
        }
        break;

    case 2:
        rec = 0;
        if (*(s16*)&item->data[0] >= 0) {
            rec = (u8*)gWorldInfo.iteminfo + *(s16*)&item->data[0] * 0x50;
        }
        if (rec != 0 && (flags & 0x200) != 0 &&
            EnemyDescType((char*)(rec + 0x28)) == 0x1E && *sub != 0x2B) {
            /* enemy chest converts to an apple generator */
            *sub = 1;
            rec = (u8*)gWorldInfo.iteminfo;
            for (k = 0; k < *(s32*)((u8*)&gWorldInfo + offsetof(WorldInfo, niteminfos)); k++) {
                s32* rec_sub = (s32*)(rec + 4);

                if (strcmp(lbl_80346F18, (char*)(rec_sub + 9)) == 0 &&
                    *(s32*)rec == 1 && *rec_sub == 3) {
                    goto found_gen;
                }
                rec += 0x50;
            }
            k = -1;
found_gen:
            *(s16*)&item->data[0] = (s16)k;
            AudioPlayEvt101(&v[1]);
            alive = 1;
            *(s16*)&item->data[2] = (s16)(lbl_80346F6C * power);
        } else if (destroyed != 0 && (item->active & 0x200) != 0) {
            if ((item->active & 1) == 0) {
                item->active |= 1;
                fn_8005E90C(item, 0);
                if (*sub == 0x2B) {
                    fn_8009DA78(&v[1]);
                    msgPost(0x1B, -1, (char*)&v[1]);
                }
            }
        } else if ((flags & 0x400) != 0 && power >= lbl_80346F68) {
            if (*sub == 0x2C) {
                item->daction = 2;
                item->action = 2;
                item->active |= 1;
                fn_8005E90C(item, 0);
            } else {
                if (rec != 0 && EnemyDescType((char*)(rec + 0x28)) == 0x1E) {
                    fn_8005E90C(item, 0);
                }
                StartFXMat(0x1F, &item->objgrp);
                StartFXMat(0x21, &item->objgrp);
                if (*sub == 0x30) {
                    MBOX_NewObject(&objects[0x164], item->objgrp.node,
                                   *(s32*)((u8*)item->objgrp.node + 0x74),
                                   0x80800);
                } else {
                    MBOX_NewObject(&objects[0x170], item->objgrp.node,
                                   *(s32*)((u8*)item->objgrp.node + 0x74),
                                   0x80800);
                }
                if (item->info->type == 1 && *(Item**)&item->data[0xC] != 0) {
                    DeleteItem(*(Item**)&item->data[0xC], 0);
                }
                if (item->info->type == 2 && *(Item**)&item->data[0xC] != 0) {
                    DeleteItem(*(Item**)&item->data[0xC], 0);
                }
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                    *(u32*)&item->atree[0] = 0;
                }
                if (item->objgrp.node != 0) {
                    MBRemoveNode(item->objgrp.node, 0);
                    item->objgrp.node = 0;
                }
                item->active = -1;
                k = ((u8*)item - (u8*)sItems) / 0xF0;
                if (k < gNextItemIdx) {
                    gNextItemIdx = k;
                }
            }
        }
        break;
    case 3:
        /* enemy generator damage-state machine */
        generator = (s16*)&item->data[0];
        thr = (s32)((f32)*(s16*)((u8*)info + 0x44) *
                    gCurLevel->gen_health);
        if (ret < 0) {
            break;
        }
        if (destroyed != 0) {
            state = 0;
        } else if (item->health <= thr) {
            state = 1;
        } else if (item->health <= thr * 2) {
            state = 2;
        } else {
            state = 3;
        }
        if (state != (s8)item->data[6]) {
            item->data[6] = state;
            if (state == 0) {
                item->armor = -1;
            } else {
                item->data[3] = (u8)(item->data[3] << 1);
            }
            if ((s8)item->data[7] == 0x1C || (s8)item->data[7] == 0x1D) {
                item->data[7] = 0;
            }
            if ((s8)item->data[7] == 0x1E) {
                item->data[7] = 0;
            }
            if (stricmp(buf, lbl_80346F70) != 0) {
                if (*generator < -1) {
                    sprintf(buf, &objects[0x17C], (s8)item->data[6]);
                } else {
                    sprintf(buf, &objects[0x18C],
                            EnemyTypePrefix(*generator),
                            (s8)item->data[6]);
                }
            }
            hdr = AtreeMatchAnyHeader(buf, 1);
            if (hdr != 0) {
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                }
                *(void**)&item->atree[0] = AtreeInit(hdr, item->atree, 0, 0x800);
                MBNodeSetParent(**(void***)&item->atree[0], item->objgrp.node);
            } else {
                k = MBOX_ReallyFindObject(buf, -1, -1, -1);
                if (k < 0) {
                    strcat(buf, sLevelOneSuffix);
                    k = MBOX_ReallyFindObject(buf, -1, -1, -1);
                }
                if (k < 0) {
                    strcat(buf, sRootSuffix);
                    k = MBOX_ReallyFindObject(buf, -1, -1, -1);
                }
                if (k < 0) {
                    if (*(u32*)&item->atree[0] != 0) {
                        AtreeDelete(item->atree);
                        *(u32*)&item->atree[0] = 0;
                    }
                    if (item->objgrp.node != 0) {
                        MBRemoveNode(item->objgrp.node, 0);
                        item->objgrp.node = 0;
                    }
                    item->active = -1;
                    k = ((u8*)item - (u8*)sItems) / 0xF0;
                    if (k < gNextItemIdx) {
                        gNextItemIdx = k;
                    }
                } else {
                    MBSetObject(item->objgrp.node, k);
                }
            }
            if (state == 0) {
                item->active &= ~1;
                item->armor = -1;
            }
            if (state == 0) {
                StartGenHitFx(&item->objgrp, *generator, 1);
            } else {
                StartGenHitFx(&item->objgrp, *generator, 0);
            }
        }
        if (state == 0) {
            s32 enemy_count;

            AudioGeneratorDies(&v[1], *generator);
            enemy_count = gNumEnemies;
            for (k = 0; k < enemy_count; k++) {
                if (*(Item**)((u8*)gEnemies + k * 0x394 + offsetof(Enemy, generator)) == item) {
                    *(Item**)((u8*)gEnemies + k * 0x394 + offsetof(Enemy, generator)) = 0;
                }
            }
        } else {
            AudioGeneratorDamaged(&v[1], *generator);
        }
        break;

    case 10:
        if (ret < 0) {
            break;
        }
        if (alive != 0 && destroyed == 0 && *sub != 0x29) {
            *(s16*)&item->data[4] = 1;
        }
        switch (*(s32*)((u8*)item->info + 4)) {
        default:
            /* 0x2B, walls, everything else: shake / rumble */
            if (destroyed != 0) {
                item->active |= 1;
                fn_8009DA78(&v[1]);
                destroyed = 0;
            } else {
                fn_8009EF4C(&v[1]);
            }
            break;
        case 0x2C:
            if (destroyed != 0) {
                item->active |= 1;
                StartExplosion(&item->objgrp, 0x18,
                               (f32)(lbl_80346F88 *
                                     gCurLevel->trap_damage));
                fn_8009D9D8(&v[1]);
                MBOX_NewObject(lbl_80346F90, item->objgrp.node,
                               *(s32*)((u8*)item->objgrp.node + 0x74),
                               0x80800);
                alive = 0;
                ret = -2;
                destroyed = 0;
            } else {
                fn_8009EF4C(&v[1]);
            }
            break;
        case 0x2D:
            if (destroyed != 0) {
                item->active |= 1;
                StartExplosion(&item->objgrp, 0x19,
                               (f32)(lbl_80346F98 *
                                     gCurLevel->trap_damage));
                fn_8009DA28(&v[1]);
                MBOX_NewObject(lbl_80346FA0, item->objgrp.node,
                               *(s32*)((u8*)item->objgrp.node + 0x74),
                               0x80800);
                alive = 0;
                ret = -2;
                destroyed = 0;
            } else {
                fn_8009EF4C(&v[1]);
            }
            break;
        case 0x2A:
            AudioExplodeWall(&v[1], item->health);
            break;
        case 0x29:
            if (*(s16*)&item->data[2] >= 0) {
                AddItemWobj(item);
                destroyed = 0;
            }
            break;
        }
        if (destroyed != 0) {
            if (*(u32*)&item->atree[0] != 0) {
                AtreeDelete(item->atree);
                *(u32*)&item->atree[0] = 0;
            }
            if (item->objgrp.node != 0) {
                MBRemoveNode(item->objgrp.node, 0);
                item->objgrp.node = 0;
            }
            item->active = -1;
            k = ((u8*)item - (u8*)sItems) / 0xF0;
            if (k < gNextItemIdx) {
                gNextItemIdx = k;
            }
        }
        break;

    case 4:
        *(s32*)&item->data[8] |= 1;
        break;

    case 5:
        if ((flags & 0x800) == 0 && *sub == 0x1F) {
            Item* w;
            ret = 1;
            for (w = item; w != 0; w = *(Item**)&w->data[8]) {
                w->playermask |= 0xF;
            }
        }
        break;
    }

    if (alive != 0) {
        k = fn_80094440(&v[1], flags, destroyed);
        if (k >= 0) {
            MBTreeSetZsortAdd(*(void**)((u8*)Effects + k * 0xF0 + offsetof(Effect, node)),
                              (s32)(lbl_80346FA8 * info->item.radius), 1);
        }
    }
    return (f32)ret;
}

/* --------------------------------------------------------------------------
 * fn_8005D730  0x8005D730  size 0x0720 - resolve a player's collision with a
 * live world item.  The outer switch selects the broad item class; individual
 * cases queue pickups, activate generators, apply hazards, or update the
 * per-player mask carried by linked trigger items.
 * ------------------------------------------------------------------------ */
extern void GetPlayerPos(s32 player, f32* position);
extern void PlayerGiveGold(s32 player, s32 amount);
extern void add_got_it(s32 player, s32 subtype, s32 count);
extern void fn_8009D038(s32 player);
extern void fn_8009D078(f32* position);
extern void fn_8009D0A8(f32* position, s32 subtype);
extern void fn_8009D8CC(f32* position);
extern void AudioDamageTile(f32* position, s32 subtype);
extern s32 damage_player(s32 player, f32 damage, s32 type, u32 flags,
                         f32* direction);
extern s32 SumnerAnimate(s32 player);
extern s32 lbl_8034476C;
extern const char lbl_80346FD8[7];
extern f32 sMusicFadeBase;

s32 fn_8005D730(Player* player, Item* item)
{
    s32 result;
    iteminfo* info;
    iteminfodata* data;
    f32 playerPos[3];
    f32 itemPos[3];
    u8 unused[28];

    result = 0;
    info = item->info;
    data = &info->item;
    GetPlayerPos(player->index, playerPos);
    itemPos[0] = item->objgrp.coll_pos[0];
    itemPos[1] = item->objgrp.coll_pos[1];
    itemPos[2] = item->objgrp.coll_pos[2];

    switch (info->type) {
    case 1:
        if (*(s16*)&item->data[0x10] <= 0 && player->speech_req == NULL) {
            player->speech_req = (s32*)item;
        }
        break;

    case 2:
        result = 1;
        if (item->action == 2) {
            if (info->item.subtype == 47) {
                s32 amount;

                if (*(s32*)&item->data[4] >= 25) {
                    msgPost(17, player->index, (char*)player->col_pos);
                }
                amount = *(s32*)&item->data[4];
                PlayerGiveGold(player->index, amount);
                fn_8009D038(player->index);
                add_got_it(player->index, 1, amount);
                item->activetime = 8;
                item->active |= 0x100;
                {
                    s32* row = (s32*)player;

                    row += player->character * 7;
                    row[777] += amount;
                }
            } else {
                Item* linked = *(Item**)&item->data[0xC];

                if (linked != NULL) {
                    result = 0;
                    if (player->speech_req == NULL) {
                        player->speech_req = (s32*)linked;
                    }
                }
            }
        } else if ((item->active & 0x10) != 0 && item->action == 0 &&
                   (item->active & 1) == 0) {
            s32 allow;

            if (player->item_body_lo > 0) {
                player->item_body_lo--;
                allow = -1;
            } else if ((*(u32*)(gGameOptions + 4) & 1) != 0) {
                allow = -1;
            } else {
                allow = 0;
            }

            if (allow != 0) {
                fn_8009D078(itemPos);
                item->active |= 1;
                item->opener = (s8)player->index;
                if (*(s16*)&item->data[0] >= 0) {
                    fn_8005E90C(item, (s32*)player);
                }
            } else if (msgPost(2, player->index,
                               (char*)player->col_pos) == 0 &&
                       data->subtype == 48) {
                msgPost(23, player->index, (char*)player->col_pos);
            }
        }
        break;

    case 3:
        {
            s32 inverted;
            s32 selected;

            if (*(s8*)&item->data[6] != 0) {
                inverted = 0;
            } else {
                inverted = 1;
            }
            if (inverted != 0) {
                selected = 0;
            } else {
                selected = 1;
            }
            result = selected;
        }
        break;

    case 4:
        if (*(f32*)&item->data[0xC] >= 0.0) {
            *(u32*)&item->data[8] |= 1;
        }
        if (fqdist(itemPos[0] - playerPos[0],
                   itemPos[2] - playerPos[2]) <= data->radius) {
            result = 1;
        } else {
            result = 0;
        }
        break;

    case 7:
        if (item->action == 0 && (item->active & 1) == 0) {
            f32 dot;

            dot = (playerPos[0] - *(f32*)((u8*)player + 0x87C)) *
                      (itemPos[0] - playerPos[0]) +
                  (playerPos[2] - *(f32*)((u8*)player + 0x884)) *
                      (itemPos[2] - playerPos[2]);
            if (dot < 0.0f) {
                result = 1;
            } else {
                s32 allow;

                if (player->item_body_lo > 0) {
                    player->item_body_lo--;
                    allow = -1;
                } else if ((*(u32*)(gGameOptions + 4) & 1) != 0) {
                    allow = -1;
                } else {
                    allow = 0;
                }

                if (allow != 0) {
                    item->active |= 1;
                    fn_8009D0A8(itemPos, data->subtype);
                } else {
                    msgPost(1, player->index,
                            (char*)&item->objgrp.worldmat[3][0]);
                    result = 1;
                }
            }
        } else {
            result = 1;
        }
        break;

    case 8:
        if ((player->flags & 1) == 0 &&
            sMusicFadeBase >= player->fxhittime) {
            s32 rawSubtype = data->subtype;
            s32 subtype = rawSubtype;
            s32 damageType = 2;
            s32 flags = info->item.properties | 0x80;
            f32 damage = *(f32*)&item->data[0];

            if (rawSubtype == 0 || (u32)(subtype - 3) <= 1) {
                damageType = 3;
            }
            if ((flags & 0x30) != 0) {
                f32 direction[3];
                u8 directionPad[24];

                direction[0] =
                    (f32)(-1.0 * item->objgrp.worldmat[2][0]);
                direction[1] =
                    (f32)(-1.0 * item->objgrp.worldmat[2][1]);
                direction[2] =
                    (f32)(-1.0 * item->objgrp.worldmat[2][2]);
                damage_player(player->index, damage, damageType, flags,
                              direction);
            } else {
                damage_player(player->index, damage, damageType, flags, NULL);
            }

            if (sMusicTrackHi == 3 && subtype == 1 &&
                strcmp(info->item.desc, lbl_80346FD8) == 0) {
                subtype = 6;
            }
            AudioDamageTile(playerPos, subtype);
            player->fxhittime =
                (f32)(0.0333333333 * (f64)(item->activetime + 1) +
                      sMusicFadeBase);
            msgPost(21, player->index, (char*)player->col_pos);
        }
        break;

    case 9:
        *(u32*)&item->data[4] |= 1 << player->index;
        result = 2;
        break;

    case 10:
        switch (data->subtype) {
        case 49:
            if ((item->active & 1) == 0) {
                fn_8009D8CC(&item->objgrp.worldmat[3][0]);
                item->active |= 1;
            }
            break;
        case 40:
        case 53:
            if ((item->active & 1) == 0) {
                fn_8009D91C(&item->objgrp.worldmat[3][0]);
                item->active |= 1;
            }
            break;
        case 41:
            if (*(s16*)&item->data[2] > 0) {
                result = 1;
            }
            break;
        case 51:
        case 52:
            break;
        default:
            result = 1;
            break;
        }
        break;

    case 12:
        if (item->info->item.subtype == 2 && (item->active & 1) == 0) {
            item->active |= 1;
        }
        break;

    case 11:
        result = 2;
        break;

    case 5: {
        s16 flags = *(s16*)&item->data[4];

        if ((flags & 0x100) != 0) {
            u8* record = *(u8**)&item->data[0];

            if (record != NULL && (u8*)player->floor_name2 != record) {
                u8* worldRecords = (u8*)gWorldInfo.wobjs;
                u8* wanted = (u8*)player->floor_name2;
                s32 index = *(s16*)(record + 0x2E);
                s32 found = 0;

                while (index >= 0) {
                    record = worldRecords + index * 0x3C;
                    if (record == wanted) {
                        found = 1;
                        break;
                    }
                    index = *(s16*)(record + 0x2C);
                }
                if (found == 0) {
                    result = 0;
                    break;
                }
            }
            if (item->playermask == 0 && lbl_8034476C > 1 &&
                (flags & 0x400) != 0) {
                msgPost(126, player->index, (char*)player->col_pos);
            }
        } else if (item->playermask == 0 && lbl_8034476C > 1 &&
                   (flags & 0x400) != 0) {
            msgPost(127, player->index, (char*)player->col_pos);
        }

        if ((*(s16*)&item->data[4] & 0x40) != 0) {
            item->playermask |= 1 << player->index;
        } else {
            Item* linked = item;
            s32 one = 1;

            while (linked != NULL) {
                linked->playermask |= one << player->index;
                linked = *(Item**)&linked->data[8];
            }
        }

        if (sMusicTrackHi == 13) {
            s32 marker = *(u8*)&item->data[6];

            if (marker == 0xF0) {
                if ((item->playermask & 0xF) != 0) {
                    if ((item->playermask & 0x10) == 0 &&
                        SumnerAnimate(player->index) != 0) {
                        item->playermask |= 0x10;
                    }
                } else {
                    item->playermask = 0;
                }
            }
        }
        break;
    }
    }

    return result;
}

/* --------------------------------------------------------------------------
 * fn_8005DE50  0x8005DE50  size 0x0ABC - item-pickup effect state machine
 * Called once per frame from do_players (player.c) for the item a player is
 * currently touching:  fn_8005DE50(player, player->special_collision_item).
 * Dispatches on the item info subtype (b->info->item.subtype) and applies the
 * pickup effect (gold / count / potion / milestone / powerup / shard / rune /
 * boss / level), then reparents + re-shows the item's scene node.
 * ------------------------------------------------------------------------ */
extern void PlayerGiveGold(s32 player, s32 amount);
extern void fn_8009CFA8(s32 player, s32 amount);
extern void add_got_it(s32 player, s32 subtype, s32 count);
extern s32  towerAwardWorldRunes(void);
extern s32  fn_8009FB30(void);
extern s32  FindStringMessageListSub_8001FC4C(s32 a, char* name);
extern void ControllerMessageBox(s32 a, s32 b, s32 c, s32 d);
extern void fn_8009CDF8(s32 player);
extern void fn_8009F748(s32 player, s32 sourcePlayer);
extern void fn_8009D038(s32 player);
extern s32  heal_player(Player* p, f32 amount);
extern s32 damage_player(s32 i, f32 dmg, s32 mode, u32 flags,
                         f32* direction);
extern void AudioPlayerPoison(s32 player);
extern void AudioPlayerEatFood(s32 player, s32 kind);
extern void PlayerAddPowerup(f32 duration, f32 strength, void* p, s32 type,
                             u32 mask);
extern void fn_8009CEE0(s32 player, s32 subtype, s32 flags);
extern void PlayerGiveShard(s32 player, s32 shard);
extern void fn_8009CE38(s32 player);
extern void StartGemFX(f32* pos, s32 kind);
extern void AudioNumRunesFound(s32 count);
extern void towerSetRuneNear(s32 player, s32 value);
extern void towerAdvanceBossRecord(s32 a, s32 b);
extern void towerAdvanceLevelRecord(s32 a, s32 b);

extern char  lbl_80112C50[];      /* .rodata string (absolute)  */
extern char  lbl_80112C5C[];      /* .rodata string (absolute)  */
extern const char lbl_80346FEC[7];/* .sdata2 string (sda21)     */
extern f32   lbl_80346FE8;        /* .sdata2 float              */
extern f32   sItemFloorRadius;    /* .sdata2 float              */
extern s32   lbl_80344810;        /* .sbss                      */
extern f32   lbl_80344818;        /* .sbss float                */
extern s32   lbl_803448A0;        /* .sbss                      */
extern s32   lbl_803448A4;        /* .sbss                      */
extern s32   gNumType7Items;      /* .sbss                      */
extern s32   welcome_timer;       /* .sbss                      */
extern s32   sSpecialItem10;      /* .sbss                      */
extern void* sItemsRootNode;      /* .sbss                      */
extern f32   sItemZero;
extern f64   sZeroDouble;

void fn_8005DE50(Player* a, Item* b)
{
    iteminfo* ev;
    iteminfodata* it;
    s32 ret;
    s32 flag;
    u8 unused[8];

    ret = 0;
    if (b == NULL)
        return;
    ev = b->info;
    if (ev == NULL)
        return;
    if (a != NULL)
        goto process_item;
    if (a == NULL)
        goto done;
    goto done;
process_item:
    switch ((it = &ev->item)->subtype) {
    case 1: { /* gold */
        s32 amount = *(s32*)&b->data[4];
        PlayerGiveGold(a->index, amount);
        {
            s32* row = (s32*)a;
            row += a->character * 7;
            row[777] += amount;
        }
        fn_8009CFA8(a->index, *(s32*)&b->data[4]);
        add_got_it(a->index, it->subtype, *(s32*)&b->data[4]);
        *(s16*)((u8*)a + 0x95C) = 1;
        if (sMusicTrackHi == 12) {
            if (towerAwardWorldRunes() != 0) {
                s32 h = fn_8009FB30();
                ControllerMessageBox(-1,
                    FindStringMessageListSub_8001FC4C(0, lbl_80112C50), 0, h);
                lbl_80344810 = 1;
                lbl_80344818 = sItemFloorRadius;
            }
        } else if (*(s32*)&b->data[4] >= 25) {
            s8 op;
            msgPost(17, a->index, (char*)a->col_pos);
            op = b->opener;
            if (op >= 0 && op != a->index)
                fn_8009F748(op, a->index);
        }
        ret = 1;
        break;
    }
    case 2: { /* accumulating count (clamped to lbl_803448A4) */
        if (a->item_body_lo + *(s32*)&b->data[4] <= lbl_803448A4) {
            s8 op;
            if (gNumType7Items != 0)
                msgPost(8, a->index, (char*)a->col_pos);
            else
                msgPost(2, a->index, (char*)a->col_pos);
            a->item_body_lo += *(s32*)&b->data[4];
            op = b->opener;
            if (op >= 0 && op != a->index)
                fn_8009F748(op, a->index);
            fn_8009CDF8(a->index);
            add_got_it(a->index, it->subtype, *(s32*)&b->data[4]);
            *(s16*)((u8*)a + 0x95C) = 1;
            ret = 1;
        } else if (a->item_body_lo < lbl_803448A4) {
            s32 room = lbl_803448A4 - a->item_body_lo;
            if (gNumType7Items != 0)
                msgPost(8, a->index, (char*)a->col_pos);
            else
                msgPost(2, a->index, (char*)a->col_pos);
            a->item_body_lo += room;
            *(s32*)&b->data[4] -= room;
            fn_8009CDF8(a->index);
            add_got_it(a->index, it->subtype, *(s32*)&b->data[4]);
            *(s16*)((u8*)a + 0x95C) = 1;
        } else {
            msgPost(4, a->index, (char*)a->col_pos);
        }
        break;
    }
    case 4: { /* milestone list */
        s32 j;
        s32 r;
        s8  op;
        if (a->item_body_hi < lbl_803448A0) {
            s32 prop = it->properties;
            for (j = 0; j < *(s32*)&b->data[4]; j++) {
                if (a->item_body_hi >= lbl_803448A0)
                    break;
                *(s32*)((u8*)a + a->item_body_hi * 4 + 13056) = prop;
                a->item_body_hi++;
            }
            r = msgPost(7, a->index, (char*)a->col_pos);
            if (r < 0)
                r = msgPost(94, a->index, (char*)a->col_pos);
            if (r < 0)
                msgPost(95, a->index, (char*)a->col_pos);
            op = b->opener;
            if (op >= 0 && op != a->index)
                fn_8009F748(op, a->index);
            fn_8009D038(a->index);
            add_got_it(a->index, it->subtype, 0);
            *(s16*)((u8*)a + 0x95C) = 1;
            ret = 1;
        } else {
            msgPost(3, a->index, (char*)a->col_pos);
        }
        break;
    }
    case 3: { /* potion (heal / damage) */
        s8 op;
        f32 amt = (f32)*(s32*)&b->data[4];
        if ((a->flags & 0x400) && strcmp(it->desc, lbl_80346F10) == 0)
            amt = lbl_80346FE8;
        if (amt >= sItemZero) {
            if (heal_player(a, amt) == 0) {
                msgPost(133, a->index, (char*)a->col_pos);
                break;
            }
        } else {
            damage_player(a->index, -amt, 0, 2048, 0);
        }
        if (*(s32*)&b->data[4] >= 100)
            msgPost(15, a->index, (char*)a->col_pos);
        else if (*(s32*)&b->data[4] >= 50)
            msgPost(16, a->index, (char*)a->col_pos);
        else if (*(s32*)&b->data[4] < 0)
            msgPost(28, a->index, (char*)a->col_pos);
        op = b->opener;
        if (op >= 0 && op != a->index)
            fn_8009F748(op, a->index);
        add_got_it(a->index, it->subtype, (s32)amt);
        if (amt < sZeroDouble) {
            *(s16*)((u8*)a + 0x95C) = 3;
            AudioPlayerPoison(a->index);
        } else {
            s32 kind = 0;
            *(s16*)((u8*)a + 0x95C) = 1;
            if (strcmp(it->desc, lbl_80112C5C) == 0)
                kind = 3;
            else if (strcmp(it->desc, lbl_80346F18) == 0)
                kind = 1;
            else if (strcmp(it->desc, lbl_80346FEC) == 0)
                kind = 2;
            AudioPlayerEatFood(a->index, kind);
        }
        ret = 1;
        break;
    }
    case 5:
    case 6:
    case 7:
    case 8:
    case 9: { /* powerup */
        s32* data;
        s32 snd;
        s32 flags220;
        s32 subtype;
        data = (s32*)&b->data[0];
        flags220 = data[0];
        snd = -1;
        subtype = it->subtype;
        if (subtype == 9 && (flags220 & 0xF000) && (a->flags & 0xF000))
            break;
        PlayerAddPowerup((f32)data[1], *(f32*)&data[2], a,
                         subtype, flags220);
        switch (it->subtype) {
        case 5: {
            s32 lownib = flags220 & 0xF;
            if (flags220 & 0x80000)
                snd = 37;
            else if (flags220 & 0x400000)
                snd = 47;
            else if (flags220 & 0x200000)
                snd = 38;
            else if (flags220 & 0x100000)
                snd = 48;
            else if (flags220 & 0x10000000)
                snd = 86;
            else if (flags220 & 0x20000000)
                snd = 87;
            else if (lownib == 1)
                snd = 40;
            else if (lownib == 2)
                snd = 41;
            else if (lownib == 3)
                snd = 42;
            else if (lownib == 4)
                snd = 43;
            break;
        }
        case 6:
            if (flags220 & 0x100000)
                snd = 54;
            else if (flags220 & 0x10000)
                snd = 35;
            else if (flags220 & 0x80000)
                snd = 49;
            else if (flags220 & 0x20000)
                snd = 52;
            else if (flags220 & 0x200000)
                snd = 91;
            else if (flags220 & 0x400000)
                snd = 92;
            else if (flags220 & 0x2000)
                snd = 132;
            break;
        case 7:
            snd = 32;
            break;
        case 8:
            snd = 33;
            break;
        case 9:
            if (flags220 & 0x4)
                snd = 36;
            else if (flags220 & 0x2)
                snd = 39;
            else if (flags220 & 0x8)
                snd = 51;
            else if (flags220 & 0x1)
                snd = 53;
            else if (flags220 & 0x10)
                snd = 81;
            else if (flags220 & 0x20)
                snd = 82;
            else if (flags220 & 0x40)
                snd = 83;
            else if (flags220 & 0x80)
                snd = 84;
            else if (flags220 & 0x100)
                snd = 88;
            else if (flags220 & 0x200)
                snd = 89;
            else if (flags220 & 0x400)
                snd = 93;
            else if (flags220 & 0x2000)
                snd = 98;
            else if (flags220 & 0x1000)
                snd = 99;
            else if (flags220 & 0x8000)
                snd = 100;
            else if (flags220 & 0x4000)
                snd = 100;
            else if (flags220 & 0x80000)
                snd = 113;
            else if (flags220 & 0x100000)
                snd = 148;
            else if (flags220 & 0x200000)
                snd = 149;
            else if (flags220 & 0x400000)
                snd = 150;
            break;
        }
        if (snd >= 0)
            msgPost(snd, a->index, (char*)a->col_pos);
        fn_8009CEE0(a->index, it->subtype, flags220);
        add_got_it(a->index, it->subtype, 0);
        *(s16*)((u8*)a + 0x95C) = 1;
        ret = 1;
        break;
    }
    case 10: { /* shard */
        s32 i;
        s32 mask;
        s32 count;
        if (PlayerHasShard(a->index, ev->item.value) != 0) {
            msgPost(90, a->index, (char*)a->col_pos);
            ret = 0;
            break;
        }
        for (i = 0; i < 4; i++)
            PlayerGiveShard(i, b->info->item.value);
        fn_8009CE38(a->index);
        add_got_it(a->index, it->subtype, *(s32*)&b->data[4]);
        welcome_timer = 300;
        StartGemFX(b->objgrp.worldmat[3], 1024);
        sSpecialItem10 = 0;
        count = 0;
        mask = 0;
        for (i = 0; i < 4; i++) {
            if (gPlayers[i].state != 0)
                mask |= gPlayers[i].shards;
        }
        for (i = 0; i < 13; i++) {
            if (mask & (1 << i))
                count++;
        }
        AudioNumRunesFound(count);
        ret = 1;
        break;
    }
    case 13: /* rune */
        msgPost(*(s32*)&b->data[4] + 113, a->index, (char*)a->col_pos);
        towerSetRuneNear(a->index, *(s32*)&b->data[4]);
        fn_8009D038(a->index);
        add_got_it(a->index, it->subtype, *(s32*)&b->data[4]);
        ret = 1;
        break;
    case 14: /* controller message */
        ControllerMessageBox(1 << a->index, -1, *(s32*)&b->data[4] - 1, -1);
        ret = 1;
        break;
    case 15: { /* boss record */
        s32 amount = *(s32*)&b->data[4];
        ret = 1;
        StartGemFX(b->objgrp.worldmat[3], amount);
        towerAdvanceBossRecord(-1, amount);
        fn_8009D038(a->index);
        add_got_it(a->index, it->subtype, amount);
        break;
    }
    case 16: /* level record */
        towerAdvanceLevelRecord(-1, *(s32*)&b->data[4]);
        fn_8009D038(a->index);
        add_got_it(a->index, it->subtype, *(s32*)&b->data[4]);
        ret = 1;
        StartGemFX(b->objgrp.worldmat[3], 256);
        break;
    }

    flag = ((s8)b->opener == -1) ? 8 : 15;
    if (ret != 0) {
        if (flag < 0)
            b->activetime = 16;
        else
            b->activetime = flag;
        b->active |= 0x100;
        if (*(void**)&b->data[0xC] != NULL) {
            if (*(void**)((u8*)b->objgrp.node + 116) != sItemsRootNode) {
                MBNodeSetParent(b->objgrp.node, sItemsRootNode);
                UpdateObjWorldMat(&b->objgrp);
            }
            {
                Item* other = *(Item**)&b->data[0xC];
                if (flag < 0)
                    other->activetime = 16;
                else
                    other->activetime = flag;
                other->active |= 0x100;
            }
        }
    }
done:
    ;
}

/* ---- fn_800606FC externs ------------------------------------------------- */
extern s32   find_enemy_slot(s32 type, s32 level);
extern void  SetPlayerVars(void);
extern s32   MBWorldSphereVisible3(f32* position, f32 radius);
extern s32   ItemVisible(Item* item);
extern void  MBTreeClearFlags(void* node, s32 flags, s32 value);
extern s32   MBTreeGetAlpha(void* node);
extern void  MBTreeSetAlpha(void* node, s32 alpha, s32 value);
extern void  MBTreeSetAltTex(void* node, s32 tex, s32 alt, s32 value);
extern void  ExtractYPR(f32* matrix, f32* angles);
extern void CreateYPRMatrix(f32* mtx, f32* pyr);
extern f32   Random(f32 scale);
extern f64   DistanceToClosestPlayer(f32* pos);
extern s32   generate_now(Item* it, f32* pos, s32 max, s32 vis);
extern void  generate_single_80063444(Item* it, s32 algorithm, s32 important);
extern s32 generate_enemy(f32* pos, s32 kind, s32 a, f32* dir, s32 b, s32 c,
                          s32 d, f32 radius);
extern void fn_80060114(Item* item, f32* pos, f32* dir);
extern void fn_80062A00(void);
extern s32   RandInt(s32 range);
extern void  place_logic12_800631AC(u8* data);
extern s32   did_generate(void* owner, s32 checkEnemies);
extern void  add_target(void* id);
extern void  del_target(void* id);
extern void  ShakeCamera(s32 type, s32 count, s32 delay, f32 rad, s32 priority);
extern void  TriggerCameraActivate(s32 type, u8* eye, u8* target, s32 duration,
                                   s32 flags, s32 variant);
extern s32   towerAllPlayersMetLevelReq(s32 level);
extern void  TowerNeedGargItemsMsg(s32 who, s32 slot);
extern void  TowerNeedCrystalsMsg(s32 who, s32 slot);
extern s32   AudioSecretProc(f32 scale, s32 sound, f32* position, u32 flags,
                             s32* instance, s32* mask);
extern void  AudioStopAll(void);
extern void  SaveAllRecords(s32 excludedItem, s32 player, f32* playerPos,
                            u32 record);
extern void  init_got_it(void);
extern void  fn_8009D9A4(f32* pos);
extern void  fn_8009D7E4(s32 mode, f32* pos);
extern void  YawMat3(f32* matrix, f32 angle);
extern f64   __frsqrte(f64 x);
extern Enemy gEnemies[25]; /* game/enemy.h; stride 0x394 */
extern Camera gCameras[6]; /* game/camera.h; stride 0x18C */
extern s32   gFrameTicks;
extern f32   gClockFrameStep;
extern s32   gNextItemIdx;
extern s32   gTriggerCameraState;
extern s32   default_gen_count;
extern s32   lbl_803447B4;
extern s32   lbl_803447B8;
extern s32   lbl_803447D0;
extern s32   lbl_803447DC;
extern s32   lbl_803447E0;
extern s32   lbl_80344500;
extern s32   lbl_80344960;
extern u8*   lbl_80344A6C;
extern u32   lbl_80344A80;
extern s32   sNumLookoutParams;
extern s32   sMusicSubIndex;
extern s32   sMusicSubState;
extern s32   sSafeRockCount;
extern s32   sPreviousSafeRockCount;
extern f32   sMusicFadeBase;
extern f32   sItemFloorRadius;
extern f32   sItemSearchDistance;
extern f64   sArrowFloorYOffset;
extern f64   sNewtonThree;
extern f64   sPi;
extern f64   sTwoPi;
extern f64   sNegativePi;
extern f32   lbl_80343C50;
extern f32   lbl_80344880;
extern f32   lbl_80344C5C;
extern f64   lbl_80346ED8;
extern f64   lbl_80346EE8;
extern f64   lbl_80346EF0;
extern f64   lbl_80346F88;
extern f64   lbl_80346F98;
extern f64   lbl_80346FB8;
extern f32   lbl_80347000;
extern f64   lbl_80347018;
extern f64   lbl_80347040;
extern f64   lbl_80347048;
extern f32   lbl_80347050;
extern f64   lbl_80347058;
extern f64   lbl_80347060;
extern f64   lbl_80347068;
extern f64   lbl_80347070;
extern f64   lbl_80347078;
extern f64   lbl_80347080;
extern f64   lbl_80347088;
extern f64   lbl_80347090;
extern f32   lbl_80347098;
extern f64   lbl_803470A0;
extern f64   lbl_803470A8;
extern f64   lbl_803470B0;
extern f32   lbl_803470B8;
extern f32   lbl_803470BC;
extern f64   lbl_803470C0;
extern f64   lbl_803470C8;
extern f64   lbl_803470D0;
extern f64   lbl_803470D8;
extern f32   lbl_80127D00[4];
extern f32   lbl_8011C904[8];
extern char  sMissingLookoutParamFmt[];

/* typed view over gGameOptions so the option words load as base+displacement
 * (the retail world_update keeps the base in a register; flat byte-offset
 * casts get re-associated into derived pointers) */
typedef struct GameOptionsView {
    s32 unk0;     /* 0x00 */
    s32 unk4;     /* 0x04 */
    s32 unk8;     /* 0x08 */
    s32 unkC;     /* 0x0C */
    s32 unk10;    /* 0x10 */
} GameOptionsView;
#define GAMEOPTS ((GameOptionsView*)gGameOptions)

/* world_update's original source wrote these as raw literal constants, not
 * named globals: MWCC pooled them in .sdata2 and cached them in callee-saved
 * FPRs across the two item loops (target world_update saves f14-f31), which
 * extern references can never reproduce because calls may clobber globals.
 * Shadow the names with their exact pool values for this function only. */
#define sArrowFloorYOffset      0.5
#define sNewtonThree            3.0
#define sZeroDouble             0.0
#define sPi                     3.141592654
#define sTwoPi                  6.283185308
#define sNegativePi             -3.141592654
#define lbl_80346EE8            1.0
#define lbl_80346EF0            2.0
#define lbl_80346F88            30.0
#define lbl_80346F98            10.0
#define lbl_80346FB8            1.5
#define lbl_80347018            50.0
#define lbl_80347040            1.75
#define lbl_80347048            15.0
#define lbl_80347058            0.7853981635
#define lbl_80347060            40.0
#define lbl_80347068            -0.05235987756666667
#define lbl_80347070            0.05235987756666667
#define lbl_80347078            0.06981317008888889
#define lbl_80347080            -0.06981317008888889
#define lbl_80347088            0.2
#define lbl_80347090            0.8
#define lbl_803470A0            2.83
#define lbl_803470A8            9.0
#define lbl_803470B0            20.0
#define lbl_803470C0            0.17453292522222225
#define lbl_803470C8            0.017453292522222223
#define lbl_803470D0            0.3490658504444445
#define lbl_803470D8            200.0
#define sItemZero               0.0f
#define sItemFloorRadius        1.0f
#define sItemSearchDistance     10.0f
#define sCameraVisibilityRadius 2.0f
#define sArrowFloorRadius       0.2f
#define lbl_80347000            100000.0f
#define lbl_80347050            6.0f
#define lbl_80347098            0.1f
#define lbl_803470B8            15.0f
#define lbl_803470BC            100.0f

/* delete a live item and recycle its pool slot */
#define KILL_ITEM(itm)                                                       \
    do {                                                                     \
        if (*(void**)(itm)->atree != NULL) {                                 \
            AtreeDelete((itm)->atree);                                       \
            *(void**)(itm)->atree = NULL;                                    \
        }                                                                    \
        if ((itm)->objgrp.node != NULL) {                                    \
            MBRemoveNode((itm)->objgrp.node, 0);                             \
            (itm)->objgrp.node = NULL;                                       \
        }                                                                    \
        (itm)->active = -1;                                                  \
        {                                                                    \
            s32 slot_ = ((s32)((u8*)(itm) - (u8*)sItems)) / 0xF0;            \
            if (slot_ < gNextItemIdx) {                                      \
                gNextItemIdx = slot_;                                        \
            }                                                                \
        }                                                                    \
    } while (0)

#define WRAP_ANGLE(dst)                                                          do {                                                                             f64 wa_ = (dst);                                                             if (wa_ > sPi) {                                                                 wa_ = wa_ - sTwoPi;                                                      } else if (wa_ <= sNegativePi) {                                                 wa_ = sTwoPi + wa_;                                                      }                                                                            (dst) = (f32)wa_;                                                        } while (0)

/* 0x800606FC - per-frame world item/object update dispatcher */
void fn_800606FC(void)
{
    Item* it;
    s32 i;
    s32 paused;
    s32 musicState;
    s32 musicIdx;
    f32 gpos[3];
    f32 gypr[3];
    u8* rt = sItemRuntime;
    u8 unused[120];

    if (gGameMode == 0x8008) {
        paused = 0;
    } else if (gGameMode == 0x4010) {
        paused = lbl_803447B8;
    } else {
        paused = 1;
    }

    default_gen_count = 0;
    sItemRandSeed = sItemRandSeed + 1;
    find_enemy_slot(0, -1);
    musicIdx = -1;
    musicState = 0;
    if (lbl_803447B4 == 0) {
        lbl_803447D0 = 0;
    }
    if (lbl_80344768 == 0) {
        SetPlayerVars();
    }

    it = sItems;
    for (i = 0; i < sNumItems; i++, it++) {
        s32 type;
        s32 vis;
        s32 cond;
        s32 isopen;
        s16 a;

        if (it->active == -1) {
            continue;
        }
        type = it->info->type;
        if (type == -1) {
            continue;
        }
        vis = MBWorldSphereVisible3(it->objgrp.attn_pos, it->visrad);
        if (vis != 0 && lbl_80344A6C != NULL && (u32)(lbl_80344A80 - 1) <= 1) {
            f32 dy = *(f32*)(lbl_80344A6C + 0xA8) - it->objgrp.attn_pos[1];
            f32 dx = *(f32*)(lbl_80344A6C + 0xA4) - it->objgrp.attn_pos[0];
            f32 dz = *(f32*)(lbl_80344A6C + 0xAC) - it->objgrp.attn_pos[2];
            f32 d2 = dy * dy;
            d2 = dx * dx + d2;
            d2 = dz * dz + d2;
            if (d2 > lbl_80343C50) {
                vis = 0;
            }
        }
        if (paused == 0 || it->info->type == 10) {
            if (vis != 0) {
                it->active |= 0x4000;
            } else {
                it->active &= ~0x4000;
            }
        }
        cond = 0;
        if (type == 1 && it->info->item.subtype == 2) {
            cond = 1;
        }
        isopen = cond ? 1 : 0;
        {
            s8 mo = it->minoff;
            if (mo == 1 ||
                (mo == 2 &&
                 (isopen || GAMEOPTS->unk10 != 0 ||
                  GAMEOPTS->unkC != 0))) {
                if (vis == 0 || isopen || GAMEOPTS->unk10 != 0 ||
                    GAMEOPTS->unkC != 0) {
                    if (ItemVisible(it) == 0) {
                        continue;
                    }
                    it->minoff = 0;
                    MBTreeClearFlags(it->objgrp.node, 2, 0);
                } else {
                    if (paused != 0) {
                        continue;
                    }
                    it->minoff = 2;
                    continue;
                }
            } else {
                if (mo != 0) {
                    continue;
                }
                if (ItemVisible(it) == 0) {
                    if (vis == 0 || GAMEOPTS->unk10 != 0 ||
                        GAMEOPTS->unkC != 0) {
                        it->minoff = 1;
                        MBTreeSetFlags(it->objgrp.node, 2, 0);
                        continue;
                    }
                }
            }
        }
        a = it->active;
        if (!(a & 0x40) && vis == 0 && !((a & 4) && (s8)it->action > 0)) {
            if (it->info->type != 2 || (s8)it->action <= 0) {
                continue;
            }
        }
        if (it->activetime > 0) {
            it->activetime -= gFrameTicks;
        }
        a = it->active;
        if ((a & 0x100) && it->activetime <= 0) {
            KILL_ITEM(it);
            continue;
        }
        if (!(a & 1)) {
            s32 mode;
            s32 dact;
            s32 act;
            if (*(void**)it->atree == NULL) {
                continue;
            }
            if ((s8)it->action == (s8)it->daction) {
                mode = 0;
            } else {
                mode = 2;
            }
            act = (s8)it->action;
            dact = (s8)it->daction;
            if (it->info->type == 5) {
                if (act == 0 && dact == 2) {
                    dact = 1;
                } else if (act == 2 && dact == 0) {
                    dact = 3;
                }
                mode = 1;
            }
            if (AnimateATree(it->atree, dact, mode) & 3) {
                it->action = it->daction;
            }
            continue;
        }
        if (*(void**)it->atree != NULL) {
            u8* anim = it->atree + 4;
            s32 mode = 2;
            s32 t;
            s32 w;
            s32 res;
            if (it->activetime <= 0) {
                if (a & 4) {
                    if ((s8)(it->daction += 1) >= *(s16*)(anim + 0xC)) {
                        if (it->active & 2) {
                            it->daction--;
                        } else {
                            it->daction = 0;
                        }
                    }
                }
                if (it->active & 0x80) {
                    it->active &= ~1;
                }
            }
            if ((s8)it->action == (s8)it->daction) {
                mode = 0;
            }
            if (!(AnimateATree(it->atree, (s8)it->daction, mode) & 3)) {
                continue;
            }
            if (!(it->active & 0x400)) {
                if ((s8)it->daction != 0) {
                    t = it->info->item.activeon << 1;
                } else {
                    iteminfo* inf = it->info;
                    t = inf->item.activeoff << 1;
                    if (inf->type == 8) {
                        t = (s32)((f32)t * gCurLevel->trap_rate);
                    }
                }
                w = (s32)(sArrowFloorYOffset +
                          (f32)*(s16*)(anim + 0x10) * *(f32*)(anim + 0x2C))
                    << 1;
                if (it->info->type == 8 && *(s16*)(anim + 0xC) > 4 && t < 0) {
                    t = 0;
                    w = 0;
                }
                res = w;
                if (t != 0) {
                    res = t;
                    if (t < 0) {
                        res = RandInt(-t) + ((-t) >> 1);
                    }
                }
                it->activetime = res;
            }
            it->action = it->daction;
            continue;
        }
        if (it->activetime > 0) {
            continue;
        }
        if (a & 4) {
            s8 c8 = it->action;
            if (!((s8)c8 != 0 && (a & 2))) {
                s32 na;
                s32 t;
                if ((s8)c8 == 0) {
                    na = 1;
                } else {
                    na = 0;
                }
                it->action = na;
                if ((s8)it->action != 0) {
                    t = it->info->item.activeon << 1;
                } else {
                    iteminfo* inf = it->info;
                    t = inf->item.activeoff << 1;
                    if (inf->type == 8) {
                        t = (s32)((f32)t * gCurLevel->trap_rate);
                    }
                }
                if (t < 0) {
                    t = RandInt(-t) + ((-t) >> 1);
                }
                it->activetime = t;
            }
        }
        if (it->active & 0x80) {
            it->active &= ~1;
        }
    }

    lbl_80344960 = -1;
    it = sItems;
    for (i = 0; i < sNumItems; i++, it++) {
        s32 type;
        if (it->active == -1) {
            continue;
        }
        if (it->active & 0x8100) {
            continue;
        }
        fn_8005A338(&it->objgrp, it->coll_offset, it->coll_offset);
        type = it->info->type;
        if (type == -1) {
            continue;
        }
        if ((s8)it->minoff != 0) {
            continue;
        }
        if (!((type == 3 && (s8)it->data[7] == 0xF) ||
              (type == 0xC && it->info->item.subtype == 2) ||
              (it->active & 0x40) || (it->active & 0x4000))) {
            continue;
        }
        switch (type) {
        case 1:
            if (*(s16*)&it->data[0x10] > 0) {
                *(s16*)&it->data[0x10] -= gFrameTicks;
            }
            switch (it->info->item.subtype) {
            case 0xF:
                if (it->active & 0x800) {
                    s32 alpha;
                    if (sZeroDouble == lbl_80344C5C) {
                        break;
                    }
                    if (lbl_80344C5C > sZeroDouble) {
                        LookoutParam* lp = (LookoutParam*)(rt + 0xCB8);
                        s32 n = sNumLookoutParams;
                        s32 i;
                        for (i = 0; i < n; i++) {
                            if (lp->param == 0) {
                                goto lookout_found;
                            }
                            lp++;
                        }
                        ErrorPrintf(sMissingLookoutParamFmt, 0);
                        lp = NULL;
                    lookout_found:
                        if (lp != NULL) {
                            f32 fade = (f32)(lbl_80347040 +
                                             (sMusicFadeBase - lbl_80344C5C));
                            f32 d = fqdist(
                                it->objgrp.worldmat[3][0] - lbl_80127D00[0],
                                it->objgrp.worldmat[3][2] - lbl_80127D00[2]);
                            if (d > lbl_80347048 * fade) {
                                break;
                            }
                        }
                    }
                    alpha = MBTreeGetAlpha(it->objgrp.node) - 8;
                    if (alpha <= 0) {
                        alpha = 0;
                        it->active &= ~0x800;
                    }
                    MBTreeSetAlpha(it->objgrp.node, alpha, 1);
                }
                break;
            case 4:
                break;
            }
            break;
        case 3: {
            u8* gen = it->data;
            s32 max;
            s32 visflag;
            if (*(s32*)(gGameOptions + 8) <= 1) {
                break;
            }
            if (*(s32*)(gGameOptions + 8) == 2) {
                break;
            }
            if ((s8)gen[6] <= 0) {
                break;
            }
            if (*(s16*)&gen[0] == -1) {
                break;
            }
            visflag = it->active & 0x4000;
            max = (s8)gen[3];
            if ((s8)gen[7] == 0xF) {
                generate_single_80063444(it, 0xF, 0);
                break;
            }
            if ((s8)gen[2] >= max) {
                break;
            }
            gpos[0] = it->objgrp.coll_pos[0];
            gpos[1] = it->objgrp.coll_pos[1];
            gpos[2] = it->objgrp.coll_pos[2];
            if (generate_now(it, gpos, max, visflag) != 0) {
                s32 slot;
                s32 imp;
                f32 rad = it->info->item.height;
                gypr[0] = it->objgrp.worldmat[2][0];
                gypr[1] = it->objgrp.worldmat[2][1];
                gypr[2] = it->objgrp.worldmat[2][2];
                if ((s8)gen[7] == 0xC && (s8)gen[4] >= 3) {
                    gen[4] -= 3;
                    gen[0xA] = 0;
                }
                if (visflag != 0) {
                    imp = 0;
                } else {
                    imp = -1;
                }
                slot = generate_enemy(gpos, *(s16*)&gen[0], (s8)gen[6], gypr,
                                      (s8)gen[7], (s32)it, imp, rad);
                if (slot < 0) {
                    break;
                }
                {
                    u8* e = (u8*)gEnemies + slot * 0x394;
                    s16 wob;
                    f32 fa = sItemFloorRadius + *(f32*)&it->data[0xC];
                    f32 rate = sItemFloorRadius /
                               (f32)(sCameraVisibilityRadius * (f32)max);
                    wob = (s16)((f32)(lbl_80347050 * (f32)(u8)gen[0xB]) * fa);
                    *(s16*)&it->data[8] = wob;
                    *(f32*)&it->data[0xC] =
                        *(f32*)&it->data[0xC] + rate;
                    if (*(f32*)&it->data[0xC] > sItemFloorRadius) {
                        *(f32*)&it->data[0xC] = sItemZero;
                    }
                    *(s16*)(e + 0x2D8) = 0;
                    *(f32*)(e + 0x24C) =
                        *(f32*)&it->data[0x10] + *(f32*)(e + 0x258);
                    WRAP_ANGLE(*(f32*)(e + 0x24C));
                    *(f32*)(e + 0x250) = *(f32*)(e + 0x24C);
                    *(f32*)(e + 0x2EC) = *(f32*)(e + 0x34);
                    *(f32*)(e + 0x2F0) = *(f32*)(e + 0x38);
                    *(f32*)(e + 0x2F4) = *(f32*)(e + 0x3C);
                    *(f32*)(e + 0x240) = sItemZero;
                    *(f32*)(e + 0x244) = *(f32*)(e + 0x24C);
                    *(f32*)(e + 0x248) = sItemZero;
                    if ((s8)gen[7] == 0xC) {
                        place_logic12_800631AC(gen);
                        break;
                    }
                    if ((s8)gen[7] == 0xD) {
                        if ((s8)gen[5] < 0) {
                            *(s32*)(e + 0x334) = -1;
                        } else {
                            u8* prev = (u8*)gEnemies + (s8)gen[5] * 0x394;
                            *(s32*)(prev + 0x338) = slot;
                            *(s32*)(e + 0x334) = (s8)gen[5];
                        }
                        *(s32*)(e + 0x338) = -1;
                        gen[5] = (s8)slot;
                        gen[2]++;
                        gen[4]++;
                        break;
                    }
                    if ((s8)gen[7] == 0xE) {
                        if (gen[4] & 1) {
                            *(f32*)(e + 0x24C) =
                                (f32)(*(f32*)(e + 0x24C) - lbl_80347058);
                            *(s32*)(e + 0x31C) = -1;
                        } else {
                            *(f32*)(e + 0x24C) =
                                (f32)(*(f32*)(e + 0x24C) + lbl_80347058);
                            *(s32*)(e + 0x31C) = 1;
                        }
                        WRAP_ANGLE(*(f32*)(e + 0x24C));
                        *(f32*)(e + 0x250) = *(f32*)(e + 0x24C);
                        gen[2]++;
                        gen[4]++;
                        break;
                    }
                    gen[2]++;
                    gen[4]++;
                }
            }
            break;
        }
        case 8:
            if (lbl_803447DC != 0) {
                it->action = 0;
                it->daction = 0;
                *(s16*)(it->atree + 0x12) = 0;
                it->activetime = 0x1E;
            } else if ((s8)it->paction == 0 && (s8)it->action == 1 &&
                       it->info->item.subtype == 4) {
                f32 dy = gCameras[0].attn[1] - it->objgrp.worldmat[3][1];
                f32 dx = gCameras[0].attn[0] - it->objgrp.worldmat[3][0];
                f32 dz = gCameras[0].attn[2] - it->objgrp.worldmat[3][2];
                f32 d2 = dy * dy;
                f32 root;
                d2 = dx * dx + d2;
                d2 = dz * dz + d2;
                if (d2 > sItemZero) {
                    f64 estimate = __frsqrte(d2);
                    estimate = sArrowFloorYOffset * estimate *
                               (sNewtonThree - estimate * estimate * d2);
                    estimate = sArrowFloorYOffset * estimate *
                               (sNewtonThree - estimate * estimate * d2);
                    estimate = sArrowFloorYOffset * estimate *
                               (sNewtonThree - estimate * estimate * d2);
                    root = (f32)(d2 *
                                 (sArrowFloorYOffset * estimate *
                                  (sNewtonThree - estimate * estimate * d2)));
                    d2 = root;
                }
                if (d2 < lbl_80347060) {
                    fn_8009D9A4(it->objgrp.worldmat[3]);
                }
            }
            it->paction = it->action;
            break;
        case 2: {
            u8* link;
            if ((s8)it->action == 0 && *(s16*)&it->data[2] > 0) {
                f32 wob[3];
                s16 t;
                wob[0] = sItemZero;
                wob[1] = *(f32*)&it->data[4];
                wob[2] = sItemZero;
                t = *(s16*)&it->data[2] - gFrameTicks;
                *(s16*)&it->data[2] = t;
                if (t <= 0) {
                    *(s16*)&it->data[2] = 0;
                } else {
                    f64 step;
                    if ((*(s16*)&it->data[2] + 4) & 8) {
                        step = lbl_80347068;
                    } else {
                        step = lbl_80347070;
                    }
                    wob[0] = (f32)(wob[0] + step);
                    WRAP_ANGLE(wob[0]);
                    if (*(s16*)&it->data[2] & 8) {
                        step = lbl_80347078;
                    } else {
                        step = lbl_80347080;
                    }
                    wob[1] = (f32)(wob[1] + step);
                    WRAP_ANGLE(wob[1]);
                }
                CreateYPRMatrix(it->objgrp.worldmat[0], wob);
                UpdateObjWorldMat(&it->objgrp);
            }
            link = *(u8**)&it->data[0xC];
            if (link != NULL && *(void**)it->atree != NULL &&
                it->info->item.subtype != 0x2C) {
                void* node2 = *(void**)(link + 0x64);
                if ((s8)it->action < 2) {
                    u8* anim = it->atree + 4;
                    f32 al;
                    if ((s8)it->action == 0) {
                        al = sArrowFloorRadius;
                    } else if (*(s16*)(anim + 0x10) > 1) {
                        al = (f32)(lbl_80347090 *
                                       ((lbl_80346EE8 + *(f32*)(anim + 0x18)) /
                                        (f64)*(s16*)(anim + 0x10)) +
                                   lbl_80347088);
                    } else {
                        al = sItemFloorRadius;
                    }
                    MBTreeSetFlags(node2, 8, 0);
                    *(f32*)((u8*)node2 + 0x40) = al;
                    *(f32*)((u8*)node2 + 0x44) = al;
                    *(f32*)((u8*)node2 + 0x48) = al;
                } else {
                    MBTreeClearFlags(node2, 8, 0);
                }
            }
            if ((s8)it->action == 2) {
                s32 sub2 = it->info->item.subtype;
                if (sub2 == 0x2C) {
                    gpos[0] = it->objgrp.coll_pos[0];
                    gpos[1] = it->objgrp.coll_pos[1];
                    gpos[2] = it->objgrp.coll_pos[2];
                    StartExplosion(&it->objgrp, 0x1D,
                                   (f32)(lbl_80347018 *
                                         gCurLevel->trap_damage));
                    fn_8009D9D8(gpos);
                    KILL_ITEM(it);
                    msgPost(0x89, -1, 0);
                } else if (*(u8**)&it->data[0xC] == NULL && sub2 != 0x2B &&
                           sub2 != 0x2F) {
                    KILL_ITEM(it);
                }
                {
                    s32 n;
                    for (n = 0; n < 25; n++) {
                        u8* e = (u8*)gEnemies + n * 0x394;
                        s32* genid = (s32*)(e + 0x340);
                        if (n == *genid) {
                            *(s32*)(e + 0x33C) = 0;
                            *genid = -1;
                            *(f32*)(e + 0x344) = lbl_80347000;
                        }
                    }
                }
            }
            break;
        }
        case 4:
            fn_80060114(it, gpos, gypr);
            break;
        case 5: {
            u8* tgt;
            s16 flags;
            s32 mask;
            s32 pdact;
            s32 subtype;
            mask = 0;
            pdact = it->daction;
            subtype = it->info->item.subtype;
            if (it->active & 0x400) {
                break;
            }
            if (*(s16*)&it->data[0x10] > 0) {
                *(s16*)&it->data[0x10] -= gFrameTicks;
            }
            flags = *(s16*)&it->data[4];
            tgt = *(u8**)&it->data[0];
            if (flags & 0x40) {
                Item* p2;
                s32 lvl = (s8)it->data[6];
                if (lvl < 100) {
                    if (it->playermask != 0 &&
                        towerAllPlayersMetBossReq(lvl) == 0) {
                        TowerNeedCrystalsMsg(it->playermask,
                                             (s8)it->data[6]);
                        it->playermask = 0;
                    }
                } else {
                    s32 garg = lvl - 0x65;
                    if (it->playermask != 0 &&
                        towerAllPlayersMetLevelReq(garg) == 0) {
                        if (garg < 3) {
                            TowerNeedGargItemsMsg(it->playermask, garg);
                        }
                        it->playermask = 0;
                    }
                }
                for (p2 = it; p2 != NULL; p2 = *(Item**)&p2->data[8]) {
                    p2->playermask = it->playermask;
                }
            }
            if (it->playermask != 0) {
                if (*(s16*)&it->data[4] & 0x100) {
                    u32 m = 0xFFFFFFF0;
                    s32 b;
                    for (b = 0; b < 4; b++) {
                        Player* p = &gPlayers[b];
                        if (p->state == 1 &&
                            (p->floor_name2 == (WorldObj*)tgt ||
                             (p->floor_name2 != NULL &&
                              *(u8**)((u8*)p->floor_name2 + 0x18) == tgt))) {
                            m |= 1 << b;
                        }
                    }
                    it->playermask &= m;
                }
                mask = it->playermask;
            }
            flags = *(s16*)&it->data[4];
            if (flags & 0x400) {
                if (mask != lbl_803447E0) {
                    it->playermask = 0;
                    mask = 0;
                    if (tgt != NULL) {
                        *(u32*)(tgt + 0x10) &= ~0x4000000;
                    }
                } else {
                    if (tgt != NULL) {
                        *(u32*)(tgt + 0x10) |= 0x4000000;
                    }
                }
            }
            if (tgt != NULL) {
                if (flags & 1) {
                    if (mask != 0) {
                        if (((s8)tgt[0x16] & ~0xF) == 0x20) {
                            tgt[0x16] &= 0xF;
                        }
                        it->daction = 2;
                    } else {
                        s32 d;
                        if (tgt[0x16] & 0x20) {
                            d = 0;
                        } else {
                            d = 2;
                        }
                        it->daction = d;
                    }
                } else if (flags & 2) {
                    if (mask != 0) {
                        if (((s8)tgt[0x16] & ~0xF) == 0) {
                            tgt[0x16] &= 0xF;
                            tgt[0x16] |= 0x20;
                        }
                        it->daction = 2;
                    } else {
                        s32 d;
                        if (tgt[0x16] & 0x20) {
                            d = 2;
                        } else {
                            d = 0;
                        }
                        it->daction = d;
                    }
                    if (subtype == 0x17 && !(gGameMode & 0x8000)) {
                        msgPost(5, -1, (char*)it->objgrp.attn_pos);
                    }
                } else if (flags & 4) {
                    if (mask != 0) {
                        s32 doset = 1;
                        s32 low = (s8)tgt[0x16] & 0xF;
                        if (((s8)tgt[0x16] & 0x10) == 0) {
                            if (low < (low | mask)) {
                                tgt[0x16] &= ~0xF;
                                tgt[0x16] |= mask;
                            }
                            if (*(s16*)&it->data[0x10] <= 0) {
                                tgt[0x16] ^= 0x20;
                                tgt[0x16] &= ~0xF;
                                tgt[0x16] |= mask;
                            } else {
                                doset = 0;
                            }
                            if (doset != 0) {
                                *(s16*)&it->data[0x10] = 0x78;
                            }
                        } else {
                            *(s16*)&it->data[0x10] = 0x78;
                        }
                        it->daction = 2;
                    } else {
                        it->daction = 0;
                        if (flags & 0x800) {
                            *(s16*)&it->data[0x10] =
                                (lbl_80344768 - 1) * 0x3C;
                        }
                    }
                } else {
                    if (mask != 0) {
                        tgt[0x16] = (s8)mask;
                        tgt[0x16] |= 0x20;
                        it->daction = 2;
                    } else {
                        s32 d;
                        if (tgt[0x16] & 0x20) {
                            d = 2;
                        } else {
                            d = 0;
                        }
                        it->daction = d;
                    }
                }
            } else {
                if (flags & 4) {
                    s32 d;
                    if (mask != 0) {
                        d = 2;
                    } else {
                        d = 0;
                    }
                    it->daction = d;
                } else if (mask != 0) {
                    it->daction = 2;
                }
            }
            if ((s8)it->daction != 0 && pdact == 0) {
                if (flags & 0x1000) {
                    ShakeCamera(0, 0, 0xB4, lbl_80347098, 0x64);
                }
                if (flags & 0x2000) {
                    f32 d;
                    Item* hit = fn_80062FF0(sItemSearchDistance,
                                            it->objgrp.worldmat[3], 4, &d, 0);
                    if (hit != NULL && d < lbl_80346F98) {
                        *(u32*)&hit->data[8] |= 1;
                    }
                }
                if (*(s16*)&it->data[0x12] >= 0 && mask != 0) {
                    u8* cam = rt + *(s16*)&it->data[0x12] * 0x28;
                    TriggerCameraActivate((s32)tgt, cam + 0x1618, cam + 0x1628,
                                          cam[0x1615], 0x1E, 0);
                }
            }
            *(s16*)&it->atree[0x38] = 0;
            if (gTriggerCameraState == 0 &&
                !(*(s16*)&it->data[4] & 0xC0)) {
                u8 pm = it->playermask;
                if (pm & 0xF) {
                    it->playermask = pm & ~0xF;
                } else {
                    it->playermask = 0;
                }
            }
            break;
        }
        case 0xC: {
            s32 sub = it->info->item.subtype;
            u8* tgt = *(u8**)&it->data[0];
            if (sub == 0) {
                f32 ang;
                if (tgt == NULL) {
                    break;
                }
                if (lbl_80344500 != 0 && (*(u32*)(tgt + 0x10) & 4)) {
                    break;
                }
                ang = *(f32*)&it->data[4] * (f32)(u32)gFrameTicks;
                it->daction = 2;
                if (*(u8**)(tgt + 0x28) == NULL) {
                    break;
                }
                *(u32*)(tgt + 0x10) &= ~1;
                YawMat3(*(f32**)(tgt + 0x28), ang);
            } else if (sub == 2) {
                if (tgt == NULL) {
                    break;
                }
                if (!(it->active & 1)) {
                    break;
                }
                if (it->active & 0x1000) {
                    break;
                }
                if ((*(f32*)&it->data[4] >= sItemZero &&
                     *(f32*)&it->data[0xC] >= *(f32*)&it->data[8]) ||
                    (*(f32*)&it->data[4] < sItemZero &&
                     *(f32*)&it->data[0xC] <= -*(f32*)&it->data[8])) {
                    fn_8009D7E4(2, (f32*)(*(u8**)(tgt + 0x28) + 0x30));
                    it->active |= 0x1000;
                    it->daction = 2;
                } else {
                    f32 ang;
                    fn_8009D7E4(0, (f32*)(*(u8**)(tgt + 0x28) + 0x30));
                    ang = *(f32*)&it->data[4] * (f32)(u32)gFrameTicks;
                    *(f32*)&it->data[0xC] = *(f32*)&it->data[0xC] + ang;
                    it->daction = 2;
                    if (*(u8**)(tgt + 0x28) != NULL) {
                        *(u32*)(tgt + 0x10) &= ~1;
                        YawMat3(*(f32**)(tgt + 0x28), ang);
                    }
                }
            } else if (sub == 1 && tgt != NULL) {
                if (lbl_80344768 > 1 && did_generate(tgt, 0) != 0) {
                    add_target(&it->objgrp);
                    lbl_80344960 = i;
                } else {
                    del_target(&it->objgrp);
                }
            }
            break;
        }
        case 9: {
            s32 pmask = 0;
            s32 count5 = 0;
            if (it->info->item.subtype == 0x32) {
                s32 b;
                s32 pbits = *(s32*)&it->data[4];
                if (pbits == 0) {
                    break;
                }
                if (lbl_8034481C >= 3) {
                    break;
                }
                if (it->active & 1) {
                    break;
                }
                for (b = 0; b < 4; b++) {
                    if (pbits & (1 << b)) {
                        break;
                    }
                }
                it->active |= 1;
                lbl_8034481C = (u8)*(s16*)&it->data[0] + 3;
                SaveAllRecords(i, b, gPlayers[b].pos,
                               (u8)*(s16*)&it->data[0]);
                init_got_it();
                break;
            } else {
                s32 b;
                for (b = 0; b < 4; b++) {
                    s32 st = gPlayers[b].state;
                    if (st == 1 || (u32)(st - 4) <= 1) {
                        pmask |= 1 << b;
                    }
                }
                it->active |= 1;
                it->active &= ~0x400;
                for (b = 0; b < 4; b++) {
                    if (gPlayers[b].state == 5) {
                        count5++;
                    }
                }
                if (count5 != 0) {
                    lbl_803447D0 = 0xF;
                } else if (pmask != 0 && *(s32*)&it->data[4] == pmask) {
                    if ((s8)it->daction == 0) {
                        it->activetime = 0;
                    }
                    if (it->activetime <= 0 && (s8)it->daction < 4) {
                        it->daction++;
                    }
                    if ((s8)it->action < 4) {
                        *(s32*)&it->data[4] = 0;
                    }
                    if ((s8)it->action == 4 && it->activetime <= 0) {
                        s32 n4 = 0;
                        for (b = 0; b < 4; b++) {
                            if (gPlayers[b].state == 4) {
                                break;
                            }
                            n4++;
                        }
                        if (n4 >= 4) {
                            lbl_803447D0 = 0xF;
                        } else {
                            lbl_803447D0 = 0xE;
                        }
                    } else {
                        lbl_803447D0 = (s8)it->action + 10;
                    }
                } else if (*(s32*)&it->data[4] != 0) {
                    if (it->activetime <= 0) {
                        if ((s8)it->daction < 3) {
                            it->daction++;
                        } else if ((s8)it->daction > 3) {
                            it->daction = 0;
                        }
                    }
                    *(s32*)&it->data[4] = 0;
                    if ((s8)it->action > lbl_803447D0) {
                        lbl_803447D0 = (s8)it->action;
                    }
                    it->paction = 2;
                } else {
                    if ((s8)it->action > 0) {
                        if (it->activetime <= 0) {
                            if ((s8)it->daction < 4) {
                                it->daction++;
                            } else {
                                it->daction = 0;
                            }
                        } else if ((s8)it->action == 3) {
                            it->activetime = 0;
                            it->daction = 4;
                            it->action = 4;
                        }
                    }
                    *(s32*)&it->data[4] = 0;
                    if ((s8)it->action > lbl_803447D0) {
                        lbl_803447D0 = (s8)it->action;
                    }
                    it->paction = 0;
                }
                if ((s8)it->action == 3) {
                    if ((s8)it->paction == 2) {
                        it->activetime = 0x2D;
                    }
                    it->active |= 0x400;
                }
                if ((s8)it->daction == 3 || (s8)it->daction <= 1) {
                    *(s16*)&it->atree[0x38] = 1;
                } else {
                    *(s16*)&it->atree[0x38] = 0;
                }
                it->paction = it->action;
            }
            break;
        }
        case 0xD: {
            f32 range = *(f32*)&it->data[0];
            f64 d;
            s16 sub;
            if (*(void**)&it->data[8] != NULL) {
                GetWorldMat(*(void**)&it->data[8],
                            it->objgrp.worldmat[0], 0);
            }
            d = DistanceToClosestPlayer(it->objgrp.worldmat[3]);
            sub = *(s16*)&it->data[0xC];
            if (sub > 0) {
                if (d < range) {
                    if (sub - 1 > musicIdx) {
                        musicState = *(s16*)&it->data[0x10];
                        musicIdx = sub - 1;
                    }
                }
                break;
            }
            {
                f32 vol;
                if (d < range || range <= lbl_80346EF0) {
                    vol = sItemFloorRadius;
                } else {
                    vol = (f32)((lbl_80346EF0 *
                                 (lbl_80346FB8 * range - d)) /
                                range);
                }
                if (vol > sItemZero || *(s16*)&it->data[0xE] < 0) {
                    s32 vmask = 0;
                    s32 vinst = 0;
                    s32 sw = *(s16*)&it->data[0xE];
                    if (sw < 0) {
                        vinst = -sw;
                    } else if (sw != 0) {
                        vmask = sw;
                    }
                    AudioSecretProc(vol, *(s32*)&it->data[4],
                                    it->objgrp.worldmat[3],
                                    *(s16*)&it->data[0x10], &vinst, &vmask);
                    if (vmask != 0) {
                        *(s16*)&it->data[0xE] = vmask;
                    } else if (vinst != 0) {
                        *(s16*)&it->data[0xE] = (s16)-vinst;
                    } else {
                        *(s16*)&it->data[0xE] = 0;
                    }
                } else if (*(s16*)&it->data[0xE] > 0) {
                    AudioStopAll();
                    *(s16*)&it->data[0xE] = 0;
                }
            }
            break;
        }
        case 0xA: {
            s16 c = *(s16*)&it->data[4];
            if (c > 1) {
                if (*(s16*)&it->data[6] < 0x1E) {
                    MBTreeSetAltTex(it->objgrp.node, -2,
                                    gWorldInfo.whitetex, 0);
                    MBTreeSetFlags(it->objgrp.node, 0x4000, 1);
                } else {
                    MBTreeSetAltTex(it->objgrp.node, -1, 0, 0);
                    MBTreeClearFlags(it->objgrp.node, 0x4000, 1);
                }
                *(s16*)&it->data[6] += gFrameTicks;
                if (*(s16*)&it->data[6] > 0x3C) {
                    *(s16*)&it->data[6] = 0;
                    *(s16*)&it->data[4] -= 1;
                    if (*(s16*)&it->data[4] == 1) {
                        *(s16*)&it->data[4] = 0;
                    }
                }
            } else if (c == 1) {
                MBTreeSetAltTex(it->objgrp.node, -2,
                                gWorldInfo.whitetex, 0);
                MBTreeSetFlags(it->objgrp.node, 0x4000, 1);
                *(s16*)&it->data[4] = 0;
                *(s16*)&it->data[6] = 0;
            } else {
                MBTreeSetAltTex(it->objgrp.node, -1, 0, 0);
                MBTreeClearFlags(it->objgrp.node, 0x4000, 1);
            }
            switch (it->info->item.subtype) {
            case 0x2C:
            case 0x2D:
                if ((s8)it->action == 1) {
                    if (it->activetime < 0x68) {
                        s32 alpha =
                            0x100 -
                            (s32)(lbl_803470A0 *
                                  (f64)(it->activetime - 0x10));
                        if (alpha > 0xFF) {
                            alpha = 0xFF;
                        }
                        MBTreeSetAlpha(it->objgrp.node, alpha, 1);
                    }
                } else if ((s8)it->action == 2) {
                    f32 best = lbl_80347000;
                    Player* p = gPlayers;
                    s32 b;
                    for (b = 0; b < 4; b++, p++) {
                        if (p->state == 1) {
                            f32 dy = it->objgrp.coll_pos[1] - p->pos[1];
                            f32 dx = it->objgrp.coll_pos[0] - p->pos[0];
                            f32 dz = it->objgrp.coll_pos[2] - p->pos[2];
                            f32 d2 = dy * dy;
                            f32 root;
                            d2 = dx * dx + d2;
                            d2 = dz * dz + d2;
                            if (d2 > sItemZero) {
                                f64 estimate = __frsqrte(d2);
                                estimate =
                                    sArrowFloorYOffset * estimate *
                                    (sNewtonThree - estimate * estimate * d2);
                                estimate =
                                    sArrowFloorYOffset * estimate *
                                    (sNewtonThree - estimate * estimate * d2);
                                estimate =
                                    sArrowFloorYOffset * estimate *
                                    (sNewtonThree - estimate * estimate * d2);
                                root = (f32)(d2 * (sArrowFloorYOffset *
                                                   estimate *
                                                   (sNewtonThree -
                                                    estimate * estimate * d2)));
                                d2 = root;
                            }
                            if (d2 < best) {
                                best = d2;
                            }
                        }
                    }
                    if (best <= lbl_803470A8) {
                        s32 msg;
                        if (it->info->item.subtype == 0x2C) {
                            msg = 0x2C;
                        } else {
                            msg = 0x2D;
                        }
                        msgPost(msg, -1, (char*)it->objgrp.attn_pos);
                    }
                    KILL_ITEM(it);
                }
                break;
            case 0x33:
                if (sSafeRockCount != sPreviousSafeRockCount) {
                    switch (sSafeRockCount) {
                    case 1:
                        *(f32*)&it->data[0xC] =
                            (f32)(lbl_803470B0 + Random(sItemSearchDistance));
                        break;
                    case 2:
                        *(f32*)&it->data[0xC] =
                            (f32)(lbl_80346F88 + Random(lbl_803470B8));
                        break;
                    case 3: {
                        f32 dir[3];
                        dir[0] = it->objgrp.worldmat[3][0] -
                                 ((f32*)rt)[7546];
                        dir[1] = it->objgrp.worldmat[3][1] -
                                 ((f32*)rt)[7547];
                        dir[2] = it->objgrp.worldmat[3][2] -
                                 ((f32*)rt)[7548];
                        NormalVector2D(dir);
                        *(f32*)&it->data[8] = (f32)(lbl_80346F98 * dir[0]);
                        *(f32*)&it->data[0xC] = (f32)(lbl_80346F98 * dir[1]);
                        *(f32*)&it->data[0x10] = (f32)(lbl_80346F98 * dir[2]);
                        *(f32*)&it->data[0xC] =
                            (f32)(lbl_80347018 + Random(lbl_803470BC));
                        break;
                    }
                    }
                    it->active |= 1;
                }
                /* fallthrough */
            case 0x28:
            case 0x31:
            case 0x34:
            case 0x35: {
                f32 s2 = lbl_8011C904[i & 7];
                f32 s1 = lbl_8011C904[~i & 7];
                f32 ypr[3];
                if (!(it->active & 1)) {
                    break;
                }
                ExtractYPR(it->objgrp.worldmat[0], ypr);
                if (it->info->item.subtype == 0x31) {
                    *(f32*)&it->data[0xC] =
                        (f32)(*(f32*)&it->data[0xC] - lbl_80346EE8);
                    ypr[0] = (f32)(lbl_803470C0 * s2 * gClockFrameStep +
                                   ypr[0]);
                    ypr[2] = (f32)(lbl_803470C0 * s1 * gClockFrameStep +
                                   ypr[2]);
                } else if (it->info->item.subtype == 0x35) {
                    *(f32*)&it->data[0xC] =
                        (f32)(*(f32*)&it->data[0xC] - lbl_80346EF0);
                    ypr[0] = (f32)(lbl_803470C8 * s2 * gClockFrameStep +
                                   ypr[0]);
                    ypr[2] = (f32)(lbl_803470C8 * s1 * gClockFrameStep +
                                   ypr[2]);
                } else {
                    *(f32*)&it->data[0xC] =
                        (f32)(*(f32*)&it->data[0xC] - lbl_80346EF0);
                    ypr[0] = (f32)(lbl_803470D0 * s2 * gClockFrameStep +
                                   ypr[0]);
                    ypr[2] = (f32)(lbl_803470D0 * s1 * gClockFrameStep +
                                   ypr[2]);
                }
                CreateYPRMatrix(it->objgrp.worldmat[0], ypr);
                it->objgrp.worldmat[3][0] +=
                    *(f32*)&it->data[8] * gClockFrameStep;
                it->objgrp.worldmat[3][1] +=
                    *(f32*)&it->data[0xC] * gClockFrameStep;
                it->objgrp.worldmat[3][2] +=
                    *(f32*)&it->data[0x10] * gClockFrameStep;
                UpdateObjWorldMat(&it->objgrp);
                if (it->objgrp.worldmat[3][1] <
                    lbl_80344880 - lbl_803470D8) {
                    KILL_ITEM(it);
                }
                break;
            }
            }
            break;
        }
        }
    }

    if (musicIdx >= 0 && musicIdx != sMusicSubIndex) {
        sMusicSubIndex = musicIdx;
        sMusicSubState = musicState;
    }
    fn_80062A00();
    sPreviousSafeRockCount = sSafeRockCount;
}

#undef sArrowFloorYOffset
#undef sNewtonThree
#undef sZeroDouble
#undef sPi
#undef sTwoPi
#undef sNegativePi
#undef lbl_80346EE8
#undef lbl_80346EF0
#undef lbl_80346F88
#undef lbl_80346F98
#undef lbl_80346FB8
#undef lbl_80347018
#undef lbl_80347040
#undef lbl_80347048
#undef lbl_80347058
#undef lbl_80347060
#undef lbl_80347068
#undef lbl_80347070
#undef lbl_80347078
#undef lbl_80347080
#undef lbl_80347088
#undef lbl_80347090
#undef lbl_803470A0
#undef lbl_803470A8
#undef lbl_803470B0
#undef lbl_803470C0
#undef lbl_803470C8
#undef lbl_803470D0
#undef lbl_803470D8
#undef sItemZero
#undef sItemFloorRadius
#undef sItemSearchDistance
#undef sCameraVisibilityRadius
#undef sArrowFloorRadius
#undef lbl_80347000
#undef lbl_80347050
#undef lbl_80347098
#undef lbl_803470B8
#undef lbl_803470BC

/* ==========================================================================
 * FUNCTION INVENTORY  (all 45 functions in 0x80058078-0x800631AC)
 * addr        size    role / evidence (real callees; "strings")
 * --------------------------------------------------------------------------
 * 80058078  0x1C3C  ResolveWorldData  [named]  MBSetupWad, MBGetFromWad,
 *                    ResolveWorldDataPointers, FatalErrorf
 * 80059CB4  0x03E4  ResolveWorldDataPointers  [named, local]  AudioFindSound,
 *                    sprintf, FatalErrorf
 * 8005A098  0x0154  LoadWorldData  [named]  FileExists, FileSize, AllocMem,
 *                    MLMReadFile, ErrorPrintf, sprintf, fn_80057F44;
 *                    "No world data file: %s", "%s.wad", "wdata"
 * 8005A1EC  0x0074  fn_ : load "anim" WAD file (FileExists/FileSize/AllocMem,
 *                    MBOX_AllocModel); returns handle.  "anim"
 * 8005A260  0x00D8  fn_ : load a level asset file ("anim"/"static"), AllocFile,
 *                    InitTexMods, strcmp, fn_8001267C, MBOX_LoadModel.  Widely
 *                    called (attract/main/auxscreen/items).  "anim","static"
 * 8005A338  0x0080  fn_ : dispatch transform (fn_8005A588, fn_8005A65C,
 *                    GetWorldMat, CopyMat4)
 * 8005A3B8  0x004C  fn_ : release/unload world object (UnparentMatrix,
 *                    CopyMat4).  Called by auxscreen.
 * 8005A404  0x0184  fn_ : matrix/vector transform helper (WorldVector)
 * 8005A588  0x00D4  fn_ : matrix/vector transform helper (WorldVector)
 * 8005A65C  0x00D4  fn_ : place-offset transform: copies/adds a vec3 to a
 *                    dst, or transforms via WorldVector by a mode flag
 * 8005A730  0x0008  fn_ : stub, returns 1
 * 8005A738  0x0130  fn_ : per-instance text draw (DrawTextKeepScale,
 *                    fn_8005A868, RandInt, strcpy)
 * 8005A868  0x03A8  fn_ : text/gfx draw (DrawTextKeepScale, fn_8003xxxx mtx,
 *                    AudioCursorH/58, AudioClick2)
 * 8005AC10  0x00D0  fn_ : accessor (no calls; gControllerButtons/8034477C)
 * 8005ACE0  0x02B8  fn_ : DEBUG draw of world-object info (DrawText, sprintf,
 *                    get_screen_pos).  "ROT:","BRID:","DOOR:","ELEV:","LIFT:",
 *                    "TRIG:","%s(%d)","%s:%s:%d(%d)"
 * 8005AF98  0x0200  fn_ : recursive name lookup in a tree (self, strcmp)
 * 8005B198  0x00DC  fn_ : iterate enemy grid (StartEnemyGrid/NextGridEnemy,
 *                    NormalVector)
 * 8005B274  0x02E4  fn_ : iterate enemy grid + object op (StartEnemyGrid/
 *                    NextGridEnemy, fqdist, NormalVector)
 * 8005B558  0x0060  fn_ : accessor (sNumItems/80344950)
 * 8005B5B8  0x02F8  fn_ : spawn/attach world object (FindWORLDOBJ, ErrorPrintf,
 *                    WorldOpen/1D00/2698, MBOX_NewObject, MBTreeSetFlags/AEAC,
 *                    sprintf)
 * 8005B8B0  0x004C  fn_ : small accessor (no calls)
 * 8005B8FC  0x008C  fn_ : post message (msgPost; gGameMode/8034481C)
 * 8005B988  0x0094  fn_ : run texmods for a world (DoSpecialTexmods, DoTexMods,
 *                    fn_800606FC, SetupPlayerTexMods)
 * 8005BA1C  0x07C0  fn_ : object/enemy setup by name (AtreeMatch, msgPost,
 *                    strcmp, fn_8005C1DC, AtreeMatchAnyHeader, fn_8009190C)
 * 8005C1DC  0x0E70  fn_ : big item/object spawn dispatcher (AtreeMatch,
 *                    AudioPlayEvt101, MBSetObject, many fn_8009xxxx).
 *                    "CHICKEN","APPLE","CHESTG3","CHESTG1","BADMEAT","GAPPLE",
 *                    "BOSSGEN","L1","ROOT","BAREXP0","BARPOI0","%s_D"
 * 8005D04C  0x0078  fn_ : wrapper -> fn_8005D0C4
 * 8005D0C4  0x0148  fn_ : enemy-grid op (StartEnemyGrid/NextGridEnemy,
 *                    MBTreeSetFlags, fqdist)
 * 8005D20C  0x01CC  fn_ : dispatch (fn_8005D3D8, fn_8005F0F4, fn_80062FF0)
 * 8005D3D8  0x01F0  fn_ : (damage_enemy; sNewtonThree)
 * 8005D5C8  0x0168  fn_ : accessor (no calls; sNewtonThree)
 * 8005D730  0x0720  fn_ : per-object update (fn_8005E90C, fn_8007xxxx anim,
 *                    fn_8009Dxxxx, msgPost, strcmp)
 * 8005DE50  0x0ABC  fn_ : big object state machine (FindStringMessageListSub,
 *                    UpdateObjWorldMat, many fn_8009/800A, msgPost, strcmp)
 * 8005E90C  0x0438  fn_ : create item instance (NewItemPtr_800642C8, SetItem,
 *                    AddItemSub, fn_8009DAF8, MBTreeSetFlags).  "AllCoins",
 *                    "PINEAPPLE"
 * 8005ED44  0x00D4  fn_ : enemy-grid op (StartEnemyGrid/NextGridEnemy,
 *                    fn_8005EE18, fn_8005F0F4)
 * 8005EE18  0x0194  fn_ : (fn_8009D91C)
 * 8005EFAC  0x0148  fn_ : enemy-grid op (StartEnemyGrid/NextGridEnemy,
 *                    fn_8005F0F4)
 * 8005F0F4  0x0A54  fn_ : big per-object worker (fn_8005FDA8, towerAllPlayersMetBossReq,
 *                    fqdist, NormalVector2D)
 * 8005FB48  0x0260  fn_ : (LineCylinderCollide, fn_8005FDA8, fqdist)
 * 8005FDA8  0x01B8  fn_ : (FatalError, CTriListCollide, MulBodyVecMat4/DD00/DE80).
 *                    "COL_OBJECT Item: idx < 0"
 * 8005FF60  0x01B4  fn_ : (ErrorPrintf, FindWobjWanim).
 *                    "Special trigger has no target"
 * 80060114  0x05E8  fn_ : spawn enemy from world data (CritterNewInst,
 *                    generate_enemy, atan2, UpdateObjWorldMat, MBWorldSphereVisible3).
 *                    "Bad EnemyInfo: type %s subtype %d not loaded",
 *                    "EnemyInfo didn't generate %s(ai=%d, reason=%s)"
 * 800606FC  0x22B4  fn_ : GIANT per-frame world update dispatcher (AudioStopAll,
 *                    ShakeCamera, TriggerCameraActivate, find_enemy_slot,
 *                    generate_enemy, fn_8005A338, fn_80060114, fn_80062A00,
 *                    fn_80062FF0, items place_logic12, +~40 more).
 *                    "CAN'T FIND LOOKPUT PARAM:%d".  [parked giant]
 * 800629B0  0x0050  fn_ : accessor (no calls; sNumItems/80344950).
 *                    Called by auxscreen.
 * 80062A00  0x05F0  fn_ : particle-system update (WorldPsysActivate/DeActivate,
 *                    did_generate, AudioBridgeOpen/Close,
 *                    AudioWorldObjectMotion, fn_8009D258..694,
 *                    MBTreeClearFlags/368/6C0)
 * 80062FF0  0x01BC  fn_ : enemy-grid op (StartEnemyGrid/NextGridEnemy,
 *                    fqdist)
 * ========================================================================== */

/* 0x8005A738 - per-player overhead name text (random name + timed draw) */
typedef struct NameTimerView {
    u8  _pad[504];
    s16 timer;              /* 0x1F8 */
} NameTimerView;
extern u8 gGameOptions[];
extern char* lbl_8011C848[];    /* random name table (+80: per-player colors) */
extern s16 lbl_80343C40[4];     /* per-player text x offsets (sdata) */
extern f32 lbl_80346DB0;        /* draw scale */
extern s16 lbl_80343C38[4];
extern f32 lbl_80346DB4;
extern u32 lbl_8011C898[4];
extern u32 pbLoad;
extern s32 RandInt(s32 range);
extern char* strcpy(char* d, const char* s);
extern void DrawTextKeepScale(f32 scale, s32 x, s32 y, u32 font, u32 color, u8* str);
extern s32 new_menu_accept(s32 player, s32 allow_start);
extern s32 new_menu_back(s32 player);
extern s32 new_up(s32 player);
extern s32 new_down(s32 player);
extern void AudioCursorH(void);
extern void AudioCursorV(void);
extern void AudioClick2(s32 player, s32 select);
extern s32 fn_8005A868(s32 player);
extern s32 gFrameTicks;

typedef struct WorldNameControl {
    u32 _pad00[3];
    u32 repeat;
    u32 _pad10[11];
} WorldNameControl;
extern WorldNameControl lbl_80240E30[4];

s32 fn_8005A738(s32 player)
{
    char** names = lbl_8011C848;
    Player* p = &gPlayers[player];
    s32 ret = 0;
    s16 t;
    u8 _spare[8];

    if (*(u32*)(gGameOptions + 44) & 1) {
        strcpy((char*)p + 2688, names[RandInt(20)]);
        return -1;
    }
    if (*(s32*)((u8*)p + 13100) == 0) {
        if ((ret = fn_8005A868(player)) <= 0) {
            goto out;
        }
        *(s32*)((u32)p + 13100) = 1;
        ((NameTimerView*)p)->timer = 60;
        ret = 0;
        if (*(s8*)((u8*)p + 2688) == 0) {
            strcpy((char*)((u32)p + 2688), names[RandInt(20)]);
        }
    } else {
        t = ((NameTimerView*)p)->timer - gFrameTicks;
        ((NameTimerView*)p)->timer = t;
        if (t < 0) {
            *(s16*)((u32)p + 504) = 0;
            ret = 1;
        } else if (*(s16*)((u32)p + 504) & 16) {
            DrawTextKeepScale(lbl_80346DB0, -lbl_80343C40[player], 340, 7,
                              (u32)(&names[player])[20],
                              (u8*)p + 2688);
        }
    }
out:
    return ret;
}

s32 fn_8005A868(s32 player)
{
    s32 accept;
    s32 skip;
    s32 i;
    u16 x;
    u32 color;
    u32 white;
    char text[2];
    Player* p;
    u32* repeat;

    p = &gPlayers[player];
    accept = new_menu_accept(player, 0);
    if (accept == 0) {
        repeat = (u32*)lbl_80240E30;
        repeat += player * 15;
        if ((*(repeat += 3) & 0x40000030) != 0) {
            switch (p->world_name_tail) {
            default:
                p->world_name_tail++;
                break;
            case 'Z':
                p->world_name_tail = '_';
                break;
            case '_':
                p->world_name_tail = '0';
                break;
            case '9':
                p->world_name_tail = '@';
                break;
            case '@':
                p->world_name_tail = 'A';
                break;
            }
            AudioCursorV();
        }
        if ((*repeat & 0x800000C0) != 0) {
            switch (p->world_name_tail) {
            default:
                p->world_name_tail--;
                break;
            case 'A':
                p->world_name_tail = '@';
                break;
            case '@':
                p->world_name_tail = '9';
                break;
            case '0':
                p->world_name_tail = '_';
                break;
            case '_':
                p->world_name_tail = 'Z';
                break;
            }
            AudioCursorV();
        }
    }

    if ((new_down(player) != 0 || new_menu_back(player) != 0) &&
        p->world_name_len > 0) {
        p->world_name_len--;
        p->world_name_tail = (s8)p->name[p->world_name_len];
        p->name[p->world_name_len] = 0;
        AudioCursorH();
    }

    if (accept != 0 || new_up(player) != 0) {
        skip = 0;
        if (p->world_name_tail == '@') {
            if (accept != 0) {
                p->world_name_len = 6;
            } else {
                skip = 1;
            }
        } else if (p->world_name_tail == '<') {
            if (p->world_name_len > 0) {
                p->world_name_len--;
                p->name[p->world_name_len] = 0;
            }
        } else {
            if (p->world_name_len + 1 < 7) {
                p->name[p->world_name_len] = (s8)p->world_name_tail;
                p->name[p->world_name_len + 1] = 0;
            }
            p->world_name_len++;
        }
        if (skip == 0) {
            p->world_name_tail = '@';
            AudioClick2(player, 1);
        }
    }

    white = 0xFFFFFF;
    x = lbl_80343C38[player] - 30;
    text[1] = 0;
    for (i = 0; i < p->world_name_len; i++, x += 18) {
        text[0] = p->name[i];
        DrawTextKeepScale(lbl_80346DB4, (u16)x - 4, 340, 7,
                          lbl_8011C898[player], (u8*)text);
    }
    if (i < 6) {
        color = (pbLoad & 0x10) ? white : 0x404040;
        text[0] = (s8)p->world_name_tail;
        DrawTextKeepScale(lbl_80346DB4, (u16)x - 4, 340, 7, color,
                          (u8*)text);
        x += 18;
    }
    i++;
    while (i < 6) {
        DrawTextKeepScale(lbl_80346DB4, (u16)x - 4, 340, 7, white,
                          (u8*)"");
        i++;
        x += 18;
    }
    if (p->world_name_len >= 6) {
        return 1;
    }
    return 0;
}

/* 0x8005D3D8 - can this world object block/affect the given enemy? */
extern Enemy gEnemies[25]; /* game/enemy.h; stride 0x394 */
extern f64 sNewtonThree;
extern s32 damage_enemy(u8* e, f32 amount, s32 dtype, s32 knock, s32 srcflags,
                        s32 arg6, s32 arg7);

s32 fn_8005D3D8(s32 index, u8* wobj)
{
    u8* hdr = *(u8**)wobj;
    s32 ret;
    u8* sub = hdr + 4;
    u8* e;
    s32 t;
    s32 bval;

    if (index >= 0) {
        e = (u8*)gEnemies + index * 916;
    } else {
        e = 0;
    }
    ret = 1;
    switch (*(u32*)hdr) {
    case 1:
        ret = 0;
        break;
    case 10:
        switch (*(s32*)sub) {
        case 40:
        case 49:
        case 51:
        case 52:
        case 53:
            ret = 0;
            break;
        case 41:
            if (*(s16*)(wobj + 222) > 0) {
                ret = 1;
            }
            break;
        default:
            ret = 1;
            break;
        }
        break;
    case 2:
        if (e == 0) {
            break;
        }
        if (*(s32*)e == 29 || *(s32*)e == 32) {
            ret = 0;
        }
        break;
    case 3:
        if (e == 0) {
            break;
        }
        if (*(s8*)(wobj + 226) != 0) {
            t = 0;
        } else {
            t = 1;
        }
        if (t != 0) {
            bval = 0;
        } else {
            bval = 1;
        }
        ret = bval;
        if (*(s16*)(wobj + 220) == 17) {
            ret = 0;
        }
        if (*(s32*)e == 29 || *(s32*)e == 32) {
            if (*(f32*)(hdr + 16) <= sNewtonThree) {
                ret = 0;
            }
        }
        break;
    case 8:
        if (e == 0) {
            break;
        }
        t = *(s32*)e;
        if (t == 30) goto case8_blocked;
        if (t == 29) goto case8_blocked;
        if (t != 32) goto case8_damage_check;
case8_blocked:
        ret = 0;
        break;
case8_damage_check:
        if (t == 3 || t == 0) {
            s32 b = *(s8*)(wobj + 200);
            if (b == 2) goto dmg;
            if (b != 4) goto nodmg;
dmg:
            damage_enemy(e, *(f32*)(wobj + 220), -1, 0, (s32)(e + 68), 0, 2);
nodmg:
            ret = 0;
        }
        break;
    case 5:
    case 9:
    case 11:
    case 12:
        ret = 0;
        break;
    }
    return ret;
}

/* 0x8005D5C8 - classify a world object for a player (jumptable pair) */
s32 fn_8005D5C8(u8* pl, u8* wobj)
{
    u8* hdr = *(u8**)wobj;
    s32 ret = 1;
    u8* sub = hdr + 4;
    s32 cls = *(s16*)(*(u8**)(*(u8**)(pl + 4) + 288) + 32);
    s32 t;

    switch (*(u32*)hdr) {
    case 1:
        ret = 0;
        break;
    case 10:
        switch (*(s32*)sub) {
        case 40:
        case 49:
        case 51:
        case 52:
        case 53:
            ret = 0;
            break;
        case 41:
            if (*(s16*)(wobj + 222) > 0) {
                ret = 1;
            }
            break;
        case 43:
        case 44:
        case 45:
            if (cls == 3 || cls == 7) {
                ret = 3;
            } else {
                ret = 1;
            }
            break;
        default:
            ret = 1;
            break;
        }
        break;
    case 2:
        ret = 1;
        if (*(s32*)sub == 43) {
            if (cls == 3 || cls == 7) {
                ret = 3;
            }
        } else {
            if (cls == 3 || cls == 7) {
                ret = 2;
            }
        }
        break;
    case 3:
        if (*(s8*)(wobj + 226) != 0) {
            t = 0;
        } else {
            t = 1;
        }
        ret = (t != 0) ? 0 : 1;
        if (*(f32*)(hdr + 16) <= sNewtonThree) {
            if (cls == 3 || cls == 7) {
                ret = 3;
            } else {
                ret = 0;
            }
        }
        break;
    case 8:
        ret = 0;
        break;
    case 5:
    case 9:
    case 11:
    case 12:
        ret = 0;
        break;
    }
    return ret;
}

extern f64 sArrowFloorYOffset;
extern f64 sZeroDouble;
extern f32 sItemZero;
extern f64 lbl_80346FB8;

/* 0x8005D20C - track/find the world object ahead of an enemy (cached in
 * e+652 with a rescan timer at e+812) */
#pragma dont_inline on
s32 fn_8005D20C(s32 index, f32* from, f32* to, s32 ticking)
{
    u8* e = (u8*)gEnemies + index * 916;
    u32 obj;
    s32 blocked;
    f32 rad;
    f32 hit;
    f64 d;

    rad = (f32)(sArrowFloorYOffset * *(f32*)(e + 568));
    obj = 0;
    blocked = 0;
    if (ticking == 0 && *(u32*)(e + 652) != 0) {
        d = fn_8005F0F4((Item*)*(u32*)(e + 652), from, to, 0, rad,
                        (f32)(lbl_80346FB8 * rad));
        obj = (d >= sZeroDouble) ? *(u32*)(e + 652) : 0;
    } else {
        if (ticking != 0) {
            *(s32*)(e + 812) = *(s32*)(e + 812) - gFrameTicks;
        }
        if (*(s32*)(e + 812) <= 0) {
            obj = (s32)fn_80062FF0(rad, to, 0, (f32*)0, &hit);
            hit = hit - rad;
            if (hit > sItemZero) {
                s32 t1 = (s32)(hit * (sArrowFloorYOffset * *(f32*)(e + 184)));
                s32 t2 = (s32)(hit * (sArrowFloorYOffset * *(f32*)(e + 184)));
                *(s32*)(e + 812) = (t2 < 30) ? t1 : 30;
            }
            if (obj != 0) {
                d = fn_8005F0F4((Item*)obj, from, to, 0, rad,
                                (f32)(lbl_80346FB8 * rad));
                obj = (d >= sZeroDouble) ? obj : 0;
            }
        }
    }
    if (obj != 0) {
        blocked = fn_8005D3D8(index, (u8*)obj);
    }
    if (blocked != 0) {
        *(s32*)(e + 652) = obj;
    } else {
        *(s32*)(e + 652) = 0;
    }
    return blocked;
}
#pragma dont_inline off

extern char lbl_80112C68[];            /* "COL OBJECT Item: idx < 0" */
extern f32 lbl_8023F7E8[3];
extern f32 lbl_8023F7F8[3];
/* gWorldInfo: game/worldinfo.h (WorldInfo, 0x8028CA8C, size 0xA4) */
extern s32 lbl_80344188;
extern char lbl_8034418C;
extern f32 lbl_80344190;
extern f32 lbl_80344194;
void FatalError(const char* msg, int code);
f32 CTriListCollide(f32 radius, s32 base, s32 count, u8** outTri,
                    s16* idxList, f32* outPt, s32 layerLo, s32 layerHi,
                    s32 noFilter);
void MulBodyVecMat4(const f32* vector, f32* out, const f32* matrix);
void MulVecMat4(const f32* vector, f32* out, const f32* matrix);

/* 0x8005FDA8 - sweep an item's collision tri list along segment a->b */
f32 fn_8005FDA8(u8* e, f32* a, f32* b, f32* outPos, f32* outNorm, f32 margin)
{
    f32* m = (f32*)((Item*)e)->objgrp.node;
    f32 pt[5];
    u8* triOut;
    f32 lo;
    f32 hi;
    f32 hit;
    char v;
    f32 k;
    f64 d1;
    f64 d2;

    if (*(s16*)(e + 192) < 0) {
        FatalError(lbl_80112C68, 0x800000);
    }
    if (a[1] < b[1]) {
        lo = a[1] - margin;
        hi = b[1] + margin;
    } else {
        lo = b[1] - margin;
        hi = a[1] + margin;
    }
    *(s32*)((u8*)&gWorldInfo + offsetof(WorldInfo, checknum)) =
        *(s32*)((u8*)&gWorldInfo + offsetof(WorldInfo, checknum)) + 1;
    if (*(s32*)((u8*)&gWorldInfo + offsetof(WorldInfo, checknum)) > 255) {
        *(s32*)((u8*)&gWorldInfo + offsetof(WorldInfo, checknum)) = 1;
    }
    v = (char)*(s32*)((u8*)&gWorldInfo + offsetof(WorldInfo, checknum));
    lbl_80344188 = 0;
    lbl_8034418C = v;
    MulBodyVecMat4(a, lbl_8023F7F8, m);
    MulBodyVecMat4(b, lbl_8023F7E8, m);
    k = m[5];
    d1 = 64.0 * (k * (lo - m[13]));
    d2 = 64.0 * (k * (hi - m[13]));
    lbl_80344194 = -0.5f;
    lbl_80344190 = 2.0f;
    hit = CTriListCollide(margin, *(s16*)(e + 192), *(s16*)(e + 194), &triOut,
                          (s16*)0, pt, (s16)(s32)d1, (s16)(s32)d2, 0);
    if (hit >= 0.0) {
        if (outNorm != 0) {
            WorldVector((f32*)(triOut + 8), outNorm, m);
        }
        if (outPos != 0) {
            MulVecMat4(pt, outPos, m);
        }
    }
    return hit;
}

extern char lbl_80112C84[];            /* "Special trigger has no target" */
extern s32 sNumItemWobjs;
extern u8 sItemRuntime[];
s16* FindWobjWanim(void* wobj);

typedef struct ItemWobjRuntime {
    f32 y[450];
    f32 initialY[450];
    u8 _pad[25616];
    u32 object[450];
} ItemWobjRuntime;

/* Fire all special triggers of the given class. */
void ActivateSpecialTrigger(s32 type, s32 flag)
{
    s32 i;
    u8* it;
    u8* w;
    u8* obj;
    u8* entry;
    s32 n;
    s32 j;
    ItemWobjRuntime* rt;
    u8 _spare[8];

    it = (u8*)sItems;
    rt = (ItemWobjRuntime*)sItemRuntime;
    for (i = 0; i < sNumItems; i++, it += 240) {
        if (*(s16*)(it + 196) != -1 && (*(s16*)(it + 196) & 0x8100) == 0 &&
            *(s32*)*(u32**)it == 5 && *(u8*)(it + 226) == type) {
            w = it;
            while (w != 0) {
                *(s16*)(w + 196) |= 0x400;
                *(s8*)(w + 200) = 2;
                *(s8*)(w + 202) = 2;
                obj = *(u8**)(w + 220);
                if (obj != 0) {
                    *(s8*)(obj + 22) = 47;
                    if (*(u32*)(obj + 16) & 0x2000000) {
                        s16* wa = FindWobjWanim(obj);
                        *(s8*)(obj + 23) = 47;
                        *(s8*)(obj + 22) = 47;
                        *(u32*)(obj + 16) |= 0x200000;
                        if (flag != 0) {
                            *(u32*)(obj + 16) |= 0x800000;
                            if (wa != 0) {
                                *(f32*)(wa + 4) = wa[1] - 1;
                            }
                        }
                    } else {
                        if (flag != 0) {
                            n = sNumItemWobjs;
                            for (j = 0; j < n; j++) {
                                entry = (u8*)rt;
                                entry += j * 4;
                                if (*(u32*)(entry + 29216) == (u32)obj) {
                                    break;
                                }
                            }
                            if (j < n) {
                                f32 v;

                                entry = (u8*)rt;
                                entry += j * 4;
                                v = *(f32*)(entry + 1800);
                                rt->y[j] = v;
                                *(f32*)(*(u32*)(obj + 40) + 52) = v;
                            }
                        }
                    }
                } else {
                    ErrorPrintf(lbl_80112C84);
                }
                w = *(u8**)(w + 228);
            }
        }
    }
}

extern s32 lbl_8034488C;
extern f32 lbl_80347014;
extern s32 gNumPlayers;
extern Camera gCameras[6]; /* game/camera.h; stride 0x18C */
extern f64 sArrowFloorYOffset;
extern f64 lbl_80347018;
extern f32 lbl_803447D8;
extern void MBTreeSetScale(void* node, f32 x, f32 y, f32 z);
extern u8* CritterNewInst(s32 type, s32 sub, void* mat);
extern s32 generate_enemy(f32* pos, s32 kind, s32 a, f32* dir, s32 b, s32 c,
                          s32 d, f32 radius);
extern f64 atan2(f64 y, f64 x);
extern f64 sPi;
extern f64 sTwoPi;
extern f64 sNegativePi;
extern void CreateYPRMatrix(f32* mtx, f32* pyr);
extern void CopyMat3(f32* src, void* dst);
extern f32 sNoNearbyPlayerDistance;
extern f64 sItemFloorYOffset;
extern f32 sItemFloorRadius;
extern char* lbl_8011B578[];
extern char lbl_80112CA4[];
extern char lbl_80112CD4[];
extern char* lbl_8011C8F0[];

extern u8 sItemRuntime[];
extern f64 lbl_80347100;
extern f64 lbl_803470F0;
extern f64 lbl_803470F8;
extern f32 lbl_803470E8;
extern f32 sItemSearchDistance;
extern s32 lbl_80344A28;
extern s32 lbl_803447B8;
extern f32 gClockFrameStep;
extern s32 sNumItemWobjs;
extern s32 did_generate(void* w, s32 a);
extern void AudioBridgeClose(f32* pos);
extern void AudioBridgeOpen(f32* pos);
extern void AudioWorldObjectMotion(f32* pos, s32 d);
extern s32 fn_8009D694(s32 mode, f32* pos, s32 d);
extern void WorldPsysActivate(void* w);
extern void WorldPsysDeActivate(void* w);
extern void MBTreeSetAlpha(void* node, s32 alpha, s32 mode);
extern void MBTreeClearFlags(void* node, s32 flags, s32 value);

/* 0x80062A00 - per-frame world-object motion pump: bridge/door audio cues,
 * fade-in/out alpha ramps, door open/close latching, continuous sliders. */
void fn_80062A00(void)
{
    u8* rt;
    u8* row;
    u8* w;
    u8* node;
    s32 heard;
    s32 i;
    s32 off;
    s32 st;
    s32 prev;
    s32 gen;
    s32 kind;
    s32 flags8;
    s32 act;
    s32 a;
    s32 on;
    u32 fl16;
    u32 fl;
    f32 pos[3];
    f32 dcur;
    f32 delta;
    f32 step;
    f64 kHi;
    f64 kRate;
    f64 kLo;
    f32 dist;
    f32 kMoving;
    f32 zero;

    rt = sItemRuntime;
    kHi = lbl_80347100;
    kRate = lbl_803470F0;
    kLo = lbl_803470F8;
    dist = sItemSearchDistance;
    kMoving = lbl_803470E8;
    zero = sItemZero;
    heard = 0;
    i = 0;
    off = 0;
    for (; i < sNumItemWobjs; i++, off += 4) {
        row = rt + off;
        w = *(u8**)(row + 29216);
        st = (s8)*(u8*)(w + 22);
        prev = (s8)*(u8*)(w + 23);
        gen = did_generate(w, 1);
        pos[0] = *(f32*)(*(u8**)(w + 40) + 48);
        pos[1] = *(f32*)(*(u8**)(w + 40) + 52);
        pos[2] = *(f32*)(*(u8**)(w + 40) + 56);
        dcur = *(f32*)(row + 2400);
        kind = *(s16*)(w + 20) & 0xFF;
        flags8 = (*(s16*)(w + 20) >> 8) & 0xFF;
        if (dcur >= zero && lbl_80344A28 == 0 && lbl_803447B8 == 0) {
            if (kind == 20 || kind == 22) {
                if ((st ^ prev) & 0x20) {
                    if (st & 0x20) {
                        AudioBridgeClose(pos);
                    } else {
                        AudioBridgeOpen(pos);
                    }
                }
            } else if (dcur >= dist) {
                if (kMoving == dcur) {
                    if ((st & 0x20) && !(prev & 0x20) &&
                        (*(u32*)(w + 16) & 0x00C00000)) {
                        AudioWorldObjectMotion(pos, (s32)(dcur - dist));
                    }
                } else if ((st ^ prev) & 0x10) {
                    AudioWorldObjectMotion(pos, (s32)(dcur - dist));
                }
            } else {
                if (st & 0x10) {
                    if (heard == 0) {
                        heard = fn_8009D694(0, pos, (s32)dcur);
                    }
                } else if (prev & 0x10) {
                    fn_8009D694(2, pos, (s32)dcur);
                }
            }
        }
        *(u8*)(w + 23) = st;
        *(u8*)(w + 53) = 0;
        fl16 = *(u32*)(w + 16);
        if (fl16 & 0x800) {
            act = 0;
            if (st & 0x20) {
                WorldPsysActivate(w);
            } else {
                WorldPsysDeActivate(w);
            }
        } else if (flags8 & 0x10) {
            if (flags8 & 0x20) {
                if (st & 47) {
                    on = 1;
                }
            } else {
                if (!(st & 0xF)) {
                    on = 1;
                }
            }
            *(s8*)(w + 53) = (s8)(on != 0 ? 255 : 0);
            act = 0;
            node = *(void**)(w + 40);
            if (node == NULL) {
                goto tail;
            }
            if (*(u32*)(node + 96) & 0x200) {
                a = 255 - *(u8*)(node + 83);
            } else {
                a = 0;
            }
            if (on != 0) {
                if (a >= 255) {
                    goto tail;
                }
                if (lbl_803447B8 != 0) {
                    a = 255;
                } else {
                    a = a + (gFrameTicks << 3);
                }
                if (a >= 248) {
                    MBTreeSetFlags(node, 2, 0);
                    MBTreeSetAlpha(*(void**)(w + 40), 255, 1);
                } else {
                    MBTreeClearFlags(node, 2, 0);
                    MBTreeSetAlpha(*(void**)(w + 40), a, 1);
                    act = 1;
                }
            } else {
                MBTreeClearFlags(node, 2, 0);
                if (a == 0) {
                    goto tail;
                }
                if (lbl_803447B8 != 0) {
                    a = 0;
                } else {
                    a = a - (gFrameTicks << 3);
                }
                if (a < 0) {
                    a = 0;
                    act = 1;
                } else {
                    act = 1;
                }
                MBTreeSetAlpha(*(void**)(w + 40), a, 1);
            }
        } else if (fl16 & 0x02000000) {
            if (!(flags8 & 8) && gen >= 2) {
                *(u32*)(w + 16) = fl16 | 0x00300000;
                *(u8*)(w + 22) = *(u8*)(w + 22) & ~0x10;
                goto next;
            }
            act = 1;
            if (flags8 & 0x20) {
                if (st & 0xF) {
                    *(u32*)(w + 16) &= ~0x00300000;
                    st |= 48;
                } else if (fl16 & 0x00800000) {
                    *(u32*)(w + 16) |= 0x00300000;
                    st &= ~0x30;
                    act = 0;
                }
            } else {
                if (st & 0x20) {
                    *(u32*)(w + 16) |= 0x00200000;
                    *(u32*)(w + 16) &= ~0x00100000;
                } else {
                    *(u32*)(w + 16) |= 0x00100000;
                    *(u32*)(w + 16) &= ~0x00200000;
                }
                fl = *(u32*)(w + 16);
                if (((fl & 0x00100000) && (fl & 0x00400000)) ||
                    ((fl & 0x00200000) && (fl & 0x00800000))) {
                    act = 0;
                }
            }
        } else {
            if (!(flags8 & 8) && gen >= 2) {
                *(f32*)(*(u8**)(w + 40) + 52) =
                    *(f32*)(row + 600) + *(f32*)row;
                goto next;
            }
            if (st & 0x20) {
                delta = *(f32*)(row + 1800) - *(f32*)row;
            } else {
                delta = *(f32*)(row + 1200) - *(f32*)row;
            }
            step = (f32)(kRate * gClockFrameStep);
            act = 1;
            if (delta > kLo) {
                if (delta > step) {
                    delta = step;
                }
            } else if (delta < kHi) {
                if (delta < -step) {
                    delta = -step;
                }
            } else {
                if (flags8 & 0x20) {
                    st ^= 0x20;
                } else {
                    act = 0;
                }
            }
            if (act != 0) {
                *(f32*)row = *(f32*)row + delta;
                *(u32*)(w + 16) |= 0x08000000;
            } else {
                *(u32*)(w + 16) &= ~0x08000000;
            }
            *(f32*)(*(u8**)(w + 40) + 52) = *(f32*)(row + 600) + *(f32*)row;
        }
    tail:
        if (act != 0) {
            if (!(flags8 & 8)) {
                *(u8*)(w + 53) = 1;
            }
            if (!(st & 0x10)) {
                st |= 0x10;
            }
            *(u32*)(w + 16) |= 0x20000000;
        } else {
            st &= ~0x30;
            *(u32*)(w + 16) &= ~0x20000000;
        }
        if (!(flags8 & 71)) {
            if (flags8 & 0x10) {
                if (gen == 0) {
                    st = 0;
                }
            } else {
                st = 0;
            }
        }
        *(s8*)(w + 22) = (s8)st;
    next:;
    }
    if (heard == 0) {
        fn_8009D694(-1, 0, 0);
    }
}

/* 0x80060114 - convert a pending enemy-spawn item into a live critter or
 * generated enemy once it becomes visible, then retire the item slot. */
void fn_80060114(Item* item, f32* pos, f32* dir)
{
    u8* it = (u8*)item;
    u8* sp;
    s32 kind;
    u8* crit;
    u8* e;
    s32 g;
    s32 idx;
    f32 root;
    f32 d2;

    sp = it + 220;
    kind = *(s16*)(it + 220);
    if (kind < 0) {
        return;
    }
    if ((gGameMode & 0x8000) && (u32)gGameMode != 0x8006 &&
        (u32)gGameMode != 0x8003) {
        return;
    }
    if (lbl_8034488C == 0) {
        return;
    }
    if (*(s32*)(gGameOptions + 8) == 0) {
        return;
    }
    if (MBWorldSphereVisible3((f32*)(it + 52),
                              lbl_80347014 * *(f32*)(it + 212)) == 0) {
        return;
    }
    if (*(s16*)(it + 220) == 31 && gNumPlayers <= 1) {
        return;
    }
    {
        f32 dy = *(f32*)((u8*)gCameras + offsetof(Camera, attn) + 4) - *(f32*)(it + 56);
        f32 dx = *(f32*)((u8*)gCameras + offsetof(Camera, attn)) - *(f32*)(it + 52);
        f32 dz = *(f32*)((u8*)gCameras + offsetof(Camera, attn) + 8) - *(f32*)(it + 60);
        d2 = dy * dy;
        d2 = dx * dx + d2;
        d2 = dz * dz + d2;
        if (d2 > sItemZero) {
            f64 estimate = __frsqrte(d2);
            estimate = sArrowFloorYOffset * estimate *
                       (sNewtonThree - estimate * estimate * d2);
            estimate = sArrowFloorYOffset * estimate *
                       (sNewtonThree - estimate * estimate * d2);
            estimate = sArrowFloorYOffset * estimate *
                       (sNewtonThree - estimate * estimate * d2);
            root = (f32)(d2 *
                         (sArrowFloorYOffset * estimate *
                          (sNewtonThree - estimate * estimate * d2)));
            d2 = root;
        }
        if ((f64)d2 > lbl_80347018) {
            return;
        }
    }
    if (kind == 29 || kind == 30 || kind == 32) {
        if (lbl_80346EE8 != (f64)lbl_803447D8 && *(void**)(it + 100) != NULL) {
            MBTreeSetScale(*(void**)(it + 100), lbl_803447D8, lbl_803447D8,
                           lbl_803447D8);
        }
        if (!(*(u32*)(it + 228) & 1)) {
            return;
        }
        if (!(*(s16*)(it + 196) & 1)) {
            *(s16*)(it + 196) |= 1;
            *(u8*)(it + 202) = 1;
            return;
        }
        if (*(s16*)(it + 198) > 0) {
            return;
        }
    }
    crit = 0;
    pos[0] = *(f32*)(it + 84);
    pos[1] = *(f32*)(it + 88);
    pos[2] = *(f32*)(it + 92);
    dir[0] = *(f32*)(it + 36);
    dir[1] = *(f32*)(it + 40);
    dir[2] = *(f32*)(it + 44);
    switch (kind) {
    case 29:
        crit = CritterNewInst(3, 0, it + 4);
        break;
    case 33:
        crit = CritterNewInst(8, 0, it + 4);
        break;
    case 32:
        crit = CritterNewInst(7, 0, it + 4);
        break;
    }
    if (crit != NULL) {
        if (*(u32*)(it + 108) != 0) {
            AtreeDelete(it + 108);
            *(s32*)(it + 108) = 0;
        }
        if (*(void**)(it + 100) != NULL) {
            MBRemoveNode(*(void**)(it + 100), 0);
            *(s32*)(it + 100) = 0;
        }
        *(s16*)(it + 196) = -1;
        idx = (s32)(it - (u8*)sItems) / 240;
        if (idx < gNextItemIdx) {
            gNextItemIdx = idx;
        }
        if (*(f32*)(sp + 12) > sZeroDouble) {
            *(f32*)(crit + 2768) =
                *(f32*)(sp + 12) * gCurLevel->ene_visrad;
        }
        if (*(s16*)(sp + 18) >= 0) {
            *(u8**)(crit + 2764) =
                (u8*)sItems + *(s16*)(sp + 18) * 240;
        }
        return;
    }
    g = generate_enemy(pos, kind, (s8)*(u8*)(sp + 2), dir, (s8)*(u8*)(sp + 3),
                       0, 1, sItemFloorRadius);
    if (g >= 0) {
        f64 yaw;
        e = (u8*)gEnemies + g * 916;
        *(s16*)(e + 728) = 1;
        *(s16*)(e + 724) = 1;
        *(f32*)(e + 588) = atan2(dir[0], *(f32*)((u8*)dir + 32));
        yaw = *(f32*)(e + 588);
        if (yaw > sPi) {
            yaw = yaw - sTwoPi;
        } else if (yaw <= sNegativePi) {
            yaw = sTwoPi + yaw;
        }
        *(f32*)(e + 588) = yaw;
        *(f32*)(e + 592) = *(f32*)(e + 588);
        *(f32*)(e + 580) = *(f32*)(e + 588);
        {
            f32 mtx[12];
            CreateYPRMatrix(mtx, (f32*)(e + 576));
            CopyMat3(mtx, e + 4);
        }
        UpdateObjWorldMat((OBJGRP*)(e + 4));
        *(f32*)(e + 748) = *(f32*)(e + 52);
        *(f32*)(e + 752) = *(f32*)(e + 56);
        *(f32*)(e + 756) = *(f32*)(e + 60);
        if (*(s32*)e != 31 && *(s32*)e != 30 && (s8)*(u8*)(sp + 2) == 0) {
            *(s32*)(e + 180) = 6;
        } else if ((s8)*(u8*)(sp + 2) < 4) {
            *(s32*)(e + 524) = 30;
        }
        if (*(s32*)e == 30) {
            *(f32*)(e + 768) = sNoNearbyPlayerDistance;
        } else if (*(f32*)(sp + 12) > sZeroDouble) {
            *(f32*)(e + 768) =
                *(f32*)(sp + 12) * gCurLevel->ene_visrad;
        }
        if (*(s16*)(sp + 16) > 0) {
            if (*(s16*)(sp + 16) == 1) {
                *(f32*)(e + 888) = sItemZero;
            } else {
                *(f32*)(e + 888) =
                    (f32)(sItemFloorYOffset * (f64)*(s16*)(sp + 16));
            }
        }
        if (*(s16*)(sp + 18) >= 0) {
            *(u8**)(e + 900) = (u8*)sItems + *(s16*)(sp + 18) * 240;
        }
    } else if (g > -99) {
        if ((s8)*(u8*)(sp + 2) >= 4 || *(s32*)e > 1) {
            if (g == -5) {
                ErrorPrintf(lbl_80112CA4, lbl_8011B578[kind],
                            (s8)*(u8*)(sp + 2));
            } else {
                ErrorPrintf(lbl_80112CD4, lbl_8011B578[kind],
                            (s8)*(u8*)(sp + 3), lbl_8011C8F0[-(g + 1)]);
            }
        }
    }
    if (*(u32*)(it + 108) != 0) {
        AtreeDelete(it + 108);
        *(s32*)(it + 108) = 0;
    }
    if (*(void**)(it + 100) != NULL) {
        MBRemoveNode(*(void**)(it + 100), 0);
        *(s32*)(it + 100) = 0;
    }
    *(s16*)(it + 196) = -1;
    idx = (s32)(it - (u8*)sItems) / 240;
    if (idx < gNextItemIdx) {
        gNextItemIdx = idx;
    }
}
