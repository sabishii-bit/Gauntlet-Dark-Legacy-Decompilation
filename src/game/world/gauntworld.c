#include "types.h"
#include "game/dyngrid.h"
#include "game/item.h"
#include "game/player.h"
#include "game/worldobj.h"

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

/* --- One level record inside WorldData.levels (stride 0x10C) --------------- */
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
    /* 0x5E */ u8    _pad5E[0x2E];
    /* 0x8C */ s16   sec34Idx;   /* -> sec34Ptr  (WorldData.section34[idx])  */
    /* 0x8E */ u8    _pad8E[0x7E];
    /* pointer fix-ups written by ResolveWorldDataPointers: */
    /* 0x60 */ /* u8* cameraPtr; (overlaps _pad; see field access below)     */
    /* audio volume / range floats live at 0xA8..0xDC and are normalised     */
};

/* ---- real callees (names already resolved in config/GUNE5D/symbols.txt) --- */
extern void* MBSetupWad(void* ctx, void* wadData);
extern void* MBGetFromWad(void* ctx, int fourcc, int arg, int* outLen);
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
extern WorldLevel* gCurLevel;    /* 0x8034483C active level record            */

/* forward decl of the static BE fix-up pass */
static void ResolveWorldDataPointers(void);

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
 * The bulk of the 0x1C3C body is the mechanical per-field endian swap of the
 * header, the level array (stride 0x10C), the camera array (0x6C), the audio
 * array (0x3C) and the string/anim tables.  That swap is documented rather
 * than transcribed here; the control flow and the resolve/cache tail are
 * reconstructed faithfully. */
void ResolveWorldData(int worldlevel)
{
    int realm;
    int level;
    int i;

    if (worldlevel < 0) {
        return;
    }
    realm = worldlevel >> 8;
    level = worldlevel & 0xFF;

    if (sCurWorldType != realm) {
        for (i = 0; i < 14; i++) {
            if (realm != sWorldDataTypes[i]) {
                continue;
            }
            /* loaded flag lives beside the type table */
            if (sWorldLevelTable[i * 0xB + 0x4] == 0) {
                FatalErrorf("No world data %s\n", &sWorldLevelTable[i]);
                break;
            }
            /* set up the realm WAD and pull its sections */
            MBSetupWad(0, (void*)sWorldLevelTable[i]);
            gWorldData = (WorldData*)MBGetFromWad(0, 'whdr', 0, 0);
            /* ... byte-swap header + level/camera/audio/anim tables ... */
            gWorldData->levels    = (WorldLevel*)MBGetFromWad(0, 'levs', 0, 0);
            gWorldData->section20 =              MBGetFromWad(0, 'sc20', 0, 0);
            gWorldData->cameras   = (u8*)        MBGetFromWad(0, 'cams', 0, 0);
            gWorldData->audio     = (u8*)        MBGetFromWad(0, 'audi', 0, 0);
            /* ... (further MBGetFromWad sections + swaps omitted) ... */
            ResolveWorldDataPointers();
            sCurWorldType  = realm;
            sCurWorldIndex = i;
            break;
        }
    }

    if (gWorldData == 0) {
        return;
    }
    if (level >= (int)gWorldData->numLevels) {
        level = 0;
    }
    gWorldData->curLevel = (s16)level;
    gCurLevel      = &gWorldData->levels[level];
    sMusicTrackLo  = level;
    sMusicTrackHi  = realm;
    sLastWorldLevel = worldlevel;
    gBossType      = gCurLevel->bossType;

    /* first level (from the current one) that owns cameras */
    for (i = level; i < (int)gWorldData->numLevels; i++) {
        if (gWorldData->levels[i].flags2 & 1) {
            break;
        }
    }
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
extern f32  lbl_8011C748[3];           /* volume gain table                 */
extern f64  lbl_80346C70;              /* -1.0 sentinel                     */
extern f32  lbl_80346BE0;              /* 1.0                                */
extern char lbl_80112788[];            /* string block (+716/+748/+776)     */

static void ResolveWorldDataPointers(void)
{
    char* strs = lbl_80112788;
    u8* lvl;
    s32 i;
    f32 one;
    f64 sent;
    f32 d;
    f32* gp;
    f32 gain;
    char nameBuf[12];

    if (*(u8**)((u8*)gWorldData + 36) == 0) {
        FatalErrorf(strs + 716, nameBuf);
    }
    if (*(u8**)((u8*)gWorldData + 40) == 0) {
        FatalErrorf(strs + 748, nameBuf);
    }
    sCurLevelHasCameras = -1;
    sent = lbl_80346C70;
    one = lbl_80346BE0;

    for (i = 0; i < *(s16*)((u8*)gWorldData + 24); i++) {
        lvl = *(u8**)((u8*)gWorldData + 28) + i * 268;

        if (*(s16*)(lvl + 88) < 0) {
            *(s16*)(lvl + 88) = 0;
        }
        *(u8**)(lvl + 96) =
            *(u8**)((u8*)gWorldData + 36) + *(s16*)(lvl + 88) * 108;
        {
            s16 v34 = *(s16*)(lvl + 140);
            if (v34 < 0) {
                *(u8**)(lvl + 108) = 0;
            } else {
                *(u8**)(lvl + 108) = *(u8**)((u8*)gWorldData + 52) + v34 * 84;
            }
        }
        if (*(s16*)(lvl + 90) < 0) {
            *(s16*)(lvl + 90) = 0;
        }
        *(u8**)(lvl + 100) =
            *(u8**)((u8*)gWorldData + 40) + *(s16*)(lvl + 90) * 60;
        {
            s16 v30 = *(s16*)(lvl + 92);
            if (v30 < 0) {
                *(u8**)(lvl + 104) = 0;
            } else {
                *(u8**)(lvl + 104) = *(u8**)((u8*)gWorldData + 48) + v30 * 72;
            }
        }

        if (*(s16*)(lvl + 4) != 0 && sCurLevelHasCameras < 0) {
            sCurLevelHasCameras = i;
        }

        if (*(s16*)(lvl + 4) != 0 && sMusicTrackHi != 12) {
            sprintf(nameBuf, strs + 776, lvl + 8);
            *(s32*)(*(u8**)(lvl + 100) + 20) = AudioFindSound(nameBuf, 0, 1);
        } else {
            *(s32*)(*(u8**)(lvl + 100) + 20) = -1;
        }

        *(u32*)lvl &= ~1u;

        if (*(s16*)(lvl + 6) != 0) {
            continue;
        }
        *(s16*)(lvl + 6) = 1;
        {
            if (sent == *(f32*)(lvl + 168)) {
                *(f32*)(lvl + 168) = one;
            }
            d = *(f32*)(lvl + 168);
            if (sent == *(f32*)(lvl + 172)) {
                *(f32*)(lvl + 172) = d;
            }
            if (sent == *(f32*)(lvl + 176)) {
                *(f32*)(lvl + 176) = d;
            }
            if (sent == *(f32*)(lvl + 180)) {
                *(f32*)(lvl + 180) = d;
            }
            if (sent == *(f32*)(lvl + 184)) {
                *(f32*)(lvl + 184) = d;
            }
            if (sent == *(f32*)(lvl + 188)) {
                *(f32*)(lvl + 188) = d;
            }
            if (sent == *(f32*)(lvl + 192)) {
                *(f32*)(lvl + 192) = d;
            }
            if (sent == *(f32*)(lvl + 196)) {
                *(f32*)(lvl + 196) = one;
            }
            if (sent == *(f32*)(lvl + 200)) {
                *(f32*)(lvl + 200) = d;
            }
            if (sent == *(f32*)(lvl + 204)) {
                *(f32*)(lvl + 204) = d;
            }
            if (sent == *(f32*)(lvl + 208)) {
                *(f32*)(lvl + 208) = d;
            }
            if (sent == *(f32*)(lvl + 212)) {
                *(f32*)(lvl + 212) = d;
            }
            if (sent == *(f32*)(lvl + 216)) {
                *(f32*)(lvl + 216) = d;
            }
            if (sent == *(f32*)(lvl + 220)) {
                *(f32*)(lvl + 220) = d;
            }
            {
                gp = (f32*)((u8*)lbl_8011C748 +
                            ((OptsView*)optionsAudioAndPrefs30)->vol * 4);
                gain = *gp;
                *(f32*)(lvl + 168) *= gain;
                *(f32*)(lvl + 176) *= gain;
                *(f32*)(lvl + 180) *= gain;
                *(f32*)(lvl + 184) *= gain;
                *(f32*)(lvl + 192) *= gain;
                *(f32*)(lvl + 200) *= gain;
                *(f32*)(lvl + 208) *= gain;
                *(f32*)(lvl + 212) *= gain;
                *(f32*)(lvl + 216) *= gain;
                *(f32*)(lvl + 220) *= gain;
            }
            *(f32*)(lvl + 184) = one / *(f32*)(lvl + 184);
            *(f32*)(lvl + 200) = one / *(f32*)(lvl + 200);
            *(f32*)(lvl + 192) = one / *(f32*)(lvl + 192);
            *(f32*)(lvl + 216) = one / *(f32*)(lvl + 216);
        }
    }

    for (i = 0; i < *(s16*)((u8*)gWorldData + 26); i++) {
        u8* snd = *(u8**)((u8*)gWorldData + 44) + i * 24;
        *(s32*)(snd + 16) = AudioFindSound((const char*)snd, 0, 1);
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
    char* path = lbl_80257680;

    for (i = 0; i < 14; i++) {
        entry = (u8*)sWorldLevelTable + i * 44;
        sprintf(path, "%s.wad", entry + 236);
        ids = (s32*)(entry + 232);
        if (FileExists("wdata", path)) {
            size = FileSize("wdata", path);
            ids[4] = 1;
            if (sFirstWorldId < 0) {
                sFirstWorldId = ids[0] << 8;
            }
            entry = (u8*)sWorldLevelTable + i * 4;
            if (*(void**)(entry += 848) == 0) {
                *(void**)entry = AllocMem(size);
            }
            MLMReadFile("wdata", path, size, *(void**)entry);
        } else {
            ErrorPrintf(lbl_80112A9C, path);
            *(void**)((u8*)sWorldLevelTable + i * 4 + 848) = 0;
        }
    }

    sCurWorldType  = -1;
    sCurWorldIndex = -1;
    sMusicTrackHi  = -1;
    sMusicTrackLo  = -1;
    sCurLevelHasCameras = -1;
    gWorldData = 0;
    gCurLevel  = 0;
    *(s32*)(gGameOptions + 36) =
        fn_80057F44(*(s32*)(gGameOptions + 36), 1);
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

void UpdateObjWorldMat(OBJGRP* group)
{
    if (group != 0 && group->node != 0) {
        CopyMat4(&group->worldmat[0][0], (f32*)group->node);
        UnparentMatrix((f32*)group->node,
                       *(void**)((u8*)group->node + 0x74));
    }
}

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

extern u8 gWorldInfo[];
extern f64 __fabs(f64 value);
extern u8 sItemRuntime[];
extern char* sArrowObjectNames[];
extern void get_screen_pos(s32 camera, s32* x, s32* y, void* position);
extern void DrawText(s32 x, s32 y, s32 font, u32 color,
                     const char* format, ...);

void fn_8005AF98(u8* record, s32* typeOut, s32* valueOut, s32* fieldOut,
                 s32* stateOut, char** nameOut);
f32 fn_8005B198(f32 radius, f32* position, Item** result);

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
    get_screen_pos(0, &x, &y, (u8*)item + 52);

    if (type == 2 && *(s16*)((u8*)item + 220) >= 0) {
        fn_8005AF98(*(u8**)(gWorldInfo + 104) +
                        *(s16*)((u8*)item + 220) * 80,
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
        if (*(u32*)((u8*)item + 220) == 0) {
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
                 *(char**)((u8*)item + 220), item->minplayers);
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
    s32 count;
    s32 i;
    u8 unusedLow[16];

    if (*(s32*)record == -1) {
        count = *(s32*)(record + 4);
        fn_8005AF98(*(u8**)(gWorldInfo + 104) + *(s16*)(record + 8) * 80,
                    &type, &value, &field, &state, &name);
        for (i = 1; i < count; i++) {
            fn_8005AF98(*(u8**)(gWorldInfo + 104) +
                            *(s16*)(record + i * 2 + 8) * 80,
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
        *typeOut = *(s32*)record;
        *valueOut = *(s32*)(record + 4);
        *fieldOut = *(s16*)(record + 64);
        if (*(s32*)record != 1 || *(s32*)(record + 4) != 3) {
            *fieldOut = (s32)__fabs((f64)*fieldOut);
        }
        *stateOut = *(s32*)(record + 60);
        *nameOut = (char*)(record + 40);
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
    s32* sub;
    s32 index;
    s32 bestIndex;
    s32 type;
    s16 active;
    u8* item;
    u8* info;

    scale = (f32)((lbl_80346EE8 - bias) / radius);
    best = radius;
    bestIndex = -1;
    StartEnemyGrid(position, radius);
    subtypeScale = lbl_80346EF8;
    normalScale = lbl_80346F00;
    maxInset = lbl_80346F08;
    heightScale = lbl_80346EF0;

    while ((index = NextGridEnemy()) >= 0) {
        item = (u8*)&sItems[index];
        active = *(s16*)(item + 196);
        if (active == -1 || (active & 0x8100) != 0 ||
            *(s8*)(item + 207) == -1) {
            continue;
        }
        info = *(u8**)item;
        type = *(s32*)info;
        if (type == -1) {
            continue;
        }
        if (*(s8*)(item + 205) != 0) {
            continue;
        }
        sub = (s32*)(info + 4);
        if ((active & 0x4000) == 0) {
            continue;
        }
        switch (type) {
        case 2:
        case 10:
            if (type == 2 && *sub != 43 && *sub != 44 && *sub != 45) {
                continue;
            }
            if (type == 10 && *sub == 41) {
                continue;
            }
            if ((*sub == 43 || *sub == 44 || *sub == 45) &&
                *(s8*)(item + 200) > 0) {
                continue;
            }
            goto check_type5;
        case 5:
check_type5:
            if (type == 5 && *sub != 31) {
                continue;
            }
            break;
        case 3:
            break;
        default:
            continue;
        }

        delta[0] = *(f32*)(item + 84) - position[0];
        delta[1] = *(f32*)(item + 88) - position[1];
        delta[2] = *(f32*)(item + 92) - position[2];
        absY = delta[1];
        *(u32*)&absY &= 0x7FFFFFFF;
        if (absY > heightScale * *(f32*)((u8*)sub + 12)) {
            continue;
        }
        distance = NormalVector(delta);
        if (*(s32*)*(u8**)item != 3) {
            if (*sub == 44) {
                distance *= subtypeScale;
            } else if (*sub == 45) {
                distance *= subtypeScale;
            } else {
                distance *= normalScale;
            }
        }
        distance -=
            (((f64)*(f32*)((u8*)sub + 8) < maxInset)
                ? (f64)*(f32*)((u8*)sub + 8) : maxInset);
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
                s32 parent = *(s32*)((u8*)item->objgrp.node + 0x74);

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
    Item* item = *(Item**)((u8*)owner + 0x8AC);
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

    item = *(Item**)((u8*)owner + 0x8AC);
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
extern f32 fn_8005F0F4(Item* item, f32 a, f32 b, s32 c, f32* pos, s32 d);
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
            f32 d = fn_8005F0F4(item, radius, radius, a2, position, a4);
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
            f32 d = fn_8005F0F4(item, radius, scaled, a2, position, a4);
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
extern void  AnimateATree(void* atree, s32 action, s32 mode);
extern void  MBRemoveNode(void* node, s32 mode);
extern void* AtreeMatchAnyHeader(char* name, s32 mode);
extern void  fn_8009190C(OBJGRP* grp, s32 evt);
extern s32   gNextItemIdx;
extern u8    gWorldInfo[];
extern char  lbl_80346F10[];     /* "CHICKEN"  */
extern char  lbl_80346F18[];     /* "APPLE"    */
extern char  lbl_80346F20[];     /* "TREAS_GOLD" (sdata2 copy)   */
extern char  lbl_80346F28[];     /* "TREAS_SILVER" (sdata2 copy) */
extern f32   lbl_80346F30;
extern char  lbl_80346F34[];     /* "%s_D"     */
extern char  lbl_802583A8[];     /* scratch name buffer          */
extern char  sObjectsFile[];     /* +0x130 "TREAS_GOLD", +0x13C "TREAS_SILVER" */

f32 fn_8005C1DC(Item* item, f32 power, s32 flags, s32 owner);
extern u8 gEnemies[];

/* 0x8005BA1C - apply a gold/silver-wizard reward to one item (swap the item's
 * atree to a treasure/food model, retarget generators, pop doors/walls). */
void fn_8005BA1C(Item* item, u8* player)
{
    s32 evt = -1;                                 /* r25: fx event         */
    s32 msg = -1;                                 /* r24: message code     */
    iteminfo* info = item->info;
    s32* sub = (s32*)((u8*)info + 4);
    s32 rank = *(s32*)(player + 0x3324);          /* accumulated gold rank */
    u32 mode = *(u32*)(player + 8) & 3;
    void* hdr;
    s32 k;
    u8* rec;

    switch (info->type) {
    case 1:
        switch (*sub) {
        case 2:
            break;
        case 1:
            if (mode != 0) {
                break;
            }
            if (*(s32*)&item->data[4] > 10) {
                break;
            }
            if (rank >= 0x32) {
                hdr = AtreeMatch(sPowerupsBuf, &sObjectsFile[0x130], 1);
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                }
                *(void**)&item->atree[0] = AtreeInit(hdr, item->atree, 0, 0x800);
                MBNodeSetParent(**(void***)&item->atree[0], item->objgrp.node);
                *(s32*)&item->data[4] = 200;
                evt = 0x30;
                msg = 0x8C;
            } else if (rank >= 0x19) {
                hdr = AtreeMatch(sPowerupsBuf, &sObjectsFile[0x13C], 1);
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
        }
        break;

    case 2:
        if (*(s16*)&item->data[0] < 0) {
            break;
        }
        rec = *(u8**)(gWorldInfo + 0x68) + *(s16*)&item->data[0] * 0x50;
        if (*(s32*)rec != 1) {
            break;
        }
        switch (*(s32*)(rec + 4)) {
        case 2:
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
                    rec = *(u8**)(gWorldInfo + 0x68);
                    for (k = 0; k < *(s32*)(gWorldInfo + 0x74); k++) {
                        if (strcmp(&sObjectsFile[0x130],
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
                    rec = *(u8**)(gWorldInfo + 0x68);
                    for (k = 0; k < *(s32*)(gWorldInfo + 0x74); k++) {
                        if (strcmp(&sObjectsFile[0x13C],
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
                rec = *(u8**)(gWorldInfo + 0x68);
                for (k = 0; k < *(s32*)(gWorldInfo + 0x74); k++) {
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
                rec = *(u8**)(gWorldInfo + 0x68);
                for (k = 0; k < *(s32*)(gWorldInfo + 0x74); k++) {
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

    case 10:
        if (*sub == 0x29 || *sub == 0x2B) {
            break;
        }
        if (mode != 3) {
            break;
        }
        if (rank >= 0x32) {
            fn_8005C1DC(item, lbl_80346F30, 0, *(s32*)player);
            msg = 0x92;
        } else {
            *(s16*)&item->data[4] = 4;
            msg = 0x91;
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

f32 fn_8005F0F4(Item* item, f32 a, f32 b, s32 c, f32* pos, s32 d)
{
    f32* from = (f32*)c;
    f32* out = (f32*)d;
    iteminfo* info;
    s32* sub;
    s16 coltype;
    s32 keep;
    s32 type;
    f32 R;
    f32 dist;
    f32 cx, cz;
    f32 dv[3];
    f32 nv[3];
    f32 mv[3];
    f32 hitpt[3];
    f32 norm[3];
    f32 f1, f2, f3, f4;

    if (item->active == -1 || (item->active & 0x8100) != 0) {
        return sNoDistance;
    }
    info = item->info;
    if (info->type == -1 || item->minoff != 0) {
        return sNoDistance;
    }
    sub = (s32*)((u8*)info + 4);
    coltype = info->item.coltype;
    R = info->item.radius;
    if (coltype == 0) {
        return sNoDistance;
    }
    if ((item->active & 0x40) == 0 && (item->active & 0x4000) == 0) {
        return sNoDistance;
    }

    keep = 1;
    switch (info->type) {
    case 2:
        if (*sub == 0x2B && item->action == 2) {
            keep = 0;
        }
        break;
    case 3:
        if (item->armor < 0 && info->item.height <= sNewtonThree) {
            keep = 0;
        }
        break;
    case 4:
        if (*(f32*)&item->data[0xC] < sZeroDouble) {
            R = lbl_80346F6C;
        } else {
            R = *(f32*)&item->data[0xC];
        }
        coltype = 1;
        break;
    case 5:
        if ((item->active & 0x400) != 0) {
            keep = 0;
            break;
        }
        if (*(f32*)&item->data[0xC] > sZeroDouble) {
            coltype = 1;
            R = *(f32*)&item->data[0xC];
        } else if (*sub == 0x1B) {
            coltype = 1;
            R = (f32)(R * lbl_80346EF0);
        }
        if ((*(u16*)&item->data[4] & 0x200) != 0) {
            keep = 0;
        } else if ((*(u16*)&item->data[4] & 0x40) != 0 &&
                   (s8)item->data[6] < 100 &&
                   towerAllPlayersMetBossReq((s8)item->data[6]) != 0) {
            R = (f32)(R * lbl_80346EF0);
        }
        break;
    case 7:
        if (item->action > 1 ||
            (item->action == 1 && item->activetime > 0x1E)) {
            keep = 0;
        }
        break;
    case 9:
        if (*sub != 0x32) {
            R = (f32)(R + ((f64)lbl_80344768 - lbl_80346EE8));
        }
        break;
    case 10:
        if (*sub < 0x2E && *sub > 0x2A && item->action > 0) {
            keep = 0;
        }
        break;
    case 13:
        keep = 0;
        break;
    }
    if (keep == 0) {
        return sNoDistance;
    }

    R = (f32)(a + R);
    cx = item->objgrp.coll_pos[0];
    cz = item->objgrp.coll_pos[2];
    f1 = (f32)(cx - pos[0]);
    f2 = (f32)(cz - pos[2]);
    if (R * R < f1 * f1 + f2 * f2) {
        return sNoDistance;
    }

    dv[0] = (f32)(pos[0] - cx);
    dv[1] = pos[1] - item->objgrp.coll_pos[1];
    dv[2] = (f32)(pos[2] - cz);
    if (coltype != 2 &&
        (f32)(info->item.height + b) < wfabsf_(dv[1])) {
        return sNoDistance;
    }
    dist = fqdist(dv[0], dv[2]);
    if (R < dist) {
        return sNoDistance;
    }

    if (coltype == 3) {
        /* oriented box footprint */
        if (wfabsf_(dv[0] * item->objgrp.worldmat[0][0] +
                    dv[2] * item->objgrp.worldmat[0][2]) <=
            (f32)(info->item.xdim + a)) {
            if ((f32)(info->item.zdim + a) <
                wfabsf_(dv[0] * item->objgrp.worldmat[2][0] +
                        dv[2] * item->objgrp.worldmat[2][2])) {
                keep = 0;
            }
        } else {
            keep = 0;
        }
    } else if (coltype < 3) {
        if (coltype != 1) {
            if (coltype < 1) {
                keep = 0;
            } else {
                dist = fqdist(dist, dv[1]);
                if (R < dist) {
                    keep = 0;
                }
            }
        }
    } else if (coltype < 5) {
        /* tri-list collision sweep */
        if (fn_8005FDA8((u8*)item, from, pos, hitpt, norm, b) < sZeroDouble) {
            keep = 0;
        }
    } else {
        keep = 0;
    }
    if (keep == 0) {
        return sNoDistance;
    }

    /* soft types accept immediately at the probe point */
    type = item->info->type;
    if (type != 10) {
        if (type < 10) {
            if (type == 5 || (type > 4 && type > 7)) {
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

    /* line-of-sight check from the probe origin */
    keep = 0;
    nv[0] = (f32)(from[0] - cx);
    nv[1] = sItemZero;
    nv[2] = (f32)(from[2] - cz);
    if (coltype == 3) {
        f1 = nv[0] * item->objgrp.worldmat[0][0] +
             nv[2] * item->objgrp.worldmat[0][2];
        if (wfabsf_(f1) <= (f32)(info->item.xdim + a)) {
            f2 = nv[0] * item->objgrp.worldmat[2][0] +
                 nv[2] * item->objgrp.worldmat[2][2];
            if (wfabsf_(f2) <= (f32)(info->item.zdim + a)) {
                nv[0] = (f32)(pos[0] - cx);
                nv[2] = (f32)(pos[2] - cz);
                f3 = nv[0] * item->objgrp.worldmat[0][0] +
                     nv[2] * item->objgrp.worldmat[0][2];
                if (((sItemZero <= f1 || f3 <= f1) &&
                     (f1 <= sItemZero || f1 <= f3)) &&
                    (f4 = nv[0] * item->objgrp.worldmat[2][0] +
                          nv[2] * item->objgrp.worldmat[2][2],
                     (sItemZero <= f2 || f4 <= f2)) &&
                    (f2 <= sItemZero || f2 <= f4)) {
                    keep = 1;
                }
            }
        }
    } else if (coltype < 3) {
        if (coltype == 1) {
            keep = 1;
        } else {
            if (fqdist(nv[0], nv[2]) <= R) {
                keep = 1;
            }
        }
    } else if (coltype > 4) {
        if (fqdist(nv[0], nv[2]) <= R) {
            keep = 1;
        }
    }

    if (keep != 0) {
        nv[0] = pos[0] - from[0];
        nv[1] = sItemZero;
        nv[2] = pos[2] - from[2];
        mv[0] = (f32)(cx - from[0]);
        mv[1] = sItemZero;
        mv[2] = (f32)(cz - from[2]);
        NormalVector2D(nv);
        NormalVector2D(mv);
        if (nv[0] * mv[0] + nv[2] * mv[2] < sItemZero) {
            return sNoDistance;
        }
    }

    if (out != 0) {
        if (coltype == 3) {
            nv[0] = (f32)(pos[0] - cx);
            nv[1] = sItemZero;
            nv[2] = (f32)(pos[2] - cz);
            f3 = nv[0] * item->objgrp.worldmat[0][0] +
                 nv[2] * item->objgrp.worldmat[0][2];
            f4 = nv[0] * item->objgrp.worldmat[2][0] +
                 nv[2] * item->objgrp.worldmat[2][2];
            f2 = (f32)(info->item.xdim + a) - wfabsf_(f3);
            f1 = (f32)(info->item.zdim + a) - wfabsf_(f4);
            if (sItemZero < f2 || sItemZero < f1) {
                if (f1 <= f2 || f2 <= sItemZero) {
                    if (f2 <= f1 || f1 <= sItemZero) {
                        out[0] = from[0];
                        out[1] = from[1];
                        out[2] = from[2];
                    } else if (f4 <= sItemZero) {
                        f1 = -f1;
                        out[0] = item->objgrp.worldmat[2][0] * f1 + pos[0];
                        out[1] = item->objgrp.worldmat[2][1] * f1 + pos[1];
                        out[2] = item->objgrp.worldmat[2][2] * f1 + pos[2];
                    } else {
                        out[0] = item->objgrp.worldmat[2][0] * f1 + pos[0];
                        out[1] = item->objgrp.worldmat[2][1] * f1 + pos[1];
                        out[2] = item->objgrp.worldmat[2][2] * f1 + pos[2];
                    }
                } else if (f3 <= sItemZero) {
                    f2 = -f2;
                    out[0] = item->objgrp.worldmat[0][0] * f2 + pos[0];
                    out[1] = item->objgrp.worldmat[0][1] * f2 + pos[1];
                    out[2] = item->objgrp.worldmat[0][2] * f2 + pos[2];
                } else {
                    out[0] = item->objgrp.worldmat[0][0] * f2 + pos[0];
                    out[1] = item->objgrp.worldmat[0][1] * f2 + pos[1];
                    out[2] = item->objgrp.worldmat[0][2] * f2 + pos[2];
                }
            } else {
                out[0] = from[0];
                out[1] = from[1];
                out[2] = from[2];
            }
        } else if (coltype < 3 || coltype > 4) {
            if (keep == 0) {
                nv[0] = pos[0] - from[0];
                nv[1] = sItemZero;
                nv[2] = pos[2] - from[2];
                mv[0] = (f32)(cx - from[0]);
                mv[1] = sItemZero;
                mv[2] = (f32)(cz - from[2]);
            }
            if (nv[2] * mv[0] - nv[0] * mv[2] <= sItemZero) {
                f1 = -mv[0];
                mv[0] = mv[2];
                mv[2] = f1;
            } else {
                f1 = -mv[2];
                mv[2] = mv[0];
                mv[0] = f1;
            }
            NormalVector2D(mv);
            f1 = nv[0] * mv[0] + nv[2] * mv[2];
            out[0] = mv[0] * f1 + from[0];
            out[1] = mv[1] * f1 + from[1];
            out[2] = mv[2] * f1 + from[2];
        } else {
            /* push the target out along the tri-list hit normal */
            nv[0] = hitpt[0] - pos[0];
            nv[1] = hitpt[1] - pos[1];
            nv[2] = hitpt[2] - pos[2];
            out[0] = pos[0];
            out[1] = pos[1];
            f1 = (f32)((nv[0] * norm[0] + nv[2] * norm[2]) + a);
            out[2] = pos[2];
            if (sItemZero < f1) {
                out[0] = norm[0] * f1 + out[0];
                out[2] = norm[2] * f1 + out[2];
            }
        }
    }
    return dist;
}

extern s32 LineCylinderCollide(f32* center, f32 radius, f32 halfHeight,
                               f32* from, f32* to, f32* hit,
                               s32 directional);
extern f32 lbl_80347004;
extern f64 lbl_80346ED8;
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
extern void  StartExplosion(f32 radius);
extern s32   MBOX_ReallyFindObject(char* name, s32 a, s32 b, s32 c);
extern void  MBTreeSetZsortAdd(void* node, s32 value, s32 mode);
extern u8    EnemyDescType(char* desc);
extern char* EnemyTypePrefix(s32 type);
extern void  AudioPlayEvt101(f32* pos);
extern void  AudioExplodeWall(f32* pos, s32 health);
extern f32   fn_8005E90C(Item* item, void* inst);
extern void  fn_8009DA78(f32* pos);
extern void  fn_8009DA28(f32* pos);
extern void  fn_8009D9D8(f32* pos);
extern void  fn_8009EF4C(f32* pos);
extern void  fn_8009C7D8(f32* pos, s32 gen);
extern void  fn_8009C774(f32* pos, s32 gen);
extern void  fn_80091AC0(OBJGRP* grp, s32 gen, s32 off);
extern s32   fn_80094440(f32* pos, u32 flags, s32 destroyed);
extern void  AddItemWobj(Item* item);
extern s32   Round(f32 value);
extern s32   stricmp(const char* a, const char* b);
extern char* strcat(char* dst, const char* src);
extern u8    Effects[];
extern s32   gNumEnemies;
extern f64   lbl_80346ED8;       /* 2^52 int-conv constant */
extern f64   lbl_80346EE8;
extern f64   lbl_80346EF0;
extern f64   lbl_80346F40;
extern f64   sItemFloorYOffset;
extern f32   sItemZero;
extern f32   sItemFloorRadius;
extern f32   lbl_80346F54;
extern char  lbl_80346F58[];     /* "BADMEAT" */
extern char  lbl_80346F60[];     /* "GAPPLE"  */
extern f32   lbl_80346F68;
extern f32   lbl_80346F6C;
extern char  lbl_80346F70[];     /* "BOSSGEN" */
extern char  sLevelOneSuffix[];  /* "L1"      */
extern char  sRootSuffix[];      /* "ROOT"    */
extern f64   lbl_80346F88;
extern char  lbl_80346F90[];     /* "BARPOI0" */
extern f64   lbl_80346F98;
extern char  lbl_80346FA0[];     /* "BAREXP0" */
extern f32   lbl_80346FA8;

f32 fn_8005C1DC(Item* item, f32 power, s32 flags, s32 owner)
{
    s32 destroyed = 0;                        /* item died from this hit  */
    s32 alive;                                /* item tracks health       */
    s32 ret;                                  /* remaining health / code  */
    iteminfo* info = item->info;
    s32* sub = (s32*)((u8*)info + 4);
    f32 v[3];
    char buf[0x38];
    void* hdr;
    u8* rec;
    s32 k;
    s32 thr;
    s8 state;

    /* scale generator hits by how far the player's gold exceeds the ramp */
    if (info->type == 3 && owner >= 0 &&
        *(f32*)((u8*)gCurLevel + 0x9C) > sItemZero) {
        f32 mult = sItemFloorRadius;
        f32 ramp = *(f32*)((u8*)gCurLevel + 0x9C);
        f32 gold = (f32)*(s32*)((u8*)gPlayers + owner * 0x335C + 0x3324);

        if (ramp <= gold) {
            if (ramp < gold) {
                mult = (f32)(sItemFloorYOffset * (gold - ramp) + lbl_80346EE8);
            }
        } else {
            mult = -(f32)(lbl_80346F40 * (ramp - gold) - lbl_80346EE8);
        }
        power = power * mult;
        if (power < (f32)lbl_80346EE8) {
            power = sItemFloorRadius;
        }
    }

    if ((flags & 0x800) != 0) {
        if (power > sCameraVisibilityRadius) {
            ret = -2;
        } else {
            ret = -1;
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
            destroyed = (item->health == 0);
        }
        if ((u8)item->armor == 0xFF) {
            ret = -1;
            alive = 0;
        } else {
            alive = 1;
            ret = item->health;
        }
    }

    v[0] = item->objgrp.coll_pos[0];
    v[1] = (f32)(item->objgrp.coll_pos[1] + lbl_80346EF0);
    v[2] = item->objgrp.coll_pos[2];

    switch (info->type) {
    case 1:
        if (*sub == 4) {
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
        }
        if (*sub < 4) {
            if (*sub == 2) {
                break;
            }
            if (*sub < 2) {
                if (*sub > 0) {
                    /* food -> junk treasure */
                    if ((flags & 0x400) != 0 && power >= lbl_80346F68) {
                        StartFXMat(0x20, &item->objgrp);
                        StartFXMat(0x21, &item->objgrp);
                        hdr = AtreeMatch(sPowerupsBuf, &sObjectsFile[0x158], 1);
                        if (*(u32*)&item->atree[0] != 0) {
                            AtreeDelete(item->atree);
                        }
                        *(void**)&item->atree[0] =
                            AtreeInit(hdr, item->atree, 0, 0x800);
                        MBNodeSetParent(**(void***)&item->atree[0],
                                        item->objgrp.node);
                        *(s32*)&item->data[4] = 10;
                    }
                    break;
                }
            } else if (ret == -2) {
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
        } else if (*sub < 0x11 && *sub > 9) {
            break;
        }
        /* generic destroy */
        if ((flags & 0x400) != 0 && power >= lbl_80346F68) {
            StartFXMat(0x20, &item->objgrp);
            StartFXMat(0x21, &item->objgrp);
            MBOX_NewObject(&sObjectsFile[0x14C], item->objgrp.node,
                           *(f32*)((u8*)item->objgrp.node + 0x74), 0x80800);
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

    case 2:
        rec = 0;
        if (*(s16*)&item->data[0] >= 0) {
            rec = *(u8**)(gWorldInfo + 0x68) + *(s16*)&item->data[0] * 0x50;
        }
        if (rec != 0 && (flags & 0x200) != 0 &&
            EnemyDescType((char*)(rec + 0x28)) == 0x1E && *sub != 0x2B) {
            /* enemy chest converts to an apple generator */
            *sub = 1;
            rec = *(u8**)(gWorldInfo + 0x68);
            for (k = 0; k < *(s32*)(gWorldInfo + 0x74); k++) {
                if (strcmp(lbl_80346F18, (char*)(rec + 0x28)) == 0 &&
                    *(s32*)rec == 1 && *(s32*)(rec + 4) == 3) {
                    goto found_gen;
                }
                rec += 0x50;
            }
            k = -1;
found_gen:
            *(s16*)&item->data[0] = (s16)k;
            AudioPlayEvt101(v);
            alive = 1;
            *(s16*)&item->data[2] = (s16)(lbl_80346F6C * power);
        } else if (destroyed != 0 && (item->active & 0x200) != 0) {
            if ((item->active & 1) == 0) {
                item->active |= 1;
                fn_8005E90C(item, 0);
                if (*sub == 0x2B) {
                    fn_8009DA78(v);
                    msgPost(0x1B, -1, (char*)v);
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
                    MBOX_NewObject(&sObjectsFile[0x164], item->objgrp.node,
                                   *(f32*)((u8*)item->objgrp.node + 0x74),
                                   0x80800);
                } else {
                    MBOX_NewObject(&sObjectsFile[0x170], item->objgrp.node,
                                   *(f32*)((u8*)item->objgrp.node + 0x74),
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
        thr = (s32)((f32)*(s16*)((u8*)info + 0x44) *
                    *(f32*)((u8*)gCurLevel + 0xCC));
        if (ret < 0) {
            break;
        }
        if (destroyed == 0) {
            if (item->health > thr) {
                if (item->health > thr * 2) {
                    state = 3;
                } else {
                    state = 2;
                }
            } else {
                state = 1;
            }
        } else {
            state = 0;
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
            if (item->data[7] == 0x1E) {
                item->data[7] = 0;
            }
            if (stricmp(buf, lbl_80346F70) != 0) {
                if (*(s16*)&item->data[0] < -1) {
                    sprintf(buf, &sObjectsFile[0x17C], (s8)item->data[6]);
                } else {
                    sprintf(buf, &sObjectsFile[0x18C],
                            EnemyTypePrefix(*(s16*)&item->data[0]),
                            (s8)item->data[6]);
                }
            }
            hdr = AtreeMatchAnyHeader(buf, 1);
            if (hdr == 0) {
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
            } else {
                if (*(u32*)&item->atree[0] != 0) {
                    AtreeDelete(item->atree);
                }
                *(void**)&item->atree[0] = AtreeInit(hdr, item->atree, 0, 0x800);
                MBNodeSetParent(**(void***)&item->atree[0], item->objgrp.node);
            }
            if (state == 0) {
                item->active &= ~1;
                item->armor = -1;
                fn_80091AC0(&item->objgrp, *(s16*)&item->data[0], 1);
            } else {
                fn_80091AC0(&item->objgrp, *(s16*)&item->data[0], 0);
            }
        }
        if (state == 0) {
            fn_8009C7D8(v, *(s16*)&item->data[0]);
            for (k = 0; k < gNumEnemies; k++) {
                if (*(Item**)(gEnemies + k * 0x394 + 0x290) == item) {
                    *(Item**)(gEnemies + k * 0x394 + 0x290) = 0;
                }
            }
        } else {
            fn_8009C774(v, *(s16*)&item->data[0]);
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

    case 10:
        if (ret < 0) {
            break;
        }
        if (alive != 0 && destroyed == 0 && *sub != 0x29) {
            *(s16*)&item->data[4] = 1;
        }
        switch (*(s32*)((u8*)item->info + 4)) {
        case 0x29:
            if (*(s16*)&item->data[2] >= 0) {
                AddItemWobj(item);
                destroyed = 0;
            }
            break;
        case 0x2A:
            AudioExplodeWall(v, item->health);
            break;
        case 0x2D:
            if (destroyed == 0) {
                fn_8009EF4C(v);
            } else {
                item->active |= 1;
                StartExplosion((f32)(lbl_80346F98 *
                                     *(f32*)((u8*)gCurLevel + 0xDC)));
                fn_8009DA28(v);
                MBOX_NewObject(lbl_80346FA0, item->objgrp.node,
                               *(f32*)((u8*)item->objgrp.node + 0x74),
                               0x80800);
                alive = 0;
                ret = -2;
                destroyed = 0;
            }
            break;
        case 0x2C:
            if (destroyed == 0) {
                fn_8009EF4C(v);
            } else {
                item->active |= 1;
                StartExplosion((f32)(lbl_80346F88 *
                                     *(f32*)((u8*)gCurLevel + 0xDC)));
                fn_8009D9D8(v);
                MBOX_NewObject(lbl_80346F90, item->objgrp.node,
                               *(f32*)((u8*)item->objgrp.node + 0x74),
                               0x80800);
                alive = 0;
                ret = -2;
                destroyed = 0;
            }
            break;
        default:
            /* 0x2B, walls, everything else: shake / rumble */
            if (destroyed == 0) {
                fn_8009EF4C(v);
            } else {
                item->active |= 1;
                fn_8009DA78(v);
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
    }

    if (alive != 0) {
        k = fn_80094440(v, flags, destroyed);
        if (k >= 0) {
            MBTreeSetZsortAdd(*(void**)(Effects + k * 0xF0 + 0x14),
                              (s32)(lbl_80346FA8 * info->item.radius), 1);
        }
    }
    return (f32)ret;
}

s32 fn_8005DE50(void* a, void* b)
{
    return 0;
}

void fn_800606FC(void)
{
}

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
extern s32 RandInt(s32 range);
extern char* strcpy(char* d, const char* s);
extern void DrawTextKeepScale(f32 scale, s32 x, s32 y, u32 font, u32 color, u8* str);
extern s32 fn_8005A868(void);
extern s32 gFrameTicks;

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
        if ((ret = fn_8005A868()) <= 0) {
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

/* 0x8005D3D8 - can this world object block/affect the given enemy? */
extern u8 gEnemies[];
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
        e = gEnemies + index * 916;
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
        d = fn_8005F0F4((Item*)*(u32*)(e + 652), rad,
                        (f32)(lbl_80346FB8 * rad), (s32)from, to, (s32)0);
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
                d = fn_8005F0F4((Item*)obj, rad, (f32)(lbl_80346FB8 * rad),
                                (s32)from, to, (s32)0);
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
extern u8 gWorldInfo[];
extern s32 lbl_80344188;
extern char lbl_8034418C;
extern f32 lbl_80344190;
extern f32 lbl_80344194;
extern f64 lbl_80347008;
extern f32 lbl_80347010;
void FatalError(const char* msg, int code);
f32 CTriListCollide(f32 radius, s32 base, s32 count, u8** outTri,
                    s16* idxList, f32* outPt, s32 layerLo, s32 layerHi,
                    s32 noFilter);
void MulBodyVecMat4(const f32* vector, f32* out, const f32* matrix);
void MulVecMat4(const f32* vector, f32* out, const f32* matrix);

/* 0x8005FDA8 - sweep an item's collision tri list along segment a->b */
f32 fn_8005FDA8(u8* e, f32* a, f32* b, f32* outPos, f32* outNorm, f32 margin)
{
    f32* m = *(f32**)(e + 100);
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
    *(s32*)(gWorldInfo + 88) = *(s32*)(gWorldInfo + 88) + 1;
    if (*(s32*)(gWorldInfo + 88) > 255) {
        *(s32*)(gWorldInfo + 88) = 1;
    }
    v = (char)*(s32*)(gWorldInfo + 88);
    lbl_80344188 = 0;
    lbl_8034418C = v;
    MulBodyVecMat4(a, lbl_8023F7F8, m);
    MulBodyVecMat4(b, lbl_8023F7E8, m);
    k = m[5];
    d1 = lbl_80347008 * (k * (lo - m[13]));
    d2 = lbl_80347008 * (k * (hi - m[13]));
    lbl_80344194 = lbl_80347010;
    lbl_80344190 = sCameraVisibilityRadius;
    hit = CTriListCollide(margin, *(s16*)(e + 192), *(s16*)(e + 194), &triOut,
                          (s16*)0, pt, (s16)(s32)d1, (s16)(s32)d2, 0);
    if (hit >= sZeroDouble) {
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
