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
 *   0x8008E3BC init_player_change              welcome-back blit (AudioWelcome[Back])
 *   0x8008E4F4 do_sel_menu          (L) per-player class picker (16x from above)[high]
 *   0x8008F768 setup_file_entries              save-name compare helper (strncmp)
 *   0x8008F914 verify_vmu_file_ok              small select helper
 *   0x8008F984 setup_vmu_entries              save-slot text formatter ("SLOT %d")
 *   0x8008FA70 setup_sel_menu              load/save sub-menu state (jumptable)
 *   0x8008FC5C sel_set_inactive              tiny select helper
 *   0x8008FC78 sel_set_choice              select-blit serve/update
 *   0x8008FE70 other_players_next_level              per-player select-state getter (+0x830)
 *   0x8008FED4 check_active_players              small select helper
 *   0x8008FF58 SelectLoadDone        (G) async-load completion poll            [med]
 *   0x8008FFB0 SelectLoadStart       (G) kick off async tower/geo load         [med]
 *   0x8008FFF0 update_class_attr              class-stat text draw (~update_class_attr)
 *   0x80090450 update_class_spec              class-spec draw (~update_class_spec)
 *   0x800907B4 init_player_select    (G) enter/init the select screen          [high]
 *   0x80090B6C hide_select_blits              blit-init helper (mbBlitInit3414)
 *   0x80090C34 setup_tex              blit text printf (vsprintf/mbInitBlit)
 *   0x80090D6C serve_blits              blit build/project helper (jumptable)
 */

#include "types.h"
#include "__va_arg.h"

/* ---- boss-requirement table (this TU, .data 0x80121DD8, 12 x 0x24) ---- */
typedef struct BossRuneReq {
    s32 boss;      /* +0x00 */
    s32 _04;
    s32 _08;
    s32 _0c;
    s32 beatFlag;  /* +0x10 */
    s32 numRunes;  /* +0x14 */
    s32 _18;
    s32 _1c;
    s32 _20;
} BossRuneReq;     /* 0x24 */
extern BossRuneReq bossRuneReqTable[];

/* ---- select-screen state (small data / bss, shared other TUs) ---- */
extern s32 lbl_80344B7C;
extern s32 lbl_80344B80;
extern s32 lbl_80344BC0;   /* load-in-progress flag */
extern s32 lbl_80343DD4;   /* async load handle      */
extern s32 gDemoMode;
extern u32 lbl_80344824;   /* active-player bit mask */
extern s32 lbl_80344A18;   /* per-(port+slot) card state (3=ready) */
extern s32 lbl_80344A14;   /* per-(port+slot) card-present flag    */
extern u8 lbl_80274578[];  /* vmu dir-info rows, 132B per (port,slot) */
extern u8 lbl_80284A88[];  /* per-player file-entry menu table, 36B ea */
extern char lbl_80348018[8]; /* save-file name prefix (5 chars checked) */
s32 get_vmu_directory(s32 a, s32 b);
extern s32 lbl_803448AC;   /* front-end screen id (8 = in-game shop?) */
extern s32 lbl_803448A8;   /* front-end sub-state                     */
extern u32 lbl_80343D6C;   /* current save-file owner tag             */
s32 vmu_directory_exists();
extern s32 gGameMode;
extern char lbl_801143F8[];  /* select-screen string pool             */
extern char* lbl_80120104[]; /* per-class texture-name pointer table  */
extern char lbl_80347F44[];  /* "?" texture name (sdata)              */
extern char lbl_80347F4C[];  /* unarmed spec label fmt (sdata)        */
extern char lbl_80347F58[];  /* "%s NAME" fmt (sdata)                 */
extern void* pbLoad;
void PlayerModel(s32 player);
void setup_player_display(s32 player);
void hide_select_blits(s32 arg0, s32 flag);

typedef struct StrBlock4 {
    char* s[4];
} StrBlock4;
extern s32 lbl_80344610;   /* memcard slot sub-state */
extern u8  lbl_80343DEC;   /* current card port/slot byte */
extern char lbl_80114718[];/* save-slot format string A */
extern char lbl_80114724[];/* save-slot format string B */
extern u8  gPlayers[]; /* 4-player array, stride 0x335C */
extern u8  lbl_80284878[]; /* 4 pages x 11 entries x 0xC blit table */
extern u8  lbl_80121688[]; /* select-menu data page */
extern s32 lbl_80343DD8;
extern f32 lbl_80348020;

extern s32  new_menu_accept(s32 plyr, s32 allow_start);
extern void new_player(s32 i);
void setup_tex(s32 id, s32 slot, s32 flags, s32 hide, char* fmt, ...);

typedef struct SelectSlot {
    u8 _pad[108];
    s32 state;
    u8 _tail[120];
} SelectSlot;

extern SelectSlot lbl_80121950[];

/* ---- audio / front-end (other TUs) ---- */
extern void AudioWelcome(s32 pidx, s32 flag);
extern void AudioWelcomeBack(s32 pidx, s32 flag);
extern void change_player(s32 i, s32 type);
extern char lbl_801200B0[][4]; /* 4-char class name table */
extern char lbl_801144A0[];    /* welcome-back blit format string */
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
extern int  vsprintf(char* buf, const char* fmt, va_list ap);
extern int  strncmp(const char* a, const char* b, int n);
extern void DrawGlowText(f32 scale, s32 x, s32 y, char* txt);
extern s32 DrawNormalText(f32 scale, char* txt, s32 font);
extern s32 DrawTextKeepScale(f32 scale, s32 x, s32 y, s32 font,
                             s32 color, char* txt);
extern char* GetStringText(s32 id, s32 sub, s32 mode);
extern void* MBNewTempBlit(void* tex, int x, int y, int w, int h);
extern int  MBCreateBlit(int a, int b, int c, int d, int e, int f);
extern void* MBOX_FindTexture_Err(char* name, s32* out, s32 err);
extern void mbBlitInit3414(void* blit, s32 hide);
extern void mbBlitProject(void* blit, s32 w, s32 h);
extern void mbInitBlitEntry(void* blit, u32 frames, s32 frame);
extern void mbBlitUpdateEntry(void* blit, u32 mask, u32 set);
extern void MBBlitSetAlpha(void* blit, s32 alpha);
extern void MBBlitSetColor4(void* blit, u32 a, u32 b, u32 c, u32 d);
extern void* memcpy(void* dst, const void* src, unsigned long size);
extern void ClearPlayerControl(s32 player, s32 code);
extern void start_optmenu_nostack(void* menu, s32 player);

/* ---- async-load primitives (other TUs) ---- */
extern int  MBOX_BGLoadModelStart(void* name, int a);
extern int  MBOX_BGLoadModelDone(void);
extern void fn_8005403C(int a);
extern char lbl_80348024[7];

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
    for (i = 0; i < 13; i++) {
        if (boss == bossRuneReqTable[i].boss) {
            return bossRuneReqTable[i].numRunes;
        }
    }
    return 0;
}

s32 GetBossBeatFlag(s32 boss)
{
    int i;
    for (i = 0; i < 13; i++) {
        if (boss == bossRuneReqTable[i].boss) {
            return bossRuneReqTable[i].beatFlag;
        }
    }
    return 0;
}

/* Clamp/wrap a class-selection index into the valid range for the current
 * select mode, skipping classes that are locked (bit 23 of +0xA8C). */
static s32 LimitSeltype(u8* player, s32 idx, s32 step)
{
    int flag;
    if (gDemoMode != 0) {
        if (idx < 4) {
            idx = 7;
        } else if (idx > 7) {
            idx = 4;
        }
    } else {
        flag = 0;
        while (flag == 0) {
            if (idx > 16) {
                idx = 0;
            }
            if (idx < 0) {
                idx = 16;
            }
            flag = 1;
            if (idx == 16 && (*(u16*)(player + 0xA8C) & 0x100) == 0) {
                idx += step;
                flag = 0;
            }
        }
    }
    return idx;
}

static void do_sel_menu(s32 player, u32 mode);

/* Top-level select state machine (invoked from gamemain / attract). */
void do_player_select(void)
{
    int i;
    init_titlescreen();
    init_attract_mode();
    for (i = 0; i < 16; i++) {
        do_sel_menu(i, 0);
    }
    AudioWelcome(0, 0);
}

extern s32 lbl_80343DDC;
extern f32 lbl_80343DE0;
extern s32 lbl_80343DE4;
extern s32 lbl_80343DE8;
extern s32 lbl_80344BC4;
extern s32 lbl_80344BB8;
extern s32 lbl_803449A0;
extern f32 lbl_80347F60;
extern f32 lbl_80347F64;
extern char lbl_80347F0C[8];
extern char lbl_80347F14[8];
extern char lbl_80347F1C[8];
extern char lbl_80347F68[8];
extern char lbl_80347F70[8];
extern char lbl_80347F78[8];
extern char lbl_80347F80[8];
extern char lbl_80347F88[8];
extern char lbl_80347F90[8];
extern char lbl_80347F98[8];
extern char lbl_80347FA0[8];
extern char lbl_80347FA8[4];
extern char lbl_80347FAC[8];
extern char lbl_80347FB4[8];
extern char lbl_80347FBC[4];
extern char lbl_80347FC0[8];
extern char lbl_80347FC8[8];
extern char lbl_80347FD0[8];
extern char lbl_80347FD8[8];
extern char lbl_80347FE0[8];
extern char lbl_80347FE8[8];
extern char lbl_80347FF0[8];
extern char lbl_80347FF8[8];
extern char lbl_80348000[8];
extern char lbl_80348008[8];
extern char lbl_80348010[8];
extern void* lbl_80344E2C;
extern void* lbl_80344E30;
extern void* lbl_80344E34;
extern void* lbl_80344E38;
extern void* lbl_80344E44;
extern void* lbl_80344E48;
extern s32 saveGetFreeBytes(s32 chan, s32 handle);
static s32 sel_set_choice(s32 player, s32 mode);

static void do_sel_menu(s32 player, u32 mode)
{
    s32* xp = (s32*)(lbl_80121688 + (player << 2));
    char* pool = lbl_801143F8;
    u8* pl = gPlayers + player * 13148;
    f32 scale = lbl_80343DE0;
    s32 lh = lbl_80343DDC;
    s32 font = lbl_80344BC4;
    s32 showSel = 1;
    s32 showBack = 0;
    s32 x = *xp;
    s32 sz = (s32)(lbl_80347F60 * scale);
    s32 sz2 = (s32)(lbl_80347F60 * scale);
    char buf[32];

    switch (mode) {
    case 0:
        showBack = 1;
        /* fall through */
    case 1:
        sel_set_choice(player, mode);
        break;
    case 2: {
        s32 nx = -(x + 64);
        s32 bx;
        s32 x2;
        s32 tx;
        s32 y2;
        DrawTextKeepScale(lbl_80347F64, nx, 64, 6, 0xFFFFFF, lbl_80347F68);
        DrawTextKeepScale(lbl_80347F64, nx, 90, 6, 0xFFFFFF, lbl_80347F70);
        DrawTextKeepScale(lbl_80347F64, nx, 116, 6, 0xFFFFFF,
                          lbl_80347F78);
        bx = *xp + sz + 10;
        y2 = lbl_80343DE4;
        lh = 20;
        x2 = bx - sz;
        MBNewTempBlit(lbl_80344E30, x2, y2, sz, sz2);
        MBNewTempBlit(lbl_80344E2C, bx, y2, sz, sz2);
        tx = bx + (sz + 8);
        DrawTextKeepScale(scale, tx, y2 + 4, font, 0xFFFFFF, lbl_80347F1C);
        MBNewTempBlit(lbl_80344E38, x2, y2 + 20, sz, sz2);
        MBNewTempBlit(lbl_80344E34, bx, y2 + 20, sz, sz2);
        DrawTextKeepScale(scale, tx, y2 + 24, font, 0xFFFFFF,
                          lbl_80347F80);
        MBNewTempBlit(lbl_80344E48, bx, y2 + 40, sz, sz2);
        DrawTextKeepScale(scale, tx, y2 + 44, font, 0xFFFFFF,
                          lbl_80347F88);
        MBNewTempBlit(lbl_80344E44, bx, y2 + 60, sz, sz2);
        DrawTextKeepScale(scale, tx, y2 + 64, font, 0xFFFFFF,
                          lbl_80347F90);
        showSel = 0;
        break;
    }
    case 3: {
        s32 bx = x + sz + 10;
        s32 by = lbl_80343DE4 + 52;
        s32 t;
        lh = 20;
        MBNewTempBlit(lbl_80344E38, bx - sz, by, sz, sz2);
        MBNewTempBlit(lbl_80344E34, bx, by, sz, sz2);
        DrawTextKeepScale(scale, bx + (sz + 8), lbl_80343DE4 + 56, font,
                          0xFFFFFF, lbl_80347F1C);
        if (*(s32*)(pl + 16) < 8) {
            t = 1;
        } else if (*(u16*)(pl + 2700) &
                   (1 << (*(s32*)(pl + 16) - 8))) {
            t = 1;
        } else {
            t = 0;
        }
        showSel = (t != 0) ? 1 : 0;
        break;
    }
    case 5:
        if (*(s32*)(pl + 13128) == 0) {
            s32 y = lbl_80343DE8 +
                    *(s32*)(lbl_80121688 + player * 232 + 728) - lh * 6;
            s32 nx = -(x + 64);
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347F98);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 220);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 244);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 256);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 220);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FA0);
            break;
        }
        /* fall through */
    case 10: {
        s32 y = lbl_80343DE8 + *(s32*)(lbl_80121688 + player * 232 + 728) -
                lh * 3;
        s32 nx = -(x + 64);
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FA8);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FAC);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FB4);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 268);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 280);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 292);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 304);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FBC);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 320);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FC0);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 336);
        showBack = 1;
        break;
    }
    case 7: {
        s32 y = lbl_80343DD8;
        s32 nx = -(x + 64);
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 348);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 360);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FAC);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FC8);
        break;
    }
    case 8: {
        s32 y = lbl_80343DE8 + *(s32*)(lbl_80121688 + player * 232 + 728) -
                lh * 2;
        s32 nx = -(x + 64);
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 376);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 388);
        showBack = 1;
        break;
    }
    case 13: {
        s32 y = lbl_80343DE8 + *(s32*)(lbl_80121688 + player * 232 + 728) -
                lh * 3;
        s32 nx = -(x + 64);
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 376);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 404);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 416);
        showBack = 1;
        break;
    }
    case 6: {
        s32 st = *(s32*)(pl + 13128);
        s32 y = lbl_80343DD8 - lh * 5;
        s32 nx;
        s32 yy;
        if (st == 0) goto fmt_none;
        if (st < 0) goto fmt_result;
        if (st >= 4) goto fmt_result;
        goto fmt_prog;
fmt_none:
        nx = -(x + 64);
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 428);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FAC);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FC8);
        y += lh;
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 440);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FD0);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 452);
        break;
fmt_prog:
        nx = -(x + 64);
        yy = lbl_80343DD8;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 464);
        yy += lh;
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 304);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 480);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, lbl_80347FD8);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, lbl_80347FC0);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 492);
        showSel = 0;
        break;
fmt_result:
        if (st >= 0) {
            nx = -(x + 64);
            yy = lbl_80343DD8;
            DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, lbl_80347FE0);
            yy += lh;
            DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 508);
        } else {
            nx = -(x + 64);
            yy = lbl_80343DD8;
            DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, lbl_80347FE0);
            yy += lh;
            DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, lbl_80347FE8);
        }
        showSel = 0;
        break;
    }
    case 11:
    case 12: {
        s32 st = *(s32*)(pl + 13128);
        s32 y = lbl_80343DD8 - lh * 5;
        s32 nx;
        s32 yy;
        s32 fr;
        if (st == 0) goto cr_none;
        if (st < 0) goto cr_result;
        if (st >= 4) goto cr_result;
        goto cr_prog;
cr_none:
        if ((s32)mode == 11) {
            nx = -(x + 64);
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 428);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FAC);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FC8);
            y += lh;
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 520);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FB4);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 536);
            y += lh;
            sprintf(buf, pool + 548, lbl_80344BB8 / 8192);
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, buf);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 560);
        } else {
            nx = -(x + 64);
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 348);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 360);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FAC);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FC8);
            y += lh;
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FF0);
            y += lh;
            sprintf(buf, pool + 548, lbl_80344BB8 / 8192);
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, buf);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 560);
            y += lh;
            y += lh;
            fr = saveGetFreeBytes(*(s32*)(pl + 13132),
                                  *(s32*)(pl + 13136));
            sprintf(buf, pool + 572, fr / 8192);
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, buf);
        }
        break;
cr_prog:
        nx = -(x + 64);
        yy = lbl_80343DD8;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 588);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 600);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 612);
        yy += lh;
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 304);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 480);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, lbl_80347FD8);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, lbl_80347FC0);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 492);
        showSel = 0;
        break;
cr_result:
        if (st < 1000) goto cr_chk2;
        nx = -(x + 64);
        yy = lbl_80343DD8;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 628);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 644);
        goto cr_done;
cr_chk2:
        if (st < 0) goto cr_fail;
        nx = -(x + 64);
        yy = lbl_80343DD8;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 656);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 668);
        goto cr_done;
cr_fail:
        nx = -(x + 64);
        yy = lbl_80343DD8;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, lbl_80347FF0);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 656);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, lbl_80347FE8);
cr_done:
        showSel = 0;
        break;
    }
    case 9:
    case 14: {
        s32 st = *(s32*)(pl + 13128);
        s32 w = (mode == 14);
        s32 nx;
        s32 yy;
        if (st == 0) goto io_none;
        if (st < 0) goto io_result;
        if (st >= 4) goto io_result;
        goto io_prog;
io_none:
        if (w == 0) goto io_prog;
        nx = -(x + 64);
        yy = lbl_80343DD8;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 680);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 692);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, lbl_80347FF8);
        break;
io_prog:
        nx = -(x + 64);
        yy = lbl_80343DD8;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF,
                          (w != 0) ? pool + 704 : pool + 716);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, lbl_80347FAC);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, lbl_80348000);
        yy += lh;
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 304);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 480);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, lbl_80347FD8);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, lbl_80347FC0);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 492);
        showSel = 0;
        break;
io_result:
        if (st < 0) goto io_neg;
        nx = -(x + 64);
        yy = lbl_80343DD8;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF,
                          (w != 0) ? lbl_80347F14 : lbl_80347F0C);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 732);
        goto io_done;
io_neg:
        if (st >= -1) goto io_done;
        nx = -(x + 64);
        yy = lbl_80343DD8;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF,
                          (w != 0) ? lbl_80347F14 : lbl_80347F0C);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, lbl_80347FE8);
io_done:
        showSel = 0;
        break;
    }
    case 4: {
        s32 y = lbl_80343DD8;
        if (gDemoMode != 0) {
            s32 nx = -(x + 64);
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 744);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 760);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 772);
        } else {
            s32 nx = -(x + 64);
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 220);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 244);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 784);
        }
        break;
    }
    }

    if (showSel != 0) {
        s32 bx = *xp + 20;
        MBNewTempBlit(lbl_80344E48, bx, 252, sz, sz2);
        DrawTextKeepScale(scale, bx + (sz + 8), 256, font, 0xFFFFFF,
                          lbl_80348008);
    }
    if (showBack != 0) {
        s32 by;
        s32 bx;
        if (showBack == 2) {
            by = 252 - (lh + 10);
        } else {
            by = lh + 262;
        }
        bx = *xp + 20;
        MBNewTempBlit(lbl_80344E44, bx, by, sz, sz2);
        DrawTextKeepScale(scale, bx + (sz + 8), by + 4, font, 0xFFFFFF,
                          lbl_80348010);
    }
}

void init_player_change(s32 idx, s32 arg1)
{
    u8* pl = gPlayers + idx * 0x335C;
    int i;
    u8* p;
    s32 v;
    s32 saved;
    s32 wflag;

    *(s32*)(pl + 0xE8) = 3;

    p = gPlayers;
    for (i = 0; i < 4; i++, p += 0x335C) {
        if (i != idx) {
            s32 st = *(s32*)(p + 0xE8);
            if (st == 1 || (u32)(st - 4) <= 1) {
                v = *(s32*)(p + 0x830);
                goto gotv;
            }
        }
    }
    v = *(s32*)(gPlayers + idx * 0x335C + 0x830);
gotv:
    *(s32*)(pl + 0x830) = v;

    saved = *(s32*)(pl + 0xF0);
    change_player(idx, arg1);
    *(s32*)(pl + 0xF0) = saved;

    setup_tex(idx, 2, 0, 0, lbl_801144A0, lbl_801200B0[arg1 & 7]);

    mbBlitProject(*(void**)((u8*)lbl_80284878 + idx * 132 + 24), -1, 320);

    wflag = *(u32*)(pl + 0xF0) ? 0 : 1;
    if (*(s32*)(pl + 0x1EC0) == 0) {
        AudioWelcomeBack(idx, wflag);
    } else {
        AudioWelcome(idx, wflag);
    }
}

int setup_file_entries(u8* pl, s32 fromLoad)
{
    s32 ok = 1;
    s32 a;
    s32 b;
    s32 state;
    s32 count;
    u8* row;
    u8* eb;
    u8* e;
    s32 k;
    s32 nameOff;
    s32 entOff;

    a = *(s32*)(pl + 0x334C);
    b = *(s32*)(pl + 0x3350);
    state = *(s32*)((u8*)&lbl_80344A18 + a * 4 + b * 4);
    if (state == 1) {
        return -1;
    }
    if (state == 3 &&
        *(s32*)((u8*)&lbl_80344A14 + a * 4 + b * 4) == 1) {
        count = get_vmu_directory(a, b);
        if (count < 0) {
            return 0;
        }
        row = lbl_80274578 + a * 132 + b * 132;
        eb = lbl_80284A88;
        k = 0;
        nameOff = 0;
        entOff = 0;
        for (; k < count; k++, nameOff += 16, entOff += 36) {
            e = eb + *(s32*)pl * 324 + entOff;
            *(u32*)e = (u32)(row + nameOff + 8);
            *(s32*)(e + 4) = k + 1000;
            *(s32*)(e + 32) = 0;
            *(s32*)(e + 8) = 4;
            if (fromLoad == 0) {
                if (strncmp((char*)*(u32*)e, lbl_80348018, 5) == 0) {
                    *(s32*)(e + 32) = -1;
                } else {
                    ok = 0;
                }
            }
            if (verify_vmu_file_ok(pl, k) == 0) {
                *(s32*)(e + 32) = -1;
            }
            if (*(s32*)(e + 32) == 0) {
            }
        }
        *(u32*)(lbl_80284A88 + *(s32*)pl * 324 + k * 36) = 0;
        if (fromLoad == 0 && ok != 0) {
            return 0;
        }
        return 1;
    }
    return -2;
}

int verify_vmu_file_ok(u8* pl, s32 v)
{
    int i;
    s32 a = *(s32*)(pl + 0x334C);
    s32 b = *(s32*)(pl + 0x3350);
    for (i = 0; i < 4; i++) {
        u8* p = gPlayers + i * 0x335C;
        if (p != pl && *(s32*)(p + 0xE8) != 0 &&
            *(s32*)(p + 0x334C) == a && *(s32*)(p + 0x3350) == b &&
            *(s32*)(p + 0x3358) == v) {
            return 0;
        }
    }
    return 1;
}

void setup_vmu_entries(void)
{
    u8* base = lbl_80284878;
    char* buf = (char*)(base + 0x744);
    int idx = 0;
    int n = 0;

    if (lbl_80344A18 == 3) {
        if (lbl_80344610 == 2) {
            sprintf(buf, lbl_80114718, 1, (s8)lbl_80343DEC);
        } else {
            if (n > 0) {
                goto end;
            }
            sprintf(buf, lbl_80114724, 1, 0);
        }
        *(u32*)(base + 0x720) = (u32)buf;
        *(s32*)(base + 0x724) = 1000;
        if (lbl_80344A14 == 1) {
            *(s32*)(base + 0x740) = 0;
        } else {
            *(s32*)(base + 0x740) = -1;
        }
        *(s32*)(base + 0x728) = 4;
        idx = 1;
    }
end:
    *(s32*)(base + idx * 36 + 0x720) = 0;
}

static s32 sel_set_choice(s32 player, s32 mode);

#pragma opt_propagation off
void setup_sel_menu(s32 player, s32 mode)
{
    u8* bss = lbl_80284878;
    u8* data = lbl_80121688;
    u8* menu;
    s32 playerOffset;
    s32 baseChoice;

    ClearPlayerControl(player, 2);
    playerOffset = player * 232;
    menu = data + playerOffset + 712;
    memcpy(menu, data + 1640, 232);

    baseChoice = ((s32*)data)[player];
    *(s32*)(data + playerOffset + 724) -= baseChoice;
    *(s32*)menu = mode;

    switch (mode) {
    case 0:
        *(void**)(data + playerOffset + 740) = data + 280;
        *(s32*)(data + playerOffset + 828) = sel_set_choice(player, mode);
        break;
    case 1:
        *(void**)(data + playerOffset + 740) = data + 388;
        *(s32*)(data + playerOffset + 828) = sel_set_choice(player, mode);
        break;
    case 15:
        *(void**)(data + playerOffset + 740) = data + 604;
        *(s32*)(data + playerOffset + 828) = 1;
        *(s32*)(data + playerOffset + 728) = lbl_80343DD8 + 64;
        break;
    case 5:
    case 10: {
        u8* entries = bss + 1824;
        s32 sum;
        s32 off;
        s32 i;
        s32* selected = (s32*)(data + playerOffset + 828);

        *(void**)(data + playerOffset + 740) = entries;
        *(s32*)(data + playerOffset + 724) = baseChoice + 4;
        *(s32*)(data + playerOffset + 728) = 70;
        *(f32*)(data + playerOffset + 764) = lbl_80348020;
        *selected = 0;
        sum = *(s32*)(gPlayers + player * 0x335C + 0x334C) +
              *(s32*)(gPlayers + player * 0x335C + 0x3350) + 1000;
        for (i = 0, off = 0; *(s32*)(entries + off) != 0; i++, off += 36) {
            if (sum == *(s32*)(entries + off + 4)) {
                *selected = i;
                break;
            }
        }
        break;
    }
    case 8:
    case 13:
        *(void**)(data + playerOffset + 740) = bss + player * 324 + 528;
        *(s32*)(data + playerOffset + 724) = baseChoice + 8;
        *(s32*)(data + playerOffset + 728) = 70;
        *(f32*)(data + playerOffset + 764) = lbl_80348020;
        *(s32*)(data + playerOffset + 828) = *(s32*)(gPlayers + player * 0x335C + 0x3358);
        if (*(s32*)(data + playerOffset + 828) < 0) {
            *(s32*)(data + playerOffset + 828) = 0;
        }
        break;
    }

    start_optmenu_nostack(menu, player);
}
#pragma opt_propagation reset

void sel_set_inactive(s32 slot)
{
    lbl_80121950[slot].state = 0;
}

static s32 sel_set_choice(s32 player, s32 mode)
{
    u8* menu = (u8*)&lbl_80121950[player];
    u8* pl = gPlayers + player * 0x335C;
    u8* e;
    s32 best = -1;
    s32 i = 0;
    s32 off = 0;
    u32 owner;

    for (;;) {
        e = *(u8**)(menu + 28) + off;
        if (*(u32*)e == 0) {
            break;
        }
        switch (*(s32*)(e + 4)) {
        case 1000:
            *(s32*)(e + 32) = 0;
            break;
        case 1001:
            if (gDemoMode != 0) {
                *(s32*)(e + 32) = -1;
            } else if (lbl_803448AC == 8 && lbl_803448A8 == 3) {
                *(s32*)(e + 32) = -1;
            } else if (vmu_directory_exists() >= 1) {
                *(s32*)(e + 32) = 0;
            } else {
                *(s32*)(e + 32) = -1;
            }
            break;
        case 1002:
            if (gDemoMode != 0) {
                *(s32*)(e + 32) = -1;
            } else if (vmu_directory_exists() >= 1) {
                *(s32*)(e + 32) = 0;
            } else {
                *(s32*)(e + 32) = -1;
            }
            break;
        case 1004:
            if (lbl_803448AC == 8 && lbl_803448A8 == 3) {
                *(s32*)(e + 32) = -1;
            } else {
                *(s32*)(e + 32) = 0;
            }
            break;
        case 1003:
            if (lbl_803448AC == 8 && lbl_803448A8 == 3) {
                *(s32*)(e + 32) = -1;
            } else {
                owner = *(u32*)(pl + 240);
                if (owner != 0 && owner != lbl_80343D6C) {
                    *(s32*)(e + 32) = -1;
                } else {
                    *(s32*)(e + 32) = 0;
                }
            }
            break;
        case 1005:
            *(s32*)(e + 32) = 0;
            break;
        }
        if (*(s32*)(e + 32) >= 0 && best < 0) {
            best = i;
        }
        i++;
        off += 36;
    }
    return best;
}

s32 other_players_next_level(s32 idx)
{
    s32 st;
    u8* p;
    int i;

    p = gPlayers;
    for (i = 0; i < 4; i++, p += 0x335C) {
        if (i != idx) {
            st = *(s32*)(p + 0xE8);
            if (st == 1 || (u32)(st - 4) <= 1) {
                return *(s32*)(p + 0x830);
            }
        }
    }
    p = gPlayers;
    p += idx * 0x335C;
    return *(s32*)(p + 0x830);
}

int check_active_players(void)
{
    int i;
    u8* p;
    int count = 0;
    new_menu_accept(-1, 1);
    p = gPlayers;
    for (i = 0; i < 4; i++, p += 0x335C) {
        if (*(s32*)(p + 0xE8) == 0 && (lbl_80344824 & (1 << i))) {
            new_player(i);
            count++;
        }
    }
    return count;
}

/* Poll the async tower/geometry load: 0 while loading, 1 when it just
 * finished, 2 once already complete. */
int SelectLoadDone(void)
{
    int result;

    if (lbl_80344BC0 != 0) {
        goto already_done;
    }
    if (MBOX_BGLoadModelDone() == 0) {
        goto loading;
    }
    fn_8005403C(2);
    lbl_80344BC0 = 1;
    result = 1;
    goto done;

loading:
    result = 0;
    goto done;
already_done:
    result = 2;
done:
    return result;
}

/* Begin the async tower/geometry load for the select scene. */
void SelectLoadStart(void)
{
    if (lbl_80343DD4 < 0) {
        lbl_80344BC0 = 0;
        lbl_80343DD4 = MBOX_BGLoadModelStart(lbl_80348024, -1);
    }
}

extern s32 lbl_80343DB8;
extern s32 lbl_80343DC0;
extern f32 lbl_80343DC4;
extern s32 lbl_80343DC8;
extern s32 lbl_80343DCC;
extern f32 lbl_80343DD0;
extern s32 lbl_80344BBC;
extern u8* lbl_80282930[];
extern char lbl_8034802C[8];
extern char lbl_80348034[4];
extern s32 ExpToLevel(s32 exp);
extern void LoadPlyrData(s32 player, s32 pad, s32 mode);
extern void* MBOX_FindTexture(char* name, s32 mode);

void update_class_attr(s32 player)
{
    u8* pl = gPlayers + player * 13148;
    char* pool = lbl_801143F8;
    s32 stats[4];
    char buf[40];
    u8* expslot;
    s32 lvl;
    s32 best;
    s32 j;

    LoadPlyrData(player, *(s32*)(pl + 12), 0);
    if (*(s32*)(pl + 13092) <= 0) {
        *(s32*)(pl + 13092) = 1;
    }
    if (*(s32*)(pl + 232) != 2) {
        return;
    }
    switch (*(s32*)(pl + 13112)) {
    case 4: {
        s32 sel = *(s32*)(pl + 16);
        s32 avail;
        s32* xp;
        s32 tx;
        s32 y;
        s32 row;
        s32 soff;
        f32 kScale;
        if (sel < 8) {
            avail = 1;
        } else if (*(u16*)(pl + 2700) & (1 << (sel - 8))) {
            avail = 1;
        } else {
            avail = 0;
        }
        if (avail == 0) {
            return;
        }
        if (sel == 16) {
            expslot = 0;
            for (j = 0; j < 4; j++) {
                stats[j] = 999;
            }
            lvl = 99;
            best = -1;
        } else {
            expslot = pl + sel * 24 + 2704;
            lvl = ExpToLevel(*(s32*)expslot);
            LoadPlyrData(player, *(s32*)(pl + 16), 0);
            {
                u8* cls = lbl_80282930[player];
                stats[0] = (s32)(*(f32*)(expslot + 8) +
                                 (*(f32*)(cls + 40) +
                                  (f32)((lvl - 1) * 5)));
                stats[1] = (s32)(*(f32*)(expslot + 20) +
                                 (*(f32*)(cls + 48) +
                                  (f32)((lvl - 1) * 5)));
                stats[2] = (s32)(*(f32*)(expslot + 12) +
                                 (*(f32*)(cls + 56) +
                                  (f32)((lvl - 1) * 5)));
                stats[3] = (s32)(*(f32*)(expslot + 16) +
                                 (*(f32*)(cls + 64) +
                                  (f32)((lvl - 1) * 5)));
            }
            for (j = 0; j < 4; j++) {
                s32 v = stats[j];
                if (v < 999) {
                } else {
                    v = 999;
                }
                stats[j] = v;
            }
            {
                s32 mx = 0;
                best = 0;
                for (j = 0; j < 4; j++) {
                    if (stats[j] > mx) {
                        mx = stats[j];
                        best = j;
                    }
                }
            }
        }
        xp = (s32*)(lbl_80121688 + (player << 2));
        kScale = lbl_80343DC4;
        tx = *xp + 81;
        y = *((s32*)&lbl_80343DB8 + 1);
        soff = 0;
        for (row = 0; row < 4; row++, y += lbl_80343DC8, soff += 4) {
            char* name = GetStringText(167, row, 0);
            s32 w = DrawNormalText(kScale, name, 6);
            s32 vx;
            if (best == row) {
                DrawGlowText(kScale, tx - w, y - 2, name);
            } else {
                DrawTextKeepScale(kScale, tx - w, y - 2, 6, 0xFFFFFF,
                                  name);
            }
            vx = lbl_80343DB8 + *xp;
            sprintf(buf, lbl_8034802C, *(s32*)((u8*)stats + soff));
            if (best == row) {
                MBNewTempBlit(MBOX_FindTexture(pool + 824, 0), vx - 6,
                              y - 6, 68, -1);
            }
            DrawTextKeepScale(kScale, vx, y, lbl_80343DC0, 0xFFFFFF, buf);
        }
        {
            s32 y2 = lbl_80343DCC;
            s32 x2 = -(*xp + 64);
            if (expslot != 0 && *(s32*)expslot > 0) {
                sprintf(buf, pool + 836, lvl);
            } else {
                sprintf(buf, lbl_80348034);
            }
            DrawTextKeepScale(lbl_80343DD0, x2, y2, lbl_80344BBC, 0xFFFFFF,
                              buf);
        }
        break;
    }
    case 1: {
        s32 y2 = lbl_80343DCC;
        s32 x2 = -(*(s32*)(lbl_80121688 + (player << 2)) + 64);
        sprintf(buf, pool + 836, ExpToLevel(*(s32*)(pl + 7872)));
        DrawTextKeepScale(lbl_80343DD0, x2, y2, lbl_80344BBC, 0xFFFFFF,
                          buf);
        break;
    }
    }
}

void update_class_spec(s32 player)
{
    char* pool = lbl_801143F8;
    u8* blitBase = lbl_80284878;
    u8* pl = gPlayers + player * 0x335C;
    s32 boff;
    u8* eWeap;
    u8* eA;
    u8* eB;
    u8* eC;
    u8* eD;
    s32 state;
    s32 cls;
    s32 spec;
    s32 known;
    char* texName;
    char* extra;
    char* qfmt;
    u8 unused[48];
    StrBlock4 tmp;

    PlayerModel(player);
    setup_player_display(player);
    setup_tex(player, 2, 0, 0, pool + 168,
              lbl_801200B0 + (*(s32*)(pl + 12) & 7) * 4);
    boff = player * 132;
    eWeap = blitBase + boff;
    mbBlitProject(*(void**)(eWeap += 24), -1, 320);
    eA = blitBase + boff;
    mbBlitInit3414(*(void**)(eA += 84), 1);
    eB = blitBase + boff;
    mbBlitInit3414(*(void**)(eB += 72), 1);
    eC = blitBase + boff;
    mbBlitInit3414(*(void**)(eC += 60), 1);
    eD = blitBase + boff;
    mbBlitInit3414(*(void**)(eD += 96), 1);

    if (gGameMode == 0x400B) {
        state = *(s32*)(pl + 232);
        if (state != 3) {
            if (state == 2) {
                goto substate;
            }
            hide_select_blits(player, 0);
        }
    }
    return;

substate:
    switch (*(s32*)(pl + 13112)) {
    case 2:
        break;
    case 1:
        cls = *(s32*)(pl + 12);
        if (cls == 2 && *(u32*)(pl + 240) == lbl_80343D6C) {
            setup_tex(player, 8, 0, 0, pool + 232);
        } else {
            setup_tex(player, 8, 0, 0, lbl_80347F58,
                      lbl_801200B0 + cls * 4);
        }
        break;
    case 0:
    case 3:
        hide_select_blits(player, 0);
        break;
    case 4:
        tmp = *(StrBlock4*)(pool + 80);
        mbBlitInit3414(*(void**)eWeap, 1);
        spec = *(s32*)(pl + 16);
        if (spec < 8) {
            known = 1;
        } else if ((*(u16*)(pl + 2700) & (1 << (spec - 8))) != 0) {
            known = 1;
        } else {
            known = 0;
        }
        if (known != 0) {
            texName = tmp.s[0];
            extra = lbl_80120104[*(s32*)(pl + 4)];
            qfmt = 0;
        } else {
            qfmt = pool + 848;
            extra = lbl_80347F44;
            texName = 0;
        }
        if (spec == 16) {
            setup_tex(player, 3, 0, 0, lbl_80347F4C);
        } else {
            setup_tex(player, 3, 0, 0, pool + 156,
                      lbl_801200B0 + spec * 4);
        }
        if (texName != 0) {
            (void)pbLoad;
            setup_tex(player, 8, 0, 0, lbl_80347F58,
                      lbl_801200B0 + *(s32*)(pl + 16) * 4);
        } else {
            mbBlitInit3414(*(void**)eB, 1);
            mbBlitInit3414(*(void**)eC, 1);
            mbBlitInit3414(*(void**)eD, 1);
        }
        if (qfmt != 0) {
            setup_tex(player, 7, 0, 0, qfmt);
            *(s32*)(blitBase + boff + 88) = 3;
            *(s32*)(blitBase + boff + 92) = 0;
        } else {
            mbBlitInit3414(*(void**)eA, 1);
        }
        mbBlitInit3414(*(void**)eWeap, 1);
        break;
    }
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

#pragma opt_propagation off
void hide_select_blits(s32 arg0, s32 flag)
{
    u8* pagebase;
    u8* entry;
    s32 pg;
    s32 j;
    s32 end;
    s32 start;
    void* handle;
    if (arg0 < 0) {
        start = 0;
        end = 3;
    } else {
        start = end = arg0;
    }
    for (pg = start; pg <= end; pg++) {
        pagebase = (u8*)lbl_80284878 + pg * 132;
        for (j = 0; j < 11; j++) {
            entry = pagebase + j * 12;
            if ((handle = *(void**)entry) != 0) {
                if (flag != 0 || (j > 1 && j < 9)) {
                    mbBlitInit3414(handle, 1);
                } else {
                    mbBlitInit3414(handle, 0);
                }
                *(s32*)(entry + 4) = 0;
            }
        }
    }
}
#pragma opt_propagation reset

void setup_tex(s32 id, s32 slot, s32 flags, s32 hide, char* fmt, ...)
{
    va_list ap;
    void** entry;
    void* tex;
    char buf[32];

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    tex = MBOX_FindTexture_Err(buf, 0, 1);
    entry = (void**)((u8*)lbl_80284878 + id * 132 + slot * 12);
    mbInitBlitEntry(*entry, (u32)tex, 0);
    mbBlitInit3414(*entry, hide);
    mbBlitProject(*entry, -1, -1);
    mbBlitUpdateEntry(*entry, -1, flags);
    MBBlitSetAlpha(*entry, 0);
    if (flags & 0x4000) {
        MBBlitSetColor4(*entry, 0x80808080, 0x80808080, 0x80808080, 0x80808080);
    }
}

void serve_blits(void)
{
    mbBlitInit3414(0, 0);
    mbBlitProject(0, 0, 0);
}
