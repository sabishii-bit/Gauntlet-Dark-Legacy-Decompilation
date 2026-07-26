#include "types.h"

/* Slice of the Gauntlet enemy module (Xbox ENEMY.OBJ / enemy.c) assigned to
 * region 0x8004A430-0x80050054.  ENEMY.OBJ is a very large single translation
 * unit that, on the GameCube build, spans well below this window (the AI
 * dispatch / move_logic* / do_ai / do_enemy_move functions live in the
 * 0x8004xxxx range beneath 0x4A430) and continues just past it (init_enemy is
 * the function at 0x80050054, immediately above this slice).  This file is the
 * middle-to-upper slice, wired NonMatching so the tree stays green while the
 * symbols are mapped; dtk substitutes the original DOL bytes for this .text
 * range, so byte-matching is out of scope for this pass.
 *
 * IDENTIFIED (real Xbox-PDB names, high confidence from strings + call graph
 * + struct layout).  The enemy record is a 0x394 (916) byte struct; the active
 * array base used throughout is gEnemies @0x80251C18, count gNumEnemies
 * @0x80344744.  A parallel table of four 0x335C (13148) byte "generator/head"
 * records lives at 0x80275AE0.
 *
 *   do_enemies              0x8004D078  - per-frame master loop over every
 *                                         active enemy; called from game_main.
 *                                         Runs animation/AI, movement, spin
 *                                         damping, shadow/alpha; global.
 *   generate_enemy          0x8004F4B4  - public spawn entry (called from boss
 *                                         code and BossGenerateEnemy); resolves
 *                                         type, validates against the type
 *                                         tables, grabs a slot and builds the
 *                                         enemy; global.
 *   find_enemy_slot         0x8004FD00  - scans gEnemies for a reusable slot,
 *                                         recycling the least-important enemy
 *                                         via kill_enemy when full; global.
 *   SetEnemyObj             0x8004FE34  - fills in a freshly allocated enemy's
 *                                         object/shadow/anim fields from the
 *                                         per-type prefix table @0x8011AF48.
 *   uncouple_enemy          0x8004F2D8  - unlinks an enemy from its generator's
 *                                         spawn list and decrements the live
 *                                         count; owner of the "Enemy has non
 *                                         generator generator" ErrorPrintf.
 *   kill_enemy              0x8004EFE4  - death/removal: drops loot, spawns the
 *                                         death FX, detaches 3D objects and
 *                                         sounds, then uncouple_enemy; global.
 *   check_enemy_pos         0x8004F9AC  - validates a candidate spawn position
 *                                         (floor + wall + object collision).
 *   turn_enemy_ang          0x8004CBB8  - steps an enemy's facing angle toward
 *                                         a target by the per-type turn rate
 *                                         (table @0x8011BED8); shared by the
 *                                         move_logic* dispatch (14 callers).
 *   find_neighbor_milestone 0x8004C9DC  - locates a path milestone's neighbour
 *                                         and picks the shorter branch.
 *   update_enemy_milestone  0x8004E514  - advances an enemy's waypoint index
 *                                         once it reaches its current target.
 *
 * The remaining functions in the slice are left as fn_XXXXXXXX and documented
 * below by behaviour (candidate PDB names noted where plausible but not certain
 * enough to commit a name):
 *   fn_8004E6F8  - high-assist (10+ cross-module callers: ProcessEffects,
 *                  weapons, world collision) enemy move-with-collision +
 *                  knockback core; returns hit/stopped/special.  Global.
 *   fn_8004CD1C  - accelerate an enemy along an angle (cos/sin -> velocity);
 *                  shared movement helper (12 callers).
 *   fn_8004C8CC  - probe a position for wall/object/world collision (blocked
 *                  test) shared by the move_logic* set (6 callers).
 *   fn_8004CE38  - compute left/right look-ahead probe angles.
 *   fn_8004DF58  - process an enemy's attach/trail segment against its head
 *                  record (calls fn_8004E6F8, fn_8004E448).
 *   fn_8004DC2C  - post-move collision resolution / bounce / land state.
 *   fn_8004D958  - per-enemy tick wrapper (drives fn_80046854 AI dispatch).
 *   fn_8004C3E4  - proximity/activation gate keyed on the current level desc.
 *   fn_8004E448  - footstep / splash surface FX by floor material.
 *   fn_8004E67C  - per-frame free pass over an object index list (candidate:
 *                  a reset/cleanup routine; called from game_main).
 *   fn_8004F87C  - enemy type/animation-class resolver used by generate_enemy.
 *   fn_8004FBC8  - 8-way spawn direction offset generator (candidate: gendir).
 *   fn_8004A430 .. fn_8004C650  - the behaviour block driven by the AI/attack
 *                  dispatch (fn_80046854 et al. just below this slice); these
 *                  read gEnemies + the generator table and run per-behaviour
 *                  distance / facing / attack logic.
 *   fn_8004CFAC, fn_8004D030, fn_8004DB3C, fn_8004E5F8, fn_8004E67C, fn_8004F1DC,
 *   fn_8004F404 - small enemy helpers (state pokes, timers, small FX hooks).
 *
 * See config/GUNE5D/symbols.txt for the mapped names.  Matching is intentionally
 * not attempted here (high mismatch accepted); this slice exists to carry the
 * symbol map and keep the tree green.
 */

/* --- enemy record array + counts (module data) --- */
extern u8 gEnemies[];      /* 0x80251C18 active enemy records, stride 0x394 */
extern s32 gNumEnemies;    /* 0x80344744 number of active enemy slots       */

/* The bodies below are documentation-level reconstructions of the two clearest
 * public entry points.  They are compiled but not linked (NonMatching); the
 * shipped bytes come from the original DOL. */

/* forward decls for the cross-referenced enemy entry points */
s32 find_enemy_slot(s32 type, s32 level);
void kill_enemy(s32 index);
void uncouple_enemy(s32 index);

/* uncouple_enemy: detach enemy `index` from its generator's spawn list.
 * (Structural sketch; field offsets are documented in the header above.) */
void uncouple_enemy(s32 index) {
    (void)index;
    /* unlink prev/next spawn-list indices, decrement the generator's live
     * count, and emit "Enemy has non generator generator" if the parent slot
     * is not actually a generator.  Body substituted from original bytes. */
}

/* find_enemy_slot: return a free/recyclable enemy slot for a new spawn.
 * Recycles the least-important live enemy through kill_enemy when the array is
 * full; returns -1 when the request cannot be satisfied. */
s32 find_enemy_slot(s32 type, s32 level) {
    (void)type;
    (void)level;
    /* scan gEnemies scoring each slot by state/health, pick the weakest,
     * kill_enemy() it if needed, return its index.  Body substituted. */
    return -1;
}
