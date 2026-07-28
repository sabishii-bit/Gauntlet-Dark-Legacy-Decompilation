/*
 * player.c -- GCN PLAYER.OBJ (shell3D.pdb module .\Release\PLAYER.OBJ), FRONT SLICE.
 *
 * Per-player game logic + the in-game player HUD: the parent/grab attachment
 * ops used by critters and PlayerMotion, the per-frame HUD writers (health,
 * gold, items, rune stones, power meter, "IT" tag), the experience/level
 * math, potion magic launch, and the master per-frame driver do_players().
 *
 * SLICE (this file, wired 2026-07-27):
 * .text       0x800745D0..0x80077BF0  (22 GC functions)
 * extab       0x80006900..0x80006988  (17 entries x 8; 5 leaves have none)
 * extabindex  0x8000A5D0..0x8000A69C  (17 entries x 12)
 *
 * FULL PLAYER.OBJ extends to 0x8008091C (get_player_pos = PMOTION.OBJ start);
 * later sessions extend the .text end. Not claimed yet: .data (potionicon_tab
 * tables at 0x80124C70.., blit pos tables 0x8011FC48..), .bss (HUD blit arrays
 * 0x80274EA0..0x80275AE0 + player records), .sdata (welcome/it globals
 * 0x80344AF0..), .sdata2 pool (0x80347608 "XRay" onward).
 *
 * BOUNDARY EVIDENCE (front seam OPTIONS | PLAYER at 0x800745D0):
 *  - see options.c header (extabindex run + sdata2/sdata seams).
 *  - extab seam: options extab ends exactly 0x80006900 / extabindex 0x8000A5D0
 *    (last OPTIONS entry: fn 0x80074548 -> extab 0x800068F8).
 * BACK seam of this slice (0x80077BF0) is fn-boundary only -- the TU
 * continues (fn_80077BF0 belongs to PLAYER.OBJ; extab entry 0x80006988).
 *
 * GC function order does NOT match the Xbox PDB listing (Xbox /Gy reorder);
 * names anchored by behavior + strings + caller symmetry:
 *
 *   0x800745D0 PlayerAttacking     (G) action-state query [critter.c 0x8003BAFC]
 *   0x80074640 PlayerUnsetParent   (G) detach from carrier, clear 0x20/0x4000 [critter.c]
 *   0x800746B0 PlayerUnsetGrabbed  (G) release grab, optional pos restore [PlayerMotion x4]
 *   0x80074734 PlayerSetParent     (G) attach to carrier node + offset [critter.c]
 *   0x80074818 PlayerSetGrabbed    (G) grab attach, optional pos [PlayerMotion x4]
 *   0x800748BC del_player_blits    (G) hide all per-player HUD blits [auxscreen]
 *   0x800749D8 WritePlayerInfo     (G) per-frame HUD driver (one/all players)
 *   0x80074B74 show_crystals       (L) picked-up crystal/fang/coin ticker
 *   0x80074D64 ShowRuneStones      (G) rune/crystal collection icons (welcome_timer)
 *   0x80074F78 write_health_and_items (L) name/level/health/keys/potions writer
 *   0x800755C4 debug_player_pos    (L) floor name + pos + facing debug text
 *   0x80075798 write_gold          (L) gold counter (right-aligned, 99999 cap)
 *   0x80075894 draw_power_meter    (L) turbo meter bar + flash states
 *   0x80075D18 setup_player_display(G) rebuild the 6 portrait-frame blits
 *   0x800760C8 get_display_mode    (G) player state -> HUD display mode
 *   0x800761B0 AddExp              (G) score exp (recurses into familiar)
 *   0x80076440 ModifyExp           (L) exp delta + level-up/down loop
 *   0x800765B8 ExpToLevel          (G) inverse level curve (99..1 scan)
 *   0x80076618 LevelToExp          (G) level curve
 *   0x8007664C PlayerGiveGold      (G) gold add, 99999 cap
 *   0x80076684 start_magic         (G) potion-magic FX/sound launcher
 *   0x80076998 do_players          (G) per-frame master driver [gamemain]
 *
 * MATCH STATUS (2026-07-27 wiring pass, cflags_demo probed best on both
 * compilers): BYTE-EXACT (reloc-name noise only): PlayerAttacking,
 * PlayerUnsetParent, PlayerUnsetGrabbed, PlayerSetGrabbed, PlayerGiveGold,
 * del_player_blits.  NEAR: LevelToExp (isel: lis+add vs addis on the 0x28550
 * tail -- 3 spellings tried), ExpToLevel (ctr-loop matched; same lis-hoist
 * isel + renumber), WritePlayerInfo (renum: p/i r27<->r28 web swap only).
 * PlayerSetParent: target spills the 3 fsubs deltas to stack slots before
 * the calls (no saved FPRs); scalar temps, d[3] array and inlined VecSub all
 * keep them in f26-f31 -- structural lever still missing.  The remaining
 * bodies are faithful Ghidra transcriptions pending match passes (biggest
 * first: do_players, write_health_and_items, draw_power_meter,
 * setup_player_display, start_magic).
 *
 * FLIP GATES (claimcheck): .bss 0xC40 (the HUD block below -- size exact),
 * .data 0x5C (jumptable), .rodata 0x104 (strings), .sdata2 0x198 (pool)
 * need claims when this TU flips to Matching.
 *
 * gPlayerRecords = lbl_80275AE0 ([4] x 0x335C player records; stays lbl_ --
 * Matching shopquery.c references it).  HUD .bss layout (PLAYER.OBJ's own):
 *   0x80274EA0 potionicon_tab[5]   potion-type -> texture id
 *   0x80274EB4 hod_blit[4]         hand-of-death (shards&0x1000) icons
 *   0x80274EC4 quest_blit[4]       QUEST_ICON blits
 *   0x80275394 tbuf[...]           sprintf scratch (tbuf+0xE truncation)
 *   0x80275880 rune13_blit[4]      13th-rune icons
 *   0x80275890 pm_blit[4][7]       power-meter blit septet
 *   0x80275900 key_blit[4][4]      key icons
 *   0x80275940 crystal_blit[4][8]  crystal icons (welcome display)
 *   0x802759C0 rune_blit[4][12]    rune-stone icons
 *   0x80275A80 frame_blit[4][6]    portrait frame set (own symbol lbl_80275A80)
 *   (interior of lbl_80274EA0 / lbl_802757E0 -- .bss unclaimed, extern here)
 *
 * Player-record field offsets confirmed by this slice (extends player.h map):
 *   0x010 respawn char, 0x074 scene node, 0x0DC saved pos[3], 0x10C magic power,
 *   0x6C8 icon node, 0x828/0x82C power target/level, 0x834 quest flag,
 *   0x838 parent-anchor pos[3], 0x844 anchor fwd[3], 0x8C4/0x8C8 floor,
 *   0x8D4 world-obj flags (0x4000 = parented), 0x8F0 action id,
 *   0x91C/0x920 counters, 0x928/0x92C/0x930 got-item id/timer/count,
 *   0x964/0x966 HUD flag words (0x20 = attached), 0xA60 display mode,
 *   0xA80 name[8], 0x1EB4 health f32, 0x1EB8 keys, 0x1EBC potions,
 *   0x1EC0 exp, 0x1EC4 gold, 0x1ECA rune/shard mask, 0x32FC potion types,
 *   0x3324 level, 0x3338 motion state, 0x3340/0x3344 meter flash state/timer.
 */

#include "types.h"
#include "game/player.h"

/* ------------------------------------------------------------------ */
/* player records                                                      */
/* ------------------------------------------------------------------ */

extern Player lbl_80275AE0[]; /* gPlayerRecords[4], stride 0x335C */
#define gPlayerRecords lbl_80275AE0
#define PREC_STRIDE 0x335C
#define P(i)          (&gPlayerRecords[i])
#define PF(p, off, T) (*(T*)((u8*)(p) + (off)))

/* ------------------------------------------------------------------ */
/* extern data                                                         */
/* ------------------------------------------------------------------ */

/*
 * This TU's .bss (0x80274EA0..0x80275AE0, 0xC40 bytes).  The target pools
 * every access off the FIRST symbol (potionicon_tab @0x80274EA0) with folded
 * constant deltas (sibling-symbol pooling law), so the arrays are DEFINED
 * here with the exact layout; the DOL-splitter names for the block are
 * lbl_80274EA0 / lbl_802753B4 / lbl_80275534 / lbl_802757D4 / lbl_802757E0 /
 * lbl_80275A80.  pad_* regions belong to PLAYER fns beyond this slice
 * (mini-inventory tables, got_it state, tb_info) -- refine when wired.
 * NonMatching TU: these never link; the DOL keeps the original .bss.
 */
static void* potionicon_tab[5];    /* 0x000 (0x80274EA0) potion type -> texture */
static void* hod_blit[4];          /* 0x014 hand-of-death icons */
static void* quest_blit[4];        /* 0x024 QUEST_ICON blits */
static u8 hud_pad_034[0x4C0];      /* 0x034 (mini-inventory/got-it, later fns) */
static char tbuf[0x20];            /* 0x4F4 (0x80275394) sprintf scratch */
static u8 hud_pad_514[0x4CC];      /* 0x514 (0x802753B4.. tables, later fns) */
static void* rune13_blit[4];       /* 0x9E0 (0x80275880) 13th-rune icons */
static void* pm_blit[4][7];        /* 0x9F0 (0x80275890) power-meter septet */
static void* key_blit[4][4];       /* 0xA60 (0x80275900) key icons */
static void* crystal_blit[4][8];   /* 0xAA0 (0x80275940) crystal icons */
static void* rune_blit[4][12];     /* 0xB20 (0x802759C0) rune-stone icons */
static void* frame_blit[4][6];     /* 0xBE0 (0x80275A80) portrait frame set */

/* this TU's .data (unclaimed) */
extern s32 lbl_80124C70[];   /* crystal totals per color {0,15,100,...,250} */
extern char lbl_80124C94[];  /* crystal color names, 8-byte entries "TOWER".. */
extern s32 lbl_80124CDC[];   /* boss-item totals {12,20,28} */
extern char lbl_80124CE8[];  /* boss item names, 0xE entries "FANGS".. */
extern char lbl_801200B0[];  /* class tags, 4-byte entries "WAR","VAL",.. */
extern u16 lbl_80120238[];   /* per-player HUD left x */
extern u16 lbl_80120240[];   /* per-player HUD right x (drawn as -x) */
extern u32 lbl_801201C8[];   /* class colors */
extern u32 lbl_801201D8[];   /* class colors (in-game set) */
extern u32 lbl_801201E8[];   /* class colors (dim set) */
extern s32 lbl_8011FC48[];   /* +8 pm bar x/y/h, +0x1C pm frame x/y/h, +0x78 frames */
extern f32 lbl_80127D00[];   /* zero vec */
extern f32 lbl_80127D60[];   /* identity matrix */

#define pm_bar_x    (lbl_8011FC48[2])
#define pm_bar_y    (lbl_8011FC48[3])
#define pm_bar_z    (lbl_8011FC48[4])
#define pm_frame_x  (lbl_8011FC48[7])
#define pm_frame_y  (lbl_8011FC48[8])
#define pm_frame_z  (lbl_8011FC48[9])
#define pm_frames   (lbl_8011FC48[30])

/* effects (sfx TU) */
extern u8 lbl_80285BCC[]; /* gEffects[]: stride 0xF0, +0 node ptr */

/* enemies/world debug */
extern s32 lbl_802575B0;  /* debug HUD mode (1 = pos, 2 = XP) */
extern s32 lbl_802575BC;

/* controls (pad state, stride 0xF words per player) */
extern u32 lbl_80240E34[];
extern u32 lbl_80240E38[];
extern f32 lbl_80240E50[];

/* game/world state (.sbss/.sdata) */
extern s32 lbl_8034477C;   /* game state */
extern s32 options_state;
extern s32 lbl_80344298;
extern s32 lbl_80344824;   /* active-player mask */
extern s32 lbl_80344760;
extern u32 lbl_803445CC;   /* sFlags: pause/movie */
extern s32 lbl_8034457C;   /* frame delta (int) */
extern s32 lbl_80344578;   /* vb elapsed */
extern f32 lbl_80344590;   /* frame delta (float) */
extern f32 lbl_80344594;
extern s32 lbl_803448F0;   /* total sum-coin count */
extern s32 lbl_803448D8;   /* music track */
extern s32 lbl_803448D4;
extern s32 lbl_803448AC;
extern s32 lbl_803448A8;
extern s32 lbl_8034489C;
extern u8* lbl_8034483C;   /* gCurLevel */
extern s32 lbl_8034481C;
extern s32 lbl_80344804;   /* any player needs pad */
extern s32 lbl_803447BC;
extern s32 lbl_803447B8;   /* paused */
extern s32 lbl_803447B4;
extern s32 lbl_80344770;
extern s32 lbl_80344A28;
extern s32 lbl_80344A30;
extern s32 lbl_80344A44;
extern s32 lbl_80344AF0;
extern u32 lbl_80344AF8;   /* demo-message once flags */
extern s32 lbl_80344AFC;   /* key texture id */
extern s32 lbl_80344B00;
extern void* lbl_80344B04; /* it_blit */
extern s32 lbl_80344B0C;   /* speech round-robin counter */
extern s32 lbl_80344B10;   /* welcome-speech delay */
extern s32 lbl_80344B14;   /* magic color cycle counter */
extern s32 lbl_80344B1C;   /* welcome_timer */
extern s32 lbl_80344B24;   /* "it" player */
extern void* lbl_80344B2C; /* world root node */
extern s32 lbl_80344C4C;
extern s32 lbl_80344C54;
extern f32 lbl_80344C5C;
extern s32 lbl_80344C60;
extern s32 lbl_80344C90;
extern s32 lbl_80344CC4;
extern s32 lbl_803449A0;
extern s32 lbl_80344568;
extern s32 lbl_803443B4;
extern s32 lbl_8034439C;
extern s32 lbl_803444E4;
extern s32 lbl_803444F4;
extern s32 lbl_803444F8;
extern s32 lbl_803444FC;
extern s32 lbl_80344500;
extern f32 lbl_80344504;
extern s32 lbl_80344508;
extern s32 lbl_8034450C;
extern s32 lbl_80344510;
extern f32 lbl_80344F60;
extern s32 lbl_80344E44;   /* HUD button texture ids */
extern s32 lbl_80344E48;
extern s32 lbl_80343D70;
extern s32 lbl_80343D74;
extern f32 lbl_80343D78;

/* ------------------------------------------------------------------ */
/* extern functions                                                    */
/* ------------------------------------------------------------------ */

extern int sprintf(char* buf, const char* fmt, ...);

/* mb blit/text library */
extern void* MBOX_FindTexture(char* name, s32* out);
extern void* MBOX_FindTexture_Err(char* name, s32* out, s32 err);
extern void* MBOX_FindTexture_Sub(char* name, s32* out, s32 a, s32 b, s32 err);
extern void* MBNewBlit(char* name, s32 x, s32 y);
extern void* MBNewTempBlit(void* tex, s32 x, s32 y, s32 w, s32 h);
extern void* MBRemoveBlit(void* blit);
extern void mbBlitInit3414(void* blit, s32 hide);
extern void mbInitBlitEntry(void* blit, u32 frames, s32 frame);
extern void fn_800B2940(void* blit, u32 rgb);
extern void fn_800B290C(void* blit, s32 alpha);
extern void mbBlitCalcWidth(void* blit, s32 x, s32 y, f32 z);
extern void mbBlitProject(void* blit, s32 w, s32 h);
extern void mbBlitCvtCoord(void* blit, f32 z);
extern void mbBlitUpdateEntry(void* blit, u32 mask, u32 set);
extern void* MBRomTexPtr(u32 tex);
extern f32 MBSetFontZ(f32 z);
extern s32 MBSetFontFlags(s32 flags);
extern void MBWorldToScreen(f32* out, f32* pos);

/* text */
extern s32 DrawNormalText(f32 scale, char* text, s32 font);
extern void DrawTextKeepScale(f32 scale, s32 x, s32 y, s32 font, u32 rgb, char* text);
extern s32 DrawText(f32 scale, s32 x, s32 y, s32 font, u32 rgb, char* fmt, ...);
extern void dbgTextPrintfCol(s32 x, s32 line, char* fmt, ...);

/* scene nodes */
extern void fn_800BAD94(void* node, void* parent);
extern void CopyMat4(f32* src, void* node);
extern void fn_800BA2C4(void* node, s32 mask, s32 set);
extern void* fn_800BA368(void* node, s32 mask, s32 set);
extern void fn_800BA408(f32 r, f32 g, f32 b, void* node);
extern void fn_800BDE80(f32* mat, f32* out, f32* pos);
extern f32 fn_800BD938(f32* v);
extern s32 MBBackgroundLoading(void);

/* math / world */
extern void fn_8005A338(f32* mat, f32* fwd, f32* pos);
extern void fn_8005A3B8(f32* mat);
extern f32 get_actual_screen_pos(s32 a, f32* out1, f32* out2, f32* pos);
extern void fn_8002C53C(f32* mat);
extern s32 fn_8002F818(u32 flags);
extern void fn_800C02F4(u32 rgb);
extern void fn_800C0ADC(f32 a, f32 b);

/* tower / sounds / messages */
extern s32 towerBossStatus(s32 player, s32 which);
extern s32 towerGetLevelRecord(s32 player, s32 which);
extern void towerClearRuneNear(s32 player, s32 track);
extern void TowerCheckMessages(s32 a);
extern s32 sumnerSpeechActive(void);
extern f32 msgUpdate(void);
extern s32 msgPost(s32 code, s32 player, u32 arg);
extern s32 FindStringMessageListSub(s32 list, char* name);
extern void fn_8006D7EC(s32 a, s32 msg, s32 b, s32 c);
extern void CrystalCamActivate(void);
extern void fn_8009D610(s32 mode, f32* pos);
extern void AudioAmbientUpdate(void);
extern void fn_8009FE4C(s32 player);
extern void fn_8009FEA0(s32 player);
extern void fn_8009FEFC(s32 player);
extern void fn_8009CADC(s32 player, s32 a);
extern void fn_8009F028(u32 color, f32* pos, s32 heal, s32 d);
extern void update_player_milestone(void);
extern void fn_8005ACE0(void);
extern void fn_8005DE50(void* p, s32* req);
extern void fn_80090450(s32 player);

/* sfx */
extern s32 fn_80092794(f32 scale, f32 power, f32* pos, u32 flags, s16 player, s32 d);
extern s32 fn_80092DF4(f32 scale, f32 power, f32* pos, u32 flags, s16 player, s32 d);
extern void fn_80092B58(f32 a, f32 b, f32 c, f32 d, f32* pos, f32* vel, u32 flags, s32 p, s32 col);
extern s32 StartLevelUpFX(f32 scale, f32 power, s32 a, u32 flags, s16 player, s32 d);
extern s32 StartMagicHealFX(f32* pos);
extern s32 PlaceEffectOnFloor(s32 fx, f32* pos);
extern void SfxSetParent(s32 fx, void* node);

/* this TU, beyond the slice (0x80077BF0..) */
extern void fn_80077BF0(void* p);
extern void fn_80077D38(void* p, s32 loaded);
extern void fn_80077FBC(void* p, s32 it);
extern s32 fn_80078108(void);
extern void fn_80079100(s32 player);
extern void fn_80079484(s32 player);
extern void fn_80079D94(s32 player);
extern void fn_8007CC48(void* p, s32 a, s32* b);
extern void fn_8007E540(void* p, void* node);
extern void fn_8007F874(void* p, s32 chartype, s32* c);
extern void fn_8007FC80(void);
extern void mini_inventory_update(s32 player);

/* player display (this slice, forward decls) */
void setup_player_display(s32 i);
static void write_health_and_items(s32 i);
static void show_crystals(Player* p);
static void write_gold(s32 i, s32 show);
static void draw_power_meter(s32 i);
static void debug_player_pos(s32 i);
s32 get_display_mode(s32 i);
void del_player_blits(s32 i);
void ShowRuneStones(void);
static s32 ModifyExp(Player* p, s32 delta);
s32 LevelToExp(s32 lv);
void WritePlayerInfo(s32 pnum);

/* motion (PMOTION.OBJ) */
extern void PlayerMotion(void);
extern void PlayerMotion_SetAnimState(void* p);

/* enemy/anim */
extern void DoPlayerAction(void* p);
extern void fn_80088688(void* p);
extern void fn_80089120(void* p);

/* ------------------------------------------------------------------ */
/* attachment ops                                                      */
/* ------------------------------------------------------------------ */

/* tiny vec helper; auto-inlined everywhere, standalone copy deadstripped */
static void VecSub(f32* d, f32* a, f32* b) {
    d[0] = a[0] - b[0];
    d[1] = a[1] - b[1];
    d[2] = a[2] - b[2];
}

/* Query whether player i is mid-attack; level widens the accepted set. */
s32 PlayerAttacking(s32 i, s32 level) {
    Player* p = P(i);
    s32 action = p->action;

    if (action >= 11) {
        return 1;
    }
    if (level <= 1 && (action == 2 || action == 5 || action == 10)) {
        return 1;
    }
    if (level <= 0 && p->action > 1) {
        return 1;
    }
    return 0;
}

/* Detach the player from a carrier object (critter.c). */
void PlayerUnsetParent(Player* p) {
    *(f32*)(p->node + 0x30) = p->pos[0];
    *(f32*)(p->node + 0x34) = p->pos[1];
    *(f32*)(p->node + 0x38) = p->pos[2];
    fn_800BAD94(p->node, lbl_80344B2C);
    p->hud_flags &= ~0x20;
    p->obj_flags &= ~0x4000;
}

/* Release a grabbed player; restore != 0 restores the saved position. */
void PlayerUnsetGrabbed(Player* p, s32 restore) {
    u8 unused[64];

    if (restore == 1) {
        p->pos[0] = p->saved_pos[0];
        p->pos[1] = p->saved_pos[1];
        p->pos[2] = p->saved_pos[2];
    }
    *(f32*)(p->node + 0x30) = p->pos[0];
    *(f32*)(p->node + 0x34) = p->pos[1];
    *(f32*)(p->node + 0x38) = p->pos[2];
    fn_800BAD94(p->node, lbl_80344B2C);
    p->hud_flags &= ~0x20;
}

/* Attach the player under a carrier node at offset pos (critter.c). */
void PlayerSetParent(Player* p, void* parent, f32* pos) {
    f32 d[3];

    if (pos == NULL) {
        pos = lbl_80127D00;
    }
    VecSub(d, pos, p->anchor_pos);
    p->saved_pos[0] = p->pos[0];
    p->saved_pos[1] = p->pos[1];
    p->saved_pos[2] = p->pos[2];
    fn_800BAD94(p->node, parent);
    CopyMat4(lbl_80127D60, p->node);
    *(f32*)(p->node + 0x30) = d[0];
    *(f32*)(p->node + 0x34) = d[1];
    *(f32*)(p->node + 0x38) = d[2];
    fn_8005A338(p->mat, p->anchor_fwd, p->anchor_pos);
    p->hud_flags |= 0x20;
    p->obj_flags |= 0x4000;
}

/* Grab-attach the player under a node; pos != NULL places the node. */
void PlayerSetGrabbed(Player* p, void* parent, f32* pos) {
    p->saved_pos[0] = p->pos[0];
    p->saved_pos[1] = p->pos[1];
    p->saved_pos[2] = p->pos[2];
    fn_800BAD94(p->node, parent);
    CopyMat4(lbl_80127D60, p->node);
    if (pos != NULL) {
        *(f32*)(p->node + 0x30) = pos[0];
        *(f32*)(p->node + 0x34) = pos[1];
        *(f32*)(p->node + 0x38) = pos[2];
    }
    fn_8005A338(p->mat, p->anchor_fwd, p->anchor_pos);
    p->hud_flags |= 0x20;
}

/* ------------------------------------------------------------------ */
/* HUD writers                                                         */
/* ------------------------------------------------------------------ */

/* Hide every per-player HUD blit for player i. */
void del_player_blits(s32 i) {
    s32 j;

    for (j = 0; j < 6; j++) {
        mbBlitInit3414(frame_blit[i][j], 1);
    }
    for (j = 0; j < 12; j++) {
        mbBlitInit3414(rune_blit[i][j], 1);
    }
    for (j = 0; j < 8; j++) {
        mbBlitInit3414(crystal_blit[i][j], 1);
    }
    for (j = 0; j < 4; j++) {
        mbBlitInit3414(key_blit[i][j], 1);
    }
    mbBlitInit3414(hod_blit[i], 1);
    mbBlitInit3414(quest_blit[i], 1);
    mbBlitInit3414(rune13_blit[i], 1);
}

/* Per-frame HUD driver: one player, or all (< 0) + the "IT" tag. */
void WritePlayerInfo(s32 pnum) {
    s32 i;
    s32 first;
    s32 end;
    Player* p;
    u8 unused[8];

    if ((!(lbl_8034477C & 0x8000) || lbl_80344298 == 0) &&
        (pnum < 0 || (lbl_80344824 & (1 << pnum)))) {
        if (pnum >= 0) {
            first = pnum;
            end = pnum + 1;
        } else {
            first = 0;
            end = 4;
        }

        for (i = first, p = P(first); i < end; i++, p++) {
            if ((lbl_80344824 & (1 << i)) && !(p->hud_flags2 & 1)) {
                p->hud_flags2 |= 1;
                switch (p->display_mode) {
                case 0:
                case 3:
                    break;
                case 6:
                    write_health_and_items(i);
                    break;
                case 10:
                    write_health_and_items(i);
                    break;
                default:
                    write_health_and_items(i);
                    show_crystals(p);
                    break;
                }
            }
        }

        if (pnum < 0) {
            ShowRuneStones();
            if (lbl_80344B04 != NULL) {
                MBRemoveBlit(lbl_80344B04);
                lbl_80344B04 = NULL;
            }
            if (lbl_80344B24 >= 0 && (lbl_80344824 & (1 << lbl_80344B24))) {
                lbl_80344B04 = MBNewBlit("IT", lbl_80120238[lbl_80344B24] + 0x22, -349);
                mbBlitProject(lbl_80344B04, 0x20, 0x20);
                mbBlitCvtCoord(lbl_80344B04, 64000.0f);
            }
        }
    }
}

/* Corner ticker for the most recent crystal/boss-item/coin pickup. */
static void show_crystals(Player* p) {
    s32 type;
    s32 cnt;
    s32 total;
    s32 i;
    void* tex;

    if (lbl_80344B1C < 1 && options_state == 0 && lbl_8034477C == 0x4010 &&
        (f64)p->got_timer > 0.0) {
        i = p->index;
        p->got_timer = p->got_timer - lbl_80344590;
        total = lbl_803448F0;
        type = p->got_type;
        if (type < 0x200) {
            if (type < 0x100) {
                cnt = towerBossStatus(i, type);
                total = lbl_80124C70[type];
                sprintf(tbuf, "SM_CRYSTAL_%s", &lbl_80124C94[type * 8]);
            } else {
                type -= 0x100;
                cnt = towerGetLevelRecord(i, type);
                total = lbl_80124CDC[type];
                sprintf(tbuf, "SM_%s", &lbl_80124CE8[type * 0xE]);
            }
        } else {
            cnt = p->got_count;
            if (type - 0x200 < 0x10) {
                sprintf(tbuf, "16_%sCOIN", &lbl_801200B0[(type - 0x200) * 4]);
            } else {
                sprintf(tbuf, "16_SUM");
            }
        }
        tbuf[14] = 0;
        tex = MBOX_FindTexture(tbuf, NULL);
        MBNewTempBlit(tex, lbl_80120238[i] + 0x1C, 0x120, 0x10, 0x10);
        if (cnt < 0) {
            cnt = total;
        }
        sprintf(tbuf, "%d/%d", cnt, total);
        DrawTextKeepScale(1.5f, lbl_80120238[i] + 0x30, 0x124, 1, 0xFFFFFF, tbuf);
    }
}

/* Rune-stone / crystal collection icons; live while welcome_timer runs. */
void ShowRuneStones(void) {
    s32 i;
    s32 j;
    void* blit;
    s32 state;

    if (lbl_80344B1C < 1 || options_state != 0) {
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 8; j++) {
                blit = crystal_blit[i][j];
                if (blit != NULL) {
                    mbBlitInit3414(blit, 1);
                }
            }
        }
    } else {
        lbl_80344B1C -= lbl_8034457C;
        if (lbl_80344B1C < 0) {
            lbl_80344B1C = 0;
        }
        for (i = 0; i < 4; i++) {
            Player* p = P(i);

            state = p->state;
            if (!(p->hud_flags2 & 2)) {
                p->hud_flags2 |= 2;
                if ((u32)(state - 1) < 2 || (u32)(state - 4) < 2) {
                    for (j = 0; j < 8; j++) {
                        blit = crystal_blit[i][j];
                        if (blit != NULL) {
                            mbBlitInit3414(
                                blit,
                                (p->char_save[p->character].rune_stones & (1 << j)) == 0);
                        }
                    }
                } else {
                    for (j = 0; j < 8; j++) {
                        blit = crystal_blit[i][j];
                        if (blit != NULL) {
                            mbBlitInit3414(blit, 1);
                        }
                    }
                }
                if (!(lbl_8034477C & 0x8000) && (state == 1 || state == 5)) {
                    for (j = 0; j < 12; j++) {
                        blit = rune_blit[i][j];
                        if (blit != NULL) {
                            mbBlitInit3414(blit, (p->shards & (1 << j)) == 0);
                        }
                    }
                }
            }
        }
    }
}

/* Name/level/health/keys/potions writer for one player's HUD row. */
static void write_health_and_items(s32 i) {
    Player* p = P(i);
    s32 hidden;
    s32 show_gold;
    u32 rgb;
    f32 oldz;
    s32 mode;
    char buf[16];
    char buf2[44];
    void* blit;

    hidden = 0;
    show_gold = 1;
    rgb = lbl_801201C8[p->class_id];
    mini_inventory_update(i);
    oldz = MBSetFontZ(63990.0f);
    if (lbl_80344A28 != 0 || lbl_802575B0 != 0) {
        hidden = 1;
    }
    if (lbl_8034477C == 0x4010 && lbl_80344760 > 0 && p->state == 0xB &&
        p->motion_state == 1) {
        u32 x = lbl_80120238[i];

        hidden = 1;
        MBNewTempBlit((void*)lbl_80344E48, x + 6, 0x14C, 0xE, 0xE);
        MBNewTempBlit((void*)lbl_80344E44, x + 6, 0x160, 0xE, 0xE);
        DrawTextKeepScale(1.2f, x + 0x14, 0x150, 1, 0xFFFFFF, "Wait In Tower");
        DrawTextKeepScale(1.2f, x + 0x14, 0x164, 1, 0xFFFFFF, "Quit Game");
        show_gold = 0;
    }
    if ((p->state == 5 || p->state == 0xB) && lbl_8034477C != 0x400D &&
        lbl_8034477C != 0x4013 && lbl_8034477C != 0x4017 && !hidden) {
        hidden = 1;
        setup_player_display(i);
        if (p->state == 0xB) {
            DrawTextKeepScale(1.2f, -lbl_80120240[i], 0x154, 1, rgb, "IN TOWER");
        } else {
            hidden = 0;
        }
    }
    if (show_gold && lbl_80344B00 == 0) {
        write_gold(i, 1);
        if (frame_blit[i][5] != NULL) {
            s32 w;

            if ((f64)p->health > 9999.0) {
                p->health = 9999.0f;
            }
            sprintf(buf, "%d", (s32)p->health);
            w = DrawNormalText(1.0f, buf, 4);
            DrawText(1.0f, (lbl_80120238[i] + 0x74) - w, 0x167, 4,
                     lbl_801201C8[p->class_id], buf);
            mbBlitInit3414(frame_blit[i][5], 0);
        }
    } else {
        write_gold(i, 0);
        if (frame_blit[i][5] != NULL) {
            if ((f64)p->health > 9999.0) {
                p->health = 9999.0f;
            }
            mbBlitInit3414(frame_blit[i][5], 1);
        }
    }
    mode = p->display_mode;
    if (mode == 6) {
        if (!hidden) {
            DrawTextKeepScale(0.667f, -lbl_80120240[i], 0x153, 7, rgb, p->name);
        }
    } else if (mode < 6) {
        if (mode < 3) {
            if (mode < 1) {
                goto tail;
            }
            if (lbl_802575B0 != 0 && p->state == 1) {
                hidden = 1;
                if (lbl_802575B0 == 1) {
                    debug_player_pos(i);
                } else {
                    DrawText(1.0f, lbl_80120238[i] + 8, 0x154, 1, 0xFFFFFF, "XP: %d",
                             p->exp);
                }
            }
        } else if (mode < 5) {
            goto tail;
        }
        if (!hidden) {
            DrawTextKeepScale(0.667f, -lbl_80120240[i], 0x153, 7, rgb, p->name);
        }
        if (lbl_80344A28 != 0 || !hidden) {
            sprintf(buf2, "LV %d", p->level);
            DrawText(1.0f, -lbl_80120240[i], 0x146, 1, 0xFFFFFF, buf2);
        }
    }
tail:
    if (p->state != 2 && p->display_mode != 0 && lbl_80344B00 == 0) {
        if (p->item_body_lo > 0) {
            blit = MBNewTempBlit((void*)lbl_80344AFC, lbl_80120238[i] + 8, 0x143, -1, -1);
            mbBlitCvtCoord(blit, 64000.0f);
            sprintf(buf2, "%d", p->item_body_lo);
            DrawTextKeepScale(0.8f, lbl_80120238[i] + 0x1A, 0x147, 4, rgb, buf2);
        }
        if (p->item_body_hi > 0) {
            blit = MBNewTempBlit(potionicon_tab[PF(p, 0x32FC + p->item_body_hi * 4, s32)],
                                 lbl_80120238[i] + 0x66, 0x143, -1, -1);
            mbBlitCvtCoord(blit, 64000.0f);
            sprintf(buf2, "%d", p->item_body_hi);
            DrawTextKeepScale(0.8f, lbl_80120238[i] + 0x5C, 0x147, 4, rgb, buf2);
        }
    }
    if (p->health <= 0.0f || lbl_80344A44 != 0) {
        s32 j;

        for (j = 0; j < 7; j++) {
            mbBlitInit3414(pm_blit[i][j], 1);
        }
    } else {
        if (lbl_8034477C == 0x4010 && p->quest_state != 0 && lbl_803448D8 != 0xD) {
            mbBlitInit3414(quest_blit[i], 0);
        } else {
            mbBlitInit3414(quest_blit[i], 1);
        }
        if ((u32)(lbl_8034477C - 0x400F) < 2 && (p->shards & 0x1000)) {
            mbBlitInit3414(hod_blit[i], 0);
        } else {
            mbBlitInit3414(hod_blit[i], 1);
        }
        if (lbl_8034477C == 0x4010) {
            draw_power_meter(i);
        }
    }
    MBSetFontZ(oldz);
}

/* Debug HUD: floor name, position, facing. */
static void debug_player_pos(s32 i) {
    f32 ang[2];
    f32 y;
    f32 out1[5];
    f32 out2;
    char* name;
    s32 oldflags;

    if (lbl_8034477C == 0x4010) {
        fn_800C02F4(0x80FF80);
        get_actual_screen_pos(0, &out2, out1, P(i)->col_pos);
        lbl_80344F60 = 1;
        name = P(i)->floor_name;
        if (name == NULL || lbl_80240E50[i * 0xF] == 0.0f) {
            name = "NO FLOOR";
            if (P(i)->floor_name2 != NULL) {
                name = P(i)->floor_name2;
            }
        }
        oldflags = MBSetFontFlags(0x40000);
        y = 330.0f;
        DrawText(1.0f, lbl_80120238[i] + 8, (s32)y, 1, 0xFFFFFF, name);
        y += 10.0f;
        sprintf(tbuf, "%.1Lf %.1Lf %.1Lf", P(i)->pos[0], P(i)->pos[1],
                P(i)->pos[2]);
        DrawText(1.0f, lbl_80120238[i] + 8, (s32)y, 1, 0xFFFFFF, tbuf);
        y += 10.0f;
        MBWorldToScreen(ang, P(i)->pos);
        sprintf(tbuf, "%.0Lf %.0Lf", ang[0], ang[1]);
        DrawText(1.0f, lbl_80120238[i] + 8, (s32)y, 1, 0xFFFFFF, tbuf);
        MBSetFontFlags(oldflags);
    }
}

/* Gold counter, right-aligned, 99999 cap. */
static void write_gold(s32 i, s32 show) {
    void** blit = &frame_blit[i][4];
    char buf[20];
    s32 w;
    Player* p;

    if (*blit != NULL) {
        p = P(i);
        if (p->gold > 99999) {
            p->gold = 99999;
        }
        if (show != 0) {
            sprintf(buf, "%d", p->gold);
            w = DrawNormalText(1.0f, buf, 4);
            DrawText(1.0f, (lbl_80120238[i] + 0x3C) - w, 0x167, 4,
                     lbl_801201C8[p->class_id], buf);
            mbBlitInit3414(*blit, 0);
        } else {
            mbBlitInit3414(*blit, 1);
        }
    }
}

/* Turbo/power meter: bar scale + color, charge flash, drain flash. */
static void draw_power_meter(s32 i) {
    Player* p = P(i);
    s32 j;
    u32 zone0;
    u32 zone;
    f32 pw;
    f64 frac;
    s32 state;
    u32 rgb;
    u32 rgb2;
    u16* tex;
    s32 w;

    for (j = 0; j < 7; j++) {
        mbBlitInit3414(pm_blit[i][j], 1);
    }
    if ((f64)(f32)(0.01 * p->power_level) >= 0.4) {
        if ((f64)(f32)(0.01 * p->power_level) >= 0.99) {
            zone0 = 3;
        } else {
            zone0 = 2;
        }
    } else {
        zone0 = 1;
    }
    if ((f64)p->power_target > 100.0) {
        p->power_target = 100.0f;
    }
    if (p->power_level <= p->power_target) {
        p->power_level = p->power_level + (f32)lbl_8034457C;
        if (p->power_target < p->power_level) {
            p->power_level = p->power_target;
        }
    } else {
        p->power_level = p->power_level - (f32)(lbl_8034457C << 1);
        if (p->power_level < p->power_target) {
            p->power_level = p->power_target;
        }
    }
    frac = (f32)(0.01 * p->power_level);
    if (frac >= 0.4) {
        if (frac >= 0.99) {
            zone = 3;
            frac = 1.0f;
        } else {
            zone = 2;
            frac = (f32)((f32)(frac - 0.4f) / 0.6f);
        }
    } else {
        zone = 1;
        frac = (f32)(frac / (f64)0.4f);
    }
    if (zone0 == zone && zone0 == 3 && p->meter_flash == 0) {
        zone0 = -1;
    }
    if (zone0 != zone) {
        if (zone == 3) {
            p->meter_flash = 2;
        } else {
            p->meter_flash = 1;
        }
        p->meter_timer = 0;
    }
    mbBlitInit3414(pm_blit[i][1], 0);
    mbBlitInit3414(pm_blit[i][0], 0);
    mbBlitInit3414(pm_blit[i][2], 0);
    state = p->meter_flash;
    if (state == 1) {
        j = p->meter_timer >> 2;
        if (j > 4 && j < 10) {
            j = 4 - (j - 5);
        }
        if (j < 5) {
            mbBlitInit3414(pm_blit[i][6], 0);
            mbInitBlitEntry(pm_blit[i][6], pm_frames, j);
        } else {
            p->meter_flash = 0;
        }
    } else if (state < 1) {
        if (state >= 0) {
            u32 a = (u32)(127.0 * frac + 128.0);

            rgb = 0;
            rgb2 = 0;
            if (zone == 1) {
                rgb = ((a & 0xFF) << 16) | ((a & 0xFF) << 8);
                rgb2 = 0;
            } else if (zone == 0) {
                rgb = (a & 0xFF) << 8;
                rgb2 = 0;
            } else if (zone < 4) {
                rgb = (a & 0xFF) << 16;
                rgb2 = 0xFFFF00;
            }
            tex = (u16*)MBRomTexPtr(PF((u8*)pm_blit[i][0], 4, u32));
            w = (s32)((f32)tex[5] * frac) >> 1;
            if (w < 1) {
                w = 1;
            }
            mbBlitCalcWidth(pm_blit[i][0],
                            (pm_bar_x + i * 0x80 + ((s32)tex[5] >> 1)) - w, pm_bar_y,
                            (f32)pm_bar_z);
            mbBlitProject(pm_blit[i][0], w << 1, 0);
            mbBlitCalcWidth(pm_blit[i][1], pm_frame_x + i * 0x80, pm_frame_y,
                            (f32)pm_frame_z);
            fn_800B2940(pm_blit[i][0], rgb);
            fn_800B2940(pm_blit[i][1], rgb2);
        }
    } else if (state < 3) {
        j = (p->meter_timer << 9) / 0x78;
        if (j > 0xFF && j < 0x200) {
            j = 0x1FF - j;
        }
        if (j < 0x100) {
            mbBlitInit3414(pm_blit[i][3], 0);
            fn_800B290C(pm_blit[i][3], 0xFF - j);
        } else {
            p->meter_timer = 0;
        }
        fn_800B2940(pm_blit[i][0], 0xFF0000);
        fn_800B2940(pm_blit[i][1], 0xFF0000);
    }
    p->meter_timer = p->meter_timer + lbl_8034457C;
}

/* Rebuild the 6 portrait-frame blits for a player's display mode. */
void setup_player_display(s32 i) {
    u16 x = lbl_80120238[i];
    s32 mode;
    s32 cls;
    s32 chr;
    u32 frames;
    char buf[40];

    mode = get_display_mode(i);
    cls = P(i)->class_id;
    del_player_blits(i);
    chr = P(i)->character;
    P(i)->display_mode = mode;
    frames = (u32)MBOX_FindTexture_Err("S3", NULL, 1);
    mbInitBlitEntry(frame_blit[i][0], frames, 0);
    mbBlitInit3414(frame_blit[i][0], 0);
    frames = (u32)MBOX_FindTexture_Err("S4", NULL, 1);
    mbInitBlitEntry(frame_blit[i][1], frames, 0);
    mbBlitInit3414(frame_blit[i][1], 0);
    if (P(i)->state == 0) {
        fn_800B2940(frame_blit[i][1], lbl_801201E8[cls]);
    } else {
        fn_800B2940(frame_blit[i][1], lbl_801201D8[cls]);
    }
    frames = (u32)MBOX_FindTexture_Err("S4_FRAME", NULL, 1);
    mbInitBlitEntry(frame_blit[i][2], frames, 0);
    mbBlitInit3414(frame_blit[i][2], 0);
    frames = (u32)MBOX_FindTexture_Err("coin", NULL, 1);
    mbInitBlitEntry(frame_blit[i][4], frames, 0);
    mbBlitInit3414(frame_blit[i][4], 0);
    mbBlitCalcWidth(frame_blit[i][4], x + 6, 0x165, 63990.0f);
    frames = (u32)MBOX_FindTexture_Err("heart", NULL, 1);
    mbInitBlitEntry(frame_blit[i][5], frames, 0);
    mbBlitInit3414(frame_blit[i][5], 0);
    mbBlitCalcWidth(frame_blit[i][5], x + 0x3D, 0x165, 63990.0f);
    if (mode == 4) {
        goto done;
    }
    if (mode < 4) {
        if (mode != 0) {
            if (mode < 0) {
                goto done;
            }
            if (mode < 3) {
                goto active;
            }
        }
        mbBlitInit3414(frame_blit[i][4], 1);
        mbBlitInit3414(frame_blit[i][5], 1);
    } else {
        if (mode == 6) {
            chr = P(i)->respawn_char;
        } else if (mode > 5) {
            goto done;
        }
    active:
        mbBlitInit3414(frame_blit[i][4], 0);
        mbBlitInit3414(frame_blit[i][5], 0);
        sprintf(buf, "BK_RUNE_STONE_02");
        frames = (u32)MBOX_FindTexture_Sub(buf, NULL, 0, 0, 1);
        mbInitBlitEntry(frame_blit[i][0], frames, 0);
        sprintf(buf, "S4_%s", &lbl_801200B0[chr * 4]);
        frames = (u32)MBOX_FindTexture_Err(buf, NULL, 1);
        mbInitBlitEntry(frame_blit[i][1], frames, 0);
        if (!(lbl_8034477C & 0x8000)) {
            s32 j;

            for (j = 0; j < 12; j++) {
                mbBlitInit3414(rune_blit[i][j],
                               (P(i)->shards & (1 << j)) == 0);
            }
        }
        if (lbl_80344824 & (1 << i)) {
            mbBlitInit3414(rune13_blit[i], 0);
        }
        frames = (u32)MBOX_FindTexture_Err("QUEST_ICON", NULL, 0);
        mbInitBlitEntry(quest_blit[i], frames, 0);
    }
done:
    if (lbl_8034477C == 0x400B || lbl_8034477C == 0x4012) {
        mbBlitUpdateEntry(frame_blit[i][0], 0xFFFFFFFF, 0x4010);
        mbBlitUpdateEntry(frame_blit[i][2], 0xFFFFFFFF, 0x4010);
    } else {
        mbBlitUpdateEntry(frame_blit[i][0], 0xFFFFBFEF, 0);
        mbBlitUpdateEntry(frame_blit[i][2], 0xFFFFBFEF, 0);
    }
}

/* Map player state (+ motion state) to a HUD display mode. */
s32 get_display_mode(s32 i) {
    s32 m;

    switch (P(i)->state) {
    case 3:
        return 2;
    case 2:
        m = P(i)->motion_state;
        if (m != 4) {
            if (m < 4) {
                if (m != 0 && m >= 0) {
                    if (m < 3) {
                        return 1;
                    }
                    return 3;
                }
            } else if (m < 0xF && m > 9) {
                return 1;
            }
            return 0;
        }
        return 6;
    case 1:
        break;
    case 11:
        return 10;
    case 4:
    case 5:
        break;
    default:
        return 0;
    }
    if (lbl_8034477C == 0x4012) {
        return 5;
    }
    if (P(i)->node != NULL) {
        return 1;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* experience / gold                                                   */
/* ------------------------------------------------------------------ */

/* Give exp; mode -2 scales by level bracket, mode 1 charges the power
 * meter, mode >= 0 forwards into an attached familiar. */
s32 AddExp(s32 pnum, s32 amount, s32 mode) {
    Player* p = P(pnum);
    s32 res;

    if (mode == -2) {
        s32 lv = p->level;
        s32 delta;

        if (amount < 0 && lv == 99) {
            return 0;
        }
        if (lv < 0x3D) {
            delta = (lv - 1) * 0x3C + 1000;
        } else {
            delta = 0x11F8;
        }
        amount = amount * (s32)(0.01 * (f32)delta);
        mode = -2;
    } else {
        f32 dist = PF(lbl_8034483C, 0x9C, f32);
        f32 fac = PF(lbl_8034483C, 0xA0, f32);

        if (dist > 0.0f) {
            if ((f32)p->level > dist) {
                fac = fac * (1.0 / (0.1 * ((f32)p->level - dist) + 1.0));
            }
        }
        amount = (s32)((f32)amount * fac);
        if (lbl_803445CC & 0x10) {
            amount = (s32)((f32)amount * 5.0);
        }
    }
    res = ModifyExp(p, amount);
    if (mode == 1) {
        if (p->action < 0xB) {
            p->power_target = (f32)(0.025 * (f32)amount + p->power_target);
        }
        if ((f64)p->power_target > 100.0) {
            p->power_target = 100.0f;
        }
    }
    if (res != 0) {
        if (amount < 0) {
            fn_8009CADC(pnum, -1);
        } else if (amount > 0) {
            f32* pos = p->col_pos;
            s32 fx;

            msgPost(0x22, pnum, (u32)pos);
            fx = StartLevelUpFX(0.0f, 0.0f, 0, p->class_id, (s16)(u32)pos, 0);
            SfxSetParent(fx, p->node);
            p->got_timer = (f32)(p->got_timer + 100.0);
        }
    }
    if (mode >= 0 && PF(p, 0x6B8, s32*) != NULL) {
        AddExp(*PF(p, 0x6B8, s32*), amount, -1);
    }
    return res;
}

/* Raw exp delta + level-up/down walk; the level curve is written inline
 * (matches the Xbox LevelDeltaExp fold). */
static s32 ModifyExp(Player* p, s32 delta) {
    s32 res = 0;
    s32 need;

    p->exp = p->exp + delta;
    if (p->exp < 0) {
        p->exp = 0;
    }
    if (delta > 0) {
        need = p->level;
        if (need + 1 <= 60) {
            need = need * ((need + 1) * 30 + 1000);
        } else {
            need = (need - 59) * 4600 + 0x28550;
        }
        while (need <= p->exp && p->level < 99) {
            res = 1;
            p->level = p->level + 1;
            fn_8007F874(p, p->character, NULL);
            need = p->level;
            if (need + 1 <= 60) {
                need = need * ((need + 1) * 30 + 1000);
            } else {
                need = (need - 59) * 4600 + 0x28550;
            }
        }
    } else if (delta < 0) {
        while ((need = p->level) > 1) {
            if (need <= 60) {
                need = (need - 1) * (need * 30 + 1000);
            } else {
                need = (need - 60) * 4600 + 0x28550;
            }
            if (need <= p->exp) {
                break;
            }
            res = -1;
            p->level = p->level - 1;
            fn_8007F874(p, p->character, NULL);
        }
    }
    return res;
}

/* Inverse of LevelToExp: scan 99..1 for the level exp buys (running
 * rate = lv*30 maintained incrementally, 99-step guard). */
s32 ExpToLevel(s32 exp) {
    s32 lv = 99;
    s32 rate = 2970;
    s32 need;
    s32 guard;

    for (guard = 99; guard != 0; guard--) {
        if (lv <= 60) {
            need = (lv - 1) * (rate + 1000);
        } else {
            need = (lv - 60) * 4600 + 0x28550;
        }
        if (exp >= need) {
            return lv;
        }
        lv--;
        rate -= 30;
    }
    return 1;
}

/* Total exp needed to reach a level. */
s32 LevelToExp(s32 lv) {
    if (lv <= 60) {
        return (lv - 1) * (lv * 30 + 1000);
    }
    return 0x28550 + (lv - 60) * 4600;
}

/* Gold add, capped at 99999. */
void PlayerGiveGold(s32 i, s32 amount) {
    Player* p = P(i);

    p->gold = p->gold + amount;
    if (p->gold > 99999) {
        p->gold = 99999;
    }
}

/* ------------------------------------------------------------------ */
/* magic                                                               */
/* ------------------------------------------------------------------ */

/* Launch potion-magic FX + sound for a player (pnum < 0: generic). */
void start_magic(s32 pnum, f32* pos, u32 flags, s32 mode) {
    Player* p;
    f32 scale;
    f32 power;
    u32 color;
    s32 fx;
    f32 vpos[3];
    f32 vel[3];
    f32 pw;

    scale = 1.0f;
    p = NULL;
    if (pnum < 0) {
        scale = 0.8f;
        power = (f32)(20.0 * lbl_80344594);
    } else {
        p = P(pnum);
        power = (f32)(p->magic_power * lbl_80344594);
        if (pnum == fn_8002F818(flags)) {
            power = (f32)(power * 1.1);
            scale = (f32)(scale + 0.1);
        }
        if (p->level > 0x18) {
            flags |= 0x800000;
        }
    }
    color = flags & 0xF;
    if (color == 0) {
        color = (lbl_80344B14 % 4) + 1;
        flags |= color;
        lbl_80344B14++;
    }
    flags |= 0x200;
    if (pnum < 0) {
        mode = 0;
    }
    if (mode < 2) {
        if (mode == 1) {
            fx = fn_80092794((f32)(25.0 * scale), (f32)(0.25 * power), pos, flags | 0x200,
                             (s16)pnum, mode);
            fn_800BAD94(*(void**)(lbl_80285BCC + fx * 0xF0), p->node);
            if (pnum >= 0) {
                fn_8009F028(color, pos, 1, mode);
            }
        } else {
            fx = fn_80092DF4((f32)(40.0 * scale), power, pos, flags | 0x200, (s16)pnum, mode);
            PlaceEffectOnFloor(fx, NULL);
            if (pnum >= 0) {
                fn_8009F028(color, pos, 0, mode);
            }
        }
    } else {
        f32 h = (f32)(1.5 * -(f32)p->throw_str + 5.0);

        vpos[0] = (f32)(2.0 * p->mat[8] + pos[0]);
        vpos[2] = (f32)(2.0 * p->mat[10] + pos[2]);
        vpos[1] = (f32)((f32)(2.0 * p->mat[9] + pos[1]) + 4.0);
        vel[0] = (f32)(p->mat[8] * 0.707) * h;
        vel[1] = 0.707f * h;
        vel[2] = (f32)(p->mat[10] * 0.707) * h;
        fn_80092B58(100.0f, (f32)(40.0 * scale), (f32)(0.75 * power), 1.5, vpos, vel,
                    flags | 0x200, pnum, color + 7);
    }
    if (p != NULL && p->level > 0x4A && mode != 1) {
        fx = StartMagicHealFX(NULL);
        SfxSetParent(fx, p->node);
    }
}

/* ------------------------------------------------------------------ */
/* master driver                                                       */
/* ------------------------------------------------------------------ */

/* Per-frame driver for all four players: welcome/demo speech, beacon
 * light decay, state machine per player (select/dead/ghost/tower),
 * action + motion, "IT" search, speech round-robin, ambient audio. */
void do_players(void) {
    s32 i;
    s32 j;
    s32 it;
    f32 best;
    Player* p;
    s32 loaded;
    s32 state;

    loaded = 0;
    it = -1;
    best = 9999.0f;
    if (lbl_803445CC & 0x10) {
        lbl_80344AF8 = 0xFFFFFFFF;
    }
    if (lbl_8034477C == 0x4010 && (s32)lbl_80344AF8 >= 0) {
        if (lbl_803447BC == 0 && lbl_803447B8 == 0) {
            lbl_80344B10 += lbl_8034457C;
            if (lbl_80344B10 > 0) {
                if (lbl_803449A0 == 0) {
                    if (lbl_803448D8 == 0xD) {
                        if (lbl_80344C60 != 0) {
                            fn_8006D7EC(-1, FindStringMessageListSub(0, "WelcomeMessage"), -1, -1);
                            lbl_80344C90 = 6;
                            lbl_80344C54 = 1;
                            lbl_80344C5C = lbl_80344594;
                            CrystalCamActivate();
                        }
                        if (lbl_803448AC == 6 && lbl_803448A8 == 1) {
                            fn_8006D7EC(-1, FindStringMessageListSub(0, "GarmMessage"), -1, -1);
                        }
                    }
                    lbl_80344AF8 = 0xFFFFFFFF;
                } else if (lbl_803448D8 == 0xD) {
                    if (lbl_80344C4C == 1 && !(lbl_80344AF8 & 1)) {
                        fn_8006D7EC(-1, FindStringMessageListSub(0, "DemoWelcome"), 0, -1);
                        lbl_80344AF8 |= 1;
                    }
                } else if (lbl_803448D8 != 0xC && (lbl_80344AF8 & 2)) {
                    fn_8006D7EC(-1, FindStringMessageListSub(0, "DemoLevel"), 0, -1);
                    lbl_80344AF8 &= ~2;
                }
            }
        } else {
            lbl_80344B10 = 0;
        }
    }
    if (lbl_8034477C != 0x4012 && lbl_8034477C != 0x400D && lbl_8034477C != 0x400F &&
        lbl_8034477C != 0x4016) {
        msgUpdate();
        for (i = 0, p = P(0); i < 4; i++, p++) {
            if (p->state != 0) {
                s32 sel = (p->state == 2 || p->state == 3);

                if (!sel) {
                    fn_8005A338(p->mat, p->anchor_fwd, p->anchor_pos);
                    if (p->platform != NULL && *p->platform != 0) {
                        fn_800BDE80((f32*)(*p->platform + 0x30), p->beacon_pos,
                                    p->mat);
                        p->beacon_pos[1] = 0.0f;
                        p->beacon_pos[0] = p->pos[0] + p->beacon_pos[0];
                        p->beacon_pos[1] = p->pos[1] + p->beacon_pos[1];
                        p->beacon_pos[2] = p->pos[2] + p->beacon_pos[2];
                    }
                }
            }
        }
        if (lbl_80344568 != 0 || lbl_80344770 != 0) {
            WritePlayerInfo(-1);
            return;
        }
        if (lbl_803443B4 == 1) {
            WritePlayerInfo(-1);
            for (i = 0, p = P(0); i < 4; i++, p++) {
                p->select_timer = p->select_timer + lbl_80344590;
            }
            return;
        }
        if (lbl_8034477C == 0x4010) {
            TowerCheckMessages(0);
        }
        if (lbl_803447BC != 0) {
            if (lbl_803447B8 != 0) {
                for (i = 0, p = P(0); i < 4; i++, p++) {
                    if (p->count_91C != 0) {
                        p->count_91C = p->count_91C - 1;
                    }
                    if (p->state == 1 && p->platform != NULL) {
                        if (lbl_8034477C == 0x4010) {
                            p->intower = 1;
                        }
                        DoPlayerAction(p);
                    }
                    p->anim_20C = 0;
                    if (!(p->hud_flags2 & 0x20)) {
                        fn_8005A3B8(p->mat);
                    }
                }
            }
            WritePlayerInfo(-1);
        }
        if (lbl_803447B8 == 0 || lbl_8034481C != 0 || lbl_80344AF0 != 0) {
            lbl_80344804 = 0;
            for (i = 0, p = P(0); i < 4; i++, p++) {
                f32 mag;
                f32 len;

                mag = (f32)(0.5 * p->light_range);
                len = fn_800BD938(p->light_vel);
                if (len > mag) {
                    len = mag;
                }
                p->light_vec[0] = p->light_vel[0] * len;
                p->light_vec[1] = p->light_vel[1] * len;
                p->light_vec[2] = p->light_vel[2] * len;
                len = (f32)(len * 0.667);
                if (len < 0.01) {
                    len = 0.0f;
                }
                p->light_vel[0] = p->light_vel[0] * len;
                p->light_vel[1] = p->light_vel[1] * len;
                p->light_vel[2] = p->light_vel[2] * len;
                if (!(p->hud_flags & 4)) {
                    p->hud_flags &= ~8;
                } else {
                    p->hud_flags |= 8;
                    p->hud_flags &= ~4;
                }
                if (p->state == 1 && p->health < best) {
                    it = i;
                    best = p->health;
                }
                if ((p->state == 4 && !(lbl_80240E34[i * 0xF] & 0xFF)) ||
                    lbl_8034481C > 2) {
                    lbl_80344804 = 1;
                }
            }
            lbl_803444FC = 0;
            lbl_803444F4 = 1;
            lbl_803444E4 = 0;
            loaded = fn_80078108();
            if (!(lbl_803445CC & 4)) {
                dbgTextPrintfCol(2, 2, "TRANSMITTER: CT=%02X, C1=%02X, C2=%02X, RATIO=%4.2f",
                                 lbl_80344508, lbl_80344510, lbl_8034450C, lbl_80344504);
            }
        }
    }
    for (i = 0, p = P(0); i < 4; i++, p++) {
        get_actual_screen_pos(0, &p->floor_hi, &p->floor_lo, p->col_pos);
        if (p->floor_hi < -2000.0) {
            p->floor_hi = -2000.0f;
        } else if (p->floor_hi > 2000.0) {
            p->floor_hi = 2000.0f;
        }
        if (p->floor_lo < -2000.0) {
            p->floor_lo = -2000.0f;
        } else if (p->floor_lo > 2000.0) {
            p->floor_lo = 2000.0f;
        }
    }
    for (i = 0, p = P(0); i < 4; i++, p++) {
        if (lbl_8034477C == 0x400B || lbl_8034477C == 0x400F) {
            continue;
        }
        state = p->state;
        if (state == 2 || state == 3) {
            goto common;
        }
        if (lbl_8034477C == 0x4012 || lbl_8034477C == 0x400D || lbl_8034477C == 0x400F ||
            lbl_8034477C == 0x4016) {
            continue;
        }
        if (lbl_803447B8 == 0 || lbl_8034481C != 0 || lbl_80344AF0 != 0) {
        common:
            if (p->count_91C != 0) {
                p->count_91C = p->count_91C - 1;
            }
            if (p->timer_1F0 > 0) {
                p->timer_1F0 = p->timer_1F0 - lbl_8034457C;
            }
            if (p->timer_1FA > 0) {
                p->timer_1FA = p->timer_1FA - lbl_8034457C;
            }
            if (p->timer_1FC > 0) {
                p->timer_1FC = p->timer_1FC - lbl_8034457C;
            }
            if (p->timer_1FE > 0) {
                p->timer_1FE = p->timer_1FE - lbl_8034457C;
            }
            if (p->vibe_on == 1) {
                if (p->vibe_timer2 == 0) {
                    p->vibe_timer = p->vibe_timer + lbl_8034457C;
                } else {
                    p->vibe_timer2 = p->vibe_timer2 + lbl_8034457C;
                }
            } else {
                p->vibe_timer = 0;
                p->vibe_timer2 = 0;
            }
            state = p->state;
            switch (state) {
            case 0:
                if (p->respawn_timer > 0) {
                    p->respawn_timer = p->respawn_timer - lbl_8034457C;
                    if (p->respawn_timer < 1) {
                        setup_player_display(i);
                    }
                }
                break;
            case 2:
                fn_80088688(p);
                for (j = 0; j < 4; j++) {
                    s32 st;

                    if (j != i && (st = P(j)->state) != 0 && st != 2 && st != 3) {
                        break;
                    }
                }
                break;
            case 3:
                fn_80088688(p);
                if (MBBackgroundLoading() == 0 && lbl_8034477C != 0x4013 &&
                    lbl_8034477C != 0x4017) {
                    fn_80079D94(i);
                } else if (lbl_802575BC == 0 ||
                           (lbl_8034477C != 0x400B && lbl_8034477C != 0x400D &&
                            lbl_8034477C != 0x400F && lbl_8034477C != 0x4016)) {
                    fn_80090450(i);
                    WritePlayerInfo(i);
                }
                for (j = 0; j < 4; j++) {
                    s32 st;

                    if (j != i && (st = P(j)->state) != 0 && st != 2 && st != 3) {
                        break;
                    }
                }
                break;
            case 4:
                if (p->prev_state != 4) {
                    fn_800BA2C4(p->node, 2, 0);
                    if (PF(p, 0x6C8, void*) != NULL) {
                        fn_800BA368(PF(p, 0x6C8, void*), 2, 0);
                    }
                }
                if (loaded != 0 || lbl_803447B4 != 0) {
                    s32 any = 0;

                    if (lbl_803447B4 == 0) {
                        for (j = 0; j < 4; j++) {
                            s32 st = P(j)->state;

                            if (st == 1 || st == 8) {
                                any = 1;
                                break;
                            }
                        }
                    }
                    if (!any) {
                        fn_80077BF0(p);
                        fn_80089120(p);
                        fn_80077D38(p, loaded);
                        break;
                    }
                }
                if (p->idle_timer > 599) {
                    p->idle_timer = p->idle_timer - 600;
                    fn_8009FE4C(i);
                }
                /* fallthrough */
            case 1:
                if (lbl_8034477C == 0x4010) {
                    p->intower = 1;
                    PF(p, 0xC28 + p->character * 0x1C, f32) =
                        PF(p, 0xC28 + p->character * 0x1C, f32) + (f32)lbl_8034457C;
                    if (PF(lbl_8034483C, 0, u32) & 8) {
                        fn_800C0ADC(lbl_80343D74, lbl_80343D70);
                    }
                }
                if ((u32)(lbl_8034489C - 2) < 2) {
                    if (p->quest_state == 0) {
                        p->pulse_7FC = 0.8f;
                    } else {
                        if (p->quest_state == 1) {
                            p->quest_state = 2;
                            towerClearRuneNear(i, lbl_803448D8);
                        }
                        p->pulse_7FC = 2.0f;
                    }
                }
                fn_8002C53C(p->mat);
                if ((lbl_803448D8 != 0xD || sumnerSpeechActive() == 0) && lbl_803443B4 == 0 &&
                    lbl_80344A30 == 0 && lbl_80344CC4 == 0 && p->name_timer > 0 &&
                    lbl_80344568 == 0 && lbl_80344770 == 0) {
                    char name[8 + 1];
                    f32 spos[2];

                    p->name_timer = p->name_timer - lbl_8034457C;
                    if (p->name_timer < 1) {
                        p->name_timer = 0;
                    }
                    for (j = 0; j < 8; j++) {
                        name[j] = p->name[j];
                        if (name[j] == '_') {
                            name[j] = ' ';
                        }
                    }
                    name[8] = 0;
                    MBWorldToScreen(spos, p->col_pos);
                    DrawTextKeepScale(0.5f, -(s32)spos[0], (s32)spos[1], 7, 0xFFFFFF, name);
                }
                if (p->prev_state != 1) {
                    fn_800BA2C4(p->node, 2, 0);
                    if (PF(p, 0x6C8, void*) != NULL) {
                        if (lbl_803448D8 == 0xC && lbl_803448D4 == 8) {
                            fn_800BA368(PF(p, 0x6C8, void*), 2, 1);
                        } else {
                            fn_800BA368(PF(p, 0x6C8, void*), 2, 0);
                        }
                    }
                }
                update_player_milestone();
                fn_8007CC48(p, 0, NULL);
                if ((lbl_803445CC & 0x10) && lbl_802575B0 != 0 && i == 0 && lbl_8034439C < 0) {
                    fn_8005ACE0();
                }
                PlayerMotion();
                if (p->fall_time > 0.0 && p->fall_time + 2.0 < lbl_80344594) {
                    if (p->fall_frames < 0x2D) {
                        if (p->fall_frames > 0x1D) {
                            fn_8009FEA0(i);
                        }
                    } else {
                        fn_8009FEFC(i);
                    }
                    p->fall_time = 0.0f;
                    p->fall_frames = 0;
                }
                fn_80077BF0(p);
                fn_80089120(p);
                fn_80077FBC(p, it == i);
                break;
            case 5:
                if (lbl_8034477C == 0x4013 || lbl_8034477C == 0x4017) {
                    if (p->node != NULL) {
                        fn_800BA2C4(p->node, 2, 0);
                        DoPlayerAction(p);
                        PF(PF(p, 0x6C8, u8*), 0x30, u32) = p->pos[0];
                        PF(PF(p, 0x6C8, u8*), 0x34, u32) = p->pos[1];
                        PF(PF(p, 0x6C8, u8*), 0x38, u32) = p->pos[2];
                        fn_8007E540(p, PF(p, 0x6C8, void*));
                        if (p->character == 0xC) {
                            fn_800BA408(1.6f, 1.6f, 1.6f, p->node);
                        } else if (p->level < 99) {
                            fn_800BA2C4(p->node, 8, 0);
                            *(f32*)(p->node + 0x40) = 1.0f;
                            *(f32*)(p->node + 0x44) = 1.0f;
                            *(f32*)(p->node + 0x48) = 1.0f;
                        } else {
                            fn_800BA408(1.2f, 1.2f, 1.2f, p->node);
                        }
                    }
                } else if (p->node != NULL) {
                    fn_800BA368(p->node, 2, 0);
                }
                break;
            case 8:
                fn_8007CC48(p, 0, NULL);
                fn_80088688(p);
                if (p->count_920 > 0) {
                    p->anim_20C = 0x7E;
                    if (p->anim_208 == 0x7E) {
                        p->count_920 = p->count_920 - 1;
                    }
                }
                DoPlayerAction(p);
                if (p->count_920 < 1 && p->anim_208 == 0x7E) {
                    p->anim_20C = 0;
                }
                PF(PF(p, 0x6C8, u8*), 0x30, u32) = p->beacon_pos[0];
                PF(PF(p, 0x6C8, u8*), 0x34, u32) = p->beacon_pos[1];
                PF(PF(p, 0x6C8, u8*), 0x38, u32) = p->beacon_pos[2];
                PF(PF(p, 0x6C8, u8*), 0x34, u32) = p->pos[1];
                fn_80077BF0(p);
                fn_80089120(p);
                if (p->count_920 < 1 && p->anim_208 != 0x7E) {
                    fn_80079100(i);
                }
                break;
            case 0xB:
                if (lbl_8034477C == 0x4010 && p->motion_state == 1) {
                    if (lbl_80240E38[i * 0xF] & 0x8000000) {
                        fn_80079484(i);
                    }
                    if (lbl_80240E38[i * 0xF] & 0x2000000) {
                        p->motion_state = 0;
                    }
                }
                break;
            }
            p->prev_state = state;
        } else if (p->state == 1) {
            fn_8007CC48(p, lbl_8034477C, NULL);
            PlayerMotion_SetAnimState(p);
            fn_80077BF0(p);
            fn_80089120(p);
            if (!(p->hud_flags2 & 0x20)) {
                fn_8005A3B8(p->mat);
            }
            PF(PF(p, 0x6C8, u8*), 0x30, u32) = p->pos[0];
            PF(PF(p, 0x6C8, u8*), 0x34, u32) = p->pos[1];
            PF(PF(p, 0x6C8, u8*), 0x38, u32) = p->pos[2];
        }
    }
    if (lbl_8034477C != 0x400B && lbl_8034477C != 0x400D && lbl_8034477C != 0x4012 &&
        lbl_8034477C != 0x400F && lbl_8034477C != 0x4016) {
        for (i = 0, p = P(0); i < 4; i++, p++) {
            if (p->state != 0 && (p->hud_flags2 & 1) &&
                !(p->hud_flags2 & 0x20)) {
                fn_8005A3B8(p->mat);
            }
        }
    }
    j = lbl_80344B0C % 4;
    lbl_80344B0C++;
    for (i = 0; i < 4; i++) {
        s32 k = (j + i) % 4;

        if (P(k)->speech_req != NULL) {
            if (P(k)->state == 1 && lbl_8034477C == 0x4010) {
                fn_8005DE50(P(k), P(k)->speech_req);
                for (j = 0; j < 4; j++) {
                    if (j != k && P(j)->speech_req == P(k)->speech_req) {
                        P(j)->speech_req = NULL;
                    }
                }
            }
            P(k)->speech_req = NULL;
        }
    }
    for (i = 0; i < 4; i++) {
        if ((P(i)->state == 1 || P(i)->state == 4) &&
            P(i)->idle_timer != 0) {
            break;
        }
    }
    if (i < 4 && lbl_8034477C != 0x4012) {
        fn_8009D610(0, P(i)->col_pos);
    } else {
        fn_8009D610(2, NULL);
    }
    if (lbl_8034477C != 0x400D && lbl_8034477C != 0x4012 && lbl_8034477C != 0x4016 &&
        lbl_803447B8 == 0) {
        if (lbl_803444FC == 0 && lbl_803444F8 < 1) {
            lbl_80344500 = 0;
        }
        WritePlayerInfo(-1);
        fn_8007FC80();
        AudioAmbientUpdate();
    }
}
