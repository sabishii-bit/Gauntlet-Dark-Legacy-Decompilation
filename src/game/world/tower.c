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
extern void* FindWORLDOBJ(int a, int b, int c);
extern int   ResolveWorldData(int a, int b);
extern int   AtreeMatch(void* obj, char* name, int flag);
extern void  CaptionText(void);
extern void  CaptionTextReset(void);
extern void  CaptionTextSub(int a, int b, int c);
extern int   GetScrollText(int a, int b);
extern int   GetStringText(int a, int b);
extern int   GetStringListText(int a, int b);
extern int   FindStringMessageListSub_8001FC4C(int a, char* list);
extern void  InitCustomEffect(char* name);
extern void  TriggerCameraEnd(void);
extern int   FindLookoutParam(int a);
extern int   AudioSoundPlaying(int a);
extern int   fn_80057BC8(int item);
extern int   sprintf(char* buf, const char* fmt, ...);
extern void  ErrorPrintf(const char* fmt, ...);

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
 *  so it is left as lbl_80275AE0 rather than renamed here.)
 */
/* Typed view of the shared player-record array (kept as lbl_80275AE0: the
 * symbol is referenced by the Matching shopquery.c, so it must not be renamed;
 * the Player layout is defined in game/player.h). */
extern Player lbl_80275AE0[4];

/* tower/sumner state (r13 small-data globals) */
extern s32 lbl_80343D6C;
extern s32 lbl_80343E48;
extern s32 lbl_80343E4C;
extern s32 lbl_80343E50;
extern s32 lbl_80343E54;
extern s32 lbl_80343E58;
extern s32 sMusicTrackLo;
extern s32 sMusicTrackHi;
extern s32 lbl_803448F0;
extern s32 lbl_80344C4C;
extern void* sSumnerObj;   /* live Sumner (good wizard) object handle   */
extern s32 lbl_80344C54;
extern f32 lbl_80344C58;
extern f32 lbl_80344C5C;
extern s32 lbl_80344C60;
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

extern s32 lbl_80124C70[9];
extern s32 lbl_80124D14[14];
extern s32 lbl_80124D4C[14];
extern s32 lbl_80124D94[3];
extern s32 lbl_80124DA0[9];
extern f32 lbl_80348588;
extern f32 lbl_8034858C;
extern f32 lbl_80348590;

#define PLAYER_AT(player, offset, type) \
    (*(type*)((u8*)&lbl_80275AE0[(player)] + (offset)))
#define TOWER_SAVE(player) \
    (&lbl_80275AE0[(player)].char_save[lbl_80275AE0[(player)].character])

/* ===================================================================== */

/* Zero the whole tower/sumner state block. Called from game_main. */
void TowerInit(void) {
    s32 none = -1;
    s32 zero = 0;

    lbl_80344C4C = zero;
    lbl_80343E48 = none;
    sSumnerObj = (void*)zero;
    lbl_80344C54 = zero;
    lbl_80344C58 = lbl_80348588;
    lbl_80344C5C = lbl_80348588;
    lbl_80344C60 = zero;
    lbl_80344C64 = zero;
    lbl_80344C68 = lbl_80348588;
    lbl_80344C6C = zero;
    lbl_80344C70 = zero;
    lbl_80344C74 = zero;
    lbl_80344C78 = zero;
    lbl_80343E4C = none;
    lbl_80343E50 = none;
    lbl_80343E54 = none;
    lbl_80343E58 = none;
    lbl_80344C7C = zero;
    lbl_80344C80 = zero;
    lbl_80344C84 = zero;
    lbl_80344C88 = zero;
}

/* Resolve the current world object handle (newcam GetPlayerAvgPos + fn_800668F8). */
void towerUpdateCurWorldObj(void) {
}

/* Per-frame: if a rune/shard object (type1 sub13/sub10) is near and not yet
 * collected by any player, play the proximity audio cue. */
void towerRuneNearAudio(void) {
}

/* Is a world unlocked?  Checks cumulative rune/shard masks and per-player
 * level records against the world's requirement table.  Heavily called (UI). */
int WorldOpen(int world) {
    s32 requirement;
    s32 player;
    s32 best = 0;

    if (world == 8) {
        if (PlayerHasRune(-1, 0x7FE) != 0) {
            return 1;
        }
        return 0;
    }
    if (world == 6) {
        if (PlayerHasRune(-1, 0x3FE) == 0) {
            return 0;
        }
        if (PlayerHasShard(-1, 0xFFF) != 0) {
            return 1;
        }
        return 0;
    }
    if (world == 5) {
        if (PlayerHasRune(-1, 0x1FE) != 0) {
            return 1;
        }
        return 0;
    }
    if (world == 13) {
        return 1;
    }

    requirement = lbl_80124D4C[world];
    for (player = 0; player < 4; player++) {
        if (lbl_80275AE0[player].state != 0) {
            s32 status = towerLevelStatus(player, requirement);
            s32 value;

            if (status != 0) {
                return 1;
            }
            value = ((s16*)&TOWER_SAVE(player)->completion2)[requirement];
            if (value > best) {
                best = value;
            }
        }
    }
    if (lbl_80124C70[requirement] <= best) {
        return 1;
    }
    return 0;
}

/* Award world runes to active players after the dwell timer elapses
 * (fields 0x928-0x930 timer, sets rune bit at 0xA8C). */
int towerAwardWorldRunes(void) {
    s32 rune = lbl_80124DA0[sMusicTrackLo];
    s32 awarded = 0;
    s32 player;

    if (rune < 0) {
        return 0;
    }
    for (player = 0; player < 4; player++) {
        if (lbl_80275AE0[player].state == 1) {
            PLAYER_AT(player, 0x930, s32)++;
            PLAYER_AT(player, 0x928, s32) = rune + 0x200;
            PLAYER_AT(player, 0x92C, f32) = lbl_8034858C;
            if (PLAYER_AT(player, 0x930, s32) >= lbl_803448F0) {
                awarded = 1;
            }
        }
    }
    if (awarded == 0) {
        return 0;
    }
    for (player = 0; player < 4; player++) {
        if (lbl_80275AE0[player].state == 1) {
            PLAYER_AT(player, 0xA8C, u16) |= 1 << (rune - 8);
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
void towerRecordLevelBeaten(u8* dst, int world) {
    (void)dst; (void)world;
}

/* Set an inventory bit (field 0x2230/0x2232) for a player - garg/legend item. */
void playerGiveGargItem(int player, int item, int count) {
    if (count == fn_80057BC8(item) - 1) {
        PlayerCharSave* save = TOWER_SAVE(player);
        u16* first = (u16*)((u8*)save + 0x145C);
        u16 bit = 1 << item;

        if ((*first & bit) == 0) {
            *first |= bit;
        } else {
            *(u16*)((u8*)save + 0x145E) |= bit;
        }
    }
}

/* True if all active players meet the level-record-A requirement (field 0xDE8). */
int towerAllPlayersMetLevelReq(int level) {
    s32 player;
    s32 best = 0;

    if (level > 2) {
        level = 2;
    }
    for (player = 0; player < 4; player++) {
        if (lbl_80275AE0[player].state != 0) {
            s32 value;

            if (PLAYER_AT(player, 0xF0, s32) == lbl_80343D6C) {
                value = 2;
            } else {
                value = ((s16*)&TOWER_SAVE(player)->completion1)[level];
                if (value < 0) {
                    value = 2;
                } else if ((u32)value == (u32)lbl_80124D94[level]) {
                    value = 1;
                } else {
                    value = 0;
                }
            }
            if (value != 0) {
                return 1;
            }
            value = ((s16*)&TOWER_SAVE(player)->completion1)[level];
            if (value > best) {
                best = value;
            }
        }
    }
    if (lbl_80124D94[level] <= best) {
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
    s32 first = player;
    s32 last = player;
    s32 i;

    if (player < 0) {
        first = 0;
        last = 3;
    }
    for (i = first; i <= last; i++) {
        Player* record = &lbl_80275AE0[i];

        if (record->state == 1 || record->state == 4) {
            s16* value = &((s16*)&TOWER_SAVE(i)->completion1)[level];

            if (*value >= 0 && *value < lbl_80124D94[level]) {
                (*value)++;
                if (sMusicTrackHi == 13) {
                    PLAYER_AT(i, 0x2234 + level * 2, s16)++;
                }
            }
            PLAYER_AT(i, 0x928, s32) = level + 0x100;
            PLAYER_AT(i, 0x92C, f32) = lbl_80348590;
        }
    }
}

/* True if all active players meet the level-record-B requirement (field 0xDEE). */
int towerAllPlayersMetBossReq(int level) {
    s32 player;
    s32 best = 0;

    for (player = 0; player < 4; player++) {
        if (lbl_80275AE0[player].state != 0) {
            s32 value;

            if (PLAYER_AT(player, 0xF0, s32) == lbl_80343D6C) {
                value = 2;
            } else {
                value = ((s16*)&TOWER_SAVE(player)->completion2)[level];
                if (value < 0) {
                    value = 2;
                } else if ((u32)value == (u32)lbl_80124C70[level]) {
                    value = 1;
                } else {
                    value = 0;
                }
            }
            if (value != 0) {
                return 1;
            }
            value = ((s16*)&TOWER_SAVE(player)->completion2)[level];
            if (value > best) {
                best = value;
            }
        }
    }
    if (lbl_80124C70[level] <= best) {
        return 1;
    }
    return 0;
}

/* Per-record level status: 0/1/2 (field 0xDEE vs requirement table). Internal. */
int towerLevelStatus(int player, int level) {
    s16 value;

    if (lbl_80275AE0[player].state == 0) {
        return 0;
    }
    if (PLAYER_AT(player, 0xF0, u32) == (u32)lbl_80343D6C) {
        return 2;
    }
    value = ((s16*)&TOWER_SAVE(player)->completion2)[level];
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

    if (value >= 0) {
        return value;
    }
    return lbl_80124C70[level];
}

/* Advance the per-level record-B counter (field 0xDEE, 0x223A, timers). */
void towerAdvanceBossRecord(int player, int level) {
    s32 first = player;
    s32 last = player;
    s32 i;

    if (player < 0) {
        first = 0;
        last = 3;
    }
    for (i = first; i <= last; i++) {
        Player* record = &lbl_80275AE0[i];

        if (record->state == 1 || record->state == 4) {
            s16* value = &((s16*)&TOWER_SAVE(i)->completion2)[level];

            if (*value >= 0 && *value < lbl_80124C70[level]) {
                (*value)++;
                if (sMusicTrackHi == 13) {
                    PLAYER_AT(i, 0x223A + level * 2, s16)++;
                }
            }
            PLAYER_AT(i, 0x928, s32) = level;
            PLAYER_AT(i, 0x92C, f32) = lbl_80348590;
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
    mask = ~(1 << level);
    for (; player <= last; player++) {
        TOWER_SAVE(player)->rune_near &= mask;
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
    for (; player <= last; player++) {
        if (lbl_80275AE0[player].state == 1 ||
            lbl_80275AE0[player].state == 4) {
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
    for (; player <= last; player++) {
        if (lbl_80275AE0[player].state != 0 &&
            (TOWER_SAVE(player)->rune_near & bit) != 0) {
            return 1;
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
        if (lbl_80275AE0[player].state == 1) {
            lbl_80275AE0[player].runes |= bit;
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
        if (lbl_80275AE0[player].state != 0) {
            bits |= TOWER_SAVE(player)->rune_stones |
                    lbl_80275AE0[player].runes;
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
        if (lbl_80275AE0[player].state == 1 ||
            lbl_80275AE0[player].state == 4) {
            lbl_80275AE0[player].shards |= bit;
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
        if (lbl_80275AE0[player].state != 0) {
            bits |= TOWER_SAVE(player)->rune_stones2 |
                    lbl_80275AE0[player].shards;
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
    (void)who; (void)slot;
}

/* Cooldown-gated "you need N crystals" (or demo-closed) tower message. */
void TowerNeedCrystalsMsg(int who, int slot) {
    (void)who; (void)slot;
}

/* Per-frame tower message/state dispatcher.  Served by do_players; drives
 * caption resets, Sumner animation matching, world-object lookups and the
 * speech/level-up/effect helpers (SumnerDoSpeech/SumnerSpeechEnd/
 * sumnerCheckLevelUp). */
void TowerCheckMessages(void) {
}

/* Enter the hub tower: spawn the collected RUNE%d/SHARD%d/RUNE13 (+wizard)
 * effects, set up the display and hand off to TowerCheckMessages. */
void EnterTower(void) {
}

/* Update the good-wizard presence/timer from per-player progress (field
 * 0xA90); sets the sumner appear/idle float. */
void sumnerUpdatePresence(void) {
}

/* Map a world id to its ordering index via the 14-entry world-order table. */
int GetWorldOrder(int worldId) {
    s32 i;

    for (i = 0; i < 14; i++) {
        if (lbl_80124D14[i] == worldId) {
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
int SumnerDoSpeech(void) {
    return 0;
}

/* End the current Sumner speech: stop the camera move, spawn the reward
 * effects and clean up caption/wizard state.  Internal. */
void SumnerSpeechEnd(void) {
}

/* Scan players for a level-up (score/level fields 0x1EC0/0x1EDC); if any
 * levelled up, summon the "WIZARD" congratulation.  Internal. */
int sumnerCheckLevelUp(void) {
    return 0;
}

/* Activate Sumner hint mode. */
void SumnerHintsActivate(void) {
    lbl_80344C90 = 4;
    lbl_80344C54 = 1;
}

/* Advance the Sumner animation / hint timer (music-fade aware). */
void SumnerAnimate(void) {
}

/* Tear down the live Sumner object. */
void SumnerEnd(void) {
    sSumnerObj = 0;
    lbl_80344C64 = 0;
}

/* Create the good-wizard (GWIZ) object, bind its animation tree, position it
 * at the lookout param, and reset tower/sumner state globals. */
void SumnerInit(void) {
    AtreeMatch(sGoodWizObj, "GWIZ", 0);
    FindLookoutParam(0);
}
