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
 *   0x8006D7BC FireScrollActive                  transition-active getter
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
void msgUpdate();
void sndTestStopAll(void);

/* ---- MB blit library (other TUs) ---- */
void* MBNewBlit(void* tex, int a, int b);
void* MBNewTempQuad(void);
void* MBNewTempBlit(void* tex, int x, int y, int w, int h);
void MBEndFrame(void);
void* MBRemoveBlit(void* blit);   /* returns NULL (clears the handle) */
void mbBlitProject(void* blit, int w, int h);
void mbBlitCalcWidth(void* blit, int x, int y, f64 depth);
void MBBlitSetAlpha(void* blit, u32 alpha);
void mbInitBlitEntry(void* blit, u32 pos, int a);   /* 0x800B2988 */
void* MBCreateBlit(void* node, s32 tex, s32 x, s32 y, s32 w, s32 h);
void mbBlitCvtCoord(void* blit, f32 depth);
void mbBlitUpdateEntry(void* blit, u32 keepMask, u32 setBits);
s32 MBOX_FindTexture(const char* name, s32* out);

/* ---- misc engine helpers (other TUs) ---- */
void ClearAllPlayerControls(int a);   /* 0x80032A80 */
void AtreeDelete(void* atree);        /* 0x800115D0 */
int MBRemoveNode(int handle, int flag); /* 0x800BAEAC */
void MBTreeClearFlags(int node, int mask, int val); /* 0x800BA2C4 */
void MBTreeSetFlags(int node, int mask, int val); /* 0x800BA368 */
void MBNodeOrder(int node, int sibling); /* 0x800BACF8 */
void ShopMusicStart();                /* 0x800A0DA8 */
void AudioSelect();                   /* 0x800A0F64 */
void AudioSelectReset(void);          /* 0x800A17D4 */
void AudioStreamStop(void);           /* 0x800176D8 */
f32 NormalVector(f32* vector);
f32 Random(f32 range);
void CreateDirMatrix(f32* matrix, f32* direction, f32* up);
void PitchMat3(f32* matrix, f32 angle);
void MulMat3(f32* lhs, f32* rhs, f32* out);
void MulVec4Mat4(const f32* vector, f32* out, const f32* matrix);
s32 MBWorldSphereClip(f32* sphere, f32 radius);
s32 AnimateATree(void* tree, s32 sequence, s32 last);
void serve_busy(s32 flags);
void ClockOncePerFrame(void);
s32 sndFxQueUpdate(void);
void LoadVU1GameLogic(void);

/* ---- screensaver-weapon struct array (this TU's .bss, stride 0x88) ---- */
extern u8 lbl_80274620[];             /* node @+0x3c, atree @+0x40 */
extern int lbl_80344A64;              /* backdrop node handle */
extern int lbl_80344ECC;              /* active-node list head (next @+0x7c) */
extern int lbl_80344EDC;              /* active front-end node excluded from hide */
extern int lbl_80344A60;              /* saved options state */
extern int options_state;             /* 0x80344A98 */
extern void* lbl_80344EE8;

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
extern void* gFireScrollImageBlit;
extern void* gFireScrollMaskBlits[2];
extern void* gFireScrollCircleBlits[2]; /* first handle is the active flag */
extern int gFireScrollCircleFrame;
extern int gFireScrollMaskFrame;
extern int gFireScrollTicks;
extern int lbl_80344A44;        /* inventory-panels-built flag */
extern u32 gClockStepTicks;     /* frame-time delta */
extern s64 gControllerButtons;
extern u32 sFlags;              /* sFlags global mode flags */
extern u32 lbl_80240FB0[4];     /* pad state A */
extern u32 lbl_80240FC0[4];     /* pad state B */
extern void* gDiag_DE8;
extern s32 gFireScrollVariant;
extern char* lbl_8011D748[];
extern f32 lbl_80347378;
extern volatile f64 lbl_80347428;
extern f64 lbl_80347430;
extern f64 lbl_80347438;
extern f64 lbl_80347440;
extern void fn_8009D37C(void);
extern f32 lbl_80343CAC;
extern f32 lbl_80343CB0;
extern f32 lbl_80343CB4;
extern f32 lbl_80343CB8;
extern f32 lbl_80343CBC;
extern f32 lbl_80343CC0;
extern f32 lbl_80343CC4;
extern f32 lbl_80347370;
extern f64 lbl_80347380;
extern f64 lbl_80347388;
extern f64 lbl_80347390;
extern f64 lbl_803473A0;
extern f32 lbl_803473A8;
extern f64 lbl_803473B0;
extern f64 lbl_803473B8;
extern f64 lbl_803473C0;
extern f32 lbl_803473C8;
extern f32 lbl_803473CC;
extern f32 lbl_803473E0;
extern f32 lbl_803473F4;
extern f32 lbl_80347408;
extern f32 lbl_8034740C;
extern f32 lbl_80347410;
extern s32 lbl_80343CA8;
extern f32 lbl_8011D658[60];
extern f32 lbl_8011DCBC[3];
extern u8 lbl_8011DA6C[];

/* ---- screensaver-weapon parallel state arrays (this TU's .bss) ---- */
extern int lbl_80274600[4];     /* per-weapon slot A (state code) */
extern int lbl_80274610[4];     /* per-weapon slot B */

/* ============================================================
 * NonMatching stubs. The final DOL links the original bytes for
 * this range via the splits.txt claim; these bodies exist only so
 * the unit compiles and objdiff has something to diff against.
 * ============================================================ */

void sysResetService(void);
void vibrators_off(void);
void MBHideMarkedMessages(void);
void MBLockMessages(int depth);
void MBUnlockMessages(int depth);
void MBBlitSetColor(void* blit, u32 color);
int strcmp(const char* a, const char* b);
char* strcpy(char* dst, const char* src);
extern char lbl_802A5D1C[];       /* current model name */
extern char lbl_80347368[8];      /* "ATTRACT"-ish sdata name */
extern void* lbl_80343CC8;        /* message-box texture */
extern u8 lbl_802A4AA4[];         /* scroll-list context */
extern int lbl_80344A4C;          /* message font */
extern f32 lbl_80344A50;          /* message font scale */
extern f32 lbl_80347374;
extern f32 lbl_8034737C;
extern void* lbl_80344BF8;        /* full-screen fade texture */
extern char gTextWorkBuf[];
extern char gTextFormatBuf[];

void ScrollMessageBox(char* msg)
{
    char* lines[17];
    u8 pad44[48];
    u8* g;
    s32 busy;
    void* blit;
    u8* hdr;
    s32 nlines;
    s32 i;
    s32 wmax;
    s32 e04;
    s32 boxw;
    s32 boxh;
    s32 x;
    s32 y;
    s32 w;
    f32 sA;
    f32 sB;

    gDiskErrorShown = 1;
    g = (u8*)gWinGlobals;
    busy = gGameBusy;
    blit = 0;
    lbl_80344A5D = 1;
    e04 = lbl_80344E04;
    hdr = 0;
    sA = lbl_8034737C;
    sB = lbl_80347378;
    sysResetService();
    sndTestStopAll();
    vibrators_off();
    gGameBusy = 1;
    gModalRenderDepth++;
    msgUpdate();
    MBHideMarkedMessages();
    MBLockMessages(gModalRenderDepth - 1);
    lbl_80344E04 = 0;
    if ((u32)gWinGlobals != 0) {
        if (*(u8**)(g + 48) != 0) {
            hdr = *(u8**)(g + 48) + 4;
        }
    }
    {
        void* quad;

        if (hdr != 0 && *(s32*)(hdr + 12) == 0 &&
            strcmp(lbl_802A5D1C, lbl_80347368) == 0) {
            blit = MBNewBlit(lbl_80343CC8, 0, 0);
        } else {
            quad = MBNewTempQuad();
        }
        if (*(u32*)(lbl_802A4AA4 + 24) != 0) {
            lbl_80344A4C = 6;
            lbl_80344A50 = lbl_80347370;
        } else {
            lbl_80344A4C = 0;
            lbl_80344A50 = lbl_80347374;
        }
        nlines = FixMLineText(msg, gTextWorkBuf, lines);
        wmax = 0;
        for (i = 0; i < nlines; i++) {
            char* line = lines[i];
            s32 font = lbl_80344A4C;

            w = DrawNormalText(lbl_80344A50, line, font);
            if (w > wmax) {
                wmax = w;
            }
        }
        wmax = FontHeight((boxw = wmax + 96, lbl_80344A50), lbl_80344A4C);
        boxh = (wmax + 4) * TextMLines(msg) + 60;
        if (boxw < 256) {
            boxw = 256;
        } else if (boxw > 512) {
            boxw = 512;
        }
        x = 256 - boxw / 2;
        y = 160 - boxh / 2;
        if (blit != 0) {
            mbBlitProject(blit, boxw, boxh);
            mbBlitCalcWidth(blit, x, y, sA);
        } else {
            mbBlitProject(quad, boxw, boxh);
            mbBlitCalcWidth(quad, x, y, sA);
            MBBlitSetColor(quad, -1);
        }
    }
    y += 32;
    strcpy(gTextFormatBuf, msg);
    for (i = 0; i < nlines; i++) {
        s32 font = lbl_80344A4C;

        DrawTextSub(lbl_80344A50, sA, -256, y, font, 0x160C03,
                    lines[i]);
        y += wmax;
    }
    {
        void* fade = MBNewTempBlit(lbl_80344BF8, 0, 0, 640, 480);

        mbBlitProject(fade, 640, 480);
        mbBlitCalcWidth(fade, 0, 0, sB);
        MBBlitSetColor(fade, 0xFF000000);
    }
    MBEndFrame();
    if (blit != 0) {
        MBRemoveBlit(blit);
    }
    MBUnlockMessages(gModalRenderDepth - 1);
    gGameBusy = busy;
    gModalRenderDepth--;
    lbl_80344E04 = e04;
}

void* MBNewNode();                     /* 0x800BB29C */
void* AtreeMatch();                    /* atree lookup by name */
void* AtreeInit();                     /* 0x80012F78 */
void MBNodeSetParent();                /* 0x800BAD94 */
extern u8 lbl_8011D568[];              /* weapon init tables (names/pos/vel) */
extern void* sPowerupsBuf;             /* atree wad */
extern f32 lbl_80347398;               /* initial weapon spin */
int RandInt(int range);                 /* 0x800BCCA8 */

/* Set up one screensaver weapon: scene node + weapon object parented to it,
 * then seed its position/velocity from the per-weapon init table. */
#pragma opt_propagation off
#pragma opt_common_subs off
#pragma opt_lifetimes off
void ScreenSaverStartWeap(int idx)
{
    u8* initTable = lbl_8011D568;
    u8* weaponTable = (u8*)lbl_80274600;
    s32 offset;
    u8* nodeSlot;
    u8* position;
    void* atree;
    void* node;
    f32 spin;

    node = MBNewNode(lbl_80344A64, 0, 0);
    offset = idx * 0x88;
    nodeSlot = weaponTable + offset;
    nodeSlot += 0x5c;
    *(void**)nodeSlot = node;
    atree = initTable + idx * 4;
    atree = AtreeMatch(sPowerupsBuf, *(char**)((u8*)atree + 0x744), 0);
    position = weaponTable + offset;
    position += 0x20;
    atree = AtreeInit(atree, position + 0x40, 0, 0);
    {
        u8* atreeDest = weaponTable + offset;

        *(void**)(atreeDest + 0x60) = atree;
        if (*(void**)(atreeDest + 0x60) != NULL &&
            **(void***)(atreeDest + 0x60) != NULL) {
            MBNodeSetParent(**(void***)(atreeDest + 0x60),
                            *(void**)nodeSlot);
        }
    }
    atree = initTable + idx * 0x0c;
    *(f32*)(position + 0x00) = *(f32*)((u8*)atree + 0x754);
    {
        u8* copyDest = weaponTable + offset;

        *(f32*)(copyDest + 0x24) = *(f32*)((u8*)atree + 0x758);
        *(f32*)(copyDest + 0x28) = *(f32*)((u8*)atree + 0x75c);
        *(f32*)(copyDest + 0x30) = *(f32*)((u8*)atree + 0x784);
        *(f32*)(copyDest + 0x34) = *(f32*)((u8*)atree + 0x788);
        *(f32*)(copyDest + 0x38) = *(f32*)((u8*)atree + 0x78c);
        spin = lbl_80347398;
        *(f32*)(copyDest + 0x2c) = spin;
        *(f32*)(copyDest + 0x3c) = spin;
    }
}
#pragma opt_common_subs reset
#pragma opt_lifetimes reset

void ScreenSaverStart(void)
{
    u8* weaponPositions;
    u8* initialPositions;
    u8* node;
    s32 i;
    s32 randomDelay;
    f64 timeScale;
    f64 randomBase;
    u8 unused[16];

    AudioSelectReset();
    AudioStreamStop();
    lbl_80344A60 = options_state;
    options_state = 100;

    for (node = (u8*)lbl_80344ECC; node != NULL;
         node = *(u8**)(node + 0x7c)) {
        if (*(s8*)(node + 0x52) != 7 && *(s8*)(node + 0x52) != 9 &&
            node != (u8*)lbl_80344EDC) {
            MBTreeSetFlags((s32)node, 2, 0);
        }
    }

    lbl_80344A64 = (s32)MBNewNode(0, 0, 0);
    MBNodeOrder(lbl_80344ECC, lbl_80344A64);

    initialPositions = (u8*)lbl_8011DCBC;
    weaponPositions = lbl_80274620;
    randomBase = lbl_80347388;
    timeScale = lbl_80347380;
    for (i = 0; i < 4; i++) {
        ScreenSaverStartWeap(i);
        *(f32*)(weaponPositions + i * 0x88) =
            *(f32*)(initialPositions + i * 0x0c);
        *(f32*)(weaponPositions + i * 0x88 + 4) =
            *(f32*)(initialPositions + i * 0x0c + 4);
        *(f32*)(weaponPositions + i * 0x88 + 8) =
            *(f32*)(initialPositions + i * 0x0c + 8);
        *(s32*)(weaponPositions + i * 0x88 + 0x28) = 0;
        *(s32*)(weaponPositions + i * 0x88 + 0x2c) =
            (s32)((f64)lbl_80343CC0 *
                  (randomBase + (f64)Random(lbl_80347378)) * timeScale) + 1;
        *(s32*)(weaponPositions + i * 0x88 + 0x30) =
            (s32)((f64)lbl_80343CC4 *
                  (randomBase + (f64)Random(lbl_80347378)) * timeScale);
        AtreeDelete(weaponPositions + i * 0x88 + 0x40);
        *(s32*)(weaponPositions + i * 0x88 + 0x3c) = MBRemoveNode(
            *(s32*)(weaponPositions + i * 0x88 + 0x3c), 1);
        randomDelay = i * 30 + RandInt(15);
        *(s32*)(weaponPositions + i * 0x88 + 0x2c) = randomDelay + 1;
    }
}

typedef struct ScreenSaverWeapon {
    u8 _pad00[0x20];
    f32 position[3];
    f32 _pad2C;
    f32 velocity[3];
    f32 _pad3C;
    f32 angle;
    s32 collisionState;
    s32 elapsed;
    s32 duration;
    s32 resetAt;
    f32 jitterX;
    f32 jitterY;
    void* node;
    u8 atree[0x28];
} ScreenSaverWeapon;

void ScreenSaverUpdateWeap(s32 idx)
{
    register s32 weaponIndex = idx;
    ScreenSaverWeapon* weapons = (ScreenSaverWeapon*)lbl_80274600;
    ScreenSaverWeapon* weapon = &weapons[weaponIndex];
    u8* initialTable = lbl_8011D568;
    f32 matrix[12];
    volatile f32 unused[5];
    f32 screenPosition[3];
    f32 frameStep;
    f32 movementStep;
    s32 collision;

    frameStep = (f32)gClockStepTicks / lbl_80347390;
    movementStep = frameStep * lbl_80343CB8;
    weapon->elapsed += gClockStepTicks;
    if (weapon->elapsed < weapon->duration) {
        return;
    }

    if (weapon->duration > 0) {
        u8* table = initialTable + weaponIndex * 0xC;
        void** node = &weapon->node;

        if (*node == NULL) {
            ScreenSaverStartWeap(weaponIndex);
        }
        weapon->position[0] = *(f32*)(table + 0x754);
        weapon->position[1] = *(f32*)(table + 0x758);
        weapon->position[2] = *(f32*)(table + 0x75C);
        weapon->velocity[0] = *(f32*)(table + 0x784);
        weapon->velocity[1] = *(f32*)(table + 0x788);
        weapon->velocity[2] = *(f32*)(table + 0x78C);
        NormalVector(weapon->velocity);
        weapon->angle = lbl_80347398;
        weapon->collisionState = 0;
        weapon->jitterX = (f32)((f64)lbl_80343CB8 *
                              (lbl_803473A0 + (f64)Random(lbl_803473A8)));
        weapon->jitterY = (f32)((f64)lbl_80343CB4 *
                              (lbl_803473A0 + (f64)Random(lbl_803473A8)));
        weapon->duration = 0;
        MBTreeClearFlags((s32)*node, 2, 0);
    }

    {
    f32* position = weapon->position;
    f32* velocity = weapon->velocity;
    f32* positionY = &weapon->position[1];
    f32* velocityY = &weapon->velocity[1];
    f32* positionZ = &weapon->position[2];
    f32* velocityZ = &weapon->velocity[2];
    f32* angleValue = &weapon->angle;
    s32* collisionState;
    void** node;

    position[0] += movementStep * velocity[0];
    *positionY += movementStep * *velocityY;
    *positionZ += movementStep * *velocityZ;
    *angleValue += -lbl_80343CB4 * frameStep;
    {
        f64 angle = (f64)*angleValue;

        if (angle > lbl_803473B0) {
            angle -= lbl_803473B8;
        } else if (angle <= lbl_803473C0) {
            angle = lbl_803473B8 + angle;
        }
        *angleValue = (f32)angle;
    }

    CreateDirMatrix(matrix, velocity, NULL);
    PitchMat3(matrix, *angleValue);
    node = &weapon->node;
    MulMat3(matrix, (f32*)lbl_80344EE8 + 25, (f32*)*node);
    MulVec4Mat4(position, screenPosition,
                (f32*)lbl_80344EE8 + 25);

    if (*positionZ > lbl_80343CAC) {
        collision = 5;
    } else if (*positionZ < lbl_80343CB0) {
        collision = 6;
    } else {
        collision = MBWorldSphereClip(screenPosition, lbl_803473C8);
    }

    collisionState = &weapon->collisionState;
    if (*collisionState > 0) {
        f32 spread;

        switch (collision) {
        case 1:
            velocity[0] =
                (f32)(lbl_80347388 + (f64)Random(lbl_80347370));
            spread = lbl_80343CBC * Random(lbl_803473CC) -
                     lbl_80343CBC;
            *velocityY += spread;
            break;
        case 2:
            velocity[0] =
                -(f32)(lbl_80347388 + (f64)Random(lbl_80347370));
            spread = lbl_80343CBC * Random(lbl_803473CC) -
                     lbl_80343CBC;
            *velocityY += spread;
            break;
        case 3:
            *velocityY =
                -(f32)(lbl_80347388 + (f64)Random(lbl_80347370));
            spread = lbl_80343CBC * Random(lbl_803473CC) -
                     lbl_80343CBC;
            velocity[0] += spread;
            break;
        case 4:
            *velocityY =
                (f32)(lbl_80347388 + (f64)Random(lbl_80347370));
            spread = lbl_80343CBC * Random(lbl_803473CC) -
                     lbl_80343CBC;
            velocity[0] += spread;
            break;
        case 5:
            *velocityZ =
                -(f32)(lbl_80347388 + (f64)Random(lbl_80347370));
            if (weapon->elapsed > weapon->resetAt) {
                u8* table = initialTable + weaponIndex * 0xC;
                s32 delay;

                position[0] = *(f32*)(table + 0x754);
                *positionY = *(f32*)(table + 0x758);
                *positionZ = *(f32*)(table + 0x75C);
                weapon->elapsed = 0;
                delay = (s32)(lbl_80347380 *
                              ((f64)lbl_80343CC0 *
                               (lbl_80347388 +
                                (f64)Random(lbl_80347378))));
                weapon->duration = delay + 1;
                weapon->resetAt =
                    (s32)(lbl_80347380 *
                          ((f64)lbl_80343CC4 *
                           (lbl_80347388 +
                            (f64)Random(lbl_80347378))));
                AtreeDelete(weapon->atree);
                *node = (void*)MBRemoveNode((s32)*node, 1);
                return;
            }
            break;
        case 6:
            *velocityZ =
                (f32)(lbl_80347388 + (f64)Random(lbl_80347370));
            break;
        }
        if (collision != 0) {
            NormalVector(velocity);
            *collisionState = -10;
        }
    } else if (*collisionState < 0) {
        *collisionState = gClockStepTicks;
    } else if (collision == 0) {
        *collisionState = 1;
    }

    *(f32*)((u8*)*node + 0x30) = screenPosition[0];
    *(f32*)((u8*)*node + 0x34) = screenPosition[1];
    *(f32*)((u8*)*node + 0x38) = screenPosition[2];
    AnimateATree(weapon->atree, 0, 0);
    }
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

int fn_80055F68(int a, int b);
void DoTexMods(void);
void PlayerControls(void);
extern u8 lbl_80240E30[];

void ScreenSaver(void)
{
    u8* weap = (u8*)lbl_80274600;
    s32 exit = 0;
    s32 i;

    if ((gGameMode & 0x8000) != 0 || gGameMode == 0x400E || gGameMode == 0x4015 ||
        fn_80055F68(0, 0) == 0) {
        lbl_80344A48 = 0;
    } else {
        lbl_80344A48 += gClockStepTicks;
        for (i = 0; i < 4; i++) {
            u8* wr = weap + i * 4;
            u8* pr = lbl_80240E30 + i * 60;
            if (*(void**)(wr + 576) != *(void**)(pr + 4)) {
                lbl_80344A48 = 0;
            }
        }
        if ((gControllerButtons & 1) != 0) {
            lbl_80344A48 = 36000;
        }
        if ((u32)lbl_80344A48 < 36000) {
            for (i = 0; i < 4; i++) {
                u8* pr = lbl_80240E30 + i * 60;
                u8* wr = weap + i * 4;
                *(void**)(wr + 576) = *(void**)(pr + 4);
            }
        } else {
            ScreenSaverStart();
            while (exit == 0) {
                serve_busy(-1);
                ClockOncePerFrame();
                if (sPowerupsBuf != NULL) {
                    DoTexMods();
                }
                for (i = 0; i < 4; i++) {
                    ScreenSaverUpdateWeap(i);
                }
                PlayerControls();
                for (i = 0; i < 4; i++) {
                    u8* pr = lbl_80240E30 + i * 60;
                    u8* wr = weap + i * 4;
                    if (*(void**)(pr + 4) != *(void**)(wr + 576)) {
                        exit = 1;
                    }
                }
                MBEndFrame();
            }
            ScreenSaverEnd();
            lbl_80344A48 = 0;
        }
    }
}

#pragma dont_inline on
int MBOX_FindTexture_Err();
u32 MBRomTexPtr(int texid);
int sprintf(char* dst, const char* fmt, ...);
extern char lbl_803473EC[2];    /* rune-name suffix */
extern char lbl_803473D8;      /* piece-name format */
extern f32 lbl_803473E4;
extern f32 lbl_803473E8;
extern s32 lbl_80343CA4;
extern void* lbl_80344E48;
extern char lbl_801137B4[];
extern f32 lbl_803473F8;
extern char lbl_803473FC[6];    /* corner label */
extern s32 lbl_80124C70[][1];
int towerGetRuneNearStat(s32 player, s32 stat);
int PlayerHasRune(s32 player, s32 rune);
void print_n_of_m(s32 style, s32 n, s32 m, s32 x, u32 node);
void DrawGlowText(s32 color, s32 y, void* text, f32 scale);
int DrawTextKeepScale();
void animate_panel_piece(f32 progress, s32* piece, void* blit, s32 xOffset,
                         s32 phase);
extern u8 gPlayers[];

#pragma opt_lifetimes off
int draw_inventory_panel(int player)
{
    u8* base;
    u8* state;
    u8* pl;
    u8* cfg;
    u8* row;
    void** blits1;
    void** blits2;
    void** blits3;
    s32 xoff;
    s32 mode;
    s32 yshift;
    s32 result;
    s32 i;
    u32 j;
    s32 off;
    s32 boff;
    s32 poff48;
    f32 prog;
    void* b;
    char* name;
    char bufA[32];
    char bufB[32];
    char bufC[32];
    s32 t;
    u8* tex;

    base = (u8*)lbl_80274600;
    state = base + player * 4;
    pl = gPlayers + player * 13148;
    cfg = (u8*)lbl_8011D568;
    xoff = player << 7;
    result = 0;
    yshift = 0;
    mode = *(s32*)state;
    if (mode == 0) {
        prog = (f32)*(s32*)(state + 16) / lbl_803473E4;
    } else {
        prog = (f32)*(s32*)(state + 16) / lbl_803473E8;
    }
    if (*(s32*)(pl + 232) == 0) {
        return 1;
    }
    poff48 = player * 48;
    blits1 = (void**)(base + poff48 + 992);
    if (*blits1 == 0) {
        for (i = 0, boff = 0, off = 0; i < 12;
             i++, boff += 4, off += 36) {
            row = cfg + off;
            name = *(char**)(row = row + 528);
            if (name == NULL) {
                b = 0;
            } else {
                strcpy(bufA, name);
                t = MBOX_FindTexture_Err(bufA, 0, 1);
                b = MBNewBlit(bufA, 0, 0);
                tex = (u8*)MBRomTexPtr(t);
                mbBlitCalcWidth(b, *(s32*)(row + 4) + xoff, *(s32*)(row + 8),
                                lbl_803473E0);
                if (tex != NULL) {
                    mbBlitProject(b, *(u16*)(tex + 10), *(u16*)(tex + 12));
                    *(s32*)(row + 28) = *(u16*)(tex + 10);
                    *(s32*)(row + 32) = *(u16*)(tex + 12);
                }
            }
            ((void**)blits1)[i] = b;
        }
        blits2 = (void**)(base + poff48 + 736);
        for (j = 0, boff = 0, off = 0; j < 12;
             j++, boff += 4, off += 36) {
            s32 r = towerGetRuneNearStat(player, j);
            row = cfg + off;
            name = *(char**)(row = row + 1428);
            if (name == NULL) {
                b = 0;
            } else {
                if ((r ? NULL : lbl_803473EC) != NULL) {
                    sprintf(bufB, &lbl_803473D8, name,
                            r ? NULL : lbl_803473EC);
                } else {
                    strcpy(bufB, name);
                }
                t = MBOX_FindTexture_Err(bufB, 0, 1);
                b = MBNewBlit(bufB, 0, 0);
                tex = (u8*)MBRomTexPtr(t);
                mbBlitCalcWidth(b, *(s32*)(row + 4) + xoff, *(s32*)(row + 8),
                                lbl_803473E0);
                if (tex != NULL) {
                    mbBlitProject(b, *(u16*)(tex + 10), *(u16*)(tex + 12));
                    *(s32*)(row + 28) = *(u16*)(tex + 10);
                    *(s32*)(row + 32) = *(u16*)(tex + 12);
                }
            }
            ((void**)blits2)[j] = b;
        }
        blits3 = (void**)(base + player * 36 + 592);
        for (j = 0, boff = 0, off = 0; j < 9;
             j++, boff += 4, off += 36) {
            if (PlayerHasRune(player, j)) {
                row = cfg + off;
                name = *(char**)(row = row + 960);
                if (name == NULL) {
                    b = 0;
                } else {
                    strcpy(bufC, name);
                    t = MBOX_FindTexture_Err(bufC, 0, 1);
                    b = MBNewBlit(bufC, 0, 0);
                    tex = (u8*)MBRomTexPtr(t);
                    mbBlitCalcWidth(b, *(s32*)(row + 4) + xoff,
                                    *(s32*)(row + 8), lbl_803473E0);
                    if (tex != NULL) {
                        mbBlitProject(b, *(u16*)(tex + 10),
                                      *(u16*)(tex + 12));
                        *(s32*)(row + 28) = *(u16*)(tex + 10);
                        *(s32*)(row + 32) = *(u16*)(tex + 12);
                    }
                }
                ((void**)blits3)[j] = b;
            } else {
                ((void**)blits3)[j] = 0;
            }
        }
    }
    for (i = 0, boff = 0, off = 0; i < 12; i++, boff += 4, off += 36) {
        row = cfg + off;
        animate_panel_piece(prog, (s32*)(row + 528),
                            ((void**)blits1)[i], xoff, mode);
    }
    blits2 = (void**)(base + poff48 + 736);
    for (j = 0, boff = 0, off = 0; j < 12; j++, boff += 4, off += 36) {
        row = cfg + off;
        animate_panel_piece(prog, (s32*)(row + 1428),
                            ((void**)blits2)[j], xoff, mode);
    }
    blits3 = (void**)(base + player * 36 + 592);
    for (j = 0, boff = 0, off = 0; j < 9; j++, boff += 4, off += 36) {
        row = cfg + off;
        animate_panel_piece(prog, (s32*)(row + 960),
                            ((void**)blits3)[j], xoff, mode);
    }
    switch (mode) {
    case 0:
        yshift = (s32)(lbl_803473F4 * (lbl_80347378 - prog));
        break;
    case 1:
        yshift = 0;
        break;
    case 2:
        yshift = (s32)(lbl_803473F4 * prog);
        break;
    }
    row = pl + *(s32*)(pl + 12) * 240;
    print_n_of_m(1, *(s16*)(row + 3560), 12, xoff, yshift);
    row = pl + *(s32*)(pl + 12) * 240;
    print_n_of_m(2, *(s16*)(row + 3562), 20, xoff, yshift);
    row = pl + *(s32*)(pl + 12) * 240;
    print_n_of_m(3, *(s16*)(row + 3564), 28, xoff, yshift);
    for (i = 0, boff = 0, off = 0; i < 8; i++, boff += 4, off += 2) {
        print_n_of_m(i + 4,
                     *(s16*)(pl + *(s32*)(pl + 12) * 240 + off + 3568),
                     *(s32*)((u8*)lbl_80124C70 + boff + 4), xoff, yshift);
    }
    switch (*(s32*)state) {
    case 0:
        *(s32*)(base + player * 4 + 16) +=
            lbl_80343CA4 * (s32)gClockStepTicks;
        if (*(s32*)(base + player * 4 + 16) >= 120) {
            *(s32*)state = 1;
        }
        break;
    case 2:
        *(s32*)(base + player * 4 + 16) +=
            lbl_80343CA4 * (s32)gClockStepTicks;
        if (*(s32*)(base + player * 4 + 16) >= 15) {
            result = 1;
        }
        break;
    }
    if (*(s32*)state != 2) {
        MBNewTempBlit(lbl_80344E48, xoff + 16, 280, 16, 16);
        DrawGlowText(xoff + 40, 280, lbl_801137B4, lbl_80347370);
    }
    DrawTextKeepScale(lbl_803473F8, -(xoff + 64), 8, 6, 0, lbl_803473FC);
    return result;
}
#pragma opt_lifetimes reset

#pragma opt_propagation off
#pragma opt_lifetimes off
#pragma opt_common_subs off
void draw_fullscreen_inventory(void)
{
    int* states;
    register int* slot;
    int done;
    int p;
    int i;
    int two;
    int zero;
    u8 unused[8];

    states = lbl_80274600;
    p = 0;
    for (i = 0; i < 4; i++) {
        slot = &states[i];
        two = 2;
        states[i] = two;
        slot[4] = 0;
    }

    while (p == 0) {
        serve_busy(-1);
        ClockOncePerFrame();
        sndFxQueUpdate();
        done = 1;
        for (p = 0; p < 4; p++) {
            if (draw_inventory_panel(p) == 0) {
                done = 0;
            }
        }
        p = done;
        MBEndFrame();
    }

    zero = 0;
    for (i = 0; i < 4; i++) {
        slot = &states[i];
        states[i] = 2;
        slot[4] = zero;
    }
    lbl_80344A44 = 0;
    LoadVU1GameLogic();
}
#pragma opt_propagation reset
#pragma opt_lifetimes reset
#pragma opt_common_subs reset
#pragma dont_inline off

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

    p = (void**)(base + player * 16);
    p = (void**)((u8*)p + 928);
    for (i = 0; i < 4; i++) { if (*p) { MBRemoveBlit(*p); } *p = 0; p++; }

    p = (void**)(base + player * 48);
    p = (void**)((u8*)p + 992);
    for (i = 0; i < 12; i++) { if (*p) { MBRemoveBlit(*p); } *p = 0; p++; }

    p = (void**)(base + player * 48);
    p = (void**)((u8*)p + 736);
    for (i = 0; i < 12; i++) { if (*p) { MBRemoveBlit(*p); } *p = 0; p++; }

    p = (void**)(base + player * 36);
    p = (void**)((u8*)p + 592);
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
void* disp_piece(u32* piece, s32 xoff, u32 mode);
#pragma dont_inline on
#pragma opt_lifetimes off
void draw_panels(void)
{
    char label[8];
    u8 unused[4];
    register u8* base = (u8*)lbl_80274600;
    register u8* group4Base;
    register s32 player;
    register s32 xoff;
    register s32 group12off;
    register s32 group4off;
    register s32 stateoff;
    register s32 handleoff;
    register s32 pieceoff;
    register u8* pieces;
    register char playerChar;
    register s32 zero;
    register s32 slot;

    {
        register s32 busy = (gFireScrollCircleBlits[0] != 0) ? 1 : 0;
        if (busy != 0) {
            return;
        }
    }
    if (lbl_80344A44 == 0) {
        pieces = lbl_8011DA6C;
        player = 0;
        xoff = 0;
        group12off = 0;
        group4off = 0;
        stateoff = 0;
        zero = 0;
        do {
            *(volatile s32*)(base + stateoff) = zero;
            *(s32*)(base + stateoff + 16) = zero;
            group4Base = base + group4off;
            *(s32*)(group4Base + 928) = zero;
            *(s32*)(base + group12off + 992) = zero;
            if (*(s32*)(group4Base += 928) == 0) {
                slot = zero;
                pieceoff = zero;
                playerChar = player + '1';
                handleoff = 0;
                do {
                    if ((u32)(slot - 2) <= 1) {
                        label[0] = zero;
                    } else {
                        label[0] = playerChar;
                        label[1] = zero;
                    }
                    *(void**)(group4Base + handleoff) = (void*)
                        disp_piece((u32*)(pieces + pieceoff), xoff, (u32)label);
                    slot++;
                    handleoff += 4;
                    pieceoff += 36;
                } while (slot < 4);
            }
            player++;
            xoff += 128;
            group12off += 48;
            group4off += 16;
            stateoff += 4;
        } while (player < 4);
    }
    slot = 0;
    do {
        draw_inventory_panel(slot);
        slot++;
    } while (slot < 4);
    lbl_80344A44 = 1;
}
#pragma opt_lifetimes reset
#pragma dont_inline reset

/* Position and fade one inventory-panel piece during its enter/leave phase. */
void animate_panel_piece(f32 progress, s32* piece, void* blit, s32 xOffset,
                         s32 phase)
{
    f32 progress2;
    f32 factor;
    f32 dx;
    f32 dy;
    s32 tableIndex;
    s32 x;
    s32 y;
    u8 unused[8];

    if (blit == NULL) {
        return;
    }

    switch (phase) {
    case 0:
        progress2 = progress * progress;
        tableIndex = (piece[2] + piece[1] +
                      (s32)(lbl_80347408 *
                            (lbl_803473E0 * (progress2 * progress)))) %
                     60;
        factor = lbl_80347378 - progress;
        x = (s32)((f32)(piece[1] + xOffset) +
                  factor * ((f32)lbl_80343CA8 *
                            ((f32*)lbl_8011D568)[tableIndex]));
        y = (s32)((f32)piece[2] +
                  factor * ((f32)lbl_80343CA8 *
                            lbl_8011D658[tableIndex]));
        mbBlitCalcWidth(blit, x, y, lbl_803473E0);

        x = piece[7];
        y = piece[8];
        progress = lbl_80347378 - progress2;
        x = (s32)((f32)x * progress + (f32)x);
        y = (s32)((f32)y * progress + (f32)y);
        mbBlitProject(blit, x, y);
        MBBlitSetAlpha(blit, (s32)(lbl_803473F4 * progress));
        break;

    case 1:
        mbBlitCalcWidth(blit, piece[1] + xOffset, piece[2],
                        lbl_803473E0);
        mbBlitProject(blit, piece[7], piece[8]);
        break;

    case 2:
        x = piece[1];
        if (x > 64) {
            dx = (f32)(x - 64);
        } else {
            dx = (f32)-(64 - x);
        }
        y = piece[2];
        if (y > 180) {
            dy = (f32)(y - 180);
        } else {
            dy = (f32)-(180 - y);
        }
        dx *= progress;
        dy *= progress;
        x = (s32)((f32)(x + xOffset) + lbl_8034740C * dx);
        y = (s32)((f32)y + lbl_8034740C * dy);
        mbBlitCalcWidth(blit, x, y, lbl_803473E0);

        x = piece[7];
        y = piece[8];
        x = (s32)((f32)x + lbl_80347410 * ((f32)x * progress));
        y = (s32)((f32)y + lbl_80347410 * ((f32)y * progress));
        mbBlitProject(blit, x, y);
        MBBlitSetAlpha(blit, (s32)(lbl_803473F4 * progress));
        break;
    }
}
#pragma opt_propagation reset

/*
 * Panel text/blit helper: lays out one label string into a temp blit,
 * positions it, and returns the blit handle (or NULL). Skeleton.
 */
int MBOX_FindTexture_Err();            /* 0x800B8B34 */
u32 MBRomTexPtr(int texid);            /* 0x800BA024 */
int sprintf(char* dst, const char* fmt, ...); /* 0x800C9BD0 */
extern char lbl_803473D8;              /* piece-name format */

/* Build one inventory piece's icon blit: format its name, find the texture,
 * create + position the blit, cache its w/h.  Returns the blit (or NULL). */
void* disp_piece(u32* piece, s32 xoff, u32 mode)
{
    char buf[36];
    void* blit;
    int tex;
    u32 info;

    if (piece[0] == 0) {
        return NULL;
    }
    if (mode != 0) {
        sprintf(buf, &lbl_803473D8, (char*)piece[0], mode);
    } else {
        strcpy(buf, (char*)piece[0]);
    }
    tex = MBOX_FindTexture_Err(buf, 0, 1);
    blit = MBNewBlit(buf, 0, 0);
    info = MBRomTexPtr(tex);
    mbBlitCalcWidth(blit, piece[1] + xoff, piece[2], lbl_803473E0);
    if (info != 0) {
        mbBlitProject(blit, *(u16*)(info + 10), *(u16*)(info + 0xC));
        piece[7] = *(u16*)(info + 10);
        piece[8] = *(u16*)(info + 0xC);
    }
    return blit;
}

int DrawTextKeepScale();               /* 0x800209BC */
void MBFontMsgSetAlpha(int handle, u32 node); /* 0x800B5AA8 */
extern u8 lbl_8011D568[];              /* n-of-m style config, stride 0x24 */
extern f64 lbl_80347418;               /* min scale to draw */
extern char lbl_80347420[4], lbl_80347424[4]; /* "%d" formats */
extern int lbl_80343CA0;               /* message font handle */

/* Per-slot numeric stat draw (print_n_of_m: "n / m", right-aligned). */
void print_n_of_m(s32 style, s32 n, s32 m, s32 x, u32 node)
{
    char buf[76];
    u8* cfg;
    f32 scale;
    s32 xr;
    s32 yr;
    s32 d;
    s32 e;
    s32 w;
    s32 h;
    u8* base;

    base = lbl_8011D568;
    if (n < 0 || n > m) {
        n = m;
    }
    cfg = base + style * 0x24;
    scale = *(f32*)(cfg + 0x220);
    yr = *(s32*)(cfg + 0x218);
    d = *(s32*)(cfg + 0x224);
    e = *(s32*)(cfg + 0x228);
    xr = x + *(s32*)(cfg + 0x214);
    if ((f64)scale <= lbl_80347418) {
        return;
    }
    sprintf(buf, lbl_80347420, n);
    w = DrawNormalText(scale, buf, lbl_80343CA0);
    xr += d;
    yr += e;
    h = DrawTextKeepScale(scale, xr - w, yr, lbl_80343CA0, 0xFFFFFF, buf);
    MBFontMsgSetAlpha(h, node);
    sprintf(buf, lbl_80347424, m);
    h = DrawTextKeepScale(scale, xr, yr, lbl_80343CA0, 0xFFFFFF, buf);
    MBFontMsgSetAlpha(h, node);
}

/* Build the green-circle screen-transition blits and seed the wipe anchors. */
s32 StartFireScroll(char* name, s32 variant, s32 x, s32 y, s32 width,
                    s32 height, s32 split, f32 depth)
{
    s32 heightAdjust;
    s32 adjustedY;
    s32 adjustedHeight;
    s32 splitWidth;
    s32 tableOffset;
    s32 maskTexture;
    s32 circleTexture;
    void* parent;
    f64 depthOffset;
    f32 imageDepth;
    f32 circleDepth;
    f32 maskDepth;

    fn_8009D37C();
    if (gFireScrollMaskBlits[0] != NULL) {
        if (split == 1) {
            return (s32)gFireScrollMaskBlits[1];
        }
        return (s32)gFireScrollMaskBlits[0];
    }

    if (variant < 0) {
        if (++gFireScrollVariant >= 4) {
            gFireScrollVariant = 0;
        }
        variant = gFireScrollVariant;
    }

    if ((f64)depth < lbl_80347428) {
        depth = lbl_80347378;
    }
    depthOffset = (f64)depth - lbl_80347428;
    maskDepth = (f32)(lbl_80347430 + depthOffset);
    circleDepth = (f32)(lbl_80347438 + depthOffset);
    imageDepth = (f32)(lbl_80347440 + depthOffset);

    heightAdjust = 0;
    adjustedY = y;
    if (y < 0) {
        heightAdjust = y + 32;
        adjustedY = 0;
    }
    adjustedHeight = height + heightAdjust;
    if (split == 1) {
        splitWidth = width / 2;
    } else {
        splitWidth = width;
    }

    tableOffset = variant * 2;
    maskTexture = MBOX_FindTexture(lbl_8011D748[tableOffset + 1], NULL);
    gFireScrollMaskBlits[0] = MBCreateBlit(NULL, maskTexture + 1, x, adjustedY,
                                          splitWidth, adjustedHeight);
    mbBlitCvtCoord(gFireScrollMaskBlits[0], maskDepth);
    mbBlitUpdateEntry(gFireScrollMaskBlits[0], 0xFFFFFFFF, 0x10000);
    if (split == 1) {
        gFireScrollMaskBlits[1] =
            MBCreateBlit(NULL, maskTexture + 1, x + splitWidth, adjustedY,
                         splitWidth, adjustedHeight);
        mbBlitCvtCoord(gFireScrollMaskBlits[1], maskDepth);
        mbBlitUpdateEntry(gFireScrollMaskBlits[1], 0xFFFFFFFF, 0x10020);
    }
    gFireScrollMaskFrame = maskTexture + 1;
    parent = gDiag_DE8;

    if (name[0] != '\0') {
        s32 imageTexture = MBOX_FindTexture(name, NULL);

        gFireScrollImageBlit = MBCreateBlit(parent, imageTexture, x, y,
                                           width, height);
        mbBlitCvtCoord(gFireScrollImageBlit, imageDepth);
        mbBlitUpdateEntry(gFireScrollImageBlit, 0xFFFFFFFF, 0x10000);
    }

    circleTexture = MBOX_FindTexture(lbl_8011D748[tableOffset], NULL);
    gFireScrollCircleBlits[0] =
        MBCreateBlit(parent, circleTexture + 1, x, adjustedY, splitWidth,
                     adjustedHeight);
    mbBlitCvtCoord(gFireScrollCircleBlits[0], circleDepth);
    if (split == 1) {
        gFireScrollCircleBlits[1] =
            MBCreateBlit(parent, circleTexture + 1, x + splitWidth, adjustedY,
                         splitWidth, adjustedHeight);
        mbBlitCvtCoord(gFireScrollCircleBlits[1], circleDepth);
        mbBlitUpdateEntry(gFireScrollCircleBlits[1], 0xFFFFFFFF, 0x20);
    }
    gFireScrollCircleFrame = circleTexture + 1;
    gFireScrollTicks = 0;

    if (split == 1) {
        return (s32)gFireScrollMaskBlits[1];
    }
    return (s32)gFireScrollMaskBlits[0];
}

/* Tear down all screen-transition blits (green-circle wipe cleanup). */
void EndFireScroll(void)
{
    void** handle;

    if (gFireScrollImageBlit) { gFireScrollImageBlit = MBRemoveBlit(gFireScrollImageBlit); }
    if (gFireScrollMaskBlits[0]) { gFireScrollMaskBlits[0] = MBRemoveBlit(gFireScrollMaskBlits[0]); }
    handle = gFireScrollMaskBlits;
    if (*++handle) { *handle = MBRemoveBlit(*handle); }
    if (gFireScrollCircleBlits[0]) { gFireScrollCircleBlits[0] = MBRemoveBlit(gFireScrollCircleBlits[0]); }
    handle = gFireScrollCircleBlits;
    if (*++handle) { *handle = MBRemoveBlit(*handle); }
}

/*
 * Advance the green-circle screen-wipe one frame. Returns 1 while the wipe
 * is still growing (progress < 0x15), 0 once it finishes (and tears the
 * blits down via EndFireScroll).
 */
#pragma opt_propagation off
int ServeFireScroll(void)
{
    int t;
    u32 pos;
    u8 unused[8];

    if (gFireScrollCircleBlits[0] == 0) {
        return 0;
    }

    if ((gControllerButtons & 8) != 0) {
        if ((lbl_80240FB0[0] & 0x2000000) ||
            (lbl_80240FC0[0] & 0x1000000)) {
            gFireScrollTicks += 2;
        }
    } else {
        gFireScrollTicks += gClockStepTicks;
    }

    t = gFireScrollTicks >> 1;
    if (t >= 0x15) {
        EndFireScroll();
        return 0;
    }

    pos = gFireScrollCircleFrame + t;
    mbInitBlitEntry(gFireScrollCircleBlits[0], pos, 0);
    if (gFireScrollCircleBlits[1]) { mbInitBlitEntry(gFireScrollCircleBlits[1], pos, 0); }
    pos = gFireScrollMaskFrame + t;
    mbInitBlitEntry(gFireScrollMaskBlits[0], pos, 0);
    if (gFireScrollMaskBlits[1]) { mbInitBlitEntry(gFireScrollMaskBlits[1], pos, 0); }
    if ((gControllerButtons & 8) == 0) {
        ClearAllPlayerControls(2);
    }
    return 1;
}
#pragma opt_propagation reset

/* ---- transition-flag accessors ---- */

int FireScrollActive(void)
{
    if (gFireScrollCircleBlits[0]) {
        return 1;
    }
    return 0;
}

void FireScrollReset(void)
{
    gFireScrollCircleBlits[0] = 0;
}

#pragma dont_inline on
int ticks_for_firescroll(void)
{
    return 0;
}
#pragma dont_inline off

/*
 * 4-player interactive dialog / message box (~ControllerMessageBox): runs a
 * modal loop drawing the prompt and polling all four pads until one accepts
 * or cancels, returning the chosen option. Giant; skeleton.
 */
int ScrollTextNum(s32 a, s32 msg);
int ScrollTextWidth(s32 a, s32 msg, s32 pos, f32 scale);
int ScrollTextHeight(s32 a, s32 msg, s32 pos, s32 b, f32 scale);
void WritePlayerInfo(s32 idx);
void EnablePlayerControls(void);
void mbBlitInit3414(void* blit, s32 a);
void AudioSysSync(s32 a);
void AudioMusicVolUpdate(void);
void DrawScrollText(s32 a, s32 color, s32 y, s32 b, s32 c, u32 flags, s32 msg,
                    s32 pos);
void DrawGlowText(s32 color, s32 y, void* text, f32 scale);
int fn_80054CDC(void);
void AudioKillBySound(s32 snd);
void MBBlitOrder(void* a, void* b);
extern s32 gDrawTextY;
extern s32 lbl_80344E44;
extern f32 lbl_80343C80;
extern void* lbl_80343C84;
extern void* lbl_80343C88;
extern s32 lbl_803445D8;
extern u8 gPlayers[];

#pragma opt_propagation off
int ControllerMessageBox(s32 mask, s32 msg, s32 count, s32 sound)
{
    s32 busySave;
    s32 maskSave;
    u8* players;
    s32 end;
    s32 ticks;
    s32 i;
    s32 quiet;
    s32 w;
    s32 y;
    s32 h;
    s32 boxw;
    s32 x;
    s32 yy;
    u32 flags;
    s32 minw;
    void* blit;
    void* bbl;
    s32 ty;

    maskSave = mask;
    busySave = gGameBusy;
    if (count < 0) {
        ticks = 0;
        end = ScrollTextNum(0, msg) - 1;
    } else {
        ticks = count;
        end = count;
    }
    EnablePlayerControls();
    for (i = 0; i < 4; i++) {
        WritePlayerInfo(i);
    }
    gGameBusy = 1;
    gModalRenderDepth++;
    msgUpdate();
    MBHideMarkedMessages();
    MBLockMessages(gModalRenderDepth - 1);
    lbl_80344E04 = 1;
    {
        void* normalText;

        minw = DrawNormalText((normalText = lbl_80343C84, lbl_80343C80),
                              normalText, 6) + 32;
    }
    blit = MBNewBlit(lbl_80343C88, 0, 0);
    bbl = MBCreateBlit(0, lbl_80344E44, 190, 8, 20, 20);
    players = gPlayers;
    for (count = ticks; count <= end; count++) {
        w = ScrollTextWidth(0, msg, count, lbl_80347378) + 96;
        h = ScrollTextHeight(0, msg, count, 4, lbl_80347378) + 96;
        if (w < minw) {
            boxw = minw;
        } else if (w > 512) {
            boxw = 512;
        } else {
            boxw = w;
        }
        x = 256 - boxw / 2;
        y = 160 - h / 2;
        yy = y + 32;
        flags = 0x160C03;
        ticks = 15;
        w = boxw;
        mbBlitProject(blit, w, h);
        mbBlitCalcWidth(blit, x, y, lbl_803473C8);
    frame_top:
            serve_busy(-5);
            quiet = sndFxQueUpdate() ? 0 : 1;
            AudioSysSync(1);
            if (sound < 0) {
                quiet = 1;
            }
            ClockOncePerFrame();
            sndFxQueUpdate();
            PlayerControls();
            if (lbl_803445D8 != 0) {
                ticks_for_firescroll();
            }
            ScreenSaver();
            AudioMusicVolUpdate();
            DrawScrollText(0, -256, yy, 4, -1, flags, msg, count);
            ty = gDrawTextY;
            mbBlitInit3414(bbl, 0);
            mbBlitCalcWidth(bbl, 190, ty + 8, lbl_803473C8);
            {
                f32 glowScale = lbl_80343C80;
                s32 glowY = ty + 8;

                DrawGlowText(-256, glowY, lbl_80343C84, glowScale);
            }
            MBEndFrame();
            if (ticks <= 0) {
                s32 nbut = 0;
                u32 buttons = 0;

                for (i = 0; i < 4; i++) {
                    if ((maskSave & (1 << i)) != 0) {
                        u8* pp = players + i * 13148;
                        if (*(s32*)(pp + 232) != 0) {
                            u8* pb = lbl_80240E30 + i * 60;
                            nbut++;
                            buttons |= *(u32*)(pb + 8);
                        }
                    }
                }
                if ((buttons & 0x08000000) == 0 && nbut != 0) {
                    goto frame_check;
                }
                if (sound >= 0 && quiet == 0) {
                    AudioKillBySound(sound);
                }
                goto frame_done;
            } else if (ticks > 0) {
                ticks -= gClockStepTicks;
            }
        frame_check:
            if (fn_80054CDC() != 0) {
                goto frame_done;
            }
            goto frame_top;
    frame_done:
        if ((gControllerButtons & 8) == 0) {
            ClearAllPlayerControls(2);
        }
    }
    MBRemoveBlit(bbl);
    MBBlitOrder((void*)StartFireScroll((char*)lbl_80343C88, -1, x, y, w, h, 0,
                                       lbl_80347398),
                blit);
    for (;;) {
        serve_busy(-1);
        ClockOncePerFrame();
        sndFxQueUpdate();
        DrawScrollText(0, -256, yy, 4, -1, flags, msg, count - 1);
        if (ServeFireScroll() == 0) {
            break;
        }
        MBEndFrame();
    }
    MBRemoveBlit(blit);
    MBUnlockMessages(gModalRenderDepth - 1);
    gModalRenderDepth--;
    lbl_80344E04 = 0;
    gGameBusy = busySave;
    for (i = 0; i < 4; i++) {
        *(s16*)(gPlayers + i * 13148 + 2406) = 0;
    }
    ClearAllPlayerControls(4);
    LoadVU1GameLogic();
    return 0;
}
#pragma opt_propagation on
