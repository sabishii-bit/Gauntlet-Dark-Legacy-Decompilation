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
extern void FreeHiMem(int a);
extern void DrawText(int a, int b, int c, int d, const char* fmt, ...);
extern void DrawTextKeepScale(void);
extern void DrawGlowText(void);
extern void DrawGlowTextMLines(int a, int b, void* c, float d);
extern void FontInitSpecial(void* a, int b);
extern int  MBNewBlit(void* a, int b, int c);
extern int  MBCreateBlit(int a, int b, int c, int d, int e, int f);
extern void MBRemoveBlit(int handle);
extern void mbInitBlitEntry(void);
extern void mbBlitInit3414(int a, int b);
extern void mbBlitCvtCoord(int a, float b);
extern void LoadWorldData(void);
extern int  AudioSelect(int a);
extern void AudioSelectReset(void);
extern void AudioBuildStreamName(void* a, int b);
extern void ProcessEffects(void);
extern void bulletproof_printf(char* fmt, ...);
extern int  sprintf(char* buf, const char* fmt, ...);
extern long long OSGetTime(void);

extern void AudioStreamStop(void);
extern int  AudioSysUpdate(int a);
extern void sndFxInit(int a, int b);
extern int  sndVoiceStart(int a, int b);          /* fn_800150CC (sndfx) */
extern int  any_level(int mask);                /* pad-held query      */
extern int  any(int mask);                /* pad-pressed query   */
extern int  start_no_assignment(void);                    /* any-pad-pressed     */
extern int  new_start(int pad);                 /* pad-active query    */
extern int  controls_first_active_player(void);
extern int  controls_remove_active_player(int pad);
extern int  fn_80055F68(int a, int b);            /* start/button check  */
extern int  SelectLoadDone(void);
extern void SelectLoadStart(void);
extern void init_player_select(int a);
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
extern int init_next_level_8005638C(int a);
extern int  fn_8005A260(void* a, void* b, int c, int d);
extern void fn_80057024(void);
extern int  NextAttractWave(void);
extern void fn_8002CF78(int a);
extern void fn_8002C640(void);
extern void AudioStreamStop(void);      /* was fn_800176D8 */
extern int  AudioSysUpdate(int a);      /* was fn_80017B18 */
extern void AudioReset(int a);          /* was fn_80017BAC */
extern void AudioEmptyCb1(void);        /* was fn_8001802C */
extern void fn_800B8DD0(void* dst, void* name, int a, int b);
extern int  MBOX_FindTexture(const char* name, int flag);
extern int  fn_800B38D0(int a, int b);
extern void fn_800B3414(int handle, int a);
extern void fn_800B290C(int handle, int alpha);
extern void fn_800B2988(int a, int b, int c, int d, int e);
extern void fn_800B2F8C(int a, float b);
extern int  fn_800B3AFC(int a, int b, int c, int d, int e, int f);
extern void fn_800B3D6C(int a);
extern void fn_800B5AA8(int a, int b);
extern void MBWindowZoom(float a);
extern void fn_800BC2EC(void* a, ...);
extern void fn_800BC4E4(void);
extern void fn_800C7864(int a);
extern int  fn_800C7874(void);
extern void fn_800D9FEC(int a);
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

/* 64-bit attract flag word at 0x803445C8 (its low half aliases sFlags).
 * Masked tests compile to the and/and/xor/xor/or. long-long idiom. */
#define ATTRACT_FLAGS64 lbl_803445C8

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
extern int  lbl_80118200[];
extern char lbl_80111238[];
extern char lbl_80126A98[];
extern char lbl_80110900[];
extern char lbl_80127D60[];
extern char lbl_80118188[];
extern char lbl_801111B8[];
extern char lbl_80111278[];
extern const char lbl_803458D4[8];
extern const char lbl_803458BC[7];
extern const char lbl_803458DC[6];
extern int  lbl_80118250[];
extern int  lbl_8023D1E0[];
extern int  lbl_8023D1F0[];
extern const char lbl_803458B4[6];
extern const char lbl_803458C4[6];
extern const float lbl_803458CC;
extern float lbl_803458B0;
extern float lbl_80344590;
extern float sMusicFadeBase;
extern const double lbl_80345920;
extern const double lbl_80345928;

/* Real .sdata/.sbss globals of ATTRACT.OBJ, named by their DOL address so
 * the EMB_SDA21 relocations carry the target symbol names. */
int lbl_80343B04 = -1;
int lbl_80343B08 = -1;
int lbl_80343B0C = -1;
int lbl_80343B10 = -1;
int lbl_80343B14 = -1;
int lbl_80343B38;
float lbl_80343B3C;
int lbl_803448A8;
int lbl_803448AC;
int lbl_803448C4;
int lbl_803448C8;
int lbl_80344260;
int lbl_80344264;
int lbl_80344268;
int lbl_8034426C;
int lbl_80344270;
int lbl_80344274;
int lbl_80344AF8;
int lbl_80344C4C;
int lbl_803441F4;
int lbl_803441F8;
int lbl_803441FC;
int lbl_80344210;
int lbl_80344214;
int lbl_80344224;
int lbl_80344238;
int lbl_80344240;
int lbl_8034424C;
int lbl_80344228;
int lbl_8034422C;
int lbl_80344230;
int lbl_80344234;
int lbl_8034423C;
int lbl_80344254;
int lbl_80344258;
int lbl_8034425C;
int lbl_80344244;
int lbl_80344248;
int lbl_80344250;
unsigned int lbl_8034428C;
int lbl_80344290;
int lbl_80344294;
int lbl_80344298;
int lbl_80344204;
unsigned int lbl_80344208;
int lbl_80344218;
int lbl_8034457C;
int lbl_80344568;
int lbl_80344578;
long long lbl_803445C8;
int lbl_80344778;
int lbl_80344794;
int lbl_803447C0;
int lbl_803449A4;
unsigned char lbl_803441F0;
int sAudioOverride;

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

    if (SelectLoadDone() == 0 || fn_80055F68(1, 0) == 0) {
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
    char* base = lbl_80110900;
    char buf[32];
    int i;
    int* e;

    FreeHiMem(2);
    if (lbl_80343B38 < 0) {
        fn_80053B20();
        sndFxInit(0x8009, -1);
        fn_800B8DD0(base + 2232, lbl_80127D60, 0, 0);
        bulletproof_printf(base + 2244);
        lbl_80343B38 = fn_8005A260((void*)lbl_803458B4, 0, 1, -1);
    } else {
        sndFxInit(0x8009, -3);
    }

    lbl_803448C4 = 0;
    lbl_803448C8 = 13;
    lbl_803448AC = -1;
    lbl_803448A8 = -1;
    lbl_80344C4C = 0;
    lbl_80344AF8 = 0;
    fn_8002CF78(1);
    bulletproof_printf(base + 2264);
    AudioSelectReset();
    bulletproof_printf(base + 2280);
    AudioSelect(1);
    lbl_80344268 = 0;
    lbl_80344298 = 0;
    lbl_80344260 = 1800;

    for (i = 0; i < 4; i++) {
        sprintf(buf, (void*)lbl_803458BC, (void*)lbl_803458C4, i);
        e = &lbl_80118250[i * 2];
        lbl_8023D1F0[i] = MBNewBlit(buf, e[0], e[1]);
        mbBlitCvtCoord(lbl_8023D1F0[i], lbl_80343B3C);
    }

    lbl_8034426C = MBOX_FindTexture(base + 2296, 0);
    lbl_80344264 = MBCreateBlit(0, lbl_8034426C, 192, 0, 128, 128);
    bulletproof_printf(base + 2308);
    SelectLoadStart();
    bulletproof_printf(base + 2324);
    fn_80053C70();
    MBWindowZoom(lbl_803458CC);
    lbl_80344270 = 0;
    lbl_80344274 = 30;
    fn_800B290C(lbl_80344264, 255);
    bulletproof_printf(base + 2336);
}

/* ================================================================== */
/* do_credits  (ATTRACT.OBJ)                                          */
/* Per-frame credits handler: run the scroller, show the build         */
/* version, and time out after the roll completes.                     */
/* ================================================================== */
void do_credits(void) {
    int fontFlag;
    int done;
    long long flags;

    done = 0;
    fontFlag = (lbl_803447C0 != 0) ? 512 : 1;

    if (lbl_80344298 != 0) {
        AudioStreamStop();
        lbl_80344204 = 1;
        init_attract_mode(-1);
        return;
    }

    if (any_level(0x400000) != 0 && any_level(0x800000) != 0) {
        DrawText(292, 340, fontFlag, 0xFFFF80, lbl_80111238, lbl_80126A98);
    }

    if (lbl_80344568 == 0) {
        done = scroll_credits();
        lbl_80344778 += lbl_8034457C;
    }

    if (AudioSysUpdate(0x186A0) != 0) {
        done = -1;
    }

    if (lbl_80344298 == 0 && lbl_80344568 == 0) {
        flags = ATTRACT_FLAGS64;
        if ((flags & 4) == 0 &&
            done >= 0 && (lbl_80344778 >= 5400 || done != 0)) {
            lbl_80344298 = 1;
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
    char* base = lbl_80110900;
    char buf[32];
    int i;
    int* e;

    fn_80054E68(30);
    lbl_80343B04 = -1;
    lbl_8034425C = 0;
    lbl_80343B0C = -1;
    lbl_80343B10 = -1;
    lbl_80343B08 = -1;
    sndFxInit(0x8000, -1);
    fn_800B8DD0(base + 2232, lbl_80127D60, 0, 0);
    init_next_level_8005638C(-1);
    lbl_80344258 = fn_8005A260((void*)lbl_803458D4, 0, 1, -1);
    FontInitSpecial((void*)lbl_803458D4, 8);
    AudioSelectReset();
    lbl_80344298 = 0;

    for (i = 0; i < 4; i++) {
        sprintf(buf, base + 2372, i);
        e = &lbl_80118250[i * 2];
        lbl_8023D1E0[i] = MBNewBlit(buf, e[0], e[1]);
    }
    AudioBuildStreamName(base + 2388, 1);
    fn_80053C70();
}

/* ================================================================== */
/* do_screen2d  (ATTRACT.OBJ)                                         */
/* Per-frame handler for the scrolling 2D info screens.                */
/* ================================================================== */
void do_screen2d(void) {
    u8 unused[8];
    char* base = lbl_80110900;
    int delta;
    int state;

    if (lbl_80344254 > 0) {
        lbl_80344254 -= lbl_80344578;
    }
    if (lbl_80344254 < 0) {
        lbl_80344254 = 0;
    }

    if (lbl_8034423C == 0) {
        attract_start_screen2d();
        return;
    }

    if (lbl_80344234 == 0) {
        if (lbl_80344230 != 0) {
            lbl_80344230 -= 1;
        } else if (fn_80055F68(1, 0) != 0) {
            lbl_80344234 = 1;
            lbl_80343B08 = lbl_80343B04;
            lbl_80343B10 = lbl_80343B0C;
        }
    }

    MBOX_FindTexture(base + 2400, 0);
    state = lbl_80344298;
    delta = lbl_8034457C;
    lbl_8034422C += delta;

    if (state != 0) {
        if (lbl_80344234 != 0 && lbl_80344794 == 0) {
            init_attract_mode(-1);
            lbl_803441F0 = 1;
            return;
        }
    }

    lbl_80344228 -= delta;
    if (state == 0 && lbl_80344568 == 0 && (ATTRACT_FLAGS64 & 4) == 0 &&
        lbl_80344234 != 0 && lbl_80344228 <= 0) {
        lbl_80344298 = 1;
    }

    attract_check_input(0);
    if (lbl_80344794 != 0) {
        DrawGlowTextMLines(-416, -320, base + 2220, lbl_803458B0);
    } else if (lbl_80344298 == 0) {
        DrawGlowTextMLines(-432, -320, base + 2412, lbl_803458B0);
    }
}

/* small helper: request the initial Screen2D and advance the sequencer */
static void attract_start_screen2d(void) {
    lbl_80344244 = 0;
    lbl_80344248 = 0;
    lbl_80344250 = 1;
    lbl_803441FC = 2;
    init_attract_mode(-1);
}

/* ================================================================== */
/* init_screen2d  (ATTRACT.OBJ)                                       */
/* Build a Screen2D entry (background blit "AAANULLOBJ", scroll texture */
/* "Scroll_A", font and stream music) for the requested slot.          */
/* ================================================================== */
int init_screen2d(int a, int slot) {
    char* base = lbl_80118188;
    char* p;
    int so;
    char buf[28];
    int i;

    lbl_80343B04 = -1;
    lbl_80343B0C = -1;
    lbl_80343B10 = -1;
    lbl_80343B08 = -1;
    sndFxInit(0x8004, -2);
    fn_800B8DD0(lbl_801111B8, lbl_80127D60, 0, 0);

    if (slot == 0) {
        lbl_803441FC = 1;
        lbl_80344240 = MBNewBlit(lbl_80111278, 0, 0);
        mbBlitInit3414(lbl_80344240, 1);
    } else {
        lbl_80343B04 = init_next_level_8005638C(a);
        lbl_80344240 = 0;
    }

    lbl_8034423C = slot;
    lbl_80344238 = lbl_80343B14;
    so = slot * 320;
    p = base + so + 232;
    lbl_80344224 = fn_8005A260(p + lbl_80344238 * 80, 0, 0, -1);
    if (*(int*)(base + so + lbl_80344238 * 80 + 288) >= 0) {
        FontInitSpecial((void*)lbl_803458DC, 8);
    }

    for (i = 0; i < 4; i++) {
        sprintf(buf, lbl_803458BC, p + lbl_80344238 * 80 + 32, i);
        lbl_8023D1E0[i] = MBNewBlit(buf, *(int*)(base + i * 8 + 200),
                                    *(int*)(base + i * 8 + 204));
    }

    if ((sAudioOverride = 1) != 0 &&
        *(signed char*)(base + so + lbl_80344238 * 80 + 292) != 0) {
        AudioBuildStreamName(p + lbl_80344238 * 80 + 60,
                             *(int*)(base + so + lbl_80344238 * 80 + 308));
    }

    lbl_80344298 = 0;
    lbl_80344228 = *(int*)(base + so + lbl_80344238 * 80 + 284);
    lbl_80344230 = 0;
    lbl_80344234 = 0;
    lbl_8034422C = 0;
    lbl_80344244 = 0;
    lbl_80344248 = 0;
    lbl_8034424C = -1;
    lbl_80344250 = 0;
    lbl_80344254 = 0;
    fn_80053C70();
    return lbl_80343B04;
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
    int flag;

    flag = 0;
    if (lbl_803449A4 != 0) {
        init_attract_mode(-1);
        return 1;
    }

    if (lbl_80344568 == 0 && (ATTRACT_FLAGS64 & 8) == 0 && lbl_80344298 == 0) {
        lbl_80344298 = 1;
        flag = 2;
        lbl_80344778 += lbl_8034457C;
    }

    if (lbl_80344298 != 0) {
        lbl_80344218 = 21;
        fn_8002C640();
        if (lbl_80344208 != 0) {
            MBRemoveBlit(lbl_80344208);
            lbl_80344208 = 0;
        }
        if (lbl_80344298 == 1) {
            init_attract_mode(-1);
        }
        lbl_80344218 = 29;
        return 1;
    }

    if (lbl_80344298 == 0 && lbl_80344568 == 0 && flag == 2) {
        lbl_80344298 = 1;
    }
    lbl_80344218 = 31;
    if (lbl_80344778 > 30) {
        attract_check_input(0);
    }
    lbl_80344218 = 32;
    for (i = 0; i < 4; i++) {
        if (new_start(i) != 0 || (ATTRACT_FLAGS64 & 4) != 0) {
            lbl_80344298 = 2;
        }
    }
    lbl_80344218 = 99;
    return 0;
}

/* ================================================================== */
/* init_movie / init_flyby  (ATTRACT.OBJ)                             */
/* Start the flyby/movie screen: pick the world + music by index.      */
/* ================================================================== */
int init_movie(int a, int idx) {
    int* row;

    if (lbl_803449A4 != 0) {
        lbl_80344298 = 1;
        return -1;
    }

    lbl_80344214 = idx;
    lbl_803441F8 = 0;
    lbl_80344210 = 1;
    lbl_80343B04 = -1;
    lbl_80343B0C = -1;
    lbl_80343B10 = -1;
    lbl_80343B08 = -1;
    sndFxInit(lbl_80344290, -1);
    fn_80053C70();
    lbl_80344210 = 2;

    row = &lbl_80118200[idx * 4];
    lbl_8034428C = row[lbl_80343B14];
    if (lbl_8034428C == 0) {
        lbl_8034428C = row[0];
    }
    lbl_80344298 = 0;
    lbl_80344210 = 10;
    fn_800D9FEC(lbl_8034428C);
    lbl_80344298 = 1;
    lbl_80344210 = 99;
    return 1;
}

/* ================================================================== */
/* ExitAttract  (ATTRACT.OBJ)                                         */
/* Poll for the exit trigger and tear the attract loop down.           */
/* ================================================================== */
int ExitAttract(void) {
    lbl_80344298 = ATTRACT_EXIT;
    if (fn_80055F68(0, 1) == 0) {
        return 0;
    }
    AudioStreamStop();
    lbl_80343B04 = -1;
    lbl_80343B0C = -1;
    lbl_80343B10 = -1;
    lbl_80343B08 = -1;
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
    controls_remove_active_player(-1);
    AudioReset(cur_screen_id);
    cur_screen_id = 0;
    AudioEmptyCb1();
    fn_800BC4E4();

    if (any_level(0x400000) != 0 && any_level(0x800000) != 0) {
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

/* reset_attract_mode  (ATTRACT.OBJ) - clear all sequencer state */
void reset_attract_mode(void) {
    lbl_80343B04 = -1;
    lbl_803441F4 = 0;
    lbl_80344294 = 0;
    lbl_80344290 = 0;
    sAudioOverride = 0;
    lbl_80343B0C = -1;
    lbl_80343B10 = -1;
    lbl_80343B08 = -1;
    lbl_80343B14 = -1;
}

/* poll pads for the Start / skip trigger while in attract */
static int attract_check_input(int block) {
    int ret = 0;

    if ((ATTRACT_FLAGS64 & 4) != 0) {
        return 0;
    }
    if (lbl_80344590 > lbl_80345920 && sMusicFadeBase > lbl_80345928) {
        if (any(0xF000000) != 0) {
            ret = 2;
        } else if (block == 0) {
            ret = start_no_assignment();
        }
    }
    if (ret != 0) {
        lbl_80344794 = 1;
    } else if (any(0x100000) != 0 || any(0x200000) != 0 ||
               any(0x400000) != 0 || any(0x800000) != 0) {
        lbl_80344298 = 1;
    }
    return ret;
}
