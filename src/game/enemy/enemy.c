#include "game/enemy.h"
#include "game/mb_font.h"
#include "game/item.h"
#include "game/gamemode.h"
#include "game/worldobj.h"
#include "game/worldcol.h"
#include "game/dyngrid.h"
#include "game/leveldata.h"
#include "game/mbnode.h"
#include "game/player.h"

/* ENEMY.OBJ owns the parallel type tables at 0x8011AF48..0x8011BFF8.
 * The Xbox PDB corroborates the 36-byte type/desc/prefix record and the
 * 34-entry attribute arrays. GameCube values, ordering and pointer targets
 * come from its .data; its 45 name pointers precede 11 state-name pointers.
 * Existing exported labels stay stable for other TUs while their declarations
 * are recovered. These are separate globals, not members of one large struct. */
typedef struct EnemyTypeName {
    e_e_tpye type;
    char desc[16];
    char prefix[16];
} EnemyTypeName;

EnemyTypeName lbl_8011AF48[44] = {
    { E_SCORP, "sco", "SCO" },
    { E_TROLL, "tro", "TRO" },
    { E_DEMON, "dem", "DEM" },
    { E_RAT, "rat", "RAT" },
    { E_GRUNT, "gru", "GRU" },
    { E_KNIGHT, "kni", "KNI" },
    { E_SNAKE, "sna", "SNA" },
    { E_SORCERER, "sor", "SOR" },
    { E_MUMMY, "mum", "MUM" },
    { E_SPIDER, "spi", "SPI" },
    { E_LIZARDMAN, "liz", "LIZ" },
    { E_TREEFOLK, "tre", "TRE" },
    { E_MAGGOT, "mag", "MAG" },
    { E_ZOMBIE, "zom", "ZOM" },
    { E_PLAGUE, "pla", "PLA" },
    { E_WOLF, "wol", "WOL" },
    { E_ICE, "ice", "ICE" },
    { E_WORM, "wrm", "WRM" },
    { E_DOG, "dog", "DOG" },
    { E_SKELETON, "ske", "SKE" },
    { E_GHOST, "gho", "GHO" },
    { E_ACID, "aci", "ACI" },
    { E_HAND, "han", "HAN" },
    { E_IMP, "imp", "IMP" },
    { E_WARLOCK, "war", "WAR" },
    { E_SKY, "sky", "SKY" },
    { E_WHIRLWIND, "wind", "WIND" },
    { E_GARM2, "grm", "GRM" },
    { E_GOLEM, "golem", "GOLEM" },
    { E_DEATH, "death", "DEATH" },
    { E_IT, "it", "IT" },
    { E_GARGOYLE, "gar", "GAR" },
    { E_GENERAL, "general", "GEN" },
    { E_DRAGON, "dragon", "DRAGON" },
    { E_CHIMERA, "chimera", "CHIM" },
    { E_DJINN, "djinn", "DJINN" },
    { E_DRIDER, "drider", "DRIDER" },
    { E_PBOSS, "pboss", "PBOSS" },
    { E_YETI, "yeti", "YETI" },
    { E_WRAITH, "wraith", "WRAITH" },
    { E_LICH, "lich", "LICH" },
    { E_SKORNE1, "skorne1", "SKORNE" },
    { E_SKORNE2, "skorne2", "SKORNE" },
    { E_GARM, "garm", "GARM1" },
};

/* These literals own .rodata 0x80112370..0x80112538 and the leading
 * .sdata2 strings at 0x803466D0..0x80346810. MWCC places strings of at most
 * eight bytes including the terminator in .sdata2; the later debug formats
 * share this same TU pool. */
char* lbl_8011B578[45] = {
    "SCORPION",
    "TROLL",
    "DEMON",
    "RAT",
    "GRUNT",
    "KNIGHT",
    "SNAKE",
    "SORCERER",
    "MUMMY",
    "SPIDER",
    "LIZARDMAN",
    "TREEFOLK",
    "MAGGOT",
    "ZOMBIE",
    "PLAGUE",
    "WOLF",
    "ICE GRUNT",
    "ICE DEMON",
    "DOG",
    "SKELETON",
    "GHOST",
    "ACID",
    "HAND",
    "IMP",
    "WARLOCK",
    "SKY",
    "WHIRLWIND",
    "GARM2",
    NULL,
    "GOLEM",
    "DEATH",
    "IT",
    "GARGOYLE",
    "GENERAL",
    "DRAGON",
    "CHIMERA",
    "GENIE",
    "DRIDER",
    "PBOSS",
    "YETI",
    "WRAITH",
    "LICH",
    "SKORNE1",
    "SKORNE2",
    "GARM",
};

char* state_tab[11] = {
    "INACTIVE",
    "ACTIVE",
    "SELECT",
    "ON_EXIT",
    "ON_NEXT_LEVEL",
    "SLEEP",
    "DECORATION",
    "DYING",
    "CONTINUE",
    "ERROR",
    "UNKNOWN",
};

f32 ene_height[34] = {
    3.0f, 6.0f, 6.0f, 3.0f, 6.0f, 6.0f,
    3.0f, 6.0f, 6.0f, 3.0f, 6.0f, 6.0f,
    3.0f, 6.0f, 6.0f, 3.0f, 6.0f, 6.0f,
    3.0f, 6.0f, 6.0f, 3.0f, 3.0f, 6.0f,
    6.0f, 6.0f, 6.0f, 10.0f, -1.0f, 12.0f,
    6.0f, 5.0f, 10.0f, 6.0f,
};

f32 ene_width[34] = {
    1.5f, 1.5f, 1.79999995f, 1.5f, 1.5f, 1.79999995f,
    1.5f, 1.5f, 1.79999995f, 1.5f, 1.5f, 1.79999995f,
    1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.79999995f,
    1.5f, 1.5f, 1.79999995f, 0.75f, 0.75f, 2.0f,
    2.0f, 2.0f, 2.0f, 4.0f, -1.0f, 3.0f,
    1.5f, 1.5f, 6.0f, 2.0f,
};

f32 ene_attn[34] = {
    2.0f, 3.79999995f, 3.79999995f, 2.0f, 3.79999995f, 3.79999995f,
    2.0f, 3.79999995f, 3.79999995f, 2.0f, 3.79999995f, 3.79999995f,
    2.0f, 3.79999995f, 3.79999995f, 2.0f, 3.79999995f, 3.79999995f,
    2.0f, 3.79999995f, 3.79999995f, 0.5f, 0.5f, 3.79999995f,
    3.79999995f, 3.79999995f, 3.79999995f, 3.79999995f, -1.0f, 5.0f,
    3.0f, 3.0f, 5.0f, 4.0f,
};

f32 ene_coll[34] = {
    1.5f, 3.0f, 3.0f, 1.5f, 3.0f, 3.0f,
    1.5f, 3.0f, 3.0f, 1.5f, 3.0f, 3.0f,
    1.5f, 3.0f, 3.0f, 1.5f, 3.0f, 3.0f,
    1.5f, 3.0f, 3.0f, 1.5f, 1.5f, 3.0f,
    3.0f, 3.0f, 3.0f, 4.0f, -1.0f, 4.0f,
    3.0f, 3.0f, 3.0f, 3.0f,
};

f32 lbl_8011B878[34] = {
    0.100000001f, 0.100000001f, 0.119999997f, 0.100000001f, 0.100000001f, 0.100000001f,
    0.100000001f, 0.100000001f, 0.100000001f, 0.100000001f, 0.100000001f, 0.100000001f,
    0.100000001f, 0.100000001f, 0.100000001f, 0.100000001f, 0.100000001f, 0.100000001f,
    0.100000001f, 0.100000001f, 0.100000001f, 0.0199999996f, 0.0500000007f, 0.100000001f,
    0.100000001f, 0.100000001f, 0.100000001f, 0.100000001f, -1.0f, 0.0900000036f,
    0.125f, 0.100000001f, 0.100000001f, 0.0900000036f,
};

f32 lbl_8011B900[34] = {
    12.0f, 15.0f, 18.0f, 12.0f, 15.0f, 18.0f,
    12.0f, 15.0f, 18.0f, 12.0f, 15.0f, 18.0f,
    12.0f, 15.0f, 15.0f, 12.0f, 15.0f, 18.0f,
    12.0f, 15.0f, 18.0f, 12.0f, 12.0f, 15.0f,
    15.0f, 15.0f, 15.0f, 20.0f, 0.0f, 20.0f,
    1.0f, 0.0f, 30.0f, 20.0f,
};

f32 enemy_armor[34] = {
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
    1.0f, 0.0f, 0.0f, 0.0f,
};

f32 lbl_8011BA10[34] = {
    21.0f, 30.0f, 46.0f, 21.0f, 30.0f, 46.0f,
    21.0f, 30.0f, 46.0f, 21.0f, 30.0f, 46.0f,
    21.0f, 30.0f, 30.0f, 21.0f, 30.0f, 46.0f,
    21.0f, 30.0f, 46.0f, 21.0f, 21.0f, 30.0f,
    46.0f, 46.0f, 46.0f, 100.0f, 0.0f, 200.0f,
    100.0f, 9999.0f, 500.0f, 200.0f,
};

f32 generator_armor[34] = {
    3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f,
    3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f,
    3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f,
    3.0f, 3.0f, 3.0f, 3.0f, 3.0f, 3.0f,
    3.0f, 3.0f, 3.0f, 3.0f, 0.0f, 3.0f,
    0.0f, 0.0f, 0.0f, 3.0f,
};

s32 lbl_8011BB20[34] = {
    1, 2, 3, 1, 2, 3,
    1, 2, 3, 1, 2, 3,
    1, 2, 2, 1, 2, 3,
    1, 2, 3, 1, 1, 2,
    3, 3, 3, 1, 0, 2,
    1, 1, 2, 2,
};

s32 lbl_8011BBA8[34] = {
    1, 2, 3, 1, 2, 3,
    1, 2, 3, 1, 2, 3,
    1, 2, 2, 1, 2, 3,
    1, 2, 3, 1, 1, 2,
    3, 3, 3, 15, 0, 30,
    1, 2, 300, 30,
};

s32 lbl_8011BC30[34] = {
    2, 4, 6, 2, 4, 6,
    2, 4, 6, 2, 4, 6,
    2, 4, 4, 2, 4, 6,
    2, 4, 6, 2, 2, 4,
    6, 6, 6, 2, 0, 4,
    1, 3, 4, 4,
};

s32 lbl_8011BCB8[34] = {
    2, 4, 6, 2, 4, 6,
    2, 4, 6, 2, 4, 6,
    2, 4, 4, 2, 4, 6,
    2, 4, 6, 2, 2, 4,
    6, 6, 6, 20, 0, 40,
    1, 4, 300, 40,
};

s32 sEnemyDefaultAlgorithm[34] = {
    2, 7, 7, 2, 7, 7,
    7, 7, 7, 7, 7, 7,
    2, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 0, 19,
    3, 27, 7, 7,
};

s32 enemy_damagetype[34] = {
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0,
};

s32 enemy_armortype[34] = {
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,
    0, 0, 0, 0,
};

f32 lbl_8011BED8[34] = {
    0.0490873866f, 0.0490873866f, 0.0490873866f, 0.0490873866f, 0.0490873866f, 0.0490873866f,
    0.0490873866f, 0.0490873866f, 0.0490873866f, 0.0490873866f, 0.0490873866f, 0.0490873866f,
    0.0490873866f, 0.0490873866f, 0.0490873866f, 0.0490873866f, 0.0490873866f, 0.0490873866f,
    0.0490873866f, 0.0490873866f, 0.0490873866f, 0.0490873866f, 0.0490873866f, 0.0490873866f,
    0.0490873866f, 0.0490873866f, 0.0490873866f, 0.0490873866f, 0.0f, 0.0490873866f,
    0.0490873866f, 0.0490873866f, 0.0490873866f, 0.0490873866f,
};

f32 lbl_8011BF60[38] = {
    0.0872664601f, -0.0872664601f, 0.17453292f, -0.17453292f, 0.261799395f, -0.261799395f,
    0.34906584f, -0.34906584f, 0.436332315f, -0.436332315f, 0.52359879f, -0.52359879f,
    0.610865235f, -0.610865235f, 0.69813168f, -0.69813168f, 0.785398185f, -0.785398185f,
    0.87266463f, -0.87266463f, 0.959931076f, -0.959931076f, 1.04719758f, -1.04719758f,
    1.13446403f, -1.13446403f, 1.22173047f, -1.22173047f, 1.30899692f, -1.30899692f,
    1.39626336f, -1.39626336f, 1.48352981f, -1.48352981f, 1.57079637f, -1.57079637f,
    1.65806282f, -1.65806282f,
};

#ifndef offsetof
#define offsetof(type, memb) ((u32) & ((type*)0)->memb)
#endif

/* Gauntlet Dark Legacy enemy module (Xbox ENEMY.OBJ / enemy.c).
 *
 * ENEMY.OBJ is a single very large translation unit.  On the GameCube build it
 * occupies one contiguous .text run, 0x800444C0 - 0x800520CC, sitting between
 * dynobjgrid.c (ends 0x800444C0) and gamemain.c (starts 0x800520CC).  This file
 * remains NonMatching while native instruction differences are reconstructed.
 * The default build links the extracted fallback object. Postprocessing is
 * retired; promotion requires exact native code, data, relocations and EH.
 *
 * Data used throughout:
 *   gEnemies        0x80251C18  active enemy records, stride 0x394 (916) bytes.
 *                               The compiler often addresses them via the base
 *                               mbdesc (= gEnemies - 0xE18) + 0xE18.
 *   gNumEnemies     0x80344744  number of active enemy slots.
 *   generator table 0x80275AE0  four 0x335C (13148) byte "generator/head" recs.
 *   jumptable_8011C0EC          32-entry AI move-logic dispatch table (see below).
 *   enemy type names 0x80112370 "SCORPION","SORCERER","LIZARDMAN","TREEFOLK",
 *                               "ICE GRUNT","ICE DEMON","SKELETON","WHIRLWIND",
 *                               "GARGOYLE" ... (debug names, GetEnemyType).
 *
 * ---------------------------------------------------------------------------
 * AI MOVE-LOGIC DISPATCH  (do_ai @0x80046854)
 * ---------------------------------------------------------------------------
 * do_ai reads the enemy's behaviour/state index at record offset 0x310 (0..31)
 * and jumps through jumptable_8011C0EC[state] to the matching move_logicNN
 * handler.  Each handler sets a desired facing/velocity and calls do_enemy_move
 * to commit the move + resolve collisions.  Verified state -> handler mapping
 * (jumptable read directly from the DOL):
 *
 *   state  0 -> move_logic00  0x80046B54      state 16 -> move_logic16 0x8004A78C
 *   state  1 -> move_logic01  0x80046F24      state 17 -> move_logic23 (shared)
 *   state  2 -> move_logic02  0x800471A4      state 18 -> move_logic18 0x8004AB20
 *   state  3 -> move_logic03  0x800473E8      state 19 -> move_logic19 0x8004AE94
 *   state  4 -> move_logic04  0x800475FC      state 20 -> move_logic20 0x8004B19C
 *   state  5 -> move_logic05  0x80047844      state 21 -> move_logic21 0x8004B5AC
 *   state  6 -> move_logic06  0x80047BF0      state 22 -> move_logic22 0x8004B788
 *   state  7 -> move_logic07  0x80047F9C      state 23 -> move_logic23 0x8004BB14
 *   state  8 -> move_logic08  0x80048408      state 24 -> move_logic24 0x8004BC5C
 *   state  9 -> move_logic00 (shared)         state 25 -> kill_enemy (inline)
 *   state 10 -> move_logic10  0x80048928      state 26 -> move_logic16 (shared)
 *   state 11 -> (inlined into do_ai)          state 27 -> (inlined into do_ai)
 *   state 12 -> move_logic12  0x80049A1C      state 28 -> move_logic28 0x8004BDDC
 *   state 13 -> move_logic13  0x80049C70      state 29 -> move_logic29 0x8004BF9C
 *   state 14 -> move_logic14  0x80049FD4      state 30 -> move_logic30 0x8004C3E4
 *   state 15 -> move_logic15  0x8004A430      state 31 -> move_logic31 0x8004C650
 *
 * move_logic09/11/17/25/26/27 have no distinct GC function: 09 reuses 00, 17
 * reuses 23, 26 reuses 16, and 11/25/27 are inlined into do_ai.  Sizes cross-
 * check against the Xbox PDB (e.g. move_logic05 and move_logic06 are 0x218 in
 * Xbox and both 0x3AC on GC; move_logic10 is the biggest handler in both).
 * move_logic13 ErrorPrintf's an enemy type name from the 0x80112370 table.
 *
 * ---------------------------------------------------------------------------
 * IDENTIFIED FUNCTIONS (real Xbox-PDB names, mapped in config symbols.txt)
 * ---------------------------------------------------------------------------
 *   closest_enemy     0x800444C0 - grid-iterate gEnemies within a firing cone;
 *                                  returns the closest enemy + its offset.
 *                                  Global (weapon/targeting callers).
 *   do_enemy_move     0x80044664 - central per-enemy move + collision commit,
 *                                  called by every move_logicNN and do_enemies
 *                                  (28 callers). Local. REAL BODY BELOW
 *                                  (final address-fold residual remains).
 *   do_enemy_collide  0x80045488 - collision core called by do_enemy_move;
 *                                  drives the EnemyCollide* helpers. Local.
 *   EnemyWorldDamage  0x80045FE4 - enemy-vs-world-object damage/push
 *                                  (WorldObjGetAllFlags -> damage_enemy). Local.
 *   do_ai             0x80046854 - the jumptable move-logic dispatcher above.
 *   move_logic00..31  0x80046B54..0x8004C650 - per-behaviour handlers (map above).
 *   find_neighbor_milestone 0x8004C9DC     turn_enemy_ang        0x8004CBB8
 *   do_enemies        0x8004D078 - per-frame master loop; global.
 *   update_enemy_milestone 0x8004E514
 *   damage_enemy      0x8004E6F8 - apply damage + knockback; global, called from
 *                                  weapons/effects/world (cross-module).
 *   kill_enemy        0x8004EFE4      uncouple_enemy   0x8004F2D8
 *   generate_enemy    0x8004F4B4      check_enemy_pos  0x8004F9AC
 *   find_enemy_slot   0x8004FD00      init_enemy      0x8004FE34
 *
 * Still fn_XXXXXXXX (behaviour understood, exact PDB name not yet pinned):
 *   fn_80045C30 - distance/proximity collision + damage helper called by
 *                 do_enemy_collide (EnemyCollidePlayer or EnemyCollideEnemy).
 *   fn_80046140 - per-type (type==30) generator/timer tick with audio + msgPost.
 *   fn_8004646C - world-grid query (StartItemGrid/NextGridItem).
 *   fn_80046680 - small gEnemies helper called by do_enemy_move.
 *   fn_8004C8CC - wall/object collision probe shared by the move_logic set.
 *   set_enemy_trans - accelerate along an angle (cos/sin -> velocity).
 *   fn_8004CE38 - left/right look-ahead probe angle helper.
 *   fn_8004D958 - per-enemy tick wrapper (drives do_ai).
 *   fn_8004DC2C/fn_8004DF58/fn_8004E448 - move post-processing / FX helpers.
 *   fn_8004F87C - generate_enemy type-resolution support.
 *   plus small state/timer pokes: fn_8004CFAC, fn_8004D030, fn_8004DB3C,
 *   adjust_msidx, enemy_update, fn_8004F1DC, check_vacancy.
 *
 * The complete TU must match code and data before being linked from source.
 */

/* --- enemy record array + counts (module data) ---
 * gEnemies[25] @0x80251C18 (stride 0x394) and gNumEnemies @0x80344744 are
 * declared by "game/enemy.h" (the reconstructed Enemy struct header). */

/* Initialized enemy data immediately preceding the AI jump tables. The
 * original broad lbl_8011BFF8 symbol incorrectly covered all these objects.
 * Preserve the five unused tables present in retail with the compiler's
 * ordinary retention pragma; otherwise mwld removes 192 bytes. This also
 * restores the natural eight-byte input-section alignment, without an ELF
 * alignment patch. Placeholder names remain until their PDB names are known. */
char* lbl_8011BFF8[3] = { "SHADOW1L1", "SHADOW2L1", "SHADOW3L1" };
/* force_active keeps these otherwise unreferenced tables in the image: without
 * it mwld dead-strips them and the DOL shrinks below the target size. It leaves
 * no trace in .text/.data/.symtab, only in .comment. */
#pragma force_active on
s32 lbl_8011C004[16] = { 16, 23, 14, 13, 7, 7, 7, 7, 2, 24, 20, 25, 30, 30, 7, 7 };
f32 lbl_8011C044[8] = { 0.0f, 0.392699093f, 0.785398185f, 1.17809725f, 1.57079637f, 1.96349537f, 2.3561945f, 2.7488935f };
f32 lbl_8011C064[8] = { 0.0f, 0.392699093f, 0.785398185f, 1.17809725f, 1.57079637f, 1.96349537f, 2.3561945f, 2.7488935f };
f32 lbl_8011C084[8] = { 0.0f, 0.392699093f, 0.785398185f, 1.17809725f, 1.57079637f, 1.96349537f, 2.3561945f, 2.7488935f };
f32 lbl_8011C0A4[8] = { 0.0f, 0.392699093f, 0.785398185f, 1.17809725f, 1.57079637f, 1.96349537f, 2.3561945f, 2.7488935f };
#pragma force_active reset
f32 lbl_8011C0C4[10] = { 0.0f, 0.392699093f, 0.392699093f, 0.392699093f, 0.392699093f, 0.392699093f, 0.392699093f, 0.392699093f, 0.392699093f, 0.392699093f };

/* forward decls for the cross-referenced enemy entry points */
s32 find_enemy_slot(s32 type, s32 level);
void kill_enemy(s32 index);
void uncouple_enemy(s32 index);
void do_enemy_move(int index);
int do_enemy_collide(int index, f32 retryThreshold);
f32 turn_enemy_ang(Enemy* e, f32 want);
s32 do_ai(s32 index);
void move_logic00(int index);
void move_logic01(s32 index); void move_logic02(int index); void move_logic03(s32 index);
void move_logic04(int index); void move_logic05(s32 index); void move_logic06(s32 index);
void move_logic07(s32 index); void move_logic08(s32 index); void move_logic10(s32 index);
void move_logic12(s32 index); void move_logic13(s32 index); void move_logic14(int index);
void move_logic15(int index); void move_logic16(s32 index); void move_logic18(s32 index);
void move_logic19(s32 index); void move_logic20(s32 index); void move_logic21(s32 index);
void move_logic22(s32 index); void move_logic23(s32 index); void move_logic24(s32 index);
void move_logic28(s32 index); void move_logic29(s32 index); void move_logic30(s32 index);
void move_logic31(s32 index);
extern void CreateYPRMatrix(f32* mat, f32* pyr);        /* pyr -> rotation matrix (fwd) */
extern void CopyMat3(f32* src, f32* dst);           /* 0x800BE8C8 (fwd) */
extern s32 gGameOptions[];   /* 0x80257590 (lbl_80257598 = [2]) */

/* branchless-abs idiom (srawi/xor/subf at -O4) */
#define ABS(x) (((x) ^ ((x) >> 31)) - ((x) >> 31))
#define ABS_REVERSED(x) ((((x) >> 31) ^ (x)) - ((x) >> 31))

/* --- same-TU statics not yet reconstructed (extern until written) --- */
extern void EnemyWorldDamage(Enemy* e, void* wobj, f32* oldpos, f32* hitnrm);
extern void fn_80046140(s32 index);                 /* generator-contact retreat */
extern int fn_8004646C(int index, f32* oldc, f32* newc, f32* newc2,
                       f32 rad, f32 hht, int* hitWorld);  /* enemy-vs-enemy probe */
extern int fn_80046680(int index, int b, f32* oldc, f32* newc, f32 rad,
                       f32 hht);                    /* generator-contact probe */
s32 fn_8004CFAC(f32* pos, f32* target);             /* turn direction (route) */
void fn_8004D030(s32 index, s32 ticks);             /* set dead_end/turn timer */
void fn_8004DB3C(Enemy* enemy, s32 delta);           /* fade enemy tree alpha */
void fn_8004E448(Enemy* enemy, s32 arg, f32* pos);   /* missile/audio dispatch */
void adjust_msidx(Enemy* enemy);                     /* update milestone history */
void enemy_update(void);                             /* update enemy texmods */
s32 check_vacancy(s32 index, f32* pos);             /* validate spawn position */
void fn_8004F1DC(Enemy* enemy);                     /* garm2 death-direction FX */

/* --- cross-module callees --- */
extern f32 fqdist(f32 x, f32 z);               /* 2D magnitude */
extern f32 NormalVector(f32* vector);
extern void fn_8005A65C(f32* worldmat, f32* coll_offset); /* refresh coll_pos */
extern s32 DeleteEffect(s32 idx, s32 mode);         /* sfx.c 0x80097790 */
extern void* EnemyWallCollide(f32 rad, f32* from, f32* to, f32* hitnrm); /* world probe */
extern s32 SlideAlongWall(f32 rad, f32* pos, f32* trans, f32* hitnrm, f32* out);
                                                    /* wall slide/deflect */
extern s32 check_enemy_pos(f32* start, f32* out, s32 slot);
extern s32 fn_8005D20C(s32 index, f32* oldc, f32* newc, s32 moved);
                                                    /* player collide + damage */
extern void CreateYPRMatrix(f32* mat, f32* pyr);        /* pyr -> rotation matrix */
extern void CopyMat3(f32* src, f32* dst);           /* 0x800BE8C8 */
extern void MBTreeSetFlags(struct mbnode* n, s32 a, s32 b); /* node show/update */
extern void MBTreeClearFlags(struct mbnode* n, s32 a, s32 b); /* node update */
extern s32 MBTreeGetAlpha(struct mbnode* n);
extern void MBTreeSetAlpha(struct mbnode* n, s32 alpha, s32 propagate);
extern void DoTexMods(void* data);
extern s32 EnemyStartMissile(Enemy* enemy, s32 arg, f32* pos, s32 kind);
extern void fn_8009DCE4(f32* pos);
extern void fn_8009DDFC(f32* pos);
extern void fn_8009DE2C(f32* pos);
extern void CreateDirMatrix(f32* matrix, f32* direction, f32* up);
extern void StartEnemyDeathFX(f32* matrix);

/* --- module data shared with other enemy helpers --- */
extern s32 gFrameTicks;      /* frame ticks (game speed units this frame) */
extern s32 gGameBusy;
extern s32 gGameplayPauseTimer;
extern f32 gClockFrameStep;   /* knockback integration scale */
extern f32 lbl_80344720;      /* current retreat/turn base angle */
extern void* lbl_80344730;    /* last worldobj hit by an enemy move */
extern s32 lbl_80344728;
extern s32 default_gen_count;
extern s32 lbl_8034471C;
extern Player gPlayers[4]; /* Four GC records, sizeof(Player) == 0x335C. */
extern f32 lbl_8023CA98[][4];
extern f32 lbl_8011BED8[];  /* 0x8011BED8 per-type turn-rate table */ /* wall-slide scratch; [1] = output vector */

/* gEnemies is a separate object at .bss +0xE18. MWCC can address several
 * globals through one section anchor; this is not evidence of an enclosing
 * C pool object. Legacy consumers below still spell that addressing manually.
 * Retire them against the complete native object: typed player/milestone views
 * are neutral in move_logic15/22, but their enemy-entry aliases still change
 * instruction counts or allocation (measured 2026-09-09). */
#define ENEMY_POOL_OFF 0xE18
#define OFF_E(field) (ENEMY_POOL_OFF + offsetof(Enemy, field))

/* TU .bss objects. The five module-local arrays below have Xbox LDATA32
 * evidence and no external GC consumers. MWCC allocates file-scope statics
 * at their definitions, in order: description buffer, loaded type indices,
 * type speeds, milestone route, wall-contact vector. These are separate
 * objects, not one BSS pool. The external globals still use the historical
 * referencer until their original declaration/initialization context is found. */
Enemy gEnemies[25];            /* 0x80251C18 */
u32 gWadAtreeHeaders[0x8B4 / 4];   /* 0x80251364 */
s32 lbl_802512B0[45];          /* 0x802512B0 per-type spawn-allowed */
s32 lbl_802511FC[45];          /* 0x802511FC per-type min-level class */
s32 lbl_80251148[45];          /* 0x80251148 per-type generator-fx enable */
/* PDB enemy_floor_col is a worldcol: matrix, squared distance and hit object.
 * GC passes this 72-byte record to FloorCollide and reads mtx[3][1] at +0x34. */
FloorCollisionResult enemy_floor_col; /* 0x80251100 */
static char mbdesc[32];              /* 0x80250E00 description scratch */
static s32 enemy_type[8];            /* 0x80250E20 loaded type indices */
static f32 lbl_80250E40[45];          /* 0x80250E40 per-type speed */
static s32 sEnemyMilestoneRoute[128]; /* 0x80250EF4 */
static f32 enemy_wall_collp[3];       /* 0x802510F4 world-probe contact */

/* Legacy accesses below still compute other objects from mbdesc's address.
 * They are unreconstructed cross-object arithmetic, not fields of this real
 * 32-byte buffer. Replace each with its actual owner and whole-TU validation;
 * the shared target BSS addressing base is not a source-level aggregate. */

/* Remaining external .bss first-use-order referencer. MWCC's global-object
 * placement is distinct from the declaration-order placement of statics above.
 * The original source responsible for this global ordering is UNRECOVERED.
 * Earlier discarded code is one hypothesis, not proof of this helper's
 * provenance. This historical unreferenced static is reconstruction debt: it
 * reproduces the order and is stripped by mwld. Its presence is not permission
 * to add similar referencers elsewhere.
 *
 * It must stay a LEAF, and it must not create a constant: a call here gives it
 * an unwind record the linked image has none for, so the object carries one
 * extab/extabindex record more than the target (measured 0x248/0x36C vs
 * 0x240/0x360, 73 vs 72 records); and an `f32` zero here enters the anonymous
 * constant pool at a new creation point, renumbering .sdata2 and moving every
 * @sda21 displacement in the linked image (measured 183 differing DOL bytes).
 * The existing integer store below references the floor record without doing
 * either. Four former stores were retired by recovering the statics above. */
static void enemy_bss_order(void)
{
    *(u32*)&enemy_floor_col = 0;
    lbl_80251148[0] = 0;
    lbl_802511FC[0] = 0;
    lbl_802512B0[0] = 0;
    gWadAtreeHeaders[0] = 0;
    gEnemies[0].type = E_SCORP;
}

/* ---------------------------------------------------------------------------
 * Declarations hoisted out of the old inter-function gaps so the function
 * DEFINITION ORDER below can follow the target's .text order (verified by
 * tools/gdl/textorder.py).  Relative order is unchanged, and none of these
 * emit storage, so the compiler sees the same declaration sequence as before.
 * --------------------------------------------------------------------------- */


/* Legacy consumers still address the separate attribute arrays through the
 * .data base at lbl_8011AF48. The definitions above and GameCube accesses prove
 * these offsets: ene_attn +2080, ene_coll +2216, base health +2760. Convert each
 * remaining consumer to its actual array while checking complete native output;
 * the unchanged byte-walk forms are reconstruction debt, not a recovered struct.
 */
#define ETYPE_ATTN_Y      2080 /* f32[34] attention-point height per type */
#define ETYPE_COLL_Y      2216 /* f32[34] collision-point height per type */
#define ETYPE_BASE_HEALTH 2760 /* f32[34] unscaled hit points per type    */

/* do_enemy_collide @0x80045488 - the enemy collision core.  Sweeps the pending
 * move (e->trans) against the world: probes walls (splitting the swept box for
 * the 0x1d flyer type), resolves wall hits by damage + slide-or-stop, tests
 * enemy-vs-enemy via fn_80045C30, snaps to the floor, reparents the mb-node to
 * whatever surface it landed on, and - when the residual slide is tiny - runs
 * the per-behaviour dead-end timers (turn/reverse).  Finally applies gravity
 * toward the floor target, dealing fall damage past the drop threshold.
 * Returns the collision class (0 none, 1 wall, 2 blocked). */
extern void* fn_80045C30(Enemy* e, f32 rad, f32 arg, f32* oldpos, f32* trans,
                         s32 collided);
extern void MBNodeSetParent(void* node, void* parent);
extern void* FloorCollide(f32* pos, s32 a, s32 b, s32 mode, f32 x, f32 y, f32 z);
extern s32 damage_enemy(Enemy* e, f32 amount, s32 dtype, s32 a, s32 b, s32 c,
                        s32 d);
extern s32 lbl_8034473C;
extern s32 AddExp(s32 player, s32 amount, s32 mode);
extern s32 damage_player(s32 player, f32 amount, s32 mode, u32 flags,
                         f32* direction);
extern s32 msgPost(s32 message, s32 player, void* position);
extern s32 StartDeathFX(void* node, s32 kind, s32 flags);
extern void AudioPlayEvt102Follow(f32* position, s32 player);
extern void AudioPlayEvt104(f32* position);
extern s32 SuicideExplosion(f32* position, f32 damage);
extern void fn_8009DAC8(f32* position);
extern s32 lbl_80344718;
extern s32 lbl_803447E4;
extern s32 lbl_80344B24;
extern f32 lbl_80344880;
extern level_data* gCurLevel;
extern void RequestEnemyAction(Enemy* enemy, s32 action);


/* ===================================================================== *
 *  AI MOVE-LOGIC STATE HANDLERS  (do_ai jumptable, 0x80046B54..0x8004C650)
 *  Each takes the enemy slot index, sets a desired facing/velocity, then
 *  calls do_enemy_move(index) to commit the move + resolve collisions.
 * ===================================================================== */

/* --- move_logic shared externs --- */
extern void RequestEnemyAction(Enemy* e, s32 action);
extern f32 get_yaw(f32* to, f32* from);       /* dir angle from->to */
extern void format_brain(s32 index);           /* AI-change transition hook */
extern void set_enemy_trans(Enemy* e, f32 spd, f32 ang); /* accel along angle */
extern f32 lbl_8011BF60[];    /* 0x8011BF60 imp retreat-speed ramp table */
extern s32 lbl_80344748;      /* 0x80344748 current "IT" enemy slot */
extern s32 RandInt(s32 n);
extern level_data* gCurLevel;         /* 0x8034483C active level record */
extern f32 sin(f32 x);
extern f32 cos(f32 x);
extern void fn_8009DDCC(f32* pos);   /* skeleton assemble fx */
extern void fn_8009DD9C(f32* pos);   /* skeleton attack fx */
extern void fn_8009DD6C(f32* pos);   /* dog pounce-ready fx */
s32 damage_enemy(Enemy* e, f32 amount, s32 dtype, s32 a, s32 b, s32 c, s32 d);
extern void fn_8009E03C(Enemy* e);   /* skeleton bone-toss fx */
extern s32 fn_8004C8CC(f32* pos, s32 index);   /* wall/object clearance probe */
extern s32 FastWallCollide(f32* from, f32* to, void* hit, s32 mode); /* ray wall probe */
extern s32 fn_8004CE38(Enemy* e);   /* 0x8004CE38 left/right look-ahead probe dir */
extern void fn_80051568(s32 index); /* 0x80051568 guard-target refresh */
struct Item;
extern struct Item* sItems;                  /* item array base (stride 0xF0) */
extern f32 lbl_8011C0C4[];    /* 0x8011C0C4 wander search-angle offsets */
extern f32 lbl_8011C0A4[];    /* 0x8011C0A4 corner search-angle offsets */
extern f32 lbl_8011C084[];    /* 0x8011C084 guard corner search-angle offsets */
extern f32 lbl_8011C044[];    /* 0x8011C044 flee corner search-angle offsets */
extern f64 __frsqrte(f64 x);
extern s32 ErrorPrintf(const char* fmt, ...);
extern s32 sFlags;            /* 0x803445CC packed config flags */
extern u64 gControllerButtons;      /* 0x803445C8 config-word pair (hi) + sFlags (lo) */
extern u8 sLookoutParams[];     /* 0x802584A8 prowl-node table (stride 0x6C) */
extern s32 sNumLookoutParams;      /* 0x80344900 prowl-node count */
extern u8 sMilestones[];     /* 0x8025B604 milestone-node table (stride 0x68) */
extern s32 sNumMilestones;      /* 0x8034491C milestone-node count */

/* World-node tables owned by items.c: LookoutParam is 0x6C bytes and
 * MilestoneParam is 0x68. Xbox MILESTONE contains one OBJGRP, corroborated by
 * GC's 104-byte milestone stride, matrix basis at 0x20/0x28, and position
 * at 0x30..0x38. Reuse the established object-group type rather than calling
 * its attention/collision vectors arbitrary positions and padding.
 * This TU-local view preserves the complete native object; items.c's older
 * flattened declaration and its traversal forms are a separate cleanup. */
#define LOOKOUT_POS_X    0x30
#define LOOKOUT_POS_Y    0x34
#define LOOKOUT_POS_Z    0x38
#define LOOKOUT_NEXT     0x68
#define MILESTONE_POS_X  0x30
#define MILESTONE_POS_Y  0x34
#define MILESTONE_POS_Z  0x38
typedef struct MilestoneParam {
    OBJGRP objgrp;
} MilestoneParam;
/* Item record (include/game/item.h, 0xF0): active @0xC4, minoff @0xCD. */
#define ITEM_ACTIVE      0xC4
#define ITEM_MINOFF      0xCD
extern void GetMilestonePos(s32 idx, f32* out);  /* 0x80066054 */
extern s32 fn_800511D0(s32 idx, f32 turn);        /* 0x800511D0 next-node picker */

/* move_logic05 @0x80047844 (state 5, one of the two "wander" fallbacks reached
 * from the recognized/closest gate).  IT-flee, drift a heading on a dead_end
 * timer (rotating -pi/2 and cycling a 4-count), then sweep two clearance probes:
 * a near ray (rad+0.5) via FastWallCollide and a far ray (speed) via the shared
 * object probe; a block on either rotates the heading and re-arms the timer. */
typedef struct EnemyMovePage05 {
    u8 _0000[64];
    f32 speed[45];
    u8 _00f4[3364];
    Enemy enemies[25];
} EnemyMovePage05;


/* move_logic10 @0x80048928 (state 10, lizardman "captain").
 * The reconstructed multi-mode pack hunter follows player milestones and
 * tests alternative headings when blocked. The target has 1085 instructions.
 *
 * Full-body call inventory (from tools/gdl/fnasm.py game/enemy/enemy move_logic10):
 *   get_yaw x18  - face/milestone bearings for each sub-mode
 *   sin/cos     x7   - heading projection for the wander + corner probes
 *   GetMilestonePos x6, find_neighbor_milestone x2, update_enemy_milestone x1
 *                     - milestone-network navigation (shares the move_logic22 stack)
 *   turn_enemy_ang x5, set_enemy_trans x5, do_enemy_move x5 - one move per sub-mode
 *   format_brain x5   - AI-change transition per sub-mode
 *   fqdist x4, fn_8004CE38 x3, fn_8004C8CC x3 - dist checks + corner probes
 *   do_ai x2         - the flee/chase bail-outs
 * Frame: 392 bytes, saves r25-r31 (_savefpr_25), pool base lbl_8011AF48 held in a
 * nonvolatile.  Uses the mbdesc + index*916 + 3608 anchor like the others.
 * The entry's pointer-copy shape remains unresolved; exact count alone is
 * not a complete register-renaming proof. */
extern f32 lbl_8011C064[];    /* 0x8011C064 pack-hunter turn-step ramp */
extern s32 find_neighbor_milestone(s32 ms, s32 nth); /* 0x8004C9DC */

/* --- kill_enemy externs --- */
extern int sprintf(char* buf, const char* fmt, ...);
extern int toupper(int c);
extern char* fn_80057ACC(s32 slot);                 /* current-level tag string */
struct Item;
extern struct Item* PlaceItem(s32 a, s32 b, char* name, void* c);
extern void StartBagFX(f32* pos, struct Item* ip, f32 z); /* toss carried item */
extern void AddItemSub(struct Item* ip);           /* commit placed item */
extern void del_target(f32* worldmat);               /* release camera target */
extern void MBRemoveNode(struct mbnode* n, s32 a);   /* delete scene node */
extern void SfxDeleteParented(struct mbnode* n, s32 a, s32 b);
extern void AtreeDelete(void* atree);               /* free anim tree */
extern s32 gTriggerCameraState;
extern s32 lbl_80344734;      /* node-delete reentry guard */
extern s32 ErrorPrintf(const char* fmt, ...);

extern s32 heal_player(Player* player, f32 amount);
extern void do_heal_players(void* player, f32* matrix, f32 amount);
extern void ModifyDamage(f32 armor, f32* damage, u32* damage_type, u32 shield);
extern void CopyMat4(f32* source, f32* destination);
extern void UpdateObjWorldMat(f32* matrix);
extern void fn_8005A404(f32* matrix, f32* coll_offset, f32* attn_offset);
extern void SetEnemyObj(Enemy* e, s32 type, s32 level);
extern void AudioPlayEvt101(f32* position);
extern void AudioPlayEvt103(f32* position);
extern void fn_8009DD48(void);
extern void fn_8009DE5C(s32 type, f32* position);
extern void fn_8009DE88(Enemy* enemy, s32 mode);
extern void fn_8009DF7C(Enemy* enemy, s32 mode);
extern void fn_800945D0(u8* position, u8* matrix, s32 damage_type, s32 alt,
                        u32 type, f32 scale);
extern void MBTreeSetAmbientAdd(struct mbnode* node, s32 value, s32 recurse);
extern void SetSkinFX(skinfx* fx, s32 base, s32 frames, s32 loops, f32 rate);
extern s32 gBossType;
extern s32 lbl_80344768;
extern f32 lbl_803447D8;
extern s32 lbl_80344BE0;
extern s32 lbl_80344BE4;
extern s32 lbl_802897B8[];
extern f32 lbl_8011B900[];
extern f32 lbl_8011BA10[];

/* kill_enemy @0x8004EFE4.  Drop the carried item (or place a "GARG<level>"
 * egg for gargoyles), then tear the enemy down: health/state clear, grid
 * release, shadow + special fx + scene node deletion, generator uncouple. */

extern s32 lbl_80344724;   /* 0x80344724 active milestone count */

/* --- generate_enemy externs --- */
extern s32 RandInt(s32 n);
extern s32 check_enemy_pos(f32* start, f32* out, s32 slot);
extern void init_enemy(s32 slot, f32* pos, s32 type, s32 level, s32 spew);
extern void UpdateObjWorldMat(f32* worldmat);              /* claim grid cell */
extern void fn_8005A404(f32* worldmat, f32* coll_offset, f32* attn_offset);
extern s32 InitAnim(f32 time, animinfo* info, s32 seq, s32 frame, s32 active);
extern void StartGenFX(f32* pos, s32 level);
extern s32 gBossType;
extern s32 gBossDying;
extern s32 gGameMode;      /* current game mode; see enum e_mode */
extern s32 lbl_803447DC;      /* generators-disabled flag */
extern s32 lbl_8034472C;      /* random-type rotation counter */
extern u32 jumptable_8011C25C[];

typedef struct EnemyGeneratorInfo {
    s32 type;
} EnemyGeneratorInfo;


typedef struct EnemyGenerator {
    EnemyGeneratorInfo* info;
    u8 _pad004[0xDA];
    s8 live_count;
    u8 _pad0DF[2];
    s8 first_enemy;
    u8 _pad0E2;
    s8 flag_e3;
} EnemyGenerator;


/* check_enemy_pos @0x8004F9AC -- validate a candidate spawn point for enemy
 * `slot`: optionally offset it, reject wall/floor failures and steep drops,
 * snap Y to the floor, then reject overlaps with world objects or other
 * enemies (unless the overlap is the enemy's own generator).  Returns 1 when
 * the position is usable, 0 when blocked by geometry/occupant, -1 on failure. */
extern void* FloorCollide(f32* pos, s32 a, s32 b, s32 mode, f32 x, f32 y, f32 z);
/* FloorCollisionResult is declared near do_enemy_collide above. */
extern FloorCollisionResult gFloorCollisionResult; /* 0x8023CAE0 */
extern void* fn_8005EFAC(f32 rad, f32* probe, f32* pos, s32 a, s32 b);
extern s32 fn_8005D3D8(s32 a, void* obj);

/* 0x8004C8CC - wall/object clearance probe shared by the move_logic set */
extern void* fn_8005EFAC(f32 rad, f32* probe, f32* pos, s32 a, s32 b);
extern s32 fn_8005D3D8(s32 a, void* obj);

/* 0x80045FE4 - enemy-vs-world-object damage (type from wobj flag nibble) */
extern u32 WorldObjGetAllFlags(void* wobj);
extern f32 NormalVector2D(f32* v);

/* 0x8004CE38 - pick turn direction: which of +/-step headings nears the player */

/* do_enemies @0x8004D078 -- per-frame master enemy loop.  Runs the critter
 * list, then (when not paused) sweeps every enemy: a scripted-camera fast path,
 * otherwise a player-aggro reset, an aggro-decay ramp, a milestone/aim probe,
 * a two-radius visibility cull, and the main per-enemy state machine (alive /
 * stunned / dying) with knockback damping and boss fade-out.  Draws the live
 * enemy count when the debug flags are set. */
extern void ProcessCritterList(void);
extern s32 gScriptedCameraState;
s32 fn_8004D958(s32 index);
extern MBTextMsg* DrawTextKeepScale(f32 scale, s32 x, s32 y, s32 font, s32 color, char* txt);
extern s32 MBWorldSphereVisible3(f32* center, f32 radius);
extern void fn_800516F8(s32 index);
extern void fn_8009FEFC(s16 sound);
extern void fn_8009FEA0(s16 sound);
extern void AudioPlayEvt102(void);
extern void ProcessSkinFX(f32* a, void* node, s32 c);
extern void fn_8005A338(f32* mat, f32* colloff, f32* attnoff);
extern void fn_8004DF58(Enemy* e);
extern f32 atan2(f32 y, f32 x);
extern void SetSkinFX(skinfx* fx, s32 base, s32 frames, s32 loops, f32 rate);
extern s32 gBoss398;
extern char gTextFormatBuf[];
extern s32 lbl_803447B8;
extern s32 lbl_80344718;
extern s32 lbl_80344740;
extern f32 lbl_803447D8;
extern s32 lbl_803447E4;
extern f32 lbl_8011B878[];        /* per-type aggro-decay ramp (0x88) */
extern s32 lbl_80344BF8;
extern s32 heal_player(Player* player, f32 amount);
extern void StartGemFX(f32* position, s32 kind);
extern void fn_8009E08C(Enemy* enemy);
extern void AudioPlayerHit(s32 player, s32 kind);

/* 0x8004D958 - per-enemy frame update: lifetime, owner change, boss-death
 * cull, AI step + type-24 hover bob timer */
extern s32 gBossDying;
extern void fn_800945D0(u8* pos, u8* a, s32 b, s32 c, u32 type, f32 scale);

extern f32 fn_80034C88(f32 x);
extern s32 LineCylinderCollide(f32* center, f32 radius, f32 halfHeight,
                               f32* from, f32* to, f32* hit, s32 directional);

/* fn_8004646C @0x8004646C -- sweep the enemy's move (oldc->newc) against the
 * critter-node mesh and the neighbouring enemies pulled from the item grid,
 * returning the id of the closest blocking enemy (or a node hit tagged with
 * 0x10000), skipping self, dead/idle occupants, already-linked pack members,
 * and short obstacles when charging.  -1 = clear. */
extern void CritterCollideStart(f32 rad, f32* pos, s32 a);
extern void* CritterMoveNodeCol(f32 rad, f32 zero, f32* from, f32* to,
                                void* hit, s32 a, s32 b);
extern s32 NextGridItem(void);

extern f32 lbl_80344880;
extern f32 FloorPos(f32 fallback, f32 radius, f32* position, s32 mode);
extern void init_enemy_vars(int slot, f32 scale, int spew);
extern void fn_8005A338(f32* worldmat, f32* coll_offset, f32* attn_offset);
extern u16 AnimateATree(void* tree, s32 sequence, s32 transition);


/* Forward declarations for the whole TU.  The definition order below is
 * the target's .text order, so a callee is routinely defined after its
 * caller; these prototypes are what let that order compile. */
static void enemy_bss_order(void);
f32 closest_enemy(f32 width, f32 range, f32* position, f32* direction, f32* offset, s32* enemy_index, s32 flags);
void do_enemy_move(int index);
int do_enemy_collide(int index, f32 retryThreshold);
static s32 EnemyMovingAwayFromBirth(Enemy* enemy, f32* oldPosition, f32* translation);
void* fn_80045C30(Enemy* enemy, f32 radius, f32 retryThreshold, f32* oldPosition, f32* translation, s32 collisionClass);
void EnemyWorldDamage(Enemy* e, void* wobj, f32* oldpos, f32* hitnrm);
void fn_80046140(s32 index);
int fn_8004646C(int index, f32* oldc, f32* newc, f32* newc2, f32 rad, f32 hht, int* hitWorld);
int fn_80046680(int index, int b, f32* oldc, f32* newc, f32 rad, f32 hht);
s32 do_ai(s32 index);
static f32 fabsf_(f32 x);
void move_logic00(int index);
void move_logic01(s32 index);
void move_logic02(int index);
void move_logic03(s32 index);
void move_logic04(int index);
void move_logic05(s32 index);
void move_logic06(s32 index);
void move_logic07(s32 index);
void move_logic08(s32 index);
void move_logic10(s32 index);
void move_logic12(s32 index);
void move_logic13(s32 index);
void move_logic14(int index);
void move_logic15(int index);
void move_logic16(s32 index);
void move_logic18(s32 index);
void move_logic19(s32 index);
void move_logic20(s32 index);
void move_logic21(s32 index);
void move_logic22(s32 index);
void move_logic23(s32 index);
void move_logic24(s32 index);
void move_logic28(s32 index);
void move_logic29(s32 index);
void move_logic30(s32 index);
static inline void update_vel(Enemy* e, f32 k);
void move_logic31(s32 index);
s32 fn_8004C8CC(f32* pos, s32 index);
s32 find_neighbor_milestone(s32 ms, s32 nth);
f32 turn_enemy_ang(Enemy* e, f32 want);
void set_enemy_trans(Enemy* enemy, f32 speed, f32 angle);
s32 fn_8004CE38(Enemy* e);
s32 fn_8004CFAC(f32* pos, f32* target);
void fn_8004D030(s32 index, s32 ticks);
void do_enemies(void);
s32 fn_8004D958(s32 index);
void fn_8004DB3C(Enemy* enemy, s32 delta);
void fn_8004DC2C(Enemy* enemy);
void fn_8004DF58(Enemy* enemy);
void fn_8004E448(Enemy* enemy, s32 arg, f32* pos);
void update_enemy_milestone(Enemy* enemy);
void adjust_msidx(Enemy* enemy);
void enemy_update(void);
s32 damage_enemy(Enemy* e, f32 amount, s32 player_index, s32 damage_type, s32 effect_position_arg, s32 hit_direction_arg, s32 play_effects);
void kill_enemy(s32 index);
void fn_8004F1DC(Enemy* enemy);
void uncouple_enemy(s32 index);
s32 check_vacancy(s32 index, f32* pos);
s32 generate_enemy(f32* pos, s32 type, s32 level, f32* dir, s32 spew, struct Item* gen, s32 imp, f32 ang);
s32 fn_8004F87C(s32 type, s32 level, s32 spew);
s32 check_enemy_pos(f32* start, f32* out, s32 slot);
static f32 gendir_8004FBC8(f32* input, f32* output, s32 direction);
s32 find_enemy_slot(s32 type, s32 level);
void init_enemy(s32 slot, f32* pos, s32 type, s32 level, s32 spew);

f32 closest_enemy(f32 width, f32 range, f32* position, f32* direction,
                  f32* offset, s32* enemy_index, s32 flags)
{
    s32 item;
    s32 best_index;
    u8 unused_before[4];
    f32 delta[3];
    u8 unused_after[4];
    f32 best_x;
    f32 best_y;
    f32 best_z;
    f32 best_distance;
    f32 spread;
    f64 maximum_vertical;

    best_index = -1;
    best_distance = range;
    spread = (1.0 - width) / range;
    StartItemGrid(position, range);
    maximum_vertical = 10.0;
    while ((item = NextGridItem()) >= 0) {
        Enemy* enemy = &gEnemies[item];

        if (enemy->state == ACTIVE || enemy->state == SLEEP) {
            if (enemy->type != E_IT &&
                (enemy->type != E_DEATH || (flags & 0x80000) != 0)) {
                f32 vertical;
                f32 distance;

                delta[0] = enemy->objgrp.coll_pos[0] - position[0];
                delta[1] = enemy->objgrp.coll_pos[1] - position[1];
                delta[2] = enemy->objgrp.coll_pos[2] - position[2];
                vertical = delta[1];
                *(u32*)&vertical &= 0x7FFFFFFF;
                if ((f64)vertical > maximum_vertical) {
                    goto next_enemy;
                }
                distance = NormalVector(delta) - enemy->rad;
                if (distance > range) {
                    goto next_enemy;
                }
                {
                    f32 horizontal = fqdist(delta[0], delta[2]);
                    f32 cone = horizontal * (distance * spread + width);
                    f32 dot = delta[0] * direction[0] +
                              delta[2] * direction[2];

                    if (dot < cone) {
                        goto next_enemy;
                    }
                    if (distance < best_distance) {
                        best_distance = distance;
                        best_x = delta[0];
                        best_y = delta[1];
                        best_z = delta[2];
                        best_index = item;
                    }
                }
            }
        }
next_enemy:
        ;
    }

    if (best_index >= 0) {
        offset[0] = best_x;
        offset[1] = best_y;
        offset[2] = best_z;
    }
    if (enemy_index != 0) {
        *enemy_index = best_index;
    }
    return best_distance;
}

void do_enemy_move(int index)
{
    Enemy* e;
    int alg;
    f32 rad;
    f32 hht;
    int blocked;
    int collide;
    f32 moveDistance;
    int result;
    int n;
    Enemy* other;
    f32 mat[16];
    u8 matrixGap[8]; /* Unrecovered local space above the movement vectors. */
    f32 oldpos[3];
    f32 rad2;
    u8 unused1[4];
    f32 oldc[3];
    u8 unused2[4];
    f32 newc[3];
    int hitWorld;
    u8 unused3[4];
    f32 half[3];
    u8 unused4[12];

    e = (Enemy*)((u8*)mbdesc + index * sizeof(Enemy));
    e = (Enemy*)((u8*)e + ENEMY_POOL_OFF);
    alg = e->algorithm;
    rad = e->rad;
    hht = e->hht;
    blocked = 0;

    /* stun freeze + knockback integration */
    if (e->stun_timer > 0) {
        e->stun_timer -= gFrameTicks;
        e->trans[0] = 0.0f;
        e->trans[1] = 0.0f;
        e->trans[2] = 0.0f;
    }
    if (e->action >= 28) {
        e->trans[0] = 0.0f;
        e->trans[1] = 0.0f;
        e->trans[2] = 0.0f;
    }
    e->trans[0] += e->pushed[0] * gClockFrameStep;
    e->trans[1] += e->pushed[1] * gClockFrameStep;
    e->trans[2] += e->pushed[2] * gClockFrameStep;
    moveDistance = fqdist(e->trans[0], e->trans[2]);
    if (moveDistance > 0.001) {
        e->moved = 1;
    } else {
        e->moved = 0;
    }

    collide = do_enemy_collide(index, moveDistance);

    /* commit the move to the world matrix + collision point */
    oldpos[0] = e->objgrp.worldmat[3][0];
    oldpos[1] = e->objgrp.worldmat[3][1];
    oldpos[2] = e->objgrp.worldmat[3][2];
    e->objgrp.worldmat[3][0] += e->trans[0];
    e->objgrp.worldmat[3][1] += e->trans[1];
    e->objgrp.worldmat[3][2] += e->trans[2];
    oldc[0] = e->objgrp.coll_pos[0];
    oldc[1] = e->objgrp.coll_pos[1];
    oldc[2] = e->objgrp.coll_pos[2];
    newc[0] = oldc[0] + e->trans[0];
    newc[1] = oldc[1] + e->trans[1];
    newc[2] = oldc[2] + e->trans[2];

    /* generator contact: full revert + retreat toward the generator */
    if (e->visactive != 0) {
        e->coll_pnum = fn_80046680(index, 0, oldc, newc, (f32)(0.5 + rad), hht);
    } else {
        e->coll_pnum = -1;
    }
    if (e->coll_pnum >= 0) {
        e->coll_enenum = -1;
        e->coll_ip = 0;
        e->moved = 0;
        e->objgrp.worldmat[3][0] = oldpos[0];
        e->objgrp.worldmat[3][1] = oldpos[1];
        e->objgrp.worldmat[3][2] = oldpos[2];
        e->trans[0] = 0.0f;
        e->trans[1] = 0.0f;
        e->trans[2] = 0.0f;
        fn_8005A65C(&e->objgrp.worldmat[0][0], e->coll_offset);
        e->route = fn_8004CFAC(&e->objgrp.worldmat[3][0],
                               gPlayers[e->coll_pnum].pos);
        fn_80046140(index);
    } else {
        hitWorld = 0;
        if (e->type == E_DEATH && e->specialfx >= 0) {
            e->specialfx = DeleteEffect(e->specialfx, 0);
        }
        if (e->attack_timer > 0) {
            if ((e->attack_timer -= gFrameTicks) <= 0) {
                e->attack_timer = 0;
            }
        }
        if (collide == 0) {
            e->coll_enenum = fn_8004646C(index, oldc, newc, newc, rad, hht, &hitWorld);
        } else {
            e->coll_enenum = fn_8004646C(index, oldc, newc, newc, rad, hht, 0);
        }
        if (e->coll_enenum >= 0) {
            /* hit another enemy */
            e->coll_ip = 0;
            other = 0;
            n = e->coll_enenum;
            if (n < 0x10000) {
                other = &gEnemies[n];
                other->coll_enenum = index;
            }
            if (hitWorld != 0) {
                /* the probe clipped the move against the world: retry the
                 * clipped translation against world objects */
                e->trans[0] = newc[0] - e->objgrp.coll_pos[0];
                e->trans[1] = newc[1] - e->objgrp.coll_pos[1];
                e->trans[2] = newc[2] - e->objgrp.coll_pos[2];
                rad2 = rad;
                rad2 *= 1.5;
                half[0] = oldpos[0] + e->trans[0];
                half[1] = oldpos[1] + e->trans[1];
                half[2] = oldpos[2] + e->trans[2];
                lbl_80344730 = EnemyWallCollide(rad2, oldpos, half, enemy_wall_collp);
                if (lbl_80344730 != 0) {
                    EnemyWorldDamage(e, lbl_80344730, oldpos, enemy_wall_collp);
                    if (*(u32*)((u8*)lbl_80344730 + 16) & 0x38) {
                        result = 0;
                    } else if (!(e->ai_flags & 1)
                               && SlideAlongWall(rad2, oldpos, e->trans,
                                              enemy_wall_collp, lbl_8023CA98[1]) < 0) {
                        result = 2;
                        e->trans[2] = 0.0f;
                        e->trans[0] = 0.0f;
                    } else {
                        result = 1;
                    }
                } else {
                    result = 0;
                }
                if (result != 0) {
                    hitWorld = 0;
                } else {
                    /* free half-step along the clipped translation */
                    e->objgrp.worldmat[3][0] = oldpos[0] + 0.5 * e->trans[0];
                    e->objgrp.worldmat[3][1] = oldpos[1] + 0.5 * e->trans[1];
                    e->objgrp.worldmat[3][2] = oldpos[2] + 0.5 * e->trans[2];
                }
            }
            if (hitWorld == 0) {
                if (other != 0 && e->pushmag2 > 1.0 && e->action >= 28) {
                    /* being knocked back: transfer half the push */
                    other->pushed[0] = 0.5 * e->pushed[0] + other->pushed[0];
                    other->pushed[1] = 0.5 * e->pushed[1] + other->pushed[1];
                    other->pushed[2] = 0.5 * e->pushed[2] + other->pushed[2];
                    other->trans[0] = 0.5 * e->trans[0];
                    other->trans[1] = 0.5 * e->trans[1];
                    other->trans[2] = 0.5 * e->trans[2];
                } else {
                    /* blocked: full revert */
                    e->moved = 0;
                    blocked = 1;
                    e->objgrp.worldmat[3][0] = oldpos[0];
                    e->objgrp.worldmat[3][1] = oldpos[1];
                    e->objgrp.worldmat[3][2] = oldpos[2];
                    e->trans[0] = 0.0f;
                    e->trans[1] = 0.0f;
                    e->trans[2] = 0.0f;
                }
            }
            fn_8005A65C(&e->objgrp.worldmat[0][0], e->coll_offset);
            if (other != 0 && alg == 0) {
                e->route = fn_8004CFAC(&e->objgrp.worldmat[3][0],
                                       &other->objgrp.worldmat[3][0]);
                if (e->dead_end <= 0) {
                    e->dead_end = 60;
                    if (e->daction == 3 || e->daction == 4) {
                        e->daction = 0;
                    }
                }
            } else if (other != 0
                       && (alg == 7 || alg == 8 || alg == 10 || alg == 20)) {
                if (e->route == 0 || ABS_REVERSED(e->route) > 2) {
                    e->route = fn_8004CFAC(&e->objgrp.worldmat[3][0],
                                           &other->objgrp.worldmat[3][0]);
                    e->collided = 0;
                }
                if (alg == 7) {
                    if (ABS_REVERSED(e->route) <= 2) {
                        e->collided++;
                        fn_8004D030(index, 15);
                    } else {
                        fn_8004D030(index, 50);
                        e->ang = lbl_80344720;
                        e->pyr[1] = lbl_80344720;
                        e->collided = 0;
                        e->route = 0;
                    }
                    if (e->collided >= 7) {
                        e->route = -e->route * 2;
                        e->collided = 0;
                    }
                } else if (alg == 8) {
                    if (ABS_REVERSED(e->route) <= 2) {
                        e->collided++;
                        fn_8004D030(index, 10);
                    } else {
                        fn_8004D030(index, 60);
                        e->ang = lbl_80344720;
                        e->pyr[1] = lbl_80344720;
                        e->collided = 0;
                        e->route = 0;
                    }
                    if (e->collided >= 7) {
                        e->route = -e->route * 2;
                        e->collided = 0;
                    }
                } else if (alg == 10) {
                    if (ABS_REVERSED(e->route) <= 2) {
                        e->collided++;
                        fn_8004D030(index, 15);
                    } else {
                        fn_8004D030(index, 50);
                        e->ang = lbl_80344720;
                        e->pyr[1] = lbl_80344720;
                        e->collided = 0;
                        e->route = 0;
                    }
                    if (e->collided >= 7) {
                        e->route = -e->route * 2;
                        e->collided = 0;
                    }
                } else if (alg == 20) {
                    if (ABS_REVERSED(e->route) <= 2) {
                        e->collided++;
                        fn_8004D030(index, 10);
                    } else {
                        fn_8004D030(index, 30);
                        {
                            f64 a;
                            f32 step = lbl_80344720;
                            e->ang = (f32)(3.141592654 + step);
                            if ((a = e->ang) > 3.141592654) {
                                a -= 6.283185308;
                            } else if (a <= (-3.141592654)) {
                                a = 6.283185308 + a;
                            }
                            e->ang = a;
                            e->pyr[1] = a;
                        }
                        e->collided = 0;
                        e->route = 0;
                    }
                    if (e->collided >= 7) {
                        e->route = -e->route * 2;
                        e->collided = 0;
                    }
                }
            } else {
                if (e->dead_end <= 0) {
                    e->dead_end = 20;
                }
            }
            e->area = 2;
        }
        if (blocked == 0
            && fn_8005D20C(index, oldc, newc, e->moved) != 0) {
            /* hit a player: full revert + per-algorithm turn logic */
            e->moved = 0;
            blocked = 1;
            e->objgrp.worldmat[3][0] = oldpos[0];
            e->objgrp.worldmat[3][1] = oldpos[1];
            e->objgrp.worldmat[3][2] = oldpos[2];
            e->trans[0] = 0.0f;
            e->trans[1] = 0.0f;
            e->trans[2] = 0.0f;
            fn_8005A65C(&e->objgrp.worldmat[0][0], e->coll_offset);
            if (alg == 0) {
                if (e->coll_ip->objgrp.node != 0) {
                    e->route = fn_8004CFAC(&e->objgrp.worldmat[3][0],
                                           e->coll_ip->objgrp.worldmat[3]);
                }
                if (e->dead_end <= 0) {
                    e->dead_end = 60;
                    if (e->daction == 3 || e->daction == 4) {
                        e->daction = 0;
                    }
                }
            } else if (alg == 7 || alg == 8 || alg == 10 || alg == 20) {
                const Enemy* contactOwner = e;
                if (e->coll_ip->objgrp.node != 0) {
                    if (e->route == 0 || ABS_REVERSED(e->route) > 2) {
                        e->route = fn_8004CFAC(&e->objgrp.worldmat[3][0],
                                               contactOwner->coll_ip->objgrp.worldmat[3]);
                        e->collided = 0;
                    }
                    if (alg == 7) {
                        if (ABS_REVERSED(e->route) <= 2) {
                            e->collided++;
                            fn_8004D030(index, 15);
                        } else {
                            fn_8004D030(index, 15);
                            e->ang = lbl_80344720;
                            e->pyr[1] = lbl_80344720;
                            e->collided = 0;
                            e->route = 0;
                        }
                        if (e->collided >= 7) {
                            e->route = -e->route * 2;
                            e->collided = 0;
                        }
                    } else if (alg == 8) {
                        if (ABS_REVERSED(e->route) <= 2) {
                            e->collided++;
                            fn_8004D030(index, 15);
                        } else {
                            fn_8004D030(index, 15);
                            e->ang = lbl_80344720;
                            e->pyr[1] = lbl_80344720;
                            e->collided = 0;
                            e->route = 0;
                        }
                        if (e->collided >= 7) {
                            e->route = -e->route * 2;
                            e->collided = 0;
                        }
                    } else if (alg == 10) {
                        if (ABS_REVERSED(e->route) <= 2) {
                            e->collided++;
                            fn_8004D030(index, 15);
                        } else {
                            fn_8004D030(index, 15);
                            e->ang = lbl_80344720;
                            e->pyr[1] = lbl_80344720;
                            e->collided = 0;
                            e->route = 0;
                        }
                        if (e->collided >= 7) {
                            e->route = -e->route * 2;
                            e->collided = 0;
                        }
                    } else if (alg == 20) {
                        if (ABS_REVERSED(e->route) <= 2) {
                            e->collided++;
                            fn_8004D030(index, 15);
                        } else {
                            fn_8004D030(index, 15);
                            {
                                f64 a;
                                f32 step = lbl_80344720;
                                e->ang = (f32)(3.141592654 + step);
                                if ((a = e->ang) > 3.141592654) {
                                    a -= 6.283185308;
                                } else if (a <= (-3.141592654)) {
                                    a = 6.283185308 + a;
                                }
                                e->ang = a;
                                e->pyr[1] = a;
                            }
                            e->collided = 0;
                            e->route = 0;
                        }
                        if (e->collided >= 7) {
                            e->route = -e->route * 2;
                            e->collided = 0;
                        }
                    }
                } else {
                    if (e->dead_end <= 0) {
                        e->dead_end = 20;
                    }
                }
            } else {
                if (e->dead_end <= 0) {
                    e->dead_end = 20;
                }
            }
            e->area = 3;
        }
        if (blocked == 0) {
            if (alg == 0 && e->dead_end <= 0) {
                e->route = 1;
                e->collided = 0;
            } else if (alg == 2 || alg == 4) {
                if (--e->play <= 0) {
                    e->count = 0;
                }
            }
        }
    }

    /* rebuild the object matrix + service the shadow node */
    if (e->state != 0) {
        if (e->pushmag2 > 0.1) {
            e->pyr[1] = e->pushang;
        }
        CreateYPRMatrix(mat, e->pyr);
        CopyMat3(mat, &e->objgrp.worldmat[0][0]);
        if (e->shadow != 0) {
            e->shadow->mat[3][0] = e->objgrp.worldmat[3][0];
            e->shadow->mat[3][1] = e->objgrp.worldmat[3][1];
            e->shadow->mat[3][2] = e->objgrp.worldmat[3][2];
            if (e->action == 1) {
                MBTreeSetFlags(e->shadow, 2, 0);
            } else {
                MBTreeClearFlags(e->shadow, 2, 0);
            }
        }
    }

    /* stuck-walk watchdog */
    if (e->moved != 0
        || (e->action != 3 && e->action != 4 && e->action != 0)) {
        e->stopped = 0;
    } else {
        e->stopped += gFrameTicks;
    }
    if (e->stopped > 180) {
        e->stopped = 0;
    }
    if (e->stopped > 60 && (e->daction == 3 || e->daction == 4)
        && alg != 18 && e->type != E_GOLEM) {
        e->daction = 0;
    }
}

int do_enemy_collide(int index, f32 retryThreshold)
{
    u8* pool = (u8*)mbdesc;
    u8* e0;
    u8* e;
    Enemy* enemy;
    s32 type;
    f32* tr;
    f32 rad;
    f32 dt;
    s32 behavior;
    s32 result = 0;
    f32 slideRad;
    WorldObj* hit = NULL;
    u8 framePad[4];
    f32 oldpos[3];
    f32 dh;
    u8 unused[4];

    (void)framePad;
    (void)unused;

    e0 = pool + index * 916;
    type = *(s32*)(e0 += ENEMY_POOL_OFF);
    e = e0;
    enemy = (Enemy*)e0;
    tr = enemy->trans;
    dt = (f32)((-16.0) * gClockFrameStep);
    behavior = enemy->algorithm;

    if (type == 0x1F || enemy->dead_end <= 0) {
        enemy->area = 0;
    }
    if (enemy->moved == 0) {
        WorldObj* mp = enemy->floor_wobj;
        if (mp != NULL && !(mp->flags & 0x1000)) {
            goto gravity;
        }
    }

    rad = enemy->rad;
    oldpos[0] = enemy->objgrp.coll_pos[0];
    oldpos[1] = enemy->objgrp.coll_pos[1];
    oldpos[2] = enemy->objgrp.coll_pos[2];
    oldpos[1] += 2.0 - enemy->flooroffset;

    if (enemy->moved != 0) {
        if (enemy->type == 0x1D) {
            f32 np[3];
            u8 npPad[4];
            s32 wallResult;

            (void)npPad;
            slideRad = (f32)(0.5 * rad);
            slideRad *= 1.5;
            np[0] = oldpos[0] + tr[0];
            np[1] = oldpos[1] + tr[1];
            np[2] = oldpos[2] + tr[2];
            lbl_80344730 = EnemyWallCollide(slideRad, oldpos, np,
                                                 (f32*)(pool + 0x2F4));
            if (lbl_80344730 != 0) {
                EnemyWorldDamage(enemy, lbl_80344730, oldpos,
                                 (f32*)(pool + 0x2F4));
                if (*(u32*)((u8*)lbl_80344730 + 0x10) & 0x38) {
                    wallResult = 0;
                } else {
                    if (!(*(u32*)(e + offsetof(Enemy, ai_flags)) & 1) &&
                        SlideAlongWall(slideRad, oldpos, tr,
                                       (f32*)(pool + 0x2F4),
                                       lbl_8023CA98[1]) < 0) {
                        tr[2] = 0.0f;
                        tr[0] = 0.0f;
                        wallResult = 2;
                    } else {
                        wallResult = 1;
                    }
                }
            } else {
                wallResult = 0;
            }
            result = wallResult;
        } else {
            f32 np[3];
            u8 npPad[4];
            s32 wallResult;

            (void)npPad;
            slideRad = rad;
            slideRad *= 1.5;
            np[0] = oldpos[0] + tr[0];
            np[1] = oldpos[1] + tr[1];
            np[2] = oldpos[2] + tr[2];
            lbl_80344730 = EnemyWallCollide(slideRad, oldpos, np,
                                                 (f32*)(pool + 0x2F4));
            if (lbl_80344730 != 0) {
                EnemyWorldDamage(enemy, lbl_80344730, oldpos,
                                 (f32*)(pool + 0x2F4));
                if (*(u32*)((u8*)lbl_80344730 + 0x10) & 0x38) {
                    wallResult = 0;
                } else {
                    if (!(*(u32*)(e + offsetof(Enemy, ai_flags)) & 1) &&
                        SlideAlongWall(slideRad, oldpos, tr,
                                       (f32*)(pool + 0x2F4),
                                       lbl_8023CA98[1]) < 0) {
                        tr[2] = 0.0f;
                        tr[0] = 0.0f;
                        wallResult = 2;
                    } else {
                        wallResult = 1;
                    }
                }
            } else {
                wallResult = 0;
            }
            result = wallResult;
        }
    }

    hit = fn_80045C30(enemy, rad, retryThreshold, oldpos, tr, result);
    if (hit != NULL) {
        enemy->floor_wobj = hit;
        if ((f64)enemy->hht <= 2.0) {
            if (hit->flags & 0x38) {
                result = 2;
                tr[2] = 0.0f;
                tr[0] = 0.0f;
            }
        }
    } else {
        WorldObj* mp = enemy->floor_wobj;
        if (mp != NULL && !(mp->flags & 0x1000)) {
            goto reparent;
        }
        tr[2] = 0.0f;
        tr[0] = 0.0f;
        hit = FloorCollide(oldpos, (s32)&enemy_floor_col, 0, 2,
                           (f32)(0.5 * rad), enemy->hht,
                           (f32)(-enemy->hht - 5.0));
        if (hit != NULL) {
            enemy->floory = enemy_floor_col.mtx[3][1] +
                            enemy->flooroffset;
            if (enemy->shadow != NULL) {
                CopyMat3(&enemy_floor_col.mtx[0][0], (f32*)enemy->shadow);
            }
        }
    }

reparent:
    if (hit != NULL) {
        void* parent;
        if ((parent = hit->nodeptr) != NULL && (hit->flags & 0x1000)) {
            MBNodeSetParent(enemy->objgrp.node, parent);
        } else {
            MBNodeSetParent(enemy->objgrp.node, (void*)lbl_8034473C);
        }
    }
    enemy->floor_surf = (hit != NULL) ? hit->flags : 0;

    if (!((f64)fqdist(tr[0], tr[2]) < 0.01)) {
        goto gravity;
    }

    if (behavior == 0) {
        if (ABS_REVERSED(*(s32*)(e + offsetof(Enemy, route))) <= 2) {
            (*(s16*)(e + offsetof(Enemy, collided)))++;
            fn_8004D030(index, 5);
        } else {
            (*(s16*)(e + offsetof(Enemy, collided)))++;
            fn_8004D030(index, 0x3C);
        }
        if (*(s16*)(e + offsetof(Enemy, collided)) >= 9) {
            *(s32*)(e + offsetof(Enemy, route)) = -*(s32*)(e + offsetof(Enemy, route)) * 2;
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
            if (ABS_REVERSED(*(s32*)(e + offsetof(Enemy, route))) > 2) {
                *(f32*)(e + offsetof(Enemy, ang)) = lbl_80344720;
                *(f32*)(e + offsetof(Enemy, pyr[1])) = lbl_80344720;
            }
        }
    } else if (behavior == 7) {
        if (ABS_REVERSED(*(s32*)(e + offsetof(Enemy, route))) <= 2) {
            (*(s16*)(e + offsetof(Enemy, collided)))++;
            fn_8004D030(index, 0xA);
        } else {
            fn_8004D030(index, 0x3C);
            *(f32*)(e + offsetof(Enemy, ang)) = lbl_80344720;
            *(f32*)(e + offsetof(Enemy, pyr[1])) = lbl_80344720;
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
            *(s32*)(e + offsetof(Enemy, route)) = 0;
        }
        if (*(s16*)(e + offsetof(Enemy, collided)) >= 7) {
            *(s32*)(e + offsetof(Enemy, route)) = -*(s32*)(e + offsetof(Enemy, route)) * 2;
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
        }
    } else if (behavior == 8) {
        if (ABS_REVERSED(*(s32*)(e + offsetof(Enemy, route))) <= 2) {
            (*(s16*)(e + offsetof(Enemy, collided)))++;
            fn_8004D030(index, 5);
        } else {
            fn_8004D030(index, 0x3C);
            *(f32*)(e + offsetof(Enemy, ang)) = lbl_80344720;
            *(f32*)(e + offsetof(Enemy, pyr[1])) = lbl_80344720;
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
            *(s32*)(e + offsetof(Enemy, route)) = 0;
        }
        if (*(s16*)(e + offsetof(Enemy, collided)) >= 7) {
            *(s32*)(e + offsetof(Enemy, route)) = -*(s32*)(e + offsetof(Enemy, route)) * 2;
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
        }
    } else if (behavior == 0xA) {
        if (ABS_REVERSED(*(s32*)(e + offsetof(Enemy, route))) <= 2) {
            (*(s16*)(e + offsetof(Enemy, collided)))++;
            fn_8004D030(index, 0xA);
        } else {
            fn_8004D030(index, 0x3C);
            *(f32*)(e + offsetof(Enemy, ang)) = lbl_80344720;
            *(f32*)(e + offsetof(Enemy, pyr[1])) = lbl_80344720;
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
            *(s32*)(e + offsetof(Enemy, route)) = 0;
        }
        if (*(s16*)(e + offsetof(Enemy, collided)) >= 7) {
            *(s32*)(e + offsetof(Enemy, route)) = -*(s32*)(e + offsetof(Enemy, route)) * 2;
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
        }
    } else if (behavior == 0x14) {
        if (ABS_REVERSED(*(s32*)(e + offsetof(Enemy, route))) <= 2) {
            (*(s16*)(e + offsetof(Enemy, collided)))++;
            fn_8004D030(index, 3);
        } else {

            fn_8004D030(index, 0x1E);
            *(f32*)(e + offsetof(Enemy, ang)) = (f32)(3.141592654 + lbl_80344720);
            {
                f64 a;

                if ((a = *(f32*)(e + offsetof(Enemy, ang))) > 3.141592654) {
                    a -= 6.283185308;
                } else if (a <= (-3.141592654)) {
                    a = 6.283185308 + a;
                }
                *(f32*)(e + offsetof(Enemy, ang)) = (f32)a;
                *(f32*)(e + offsetof(Enemy, pyr[1])) = (f32)a;
            }
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
            *(s32*)(e + offsetof(Enemy, route)) = 0;
        }
        if (*(s16*)(e + offsetof(Enemy, collided)) >= 7) {
            *(s32*)(e + offsetof(Enemy, route)) = -*(s32*)(e + offsetof(Enemy, route)) * 2;
            *(s16*)(e + offsetof(Enemy, collided)) = 0;
        }
    } else {
        if (*(s32*)(e + offsetof(Enemy, dead_end)) <= 0) {
            *(s32*)(e + offsetof(Enemy, dead_end)) = 0x14;
        }
    }
    *(s16*)(e + offsetof(Enemy, area)) = 1;

gravity:
    dh = *(f32*)(e + offsetof(Enemy, floory)) - *(f32*)(e + offsetof(Enemy, objgrp.worldmat[3][1]));
    if ((f64)dh < (-5.0)) {
        damage_enemy(enemy, 99999.0f, -1, 0, 0, 0, 0);
    }
    if (dh < dt) {
        dh = dt;
    }
    tr[1] += dh;
    *(f32*)(e + offsetof(Enemy, floory)) = *(f32*)(e + offsetof(Enemy, objgrp.worldmat[3][1])) + dh;
    return result;
}

static s32 EnemyMovingAwayFromBirth(Enemy* enemy, f32* oldPosition,
                                    f32* translation)
{
    f32 movedY;
    f32 dy;
    f32 movedX;
    f32 dz;
    f32 dx;
    f32 movedZ;

    dy = enemy->birth_pos[1] - oldPosition[1];
    dx = enemy->birth_pos[0] - oldPosition[0];
    movedY = dy + translation[1];
    dz = enemy->birth_pos[2] - oldPosition[2];
    movedX = dx + translation[0];
    movedZ = dz + translation[2];
    return movedZ * movedZ + (movedX * movedX + movedY * movedY) >
           dz * dz + (dx * dx + dy * dy);
}

void* fn_80045C30(Enemy* enemy, f32 radius, f32 retryThreshold,
                  f32* oldPosition, f32* translation, s32 collisionClass)
{
    u8* pool;
    f32* floorYAddress;
    void* floorObject;
    f32 tolerance;
    f32 distance;
    f32 probe[3];
    f32 step[3];
    u8 unused[8];
    f32 baseY;
    f64 halfRadius;
    f32 floorY;

    (void)unused;
    pool = (u8*)mbdesc;
    tolerance = (f32)(2.0 *
                      (0.1 + (f64)(retryThreshold + radius)));
    if (enemy->type == E_GOLEM || (f64)enemy->hht <= 2.0) {
        if (EnemyMovingAwayFromBirth(enemy, oldPosition, translation)) {
            tolerance = (f32)(0.5 *
                              (retryThreshold + radius));
        }
    }

    step[0] = translation[0];
    step[1] = translation[1];
    step[2] = translation[2];
    distance = radius + NormalVector(step);
    step[0] *= distance;
    step[1] *= distance;
    step[2] *= distance;
    probe[0] = oldPosition[0] + step[0];
    probe[1] = oldPosition[1] + step[1];
    probe[2] = oldPosition[2] + step[2];

    halfRadius = 0.5 * radius;
    floorObject = (void*)FloorCollide(
        probe, (s32)&enemy_floor_col, 0, 2, (f32)halfRadius, enemy->hht,
        (f32)(-enemy->hht - 5.0));
    if (floorObject != 0) {
        EnemyWorldDamage(enemy, floorObject, oldPosition,
                         enemy_floor_col.mtx[3]);
    } else {
        if ((f64)enemy->pushmag2 < 0.01) {
            translation[0] = translation[2] = 0.0f;
        } else {
            enemy->floory = (f32)((f64)lbl_80344880 - 1000.0);
        }
        return 0;
    }

    floorYAddress = &enemy_floor_col.mtx[3][1];
    baseY = enemy->floory - enemy->flooroffset;
    floorY = *floorYAddress;
    distance = floorY - baseY;
    if (distance < 0.0f) {
        distance = -distance;
    }
    if (distance > tolerance) {
        if ((f64)enemy->pushmag2 < 0.01) {
            f32 zero = 0.0f;
            translation[2] = zero;
            translation[0] = zero;
        } else {
            enemy->floory = floorY + enemy->flooroffset;
        }
        return 0;
    }

    if ((f64)retryThreshold > 0.0) {
        if ((f64)distance > 0.1 * retryThreshold) {
            probe[0] = oldPosition[0] + translation[0];
            probe[1] = oldPosition[1] + translation[1];
            probe[2] = oldPosition[2] + translation[2];
            floorObject = (void*)FloorCollide(
                probe, (s32)&enemy_floor_col, 0, 2, (f32)halfRadius,
                enemy->hht, (f32)(-enemy->hht - 5.0));
            if (floorObject == 0) {
                translation[0] = translation[2] = 0.0f;
                return 0;
            }
            floorY = *floorYAddress;
        }
    }

    if ((u32)(collisionClass - 1) <= 1 ||
        (collisionClass == 3 && lbl_80344730 != floorObject)) {
        if (collisionClass != 1 && collisionClass != 3) {
            goto collision_blocked;
        }
        if ((enemy->ai_flags & 1) != 0) {
            goto collision_blocked;
        }
        if (SlideAlongWall(radius, oldPosition, translation,
                           (f32*)(pool + 0x2F4), lbl_8023CA98[1]) < 0) {
            f32 zero = 0.0f;
            translation[2] = zero;
            translation[0] = zero;
            return 0;
        }
        enemy->floory = floorY + enemy->flooroffset;
        goto collision_done;
    collision_blocked:
        {
            f32 zero = 0.0f;
            translation[2] = zero;
            translation[0] = zero;
            return 0;
        }
    }
collision_done:
    enemy->floory = floorY + enemy->flooroffset;
    if (enemy->shadow != 0) {
        CopyMat3(&enemy_floor_col.mtx[0][0], (f32*)enemy->shadow);
    }
    return floorObject;
}

void EnemyWorldDamage(Enemy* e, void* wobj, f32* oldpos, f32* hitnrm)
{
    u32 flags;
    f32 dir[3];

    flags = WorldObjGetAllFlags(wobj);
    if (e->type == E_GARM2) {
        return;
    }
    if ((flags & 0xF0000) == 0) {
        return;
    }
    if (flags & 0x02000000) {
        if (!(flags & 0x08000000)) {
            return;
        }
    }
    dir[0] = oldpos[0] - hitnrm[0];
    dir[1] = 0.0f;
    dir[2] = oldpos[2] - hitnrm[2];
    NormalVector2D(dir);
    flags &= 0xF0000;
    switch (flags) {
    case 0x10000:
        damage_enemy(e, 5.0f, -1, 0, (s32)hitnrm, (s32)dir, 1);
        break;
    case 0x20000:
        damage_enemy(e, 5.0f, -1, 16, (s32)hitnrm, (s32)dir, 1);
        break;
    case 0x30000:
    case 0x40000:
    case 0x50000:
        damage_enemy(e, 15.0f, -1, 32, (s32)hitnrm, (s32)dir, 1);
        break;
    case 0x60000:
        break;
    }
}

void fn_80046140(s32 index)
{
    u8* pool = (u8*)mbdesc;
    Enemy* enemy = (Enemy*)(pool + index * 0x394 + 0xE18);
    s32 playerIndex;
    s32 damaged;

    if (enemy->type == E_DEATH) {
        if (enemy->stun_timer <= 0 &&
            (gPlayers[enemy->coll_pnum].shield_flags & 0x80000) == 0) {
            if (enemy->endurance > 0) {
                enemy->endurance--;
            } else {
                RequestEnemyAction(enemy, E_ATTACK);
                if ((enemy->attack_timer -= gFrameTicks) <= 0) {
                    enemy->attack_timer += 3;
                    if (enemy->org_lvl == 2) {
                        AddExp(enemy->coll_pnum, -1, -2);
                        damaged = 0;
                    } else {
                        damaged = damage_player(enemy->coll_pnum, -enemy->atts.fight,
                                                1, 0x1000, 0);
                    }
                    if (enemy->org_lvl == 2) {
                        msgPost(0x80, enemy->coll_pnum, &enemy->objgrp.attn_pos[0]);
                    } else {
                        msgPost(0x82, enemy->coll_pnum, &enemy->objgrp.attn_pos[0]);
                    }
                    lbl_803447E4 = 1;
                    enemy->attack_count++;
                    if (damaged != 0) {
                        enemy->health = 0.0f;
                    } else if (enemy->specialfx < 0) {
                        enemy->specialfx = StartDeathFX(enemy->objgrp.node,
                                                       enemy->org_lvl, 0);
                    }
                    if ((f64)(enemy->health -= enemy->atts.fight) >= 0.0) {
                        AudioPlayEvt102Follow(&enemy->objgrp.worldmat[3][0],
                                              enemy->coll_pnum);
                        lbl_80344718 = 1;
                    } else {
                        enemy->flag2 = 1;
                        AudioPlayEvt104(&enemy->objgrp.worldmat[3][0]);
                        playerIndex = enemy->coll_pnum;
                        pool = (u8*)(enemy - (Enemy*)(pool + 0xE18));
                        enemy->health = 0.0f;
                        enemy->state = DYING;
                        enemy->area = (s16)playerIndex;
                        if (enemy->algorithm == 18) {
                            SuicideExplosion(&enemy->objgrp.coll_pos[0],
                                (f32)(50.0 * gCurLevel->ene_damage));
                            fn_8009DAC8(&enemy->objgrp.coll_pos[0]);
                        }
                        uncouple_enemy((s32)pool);
                    }
                }
            }
        }
    } else if (enemy->type == E_IT) {
        /* +0x954 is also the IT-transfer timer in PlayerMotion. Keep the
         * canonical header's speak_timer name; this is not a second field. */
        if (lbl_80344B24 >= 0) {
            gPlayers[lbl_80344B24].speak_timer = 0;
        }
        lbl_80344B24 = enemy->coll_pnum;
        gPlayers[lbl_80344B24].speak_timer = 1;
        msgPost(0x32, gPlayers[lbl_80344B24].index,
                &gPlayers[lbl_80344B24].col_pos[0]);

        playerIndex = lbl_80344B24;
        pool = (u8*)(enemy - (Enemy*)(pool + 0xE18));
        enemy->health = 0.0f;
        enemy->state = DYING;
        enemy->area = (s16)playerIndex;
        if (enemy->algorithm == 18) {
            SuicideExplosion(&enemy->objgrp.coll_pos[0],
                (f32)(50.0 * gCurLevel->ene_damage));
            fn_8009DAC8(&enemy->objgrp.coll_pos[0]);
        }
        uncouple_enemy((s32)pool);
    } else if (enemy->algorithm != 31) {
        enemy->attack_index = (s16)enemy->coll_pnum;
        if ((enemy->attack_count & 7) == 7) {
            RequestEnemyAction(enemy, E_ATTACK_PWR);
        } else {
            RequestEnemyAction(enemy, E_ATTACK);
        }
    }
}

extern s32 NextGridItem(void);

/* Xbox ENEMY.OBJ names this source helper is_tail(int, int).  The retail
 * Xbox body and the GC inlined body both walk next_enemy through a pointer;
 * retaining that source-level helper also preserves the GC loop topology. */
static inline int is_tail(int my_idx, int chk_idx)
{
    Enemy* enemy;
    int idx;

    for (enemy = &gEnemies[my_idx];
         (idx = enemy->next_enemy) >= 0;
         enemy = &gEnemies[idx]) {
        if (idx == chk_idx) {
            return -1;
        }
    }
    return 0;
}

int fn_8004646C(int index, f32* oldc, f32* newc, f32* newc2, f32 rad, f32 hht,
                int* hitWorld)
{
    Enemy* other;
    int st;
    int node;
    int result = -1;
    int hint = -1;
    int startNode;
    f64 minimum_hht;
    f32 dist;
    f32 best = 100000.0f;
    void* nodeCol;
    u8 stack_top[8];
    f32 scratch[3];
    u8 stack_gap[12];
    f32 delta[3];
    u8 stack_bottom[28];

    startNode = gEnemies[index].coll_enenum;
    if (hitWorld == NULL && startNode < 0x10000) {
        hint = startNode;
    }
    CritterCollideStart(rad, newc, 0);
    nodeCol = CritterMoveNodeCol(rad, 0.0f, oldc, newc, scratch, -1, 2);
    if (nodeCol != NULL) {
        return *(s16*)nodeCol | 0x10000;
    }
    StartItemGrid(newc, rad);
    minimum_hht = 2.0;
    for (;;) {
        if (hint < 0) {
            node = NextGridItem();
        } else {
            node = hint;
            hint = -1;
        }
        if (node < 0) {
            break;
        }
        other = &gEnemies[node];
        if (node == index) {
            continue;
        }
        st = other->state;
        if (st == 0 || st == 8) {
            continue;
        }
        if (is_tail(index, node)) {
            continue;
        }
        if ((f64)other->hht <= minimum_hht) {
            if (other->type == E_GOLEM) {
                continue;
            }
        }
        delta[0] = other->objgrp.coll_pos[0] - newc[0];
        delta[1] = other->objgrp.coll_pos[1] - newc[1];
        delta[2] = other->objgrp.coll_pos[2] - newc[2];
        {
            dist = NormalVector2D(delta);

            if (dist >= best) {
                continue;
            }
            if (LineCylinderCollide(&other->objgrp.coll_pos[0],
                                    rad + other->rad,
                                    hht + other->hht, oldc,
                                    newc, scratch, 1) == 0) {
                continue;
            }
            best = dist;
        }
        result = node;
        if (hitWorld != NULL) {
            continue;
        }
        if (node != startNode) {
            continue;
        }
        break;
    }
    return result;
}

/* Compiler-compatibility exception approved by the user, 2026-09-05.
 * Emit the existing controls.c sqrt body here so MWCC creates the 3.0
 * constant at the observed point in this TU's pool. mwld discards this weak
 * duplicate in favor of the original controls implementation at 0x80034C88;
 * its code and exception entries are not linked. Full-image comparison
 * verifies the resulting pool and metadata. Original header provenance is
 * UNPROVEN: this is documented compatibility scaffolding, not a claim that
 * Midway wrote a second definition here. Other compilers use the extern
 * declaration above instead. Graph record:
 * attempt.R62_enemy-literal-pool-and-initialized-tables-exact-link.20260905.v1 */
#ifdef __MWERKS__
#pragma dont_inline on
__declspec(weak) f32 fn_80034C88(f32 x)
{
    volatile f32 result;

    if (x > 0.0f) {
        f64 y = __frsqrte(x);

        y = 0.5 * y * (3.0 - y * y * x);
        y = 0.5 * y * (3.0 - y * y * x);
        y = 0.5 * y * (3.0 - y * y * x);
        result = (f32)(x * (0.5 * y * (3.0 - y * y * x)));
        x = result;
    }
    return x;
}

#pragma dont_inline reset
#endif

/* Xbox retains an int-returning get_actual_closest_player(Enemy*), with
 * nearest initialized before the distance bound (source lines 278/279).
 * GC inlines that search. Its vector arithmetic and the caller's real hit
 * and delta vectors replace the former output-parameter and padding trick. */
static inline int get_actual_closest_player(Enemy* e)
{
    Player* p;
    int i;
    int nearest = -1;
    f32 best = 100000.0f;
    f32 d;

    for (i = 0, p = gPlayers; i < 4; i++, p++) {
        if (p->state == 1) {
            /* A live mikey supplies its collision position instead of
             * the player's own effectpos. */
            if (p->field_A1C > 2) {
                f32 distance[3];
                distance[0] = e->objgrp.coll_pos[0] - p->mikey_coll_pos[0];
                distance[1] = e->objgrp.coll_pos[1] - p->mikey_coll_pos[1];
                distance[2] = e->objgrp.coll_pos[2] - p->mikey_coll_pos[2];
                d = fn_80034C88(distance[0] * distance[0] +
                                distance[1] * distance[1] +
                                distance[2] * distance[2]);
            } else {
                f32 distance[3];
                distance[0] = e->objgrp.coll_pos[0] - p->effectpos[0];
                distance[1] = e->objgrp.coll_pos[1] - p->effectpos[1];
                distance[2] = e->objgrp.coll_pos[2] - p->effectpos[2];
                d = fn_80034C88(distance[0] * distance[0] +
                                distance[1] * distance[1] +
                                distance[2] * distance[2]);
            }
            if (d < best) {
                best = d;
                nearest = i;
            }
        }
    }
    return nearest;
}

/* 0x80046680 - pick the player hit by the enemy's swept collision cylinder;
 * b==0 restricts the sweep to the nearest live player. */
int fn_80046680(int index, int b, f32* oldc, f32* newc, f32 rad, f32 hht)
{
    Player* q;
    int j;
    int last;
    Enemy* e = &gEnemies[index];
    int ret = -1;
    int start;
    f32 best = 100000.0f;
    f32 hit[3];
    f32 delta[3];
    f32 d;

    if (b != 0) {
        start = 0;
        last = 3;
    } else {
        if (e->closest < 0) {
            return -1;
        }
        last = get_actual_closest_player(e);
        start = last;
    }
    q = gPlayers + start;
    for (j = start; j <= last; j++, q++) {
        if (q->state == 1) {
            if (LineCylinderCollide(q->effectpos,
                                    rad + q->col_radius,
                                    hht + q->col_height,
                                    oldc, newc, hit, 1) != 0) {
                delta[0] = hit[0] - newc[0];
                delta[1] = hit[1] - newc[1];
                delta[2] = hit[2] - newc[2];
                d = fqdist(delta[0], delta[2]);
                if (d < best) {
                    ret = j;
                    best = d;
                }
            }
        }
    }
    return ret;
}

/* do_ai @0x80046854 - central AI dispatcher.  Zeroes the per-frame translation,
 * then jumps through jumptable_8011C0EC[algorithm] to the matching move_logicNN
 * handler (0xB idle-wobble and 0x1B spawn are handled inline).  Returns -1 if the
 * enemy dropped to INACTIVE this tick, else 0. */
s32 do_ai(s32 index)
{
    Enemy* e = &gEnemies[index];
    f32 mat[16];
    u8 unused[8];

    e->trans[0] = 0.0f;
    e->trans[1] = 0.0f;
    e->trans[2] = 0.0f;
    switch (e->algorithm) {
    case 0:
        move_logic00(index);
        break;
    case 1:
        move_logic01(index);
        break;
    case 2:
        move_logic02(index);
        break;
    case 3:
        move_logic03(index);
        break;
    case 4:
        move_logic04(index);
        break;
    case 5:
        move_logic05(index);
        break;
    case 6:
        move_logic06(index);
        break;
    case 7:
        move_logic07(index);
        break;
    case 8:
        move_logic08(index);
        break;
    case 9:
        move_logic00(index);
        break;
    case 10:
        move_logic10(index);
        break;
    case 11:
        e->daction = 0;
        e->ang = 0.005555555555555556 * (3.141592654 * (f32)(u32)gFrameTicks) + e->ang;
        {
            f64 a;

            if ((a = e->ang) > 3.141592654) {
                a -= 6.283185308;
            } else if (a <= -3.141592654) {
                a = 6.283185308 + a;
            }
            e->ang = a;
        }
        e->pyr[1] = turn_enemy_ang(e, e->ang);
        CreateYPRMatrix(mat, &e->pyr[0]);
        CopyMat3(mat, &e->objgrp.worldmat[0][0]);
        if (e->generator == 0 || (e->state == 7 && gGameOptions[2] > 1)) {
            kill_enemy(index);
        }
        break;
    case 12:
        move_logic12(index);
        break;
    case 13:
        move_logic13(index);
        break;
    case 14:
        move_logic14(index);
        break;
    case 15:
        move_logic15(index);
        break;
    case 16:
        move_logic16(index);
        break;
    case 17:
        move_logic23(index);
        break;
    case 18:
        move_logic18(index);
        break;
    case 19:
        move_logic19(index);
        break;
    case 20:
        move_logic20(index);
        break;
    case 21:
        move_logic21(index);
        break;
    case 22:
        move_logic22(index);
        break;
    case 23:
        move_logic23(index);
        break;
    case 24:
        move_logic24(index);
        break;
    case 26:
        move_logic16(index);
        break;
    case 27:
        if (e->closest < 0) {
            e->trans[0] = 0.0f;
            e->trans[1] = 0.0f;
            e->trans[2] = 0.0f;
            do_enemy_move(index);
        } else {
            e->sight = 999999.0f;
            e->algorithm = 0;
            move_logic00(index);
        }
        break;
    case 28:
        move_logic28(index);
        break;
    case 29:
        move_logic29(index);
        break;
    case 30:
        move_logic30(index);
        break;
    case 31:
        move_logic31(index);
        break;
    default:
        kill_enemy(index);
        break;
    }
    if (e->state == 0) {
        return -1;
    }
    return 0;
}

/* float magnitude via sign-bit clear (matches the enemy AI's inline fabs). */
static f32 fabsf_(f32 x)
{
    *(u32*)&x &= 0x7FFFFFFF;
    return x;
}

/* One-step heading wrap shared by the enemy movement calculations. The
 * input and returned heading are floats; the conditional arithmetic uses
 * double constants, with one final float rounding at the join. */
static inline f32 enemy_normalized_heading(f32 a)
{
    return a > 3.141592654 ? a - 6.283185308 :
          (a <= -3.141592654 ? 6.283185308 + a : a);
}

/* The target expands the same four-step square-root kernel at each
 * distance site, including a volatile float rounding store/reload. This
 * is the operation also described by MSL's sqrtf_accurate, not a request
 * for an arbitrary native sqrt implementation with different rounding. */
static inline f32 enemy_distance_sqrt(f32 x)
{
    volatile f32 y;

    if (x > 0.0f) {
        f64 guess = __frsqrte((f64)x);

        guess = 0.5 * guess * (3.0 - guess * guess * x);
        guess = 0.5 * guess * (3.0 - guess * guess * x);
        guess = 0.5 * guess * (3.0 - guess * guess * x);
        guess = 0.5 * guess * (3.0 - guess * guess * x);
        y = (f32)(x * guess);
        return y;
    }
    return x;
}

/* The retained Xbox helper and repeated GC caller expansions identify this
 * bomber query. Its parameter/result are int, not the project's signed-long
 * s32: equal ABI widths do not give MWCC identical inline argument lifetimes.
 * The real three-dimensional delta replaces the callers' old frame padding.
 * Preserve GC's early greater-than rejection, including unordered inputs. */
static inline int FoundSuicideBomber(int num)
{
    Enemy* self = &gEnemies[num];
    int it = lbl_80344748;

    if (it < 0) {
        return 0;
    }
    if (gEnemies[it].state != ACTIVE) {
        return 0;
    }
    if (gEnemies[it].actual_dist > self->sight) {
        return 0;
    }
    if (num != it && self->birth_style == 0 && self->dead_end <= 0) {
        f32 delta[3];
        delta[0] = gEnemies[it].objgrp.worldmat[3][0] - self->objgrp.worldmat[3][0];
        delta[1] = gEnemies[it].objgrp.worldmat[3][1] - self->objgrp.worldmat[3][1];
        delta[2] = gEnemies[it].objgrp.worldmat[3][2] - self->objgrp.worldmat[3][2];
        if (delta[0] * delta[0] + delta[1] * delta[1] + delta[2] * delta[2] < 100.0) {
            return -1;
        }
    }
    return 0;
}

/* get_face_ang(Enemy*, int) is retained in the Xbox executable. GC callers
 * inline this same player/mikey selection and yaw fallback. Keep the returned
 * float's lifetime at callers; manually flattening it loses inline locals. */
static inline f32 get_face_ang(Enemy* e, int always)
{
    f32 angle;
    if (e->closest >= 0 && always) {
        if (gPlayers[e->closest].field_A1C > 2) {
            angle = get_yaw(gPlayers[e->closest].mikey_worldmat[3], &e->objgrp.worldmat[3][0]);
        } else {
            angle = get_yaw(gPlayers[e->closest].pos, &e->objgrp.worldmat[3][0]);
        }
    } else {
        angle = e->ang;
    }
    return angle;
}

/* move_logic00 @0x80046B54 (state 0 + 9, base wander/seek).  IT-flee / chase
 * gates, then face the closest player and sweep up to 9 offset headings,
 * projecting each with sin/cos and probing for wall clearance; commit the first
 * clear heading (recording the try count), else keep the straight bearing. */
void move_logic00(int index)
{
    Enemy* e = &gEnemies[index];
    int i;
    f32 spd = lbl_80250E40[e->type];
    f32 dest[3];
    f32 ang;

    if (FoundSuicideBomber(index) != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    if (e->dead_end <= 0) {
        {
            f32 f = get_face_ang(e, 1);
            ang = f;
        }
        lbl_80344720 = ang;
        {
            i = 0;
            do {
                f32 d;
                if (e->route > 0) {
                    ang = ang + lbl_8011C0C4[i];
                } else {
                    ang = ang - lbl_8011C0C4[i];
                }
                ang = enemy_normalized_heading(ang);
                dest[0] = e->objgrp.worldmat[3][0];
                dest[1] = e->objgrp.worldmat[3][1];
                dest[2] = e->objgrp.worldmat[3][2];
                dest[1] += 0.1 + e->rad;
                dest[0] += spd * sin(ang);
                dest[2] += spd * cos(ang);
                d = ang - e->ang;
                d = enemy_normalized_heading(d);
                if ((!(fabsf_(e->ang - e->angbak) > 0.034906585044444445)
                     || !(fabsf_(ang - e->angbak) <= 0.034906585044444445))
                    && !(fabsf_(d) >= 3.106686068955556)
                    && fn_8004C8CC(dest, index) != 0) {
                    break;
                }
                i++;
            } while (i < 9);
            if (i >= 9) {
                ang = lbl_80344720;
            } else {
                e->collided = i;
            }
            e->angbak = e->ang;
            e->ang = ang;
        }
    }
    set_enemy_trans(e, 1.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic01 @0x80046F24 (state 1, patrol-toward-milestone).  IT-flee / chase
 * gates, then face a milestone only after an operation-speed cadence, run down a
 * short area-2 timer, and re-lock the heading when it drifts past pi/30. */
void move_logic01(s32 index)
{
    s32 stuck;
    u8* base = (u8*)mbdesc;
    u8* row01;
    u8* e0;
    Enemy* e;
    s32 it;
    s32 dead0;
    s32 flee;
    f32 a;
    u8 unused[24];

    row01 = base + index * 916;
    dead0 = ((Enemy *)(row01 + ENEMY_POOL_OFF))->dead_end;
    e0 = row01 + 3608;
    e = (Enemy*)(u8*)e0;
    if (dead0 > 0) {
        stuck = 1;
    } else {
        stuck = 0;
    }
    it = lbl_80344748;
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = base + it * 916;
        if (((Enemy *)(other + ENEMY_POOL_OFF))->state != ACTIVE) {
            flee = 0;
        } else if (((Enemy *)(other + ENEMY_POOL_OFF))->actual_dist > ((Enemy *)e0)->sight) {
            flee = 0;
        } else if (index == it || ((Enemy *)e0)->birth_style != 0 || dead0 > 0) {
            goto flee_zero01;
        } else {
            f32 dx = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][0] - ((Enemy *)e0)->objgrp.worldmat[3][0];
            f32 dy = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][1] - ((Enemy *)e0)->objgrp.worldmat[3][1];
            f32 dz = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][2] - ((Enemy *)e0)->objgrp.worldmat[3][2];
            if (dx * dx + dy * dy + dz * dz < 100.0) {
                flee = -1;
            } else {
            flee_zero01:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->closest < 0 || e->operation_count < e->operation_speed) {
        a = e->ang;
    } else {
        if (gPlayers[e->closest].field_A1C > 2) {
            e->ang = get_yaw(gPlayers[e->closest].mikey_worldmat[3], &e->objgrp.worldmat[3][0]);
        } else {
            e->ang = get_yaw(gPlayers[e->closest].pos, &e->objgrp.worldmat[3][0]);
        }
        a = e->ang;
    }
    e->ang = a;
    if (e->area == 2 && e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    {
        f32 d;
        u8 _blk01[8];
        if (e->dead_end <= 0
            || ((d = e->ang - e->anghit), (*(u32*)&d &= 0x7FFFFFFF),
                d >= 0.10471975513333334)) {
            e->dead_end = 0;
            set_enemy_trans(e, 1.0f, e->ang);
        }
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    if (stuck == 0 && e->dead_end > 0) {
        e->anghit = e->ang;
    }
}
/* move_logic02 @0x800471A4 (state 2, demon-flee-from-IT).  If the "IT" enemy is
 * active, close, at the same height and this enemy is fresh, flee it (algorithm
 * 24); once it has closed on a player switch to chase; otherwise drift on a
 * timer, wobbling the heading by pi/4 and periodically re-rolling to ON_EXIT. */
void move_logic02(int index)
{
    Enemy* e;
    s32 it;

    e = &gEnemies[index];
    if (FoundSuicideBomber(index) != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->closest >= 0 && e->close_dist <= 8.0) {
        e->algorithm = 0;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->dead_end > 0) {
        if ((e->dead_end -= gFrameTicks) <= 0) {
            e->ang += 0.7853981635;
            {
                f64 a = e->ang;
                if (a > 3.141592654) {
                    a -= 6.283185308;
                } else if (a <= (-3.141592654)) {
                    a = 6.283185308 + a;
                }
                e->ang = a;
            }
            if (++e->count >= 4) {
                e->count = 0;
                e->algorithm = 4;
                e->old_ai = 4;
                e->play = 4;
            }
        }
    }
    if ((it = e->coll_pnum) >= 0) {
        e->ang = get_yaw(gPlayers[it].pos, &e->objgrp.worldmat[3][0]);
    }
    set_enemy_trans(e, 1.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic03 @0x800473E8 (state 3, rat-scatter flee).  Bounce to the chase
 * or wander algorithms unless it is actively fleeing a milestone target; ramp
 * a retreat speed, face away from that target (facing + pi), normalize,
 * accelerate, damp the resulting velocity to 0.9, turn + move, refresh. */
void move_logic03(s32 index)
{
    Enemy* e = &gEnemies[index];
    f32 spd;
    s32 stuck;
    s32 c;
    f32 face;

    if (e->recognized == 0 || e->closest < 0) {
        if (e->counter2 < 0) {
            e->algorithm = (index & 1) + 5;
            do_ai(index);
            return;
        }
        spd = 0.0f;
        if (e->dead_end > 0) {
            stuck = 1;
        } else {
            stuck = 0;
        }
        if (e->dead_end > 0 && (c = e->counter1) < 8) {
            e->counter1 = c + 1;
            spd = lbl_8011BF60[c];
            e->dead_end = 0;
        }
        if (e->counter2 >= 0) {
            if (gPlayers[e->counter2].field_A1C > 2) {
                face = get_yaw(gPlayers[e->counter2].mikey_worldmat[3], &e->objgrp.worldmat[3][0]);
            } else {
                face = get_yaw(gPlayers[e->counter2].pos, &e->objgrp.worldmat[3][0]);
            }
        } else {
            face = e->ang;
        }
        e->ang = spd + (3.141592654 + face);
        {
            f64 a;

            if ((a = e->ang) > 3.141592654) {
                a -= 6.283185308;
            } else if (a <= -3.141592654) {
                a = 6.283185308 + a;
            }
            e->ang = a;
        }
        set_enemy_trans(e, 1.0f, e->ang);
        e->pyr[1] = turn_enemy_ang(e, e->ang);
        e->trans[0] *= 0.9;
        e->trans[1] *= 0.9;
        e->trans[2] *= 0.9;
        do_enemy_move(index);
        if (e->moved != 0) {
            e->dead_end = 0;
        }
        if (stuck == 0 && e->dead_end > 0) {
            e->anghit = e->ang;
            e->counter1 = 0;
        }
    } else {
        e->algorithm = 0;
        do_ai(index);
    }
}

/* move_logic04 @0x800475FC (state 4, sibling of move_logic02).  Same IT-flee /
 * chase gating, but the drift-timer wobbles the heading the other way (-pi/4)
 * and, on expiry, rolls to the ON_EXIT taunt (algorithm/old_ai = 2, play = 4). */
void move_logic04(int index)
{
    Enemy* e;

    e = &gEnemies[index];
    if (FoundSuicideBomber(index) != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->closest >= 0 && e->close_dist <= 8.0) {
        e->algorithm = 0;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->dead_end > 0) {
        if ((e->dead_end -= gFrameTicks) <= 0) {
            e->ang -= 0.7853981635;
            {
                f64 a = e->ang;
                if (a > 3.141592654) {
                    a -= 6.283185308;
                } else if (a <= (-3.141592654)) {
                    a = 6.283185308 + a;
                }
                e->ang = a;
            }
            if (++e->count >= 4) {
                e->count = 0;
                e->algorithm = 2;
                e->old_ai = 2;
                e->play = 4;
            }
        }
    }
    if (e->coll_pnum >= 0) {
        e->ang = get_yaw(gPlayers[e->coll_pnum].pos, &e->objgrp.worldmat[3][0]);
    }
    set_enemy_trans(e, 1.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

#pragma opt_propagation off
void move_logic05(s32 index)
{
    Enemy* e;
    Enemy* e0;
    s32 it = lbl_80344748;
    s32 type;
    f32 dist;
    f32 speed;
    s32 flee;
    f32 probe2[3];
    f32 probe[3];
    f32 probeEnd[3];
    u8 _pad05[56];

    e0 = &gEnemies[index];
    type = e0->type;
    e = e0;
    dist = e->rad;
    speed = lbl_80250E40[type];
    if (it < 0) {
        flee = 0;
    } else {
        if (gEnemies[it].state != ACTIVE) {
            flee = 0;
        } else if (gEnemies[it].actual_dist > e->sight) {
            flee = 0;
        } else if (index == it || e->birth_style != 0 || e->dead_end > 0) {
            goto flee_zero05;
        } else {
            f32 dx = gEnemies[it].objgrp.worldmat[3][0] - e0->objgrp.worldmat[3][0];
            f32 dy = gEnemies[it].objgrp.worldmat[3][1] - e0->objgrp.worldmat[3][1];
            f32 dz = gEnemies[it].objgrp.worldmat[3][2] - e0->objgrp.worldmat[3][2];
            if (dx * dx + dy * dy + dz * dz < 100.0) {
                flee = -1;
            } else {
            flee_zero05:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->dead_end > 0) {
        if ((e->dead_end -= gFrameTicks) <= 0) {
            e->ang = e->ang - 1.570796327;
            {
                f64 a = e->ang;
                if (a > 3.141592654) {
                    a -= 6.283185308;
                } else if (a <= (-3.141592654)) {
                    a = 6.283185308 + a;
                }
                e->ang = a;
            }
            if (++e->count >= 4) {
                e->count = 0;
            }
        }
    }
    probe[0] = e0->objgrp.worldmat[3][0];
    probe[1] = e0->objgrp.worldmat[3][1];
    probe[2] = e0->objgrp.worldmat[3][2];
    probe[1] += 0.1 + e->rad;
    probeEnd[0] = probe[0];
    probeEnd[1] = probe[1];
    probeEnd[2] = probe[2];
    dist += 0.5;
    probeEnd[0] += dist * sin(e->ang);
    probeEnd[2] += dist * cos(e->ang);
    if (FastWallCollide(probe, probeEnd, 0, 2) != 0) {
        e->ang = e->ang - 1.570796327;
        {
            f64 a = e->ang;
            if (a > 3.141592654) {
                a -= 6.283185308;
            } else if (a <= (-3.141592654)) {
                a = 6.283185308 + a;
            }
            e->ang = a;
        }
        if (e->dead_end <= 0) {
            e->dead_end = 20;
        }
    } else {
        probe2[0] = probe[0];
        probe2[1] = probe[1];
        probe2[2] = probe[2];
        probe2[0] += speed * sin(e->ang);
        probe2[2] += speed * cos(e->ang);
        if (fn_8004C8CC(probe2, index) == 0) {
            e->ang = e->ang - 1.570796327;
            {
                f64 a = e->ang;
                if (a > 3.141592654) {
                    a -= 6.283185308;
                } else if (a <= (-3.141592654)) {
                    a = 6.283185308 + a;
                }
                e->ang = a;
            }
            if (e->dead_end <= 0) {
                e->dead_end = 20;
            }
        }
    }
    set_enemy_trans(e, 1.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}
#pragma opt_propagation reset

/* move_logic06 @0x80047BF0 (state 6, mirror of move_logic05).  Same IT-flee and
 * two-probe clearance search, but every heading correction rotates +pi/2 instead
 * of -pi/2, so it sweeps the opposite way around an obstacle. */
#pragma opt_propagation off
void move_logic06(s32 index)
{
    Enemy* e;
    Enemy* e0;
    s32 it = lbl_80344748;
    s32 type;
    f32 dist;
    f32 speed;
    s32 flee;
    f32 probe2[3];
    f32 probe[3];
    f32 probeEnd[3];
    u8 _pad06[56];

    e0 = &gEnemies[index];
    type = e0->type;
    e = e0;
    dist = e->rad;
    speed = lbl_80250E40[type];
    if (it < 0) {
        flee = 0;
    } else {
        if (gEnemies[it].state != ACTIVE) {
            flee = 0;
        } else if (gEnemies[it].actual_dist > e->sight) {
            flee = 0;
        } else if (index == it || e->birth_style != 0 || e->dead_end > 0) {
            goto flee_zero06;
        } else {
            f32 dx = gEnemies[it].objgrp.worldmat[3][0] - e0->objgrp.worldmat[3][0];
            f32 dy = gEnemies[it].objgrp.worldmat[3][1] - e0->objgrp.worldmat[3][1];
            f32 dz = gEnemies[it].objgrp.worldmat[3][2] - e0->objgrp.worldmat[3][2];
            if (dx * dx + dy * dy + dz * dz < 100.0) {
                flee = -1;
            } else {
            flee_zero06:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->dead_end > 0) {
        if ((e->dead_end -= gFrameTicks) <= 0) {
            e->ang += 1.570796327;
            {
                f64 a = e->ang;
                if (a > 3.141592654) {
                    a -= 6.283185308;
                } else if (a <= (-3.141592654)) {
                    a = 6.283185308 + a;
                }
                e->ang = a;
            }
            if (++e->count >= 4) {
                e->count = 0;
            }
        }
    }
    probe[0] = e0->objgrp.worldmat[3][0];
    probe[1] = e0->objgrp.worldmat[3][1];
    probe[2] = e0->objgrp.worldmat[3][2];
    probe[1] += 0.1 + e->rad;
    probeEnd[0] = probe[0];
    probeEnd[1] = probe[1];
    probeEnd[2] = probe[2];
    dist += 0.5;
    probeEnd[0] += dist * sin(e->ang);
    probeEnd[2] += dist * cos(e->ang);
    if (FastWallCollide(probe, probeEnd, 0, 2) != 0) {
        e->ang += 1.570796327;
        {
            f64 a = e->ang;
            if (a > 3.141592654) {
                a -= 6.283185308;
            } else if (a <= (-3.141592654)) {
                a = 6.283185308 + a;
            }
            e->ang = a;
        }
        if (e->dead_end <= 0) {
            e->dead_end = 20;
        }
    } else {
        probe2[0] = probe[0];
        probe2[1] = probe[1];
        probe2[2] = probe[2];
        probe2[0] += speed * sin(e->ang);
        probe2[2] += speed * cos(e->ang);
        if (fn_8004C8CC(probe2, index) == 0) {
            e->ang += 1.570796327;
            {
                f64 a = e->ang;
                if (a > 3.141592654) {
                    a -= 6.283185308;
                } else if (a <= (-3.141592654)) {
                    a = 6.283185308 + a;
                }
                e->ang = a;
            }
            if (e->dead_end <= 0) {
                e->dead_end = 20;
            }
        }
    }
    set_enemy_trans(e, 1.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}
#pragma opt_propagation reset

/* move_logic07 @0x80047F9C (state 7, rat-style corner-hugging chase).  IT-flee /
 * recognized gates, face a milestone/player, then when the dead_end timer allows
 * pick a corner-avoidance heading (fn_8004CE38 route + lbl_8011C0A4 offset table),
 * normalize it, probe clearance, and count consecutive stuck frames; bail back to
 * the straight bearing after 10.  Rats (type 3) poke a walk action at the end. */
#pragma opt_propagation off
void move_logic07(s32 index)
{
    Enemy* e;
    Enemy* e0;
    s32 it = lbl_80344748;
    s32 type;
    f32 speed;
    s32 flee;
    s32 found = 0;
    f32 cand;
    f32 probe[3];
    u8 unusedA[20];
    f32 d1;
    f32 d2;
    u8 unusedB[16];

    e0 = &gEnemies[index];
    type = e0->type;
    e = e0;
    speed = lbl_80250E40[type];
    if (it < 0) {
        flee = 0;
    } else {
        if (gEnemies[it].state != ACTIVE) {
            flee = 0;
        } else if (gEnemies[it].actual_dist > e0->sight) {
            flee = 0;
        } else if (index == it || e0->birth_style != 0 || e0->dead_end > 0) {
            goto flee_zero07;
        } else {
            f32 dx = gEnemies[it].objgrp.worldmat[3][0] - e0->objgrp.worldmat[3][0];
            f32 dy = gEnemies[it].objgrp.worldmat[3][1] - e0->objgrp.worldmat[3][1];
            f32 dz = gEnemies[it].objgrp.worldmat[3][2] - e0->objgrp.worldmat[3][2];
            if (dx * dx + dy * dy + dz * dz < 100.0) {
                flee = -1;
            } else {
            flee_zero07:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    {
        s16 c = e->closest;
        f32 f;
        if (c >= 0) {
            if (gPlayers[c].field_A1C > 2) {
                f = get_yaw(gPlayers[c].mikey_worldmat[3], &e->objgrp.worldmat[3][0]);
            } else {
                f = get_yaw(gPlayers[c].pos, &e->objgrp.worldmat[3][0]);
            }
        } else {
            f = e->ang;
        }
        lbl_80344720 = f;
    }
    if (e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    if (e->dead_end <= 0) {
        if (e->coll_pnum >= 0) {
            cand = lbl_80344720;
        } else if (e->area == 1) {
            s32 col;
            cand = lbl_80344720;
            col = e->collided;
            if (e->route == 0) {
                e->route = fn_8004CE38(e);
            }
            if (e->route > 0) {
                cand = cand + lbl_8011C0A4[col];
            } else {
                cand = cand - lbl_8011C0A4[col];
            }
        } else if (e->coll_ip != 0 || e->coll_enenum >= 0) {
            s32 col2;
            cand = e->ang;
            col2 = e->collided;
            if (e->route > 0) {
                cand = cand + lbl_8011C0A4[col2];
            } else {
                cand = cand - lbl_8011C0A4[col2];
            }
        } else {
            cand = lbl_80344720;
        }
        {
            f64 a;
            if (cand > 3.141592654) {
                a = cand - 6.283185308;
            } else if (cand <= -3.141592654) {
                a = 6.283185308 + cand;
            } else {
                a = cand;
            }
            cand = a;
        }
        probe[0] = e->objgrp.worldmat[3][0];
        probe[1] = e->objgrp.worldmat[3][1];
        probe[2] = e->objgrp.worldmat[3][2];
        probe[1] += 0.1 + e->rad;
        probe[0] += speed * sin(cand);
        probe[2] += speed * cos(cand);
        d1 = e->ang - e->angbak;
        *(u32*)&d1 &= 0x7FFFFFFF;
        if ((d1 > 0.034906585044444445
             && ((d2 = cand - e->angbak), (*(u32*)&d2 &= 0x7FFFFFFF),
                 d2 <= 0.034906585044444445))
            || fn_8004C8CC(probe, index) == 0) {
            found = 1;
            e->stuck_count++;
        } else {
            e->stuck_count = 0;
        }
        if (e->stuck_count > 10) {
            cand = lbl_80344720;
            e->ang = cand;
        }
        if (found == 0) {
            e->angbak = e->ang;
            e->ang = cand;
        }
    } else {
        cand = e->ang;
    }
    set_enemy_trans(e, 1.0f, cand);
    if (found == 0 || cand == lbl_80344720) {
        e->pyr[1] = turn_enemy_ang(e, cand);
    }
    do_enemy_move(index);
    if (e->type == E_RAT && e->daction == 0) {
        RequestEnemyAction(e, 3);
    }
}
#pragma opt_propagation reset

/* move_logic08 @0x80048408 (state 8, guard/warlock corner-hug chase).  Sibling of
 * move_logic07 but with a guard target: fn_80051568 refreshes guard_closest, which
 * (when >=0) overrides the milestone bearing with the guarded item's heading.  When
 * cornered it also validates the collided item (a live spawner of type 2) before
 * choosing a corner-avoidance offset from lbl_8011C084. */
#pragma opt_propagation off
void move_logic08(s32 index)
{
    Enemy* e;
    Enemy* e0;
    s32 it = lbl_80344748;
    s32 type;
    f32 speed;
    s32 flee;
    s32 found = 0;
    f32 cand;
    f32 probe[3];
    u8 unusedA[20];
    f32 d1;
    f32 d2;
    u8 unusedB[16];

    e0 = &gEnemies[index];
    type = e0->type;
    e = e0;
    speed = lbl_80250E40[type];
    if (it < 0) {
        flee = 0;
    } else {
        if (gEnemies[it].state != ACTIVE) {
            flee = 0;
        } else if (gEnemies[it].actual_dist > e0->sight) {
            flee = 0;
        } else if (index == it || e0->birth_style != 0 || e0->dead_end > 0) {
            goto flee_zero08;
        } else {
            f32 dx = gEnemies[it].objgrp.worldmat[3][0] - e0->objgrp.worldmat[3][0];
            f32 dy = gEnemies[it].objgrp.worldmat[3][1] - e0->objgrp.worldmat[3][1];
            f32 dz = gEnemies[it].objgrp.worldmat[3][2] - e0->objgrp.worldmat[3][2];
            if (dx * dx + dy * dy + dz * dz < 100.0) {
                flee = -1;
            } else {
            flee_zero08:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    fn_80051568(index);
    if (e->guard_closest >= 0) {
        lbl_80344720 = get_yaw((f32*)((u8*)sItems + e->guard_closest * 240 + 52),
                               &e->objgrp.worldmat[3][0]);
    } else {
        s16 c = e->closest;
        f32 f;
        if (c >= 0) {
            if (gPlayers[c].field_A1C > 2) {
                f = get_yaw(gPlayers[c].mikey_worldmat[3], &e->objgrp.worldmat[3][0]);
            } else {
                f = get_yaw(gPlayers[c].pos, &e->objgrp.worldmat[3][0]);
            }
        } else {
            f = e->ang;
        }
        lbl_80344720 = f;
    }
    if (e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    if (e->dead_end <= 0) {
        if (e->coll_pnum >= 0) {
            s16 c = e->closest;
            f32 f;
            if (c >= 0) {
                if (gPlayers[c].field_A1C > 2) {
                    f = get_yaw(gPlayers[c].mikey_worldmat[3], &e->objgrp.worldmat[3][0]);
                } else {
                    f = get_yaw(gPlayers[c].pos, &e->objgrp.worldmat[3][0]);
                }
            } else {
                f = e->ang;
            }
            cand = f;
            lbl_80344720 = f;
        } else {
            u8* ip = (u8*)e->coll_ip;
            s32 valid;
            if (ip == 0) {
                valid = 0;
            } else if (((Item *)ip)->active == -1 || **(s32**)ip != 2
                       || ((Item *)ip)->minoff != 0) {
                valid = 0;
            } else {
                valid = -1;
            }
            if (valid != 0) {
                cand = lbl_80344720;
            } else if (e->area == 1) {
                s32 col;
                cand = lbl_80344720;
                col = e->collided;
                if (e->route == 0) {
                    e->route = fn_8004CE38(e);
                }
                if (e->route > 0) {
                    cand = cand + lbl_8011C084[col];
                } else {
                    cand = cand - lbl_8011C084[col];
                }
            } else if (ip != 0 || e->coll_enenum >= 0) {
                s32 col2;
                cand = e->ang;
                col2 = e->collided;
                if (e->route > 0) {
                    cand = cand + lbl_8011C084[col2];
                } else {
                    cand = cand - lbl_8011C084[col2];
                }
            } else {
                cand = lbl_80344720;
            }
        }
        {
            f64 a;
            if (cand > 3.141592654) {
                a = cand - 6.283185308;
            } else if (cand <= -3.141592654) {
                a = 6.283185308 + cand;
            } else {
                a = cand;
            }
            cand = a;
        }
        probe[0] = e->objgrp.worldmat[3][0];
        probe[1] = e->objgrp.worldmat[3][1];
        probe[2] = e->objgrp.worldmat[3][2];
        probe[1] += 0.1 + e->rad;
        probe[0] += speed * sin(cand);
        probe[2] += speed * cos(cand);
        d1 = e->ang - e->angbak;
        *(u32*)&d1 &= 0x7FFFFFFF;
        if ((d1 > 0.034906585044444445
             && ((d2 = cand - e->angbak), (*(u32*)&d2 &= 0x7FFFFFFF),
                 d2 <= 0.034906585044444445))
            || fn_8004C8CC(probe, index) == 0) {
            found = 1;
            e->stuck_count++;
        } else {
            e->stuck_count = 0;
        }
        if (e->stuck_count > 10) {
            cand = lbl_80344720;
            e->ang = cand;
        }
        if (found == 0) {
            e->angbak = e->ang;
            e->ang = cand;
        }
    } else {
        cand = e->ang;
    }
    set_enemy_trans(e, 1.0f, cand);
    if (found == 0 || cand == lbl_80344720) {
        e->pyr[1] = turn_enemy_ang(e, cand);
    }
    do_enemy_move(index);
}
#pragma opt_propagation reset

/* Xbox retains set_turn_to_ms(Enemy*) with mpos, trans and dtrans as
 * three 12-byte vectors. Both GC route tests inline that same operation.
 * These real vectors replace the old 24-byte reservations at each callsite;
 * only their X/Z components participate in the horizontal distance test.
 * Preserve float rounding both before and after double heading wrapping. */
static inline int set_turn_to_ms(Enemy* e)
{
    f32 mpos[3];
    f32 trans[3];
    f32 len_r;
    f32 dtrans[3];
    f32 right_angle;
    f32 left_angle;

    GetMilestonePos(e->plr_ms, mpos);
    dtrans[0] = e->objgrp.worldmat[3][0] - mpos[0];
    dtrans[2] = e->objgrp.worldmat[3][2] - mpos[2];
    {
        f64 a = (f32)(0.5235987756666667 + e->pyr[1]);
        if (a > 3.141592654) {
            a -= 6.283185308;
        } else if (a <= -3.141592654) {
            a = 6.283185308 + a;
        }
        right_angle = a;
    }
    trans[0] = sin(right_angle);
    trans[2] = cos(right_angle);
    trans[0] += dtrans[0];
    trans[2] += dtrans[2];
    len_r = fqdist(trans[0], trans[2]);
    {
        f64 a = (f32)(e->pyr[1] - 0.5235987756666667);
        if (a > 3.141592654) {
            a -= 6.283185308;
        } else if (a <= -3.141592654) {
            a = 6.283185308 + a;
        }
        left_angle = a;
    }
    trans[0] = sin(left_angle);
    trans[2] = cos(left_angle);
    trans[0] += dtrans[0];
    trans[2] += dtrans[2];
    return fqdist(trans[0], trans[2]) <= len_r ? -1 : 1;
}

#pragma opt_propagation off
void move_logic10(s32 index)
{
    u8* base = (u8*)mbdesc;
    u8* tbl = (u8*)lbl_8011AF48;
    u8* e0 = base + index * 916;
    Enemy* e;
    s32 type;
    f32 speed;
    s32 flee;
    f32 probe[3];
    f32 cand;
    s32 it = lbl_80344748;
    s32 found = 0;
    f32* q;
    u8* t;
    u8* other;

    type = ((EnemyMovePage05*)e0)->enemies[0].type;
    e = ((EnemyMovePage05*)e0)->enemies;
    e0 = (u8*)((EnemyMovePage05*)e0)->enemies;
    t = base;
    t += type * 4;
    speed = *(f32*)(t + offsetof(EnemyMovePage05, speed));
    if (it < 0) {
        flee = 0;
    } else {
        other = base + it * 916;
        if (((Enemy *)(other + ENEMY_POOL_OFF))->state != ACTIVE) {
            flee = 0;
        } else if (((Enemy *)(other + ENEMY_POOL_OFF))->actual_dist > *(f32*)(e0 + offsetof(Enemy, sight))) {
            flee = 0;
        } else if (index == it || *(s16*)(e0 + offsetof(Enemy, birth_style)) != 0 || *(s32*)(e0 + offsetof(Enemy, dead_end)) > 0) {
            goto flee_zero10;
        } else {
            f32 dx = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][0] - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][0]));
            f32 dy = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][1] - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][1]));
            f32 dz = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][2] - *(f32*)(e0 + offsetof(Enemy, objgrp.worldmat[3][2]));
            if (dx * dx + dy * dy + dz * dz < 100.0) {
                flee = -1;
            } else {
            flee_zero10:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    update_enemy_milestone(e);
    switch (e->mode1) {
    case 0: {
        s32 skip;
        u8 _g1[24];
        if (*(s32*)(e0 + offsetof(Enemy, coll_pnum)) >= 0) {
            if (*(s16*)(e0 + offsetof(Enemy, algorithm)) != *(s16*)(e0 + offsetof(Enemy, prev_ai))) {
                format_brain(index);
            }
            {
                f32 f = get_face_ang((Enemy*)e0, 1);
                *(f32*)(e0 + offsetof(Enemy, ang)) = f;
            }
            *(s32*)(e0 + offsetof(Enemy, dead_end)) = 0;
            set_enemy_trans((Enemy*)e0, 1.0f, *(f32*)(e0 + offsetof(Enemy, ang)));
            {
                f32 aa = *(f32*)(e0 + offsetof(Enemy, ang));
                *(f32*)(e0 + offsetof(Enemy, pyr[1])) = turn_enemy_ang((Enemy*)e0, aa);
            }
            do_enemy_move(index);
            skip = -1;
        } else {
            skip = 0;
        }
        if (skip != 0) {
            return;
        }
        if (e->algorithm != e->prev_ai) {
            format_brain(index);
        }
        if (e->collided >= 5) {
            e->stuck_count = 0;
            e->plr_ms = gPlayers[e->closest].milestone[0];
            if (e->plr_ms >= 0) {
                e->mode1++;
                e->mode2 = 0;
                e->collided = 0;
            }
        } else {
            f32 f = get_face_ang(e, 1);
            lbl_80344720 = f;
        }
        if (e->dead_end > 0) {
            e->dead_end -= gFrameTicks;
        }
        if (e->dead_end <= 0) {
            if (e->area == 1) {
                s32 col;
                cand = lbl_80344720;
                col = e->collided;
                if (e->route == 0) {
                    e->route = fn_8004CE38(e);
                }
                if (e->route > 0) {
                    q = (f32*)(tbl + col * 4);
                    cand = cand + q[1095];
                } else {
                    q = (f32*)(tbl + col * 4);
                    cand = cand - q[1095];
                }
            } else if (e->coll_ip != 0 || e->coll_enenum >= 0) {
                s32 col2;
                cand = e->ang;
                col2 = e->collided;
                if (e->route > 0) {
                    q = (f32*)(tbl + col2 * 4);
                    cand = cand + q[1095];
                } else {
                    q = (f32*)(tbl + col2 * 4);
                    cand = cand - q[1095];
                }
            } else {
                cand = lbl_80344720;
            }
            cand = enemy_normalized_heading(cand);
            probe[0] = e->objgrp.worldmat[3][0];
            probe[1] = e->objgrp.worldmat[3][1];
            probe[2] = e->objgrp.worldmat[3][2];
            probe[1] += 0.1 + e->rad;
            probe[0] += speed * sin(cand);
            probe[2] += speed * cos(cand);
            {
                f32 d1;
                f32 d2;

                d1 = e->ang - e->angbak;
                *(u32*)&d1 &= 0x7FFFFFFF;
                if ((d1 > 0.034906585044444445
                     && ((d2 = cand - e->angbak), (*(u32*)&d2 &= 0x7FFFFFFF),
                         d2 <= 0.034906585044444445))
                    || fn_8004C8CC(probe, index) == 0) {
                    found = 1;
                    e->stuck_count++;
                } else {
                    e->stuck_count = 0;
                }
            }
            if (e->stuck_count > 10) {
                cand = lbl_80344720;
                e->ang = cand;
            }
            if (found == 0) {
                e->angbak = e->ang;
                e->ang = cand;
            }
        } else {
            cand = e->ang;
        }
        set_enemy_trans(e, 1.0f, cand);
        if (found == 0 || cand == lbl_80344720) {
            e->pyr[1] = turn_enemy_ang(e, cand);
        }
        do_enemy_move(index);
        break;
    }
    case 1: {
        s32 skip;
        if (*(s32*)(e0 + offsetof(Enemy, coll_pnum)) >= 0) {
            if (*(s16*)(e0 + offsetof(Enemy, algorithm)) != *(s16*)(e0 + offsetof(Enemy, prev_ai))) {
                format_brain(index);
            }
            {
                f32 f = get_face_ang((Enemy*)e0, 1);
                *(f32*)(e0 + offsetof(Enemy, ang)) = f;
            }
            *(s32*)(e0 + offsetof(Enemy, dead_end)) = 0;
            set_enemy_trans((Enemy*)e0, 1.0f, *(f32*)(e0 + offsetof(Enemy, ang)));
            {
                f32 aa = *(f32*)(e0 + offsetof(Enemy, ang));
                *(f32*)(e0 + offsetof(Enemy, pyr[1])) = turn_enemy_ang((Enemy*)e0, aa);
            }
            do_enemy_move(index);
            skip = -1;
        } else {
            skip = 0;
        }
        if (skip != 0) {
            return;
        }
        if (e->algorithm != e->prev_ai) {
            format_brain(index);
        }
        {
            s32 ms = e->plr_ms;
            if (ms >= 0) {
                f32 b1[3];
                if (e->stuck_count >= 5) {
                    e->plr_ms = find_neighbor_milestone(ms, ++e->mode2);
                    if (e->plr_ms < 0) {
                        e->plr_ms = ms;
                    }
                    e->stuck_count = 0;
                    e->collided = 0;
                }
                GetMilestonePos(e->plr_ms, b1);
                lbl_80344720 = get_yaw(b1, &e->objgrp.worldmat[3][0]);
            } else {
                s32 got = 0;
                if (--e->mode2 > 0) {
                    e->plr_ms = find_neighbor_milestone(e->plr_ms, e->mode2);
                    if (e->plr_ms >= 0) {
                        got = 1;
                    }
                    if (got != 0) {
                        f32 b2[3];
                        e->stuck_count = 0;
                        e->collided = 0;
                        GetMilestonePos(e->plr_ms, b2);
                        lbl_80344720 = get_yaw(b2, &e->objgrp.worldmat[3][0]);
                    }
                }
                if (got == 0) {
                    {
                        f32 f = get_face_ang(e, 1);
                        lbl_80344720 = f;
                    }
                    e->mode1 = 0;
                    e->mode2 = 0;
                    e->stuck_count = 0;
                    e->collided = 0;
                }
            }
        }
        if (e->dead_end > 0) {
            e->dead_end -= gFrameTicks;
        }
        if (e->dead_end <= 0) {
            if (e->plr_ms >= 0) {
                s32 col;
                cand = lbl_80344720;
                col = e->collided;
                if (e->route == 0) {
                    e->route = set_turn_to_ms(e);
                }
                if (e->route > 0) {
                    q = (f32*)(tbl + col * 4);
                    cand = cand + q[1095];
                } else {
                    q = (f32*)(tbl + col * 4);
                    cand = cand - q[1095];
                }
            } else if (e->area == 1) {
                s32 col;
                cand = lbl_80344720;
                col = e->collided;
                if (e->route == 0) {
                    e->route = fn_8004CE38(e);
                }
                if (e->route > 0) {
                    q = (f32*)(tbl + col * 4);
                    cand = cand + q[1095];
                } else {
                    q = (f32*)(tbl + col * 4);
                    cand = cand - q[1095];
                }
            } else if (e->coll_ip != 0 || e->coll_enenum >= 0) {
                s32 col2;
                cand = e->ang;
                col2 = e->collided;
                if (e->route > 0) {
                    q = (f32*)(tbl + col2 * 4);
                    cand = cand + q[1095];
                } else {
                    q = (f32*)(tbl + col2 * 4);
                    cand = cand - q[1095];
                }
            } else {
                cand = lbl_80344720;
            }
            cand = enemy_normalized_heading(cand);
            probe[0] = e->objgrp.worldmat[3][0];
            probe[1] = e->objgrp.worldmat[3][1];
            probe[2] = e->objgrp.worldmat[3][2];
            probe[1] += 0.1 + e->rad;
            probe[0] += speed * sin(cand);
            probe[2] += speed * cos(cand);
            {
                f32 d3;
                f32 d4;

                d3 = e->ang - e->angbak;
                *(u32*)&d3 &= 0x7FFFFFFF;
                if ((d3 > 0.034906585044444445
                     && ((d4 = cand - e->angbak), (*(u32*)&d4 &= 0x7FFFFFFF),
                         d4 <= 0.034906585044444445))
                    || fn_8004C8CC(probe, index) == 0) {
                    found = 1;
                    e->stuck_count++;
                } else {
                    e->stuck_count = 0;
                }
            }
            if (e->stuck_count > 10) {
                cand = lbl_80344720;
                e->ang = cand;
            }
            if (found == 0) {
                e->angbak = e->ang;
                e->ang = cand;
            }
        } else {
            cand = e->ang;
        }
        set_enemy_trans(e, 1.0f, cand);
        if (found == 0 || cand == lbl_80344720) {
            e->pyr[1] = turn_enemy_ang(e, cand);
        }
        do_enemy_move(index);
        break;
    }
    default: {
        if (e->algorithm != e->prev_ai) {
            format_brain(index);
        }
        if (e->plr_ms >= 0) {
            if (e->stuck_count < 5) {
                f32 b4[3];
                GetMilestonePos(e->plr_ms, b4);
                lbl_80344720 = get_yaw(b4, &e->objgrp.worldmat[3][0]);
            } else {
                s32 v;
                e->ms_idx++;
                if (e->ms_idx > e->max_msidx
                    || (v = gPlayers[e->closest].milestone[e->ms_idx]) < 0) {
                    e->ms_idx = 0;
                    e->max_msidx = 4;
                    e->plr_ms = -1;
                    {
                        f32 f = get_face_ang(e, 1);
                        lbl_80344720 = f;
                    }
                } else {
                    e->plr_ms = v;
                    e->stuck_count = 0;
                }
            }
        } else {
            if (e->stuck_count >= 5) {
                e->plr_ms = gPlayers[e->closest].milestone[e->ms_idx];
                if (e->plr_ms >= 0) {
                    f32 b5[3];
                    GetMilestonePos(e->plr_ms, b5);
                    lbl_80344720 = get_yaw(b5, &e->objgrp.worldmat[3][0]);
                } else {
                    f32 f = get_face_ang(e, 1);
                    lbl_80344720 = f;
                }
            } else {
                f32 f = get_face_ang(e, 1);
                lbl_80344720 = f;
            }
        }
        if (e->dead_end > 0) {
            e->dead_end -= gFrameTicks;
        }
        if (e->dead_end <= 0) {
            if (e->plr_ms >= 0) {
                s32 col;
                cand = lbl_80344720;
                col = e->collided;
                if (e->route == 0) {
                    e->route = set_turn_to_ms(e);
                }
                if (e->route > 0) {
                    q = (f32*)(tbl + col * 4);
                    cand = cand + q[1095];
                } else {
                    q = (f32*)(tbl + col * 4);
                    cand = cand - q[1095];
                }
            } else if (e->area == 1) {
                s32 col;
                cand = lbl_80344720;
                col = e->collided;
                if (e->route == 0) {
                    e->route = fn_8004CE38(e);
                }
                if (e->route > 0) {
                    q = (f32*)(tbl + col * 4);
                    cand = cand + q[1095];
                } else {
                    q = (f32*)(tbl + col * 4);
                    cand = cand - q[1095];
                }
            } else if (e->coll_ip != 0 || e->coll_enenum >= 0) {
                s32 col2;
                cand = e->ang;
                col2 = e->collided;
                if (e->route > 0) {
                    q = (f32*)(tbl + col2 * 4);
                    cand = cand + q[1095];
                } else {
                    q = (f32*)(tbl + col2 * 4);
                    cand = cand - q[1095];
                }
            } else {
                cand = lbl_80344720;
            }
            cand = enemy_normalized_heading(cand);
            probe[0] = e->objgrp.worldmat[3][0];
            probe[1] = e->objgrp.worldmat[3][1];
            probe[2] = e->objgrp.worldmat[3][2];
            probe[1] += 0.1 + e->rad;
            probe[0] += speed * sin(cand);
            probe[2] += speed * cos(cand);
            {
                f32 d5;
                f32 d6;
                u8 _g4[120];
                d5 = e->ang - e->angbak;
                *(u32*)&d5 &= 0x7FFFFFFF;
                if ((d5 > 0.034906585044444445
                     && ((d6 = cand - e->angbak), (*(u32*)&d6 &= 0x7FFFFFFF),
                         d6 <= 0.034906585044444445))
                    || fn_8004C8CC(probe, index) == 0) {
                    found = 1;
                    e->stuck_count++;
                } else {
                    e->stuck_count = 0;
                }
            }
            if (e->stuck_count > 10) {
                cand = lbl_80344720;
                e->ang = cand;
            }
            if (found == 0) {
                e->angbak = e->ang;
                e->ang = cand;
            }
        } else {
            cand = e->ang;
        }
        set_enemy_trans(e, 1.0f, cand);
        if (found == 0 || cand == lbl_80344720) {
            e->pyr[1] = turn_enemy_ang(e, cand);
        }
        do_enemy_move(index);
        break;
    }
    }
}
#pragma opt_propagation reset

/* move_logic12 @0x80049A1C (state 12, maggot-egg tether).  Shares the IT-flee /
 * chase gate, then runs a small generator-egg state machine: snap to the dest,
 * flag the egg, and hatch back when the egg reports ready. */
#pragma opt_propagation off
void move_logic12(s32 index)
{
    u8* base = (u8*)mbdesc;
    Enemy* e;
    struct Item* gen;
    s32 flee;
    f32 a;
    u8* p;
    s32 it;
    u8 unused[16];

    p = base + index * 916;
    it = lbl_80344748;
    gen = ((Enemy *)(p + ENEMY_POOL_OFF))->generator;
    p += ENEMY_POOL_OFF;
    e = (Enemy*)(u8*)p;
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = base + it * 916;
        if (((Enemy *)(other + ENEMY_POOL_OFF))->state != ACTIVE) {
            flee = 0;
        } else if (((Enemy *)(other + ENEMY_POOL_OFF))->actual_dist >
                   ((Enemy *)p)->sight) {
            flee = 0;
        } else if (index == it || ((Enemy *)p)->birth_style != 0 ||
                   ((Enemy *)p)->dead_end > 0) {
            goto flee_zero;
        } else {
            f32 dx = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][0] -
                     ((Enemy *)p)->objgrp.worldmat[3][0];
            f32 dy = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][1] -
                     ((Enemy *)p)->objgrp.worldmat[3][1];
            f32 dz = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][2] -
                     ((Enemy *)p)->objgrp.worldmat[3][2];
            if (dx * dx + dy * dy + dz * dz < 100.0) {
                flee = -1;
            } else {
            flee_zero:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->mode1 >= 2 || gen == 0) {
        e->algorithm = 0;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->closest >= 0) {
        if (gPlayers[e->closest].field_A1C > 2) {
            a = get_yaw(gPlayers[e->closest].mikey_worldmat[3], &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(gPlayers[e->closest].pos, &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    switch (e->mode1) {
    case 0:
        if (gen != 0) {
            *((u8*)gen + 230) |= e->flag1;
            e->objgrp.worldmat[3][0] = e->dest[0];
            e->objgrp.worldmat[3][1] = e->dest[1];
            e->objgrp.worldmat[3][2] = e->dest[2];
            fn_8005A65C(&e->objgrp.worldmat[0][0], e->coll_offset);
            e->mode1 = 1;
        }
        break;
    case 1:
        if (gen != 0 && *((u8*)gen + 230) == 7) {
            f32 z = 0.0f;
            e->dest[0] = z;
            e->dest[1] = z;
            e->dest[2] = z;
            e->mode1 = 2;
        }
        break;
    default:
        e->mode1 = 2;
        break;
    }
}
#pragma opt_propagation reset

/* move_logic13 @0x80049C70 (state 13, zombie chain-follower).  Follows its
 * prev_enemy link toward the chain head; if the debug flag is set it dumps the
 * prev/next indices.  Each frame it faces its parent, measures the gap (inline
 * sqrt), and after 180 frames "lost" (parent too far) snaps the whole downstream
 * chain to the idle algorithm and cuts itself loose. */
#pragma opt_propagation off
void move_logic13(s32 index)
{
    u8* base = (u8*)mbdesc;
    Enemy* e;
    struct Item* gen;
    u8* p;
    s32 it;
    s32 flee;
    u8 _pad[32];

    p = base + index * 916;
    it = lbl_80344748;
    gen = ((Enemy *)(p + ENEMY_POOL_OFF))->generator;
    p += ENEMY_POOL_OFF;
    e = (Enemy*)(u8*)p;
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = base + it * 916;
        if (((Enemy *)(other + ENEMY_POOL_OFF))->state != ACTIVE) {
            flee = 0;
        } else if (((Enemy *)(other + ENEMY_POOL_OFF))->actual_dist >
                   ((Enemy *)p)->sight) {
            flee = 0;
        } else if (index == it || ((Enemy *)p)->birth_style != 0 ||
                   ((Enemy *)p)->dead_end > 0) {
            goto flee_zero13;
        } else {
            f32 dx = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][0] -
                     ((Enemy *)p)->objgrp.worldmat[3][0];
            f32 dy = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][1] -
                     ((Enemy *)p)->objgrp.worldmat[3][1];
            f32 dz = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][2] -
                     ((Enemy *)p)->objgrp.worldmat[3][2];
            if (dx * dx + dy * dy + dz * dz < 100.0) {
                flee = -1;
            } else {
            flee_zero13:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (gControllerButtons & 0x10) {
        if (index == e->prev_enemy) {
            ErrorPrintf("E%02X==PREV", index);
        }
        if (index == e->next_enemy) {
            ErrorPrintf("E%02X==NEXT", index);
        }
        if (e->prev_enemy >= 0 && e->prev_enemy == e->next_enemy) {
            ErrorPrintf("E%02X: prev==next (%02X)", index, e->prev_enemy);
        }
    }
    if (gen == 0 || e->prev_enemy < 0) {
        e->algorithm = 7;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    {
        Enemy* prev;
        f32 dy;
        f32 dx;
        f32 dz;
        f32 dist2;

        p = base + e->prev_enemy * 916;
        {
            f32* ysrc = (f32*)(p + 3660);
            prev = (Enemy*)(p += 3608);
            e->ang = get_yaw(ysrc, &e->objgrp.worldmat[3][0]);
        }
        set_enemy_trans(e, 1.0f, e->ang);
        e->pyr[1] = turn_enemy_ang(e, e->ang);
        do_enemy_move(index);
        dx = prev->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
        dy = prev->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
        dz = prev->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
        dist2 = dx * dx + dy * dy + dz * dz;
        if (dist2 > 0.0f) {
            volatile f32 tmp;
            f64 y = __frsqrte(dist2);
            y = 0.5 * y * (3.0 - y * y * dist2);
            y = 0.5 * y * (3.0 - y * y * dist2);
            y = 0.5 * y * (3.0 - y * y * dist2);
            tmp = (f32)(dist2 * (0.5 * y * (3.0 - y * y * dist2)));
            dist2 = tmp;
        }
        if (dist2 >= 15.0) {
            e->lost += gFrameTicks;
            if (e->lost >= 180) {
                s32 n;
                Enemy* q;
                q = (Enemy*)(base + e->prev_enemy * 916);
                q = (Enemy*)((u8*)q + 3608);
                do {
                    q->algorithm = 7;
                    q->old_ai = 7;
                    q->next_enemy = -1;
                    n = q->prev_enemy;
                    q->prev_enemy = -1;
                    q = (Enemy*)(base + n * 916);
                    q = (Enemy*)((u8*)q + 3608);
                } while (n >= 0);
                e->prev_enemy = -1;
            }
        }
    }
}
#pragma opt_propagation reset

/* move_logic14 @0x80049FD4 (state 14, plague zig-zag skirmisher).  If a player is
 * within 8 units it switches to the chase algorithm; otherwise it strafes: swing
 * the heading +/-pi/2 on a count timer, and once mode1 has built up (and the
 * drift has opened past pi/2) re-seed the strafe with a flag2-scaled offset. */
void move_logic14(int index)
{
    Enemy* e;
    f32 face;
    f32 drift;
    f32 diff;

    e = &gEnemies[index];
    if (FoundSuicideBomber(index) != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->closest >= 0 && e->close_dist <= 8.0) {
        e->algorithm = 0;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    face = get_face_ang(e, 1);
    lbl_80344720 = face;
    if ((e->count -= gFrameTicks) <= 0) {
        e->flag1 = -e->flag1;
        if (e->flag1 > 0) {
            e->ang += 1.570796327;
        } else {
            e->ang = e->ang - 1.570796327;
        }
        {
            f64 a = e->ang;
            if (a > 3.141592654) {
                a -= 6.283185308;
            } else if (a <= (-3.141592654)) {
                a = 6.283185308 + a;
            }
            e->ang = a;
        }
        e->count += 45;
        e->mode1++;
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    set_enemy_trans(e, 1.0f, e->ang);
    do_enemy_move(index);
    diff = lbl_80344720 - e->ang;
    {
        f64 d;
        if (diff > 3.141592654) {
            d = diff - 6.283185308;
        } else if (diff <= (-3.141592654)) {
            d = 6.283185308 + diff;
        } else {
            d = diff;
        }
        drift = d;
    }
    if (e->counter1 <= 0
        && (e->dead_end > 0
            || (e->mode1 >= 4 && fabsf_(drift) > 1.570796327))) {
        f32 scale = (f32)(0.26179938783333334 * (f64)e->flag2 + 0.7853981635);
        if (e->mode1 >= 4) {
            e->flag1 = -e->flag1;
        }
        e->dead_end = 0;
        e->ang = get_yaw(gPlayers[e->closest].pos, &e->objgrp.worldmat[3][0]);
        if (e->flag1 > 0) {
            e->ang = e->ang + scale;
        } else {
            e->ang = e->ang - scale;
        }
        {
            f64 a = e->ang;
            if (a > 3.141592654) {
                a -= 6.283185308;
            } else if (a <= (-3.141592654)) {
                a = 6.283185308 + a;
            }
            e->ang = a;
        }
        e->count = 0;
        e->mode1 = 0;
        e->flag2++;
        e->counter1 = 30;
    } else {
        if (e->flag2 > 0) {
            e->flag2--;
        }
        e->counter1 -= gFrameTicks;
    }
}

/* move_logic15 @0x8004A430 (state 15, wolf prowl patrol).  When it loses the
 * player it walks a linked prowl-node path: on entry (mode1==0) it scans the node
 * table for the nearest active node, then each frame faces the current node and
 * advances to the node's link when it arrives.  A player inside 0.8*sight (and its
 * milestone likewise) snaps it back to the chase algorithm. */
void move_logic15(int index)
{
    Enemy* e;
    /* Xbox records both as float[3]; GC reuses the same delta across tests. */
    f32 pos[3];
    f32 tpos[3];
    f32 d;

    e = &gEnemies[index];
    if (FoundSuicideBomber(index) != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->closest >= 0 && e->close_dist <= 0.8 * e->sight) {
        tpos[0] = gPlayers[e->closest].pos[0] - e->objgrp.worldmat[3][0];
        tpos[1] = gPlayers[e->closest].pos[1] - e->objgrp.worldmat[3][1];
        tpos[2] = gPlayers[e->closest].pos[2] - e->objgrp.worldmat[3][2];
        d = fqdist(tpos[0], tpos[2]);
        if (d <= 0.8 * e->sight) {
            e->algorithm = 0;
            do_ai(index);
            e->mode1 = 0;
            return;
        }
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    switch (e->mode1) {
    case 0: {
        LookoutParam* n = (LookoutParam*)sLookoutParams;
        int i;
        int best_idx = -1;
        f32 best_dist;

        pos[0] = e->objgrp.worldmat[3][0];
        pos[1] = e->objgrp.worldmat[3][1];
        pos[2] = e->objgrp.worldmat[3][2];
        best_dist = 100000.0f;

        for (i = 0; i < sNumLookoutParams; i++, n++) {
            if (n->next >= 0) {
                tpos[0] = n->worldmat[3][0] - pos[0];
                tpos[1] = n->worldmat[3][1] - pos[1];
                tpos[2] = n->worldmat[3][2] - pos[2];
                d = enemy_distance_sqrt(tpos[0] * tpos[0] +
                                        tpos[1] * tpos[1] + tpos[2] * tpos[2]);
                if (d < best_dist) {
                    best_dist = d;
                    best_idx = i;
                }
            }
        }
        e->flag1 = best_idx;
        e->mode1 = 1;
    }
    case 1: {
        LookoutParam* n = &((LookoutParam*)sLookoutParams)[e->flag1];

        e->ang = get_yaw(n->worldmat[3], &e->objgrp.worldmat[3][0]);
        tpos[0] = n->worldmat[3][0] - e->objgrp.worldmat[3][0];
        tpos[1] = n->worldmat[3][1] - e->objgrp.worldmat[3][1];
        tpos[2] = n->worldmat[3][2] - e->objgrp.worldmat[3][2];
        if (fabsf_(tpos[1]) < 4.0 && fqdist(tpos[0], tpos[2]) < 1.0) {
            e->flag1 = n->next;
        }
        break;
    }
    default:
        break;
    }
    set_enemy_trans(e, 1.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic16 @0x8004A78C (state 16, ice leap-attacker).  Faces the target, and
 * when the player is at a shallow height difference it arms (flag1) inside 0.6*
 * sight; once armed and inside 0.8*sight it charges a leap speed off lbl_8011BF60,
 * then fires a run-attack action, aiming the leap 180deg + a ramp offset. */
void move_logic16(s32 index)
{
    s32 stuck;
    u8* base = (u8*)mbdesc;
    u8* row16;
    u8* e0;
    Enemy* e;
    s32 it;
    s32 dend;
    f32 leapspeed = 0.0f;
    s32 flee;
    f32 a;
    u8 _pad16[24];

    row16 = base + index * 916;
    dend = ((Enemy *)(row16 + ENEMY_POOL_OFF))->dead_end;
    e0 = row16 + 3608;
    e = (Enemy*)(u8*)e0;
    if (dend > 0) {
        stuck = 1;
    } else {
        stuck = 0;
    }
    it = lbl_80344748;
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = base + it * 916;
        if (((Enemy *)(other + ENEMY_POOL_OFF))->state != ACTIVE) {
            flee = 0;
        } else if (((Enemy *)(other + ENEMY_POOL_OFF))->actual_dist > ((Enemy *)e0)->sight) {
            flee = 0;
        } else if (index == it || ((Enemy *)e0)->birth_style != 0 || dend > 0) {
            goto flee_zero16;
        } else {
            f32 dx = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][0] - ((Enemy *)e0)->objgrp.worldmat[3][0];
            f32 dy = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][1] - ((Enemy *)e0)->objgrp.worldmat[3][1];
            f32 dz = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][2] - ((Enemy *)e0)->objgrp.worldmat[3][2];
            if (dx * dx + dy * dy + dz * dz < 100.0) {
                flee = -1;
            } else {
            flee_zero16:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    {
        s16 c = e->closest;
        if (c >= 0) {
            if (gPlayers[c].field_A1C > 2) {
                a = get_yaw(gPlayers[c].mikey_worldmat[3], &e->objgrp.worldmat[3][0]);
            } else {
                a = get_yaw(gPlayers[c].pos, &e->objgrp.worldmat[3][0]);
            }
        } else {
            a = e->ang;
        }
    }
    e->ang = a;
    {
    s16 c16 = e->closest;
    if (c16 >= 0) {
        u8* gp = (u8*)&gPlayers + c16 * PLAYER_STRIDE;
        f32 dvert = e->objgrp.worldmat[3][1] -
                    ((Player *)gp)->pos[1];
        if (e->visactive != 0 && dvert >= -10.0 && dvert <= 10.0) {
            if (e->flag1 == 0) {
                if (e->actual_dist <= 0.6 * e->sight) {
                    e->flag1 = 1;
                }
            } else if (e->actual_dist > 0.8 * e->sight) {
                e->flag1 = 0;
            } else if (e->dead_end > 0) {
                s32 leap = e->counter1;
                if (leap < 8) {
                    e->counter1 = leap + 1;
                    leapspeed = lbl_8011BF60[leap];
                    e->dead_end = 0;
                } else {
                    e->flag1 = 0;
                }
            }
            if (e->flag2 > 0) {
                e->flag2 -= gFrameTicks;
            } else if (e->flag1 == 0) {
                RequestEnemyAction(e, 24);
            } else {
                f32 v = 3.141592654 + e->ang + leapspeed;
                f32 la;
                {
                    f64 nv;
                    if (v > 3.141592654) {
                        nv = v - 6.283185308;
                    } else if (v <= -3.141592654) {
                        nv = 6.283185308 + v;
                    } else {
                        nv = v;
                    }
                    la = nv;
                }
                RequestEnemyAction(e, 22);
                if (e->action == 22 || e->action == 23) {
                    set_enemy_trans(e, 0.8f, la);
                }
                e->dead_end = 0;
            }
        }
    }
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    if (e->moved != 0) {
        e->dead_end = 0;
    }
    if (stuck == 0 && e->dead_end > 0) {
        e->anghit = e->ang;
        e->counter1 = 0;
    }
}

/* move_logic18 @0x8004AB20 (state 18, dog stalk-and-pounce).  Creeps to a crouch
 * (mode1 1) inside sight, holds a ready pose (RequestEnemyAction 9) until the run
 * anim fires, then charges: it ramps a leap speed off lbl_8011BF60 and after 240
 * frames of pursuit (or a player contact) turns "IT" - clearing the IT slot and
 * dealing 999 damage to the caught player. */
void move_logic18(s32 index)
{
    s32 stuck;
    Enemy* e = &gEnemies[index];
    f32 leapspeed = 0.0f;
    s32 dend = e->dead_end;
    s16 sVar1;
    f32 a;

    if (dend > 0) {
        stuck = 1;
    } else {
        stuck = 0;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->closest >= 0) {
        if (gPlayers[e->closest].field_A1C > 2) {
            a = get_yaw(gPlayers[e->closest].mikey_worldmat[3],
                        &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(gPlayers[e->closest].pos,
                        &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    switch (e->mode1) {
    default:
    case 0:
        if (e->closest >= 0 && e->actual_dist <= e->sight) {
            e->mode1++;
            e->flag1 = 60;
            e->flag2 = 0;
        }
        goto move;
    case 1:
        if (e->closest >= 0 && e->action != 4
            && (e->flag1 -= gFrameTicks) <= 0) {
            RequestEnemyAction(e, 9);
        }
        if (e->action != 4) {
            goto move;
        }
        e->mode1++;
        fn_8009DD6C(&e->objgrp.attn_pos[0]);
    case 2:
        e->flag2 += gFrameTicks;
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->area == 2 && e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    if (e->dead_end > 0) {
        s32 leap = e->counter1;
        if (leap < 16) {
            e->counter1 = leap + 1;
            leapspeed = lbl_8011BF60[leap];
            e->dead_end = 0;
        }
        e->ang = e->ang + leapspeed;
        {
            f64 av = e->ang;
            if (av > 3.141592654) {
                av -= 6.283185308;
            } else if (av <= (-3.141592654)) {
                av = 6.283185308 + av;
            }
            e->ang = av;
        }
    }
    if (e->dead_end <= 0
        || fabsf_(e->ang - e->anghit) >= 0.10471975513333334) {
        e->dead_end = 0;
        set_enemy_trans(e, 1.5f, e->ang);
    }
    }
move:
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    if (e->moved != 0) {
        e->dead_end = 0;
    }
    if ((f64)e->flag2 >= 240.0 || e->coll_pnum >= 0) {
        lbl_80344748 = -1;
        damage_enemy(e, 999.0f, -2, 1, 0, 0, 0);
    } else if (stuck == 0 && e->dead_end > 0) {
        e->anghit = e->ang;
        e->counter1 = 0;
    }
}

/* move_logic19 @0x8004AE94 (state 19, skeleton assemble-and-fight).  Wait
 * dormant until a player is in sight, then rise (flag1), face the closest
 * player, run a 3-phase attack cadence, and spawn bone FX on the anim frames. */
void move_logic19(s32 index)
{
    Enemy* e = &gEnemies[index];
    animinfo* ai = &e->atree.animinfo;
    s32 mode;
    f32 a;

    if (e->flag1 == 0 && e->closest >= 0 && e->actual_dist < e->sight
        && e->visactive != 0) {
        e->flag1 = 1;
        fn_8009DDCC(&e->objgrp.worldmat[3][0]);
    }
    if (e->flag1 == 0) {
        ai->active = 0;
        return;
    }
    ai->active = 1;
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->closest >= 0) {
        if (gPlayers[e->closest].field_A1C > 2) {
            a = get_yaw(gPlayers[e->closest].mikey_worldmat[3], &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(gPlayers[e->closest].pos, &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    mode = e->mode1;
    switch (mode) {
    case 0:
    default:
        e->counter1 = 1;
        e->mode1++;
    case 1:
        RequestEnemyAction(e, 3);
        if (e->action == 3) {
            e->counter1 = RandInt(90) + 30;
            e->mode1++;
        }
        if (e->mode1 != 2) {
            goto move;
        }
        break;
    case 2:
        break;
    }
    if (e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    if (e->dead_end <= 0) {
        set_enemy_trans(e, 1.0f, e->ang);
    }
    if ((e->counter1 -= gFrameTicks) <= 0 && e->daction == 3) {
        e->daction = 0;
    }
    if ((e->dead_end > 0 && e->area == 1) || e->action == 0) {
        e->mode1 = 0;
    }
move:
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    if (e->action == 3) {
        if (e->dead_end > 0 && e->counter1 > 1) {
            e->counter1 = 1;
        }
        if ((e->prev_frame < 10.0 && ai->frame >= 10.0)
            || (e->prev_frame < 25.0 && ai->frame >= 25.0)) {
            fn_8009E03C(e);
        }
    }
    if ((e->action == 12 || e->action == 14 || e->action == 16)
        && e->prev_frame < 2.0 && ai->frame >= 2.0) {
        fn_8009DD9C(&e->objgrp.worldmat[3][0]);
    }
    e->prev_frame = ai->frame;
}

/* move_logic20 @0x8004B19C (state 20, ghost flee-drift).  Sibling of move_logic07
 * but it always heads 180deg away from the target (pi + the facing bearing) and
 * corner-hugs that flee heading, counting stuck frames and snapping back to the
 * straight flee bearing after 10. */
#pragma opt_propagation off
void move_logic20(s32 index)
{
    Enemy* e = &gEnemies[index];
    s32 found = 0;
    u8* tbl = (u8*)lbl_8011AF48;
    f32 speed = lbl_80250E40[e->type];
    f32 cand;
    f32* q;
    f32 probe[3];
    f32 d1;
    f32 d2;
    s32 col;
    s32 col2;
    u8 unusedB[16];

    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    {
        s16 c = e->closest;
        f32 f;
        if (c >= 0) {
            if (gPlayers[c].field_A1C > 2) {
                f = get_yaw(gPlayers[c].mikey_worldmat[3], &e->objgrp.worldmat[3][0]);
            } else {
                f = get_yaw(gPlayers[c].pos, &e->objgrp.worldmat[3][0]);
            }
        } else {
            f = e->ang;
        }
        lbl_80344720 = f;
    }
    if (e->dead_end > 0) {
        e->dead_end -= gFrameTicks;
    }
    if (e->dead_end <= 0) {
        if (e->coll_pnum >= 0) {
            cand = 3.141592654 + lbl_80344720;
        } else if (e->area == 1) {
            cand = 3.141592654 + lbl_80344720;
            col = e->collided;
            if (e->route == 0) {
                e->route = fn_8004CE38(e);
            }
            if (e->route > 0) {
                q = (f32*)(tbl + col * 4);
                cand = cand + q[1087];
            } else {
                q = (f32*)(tbl + col * 4);
                cand = cand - q[1087];
            }
        } else if (e->coll_ip != 0 || e->coll_enenum >= 0) {
            cand = e->ang;
            col2 = e->collided;
            if (e->route > 0) {
                q = (f32*)(tbl + col2 * 4);
                cand = cand + q[1087];
            } else {
                q = (f32*)(tbl + col2 * 4);
                cand = cand - q[1087];
            }
        } else {
            cand = 3.141592654 + lbl_80344720;
        }
        {
            f64 a;
            if (cand > 3.141592654) {
                a = cand - 6.283185308;
            } else if (cand <= -3.141592654) {
                a = 6.283185308 + cand;
            } else {
                a = cand;
            }
            cand = a;
        }
        probe[0] = e->objgrp.worldmat[3][0];
        probe[1] = e->objgrp.worldmat[3][1];
        probe[2] = e->objgrp.worldmat[3][2];
        probe[1] += 0.1 + e->rad;
        probe[0] += speed * sin(cand);
        probe[2] += speed * cos(cand);
        d1 = e->ang - e->angbak;
        *(u32*)&d1 &= 0x7FFFFFFF;
        if ((d1 > 0.034906585044444445
             && ((d2 = cand - e->angbak), (*(u32*)&d2 &= 0x7FFFFFFF),
                 d2 <= 0.034906585044444445))
            || fn_8004C8CC(probe, index) == 0) {
            found = 1;
            e->stuck_count++;
        } else {
            e->stuck_count = 0;
        }
        if (e->stuck_count > 10) {
            f64 sum = 3.141592654 + lbl_80344720;
            f64 a = (f32)sum;
            if (a > 3.141592654) {
                a -= 6.283185308;
            } else if (a <= -3.141592654) {
                a = 6.283185308 + a;
            }
            cand = a;
            e->ang = cand;
        }
        if (found == 0) {
            e->angbak = e->ang;
            e->ang = cand;
        }
    } else {
        cand = e->ang;
    }
    set_enemy_trans(e, 2.0f, cand);
    if (found != 0) {
        f64 w = 3.141592654 + lbl_80344720;
        if (w > 3.141592654) {
            w = w - 6.283185308;
        } else if (w <= -3.141592654) {
            w = 6.283185308 + w;
        }
        if (cand != w) {
            goto skip20;
        }
    }
    e->pyr[1] = turn_enemy_ang(e, cand);
skip20:
    do_enemy_move(index);
}
#pragma opt_propagation reset

/* move_logic21 @0x8004B5AC (state 21, acid-splat flee-and-face).  If it hasn't
 * recognized a target yet, bounce to a wander algorithm; otherwise ramp a
 * retreat speed, face away from the closest player (facing + pi), normalize,
 * accelerate + turn + move, and refresh the corner state. */
void move_logic21(s32 index)
{
    s32 stuck;
    Enemy* e = &gEnemies[index];
    f32 spd = 0.0f;
    s32 c;
    f32 face;

    if (e->dead_end > 0) {
        stuck = 1;
    } else {
        stuck = 0;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->dead_end > 0 && (c = e->counter1) < 8) {
        e->counter1 = c + 1;
        spd = lbl_8011BF60[c];
        e->dead_end = 0;
    }
    if (e->closest >= 0) {
        if (gPlayers[e->closest].field_A1C > 2) {
            face = get_yaw(gPlayers[e->closest].mikey_worldmat[3], &e->objgrp.worldmat[3][0]);
        } else {
            face = get_yaw(gPlayers[e->closest].pos, &e->objgrp.worldmat[3][0]);
        }
    } else {
        face = e->ang;
    }
    e->ang = spd + (3.141592654 + face);
    {
        f64 a;

        if ((a = e->ang) > 3.141592654) {
            a -= 6.283185308;
        } else if (a <= -3.141592654) {
            a = 6.283185308 + a;
        }
        e->ang = a;
    }
    set_enemy_trans(e, 2.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    if (e->moved != 0) {
        e->dead_end = 0;
    }
    if (stuck == 0 && e->dead_end > 0) {
        e->anghit = e->ang;
        e->counter1 = 0;
    }
}

/* move_logic22 @0x8004B788 (state 22, hand milestone-crawler).  On entry (mode1 0)
 * it scans the milestone-node network for the nearest node, then walks the linked
 * path: when it reaches a node (within 1.5) it asks fn_800511D0 for the next node,
 * ending (mode1 -1) when the path loops back on itself. */
void move_logic22(s32 index)
{
    u8* row22;
    u8* base = (u8*)mbdesc;
    u8* e0;
    Enemy* e;
    s32 it = lbl_80344748;
    s32 flee;
    u8 _pad22_hi[60];
    f32 buf1[3];
    f32 buf2[3];
    volatile f32 tmp;
    u8 _pad22_lo[12];

    e0 = (row22 = base + index * 916) + 3608;
    e = (Enemy*)(u8*)e0;
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = base + it * 916;
        if (((Enemy *)(other + ENEMY_POOL_OFF))->state != ACTIVE) {
            flee = 0;
        } else if (((Enemy *)(other + ENEMY_POOL_OFF))->actual_dist > ((Enemy *)e0)->sight) {
            flee = 0;
        } else if (index == it || ((Enemy *)e0)->birth_style != 0 || ((Enemy *)e0)->dead_end > 0) {
            goto flee_zero22;
        } else {
            f32 dx = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][0] - ((Enemy *)e0)->objgrp.worldmat[3][0];
            f32 dy = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][1] - ((Enemy *)e0)->objgrp.worldmat[3][1];
            f32 dz = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][2] - ((Enemy *)e0)->objgrp.worldmat[3][2];
            if (dx * dx + dy * dy + dz * dz < 100.0) {
                flee = -1;
            } else {
            flee_zero22:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    switch (e->mode1) {
    case 0: {
        MilestoneParam* node;
        s32 i;
        s32 best_idx = -1;
        f32 best_dist = 100000.0f;

        for (node = (MilestoneParam*)sMilestones, i = 0;
             i < sNumMilestones; i++, node++) {
            f32 dx = e->objgrp.worldmat[3][0] - node->objgrp.worldmat[3][0];
            f32 dy = e->objgrp.worldmat[3][1] - node->objgrp.worldmat[3][1];
            f32 dz = e->objgrp.worldmat[3][2] - node->objgrp.worldmat[3][2];
            f32 d;
            if ((d = dx * dx + dy * dy + dz * dz) > 0.0f) {
                f64 y = __frsqrte(d);
                y = 0.5 * y * (3.0 - y * y * d);
                y = 0.5 * y * (3.0 - y * y * d);
                y = 0.5 * y * (3.0 - y * y * d);
                tmp = (f32)(d * (0.5 * y * (3.0 - y * y * d)));
                d = tmp;
            }
            if (d < best_dist) {
                best_idx = i;
                best_dist = d;
            }
        }
        e->flag1 = best_idx;
        GetMilestonePos(e->flag1, buf1);
        e->ang = get_yaw(buf1, &e->objgrp.worldmat[3][0]);
        e->mode1 = 1;
    }
    default: {
        MilestoneParam* node = (MilestoneParam*)sMilestones;
        f32 dist;

        node += e->flag1;
        dist = fqdist(node->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0],
                          node->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2]);
        if (dist <= 1.5) {
            s32 old = e->flag1;
            e->flag1 = fn_800511D0(old, 0.17453292f);
            if (e->flag1 != old) {
                e->mode1++;
            } else {
                e->mode1 = -1;
            }
        }
        GetMilestonePos(e->flag1, buf2);
        e->ang = get_yaw(buf2, &e->objgrp.worldmat[3][0]);
        break;
    }
    case -1: {
        s16 c = e->closest;
        f32 f;
        if (c >= 0) {
            if (gPlayers[c].field_A1C > 2) {
                f = get_yaw(gPlayers[c].mikey_worldmat[3], &e->objgrp.worldmat[3][0]);
            } else {
                f = get_yaw(gPlayers[c].pos, &e->objgrp.worldmat[3][0]);
            }
        } else {
            f = e->ang;
        }
        e->ang = f;
        break;
    }
    }
    if (e->mode1 >= 0) {
        set_enemy_trans(e, 1.0f, e->ang);
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic23 @0x8004BB14 (state 17 + 23 share it, imp behaviour).  Face the
 * closest player (via a milestone position when it has one), and if the player
 * is within sight and roughly the same height, count down flag2 / poke a
 * throw action; then turn + move. */
void move_logic23(s32 index)
{
    Enemy* e = &gEnemies[index];
    f32 a;

    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->closest >= 0) {
        if (gPlayers[e->closest].field_A1C > 2) {
            a = get_yaw(gPlayers[e->closest].mikey_worldmat[3], &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(gPlayers[e->closest].pos, &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    if (e->closest >= 0) {
        Player* player = &gPlayers[e->closest];
        f32 sight = e->sight;
        f32 dy = e->objgrp.worldmat[3][1] - player->pos[1];
        if (e->visactive != 0 && e->actual_dist <= sight
            && dy >= -10.0 && dy <= 10.0) {
            if (e->flag2 <= 0) {
                RequestEnemyAction(e, 24);
            } else {
                e->flag2 -= gFrameTicks;
            }
        }
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic24 @0x8004BC5C (state 24, "IT"/warlock chase).  Ramp a retreat
 * speed the first frames after being cornered, face the IT enemy plus pi,
 * normalize, then accelerate + turn + move; refresh the corner state after. */
void move_logic24(s32 index)
{
    s32 stuck;
    Enemy* e = &gEnemies[index];
    f32 spd = 0.0f;
    s32 c;

    if (e->dead_end > 0) {
        stuck = 1;
    } else {
        stuck = 0;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->dead_end > 0 && (c = e->counter1) < 8) {
        e->counter1 = c + 1;
        spd = lbl_8011BF60[c];
        e->dead_end = 0;
    }
    e->ang = spd
        + (3.141592654
           + get_yaw(&gEnemies[lbl_80344748].objgrp.worldmat[3][0],
                         &e->objgrp.worldmat[3][0]));
    {
        f64 a;

        if ((a = e->ang) > 3.141592654) {
            a -= 6.283185308;
        } else if (a <= -3.141592654) {
            a = 6.283185308 + a;
        }
        e->ang = a;
    }
    set_enemy_trans(e, 2.0f, e->ang);
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    if (e->moved != 0) {
        e->dead_end = 0;
    }
    if (stuck == 0 && e->dead_end > 0) {
        e->anghit = e->ang;
        e->counter1 = 0;
    }
}

/* move_logic28 @0x8004BDDC (state 28, imp close-quarters).  If the target is
 * in melee range, switch to the run algorithm and re-dispatch; otherwise face
 * the target and, when lined up, throw / power-attack on a cadence. */
void move_logic28(s32 index)
{
    Enemy* e = &gEnemies[index];
    f32 a;
    u8 unused[8];

    if (e->closest >= 0 && e->actual_dist <= 6.0) {
        e->algorithm = 7;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->closest >= 0) {
        if (gPlayers[e->closest].field_A1C > 2) {
            a = get_yaw(gPlayers[e->closest].mikey_worldmat[3], &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(gPlayers[e->closest].pos, &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    if (e->closest >= 0) {
        Player* player = &gPlayers[e->closest];
        f32 sight = e->sight;
        f32 dy = e->objgrp.worldmat[3][1] - player->pos[1];
        if (e->visactive != 0 && e->actual_dist <= sight
            && dy >= -10.0 && dy <= 10.0) {
            if (e->flag2 <= 0) {
                if (e->org_lvl >= 3) {
                    RequestEnemyAction(e, 16);
                } else if ((++e->counter2 & 1) != 0) {
                    RequestEnemyAction(e, 12);
                } else {
                    RequestEnemyAction(e, 14);
                }
            } else {
                e->flag2 -= gFrameTicks;
            }
        }
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

/* move_logic29 @0x8004BF9C (state 29, golem swing-attacker).  Approaches until the
 * player is within 6, then runs a range state machine (flag1): near hits swing
 * attacks (alternating 12/14, or a heavy 16 at high level); at leap range it charges
 * off lbl_8011BF60 and fires a lunge (action 3) aimed 180deg + ramp when flag1==1. */
void move_logic29(s32 index)
{
    s32 stuck;
    u8* base = (u8*)mbdesc;
    u8* row29;
    u8* e0;
    Enemy* e;
    s32 it;
    s32 dend;
    f32 leapspeed = 0.0f;
    s32 flee;
    f32 a;
    u8 _pad29[32];

    row29 = base + index * 916;
    dend = ((Enemy *)(row29 + ENEMY_POOL_OFF))->dead_end;
    e0 = row29 + 3608;
    e = (Enemy*)(u8*)e0;
    if (dend > 0) {
        stuck = 1;
    } else {
        stuck = 0;
    }
    it = lbl_80344748;
    if (it < 0) {
        flee = 0;
    } else {
        u8* other = base + it * 916;
        if (((Enemy *)(other + ENEMY_POOL_OFF))->state != ACTIVE) {
            flee = 0;
        } else if (((Enemy *)(other + ENEMY_POOL_OFF))->actual_dist > ((Enemy *)e0)->sight) {
            flee = 0;
        } else if (index == it || ((Enemy *)e0)->birth_style != 0 || dend > 0) {
            goto flee_zero29;
        } else {
            f32 dx = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][0] - ((Enemy *)e0)->objgrp.worldmat[3][0];
            f32 dy = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][1] - ((Enemy *)e0)->objgrp.worldmat[3][1];
            f32 dz = ((Enemy *)(other + ENEMY_POOL_OFF))->objgrp.worldmat[3][2] - ((Enemy *)e0)->objgrp.worldmat[3][2];
            if (dx * dx + dy * dy + dz * dz < 100.0) {
                flee = -1;
            } else {
            flee_zero29:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->closest >= 0 && e->actual_dist <= 6.0) {
        e->algorithm = 7;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    {
        s16 c = e->closest;
        if (c >= 0) {
            if (gPlayers[c].field_A1C > 2) {
                a = get_yaw(gPlayers[c].mikey_worldmat[3], &e->objgrp.worldmat[3][0]);
            } else {
                a = get_yaw(gPlayers[c].pos, &e->objgrp.worldmat[3][0]);
            }
        } else {
            a = e->ang;
        }
    }
    e->ang = a;
    {
    s16 c29 = e->closest;
    if (c29 >= 0) {
        u8* gp = (u8*)&gPlayers + c29 * PLAYER_STRIDE;
        f32 dvert = e->objgrp.worldmat[3][1] -
                    ((Player *)gp)->pos[1];
        if (e->visactive != 0 && dvert >= -10.0 && dvert <= 10.0) {
            if (e->flag1 == 0) {
                if (e->actual_dist <= 8.0) {
                    e->flag1 = 1;
                }
                if (e->actual_dist > 18.0) {
                    e->flag1 = 2;
                }
            } else {
                if (e->flag1 == 1) {
                    if (e->actual_dist > 10.0) {
                        e->flag1 = 0;
                    }
                } else if (e->actual_dist <= 16.0) {
                    e->flag1 = 0;
                }
                if (e->flag1 != 0 && e->dead_end > 0) {
                    s32 leap = e->counter1;
                    if (leap < 8) {
                        e->counter1 = leap + 1;
                        leapspeed = lbl_8011BF60[leap];
                        e->dead_end = 0;
                    } else {
                        e->flag1 = 0;
                    }
                }
            }
            if (e->flag2 > 0) {
                e->flag2 -= gFrameTicks;
            } else if (e->flag1 == 0 || e->area == 1) {
                if (e->org_lvl >= 3) {
                    RequestEnemyAction(e, 16);
                } else if ((++e->counter2 & 1) != 0) {
                    RequestEnemyAction(e, 12);
                } else {
                    RequestEnemyAction(e, 14);
                }
            } else {
                f32 v;
                f32 la;
                if (e->flag1 == 1) {
                    v = 3.141592654 + e->ang + leapspeed;
                } else {
                    v = e->ang + leapspeed;
                }
                {
                    f64 nv;
                    if (v > 3.141592654) {
                        nv = v - 6.283185308;
                    } else if (v <= -3.141592654) {
                        nv = 6.283185308 + v;
                    } else {
                        nv = v;
                    }
                    la = nv;
                }
                set_enemy_trans(e, 0.8f, la);
                RequestEnemyAction(e, 3);
                e->dead_end = 0;
            }
        }
    }
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
    if (e->moved != 0) {
        e->dead_end = 0;
    }
    if (stuck == 0 && e->dead_end > 0) {
        e->anghit = e->ang;
        e->counter1 = 0;
    }
}

/* move_logic30 @0x8004C3E4 (state 30, death-lure wander).  IT-flee / chase /
 * melee gates, then a randomized roam timer scaled by level, taunt cues, and a
 * delegated base wander (move_logic00) with the algorithm parked at 30. */
void move_logic30(s32 index)
{
    Enemy* e = &gEnemies[index];
    s32 it = lbl_80344748;
    s32 flee;
    u8 unused[24];

    if (it < 0) {
        flee = 0;
    } else {
        if (gEnemies[it].state != ACTIVE) {
            flee = 0;
        } else if (gEnemies[it].actual_dist > e->sight) {
            flee = 0;
        } else if (index == it || e->birth_style != 0 || e->dead_end > 0) {
            goto flee_zero30;
        } else {
            f32 dx = gEnemies[it].objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
            f32 dy = gEnemies[it].objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
            f32 dz = gEnemies[it].objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
            if (dx * dx + dy * dy + dz * dz < 100.0) {
                flee = -1;
            } else {
            flee_zero30:
                flee = 0;
            }
        }
    }
    if (flee != 0) {
        e->algorithm = 24;
        do_ai(index);
        return;
    }
    if (e->recognized == 0 || e->closest < 0) {
        e->algorithm = (index & 1) + 5;
        do_ai(index);
        return;
    }
    if (e->closest >= 0 && e->actual_dist <= 6.0) {
        e->algorithm = 7;
        do_ai(index);
        return;
    }
    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->dead_end <= 0 && (e->counter1 -= gFrameTicks) <= 0) {
        s32 n = (s32)(90.0 * gCurLevel->ene_mrate);
        s32 r = RandInt(10) + 20;
        if (e->dead_end <= 0) {
            e->dead_end = r;
            if (r >= 60) {
                if (e->daction == 3 || e->daction == 4) {
                    e->daction = 0;
                }
            }
        }
        e->counter1 = e->dead_end + RandInt(n >> 1) + n;
    }
    if (e->dead_end > 0 && e->visactive != 0) {
        if (e->org_lvl >= 2) {
            RequestEnemyAction(e, 16);
        } else {
            RequestEnemyAction(e, 12);
        }
    }
    e->algorithm = 0;
    move_logic00(index);
    e->algorithm = 30;
}

/* Integrate the heading into a horizontal velocity, refreshing the cached
 * sin/cos when the heading changed; scaled by k and the per-type speed table
 * at lbl_80250E40 (= mbdesc + 64).  Inlined into move_logic31. */
static inline void update_vel(Enemy* e, f32 k)
{
    f32 ang = e->ang;
    f32 spd;
    f32 vx;
    f32 vz;

    if (e->prev_dir != ang) {
        e->xspd = sin(ang);
        e->zspd = cos(ang);
        e->prev_dir = ang;
    }
    spd = lbl_80250E40[e->type];
    vx = k * (e->xspd * spd);
    vz = k * (e->zspd * spd);
    e->trans[0] += vx;
    e->trans[2] += vz;
}

/* move_logic31 @0x8004C650 (state 31, IT tag-runner).  Face the closest player,
 * then per current action integrate a sin/cos velocity along the heading (scaled
 * by a per-type speed table), roll cooldown timers, and cue attacks. */
void move_logic31(s32 index)
{
    Enemy* e = &gEnemies[index];
    f32 a;
    u8 unused[16];

    if (e->algorithm != e->prev_ai) {
        format_brain(index);
    }
    if (e->closest >= 0) {
        if (gPlayers[e->closest].field_A1C > 2) {
            a = get_yaw(gPlayers[e->closest].mikey_worldmat[3], &e->objgrp.worldmat[3][0]);
        } else {
            a = get_yaw(gPlayers[e->closest].pos, &e->objgrp.worldmat[3][0]);
        }
    } else {
        a = e->ang;
    }
    e->ang = a;
    if (e->closest >= 0) {
        e->daction = 0;
        if (e->visactive != 0) {
            s32 act = e->action;
            if (act == 12) {
                goto action_12_or_13;
            }
            if (act != 13) {
                goto other_action31;
            }
action_12_or_13:
            {
                update_vel(e, 1.0f);
                e->flag2 = RandInt(30) + 30;
                if (e->actual_dist <= 7.5) {
                    e->attack_index = e->closest;
                }
                goto action_done31;
            }
other_action31:
            if (act == 16 || act == 17) {
                e->flag2 = RandInt(30) + 30;
            } else if (e->flag2 <= 0) {
                if (e->actual_dist <= 10.0) {
                    RequestEnemyAction(e, 12);
                } else {
                    RequestEnemyAction(e, 16);
                }
            } else if (act != 1) {
                e->flag2 -= gFrameTicks;
                update_vel(e, 0.5f);
            }
action_done31:
            ;
        }
    }
    e->pyr[1] = turn_enemy_ang(e, e->ang);
    do_enemy_move(index);
}

s32 fn_8004C8CC(f32* pos, s32 index)
{
    Enemy* e = &gEnemies[index];
    s32 result;
    f32 rad;
    f32 hht;
    f32 probe[3];
    void* obj;

    rad = (f32)(0.1 + e->rad);
    hht = (f32)(0.1 + e->hht);
    probe[0] = ((Enemy *)e)->objgrp.coll_pos[0];
    probe[1] = ((Enemy *)e)->objgrp.coll_pos[1];
    result = -1;
    probe[2] = ((Enemy *)e)->objgrp.coll_pos[2];
    probe[1] = pos[1];
    if (fn_8004646C(index, probe, pos, 0, rad, hht, 0) >= 0) {
        result = 0;
    }
    if (result != 0) {
        obj = fn_8005EFAC(rad, probe, pos, 0, 0);
        if (obj != 0) {
            if (fn_8005D3D8(-1, obj) != 0) {
                result = 0;
            }
        }
    }
    if (result != 0) {
        if (FastWallCollide(probe, pos, 0, 2) != 0) {
            result = 0;
        }
    }
    return result;
}

/* find_neighbor_milestone @0x8004C9DC -- locate milestone id `ms` in the active
 * milestone list, then return the id `nth` slots away (reflecting at the ends),
 * preferring whichever candidate sits closer to the world origin. */
s32 find_neighbor_milestone(s32 ms, s32 nth)
{
    s32 count = lbl_80344724;
    s32 idx = 0;
    s32 lo;
    s32 hi;
    s32 i;
    u8 unused[24];

    for (i = 0; i < count; i++) {
        if (ms == sEnemyMilestoneRoute[i]) {
            break;
        }
        idx++;
    }
    if (idx >= count) {
        return -1;
    }
    lo = idx - nth;
    if (lo < 0) {
        return sEnemyMilestoneRoute[idx + nth];
    }
    hi = idx + nth;
    if (hi > count - 1) {
        return sEnemyMilestoneRoute[lo];
    }
    {
        u8* milestoneBase;
        u8* milestoneY;
        u8* milestoneX;
        u8* milestoneZ;
        s32 m_lo;
        s32 m_hi;
        f32 x;
        f32 y;
        f32 z;
        f32 dlo;
        f32 dhi;

        m_lo = sEnemyMilestoneRoute[lo];
        milestoneBase = sMilestones;
        milestoneX = milestoneBase + 0x30;
        milestoneY = milestoneBase + 0x34;
        milestoneZ = milestoneBase + 0x38;
        x = *(f32*)(milestoneX + m_lo * 0x68);
        y = *(f32*)(milestoneY + m_lo * 0x68);
        z = *(f32*)(milestoneZ + m_lo * 0x68);
        dlo = y * y;
        dlo = x * x + dlo;
        dlo = z * z + dlo;

        if (dlo > 0.0f) {
            volatile f32 tmp;
            f64 y = __frsqrte(dlo);
            y = 0.5 * y * (3.0 - y * y * dlo);
            y = 0.5 * y * (3.0 - y * y * dlo);
            y = 0.5 * y * (3.0 - y * y * dlo);
            tmp = (f32)(dlo * (0.5 * y * (3.0 - y * y * dlo)));
            dlo = tmp;
        }
        m_hi = sEnemyMilestoneRoute[hi];
        x = *(f32*)(milestoneX + m_hi * 0x68);
        y = *(f32*)(milestoneY + m_hi * 0x68);
        z = *(f32*)(milestoneZ + m_hi * 0x68);
        dhi = z * z + (dhi = x * x + y * y);
        if (dhi > 0.0f) {
            volatile f32 tmp;
            f64 y = __frsqrte(dhi);
            y = 0.5 * y * (3.0 - y * y * dhi);
            y = 0.5 * y * (3.0 - y * y * dhi);
            y = 0.5 * y * (3.0 - y * y * dhi);
            tmp = (f32)(dhi * (0.5 * y * (3.0 - y * y * dhi)));
            dhi = tmp;
        }
        if (dlo < dhi) {
            return m_lo;
        }
        return m_hi;
    }
}

/* turn_enemy_ang @0x8004CBB8 (14 callers: the move_logic set).  Rotate the
 * enemy's yaw toward `want` at the per-type turn rate (lbl_8011BED8 table,
 * 3x rate while running/reacting), clamping to `want` when within one step,
 * and normalize the result to (-pi, pi]. */
f32 turn_enemy_ang(Enemy* e, f32 want)
{
    s32 act = e->action;
    s32 hit = 0;
    s32 dir;
    f32 d;
    f32 d32;
    f32 cur;
    f32 rate;
    f32 step;
    f64 nd;
    f64 r;

    if (act >= 28) {
        return e->pyr[1];
    }
    cur = e->pyr[1];
    rate = lbl_8011BED8[e->type];
    if (act == 1) {
        return cur;
    }
    if (act == 4 || act == 28 || act == 29) {
        rate *= 3.0;
    }
    d = want - cur;
    if (d > 3.141592654) {
        nd = d - 6.283185308;
    } else if (d <= -3.141592654) {
        nd = 6.283185308 + d;
    } else {
        nd = d;
    }
    d32 = (f32)nd;
    step = rate * (f32)(u32)gFrameTicks;
    if (d32 >= 0.0f) {
        if (d32 <= step) {
            hit = 1;
        }
        dir = 1;
    } else {
        if (d32 >= -step) {
            hit = 1;
        }
        dir = -1;
    }
    switch (hit) {
    case 0:
        if (dir > 0) {
            want = cur + step;
        } else {
            want = cur - step;
        }
        break;
    }
    r = want;
    if (r > 3.141592654) {
        r -= 6.283185308;
    } else if (r <= -3.141592654) {
        r = 6.283185308 + r;
    }
    return r;
}

/* Accelerate an enemy along an angle, caching the trig pair between calls. */
void set_enemy_trans(Enemy* enemy, f32 speed, f32 angle)
{
    if (enemy->type != gBossType && enemy->action != 1) {
        s32 action;

        if (speed >= 1.25) {
            action = 4;
        } else {
            action = 3;
        }
        RequestEnemyAction(enemy, action);

        if (enemy->action == 3 || enemy->action == 4 ||
            (u32)(enemy->action - 22) <= 1 || enemy->coll_pnum >= 0) {
            f32 typeSpeed;
            f32 dx;
            f32 dz;

            if (enemy->prev_dir != angle) {
                enemy->xspd = sin(angle);
                enemy->zspd = cos(angle);
                enemy->prev_dir = angle;
            }
            typeSpeed = lbl_80250E40[enemy->type];
            dx = speed * (enemy->xspd * typeSpeed);
            dz = speed * (enemy->zspd * typeSpeed);
            enemy->trans[0] += dx;
            enemy->trans[2] += dz;
        }
    }
}

s32 fn_8004CE38(Enemy* e)
{
    Player* p;
    f32 dx;
    f32 dz;
    f32 a;
    f32 ang;
    f64 t;
    f32 x1;
    f32 z1;
    f32 x2;
    f32 z2;

    p = &gPlayers[e->closest];
    /* When the closest player's mikey is live (field_A1C > 2) the chase
     * bearing is taken from the mikey object's world translation
     * (mikey_worldmat[3], the mikey OBJGRP embed) rather than from pos[]. */
    if (p->field_A1C > 2) {
        dx = e->objgrp.worldmat[3][0] -
             p->mikey_worldmat[3][0];
        dz = e->objgrp.worldmat[3][2] -
             p->mikey_worldmat[3][2];
    } else {
        dx = e->objgrp.worldmat[3][0] -
             p->pos[0];
        dz = e->objgrp.worldmat[3][2] -
             p->pos[2];
    }
    a = (f32)(0.5235987756666667 + e->pyr[1]);
    if (a > 3.141592654) {
        t = a - 6.283185308;
    } else if (a <= (-3.141592654)) {
        t = 6.283185308 + a;
    } else {
        t = a;
    }
    ang = (f32)t;
    x1 = dx + sin(ang);
    z1 = dz + cos(ang);
    a = (f32)(e->pyr[1] - 0.5235987756666667);
    if (a > 3.141592654) {
        t = a - 6.283185308;
    } else if (a <= (-3.141592654)) {
        t = 6.283185308 + a;
    } else {
        t = a;
    }
    ang = (f32)t;
    x2 = dx + sin(ang);
    z2 = dz + cos(ang);
    if (x2 * x2 + z2 * z2 <= x1 * x1 + z1 * z1) {
        return -1;
    }
    return 1;
}

/* Choose the turn direction on the axis with the larger separation.
 * Xbox get_turn_dir corroborates the two point inputs and absolute differences.
 * The GC inline magnitude calls supply their own temporaries; the former
 * caller padding and manually expanded sign-bit writes are not needed. */
s32 fn_8004CFAC(f32* pos, f32* target)
{
    f32 x = pos[0];
    f32 targetX = target[0];
    f32 dx = fabsf_(x - targetX);
    f32 z;
    f32 targetZ;
    f32 dz;

    z = pos[2];
    targetZ = target[2];
    dz = fabsf_(z - targetZ);

    if (dx >= dz) {
        if (z < targetZ) {
            return 1;
        }
        return -1;
    }
    if (x < targetX) {
        return -1;
    }
    return 1;
}

/* Arm an enemy's dead-end timer and clear blocked desired actions. */
void fn_8004D030(s32 index, s32 ticks)
{
    Enemy* enemy = &gEnemies[index];

    if (enemy->dead_end > 0) {
        return;
    }
    enemy->dead_end = ticks;
    if (ticks < 60) {
        return;
    }
    if (enemy->daction == 3 || enemy->daction == 4) {
        enemy->daction = 0;
    }
}

void do_enemies(void)
{
    /* Independent cursors for the update, visibility and scripted passes. */
    Enemy* e;
    Enemy* visibleEnemy;
    Enemy* scriptEnemy;
    s32 shown = 0;
    s32 i;
    u8 unused[8];

    (void)unused;

    ProcessCritterList();
    if (gBoss398 >= 0) {
        gEnemies[gBoss398].state = ACTIVE;
    }
    if ((gGameBusy | gGameplayPauseTimer) != 0) {
        return;
    }

    if (gScriptedCameraState != 0) {
        if (lbl_803447B8 == 0) {
            return;
        }
        scriptEnemy = gEnemies;
        for (i = 0; i < gNumEnemies; i++, scriptEnemy++) {
            s32 type;

            if (scriptEnemy->state != ACTIVE) {
                continue;
            }
            type = scriptEnemy->type;
            if (type == gBossType) {
                continue;
            }
            if (type == 0x1D) {
                scriptEnemy->daction = 1;
            } else if (type == 0) {
                scriptEnemy->daction = 3;
            } else {
                scriptEnemy->daction = 0;
            }
            if (scriptEnemy->atree.root != 0) {
                scriptEnemy->action = DoEnemyAction(scriptEnemy);
            }
        }
        return;
    }

    {
        Player* pl = gPlayers;

        for (i = 0; i < 4; i++, pl++) {
            if (pl->state == 1) {
                pl->num_approaching = 0;
                pl->dist_offset = 0.0f;
            }
        }
    }

    lbl_80344718 = 0;
    {
        f32 rate = gCurLevel->ene_speed * (f32)(u32)gFrameTicks;

        for (i = 0; i < 45; i++) {
            lbl_80250E40[i] =
                rate * lbl_8011B878[i];
        }
    }

    {
        Enemy* e = gEnemies;

        for (i = 0; i < gNumEnemies; i++, e++) {
            if (e->state == ACTIVE && e->algorithm == 0x12 &&
                e->visactive != 0) {
                if (e->action == 4 || e->daction == 4) {
                    lbl_80344748 = i;
                    break;
                }
            }
        }
    }

    {
        visibleEnemy = gEnemies;

        lbl_80344740 = 0;
        for (i = 0; i < gNumEnemies; i++, visibleEnemy++) {
            f32 r;

            if (visibleEnemy->state == 0) {
                continue;
            }
            r = 2.0f * visibleEnemy->rad;
            visibleEnemy->visible =
                (s16)MBWorldSphereVisible3(visibleEnemy->objgrp.attn_pos, r);
            r += 15.0;
            visibleEnemy->visactive =
                (s16)MBWorldSphereVisible3(visibleEnemy->objgrp.attn_pos, r);
            if (visibleEnemy->visible != 0) {
                lbl_80344740++;
            }
        }
    }

    {
        e = gEnemies;

        for (i = 0; i < gNumEnemies; i++, e++) {
            e->old_ai = e->algorithm;
            e->operation_count += gFrameTicks;
            if (e->idle_secs > 0.0f) {
                e->idle_secs -= gClockFrameStep;
            }
            if (e->type == 0) {
                e->daction = 3;
            } else {
                e->daction = 0;
            }

            switch (e->state) {
            case 1:
            case 7:
                shown++;
                if (e->type == gBossType) {
                    break;
                }
                fn_8005A338(&e->objgrp.worldmat[0][0], e->coll_offset,
                            e->attn_offset);
                if (lbl_803447DC != 0) {
                    e->atree.animinfo.starttime += gClockFrameStep;
                    fn_8004DC2C(e);
                    do_enemy_collide(i, 0.0f);
                } else if (gTriggerCameraState != 0) {
                    e->daction = 0;
                    e->action = DoEnemyAction(e);
                    do_enemy_collide(i, 0.0f);
                    fn_8004DC2C(e);
                } else {
                    fn_800516F8(i);
                    fn_8004DF58(e);
                    fn_8004DC2C(e);
                    if (fn_8004D958(i) != 0) {
                        break;
                    }
                    if (e->atree.root != 0) {
                        e->action = DoEnemyAction(e);
                    }
                    e->anim_done = (e->action == e->daction) ? -1 : 0;
                }
                ProcessSkinFX((f32*)&e->skinfx, e->objgrp.node, 0);
                UpdateObjWorldMat(&e->objgrp.worldmat[0][0]);
                break;
            case 6:
                fn_8005A338(&e->objgrp.worldmat[0][0], e->coll_offset,
                            e->attn_offset);
                shown++;
                break;
            case 8:
                fn_8005A338(&e->objgrp.worldmat[0][0], e->coll_offset,
                            e->attn_offset);
                shown++;
                if (e->type == gBossType) {
                    goto sync;
                }
                if (e->type == 0x1E) {
                    s32 eff = e->specialfx;
                    s32 alpha = e->alpha;

                    if (eff >= 0) {
                        e->specialfx = DeleteEffect(eff, 1);
                    }
                    if (alpha >= 0xFF) {
                        if (e->flag2 != 0) {
                            if (e->org_lvl == 2) {
                                msgPost(0x81, e->coll_pnum,
                                        &e->objgrp.attn_pos[0]);
                            } else {
                                msgPost(0x83, e->coll_pnum,
                                        &e->objgrp.attn_pos[0]);
                            }
                        }
                        kill_enemy(i);
                        break;
                    }
                    MBTreeSetAlpha(e->objgrp.node, alpha, 1);
                    e->alpha = e->alpha + gFrameTicks * 4;
                    e->objgrp.worldmat[3][1] =
                        (f32)(10.0 * gClockFrameStep +
                              e->objgrp.worldmat[3][1]);
                    UpdateObjWorldMat(&e->objgrp.worldmat[0][0]);
                    goto sync;
                } else {
                    s32 cc;

                    fn_8004DC2C(e);
                    e->daction = 0x20;
                    e->pyr[1] = turn_enemy_ang(e, e->ang);
                    do_enemy_move(i);
                    if (e->atree.root != 0) {
                        if (e->type != gBossType) {
                            e->action = DoEnemyAction(e);
                        }
                        if (e->action == e->daction) {
                            e->anim_done = -1;
                        } else {
                            e->anim_done = 0;
                        }
                    }
                    ProcessSkinFX((f32*)&e->skinfx, e->objgrp.node, 0);
                    cc = e->action;
                    if ((cc != E_HIT_REACT1 && cc != E_HIT_REACT2 && cc != E_DYING) ||
                        e->skinfx.nframes <= 0.0f) {
                        if (e->type == E_GOLEM) {
                            if (RandInt(2) == 0) {
                                fn_8009FEFC(e->area);
                            } else {
                                fn_8009FEA0(e->area);
                            }
                        }
                        kill_enemy(i);
                        break;
                    }
                    UpdateObjWorldMat(&e->objgrp.worldmat[0][0]);
                    goto sync;
                }
            sync:
                if (e->shadow != 0) {
                    e->shadow->mat[3][0] = e->objgrp.worldmat[3][0];
                    e->shadow->mat[3][1] = e->objgrp.worldmat[3][1];
                    e->shadow->mat[3][2] = e->objgrp.worldmat[3][2];
                }
                break;
            case 0:
            default:
                break;
            }

            e->prev_ai = e->algorithm;
            e->algorithm = e->old_ai;
            if (e->operation_count >= e->operation_speed) {
                e->operation_count -= e->operation_speed;
            }
            e->pushed[0] = (f32)(0.8 * e->pushed[0]);
            e->pushed[1] = (f32)(0.8 * e->pushed[1]);
            e->pushed[2] = (f32)(0.8 * e->pushed[2]);
            if (fabsf_(e->pushed[0]) < 0.01) {
                e->pushed[0] = 0.0f;
            }
            if (fabsf_(e->pushed[1]) < 0.01) {
                e->pushed[1] = 0.0f;
            }
            if (fabsf_(e->pushed[2]) < 0.01) {
                e->pushed[2] = 0.0f;
            }
            if (e->pushed[1] > 0.0f) {
                e->pushed[1] =
                    e->pushed[1] - 100.0f * gClockFrameStep;
                if (e->pushed[1] < 0.0f) {
                    e->pushed[1] = 0.0f;
                }
            }
            if (gBossType < 0) {
                if ((f64)lbl_803447D8 != 1.0) {
                    if (e->objgrp.node != 0) {
                        MBTreeSetFlags(e->objgrp.node, 8, 0);
                        e->objgrp.node->scale[0] = lbl_803447D8;
                        e->objgrp.node->scale[1] = lbl_803447D8;
                        e->objgrp.node->scale[2] = lbl_803447D8;
                    }
                    if (e->shadow != 0) {
                        MBTreeSetFlags(e->shadow, 8, 0);
                        e->shadow->scale[0] = lbl_803447D8;
                        e->shadow->scale[1] = lbl_803447D8;
                        e->shadow->scale[2] = lbl_803447D8;
                    }
                } else {
                    if (e->objgrp.node != 0) {
                        MBTreeClearFlags(e->objgrp.node, 8, 0);
                        e->objgrp.node->scale[0] = 1.0f;
                        e->objgrp.node->scale[1] = 1.0f;
                        e->objgrp.node->scale[2] = 1.0f;
                    }
                    if (e->shadow != 0) {
                        MBTreeClearFlags(e->shadow, 8, 0);
                        e->shadow->scale[0] = 1.0f;
                        e->shadow->scale[1] = 1.0f;
                        e->shadow->scale[2] = 1.0f;
                    }
                }
            }
        }
    }

    if (lbl_80344718 == 0) {
        AudioPlayEvt102();
    }
    if ((gControllerButtons & 0x10) != 0 &&
        (gControllerButtons & 1) != 0) {
        MBTextMsg* message;
        sprintf(gTextFormatBuf, "%d", shown);
        message = DrawTextKeepScale(1.2f, -0x100, 0x144, 0, 0xFF0000,
                                    gTextFormatBuf);
        message->flags |= 0x40000;
    }
}

s32 fn_8004D958(s32 index)
{
    Enemy* e = &gEnemies[index];
    s32 dir;
    s16 own;
    s16 t;
    s16 v;

    e->watchdog += gFrameTicks;
    if (e->actual_dist >
        e->sight) {
        if (e->visactive == 0) {
            return -1;
        }
    }
    own = e->prev_closest;
    if (own >= 0 && e->closest != own) {
        e->ms_idx = 0;
        e->max_msidx = 4;
    }
    if (gBossType >= 0 && gBossDying != 0) {
        fn_800945D0((u8*)e + offsetof(Enemy, objgrp.attn_pos[0]),
                    (u8*)e + offsetof(Enemy, objgrp), 0, 1, *(u32*)e,
                    e->hht);
        kill_enemy(index);
        return -1;
    }
    index = do_ai(index);
    if (e->action == 1) {
        e->daction = 3;
    }
    switch (*(s32*)e) {
    case 24: {
        s32 st = e->action;
        dir = 16;
        if (st == 3) goto bob;
        if (st == 4) goto bob;
        if (st != 0) goto stop;
        {
bob:
            t = e->endurance;
            if (t == 0) {
                fn_8004DB3C(e, -dir);
                e->endurance = RandInt(60) + 60;
            } else if (t > 0) {
                fn_8004DB3C(e, -dir);
                v = e->endurance - gFrameTicks;
                e->endurance = v;
                if (v < 0) {
                    e->endurance = -(RandInt(60) + 60);
                }
            } else {
                fn_8004DB3C(e, 16);
                v = e->endurance + gFrameTicks;
                e->endurance = v;
                if (v > 0) {
                    e->endurance = 0;
                }
            }
        }
        break;
stop:
        fn_8004DB3C(e, -dir);
        e->endurance = 0;
        break;
    }
    }
    return index;
}

/* Fade an enemy and its shadow, hiding both when fully opaque. */
void fn_8004DB3C(Enemy* enemy, s32 delta)
{
    s32 alpha = MBTreeGetAlpha(enemy->objgrp.node);
    s32 value;

    if ((delta > 0 && alpha >= 255) || (delta < 0 && alpha == 0)) {
        return;
    }
    value = alpha + delta * gFrameTicks;
    if (value < 0) {
        delta = 0;
    } else if (value > 255) {
        delta = 255;
    } else {
        delta = value;
    }
    if (delta == 255) {
        MBTreeSetFlags(enemy->objgrp.node, 2, 0);
        MBTreeSetFlags(enemy->shadow, 2, 0);
    } else {
        MBTreeSetAlpha(enemy->objgrp.node, delta, 1);
        MBTreeSetAlpha(enemy->shadow, delta, 1);
        MBTreeClearFlags(enemy->objgrp.node, 2, 0);
        MBTreeClearFlags(enemy->shadow, 2, 0);
    }
}

void fn_8004DC2C(Enemy* enemy)
{
    f32 scale;
    f64 limit;
    u32 damageType;
    s32 type;
    s32 typeCopy;
    f64 angle;

    typeCopy = enemy->type;
    type = typeCopy;
    if (type == gBossType) {
        return;
    }
    if ((f64)enemy->damage >= 1.0) {
        damageType = enemy->damagetype;
        if ((damageType & 0x10160) != 0 ||
            (enemy->damage > 10.0f &&
             (damageType & 0x200) != 0)) {
            RequestEnemyAction(enemy, E_HIT_REACT2);
            if (enemy->type == E_GOLEM) {
                enemy->flag1 = 1;
            }
            if (enemy->type == E_ACID) {
                scale = 0.0f;
            } else if ((f64)enemy->hht <= 2.0 && enemy->type != E_IT) {
                scale = 40.0f;
            } else if (enemy->type == E_GOLEM) {
                scale = 2.0f;
            } else {
                scale = 20.0f;
            }
            enemy->pushed[0] += enemy->damagedir[0] * scale;
            enemy->pushed[1] += enemy->damagedir[1] * scale;
            enemy->pushed[2] += enemy->damagedir[2] * scale;
            scale = enemy->damagedir[2];
            angle = atan2(enemy->damagedir[0], scale);
            limit = 3.141592654;
            enemy->pushang = (f32)(limit + angle);
            angle = enemy->pushang;
            if (angle > limit) {
                angle -= 6.283185308;
            } else if (angle <= (-3.141592654)) {
                angle = 6.283185308 + angle;
            }
            enemy->pushang = (f32)angle;
            scale = 0.0f;
            enemy->damagedir[0] = scale;
            enemy->damagedir[1] = scale;
            enemy->damagedir[2] = scale;
        } else if ((damageType & 0x10) != 0) {
            if (type == E_GOLEM || type == E_ACID) {
                enemy->flag1 = 1;
            } else {
                RequestEnemyAction(enemy, E_HIT_REACT1);
                scale = 8.0f;
                enemy->pushed[0] += enemy->damagedir[0] * scale;
                enemy->pushed[1] += enemy->damagedir[1] * scale;
                enemy->pushed[2] += enemy->damagedir[2] * scale;
                scale = enemy->damagedir[2];
                angle = atan2(enemy->damagedir[0], scale);
                limit = 3.141592654;
                enemy->pushang = (f32)(limit + angle);
                angle = enemy->pushang;
                if (angle > limit) {
                    angle -= 6.283185308;
                } else if (angle <= (-3.141592654)) {
                    angle = 6.283185308 + angle;
                }
                enemy->pushang = (f32)angle;
                scale = 0.0f;
                enemy->damagedir[0] = scale;
                enemy->damagedir[1] = scale;
                enemy->damagedir[2] = scale;
            }
        } else {
            if (type != E_GOLEM) {
                RequestEnemyAction(enemy, E_HIT_REACT1);
            } else {
                enemy->flag1 = 1;
            }
        }

        if ((f64)(enemy->pushed[0] * enemy->pushed[0] +
                  enemy->pushed[1] * enemy->pushed[1] +
                  enemy->pushed[2] * enemy->pushed[2]) > 1600.0) {
            NormalVector(enemy->pushed);
            limit = 40.0;
            enemy->pushed[0] = (f32)(limit * enemy->pushed[0]);
            enemy->pushed[1] = (f32)(limit * enemy->pushed[1]);
            enemy->pushed[2] = (f32)(limit * enemy->pushed[2]);
        }
        scale = 0.0f;
        enemy->damage = scale;
        enemy->damagetype = 0;
        if (enemy->health <= scale) {
            RequestEnemyAction(enemy, E_DYING);
        } else {
            SetSkinFX(&enemy->skinfx, lbl_80344BF8, 1, 1, 1.0f);
        }
    }
    enemy->pushmag2 = enemy->pushed[0] * enemy->pushed[0] +
                      enemy->pushed[2] * enemy->pushed[2];
}

void fn_8004DF58(Enemy* enemy)
{
    u32 playerFlags;
    Player* player;
    s32 damageMode;
    f32 amount;
    f32 direction[3];
    f32 missilePosition[3];
    f32 healedPosition[3];
    f32 reflectedPosition[3];
    u8 unused[16];
    s32 hitKind;

    (void)unused;
    playerFlags = 0;
    if (((enemy->algorithm == 28 ||
          (u16)(enemy->algorithm - 29) <= 1) &&
         (enemy->attack_flag & 0xF) != 0) ||
        (enemy->algorithm == 31 && (enemy->attack_flag & 2) != 0)) {
        if (enemy->coll_pnum < 0) {
            if (enemy->closest >= 0) {
                missilePosition[0] = gPlayers[enemy->closest].effectpos[0];
                missilePosition[1] = gPlayers[enemy->closest].effectpos[1];
                missilePosition[2] = gPlayers[enemy->closest].effectpos[2];
            } else {
                missilePosition[0] = enemy->objgrp.coll_pos[0];
                missilePosition[1] = enemy->objgrp.coll_pos[1];
                missilePosition[2] = enemy->objgrp.coll_pos[2];
                missilePosition[0] = (f32)(20.0 * sin(enemy->pyr[1]) +
                                           missilePosition[0]);
                missilePosition[2] = (f32)(20.0 * cos(enemy->pyr[1]) +
                                           missilePosition[2]);
            }
            fn_8004E448(enemy, (s32)missilePosition,
                        enemy->objgrp.coll_pos);
            enemy->attack_flag &= ~0xF;
        }
    }

    if (enemy->attack_index >= 0 && (enemy->attack_flag & 0xF) != 0) {
        player = &gPlayers[enemy->attack_index];
        if (player->state == 1) {
            damageMode = 1;
            amount = enemy->atts.fight;
            if ((f64)lbl_803447D8 < 1.0) {
                playerFlags |= 0x40000000;
                amount *= 0.5;
            } else {
                if ((enemy->attack_flag & 2) != 0) {
                    amount *= 1.5;
                    if ((f64)enemy->hht > 2.0) {
                        playerFlags |= 0x10;
                    }
                }
                if (enemy->type == E_GOLEM) {
                    playerFlags |= 0x20;
                }
                if ((f64)enemy->hht <= 2.0) {
                    playerFlags |= 0x40000000;
                }
            }

            if (player->field_A20 != 0) {
                healedPosition[0] = enemy->objgrp.coll_pos[0];
                healedPosition[1] = enemy->objgrp.coll_pos[1];
                healedPosition[2] = enemy->objgrp.coll_pos[2];
                player->effectpos[0] =
                    healedPosition[0] - healedPosition[0];
                player->effectpos[1] =
                    healedPosition[1] - healedPosition[1];
                player->effectpos[2] =
                    healedPosition[2] - healedPosition[2];
                damage_enemy(enemy, amount, -1, 0x200,
                             (s32)player->effectpos,
                             (s32)healedPosition, 1);
                heal_player(player, amount);
                amount = 0.0f;
                playerFlags = 0x40000000;
                StartGemFX(player->col_pos, 1);
            } else if (player->field_A1E != 0) {
                reflectedPosition[0] = enemy->objgrp.coll_pos[0];
                reflectedPosition[1] = enemy->objgrp.coll_pos[1];
                reflectedPosition[2] = enemy->objgrp.coll_pos[2];
                player->effectpos[0] =
                    reflectedPosition[0] - reflectedPosition[0];
                player->effectpos[1] =
                    reflectedPosition[1] - reflectedPosition[1];
                player->effectpos[2] =
                    reflectedPosition[2] - reflectedPosition[2];
                damage_enemy(enemy, amount, -1, 0,
                             (s32)player->effectpos,
                             (s32)reflectedPosition, 1);
                amount = 0.0f;
                playerFlags = 0x40000000;
                StartGemFX(player->col_pos, 1);
            }

            if ((f64)enemy->hht <= 2.0) {
                if (enemy->type != E_IT) {
                    goto generic_hit_sound;
                }
            }
            if (enemy->type == E_DEMON || enemy->type == E_MUMMY ||
                enemy->type == E_TREEFOLK) {
            generic_hit_sound:
                fn_8009E08C(enemy);
                damageMode = 0;
            } else if (enemy->type == E_GRUNT || enemy->type == E_KNIGHT ||
                       enemy->type == E_LIZARDMAN) {
                if (enemy->org_lvl >= 2) {
                    hitKind = 3;
                } else {
                    hitKind = 4;
                }
                AudioPlayerHit(enemy->attack_index, hitKind);
                damageMode = 0;
            }
            if ((playerFlags & 0x130) != 0) {
                direction[0] = player->effectpos[0] -
                               enemy->objgrp.coll_pos[0];
                direction[1] = player->effectpos[1] -
                               enemy->objgrp.coll_pos[1];
                direction[2] = player->effectpos[2] -
                               enemy->objgrp.coll_pos[2];
                direction[1] = -direction[1];
                NormalVector(direction);
                damage_player(enemy->attack_index, amount, damageMode,
                              playerFlags, direction);
            } else {
                damage_player(enemy->attack_index, amount, damageMode,
                              playerFlags, 0);
            }
            lbl_803447E4 = 1;
            enemy->attack_count++;
            enemy->attack_flag &= ~0xF;
        }
        enemy->attack_index = -1;
    }

    if ((enemy->attack_flag & 0x10) != 0) {
        if (enemy->closest >= 0) {
            missilePosition[0] = gPlayers[enemy->closest].effectpos[0];
            missilePosition[1] = gPlayers[enemy->closest].effectpos[1];
            missilePosition[2] = gPlayers[enemy->closest].effectpos[2];
        } else {
            missilePosition[0] = enemy->objgrp.coll_pos[0];
            missilePosition[1] = enemy->objgrp.coll_pos[1];
            missilePosition[2] = enemy->objgrp.coll_pos[2];
            missilePosition[0] = (f32)(20.0 * sin(enemy->pyr[1]) +
                                       missilePosition[0]);
            missilePosition[2] = (f32)(20.0 * cos(enemy->pyr[1]) +
                                       missilePosition[2]);
        }
        fn_8004E448(enemy, (s32)missilePosition,
                    enemy->objgrp.coll_pos);
        enemy->flag1 = 1;
        enemy->attack_flag &= ~0x10;
    }
}

/* Select and launch an enemy missile, then dispatch its positional sound. */
void fn_8004E448(Enemy* enemy, s32 arg, f32* pos)
{
    s32 kind;

    if (enemy->algorithm == 28 || enemy->algorithm == 29) {
        kind = 2;
    } else if (enemy->algorithm == 16 || enemy->algorithm == 23) {
        kind = 0;
    } else if (enemy->algorithm == 17 || enemy->algorithm == 26) {
        kind = 1;
    } else {
        kind = 2;
    }

    if (EnemyStartMissile(enemy, arg, pos, kind) != 0) {
        if (kind == 0) {
            fn_8009DE2C(pos);
        }
        if (kind == 2) {
            if (enemy->type == 2) {
                fn_8009DCE4(pos);
            } else {
                fn_8009DDFC(pos);
            }
        }
    }
}

/* Advance an enemy after it reaches its assigned milestone. */
void update_enemy_milestone(Enemy* enemy)
{
    u8 frame_pad[12];
    f32 milestone[3];
    f32 vertical;
    u8 local_pad[12];
    f32 dx;
    f32 dz;

    if (enemy->plr_ms >= 0) {
        GetMilestonePos(enemy->plr_ms, milestone);
        dx = enemy->objgrp.worldmat[3][0] - milestone[0];
        dz = enemy->objgrp.worldmat[3][2] - milestone[2];
        vertical = enemy->objgrp.worldmat[3][1] - milestone[1];
        *(u32*)&vertical &= 0x7FFFFFFF;
        if ((f64)vertical < 2.5 &&
            (f64)fqdist(dx, dz) < 1.4) {
            adjust_msidx(enemy);
            enemy->plr_ms = -1;
            enemy->operation_count = enemy->operation_speed;
            if (enemy->ms_idx > 0) {
                if (enemy->ms_idx > 0) {
                    enemy->ms_idx--;
                }
                enemy->max_msidx = enemy->ms_idx;
            }
            if (enemy->algorithm == 10) {
                enemy->collided = 0;
                enemy->route = 0;
                enemy->stuck_count = 0;
            }
        }
    }
}

/* Track this enemy's target milestone in the player's recent-history ring. */
void adjust_msidx(Enemy* enemy)
{
    Player* player = &gPlayers[enemy->closest];
    s32 i;

    for (i = 0; i < 5; i++) {
        if (enemy->plr_ms == player->milestone[i]) {
            break;
        }
    }
    if (i < 5) {
        enemy->ms_idx = i;
        if (enemy->max_msidx < enemy->ms_idx) {
            enemy->max_msidx = enemy->ms_idx;
        }
    } else {
        enemy->ms_idx = 0;
        enemy->max_msidx = 4;
        enemy->plr_ms = -1;
    }
}

/* Update texture animations for the loaded enemy types. GC reads the
 * eight-slot enemy_type array at .bss +0x20, then the corresponding animation
 * header at .bss +0x564. PDB names/types and AllocEnemy/LoadEnemy/ResetEnemies
 * corroborate the two distinct arrays. The target's shared section base does
 * not imply an enclosing source object. */
void enemy_update(void)
{
    s32 i;
    s32 idx;

    if ((gGameBusy | gGameplayPauseTimer) == 0) {
        for (i = 0; i < lbl_8034471C; i++) {
            idx = enemy_type[i];
            if (((void**)gWadAtreeHeaders)[idx] != 0) {
                DoTexMods(((void**)gWadAtreeHeaders)[idx]);
            }
        }
    }
}

/* The Xbox PDB names this helper get_enemy_fight(type, health), and the Xbox
 * executable computes both the base fight value and its health thresholds in
 * the helper.  The GameCube compiler inlines it into damage_enemy. */
static inline f32 get_enemy_fight(e_e_tpye type, f32 health)
{
    f32 fight;
    f32 l3;
    f32 lower;
    f32 upper;
    fight = gCurLevel->ene_damage * lbl_8011B900[type];
    l3 = gCurLevel->ene_health * lbl_8011BA10[type];
    upper = (f32)(0.667 * l3);
    lower = (f32)(0.333 * l3);

    if (health > upper) {
        goto done;
    }
    if (type == E_DEATH) {
        return fight;
    }
    if (health > lower) {
        return (f32)(0.667 * fight);
    }
    return (f32)(0.333 * fight);
done:
    return fight;
}

/* Apply damage and accumulated hit direction, then run the enemy-specific
 * heal, reaction, death, sound, skin and burst-effect cascades. */
s32 damage_enemy(Enemy* e, f32 amount, s32 player_index, s32 damage_type,
                 s32 effect_position_arg, s32 hit_direction_arg,
                 s32 play_effects)
{
    Player* player = NULL;
    f32* effect_position = (f32*)effect_position_arg;
    f32* hit_direction = (f32*)hit_direction_arg;
    f32 old_health = e->health;
    f32 effect_pos[3];
    u8 unused1[4];
    f32 saved_matrix[16];
    s32 enemy_index;

    if (e->state == DECORATION) {
        return -1;
    }
    if (e->state == DYING) {
        return -1;
    }
    if (e->type == E_IT) {
        return -1;
    }

    if (player_index >= 0) {
        player = &gPlayers[player_index];
    }
    if (player != NULL) {
        lbl_803447E4 = 1;
    }

    if (e->type == E_DEATH) {
        if ((damage_type & 0x200) != 0) {
            if (player != NULL && player->level > 75) {
                f32 heal_scale = (f32)(0.032 *
                    (f64)(player->level - 75) + 0.2);
                heal_player(player, e->health * heal_scale);
            }
            e->health = 0.0f;
        } else if (e->state == SLEEP) {
            if (play_effects != 0) {
                fn_8009DE5C(e->type, &e->objgrp.worldmat[3][0]);
            }
            {
                s16 endurance = e->endurance - 1;
                e->endurance = endurance;
                if (endurance <= 0) {
                    e->state = ACTIVE;
                    if (play_effects != 0) {
                        AudioPlayEvt103(&e->objgrp.worldmat[3][0]);
                    }
                    CopyMat4(&e->objgrp.worldmat[0][0], saved_matrix);
                    SetEnemyObj(e, e->type, 1);
                    CopyMat4(saved_matrix, &e->objgrp.worldmat[0][0]);
                    UpdateObjWorldMat(&e->objgrp.worldmat[0][0]);
                    fn_8005A404(&e->objgrp.worldmat[0][0], e->coll_offset,
                                 e->attn_offset);
                    MBTreeClearFlags(e->objgrp.node, 2, 0);
                }
            }
            return 0;
        } else if (player != NULL && (player->shield_flags & 0x80000) != 0) {
            f64 one;

            e->health = (f32)(e->health - (one = 1.0));
            if (e->org_lvl == 2) {
                AddExp(player_index, 1, -2);
            } else {
                player->health = (f32)(player->health + one);
            }
        } else {
            e->health = (f32)(e->health - 1.0);
            if (player != NULL) {
                msgPost(0, player_index, player->col_pos);
            }
        }

        if (e->health <= 0.0f) {
            if (play_effects != 0) {
                AudioPlayEvt101(&e->objgrp.worldmat[3][0]);
            }
            e->health = 0.0f;
            enemy_index = (s32)(e - gEnemies);
            e->state = DYING;
            e->area = (s16)player_index;
            if (e->algorithm == 18) {
                SuicideExplosion(e->objgrp.coll_pos,
                    (f32)(50.0 * gCurLevel->ene_damage));
                fn_8009DAC8(e->objgrp.coll_pos);
            }
            uncouple_enemy(enemy_index);
            if (player != NULL) {
                player->save.stats[player->character].enemies_killed++;
            }
            return 1;
        }
        return 0;
    }

    e->watchdog = 0;
    if ((damage_type & 0x800000) != 0 && player != NULL) {
        f32 healed = amount;
        if (healed > e->health) {
            healed = e->health;
        }
        do_heal_players(player, &e->objgrp.worldmat[0][0], healed);
    }

    if (player_index >= 0 && gCurLevel->plevel > 0.0f) {
        f32 level = (f32)player->level;
        f32 target_level = gCurLevel->plevel;
        f32 scale = 1.0f;

        if (level < target_level) {
            scale = (f32)(1.0 -
                          0.01 * (target_level - level));
        } else if (level > target_level) {
            scale = (f32)(1.0 +
                          0.1 * (level - target_level));
        }
        amount *= scale;
    }

    {
        u32 shield = e->atts.armortype;
        ModifyDamage(e->atts.armor, &amount, (u32*)&damage_type, shield);
    }
    if ((f64)lbl_803447D8 < 1.0) {
        amount *= 2.0;
    }
    if (player_index >= 0 &&
        (f64)amount < 1.0) {
        amount = 1.0f;
    }

    e->damage += amount;
    if ((damage_type & 0xF) != 0) {
        e->damagetype &= ~0xF;
    }
    e->damagetype |= damage_type;
    if (hit_direction != NULL) {
        e->damagedir[0] = hit_direction[0] + e->damagedir[0];
        e->damagedir[1] = hit_direction[1] + e->damagedir[1];
        e->damagedir[2] = hit_direction[2] + e->damagedir[2];
    }

    effect_pos[0] = e->objgrp.attn_pos[0];
    effect_pos[1] = e->objgrp.attn_pos[1];
    effect_pos[2] = e->objgrp.attn_pos[2];

    if (e->type == E_GOLEM) {
        if (lbl_80344768 >= 3) {
            amount *= 0.75;
        } else if (lbl_80344768 >= 2) {
            amount *= 0.5;
        }
    }
    if (amount <= 0.0f) {
        play_effects = 0;
    } else {
        e->damage_count++;
    }

    {
        f64 applied;

        if (gGameOptions[0] == 3) {
            applied = 10000.0;
        } else {
            applied = amount;
        }
        e->health = (f32)((f64)e->health - applied);
    }

    e->atts.fight = get_enemy_fight(e->type, e->health);

    if (e->algorithm == 12 && e->mode1 < 2 && e->generator != NULL) {
        ((u8*)e->generator)[0xE6] = 7;
        ((u8*)e->generator)[0xE0] = 3;
    } else if (e->algorithm == 15 && e->generator != NULL) {
        ((u8*)e->generator)[0xE3] = 0;
    }

    if ((f64)e->health <= 0.0) {
        if (e->type == gBossType) {
            if ((f64)old_health > 0.0 && player != NULL) {
                player->save.stats[player->character].enemies_killed++;
            }
            return 1;
        }

        if (play_effects != 0) {
            if (e->algorithm == 18) {
                fn_8009DD48();
            }
            fn_8009DF7C(e, play_effects);
        }
        e->health = 0.0f;
        enemy_index = (s32)(e - gEnemies);
        e->state = DYING;
        e->area = (s16)player_index;
        if (e->algorithm == 18) {
            SuicideExplosion(e->objgrp.coll_pos,
                (f32)(50.0 * gCurLevel->ene_damage));
            fn_8009DAC8(e->objgrp.coll_pos);
        }
        uncouple_enemy(enemy_index);
        if (player != NULL) {
            player->save.stats[player->character].enemies_killed++;
        }

        if (player_index >= -1) {
            if (e->objgrp.node != NULL) {
                if (e->type == E_GOLEM && (damage_type & 0xF) == 0) {
                    SetSkinFX(&e->skinfx, lbl_80344BE4, 15, 0,
                              0.5f);
                } else if (e->type == E_TREEFOLK &&
                           (damage_type & 0xF) == 0) {
                    SetSkinFX(&e->skinfx, lbl_80344BE0, 10, 0,
                              0.5f);
                } else if (e->type == E_KNIGHT &&
                           (damage_type & 0xF) == 0) {
                    SetSkinFX(&e->skinfx, lbl_80344BE0, 10, 0,
                              0.5f);
                } else if ((f64)e->hht > 2.0) {
                    SetSkinFX(&e->skinfx,
                              lbl_802897B8[damage_type & 0xF], 10, 0,
                              0.5f);
                }
                MBTreeSetAmbientAdd(e->objgrp.node, 999, 1);
            }
            if ((damage_type & 0x1000000) == 0 && e->type != gBossType) {
                fn_800945D0((u8*)effect_pos, (u8*)&e->objgrp,
                            damage_type, 1, e->type, e->hht);
            }
        }
        return 1;
    }

    if (play_effects != 0) {
        fn_8009DE88(e, play_effects);
    }
    if ((damage_type & 0x1000000) == 0 && e->type != gBossType) {
        if (effect_position != NULL && (f64)e->hht >= 4.0) {
            effect_pos[0] = *(f32*)((u8*)effect_position + 0);
            effect_pos[1] = *(f32*)((u8*)effect_position + 4);
            effect_pos[2] = *(f32*)((u8*)effect_position + 8);
        }
        fn_800945D0((u8*)effect_pos, (u8*)&e->objgrp,
                    damage_type, 0, e->type, e->hht);
    }
    return 0;
}

void kill_enemy(s32 index)
{
    Enemy* e = &gEnemies[index];
    struct Item* item = 0;
    s32 carried = 0;
    char* p;
    char buf[32];

    if (gTriggerCameraState != 0) {
        return;
    }
    if (e->gotitem != 0) {
        item = e->gotitem;
        e->gotitem = 0;
        carried = 1;
    } else {
        switch (e->type) {
        case E_GARGOYLE:
            sprintf(buf, "GARG%s", fn_80057ACC(e->type));
            for (p = buf; *p != 0; p++) {
                *p = toupper(*p);
            }
            item = (struct Item*)PlaceItem(1, 16, buf, 0);
            break;
        }
    }
    if (item != 0) {
        if (carried != 0) {
            *((u8*)item + 205) = 10;
            StartBagFX(e->objgrp.attn_pos, item, 0.0f);
        } else {
            *((u8*)item + 205) = 0;
            MBTreeClearFlags(*(struct mbnode**)((u8*)item + 100), 2, 0);
            if (**(s32**)item == 1) {
                *(s16*)((u8*)item + 236) = 60;
            }
            *(f32*)((u8*)item + 52) = e->objgrp.worldmat[3][0];
            *(f32*)((u8*)item + 56) = e->objgrp.worldmat[3][1];
            *(f32*)((u8*)item + 60) = e->objgrp.worldmat[3][2];
            AddItemSub(item);
        }
    }
    if (e->type == 27) {
        fn_8004F1DC(e);
    }
    e->health = 0.0f;
    e->state = 0;
    del_target(&e->objgrp.worldmat[0][0]);
    if (e->shadow != 0) {
        MBRemoveNode(e->shadow, 0);
        e->shadow = 0;
    }
    if (e->specialfx >= 0) {
        e->specialfx = DeleteEffect(e->specialfx, 1);
    }
    if (e->type == 27) {
        SfxDeleteParented(e->objgrp.node, 0, -1);
    }
    AtreeDelete(&e->atree);
    lbl_80344734 = 1;
    MBRemoveNode(e->objgrp.node, 0);
    lbl_80344734 = 0;
    e->objgrp.node = 0;
    uncouple_enemy(index);
}

/* Point the Garm death effect toward its target (or the first active player). */
void fn_8004F1DC(Enemy* enemy)
{
    volatile f32 enemyPos[3];
    f32 matrix[12];
    f32 direction[3];
    s32 i;
    Player* player = NULL;

    if (enemy->closest >= 0) {
        player = &gPlayers[enemy->closest];
    } else {
        for (i = 0; i < 4; i++) {
            if (gPlayers[i].state == 1) {
                break;
            }
        }
        if (i < 4) {
            player = &gPlayers[i];
        }
    }

    if (player != 0) {
        enemyPos[0] = enemy->objgrp.worldmat[3][0];
        enemyPos[1] = enemy->objgrp.worldmat[3][1];
        enemyPos[2] = enemy->objgrp.worldmat[3][2];
        direction[0] = player->pos[0] - enemyPos[0];
        direction[1] = player->pos[1] - enemyPos[1];
        direction[2] = player->pos[2] - enemyPos[2];
        NormalVector(direction);
        CreateDirMatrix(matrix, direction, 0);
        StartEnemyDeathFX(matrix);
    }
}

/* uncouple_enemy: detach enemy `index` from its generator's spawn list.
 * The prev_enemy/next_enemy relink below is transcribed from the verified GC
 * asm (uncouple_enemy @0x8004F2D8) and exercises the reconstructed Enemy
 * fields; the generator-record fixup (item*) is left as a comment because the
 * item struct belongs to another module.  NonMatching: shipped bytes come from
 * the original DOL. */
void uncouple_enemy(s32 index) {
    Enemy* e = &gEnemies[index];

    if (e->prev_enemy >= 0) {
        gEnemies[e->prev_enemy].next_enemy = e->next_enemy;
        e->prev_enemy = -1;
    }
    if (e->next_enemy >= 0) {
        gEnemies[e->next_enemy].prev_enemy = e->prev_enemy;
        e->next_enemy = -1;
    }
    if (e->algorithm == E_DOG) {
        lbl_80344748 = -1;
        e->algorithm = -e->algorithm;
    }
    if (e->generator != 0) {
        if (((EnemyGenerator*)e->generator)->first_enemy == index) {
            ((EnemyGenerator*)e->generator)->first_enemy =
                (s8)e->prev_enemy;
        }
        if (e->algorithm == 15) {
            ((EnemyGenerator*)e->generator)->flag_e3 = 0;
            ((EnemyGenerator*)e->generator)->live_count = 0;
        }
        if (((EnemyGenerator*)e->generator)->info->type == 3) {
            if (((EnemyGenerator*)e->generator)->live_count > 0) {
                ((EnemyGenerator*)e->generator)->live_count--;
            }
        } else {
            ErrorPrintf("Enemy has non generator generator", e->generator);
        }
        e->generator = 0;
    }
}

/* Test a proposed enemy location and hide the slot again when it is blocked. */
s32 check_vacancy(s32 index, f32* pos)
{
    Enemy* enemy = &gEnemies[index];
    f32 adjusted[3];

    adjusted[0] = pos[0];
    adjusted[1] = pos[1];
    adjusted[2] = pos[2];
    adjusted[1] += enemy->coll_offset[1];

    if (check_enemy_pos(adjusted, 0, index) <= 0) {
        MBTreeSetFlags(enemy->objgrp.node, 2, 0);
        if (enemy->shadow != 0) {
            MBTreeSetFlags(enemy->shadow, 2, 0);
        }
        enemy->state = INACTIVE;
        return 0;
    }
    return -1;
}

/* Addressing view over the existing enemy BSS symbols, not new storage.
 * Offsets are relative to mbdesc; the arrays below start at +0x348,
 * +0x3FC, +0x4B0, +0x564 and +0xE18 respectively. Keeping the actual pool
 * owner explicit avoids a second compiler-created base for the spawn path. */
typedef struct EnemySpawnPoolView {
    u8 _000[0x348];
    s32 lbl_80251148[45];
    s32 lbl_802511FC[45];
    s32 lbl_802512B0[45];
    u32 gWadAtreeHeaders[0x8B4 / 4];
    Enemy gEnemies[25];
} EnemySpawnPoolView;

/* generate_enemy @0x8004F4B4 (global).  Spawn an enemy of `type` at `pos`:
 * validate world/boss state and per-type limits, resolve random types
 * (-2/-3), take a slot, then for generator spawns search the 8 (or 2)
 * directions around the generator for a free position; finish by claiming
 * the grid cell, starting the E_START anim and the generator fx.
 *
 * The target retains the BSS row address and its advance to the gEnemies
 * member as one register lifetime.  MWCC's default lifetime split folds the
 * member offset into a temporary and changes four words; this local pragma
 * preserves the target's single cursor without changing the TU-wide flags. */
#pragma opt_lifetimes off
s32 generate_enemy(f32* pos, s32 type, s32 level, f32* dir, s32 spew,
                   struct Item* gen, s32 imp, f32 ang)
{
    u8* tbl = (u8*)lbl_8011AF48;
    EnemySpawnPoolView* pool = (EnemySpawnPoolView*)mbdesc;
    Enemy* e;
    s32 slot;
    s32 otype;
    s32 mask = 0;
    s32 ndirs;
    s32 d;
    s32 start;
    s32 i;
    s32 r;
    f32 startv[3];
    f32 out[3];
    f32 v[3];

    if (gGameMode == MA_HSTABLE) {
        return -1;
    }
    if (lbl_803447DC != 0 && gen != 0) {
        return -4;
    }
    if (gBossType >= 0 && gBossDying != 0) {
        return -4;
    }
    spew = fn_8004F87C(type, level, spew);
    otype = type;
    if (type == -2) {
        RandInt(4);
        level = 2;
        i = lbl_8034472C;
        lbl_8034472C = i + 1;
        type = *(s32*)(tbl + ((i & 3) << 2) + 4284);
        spew = *(s32*)(tbl + ((i & 3) << 2) + 4300);
    } else if (type == -3) {
        RandInt(4);
        level = 3;
        i = lbl_8034472C;
        lbl_8034472C = i + 1;
        type = *(s32*)(tbl + ((i & 3) << 2) + 4316);
        spew = *(s32*)(tbl + ((i & 3) << 2) + 4332);
    } else if (type < 0) {
        return -6;
    }
    if (type != 30 && type != 31) {
        if (pool->lbl_802512B0[type] < 0) {
            return -5;
        }
        if (pool->lbl_802511FC[type] == 4 && level < 4) {
            return -5;
        }
    }
    slot = find_enemy_slot(type, imp);
    if (slot < 0) {
        return -2;
    }
    init_enemy(slot, pos, type, level, spew);
    e = (Enemy*)((u8*)pool + slot * sizeof(Enemy));
    e = (Enemy*)((u8*)e + offsetof(EnemySpawnPoolView, gEnemies));
    e->generator = gen;
    if (gen == 0 || type == 30) {
        e->genang_offset = 0.0f;
    } else {
        switch (otype) {
        case 1:
        case 4:
        case 5:
        case 7:
        case 8:
        case 10:
        case 11:
        case 14:
        case 15:
        case 19:
        case 24:
        case 25:
            mask = 0xFFCE;
            ndirs = 8;
            break;
        case 18:
            ndirs = 2;
            break;
        case 0:
        case 2:
        case 3:
        case 6:
        case 12:
        case 13:
        case 16:
        case 17:
        case 20:
        case 21:
        case 22:
        case 23:
            ndirs = 8;
            break;
        default:
            ndirs = 8;
            break;
        }
        if (spew == 12) {
            ndirs = 1;
        }
        startv[0] = pos[0];
        startv[1] = pos[1];
        startv[2] = pos[2];
        startv[1] = startv[1] + e->coll_offset[1];
        switch (type) {
        case 17:
            ang = 0.0f;
            break;
        default:
            ang = ang + e->rad;
            break;
        }
        v[0] = dir[0] * ang;
        v[1] = 0.0f;
        v[2] = dir[2] * ang;
        start = RandInt(ndirs);
        d = start;
        do {
            if ((mask & 0xFFFF & (1 << d)) == 0) {
                e->genang_offset = gendir_8004FBC8(v, out, d);
                r = check_enemy_pos(startv, out, slot);
                if (r > 0) {
                    goto placed;
                }
                if (r < 0) {
                    mask |= 1 << d;
                }
            }
            d = (d + 1) % ndirs;
        } while (d != start);
        MBTreeSetFlags(e->objgrp.node, 2, 0);
        if (e->shadow != 0) {
            MBTreeSetFlags(e->shadow, 2, 0);
        }
        e->state = 0;
        return -3;
    }
placed:
    UpdateObjWorldMat(&e->objgrp.worldmat[0][0]);
    fn_8005A404(&e->objgrp.worldmat[0][0], e->coll_offset, e->attn_offset);
    e->action = 1;
    {
        u32 animation = e->actionlist[1].animidx;
        /* Animation indices use a negative signed sentinel for no sequence. */
        if ((s32)animation >= 0) {
            InitAnim(0.0f, &e->atree.animinfo, animation, 0, 1);
        }
    }
    if (e->hht > 2.0 && level <= 3 && pool->lbl_80251148[type] != 0) {
        StartGenFX(pos, level);
    }
    return slot;
}
#pragma opt_lifetimes reset

/* Resolve the generator/spew class shared by groups of enemy types. */
s32 fn_8004F87C(s32 type, s32 level, s32 spew)
{
    switch (type) {
    case 0:
    case 3:
    case 6:
    case 9:
    case 12:
    case 15:
    case 18:
    case 21:
    case 22:
        if (spew != 2 && spew != 4) {
            spew = RandInt(1) != 0 ? 4 : 2;
        }
        break;
    case 2:
    case 7:
    case 14:
    case 17:
    case 24:
        if (spew == 0) {
            switch (level) {
            default:
                spew = 7;
                break;
            case 3:
                spew = 30;
                break;
            case 4:
                spew = 23;
                break;
            case 5:
                spew = 17;
                break;
            case 6:
                spew = 18;
                break;
            }
        }
        break;
    case 1:
    case 4:
    case 5:
    case 8:
    case 10:
    case 11:
    case 13:
    case 16:
    case 19:
    case 20:
    case 25:
    case 32:
    case 33:
        if (spew == 0) {
            switch (level) {
            default:
                spew = 7;
                break;
            case 4:
                spew = 23;
                break;
            case 5:
                spew = 17;
                break;
            case 6:
                spew = 18;
                break;
            }
        }
        break;
    case 27:
        spew = 31;
        break;
    case 30:
        spew = 3;
        break;
    case 31:
        spew = 27;
        break;
    case 29:
        spew = 19;
        break;
    }
    return spew;
}

s32 check_enemy_pos(f32* start, f32* out, s32 slot)
{
    Enemy* e = &gEnemies[slot];
    f32 rad = e->rad;
    f32 hht = e->hht;
    f32 pos[3];
    u8 _ppad[4];
    f64 half;
    void* obj;
    s32 grounded;

    if (out != NULL) {
        pos[0] = out[0] + start[0];
        pos[1] = out[1] + start[1];
        pos[2] = out[2] + start[2];
        if (EnemyWallCollide(rad, start, pos, 0) != 0) {
            return -1;
        }
    } else {
        pos[0] = start[0];
        pos[1] = start[1];
        pos[2] = start[2];
    }
    e->objgrp.worldmat[3][0] = pos[0];
    e->objgrp.worldmat[3][1] = pos[1];
    e->objgrp.worldmat[3][2] = pos[2];
    if (FloorCollide(pos, 0, 0, 2, 0.1f, 4.0f, (-10.0f))
        != 0) {
        grounded = 1;
    } else {
        grounded = 0;
    }
    if (grounded == 0) {
        return -1;
    }
    {
        f32 floorY = gFloorCollisionResult.mtx[3][1];
        f32 dy = floorY - start[1];
        u8 _dpad[8];

        *(u32*)&dy &= 0x7FFFFFFF;
        if (dy > 6.0) {
            return -1;
        }
        e->objgrp.worldmat[3][1] = floorY;
    }
    fn_8005A65C(&e->objgrp.worldmat[0][0], e->coll_offset);
    if (fn_80046680(slot, 1, start, pos, rad, hht) >= 0) {
        return 0;
    }
    half = 0.5 * rad;
    if (fn_8004646C(slot, start, pos, 0, (f32)half, hht, 0) >= 0) {
        return 0;
    }
    obj = fn_8005EFAC((f32)half, start, pos, 0, 0);
    if (obj != NULL && obj != e->generator) {
        if (fn_8005D3D8(-1, obj) != 0) {
            return 0;
        }
    }
    return 1;
}

/* Rotate a horizontal direction into one of the eight generator octants. */
static f32 gendir_8004FBC8(f32* input, f32* output, s32 direction)
{
    f32 x = input[0];
    f32 z = input[2];

    output[1] = input[1];
    switch (direction) {
    default:
        output[0] = x;
        output[2] = z;
        return 0.0f;
    case 1:
        output[0] = -x;
        output[2] = -z;
        return 3.1415927f;
    case 2:
        output[0] = -z;
        output[2] = x;
        return (-1.5707964f);
    case 3:
        output[0] = z;
        output[2] = -x;
        return 1.5707964f;
    case 4:
        output[0] = 0.707 * x + 0.707 * z;
        output[2] = 0.707 * -x + 0.707 * z;
        return 0.7853982f;
    case 5:
        output[0] = 0.707 * -z + 0.707 * x;
        output[2] = 0.707 * z + 0.707 * x;
        return (-0.7853982f);
    case 6:
        output[0] = 0.707 * z + 0.707 * -x;
        output[2] = 0.707 * -z + 0.707 * -x;
        return 2.3561945f;
    case 7:
        output[0] = 0.707 * -x + 0.707 * -z;
        output[2] = 0.707 * x + 0.707 * -z;
        return (-2.3561945f);
    }
}

/* find_enemy_slot: return a free/recyclable enemy slot for a new spawn.
 * Recycles the least-important live enemy through kill_enemy when the array is
 * full; returns -1 when the request cannot be satisfied. */
s32 find_enemy_slot(s32 type, s32 level) {
    s32 enemy_state;
    Enemy* enemy;
    s32 index;
    s32 best_visible;
    s32 visible;


    f32 distance;
    f32 best_distance;
    s32 best_index;
    s32 count;

    best_visible = 1;
    index = 0;
    best_index = 0;
    count = gNumEnemies;
    best_distance = 0.0f;

    enemy = gEnemies;

    for (index = 0; index < count; index++, enemy++) {
        enemy_state = enemy->state;

        if (enemy_state == INACTIVE) {
            return index;
        }
        if (enemy->type != E_IT) {
            distance = enemy->actual_dist;
            visible = enemy->visactive;

            if (enemy_state == DYING || enemy_state == SLEEP ||
                enemy->birth_style != 0) {
                distance *= 0.01;
            } else if (visible == 0) {
                distance += 10000.0;
            }
            if (distance > best_distance) {
                best_distance = distance;
                best_index = index;
                best_visible = visible;
            }
        }
    }

    default_gen_count = 1;
    if (type < E_NTYPES && level < best_visible) {
        return -1;
    }
    if (best_visible != 0) {
        lbl_80344728++;
    }
    kill_enemy(best_index);
    {
        Enemy* k = (Enemy*)((u8*)gEnemies + best_index * 916);


        k->close_dist = 1.0f;
        k->actual_dist = 1.0f;
    }
    return best_index;
}

/* 0x8004FE34 - initialise a freshly claimed enemy slot's object state.
 *
 * Zeroes the attention/collision offsets and orientation, takes the type's
 * attention and collision heights from the per-type tables, hands the slot to
 * SetEnemyObj to build the model and animation tree, then derives the starting
 * hit points: the type's base health, scaled by the level's ene_health rate for
 * everything but Death, and by 0.333 * difficulty for the ordinary (non-boss)
 * types.  If SetEnemyObj produced a scene node the enemy is dropped onto the
 * floor at `pos`, its per-type variables are initialised, and its shadow node
 * is parked underneath it.
 */
/* init_enemy followed do_enemies in the pre-reorder file and so compiled under
 * the `#pragma opt_propagation off` that do_enemies' body opens (its bare
 * `#pragma reset` does not close it).  The reorder moves init_enemy out of that
 * region, so the bracket is now explicit here. */
#pragma opt_propagation off
void init_enemy(s32 slot, f32* pos, s32 type, s32 level, s32 spew)
{
    Enemy* e = &gEnemies[slot];
    u8* tbl = (u8*)lbl_8011AF48;
    s32 toff;
    f32 zero;
    f32 health;

    e->type = type;
    toff = type * 4;
    zero = 0.0f;
    e->attn_offset[0] = zero;
    e->attn_offset[1] = *(f32*)(tbl + toff + ETYPE_ATTN_Y);
    e->attn_offset[2] = zero;
    e->coll_offset[0] = zero;
    e->coll_offset[1] = *(f32*)(tbl + toff + ETYPE_COLL_Y);
    e->coll_offset[2] = zero;
    e->pyr[0] = zero;
    e->pyr[1] = zero;
    e->pyr[2] = zero;
    e->state = ACTIVE;
    e->endurance = 0;
    SetEnemyObj(e, type, level);
    if (level > 3) {
        level = 2;
    }
    if (spew == 18) {
        level = 1;
    }
    /* Kept as a two-step walk instead of the obvious
     * `health = *(f32*)(tbl + toff + ETYPE_BASE_HEALTH);`: the target
     * re-materialises the table row after the SetEnemyObj call (`add r3,r29,r28`
     * at 0xb4), and the flat single-expression form lets MWCC reuse the entry
     * region's row register instead, deleting that `add` and cascading an
     * entry-schedule rewrite - measured real 27 -> 76, opcode multiset DIFFERS. */
    {
        u8* r = tbl;
        r += toff;
        health = *(f32*)(r + ETYPE_BASE_HEALTH);
    }
    if (type != E_DEATH) {
        health = health * gCurLevel->ene_health;
    }
    if (type < E_NTYPES) {
        f64 scaled = 0.333 * health;
        health = (f32)(scaled * level);
    }
    fn_8005A338(&e->objgrp.worldmat[0][0], e->coll_offset, e->attn_offset);
    if (e->objgrp.node != NULL) {
        e->objgrp.worldmat[3][0] = pos[0];
        e->objgrp.worldmat[3][1] = pos[1];
        e->objgrp.worldmat[3][2] = pos[2];
        e->objgrp.worldmat[3][1] = FloorPos(lbl_80344880, 0.1f, pos, 2);
        MBTreeClearFlags(e->objgrp.node, 2, 0);
        e->health = health;
        init_enemy_vars(slot, health, spew);
        if (type == E_DEATH) {
            e->org_lvl = level;
        }
        e->objgrp.worldmat[3][1] = e->objgrp.worldmat[3][1] + e->flooroffset;
    }
    UpdateObjWorldMat(&e->objgrp.worldmat[0][0]);
    fn_8005A404(&e->objgrp.worldmat[0][0], e->coll_offset, e->attn_offset);
    e->floory = e->objgrp.worldmat[3][1];
    if (e->shadow != NULL) {
        /* Park the shadow on the body node's world translation. The target
         * reloads both node fields for each component; ordinary mbnode field
         * access preserves those loads without unrelated pointer casts. */
        e->shadow->mat[3][0] = e->objgrp.node->mat[3][0];
        e->shadow->mat[3][1] = e->objgrp.node->mat[3][1];
        e->shadow->mat[3][2] = e->objgrp.node->mat[3][2];
    }
    if (e->atree.root != NULL) {
        AnimateATree(&e->atree, 0, 2);
    }
}
#pragma opt_propagation reset

/* Enemy loading, targeting and milestone tail: recovered TU ownership. */
extern s32  lbl_802577CC[];
extern s32   lbl_8034476C;
extern s32   gNumPlayers;
extern s32   lbl_80344760;
extern s32   lbl_80344738;
extern s32   lbl_80344750;
extern s32   lbl_8034474C;
extern s32   gSceneRoot;
extern f32   gIdentityMatrix[];
extern void* sGoodWizObj;
extern char* lbl_8011BFF8[];
extern u8    lbl_80126EC0[];
extern s32   stricmp(const char* a, const char* b);
extern void* MBNewNode(s32 parent, void* tmpl, s32 arg2);
extern s32   fn_80011BBC(void* model, const char* name, void* atreeOut,
                         const char* work, s32 workSize);
extern void  InitActions(void* atree, void* actionList, void* actionTable);
extern void* MBOX_NewObject(const char* name, f32* matrix, void* parent, u32 flags);
extern void* MBOX_ReallyFindObject(const char* name, s32 type1, s32 type2, s32 exact);
extern void* MBNewObject(void* object, f32* matrix, void* parent, u32 flags);
extern s32   lbl_80344800;
extern s32   lbl_802577AC[];
extern void  InitEnemyMissiles(s32 idx);
extern s32   fn_8005A1EC(const char* name, void** outData);
extern s32   LoadModel(const char* name, void** outData, s32 initTexMods, s32 model);
extern void  FatalErrorf(const char* fmt, ...);
extern void  InitTexMods(void* tex, s32 arg1);
extern f32 gDefaultPlayerPosition[3];
/* Level variants 4..7 use the retail suffixes A/B/S/F, not a numeric level. */
char lbl_80343BF8[5] = "ABSF";
extern void StartEnemyGrid(f32* pos, f32 radius);
extern s32 NextGridEnemy(void);
extern void fn_800520C8(void);
extern s32 fn_80051480(f32* pos);
extern char* fn_80051E1C(s32 world, s32 lvl, s32 flag);
#define DIST3(dst, av, bv, kZ, kH, kT)     {         f32 dy_ = (av)[1] - (bv)[1];         f32 dx_ = (av)[0] - (bv)[0];         f32 dz_ = (av)[2] - (bv)[2];         (dst) = dx_ * dx_ + dy_ * dy_;         (dst) = dz_ * dz_ + (dst);         if ((dst) > (kZ)) {             volatile f32 tmp_;             f64 y_ = __frsqrte((dst));             y_ = (kH) * y_ * ((kT) - y_ * y_ * (dst));             y_ = (kH) * y_ * ((kT) - y_ * y_ * (dst));             y_ = (kH) * y_ * ((kT) - y_ * y_ * (dst));             tmp_ = (f32)((dst) * ((kH) * y_ * ((kT) - y_ * y_ * (dst))));             (dst) = tmp_;         }     }

static s32 PlayersAverageLevel(void)
{
    s32* player = (s32*)gPlayers;
    s32 activePlayers = 0;
    s32 totalLevel = 0;
    s32 i;

    for (i = 0; i < 4; i++) {
        if (player[58] == 1) {
            activePlayers++;
            totalLevel += player[3273];
        }
        player += 3287;
    }
    if (activePlayers == 0) {
        return 1;
    }
    return totalLevel / activePlayers;
}


static char* findWorldName(s32 world)
{
    EnemyTypeName* tbl = lbl_8011AF48;
    s32 i;

    for (i = 0; i < 44; i++) {
        if (world == tbl[i].type) {
            return tbl[i].desc;
        }
    }
    return 0;
}

/* The PDB identifies get_enemy_level(enum type, float health) -> int.
 * GC's caller expands its level-scaled health thresholds here. Keep the
 * operation together rather than passing cached thresholds to a made-up ABI. */
static inline int get_enemy_level(e_e_tpye type, f32 health)
{
    f32 full = gCurLevel->ene_health * lbl_8011BA10[type];
    f32 lower = (f32)(0.333 * full);
    f32 upper = (f32)(0.667 * full);
    if (health > upper) {
        return 3;
    }
    if (health > lower) {
        return 2;
    }
    if (health > 0.0f) {
        return 1;
    }
    return 0;
}

void init_enemy_vars(int slot, f32 scale, int spew)
{
    /* The real tier helper and direct field constants reproduce the native
     * frame without a local reservation or propagation override. */
    Enemy* enemy;
    s32 i4;
    s16 sv;

    enemy = &gEnemies[slot];
    enemy->skinfx.nframes = 0.0f;
    enemy->next_enemy = -1;
    enemy->prev_enemy = -1;
    enemy->action = 0;
    enemy->attack_timer = 0;
    enemy->stun_timer = 0;
    enemy->prev_closest = -1;
    enemy->closest = -1;
    enemy->sight = (f32)(30.0 * gCurLevel->ene_visrad);
    enemy->close_dist = 1.0f;
    enemy->actual_dist = 1.0f;
    enemy->hht = (f32)(0.5 * ene_height[enemy->type]);
    enemy->rad = ene_width[enemy->type];
    enemy->area = 0;
    enemy->coll_pnum = -1;
    enemy->coll_enenum = -1;
    enemy->coll_ip = NULL;
    enemy->floor_wobj = NULL;
    enemy->count = 0;
    enemy->moved = 0;
    enemy->stopped = 0;
    enemy->attack_index = -1;
    enemy->attack_count = 0;
    enemy->attack_flag = 0;
    enemy->damage = 0.0f;
    enemy->damagetype = 0;
    for (i4 = 0; i4 < 5; i4++) {
        enemy->fxhittime[i4] = 0.0f;
    }
    enemy->damagedir[0] = 0.0f;
    enemy->damagedir[1] = 0.0f;
    enemy->damagedir[2] = 0.0f;
    enemy->pushed[0] = 0.0f;
    enemy->pushed[1] = 0.0f;
    enemy->pushed[2] = 0.0f;
    enemy->pushang = 0.0f;
    enemy->push_cnt = 0;
    enemy->generator = NULL;
    enemy->anim_done = -1;
    enemy->idle_time = 1.0f;
    enemy->idle_secs = 0.0f;
    enemy->idle_frac = 0.0f;
    enemy->damage_count = 0;
    enemy->org_lvl = get_enemy_level(enemy->type, scale);
    enemy->mode2 = 0;
    enemy->mode1 = 0;
    enemy->flag2 = 0;
    enemy->flag1 = 0;
    enemy->counter2 = 0;
    enemy->counter1 = 0;
    enemy->recognized = 0;
    enemy->skip_itemcol = 0;
    enemy->prev_dir = (-999.9f);
    enemy->zspd = 0.0f;
    enemy->xspd = 0.0f;
    enemy->visible = 1;
    enemy->visactive = 1;
    enemy->gotitem = NULL;
    enemy->specialfx = -1;
    enemy->alpha = 0;
    if (spew == 1) {
        spew = 0;
    }
    if (spew == 10) {
        spew = 7;
    }
    if (spew < 0 || spew > 31) {
        enemy->algorithm = (s16)sEnemyDefaultAlgorithm[enemy->type];
    } else {
        enemy->algorithm = (s16)spew;
    }
    sv = enemy->algorithm;
    enemy->old_ai = sv;
    enemy->prev_ai = sv;
    /* Retail keeps the incoming slot in r3 through this call (+0x254). */
    format_brain(slot);
    enemy->atts.invspeed = (f32)(1.0 / lbl_8011B878[enemy->type]);
    enemy->atts.fight = get_enemy_fight(enemy->type, enemy->health);
    enemy->atts.armor = enemy_armor[enemy->type];
    enemy->atts.damagetype = enemy_damagetype[enemy->type];
    enemy->atts.armortype = enemy_armortype[enemy->type];
}

void format_brain(s32 index)
{
    Enemy* enemy = &gEnemies[index];

    enemy->view = 3.159046f;
    enemy->ai_flags = 0;

    switch (enemy->algorithm) {
    case 0:
        enemy->route = 1;
        enemy->collided = 0;
        break;
    case 3:
        enemy->route = 1;
        enemy->collided = 0;
        enemy->counter1 = 0;
        enemy->counter2 = -1;
        enemy->flag2 = 0;
        break;
    case 2:
    case 4:
        enemy->play = 0;
        enemy->ai_flags |= 1;
        break;
    case 5:
    case 6:
        enemy->ai_flags |= 1;
        break;
    case 7:
        enemy->route = 0;
        enemy->collided = 0;
        enemy->stuck_count = 0;
        break;
    case 8:
        enemy->route = 0;
        enemy->collided = 0;
        enemy->stuck_count = 0;
        enemy->guard_mode = 0;
        enemy->guard_closest = -1;
        enemy->guard_dist = 100000.0f;
        break;
    case 9:
        enemy->daction = 0;
        enemy->ai_flags |= 5;
        break;
    case 10:
        enemy->route = 0;
        enemy->collided = 0;
        enemy->stuck_count = 0;
        break;
    case 11:
        enemy->ai_flags |= 7;
        break;
    case 12: {
        f32 zero = 0.0f;
        enemy->dest[0] = zero;
        enemy->dest[1] = zero;
        enemy->dest[2] = zero;
        enemy->route = 0;
        enemy->collided = 0;
        enemy->stuck_count = 0;
        break;
    }
    case 13:
        enemy->route = 0;
        enemy->collided = 0;
        enemy->stuck_count = 0;
        break;
    case 14:
        enemy->route = 0;
        enemy->collided = 0;
        enemy->stuck_count = 0;
        enemy->counter1 = 0;
        break;
    case 16:
    case 23:
        enemy->flag2 = RandInt(10);
        enemy->counter1 = 0;
        break;
    case 17:
    case 26:
        enemy->flag2 = RandInt(10);
        enemy->counter1 = 0;
        break;
    case 20:
        enemy->route = 0;
        enemy->collided = 0;
        enemy->stuck_count = 0;
        break;
    case 21:
        enemy->counter1 = 0;
        break;
    case 24:
        enemy->counter1 = 0;
        break;
    case 27:
        enemy->route = 1;
        enemy->collided = 0;
        break;
    case 28:
    case 29:
    case 31:
        enemy->flag2 = RandInt(30);
        enemy->counter1 = 0;
        enemy->counter2 = 0;
        break;
    case 30:
        enemy->route = 1;
        enemy->collided = 0;
        enemy->counter1 = RandInt(60) + 60;
        break;
    }

    enemy->count = 0;
    enemy->dead_end = 0;
    enemy->plr_ms = -1;
    enemy->ms_idx = 0;
    enemy->max_msidx = 4;
    enemy->watchdog = 0;
    enemy->lv = PlayersAverageLevel();
    enemy->operation_speed = 8;
    enemy->operation_count = RandInt(enemy->operation_speed);
}

void SetEnemyObj(Enemy* enemy, s32 type, s32 level)
{
    void* object;
    s32 shadowObject;
    s32 shadowLevel;

    if (enemy->type == 27) {
        SfxDeleteParented(enemy->objgrp.node, 0, -1);
    }
    if (enemy->atree.root != 0) {
        AtreeDelete(&enemy->atree.root);
    }
    if (enemy->objgrp.node != 0) {
        lbl_80344734 = 1;
        MBRemoveNode(enemy->objgrp.node, 0);
        lbl_80344734 = 0;
    }
    enemy->atree.root = 0;
    enemy->objgrp.node = 0;
    enemy->flooroffset = 0.0f;

    if (type == 31) {
        enemy->atree.root = (void*)fn_80011BBC(
            sGoodWizObj, "IT", &enemy->atree.root, "IT", 2048);
        enemy->flooroffset = 3.0f;
    } else if (((void**)gWadAtreeHeaders)[type] != 0) {
        char* name = fn_80051E1C(type, level, 0);
        enemy->atree.root = (void*)fn_80011BBC(
            ((void**)gWadAtreeHeaders)[type], name, &enemy->atree.root, name, 2048);
    }

    if (enemy->atree.root != 0) {
        enemy->objgrp.node = MBNewNode(lbl_8034473C,
                                      (void*)gIdentityMatrix, 1);
        MBNodeSetParent(*(void**)enemy->atree.root, enemy->objgrp.node);
        InitActions(&enemy->atree.root, enemy->actionlist, lbl_80126EC0);
    } else {
        InitActions(0, enemy->actionlist, lbl_80126EC0);
    }

    if (enemy->objgrp.node == 0) {
        char* name = fn_80051E1C(type, level, 1);
        enemy->objgrp.node = MBOX_NewObject(name, (f32*)gIdentityMatrix,
                                            (void*)lbl_8034473C, 0);
        MBTreeSetFlags(enemy->objgrp.node, 2048, 0);
    }

    if (enemy->shadow != 0) {
        MBRemoveNode(enemy->shadow, 0);
        enemy->shadow = 0;
    }

    if (enemy->type != 30 && enemy->type != 0 &&
        enemy->type != 31 && enemy->type != 21) {
        shadowObject = lbl_802512B0[type];
        if (level < 1) {
            shadowLevel = 1;
        } else if (level > 3) {
            shadowLevel = 3;
        } else {
            shadowLevel = level;
        }
        level = shadowLevel - 1;
        object = MBOX_ReallyFindObject(lbl_8011BFF8[level],
                                       shadowObject, shadowObject, 1);
        enemy->shadow = MBNewObject(object, (f32*)gIdentityMatrix, 0, 2176);
        enemy->shadow->zsort_add = 3.0f;
        enemy->shadow->zmod = -32;
    }
}

/* Keep the per-index resource base shared by the two table reads. */
#pragma opt_propagation off
void fn_800508A0(void)
{
    s32 i;
    s32 idx;
    u8 unused[8];

    for (i = 0; i < lbl_8034471C; i++) {
        idx = enemy_type[i];
        if (((void**)gWadAtreeHeaders)[idx] != 0) {
            InitTexMods(((void**)gWadAtreeHeaders)[idx], lbl_802512B0[idx]);
        }
    }
}
#pragma opt_propagation reset

void fn_80050910(s32 arg0)
{
    lbl_802511FC[arg0] = -lbl_802511FC[arg0];
    InitEnemyMissiles(arg0);
}

void AllocEnemy(s32 id, s32 model)
{
    char buf[68];
    u8 unused[4];
    EnemyTypeName* tbl = lbl_8011AF48;
    s32* pool = (s32*)mbdesc;
    char* name;
    s32 i;

    lbl_8034471C++;
    if (lbl_8034471C > 8) {
        FatalErrorf("%d > MAX:%d ETYPES\n", lbl_8034471C, 8);
    }
    pool[7 + lbl_8034471C] = id;

    if (id == E_GOLEM || id == E_GENERAL) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].type) {
                name = tbl[i].desc;
                goto alloc_fmt1;
            }
        }
        name = 0;
alloc_fmt1:
        sprintf(buf, "monsters/%s/%s", name, fn_80057ACC(id));
    } else if (id == E_GARGOYLE) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].type) {
                name = tbl[i].desc;
                goto alloc_fmt2;
            }
        }
        name = 0;
alloc_fmt2:
        sprintf(buf, "monsters/%s_%s", name, fn_80057ACC(id));
    } else if (model == 4) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].type) {
                name = tbl[i].desc;
                goto alloc_fmt3;
            }
        }
        name = 0;
alloc_fmt3:
        sprintf(buf, "monsters/%saux", name);
    } else if (model > 10) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].type) {
                name = tbl[i].desc;
                goto alloc_fmt4;
            }
        }
        name = 0;
alloc_fmt4:
        sprintf(buf, "monsters/%s%d", name, model - 10);
    } else {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].type) {
                name = tbl[i].desc;
                goto alloc_fmt5;
            }
        }
        name = 0;
alloc_fmt5:
        sprintf(buf, "monsters/%s", name);
    }

    id <<= 2;
    *(s32*)((u8*)&pool[300] + id) =
        fn_8005A1EC(buf, (void**)((u8*)&pool[345] + id));
    *(s32*)((u8*)&pool[255] + id) = -model;
}

void LoadEnemy(s32 id, s32 model)
{
    char buf[68];
    u8 unused[4];
    EnemyTypeName* tbl = lbl_8011AF48;
    s32* pool = (s32*)mbdesc;
    char* name;
    s32 i;
    s32 offset;

    lbl_8034471C++;
    if (lbl_8034471C > 8) {
        FatalErrorf("%d > MAX:%d ETYPES\n", lbl_8034471C, 8);
    }
    pool[7 + lbl_8034471C] = id;

    if (id == E_GOLEM || id == E_GENERAL) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].type) {
                name = tbl[i].desc;
                goto load_fmt1;
            }
        }
        name = 0;
load_fmt1:
        sprintf(buf, "monsters/%s/%s", name, fn_80057ACC(id));
    } else if (id == E_GARGOYLE) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].type) {
                name = tbl[i].desc;
                goto load_fmt2;
            }
        }
        name = 0;
load_fmt2:
        sprintf(buf, "monsters/%s_%s", name, fn_80057ACC(id));
    } else if (model == 4) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].type) {
                name = tbl[i].desc;
                goto load_fmt3;
            }
        }
        name = 0;
load_fmt3:
        sprintf(buf, "monsters/%saux", name);
    } else if (model > 10) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].type) {
                name = tbl[i].desc;
                goto load_fmt4;
            }
        }
        name = 0;
load_fmt4:
        sprintf(buf, "monsters/%s%d", name, model - 10);
    } else {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].type) {
                name = tbl[i].desc;
                goto load_fmt5;
            }
        }
        name = 0;
load_fmt5:
        sprintf(buf, "monsters/%s", name);
    }

    offset = id << 2;
    *(s32*)((u8*)&pool[300] + offset) =
        LoadModel(buf, ((void**)&pool[345]) + id, 0, -1);
    *(s32*)((u8*)&pool[255] + offset) = model;
    InitEnemyMissiles(id);
}

void fn_80050DD8(char* buf, s32 id, s32 qty)
{
    EnemyTypeName* tbl = lbl_8011AF48;
    char* name;
    s32 i;

    if (id == E_GOLEM || id == E_GENERAL) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].type) {
                name = tbl[i].desc;
                goto f1;
            }
        }
        name = 0;
f1:
        sprintf(buf, "monsters/%s/%s", name, fn_80057ACC(id));
    } else if (id == E_GARGOYLE) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].type) {
                name = tbl[i].desc;
                goto f2;
            }
        }
        name = 0;
f2:
        sprintf(buf, "monsters/%s_%s", name, fn_80057ACC(id));
    } else if (qty == 4) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].type) {
                name = tbl[i].desc;
                goto f3;
            }
        }
        name = 0;
f3:
        sprintf(buf, "monsters/%saux", name);
    } else if (qty > 10) {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].type) {
                name = tbl[i].desc;
                goto f4;
            }
        }
        name = 0;
f4:
        sprintf(buf, "monsters/%s%d", name, qty - 10);
    } else {
        for (i = 0; i < 44; i++) {
            if (id == tbl[i].type) {
                name = tbl[i].desc;
                goto f5;
            }
        }
        name = 0;
f5:
        sprintf(buf, "monsters/%s", name);
    }
}

s32 GetEnemyType(s32 w, s32 l)
{
    s32 result = w;

    if (w == 3) {
        result = lbl_802577AC[1];
    }
    if ((u32)(w - 4) <= 1) {
        if (l >= 4 && lbl_802577AC[4] >= 0) {
            result = lbl_802577AC[4];
        } else if (lbl_802577AC[2] >= 0) {
            result = lbl_802577AC[2];
        } else if (lbl_802577AC[3] >= 0) {
            result = lbl_802577AC[3];
        }
    }
    if (result == -1) {
        ErrorPrintf("No enemy loaded: %s (type=%d subtype=%d)", findWorldName(w), w, l);
    }
    return result;
}

void fn_800510A4(void)
{
    s32* pool = (s32*)mbdesc;
    Enemy* e = (Enemy*)((u8*)pool + 3608);   /* = gEnemies */
    s32 i;

    for (i = 0; i < 25; i++) {
        e->state = 0;
        e->objgrp.node = 0;
        e->objgrp.flags = 2;
        e->flooroffset = 0.0f;
        e->atree.root = 0;
        e->shadow = 0;
        e++;
    }
    lbl_8034473C = (s32)MBNewNode(gSceneRoot, gIdentityMatrix, 1);
    gNumEnemies = gCurLevel->maxenemies;
    lbl_80344740 = 0;
    lbl_80344748 = -1;
    lbl_80344750 = -1;
    lbl_8034474C = 0;
    for (i = 0; i < 256; i++) {
        pool[390 + i] = 0;   /* +0x618 */
        pool[646 + i] = 0;   /* +0xA18 */
    }
}

/* ResetEnemies in the Xbox symbols corroborates these as three distinct
 * 45-entry resource arrays.  GC combines their stores into one counted loop. */
#pragma opt_propagation off
void fn_80051164(void)
{
    s32 i;

    for (i = 0; i < 45; i++) {
        gWadAtreeHeaders[i] = 0;
        lbl_802512B0[i] = -1;
        lbl_802511FC[i] = 0;
    }
    for (i = 0; i < 8; i++) {
        enemy_type[i] = -1;
    }
    lbl_8034471C = 0;
    lbl_80344738 = -1;
}
#pragma opt_propagation reset

#pragma opt_propagation off
s32 fn_800511D0(s32 milestone, f32 tolerance)
{
    /* find_next_milestone's Xbox locals include temp and cpos vectors.
     * The GC distance calculation keeps temp in registers; pos escapes to
     * get_yaw. Recover the real delta vector instead of the old 12-byte pad. */
    f32 temp[3];
    f32 pos[3];
    f32 ad;
    volatile f32 tmp;
    f32 t1;
    f32 t2;
    f32 bestDist;
    f32 secondDist;
    f32 bestDy;
    f32 secondDy;
    f32 base;
    f64 kThree;
    f64 kHalf;
    f32 kZero;
    f64 k2Pi;
    f64 kNegPi;
    f64 kPi;
    MilestoneParam* m;
    s32 i;
    s32 best;
    s32 second;
    u8 unusedLo[28];

    bestDist = 100000.0f;
    best = -1;
    secondDist = bestDist;
    bestDy = bestDist;
    second = -1;
    secondDy = bestDist;
    if (milestone < 0) {
        return milestone;
    }

    m = (MilestoneParam*)sMilestones + milestone;
    pos[0] = m->objgrp.worldmat[3][0];
    pos[1] = m->objgrp.worldmat[3][1];
    pos[2] = m->objgrp.worldmat[3][2];
    {
        f32 x = m->objgrp.worldmat[2][2];
        f32 r = atan2(m->objgrp.worldmat[2][0], x);
        f64 p = 3.141592654;
        f32 a = (f32)(p + r);
        f64 t;
        if (a > p) {
            t = a - 6.283185308;
        } else if (a <= (-3.141592654)) {
            t = 6.283185308 + a;
        } else {
            t = a;
        }
        base = (f32)t;
    }

    kZero = 0.0f;
    kHalf = 0.5;
    kThree = 3.0;
    kNegPi = (-3.141592654);
    k2Pi = 6.283185308;
    kPi = 3.141592654;
    {
        MilestoneParam* m0 = (MilestoneParam*)sMilestones;
        m = m0;
    }
    for (i = 0; i < sNumMilestones; i++, m++) {
        f32 d;
        f64 nd;
        f32 dist;

        if (i == milestone) {
            continue;
        }
        d = get_yaw(&m->objgrp.worldmat[3][0], pos) - base;
        if (d > kPi) {
            nd = d - k2Pi;
        } else if (d <= kNegPi) {
            nd = k2Pi + d;
        } else {
            nd = d;
        }
        ad = (f32)nd;
        *(u32*)&ad &= 0x7FFFFFFF;
        if (ad <= tolerance) {
            temp[0] = m->objgrp.worldmat[3][0] - pos[0];
            temp[1] = m->objgrp.worldmat[3][1] - pos[1];
            temp[2] = m->objgrp.worldmat[3][2] - pos[2];
            dist = temp[2] * temp[2] +
                   (dist = temp[0] * temp[0] + temp[1] * temp[1]);
            if (dist > kZero) {
                f64 y = __frsqrte(dist);
                y = kHalf * y * (kThree - y * y * dist);
                y = kHalf * y * (kThree - y * y * dist);
                y = kHalf * y * (kThree - y * y * dist);
                tmp = (f32)(dist * (kHalf * y * (kThree - y * y * dist)));
                dist = tmp;
            }
            if (dist < bestDist) {
                t1 = temp[1];
                secondDist = bestDist;
                second = best;
                secondDy = bestDy;
                *(u32*)&t1 &= 0x7FFFFFFF;
                bestDist = dist;
                best = i;
                bestDy = t1;
            } else if (dist < secondDist) {
                t2 = temp[1];
                secondDist = dist;
                second = i;
                *(u32*)&t2 &= 0x7FFFFFFF;
                secondDy = t2;
            }
        }
    }

    if (bestDy > secondDy) {
        best = second;
    }
    if (best < 0) {
        if ((gControllerButtons & 0x10) != 0) {
            ErrorPrintf("Next Milestone Not Found (%02X)", milestone);
        }
        best = milestone;
    }
    return best;
}
#pragma opt_propagation reset

s32 fn_80051480(f32* pos)
{
    f32 delta[3];
    f32 d;
    s32 best_idx = -1;
    f32 best_dist = 100000.0f;
    MilestoneParam* node = (MilestoneParam*)sMilestones;
    s32 i;

    for (i = 0; i < sNumMilestones; i++, node++) {
        delta[0] = pos[0] - node->objgrp.worldmat[3][0];
        delta[1] = pos[1] - node->objgrp.worldmat[3][1];
        delta[2] = pos[2] - node->objgrp.worldmat[3][2];
        if ((d = enemy_distance_sqrt(delta[2] * delta[2] +
                    (delta[0] * delta[0] + delta[1] * delta[1]))) < best_dist) {
            best_idx = i;
            best_dist = d;
        }
    }
    return best_idx;
}

#pragma opt_propagation off
void fn_80051568(s32 index)
{
    Enemy* e = &gEnemies[index];
    s32 i;
    Item* it;
    iteminfo* hdr;
    f32 dx;
    f32 dy;
    f32 dz;
    f32 dist2;
    f64 kHalf;
    f32 kZero;
    f64 kThree;
    u8 _spare[24];
    u8 unused[8];

    if (e->closest >= 0 &&
        e->actual_dist <= 14.0) {
        e->guard_mode = 0;
        e->guard_closest = -1;
        e->guard_dist = 100000.0f;
        return;
    }
    if (e->guard_mode != 0) {
        return;
    }
    StartEnemyGrid(e->objgrp.worldmat[3], 20.0f);
    kZero = 0.0f;
    kHalf = 0.5;
    kThree = 3.0;
    while ((i = NextGridEnemy()) >= 0) {
        it = &sItems[i];
        hdr = it->info;
        if (it->active == -1) {
            continue;
        }
        if (hdr->type != 2) {
            continue;
        }
        if (it->minoff != 0) {
            continue;
        }
        dx = it->objgrp.worldmat[3][0] - e->objgrp.worldmat[3][0];
        dy = it->objgrp.worldmat[3][1] - e->objgrp.worldmat[3][1];
        dz = it->objgrp.worldmat[3][2] - e->objgrp.worldmat[3][2];
        dist2 = dx * dx + dy * dy + dz * dz;
        if (dist2 > kZero) {
            volatile f32 tmp;
            f64 y = __frsqrte(dist2);
            y = kHalf * y * (kThree - y * y * dist2);
            y = kHalf * y * (kThree - y * y * dist2);
            y = kHalf * y * (kThree - y * y * dist2);
            dist2 = (f32)(dist2 * (kHalf * y * (kThree - y * y * dist2)));
            tmp = dist2;
            dist2 = tmp;
        }
        if (dist2 < e->guard_dist) {
            e->guard_dist = dist2;
            e->guard_closest = i;
            e->guard_mode = 1;
        }
    }
}
#pragma opt_propagation reset

/* Xbox retains calc_enemy_to_player_distance(Enemy*, Player*). The GC
 * selector uses the same live-mikey choice and collision-position fields.
 * Keeping that real operation separate removes the old caller-owned
 * constant caches and synthetic distance-rounding arguments. */
static inline f32 calc_enemy_to_player_distance(Enemy* e, Player* p)
{
    f32 delta[3];
    f32 distance;

    if (p->field_A1C > 2) {
        delta[0] = e->objgrp.coll_pos[0] - p->mikey_coll_pos[0];
        delta[1] = e->objgrp.coll_pos[1] - p->mikey_coll_pos[1];
        delta[2] = e->objgrp.coll_pos[2] - p->mikey_coll_pos[2];
        distance = enemy_distance_sqrt(delta[2] * delta[2] +
                                      (delta[0] * delta[0] + delta[1] * delta[1]));
    } else {
        delta[0] = e->objgrp.coll_pos[0] - p->effectpos[0];
        delta[1] = e->objgrp.coll_pos[1] - p->effectpos[1];
        delta[2] = e->objgrp.coll_pos[2] - p->effectpos[2];
        distance = enemy_distance_sqrt(delta[2] * delta[2] +
                                      (delta[0] * delta[0] + delta[1] * delta[1]));
    }
    return distance;
}

void fn_800516F8(s32 slot)
{
    Player* p;
    Enemy* e;
    s32 i;
    s32 t;
    f32 dist;
    f32 range;
    f32 bestSpecial;
    f32 ad;

    e = &gEnemies[slot];
    bestSpecial = 100000.0f;

    for (i = 0, p = gPlayers; i < 4; i++, p++) {
        if (p->state == 1) {
            break;
        }
    }
    if (i >= 4) {
        e->recognized = 0;
    }

    t = lbl_80344B24;
    if (t >= 0 && gPlayers[t].state == ACTIVE &&
        !(gPlayers[t].flags & 4) &&
        !(e->type == 30 && (gPlayers[t].shield_flags & 0x80000))) {
        Player* q;
        e->prev_closest = e->closest;
        e->closest = (s16)lbl_80344B24;
        q = &gPlayers[lbl_80344B24];
        e->actual_dist = calc_enemy_to_player_distance(e, q);
        e->close_dist = e->actual_dist +
                           gPlayers[lbl_80344B24].dist_offset;
    } else {
        s32 go = 1;
        s32 cur;
        if ((lbl_80344800 & 7) != (slot & 7) && e->closest >= 0) {
            go = 0;
        }
        cur = e->closest;
        if ((s16)cur >= 0 &&
            gPlayers[cur].state != ACTIVE) {
            go = -1;
        }
        if (go != 0) {
            e->prev_closest = (s16)cur;
            e->closest = -1;
            e->close_dist = 100000.0f;
            e->actual_dist = 100000.0f;
            if (e->type == 30) {
                e->counter2 = -1;
            }
            {
                for (; i < 4; i++, p++) {
                    if (p->state != 1) {
                        continue;
                    }
                    if (p->flags & 4) {
                        continue;
                    }
                    range = dist = calc_enemy_to_player_distance(e, p);
                    if (range > e->sight) {
                        continue;
                    }
                    if (e->type == 30 && (p->shield_flags & 0x80000)) {
                        if (range < bestSpecial) {
                            bestSpecial = range;
                            e->counter2 = i;
                        }
                        continue;
                    }
                    if (range > 5.0 * e->rad) {
                        range += p->dist_offset;
                    }
                    if (!(range < e->close_dist)) {
                        continue;
                    }
                    if (e->view < 3.141592654) {
                        ad = fabsf_(get_yaw(p->effectpos, e->objgrp.coll_pos) -
                                    e->pyr[1]);
                        if (ad > e->view) {
                            continue;
                        }
                    }
                    e->close_dist = range;
                    e->actual_dist = dist;
                    e->closest = (s16)i;
                }
            }
        }
    }

    if (e->closest >= 0) {
        if (e->actual_dist <= e->sight) {
            Player* base;
            e->recognized = 1;
            base = gPlayers;
            base[e->closest].num_approaching++;
            {
                Player* r = &base[e->closest];
                r->dist_offset += 2.0;
            }
        }
    } else {
        e->actual_dist = 100000.0f;
        e->close_dist = 100000.0f;
    }
}

/* Build the route in the same milestone-index array used by the neighbor
 * lookup. Its stores are .bss-anchor-relative in the target; that addressing
 * does not imply a larger enclosing object. */
void fn_80051C78(void)
{
    s32 best;
    s32 cur;
    s32 i;

    for (i = 0; i < 128; i++) {
        sEnemyMilestoneRoute[i] = -1;
    }
    lbl_80344724 = 0;
    best = fn_80051480(gDefaultPlayerPosition);

    cur = best;
    for (;;) {
        s32 prev = cur;

        sEnemyMilestoneRoute[lbl_80344724++] = cur;
        cur = fn_800511D0(prev, 0.17453292f);
        for (i = 0; i < lbl_80344724; i++) {
            if (cur == sEnemyMilestoneRoute[i]) {
                break;
            }
        }
        if (i < lbl_80344724) {
            break;
        }
        if (prev == cur) {
            break;
        }
    }
}

/* EnemyDesc formats into the shared mbdesc buffer. Numbered levels share
 * the default arm; lettered levels use the existing suffix map. */
char* fn_80051E1C(s32 world, s32 lvl, s32 flag)
{
    char* character;
    u32 i;
    s32 n = lvl;

    if (lvl == 0) {
        n = 1;
    }
    switch (lvl) {
    case 1:
    case 2:
    case 3:
    default:
        sprintf(mbdesc, "%s%d", findWorldName(world), n);
        break;
    case 4:
    case 5:
    case 6:
    case 7:
        sprintf(mbdesc, "%s%c", findWorldName(world), lbl_80343BF8[n - 4]);
        break;
    }
    if (flag != 0) {
        strcat(mbdesc, "L1");
    }
    for (i = 0; i < strlen(mbdesc); i++) {
        character = mbdesc + i;
        *character = toupper(*character);
    }
    return mbdesc;
}

void* EnemyTypePrefix(s32 id)
{
    s32 i;

    for (i = 0; i < 44; i++) {
        if (lbl_8011AF48[i].type == id) {
            return lbl_8011AF48[i].prefix;
        }
    }
    return 0;
}

void* EnemyTypeDesc(s32 id)
{
    s32 i;

    for (i = 0; i < 44; i++) {
        if (lbl_8011AF48[i].type == id) {
            return lbl_8011AF48[i].desc;
        }
    }
    return 0;
}

s32 EnemyDescType(const char* name)
{
    u32 i;

    if (stricmp(name, "BOSSGEN") == 0) {
        s32 t = lbl_802577CC[0];
        if (t != gBossType) {
            return t;
        }
        return -1;
    }
    for (i = 0; i < 44; i++) {
        if (stricmp(name, lbl_8011AF48[i].desc) == 0) {
            return lbl_8011AF48[i].type;
        }
    }
    return -1;
}

void fn_8005207C(s32 arg0, s32 arg1, s32 arg2)
{
    lbl_8034476C = arg0;
    if (arg1 < 0) {
        arg1 = 0;
    } else if (arg1 > 4) {
        arg1 = 4;
    }
    lbl_80344768 = arg1;
    if (gGameOptions[3] == 0) {
        gNumPlayers = arg0;
    } else {
        gNumPlayers = gGameOptions[3];
    }
    lbl_80344760 = arg2;
}

void fn_800520C8(void)
{
}
