#include "game/timing.h"

/* TIMING owns the profiling descriptors, samples and display handles.
 * GC: descriptors 0x80124458..0x80124C70, blits 0x8028BCC0..0x8028BDE8,
 * samples 0x8028BDE8..0x8028C288. Xbox TIMING.OBJ independently identifies
 * the 74 descriptors and their name/level/color/precision layout.
 * Padded names are display text; color values are packed RGB, not offsets. */
// lint-allow-next-line FM007: Descriptor colors are the retail packed RGB palette consumed by the timer renderer, not memory offsets or machine instructions.
TimerDesc lbl_80124458[74] = {
    {"GAME           ", 0, 0xffffff, 0},
    {"TEMP1          ", 1, 0xffff, 0},
    {"TEMP2          ", 1, 0xffff, 0},
    {"TEMP3          ", 1, 0xffff, 0},
    {"TEMP4          ", 1, 0xffff, 0},
    {"GAMELOOP       ", 1, 0xffff, 0},
    {"COLLIDE        ", 2, 0xffff00, 0},
    {"SETUP          ", 3, 0xff00, 0},
    {"WORLDCOL       ", 3, 0xff00, 0},
    {"OBJCOL         ", 4, 0xff6000, 0},
    {"BTRICOL        ", 5, 0x804040, 0},
    {"TRICOL         ", 5, 0x804040, 0},
    {"NEXTGRID       ", 4, 0xff6000, 0},
    {"DYNAMIC        ", 4, 0xff6000, 0},
    {"COLLIDE WALLS  ", -2, 0xffff00, 0},
    {"COLLIDE FLOORS ", -2, 0xffff00, 0},
    {"ITEMS          ", 2, 0xffff00, 1},
    {"ANIM           ", 3, 0xff00, 1},
    {"GENERATORS     ", 3, 0xff00, 1},
    {"ENEMIES        ", 2, 0xffff00, 0},
    {"AI             ", 3, 0xff00, 0},
    {"CHECK VACANCY  ", 4, 0xff6000, 0},
    {"AI 0           ", -4, 0xff6000, 0},
    {"AI 1           ", -4, 0xff6000, 0},
    {"AI 2           ", -4, 0xff6000, 0},
    {"AI 3           ", -4, 0xff6000, 0},
    {"AI 4           ", -4, 0xff6000, 0},
    {"AI 5           ", -4, 0xff6000, 0},
    {"AI 6           ", -4, 0xff6000, 0},
    {"AI 7           ", -4, 0xff6000, 0},
    {"AI 8           ", -4, 0xff6000, 0},
    {"AI 9           ", -4, 0xff6000, 0},
    {"AI 10          ", -4, 0xff6000, 0},
    {"AI 11          ", -4, 0xff6000, 0},
    {"AI 12          ", -4, 0xff6000, 0},
    {"AI 13          ", -4, 0xff6000, 0},
    {"AI 14          ", -4, 0xff6000, 0},
    {"AI 15          ", -4, 0xff6000, 0},
    {"AI 16          ", -4, 0xff6000, 0},
    {"AI 17          ", -4, 0xff6000, 0},
    {"AI 18          ", -4, 0xff6000, 0},
    {"AI 19          ", -4, 0xff6000, 0},
    {"AI 20          ", -4, 0xff6000, 0},
    {"AI 21          ", -4, 0xff6000, 0},
    {"AI 22          ", -4, 0xff6000, 0},
    {"AI 23          ", -4, 0xff6000, 0},
    {"AI 24          ", -4, 0xff6000, 0},
    {"AI 25          ", -4, 0xff6000, 0},
    {"AI 26          ", -4, 0xff6000, 0},
    {"AI 27          ", -4, 0xff6000, 0},
    {"AI 28          ", -4, 0xff6000, 0},
    {"AI 29          ", -4, 0xff6000, 0},
    {"AI 30          ", -4, 0xff6000, 0},
    {"ANIM           ", 3, 0xff00, 0},
    {"COL WALL/FLOOR ", 3, 0xff00, 0},
    {"COL PLAYERS    ", 3, 0xff00, 0},
    {"COL ENEMIES    ", 3, 0xff00, 0},
    {"COL ITEMS      ", 3, 0xff00, 0},
    {"PLAYERS        ", 2, 0xffff00, 0},
    {"PMOTION        ", 3, 0xff00, 0},
    {"COLLIDE        ", 4, 0xff6000, 0},
    {"COLPLAYER      ", -4, 0x804040, 0},
    {"COLENEMY       ", -4, 0x804040, 0},
    {"COLITEM        ", -5, 0xff00, 0},
    {"COLWALLS       ", -4, 0x804040, 0},
    {"ANIM           ", 4, 0xff6000, 0},
    {"GETTARGET      ", 4, 0xff6000, 0},
    {"SFX            ", 2, 0xffff00, 0},
    {"COLPLAYER      ", -4, 0xff00, 0},
    {"COLENEMY       ", -4, 0xff00, 0},
    {"COLITEM        ", -3, 0xff00, 0},
    {"COLWALLS       ", -4, 0xff00, 0},
    {"AUDIOQUE       ", 1, 0xffff, 0},
    {"CAMERA         ", 1, 0xffff, 0},
};

extern struct MBBlit* lbl_8028BCC0[74];
extern TimerSample lbl_8028BDE8[74];

/* InitGameTimers; keep the existing GC export until callers are renamed. */
void AudioRegisterMenu(void)
{
    fn_800C031C(lbl_8028BDE8, lbl_80124458, lbl_8028BCC0, 74);
}

/* Keep the external definitions after the initializer. With this MWCC edge,
 * their definition order here reproduces the independently observed BSS
 * addresses above; moving them before the function reverses that placement.
 * These are separate real arrays, not a synthetic storage wrapper. */
TimerSample lbl_8028BDE8[74];
struct MBBlit* lbl_8028BCC0[74];
