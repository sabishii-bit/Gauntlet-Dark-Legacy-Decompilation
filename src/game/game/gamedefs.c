#include "types.h"
#include "game/options.h"

/* GAMEDEFS owns this option record, not the adjacent enemy/game-flow TUs.
 * Xbox's OPTIONS type and PS2's default_options corroborate all twelve words;
 * GC stores and the 0x30-byte object at 0x80257590 establish this layout.
 * Defining the real object also removes default_options' old word-array view.
 */
OPTIONS gGameOptions;

extern s32 lbl_8034476C;
extern s32 lbl_80344768;
extern s32 lbl_80344760;
extern s32 gNumPlayers;
extern void init_prefs(void);

/* SetNumPlayers: preserve existing exported names while other callers are
 * reconstructed. The Xbox body confirms the clamp and option override. */
void fn_8005207C(s32 arg0, s32 arg1, s32 arg2)
{
    lbl_8034476C = arg0;
    if (arg1 < 0) {
        arg1 = 0;
    } else if (arg1 > 4) {
        arg1 = 4;
    }
    lbl_80344768 = arg1;
    if (gGameOptions.players == 0) {
        gNumPlayers = arg0;
    } else {
        gNumPlayers = gGameOptions.players;
    }
    lbl_80344760 = arg2;
}

/* PS2 load_options is empty; the Xbox PDB records a one-byte body as well.
 * PS2 game_main calls it
 * in the same restart sequence as GC's call at 0x8005445C. This is a retail
 * no-op, not a missing implementation or UpdateDoPrint's debug-text update. */
void fn_800520C8(void)
{
}

void default_options(void)
{
    gGameOptions.no_damage = 0;
    gGameOptions.unlimited = 0;
    gGameOptions.gen_active = 3;
    gGameOptions.fly = 0;
    gGameOptions.showfps = 1;
    gGameOptions.showpos = 0;
    gGameOptions.players = 0;
    gGameOptions.items = 0;
    gGameOptions.showcam = 0;
    gGameOptions.startwave = 512;
    gGameOptions.testai = 0;
    gGameOptions.skip = 0;
    init_prefs();
}
