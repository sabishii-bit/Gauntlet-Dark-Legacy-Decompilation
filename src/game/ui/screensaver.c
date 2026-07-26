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
void MBRemoveBlit(void* blit);
void mbBlitProject(void* blit, int w, int h);
void mbBlitCalcWidth(f32 z, void* blit, int x, int y);

/* ---- shared front-end state (small data / bss, other TUs) ---- */
extern int gWinGlobals;         /* lbl_80344FC0 */
extern u8 lbl_80344A5C;
extern u8 lbl_80344A5D;
extern int lbl_80344568;
extern int lbl_80344A30;        /* modal-render depth */
extern int lbl_80344E04;
extern int lbl_80344A48;        /* screensaver idle timer */
extern int lbl_8034477C;        /* game-mode flag */
extern int lbl_80343C94;        /* transition-active flag */

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
