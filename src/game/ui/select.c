/*
 * select.c -- GCN SELECT.OBJ (shell3D.pdb module .\Release\SELECT.OBJ).
 *
 * The player / character-select front-end. do_player_select() is the top
 * level state machine (called from game/game/gamemain.c and the attract
 * sequencer); it drives the per-player class picker do_sel_menu(), the
 * boss-rune requirement queries used by the "good wizard" auxscreen, and the
 * asynchronous tower/geometry load that primes the select scene.
 *
 * GC emits this TU with the two large drivers (do_player_select, do_sel_menu)
 * near the low end of the range and the small blit/query helpers after them --
 * NOT clean reverse shell3D source order, so functions are mapped behaviorally
 * and confirmed against the PDB roster rather than by position.
 *
 * Text range 0x8008C52C-0x800911C8 (NonMatching: the final DOL links the
 * original bytes for this range via the splits.txt claim; these bodies exist
 * only so the unit compiles and objdiff has something to diff against). The
 * camera/world scene helpers just below (0x8008BF88-0x8008C52C) belong to the
 * world system (gauntworld caller), not this TU.
 *
 * Function map (GCN addr -> shell3D name; confidence):
 *   0x8008C52C reset_sel_menu       (G) zero sel-menu state globals            [med]
 *   0x8008C53C GetBossNumRunes      (G) boss-req table getter, field 0x14=count[high]
 *   0x8008C584 GetBossBeatFlag      (G) boss-req table getter, field 0x10=flags[high]
 *   0x8008C5CC do_player_select     (G) top-level select state machine         [high]
 *   0x8008E340 LimitSeltype         (L) clamp/wrap class index, skip locked    [high]
 *   0x8008E3BC fn_8008E3BC              welcome-back blit (AudioWelcome[Back])
 *   0x8008E4F4 do_sel_menu          (L) per-player class picker (16x from above)[high]
 *   0x8008F768 fn_8008F768              save-name compare helper (strncmp)
 *   0x8008F914 fn_8008F914              small select helper
 *   0x8008F984 fn_8008F984              save-slot text formatter ("SLOT %d")
 *   0x8008FA70 fn_8008FA70              load/save sub-menu state (jumptable)
 *   0x8008FC5C fn_8008FC5C              tiny select helper
 *   0x8008FC78 fn_8008FC78              select-blit serve/update
 *   0x8008FE70 fn_8008FE70              per-player select-state getter (+0x830)
 *   0x8008FED4 fn_8008FED4              small select helper
 *   0x8008FF58 SelectLoadDone        (G) async-load completion poll            [med]
 *   0x8008FFB0 SelectLoadStart       (G) kick off async tower/geo load         [med]
 *   0x8008FFF0 fn_8008FFF0              class-stat text draw (~update_class_attr)
 *   0x80090450 fn_80090450              class-spec draw (~update_class_spec)
 *   0x800907B4 init_player_select    (G) enter/init the select screen          [high]
 *   0x80090B6C fn_80090B6C              blit-init helper (mbBlitInit3414)
 *   0x80090C34 fn_80090C34              blit text printf (vsprintf/mbInitBlit)
 *   0x80090D6C fn_80090D6C              blit build/project helper (jumptable)
 */

#include "types.h"

/* ---- boss-requirement table (this TU, .data 0x80121DD8, 12 x 0x24) ---- */
extern u8 bossRuneReqTable[];

/* ---- select-screen state (small data / bss, shared other TUs) ---- */
extern s32 lbl_80344B7C;
extern s32 lbl_80344B80;
extern s32 lbl_80344BC0;   /* load-in-progress flag */
extern s32 lbl_80343DD4;   /* async load handle      */
extern s32 lbl_803449A0;   /* select mode flag       */
extern u8  lbl_80275AE0[]; /* 4-player array, stride 0x335C */

/* ---- audio / front-end (other TUs) ---- */
extern void AudioWelcome(void);
extern void AudioWelcomeBack(void);
extern void AudioSelectReset(void);
extern void AudioStopSelect(void);
extern void init_titlescreen(void);
extern void init_attract_mode(void);
extern void LoadTowerAndSelect(int a);
extern void remove_player_geo(int a);
extern void msgInit(void);
extern int  saveFileSize(int a);

/* ---- text / MB blit library (other TUs) ---- */
extern int  sprintf(char* buf, const char* fmt, ...);
extern int  vsprintf(char* buf, const char* fmt, void* ap);
extern int  strncmp(const char* a, const char* b, int n);
extern void DrawGlowText(void);
extern void DrawNormalText(void);
extern void DrawTextKeepScale(void);
extern char* GetStringText(int id);
extern void* MBNewTempBlit(void* tex, int x, int y, int w, int h);
extern int  MBCreateBlit(int a, int b, int c, int d, int e, int f);
extern void mbBlitInit3414(int a, int b);
extern void mbBlitProject(void* blit, int w, int h);
extern void mbInitBlitEntry(void);
extern void mbBlitUpdateEntry(void* e);

/* ---- async-load primitives (other TUs) ---- */
extern int  fn_800B79AC(void* name, int a);
extern int  fn_800B7758(void);
extern void fn_8005403C(int a);

/* ============================================================
 * NonMatching stubs.
 * ============================================================ */

void reset_sel_menu(void)
{
    lbl_80344B7C = 0;
    lbl_80344B80 = 0;
}

/* Boss-requirement getters: linear-search a 12-entry, 0x24-byte table keyed on
 * the boss id in entry[0]; return a field of the matching entry (0 if none). */
s32 GetBossNumRunes(s32 boss)
{
    int i;
    s32* e = (s32*)bossRuneReqTable;
    for (i = 0; i < 13; i++, e += 9) {
        if (e[0] == boss) {
            return e[5]; /* +0x14: number of runes required */
        }
    }
    return 0;
}

s32 GetBossBeatFlag(s32 boss)
{
    int i;
    s32* e = (s32*)bossRuneReqTable;
    for (i = 0; i < 13; i++, e += 9) {
        if (e[0] == boss) {
            return e[4]; /* +0x10: rune "beat" flag mask */
        }
    }
    return 0;
}

/* Clamp/wrap a class-selection index into the valid range for the current
 * select mode, skipping classes that are locked (bit 23 of +0xA8C). */
static s32 LimitSeltype(u8* player, s32 idx, s32 step)
{
    if (lbl_803449A0 == 0) {
        if (idx < 4) {
            return 7;
        }
        if (idx > 7) {
            return 4;
        }
        return idx;
    }
    for (;;) {
        if (idx > 16) {
            idx = 0;
        }
        if (idx < 0) {
            idx = 16;
        }
        if (idx == 16 && (*(u16*)(player + 0xA8C) & 0x100) == 0) {
            idx += step;
            continue;
        }
        return idx;
    }
}

static void do_sel_menu(int player);

/* Top-level select state machine (invoked from gamemain / attract). */
void do_player_select(void)
{
    int i;
    init_titlescreen();
    init_attract_mode();
    for (i = 0; i < 16; i++) {
        do_sel_menu(i);
    }
    AudioWelcome();
}

static void do_sel_menu(int player)
{
    (void)player;
    LimitSeltype(lbl_80275AE0, 4, 1);
    DrawGlowText();
    DrawTextKeepScale();
    MBNewTempBlit(0, 0, 0, 0, 0);
}

void fn_8008E3BC(void)
{
    AudioWelcomeBack();
    AudioWelcome();
    mbBlitProject(0, 0, 0);
}

int fn_8008F768(const char* name)
{
    return strncmp(name, (const char*)lbl_80275AE0, 8);
}

void fn_8008F914(void)
{
}

void fn_8008F984(char* buf, int slot)
{
    sprintf(buf, " SLOT  %d", slot);
}

void fn_8008FA70(void)
{
}

void fn_8008FC5C(void)
{
}

void fn_8008FC78(void)
{
    mbBlitProject(0, 0, 0);
}

s32 fn_8008FE70(s32 idx)
{
    int i;
    u8* p = lbl_80275AE0;
    for (i = 0; i < 4; i++, p += 0x335C) {
        if (i == idx) {
            return *(s32*)(p + 0x830);
        }
    }
    return *(s32*)(lbl_80275AE0 + idx * 0x335C + 0x830);
}

void fn_8008FED4(void)
{
}

/* Poll the async tower/geometry load: 0 while loading, 1 when it just
 * finished, 2 once already complete. */
int SelectLoadDone(void)
{
    if (lbl_80344BC0 != 0) {
        return 2;
    }
    if (fn_800B7758() == 0) {
        return 0;
    }
    fn_8005403C(2);
    lbl_80344BC0 = 1;
    return 1;
}

/* Begin the async tower/geometry load for the select scene. */
void SelectLoadStart(void)
{
    if (lbl_80343DD4 < 0) {
        lbl_80344BC0 = 0;
        lbl_80343DD4 = fn_800B79AC(0, -1);
    }
}

void fn_8008FFF0(void)
{
    GetStringText(0);
    DrawNormalText();
    DrawGlowText();
    MBNewTempBlit(0, 0, 0, 0, 0);
}

void fn_80090450(void)
{
    mbBlitInit3414(0, 0);
    mbBlitProject(0, 0, 0);
}

/* Enter / initialise the select screen. */
void init_player_select(int mode)
{
    (void)mode;
    AudioStopSelect();
    remove_player_geo(0);
    AudioSelectReset();
    LoadTowerAndSelect(0);
    msgInit();
    MBCreateBlit(0, 0, 0, 0, 0, 0);
}

void fn_80090B6C(void)
{
    mbBlitInit3414(0, 0);
}

void fn_80090C34(char* fmt)
{
    char buf[128];
    vsprintf(buf, fmt, 0);
    mbInitBlitEntry();
    mbBlitProject(0, 0, 0);
}

void fn_80090D6C(void)
{
    mbBlitInit3414(0, 0);
    mbBlitProject(0, 0, 0);
}
