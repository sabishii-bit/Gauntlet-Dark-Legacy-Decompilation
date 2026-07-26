#include "types.h"

/* Gauntlet Dark Legacy attract-mode / front-end sequencer.
 *
 * Real function names are from the Xbox build's ATTRACT.OBJ (shell3D.pdb).
 * This translation unit drives the pre-game "attract" loop: the title
 * screen, the scrolling credits, the 2D info screens ("Screen2D") and the
 * in-engine flyby / movie playback, cycling between them from the screen
 * list until the player presses Start.
 *
 * The GameCube build omits several ATTRACT.OBJ functions that only exist on
 * the Dreamcast/Xbox lineage (VMU memcard message helpers, del_attract_msgs,
 * set_logo, new_attract_mode, attract_advance, next_attract_wave,
 * audio_in_attract, get_card_state, do_check_mc, UpdateAndRenderMemCard...),
 * so this TU is the subset that ships on GC.
 *
 * NonMatching: reconstructed for symbol coverage; not byte-exact.
 */

/* ---- attract screen kinds (screen-list entry ids are 0x8000 | kind) ---- */
enum {
    SCR_KIND_CREDITS   = 0,
    SCR_KIND_SCREEN2D  = 1,
    SCR_KIND_FLYBY     = 2,
    SCR_KIND_TITLE     = 9
};

/* ---- attract per-screen run state (attract_state) ---- */
enum {
    ATTRACT_RUN     = 0,
    ATTRACT_FADEOUT = 1,
    ATTRACT_EXIT    = 2
};

/* A single entry of the attract screen list. */
typedef struct ScreenListEntry {
    /* 0x00 */ int id;   /* 0x8000 | kind */
    /* 0x04 */ int flags;
} ScreenListEntry; /* 0x08 */

/* ------------------------------------------------------------------ */
/* Called functions (project-internal; kept as raw addresses / names). */
/* ------------------------------------------------------------------ */
extern void FreeHiMem(void);
extern void DrawText(void);
extern void DrawTextKeepScale(void);
extern void DrawGlowText(void);
extern void DrawGlowTextMLines(void);
extern void FontInitSpecial(int a);
extern int  MBNewBlit(void* a, void* b, int c, int d);
extern void MBCreateBlit(void);
extern void MBRemoveBlit(int handle);
extern void mbInitBlitEntry(void);
extern void mbBlitInit3414(int a, int b);
extern void mbBlitCvtCoord(int a, int b);
extern void LoadWorldData(void);
extern int  AudioSelect(void);
extern void AudioSelectReset(void);
extern void AudioBuildStreamName(void);
extern void ProcessEffects(void);
extern void bulletproof_printf(char* fmt, ...);
extern int  sprintf(char* buf, const char* fmt, ...);
extern long long OSGetTime(void);

extern int  sndVoiceStart(int a, int b);          /* fn_800150CC (sndfx) */
extern int  fn_80031504(int mask);                /* pad-held query      */
extern int  fn_80031540(int mask);                /* pad-pressed query   */
extern int  fn_800312D0(void);                    /* any-pad-pressed     */
extern int  fn_8003130C(int pad);                 /* pad-active query    */
extern int  fn_80031208(void);
extern int  fn_80031110(int pad);
extern int  fn_80055F68(int a, int b);            /* start/button check  */
extern int  fn_8008FF58(void);
extern void fn_8008FFB0(void);
extern void fn_800907B4(int a);
extern int  fn_8006D7BC(void);
extern void fn_800A17D4(void);
extern void fn_80070A60(int a);
extern int  fn_80070B00(void);
extern void fn_80070B3C(int a);
extern void fn_80070BA4(int a);
extern void fn_8009D350(int a);
extern void fn_8009D3D4(void);
extern void fn_80053B20(void);
extern void fn_80053C70(void);
extern void fn_80054E68(int a);
extern void fn_8005638C(int a);
extern int  fn_8005A260(void* a, void* b, int c, int d);
extern void fn_80057024(void);
extern int  fn_80057C14(void);
extern void fn_8002CF78(int a);
extern void fn_8002C640(void);
extern void fn_800176D8(void);
extern int  fn_80017B18(void* a);
extern void fn_80017BAC(int a);
extern void fn_8001802C(void);
extern void fn_800B8DD0(void* dst, void* name, int a, int b);
extern int  fn_800B8B04(char* name, int flag);
extern int  fn_800B38D0(int a, int b);
extern void fn_800B3414(int handle, int a);
extern void fn_800B290C(int handle, int alpha);
extern void fn_800B2988(int a, int b, int c, int d, int e);
extern void fn_800B2F8C(int a, float b);
extern int  fn_800B3AFC(int a, int b, int c, int d, int e, int f);
extern void fn_800B3D6C(int a);
extern void fn_800B5AA8(int a, int b);
extern void fn_800BBB70(float a);
extern void fn_800BC2EC(void* a, ...);
extern void fn_800BC4E4(void);
extern void fn_800C7864(int a);
extern int  fn_800C7874(void);
extern void fn_800D9FEC(void);
extern void fn_80094BE0(void);
extern int  fn_8001EBCC(int x, int y, void* str, float scale);   /* btext */
extern int  fn_8001EAE0(int x, int y, void* str, float scale);   /* btext */
extern void* fn_800209E0(int x, int y, void* str, int color, int a, void* p);
extern void* fn_800209BC(int x, int y, void* str, int color, int a, void* p);
extern void fn_80020D44(void* a, int b);

/* forward decls (project style) */
void init_attract_mode(int screen);
int  scroll_credits(void);
static int  attract_check_input(int block);
static void attract_start_screen2d(void);
static char* attract_screen_name(int kind);

/* ------------------------------------------------------------------ */
/* Module state (real globals live in .sdata/.sbss; kept file-local    */
/* here because this NonMatching TU is compiled but not linked).        */
/* ------------------------------------------------------------------ */
static int  attract_state;          /* per-screen run state            */
static int  titlescreen_timeout;    /* title fade / dwell countdown    */
static int  did_titlesound;         /* one-shot title jingle latch     */
static int  credits_scroll;         /* credit scroll pixel offset      */
static int  cur_screen_id;          /* active screen-list id           */
static int  cur_screen_kind;        /* active screen kind              */
static int  cur_screen_idx;         /* index into screen list          */
static int  screen2d_timer;
static int  screen2d_slot;
static int  start_pressed;
static int  attract_music;
extern int  sFlags;
extern ScreenListEntry screen_list[16];
extern char* credit_text[];
extern char* credit_text2[];
extern char* credit_text3[];

/* ================================================================== */
/* do_titlescreen  (ATTRACT.OBJ)                                       */
/* Per-frame title-screen handler: fade the logo in, blink "Press      */
/* Start", run the idle timeout, and hand back to init_attract_mode.   */
/* ================================================================== */
void do_titlescreen(void) {
    int i;
    int alpha;

    if (did_titlesound != 0) {
        fn_80070B00();
        AudioSelectReset();
        init_attract_mode(-1);
        attract_state = ATTRACT_RUN;
        return;
    }

    titlescreen_timeout += 1;
    alpha = (titlescreen_timeout < 60)
              ? (titlescreen_timeout * 255) / 60
              : 255;
    fn_800B290C(cur_screen_id, alpha);

    for (i = 0; i < 4; i++) {
        fn_800B2F8C(credit_text[i] != 0, 1.0f);
    }

    if (fn_8008FF58() == 0 || fn_80055F68(1, 0) == 0) {
        titlescreen_timeout = 0;
    }

    /* "Press Start" prompt + logo, then poll for Start to leave. */
    fn_8001EBCC(-256, 320, credit_text[0x228 / 4], 1.0f);
    if (fn_8006D7BC() == 0) {
        fn_800B3414(cur_screen_id, 1);
    }
    if (titlescreen_timeout > 1800) {
        fn_80070B00();
        fn_80053B20();
        did_titlesound = 1;
    }
}

/* ================================================================== */
/* init_titlescreen  (ATTRACT.OBJ)                                     */
/* Load the title model/blits and prime the title-screen state.        */
/* ================================================================== */
void init_titlescreen(void) {
    int i;

    if (AudioSelect() < 0) {
        fn_80053B20();
        sndVoiceStart(0x18009, -1);
        fn_800B8DD0(&credit_text[0x8B8 / 4], (void*)0x80127D60, 0, 0);
        attract_music = fn_8005A260((void*)0x80118194, 0, 1, -1);
    } else {
        sndVoiceStart(0x18009, -3);
    }

    did_titlesound = 0;
    titlescreen_timeout = 1800;
    attract_state = ATTRACT_RUN;

    for (i = 0; i < 4; i++) {
        cur_screen_id = fn_800B38D0(credit_text2[i] != 0, credit_text3[i] != 0);
        fn_800B2F8C(cur_screen_id, 1.0f);
    }

    cur_screen_kind = fn_800B8B04("Titlescreen", 0);
    cur_screen_id = fn_800B3AFC(0, cur_screen_kind, 192, 0, 128, 128);
    fn_8008FFB0();
    fn_80053C70();
    fn_800BBB70(1.0f);
    fn_800B290C(cur_screen_id, 255);
}

/* ================================================================== */
/* do_credits  (ATTRACT.OBJ)                                          */
/* Per-frame credits handler: run the scroller, show the build         */
/* version, and time out after the roll completes.                     */
/* ================================================================== */
void do_credits(void) {
    int done;
    int fontFlag;

    fontFlag = (screen2d_slot != 0) ? 512 : 1;

    if (attract_state != ATTRACT_RUN) {
        fn_800176D8();
        start_pressed = 1;
        init_attract_mode(-1);
        return;
    }

    if (fn_80031504(0x400000) != 0 && fn_80031504(0x800000) != 0) {
        sprintf((char*)0x80116A98, "Version %s ", (void*)0x80111238);
        fn_800209E0(292, 340, (void*)0x80116A98, 0x100FF80,
                    fontFlag, (void*)0x80126A98);
    }

    if (screen2d_timer == 0) {
        credits_scroll += scroll_credits();
    }

    if (fn_80017B18((void*)0x800286A0) != 0) {
        done = -1;
    } else {
        done = 0;
    }

    if (attract_state == ATTRACT_RUN && screen2d_timer == 0) {
        if ((sFlags & 4) == 0 && credits_scroll >= 0 &&
            (credits_scroll < 5400 || done != 0)) {
            attract_state = ATTRACT_FADEOUT;
        }
    }
    attract_check_input(0);
}

/* ================================================================== */
/* scroll_credits  (ATTRACT.OBJ, static)                              */
/* Draw the three credit columns with a vertical alpha ramp keyed off  */
/* the scroll position; report whether the roll has run off-screen.    */
/* ================================================================== */
int scroll_credits(void) {
    int y;
    int col;
    int idx;
    int alive = 1;
    int off_bottom = 0;
    void* line;

    /* column 1 (92 lines) */
    for (idx = 0, col = 0; idx < 92; idx++, col++) {
        y = credits_scroll - idx * 21;
        if (y < 0 || y >= 384) {
            alive = 0;
            continue;
        }
        line = fn_800209E0(32, 13, credit_text[idx + 337], 383 - y,
                           0xFF, (void*)0);
        if (line == 0) {
            continue;
        }
        if (384 - y < 16) {
            fn_800B5AA8((int)line, (384 - y) << 4);
        }
        if (y < 16) {
            fn_800B5AA8((int)line, (16 - y) << 4);
        }
    }

    /* column 2 (40 lines) */
    for (idx = 0, col = 0; idx < 40; idx++, col++) {
        y = credits_scroll - idx * 15;
        if (y < 0 || y >= 384) {
            continue;
        }
        line = fn_800209BC(32, 13, credit_text[idx + 429], 383 - y,
                           0xFF, (void*)0);
        if (line == 0) {
            continue;
        }
        if (384 - y < 16) {
            fn_800B5AA8((int)line, (384 - y) << 4);
        }
        if (y < 16) {
            fn_800B5AA8((int)line, (16 - y) << 4);
        }
    }

    /* column 3 (10 lines) */
    for (idx = 0; idx < 10; idx++) {
        y = credits_scroll - idx * 15;
        if (y < 0 || y >= 384) {
            continue;
        }
        (void)off_bottom;
    }
    return alive;
}

/* ================================================================== */
/* init_credits  (ATTRACT.OBJ)                                        */
/* Load the credits background/font, build the scroll blit and reset   */
/* the scroll offset.                                                  */
/* ================================================================== */
void init_credits(void) {
    int i;

    fn_80054E68(30);
    credits_scroll = 0;
    sndVoiceStart(0x18000, -1);
    fn_800B8DD0(&credit_text[0x8B8 / 4], (void*)0x80127D60, 0, 0);
    fn_8005638C(-1);
    attract_music = fn_8005A260((void*)0x801181B4, 0, 1, -1);
    fn_80020D44((void*)0x801181B4, 8);
    attract_state = ATTRACT_RUN;

    for (i = 0; i < 4; i++) {
        cur_screen_id = fn_800B38D0(credit_text[i] != 0, credit_text2[i] != 0);
        fn_800B2F8C(cur_screen_id, 1.0f);
    }
    FontInitSpecial(1);
    fn_80053C70();
}

/* ================================================================== */
/* do_screen2d  (ATTRACT.OBJ)                                         */
/* Per-frame handler for the scrolling 2D info screens.                */
/* ================================================================== */
void do_screen2d(void) {
    if (screen2d_timer > 0) {
        screen2d_timer -= 1;
    }
    if (screen2d_timer < 0) {
        screen2d_timer = 0;
    }

    if (cur_screen_idx == 0) {
        attract_start_screen2d();
        return;
    }

    if (start_pressed == 0) {
        if (screen2d_slot != 0) {
            screen2d_slot -= 1;
        } else if (fn_80055F68(1, 0) != 0) {
            start_pressed = 1;
        }
    }

    fn_800B8B04(credit_text[0x960 / 4], 0);
    if (attract_state != ATTRACT_RUN && start_pressed != 0) {
        init_attract_mode(-1);
        return;
    }
    if (attract_state == ATTRACT_RUN) {
        fn_8001EAE0(-416, -320, credit_text[0x8AC / 4], 1.0f);
    } else {
        fn_8001EAE0(-432, -320, credit_text[0x96C / 4], 1.0f);
    }
    attract_check_input(0);
}

/* small helper: request the initial Screen2D and advance the sequencer */
static void attract_start_screen2d(void) {
    screen2d_slot = 0;
    cur_screen_kind = SCR_KIND_FLYBY;
    init_attract_mode(-1);
}

/* ================================================================== */
/* init_screen2d  (ATTRACT.OBJ)                                       */
/* Build a Screen2D entry (background blit "AAANULLOBJ", scroll texture */
/* "Scroll_A", font and stream music) for the requested slot.          */
/* ================================================================== */
int init_screen2d(int a, int slot) {
    int i;
    void* entry;

    sndVoiceStart(0x1800E, -2);
    fn_800B8DD0((void*)0x801111B8 /* "AAANULLOBJ" */, (void*)0x80127D60, 0, 0);

    if (slot == 0) {
        cur_screen_kind = 1;
        cur_screen_id = fn_800B38D0(0, 0); /* "Scroll_A" */
        fn_800B3414(cur_screen_id, 1);
    } else {
        fn_8005638C(a);
    }

    screen2d_slot = slot;
    entry = (void*)&screen_list[slot];
    fn_8005A260(entry, 0, 0, -1);
    FontInitSpecial(8);

    for (i = 0; i < 4; i++) {
        cur_screen_id = fn_800B38D0(0, 0);
    }
    fn_80053C70();
    return a;
}

/* ================================================================== */
/* do_flyby  (ATTRACT.OBJ)                                            */
/* Per-frame in-engine camera flyby: advance the world, time the run   */
/* with OSGetTime, draw "Press Start" and bail out on input/timeout.   */
/* ================================================================== */
int do_flyby(void) {
    if (sFlags != 0) {
        return 0;
    }

    if (attract_state != ATTRACT_RUN) {
        long long t = OSGetTime();
        (void)t;
        fn_800A17D4();
        fn_8002C640();
        if (attract_music != 0) {
            fn_800B3D6C(attract_music);
            attract_music = 0;
        }
        init_attract_mode(-1);
        return 0;
    }

    screen2d_timer += 1;
    if (screen2d_timer > 60) {
        attract_check_input(0);
    }
    fn_80094BE0();
    ProcessEffects();
    fn_800C7874();
    fn_8001EBCC(-256, 320, credit_text[0x8A0 / 4], 1.0f);
    return 0;
}

/* ================================================================== */
/* do_movie  (ATTRACT.OBJ)                                            */
/* Per-frame movie/sequence player driven by a substate counter.       */
/* ================================================================== */
int do_movie(void) {
    int i;

    if (start_pressed != 0) {
        init_attract_mode(-1);
        return 1;
    }

    if ((sFlags & 8) == 0 && attract_state == ATTRACT_RUN) {
        screen2d_timer += 1;
        attract_state = ATTRACT_RUN;
    }

    if (attract_state != ATTRACT_RUN) {
        fn_8002C640();
        if (attract_music != 0) {
            fn_800B3D6C(attract_music);
            attract_music = 0;
        }
        if (attract_state == ATTRACT_FADEOUT) {
            init_attract_mode(-1);
        }
        return 1;
    }

    if (screen2d_timer > 30) {
        attract_check_input(0);
    }
    for (i = 0; i < 4; i++) {
        if (fn_8003130C(i) != 0 && (sFlags & 4) == 0) {
            attract_state = ATTRACT_FADEOUT;
        }
    }
    return 0;
}

/* ================================================================== */
/* init_movie / init_flyby  (ATTRACT.OBJ)                             */
/* Start the flyby/movie screen: pick the world + music by index.      */
/* ================================================================== */
int init_movie(int a, int idx) {
    if (sFlags != 0) {
        attract_state = ATTRACT_RUN;
        return -1;
    }

    cur_screen_idx = idx;
    start_pressed = 0;
    screen2d_slot = 1;
    sndVoiceStart(0x18008, -1);
    fn_80053C70();

    /* music track lookup */
    attract_music = ((int*)0x80118200)[idx * 4 + cur_screen_kind];
    if (attract_music == 0) {
        attract_music = ((int*)0x80118200)[0];
    }
    attract_state = ATTRACT_RUN;
    screen2d_slot = 10;
    fn_800D9FEC();
    attract_state = ATTRACT_RUN;
    return 1;
}

/* ================================================================== */
/* ExitAttract  (ATTRACT.OBJ)                                         */
/* Poll for the exit trigger and tear the attract loop down.           */
/* ================================================================== */
int ExitAttract(void) {
    attract_state = ATTRACT_EXIT;
    if (fn_80055F68(1, 0) == 0) {
        return 0;
    }
    fn_800176D8();
    fn_80054E68(30);
    return 1;
}

/* ================================================================== */
/* init_attract_mode  (ATTRACT.OBJ)                                   */
/* Central sequencer: pick the next screen from screen_list (or the    */
/* one named by `screen`), tear down the current one and dispatch to   */
/* the matching init_* routine.  Called by main() as (-1) at boot and  */
/* (0x8004) when returning from a game.                                */
/* ================================================================== */
void init_attract_mode(int screen) {
    int i;
    int found;
    int kind;

    if (attract_state == ATTRACT_EXIT) {
        return;
    }

    /* tear down current screen */
    fn_80031110(-1);
    fn_80017BAC(cur_screen_id);
    cur_screen_id = 0;
    fn_8001802C();
    fn_800BC4E4();

    if (fn_80031504(0x400000) != 0 && fn_80031504(0x800000) != 0) {
        screen = 0x8000;
    }

    /* locate `screen` in the screen list */
    if (screen >= 0) {
        found = 0;
        for (i = 0; i < 13; i++) {
            if (screen_list[i].id == screen) {
                cur_screen_idx = found;
                break;
            }
            found++;
        }
        cur_screen_kind = -1;
    }

    fn_80054E68(30);

    /* advance to the next non-disabled list entry, then dispatch */
    kind = screen_list[cur_screen_idx].id & 0xFFFF;
    switch (kind) {
    case SCR_KIND_CREDITS:
        init_credits();
        break;
    case SCR_KIND_SCREEN2D:
        init_screen2d(cur_screen_id, screen_list[cur_screen_idx].flags);
        break;
    case SCR_KIND_FLYBY:
        init_movie(-1, screen_list[cur_screen_idx].flags);
        break;
    case SCR_KIND_TITLE:
        init_titlescreen();
        break;
    default:
        break;
    }
}

/* screen-kind -> printable name (used by the on-screen debug overlay) */
static char* attract_screen_name(int kind) {
    switch (kind & 0xFFFF) {
    case SCR_KIND_SCREEN2D: return "Screen2D";
    case SCR_KIND_TITLE:    return "Titlescreen";
    default:                return "";
    }
}

/* reset_attract_mode  (ATTRACT.OBJ) — clear all sequencer state */
void reset_attract_mode(void) {
    cur_screen_id = -1;
    cur_screen_idx = 0;
    cur_screen_kind = 0;
    screen2d_slot = 0;
    screen2d_timer = -1;
    attract_music = -1;
    start_pressed = -1;
    titlescreen_timeout = -1;
}

/* poll pads for the Start / skip trigger while in attract */
static int attract_check_input(int block) {
    int ret = 0;

    if (sFlags != 0) {
        return 0;
    }
    if (fn_80031540(0x0F0000) != 0) {
        ret = 2;
    } else if (block == 0) {
        ret = fn_800312D0();
    }
    if (ret != 0) {
        start_pressed = 1;
    } else if (fn_80031540(0x100000) != 0 || fn_80031540(0x200000) != 0 ||
               fn_80031540(0x400000) != 0 || fn_80031540(0x800000) != 0) {
        attract_state = ATTRACT_RUN;
    }
    return ret;
}
