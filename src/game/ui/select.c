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
#include "game/player.h"

/* Never cast a live u8* player-record pointer to Player* at these call
 * sites -- MWCC's addressing for a walked/multi-field-touching base
 * regresses under a typed alias (see claim.law.multifield-alias-defeats-
 * indexed-addressing) while a bare offsetof()-computed displacement on the
 * raw pointer constant-folds to the identical literal pre-codegen (see
 * claim.law.offsetof-fused-immediate-counter). offsetof() is only used to
 * name a verified include/game/player.h field; never introduce a Player*
 * lvalue alongside it. */
#ifndef offsetof
#define offsetof(type, memb) ((u32) & ((type*)0)->memb)
#endif

/* ---- boss-requirement table (this TU, .data 0x80121DD8, 12 x 0x24) ---- */
typedef struct SelOptsView { u8 _pad[44]; u32 flags44; } SelOptsView;
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
extern char lbl_80347F44[8];  /* "?" texture name (sdata)              */
extern char lbl_80347F4C[12];  /* unarmed spec label fmt (sdata)        */
extern char lbl_80347F58[8];  /* "%s NAME" fmt (sdata)                 */
extern volatile u32 pbLoad;
void PlayerModel(s32 player);
void setup_player_display(s32 player);
void hide_select_blits(s32 arg0, s32 flag);

typedef struct StrBlock4 {
    char* s[4];
} StrBlock4;
extern s32 lbl_80344610[2]; /* mtap port status */
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
extern void init_attract_mode(s32 mode);
extern void LoadTowerAndSelect();
extern void remove_player_geo(int a);
extern void msgInit(void);
extern int  saveFileSize();
extern void AudioSelect(s32 code);
extern void AudioCursorSelect(void);
extern void AudioCursorChar(void);
extern void AudioBuzzer(void);

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
extern void LockModels(int a);
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

static void do_sel_menu_8008E4F4(s32 player, u32 mode);

/* ---- extra externs / protos used by do_player_select ---- */
extern s32 lbl_80344BB0;   /* select-audio countdown */
extern s32 lbl_80344BA8;   /* leave-select flag */
extern s32 lbl_80344BAC;   /* all-idle frame counter */
extern s32 lbl_80344BB4;   /* save-list debounce timer */
extern s32 lbl_80344BB8;   /* required save file size */
extern s32 lbl_803445DC;   /* in-select flag */
extern s32 sFlags;         /* demo / cpu-select flags */
extern s64 gControllerButtons; /* 64-bit; low half aliases sFlags */
extern s32 gClockStepTicks;
extern s32 gFrameTicks;
extern f32 lbl_80347F3C;   /* "Loading..." text scale */
extern f32 lbl_80347F54;   /* "Press START" text scale */
extern char lbl_80347F40[4]; /* auto-select save name */
extern u8 lbl_80240E30[];  /* pad states, 4 x 0x3C, buttons at +8 */
extern void show_optmenu();
extern s32 do_optmenu(void* menu, s32 serve);
extern void remove_optmenu(void* menu);
extern s32 saveExists(void);
extern char* strcpy(char* dst, const char* src);
extern void clear_player(s32 i, s32 mode);
extern void abort_player(s32 i);
extern s32 set_hidden_player(u8* pl);
extern s32 fn_8005AC10(s32 i);  /* name-entry open */
extern s32 fn_8005A738(s32 i);  /* name-entry serve; 1 = done */
extern s32 saveMount(s32 chan, s32 slot, s32 mode);
extern u8 MemCardCreateGaunt(s32 chan, s32 slot);
extern void set_directory_refresh_flags(s32 mask);
extern s32 fn_80055F68(s32 a, s32 b);  /* async card-op poll */
extern s32 PlayerLoadSaveFile(s32 i, s32 file);
extern s32 PlayerWriteSaveFile(s32 i, s32 file);
extern void add_vmu_file(s32 chan, s32 slot, s32 file, char* name, s32 flags,
                         s32 cls);
extern s32 ShowLoading(void);   /* front-end mode poll */
extern void EndTower(void);  /* front-end teardown */
extern void WritePlayerInfo(s32 i);
extern s32 saveGetFreeBytes(s32 chan, s32 handle);
extern void LoadPlyrData(s32 player, s32 pad, s32 mode);

void setup_sel_menu(s32 player, s32 mode);
void init_player_change(s32 idx, s32 arg1);
int setup_file_entries(u8* pl, s32 fromLoad);
int verify_vmu_file_ok(u8* pl, s32 v);
s32 other_players_next_level(s32 idx);
void setup_vmu_entries(void);
s32 serve_blits(s32 player);
void update_class_attr(s32 player);
void update_class_spec(s32 player);

/* select-time snapshot of the persistent player block (0xA80..0x1EB4),
 * struct-assigned into the save shadow at +0x1ECC */
typedef struct SaveSnap {
    s32 w[0x50D];
} SaveSnap;

#pragma dont_inline on
/* Top-level select state machine (invoked from gamemain / attract).
 * Returns 1 once every pad has sat idle long enough to leave the screen. */
s32 do_player_select(void)
{
    u8* blitbase = lbl_80284878;
    char* pool = lbl_801143F8;
    u8* page = lbl_80121688;
    s32 allIdle = 1;
    s32 anySelecting = 0;
    s32 servedMask = 0;
    s32 i;
    u8* pl;
    s32 padoff;
    s32 xoff;
    s32 poff;
    s32 boff;
    s32 moff;
    s32 choice;
    u8 unused[24];

    if (lbl_80344BB0 > 0) {
        lbl_80344BB0--;
        if (lbl_80344BB0 == 0) {
            AudioSelect(1);
        } else {
            DrawGlowText(lbl_80347F3C, 0x154, 0x104, pool + 144);
        }
    }
    lbl_803445DC = 1;
    new_menu_accept(-1, 1);

    pl = gPlayers;
    for (i = 0; i < 4; i++, pl += 13148) {
        if (*(s32*)(pl + offsetof(Player, state)) == 0 && (lbl_80344824 & (1 << i))) {
            new_player(i);
        }
    }
    setup_vmu_entries();

    pl = gPlayers;
    for (i = 4; i != 0; i--, pl += 13148) {
        switch (*(s32*)(pl + offsetof(Player, state))) {
        case 2:
            anySelecting = 1;
            allIdle = 0;
            break;
        case 3:
            anySelecting = 1;
            break;
        }
    }
    if (lbl_80344824 == 0) {
        lbl_80344BA8 = 1;
    }
    if (anySelecting) {
        for (i = 0; i < 4; i++) {
            if (serve_blits(i) != 0) {
                servedMask |= 1 << i;
            }
        }
        if (servedMask != 0) {
            allIdle = 0;
        }
    } else {
        hide_select_blits(-1, 1);
    }
    if (lbl_80344BB4 > 0) {
        lbl_80344BB4 -= gClockStepTicks;
        if (lbl_80344BB4 < 0) {
            lbl_80344BB4 = 0;
        }
    }

    padoff = 0;
    xoff = 0;
    poff = 0;
    boff = 0;
    moff = 0;
    pl = gPlayers;
    for (i = 0; i < 4; i++, padoff += 60, poff += 13148, xoff += 4,
        boff += 132, moff += 232, pl += 13148) {
        s32 costume = *(s32*)(pl + offsetof(Player, class_id));
        s32 st;
        u8* menu;

        if (lbl_80344BB0 != 0) {
            allIdle = 0;
            continue;
        }
        if (lbl_80344BA8 != 0) {
            continue;
        }
        st = *(s32*)(pl + offsetof(Player, state));
        switch (st) {
        case 2:
            switch (*(u32*)(pl + offsetof(Player, motion_state))) {
            case 0: /* top select menu */
                *(s32*)(pl + 0x333C) = *(s32*)(pl + offsetof(Player, motion_state));
                menu = page + moff + 712;
                if (gControllerButtons & 4) {
                    *(s32*)(pl + offsetof(Player, state)) = 3;
                    strcpy((char*)(pl + offsetof(Player, name)), lbl_80347F40);
                } else {
                    if (*(s32*)(menu + 108) == 0) {
                        setup_sel_menu(i, 0);
                        if (vmu_directory_exists() >= 2 && saveExists() != 0) {
                            u8* base = *(u8**)(menu + 28);
                            s32 k = 0;
                            s32 off = k;
                            s32 found = -1;
                            for (;;) {
                                u8* en = base + off;
                                if (*(u32*)en == 0) {
                                    break;
                                }
                                if (*(s32*)(en + 4) == 1001) {
                                    if (*(s32*)(en + 32) >= 0) {
                                        found = k;
                                        break;
                                    }
                                }
                                k++;
                                off += 36;
                            }
                            if (found >= 0) {
                                *(s32*)(menu + 116) = found;
                            }
                        }
                    }
                    show_optmenu(menu);
                    choice = do_optmenu(menu, 1);
                    do_sel_menu_8008E4F4(i, 0);
                    switch (choice) {
                    case -1:
                        remove_optmenu(menu);
                        if ((lbl_80344824 & ~(1 << i)) != 0) {
                            abort_player(i);
                        } else {
                            clear_player(i, 1);
                            lbl_80344BA8 = 1;
                        }
                        break;
                    case 1000:
                        remove_optmenu(menu);
                        *(s32*)(pl + 0x3348) = 1;
                        *(s32*)(pl + offsetof(Player, motion_state)) = 3;
                        AudioCursorSelect();
                        break;
                    case 1001:
                        remove_optmenu(menu);
                        *(s32*)(pl + 0x3348) = 1;
                        *(s32*)(pl + offsetof(Player, motion_state)) = 5;
                        AudioCursorSelect();
                        break;
                    }
                }
                break;
            case 1: { /* load/save menu */
                *(s32*)(pl + 0x333C) = *(s32*)(pl + offsetof(Player, motion_state));
                menu = page + moff + 712;
                if (*(s32*)(menu + 108) == 0) {
                    setup_sel_menu(i, 1);
                    if (vmu_directory_exists() < 2 || saveExists() == 0) {
                        u8* base = *(u8**)(menu + 28);
                        s32 k = 0;
                        s32 off = k;
                        s32 found = -1;
                        for (;;) {
                            u8* en = base + off;
                            if (*(u32*)en == 0) {
                                break;
                            }
                            if (*(s32*)(en + 4) == 1005) {
                                if (*(s32*)(en + 32) >= 0) {
                                    found = k;
                                    break;
                                }
                            }
                            k++;
                            off += 36;
                        }
                        if (found >= 0) {
                            *(s32*)(menu + 116) = found;
                        }
                    }
                    if (lbl_803448AC == 8 && lbl_803448A8 == 3) {
                        s32 off2;
                        off2 = 0;
                        for (;;) {
                            u8* en = *(u8**)(menu + 28) + off2;
                            if (*(u32*)en == 0) {
                                break;
                            }
                            if (*(s32*)(en + 4) == 1003) {
                                if (*(s32*)(en + 32) >= 0) {
                                    *(s32*)(en + 32) = -1;
                                }
                            }
                            off2 += 36;
                        }
                        off2 = 0;
                        for (;;) {
                            u8* en = *(u8**)(menu + 28) + off2;
                            if (*(u32*)en == 0) {
                                break;
                            }
                            if (*(s32*)(en + 4) == 1001) {
                                if (*(s32*)(en + 32) >= 0) {
                                    *(s32*)(en + 32) = -1;
                                }
                            }
                            off2 += 36;
                        }
                        off2 = 0;
                        for (;;) {
                            u8* en = *(u8**)(menu + 28) + off2;
                            if (*(u32*)en == 0) {
                                break;
                            }
                            if (*(s32*)(en + 4) == 1004) {
                                if (*(s32*)(en + 32) >= 0) {
                                    *(s32*)(en + 32) = -1;
                                }
                            }
                            off2 += 36;
                        }
                    }
                }
                show_optmenu(menu);
                choice = do_optmenu(menu, 1);
                do_sel_menu_8008E4F4(i, 1);
                switch (choice) {
                case 1005:
                    AudioCursorSelect();
                    remove_optmenu(menu);
                    *(s32*)(pl + offsetof(Player, state)) = 3;
                    break;
                case 1002:
                    remove_optmenu(menu);
                    *(s32*)(pl + offsetof(Player, motion_state)) = 10;
                    break;
                case 1001:
                    remove_optmenu(menu);
                    if (*(s8*)(pl + 0xA8B) != 0) {
                        *(s32*)(pl + 0x3348) = 1;
                    } else {
                        *(s32*)(pl + 0x3348) = 0;
                    }
                    *(s32*)(pl + offsetof(Player, motion_state)) = 5;
                    break;
                case 1003: /* resume character */
                    remove_optmenu(menu);
                    if (set_hidden_player(pl) != 0) {
                        s32 pi = *(s32*)pl;
                        u8* b;
                        init_player_change(pi, *(s32*)(pl + offsetof(Player, character)));
                        b = blitbase + pi * 132;
                        *(s32*)(b + 0x1C) = 5;
                        *(s32*)(b + 0x20) = 0;
                        mbBlitInit3414(*(void**)(blitbase + (pi * 132 + 24)),
                                       0);
                        MBBlitSetAlpha(*(void**)(blitbase + (pi * 132 + 24)),
                                       0xFF);
                        *(s32*)(b + 0x28) = 7;
                        *(s32*)(b + 0x2C) = 0;
                        *(s32*)(b + 0x4C) = 1;
                        *(s32*)(b + 0x50) = 0;
                        *(s32*)(b + 0x40) = 1;
                        *(s32*)(b + 0x44) = 0;
                    } else {
                        *(s32*)(pl + offsetof(Player, respawn_char)) =
                            LimitSeltype(pl, *(s32*)(pl + offsetof(Player, character)), 0);
                        *(s32*)(pl + offsetof(Player, motion_state)) = 4;
                    }
                    break;
                case 1004: /* change character */
                    remove_optmenu(menu);
                    if (*(s8*)(pl + 0xA8B) != 0) {
                        *(s32*)(pl + 0x3348) = 1;
                    } else {
                        *(s32*)(pl + 0x3348) = 0;
                    }
                    *(s32*)(pl + offsetof(Player, motion_state)) = 2;
                    break;
                case -1:
                    break;
                }
                break;
            }
            case 2: /* change-character confirm */
                menu = page + moff + 712;
                switch (*(s32*)(pl + 0x3348)) {
                case 0:
                    if (*(s32*)(menu + 108) == 0) {
                        setup_sel_menu(i, 15);
                    }
                    show_optmenu(menu);
                    choice = do_optmenu(menu, 1);
                    do_sel_menu_8008E4F4(i, 4);
                    switch (choice) {
                    case -1:
                    case 1007:
                        remove_optmenu(menu);
                        *(s32*)(pl + offsetof(Player, motion_state)) = 1;
                        break;
                    case 1006:
                        remove_optmenu(menu);
                        *(s32*)(pl + 0x3348) = 1;
                        break;
                    }
                    break;
                case 1:
                default:
                    if ((lbl_80344824 & ~(1 << i)) != 0) {
                        abort_player(i);
                    } else {
                        clear_player(i, 1);
                        lbl_80344BA8 = 1;
                    }
                    break;
                }
                break;
            case 5: /* pick a memory card (load) */
                menu = page + moff + 712;
                switch (*(s32*)(pl + 0x3348)) {
                case 0:
                    if (*(s32*)(menu + 108) == 0) {
                        setup_sel_menu(i, 15);
                    }
                    show_optmenu(menu);
                    switch (do_optmenu(menu, 1)) {
                    case -1:
                    case 1007:
                        remove_optmenu(menu);
                        *(s32*)(pl + offsetof(Player, motion_state)) = *(s32*)(pl + 0x333C);
                        break;
                    case 1006:
                        remove_optmenu(menu);
                        *(s32*)(pl + 0x3348) = 1;
                        break;
                    }
                    do_sel_menu_8008E4F4(i, 5);
                    break;
                }
                if (*(s32*)(pl + 0x3348) != 0) {
                    if (*(s32*)(menu + 108) == 0) {
                        setup_sel_menu(i, 5);
                    }
                    *(s32*)(menu + 132) = 0;
                    show_optmenu(menu);
                    choice = do_optmenu(menu, 1);
                    if (vmu_directory_exists() <= 0) {
                        choice = -1;
                    }
                    do_sel_menu_8008E4F4(i, 5);
                    switch (choice) {
                    case -1:
                        remove_optmenu(menu);
                        *(s32*)(pl + offsetof(Player, motion_state)) = *(s32*)(pl + 0x333C);
                        break;
                    default:
                    if (choice >= 1000) {
                        s32 r;
                        set_directory_refresh_flags(-1);
                        remove_optmenu(menu);
                        *(s32*)(pl + 0x334C) = choice - 1000;
                        *(s32*)(pl + 0x3350) = 0;
                        r = setup_file_entries(pl, 1);
                        if (r == -1) {
                            s32 busy;
                            s32 qoff = 0;
                            s32 n;
                            for (n = 4; n != 0; n--, qoff += 13148) {
                                u8* q = gPlayers + qoff;
                                if (*(s32*)(q + offsetof(Player, state)) == 2 && pl != q &&
                                    *(s32*)(q + 0x334C) == *(s32*)(pl + 0x334C) &&
                                    *(s32*)(q + 0x3350) == *(s32*)(pl + 0x3350)) {
                                    switch (*(s32*)(q + offsetof(Player, motion_state))) {
                                    case 6:
                                    case 11:
                                    case 12:
                                        busy = 1;
                                        goto got5;
                                    }
                                }
                            }
                            busy = 0;
                            got5:;
                            if (busy) {
                                *(s32*)(pl + offsetof(Player, motion_state)) = 5;
                            } else {
                                *(s32*)(pl + offsetof(Player, motion_state)) = 6;
                            }
                            *(s32*)(pl + 0x3348) = 0;
                        } else if (r <= 0) {
                            *(s32*)(pl + offsetof(Player, motion_state)) = 7;
                            *(s32*)(pl + 0x3348) = 1;
                        } else {
                            *(s32*)(pl + offsetof(Player, motion_state)) = 8;
                        }
                    }
                        break;
                    }
                }
                break;
            case 7: /* "no files" notice */
                do_sel_menu_8008E4F4(i, 7);
                *(s32*)(pl + 0x3348) += gFrameTicks;
                if (*(s32*)(pl + 0x3348) >= 0x78) {
                    *(s32*)(pl + offsetof(Player, motion_state)) = *(s32*)(pl + 0x333C);
                    *(s32*)(pl + 0x3348) = 0;
                }
                break;
            case 8: /* pick a save file (load) */
                menu = page + moff + 712;
                if (*(s32*)(menu + 108) == 0) {
                    setup_sel_menu(i, 8);
                }
                *(s32*)(menu + 132) = 0;
                show_optmenu(menu);
                choice = do_optmenu(menu, 1);
                if (*(s32*)((u8*)&lbl_80344A14 + *(s32*)(pl + 0x334C) * 4 +
                            *(s32*)(pl + 0x3350) * 4) != 1 ||
                    *(s32*)((u8*)&lbl_80344A18 + *(s32*)(pl + 0x334C) * 4 +
                            *(s32*)(pl + 0x3350) * 4) != 3) {
                    choice = -1;
                }
                setup_file_entries(pl, 0);
                do_sel_menu_8008E4F4(i, 8);
                switch (choice) {
                case -1:
                    remove_optmenu(menu);
                    *(s32*)(pl + offsetof(Player, motion_state)) = 5;
                    break;
                default:
                    if (choice >= 1000 &&
                        verify_vmu_file_ok(pl, choice - 1000) != 0) {
                        remove_optmenu(menu);
                        *(s32*)(pl + offsetof(Player, motion_state)) = 9;
                        *(s32*)(pl + 0x3354) = choice - 1000;
                        *(s32*)(pl + 0x3358) = choice - 1000;
                        *(s32*)(pl + 0x3348) = 0;
                    }
                    break;
                }
                break;
            case 9: { /* load-file progress */
                s32 t = *(s32*)(pl + 0x3348);
                switch (t) {
                case 0:
                case 1:
                case 2:
                    if (fn_80055F68(0, 0) != 0) {
                        *(s32*)(pl + 0x3348) += 1;
                    }
                    break;
                case 3:
                    if (PlayerLoadSaveFile(i, *(s32*)(pl + 0x3354)) != 0) {
                        *(s32*)(pl + 0x3348) += 1;
                    } else {
                        *(s32*)(pl + 0x3348) = -1;
                    }
                    break;
                default:
                if (t >= 0) {
                    *(s32*)(pl + 0x3348) += gFrameTicks;
                    if (*(s32*)(pl + 0x3348) >= 0x78) {
                        if (set_hidden_player(pl) != 0) {
                            s32 pi = *(s32*)pl;
                            u8* b;
                            init_player_change(pi, *(s32*)(pl + offsetof(Player, character)));
                            b = blitbase + pi * 132;
                            *(s32*)(b + 0x1C) = 5;
                            *(s32*)(b + 0x20) = 0;
                            mbBlitInit3414(
                                *(void**)(blitbase + (pi * 132 + 24)), 0);
                            MBBlitSetAlpha(
                                *(void**)(blitbase + (pi * 132 + 24)), 0xFF);
                            *(s32*)(b + 0x28) = 7;
                            *(s32*)(b + 0x2C) = 0;
                            *(s32*)(b + 0x4C) = 1;
                            *(s32*)(b + 0x50) = 0;
                            *(s32*)(b + 0x40) = 1;
                            *(s32*)(b + 0x44) = 0;
                        } else {
                            *(s32*)(pl + offsetof(Player, respawn_char)) =
                                LimitSeltype(pl, *(s32*)(pl + offsetof(Player, character)), 0);
                            *(s32*)(pl + offsetof(Player, motion_state)) = 4;
                        }
                        *(s32*)(pl + offsetof(Player, intower)) = 1;
                        *(s32*)(pl + 0x3348) = -1;
                    }
                } else {
                    *(s32*)(pl + 0x3348) -= gFrameTicks;
                    if (*(s32*)(pl + 0x3348) <= -0x78) {
                        *(s32*)(pl + offsetof(Player, motion_state)) = *(s32*)(pl + 0x333C);
                        *(s32*)(pl + 0x3348) = -1;
                    }
                }
                    break;
                }
                do_sel_menu_8008E4F4(i, 9);
                break;
            }
            case 10: /* pick a memory card (save) */
                menu = page + moff + 712;
                if (*(s32*)(menu + 108) == 0) {
                    setup_sel_menu(i, 10);
                }
                *(s32*)(menu + 132) = 0;
                show_optmenu(menu);
                choice = do_optmenu(menu, 1);
                if (vmu_directory_exists() <= 0) {
                    choice = -1;
                }
                do_sel_menu_8008E4F4(i, 10);
                switch (choice) {
                case -1:
                    remove_optmenu(menu);
                    *(s32*)(pl + offsetof(Player, motion_state)) = *(s32*)(pl + 0x333C);
                    break;
                default:
                if (choice >= 1000) {
                    s32 r;
                    set_directory_refresh_flags(-1);
                    remove_optmenu(menu);
                    *(s32*)(pl + 0x334C) = choice - 1000;
                    *(s32*)(pl + 0x3350) = 0;
                    r = setup_file_entries(pl, 1);
                    if (r == -2) {
                        remove_optmenu(menu);
                        *(s32*)(pl + offsetof(Player, motion_state)) = 10;
                    } else if (r == -1) {
                        s32 busy;
                        s32 qoff = 0;
                        s32 n;
                        for (n = 4; n != 0; n--, qoff += 13148) {
                            u8* q = gPlayers + qoff;
                            if (*(s32*)(q + offsetof(Player, state)) == 2 && pl != q &&
                                *(s32*)(q + 0x334C) == *(s32*)(pl + 0x334C) &&
                                *(s32*)(q + 0x3350) == *(s32*)(pl + 0x3350)) {
                                switch (*(s32*)(q + offsetof(Player, motion_state))) {
                                case 6:
                                case 11:
                                case 12:
                                    busy = 1;
                                    goto got6;
                                }
                            }
                        }
                        busy = 0;
                        got6:;
                        if (busy) {
                            *(s32*)(pl + offsetof(Player, motion_state)) = 10;
                        } else {
                            *(s32*)(pl + offsetof(Player, motion_state)) = 11;
                        }
                        *(s32*)(pl + 0x3348) = 0;
                    } else if (r == 0) {
                        s32 busy;
                        s32 qoff = 0;
                        s32 n;
                        for (n = 4; n != 0; n--, qoff += 13148) {
                            u8* q = gPlayers + qoff;
                            if (*(s32*)(q + offsetof(Player, state)) == 2 && pl != q &&
                                *(s32*)(q + 0x334C) == *(s32*)(pl + 0x334C) &&
                                *(s32*)(q + 0x3350) == *(s32*)(pl + 0x3350)) {
                                switch (*(s32*)(q + offsetof(Player, motion_state))) {
                                case 6:
                                case 11:
                                case 12:
                                    busy = 1;
                                    goto got10a;
                                }
                            }
                        }
                        busy = 0;
                        got10a:;
                        if (busy) {
                            *(s32*)(pl + offsetof(Player, motion_state)) = 10;
                        } else {
                            *(s32*)(pl + offsetof(Player, motion_state)) = 12;
                        }
                        *(s32*)(pl + 0x3348) = 0;
                    } else {
                        *(s32*)(pl + offsetof(Player, motion_state)) = 13;
                    }
                }
                    break;
                }
                break;
            case 6:  /* mount / create-file confirm + progress */
            case 11:
            case 12: {
                s32 t = *(s32*)(pl + 0x3348);
                menu = page + moff + 712;
                switch (t) {
                case 0:
                    if (*(s32*)(menu + 108) == 0) {
                        setup_sel_menu(i, 15);
                    }
                    show_optmenu(menu);
                    choice = do_optmenu(menu, 1);
                    if (*(s32*)((u8*)&lbl_80344A14 +
                                *(s32*)(pl + 0x334C) * 4 +
                                *(s32*)(pl + 0x3350) * 4) != 1) {
                        choice = -1;
                    }
                    if (setup_file_entries(pl, 1) > 0) {
                        choice = -1;
                    }
                    {
                        s32 busy;
                        s32 qoff = 0;
                        s32 n;
                        for (n = 4; n != 0; n--, qoff += 13148) {
                            u8* q = gPlayers + qoff;
                            if (*(s32*)(q + offsetof(Player, state)) == 2 && pl != q &&
                                *(s32*)(q + 0x334C) == *(s32*)(pl + 0x334C) &&
                                *(s32*)(q + 0x3350) == *(s32*)(pl + 0x3350)) {
                                switch (*(s32*)(q + offsetof(Player, motion_state))) {
                                case 6:
                                case 11:
                                case 12:
                                    busy = 1;
                                    goto got6b;
                                }
                            }
                        }
                        busy = 0;
                        got6b:;
                        if (busy) {
                            choice = -1;
                        }
                    }
                    switch (choice) {
                    case -1:
                    case 1007:
                        remove_optmenu(menu);
                        if (*(s32*)(pl + offsetof(Player, motion_state)) == 6) {
                            *(s32*)(pl + offsetof(Player, motion_state)) = 5;
                            *(s32*)(pl + 0x3348) = 1;
                        } else {
                            *(s32*)(pl + offsetof(Player, motion_state)) = 10;
                        }
                        break;
                    case 1006:
                        remove_optmenu(menu);
                        *(s32*)(pl + 0x3348) = 1;
                        break;
                    }
                    break;
                case 1:
                case 2:
                    if (fn_80055F68(0, 0) != 0) {
                        *(s32*)(pl + 0x3348) += 1;
                    }
                    break;
                case 3: {
                    s32 ok;
                    if (*(s32*)(pl + offsetof(Player, motion_state)) == 6) {
                        ok = saveMount(*(s32*)(pl + 0x334C),
                                       *(s32*)(pl + 0x3350), 1);
                        if (ok < 0) {
                            ok = 0;
                        }
                    } else {
                        if (*(s32*)(pl + offsetof(Player, motion_state)) == 12 &&
                            saveGetFreeBytes(*(s32*)(pl + 0x334C),
                                             *(s32*)(pl + 0x3350)) <
                                lbl_80344BB8) {
                            *(s32*)(pl + 0x3348) = 1000;
                            goto serve6;
                        }
                        ok = MemCardCreateGaunt(*(s32*)(pl + 0x334C),
                                                *(s32*)(pl + 0x3350));
                    }
                    if (ok != 0) {
                        *(s32*)(pl + 0x3348) += 1;
                    } else {
                        *(s32*)(pl + 0x3348) = -1;
                    }
                    break;
                }
                default:
                    if (t >= 1000) {
                        *(s32*)(pl + 0x3348) += gFrameTicks;
                        if (*(s32*)(pl + 0x3348) >= 0x460) {
                            *(s32*)(pl + 0x3348) = 0;
                            *(s32*)(pl + offsetof(Player, motion_state)) = *(s32*)(pl + 0x333C);
                        }
                    } else if (t >= 0) {
                        *(s32*)(pl + 0x3348) += gFrameTicks;
                        if (*(s32*)(pl + 0x3348) >= 0x78) {
                            if (*(s32*)(pl + offsetof(Player, motion_state)) == 6) {
                                *(s32*)(pl + offsetof(Player, motion_state)) = 5;
                                *(s32*)(pl + 0x3348) = 1;
                            } else {
                                *(s32*)(pl + offsetof(Player, motion_state)) = 13;
                                setup_file_entries(pl, 1);
                                *(s32*)(pl + 0x3348) = 0;
                            }
                        }
                    } else {
                        *(s32*)(pl + 0x3348) -= gFrameTicks;
                        if (*(s32*)(pl + 0x3348) <= -0x78) {
                            *(s32*)(pl + offsetof(Player, motion_state)) = *(s32*)(pl + 0x333C);
                            *(s32*)(pl + 0x3348) = 0;
                        }
                    }
                    break;
                }
            serve6:
                if (*(s32*)(pl + offsetof(Player, motion_state)) == 11) {
                    do_sel_menu_8008E4F4(i, 11);
                } else if (*(s32*)(pl + offsetof(Player, motion_state)) == 6) {
                    do_sel_menu_8008E4F4(i, 6);
                } else {
                    do_sel_menu_8008E4F4(i, 12);
                }
                break;
            }
            case 13: /* pick a save slot (save target) */
                menu = page + moff + 712;
                if (*(s32*)(menu + 108) == 0) {
                    setup_sel_menu(i, 13);
                }
                show_optmenu(menu);
                setup_file_entries(pl, 1);
                choice = do_optmenu(menu, 1);
                if (vmu_directory_exists() <= 0) {
                    choice = -1;
                }
                do_sel_menu_8008E4F4(i, 13);
                switch (choice) {
                case -1:
                    remove_optmenu(menu);
                    *(s32*)(pl + offsetof(Player, motion_state)) = 10;
                    break;
                default:
                    if (choice >= 1000 && lbl_80344BB4 == 0) {
                        remove_optmenu(menu);
                        *(s32*)(pl + offsetof(Player, motion_state)) = 14;
                        *(s32*)(pl + 0x3354) = choice - 1000;
                        *(s32*)(pl + 0x3358) = choice - 1000;
                        *(s32*)(pl + 0x3348) = 0;
                        lbl_80344BB4 = 0x1E;
                    }
                    break;
                }
                break;
            case 14: { /* write-file progress */
                s32 t = *(s32*)(pl + 0x3348);
                menu = page + moff + 712;
                switch (t) {
                case 0:
                    if (*(s32*)(lbl_80274578 + *(s32*)(pl + 0x334C) * 132 +
                                *(s32*)(pl + 0x3350) * 132 +
                                *(s32*)(pl + 0x3354) * 16) < 0) {
                        *(s32*)(pl + 0x3348) = 1;
                    } else {
                        if (*(s32*)(menu + 108) == 0) {
                            setup_sel_menu(i, 15);
                        }
                        show_optmenu(menu);
                        switch (do_optmenu(menu, 1)) {
                        case -1:
                        case 1007:
                            remove_optmenu(menu);
                            *(s32*)(pl + offsetof(Player, motion_state)) = 13;
                            *(s32*)(pl + 0x3358) = -1;
                            break;
                        case 1006:
                            remove_optmenu(menu);
                            *(s32*)(pl + 0x3348) = 1;
                            break;
                        }
                    }
                    break;
                case 1:
                case 2:
                    if (fn_80055F68(0, 0) != 0) {
                        *(s32*)(pl + 0x3348) += 1;
                    }
                    break;
                case 3:
                    if (PlayerWriteSaveFile(i, *(s32*)(pl + 0x3354)) != 0) {
                        *(s32*)(pl + 0x3348) += 1;
                        add_vmu_file(*(s32*)(pl + 0x334C),
                                     *(s32*)(pl + 0x3350),
                                     *(s32*)(pl + 0x3354),
                                     (char*)(pl + offsetof(Player, name)),
                                     *(u16*)(pl + 0xA8E), *(s32*)(pl + offsetof(Player, character)));
                    } else {
                        *(s32*)(pl + 0x3348) = -1;
                    }
                    break;
                default:
                    if (t >= 0) {
                        *(s32*)(pl + 0x3348) += gFrameTicks;
                        if (*(s32*)(pl + 0x3348) >= 0x78) {
                            *(s32*)(pl + 0x3348) = 0;
                            *(s32*)(pl + offsetof(Player, motion_state)) = *(s32*)(pl + 0x333C);
                            setup_sel_menu(i, *(s32*)(pl + offsetof(Player, motion_state)));
                            {
                                u8* base = *(u8**)(menu + 28);
                                s32 k = 0;
                                s32 off = k;
                                s32 found = -1;
                                for (;;) {
                                    u8* en = base + off;
                                    if (*(u32*)en == 0) {
                                        break;
                                    }
                                    if (*(s32*)(en + 4) == 1005) {
                                        if (*(s32*)(en + 32) >= 0) {
                                            found = k;
                                            break;
                                        }
                                    }
                                    k++;
                                    off += 36;
                                }
                                if (found >= 0) {
                                    *(s32*)(menu + 116) = found;
                                }
                            }
                        }
                    } else {
                        *(s32*)(pl + 0x3348) -= gFrameTicks;
                        if (*(s32*)(pl + 0x3348) <= -0x78) {
                            *(s32*)(pl + offsetof(Player, motion_state)) = *(s32*)(pl + 0x333C);
                            *(s32*)(pl + 0x3348) = 0;
                        }
                    }
                    break;
                }
            serve14:
                do_sel_menu_8008E4F4(i, 14);
                break;
            }
            case 3: /* name entry */
                menu = page + moff + 712;
                if (*(s32*)(pl + 0x3348) < 2 && *(s32*)(pl + 0x3348) >= 0) {
                    fn_8005AC10(i);
                    *(s32*)(pl + 0x3348) = 2;
                }
                if (fn_8005A738(i) != 0) {
                    if (set_hidden_player(pl) != 0) {
                        s32 pi = *(s32*)pl;
                        u8* b;
                        init_player_change(pi, *(s32*)(pl + offsetof(Player, character)));
                        b = blitbase + pi * 132;
                        *(s32*)(b + 0x1C) = 5;
                        *(s32*)(b + 0x20) = 0;
                        mbBlitInit3414(*(void**)(blitbase + (pi * 132 + 24)),
                                       0);
                        MBBlitSetAlpha(*(void**)(blitbase + (pi * 132 + 24)),
                                       0xFF);
                        *(s32*)(b + 0x28) = 7;
                        *(s32*)(b + 0x2C) = 0;
                        *(s32*)(b + 0x4C) = 1;
                        *(s32*)(b + 0x50) = 0;
                        *(s32*)(b + 0x40) = 1;
                        *(s32*)(b + 0x44) = 0;
                    } else {
                        *(s32*)(pl + offsetof(Player, respawn_char)) =
                            LimitSeltype(pl, *(s32*)(pl + offsetof(Player, character)), 0);
                        *(s32*)(pl + offsetof(Player, motion_state)) = 4;
                    }
                    *(SaveSnap*)(pl + 0x1ECC) = *(SaveSnap*)(pl + offsetof(Player, name));
                }
                choice = do_optmenu(menu, 0);
                do_sel_menu_8008E4F4(i, 2);
                switch (choice) {
                case -1:
                    *(s32*)(pl + offsetof(Player, motion_state)) = *(s32*)(pl + 0x333C);
                    break;
                }
                break;
            case 4: { /* class / costume pick */
                s32 sel = *(s32*)(pl + offsetof(Player, respawn_char));
                s32 moved = 0;
                s32 step = 0;
                s32 known;
                char* texname;

                menu = page + moff + 712;
                choice = do_optmenu(menu, 0);
                do_sel_menu_8008E4F4(i, 3);
                switch (choice) {
                case 7:
                    costume++;
                    if (costume >= 4) {
                        costume = 0;
                    }
                    moved = 1;
                    break;
                case 8:
                    costume--;
                    if (costume < 0) {
                        costume = 3;
                    }
                    moved = 1;
                    break;
                case 3:
                    step = -1;
                    moved = 1;
                    break;
                case 4:
                    step = 1;
                    moved = 1;
                    break;
                case -1:
                    *(s32*)(pl + offsetof(Player, motion_state)) = *(s32*)(pl + 0x333C);
                    if (*(s32*)(pl + offsetof(Player, motion_state)) == 0) {
                        new_player(i);
                    }
                    mbBlitInit3414(*(void**)((blitbase + boff) + 0x24), 1);
                    break;
                }
                sel += step;
                if (!(moved == 0 && sel == *(s32*)(pl + offsetof(Player, respawn_char)) &&
                      *(s32*)(pl + offsetof(Player, prev_state)) == 2)) {
                    *(s32*)(pl + offsetof(Player, prev_state)) = 2;
                    sel = LimitSeltype(pl, sel, step);
                    if (sel != 16) {
                        LoadPlyrData(i, sel, 0);
                    }
                    if (moved != 0 || sel != *(s32*)(pl + offsetof(Player, respawn_char))) {
                        if (*(s32*)(pl + offsetof(Player, respawn_char)) < 8) {
                            known = 1;
                        } else if (*(u16*)(pl + 0xA8C) &
                                   (1 << (*(s32*)(pl + offsetof(Player, respawn_char)) - 8))) {
                            known = 1;
                        } else {
                            known = 0;
                        }
                        if (known) {
                            texname = lbl_80120104[costume];
                        } else {
                            texname = lbl_80347F44;
                        }
                        AudioCursorChar();
                        if (*(s32*)(pl + offsetof(Player, respawn_char)) == 16) {
                            setup_tex(i, 4, 0, 0, lbl_80347F4C);
                        } else {
                            setup_tex(i, 4, 0, 0, pool + 156,
                                      lbl_801200B0[*(s32*)(pl + offsetof(Player, respawn_char))],
                                      texname);
                        }
                        *(s32*)((blitbase + boff) + 0x34) = 2;
                        *(s32*)((blitbase + boff) + 0x38) = 0;
                        if (serve_blits(i) != 0) {
                            servedMask |= 1 << i;
                        }
                    }
                    *(s32*)(pl + offsetof(Player, respawn_char)) = sel;
                    *(s32*)(pl + offsetof(Player, class_id)) = costume;
                }
                if (new_menu_accept(i, 0) != 0) {
                    if (*(s32*)(pl + offsetof(Player, respawn_char)) < 8) {
                        known = 1;
                    } else if (*(u16*)(pl + 0xA8C) &
                               (1 << (*(s32*)(pl + offsetof(Player, respawn_char)) - 8))) {
                        known = 1;
                    } else {
                        known = 0;
                    }
                    if (known) {
                        AudioCursorSelect();
                        if (*(s32*)(pl + offsetof(Player, intower)) != 0) {
                            s32 wflag = 1;
                            change_player(i, *(s32*)(pl + offsetof(Player, respawn_char)));
                            *(s32*)(pl + offsetof(Player, intower)) = 1;
                            *(s32*)(pl + offsetof(Player, motion_state)) = 1;
                            setup_sel_menu(i, *(s32*)(pl + offsetof(Player, motion_state)));
                            {
                                u8* base = *(u8**)(menu + 28);
                                s32 k = 0;
                                s32 off = k;
                                s32 found = -1;
                                for (;;) {
                                    u8* en = base + off;
                                    if (*(u32*)en == 0) {
                                        break;
                                    }
                                    if (*(s32*)(en + 4) == 1005) {
                                        if (*(s32*)(en + 32) >= 0) {
                                            found = k;
                                            break;
                                        }
                                    }
                                    k++;
                                    off += 36;
                                }
                                if (found >= 0) {
                                    *(s32*)(menu + 116) = found;
                                }
                            }
                            setup_tex(i, 2, 0, 0, pool + 168,
                                      lbl_801200B0[*(s32*)(pl + offsetof(Player, respawn_char)) & 7]);
                            mbBlitProject(*(void**)((blitbase + boff) + 0x18), -1, 320);
                            if (*(s32*)(pl + offsetof(Player, respawn_char)) == 16 ||
                                *(u32*)(pl + offsetof(Player, hidden_code)) != 0) {
                                wflag = 0;
                            }
                            if (*(s32*)(pl + offsetof(Player, exp)) == 0) {
                                AudioWelcomeBack(i, wflag);
                            } else {
                                AudioWelcome(i, wflag);
                            }
                        } else {
                            s32 picked = *(s32*)(pl + offsetof(Player, respawn_char));
                            u8* p2 = gPlayers + poff;
                            s32 saved;
                            s32 wflag;
                            *(s32*)(p2 + offsetof(Player, state)) = 3;
                            *(s32*)(p2 + offsetof(Player, exit_dest)) = other_players_next_level(i);
                            saved = *(s32*)(p2 + offsetof(Player, hidden_code));
                            change_player(i, picked);
                            *(s32*)(p2 + offsetof(Player, hidden_code)) = saved;
                            setup_tex(i, 2, 0, 0, pool + 168,
                                      lbl_801200B0[picked & 7]);
                            mbBlitProject(*(void**)((blitbase + boff) + 0x18),
                                          -1, 320);
                            if (*(u32*)(p2 + offsetof(Player, hidden_code)) != 0) {
                                wflag = 0;
                            } else {
                                wflag = 1;
                            }
                            if (*(s32*)(p2 + offsetof(Player, exp)) == 0) {
                                AudioWelcomeBack(i, wflag);
                            } else {
                                AudioWelcome(i, wflag);
                            }
                        }
                        *(s32*)((blitbase + boff) + 0x1C) = 5;
                        *(s32*)((blitbase + boff) + 0x20) = 0;
                        mbBlitInit3414(*(void**)((blitbase + boff) + 0x18), 0);
                        MBBlitSetAlpha(*(void**)((blitbase + boff) + 0x18), 0xFF);
                        *(s32*)((blitbase + boff) + 0x28) = 7;
                        *(s32*)((blitbase + boff) + 0x2C) = 0;
                        *(s32*)((blitbase + boff) + 0x4C) = 1;
                        *(s32*)((blitbase + boff) + 0x50) = 0;
                        *(s32*)((blitbase + boff) + 0x40) = 1;
                        *(s32*)((blitbase + boff) + 0x44) = 0;
                    } else {
                        AudioBuzzer();
                    }
                }
                break;
            }

            }

            if (*(s32*)(pl + offsetof(Player, state)) == 2) {
                update_class_spec(i);
                update_class_attr(i);
            }
            break;
        case 3: /* character locked in */
            if (allIdle == 0 && !(servedMask & (1 << i))) {
                s32 nx = -(*(s32*)(page + xoff) + 64);
                DrawTextKeepScale(lbl_80347F54, nx, 0x8E, 0, 0xFFFFFF,
                                  pool + 180);
                DrawTextKeepScale(lbl_80347F54, nx, 0x9A, 0, 0xFFFFFF,
                                  pool + 192);
                DrawTextKeepScale(lbl_80347F54, nx, 0xA6, 0, 0xFFFFFF,
                                  pool + 208);
                DrawTextKeepScale(lbl_80347F54, nx, 0xB2, 0, 0xFFFFFF,
                                  pool + 220);
                if (*(u32*)(lbl_80240E30 + padoff + 8) & 0x40000) {
                    *(s32*)(pl + offsetof(Player, state)) = 2;
                    *(s32*)(pl + offsetof(Player, motion_state)) = 1;
                    *(s32*)(pl + 0x333C) = 1;
                }
            }
            if (*(s32*)(pl + offsetof(Player, character)) == 2 &&
                *(u32*)(pl + offsetof(Player, hidden_code)) == lbl_80343D6C) {
                setup_tex(i, 8, 0, 0, pool + 232);
            } else {
                setup_tex(i, 8, 0, 0, lbl_80347F58,
                          lbl_801200B0[*(s32*)(pl + offsetof(Player, character))]);
            }
            break;
        case 1:
            break;
        default:
            *(s32*)((blitbase + boff) + 0x1C) = 1;
            *(s32*)((blitbase + boff) + 0x20) = 0;
            break;
        }
    }

    if (gGameMode == 0x400B) {
        s32 r = ShowLoading();
        if (r == 0) {
            if (allIdle != 0 || lbl_80344BA8 != 0) {
                DrawGlowText(lbl_80347F3C, -0x100, 0xEA, pool + 144);
                WritePlayerInfo(-1);
            }
            allIdle = 0;
        } else if (lbl_803448AC == 8 && lbl_803448A8 == 3) {
            if (allIdle != 0 || lbl_80344BA8 != 0) {
                EndTower();
                for (i = 0; i < 4; i++) {
                    abort_player(i);
                }
                init_attract_mode(0x8000);
                lbl_803445DC = 0;
                return 0;
            }
        } else if (lbl_80344BA8 != 0) {
            lbl_803445DC = 0;
            init_titlescreen();
        }
    }
    if (allIdle != 0) {
        lbl_80344BAC++;
    } else {
        lbl_80344BAC = 0;
    }
    if (lbl_80344BAC > 8) {
        lbl_803445DC = 0;
        return 1;
    }
    return 0;
}
#pragma dont_inline off

extern s32 lbl_80343DDC;
extern f32 lbl_80343DE0;
extern s32 lbl_80343DE4;
extern s32 lbl_80343DE8;
extern s32 lbl_80344BC4;
extern s32 lbl_80344BB8;
extern s32 gDemoMode;
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

#pragma opt_propagation off
static void do_sel_menu_8008E4F4(s32 player, u32 mode)
{
    s32* xp;
    u8* tbl;
    s32 lh;
    s32 sz;
    s32 sz2;
    s32 font;
    s32 showBack;
    char* pool;
    s32 showSel;
    s32 x;
    f32 scale;
    u8* pl;
    f32 scale0;
    char buf[32];

    scale0 = lbl_80343DE0;
    tbl = lbl_80121688;
    xp = (s32*)(tbl + (player << 2));
    pool = lbl_801143F8;
    pl = gPlayers + player * 13148;
    scale = scale0;
    lh = lbl_80343DDC;
    font = lbl_80344BC4;
    showSel = 1;
    showBack = 0;
    x = *xp;
    sz = (s32)(lbl_80347F60 * scale0);
    sz2 = (s32)(lbl_80347F60 * scale0);

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
        bx = *xp + sz;
        bx += 10;
        y2 = lbl_80343DE4;
        lh = 20;
        x2 = bx - sz;
        MBNewTempBlit(lbl_80344E30, x2, y2, sz, sz2);
        MBNewTempBlit(lbl_80344E2C, bx, y2, sz, sz2);
        tx = sz + 8;
        tx = bx + tx;
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
        s32 bx = x + sz;
        s32 by = lbl_80343DE4 + 52;
        s32 t;
        bx += 10;
        lh = 20;
        MBNewTempBlit(lbl_80344E38, bx - sz, by, sz, sz2);
        MBNewTempBlit(lbl_80344E34, bx, by, sz, sz2);
        DrawTextKeepScale(scale, bx + (sz + 8), by + 4, font,
                          0xFFFFFF, lbl_80347F1C);
        if (*(s32*)(pl + 16) < 8) {
            t = 1;
        } else {
            t = 1;
            switch (*(u16*)(pl + 2700) & (t << (*(s32*)(pl + 16) - 8))) {
            case 0:
                t = 0;
                break;
            default:
                break;
            }
        }
        if (t != 0) {
            showSel = 1;
        } else {
            showSel = 0;
        }
        break;
    }
    case 5:
        if (*(s32*)(pl + 13128) == 0) {
            u8* row = tbl + player * 232;
            s32 y = lbl_80343DE8 + *(s32*)(row + 728) - lh * 6;
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
        u8* row = tbl + player * 232;
        s32 y = lbl_80343DE8 + *(s32*)(row + 728) - lh * 3;
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
        u8* row = tbl + player * 232;
        s32 y = lbl_80343DE8 + *(s32*)(row + 728) - lh * 2;
        s32 nx = -(x + 64);
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 376);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 388);
        showBack = 1;
        break;
    }
    case 13: {
        u8* row = tbl + player * 232;
        s32 y = lbl_80343DE8 + *(s32*)(row + 728) - lh * 3;
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
        y = lbl_80343DD8;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 464);
        y += lh;
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 304);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 480);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FD8);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FC0);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 492);
        showSel = 0;
        break;
fmt_result:
        if (st >= 0) {
            nx = -(x + 64);
            y = lbl_80343DD8;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FE0);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 508);
        } else {
            nx = -(x + 64);
            y = lbl_80343DD8;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FE0);
            y += lh;
            DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FE8);
        }
        showSel = 0;
        break;
    }
    case 11:
    case 12: {
        s32 st = *(s32*)(pl + 13128);
        s32 y = lbl_80343DD8 - lh * 5;
        s32 nx;
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
        y = lbl_80343DD8;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 588);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 600);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 612);
        y += lh;
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 304);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 480);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FD8);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FC0);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 492);
        showSel = 0;
        break;
cr_result:
        if (st < 1000) goto cr_chk2;
        nx = -(x + 64);
        y = lbl_80343DD8;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 628);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 644);
        goto cr_done;
cr_chk2:
        if (st < 0) goto cr_fail;
        nx = -(x + 64);
        y = lbl_80343DD8;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 656);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 668);
        goto cr_done;
cr_fail:
        nx = -(x + 64);
        y = lbl_80343DD8;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FF0);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, pool + 656);
        y += lh;
        DrawTextKeepScale(scale, nx, y, font, 0xFFFFFF, lbl_80347FE8);
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
        yy = lbl_80343DD8;
        if (w == 0) goto io_prog;
        nx = -(x + 64);
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 680);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 692);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, lbl_80347FF8);
        break;
io_prog:
        yy = lbl_80343DD8;
        DrawTextKeepScale(scale, -(x + 64), yy, font, 0xFFFFFF,
                          (w != 0) ? pool + 704 : pool + 716);
        nx = -(x + 64);
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
        yy = lbl_80343DD8;
        if (st < 0) goto io_neg;
        DrawTextKeepScale(scale, -(x + 64), yy, font, 0xFFFFFF,
                          (w != 0) ? lbl_80347F14 : lbl_80347F0C);
        nx = -(x + 64);
        yy += lh;
        DrawTextKeepScale(scale, nx, yy, font, 0xFFFFFF, pool + 732);
        goto io_done;
io_neg:
        if (st >= -1) goto io_done;
        DrawTextKeepScale(scale, -(x + 64), yy, font, 0xFFFFFF,
                          (w != 0) ? lbl_80347F14 : lbl_80347F0C);
        nx = -(x + 64);
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
        s32 t8 = sz + 8;
        MBNewTempBlit(lbl_80344E48, bx, 252, sz, sz2);
        DrawTextKeepScale(scale, bx + t8, 256, font, 0xFFFFFF,
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
        {
            s32 t8 = sz + 8;
            MBNewTempBlit(lbl_80344E44, bx, by, sz, sz2);
            DrawTextKeepScale(scale, bx + t8, by + 4, font, 0xFFFFFF,
                              lbl_80348010);
        }
    }
}
#pragma opt_propagation reset

void init_player_change(s32 idx, s32 arg1)
{
    u8* pl;
    int i;
    u8* p;
    s32 v;
    s32 saved;
    s32 wflag;

    p = gPlayers;
    pl = p + idx * 0x335C;
    ((Player*)pl)->state = 3;

    for (i = 0; i < 4; i++, p += 0x335C) {
        if (i != idx) {
            s32 st = ((Player*)p)->state;
            if (st == 1 || (u32)(st - 4) <= 1) {
                v = ((Player*)p)->exit_dest;
                goto gotv;
            }
        }
    }
    v = *(s32*)((u32)gPlayers + (u32)(idx * 0x335C) + 0x830);
gotv:
    ((Player*)pl)->exit_dest = v;

    saved = *(s32*)(pl + 0xF0);
    change_player(idx, arg1);
    *(s32*)(pl + 0xF0) = saved;

    setup_tex(idx, 2, 0, 0, lbl_801144A0, lbl_801200B0[arg1 & 7]);

    mbBlitProject(*(void**)((u8*)lbl_80284878 + idx * 132 + 24), -1, 320);

    wflag = *(u32*)(pl + 0xF0) ? 0 : 1;
    if (((Player*)pl)->exp == 0) {
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
            e = eb + *(s32*)pl * 324;
            e += entOff;
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
            if (*(volatile s32*)(e + 32) == 0) {
                continue;
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
        if (p != pl && ((Player*)p)->state != 0 &&
            *(s32*)(p + 0x334C) == a && *(s32*)(p + 0x3350) == b &&
            *(s32*)(p + 0x3358) == v) {
            return 0;
        }
    }
    return 1;
}

typedef struct VmuMenuEntry {
    char* text;
    s32 id;
    s32 type;
    u8 _pad0C[20];
    s32 state;
} VmuMenuEntry;

void setup_vmu_entries(void)
{
    char* slotFormat = lbl_80114718;
    u8* base = lbl_80284878;
    s32 n = 0;
    s32 idx = 0;
    char* buf = (char*)(base + 0x744);
    VmuMenuEntry* entry;
    s32* cardPresent = &lbl_80344A14;
    if (lbl_80344A18 == 3) {
        if (lbl_80344610[0] == 2) {
            sprintf(buf, slotFormat, n + 1, (s8)lbl_80343DEC);
        } else {
            slotFormat = lbl_80114724;
            if (n > 0) {
                goto end;
            }
            sprintf(buf, slotFormat, n + 1, idx);
        }
        entry = (VmuMenuEntry*)(base + 0x720);
        entry->text = buf;
        entry->id = 1000;
        if (*cardPresent == 1) {
            entry->state = 0;
        } else {
            entry->state = -1;
        }
        entry->type = 4;
        idx = 1;
    }
end:
    ((VmuMenuEntry*)(base + 0x720))[idx].text = 0;
}

static s32 sel_set_choice(s32 player, s32 mode);

#pragma opt_propagation off
void setup_sel_menu(s32 player, s32 mode)
{
    u8* bss = lbl_80284878;
    u8* data = lbl_80121688;
    u8* menu;
    u8* field;
    s32 playerOffset;
    s32 baseChoice;

    ClearPlayerControl(player, 2);
    playerOffset = player * 232;
    menu = data;
    menu += playerOffset;
    menu += 712;
    memcpy(menu, data + 1640, 232);

    field = data;
    field += playerOffset;
    baseChoice = ((s32*)data)[player];
    *(s32*)(field += 724) -= baseChoice;
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
        VmuMenuEntry* entry;
        s32 sum;
        s32 off;
        s32 i;
        s32* selected;

        *(void**)(data + playerOffset + 740) = entries;
        sum = player * 0x335C;
        *(s32*)field = baseChoice + 4;
        *(s32*)(data + playerOffset + 728) = 70;
        selected = (s32*)(data + playerOffset + 828);
        *(f32*)(data + playerOffset + 764) = lbl_80348020;
        off = 0;
        *selected = off;
        sum = *(s32*)(gPlayers + sum + 0x334C) +
              *(s32*)(gPlayers + sum + 0x3350);
        sum += 1000;
        i = 0;
        for (;; i++, off += 36) {
            entry = (VmuMenuEntry*)(entries + off);
            if (entry->text == NULL) {
                break;
            }
            if (sum == entry->id) {
                *selected = i;
                break;
            }
        }
        break;
    }
    case 8:
    case 13:
        *(void**)(data + playerOffset + 740) =
            (field = bss + player * 324) + 528;
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

#pragma opt_propagation off
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
        switch (((VmuMenuEntry*)e)->id) {
        case 1000:
            ((VmuMenuEntry*)e)->state = 0;
            break;
        case 1001:
            if (gDemoMode != 0) {
                ((VmuMenuEntry*)e)->state = -1;
            } else if (lbl_803448AC == 8 && lbl_803448A8 == 3) {
                ((VmuMenuEntry*)e)->state = -1;
            } else if (vmu_directory_exists() >= 1) {
                ((VmuMenuEntry*)e)->state = 0;
            } else {
                ((VmuMenuEntry*)e)->state = -1;
            }
            break;
        case 1002:
            if (gDemoMode != 0) {
                ((VmuMenuEntry*)e)->state = -1;
            } else if (vmu_directory_exists() >= 1) {
                ((VmuMenuEntry*)e)->state = 0;
            } else {
                ((VmuMenuEntry*)e)->state = -1;
            }
            break;
        case 1004:
            if (lbl_803448AC == 8 && lbl_803448A8 == 3) {
                ((VmuMenuEntry*)e)->state = -1;
            } else {
                ((VmuMenuEntry*)e)->state = 0;
            }
            break;
        case 1003:
            if (lbl_803448AC == 8 && lbl_803448A8 == 3) {
                ((VmuMenuEntry*)e)->state = -1;
            } else {
                owner = *(u32*)(pl + 240);
                if (owner != 0 && owner != lbl_80343D6C) {
                    ((VmuMenuEntry*)e)->state = -1;
                } else {
                    ((VmuMenuEntry*)e)->state = 0;
                }
            }
            break;
        case 1005:
            ((VmuMenuEntry*)e)->state = 0;
            break;
        }
        if (((VmuMenuEntry*)e)->state >= 0 && best < 0) {
            best = i;
        }
        i++;
        off += 36;
    }
    return best;
}
#pragma opt_propagation reset

s32 other_players_next_level(s32 idx)
{
    s32 st;
    u8* p;
    int i;

    p = gPlayers;
    for (i = 0; i < 4; i++, p += 0x335C) {
        if (i != idx) {
            st = ((Player*)p)->state;
            if (st == 1 || (u32)(st - 4) <= 1) {
                return ((Player*)p)->exit_dest;
            }
        }
    }
    p = gPlayers;
    p += idx * 0x335C;
    return ((Player*)p)->exit_dest;
}

int check_active_players(void)
{
    int i;
    u8* p;
    int count = 0;
    new_menu_accept(-1, 1);
    p = gPlayers;
    for (i = 0; i < 4; i++, p += 0x335C) {
        if (((Player*)p)->state == 0 && (lbl_80344824 & (1 << i))) {
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
    LockModels(2);
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
    u8 statsPad[4];
    char buf[40];
    u8 bufPad[4];
    u8* expslot;
    s32 lvl;
    s32 best;
    s32 j;

    LoadPlyrData(player, *(s32*)(pl + 12), 0);
    if (*(s32*)(pl + 13092) <= 0) {
        *(s32*)(pl + 13092) = 1;
    }
    switch (*(s32*)(pl + 232)) {
    case 2:
        break;
    default:
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
        for (row = 0; row < 4; row++, y += lbl_80343DC8) {
            char* name = GetStringText(167, row, 0);
            s32 w = DrawNormalText(kScale, name, 6);
            s32 vx;
            s32 textX = tx - w;
            if (best == row) {
                DrawGlowText(kScale, textX, y - 2, name);
            } else {
                DrawTextKeepScale(kScale, textX, y - 2, 6, 0xFFFFFF,
                                  name);
            }
            vx = lbl_80343DB8 + *xp;
            sprintf(buf, lbl_8034802C, stats[row]);
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

#pragma opt_propagation off
void update_class_spec(s32 player)
{
    char* pool = lbl_801143F8;
    u8* blitBase = lbl_80284878;
    u8* pl = gPlayers + player * 0x335C;
    s32 boff;
    u8* eWeap;
    u8* eB;
    u8* eC;
    u8* eD;
    u8* eA;
    s32 state;
    s32 cls;
    s32 spec;
    s32 known;
    char* qfmt;
    char* extra;
    char* texName;
    u8 unused[40];
    StrBlock4 tmp;
    u8 pad[8];

    PlayerModel(player);
    setup_player_display(player);
    setup_tex(player, 2, 0, 0, pool + 168,
              lbl_801200B0[*(s32*)(pl + 12) & 7]);
    boff = player * 132;
    eWeap = blitBase;
    eWeap += boff;
    mbBlitProject(*(void**)(eWeap += 24), -1, 320);
    eA = blitBase;
    eA += boff;
    mbBlitInit3414(*(void**)(eA += 84), 1);
    eB = blitBase;
    eB += boff;
    mbBlitInit3414(*(void**)(eB += 72), 1);
    eC = blitBase;
    eC += boff;
    mbBlitInit3414(*(void**)(eC += 60), 1);
    eD = blitBase;
    eD += boff;
    mbBlitInit3414(*(void**)(eD += 96), 1);

    if (gGameMode == 0x400B) {
        state = *(s32*)(pl + 232);
        if (state == 3) {
            return;
        }
        if (state < 3) {
            if (state >= 2) {
                goto substate;
            }
        }
        hide_select_blits(player, 0);
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
                      lbl_801200B0[cls]);
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
        } else {
            known = 1;
            if ((*(u16*)(pl + 2700) & (known << (spec - 8))) == 0) {
                known = 0;
            }
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
                      lbl_801200B0[spec], extra);
        }
        if (texName != 0) {
            (void)pbLoad;
            setup_tex(player, 8, 0, 0, lbl_80347F58,
                      lbl_801200B0[*(s32*)(pl + 16)]);
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
#pragma opt_propagation reset

/* Enter / initialise the select screen. */
extern void abort_player(s32 i);
extern void player_store_in_save(u8* pl);
extern void init_targets(void);
extern void InitCamera(s32 mode);
extern s32 gGameBusy;
extern s32 lbl_8034481C;
extern s32 good_wiz_exit_timer;
extern s32 lbl_80344BB8;
extern s32 lbl_80344B84;
extern s32 welcome_timer;
extern s32 lbl_80344BA8;
extern s32 lbl_80344BAC;
extern s32 lbl_80344BB4;
extern s32 lbl_80344BC8;
extern s32 lbl_80344BB0;
extern s32 lbl_80344B90;
extern s32 lbl_80344B94;
extern s32 lbl_80344B98;
extern s32 lbl_80344B9C;
extern s32 lbl_80344BA0;
extern s32 lbl_80344BA4;
extern s32 sLastWorldLevel;
extern u8 gGameOptions[];
extern u8 lbl_80284878[];
extern f64 lbl_80348038;
extern void fn_80053C70(void);
extern void fn_800BC418(s32 startLine, s32 count);
extern void towerUpdateCurWorldObj(void);
extern void PlayersRestoreHealth(void);
extern void AudioStopSelect(void);
extern void AudioSelectReset(void);
extern void mbBlitInit3414(void* blit, s32 hide);
extern void mbBlitCvtCoord(void* blit, f32 c);
extern void mbBlitCalcWidth(void* blit, s32 x, s32 w, f32 h);
void update_class_spec(s32 player);

#pragma opt_propagation off
void init_player_select(s32 mode)
{
    char* pool = lbl_801143F8;
    u8* page = lbl_80121688;
    s32 i2;
    s32* xp;
    u8* blits;
    u8* pl;
    u8 _spare[32];

    AudioStopSelect();
    lbl_80344BB8 = saveFileSize();
    gGameMode = 0x400B;
    gGameBusy = 0;
    lbl_8034481C = 0;
    good_wiz_exit_timer = 0;
    lbl_80344B84 = -1;
    welcome_timer = 0;
    lbl_80344BA8 = 0;
    lbl_80344BAC = 0;
    lbl_80344BB4 = 0;
    if (mode == 1) {
        towerUpdateCurWorldObj();
        PlayersRestoreHealth();
    }
    {
        s32 i1;
        pl = gPlayers;
        for (i1 = 0; i1 < 4; i1++, pl += 13148) {
            if (!(lbl_80344824 & (1 << i1))) {
                abort_player(i1);
            }
            *(s32*)pl = i1;
            if (*(s32*)(pl + 232) == 4) {
                *(s32*)(pl + 232) = 1;
            }
            if (mode == 2 && *(s32*)(pl + 232) == 5) {
                *(s32*)(pl + 232) = 2;
                *(s32*)(pl + 13112) = 1;
            }
            if (*(s32*)(pl + 232) == 1) {
                *(s32*)(pl + 232) = 3;
                player_store_in_save(pl);
                remove_player_geo(i1);
            }
        }
    }
    if (mode == 0) {
        msgInit();
    }
    lbl_80344BC8 = 0;
    init_targets();
    lbl_80344BB0 = 0;
    if (mode != 0) {
        AudioSelectReset();
        lbl_80344BB0 = 4;
    }
    fn_800BC418(2, -1);
    LoadTowerAndSelect();
    InitCamera(0);
    i2 = 0;
    new_menu_accept(-1, 1);
    {
        for (pl = gPlayers; i2 < 4; i2++, pl += 13148) {
            if (*(s32*)(pl + 232) == 0 && (lbl_80344824 & (1 << i2))) {
                new_player(i2);
            }
        }
    }
    {
        s32 i3 = 0;
        s32 j;
        s32 o132 = 0;
        s32 o4 = 0;
        for (pl = gPlayers; i3 < 4;
             i3++, o132 += 132, o4 += 4, pl += 13148) {
            s32 joff;
            xp = (s32*)(page + o4);
            blits = lbl_80284878 + o132;
            for (j = 0, joff = 0; j < 11; j++, joff += 12) {
                u8* e = page + joff;
                void* b;
                s32 w = *(s32*)(e + 32);
                s32 h = *(s32*)(e + 36);
                w += *xp;
                e += 32;
                b = (void*)MBCreateBlit(0, 0, w, h, -1, -1);
                *(void**)(blits + joff) = b;
                mbBlitInit3414(*(void**)(blits + joff), 1);
                mbBlitCvtCoord(*(void**)(blits + joff),
                               (f32)*(s32*)(e + 8));
                *(s32*)(blits + joff + 4) = 0;
            }
            *(s32*)(pl + 2096) = sLastWorldLevel;
            if (!(((SelOptsView*)gGameOptions)->flags44 & 1)) {
                setup_tex(i3, 0, 0, 0, pool + 868, i3 + 1);
                setup_tex(i3, 1, 0, 0, pool + 880, i3 + 1);
                setup_tex(i3, 9, 16384, 0, pool + 892);
                setup_tex(i3, 10, 16384, 0, pool + 904);
                update_class_spec(i3);
            }
            mbBlitCalcWidth(*(void**)(blits + 108),
                            *(s32*)(page + 140) + *xp,
                            *(s32*)(page + 144),
                            (f32)*(s32*)(page + 148));
            mbBlitCalcWidth(*(void**)(blits + 120),
                            *(s32*)(page + 152) + *xp,
                            *(s32*)(page + 156),
                            (f32)*(s32*)(page + 160));
        }
    }
    if (!(*(u32*)((u32)gGameOptions + 44) & 1)) {
        lbl_80344B90 = 0;
        lbl_80344B98 = 0;
        lbl_80344B94 = 0;
        lbl_80344BA0 = 0;
        lbl_80344B9C = 0;
    }
    lbl_80344BA4 = 0;
    fn_80053C70();
}
#pragma opt_propagation reset

#pragma opt_propagation off
/* file-local view: per-slot select blit entry, 12 bytes (handle, mode,
 * timer) -- same layout documented above serve_blits(). */
typedef struct BlitEntry {
    void* handle;
    s32 mode;
    s32 timer;
} BlitEntry;

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
                ((BlitEntry*)entry)->mode = 0;
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

/* Animate the per-player select blits (one 12-byte entry per slot: handle,
 * mode, timer).  Returns how many entries are still mid-animation. */
extern u8* MBRomTexPtr(u32 tex);
extern f64 lbl_80348040;   /* pulse-scale base */
extern f64 lbl_80348048;   /* pulse-scale step */
extern f32 lbl_80348050;   /* zoom-in scale step */

s32 serve_blits(s32 player)
{
    u8* page = lbl_80121688;
    u8* blits = lbl_80284878 + player * 132;
    s32* xp = (s32*)(lbl_80121688 + player * 4);
    f64 kb;
    f64 ka = lbl_80348048;
    f32 kf = lbl_80348050;
    s32* sp;
    s32* tp2;
    s32* tp4;
    s32 j;
    s32 count = 0;
    u8* h;
    u8* e;
    s32 off;

    kb = lbl_80348040;
    for (j = 0, off = 0; j < 11; j++, off += 12) {
        e = blits + off;
        sp = (s32*)(e + 4);
        h = *(u8**)e;

        switch (*(u32*)(e + 4)) {
        case 0:
            break;

        case 1: /* hide */
            *sp = 0;
            *(s32*)(e + 8) = 0;
            mbBlitInit3414(h, 1);
            break;

        case 3: { /* looping pulse */
            u8* tex = MBRomTexPtr(*(u32*)(h + 4));
            s32 w = *(u16*)(tex + 10);
            s32 ht = *(u16*)(tex + 12);
            s32 amp;
            f32 f;
            s32 du;
            s32 dv;
            *(s32*)(e + 8) = (*(s32*)(e + 8) + 1) & 0x3F;
            amp = *(s32*)(e + 8);
            if (amp >= 0x20) {
                amp = 0x20 - (amp & 0x1F);
            }
            f = (f32)(ka * (f64)amp + kb);
            du = (s32)((f32)w * f);
            dv = (s32)((f32)ht * f);
            {
                u8* pe = page + off;
                mbBlitCalcWidth(h, *(s32*)(pe + 0x20) - du / 2 + *xp,
                                *(s32*)(pe + 0x24) - dv / 2,
                                (f32)*(s32*)(pe + 0x28));
            }
            mbBlitProject(h, du, dv);
            break;
        }

        case 2: { /* fly out right while fading */
            s32 t;
            s32 u;
            tp2 = (s32*)(e + 8);
            *tp2 += gFrameTicks;
            t = *tp2;
            u = t * t;
            {
                u8* pe = page + off;
                mbBlitCalcWidth(h, *(s32*)(pe + 0x20) + (*xp + u / 8),
                                u + t * 3, (f32)*(s32*)(pe + 0x28));
            }
            MBBlitSetAlpha(h, u);
            mbBlitProject(h, 0x80, 0x100 - u);
            if (u >= 0x100) {
                *sp = 0;
                *tp2 = 0;
                mbBlitInit3414(h, 1);
            } else {
                mbBlitInit3414(h, 0);
            }
            count++;
            break;
        }

        case 6: /* fade out (7 = delayed fade out) */
        case 7: {
            s32 half;
            s32 a;
            s32 x;
            x = *(s32*)(e += 8) + gFrameTicks;
            *(s32*)e = x;
            half = x >> 1;
            if (*sp == 7) {
                half -= 0x10;
                if (half < 0) {
                    half = 0;
                }
            }
            a = half * half;
            MBBlitSetAlpha(h, a);
            if (a >= 0x100) {
                *sp = 0;
                *(s32*)e = 0;
                mbBlitInit3414(h, 1);
            }
            count++;
            break;
        }

        case 4: { /* zoom in from center */
            u8* tex;
            s32 w;
            s32 ht;
            s32 half;
            s32 a;
            f32 f;
            s32 du;
            s32 dv;
            tp4 = (s32*)(e + 8);
            *(s32*)(e + 8) += gFrameTicks;
            half = *(s32*)(e + 8) >> 1;
            tex = MBRomTexPtr(*(u32*)(h + 4));
            a = half * half;
            w = *(u16*)(tex + 10);
            ht = *(u16*)(tex + 12);
            f = (f32)(0x100 - a) * kf;
            du = (s32)((f32)w * f);
            dv = (s32)((f32)ht * f);
            {
                u8* pe = page + off;
                mbBlitCalcWidth(h, w / 2 + *(s32*)(pe + 0x20) - du / 2 + *xp,
                                ht / 2 + *(s32*)(pe + 0x24) - dv / 2,
                                (f32)*(s32*)(pe + 0x28));
            }
            mbBlitProject(h, du, dv);
            if (a >= 0x100) {
                *sp = 0;
                *tp4 = 0;
                mbBlitInit3414(h, 1);
            }
            count++;
            break;
        }

        case 5: { /* fade in */
            s32 a;
            s32 x;
            x = *(s32*)(e += 8) + gFrameTicks;
            a = x >> 1;
            *(s32*)e = x;
            a = a * a;
            if (a > 0x100) {
                a = 0x100;
            }
            MBBlitSetAlpha(h, 0x100 - a);
            if (a >= 0x100) {
                *sp = 0;
                *(s32*)e = 0;
            }
            count++;
            break;
        }
        }
    }
    return count;
}
