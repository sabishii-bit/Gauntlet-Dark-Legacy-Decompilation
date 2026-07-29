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
extern s32 lbl_803449A0;   /* select mode flag       */
extern u32 lbl_80344824;   /* active-player bit mask */
extern s32 lbl_80344A18;   /* per-(port+slot) card state (3=ready) */
extern s32 lbl_80344A14;   /* per-(port+slot) card-present flag    */
extern s32 lbl_80344610;   /* memcard slot sub-state */
extern u8  lbl_80343DEC;   /* current card port/slot byte */
extern char lbl_80114718[];/* save-slot format string A */
extern char lbl_80114724[];/* save-slot format string B */
extern u8  gPlayers[]; /* 4-player array, stride 0x335C */
extern u8  lbl_80284878[]; /* 4 pages x 11 entries x 0xC blit table */

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
extern void DrawGlowText(void);
extern void DrawNormalText(void);
extern void DrawTextKeepScale(void);
extern char* GetStringText(int id);
extern void* MBNewTempBlit(void* tex, int x, int y, int w, int h);
extern int  MBCreateBlit(int a, int b, int c, int d, int e, int f);
extern void* MBOX_FindTexture_Err(char* name, s32* out, s32 err);
extern void mbBlitInit3414(void* blit, s32 hide);
extern void mbBlitProject(void* blit, s32 w, s32 h);
extern void mbInitBlitEntry(void* blit, u32 frames, s32 frame);
extern void mbBlitUpdateEntry(void* blit, u32 mask, u32 set);
extern void MBBlitSetAlpha(void* blit, s32 alpha);
extern void MBBlitSetColor4(void* blit, u32 a, u32 b, u32 c, u32 d);

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
    if (lbl_803449A0 != 0) {
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
    AudioWelcome(0, 0);
}

static void do_sel_menu(int player)
{
    (void)player;
    LimitSeltype(gPlayers, 4, 1);
    DrawGlowText();
    DrawTextKeepScale();
    MBNewTempBlit(0, 0, 0, 0, 0);
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

int setup_file_entries(const char* name)
{
    return strncmp(name, (const char*)gPlayers, 8);
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

void setup_sel_menu(void)
{
}

void sel_set_inactive(s32 slot)
{
    lbl_80121950[slot].state = 0;
}

void sel_set_choice(void)
{
    mbBlitProject(0, 0, 0);
}

s32 other_players_next_level(s32 idx)
{
    int i;
    u8* p = gPlayers;
    for (i = 0; i < 4; i++, p += 0x335C) {
        if (i != idx) {
            s32 st = *(s32*)(p + 0xE8);
            if (st == 1 || (u32)(st - 4) <= 1) {
                return *(s32*)(p + 0x830);
            }
        }
    }
    return *(s32*)(gPlayers + idx * 0x335C + 0x830);
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

void update_class_attr(void)
{
    GetStringText(0);
    DrawNormalText();
    DrawGlowText();
    MBNewTempBlit(0, 0, 0, 0, 0);
}

void update_class_spec(void)
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

void hide_select_blits(s32 arg0, s32 flag)
{
    s32 start, end;
    s32 pg, j;
    if (arg0 < 0) {
        start = 0;
        end = 3;
    } else {
        start = end = arg0;
    }
    for (pg = start; pg <= end; pg++) {
        u8* pagebase = (u8*)lbl_80284878 + pg * 132;
        for (j = 0; j < 11; j++) {
            u8* entry = pagebase + j * 12;
            u32 h = *(u32*)entry;
            if (h != 0) {
                if (flag != 0 || (j > 1 && j < 9)) {
                    mbBlitInit3414((void*)h, 1);
                } else {
                    mbBlitInit3414((void*)h, 0);
                }
                *(s32*)(entry + 4) = 0;
            }
        }
    }
}

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
