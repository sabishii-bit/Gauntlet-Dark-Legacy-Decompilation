#include "types.h"
#include "game/player.h"
#include "game/tower.h"

/* Gauntlet Dark Legacy - TOWER module (Xbox TOWER.OBJ), region
 * 0x800A18E8-0x800A4870 in the GC build (between game/sound/sounds.c and
 * game/ui/message.c).  This is the hub-tower / "Sumner" (good wizard)
 * subsystem: per-player world/rune/shard/crystal/garg-item progress
 * bookkeeping, world-unlock queries, the tower-message dispatcher served
 * per-frame by do_players (TowerCheckMessages), and the Sumner speech /
 * good-wizard cinematic (captions + scroll/string text + audio + camera +
 * rune/shard/wizard effects).
 *
 * Module id CONFIRMED via strings ("WIZARD","SHARD%d","RUNE%d","RUNE13",
 * "GWIZ"), call set (CaptionText, GetScrollText, AtreeMatch, FindWORLDOBJ,
 * InitCustomEffect, TriggerCameraEnd, FindLookoutParam, ResolveWorldData) and
 * behaviour.  Xbox TOWER.OBJ has 43 functions; the GC TU has 36 (the small
 * Player{Has,Give,Toggle}{Rune,Shard,Crystal,GargItem} accessors were
 * inlined by MWCC here).
 *
 * NonMatching: this TU is not linked (dtk uses the original extracted bytes);
 * the reconstruction below documents behaviour and carries the real symbol
 * names.  Names in PascalCase (TowerInit, WorldOpen, PlayerHasRune,
 * PlayerHasShard, TowerNeedCrystalsMsg, TowerNeedGargItemsMsg,
 * TowerCheckMessages, SumnerInit, SumnerEnd, SumnerDoSpeech,
 * SumnerSpeechEnd, SumnerAnimate, SumnerHintsActivate, EnterTower,
 * GetWorldOrder) are Xbox-PDB names confirmed from behaviour; camelCase
 * tower / sumner / player names are descriptive (exact Midway identifier
 * unconfirmed - see the struct-field notes for each).
 */

/* --- external subsystem callees (names already mapped elsewhere) --- */
extern void* FindWORLDOBJ();
extern int   ResolveWorldData(int a, int b);
extern int   AtreeMatch(void* obj, char* name, int flag);
extern s32   CaptionText(s32 a, s32 b, s32 c, s32 d, s32 e);
extern void  CaptionTextReset(void);
extern s32   CaptionTextSub(char* text, f32 scale, s32 line, s32 time, s32 width);
extern char* GetScrollText(s32 a, s32 b, s32 c, s32* d);
extern char* GetStringText(s32 a, s32 b, u32* c);
extern char* GetStringListText(s32 a, s32 b, s32 c, u32* d);
extern int   FindStringMessageListSub_8001FC4C(int a, char* list);
extern s32   InitCustomEffect();
extern void  TriggerCameraEnd(void);
extern void* FindLookoutParam(int a);
typedef struct SumnerLookoutParam {
    u8 _pad00[0x6A];
    s16 id;
} SumnerLookoutParam;
extern SumnerLookoutParam* FindClosestWaypoint(f64 maxDist, f32* pos, s32 all);
extern void SumnerCamActivate(s32 idx, s32 sub);
extern int   AudioSoundPlaying(int a);
extern int   fn_80057BC8(int item);
extern int   sprintf(char* buf, const char* fmt, ...);
extern void  ErrorPrintf(const char* fmt, ...);
extern void  GetPlayerAvgPos(f32* avg, f32* bmax, f32* bmin, int mode);
extern int   ClosestStartPos(f32* pos);
extern void  HintMenu(void);
extern f32   sMusicFadeBase;
extern void  AtreeDelete(void* atree);
extern void* MBRemoveNode(void* node, s32 recursive);
extern void  EnablePlayerControls(void);
extern void  GetWorldMat(f32* matrix, f32* out, void* unused);
extern s32   StartFXSub(f32 scale, s32 effect, void* state, u32 flags, u32 mode);
extern void  SfxSetParent(s32 effect, void* parent);
extern u8*   fn_8005B558(s32 id);
extern void  MBTreeSetAlpha(void* tree, s32 alpha, s32 recurse);
extern void  MBTreeSetFlags(void* tree, s32 flags, s32 recurse);
extern void  fn_8009C460(s32 id);
extern s32   fn_8009CD80(s32 player, s32 character, s32 level);
extern void  fn_8009C4F0(s32 idx);
extern void  fn_8009C688(s32 idx);
extern s32   StartLevelUpFX(f32* pos, s32 color);
extern s32   StartGemFX(f32* pos, s32 color);
void SumnerSpeechEnd(void);
int  sumnerCheckLevelUp(void);
void SumnerDoSpeech(void);

/* --- TowerCheckMessages / EnterTower externs --- */
extern s32   gGameMode;
extern s32   gGameBusy;
extern s32   gTriggerCameraState;
extern s32   lbl_80344A28;
extern s32   lbl_803448C8;    /* current world id (EnterTower start-pos lookup) */
extern s32   lbl_803443BC;    /* scripted-camera timers (bosscam) */
extern s32   lbl_803443C0;
extern f32   lbl_80344C48;    /* tower-message cooldown timestamp */
extern f64   lbl_803485B0;    /* 3.0 - message cooldown period */
extern f64   lbl_803485B8;    /* 1.0 */
extern f64   lbl_803485C8;    /* 1/30 frame time */
extern f32   lbl_803485A0;    /* 2.0 */
extern u8    Effects[];       /* live fx pool, stride 0xF0 (game/effect.h) */
extern f32   gClockTime;
extern void  WindowCamActivate(s32 mode);
extern void  RuneCamActivate(s32 mode);
extern void  ActivateSpecialTrigger(s32 id, s32 flag);
extern void  MBTreeClearFlags(void* obj, u32 flags, s32 recurse);
extern s32   AnimateATree(void* tree, s32 seq, s32 mode);
extern s32   fn_8009C5B8(s32 idx);
extern s32   fn_8009C620(s32 idx);
extern void  fn_8005B5B8(void);
extern void  SetPlayerStartPos(s32 pos);
extern void  AtreeKillPsys(void* tree);

extern s32 lbl_80343C10;
extern double lbl_803485F8;   /* 0.0 */
extern double lbl_80348608;   /* 2.0 */
extern double lbl_80348598;   /* 10.0 - tower-message cooldown reset delta */

extern void ControllerMessageBox(int who, int msg, int slot, int flag);
extern f32  lbl_8028C2A8[];   /* garg-item "need items" message cooldown timers */

/* lbl_8028C288: tower-message state block.  The crystal-message cooldown
 * timers occupy the head; the GWIZ animation-tree instance handle lives at
 * +0xA4.  (The garg-item cooldowns alias this block at +0x20 but keep their
 * own symbol lbl_8028C2A8.) */
typedef struct {
    f32   cooldown[8];         /* 0x00 crystal "need crystals" msg cooldowns */
    f32   gargCooldown[3];     /* 0x20 garg-item cooldowns (alias lbl_8028C2A8) */
    char  effectName[0x20];    /* 0x2C temporary SHARD/RUNE effect name */
    s32   levelUpLevel[4];     /* 0x4C per-player level-up scratch slots */
    void* wizAtree;            /* 0x5C WIZARD (level-up) atree instance */
    u8    _pad60[0xA4 - 0x60]; /* 0x60 */
    void* gwizAtree;           /* 0xA4 GWIZ atree instance */
} TowerMsgState;
extern TowerMsgState lbl_8028C288;
extern char lbl_80114D50[];   /* garg-item message-list string */
extern char lbl_80114D60[];   /* crystals-needed message-list string */
extern char lbl_80114D6C[];   /* demo-closed message-list string */
extern s32  gDemoMode;
extern f32  lbl_803485E8;     /* -1.0 */
extern int  lbl_80348610;     /* "GWIZ" packed (sdata2, SDA-addressed) */
extern int  lbl_803485A4;     /* "WIZARD" packed (sdata2, SDA-addressed) */
extern f32  lbl_80348600;     /* 0.5 */
extern s32  lbl_80344C8C;
extern s32  ExpToLevel(s32 exp);
extern void DisablePlayerControls(void);
extern f32  gIdentityMatrix[];   /* identity matrix */
extern void* gSceneRoot;    /* MB parent object */
extern void* MBNewNode(void*, void*, int);
extern void  MBNodeSetParent(void* node, void* parent);
extern void  CopyMat4(f32* src, void* node);
extern s32   AtreeInit(void* atree, void* out, s32 a, s32 flags);
extern u8*   gCurLevel;       /* current-level descriptor pointer */
extern s32   lbl_803448A8;    /* last recorded world */
extern s32   lbl_803448AC;    /* last recorded level */
extern s32   sSpecialItem13;    /* rune-proximity world-object handle */
extern s32   sSpecialItem10;    /* shard-proximity world-object handle */
extern void  fn_8009C3EC(void); /* play rune-near audio cue */
extern void  fn_8009C378(void); /* play shard-near audio cue */

/* --- per-player progress record (gPlayerRecords[4], stride 0x335C) ---
 * Field offsets observed in this TU:
 *   0x0C   (12)    current world/level slot index
 *   0xE8   (232)   record-active state (0=empty,1=active,4/5=variants)
 *   0xF0   (240)   world id of this record
 *   0x928-0x930    world-dwell timer fields (award pacing)
 *   0xA8C  (2700)  world-rune bit accumulator
 *   0xDD4  (3540)  per-level rune-collected mask (base)
 *   0xDD6  (3542)  per-level shard-collected mask (base)
 *   0xDD8  (3544)  per-level "rune near" flags (base)
 *   0xDDC-0xDE2    per-level level/boss beaten masks
 *   0xDE8  (3560)  per-level completion record A ("levels")
 *   0xDEE  (3566)  per-level completion record B ("bosses")
 *   0x1CD0 (7376)  per-level status byte array
 *   0x1EC8 (7880)  cumulative runes bitmask
 *   0x1ECA (7882)  cumulative shards bitmask
 *   0x2220-0x223A  crystal / garg-item / legend-item counters+masks
 * (this array is shared game-wide - shop/sound/auxscreen/message also use it -
 *  so it is left as gPlayers rather than renamed here.)
 */
/* Typed view of the shared player-record array (kept as gPlayers: the
 * symbol is referenced by the Matching shopquery.c, so it must not be renamed;
 * the Player layout is defined in game/player.h). */
extern Player gPlayers[4];

/* tower/sumner state (r13 small-data globals) */
extern s32 lbl_80343D6C;
extern s32 lbl_80343E48;
extern s32 lbl_80343E4C;
extern s32 lbl_80343E50;
extern s32 lbl_80343E54;
extern s32 lbl_80343E58;
extern s32 sMusicTrackLo;
extern s32 sMusicTrackHi;
extern s32 sVisibleSumCoinCount;
extern s32 lbl_80344C4C;
extern void* sSumnerObj;   /* live Sumner (good wizard) object handle   */
extern s32 lbl_80344C54;
extern f32 lbl_80344C58;
extern f32 lbl_80344C5C;
extern s32 gSumnerReady;
extern s32 lbl_80344C64;   /* Sumner active/state latch                 */
extern f32 lbl_80344C68;
extern s32 lbl_80344C6C;
extern s32 lbl_80344C70;
extern s32 lbl_80344C74;
extern s32 lbl_80344C78;
extern s32 lbl_80344C7C;
extern s32 lbl_80344C80;
extern s32 lbl_80344C84;
extern s32 lbl_80344C88;
extern s32 lbl_80344C90;
extern void* sGoodWizObj;  /* GWIZ animation/effect object              */

s32 lbl_80124C70[9] = {0, 15, 100, 125, 150, 175, 200, 225, 250};
char lbl_80124C94[9][8] = {
    "TOWER", "ORANGE", "RED", "PURPLE", "BLUE", "GREEN", "YELLOW", "WHITE",
    "BLACK"
};
s32 lbl_80124CDC[3] = {12, 20, 28};
char lbl_80124CE8[3][14] = {"FANGS", "FEATHERS", "CLAWS"};
s32 crystal_order[14] = {13, 7, 2, 1, 11, 4, 3, 9, 10, 5, 6, 8, 0, 0};
s32 lbl_80124D4C[14] = {0, 3, 2, 6, 5, 0, 0, 1, 0, 7, 8, 4, 0, 0};
s32 lbl_80124D84[4] = {6, 4, 2, 5};
s32 lbl_80124D94[3] = {12, 20, 28};
s32 lbl_80124DA0[9] = {10, 11, 9, 8, 16, 12, 15, 14, 13};
extern f32 lbl_80348588;
extern f32 lbl_8034858C;
extern f32 lbl_80348590;
extern f32 lbl_803485EC;
extern f32 lbl_803485F0;
extern s32 gFrameTicks;
extern f32 gClockFrameStep;
extern s32 gScriptedCameraState;
extern s32 lbl_803447B8;
extern char lbl_803485C0;
extern char lbl_803485D0;
extern char lbl_803485D8;

#define PLAYER_AT(player, offset, type) \
    (*(type*)((u8*)&gPlayers[(player)] + (offset)))
#define TOWER_SAVE(player) \
    (&gPlayers[(player)].char_save[gPlayers[(player)].character])

/* ===================================================================== */

/* Zero the whole tower/sumner state block. Called from game_main. */
#pragma opt_propagation off
void TowerInit(void) {
    s32 none = -1;

    lbl_80344C4C = 0;
    lbl_80343E48 = none;
    sSumnerObj = 0;
    lbl_80344C54 = 0;
    lbl_80344C58 = lbl_80348588;
    lbl_80344C5C = lbl_80348588;
    gSumnerReady = 0;
    lbl_80344C64 = 0;
    lbl_80344C68 = lbl_80348588;
    lbl_80344C6C = 0;
    lbl_80344C70 = 0;
    lbl_80344C74 = 0;
    lbl_80344C78 = 0;
    lbl_80343E4C = none;
    lbl_80343E50 = none;
    lbl_80343E54 = none;
    lbl_80343E58 = none;
    lbl_80344C7C = 0;
    lbl_80344C80 = 0;
    lbl_80344C84 = 0;
    lbl_80344C88 = 0;
}
#pragma opt_propagation reset

/* Resolve the current world object handle (newcam GetPlayerAvgPos + ClosestStartPos). */
void towerUpdateCurWorldObj(void) {
    f32 pos[3];

    if (lbl_80343C10 >= 0) {
        GetPlayerAvgPos(pos, 0, 0, 0);
        lbl_80343E48 = ClosestStartPos(pos);
    }
}

/* Per-frame: if a rune/shard object (type1 sub13/sub10) is near and not yet
 * collected by any player, play the proximity audio cue. */
void towerRuneNearAudio(void) {
    u8* obj;
    s32* node;
    s32 i;

    if (sMusicTrackHi == 5 || sMusicTrackHi == 8) {
        return;
    }
    obj = (u8*)sSpecialItem13;
    if (obj != 0) {
        node = *(s32**)obj;
        if (node[0] == 1 && node[1] == 13) {
            s32 rune = *(s32*)(obj + 224);
            s32 found;

            for (i = 0; i < 4; i++) {
                Player* rec = &gPlayers[i];

                if (rec->state != 0 &&
                    (rec->char_save[rec->character].rune_near & (1 << rune)) != 0) {
                    found = 1;
                    goto checkRune;
                }
            }
            found = 0;
        checkRune:
            if (found == 0) {
                fn_8009C3EC();
            }
            return;
        }
    }
    obj = (u8*)sSpecialItem10;
    if (obj != 0) {
        node = *(s32**)obj;
        if (node[0] == 1 && node[1] == 10) {
            if (PlayerHasShard(-1, *(s16*)((u8*)node + 64)) == 0) {
                fn_8009C378();
            }
        }
    }
}

/* Is a world unlocked?  Checks cumulative rune/shard masks and per-player
 * level records against the world's requirement table.  Heavily called (UI). */
int WorldOpen(int world) {
    s32 result = 0;
    s32 requirement;
    s32 best;
    s32 player;
    s32 met;

    switch (world) {
    case 13:
        result = 1;
        break;
    case 5:
        if (PlayerHasRune(-1, 0x1FE) != 0) {
            result = 1;
        }
        break;
    case 6:
        if (PlayerHasRune(-1, 0x3FE) != 0) {
            if (PlayerHasShard(-1, 0xFFF) != 0) {
                result = 1;
            }
        }
        break;
    case 8:
        if (PlayerHasRune(-1, 0x7FE) != 0) {
            result = 1;
        }
        break;
    default:
        requirement = lbl_80124D4C[world];
        best = 0;
        for (player = best; player < 4; player++) {
            Player* p = &gPlayers[player];

            if (p->state != 0) {
                u8* levelRecord;
                s32 value;

                if (towerLevelStatus(player, requirement) != 0) {
                    met = 1;
                    goto done;
                }
                levelRecord = (u8*)(requirement * 2);
                value = *(s16*)(levelRecord + (s32)p + p->character * 240 +
                                3566);
                if (best <= value) {
                    best = value;
                }
            }
        }
        if (best >= lbl_80124C70[requirement]) {
            met = 1;
        } else {
            met = 0;
        }
    done:
        if (met != 0) {
            result = 1;
        }
        break;
    }
    return result;
}

/* Award world runes to active players after the dwell timer elapses
 * (fields 0x928-0x930 timer, sets rune bit at 0xA8C). */
#pragma opt_propagation off
int towerAwardWorldRunes(void) {
    s32 rune = lbl_80124DA0[sMusicTrackLo];
    s32 awarded = 0;
    s32 player;
    f32 resetValue;

    if (rune < 0) {
        return 0;
    }
    resetValue = lbl_8034858C;
    for (player = 0; player < 4; player++) {
        u8* record = (u8*)&gPlayers[player];

        if (*(s32*)(record + 0xE8) == 1) {
            (*(s32*)(record + 0x930))++;
            *(s32*)(record + 0x928) = rune + 0x200;
            *(f32*)(record + 0x92C) = resetValue;
            if (*(s32*)(record + 0x930) >= sVisibleSumCoinCount) {
                awarded = 1;
            }
        }
    }
    if (awarded != 0) {
        for (player = 0; player < 4; player++) {
            if (gPlayers[player].state == 1) {
                PLAYER_AT(player, 0xA8C, u16) |= 1 << (rune - 8);
            }
        }
    }
    return awarded;
}

/* Return the per-level status byte (field 0x1CD0) for a world record. */
int towerGetLevelFlag(u8* rec, int level) {
    u32 offset;

    if (*(u32*)(rec + 0xF0) == (u32)lbl_80343D6C) {
        return -1;
    }
    offset = *(s32*)(rec + 0xC) * 14;
    return rec[offset + level + 0x1CD0];
}

/* Record level/boss completion across players (ResolveWorldData, sets
 * masks 0xDDC-0xDE2 and status byte 0x1CD0). */
void towerRecordLevelBeaten(int level, int world) {
    s32 i;

    lbl_803448A8 = world;
    lbl_803448AC = level;
    ResolveWorldData((level << 8) | (world & 0xFF), world);
    for (i = 0; i < 4; i++) {
        Player* rec = &gPlayers[i];
        s32 state = rec->state;

        if (state == 1 || state == 4 || state == 5) {
            s16 lvl = *(s16*)(gCurLevel + 0x90);

            if (lvl > 0) {
                int mask = 1 << (lvl - 1);

                if ((rec->char_save[rec->character].level_masks[0] & mask) != 0) {
                    rec->char_save[rec->character].level_masks[1] |= mask;
                } else {
                    rec->char_save[rec->character].level_masks[0] |= mask;
                }
            }
            {
                s16 boss = *(s16*)(gCurLevel + 0x92);

                if (boss > 0) {
                    int mask = 1 << boss;

                    if ((rec->char_save[rec->character].level_masks[2] & mask) != 0) {
                        rec->char_save[rec->character].level_masks[3] |= mask;
                    } else {
                        rec->char_save[rec->character].level_masks[2] |= mask;
                    }
                }
            }
            ((u8*)rec)[rec->character * 14 + level + 0x1CD0] |= (1 << world);
        }
    }
}

/* Set an inventory bit (field 0x2230/0x2232) for a player - garg/legend item. */
void playerGiveGargItem(int player, int item, int count) {
    Player* record = &gPlayers[player];

    if (count == fn_80057BC8(item) - 1) {
        u32 bit;
        s32 characterOffset;
        u8* first;

        first = (u8*)record +
                (characterOffset = record->character * sizeof(PlayerCharSave));
        bit = 1 << item;

        if (*(u16*)(first += 0x2230) & bit) {
            u8* save = (u8*)record;
            save += characterOffset;
            *(u16*)(save + 0x2232) |= bit;
        } else {
            *(u16*)first |= bit;
        }
    }
}

static inline int towerLevelStatusA(int player, int level) {
    s32 value;
    u32 world;
    u8* levelRecord;
    Player* record;

    if (gPlayers[player].state == 0) {
        return 0;
    }
    world = PLAYER_AT(player, 0xF0, u32);
    if (world == (u32)lbl_80343D6C) {
        return 2;
    }
    levelRecord = (u8*)(level * 2);
    record = &gPlayers[player];
    value = *(s16*)(levelRecord + (s32)record +
                   record->character * 240 + 3560);
    if (value < 0) {
        return 2;
    }
    if (value == lbl_80124D94[level]) {
        return 1;
    }
    return 0;
}
#pragma opt_propagation reset

/* True if all active players meet the level-record-A requirement (field 0xDE8). */
int towerAllPlayersMetLevelReq(int level) {
    s32 player;
    s32 best = 0;

    if (level > 2) {
        level = 2;
    }
    for (player = 0; player < 4; player++) {
        if (gPlayers[player].state != 0) {
            s32 value = towerLevelStatusA(player, level);
            u8* levelRecord;
            Player* record;

            if (value != 0) {
                return 1;
            }
            levelRecord = (u8*)(level * 2);
            record = &gPlayers[player];
            value = *(s16*)(levelRecord + (s32)record +
                           record->character * 240 + 3560);
            if (best > value) {
                value = best;
            }
            best = value;
        }
    }
    if (best >= lbl_80124D94[level]) {
        return 1;
    }
    return 0;
}

/* Get a per-level record-A value (field 0xDE8). */
int towerGetLevelRecord(int player, int level) {
    return ((s16*)&TOWER_SAVE(player)->completion1)[level];
}

/* Advance the per-level record-A counter toward its cap (field 0xDE8, 0x2234). */
void towerAdvanceLevelRecord(int player, int level) {
    s32 last;
    s32 i;
    f32 resetTime;

    if (player < 0) {
        player = 0;
        last = 3;
    } else {
        last = player;
    }
    resetTime = lbl_80348590;
    for (i = player; i <= last; i++) {
        Player* record = &gPlayers[i];

        if (record->state == 1 || record->state == 4) {
            s16* value;
            u8* levelRecord;

            levelRecord = (u8*)(level * 2);
            levelRecord += (s32)record;
            value = (s16*)(levelRecord + record->character * 240 + 3560);

            if (*value >= 0 && *value < lbl_80124D94[level]) {
                (*value)++;
                if (sMusicTrackHi == 13) {
                    s16* extra = (s16*)(levelRecord + record->character * 240 + 8756);
                    (*extra)++;
                }
            }
            PLAYER_AT(i, 0x928, s32) = level + 0x100;
            PLAYER_AT(i, 0x92C, f32) = resetTime;
        }
    }
}

static inline int towerLevelStatusB(int player, int level) {
    s32 value;
    u32 world;
    u8* levelRecord;
    Player* record;

    if (gPlayers[player].state == 0) {
        return 0;
    }
    world = PLAYER_AT(player, 0xF0, u32);
    if (world == (u32)lbl_80343D6C) {
        return 2;
    }
    levelRecord = (u8*)(level * 2);
    record = &gPlayers[player];
    value = *(s16*)(levelRecord + (s32)record +
                   record->character * 240 + 3566);
    if (value < 0) {
        return 2;
    }
    if (value == lbl_80124C70[level]) {
        return 1;
    }
    return 0;
}

/* True if all active players meet the level-record-B requirement (field 0xDEE). */
int towerAllPlayersMetBossReq(int level) {
    s32 player;
    s32 best = 0;

    for (player = 0; player < 4; player++) {
        if (gPlayers[player].state != 0) {
            s32 value = towerLevelStatusB(player, level);
            u8* levelRecord;
            Player* record;

            if (value != 0) {
                return 1;
            }
            levelRecord = (u8*)(level * 2);
            record = &gPlayers[player];
            value = *(s16*)(levelRecord + (s32)record +
                           record->character * 240 + 3566);
            if (best > value) {
                value = best;
            }
            best = value;
        }
    }
    if (best >= lbl_80124C70[level]) {
        return 1;
    }
    return 0;
}

/* Per-record level status: 0/1/2 (field 0xDEE vs requirement table). Internal. */
int towerLevelStatus(int player, int level) {
    s32 value;
    u32 world;
    u8* save;

    if (gPlayers[player].state == 0) {
        return 0;
    }
    world = PLAYER_AT(player, 0xF0, u32);
    if (world == (u32)lbl_80343D6C) {
        return 2;
    }
    save = (u8*)&gPlayers[player] + gPlayers[player].character * 240;
    save += level * 2;
    value = *(s16*)(save + 3566);
    if (value < 0) {
        return 2;
    }
    if (value == lbl_80124C70[level]) {
        return 1;
    }
    return 0;
}

/* Get a per-level record-B value (field 0xDEE). */
int towerBossStatus(int player, int level) {
    s32 value = ((s16*)&TOWER_SAVE(player)->completion2)[level];
    s32 result = value;

    if (value < 0) {
        result = lbl_80124C70[level];
    }
    return result;
}

/* Advance the per-level record-B counter (field 0xDEE, 0x223A, timers). */
void towerAdvanceBossRecord(int player, int level) {
    s32 last;
    s32 i;
    f32 resetTime;

    if (player < 0) {
        player = 0;
        last = 3;
    } else {
        last = player;
    }
    resetTime = lbl_80348590;
    for (i = player; i <= last; i++) {
        Player* record = &gPlayers[i];

        if (record->state == 1 || record->state == 4) {
            u8* levelRecord;
            s16* value;

            levelRecord = (u8*)(level * 2);
            levelRecord += (s32)record;
            value = (s16*)(levelRecord + record->character * 240 + 3566);

            if (*value >= 0 && *value < lbl_80124C70[level]) {
                (*value)++;
                if (sMusicTrackHi == 13) {
                    s16* extra = (s16*)(levelRecord + record->character * 240 + 8762);
                    (*extra)++;
                }
            }
            PLAYER_AT(i, 0x928, s32) = level;
            PLAYER_AT(i, 0x92C, f32) = resetTime;
        }
    }
}

/* Clear a per-level "rune near" bit (field 0xDD8). */
void towerClearRuneNear(int player, int level) {
    s32 last;
    u32 mask;

    if (player < 0) {
        player = 0;
        last = 3;
        goto setup;
    }
    last = player;
setup:
    mask = 1 << level;
    for (; player <= last; player++) {
        TOWER_SAVE(player)->rune_near &= ~mask;
    }
}

/* Set a per-level "rune near" bit for active players (field 0xDD8). */
void towerSetRuneNear(int player, int level) {
    s32 last;
    u32 bit;

    if (player < 0) {
        player = 0;
        last = 3;
        goto setup;
    }
    last = player;
setup:
    bit = 1 << level;
    bit = bit;
    for (; player <= last; player++) {
        if (gPlayers[player].state == 1 ||
            gPlayers[player].state == 4) {
            TOWER_SAVE(player)->rune_near |= bit;
        }
    }
}

/* Query a per-level "rune near" bit for one player, or any player if -1. */
int towerGetRuneNearStat(int player, int level) {
    s32 last;
    u32 bit;

    if (player < 0) {
        player = 0;
        last = 3;
        goto setup;
    }
    last = player;
setup:
    bit = 1 << level;
    bit = bit;
    for (; player <= last; player++) {
        if (gPlayers[player].state != 0) {
            u32 near = TOWER_SAVE(player)->rune_near;
            near &= bit;
            if (near != 0) {
                return 1;
            }
        }
    }
    return 0;
}

/* Give a rune to one player, or all active players if player is -1. */
void PlayerGiveRune(int player, int rune) {
    s32 last;
    u32 bit;

    if (player < 0) {
        player = 0;
        last = 3;
        goto setup;
    }
    last = player;
setup:
    bit = 1 << rune;
    for (; player <= last; player++) {
        if (gPlayers[player].state == 1) {
            gPlayers[player].runes |= bit;
        }
    }
}

/* Does the player (or any, if -1) have the given rune(s)?  Field 0x1EC8/0xDD4. */
int PlayerHasRune(int player, int rune) {
    s32 last;
    u32 bits = 0;

    if (player < 0) {
        player = 0;
        last = 3;
        goto setup;
    }
    last = player;
setup:
    for (; player <= last; player++) {
        if (gPlayers[player].state != 0) {
            bits |= TOWER_SAVE(player)->rune_stones |
                    gPlayers[player].runes;
        }
    }
    if (rune < 0) {
        return (u16)bits;
    } else {
        u16 mask;

        if (rune > 14) {
            mask = rune;
        } else {
            mask = 1 << rune;
        }
        if (mask == (mask & (u16)bits)) {
            return 1;
        }
    }
    return 0;
}

/* Give a shard to one player, or all eligible players if player is -1. */
void PlayerGiveShard(int player, int shard) {
    s32 last;
    u32 bit;

    if (player < 0) {
        player = 0;
        last = 3;
        goto setup;
    }
    last = player;
setup:
    bit = 1 << shard;
    for (; player <= last; player++) {
        if (gPlayers[player].state == 1 ||
            gPlayers[player].state == 4) {
            gPlayers[player].shards |= bit;
        }
    }
}

/* Does the player (or any, if -1) have the given shard(s)? Field 0x1ECA/0xDD6. */
int PlayerHasShard(int player, int shard) {
    s32 last;
    u32 bits = 0;

    if (player < 0) {
        player = 0;
        last = 3;
        goto setup;
    }
    last = player;
setup:
    for (; player <= last; player++) {
        if (gPlayers[player].state != 0) {
            bits |= TOWER_SAVE(player)->rune_stones2 |
                    gPlayers[player].shards;
        }
    }
    if (shard < 0) {
        return (u16)bits;
    } else {
        u16 mask;

        if (shard > 13) {
            mask = shard;
        } else {
            mask = 1 << shard;
        }
        if (mask == (mask & (u16)bits)) {
            return 1;
        }
    }
    return 0;
}

/* Cooldown-gated "you need gargoyle items" tower message. */
void TowerNeedGargItemsMsg(int who, int slot) {
    f32* cd = &lbl_8028C2A8[slot];

    if (*cd < sMusicFadeBase) {
        int msg = FindStringMessageListSub_8001FC4C(0, lbl_80114D50);
        ControllerMessageBox(who, msg, slot, -1);
        *cd = lbl_80348598 + sMusicFadeBase;
    }
}

/* Cooldown-gated "you need N crystals" (or demo-closed) tower message. */
void TowerNeedCrystalsMsg(int who, int slot) {
    f32* cd = &lbl_8028C288.cooldown[slot];

    if (*cd < sMusicFadeBase) {
        int msg;

        if (gDemoMode != 0) {
            msg = FindStringMessageListSub_8001FC4C(0, lbl_80114D60);
            ControllerMessageBox(who, msg, 0, -1);
        } else {
            msg = FindStringMessageListSub_8001FC4C(0, lbl_80114D6C);
            ControllerMessageBox(who, msg, slot, -1);
        }
        *cd = lbl_80348598 + sMusicFadeBase;
    }
}

/* Summon the good wizard's WIZARD atree + node once (shared helper, inlined
 * at every call site in the original TU). */
static void towerSummonWizard(TowerMsgState* state) {
    if ((u32)lbl_80344C64 == 0) {
        void* atree = (void*)AtreeMatch(sGoodWizObj, (char*)&lbl_803485A4, 0);

        if (atree != 0) {
            state->wizAtree = (void*)AtreeInit(atree, &state->wizAtree, 0,
                                               0xC00880);
            lbl_80344C64 = (s32)MBNewNode(gSceneRoot, gIdentityMatrix, 1);
            MBNodeSetParent(*(void**)state->wizAtree, (void*)lbl_80344C64);
        }
    }
}

/* Per-frame tower message/state dispatcher.  Served by do_players; drives
 * caption resets, Sumner animation matching, world-object lookups and the
 * speech/level-up/effect helpers (SumnerDoSpeech/SumnerSpeechEnd/
 * sumnerCheckLevelUp). */
void TowerCheckMessages(s32 mode) {
    char* strings = lbl_80114D50;
    TowerMsgState* state = &lbl_8028C288;
    s32 levels[3];
    s32 bosses[8];
    s32 i;
    s32 j;
    s32 msg;
    s32 arg;

    if (sMusicTrackHi == 13) {
        if (gGameMode == 16400) {
            SumnerDoSpeech();
            if (sSumnerObj != 0) {
                if (AnimateATree(&state->gwizAtree, lbl_80344C90,
                                 lbl_80344C54 != 0 ? 2 : 0) != 0) {
                    lbl_80344C90++;
                    if (lbl_80344C90 > 2) {
                        lbl_80344C90 = 0;
                    }
                    lbl_80344C54 = 0;
                }
            }
            if ((u32)lbl_80344C64 != 0) {
                AnimateATree(&state->wizAtree, lbl_80344C8C, 0);
            }
        }
        if (gScriptedCameraState != 0) {
            return;
        }
        if (lbl_803447B8 != 0) {
            return;
        }
        switch (lbl_80344C7C) {
        case 0:
            break;
        case 10:
            if (lbl_803443C0 < 250) {
                fn_8009C460(2);
                lbl_80344C7C++;
                break;
            }
            /* fall through */
        case 11:
            if (lbl_803443BC <= 4 && lbl_803443C0 <= 4) {
                u8* object;

                WindowCamActivate(1);
                lbl_80344C88 = (s32)fn_8005B558(0x500);
                MBTreeSetAlpha(*(void**)(lbl_80344C88 + 100), 255, 1);
                object = (u8*)FindWORLDOBJ(strings + 44);
                if (object != 0) {
                    MBTreeClearFlags(*(void**)(object + 40), 2, 0);
                }
                lbl_80344C84 = *(s32*)(object + 40);
                MBTreeSetAlpha((void*)lbl_80344C84, 255, 1);
                lbl_80344C80 = 180;
                lbl_80344C7C = 116;
                fn_8009C460(11);
            }
            break;
        case 14:
            if (lbl_803443C0 < 250) {
                fn_8009C460(2);
                lbl_80344C7C++;
                break;
            }
            /* fall through */
        case 15:
        case 16:
            if (lbl_803443BC <= 4 && lbl_803443C0 <= 4) {
                lbl_80344C6C = lbl_80344C7C;
                lbl_80344C68 = lbl_803485A0;
                lbl_80344C70 = 0;
                lbl_80344C7C = 0;
                CaptionTextReset();
            }
            break;
        case 21:
            lbl_80344C88 = (s32)fn_8005B558(0x600);
            MBTreeSetAlpha(*(void**)(lbl_80344C88 + 100), 255, 1);
            if (lbl_803443BC <= 4 && lbl_803443C0 <= 4) {
                RuneCamActivate(0);
                lbl_80344C84 = 0;
                lbl_80344C80 = 180;
                lbl_80344C7C = 126;
            }
            break;
        case 20:
        case 22:
        case 24:
            if (lbl_803443C0 < 250) {
                fn_8009C460(1);
                lbl_80344C7C++;
            }
            break;
        case 30:
        case 32:
            if (lbl_803443C0 < 220) {
                fn_8009C460(1);
                lbl_80344C7C++;
            }
            break;
        case 23:
        case 33:
            lbl_80344C7C = 0;
            break;
        case 25:
        case 26:
        case 36:
            if (lbl_803443BC <= 4 && lbl_803443C0 <= 4) {
                lbl_80344C70 = 0;
                lbl_80344C68 = lbl_803485A0;
                lbl_80344C6C = lbl_80344C7C + 100;
                lbl_80344C7C = 0;
                CaptionTextReset();
            }
            break;
        case 31:
            if (lbl_803443BC <= 4 && lbl_803443C0 <= 4) {
                RuneCamActivate(2);
                ActivateSpecialTrigger(255, 0);
                lbl_80344C88 = (s32)fn_8005B558(0x803);
                MBTreeSetAlpha(*(void**)(lbl_80344C88 + 100), 255, 1);
                lbl_80344C84 = 0;
                lbl_80344C80 = 180;
                lbl_80344C7C = 136;
            }
            break;
        default:
            if (lbl_80344C7C < 0) {
                lbl_80344C7C = 0;
            } else {
                s32 alpha;

                lbl_80344C80 -= gFrameTicks;
                if (lbl_80344C80 <= 0) {
                    lbl_80344C80 = 0;
                    lbl_80344C7C -= 100;
                }
                alpha = lbl_80344C80 * 255 / 180;
                MBTreeSetAlpha((void*)lbl_80344C84, alpha, 1);
                if ((u32)lbl_80344C88 != 0) {
                    MBTreeSetAlpha(*(void**)(lbl_80344C88 + 100), alpha, 1);
                }
            }
            break;
        }
        if (gTriggerCameraState != 0) {
            return;
        }
        if (gGameBusy != 0) {
            return;
        }
        if (lbl_80344A28 != 0) {
            return;
        }
        sumnerCheckLevelUp();
        if (mode != 0) {
            u32 runeGot = 0;
            u32 runeBanked = 0;
            u32 shardGot = 0;
            u32 shardBanked = 0;
            u32 newRunes;
            u32 newShards;
            s32 rune;
            s32 shard;

            for (i = 0; i < 4; i++) {
                Player* p = &gPlayers[i];

                if (p->state != 0 &&
                    *(u32*)((u8*)p + 0xF0) != (u32)lbl_80343D6C) {
                    u8* rec = (u8*)p + p->character * 240;

                    runeGot |= p->runes;
                    *(u16*)(rec + 3540) |= p->runes;
                    runeBanked |= *(u16*)(rec + 8736);
                    *(u16*)((u8*)p + p->character * 240 + 8736) |= p->runes;
                }
            }
            newRunes = (u16)((u16)runeGot & ((u16)runeGot ^ (u16)runeBanked));
            for (rune = 1; rune <= 8; rune++) {
                if (newRunes & (1 << rune)) {
                    towerSummonWizard(state);
                    lbl_80344C6C = rune;
                    lbl_80344C68 = lbl_80348590;
                    lbl_80344C70 = 0;
                    DisablePlayerControls();
                    lbl_80344C8C = 0;
                    CaptionTextReset();
                    break;
                }
            }
            for (i = 0; i < 4; i++) {
                Player* p = &gPlayers[i];

                if (p->state != 0 &&
                    *(u32*)((u8*)p + 0xF0) != (u32)lbl_80343D6C) {
                    u8* rec = (u8*)p + p->character * 240;

                    shardGot |= p->shards;
                    *(u16*)(rec + 3542) |= p->shards;
                    shardBanked |= *(u16*)(rec + 8738);
                    *(u16*)((u8*)p + p->character * 240 + 8738) |= p->shards;
                }
            }
            shardBanked = (u16)shardBanked;
            newShards = (u16)((u16)shardGot & ((u16)shardGot ^ (u16)shardBanked));
            for (shard = 0; shard < 13; shard++) {
                if (newShards & (1 << shard)) {
                    towerSummonWizard(state);
                    if (shard <= 13) {
                        lbl_80344C6C = shard + 100;
                        lbl_80344C68 = lbl_80348590;
                        lbl_80344C70 = 0;
                    } else {
                        lbl_80344C6C = shard + 100;
                        lbl_80344C68 = lbl_80348588;
                        SumnerSpeechEnd();
                    }
                    DisablePlayerControls();
                    lbl_80344C8C = 0;
                    CaptionTextReset();
                    break;
                }
            }
            if (shard == 13) {
                if (lbl_803448AC == 8 && lbl_803448A8 == 2 &&
                    !(shardBanked & 0x1000)) {
                    towerSummonWizard(state);
                    lbl_80344C6C = 113;
                    lbl_80344C68 = lbl_80348590;
                    lbl_80344C70 = 0;
                    DisablePlayerControls();
                    lbl_80344C8C = 0;
                    CaptionTextReset();
                    lbl_803448AC = -1;
                    lbl_803448A8 = -1;
                }
                if (((s32)shardBanked & 0xFFF) == 0xFFF &&
                    (newRunes & 0x200) != 0) {
                    towerSummonWizard(state);
                    lbl_80344C6C = 114;
                    lbl_80344C68 = lbl_80348588;
                    SumnerSpeechEnd();
                    DisablePlayerControls();
                    lbl_80344C8C = 0;
                    CaptionTextReset();
                }
            }
        }
        if (sMusicFadeBase - lbl_80344C48 >= 3.0) {
            for (j = 0; j < 3; j++) {
                s32* best = &levels[j];
                u32 curWorld = lbl_80343D6C;

                *best = 0;
                for (i = 0; i < 4; i++) {
                    Player* p = &gPlayers[i];

                    if (p->state != 0 &&
                        *(u32*)((u8*)p + 0xF0) != curWorld) {
                        u8* levelRecord = (u8*)(j * 2);
                        s32 val = *(s16*)(levelRecord + (s32)p +
                                          p->character * 240 + 3560);

                        if (*best >= 0 && (val < 0 || val > *best)) {
                            *best = val;
                        }
                    }
                }
                if (*best == lbl_80124CDC[j]) {
                    msg = FindStringMessageListSub_8001FC4C(0, strings + 60);
                    arg = fn_8009C5B8(j);
                    ControllerMessageBox(-1, msg, j, arg);
                    for (i = 0; i < 4; i++) {
                        Player* p = &gPlayers[i];
                        s16* val = (s16*)((u8*)(j * 2) + (s32)p +
                                          p->character * 240 + 3560);

                        if (*val == lbl_80124CDC[j]) {
                            *val = -1;
                            *(s16*)((u8*)(j * 2) + (s32)p +
                                    p->character * 240 + 8756) = -1;
                        }
                    }
                    lbl_80344C48 = sMusicFadeBase;
                }
            }
            for (j = 0; j < 8; j++) {
                if (lbl_80124C70[j] != 0) {
                    s32* best = &bosses[j];
                    u32 curWorld = lbl_80343D6C;

                    *best = 0;
                    for (i = 0; i < 4; i++) {
                        Player* p = &gPlayers[i];

                        if (p->state != 0 &&
                            *(u32*)((u8*)p + 0xF0) != curWorld) {
                            u8* levelRecord = (u8*)(j * 2);
                            s32 val = *(s16*)(levelRecord + (s32)p +
                                              p->character * 240 + 3566);

                            if (*best >= 0 && (val < 0 || val > *best)) {
                                *best = val;
                            }
                        }
                    }
                    if (*best == lbl_80124C70[j]) {
                        msg = FindStringMessageListSub_8001FC4C(0, strings + 76);
                        arg = fn_8009C620(j);
                        ControllerMessageBox(-1, msg, j, arg);
                        for (i = 0; i < 4; i++) {
                            Player* p = &gPlayers[i];
                            s16* val = (s16*)((u8*)(j * 2) + (s32)p +
                                              p->character * 240 + 3566);

                            if (*val == lbl_80124C70[j]) {
                                *val = -1;
                                *(s16*)((u8*)(j * 2) + (s32)p +
                                        p->character * 240 + 8762) = -1;
                            }
                        }
                        lbl_80344C48 = sMusicFadeBase;
                    }
                }
            }
        }
    }
}

/* Enter the hub tower: spawn the collected RUNE%d/SHARD%d/RUNE13 (+wizard)
 * effects, set up the display and hand off to TowerCheckMessages. */
/* Enter the hub tower: spawn the collected RUNE%d/SHARD%d/RUNE13 (+wizard)
 * effects, set up the display and hand off to TowerCheckMessages. */
void EnterTower(void) {
    char* strings = lbl_80114D50;
    TowerMsgState* state = &lbl_8028C288;
    f32 world[12];
    s32 effectState[6];
    s32 i;
    s32 j;
    s32 k;
    s32 count;
    s32 pos;
    s32 req;
    s32 worldId;
    s32 st;
    s32 effect;
    s32 fx;
    u32 runes;
    u32 shards;
    u32 runeMask;
    u32 shardMask;
    s32 have13;
    u8* object;

    lbl_80344C4C++;
    lbl_80344C58 = lbl_80348588;
    if (lbl_80343E48 >= 0) {
        pos = lbl_80343E48;
    } else {
        for (k = 0; k < 14; k++) {
            if (lbl_803448C8 == crystal_order[k]) {
                goto found;
            }
        }
        k = 0;
    found:
        pos = k;
    }
    SetPlayerStartPos(pos);
    lbl_80343E48 = -1;
    lbl_80344C48 = lbl_803485B8 + sMusicFadeBase;
    for (i = 0; i < 8; i++) {
        state->cooldown[i] = lbl_80348588;
    }
    for (i = 0; i < 3; i++) {
        ((f32*)((u8*)state + i * 4))[8] = lbl_80348588;
    }
    for (j = 0; j < 3; j++) {
        req = lbl_80124D94[j];
        count = 0;
        for (i = 0; i < 4; i++) {
            Player* p;

            if (gDemoMode != 0) {
                break;
            }
            p = &gPlayers[i];
            if (p->state == 0) {
                st = 0;
            } else if (*(u32*)((u8*)p + 0xF0) == (u32)lbl_80343D6C) {
                st = 2;
            } else {
                u8* levelRecord = (u8*)(j * 2);
                s32 val = *(s16*)(levelRecord + (s32)p + p->character * 240 +
                                  3560);

                if (val < 0) {
                    st = 2;
                } else if (val == req) {
                    st = 1;
                } else {
                    st = 0;
                }
            }
            if (st == 2) {
                break;
            }
            count++;
        }
        if (count < 4) {
            ActivateSpecialTrigger(j + 101, 1);
            if (j == 2) {
                ActivateSpecialTrigger(104, 1);
                ActivateSpecialTrigger(199, 1);
            }
        }
    }
    for (j = 0; j < 8; j++) {
        req = lbl_80124C70[j];
        worldId = crystal_order[j];
        if (req != 0) {
            count = 0;
            for (i = 0; i < 4; i++) {
                Player* p;

                if (gDemoMode != 0 &&
                    (worldId == 7 || (u32)(worldId - 10) <= 1)) {
                    break;
                }
                p = &gPlayers[i];
                if (p->state == 0) {
                    st = 0;
                } else if (*(u32*)((u8*)p + 0xF0) == (u32)lbl_80343D6C) {
                    st = 2;
                } else {
                    u8* levelRecord = (u8*)(j * 2);
                    s32 val = *(s16*)(levelRecord + (s32)p + p->character * 240 +
                                      3566);

                    if (val < 0) {
                        st = 2;
                    } else if (val == req) {
                        st = 1;
                    } else {
                        st = 0;
                    }
                }
                if (st == 2) {
                    break;
                }
                count++;
            }
            if (count < 4) {
                ActivateSpecialTrigger(j, 1);
            }
        }
    }
    runes = 0;
    for (i = 0; i < 4; i++) {
        Player* p = &gPlayers[i];

        if (p->state != 0) {
            if (*(u32*)((u8*)p + 0xF0) == (u32)lbl_80343D6C) {
                runes = 0x7FE;
            } else {
                runes |= *(u16*)((u8*)p + p->character * 240 + 8736);
            }
        }
    }
    runeMask = (u16)runes;
    if (runeMask != 0) {
        object = (u8*)FindWORLDOBJ(strings + 88);
        if (object != 0) {
            GetWorldMat(*(f32**)(object + 40), world, 0);
            {
            f64 scale = lbl_803485C8;
            for (j = 1; j < 9; j++) {
                if (runeMask & (1 << j)) {
                    sprintf(state->effectName, &lbl_803485C0, j);
                    effect = InitCustomEffect(0, state->effectName, 0, 0);
                    if (object != 0 && effect >= 0) {
                        u8* e;
                        u8* ai;

                        fx = StartFXSub(lbl_80348588, effect, world + 12,
                                        0x80000, 0x800);
                        SfxSetParent(fx, gSceneRoot);
                        e = (u8*)Effects + fx * 240;
                        ai = e + 28;
                        *(f32*)(ai + 32) =
                            (f32)(gClockTime - scale * *(s16*)(e + 44));
                        *(f32*)(ai + 24) = *(s16*)(ai + 16);
                        AtreeKillPsys(e + 24);
                    }
                }
            }
            }
        } else {
            ErrorPrintf(strings + 104);
        }
    }
    if (((s32)runes & 0x1FE) != 0x1FE) {
        MBTreeSetFlags(*(void**)((u8*)FindWORLDOBJ(strings + 44) + 40), 2, 0);
    }
    shards = 0;
    for (i = 0; i < 4; i++) {
        Player* p = &gPlayers[i];

        if (p->state != 0) {
            if (*(u32*)((u8*)p + 0xF0) == (u32)lbl_80343D6C) {
                shards = 0x1FFF;
            } else {
                shards |= *(u16*)((u8*)p + p->character * 240 + 8738);
            }
        }
    }
    shardMask = (u16)shards;
    if (shardMask != 0) {
        have13 = shardMask & 0x1000;
        if (have13 != 0) {
            ActivateSpecialTrigger(255, 1);
        }
        object = (u8*)FindWORLDOBJ(strings + 128);
        if (object != 0) {
            GetWorldMat(*(f32**)(object + 40), world, 0);
            {
            f64 scale = lbl_803485C8;
            for (j = 0; j < 12; j++) {
                if (shardMask & (1 << j)) {
                    sprintf(state->effectName, &lbl_803485D0, j + 1);
                    effect = InitCustomEffect(0, state->effectName, 0, 0);
                    if (object != 0 && effect >= 0) {
                        u8* e;
                        u8* ai;

                        fx = StartFXSub(lbl_80348588, effect, world + 12,
                                        0x80000, 0x800);
                        SfxSetParent(fx, gSceneRoot);
                        e = (u8*)Effects + fx * 240;
                        ai = e + 28;
                        *(f32*)(ai + 32) =
                            (f32)(gClockTime - scale * *(s16*)(ai + 16));
                        *(f32*)(ai + 24) = *(s16*)(ai + 16);
                    }
                }
            }
            }
        } else {
            ErrorPrintf(strings + 140);
        }
        object = (u8*)FindWORLDOBJ(strings + 164);
        if (object != 0) {
            if (have13 != 0) {
                u8* e;
                u8* ai;
                f64 scale = lbl_803485C8;

                GetWorldMat(*(f32**)(object + 40), world, 0);
                effect = InitCustomEffect(0, &lbl_803485D8, 0, 0);
                if (object != 0 && effect >= 0) {
                    fx = StartFXSub(lbl_80348588, effect, world + 12, 0x80000,
                                    0x800);
                    SfxSetParent(fx, gSceneRoot);
                    e = (u8*)Effects + fx * 240;
                    ai = e + 28;
                    *(f32*)(ai + 32) =
                        (f32)(gClockTime - scale * *(s16*)(e + 44));
                    *(f32*)(ai + 24) = *(s16*)(ai + 16);
                }
            }
        } else {
            ErrorPrintf(strings + 176);
        }
    }
    fn_8005B5B8();
    TowerCheckMessages(1);
}

/* Update the good-wizard presence/timer from per-player progress (field
 * 0xA90); sets the sumner appear/idle float. */
void sumnerUpdatePresence(void) {
    if (lbl_80344C4C == 0) {
        s32 player;

        gSumnerReady = 1;
        for (player = 0; player < 4; player++) {
            Player* rec = &gPlayers[player];

            if (rec->state != 0) {
                s32 k;

                for (k = 0; k < 16; k++) {
                    if (*(s32*)((u8*)rec + k * 24 + 0xA90) > 0) {
                        gSumnerReady = 0;
                    }
                }
            }
        }
    } else {
        gSumnerReady = 0;
    }
    if (gSumnerReady != 0) {
        lbl_80344C5C = lbl_80348588;
    } else {
        lbl_80344C5C = lbl_803485E8;
    }
}

/* Map a world id to its ordering index via the 14-entry world-order table. */
int GetWorldOrder(int worldId) {
    s32 i;

    for (i = 0; i < 14; i++) {
        if (crystal_order[i] == worldId) {
            return i;
        }
    }
    return 0;
}

/* True while a Sumner speech timer is running. */
int sumnerSpeechActive(void) {
    if (lbl_80344C68 > lbl_80348588) {
        return 1;
    }
    return 0;
}

/* Run the Sumner speech: fetch scroll/string/list text, show captions,
 * play the speech audio, and advance the wizard animation. */
#pragma opt_propagation off
void SumnerDoSpeech(void) {
    s32 scrollArgs[2];
    char caption[256];
    f32 avg[4];
    char* strings = lbl_80114D50;
    TowerMsgState* state = &lbl_8028C288;
    s32 message = -1;
    s32 argument = 0;
    s32 hasLevelUp = 0;
    s32 i;
    s32 elapsed;
    s32 done;
    u8* entry;

    if (lbl_80344C68 <= lbl_80348588 || gScriptedCameraState != 0 ||
        lbl_803447B8 != 0) {
        return;
    }
    if (lbl_80344C78 > 0) {
        lbl_80344C78 -= gFrameTicks;
        return;
    }

    if (lbl_80343E4C >= 0) {
        for (i = 0; i < 16; i += 4) {
            entry = (u8*)state;
            entry += i;

            if (*(s32*)(entry + 76) > 0) {
                hasLevelUp = 1;
            }
        }
    }
    if (lbl_80344C74 == 0) {
        SumnerLookoutParam* waypoint;

        if ((u32)lbl_80344C64 == 0) {
            void* atree = (void*)AtreeMatch(sGoodWizObj, (char*)&lbl_803485A4, 0);

            if (atree != 0) {
                state->wizAtree = (void*)AtreeInit(atree, &state->wizAtree, 0,
                                                  0xC00880);
                lbl_80344C64 = (s32)MBNewNode(gSceneRoot, gIdentityMatrix, 1);
                MBNodeSetParent(*(void**)state->wizAtree, (void*)lbl_80344C64);
            }
        }
        GetPlayerAvgPos(avg, 0, 0, 0);
        waypoint = FindClosestWaypoint((f64)lbl_803485EC, avg, 1);

        if (waypoint != 0) {
            s32 camera = 0;

            CopyMat4((f32*)waypoint, (void*)lbl_80344C64);
            if (hasLevelUp) {
                camera = 0;
            } else if (lbl_80344C6C >= 100) {
                camera = 2;
            } else if (lbl_80344C6C >= 0) {
                camera = 1;
            }
            SumnerCamActivate(camera, waypoint->id);
        }
        lbl_80344C74 = 1;
    }

    {
        Player* player;

        for (i = 0, player = gPlayers; i < 4; i++, player++) {
            player->vibe_timer = 0;
            player->vibe_timer2 = 0;
        }
    }
    lbl_80344C70 += gFrameTicks;
    i = lbl_80344C70 - 120;
    elapsed = i;
    if (i < 0) {
        return;
    }

    if (lbl_80343E4C >= 0) {
        f32 captionScale = lbl_803485F0;
        Player* players = gPlayers;
        s32* gemColors = lbl_80124D84;
        register s32 playerIndex;

        while ((playerIndex = lbl_80343E4C) < 4) {
            Player* player;
            char* classText;
            char* scroll;
            char* characterText;
            char* levelText;
            s32 level;

            if (((TowerMsgState*)((u8*)state + playerIndex * 4))
                    ->levelUpLevel[0] > 0) {
                player = &players[lbl_80343E4C];
                scroll = GetScrollText(-1, 0x18, 0, scrollArgs);
                classText = GetStringText(0x16, player->class_id, 0);
                characterText = GetStringText(0x17, player->character, 0);
                level = player->level / 10;
                if (player->level == 99) {
                    levelText = GetStringText(0x15, 0, 0);
                } else {
                    levelText = GetStringListText(0, player->character,
                                                  level >> 1, 0);
                }
                sprintf(caption, scroll, classText, characterText, player->level,
                        levelText);
                done = CaptionTextSub(caption, captionScale, 6, elapsed >> 1,
                                      0x138);

                if (lbl_80343E58 < 0) {
                    lbl_80343E58 = fn_8009CD80(lbl_80343E4C, player->character,
                                               player->level);
                }
                if (lbl_80343E50 < 0 && elapsed >= 240) {
                    lbl_80343E50 = StartLevelUpFX(0, player->class_id);
                    SfxSetParent(lbl_80343E50, player->node);
                }
                if (lbl_80343E54 < 0 && elapsed >= 270) {
                    s32* gem = gemColors;

                    gem += player->class_id;
                    lbl_80343E54 = StartGemFX(0, *gem);
                    SfxSetParent(lbl_80343E54, player->node);
                }
                if (done == 0 || elapsed < 360 ||
                    AudioSoundPlaying(lbl_80343E58) != 0) {
                    break;
                }

                CaptionTextReset();
                playerIndex = lbl_80343E4C;
                lbl_80343E50 = -1;
                playerIndex++;
                lbl_80343E4C = playerIndex;
                elapsed = 0;
                lbl_80344C70 = 120;
                lbl_80343E54 = -1;
                lbl_80343E58 = -1;
            } else {
                lbl_80343E4C++;
            }
        }
        if (lbl_80343E4C == 4) {
            lbl_80343E4C = -1;
        }
        return;
    }

    if (lbl_80344C6C >= 100) {
        s32 speech = lbl_80344C6C - 100;

        argument = speech;
        if (speech < 13) {
            message = FindStringMessageListSub_8001FC4C(0, strings + 196);
            argument = 0;
            if (lbl_80343E58 < 1) {
                fn_8009C4F0(0);
                lbl_80343E58 = 1;
            }
        } else if (speech == 13) {
            message = FindStringMessageListSub_8001FC4C(0, strings + 208);
            argument = -1;
        } else if (speech == 25) {
            message = FindStringMessageListSub_8001FC4C(0, strings + 220);
            argument = -1;
        } else if (speech == 26) {
            message = FindStringMessageListSub_8001FC4C(0, strings + 236);
            argument = -1;
        } else if (speech == 36) {
            message = FindStringMessageListSub_8001FC4C(0, strings + 252);
            argument = -1;
        }
        if (message >= 0 && argument == -1 && lbl_80343E58 < 2) {
            fn_8009C4F0(speech);
            lbl_80343E58 = 2;
        }
    } else if (lbl_80344C6C >= 0) {
        s32 speech = lbl_80344C6C;

        argument = lbl_80344C6C;
        if (lbl_80344C6C < 9) {
            message = FindStringMessageListSub_8001FC4C(0, strings + 264);
            if (lbl_80343E58 < 1) {
                fn_8009C688(argument);
                lbl_80343E58 = 1;
            }
        } else if (lbl_80344C6C == 15) {
            message = FindStringMessageListSub_8001FC4C(0, strings + 276);
            argument = -1;
        } else if (lbl_80344C6C == 16) {
            message = FindStringMessageListSub_8001FC4C(0, strings + 288);
            argument = -1;
        }
        if (message >= 0 && argument == -1 && lbl_80343E58 < 2) {
            fn_8009C688(speech);
            lbl_80343E58 = 2;
        }
    }

    if (message >= 0) {
        done = CaptionText(0, message, argument, (done = elapsed >> 1),
                           0x138);
    } else {
        done = 1;
    }
    if (done != 0) {
        lbl_80344C68 -= gClockFrameStep;
    }
    if (lbl_80344C68 <= lbl_80348588) {
        SumnerSpeechEnd();
    }
}
#pragma opt_propagation reset

/* End the current Sumner speech: stop the camera move, spawn the reward
 * effects and clean up caption/wizard state.  Internal. */
void SumnerSpeechEnd(void) {
    char* strings = lbl_80114D50;
    TowerMsgState* state = &lbl_8028C288;
    s32 effectState[6];
    f32 world[12];
    s32 speech;
    u8* object;
    s32 effect;

    if ((u32)lbl_80344C64 != 0) {
        AtreeDelete(&state->wizAtree);
        lbl_80344C64 = (s32)MBRemoveNode((void*)lbl_80344C64, 1);
    }
    TriggerCameraEnd();
    lbl_80344C74 = 0;
    EnablePlayerControls();

    speech = lbl_80344C6C;
    if (speech == 112) {
        object = (u8*)FindWORLDOBJ(strings + 164);
        if (object != 0) {
            GetWorldMat(*(f32**)(object + 40), world, 0);
            effect = InitCustomEffect(0, &lbl_803485D8, 0, 0);
            if (object != 0 && effect >= 0) {
                effect = StartFXSub(lbl_80348588, effect, effectState,
                                    0x80000, 0x800);
                SfxSetParent(effect, gSceneRoot);
            }
            RuneCamActivate(1);
            MBTreeSetAlpha(*(void**)(fn_8005B558(0x803) + 100), 0xFF, 1);
            if (PlayerHasShard(-1, 0x1FFF) != 0) {
                lbl_80344C7C = 30;
            } else {
                lbl_80344C7C = 32;
            }
        } else {
            ErrorPrintf(strings + 176);
        }
    } else if (speech >= 100) {
        speech -= 100;
        if (speech <= 14 &&
            (object = (u8*)FindWORLDOBJ(strings + 128)) != 0) {
            if (speech < 13) {
                GetWorldMat(*(f32**)(object + 40), world, 0);
                sprintf(state->effectName, &lbl_803485D0, speech + 1);
                effect = InitCustomEffect(0, state->effectName, 0, 0);
                if (object != 0 && effect >= 0) {
                    effect = StartFXSub(lbl_80348588, effect, effectState,
                                        0x80000, 0x800);
                    SfxSetParent(effect, gSceneRoot);
                }
                RuneCamActivate(0);
                fn_8009C460(21);
            }
            if (PlayerHasRune(-1, 0x3FE) != 0 &&
                PlayerHasShard(-1, 0xFFF) != 0) {
                if (speech < 13) {
                    lbl_80344C7C = 20;
                } else if (PlayerHasRune(-1, 0x7FE) == 0) {
                    lbl_80344C7C = 21;
                } else {
                    lbl_80344C7C = 0;
                }
            } else if (PlayerHasShard(-1, 0xFFF) != 0) {
                lbl_80344C7C = 24;
            } else {
                lbl_80344C7C = 22;
            }
            if (lbl_80344C7C != 0) {
                MBTreeSetAlpha(*(void**)(fn_8005B558(0x600) + 100),
                               0xFF, 1);
            }
        } else if (speech < 13) {
            ErrorPrintf(strings + 140);
        }
    } else if (speech >= 0) {
        if (speech < 9 &&
            (object = (u8*)FindWORLDOBJ(strings + 88)) != 0) {
            GetWorldMat(*(f32**)(object + 40), world, 0);
            sprintf(state->effectName, &lbl_803485C0, speech);
            effect = InitCustomEffect(0, state->effectName, 0, 0);
            if (object != 0 && effect >= 0) {
                effect = StartFXSub(lbl_80348588, effect, effectState,
                                    0x80000, 0x800);
                SfxSetParent(effect, gSceneRoot);
            }
            WindowCamActivate(0);
            object = (u8*)FindWORLDOBJ(strings + 44);
            if (object != 0) {
                MBTreeSetFlags(*(void**)(object + 40), 2, 0);
            }
            if (PlayerHasRune(-1, 0x1FE) != 0) {
                MBTreeSetAlpha(*(void**)(fn_8005B558(0x500) + 100),
                               0xFF, 1);
                lbl_80344C7C = 10;
                fn_8009C460(10);
            } else {
                lbl_80344C7C = 14;
                fn_8009C460(14);
            }
        } else if (speech < 9) {
            ErrorPrintf(strings + 104);
        }
    }
}

/* Scan players for a level-up (score/level fields 0x1EC0/0x1EDC); if any
 * levelled up, summon the "WIZARD" congratulation.  Internal. */
#pragma opt_common_subs off
#pragma opt_propagation off
int sumnerCheckLevelUp(void) {
    TowerMsgState* s = &lbl_8028C288;
    s32 count = 0;
    s32 i;
    Player* p;
    s32 off;
    u8* levelSlot;

    if (lbl_80343E4C < 0) {
        return 0;
    }
    if (lbl_80344C68 > lbl_803485F8) {
        return 0;
    }
    for (p = &gPlayers[0], i = 0, off = 0; i < 4; i++, off += 4, p++) {
        levelSlot = (u8*)s + off;
        *(s32*)(levelSlot += 76) = 0;
        if (p->state != 0 && *(u32*)((u8*)p + 0xF0) != (u32)lbl_80343D6C) {
            int lvlOld = ExpToLevel(*(s32*)((u8*)p + p->character * 24 + 0x1EDC));
            int lvlNew = ExpToLevel(p->exp);

            if (lvlNew >= 99 && lvlOld < 99) {
                *(s32*)levelSlot = lvlNew;
                count++;
            } else if (lvlOld / 10 != lvlNew / 10) {
                *(s32*)levelSlot = lvlNew;
                count++;
            }
            *(s32*)((u8*)p + p->character * 24 + 0x1EDC) = p->exp;
            *(s32*)((u8*)p + p->character * 24 + 0xA90) = p->exp;
        }
    }
    if (count == 0) {
        return 0;
    }
    if ((u32)lbl_80344C64 == 0) {
        void* atree = (void*)AtreeMatch(sGoodWizObj, (char*)&lbl_803485A4, 0);

        if (atree != 0) {
            s->wizAtree = (void*)AtreeInit(atree, &s->wizAtree, 0, 0xC00880);
            lbl_80344C64 = (s32)MBNewNode(gSceneRoot, gIdentityMatrix, 1);
            MBNodeSetParent(*(void**)s->wizAtree, (void*)lbl_80344C64);
        }
    }
    lbl_80344C70 = 0;
    lbl_80344C68 = lbl_80348600;
    DisablePlayerControls();
    lbl_80344C8C = 0;
    CaptionTextReset();
    return count;
}
#pragma opt_propagation reset
#pragma opt_common_subs reset

/* Activate Sumner hint mode. */
void SumnerHintsActivate(void) {
    lbl_80344C90 = 4;
    lbl_80344C54 = 1;
}

/* Advance the Sumner animation / hint timer (music-fade aware). */
int SumnerAnimate(void) {
    if (lbl_803485F8 == lbl_80344C58) {
        lbl_80344C90 = 3;
        lbl_80344C54 = 1;
        lbl_80344C58 = lbl_80348608 + sMusicFadeBase;
        return 0;
    }
    if (sMusicFadeBase < lbl_80344C58) {
        return 0;
    }
    HintMenu();
    lbl_80344C58 = lbl_80348588;
    return 1;
}

/* Tear down the live Sumner object. */
void SumnerEnd(void) {
    sSumnerObj = 0;
    lbl_80344C64 = 0;
}

/* Create the good-wizard (GWIZ) object, bind its animation tree, position it
 * at the lookout param, and reset tower/sumner state globals. */
void SumnerInit(void) {
    TowerMsgState* s = &lbl_8028C288;
    void* lp;

    s->gwizAtree =
        (void*)AtreeInit((void*)AtreeMatch(sGoodWizObj, (char*)&lbl_80348610, 0),
                           &s->gwizAtree, 0, 2048);
    sSumnerObj = MBNewNode(gSceneRoot, gIdentityMatrix, 1);
    MBNodeSetParent(*(void**)s->gwizAtree, sSumnerObj);
    lp = FindLookoutParam(0);
    if (lp != 0) {
        CopyMat4((f32*)lp, sSumnerObj);
    }
    lbl_80344C90 = 0;
    lbl_80344C54 = 0;
    lbl_80344C64 = 0;
    lbl_80344C74 = 0;
    lbl_80343E4C = 0;
    lbl_80343E50 = -1;
    lbl_80343E54 = -1;
    lbl_80343E58 = -1;
    lbl_80344C78 = 60;
    lbl_80344C6C = -1;
    lbl_80344C7C = 0;
}
