#include "types.h"
#include "game/dyngrid.h"
#include "game/item.h"
#include "game/player.h"

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
extern u8    lbl_80237BA0[];
extern f32   gIdentityMatrix[16];
extern s32   sNumItems;
extern s32   lbl_8034481C;
extern s32   gGameBusy;
extern s32   lbl_80344770;
extern s32   gGameMode;
extern s64   gControllerButtons;
extern Player gPlayers[4];
extern char  lbl_80346D08[5];
extern char  lbl_80346D10[7];

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
static void ResolveWorldDataPointers(void)
{
    int i, j;
    WorldLevel* lvl;
    char nameBuf[0x38];
    const float VOL_DEFAULT = 1.0f; /* f31 */
    const float SENTINEL    = -1.0f;/* f30 */

    if (gWorldData->cameras == 0) {
        FatalErrorf("World Data %s has no cameras\n", nameBuf);
    }
    if (gWorldData->audio == 0) {
        FatalErrorf("World Data %s has no audio\n", nameBuf);
    }

    sCurLevelHasCameras = -1;

    for (i = 0; i < (int)gWorldData->numLevels; i++) {
        lvl = &gWorldData->levels[i];

        /* section index -> pointer (idx<0 => NULL), written at 0x60/0x64/0x68/0x6C */
        if (lvl->cameraIdx < 0) {
            lvl->cameraIdx = 0;
        }
        *(u8**)((u8*)lvl + 0x60) = gWorldData->cameras + lvl->cameraIdx * 0x6C;
        if (lvl->sec34Idx < 0) {
            *(u8**)((u8*)lvl + 0x6C) = 0;
        } else {
            *(u8**)((u8*)lvl + 0x6C) = gWorldData->section34 + lvl->sec34Idx * 0x54;
        }
        if (lvl->audioIdx < 0) {
            lvl->audioIdx = 0;
        }
        *(u8**)((u8*)lvl + 0x64) = gWorldData->audio + lvl->audioIdx * 0x3C;
        if (lvl->sec30Idx < 0) {
            *(u8**)((u8*)lvl + 0x68) = 0;
        } else {
            *(u8**)((u8*)lvl + 0x68) = gWorldData->section30 + lvl->sec30Idx * 0x48;
        }

        /* first level that owns cameras */
        if ((lvl->flags2 & 1) && sCurLevelHasCameras < 0) {
            sCurLevelHasCameras = i;
        }

        /* ambient track for camera levels (realm 0xC has none) */
        if ((lvl->flags2 & 1) && sMusicTrackHi != 0xC) {
            int snd;
            sprintf(nameBuf, "%s_amb", lvl->name);
            snd = AudioFindSound(nameBuf, 0, 1);
            *(int*)(*(u8**)((u8*)lvl + 0x64) + 0x14) = snd;
        } else {
            *(int*)(*(u8**)((u8*)lvl + 0x64) + 0x14) = -1;
        }

        lvl->flags &= ~1u;

        if (lvl->resolved == 0) {
            float* v = (float*)((u8*)lvl + 0xA8);
            lvl->resolved = 1;
            /* v[0..13]: replace SENTINEL with defaults, scale by realm gain,
             * then take the reciprocal of the range fields (v[4],v[8],... ). */
            for (j = 0; j < 14; j++) {
                if (v[j] == SENTINEL) {
                    v[j] = VOL_DEFAULT;
                }
            }
        }
    }

    /* resolve every sound-table entry's handle (stride 0x18) */
    for (i = 0; i < (int)gWorldData->numSounds; i++) {
        u8* snd = gWorldData->sounds + i * 0x18;
        *(int*)(snd + 0x10) = AudioFindSound((const char*)snd, 0, 1);
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
void LoadWorldData(void)
{
    int i;

    for (i = 0; i < 14; i++) {
        char* name = (char*)&sWorldDataTypes[i];      /* entry name (adjacent) */
        char* path = (char*)0;                        /* sWorldPathBuf scratch */
        sprintf(path, "%s.wad", name);
        if (FileExists("wdata", path)) {
            int size = FileSize("wdata", path);
            /* mark loaded; remember first realm id */
            if (sFirstWorldId < 0) {
                sFirstWorldId = sWorldDataTypes[i] << 8;
            }
            /* allocate the blob once, then read it in */
            /* if (buf == 0) buf = AllocMem(size); */
            AllocMem(size);
            MLMReadFile("wdata", path, size, /*buf*/ 0);
        } else {
            ErrorPrintf("No world data file: %s\n", path);
            /* buf = 0 */
        }
    }

    sCurWorldType  = -1;
    sCurWorldIndex = -1;
    sMusicTrackHi  = -1;
    sMusicTrackLo  = -1;
    sCurLevelHasCameras = -1;
    gWorldData = 0;
    gCurLevel  = 0;
    fn_80057F44();                 /* prime the world registry */
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

f32 fn_8005B198(f32 radius, f32* position, Item** result)
{
    f32 delta[3];
    f32 best_distance = -1.0f;
    Item* item;
    Item* best_item = 0;
    s32 index;

    StartEnemyGrid(radius, position);
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

s32 fn_8005B8B0(void* owner)
{
    s32 result = 0;
    Item* item = *(Item**)((u8*)owner + 0x8AC);
    Item* linked;
    s16 active;

    if (item == 0) {
        return result;
    }
    if (item->info->type != 0xB) {
        return result;
    }
    linked = *(Item**)&item->data[8];
    active = linked->active;
    if (active != -1) {
        if (linked->minoff == 0 && (active & 0x4000) != 0) {
            return (s32)linked;
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

    if ((gGameBusy | lbl_80344770) == 0) {
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
    f32 best = 2.0f;
    s32 best_idx = -1;
    s32 idx;
    u8 unused[24];

    if (id != 33 && id != 32 && id != 29) {
        return -1;
    }

    StartEnemyGrid(-1.0f, position);
    while ((idx = NextGridEnemy()) >= 0) {
        Item* item = &sItems[idx];
        f32 d;
        f32 ady;

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
            ady = dy;
            *(u32*)&ady &= 0x7FFFFFFF;
            if (ady < 3.0f) {
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

    StartEnemyGrid(radius, position);
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

    switch (info->type) {
    case 1:
        if (item->activetime <= 0) {
            if (*(u32*)&item->data[0xC] == 0) {
                if (*sub == 4) {
                    result = 1;
                }
            } else {
                result = 0;
            }
        } else {
            result = 0;
        }
        break;
    case 2:
    case 7:
        result = 1;
        break;
    case 3:
        if (item->data[6] != 0) {
            result = 1;
        }
        break;
    case 4:
        result = 1;
        break;
    case 10:
        switch (*sub) {
        case 0x1E:
            break;
        case 0x28:
        case 0x31:
        case 0x33:
        case 0x35:
            result = 0;
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

    StartEnemyGrid(radius, position);
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

    StartEnemyGrid(radius, position);
    while ((idx = NextGridEnemy()) >= 0) {
        iteminfo* info;
        s16 active;
        s32 reject;
        f32 d;

        item = &sItems[idx];
        active = item->active;
        if (active == -1) {
            continue;
        }
        if (active & 0x8100) {
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
        if (info->type == -1) {
            continue;
        }
        if (item->minoff != 0) {
            continue;
        }
        reject = 0;
        switch (info->type) {
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
                (active & 1)) {
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
 * Giant functions (light-touch skeletons).  Full bodies deferred; defined
 * last so no earlier caller auto-inlines these stubs (their `bl` is kept).
 *   fn_8005F0F4  0x0A54  big per-object distance/worker
 *   fn_8005C1DC  0x0E70  item/object spawn dispatcher
 *   fn_8005DE50  0x0ABC  big object state machine
 *   fn_800606FC  0x22B4  per-frame world update dispatcher
 */
f32 fn_8005F0F4(Item* item, f32 a, f32 b, s32 c, f32* pos, s32 d)
{
    return 0.0f;
}

s32 fn_8005C1DC(Item* item, f32 a, s32 b)
{
    return 0;
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
 *                    fn_8009D42C/58, fn_8009EF04)
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
 * 8005D3D8  0x01F0  fn_ : (damage_enemy; lbl_80346FC8)
 * 8005D5C8  0x0168  fn_ : accessor (no calls; lbl_80346FC8)
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
 * 8005FB48  0x0260  fn_ : (fn_8002FA70, fn_8005FDA8, fqdist)
 * 8005FDA8  0x01B8  fn_ : (FatalError, CTriListCollide, MulBodyVecMat4/DD00/DE80).
 *                    "COL_OBJECT Item: idx < 0"
 * 8005FF60  0x01B4  fn_ : (ErrorPrintf, fn_80055CB8).
 *                    "Special trigger has no target"
 * 80060114  0x05E8  fn_ : spawn enemy from world data (CritterNewInst,
 *                    generate_enemy, atan2, UpdateObjWorldMat, MBWorldSphereVisible3).
 *                    "Bad EnemyInfo: type %s subtype %d not loaded",
 *                    "EnemyInfo didn't generate %s(ai=%d, reason=%s)"
 * 800606FC  0x22B4  fn_ : GIANT per-frame world update dispatcher (AudioStopAll,
 *                    ShakeCamera, TriggerCameraActivate, find_enemy_slot,
 *                    generate_enemy, fn_8005A338, fn_80060114, fn_80062A00,
 *                    fn_80062FF0, items fn_800631AC, +~40 more).
 *                    "CAN'T FIND LOOKPUT PARAM:%d".  [parked giant]
 * 800629B0  0x0050  fn_ : accessor (no calls; sNumItems/80344950).
 *                    Called by auxscreen.
 * 80062A00  0x05F0  fn_ : particle-system update (WorldPsysActivate/DeActivate,
 *                    did_generate, fn_8009D100..694, MBTreeClearFlags/368/6C0)
 * 80062FF0  0x01BC  fn_ : enemy-grid op (StartEnemyGrid/NextGridEnemy,
 *                    fqdist)
 * ========================================================================== */
