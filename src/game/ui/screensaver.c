/*
 * screensaver.c -- GCN MESSAGE.OBJ (shell3D.pdb module .\Release\MESSAGE.OBJ).
 *
 * This is the REAL MESSAGE.OBJ translation unit (the file game/ui/message.c in
 * this tree is a different, provisionally-named module -- the in-game message
 * queue at 0x800A4870 -- NOT this one). Named after ScreenSaver(), the unique,
 * globally-visible PDB anchor that the main loop (game/sys/main.c) calls once
 * the idle timer trips.
 *
 * The TU bundles the front-end overlay displays: the attract-mode "screen
 * saver" (four weapons bouncing around the screen), the between-level inventory
 * panel, and the modal message/dialog boxes. GC emits the module in ~reverse
 * shell3D source order, so ScrollMessageBox (last in source) lands first.
 *
 * Text range 0x8006B540-0x8006DC2C (NonMatching: bytes come from the DOL split;
 * this file documents structure and carries the real symbol names).
 *
 * Function map (GCN addr -> shell3D name; confidence):
 *   0x8006B540 ScrollMessageBox         (G) modal multi-line msg + fullscreen fade  [high]
 *   0x8006B85C ScreenSaver              (G) idle-timer attract loop (36000 frames)   [high]
 *   0x8006BA2C ScreenSaverEnd           (L) destroy 4 weapons, restore audio/state   [high]
 *   0x8006BAF8 ScreenSaverStart         (L) create 4 weapons at random positions     [high]
 *   0x8006BCA0 ScreenSaverUpdateWeap    (L) per-weapon physics update (called 4x)     [high]
 *   0x8006C1D4 ScreenSaverStartWeap     (L) set up one weapon (called 4x from Start)  [high]
 *   0x8006C2C8 draw_fullscreen_inventory(G) interactive 4-player inventory loop       [high]
 *   0x8006C644 draw_inventory_panel     (G) one player's panel ("window_empty")       [high]
 *   0x8006C3A0 fn_8006C3A0                  inventory sub (draws via draw_inventory_panel)
 *   0x8006C4D0 fn_8006C4D0                  inventory blit teardown
 *   0x8006C5DC fn_8006C5DC                  small inventory helper
 *   0x8006C60C fn_8006C60C                  small inventory helper
 *   0x8006CCB8 fn_8006CCB8                  per-slot panel piece draw (~animate_panel_piece)
 *   0x8006D0A4 fn_8006D0A4                  panel text/blit helper
 *   0x8006D18C fn_8006D18C                  per-slot numeric stat draw (~print_n_of_m)
 *   0x8006D29C fn_8006D29C                  blit-entry setup
 *   0x8006D458 fn_8006D458                  blit teardown
 *   0x8006D4E8 fn_8006D4E8                  green-circle screen-transition build
 *   0x8006D7BC fn_8006D7BC                  transition-active getter (lbl_80343C94)
 *   0x8006D7D8 fn_8006D7D8                  transition clear
 *   0x8006D7E4 fn_8006D7E4                  returns 0
 *   0x8006D7EC fn_8006D7EC                  4-player interactive dialog (~ControllerMessageBox)
 */

#include "types.h"

/* ---- text / message-box library (other TUs) ---- */
int FixMLineText(char* src, char* dst, void* lines);
int DrawNormalText(f32 scale, char* text, int font);
int FontHeight(f32 scale, int font);
int TextMLines(char* text);
void DrawTextSub(f32 scale, f32 x, u32 color, int y, int font, u32 flags, char* text);
void msgUpdate(void* frame);
void sndTestStopAll(void);

/* ---- MB blit library (other TUs) ---- */
void* MBNewBlit(void* tex, int a, int b);
void* MBNewTempQuad(void);
void* MBNewTempBlit(void* tex, int x, int y, int w, int h);
void MBEndFrame(void);
void* MBRemoveBlit(void* blit);   /* returns NULL (clears the handle) */
void mbBlitProject(void* blit, int w, int h);
void mbBlitCalcWidth(f32 z, void* blit, int x, int y);
void mbInitBlitEntry(void* blit, u32 pos, int a);   /* 0x800B2988 */

/* ---- misc engine helpers (other TUs) ---- */
void ClearAllPlayerControls(int a);   /* 0x80032A80 */

/* ---- shared front-end state (small data / bss, other TUs) ---- */
extern int gWinGlobals;         /* lbl_80344FC0 */
extern u8 lbl_80344A5C;
extern u8 lbl_80344A5D;
extern int lbl_80344568;
extern int lbl_80344A30;        /* modal-render depth */
extern int lbl_80344E04;
extern int lbl_80344A48;        /* screensaver idle timer */
extern int lbl_8034477C;        /* game-mode flag */

/* ---- screen-transition blit handles + wipe state (green-circle wipe) ---- */
extern void* lbl_80344A34;
extern void* lbl_80343C8C;
extern void* lbl_80343C90;
extern void* lbl_80343C94;      /* transition-active handle (0 = inactive) */
extern void* lbl_80343C98;
extern int lbl_80344A38;        /* wipe X anchor */
extern int lbl_80344A3C;        /* wipe Y anchor */
extern int lbl_80344A40;        /* wipe progress accumulator */
extern int lbl_80344A44;        /* inventory-panels-built flag */
extern int lbl_80344578;        /* frame-time delta */
extern u32 sFlags;              /* lbl_803445CC global mode flags */
extern u32 lbl_80240FB0;        /* pad state A */
extern u32 lbl_80240FC0;        /* pad state B */

/* ---- screensaver-weapon parallel state arrays (this TU's .bss) ---- */
extern int lbl_80274600[4];     /* per-weapon slot A (state code) */
extern int lbl_80274610[4];     /* per-weapon slot B */

/* ============================================================
 * NonMatching stubs. The final DOL links the original bytes for
 * this range via the splits.txt claim; these bodies exist only so
 * the unit compiles and objdiff has something to diff against.
 * ============================================================ */

void ScrollMessageBox(char* msg)
{
    (void)msg;
    sndTestStopAll();
    lbl_80344A30 += 1;
    MBEndFrame();
    lbl_80344A30 -= 1;
}

void ScreenSaverStartWeap(int idx)
{
    (void)idx;
}

void ScreenSaverStart(void)
{
    int i;
    for (i = 0; i < 4; i++) {
        ScreenSaverStartWeap(i);
    }
}

void ScreenSaverUpdateWeap(void)
{
}

void ScreenSaverEnd(void)
{
}

void ScreenSaver(void)
{
    int i;

    if ((lbl_8034477C & 0x8000) != 0) {
        return;
    }
    lbl_80344A48 += 1;
    if (lbl_80344A48 < 36000) {
        return;
    }
    ScreenSaverStart();
    for (i = 0; i < 4; i++) {
        ScreenSaverUpdateWeap();
    }
    ScreenSaverEnd();
    lbl_80344A48 = 0;
}

int draw_inventory_panel(int player)
{
    (void)player;
    return 1;
}

void draw_fullscreen_inventory(void)
{
    int done;
    int p;

    do {
        done = 1;
        for (p = 0; p < 4; p++) {
            if (draw_inventory_panel(p) == 0) {
                done = 0;
            }
        }
        MBEndFrame();
    } while (!done);
}

/* ---- small inventory-slot helpers ---- */

void fn_8006C5DC(int idx)
{
    lbl_80274600[idx] = 2;
    lbl_80274610[idx] = 0;
}

/*
 * The per-player inventory-panel blit tables all live in one .bss object
 * anchored at lbl_80274600 (the compiler pools them off that base as a
 * single held pointer + constant displacements):
 *   +592 (0x250)  arrE[4][9]   handles   (stride 36)
 *   +736 (0x2E0)  arrF[4][12]  handles   (stride 48)
 *   +928 (0x3A0)  arrG[4][4]   handles   (stride 16)
 *   +992 (0x3E0)  arrH[4][12]  handles   (stride 48)
 * The two int arrays lbl_80274600[4]/lbl_80274610[4] sit at +0/+16.
 */
void fn_8006C60C(int idx)
{
    lbl_80274600[idx] = 0;
    *(int*)((u8*)lbl_80274600 + idx * 4 + 16) = 0;
    *(void**)((u8*)lbl_80274600 + idx * 16 + 928) = 0;
    *(void**)((u8*)lbl_80274600 + idx * 48 + 992) = 0;
}

/* Free (MBRemoveBlit) and null every blit handle in one player's panel. */
void fn_8006C4D0(int player)
{
    u8* base = (u8*)lbl_80274600;
    void** p;
    int i;

    p = (void**)(base + player * 16 + 928);
    for (i = 0; i < 4; i++) { if (*p) { MBRemoveBlit(*p); } *p = 0; p++; }

    p = (void**)(base + player * 48 + 992);
    for (i = 0; i < 12; i++) { if (*p) { MBRemoveBlit(*p); } *p = 0; p++; }

    p = (void**)(base + player * 48 + 736);
    for (i = 0; i < 12; i++) { if (*p) { MBRemoveBlit(*p); } *p = 0; p++; }

    p = (void**)(base + player * 36 + 592);
    for (i = 0; i < 9; i++) { if (*p) { MBRemoveBlit(*p); } *p = 0; p++; }
}

/*
 * ============================================================
 * Structural skeletons (giants). Real names + verified control
 * shape / global touches from Ghidra; full float-ABI bodies not
 * yet reconstructed. NonMatching -- the DOL links original bytes.
 * ============================================================
 */

/*
 * Build the four players' inventory panels once (guarded by lbl_80344A44),
 * then (re)draw them every call. The one-time build labels each player/slot
 * ("s1 plyr" ...) and creates the text/icon blits via fn_8006D0A4, caching
 * handles into the arrG/arrH tables. Suppressed while a wipe is active.
 */
void fn_8006C3A0(void)
{
    int p;

    if (lbl_80343C94 == 0) {
        if (lbl_80344A44 == 0) {
            /* one-time panel-handle construction (float-ABI body elided) */
        }
        for (p = 0; p < 4; p++) {
            draw_inventory_panel(p);
        }
        lbl_80344A44 = 1;
    }
}

/* Per-slot panel piece draw (~animate_panel_piece). Giant; skeleton. */
void fn_8006CCB8(void)
{
}

/*
 * Panel text/blit helper: lays out one label string into a temp blit,
 * positions it, and returns the blit handle (or NULL). Skeleton.
 */
void* fn_8006D0A4(void)
{
    return 0;
}

/* Per-slot numeric stat draw (~print_n_of_m: "n / m"). Skeleton. */
void fn_8006D18C(void)
{
}

/*
 * Build the green-circle screen-transition blits (allocates lbl_80343C94 /
 * lbl_80343C8C ... and seeds the wipe anchors). Giant; skeleton.
 */
void fn_8006D4E8(void)
{
}

/* Tear down all screen-transition blits (green-circle wipe cleanup). */
void fn_8006D458(void)
{
    if (lbl_80344A34) { lbl_80344A34 = MBRemoveBlit(lbl_80344A34); }
    if (lbl_80343C8C) { lbl_80343C8C = MBRemoveBlit(lbl_80343C8C); }
    if (lbl_80343C90) { lbl_80343C90 = MBRemoveBlit(lbl_80343C90); }
    if (lbl_80343C94) { lbl_80343C94 = MBRemoveBlit(lbl_80343C94); }
    if (lbl_80343C98) { lbl_80343C98 = MBRemoveBlit(lbl_80343C98); }
}

/*
 * Advance the green-circle screen-wipe one frame. Returns 1 while the wipe
 * is still growing (progress < 0x15), 0 once it finishes (and tears the
 * blits down via fn_8006D458).
 */
int fn_8006D29C(void)
{
    int t;
    u32 pos;

    if (lbl_80343C94 == 0) {
        return 0;
    }

    if ((sFlags & 8) == 0) {
        lbl_80344A40 += lbl_80344578;
    } else if ((lbl_80240FB0 & 0x2000000) || (lbl_80240FC0 & 0x1000000)) {
        lbl_80344A40 += 2;
    }

    t = lbl_80344A40 >> 1;
    if (t < 0x15) {
        pos = lbl_80344A38 + t;
        mbInitBlitEntry(lbl_80343C94, pos, 0);
        if (lbl_80343C98) { mbInitBlitEntry(lbl_80343C98, pos, 0); }
        pos = lbl_80344A3C + t;
        mbInitBlitEntry(lbl_80343C8C, pos, 0);
        if (lbl_80343C90) { mbInitBlitEntry(lbl_80343C90, pos, 0); }
        if ((sFlags & 8) == 0) {
            ClearAllPlayerControls(2);
        }
        return 1;
    }

    fn_8006D458();
    return 0;
}

/* ---- transition-flag accessors (lbl_80343C94) ---- */

int fn_8006D7BC(void)
{
    if (lbl_80343C94) {
        return 1;
    }
    return 0;
}

void fn_8006D7D8(void)
{
    lbl_80343C94 = 0;
}

int fn_8006D7E4(void)
{
    return 0;
}

/*
 * 4-player interactive dialog / message box (~ControllerMessageBox): runs a
 * modal loop drawing the prompt and polling all four pads until one accepts
 * or cancels, returning the chosen option. Giant; skeleton.
 */
int fn_8006D7EC(void)
{
    return 0;
}
