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
 *   0x8006C3A0 draw_panels                  inventory sub (draws via draw_inventory_panel)
 *   0x8006C4D0 end_inventory_panel                  inventory blit teardown
 *   0x8006C5DC init_inventory_panel                  small inventory helper
 *   0x8006C60C init_panel_blits                  small inventory helper
 *   0x8006CCB8 animate_panel_piece                  per-slot panel piece draw (~animate_panel_piece)
 *   0x8006D0A4 disp_piece                  panel text/blit helper
 *   0x8006D18C print_n_of_m                  per-slot numeric stat draw (~print_n_of_m)
 *   0x8006D29C ServeFireScroll                  blit-entry setup
 *   0x8006D458 EndFireScroll                  blit teardown
 *   0x8006D4E8 StartFireScroll                  green-circle screen-transition build
 *   0x8006D7BC FireScrollActive                  transition-active getter (lbl_80343C94)
 *   0x8006D7D8 FireScrollReset                  transition clear
 *   0x8006D7E4 ticks_for_firescroll                  returns 0
 *   0x8006D7EC ControllerMessageBox                  4-player interactive dialog (~ControllerMessageBox)
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
void AtreeDelete(void* atree);        /* 0x800115D0 */
int MBRemoveNode(int handle, int flag); /* 0x800BAEAC */
void MBTreeClearFlags(int node, int mask, int val); /* 0x800BA2C4 */
void ShopMusicStart();                /* 0x800A0DA8 */
void AudioSelect();                   /* 0x800A0F64 */

/* ---- screensaver-weapon struct array (this TU's .bss, stride 0x88) ---- */
extern u8 lbl_80274620[];             /* node @+0x3c, atree @+0x40 */
extern int lbl_80344A64;              /* backdrop node handle */
extern int lbl_80344ECC;              /* active-node list head (next @+0x7c) */
extern int lbl_80344A60;              /* saved options state */
extern int options_state;             /* 0x80344A98 */

/* ---- shared front-end state (small data / bss, other TUs) ---- */
extern int gWinGlobals;         /* gWinGlobals */
extern u8 gDiskErrorShown;
extern u8 lbl_80344A5D;
extern int gGameBusy;
extern int gModalRenderDepth;
extern int lbl_80344E04;
extern int lbl_80344A48;        /* screensaver idle timer */
extern int gGameMode;        /* game-mode flag */

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
extern int gClockStepTicks;     /* frame-time delta */
extern u32 sFlags;              /* sFlags global mode flags */
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
    gModalRenderDepth += 1;
    MBEndFrame();
    gModalRenderDepth -= 1;
}

void* MBNewNode();                     /* 0x800BB29C */
void* AtreeMatch();                    /* atree lookup by name */
void* AtreeInit();                     /* 0x80012F78 */
void MBNodeSetParent();                /* 0x800BAD94 */
extern u8 lbl_8011D568[];              /* weapon init tables (names/pos/vel) */
extern void* sPowerupsBuf;             /* atree wad */
extern f32 lbl_80347398;               /* initial weapon spin */

/* Set up one screensaver weapon: scene node + weapon object parented to it,
 * then seed its position/velocity from the per-weapon init table. */
void ScreenSaverStartWeap(int idx)
{
    u8* w = (u8*)lbl_80274600 + idx * 0x88;
    u8* t = lbl_8011D568 + idx * 0xc;
    void* atree;

    *(void**)(w + 0x5c) = MBNewNode(lbl_80344A64, 0, 0);
    atree = AtreeMatch(sPowerupsBuf, *(char**)(lbl_8011D568 + 0x744 + idx * 4), 0);
    *(void**)(w + 0x60) = AtreeInit(atree, w + 0x60, 0, 0);
    if (*(s32**)(w + 0x60) != 0 && **(s32**)(w + 0x60) != 0) {
        MBNodeSetParent(**(s32**)(w + 0x60), *(void**)(w + 0x5c));
    }
    *(u32*)(w + 0x20) = *(u32*)(t + 0x754);
    *(u32*)(w + 0x24) = *(u32*)(t + 0x758);
    *(u32*)(w + 0x28) = *(u32*)(t + 0x75c);
    *(u32*)(w + 0x30) = *(u32*)(t + 0x784);
    *(u32*)(w + 0x34) = *(u32*)(t + 0x788);
    *(u32*)(w + 0x38) = *(u32*)(t + 0x78c);
    *(f32*)(w + 0x2c) = lbl_80347398;
    *(f32*)(w + 0x3c) = lbl_80347398;
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
    u8* base;
    s32 off;
    u8* node;
    s32 i;

    base = lbl_80274620;
    for (i = 0; i < 4; i++) {
        off = i * 0x88;
        AtreeDelete(base + 0x40 + off);
        *(s32*)(base + 0x3c + off) =
            MBRemoveNode(*(s32*)(base + 0x3c + off), 1);
    }
    MBRemoveNode(lbl_80344A64, 1);
    for (node = (u8*)lbl_80344ECC; node != 0; node = *(u8**)(node + 0x7c)) {
        MBTreeClearFlags((s32)node, 2, 0);
    }
    ClearAllPlayerControls(-2);
    options_state = lbl_80344A60;
    switch (gGameMode) {
    case 0x4012:
        ShopMusicStart();
        break;
    case 0x400B:
        AudioSelect(1);
        break;
    }
}

void ScreenSaver(void)
{
    int i;

    if ((gGameMode & 0x8000) != 0) {
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

void init_inventory_panel(int idx)
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
typedef struct PanelBlitOverlay {
    int state[4];
    int state2[4];
    u8 _pad20[0x3A0 - 0x20];
    int group4[4][4];
    int group12[4][12];
} PanelBlitOverlay;

void init_panel_blits(int idx)
{
    PanelBlitOverlay* panels = (PanelBlitOverlay*)lbl_80274600;

    panels->state[idx] = 0;
    panels->state2[idx] = 0;
    panels->group4[idx][0] = 0;
    panels->group12[idx][0] = 0;
}

/* Free (MBRemoveBlit) and null every blit handle in one player's panel. */
void end_inventory_panel(int player)
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
 * ("s1 plyr" ...) and creates the text/icon blits via disp_piece, caching
 * handles into the arrG/arrH tables. Suppressed while a wipe is active.
 */
void draw_panels(void)
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
void animate_panel_piece(void)
{
}

/*
 * Panel text/blit helper: lays out one label string into a temp blit,
 * positions it, and returns the blit handle (or NULL). Skeleton.
 */
void strcpy();                         /* 0x800E80D4 */
int MBOX_FindTexture_Err();            /* 0x800B8B34 */
int MBRomTexPtr(int texid);            /* 0x800BA024 */
void sprintf();                        /* 0x800C9BD0 */
extern char lbl_803473D8[];            /* piece-name format */
extern f32 lbl_803473E0;               /* blit scale */

/* Build one inventory piece's icon blit: format its name, find the texture,
 * create + position the blit, cache its w/h.  Returns the blit (or NULL). */
void* disp_piece(u32* piece, s32 xoff, u32 mode)
{
    char buf[36];
    char* name = (char*)piece[0];
    void* blit;

    if (name == 0) {
        blit = 0;
    } else {
        int tex;
        int info;
        if (mode != 0) {
            sprintf(buf, lbl_803473D8, name, mode);
        } else {
            strcpy(buf, name);
        }
        tex = MBOX_FindTexture_Err(buf, 0, 1);
        blit = MBNewBlit(buf, 0, 0);
        info = MBRomTexPtr(tex);
        mbBlitCalcWidth(lbl_803473E0, blit, piece[1] + xoff, piece[2]);
        if (info != 0) {
            mbBlitProject(blit, *(u16*)(info + 10), *(u16*)(info + 0xc));
            piece[7] = *(u16*)(info + 10);
            piece[8] = *(u16*)(info + 0xc);
        }
    }
    return blit;
}

int DrawTextKeepScale();               /* 0x800209BC */
void MBFontMsgSetAlpha(int handle, u32 node); /* 0x800B5AA8 */
void sprintf();                        /* 0x800C9BD0 */
extern u8 lbl_8011D568[];              /* n-of-m style config, stride 0x24 */
extern f64 lbl_80347418;               /* min scale to draw */
extern char lbl_80347420[], lbl_80347424[]; /* "%d" formats */
extern int lbl_80343CA0;               /* message font handle */

/* Per-slot numeric stat draw (print_n_of_m: "n / m", right-aligned). */
void print_n_of_m(s32 style, s32 n, s32 m, s32 x, u32 node)
{
    char buf[76];
    u8* cfg = lbl_8011D568 + style * 0x24;
    f32 scale = *(f32*)(cfg + 0x220);
    s32 a = *(s32*)(cfg + 0x214);
    s32 b = *(s32*)(cfg + 0x218);
    s32 d = *(s32*)(cfg + 0x224);
    s32 e = *(s32*)(cfg + 0x228);
    s32 xr;
    int h;

    if (n < 0 || m < n) {
        n = m;
    }
    if (lbl_80347418 < (f64)scale) {
        xr = x + a + d;
        sprintf(buf, lbl_80347420, n);
        h = DrawTextKeepScale(scale, xr - DrawNormalText(scale, buf, lbl_80343CA0),
                              b + e, lbl_80343CA0, 0xFFFFFF, buf);
        MBFontMsgSetAlpha(h, node);
        sprintf(buf, lbl_80347424, m);
        h = DrawTextKeepScale(scale, xr, b + e, lbl_80343CA0, 0xFFFFFF, buf);
        MBFontMsgSetAlpha(h, node);
    }
}

/*
 * Build the green-circle screen-transition blits (allocates lbl_80343C94 /
 * lbl_80343C8C ... and seeds the wipe anchors). Giant; skeleton.
 */
void StartFireScroll(void)
{
}

/* Tear down all screen-transition blits (green-circle wipe cleanup). */
void EndFireScroll(void)
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
 * blits down via EndFireScroll).
 */
int ServeFireScroll(void)
{
    int t;
    u32 pos;

    if (lbl_80343C94 == 0) {
        return 0;
    }

    if ((sFlags & 8) == 0) {
        lbl_80344A40 += gClockStepTicks;
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

    EndFireScroll();
    return 0;
}

/* ---- transition-flag accessors (lbl_80343C94) ---- */

int FireScrollActive(void)
{
    if (lbl_80343C94) {
        return 1;
    }
    return 0;
}

void FireScrollReset(void)
{
    lbl_80343C94 = 0;
}

int ticks_for_firescroll(void)
{
    return 0;
}

/*
 * 4-player interactive dialog / message box (~ControllerMessageBox): runs a
 * modal loop drawing the prompt and polling all four pads until one accepts
 * or cancels, returning the chosen option. Giant; skeleton.
 */
int ControllerMessageBox(void)
{
    return 0;
}
