#ifndef GAME_GAMEMODE_H
#define GAME_GAMEMODE_H

/*
 * e_mode -- the game-mode vocabulary held in the global `gGameMode`.
 *
 * Source of names + values: shell3D.pdb "enum e_mode", see
 * research/xbox_symbols/misc.h.  Every value below was re-verified against
 * that dump verbatim (the dump spells them in decimal; the hex spelling here
 * is the same value in the form the GC target disassembly compares against),
 * and the declaration order is the PDB's own -- note MG_WORLD_SELECT precedes
 * MG_ROUND_START there, which is why they appear out of numeric order.
 *
 * The mode id carries its group in the high bits: 0x8000 marks the
 * attract-loop modes (MA_*) and 0x4000 the in-game modes (MG_*), which is why
 * the code tests `gGameMode & 0x8000` / `& 0x4000` to classify a mode without
 * listing its members.  The bare M_* ids are the ungrouped/base modes and the
 * low byte of a grouped id is its M_* counterpart (MG_SHOP 0x4012 -> M_SHOP
 * 18), so the three families are one numbering scheme, not three.
 *
 * This header holds no prototypes and no aggregate types: it is pure enum
 * text, so adopting it is a value-identity respelling that constant-folds and
 * is byte-neutral by construction
 * (claim.law.raw-offset-form-supplies-value-identity-not-addressing-shape).
 * It is deliberately NOT the place for a gamemain.h with declarations -- see
 * claim.gamemode-enum-needs-a-header-owner.20260831.v1.
 */
enum e_mode {
    M_CREDITS         = 0,
    M_TITLEMOVIE      = 1,
    M_MOVIE           = 2,
    M_INSTRUCT        = 3,
    M_SCREEN2D        = 4,
    M_CONTEST         = 5,
    M_DEMO            = 6,
    M_HSTABLE         = 7,
    M_FLYBY           = 8,
    M_TITLESCREEN     = 9,
    M_VIEWMENU        = 10,
    M_PLAYER_SELECT   = 11,
    M_ROUND_START     = 12,
    M_WORLD_SELECT    = 13,
    M_GAMEMOVIE       = 14,
    M_MAPSCREEN       = 15,
    M_PLAY            = 16,
    M_VICTORY         = 17,
    M_SHOP            = 18,
    M_LEVEL_ADVANCE   = 19,
    M_OVER            = 20,
    M_ENDING          = 21,
    M_STATS           = 22,
    M_GWIZ_SPEECH     = 23,
    M_CHAR_MANAGEMENT = 24,

    MA_CREDITS        = 0x8000,
    MA_TITLEMOVIE     = 0x8001,
    MA_MOVIE          = 0x8002,
    MA_INSTRUCT       = 0x8003,
    MA_SCREEN2D       = 0x8004,
    MA_CONTEST        = 0x8005,
    MA_DEMO           = 0x8006,
    MA_HSTABLE        = 0x8007,
    MA_FLYBY          = 0x8008,
    MA_TITLESCREEN    = 0x8009,
    MA_VIEWMENU       = 0x800A,

    MG_PLAYER_SELECT  = 0x400B,
    MG_WORLD_SELECT   = 0x400D,
    MG_ROUND_START    = 0x400C,
    MG_GAMEMOVIE      = 0x400E,
    MG_MAPSCREEN      = 0x400F,
    MG_PLAY           = 0x4010,
    MG_VICTORY        = 0x4011,
    MG_SHOP           = 0x4012,
    MG_LEVEL_ADVANCE  = 0x4013,
    MG_OVER           = 0x4014,
    MG_ENDING         = 0x4015,
    MG_STATS          = 0x4016,
    MG_GWIZ_SPEECH    = 0x4017
};

/*
 * Family-bit masks for the groups described above.
 *
 * DERIVATION (from the enum values alone, then corroborated at every use
 * site).  Partitioning all 49 e_mode constants by their high bits gives a
 * clean, disjoint three-way split with no exceptions:
 *
 *   bit 0x8000 set : exactly the 11 MA_* ids, MA_CREDITS(0x8000) through
 *                    MA_VIEWMENU(0x800A) -- i.e. base ids 0..10.
 *   bit 0x4000 set : exactly the 13 MG_* ids, MG_PLAYER_SELECT(0x400B)
 *                    through MG_GWIZ_SPEECH(0x4017) -- i.e. base ids 11..23.
 *   neither set    : exactly the 25 bare M_* ids, 0..24.
 *   both set       : NO constant.  The two groups are disjoint, and no
 *                    grouped id's low bits collide with the other group's
 *                    (MA_* covers base 0..10, MG_* covers base 11..23), so
 *                    `& 0x8000` and `& 0x4000` each select one whole family
 *                    and nothing else.
 *
 * The names come from the enum's own prefixes -- MA_ = attract, MG_ = game --
 * not from an interpretation of behaviour; the use sites merely agree.  They
 * do: gamemain.c tests `& 0x8000` inside the attract-loop dispatch itself
 * (the MA_FLYBY arm's `attract_tail`) and to let a start press assign a
 * controller, and gauntworld.c writes `(gGameMode & 0x8000) && gGameMode !=
 * MA_DEMO && gGameMode != MA_INSTRUCT`, excluding two MA_* ids from the very
 * set the mask selects.  The `& 0x4000` sites are in-game-session work --
 * per-player display setup, shard checks, per-player timers.
 *
 * Spelled as macros rather than enumerators deliberately: an e_mode
 * enumerator would assert these masks are themselves modes (they are not),
 * and a macro expanding to the identical literal token keeps every converted
 * site byte-identical by construction rather than by constant folding.
 */
#define MODE_GROUP_ATTRACT 0x8000
#define MODE_GROUP_GAME    0x4000

#endif /* GAME_GAMEMODE_H */
