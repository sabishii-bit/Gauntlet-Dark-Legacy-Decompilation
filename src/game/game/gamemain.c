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
/*                                      level via init_next_level.      */
/*   0x8005638C  init_next_level    -- builds the "levels/level%s" path */
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

void game_main(void);
void do_stats_display(void);
void LoadTowerAndSelect(s32 arg0);
s32  init_next_level(s32 arg0);
void init_thermometer(void);
void GetEnemyTypes(void);

/* Recovered this pass (bodies still supplied from asm; NonMatching). */
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
