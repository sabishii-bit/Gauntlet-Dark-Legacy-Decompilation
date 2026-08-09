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
extern int  sndVoiceStart(int a, int b);          /* sndFxInit (sndfx) */
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
extern int  FireScrollActive(void);
extern s32  HintMenu(s32 type);
extern void TitleMenuEnd(void);
extern s32  TitleMenu(s32 y);
extern void TitleMenuInit(s32 sel);
extern void fn_8009D350(int a);
extern void AudioCursorSelect(void);
extern void EndTower(void);
extern void fn_80053C70(void);
extern void SetMaxFPS(int a);
extern void init_next_level(int a);
extern int init_next_level_8005638C(int a);
extern int  LoadModel(void* a, void* b, int c, int d);
extern void fn_80057024(void);
extern int  NextAttractWave(void);
extern void InitCamera(int a);
extern void init_targets(void);
extern void AudioReset(int a);          /* was AudioReset */
extern void AudioEmptyCb1(void);        /* was AudioEmptyCb1 */
extern void MBOX_NewObject(void* dst, void* name, int a, int b);
extern int  MBOX_FindTexture(const char* name, int flag);
extern void MBBlitSetAlpha(int handle, int alpha);
extern void MBFontMsgSetAlpha(int a, int b);
extern void MBWindowZoom(float a);
extern void fn_800BC4E4(void);
extern void fn_800C7864(int a);
extern int  fn_800C7874(void);
extern void PlayVQMovie(int a);
extern int  DrawGlowText(int x, int y, void* str, float scale);   /* btext */
extern int  DrawGlowTextMLines(int x, int y, void* str, float scale);   /* btext */
extern void* DrawText(int x, int y, int flags, u32 color, const char* fmt, ...);
extern void* DrawTextKeepScale(float scale, int x, int y, int flags,
                               u32 color, const char* text);

/* forward decls (project style) */
void init_attract_mode(int screen);
int  scroll_credits(void);
static int  attract_check_input(int block);
static void attract_start_screen2d(void);
const char* attract_screen_name_80014F34(int kind);

/* 64-bit attract flag word at 0x803445C8 (its low half aliases sFlags).
 * Masked tests compile to the and/and/xor/xor/or. long-long idiom. */
#define ATTRACT_FLAGS64 gControllerButtons

/* ------------------------------------------------------------------ */
/* Module state (real globals live in .sdata/.sbss; kept file-local    */
/* here because this NonMatching TU is compiled but not linked).        */
/* ------------------------------------------------------------------ */
static int  attract_state;          /* per-screen run state            */
static int  titlescreen_timeout;    /* title fade / dwell countdown    */
static int  did_titlesound;         /* one-shot title jingle latch     */
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
extern const char lbl_80111294[];
extern const char lbl_801112A0[];
extern char lbl_80126A98[];
extern char lbl_80110900[];
extern char gIdentityMatrix[];
extern char lbl_80118188[];
extern char lbl_801111B8[];
extern char lbl_80111278[];
extern const char creditsModelName[8];
extern const char numberedTextureFmt[7];
extern const char gauntFontName[6];
extern int  lbl_80118250[];
extern int  lbl_8023D1E0[];
extern int  lbl_8023D1F0[];
extern const char titleModelName[6];
extern const char titleTexturePrefix[6];
extern const float titleWindowZoom;
extern float screen2dTextScale;
extern float gClockFrameStep;
extern float sMusicFadeBase;
extern const double lbl_80345920;
extern const double lbl_80345928;
extern const char lbl_803458F8;
extern const char lbl_80345900;
extern const char lbl_80345908;
extern const char lbl_80345910;
extern const char lbl_80345918;

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
extern s32 opt_force_player;
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
int credits_scroll;
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
unsigned int gFrameTicks;
int gGameBusy;
int gClockStepTicks;
long long gControllerButtons;
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
extern float creditsTextScale;

/* ================================================================== */
/* do_titlescreen  (ATTRACT.OBJ)                                       */
/* Per-frame title-screen handler: fade the logo in, blink "Press      */
/* Start", run the idle timeout, and hand back to init_attract_mode.   */
/* ================================================================== */
void do_titlescreen(void) {
    int i;
    int alpha;

    if (did_titlesound != 0) {
        TitleMenuEnd();
        AudioSelectReset();
        init_attract_mode(-1);
        attract_state = ATTRACT_RUN;
        return;
    }

    titlescreen_timeout += 1;
    alpha = (titlescreen_timeout < 60)
              ? (titlescreen_timeout * 255) / 60
              : 255;
    MBBlitSetAlpha(cur_screen_id, alpha);

    for (i = 0; i < 4; i++) {
        mbBlitCvtCoord(credit_text[i] != 0, 1.0f);
    }

    if (SelectLoadDone() == 0 || fn_80055F68(1, 0) == 0) {
        titlescreen_timeout = 0;
    }

    /* "Press Start" prompt + logo, then poll for Start to leave. */
    DrawGlowText(-256, 320, credit_text[0x228 / 4], 1.0f);
    if (FireScrollActive() == 0) {
        mbBlitInit3414(cur_screen_id, 1);
    }
    if (titlescreen_timeout > 1800) {
        TitleMenuEnd();
        EndTower();
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
        EndTower();
        sndFxInit(0x8009, -1);
        MBOX_NewObject(base + 2232, gIdentityMatrix, 0, 0);
        bulletproof_printf(base + 2244);
        lbl_80343B38 = LoadModel((void*)titleModelName, 0, 1, -1);
    } else {
        sndFxInit(0x8009, -3);
    }

    lbl_803448C4 = 0;
    lbl_803448C8 = 13;
    lbl_803448AC = -1;
    lbl_803448A8 = -1;
    lbl_80344C4C = 0;
    opt_force_player = 0;
    InitCamera(1);
    bulletproof_printf(base + 2264);
    AudioSelectReset();
    bulletproof_printf(base + 2280);
    AudioSelect(1);
    lbl_80344268 = 0;
    lbl_80344298 = 0;
    lbl_80344260 = 1800;

    for (i = 0; i < 4; i++) {
        sprintf(buf, (void*)numberedTextureFmt, (void*)titleTexturePrefix, i);
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
    MBWindowZoom(titleWindowZoom);
    lbl_80344270 = 0;
    lbl_80344274 = 30;
    MBBlitSetAlpha(lbl_80344264, 255);
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

    if (gGameBusy == 0) {
        done = scroll_credits();
        lbl_80344778 += gFrameTicks;
    }

    if (AudioSysUpdate(0x186A0) != 0) {
        done = -1;
    }

    if (lbl_80344298 == 0 && gGameBusy == 0) {
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
    char** text;
    int y;
    int result;
    int alive;
    u32 idx;
    int offBottom;
    int offset;
    void* line;

    text = credit_text;
    result = 0;
    alive = 1;
    idx = 0;
    offBottom = 0;
    offset = 0;
    y = credits_scroll;

    /* column 1 (92 lines) */
    do {
        if (y < 0) {
            break;
        }
        if (y < 384) {
            line = DrawText(32, 383 - y, 13, 0xFFFFFF,
                            ((char**)((u8*)text + offset))[337]);
            if (line != NULL) {
                if (384 - y < 16) {
                    MBFontMsgSetAlpha((int)line,
                                      255 - ((384 - y) << 4));
                }
                if (y < 16) {
                    MBFontMsgSetAlpha((int)line, (16 - y) << 4);
                }
                if (idx == 91 && y > 192) {
                    offBottom = 1;
                }
                alive = 0;
            }
        }
        idx++;
        y -= 21;
        offset += 4;
    } while (idx < 92);

    if (offBottom != 0 || alive != 0) {
        result = 1;
    }

    idx = 0;
    offset = 0;
    do {
        if (y < 0) {
            break;
        }
        if (y < 384) {
            line = DrawTextKeepScale(creditsTextScale, 32, 383 - y, 13,
                                     0xFFFFFF,
                                     ((char**)((u8*)text + offset))[429]);
            if (line != NULL) {
                if (384 - y < 16) {
                    MBFontMsgSetAlpha((int)line,
                                      255 - ((384 - y) << 4));
                }
                if (y < 16) {
                    MBFontMsgSetAlpha((int)line, (16 - y) << 4);
                }
                alive = 0;
            }
        }
        idx++;
        y -= 15;
        offset += 4;
    } while (idx < 40);

    idx = 0;
    offset = 0;
    do {
        if (y < 0) {
            break;
        }
        if (y < 384) {
            line = DrawTextKeepScale(creditsTextScale, 32, 383 - y, 13,
                                     0xFFFFFF,
                                     ((char**)((u8*)text + offset))[469]);
            if (line != NULL) {
                if (384 - y < 16) {
                    MBFontMsgSetAlpha((int)line,
                                      255 - ((384 - y) << 4));
                }
                if (y < 16) {
                    MBFontMsgSetAlpha((int)line, (16 - y) << 4);
                }
                alive = 0;
            }
        }
        idx++;
        y -= 15;
        offset += 4;
    } while (idx < 10);

    if (gFrameTicks != 0) {
        if (lbl_803447C0 != 0) {
            credits_scroll += 3;
        } else {
            credits_scroll += result + 1;
        }
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

    SetMaxFPS(30);
    lbl_80343B04 = -1;
    credits_scroll = 0;
    lbl_80343B0C = -1;
    lbl_80343B10 = -1;
    lbl_80343B08 = -1;
    sndFxInit(0x8000, -1);
    MBOX_NewObject(base + 2232, gIdentityMatrix, 0, 0);
    init_next_level_8005638C(-1);
    lbl_80344258 = LoadModel((void*)creditsModelName, 0, 1, -1);
    FontInitSpecial((void*)creditsModelName, 8);
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
        lbl_80344254 -= gClockStepTicks;
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
    lbl_8034422C = lbl_8034422C + gFrameTicks;
    delta = gFrameTicks;

    if (state != 0) {
        if (lbl_80344234 != 0 && lbl_80344794 == 0) {
            init_attract_mode(-1);
            lbl_803441F0 = 1;
            return;
        }
    }

    lbl_80344228 -= delta;
    if (state == 0 && gGameBusy == 0 && (ATTRACT_FLAGS64 & 4) == 0 &&
        lbl_80344234 != 0 && lbl_80344228 <= 0) {
        lbl_80344298 = 1;
    }

    attract_check_input(0);
    if (lbl_80344794 != 0) {
        DrawGlowTextMLines(-416, -320, base + 2220, screen2dTextScale);
    } else if (lbl_80344298 == 0) {
        DrawGlowTextMLines(-432, -320, base + 2412, screen2dTextScale);
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
    MBOX_NewObject(lbl_801111B8, gIdentityMatrix, 0, 0);

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
    lbl_80344224 = LoadModel(p + lbl_80344238 * 80, 0, 0, -1);
    if (*(int*)(base + so + lbl_80344238 * 80 + 288) >= 0) {
        FontInitSpecial((void*)gauntFontName, 8);
    }

    for (i = 0; i < 4; i++) {
        sprintf(buf, numberedTextureFmt, p + lbl_80344238 * 80 + 32, i);
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
        AudioSelectReset();
        init_targets();
        if (attract_music != 0) {
            MBRemoveBlit(attract_music);
            attract_music = 0;
        }
        init_attract_mode(-1);
        return 0;
    }

    screen2d_timer += 1;
    if (screen2d_timer > 60) {
        attract_check_input(0);
    }
    ProcessEffects();
    ProcessEffects();
    fn_800C7874();
    DrawGlowText(-256, 320, credit_text[0x8A0 / 4], 1.0f);
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

    if (gGameBusy == 0 && (ATTRACT_FLAGS64 & 8) == 0 && lbl_80344298 == 0) {
        lbl_80344298 = 1;
        flag = 2;
        lbl_80344778 += gFrameTicks;
    }

    if (lbl_80344298 != 0) {
        lbl_80344218 = 21;
        init_targets();
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

    if (lbl_80344298 == 0 && gGameBusy == 0 && flag == 2) {
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
    PlayVQMovie(lbl_8034428C);
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
    SetMaxFPS(30);
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

    SetMaxFPS(30);

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
#pragma force_active on
const char* attract_screen_name_80014F34(int kind) {
    switch (kind) {
    default:
    case 0x8007:
        return &lbl_803458F8;
    case 0x8000:
        return &lbl_80345900;
    case 0x8004:
        return lbl_80111294;
    case 0x8001:
    case 0x8002:
        return &lbl_80345908;
    case 0x8003:
    case 0x8006:
    case 0x8008:
        return &lbl_80345910;
    case 0x8009:
        return lbl_801112A0;
    case 0x8005:
        return &lbl_80345918;
    }
}
#pragma force_active reset

/* reset_attract_mode  (ATTRACT.OBJ) - clear all sequencer state */
#pragma opt_propagation off
void reset_attract_mode(void) {
    register s32 none;
    register s32 zero;

    none = -1;
    zero = 0;

    lbl_80343B04 = none;
    lbl_803441F4 = zero;
    lbl_80344294 = zero;
    lbl_80344290 = zero;
    sAudioOverride = zero;
    lbl_80343B0C = none;
    lbl_80343B10 = none;
    lbl_80343B08 = none;
    lbl_80343B14 = none;
}
#pragma opt_propagation reset

/* poll pads for the Start / skip trigger while in attract */
static int attract_check_input(int block) {
    int ret = 0;

    if ((ATTRACT_FLAGS64 & 4) != 0) {
        return 0;
    }
    if (gClockFrameStep > lbl_80345920 && sMusicFadeBase > lbl_80345928) {
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
