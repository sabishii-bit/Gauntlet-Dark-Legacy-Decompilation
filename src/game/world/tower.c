#include "types.h"
#include "game/player.h"

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
extern void* sSumnerObj;   /* live Sumner (good wizard) object handle   */
extern s32 lbl_80344C64;   /* Sumner active/state latch                 */
extern void* sGoodWizObj;  /* GWIZ animation/effect object              */

/* ===================================================================== */

/* Zero the whole tower/sumner state block. Called from game_main. */
void TowerInit(void) {
    /* clears sumner handles, timers (-1), speech floats, hint state */
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
    (void)world;
    return 0;
}

/* Award world runes to active players after the dwell timer elapses
 * (fields 0x928-0x930 timer, sets rune bit at 0xA8C). */
void towerAwardWorldRunes(void) {
}

/* Return the per-level status byte (field 0x1CD0) for a world record. */
int towerGetLevelFlag(u8* rec, int level) {
    (void)rec; (void)level;
    return -1;
}

/* Record level/boss completion across players (ResolveWorldData, sets
 * masks 0xDDC-0xDE2 and status byte 0x1CD0). */
void towerRecordLevelBeaten(u8* dst, int world) {
    (void)dst; (void)world;
}

/* Set an inventory bit (field 0x2230/0x2232) for a player - garg/legend item. */
int playerGiveGargItem(int player, int item, int a) {
    (void)player; (void)item; (void)a;
    return 0;
}

/* True if all active players meet the level-record-A requirement (field 0xDE8). */
int towerAllPlayersMetLevelReq(int level) {
    (void)level;
    return 0;
}

/* Get a per-level record-A value (field 0xDE8). */
int towerGetLevelRecord(int level) {
    (void)level;
    return 0;
}

/* Advance the per-level record-A counter toward its cap (field 0xDE8, 0x2234). */
void towerAdvanceLevelRecord(int player, int level) {
    (void)player; (void)level;
}

/* True if all active players meet the level-record-B requirement (field 0xDEE). */
int towerAllPlayersMetBossReq(int level) {
    (void)level;
    return 0;
}

/* Per-record level status: 0/1/2 (field 0xDEE vs requirement table). Internal. */
int towerLevelStatus(int world, int level) {
    (void)world; (void)level;
    return 0;
}

/* Get a per-level record-B value (field 0xDEE). */
int towerBossStatus(int world, int level) {
    (void)world; (void)level;
    return 0;
}

/* Advance the per-level record-B counter (field 0xDEE, 0x223A, timers). */
void towerAdvanceBossRecord(int player, int level) {
    (void)player; (void)level;
}

/* Query the per-level "rune near" record (field 0xDD8). */
int towerGetRuneNearStat(int world, int level) {
    (void)world; (void)level;
    return 0;
}

/* Max per-level "rune near" value across levels (field 0xDD8). */
int towerGetRuneNearMax(int world) {
    (void)world;
    return 0;
}

/* Set a per-level "rune near" flag (field 0xDD8). */
void towerSetRuneNear(int world, int level) {
    (void)world; (void)level;
}

/* Return the cumulative rune bitmask (field 0x1EC8) for a player. */
int playerGetRuneBits(int player) {
    (void)player;
    return 0;
}

/* Does the player (or any, if -1) have the given rune(s)?  Field 0x1EC8/0xDD4. */
int PlayerHasRune(int player, int rune) {
    (void)player; (void)rune;
    return 0;
}

/* Return the cumulative shard bitmask (field 0x1ECA) for a player. */
int playerGetShardBits(int player) {
    (void)player;
    return 0;
}

/* Does the player (or any, if -1) have the given shard(s)? Field 0x1ECA/0xDD6. */
int PlayerHasShard(int player, int shard) {
    (void)player; (void)shard;
    return 0;
}

/* Cooldown-gated "you need N crystals" tower message. */
void TowerNeedCrystalsMsg(int who, int slot) {
    (void)who; (void)slot;
}

/* Cooldown-gated "you need gargoyle items" tower message. */
void TowerNeedGargItemsMsg(int who, int slot) {
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
    (void)worldId;
    return 0;
}

/* True while a Sumner speech timer is running. */
int sumnerSpeechActive(void) {
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
