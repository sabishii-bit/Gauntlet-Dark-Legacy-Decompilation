/*
 * main.c - game entry (Xbox: MAIN.OBJ: main_init, sendPlayerPos, playMovie,
 * test_movies, DefunctThreads, GauntletMain). GCN main() is GauntletMain
 * merged with the console entry.
 */
#include "types.h"

void DEMOInit(void* renderMode);
int DVDOpen(const char* path, void* fileInfo);

void GXSetCopyClear(void* color, u32 z);
void GXSetZMode(int enable, int func, int update);
void GXSetVtxAttrFmt(int fmt, int attr, int cnt, int type, int frac);
void C_MTXOrtho(void* m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);
void GXSetProjection(void* m, int type);
void PSMTXIdentity(void* m);
void GXLoadPosMtxImm(void* m, u32 id);
void GXSetCullMode(int mode);
void GXSetNumChans(u8 n);
void GXSetNumTevStages(u8 n);
void GXSetNumTexGens(u8 n);
void GXSetChanCtrl(int chan, int enable, int amb, int mat, int lights, int diff, int attn);
void GXClearVtxDesc(void);
void GXSetVtxDesc(int attr, int type);
void GXSetTexCoordGen2(int dst, int fn, int src, u32 mtx, int normalize, u32 pt);
void GXSetTevKColor(int sel, void* color);
void GXSetTevKAlphaSel(int stage, int sel);
void GXSetTevOrder(int stage, int coord, int map, int color);
void GXSetTevOp(int stage, int mode);
void GXSetTevAlphaIn(int stage, int a, int b, int c, int d);
void GXSetTevAlphaOp(int stage, int op, int bias, int scale, int clamp, int out);
void GXSetTevColorOp(int stage, int op, int bias, int scale, int clamp, int out);
void GXSetTevColorIn(int stage, int a, int b, int c, int d);
void GXInvalidateTexAll(void);
void GXGetViewportv(f32* vp);

void ARInit(u32* stack_index_addr, u32 num_entries);
void ARQInit(void);
void AIInit(u8* stack);
void AXInit(void);
void sndVoiceInit(void);

void pbPulseTime(void);
void DrawText(int x, int y, int flags, u32 color, const char* fmt, ...);
void LockMem(int arg);
void FreeUnlockedMem(int arg);
void PlayVQMovie(const char* name);

extern char* movie_list1[];   /* movie_list1 (7) */
extern char* movie_list2[];   /* movie_list2 (26) */
extern s32 sMovieIdx1;       /* movie_list1 index */
extern s32 sMovieIdx2;       /* movie_list2 index */
extern u32 lbl_80344620;       /* pad buttons (held) */
extern u64 gControllerButtons;       /* pad buttons 64-bit */
extern s32 dbg_test_movies;       /* keep-running flag */

void fn_800B27C4(void);
void padInit(void);
void sysSetResetCallback(void (*handler)(char**, int));
void sysSetMsgCallback(void (*printer)(const char*));
u32 pbGetTime(void);
void srand(unsigned int seed);
s64 OSGetTime(void);
void game_init_once(const char* name);
void LoadModel(void* a, void* b, int c, int d);
void MBNewBlit(const char* name, int x, int y);
void MBEndFrame(void);
void MBOX_ResetUnlockedModels(int arg);
void ReadControls(void);
void bulletproof_printf(const char* fmt, ...);
void game_init_data(void);
void OptionsSetup(void);
void FontInit(void);
u32 BytesFree(void);
void ControlsUpdate(void);
void AtreeAlloc(int w, int h);
void pbInitDiag(int arg);
void ResetModels(void);
void LockModels(int arg);
void init_attract_mode(int screen);
void ClockOncePerFrame(void);
void LoadVU1GameLogic(void);
void PlayerControls(void);
void sndSysStub1(void);
void ScreenSaver(void);
int DoOptions(void);
void sndFxQueUpdate(void);
void AudioSysSync(int arg);
int pbDiagDrawMenu(void);
void AtreeInitLists(int arg);
void fn_8006FF1C(void);
void DoLighting(int arg);
int sysTestFlags(int arg);
void game_main(void);
int TriggerCamUpdate(void);
void MBCameraUpdate(void* a, void* b);
int BossCameraUpdate(void);
void do_camera(void);
void fn_8006FE30(void);
void UpdateCam(void);
void fn_80052134(void);
void fn_800C0394(void);
void sndSysStub0(void);
void default_options(void);
void ResetClock(void);
void InitMemHandler(void);
void init_psx2(s32 arg);
void AdsAllocBuffer(void);
void fn_800C73E0(void);
int sprintf(char* buf, const char* fmt, ...);
void fn_800BC418(int a, int b);
void dbgTextInit(void);
void MBInit(void);
void FontEndFrame(void);
void FontInitDefault(void);
void AudioLoadRom(void);
void InitControls(void);
void MBWindowZoom(f32 zoom);
void fn_800C1170(int a, void* b, int c);

extern char sBootStrings[];    /* string table (soulsave.. boot strings) */
extern char lbl_80126A98[];    /* version string */
extern u8 lbl_80127D00[];
extern u8 gIdentityMatrix[];
extern u32 gErrorCode;         /* boot clear color */
extern s32 sBootPhase;       /* boot step */
extern u8* mlmMemBase;
extern s32 mlmMemLimit;
extern u32 lbl_803472BC;
extern u32 lbl_803472C4;
extern u32 lbl_803472CC;
extern u32 sBootModelHandle;
extern f32 sBootWindowZoom;       /* boot window zoom */
extern u32 lbl_80344DA8;
extern s32 sDiagPending;       /* save-pending flag */
extern u8 pbMeasureLoad;
extern s32 gGameBusy;
extern s32 gDemoMode;
extern s32 lbl_80343B00;
extern s32 gGameMode;       /* game state id */
extern s32 lbl_80344A80;
extern s32 sSpecialCamInit;
extern u32 gBossObj;
extern s32 lbl_80343C5C;
extern s32 lbl_803444E0;
extern s32 lbl_8034475C;
extern s32 lbl_80344F80;
extern u32 sMainFrames;
extern u8 lbl_803441F0;
extern s64 lbl_80344278;       /* OSTime snapshot */
extern u32 sMainTimerTicks;

extern char* lbl_80344800;      /* debug print list base */
extern char* sDbgPrintPtr;
extern s32 sDbgPrintY;        /* debug print y cursor */
void serve_io(void);
void adsPoll(void);             /* ADSTREAM per-frame poll (adsPoll) */

/* main globals block: DVDFileInfo @0, ortho Mtx44 @60, viewport @124 */
extern u8 gDvdScratchFileInfo[];
extern char sCheckTxt[]; /* "check.txt" */

extern f32 sMusicFadeBase;
extern f64 lbl_803471B8;
extern f32 AmbientSpecialTime;
extern f32 AmbientSpecialValue;
extern u32 sBootClearColor;  /* copy-clear color */
extern f32 lbl_803472B4;  /* 1.0f */
extern f32 lbl_803472B8;  /* -1.0f */
extern u32 lbl_803472B0;  /* tev kcolor */

/* peak tracker over the frame meter (Xbox data: vb_last_print) */
void fn_80067AE0(f32 t, f32 v)
{
    f32 x = (f32) (lbl_803471B8 + (sMusicFadeBase + t));

    if (x > AmbientSpecialTime) {
        AmbientSpecialTime = x;
        AmbientSpecialValue = v;
    }
}

/* per-frame service pump */
void serve_busy(int flags)
{
    if (flags & 1) {
        pbPulseTime();
    }
    if (flags & 2) {
        serve_io();
    }
    if (flags & 4) {
        adsPoll();
    }
}

void main_init(high)
register u32 high;
{
    f32 ident[3][4];
    u32 clear;
    u8* g;

    high = 0x80260000;
    g = (u8*)(high - 0x1218); /* &gDvdScratchFileInfo; literal form needed for codegen */
    DEMOInit(0);
    while (DVDOpen(sCheckTxt, g) == 0) {
    }
    clear = sBootClearColor;
    GXSetCopyClear(&clear, 0);
    GXSetZMode(1, 6, 1);
    GXSetVtxAttrFmt(0, 9, 1, 4, 0);
    GXSetVtxAttrFmt(0, 11, 1, 5, 0);
    C_MTXOrtho(g + 60, lbl_803472B4, lbl_803472B8, lbl_803472B8,
               lbl_803472B4, lbl_803472B8, lbl_803472B4);
    GXSetProjection(g + 60, 1);
    PSMTXIdentity(ident);
    GXLoadPosMtxImm(ident, 0);
    GXSetCullMode(0);
    GXSetNumChans(1);
    GXSetNumTevStages(1);
    GXSetChanCtrl(4, 0, 0, 1, 0, 0, 2);
    GXClearVtxDesc();
    GXSetVtxDesc(9, 1);
    GXSetVtxDesc(11, 1);
    GXSetTexCoordGen2(1, 1, 5, 60, 0, 125);
    *(u32*)((u8*)ident - 8) = lbl_803472B0;
    GXSetTevKColor(0, (u8*)ident - 8);
    GXSetTevKAlphaSel(1, 28);
    GXSetTevOrder(2, 255, 255, 4);
    GXSetTevOp(2, 4);
    GXSetTevAlphaIn(2, 7, 5, 0, 7);
    GXSetTevAlphaOp(2, 0, 0, 0, 1, 0);
    GXSetVtxAttrFmt(0, 13, 1, 4, 0);
    GXSetVtxAttrFmt(0, 14, 1, 4, 0);
    GXSetTexCoordGen2(0, 1, 4, 60, 0, 125);
    GXSetTevOrder(0, 0, 0, 4);
    GXSetNumTexGens(1);
    GXSetTevOp(0, 0);
    GXSetTevColorOp(0, 0, 0, 1, 1, 0);
    GXSetTevColorIn(0, 15, 10, 8, 15);
    GXInvalidateTexAll();
    GXSetVtxDesc(13, 1);
    GXGetViewportv((f32*) (g + 124));
    ARInit(0, 0);
    ARQInit();
    AIInit(0);
    AXInit();
    sndVoiceInit();
}
void main_report_cb(char** msg, int level);
void main_print_cb(const char* str);

void test_movies(void)
{
    LockMem(1);
    while (dbg_test_movies != 0) {
        if (sMovieIdx1 >= 0) {
            PlayVQMovie(movie_list1[sMovieIdx1]);
            FreeUnlockedMem(1);
            if (lbl_80344620 & 0x10000000) {
                sMovieIdx1--;
            } else {
                sMovieIdx1++;
            }
            if ((u32) sMovieIdx1 >= 7) {
                sMovieIdx1 = -1;
                sMovieIdx2 = 0;
            } else if (sMovieIdx1 < 0) {
                sMovieIdx2 = 24;
            }
        } else {
            if (sMovieIdx2 < 0) {
                sMovieIdx2 = 0;
            }
            PlayVQMovie(movie_list2[sMovieIdx2]);
            FreeUnlockedMem(1);
            if (lbl_80344620 & 0x10000000) {
                sMovieIdx2--;
            } else {
                sMovieIdx2++;
            }
            if ((u32) sMovieIdx2 >= 26) {
                sMovieIdx2 = -1;
                sMovieIdx1 = 0;
            } else if (sMovieIdx2 < 0) {
                sMovieIdx1 = 5;
            }
        }
        if (lbl_80344620 & 0x00040000) {
            break;
        }
        if (gControllerButtons & 4) {
            break;
        }
    }
}

void main(void)
{
    char* st = sBootStrings;

    main_init();
    fn_800B27C4();
    padInit();
    sysSetResetCallback(main_report_cb);
    sysSetMsgCallback(main_print_cb);
    pbPulseTime();
    gErrorCode = 0xFF;
    sBootPhase = 0;
    game_init_once(st + 48);
    LoadModel(&lbl_803472BC, 0, 0, -1);
    MBNewBlit(st + 60, 0, 0);
    MBNewBlit(st + 76, 256, 0);
    MBNewBlit(st + 92, 0, 256);
    MBNewBlit(st + 108, 256, 256);
    MBEndFrame();
    MBEndFrame();
    MBOX_ResetUnlockedModels(0);
    ReadControls();
    bulletproof_printf(st + 124, lbl_80126A98);
    game_init_data();
    OptionsSetup();
    FontInit();
    bulletproof_printf(st + 156, BytesFree(), mlmMemBase,
                       mlmMemBase + (mlmMemLimit / 4) * 4);
    ControlsUpdate();
    if (lbl_80344620 & 0x02000000) {
        dbg_test_movies = 1;
    } else {
        dbg_test_movies = 0;
    }
    if (dbg_test_movies != 0) {
        test_movies();
    }
    if (sDiagPending != 0) {
        u32 tmp;
        u8 pad[16]; /* unused, matches original frame */

        AtreeAlloc(1024, 1024);
        LoadModel(&lbl_803472C4, &tmp, 1, -1);
        pbInitDiag(2);
    } else {
        bulletproof_printf(st + 208);
        ResetModels();
        LockModels(1);
        init_attract_mode(-1);
    }
    bulletproof_printf(st + 236, BytesFree());
    gErrorCode = 0x00FF0000;
    pbMeasureLoad = 1;

    for (;;) {
        pbPulseTime();
        srand(pbGetTime() >> 3);
        gGameBusy = 0;
        sBootPhase = 2;
        ClockOncePerFrame();
        LoadVU1GameLogic();
        sBootPhase = 3;
        PlayerControls();
        if (gDemoMode == 0) {
            sndSysStub1();
        }
        sBootPhase = 11;
        ScreenSaver();
        sBootPhase = 4;
        if (DoOptions() != 0) {
            gGameBusy = 1;
        }
        sBootPhase = 5;
        sndFxQueUpdate();
        AudioSysSync(1);
        if (sDiagPending != 0) {
            sBootPhase = 6;
            if (pbDiagDrawMenu() != 0) {
                lbl_80343B00 = -1;
                MBOX_ResetUnlockedModels(0);
                AtreeInitLists(0);
                bulletproof_printf(st + 208);
                FontInit();
                ResetModels();
                LockModels(1);
                init_attract_mode(0x8004);
                sDiagPending = 0;
            }
            fn_8006FF1C();
        } else {
            sBootPhase = 7;
            DoLighting(0);
            sBootPhase = 8;
            if (sysTestFlags(32) == 0) {
                game_main();
            }
            sBootPhase = 9;
            if (TriggerCamUpdate() == 0) {
                s32 state = gGameMode;

                if (state == 0x8009 || state == 0x400B || state == 0x4012) {
                    MBCameraUpdate(lbl_80127D00, gIdentityMatrix);
                } else if (state == 0x4013 || state == 0x400D || state == 0x4017) {
                    if (BossCameraUpdate() == 0) {
                        do_camera();
                    }
                } else if (lbl_80344A80 == 1) {
                    if (sSpecialCamInit == 0) {
                        fn_8006FE30();
                        sSpecialCamInit = 1;
                    }
                    fn_8006FF1C();
                } else if (state & 0x8000) {
                    lbl_80344A80 = 0;
                    sSpecialCamInit = 0;
                    do_camera();
                } else {
                    sSpecialCamInit = 0;
                    if (gBossObj != 0) {
                        lbl_80344A80 = 0;
                        if (BossCameraUpdate() == 0) {
                            do_camera();
                        }
                    } else if (lbl_80343C5C == 0) {
                        lbl_80344A80 = 0;
                        lbl_803444E0 = 0;
                        do_camera();
                    } else {
                        lbl_803444E0 = 0;
                        lbl_80344A80 = 2;
                        UpdateCam();
                    }
                }
            }
            fn_80052134();
        }
        if (lbl_8034475C != 0) {
            fn_800C0394();
        }
        lbl_80344F80 = (gGameBusy != 0) ? 0 : 1;
        sndSysStub0();
        MBEndFrame();
        serve_io();
        sMainFrames++;
        /* PARKED 1-bit residual: target compares this u8 with cmplwi;
           every source form tried emits cmpwi (same semantics for a
           zero-extended byte vs 0). Plus one reloc-name split on the
           OSTime store (lbl_80344278+4 vs lbl_8034427C; same address). */
        if (lbl_803441F0 != 0) {
            if (sMainFrames > 1) {
                if (sMainFrames == 2) {
                    lbl_80344278 = OSGetTime();
                } else {
                    sMainTimerTicks++;
                }
            }
        }
    }
}

/* report handler: prints levels 0/1 centered-ish in white */
void main_report_cb(char** msg, int level)
{
    switch (level) {
    case 0:
    case 1:
        DrawText(320, 24, 0, 0x00FFFFFF, *msg);
        break;
    case 2:
        break;
    }
}

/* debug print-line callback: NULL resets; first line yellow, then white */
void main_print_cb(const char* str)
{
    if (str == 0) {
        sDbgPrintPtr = lbl_80344800;
        sDbgPrintY = 24;
        return;
    }
    {
        s32 y = sDbgPrintY;
        DrawText(16, y, 0, (y == 24) ? 0x00FFFF00 : 0x00FFFFFF, str);
    }
    sDbgPrintY += 12;
}

/* 0x800684D4  one-time boot bring-up: memory, mb/pb, fonts, audio rom,
 * controls. Final fn of the main.c TU (map gap 0x800684D4-0x800685EC). */
void game_init_once(const char* name)
{
    char buf[36];
    char* st = sBootStrings;

    default_options();
    ResetClock();
    bulletproof_printf(st + 264);
    InitMemHandler();
    init_psx2(*(s32*)name);
    AdsAllocBuffer();
    fn_800C73E0();
    sprintf(buf, st + 288, mlmMemLimit / 1024, mlmMemLimit / 0x100000);
    bulletproof_printf(buf);
    fn_800BC418(2, -1);
    dbgTextInit();
    MBInit();
    bulletproof_printf(st + 316);
    LoadModel(&lbl_803472CC, &sBootModelHandle, 1, -1);
    FontEndFrame();
    FontInitDefault();
    bulletproof_printf(st + 328);
    AudioLoadRom();
    bulletproof_printf(st + 348);
    InitControls();
    MBWindowZoom(sBootWindowZoom);
    LockModels(0);
    fn_800C1170(325, &lbl_80344DA8, 0);
    bulletproof_printf(st + 376, BytesFree());
}
