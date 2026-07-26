#include "types.h"

/* Slice of the Gauntlet world-data module (Xbox gauntworld.obj) assigned to
 * region 0x80058078-0x800631AC.  The full gauntworld TU begins before this
 * region (fn_80057F44 etc. are called from here) and continues past it; this
 * file is the middle slice, wired NonMatching so the tree stays green while
 * the symbols are mapped.
 *
 * IDENTIFIED (real Xbox-PDB names, high confidence from strings + call graph):
 *   LoadWorldData            fn_8005A098  - loads the 14 realm world-data WAD
 *                                           files into sWorldDataBufs[]
 *   ResolveWorldData         fn_80058078  - byte-swaps a realm's loaded blob
 *                                           for the big-endian GameCube and
 *                                           resolves it; public API (9 callers)
 *   ResolveWorldDataPointers fn_80059CB4  - static fix-up pass called only by
 *                                           ResolveWorldData: turns section
 *                                           indices into pointers, scales audio
 *
 * The remaining ~42 functions in the slice are gameplay/world helpers left as
 * fn_XXXXXXXX (candidates listed in the session report).  Only the three
 * world-data-core functions are reconstructed here; matching is out of scope
 * for this pass (high mismatch accepted).  The per-field byte-swap body of
 * ResolveWorldData is documented rather than transcribed. */

/* --- world-data file/archive API (fn_800BFxxx) --- */
extern int fn_800BF70C(); /* archive/file open -> handle (0 = missing) */
extern int fn_800BF618(); /* query size */
extern int fn_800BF2C4(); /* allocate buffer */
extern void fn_800BF97C(); /* read into buffer */
extern int fn_800BEA84(); /* decompress helper */
extern int fn_800BE9F8(); /* section lookup within blob */
extern int fn_800BC590(); /* ErrorPrintf-style logger (returns varargs cursor) */
extern int AudioFindSound(); /* audio bank lookup by name */
extern int fn_80057F44(); /* world registration hook (prior slice) */
extern int sprintf(char* buf, const char* fmt, ...);
extern int ErrorPrintf(const char* fmt, ...);

/* --- module data (see symbols.txt for the mapped names) --- */
extern s32 sWorldLevelTable[];  /* 0x8011C3C0 world/level id pairs */
extern s32 sWorldDataTypes[];   /* 0x8011C4A8 per-realm world-data type ids */
extern s32 sWorldDataLoaded[];  /* 0x8011C4B8 per-realm loaded flag */
extern char sWorldNames[];      /* 0x8011C4AC "castle".. names, stride 0x2C */
extern void* sWorldDataBufs[];  /* 0x8011C710 per-realm loaded blob buffers */
extern char sWorldPathBuf[];    /* 0x80257680 scratch path buffer */
extern char sWorldPathFmt[];    /* 0x80346CF8 path format string */
extern void* sArchiveCtx;       /* 0x80346D00 archive context */

extern s32* gWorldData;         /* 0x80344838 active world-data header */
extern u8* gCurLevel;           /* 0x8034483C active level descriptor */
extern s32 sCurWorldType;       /* 0x80343C28 cached resolved world-data type */
extern s32 sFirstWorldId;       /* 0x80343C2C first loaded realm id (<<8) */
extern s32 sCurWorldIndex;      /* 0x80344844 index into type table */
extern s32 sLastWorldLevel;     /* 0x80344820 last resolved worldlevel id */
extern s32 sCurLevelHasCameras; /* 0x803448C0 first level index with cameras */
extern s32 gCurWorldLevel;      /* 0x803448D4 current level within realm */
extern s32 gCurWorldDataType;   /* 0x803448D8 current realm data type */
extern s32 sWorldDataConst;     /* 0x80344848 = 0xD00 */
extern s32 lbl_802575B4;

/* forward decl of the static fix-up pass */
static void ResolveWorldDataPointers(void);

/* Load every realm's world-data file into sWorldDataBufs[].  Called from the
 * game/attract setup paths (3 callers).  On a missing file it logs
 * "No world data file %s" and leaves the buffer NULL. */
void LoadWorldData(void)
{
    int i;
    char* name;
    int handle;

    for (i = 0; i < 14; i++) {
        name = &sWorldNames[i * 0x2C];
        sprintf(sWorldPathBuf, sWorldPathFmt, name);
        handle = fn_800BF70C(&sArchiveCtx, sWorldPathBuf);
        if (handle == 0) {
            ErrorPrintf("No world data file %s\n", sWorldPathBuf);
            sWorldDataBufs[i] = 0;
        } else {
            fn_800BF618(&sArchiveCtx, sWorldPathBuf);
            sWorldDataLoaded[i] = 1;
            if (sFirstWorldId < 0) {
                sFirstWorldId = sWorldDataTypes[i] << 8;
            }
            if (sWorldDataBufs[i] == 0) {
                sWorldDataBufs[i] = (void*)fn_800BF2C4(handle, sWorldPathBuf);
            }
            fn_800BF97C(&sArchiveCtx, sWorldPathBuf, handle, sWorldDataBufs[i]);
        }
    }

    sCurWorldType = -1;
    sCurWorldIndex = -1;
    gCurWorldDataType = -1;
    gCurWorldLevel = -1;
    sCurLevelHasCameras = -1;
    gWorldData = 0;
    gCurLevel = 0;
    lbl_802575B4 = fn_80057F44(lbl_802575B4, 1, -1);
    sWorldDataConst = 0xD00;
}

/* Resolve (activate) a realm+level: locate its loaded blob, byte-swap all of
 * the packed structures for the big-endian GameCube, fix up the section
 * pointers, and cache the current world/level globals.
 *
 * worldlevel = (realmType << 8) | levelIndex.
 *
 * NOTE: the bulk of the real function is the per-field endian swap of the
 * level array (stride 0x10C), the camera array (0x6C), the audio array (0x3C)
 * and the string/anim tables.  That mechanical swap is intentionally omitted
 * in this NonMatching slice; only the control flow and the resolve/cache tail
 * are reconstructed. */
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
            if (sWorldDataLoaded[i] == 0) {
                fn_800BC590("No world data %s\n", &sWorldNames[i * 0x2C]);
                break;
            }
            /* decompress + fetch header, byte-swap the whole blob, then: */
            gWorldData = (s32*)fn_800BE9F8();
            /* ... endian swap of levels/cameras/audio/strings (omitted) ... */
            ResolveWorldDataPointers();
            sCurWorldType = realm;
            sCurWorldIndex = i;
            break;
        }
    }

    if (gWorldData == 0) {
        return;
    }
    if (level >= (int)(*(s16*)((u8*)gWorldData + 0x18))) {
        level = 0;
    }
    *(s16*)((u8*)gWorldData + 0x16) = (s16)level;
    gCurLevel = (u8*)gWorldData[7] + level * 0x10C;
    sLastWorldLevel = worldlevel;
    gCurWorldLevel = level;
    gCurWorldDataType = realm;
}

/* Turn the loaded section indices into absolute pointers and normalise the
 * per-level audio volumes.  Static: called only by ResolveWorldData. */
static void ResolveWorldDataPointers(void)
{
    int i;
    u8* base = (u8*)gWorldData;
    int levels = *(s16*)(base + 0x18);
    u8* lvl;

    if (*(s32*)(base + 0x24) == 0) {
        fn_800BC590("World Data %s has no cameras\n");
    }
    if (*(s32*)(base + 0x28) == 0) {
        fn_800BC590("World Data %s has no audio\n");
    }

    sCurLevelHasCameras = -1;
    for (i = 0; i < levels; i++) {
        lvl = (u8*)gWorldData[7] + i * 0x10C;
        /* section index -> pointer resolution for cameras/audio/anim/etc.
         * and per-channel volume scaling live here (see fn_80059CB4). */
        if (*(s16*)(lvl + 4) != 0 && sCurLevelHasCameras < 0) {
            sCurLevelHasCameras = i;
        }
    }
}
