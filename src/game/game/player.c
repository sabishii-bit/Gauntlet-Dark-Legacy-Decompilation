/*
 * player.c -- GCN PLAYER.OBJ (shell3D.pdb module .\Release\PLAYER.OBJ), FULL MODULE.
 *
 * Per-player game logic + the in-game player HUD: the parent/grab attachment
 * ops used by critters and PlayerMotion, the per-frame HUD writers (health,
 * gold, items, rune stones, power meter, "IT" tag), the experience/level
 * math, potion magic launch, the master per-frame driver do_players(), and
 * (back slice) damage/death/lifecycle, the per-character save image, model
 * loading, powerups, see-thru, got-it ticker and mini-inventory.
 *
 * CLAIM (extended to the whole module 2026-07-27):
 * .text       0x800745D0..0x8008091C  (92 GC functions)
 * extab       0x80006900..0x80006B30  (70 entries x 8)
 * extabindex  0x8000A5D0..0x8000A918  (70 entries x 12)
 *
 * Not claimed yet: .data (potionicon tables at 0x80124C70.., blit pos tables
 * 0x8011FC48.., hidden-char table 0x80120618, pup-cheat table 0x801209E4),
 * .bss (HUD/state block 0x80274EA0..0x80275AE0 + player records), .sdata
 * (welcome/it globals 0x80344AF0..), .sdata2 pool (0x80347608 "XRay" onward).
 *
 * BOUNDARY EVIDENCE:
 *  - FRONT seam OPTIONS | PLAYER at 0x800745D0: see options.c header
 *    (extabindex run + sdata2/sdata seams; last OPTIONS entry:
 *    fn 0x80074548 -> extab 0x800068F8 / extabindex 0x8000A5D0).
 *  - BACK seam PLAYER | PMOTION at 0x8008091C: extabindex is fn-address
 *    ordered; the entry at 0x8000A918 is {fn 0x8008091C (get_player_pos,
 *    PMOTION.OBJ's first fn), extab 0x80006B30} -- so PLAYER.OBJ's extab
 *    run ends exactly at 0x80006B30 / extabindex at 0x8000A918.  All 53
 *    back-slice extab entries are contiguous 8-byte records.
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
 * BACK SLICE names (same rules; all Xbox-listed unless marked GC-only):
 *   0x80077BF0 PlayerProcessScale      0x80077D04 PlayerSelecting
 *   0x80077D38 do_exit (L)             0x80077FBC do_weakening (L)
 *   0x80078108 all_players_go_to_same_level (L)
 *   0x800781C0 PlayerOnMovingObject    0x80078260 OtherPlayerOnOtherMovingObject
 *   0x800782D8 do_heal_players         0x800784E0 heal_player
 *   0x8007857C player_max_health       0x800785CC damage_player
 *   0x80078E0C player_can_be_damaged   0x80078E54 kill_player
 *   0x80079100 inactivate_player       0x80079484 abort_player
 *   0x80079770 GetPlayerColPos         0x8007979C GetPlayerPos
 *   0x800797C8 remove_player_geo       0x80079A6C change_player
 *   0x80079B38 new_player              0x80079BA0 clear_player
 *   0x80079D94 activate_player         0x80079F44 load_player
 *   0x8007A3B8 PlayerLoadSaveFile      0x8007A4D4 PlayerWriteSaveFile
 *   0x8007A560 PlayersRestoreHealth    0x8007A614 PlayerRestoreState
 *   0x8007A6DC PlayerSaveState         0x8007A7A4 player_get_from_save
 *   0x8007AA4C player_store_in_save    0x8007AC58 player_save_controls
 *   0x8007ACC8 PlayerUpdateAtts        0x8007AE68 set_player_default_atts
 *   0x8007AEF0 load_player_geo ("PLAYER.OBJ NODE EXISTS BEFORE LOAD")
 *   0x8007B558 set_hidden_player ("Access?".."Rand??" cheat strings)
 *   0x8007BEC8 load_player_model ("players\\%s\\sfx%s")
 *   0x8007C008 load_player_model_sub ("players\\%s\\%s(%d0)","..\\anim")
 *   0x8007C1D8 SetupPlayerTexMods      0x8007C230 DoPlayerTexMods [items.c]
 *   0x8007C298 init_players            0x8007C3FC create_player_blits (L, GC-only name)
 *   0x8007C938 reset_players           0x8007C998 setup_player_models
 *   0x8007CA8C GetMaxPlayerModelSize (L)  0x8007CC30 PlayerModel
 *   0x8007CC48 PlayerProcessPowerups (GIANT 0x18F8; skeleton, see body note)
 *   0x8007E540 PlayerProcessSkinFX (L)    0x8007E950 PlayerProcessMikeyPUP
 *   0x8007EC24 AppendBigapePowerupsToScene  0x8007ECB0 AppendItemToLevel
 *   0x8007EDE8 do_see_thru (L; end_see_thru inlined)
 *   0x8007F32C ClosestChest (L)        0x8007F4A8 player_get_powerup_state
 *   0x8007F580 PlayerAddPowerup        0x8007F704 PlayerIncSpeed
 *   0x8007F760 PlayerIncMagic          0x8007F7BC PlayerIncArmor
 *   0x8007F818 PlayerIncFight (Inc* stat index proven by 0xA98/9C/A0/A4
 *   displacement scan; GC order is Xbox order REVERSED)
 *   0x8007F874 check_player_atts (player_max_att auto-inlined x4)
 *   0x8007FAB4 SetPlayerWindows        0x8007FC80 do_got_it_8007FC80 (L)
 *   0x80080158 kill_got_it             0x800801EC add_got_it
 *   0x80080270 init_got_it             0x800802E4 UpdatePlayerWorldMat
 *   0x8008033C mini_inventory_update   0x80080718 mini_inventory_draw_label
 *   0x800807CC mini_inventory_find_previous_selectable_item
 *   0x80080854 mini_inventory_find_next_selectable_item
 *   0x800808D4 mini_inventory_setup ("INVENTORY")
 *
 * Xbox fns NOT present as GC symbols (auto-inlined into the hosts above,
 * inlined-shared-helper law): player_dies + drop_keys (kill_player /
 * inactivate_player / abort_player), end_see_thru (do_see_thru),
 * player_max_att (check_player_atts x4), PlayerRestoreState's copy loop
 * (inactivate_player), LevelDeltaExp, GetFight..GetMissileSpd accessors,
 * PlayerProcessFamiliar / PlayerUsePowerup / DropMikey /
 * player_find_powerup_from_typemask (PlayerProcessPowerups giant),
 * SetDebugNode/clearDebugNode, hide_power_meter, write_health, WriteName,
 * IncLevel/SetPlayerLevel/IncAtt, UserYes, CopyPlayer, def_char_type,
 * save_player_atts/load_player_atts, anybody_playing, advance_ok,
 * ReplaceTree, DropMikey (either inlined or hosted in other GC TUs).
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
 * BACK-SLICE MATCH STATUS (2026-07-27 extension pass):
 * BYTE-EXACT (0 real diff lines): SetupPlayerTexMods, DoPlayerTexMods,
 * PlayerModel, kill_got_it, init_got_it, add_got_it, mini_inventory_setup.
 * These prove the player_multiple_models[]/got_it[] static layouts (their displacements
 * link off in-TU statics, so they land exact pre-flip).
 * BSS-POOLING GATE (opcode-identical, displacement-only): GetPlayerPos,
 * GetPlayerColPos, PlayerSelecting, new_player, change_player,
 * player_can_be_damaged, all_players_go_to_same_level and every other fn
 * touching gPlayerRecords: the TARGET pools all player-record accesses off
 * potionicon_tab (.bss block base, -0xC40 from the records), so our
 * extern-based displacements differ by construction until the flip-time
 * .bss claim; gPlayerRecords itself must stay extern gPlayers
 * (Matching shopquery pin).  NOT a per-fn matching failure -- do not grind.
 * All other back-slice bodies are faithful Ghidra transcriptions pending
 * match passes (biggest first: PlayerProcessPowerups (skeleton -- real body
 * needed), set_hidden_player, damage_player, do_see_thru, load_player,
 * load_player_geo, do_got_it_8007FC80, create_player_blits, mini_inventory_update).
 *
 * FLIP GATES (claimcheck): .bss 0xC40 (the HUD block below -- size exact),
 * .data 0x5C (jumptable), .rodata 0x104 (strings), .sdata2 0x198 (pool)
 * need claims when this TU flips to Matching.
 *
 * gPlayerRecords = gPlayers ([4] x 0x335C player records; stays lbl_ --
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
 *   0x80275A80 frame_blit[4][6]    portrait frame set (own symbol frame_blit)
 *   (interior of potionicon_tab / tb_info -- .bss unclaimed, extern here)
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

extern Player gPlayers[]; /* gPlayerRecords[4], stride 0x335C */
#define gPlayerRecords gPlayers
#define PREC_STRIDE 0x335C
#define P(i)          (&gPlayerRecords[i])
#define PT(i)         ((Player*)((u8*)potionicon_tab + (i) * PREC_STRIDE + 0xC40))
#define PF(p, off, T) (*(T*)((u8*)(p) + (off)))

/* ------------------------------------------------------------------ */
/* extern data                                                         */
/* ------------------------------------------------------------------ */

/*
 * This TU's .bss (0x80274EA0..0x80275AE0, 0xC40 bytes).  The target pools
 * every access off the FIRST symbol (potionicon_tab @0x80274EA0) with folded
 * constant deltas (sibling-symbol pooling law), so the arrays are DEFINED
 * here with the exact layout; the DOL-splitter names for the block are
 * potionicon_tab / player_multiple_models / lbl_80275534 / gDefaultPlayerPosition / tb_info /
 * frame_blit.  pad_* regions belong to PLAYER fns beyond this slice
 * (mini-inventory tables, got_it state, tb_info) -- refine when wired.
 * NonMatching TU: these never link; the DOL keeps the original .bss.
 */
static void* potionicon_tab[5];    /* 0x000 (0x80274EA0) potion type -> texture */
static void* hod_blit[4];          /* 0x014 hand-of-death icons */
static void* quest_blit[4];        /* 0x024 QUEST_ICON blits */
static u8 hud_pad_034[0x4C0];      /* 0x034 (0x80274ED4, unmapped scratch) */
static char tbuf[0x20];            /* 0x4F4 (0x80275394) sprintf/path scratch */

/* per-player model arena slot (0x802753B4, stride 0x4C).  Xbox analogue:
 * the player_multiple_models/got_max_player_sizes machinery. */
typedef struct PlayerModelSlot {
    /* 0x00 */ s32 cur_class;      /* loaded class id, -1 = empty */
    /* 0x04 */ s32 cur_pad;        /* controller/costume index at load */
    /* 0x08 */ s32 cur_tier;       /* level/10 weapon tier at load */
    /* 0x0C */ s32 cur_override;   /* hidden-char name ptr at load (0 = none) */
    /* 0x10 */ void* arena;        /* texture arena (PlayerModel() handle) */
    /* 0x14 */ s32 anim_remap;     /* anim remap table (fn_8001267C), -1 */
    /* 0x18 */ u32 model_max;      /* model arena byte budget   (0x0B800) */
    /* 0x1C */ u32 model_buf_max;  /* model file byte budget    (0x4D800) */
    /* 0x20 */ u32 arena_max;      /* texture arena byte budget (0x0B000) */
    /* 0x24 */ char* model_buf;    /* model file buffer */
    /* 0x28 */ u32 anim_max;       /* anim file byte budget     (0x01000) */
    /* 0x2C */ s32 anim_remap2;    /* -1 */
    /* 0x30 */ char* anim_buf;     /* anim file buffer */
    /* 0x34 */ void* sfx_arena;    /* sfx model arena (0x2BC00 budget) */
    /* 0x38 */ s32 sfx_remap;      /* -1 */
    /* 0x3C */ u32 sfx_max;        /* sfx model budget (0x2BC00) */
    /* 0x40 */ u32 sfx_buf_max;    /* sfx file budget  (0x0C000) */
    /* 0x44 */ u32 sfx_arena_max;  /* sfx arena budget (0x18000) */
    /* 0x48 */ char* sfx_buf;      /* sfx file buffer */
} PlayerModelSlot;
static PlayerModelSlot player_multiple_models[4]; /* 0x514 (0x802753B4) */

/* AppendItemToLevel template (0x802754E4, 0x50 bytes incl. name buf) */
typedef struct AppendedItemTemplate {
    s32 type;
    s32 count;
    s16 active;
    s16 pad0A;
    f32 scale;
    f32 radius;
    f32 pos[3];
    f32 rot[2];
    char name[0x14];
    u32 flags;
    s16 field40;
    s16 field42;
    s16 field44;
    s16 field46;
    s16 field48;
    s16 field4A;
    void* atree;
} AppendedItemTemplate;
static AppendedItemTemplate appended_item_template; /* 0x644 (0x802754E4) */

/* got-it ticker entries (0x80275534, 24 entries) */
typedef struct GotIt {
    /* 0x00 */ s32 state;          /* 0 free, 1 fade-in, 2 hold, 3 wait, 4 out */
    /* 0x04 */ s32 player;
    /* 0x08 */ s32 type;           /* item class (1..10, 0xD, 0xF, 0x10) */
    /* 0x0C */ s32 count;
    /* 0x10 */ s32 timer;          /* hold countdown (0x5A) */
    /* 0x14 */ void* blit1;        /* count text blit */
    /* 0x18 */ void* blit2;        /* item icon blit */
} GotIt;
static GotIt got_it[24];           /* 0x694 (0x80275534) */

static f32 death_pos[3];           /* 0x934 (0x802757D4) last death position */

/* mini-inventory per-player box info (0x802757E0, "tb_info" on Xbox) */
typedef struct TbInfo {
    /* 0x00 */ s32 sel;            /* selected powerup slot, -1 none */
    /* 0x04 */ s32 state;          /* 0 closed, 1 open, 2 sliding in, 3 out */
    /* 0x08 */ s32 slide;          /* slide position 0..0x80 */
    /* 0x0C */ s32 x_right;        /* label x (right anchor - 0x34) */
    /* 0x10 */ s32 x_left;         /* box x (right anchor - 0x40) */
    /* 0x14 */ s32 y_top;          /* 0x14F */
    /* 0x18 */ s32 y_box;          /* 0x143 */
    /* 0x1C */ s32 tex1;           /* 0xF9F1 */
    /* 0x20 */ s32 tex2;           /* 0xF9F2 */
    /* 0x24 */ char* label;        /* current powerup label */
} TbInfo;
static TbInfo tb_info[4];          /* 0x940 (0x802757E0) */

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
extern f32 gIdentityMatrix[];   /* identity matrix */

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
extern s32 gGameMode;   /* game state */
extern s32 options_state;
extern s32 lbl_80344298;
extern s32 lbl_80344824;   /* active-player mask */
extern s32 lbl_80344760;
extern u32 sFlags;   /* sFlags: pause/movie */
extern s32 gFrameTicks;   /* frame delta (int) */
extern s32 gClockStepTicks; /* vb elapsed */
extern f32 gClockFrameStep; /* frame delta (float) */
extern f32 sMusicFadeBase;
extern s32 sVisibleSumCoinCount;   /* total visible sum-coin count */
extern s32 sMusicTrackHi;   /* music track */
extern s32 sMusicTrackLo;
extern s32 lbl_803448AC;
extern s32 lbl_803448A8;
extern s32 lbl_8034489C;
extern u8* gCurLevel;   /* gCurLevel */
extern s32 lbl_8034481C;
extern s32 lbl_80344804;   /* any player needs pad */
extern s32 gScriptedCameraState;
extern s32 lbl_803447B8;   /* paused */
extern s32 lbl_803447B4;
extern s32 gGameplayPauseTimer;
extern s32 lbl_80344A28;
extern s32 gModalRenderDepth;
extern s32 lbl_80344A44;
extern s32 opt_restart_request;
extern u32 opt_force_player;   /* demo-message once flags */
extern s32 key_blit_idx;   /* key texture id */
extern s32 alpha;
extern void* it_blit; /* it_blit */
extern s32 firstgetidx;   /* speech round-robin counter */
extern s32 lbl_80344B10;   /* welcome-speech delay */
extern s32 randpottype;   /* magic color cycle counter */
extern s32 welcome_timer;   /* welcome_timer */
extern s32 lbl_80344B24;   /* "it" player */
extern void* lbl_80344B2C; /* world root node */
extern s32 lbl_80344C4C;
extern s32 lbl_80344C54;
extern f32 lbl_80344C5C;
extern s32 gSumnerReady;
extern s32 lbl_80344C90;
extern s32 gMessageActive;
extern s32 gDemoMode;
extern s32 gGameBusy;
extern s32 gTriggerCameraState;
extern s32 gBossType;
extern s32 lbl_803444E4;
extern s32 lbl_803444F4;
extern s32 lbl_803444F8;
extern s32 lbl_803444FC;
extern s32 lbl_80344500;
extern f32 lbl_80344504;
extern s32 lbl_80344508;
extern s32 lbl_8034450C;
extern s32 lbl_80344510;
extern f32 dbgTextFlagA;
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
extern void MBBlitSetColor(void* blit, u32 rgb);
extern void MBBlitSetAlpha(void* blit, s32 alpha);
extern void mbBlitCalcWidth(void* blit, s32 x, s32 y, f64 z);
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
extern s32 DrawText(s32 x, s32 y, s32 font, u32 rgb, char* fmt, ...);
extern void dbgTextPrintfCol(s32 x, s32 line, char* fmt, ...);

/* scene nodes */
extern void MBNodeSetParent(void* node, void* parent);
extern void CopyMat4(f32* src, void* node);
extern void MBTreeClearFlags(void* node, s32 mask, s32 set);
extern void* MBTreeSetFlags(void* node, s32 mask, s32 set);
extern void MBTreeSetScale(f32 r, f32 g, f32 b, void* node);
extern void WorldVector(f32* vector, f32* out, f32* matrix);
extern f32 NormalVector2D(f32* v);
extern s32 MBBackgroundLoading(void);

/* math / world */
extern void fn_8005A338(f32* mat, f32* fwd, f32* pos);
extern void UpdateObjWorldMat(f32* mat);
extern f32 get_actual_screen_pos(s32 a, f32* out1, f32* out2, f32* pos);
extern void add_target(f32* mat);
extern s32 DamageColor(u32 flags);
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
extern void ControllerMessageBox(s32 a, s32 msg, s32 b, s32 c);
extern void CrystalCamActivate(void);
extern void fn_8009D610(s32 mode, f32* pos);
extern void AudioAmbientUpdate(void);
extern void AudioPlayerBreath(s32 player);
extern void fn_8009FEA0(s32 player);
extern void fn_8009FEFC(s32 player);
extern void AudioExp(s32 player, s32 a);
extern void AudioPotion(u32 color, f32* pos, s32 heal, s32 d);
extern void update_player_milestone(Player* player);
extern void fn_8005ACE0(void);
extern void fn_8005DE50(void* p, s32* req);
extern void update_class_spec(s32 player);

/* sfx */
extern s32 StartShieldFX(f32 scale, f32 power, f32* pos, u32 flags, s16 player, s32 d);
extern s32 StartMagicFX(f32 scale, f32 power, f32* pos, u32 flags, s16 player, s32 d);
extern void StartThrowMagicFX(f32 a, f32 b, f32 c, f32 d, f32* pos, f32* vel, u32 flags, s32 p, s32 col);
extern s32 StartLevelUpFX(f32 scale, f32 power, s32 a, u32 flags, s16 player, s32 d);
extern s32 StartMagicHealFX(f32* pos);
extern s32 PlaceEffectOnFloor(s32 fx, f32* pos);
extern void SfxSetParent(s32 fx, void* node);

/* this TU, back slice (0x80077BF0..0x8008091C), forward decls */
void PlayerProcessScale(void* p);
static void do_exit(void* p, s32 dest);
static s32 do_weakening(void* p, s32 active);
static s32 all_players_go_to_same_level(void);
void inactivate_player(s32 player);
void abort_player(s32 player);
s32 activate_player(s32 player);
void PlayerProcessPowerups(void* p, s32 a, s32* b);
static void PlayerProcessSkinFX(void* p);
void check_player_atts(void* p, s32 chartype, s32* stats);
static void do_got_it_8007FC80(void);
void mini_inventory_update(s32 player);
s32 heal_player(f32 amount, Player* p);
f32 player_max_health(void* p);
s32 player_can_be_damaged(void* p);
void kill_player(s32 player);
void clear_player(s32 player, s32 full);
void load_player(s32 player);
void load_player_geo(s32 player, void* p);
s32 set_hidden_player(void* p);
s32 load_player_model(s32 player, void* p, s32 alt, char* name);
s32 load_player_model_sub(s32 player, void* p, s32 cls, char* name, void* slot);
void player_get_from_save(void* p, s32 chartype);
void player_store_in_save(void* p);
void PlayerUpdateAtts(void* p);
void set_player_default_atts(void* p);
static void create_player_blits(s32 i);
static void GetMaxPlayerModelSize(void);
void PlayerProcessMikeyPUP(void* p);
void AppendItemToLevel(f32 x, f32 y, f32 z, char* name, u32 flags);
static s32 do_see_thru(void* p);
static f32 ClosestChest(void* p);
void PlayerAddPowerup(f32 duration, f32 strength, void* p, s32 type, u32 mask);
void kill_got_it(s32 player);
void SetPlayerWindows(s32 on);
void PlayerRestoreState(s32 player);
void PlayerSaveState(s32 player, s32 full);
void mini_inventory_draw_label(s32 i);
s32 mini_inventory_find_previous_selectable_item(s32 i);
s32 mini_inventory_find_next_selectable_item(s32 i);
void mini_inventory_setup(void);
void init_got_it(void);
void add_got_it(s32 player, s32 type, s32 count);
void UpdatePlayerWorldMat(void* p, s32 anchor);
void* PlayerModel(s32 i);
void reset_players(void);
void init_players(void);
void setup_player_models(void);
void SetupPlayerTexMods(s32 i);
void DoPlayerTexMods(s32 i);
void player_save_controls(s32 i);
s32 PlayerLoadSaveFile(s32 i, s32 slot);
s32 PlayerWriteSaveFile(s32 i, s32 slot);
void PlayersRestoreHealth(void);
void change_player(s32 i, s32 type);
void new_player(s32 i);
void damage_player(s32 i, f32 dmg, u32 flags, f32* dir);
void do_heal_players(void* p, f32* mat, f32 amount);
s32 PlayerOnMovingObject(void);
s32 OtherPlayerOnOtherMovingObject(s32 i, u8* obj);
void GetPlayerPos(s32 i, f32* out);
void GetPlayerColPos(s32 i, f32* out);
s32 PlayerSelecting(s32 i);
s32 player_get_powerup_state(f32 dt, void* p, s32 type, u32 mask);
void PlayerIncFight(void* p, u32 amount);
void PlayerIncArmor(void* p, u32 amount);
void PlayerIncMagic(void* p, u32 amount);
void PlayerIncSpeed(void* p, u32 amount);
void inactivate_player(s32 player);
void abort_player(s32 player);
void AppendBigapePowerupsToScene(void);
void remove_player_geo(s32 i);

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
extern void PlayerCheckMovingFloor_80088688(void* p);
extern void PlayerDoWeapTrail(void* p);

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
    MBNodeSetParent(p->node, lbl_80344B2C);
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
    MBNodeSetParent(p->node, lbl_80344B2C);
    p->hud_flags &= ~0x20;
}

/* Attach the player under a carrier node at offset pos (critter.c). */
void PlayerSetParent(Player* p, void* parent, f32* pos) {
    volatile f32 d[3];

    if (pos == NULL) {
        pos = lbl_80127D00;
    }
    d[0] = pos[0] - p->anchor_pos[0];
    d[1] = pos[1] - p->anchor_pos[1];
    d[2] = pos[2] - p->anchor_pos[2];
    p->saved_pos[0] = p->pos[0];
    p->saved_pos[1] = p->pos[1];
    p->saved_pos[2] = p->pos[2];
    MBNodeSetParent(p->node, parent);
    CopyMat4(gIdentityMatrix, p->node);
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
    MBNodeSetParent(p->node, parent);
    CopyMat4(gIdentityMatrix, p->node);
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
    Player* p;
    s32 i;
    s32 first;
    s32 end;
    u8 unused[8];

    if ((!(gGameMode & 0x8000) || lbl_80344298 == 0) &&
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
            if (it_blit != NULL) {
                MBRemoveBlit(it_blit);
                it_blit = NULL;
            }
            if (lbl_80344B24 >= 0 && (lbl_80344824 & (1 << lbl_80344B24))) {
                it_blit = MBNewBlit("IT", lbl_80120238[lbl_80344B24] + 0x22, -349);
                mbBlitProject(it_blit, 0x20, 0x20);
                mbBlitCvtCoord(it_blit, 64000.0f);
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

    if (welcome_timer < 1 && options_state == 0 && gGameMode == 0x4010 &&
        (f64)p->got_timer > 0.0) {
        i = p->index;
        p->got_timer = p->got_timer - gClockFrameStep;
        total = sVisibleSumCoinCount;
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

    if (welcome_timer < 1 || options_state != 0) {
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 8; j++) {
                blit = crystal_blit[i][j];
                if (blit != NULL) {
                    mbBlitInit3414(blit, 1);
                }
            }
        }
    } else {
        welcome_timer -= gFrameTicks;
        if (welcome_timer < 0) {
            welcome_timer = 0;
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
                if (!(gGameMode & 0x8000) && (state == 1 || state == 5)) {
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
    if (gGameMode == 0x4010 && lbl_80344760 > 0 && p->state == 0xB &&
        p->motion_state == 1) {
        u32 x = lbl_80120238[i];

        hidden = 1;
        MBNewTempBlit((void*)lbl_80344E48, x + 6, 0x14C, 0xE, 0xE);
        MBNewTempBlit((void*)lbl_80344E44, x + 6, 0x160, 0xE, 0xE);
        DrawTextKeepScale(1.2f, x + 0x14, 0x150, 1, 0xFFFFFF, "Wait In Tower");
        DrawTextKeepScale(1.2f, x + 0x14, 0x164, 1, 0xFFFFFF, "Quit Game");
        show_gold = 0;
    }
    if ((p->state == 5 || p->state == 0xB) && gGameMode != 0x400D &&
        gGameMode != 0x4013 && gGameMode != 0x4017 && !hidden) {
        hidden = 1;
        setup_player_display(i);
        if (p->state == 0xB) {
            DrawTextKeepScale(1.2f, -lbl_80120240[i], 0x154, 1, rgb, "IN TOWER");
        } else {
            hidden = 0;
        }
    }
    if (show_gold && alpha == 0) {
        write_gold(i, 1);
        if (frame_blit[i][5] != NULL) {
            s32 w;

            if ((f64)p->health > 9999.0) {
                p->health = 9999.0f;
            }
            sprintf(buf, "%d", (s32)p->health);
            w = DrawNormalText(1.0f, buf, 4);
            DrawText((lbl_80120238[i] + 0x74) - w, 0x167, 4,
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
                    DrawText(lbl_80120238[i] + 8, 0x154, 1, 0xFFFFFF, "XP: %d",
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
            DrawText(-lbl_80120240[i], 0x146, 1, 0xFFFFFF, buf2);
        }
    }
tail:
    if (p->state != 2 && p->display_mode != 0 && alpha == 0) {
        if (p->item_body_lo > 0) {
            blit = MBNewTempBlit((void*)key_blit_idx, lbl_80120238[i] + 8, 0x143, -1, -1);
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
        if (gGameMode == 0x4010 && p->quest_state != 0 && sMusicTrackHi != 0xD) {
            mbBlitInit3414(quest_blit[i], 0);
        } else {
            mbBlitInit3414(quest_blit[i], 1);
        }
        if ((u32)(gGameMode - 0x400F) < 2 && (p->shards & 0x1000)) {
            mbBlitInit3414(hod_blit[i], 0);
        } else {
            mbBlitInit3414(hod_blit[i], 1);
        }
        if (gGameMode == 0x4010) {
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

    if (gGameMode == 0x4010) {
        fn_800C02F4(0x80FF80);
        get_actual_screen_pos(0, &out2, out1, P(i)->col_pos);
        dbgTextFlagA = 1;
        name = P(i)->floor_name;
        if (name == NULL || lbl_80240E50[i * 0xF] == 0.0f) {
            name = "NO FLOOR";
            if (P(i)->floor_name2 != NULL) {
                name = P(i)->floor_name2;
            }
        }
        oldflags = MBSetFontFlags(0x40000);
        y = 330.0f;
        DrawText(lbl_80120238[i] + 8, (s32)y, 1, 0xFFFFFF, name);
        y += 10.0f;
        sprintf(tbuf, "%.1Lf %.1Lf %.1Lf", P(i)->pos[0], P(i)->pos[1],
                P(i)->pos[2]);
        DrawText(lbl_80120238[i] + 8, (s32)y, 1, 0xFFFFFF, tbuf);
        y += 10.0f;
        MBWorldToScreen(ang, P(i)->pos);
        sprintf(tbuf, "%.0Lf %.0Lf", ang[0], ang[1]);
        DrawText(lbl_80120238[i] + 8, (s32)y, 1, 0xFFFFFF, tbuf);
        MBSetFontFlags(oldflags);
    }
}

/* Gold counter, right-aligned, 99999 cap. */
static void write_gold(s32 i, s32 show) {
    s32 w;
    s32 x;
    Player* p = P(i);
    char buf[12];

    if (frame_blit[i][4] != NULL) {
        if (p->gold > 99999) {
            p->gold = 99999;
        }
        if (show != 0) {
            sprintf(buf, "%d", p->gold);
            w = DrawNormalText(1.0f, buf, 4);
            x = (lbl_80120238[i] + 0x3C) - w;
            DrawText(x, 0x167, 4,
                     lbl_801201C8[p->class_id], buf);
            mbBlitInit3414(frame_blit[i][4], 0);
        } else {
            mbBlitInit3414(frame_blit[i][4], 1);
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
        p->power_level = p->power_level + (f32)gFrameTicks;
        if (p->power_target < p->power_level) {
            p->power_level = p->power_target;
        }
    } else {
        p->power_level = p->power_level - (f32)(gFrameTicks << 1);
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
            MBBlitSetColor(pm_blit[i][0], rgb);
            MBBlitSetColor(pm_blit[i][1], rgb2);
        }
    } else if (state < 3) {
        j = (p->meter_timer << 9) / 0x78;
        if (j > 0xFF && j < 0x200) {
            j = 0x1FF - j;
        }
        if (j < 0x100) {
            mbBlitInit3414(pm_blit[i][3], 0);
            MBBlitSetAlpha(pm_blit[i][3], 0xFF - j);
        } else {
            p->meter_timer = 0;
        }
        MBBlitSetColor(pm_blit[i][0], 0xFF0000);
        MBBlitSetColor(pm_blit[i][1], 0xFF0000);
    }
    p->meter_timer = p->meter_timer + gFrameTicks;
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
        MBBlitSetColor(frame_blit[i][1], lbl_801201E8[cls]);
    } else {
        MBBlitSetColor(frame_blit[i][1], lbl_801201D8[cls]);
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
        if (!(gGameMode & 0x8000)) {
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
    if (gGameMode == 0x400B || gGameMode == 0x4012) {
        mbBlitUpdateEntry(frame_blit[i][0], 0xFFFFFFFF, 0x4010);
        mbBlitUpdateEntry(frame_blit[i][2], 0xFFFFFFFF, 0x4010);
    } else {
        mbBlitUpdateEntry(frame_blit[i][0], 0xFFFFBFEF, 0);
        mbBlitUpdateEntry(frame_blit[i][2], 0xFFFFBFEF, 0);
    }
}

/* Map player state (+ motion state) to a HUD display mode. */
s32 get_display_mode(s32 i) {
    Player* p = P(i);

    switch (p->state) {
    case 1:
    case 4:
    case 5:
        if (gGameMode == 0x4012) {
            return 5;
        }
        if (*(u32*)((u8*)p + 116) != 0) {
            return 1;
        }
        return 1;
    case 2:
        switch (p->motion_state) {
        case 3:
            return 3;
        case 1:
        case 2:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
            return 1;
        case 4:
            return 6;
        case 0:
        default:
            return 0;
        }
    case 3:
        return 2;
    case 11:
        return 10;
    default:
        return 0;
    }
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
        f32 dist = PF(gCurLevel, 0x9C, f32);
        f32 fac = PF(gCurLevel, 0xA0, f32);

        if (dist > 0.0f) {
            if ((f32)p->level > dist) {
                fac = fac * (1.0 / (0.1 * ((f32)p->level - dist) + 1.0));
            }
        }
        amount = (s32)((f32)amount * fac);
        if (sFlags & 0x10) {
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
            AudioExp(pnum, -1);
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
            check_player_atts(p, p->character, NULL);
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
            check_player_atts(p, p->character, NULL);
        }
    }
    return res;
}

/* Inverse of LevelToExp: scan 99..1 for the level exp buys (running
 * rate = lv*30 maintained incrementally, 99-step guard). */
s32 ExpToLevel(s32 exp) {
    s32 need;
    s32 rate;
    s32 lv;
    s32 product;

    lv = 99;
    rate = 2970;
    for (; lv != 0; lv--) {
        if (lv <= 60) {
            need = (lv - 1) * (rate + 1000);
        } else {
            product = (lv - 60) * 4600;
            need = 0x28550;
            need += product;
        }
        if (exp >= need) {
            return lv;
        }
        rate -= 30;
    }
    return 1;
}

/* Total exp needed to reach a level. */
s32 LevelToExp(s32 lv) {
    s32 product;
    s32 result;

    if (lv <= 60) {
        return (lv - 1) * (lv * 30 + 1000);
    }
    product = (lv - 60) * 4600;
    result = 0x28550;
    result += product;
    return result;
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
        power = (f32)(20.0 * sMusicFadeBase);
    } else {
        p = P(pnum);
        power = (f32)(p->magic_power * sMusicFadeBase);
        if (pnum == DamageColor(flags)) {
            power = (f32)(power * 1.1);
            scale = (f32)(scale + 0.1);
        }
        if (p->level > 0x18) {
            flags |= 0x800000;
        }
    }
    color = flags & 0xF;
    if (color == 0) {
        color = (randpottype % 4) + 1;
        flags |= color;
        randpottype++;
    }
    flags |= 0x200;
    if (pnum < 0) {
        mode = 0;
    }
    if (mode < 2) {
        if (mode == 1) {
            fx = StartShieldFX((f32)(25.0 * scale), (f32)(0.25 * power), pos, flags | 0x200,
                             (s16)pnum, mode);
            MBNodeSetParent(*(void**)(lbl_80285BCC + fx * 0xF0), p->node);
            if (pnum >= 0) {
                AudioPotion(color, pos, 1, mode);
            }
        } else {
            fx = StartMagicFX((f32)(40.0 * scale), power, pos, flags | 0x200, (s16)pnum, mode);
            PlaceEffectOnFloor(fx, NULL);
            if (pnum >= 0) {
                AudioPotion(color, pos, 0, mode);
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
        StartThrowMagicFX(100.0f, (f32)(40.0 * scale), (f32)(0.75 * power), 1.5, vpos, vel,
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
    if (sFlags & 0x10) {
        opt_force_player = 0xFFFFFFFF;
    }
    if (gGameMode == 0x4010 && (s32)opt_force_player >= 0) {
        if (gScriptedCameraState == 0 && lbl_803447B8 == 0) {
            lbl_80344B10 += gFrameTicks;
            if (lbl_80344B10 > 0) {
                if (gDemoMode == 0) {
                    if (sMusicTrackHi == 0xD) {
                        if (gSumnerReady != 0) {
                            ControllerMessageBox(-1, FindStringMessageListSub(0, "WelcomeMessage"), -1, -1);
                            lbl_80344C90 = 6;
                            lbl_80344C54 = 1;
                            lbl_80344C5C = sMusicFadeBase;
                            CrystalCamActivate();
                        }
                        if (lbl_803448AC == 6 && lbl_803448A8 == 1) {
                            ControllerMessageBox(-1, FindStringMessageListSub(0, "GarmMessage"), -1, -1);
                        }
                    }
                    opt_force_player = 0xFFFFFFFF;
                } else if (sMusicTrackHi == 0xD) {
                    if (lbl_80344C4C == 1 && !(opt_force_player & 1)) {
                        ControllerMessageBox(-1, FindStringMessageListSub(0, "DemoWelcome"), 0, -1);
                        opt_force_player |= 1;
                    }
                } else if (sMusicTrackHi != 0xC && (opt_force_player & 2)) {
                    ControllerMessageBox(-1, FindStringMessageListSub(0, "DemoLevel"), 0, -1);
                    opt_force_player &= ~2;
                }
            }
        } else {
            lbl_80344B10 = 0;
        }
    }
    if (gGameMode != 0x4012 && gGameMode != 0x400D && gGameMode != 0x400F &&
        gGameMode != 0x4016) {
        msgUpdate();
        for (i = 0, p = P(0); i < 4; i++, p++) {
            if (p->state != 0) {
                s32 sel = (p->state == 2 || p->state == 3);

                if (!sel) {
                    fn_8005A338(p->mat, p->anchor_fwd, p->anchor_pos);
                    if (p->platform != NULL && *p->platform != 0) {
                        WorldVector((f32*)(*p->platform + 0x30), p->beacon_pos,
                                    p->mat);
                        p->beacon_pos[1] = 0.0f;
                        p->beacon_pos[0] = p->pos[0] + p->beacon_pos[0];
                        p->beacon_pos[1] = p->pos[1] + p->beacon_pos[1];
                        p->beacon_pos[2] = p->pos[2] + p->beacon_pos[2];
                    }
                }
            }
        }
        if (gGameBusy != 0 || gGameplayPauseTimer != 0) {
            WritePlayerInfo(-1);
            return;
        }
        if (gTriggerCameraState == 1) {
            WritePlayerInfo(-1);
            for (i = 0, p = P(0); i < 4; i++, p++) {
                p->select_timer = p->select_timer + gClockFrameStep;
            }
            return;
        }
        if (gGameMode == 0x4010) {
            TowerCheckMessages(0);
        }
        if (gScriptedCameraState != 0) {
            if (lbl_803447B8 != 0) {
                for (i = 0, p = P(0); i < 4; i++, p++) {
                    if (p->count_91C != 0) {
                        p->count_91C = p->count_91C - 1;
                    }
                    if (p->state == 1 && p->platform != NULL) {
                        if (gGameMode == 0x4010) {
                            p->intower = 1;
                        }
                        DoPlayerAction(p);
                    }
                    p->anim_20C = 0;
                    if (!(p->hud_flags2 & 0x20)) {
                        UpdateObjWorldMat(p->mat);
                    }
                }
            }
            WritePlayerInfo(-1);
        }
        if (lbl_803447B8 == 0 || lbl_8034481C != 0 || opt_restart_request != 0) {
            lbl_80344804 = 0;
            for (i = 0, p = P(0); i < 4; i++, p++) {
                f32 mag;
                f32 len;

                mag = (f32)(0.5 * p->light_range);
                len = NormalVector2D(p->light_vel);
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
            loaded = all_players_go_to_same_level();
            if (!(sFlags & 4)) {
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
        if (gGameMode == 0x400B || gGameMode == 0x400F) {
            continue;
        }
        state = p->state;
        if (state == 2 || state == 3) {
            goto common;
        }
        if (gGameMode == 0x4012 || gGameMode == 0x400D || gGameMode == 0x400F ||
            gGameMode == 0x4016) {
            continue;
        }
        if (lbl_803447B8 == 0 || lbl_8034481C != 0 || opt_restart_request != 0) {
        common:
            if (p->count_91C != 0) {
                p->count_91C = p->count_91C - 1;
            }
            if (p->timer_1F0 > 0) {
                p->timer_1F0 = p->timer_1F0 - gFrameTicks;
            }
            if (p->timer_1FA > 0) {
                p->timer_1FA = p->timer_1FA - gFrameTicks;
            }
            if (p->timer_1FC > 0) {
                p->timer_1FC = p->timer_1FC - gFrameTicks;
            }
            if (p->timer_1FE > 0) {
                p->timer_1FE = p->timer_1FE - gFrameTicks;
            }
            if (p->vibe_on == 1) {
                if (p->vibe_timer2 == 0) {
                    p->vibe_timer = p->vibe_timer + gFrameTicks;
                } else {
                    p->vibe_timer2 = p->vibe_timer2 + gFrameTicks;
                }
            } else {
                p->vibe_timer = 0;
                p->vibe_timer2 = 0;
            }
            state = p->state;
            switch (state) {
            case 0:
                if (p->respawn_timer > 0) {
                    p->respawn_timer = p->respawn_timer - gFrameTicks;
                    if (p->respawn_timer < 1) {
                        setup_player_display(i);
                    }
                }
                break;
            case 2:
                PlayerCheckMovingFloor_80088688(p);
                for (j = 0; j < 4; j++) {
                    s32 st;

                    if (j != i && (st = P(j)->state) != 0 && st != 2 && st != 3) {
                        break;
                    }
                }
                break;
            case 3:
                PlayerCheckMovingFloor_80088688(p);
                if (MBBackgroundLoading() == 0 && gGameMode != 0x4013 &&
                    gGameMode != 0x4017) {
                    activate_player(i);
                } else if (lbl_802575BC == 0 ||
                           (gGameMode != 0x400B && gGameMode != 0x400D &&
                            gGameMode != 0x400F && gGameMode != 0x4016)) {
                    update_class_spec(i);
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
                    MBTreeClearFlags(p->node, 2, 0);
                    if (PF(p, 0x6C8, void*) != NULL) {
                        MBTreeSetFlags(PF(p, 0x6C8, void*), 2, 0);
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
                        PlayerProcessScale(p);
                        PlayerDoWeapTrail(p);
                        do_exit(p, loaded);
                        break;
                    }
                }
                if (p->idle_timer > 599) {
                    p->idle_timer = p->idle_timer - 600;
                    AudioPlayerBreath(i);
                }
                /* fallthrough */
            case 1:
                if (gGameMode == 0x4010) {
                    p->intower = 1;
                    PF(p, 0xC28 + p->character * 0x1C, f32) =
                        PF(p, 0xC28 + p->character * 0x1C, f32) + (f32)gFrameTicks;
                    if (PF(gCurLevel, 0, u32) & 8) {
                        fn_800C0ADC(lbl_80343D74, lbl_80343D70);
                    }
                }
                if ((u32)(lbl_8034489C - 2) < 2) {
                    if (p->quest_state == 0) {
                        p->pulse_7FC = 0.8f;
                    } else {
                        if (p->quest_state == 1) {
                            p->quest_state = 2;
                            towerClearRuneNear(i, sMusicTrackHi);
                        }
                        p->pulse_7FC = 2.0f;
                    }
                }
                add_target(p->mat);
                if ((sMusicTrackHi != 0xD || sumnerSpeechActive() == 0) && gTriggerCameraState == 0 &&
                    gModalRenderDepth == 0 && gMessageActive == 0 && p->name_timer > 0 &&
                    gGameBusy == 0 && gGameplayPauseTimer == 0) {
                    char name[8 + 1];
                    f32 spos[2];

                    p->name_timer = p->name_timer - gFrameTicks;
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
                    MBTreeClearFlags(p->node, 2, 0);
                    if (PF(p, 0x6C8, void*) != NULL) {
                        if (sMusicTrackHi == 0xC && sMusicTrackLo == 8) {
                            MBTreeSetFlags(PF(p, 0x6C8, void*), 2, 1);
                        } else {
                            MBTreeSetFlags(PF(p, 0x6C8, void*), 2, 0);
                        }
                    }
                }
                update_player_milestone(p);
                PlayerProcessPowerups(p, 0, NULL);
                if ((sFlags & 0x10) && lbl_802575B0 != 0 && i == 0 && gBossType < 0) {
                    fn_8005ACE0();
                }
                PlayerMotion();
                if (p->fall_time > 0.0 && p->fall_time + 2.0 < sMusicFadeBase) {
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
                PlayerProcessScale(p);
                PlayerDoWeapTrail(p);
                do_weakening(p, it == i);
                break;
            case 5:
                if (gGameMode == 0x4013 || gGameMode == 0x4017) {
                    if (p->node != NULL) {
                        MBTreeClearFlags(p->node, 2, 0);
                        DoPlayerAction(p);
                        PF(PF(p, 0x6C8, u8*), 0x30, u32) = p->pos[0];
                        PF(PF(p, 0x6C8, u8*), 0x34, u32) = p->pos[1];
                        PF(PF(p, 0x6C8, u8*), 0x38, u32) = p->pos[2];
                        PlayerProcessSkinFX(p);
                        if (p->character == 0xC) {
                            MBTreeSetScale(1.6f, 1.6f, 1.6f, p->node);
                        } else if (p->level < 99) {
                            MBTreeClearFlags(p->node, 8, 0);
                            *(f32*)(p->node + 0x40) = 1.0f;
                            *(f32*)(p->node + 0x44) = 1.0f;
                            *(f32*)(p->node + 0x48) = 1.0f;
                        } else {
                            MBTreeSetScale(1.2f, 1.2f, 1.2f, p->node);
                        }
                    }
                } else if (p->node != NULL) {
                    MBTreeSetFlags(p->node, 2, 0);
                }
                break;
            case 8:
                PlayerProcessPowerups(p, 0, NULL);
                PlayerCheckMovingFloor_80088688(p);
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
                PlayerProcessScale(p);
                PlayerDoWeapTrail(p);
                if (p->count_920 < 1 && p->anim_208 != 0x7E) {
                    inactivate_player(i);
                }
                break;
            case 0xB:
                if (gGameMode == 0x4010 && p->motion_state == 1) {
                    if (lbl_80240E38[i * 0xF] & 0x8000000) {
                        abort_player(i);
                    }
                    if (lbl_80240E38[i * 0xF] & 0x2000000) {
                        p->motion_state = 0;
                    }
                }
                break;
            }
            p->prev_state = state;
        } else if (p->state == 1) {
            PlayerProcessPowerups(p, gGameMode, NULL);
            PlayerMotion_SetAnimState(p);
            PlayerProcessScale(p);
            PlayerDoWeapTrail(p);
            if (!(p->hud_flags2 & 0x20)) {
                UpdateObjWorldMat(p->mat);
            }
            PF(PF(p, 0x6C8, u8*), 0x30, u32) = p->pos[0];
            PF(PF(p, 0x6C8, u8*), 0x34, u32) = p->pos[1];
            PF(PF(p, 0x6C8, u8*), 0x38, u32) = p->pos[2];
        }
    }
    if (gGameMode != 0x400B && gGameMode != 0x400D && gGameMode != 0x4012 &&
        gGameMode != 0x400F && gGameMode != 0x4016) {
        for (i = 0, p = P(0); i < 4; i++, p++) {
            if (p->state != 0 && (p->hud_flags2 & 1) &&
                !(p->hud_flags2 & 0x20)) {
                UpdateObjWorldMat(p->mat);
            }
        }
    }
    j = firstgetidx % 4;
    firstgetidx++;
    for (i = 0; i < 4; i++) {
        s32 k = (j + i) % 4;

        if (P(k)->speech_req != NULL) {
            if (P(k)->state == 1 && gGameMode == 0x4010) {
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
    if (i < 4 && gGameMode != 0x4012) {
        fn_8009D610(0, P(i)->col_pos);
    } else {
        fn_8009D610(2, NULL);
    }
    if (gGameMode != 0x400D && gGameMode != 0x4012 && gGameMode != 0x4016 &&
        lbl_803447B8 == 0) {
        if (lbl_803444FC == 0 && lbl_803444F8 < 1) {
            lbl_80344500 = 0;
        }
        WritePlayerInfo(-1);
        do_got_it_8007FC80();
        AudioAmbientUpdate();
    }
}

/* ================================================================== */
/* BACK SLICE (0x80077BF0..0x8008091C) -- wired 2026-07-27.            */
/* Faithful Ghidra transcriptions pending match passes; giants noted.  */
/* ================================================================== */

/* extern data (back slice) */
extern char* lbl_80343D6C;    /* active hidden-character code ptr (set_hidden_player) */
extern s32 lbl_80343D68;      /* mini-inventory label table count */
extern s32 lbl_80343DAC;      /* bigape powerup table count */
extern f32 lbl_80343D7C[];    /* att ranges: dmg lo/hi, armor, magic, speed, mdmg, mspd */
extern s32 gGameOptions;      /* NoDamage? cheat (1 = no damage, 2.. = invuln) */
extern s32 lbl_80257594;      /* Unlimited? cheat (3 = unlimited turbo) */
extern s32 lbl_802575A8;      /* Access? cheat (levels open) */
extern s32 lbl_80257630[4];   /* per-player targeting state cleared at init */
extern s32 good_wiz_state;      /* cutscene/no-damage global */
extern s32 gBoss398;      /* boss floor index */
extern s32 lbl_80251CCC[];    /* floor records, stride 0xE5 words */
extern s32 gNumEnemies;      /* enemy count */
extern s32 lbl_80251F44[];    /* enemy records target field, stride 0xE5 words */
typedef struct BigapePowerupInfo {
    char* name;
    u32 flags;
} BigapePowerupInfo;

typedef struct BigapePowerupSpawn {
    char level[4];
    s32 type;
    f32 x;
    f32 y;
    f32 z;
} BigapePowerupSpawn;

extern BigapePowerupInfo lbl_80120274[3];
extern BigapePowerupSpawn lbl_8012028C[];
extern u8* sItems;      /* gItems base (stride 0xF0) */
extern s32 lbl_8028CAF4;      /* floor tree table (stride 0x50) */
extern void* sKeyringAtree;    /* see-thru tree (low) */
extern void* sDeathIconAtree;    /* see-thru tree (high) */
extern s32 lbl_8025EC68[4];   /* see-thru: player tree node */
extern s32 lbl_8025EC78[4];   /* see-thru: active floor id, -1 none */
extern u8* lbl_8025EC88[4];   /* see-thru: chest item ptr */
extern s32 lbl_8025EC98[4];   /* see-thru: saved parent */
extern u8* lbl_8025ECA8[4];   /* see-thru: proxy node */
extern void* lbl_8025ECB8[4][0x12]; /* see-thru: overlay handle (stride 0x48) */
extern u8* lbl_80282930[4];   /* per-player class record (att bases at +0x28..) */
extern void* FamiliarTree[4][2]; /* level-tier halo atrees */
typedef struct PlayerControlState {
    u32 ctl;
    u32 levels;
    u32 edges;
    u32 repeatEdges;
    s32 specialTimer;
    s32 specialResult;
    s32 specialLast;
    f32 leftAngle;
    f32 leftMagnitude;
    f32 rightAngle;
    f32 rightMagnitude;
    s32 scheme;
    s32 hasActuator;
    s32 unk34;
    s32 unk38;
} PlayerControlState;
extern PlayerControlState lbl_80240E30[4];
extern u32 lbl_80240E5C[];    /* pad config words, stride 0xF */
extern u32 lbl_80240E60[];
extern u32 lbl_80240E64[];
extern u32 lbl_80240E68[];
extern s32 lbl_80344828;      /* hidden-characters-allowed count */
extern s32 sWeaponsBuf;      /* weapons-in-hand enabled */
extern void* gSceneRoot;    /* HUD camera parent */
extern s32 got_max_player_sizes;      /* got_max_player_sizes once-flag */
extern s32 lbl_80344B28;      /* INVENTORY file handle */
extern void* sPowerupsBuf;    /* global atree bank */
extern void* lbl_80344BD4;    /* mikey camera parent */
extern s32 lbl_80344BEC;      /* exit FX color */
extern s32 sLastWorldLevel;      /* secret-exit destination */
extern s32 sWorldDataConst;      /* town level id */
extern s32 lbl_80344B84;      /* battle-tower level id */
extern s32 gClockFrameNumber; /* frame parity counter */
extern s32 lbl_80344DA4;      /* world loaded */
extern s32 lbl_803447D0;      /* level-clear/exit progress state */
extern s32 lbl_803447A8[2];   /* cleared at init_players */
extern f32 lbl_80344B20;      /* x-ray range (mask & 8 powerup strength) */
extern s32 lbl_803447C0;      /* widescreen/mode flag (rune13 blit) */

/* hidden-character table (0x80120618, stride 0x24, 27 entries) */
typedef struct HiddenChar {
    /* 0x00 */ s32 class_id;
    /* 0x04 */ s32 char_type;
    /* 0x08 */ char name[8];   /* 6-char cheat name ("ICE600"..) */
    /* 0x10 */ char code[16];  /* model/dir override tag */
    /* 0x20 */ s32 unlocked;   /* available without cheat */
} HiddenChar;
extern HiddenChar Hidden[27];

/* powerup-cheat table (0x801209E4, stride 0x14, 27 entries) */
typedef struct PupCheat {
    /* 0x00 */ char name[8];
    /* 0x08 */ s32 type;
    /* 0x0C */ f32 value;
    /* 0x10 */ u32 mask;
} PupCheat;
extern PupCheat Cheats[27];

/* mini-inventory label table (0x8011FCE8, stride 0xC) */
extern s32 lbl_8011FCE8[];    /* [i*3+0] type, [i*3+1] mask, [i*3+2] name ptr */

extern char* lbl_80120104[];  /* per-pad player color names */
extern char* lbl_801200F4[];  /* per-pad color dir names */
extern char* lbl_8012006C[];  /* per-class dir names */
extern char* lbl_80120184[];  /* per-class R_WRIST node names */
extern char* lbl_80120144[];  /* per-class L_WRIST node names */
extern s32 lbl_80120598[];    /* per-class weapon-variant flag */
extern u8 lbl_80113AE0[];     /* .rodata fmt block base (fmts at +1464..+1520) */
extern char lbl_80114098[];   /* "players\%s\sfx%s" */
extern char lbl_80347A38[];   /* "rb" (sdata2) */
extern char* lbl_801205D8[];  /* rune world tags (4) */
extern char* lbl_801205F8[];  /* crystal color tags (8) */
extern char* lbl_8011FCD4[];  /* POTION_ICON_* names (5) */

/* extern functions (back slice) */
extern int rand(void);
extern s32 strncmp(const char* a, const char* b, u32 n);
extern void* memset(void* p, int c, u32 n);
extern void* memcpy(void* d, const void* s, u32 n);
extern f64 __fabs(f64 x);
extern void FatalError(const char* fmt, ...);
extern void FatalErrorf(const char* fmt, ...);
extern int bulletproof_printf(const char* fmt, ...);
extern s32 BytesFree(void);
extern char* AllocMem(u32 size, s32 pool, char* tag);
extern char* AllocFile(char* name, char* mode, u32 sizehint);
extern void MLMReadFile(char* name, char* mode, u32 size, char* buf);
extern void* AtreeMatch(void* bank, char* name, s32 a);
extern u32 MBOX_LoadModelFixed(char* name, u32 arena, s32 a, char* b, u32 c);
extern void CopyMat3(f32* src, f32* dst);

extern void MBTreeSetAmbientAdd(void* node, s32 frame, s32 mode);
extern void MBTreeSetAltTex(void* node, s32 a, s32 b, s32 c);
extern void MBTreeSetAlpha(void* node, s32 mask, s32 set);
extern void MBRemoveNode(void* node, s32 a);
extern s32 ProcessSkinFX(f32* v, void* node, s32 a);
extern s32 FireScrollActive(void);
extern void fn_8009D258(f32* pos);
extern void towerRuneNearAudio(void);
extern u32 NextWorldLevel(s32 a);
extern u32 PrevWorldLevel(s32 a);
extern void SetSkinFX(f32 scale, f32* v, s32 color, s32 n, s32 one);
extern void del_target(f32* mat);
extern void YawMat3(f32* mat, f32 ang);
extern void AudioHeartBeat(s32 player);
extern s32 fn_8005B8FC(void* p);
extern f32 fqdist(f32 dx, f32 dz);
extern void fn_8009190C(f32* pos, s32 amount);
extern s32 ModifyDamage(f32 armor, f32* dmg, u32* flags, u32 shield);
extern f64 atan2(f64 x, f64 z);
extern void StartBlockFX(f32 dmg, s32 player);
extern void fn_8009FFF4(s32 kind, s32 player);
extern void AudioPlayerPain(s32 player);
extern void AudioPlayerPoison(s32 player);
extern void AudioPlayerSeverePain(s32 player);
extern void AudioPlayerDies(s32 player);
extern void AudioPlayerHit(f32 dmg, s32 player, s32 kind);
extern s32 do_vibe(s32 player, s32 lvl, s32 n);
extern void AtreeDelete(void** h);
extern s32 AtreeInit(void* atree, void* out, s32 a, s32 flags);
extern void AnimateATree(void** h, s32 a, s32 b);
extern void StartGemFX(f32* pos, s32 n);
extern s32* PlaceItem(s32 a, s32 b, char* name, f32* mat);
extern s32* AddItem(s32* tmpl, f32* pos);
extern void AddItemSub(s32* item);
extern s32 RandItemIdx(s32 item, s32 a, s32 b);
extern void AudioPlayEvt102(void);
extern void playerGiveGargItem(s32 player, s32 track, s32 sub);
extern void sel_set_inactive(s32 player);
extern s32 other_players_next_level(s32 player);
extern void LoadPlyrData(s32 player, s32 chartype, void* flag);
extern void controls_remove_active_player(s32 player);
extern void InitPlayerMissiles(void* p);
extern void strcpy(char* dst, char* src);
extern s32 toupper(s32 c);
extern void strncpy(char* dst, char* src, s32 n);
extern void* MBNewNode(void* parent, f32* mat, s32 a);
extern s32 fn_80011BBC(void* modelbuf, char* name, void* atree_out, char* tmp, s32 size);
extern void InitActions(void* atree, void* animctx, s32 bank);
extern s32 MBOX_ReallyFindObject(char* name, s32 a, s32 b, s32 dir);
extern s32* AtreeFindMbidxNode(void* atree, s32 idx);
extern void* MBNewObject(s32 node, f32* mat, void* c, u32 flags);
extern void MBPsysSetDebugNode(s32 node, s32 a);
extern s32 AddSpecialTexmod(s32 model, char* fmt, char* code, char* d, s32 e, s32 f);
extern void* MBCreateBlit(s32 a, u32 tex, s32 x, s32 y, s32 w, s32 h);
extern s32 MBBlitGetTex(void* blit);
extern void mbBlitCalcRect(void* blit, s32* x, s32* y, f32* z);
extern void mbBlitCalcY(void* blit, s32 y);
extern void DrawGlowText(f32 scale, s32 x, s32 y, char* text);
extern void AudioCursorH(void);
extern void AudioCursorSelect(void);
extern void AudioCursorV(void);
extern void StartEnemyGrid(f32* pos, f32 pad);
extern s32 NextGridEnemy(void);
extern s32 PointVisible(f32 r, s32* pos);
extern s32 saveLoad(s32 a, s32 b, s32 c, void* buf, s32* size);
extern s32 saveSave(s32 a, s32 b, s32 c, void* buf, s32 size);
extern s32 InitPreferences(s32 a, s32 b);
extern s32 memCardErrorPrompt();
extern s32 saveMenuPrompt(char* prompt, s32* colors, s32 n);
extern s32 OptionsSetup(s32 a);
extern void ControlsUpdate(s32 a, s32* b, s32 c);
extern void ReadControls(void);
extern s32 any_level(u32 button);
extern s32 any(u32 button);
extern s32 InitTexMods(void* modelbuf, u32 arena);
extern void DoTexMods(void* modelbuf);
extern s32 fn_8001267C(u16* anim, u32 arena, s32 old);
extern void fn_8005A404(f32* mat, f32* fwd, f32* pos);
extern void ShopLoadData(s32 file);
extern void AudioPlayerXray(s32 player);
extern void get_player_pos(void);
extern void CreateYPRMatrix(f32* out, f32* rec, u8* c);
extern s32 InLevel(s32* entry);
extern u32 MBOX_FindTexture2(char* name, s32* out); /* MBOX_FindTexture */
extern void DelSpecialTexmod(s32 sfx);
extern void ErrorPrintf(const char* fmt, ...);
extern void SfxDeleteParented(u8* node, s32 a, s32 player);
extern void ClearPlyrData(s32 player);

/* per-char save-stat block view: p + 0xA90 + character*0x18 */
#define CHAR_STATS(p, t) ((s32*)((u8*)(p) + 0xA90 + (t) * 0x18))
/* [0]=exp [1]=health [2]=fight [3]=armor [4]=magic [5]=speed (f32 bits) */

/* per-char item block view: p + 0xDD0 + character*0xF0 */
#define CHAR_ITEMS(p, t) ((u8*)(p) + 0xDD0 + (t) * 0xF0)
/* +0 keys(s16) +2 potions(s16) +4 runes(u16) +6 shards(u16) +0xA pottypes +0x30 gold */

/* powerup slot view: p + 0x130 + i*0x10 (11 slots) */
#define PUP_TIMELEFT(p, i)     PF(p, 0x130 + (i) * 0x10, f32)
#define PUP_TYPE(p, i)         PF(p, 0x134 + (i) * 0x10, s32)
#define PUP_ATTRIBUTEADD(p, i) PF(p, 0x138 + (i) * 0x10, f32)
#define PUP_SPECIALFLAGS(p, i) PF(p, 0x13C + (i) * 0x10, u32)
#define PUP_DIRTY(p, i)    PF(p, 0x1E0 + (i), u8)

/* attribute norms / derived stats */
#define ATT_FIGHT(p)   PF(p, 0xF4, f32)
#define ATT_ARMOR(p)   PF(p, 0xF8, f32)
#define ATT_MAGIC(p)   PF(p, 0xFC, f32)
#define ATT_SPEED(p)   PF(p, 0x100, f32)
#define STAT_DMG(p)    PF(p, 0x104, f32)
#define STAT_ARMOR(p)  PF(p, 0x108, f32)
#define STAT_MAGIC(p)  PF(p, 0x10C, f32)
#define STAT_SPEED(p)  PF(p, 0x110, f32)
#define STAT_MDMG(p)   PF(p, 0x114, f32)
#define STAT_MSPD(p)   PF(p, 0x118, f32)
#define HIDDEN_CODE(p) PF(p, 0xF0, char*)

/* ------------------------------------------------------------------ */
/* per-frame processors                                                */
/* ------------------------------------------------------------------ */

extern const f64 lbl_803478E8;

static inline f64 PlayerScaleMultiply(f32 value, const f64* factor)
{
    return (f64)value * *factor;
}

/* Decay the shrink/grow potion scale back toward 1 and clamp tiny.    */
#pragma opt_common_subs off
#pragma opt_propagation off
void PlayerProcessScale(void* vp) {
    Player* p = vp;
    f32 s;
    f32 ambientScale;
    f32 zero;
    f32 av;
    u8 unused[8];

    s = PF(p, 0x7FC, f32);
    if (s != 0.0) {
        ambientScale = s;
        PF(p, 0x7FC, f32) = (f32)PlayerScaleMultiply(s, &lbl_803478E8);
        av = PF(p, 0x7FC, f32);
        *(u32*)&av &= 0x7FFFFFFF;
        if (av < 0.01) {
            zero = 0.0f;
            ambientScale = zero;
            PF(p, 0x7FC, f32) = zero;
        }
        MBTreeSetAmbientAdd(p->node, (s32)(100.0 * ambientScale), 1);
    }
    if (PF(p, 0x7DC, f32) > 0.0f) {
        PF(p, 0x95A, s16) = 1;
    } else if (PF(p, 0x95A, s16) != 0) {
        PF(p, 0x95A, s16) = 0;
    }
    if (ProcessSkinFX((f32*)((u8*)p + 0x7DC), p->node, 0) == 0 &&
        *(s16*)(p->node + 0x5C) <= -2) {
        MBTreeSetAltTex(p->node, -1, 0, 1);
        MBTreeSetAmbientAdd(p->node, 0, 1);
    }
}
#pragma opt_propagation reset
#pragma opt_common_subs reset

/* Is player i on the character-select overlay?                        */
s32 PlayerSelecting(s32 i) {
    if (P(i)->state == 2 || P(i)->state == 3) {
        return 1;
    }
    return 0;
}

/* Sink-and-spin exit sequence; dest chooses the next level.           */
static void do_exit(void* vp, s32 dest) {
    Player* p = vp;
    s16 t;

    if (FireScrollActive() != 0) {
        return;
    }
    if (PF(p, 0x1F2, s16) == 0 && dest != 0) {
        PF(p, 0x1F2, s16) = (lbl_8034481C == 0) ? 0x32 : 0;
        if (lbl_803447B4 == 0) {
            fn_8009D258(p->pos);
            towerRuneNearAudio();
            lbl_803447B4 = 1;
        }
        PF(p, 0x8B8, f32) = PF(p, 0x8B4, f32);
        if (lbl_8034481C >= 0x10000) {
            PF(p, 0x830, s32) = lbl_8034481C - 0x10000;
        } else if (lbl_8034481C >= 0xD) {
            PF(p, 0x830, s32) = lbl_80344B84;
        } else if (lbl_8034481C == 0xC) {
            PF(p, 0x830, s32) = sWorldDataConst;
        } else if (lbl_8034481C >= 3) {
            PF(p, 0x830, s32) = ((lbl_8034481C - 3) & 0xFF) | 0xC00;
        } else if (lbl_8034481C == 2) {
            PF(p, 0x830, s32) = (dest < 0) ? sLastWorldLevel : dest;
        } else if (lbl_8034481C == 1) {
            PF(p, 0x830, s32) = (dest < 0) ? (s32)NextWorldLevel(1) : dest;
        } else if (lbl_8034481C == -1) {
            PF(p, 0x830, s32) = (dest < 0) ? (s32)PrevWorldLevel(1) : dest;
        } else {
            PF(p, 0x830, s32) = dest;
        }
        SetSkinFX(1.5f, (f32*)((u8*)p + 0x7DC), lbl_80344BEC, 10, 1);
    }
    t = PF(p, 0x1F2, s16) - gFrameTicks;
    PF(p, 0x1F2, s16) = t;
    if (t < 1) {
        PF(p, 0x1F2, s16) = 0;
        if (p->state != 5) {
            p->state = 5;
            del_target(p->mat);
        }
    } else if (PF(p, 0x8B8, f32) < 1.0 + 0.5 * PF(p, 0x854, f32) + p->pos[1]) {
        /* still above the hole floor: sink and spin */
        p->pos[0] += 0.0f * (f32)gFrameTicks;
        p->pos[1] += -0.06f * (f32)gFrameTicks;
        p->pos[2] += 0.0f * (f32)gFrameTicks;
        YawMat3(p->mat, (f32)(0.5 * gClockFrameStep));
        p->hud_flags |= 1;
    }
}

/* Health-drain warning beeps; returns -1 once health is gone.         */
static s32 do_weakening(void* vp, s32 active) {
    Player* p = vp;
    s16 t;

    if (PF(p, 0xA30, s32) != 0) {
        if (p->state == 1 && (sFlags & 4) == 0 && sMusicTrackHi != 0xC) {
            PF(p, 0xA2C, s32) += gFrameTicks;
            if (PF(p, 0xA2C, s32) >= PF(p, 0xA30, s32)) {
                PF(p, 0xA2C, s32) -= PF(p, 0xA30, s32);
            }
        }
    }
    if (p->health < 1.0) {
        return -1;
    }
    if (active != 0 && p->health <= 150.0f) {
        t = PF(p, 0x1F6, s16) - gFrameTicks;
        PF(p, 0x1F6, s16) = t;
        if (t <= 0) {
            if (sMusicTrackHi != 0xD && (PF(p, 0x120, u32) & 0x110000) == 0) {
                AudioHeartBeat(p->index);
            }
            if (p->health >= 100.0f) {
                PF(p, 0x1F6, s16) = 0x78;
            } else if (p->health >= 50.0f) {
                PF(p, 0x1F6, s16) = 0x3C;
            } else {
                PF(p, 0x1F6, s16) = 0x1E;
            }
        }
    }
    return 0;
}

/* All exiting players agree on a destination? 0 = no/blocked.         */
static s32 all_players_go_to_same_level(void) {
    Player* p = P(0);
    s32 i;
    s32 dest = 0;

    if (lbl_803447D0 < 0xE && lbl_8034481C == 0) {
        return 0;
    }
    for (i = 0; i < 4; i++, p++) {
        if (p->state == 1) {
            return 0;
        }
        if (p->state == 4) {
            if (dest == 0) {
                dest = fn_8005B8FC(p);
            } else if (dest != fn_8005B8FC(p)) {
                return 0;
            }
        }
    }
    return dest;
}

/* Player index+1 if someone is riding a moving lift/platform.         */
#pragma opt_propagation off
s32 PlayerOnMovingObject(void) {
    u8* obj;
    u8* mo;
    u32 flags;
    s32 i;

    if (gGameMode != 0x4010) {
        return 0;
    }
    for (i = 0; i < 4; i++) {
        Player* p = P(i);
        if (p->state == 1 && (obj = PF(p, 0x8C4, u8*)) != NULL &&
            *(u32*)(obj + 0x28) != 0) {
            mo = *(u8**)(obj + 0x18);
            flags = *(u32*)(obj + 0x10);
            if (mo != NULL) {
                flags |= *(u32*)(mo + 0x10);
            }
            if ((flags & 0x1000) && (flags & 0x8000000) && (flags & 0x300000)) {
                return i + 1;
            }
        }
    }
    return 0;
}
#pragma opt_propagation reset

/* Another player (not i / not obj) currently riding something?        */
s32 OtherPlayerOnOtherMovingObject(s32 i, u8* obj) {
    u8* o;
    s32 j;

    for (j = 0; j < 4; j++) {
        Player* p = P(j);
        if (j != i && p->state == 1 && (o = PF(p, 0x8C4, u8*)) != NULL && o != obj) {
            if (*(u32*)(o + 0x28) != 0 && (*(u32*)(o + 0x10) & 0x4000)) {
                return 1;
            }
        }
    }
    return 0;
}

/* Heal-others potion: heal every other player in range of p.          */
void do_heal_players(void* vp, f32* mat, f32 amount) {
    Player* p = vp;
    Player* q;
    f32 cap;
    f32 give;
    f32 giveq;
    f32 d;
    s32 lvl;
    s32 i;
    s32 typ;

    typ = -1;
    lvl = p->level;
    if (lvl >= 75) {
        /* heal caster (heal_player inlined, return discarded) */
        cap = 100.0 * (lvl - 1) + 500.0;
        give = amount * (f32)(0.016 * (lvl - 75) + 0.1);
        if (cap > 9999.0f) {
            cap = 9999.0f;
        }
        if (!(give > 0.0f && p->health >= cap)) {
            p->health += give;
            if (p->health > cap) {
                p->health = cap;
            }
        }
        giveq = 0.5 * give;
        typ = 50;
        for (i = 0; i < 4; i++) {
            if (i == p->index) {
                continue;
            }
            q = (Player*)((u8*)gPlayers + i * 13148);
            if (q->state != 1) {
                continue;
            }
            d = fqdist(p->pos[0] - q->pos[0], p->pos[2] - q->pos[2]);
            if (d < p->magic_power) {
                cap = 100.0 * (q->level - 1) + 500.0;
                if (cap > 9999.0f) {
                    cap = 9999.0f;
                }
                if (!(giveq > 0.0f && q->health >= cap)) {
                    q->health += giveq;
                    if (q->health > cap) {
                        q->health = cap;
                    }
                }
            }
        }
        msgPost(0x93, p->index, (u32)p->pos);
    }
    if (typ >= 0) {
        fn_8009190C(mat, typ);
    }
}

/* Heal one player; 0 = already full, 1 = capped, 2 = healed.          */
s32 heal_player(f32 amount, Player* p) {
    f32 cap;

    cap = 100.0 * (p->level - 1) + 500.0;
    if (cap > 9999.0f) {
        cap = 9999.0f;
    }
    if (amount > 0.0f && p->health >= cap) {
        return 0;
    }
    p->health += amount;
    if (p->health > cap) {
        p->health = cap;
        return 1;
    }
    return 2;
}

/* Level-scaled maximum health (9999 cap).                             */
f32 player_max_health(void* vp) {
    Player* p = vp;
    f32 cap;

    cap = 100.0 * (p->level - 1) + 500.0;
    if (cap > 9999.0f) {
        cap = 9999.0f;
    }
    return cap;
}

/* Vulnerability gate shared with damage_player.                       */
s32 player_can_be_damaged(void* vp) {
    Player* p = vp;

    if ((PF(p, 0x208, s32) < 0x58 || PF(p, 0x208, s32) > 0x5A) &&
        p->action < 0xB && (p->hud_flags & 0x10) == 0) {
        if (PF(p, 0x6B8, u32) == 0) {
            goto can_damage;
        }
    }
    return 0;
can_damage:
    return 1;
}

/* ------------------------------------------------------------------ */
/* damage / death / lifecycle                                          */
/* ------------------------------------------------------------------ */

extern u8 lbl_801201C4;       /* weakening default period */

/*
 * Apply damage to player i.  flags carry the damage-kind mask (0x600 =
 * directional/back-stab family, 0x10160 = heavy, 0x8000 = scripted,
 * 0x200 = front-arc-checked); dir is the attacker facing for the
 * front-arc test.  Armor absorbs via ModifyDamage, grunts/gore route
 * through the sfx TU, and the death path posts msg 0xD, drops the meter
 * and got-it entries, and parks state 8 (dying).
 */
void damage_player(s32 i, f32 dmg_in, u32 flags, f32* dir) {
    Player* p = P(i);
    f32 dmg = dmg_in;
    u32 fl = flags;
    s32 invuln;
    s32 hp_new;
    s32 hp_old;
    f32 hp;
    f64 red;
    u16 hf;

    if (p->state != 1 || gTriggerCameraState != 0 || (fl & 0x200) != 0) {
        return;
    }
    if (player_can_be_damaged(p) == 0) {
        return;
    }

    invuln = gGameOptions;
    if (invuln < 2) {
        invuln = (invuln >= 1 && dmg == 999999.0f) ? 1 : 0;
    }
    if ((fl & 0x8000) == 0) {
        if (invuln > 2 || p->state == 4 || good_wiz_state != 0 ||
            (gBossType >= 0 && gBoss398 >= 0 &&
             lbl_80251CCC[gBoss398 * 0xE5] != 1)) {
            return;
        }
        if (dmg > 1.0) {
            dmg = dmg * PF(gCurLevel, 0xA4, f32);
        }
    }
    ModifyDamage(STAT_ARMOR(p), &dmg, &fl, PF(p, 0x120, u32));
    if ((fl & 0x40000000) && (PF(p, 0x124, u32) & 1)) {
        dmg = 0.0f;
    }
    if (dmg > 0.05f) {
        hf = p->hud_flags;
        if ((hf & 0x600) == 0) {
            /* front hit: "ouch" speech occasionally */
            if (dmg > 40.0f && (fl & 0x10160) && (hf & 0x2000) == 0 &&
                sMusicFadeBase > 5.0) {
                msgPost(0x7D, p->index, (u32)&p->name);
            }
        } else {
            /* shielded/back arc */
            if ((hf & 0x200) == 0) {
                dmg = dmg * 0.25;
            } else if (dir == NULL) {
                dmg = 0.0f;
            } else {
                red = atan2(dir[0], dir[2]) - PF(p, 0x894, f32);
                if (red > 3.141592653589793) {
                    red = red - 6.283185307179586;
                } else if (red <= -3.141592653589793) {
                    red = red + 6.283185307179586;
                }
                if ((f32)red <= -1.5707963267948966 || (f32)red >= 1.5707963267948966) {
                    dmg = dmg * 0.25;
                } else {
                    dmg = 0.0f;
                }
            }
            if (dmg_in - dmg > 0.5 && PF(p, 0x1FE, s16) < 1) {
                f64 clank = 0.75 * dmg;
                if (clank < 0.1) {
                    clank = 0.1;
                } else if (clank > 1.0) {
                    clank = 1.0;
                }
                StartBlockFX((f32)clank, p->index);
                PF(p, 0x1FE, s16) = (s16)(s32)(5.0 * clank);
                p->hud_flags |= 0x2000;
            }
            if ((fl & 0x10160) == 0) {
                fl &= ~0x10;
            } else {
                fl = (fl & 0xFFFEFE9F) | 0x10;
            }
        }
    }

    hp = p->health;
    if (dmg >= 0.0) {
        if (invuln == 0 && (fl & 0x8000) == 0) {
            p->health = hp - dmg;
        }
        PF(p, 0x8D0, f32) += dmg;
        if (invuln < 2) {
            if (fl & 0xF) {
                PF(p, 0x8D4, u32) &= ~0xF;
            }
            if (dmg <= 0.5f) {
                fl &= 0xFFFEFE8F;
            }
            PF(p, 0x8D4, u32) |= fl;
            if (dir != NULL) {
                PF(p, 0x8DC, f32) += dir[0];  /* hit push vec */
                PF(p, 0x8E0, f32) += dir[1];
                PF(p, 0x8E4, f32) += dir[2];
            }
            if (dmg > 0.0f) {
                if (fl & 0x800) {
                    PF(p, 0x898, f32) = 1.0f + sMusicFadeBase;
                }
                if (fl & 0x1000) {
                    PF(p, 0x898, f32) = 4.0f + sMusicFadeBase;
                }
                if (fl & 0x10040) {
                    do_vibe(i, 3, 0x1E);
                } else if (fl & 0x120) {
                    do_vibe(i, 2, 0x14);
                } else if (fl & 0x90) {
                    do_vibe(i, 1, 0xF);
                } else {
                    do_vibe(i, 0, 10);
                }
            }
        }
    } else {
        /* negative damage heals */
        p->health = hp - dmg;
        if (fl & 0x8000) {
            if (fl & 0xF) {
                PF(p, 0x8D4, u32) &= ~0xF;
            }
            PF(p, 0x8D4, u32) |= fl;
            if (dir != NULL) {
                PF(p, 0x8DC, f32) += dir[0];
                PF(p, 0x8E0, f32) += dir[1];
                PF(p, 0x8E4, f32) += dir[2];
            }
        }
    }

    if (p->health >= 1.0) {
        /* grunt tiers on crossing 150/50 hp; big-hit speech (msg 0xD) */
        hp_old = (s32)(0.25 + hp);
        hp_new = (s32)(0.25 + p->health);
        if (hp_old >= 0x97 && hp_new <= 0x96) {
            if (msgPost(0xD, i, (u32)&p->name) == 0) {
                fn_8009FFF4(1, i);
            }
        } else if (hp_old >= 0x33 && hp_new <= 0x32) {
            fn_8009FFF4((gClockFrameNumber & 1) ? 3 : 2, i);
        } else {
            s32 kind = (s32)dmg;
            if (kind == 2) {
                if (dmg > 0.0f) {
                    AudioPlayerPain(i);
                }
                PF(p, 0x8D0, f32) = 0.0f;
            } else if (kind == 3) {
                AudioPlayerPoison(i);
                PF(p, 0x8D0, f32) = 0.0f;
            } else if (kind != 0) {
                if (hp_old - hp_new < 0x3D) {
                    if (PF(p, 0x8D0, f32) >= 45.0) {
                        PF(p, 0x8D0, f32) -= 45.0;
                        if (dmg > 0.0f) {
                            AudioPlayerPain(i);
                        }
                    }
                } else {
                    if (dmg > 0.0f) {
                        AudioPlayerPain(i);
                    }
                    PF(p, 0x8D0, f32) = 0.0f;
                }
            }
        }
        /* gore burst on heavy hit */
        if (dmg >= 1.0 && (fl & 0x800) == 0) {
            if (PF(p, 0x1F0, s16) < 1) {
                s32 kind2 = 0;
                if (fl & 0x20000) {
                    kind2 = 1;
                } else if (fl & 0x40000) {
                    kind2 = 2;
                }
                if (dmg > 0.0f) {
                    AudioPlayerHit(dmg, i, kind2);
                }
                PF(p, 0x1F0, s16) = 0x1E;
            }
        } else if (dmg >= 1.0) {
            if (dmg > 0.0f) {
                AudioPlayerSeverePain(i);
            }
        }
    } else {
        /* death */
        AudioPlayerDies(i);
        p->state = 8;
        PF(p, 0x1920, s32) = 4;
        p->health = 0.0f;
        PF(p, 0x828, f32) = 0.0f;
        kill_got_it(i);
        if (lbl_80344B24 == i) {
            lbl_80344B24 = -1;
            if (it_blit != NULL) {
                MBRemoveBlit(it_blit);
                it_blit = NULL;
            }
        }
    }
}

/*
 * player_dies teardown -- static on Xbox, auto-inlined on GC into
 * kill_player / inactivate_player / abort_player (inlined-shared-helper
 * law; the standalone copy deadstrips).  Stops motion, unhooks the
 * grab proxy, clears got-it entries, and drops the keyring item.
 */
static inline void player_dies(s32 i) {
    Player* p = PT(i);
    s32* chest;
    s32 j;
    f32 m[16];

    del_target(p->mat);
    if (lbl_8025ECB8[i][0] != NULL) {
        AtreeDelete(&lbl_8025ECB8[i][0]);
    }
    MBTreeSetFlags((void*)lbl_8025EC68[i], 1, 0);
    /* restore the see-thru chest proxy to its tree */
    if (lbl_8025EC88[i] != NULL && *(u8**)(lbl_8025EC88[i] + 100) != NULL) {
        MBNodeSetParent(*(u8**)(lbl_8025EC88[i] + 100), (void*)lbl_8025EC98[i]);
        MBTreeSetAlpha(*(u8**)(lbl_8025EC88[i] + 100), 0, 1);
        CopyMat3((f32*)lbl_8025ECA8[i], *(f32**)(lbl_8025EC88[i] + 100));
        *(f32*)(*(u8**)(lbl_8025EC88[i] + 100) + 0x30) = *(f32*)(lbl_8025ECA8[i] + 0x30);
        *(f32*)(*(u8**)(lbl_8025EC88[i] + 100) + 0x34) = *(f32*)(lbl_8025ECA8[i] + 0x34);
        *(f32*)(*(u8**)(lbl_8025EC88[i] + 100) + 0x38) = *(f32*)(lbl_8025ECA8[i] + 0x38);
    }
    lbl_8025EC88[i] = NULL;
    for (j = 0; j < 24; j++) {
        if (got_it[j].player == i) {
            got_it[j].state = 0;
            if (got_it[j].blit1 != NULL) {
                MBRemoveBlit(got_it[j].blit1);
                got_it[j].blit1 = NULL;
            }
            if (got_it[j].blit2 != NULL) {
                MBRemoveBlit(got_it[j].blit2);
                got_it[j].blit2 = NULL;
            }
        }
    }
    if (PF(p, 0x1EB8, s32) > 0 && sMusicTrackHi != 0xD) {
        CopyMat4(gIdentityMatrix, m);
        m[12] = death_pos[0];
        m[13] = death_pos[1];
        m[14] = death_pos[2];
        CopyMat4(p->mat, m);
        if (gBossType < 0) {
            chest = PlaceItem(1, 2, (PF(p, 0x1EB8, s32) == 1) ? "KEY" : "KEYRING", m);
            if (chest != NULL) {
                chest[0x38] = PF(p, 0x1EB8, s32);
            }
        }
    }
    PF(p, 0x1EB8, s32) = 0;
    remove_player_geo(i);
    AudioPlayEvt102();
    for (j = 0; j < 11; j++) {
        memset((u8*)p + 0x130 + j * 0x10, 0, 0x10);
    }
    PF(p, 0x1EC, s32) = 0;
    PF(p, 0x124, u32) = 0;
}

/* Kill player i outright (health gone): teardown + dead-display.      */
void kill_player(s32 i) {
    Player* p = PT(i);

    if (p->state != 0) {
        if (p->node != NULL) {
            player_dies(i);
        }
        PF(p, 0x834, s32) = 0;
        p->health = 0.0f;
        p->state = 0xB;
        p->motion_state = 1;
        setup_player_display(i);
    }
}

/* Park player i (level change / joined-late slot). In the tower the   */
/* slot just goes back to selecting with saved-health restore.         */
void inactivate_player(s32 i) {
    Player* p = PT(i);

    if (sMusicTrackHi == 0xD) {
        p->state = 1;
        PlayerRestoreState(i);
        return;
    }
    playerGiveGargItem(i, sMusicTrackHi, sMusicTrackLo);
    p->state = 0xB;
    p->motion_state = 1;
    setup_player_display(i);
    p->health = 0.0f;
    if (p->node != NULL) {
        death_pos[0] = p->pos[0];
        death_pos[1] = p->pos[1];
        death_pos[2] = p->pos[2];
        player_dies(i);
    }
    setup_player_display(i);
}

/* Hard-drop player i out of the game (slot free).                     */
void abort_player(s32 i) {
    Player* p = PT(i);
    s32 j;

    PF(p, 0x3328, s32) = 0;
    p->state = 0;
    p->motion_state = 0;
    PF(p, 0x333C, s32) = 0;
    controls_remove_active_player(i);
    PF(p, 0x1F4, s16) = 0xB4;
    if (p->node != NULL) {
        player_dies(i);
    }
    if (gGameMode & 0x4000) {
        setup_player_display(i);
        for (j = 0; j < 7; j++) {
            mbBlitInit3414(pm_blit[i][j], 1);
        }
    }
    clear_player(i, 1);
}

/* Collision-probe position (offset 0x64).                             */
void GetPlayerColPos(s32 i, f32* out) {
    u8* p = (u8*)potionicon_tab + i * PREC_STRIDE;

    out[0] = *(f32*)(p + 0xCA4);
    out[1] = *(f32*)(p + 0xCA8);
    out[2] = *(f32*)(p + 0xCAC);
}

/* World position (offset 0x44).                                       */
void GetPlayerPos(s32 i, f32* out) {
    u8* p = (u8*)potionicon_tab + i * PREC_STRIDE;

    out[0] = *(f32*)(p + 0xC84);
    out[1] = *(f32*)(p + 0xC88);
    out[2] = *(f32*)(p + 0xC8C);
}

/* Tear down the in-world player geo (nodes, weapon, mikey, atree).    */
void remove_player_geo(s32 i) {
    Player* p = P(i);
    u8* kid;

    if (PF(p, 0x6D0, s32) != 0) {
        MBPsysSetDebugNode(PF(p, 0x6D0, s32), 1);
    }
    if (PF(p, 0x6CC, s32) != 0) {
        MBPsysSetDebugNode(PF(p, 0x6CC, s32), 1);
    }
    if (PF(p, 0x6D4, s32) != 0) {
        MBPsysSetDebugNode(PF(p, 0x6D4, s32), 1);
    }
    if (PF(p, 0x7F8, s32) >= 0) {
        DelSpecialTexmod(PF(p, 0x7F8, s32));
    }
    if (PF(p, 0x6E0, void*) != NULL) {
        MBRemoveNode(PF(p, 0x6E0, void*), 0);
        PF(p, 0x6E0, void*) = NULL;
    }
    if (PF(p, 0x748, s32) != 0) {
        AtreeDelete((void**)((u8*)p + 0x748));
    }
    if (PF(p, 0x790, s32) != 0) {
        AtreeDelete((void**)((u8*)p + 0x790));
    }
    if (PF(p, 0x6E4, s32) != 0) {
        AtreeDelete((void**)((u8*)p + 0x6E4));
    }
    if (PF(p, 0x730, void*) != NULL) {
        MBRemoveNode(PF(p, 0x730, void*), 0);
        PF(p, 0x730, void*) = NULL;
    }
    if (PF(p, 0x72C, void*) != NULL) {
        MBRemoveNode(PF(p, 0x72C, void*), 0);
        PF(p, 0x72C, void*) = NULL;
    }
    if (PF(p, 0x734, void*) != NULL) {
        MBRemoveNode(PF(p, 0x734, void*), 0);
        PF(p, 0x734, void*) = NULL;
    }
    if (PF(p, 0x73C, void*) != NULL) {
        MBRemoveNode(PF(p, 0x73C, void*), 0);
        PF(p, 0x73C, void*) = NULL;
    }
    if (PF(p, 0x740, void*) != NULL) {
        MBRemoveNode(PF(p, 0x740, void*), 0);
        PF(p, 0x740, void*) = NULL;
    }
    if (PF(p, 0x968, void*) != NULL) {
        MBRemoveNode(PF(p, 0x968, void*), 0);
        PF(p, 0x968, void*) = NULL;
    }
    if (PF(p, 0x96C, s32) != 0) {
        AtreeDelete((void**)((u8*)p + 0x96C));
    }
    PF(p, 0xA1C, s16) = 0;
    if (PF(p, 0x96C, s32) != 0) {
        AtreeDelete((void**)((u8*)p + 0x96C));
    }
    if (PF(p, 0xA14, void*) != NULL) {
        MBRemoveNode(PF(p, 0xA14, void*), 1);
        PF(p, 0xA14, void*) = NULL;
    }
    if (PF(p, 0xA14, u8*) != NULL && *(s32*)(PF(p, 0xA14, u8*) + 0x78) != 0) {
        ErrorPrintf("mikey objgrp OBJ NODE HAS KIDS AFTER ALL REMOVED\n");
    }
    /* orphan any remaining children back onto the world */
    if (p->node != NULL && *(s32*)(p->node + 0x78) != 0) {
        while ((kid = *(u8**)(*(u8**)(p->node + 0x78) + 0x7C)) != NULL) {
            MBNodeSetParent(kid, *(void**)(p->node + 0x74));
        }
    }
    SfxDeleteParented(p->node, 1, i);
    AtreeDelete((void**)((u8*)p + 0x7C));
    if (p->node != NULL) {
        if (*(s32*)(p->node + 0x78) != 0) {
            ErrorPrintf("PLAYER OBJ NODE HAS KIDS AFTER ALL REMOVED\n");
        }
        MBRemoveNode(p->node, 1);
        p->node = NULL;
    }
    MBRemoveNode(PF(p, 0x6C8, void*), 0);
    PF(p, 0x6C8, void*) = NULL;
    ClearPlyrData(i);
}

/* ------------------------------------------------------------------ */
/* character switch / lifecycle                                        */
/* ------------------------------------------------------------------ */

/* Swap player i to character type; 0x10 = the active hidden char.     */
void change_player(s32 i, s32 type) {
    u8 unused[8];
    Player* p = P(i);

    player_store_in_save(p);
    if (type == 0x10) {
        type = 2;
        HIDDEN_CODE(p) = lbl_80343D6C;
    } else {
        HIDDEN_CODE(p) = NULL;
    }
    p->character = type;
    p->char_type = type;
    if (p->char_type >= 8) {
        p->char_type -= 8;
    }
    LoadPlyrData(i, type, NULL);
    player_get_from_save(p, type);
    if ((gGameMode == 0x4010 || gGameMode == 0x400B) && p->node != NULL) {
        remove_player_geo(i);
        load_player(i);
    }
}

/* Fresh slot -> character select.                                     */
void new_player(s32 i) {
    Player* p = P(i);

    clear_player(i, 1);
    p->state = 2;
    p->motion_state = 0;
    PF(p, 0xA8B, s8) = -1;
    PF(p, 0x3358, s32) = -1;
    sel_set_inactive(i);
}

/* Reset a record to new-character defaults; full also wipes both save */
/* images and parks the slot.                                          */
void clear_player(s32 i, s32 full) {
    Player* p = P(i);
    s32 cls;
    s32 j;

    PF(p, 0x1EBC, s32) = 0;
    for (j = 0; j < 9; j++) {
        PF(p, 0x3300 + j * 4, s32) = j & 3;
    }
    p->gold = gDemoMode ? 0x9C4 : 0;
    p->health = 100.0f;
    PF(p, 0x1EC8, u16) = 0;
    PF(p, 0x1ECA, u16) = 0;
    PF(p, 0x334C, s32) = 0;
    PF(p, 0x3350, s32) = 0;
    PF(p, 0x3354, s32) = 0;
    PF(p, 0x3358, s32) = -1;
    HIDDEN_CODE(p) = NULL;
    for (j = 0; j < 11; j++) {
        memset((u8*)p + 0x130 + j * 0x10, 0, 0x10);
    }
    PF(p, 0x1EC, s32) = 0;
    PF(p, 0x124, u32) = 0;
    p->level = 1;
    p->exp = 0;
    cls = p->character;
    if (cls == 2) {
        cls = 4;
    } else if (cls < 2) {
        cls = (cls != 0 && cls > -1) ? 5 : 6;
    } else if (cls < 4) {
        cls = 7;
    } else {
        cls = 6;
    }
    p->char_type = cls;
    p->character = p->char_type;
    PF(p, 0x1EB8, s32) = 0;
    PF(p, 0x828, f32) = 0.0f;
    PF(p, 0x82C, f32) = 0.0f;
    PF(p, 0x3330, s32) = 0;
    PF(p, 0x3328, s32) = 0;
    if (full != 0) {
        memset((u8*)p + 0xA80, 0, 0x1434);
        memset((u8*)p + 0x1ECC, 0, 0x1434);
        p->state = 0;
        p->motion_state = 0;
        PF(p, 0x333C, s32) = 0;
    }
    for (j = 0; j < 16; j++) {
        LoadPlyrData(p->index, j, NULL);
        *(f32*)&CHAR_STATS(p, j)[2] = 0.0f;
        *(f32*)&CHAR_STATS(p, j)[3] = 0.0f;
        *(f32*)&CHAR_STATS(p, j)[4] = 0.0f;
        *(f32*)&CHAR_STATS(p, j)[5] = 0.0f;
    }
    check_player_atts(p, p->character, NULL);
}

/* Take player i live into the world (post-select).                    */
s32 activate_player(s32 i) {
    Player* p = P(i);
    s32 j;

    p->state = 1;
    PF(p, 0x830, s32) = other_players_next_level(i);
    del_player_blits(i);
    LoadPlyrData(i, p->character, (void*)1);
    if (gGameMode == 0x4010) {
        load_player(i);
        if (lbl_803447B8 == 0) {
            PlayerAddPowerup(0.0f, 5.0f, p, 9, 4);
        }
        for (j = 0; j < gNumEnemies; j++) {
            lbl_80251F44[j * 0xE5] = 0;
        }
        if (lbl_803447B4 != 0 || lbl_803447D0 > 9 || gGameMode == 0x4016) {
            for (j = 0; j < 4; j++) {
                if (lbl_803447B4 != 0 || P(j)->state == 5) {
                    if (P(j)->state - 4U < 2) {
                        PF(p, 0x830, s32) = PF(P(j), 0x830, s32);
                    }
                    p->state = 5;
                }
            }
        }
        if (lbl_803447D0 < 10 && gGameMode != 0x400B && gGameMode != 0x400D) {
            add_target(p->mat);
        } else {
            MBTreeSetFlags(p->node, 2, 0);
        }
        lbl_80240E38[i * 0xF] &= ~0x40000;
    }
    return 1;
}

/*
 * Rebuild the player world state after select/level load: cheat-level
 * bump (AllChars), geo load, and the big live-gameplay block reset.
 * Xbox analogue: load_player.
 */
void load_player(s32 i) {
    Player* p = P(i);
    u8* cls;
    s32 lvl;
    s32 j;
    f32 m[16];

    if (gDemoMode != 0 && sMusicTrackHi != 0xD) {
        /* cheat build: force the level stamped on the current level */
        if ((f32)p->level != PF(gCurLevel, 0x9C, f32)) {
            opt_force_player |= 2;
        }
        lvl = (s32)PF(gCurLevel, 0x9C, f32);
        p->exp = (lvl < 0x3D) ? (lvl - 1) * (lvl * 0x1E + 1000)
                              : (lvl - 0x3C) * 0x11F8 + 0x28550;
        p->level = lvl;
        set_player_default_atts(p);
        check_player_atts(p, p->character, NULL);
        p->health = 0.5f * (lvl - 1) + 30.0f;
    }
    p->node = NULL;
    PF(p, 0x78, s32) = 0;
    load_player_geo(i, p);
    /* live-gameplay block reset (transcription abridged where the      */
    /* target just zeroes fields; see Ghidra 0x80079F44 for the map)    */
    PF(p, 0x800, s32) = 0;
    PF(p, 0x804, s32) = 0;
    for (j = 0; j < 8; j++) {
        PF(p, 0x808 + j * 4, s32) = 0;
    }
    PF(p, 0x208, s32) = 0;
    PF(p, 0x20C, s32) = 0;
    PF(p, 0x204, s32) = 0;
    cls = lbl_80282930[i];
    PF(p, 0x910, f32) = 0.0f;
    PF(p, 0x6B8, s32) = 0;
    PF(p, 0x6BC, s32) = 0;
    PF(p, 0x838, f32) = 0.0f;
    PF(p, 0x83C, f32) = PF(cls, 0x50, f32);
    PF(p, 0x840, f32) = 0.0f;
    PF(p, 0x844, f32) = 0.0f;
    PF(p, 0x848, f32) = PF(cls, 0x54, f32);
    PF(p, 0x84C, f32) = 0.0f;
    PF(p, 0x858, f32) = 0.0f;
    PF(p, 0x85C, f32) = 0.0f;
    PF(p, 0x860, f32) = 0.0f;
    PF(p, 0x870, f32) = 0.0f;
    PF(p, 0x874, f32) = 0.0f;
    PF(p, 0x878, f32) = 0.0f;
    PF(p, 0x888, f32) = 0.0f;
    PF(p, 0x88C, f32) = 0.0f;
    PF(p, 0x890, f32) = 0.0f;
    PF(p, 0x898, f32) = 0.0f;
    PF(p, 0x89C, f32) = 0.0f;
    PF(p, 0x8D0, f32) = 0.0f;
    PF(p, 0x8D4, u32) = 0;
    PF(p, 0x8D8, s32) = 0;
    PF(p, 0x8DC, f32) = 0.0f;
    PF(p, 0x8E0, f32) = 0.0f;
    PF(p, 0x8E4, f32) = 0.0f;
    PF(p, 0x8E8, f32) = 0.0f;
    PF(p, 0x8EC, f32) = 0.0f;
    PF(p, 0x8A0, f32) = 0.68f;
    PF(p, 0x8A4, f32) = 0.36f;
    PF(p, 0x7DC, f32) = 0.0f;
    PF(p, 0x95A, s16) = 0;
    PF(p, 0x1F0, s16) = 0;
    PF(p, 0x1F2, s16) = 0;
    PF(p, 0x1F6, s16) = 0;
    PF(p, 0x1F8, s16) = 0xF0;
    PF(p, 0x200, s16) = 0;
    PF(p, 0x202, s16) = 0;
    PF(p, 0x8F4, s32) = 0;
    PF(p, 0x8F8, s32) = 0;
    PF(p, 0x900, u32) = 0;
    PF(p, 0x8FC, f32) = 0.0f;
    PF(p, 0x904, f32) = 0.0f;
    PF(p, 0x908, s32) = 0;
    PF(p, 0x90C, s32) = 0;
    PF(p, 0x914, f32) = 0.0f;
    PF(p, 0x918, s32) = 0;
    PF(p, 0xA48, f32) = 10000.0f;
    PF(p, 0xA4C, f32) = 10000.0f;
    PF(p, 0xA50, f32) = 10000.0f;
    PF(p, 0xA54, f32) = 10000.0f;
    PF(p, 0x956, s16) = 0x10;
    PF(p, 0x958, s16) = 0;
    PF(p, 0x954, s16) = 0;
    PF(p, 0x95C, s16) = 0;
    PF(p, 0x950, s16) = 0;
    PF(p, 0xA2C, s32) = 0;
    PF(p, 0xA30, u32) = lbl_801201C4;
    PF(p, 0x952, s16) = 0;
    PF(p, 0x91C, s32) = PF(p, 0x920, s32);
    PF(p, 0x8A8, s32) = 0;
    PF(p, 0xA24, s32) = 0;
    PF(p, 0xA28, f32) = 0.0f;
    PF(p, 0xA68, s32) = 0;
    PF(p, 0x93C, s32) = 0;
    PF(p, 0x940, s32) = 0;
    PF(p, 0x8B0, s32) = 0;
    PF(p, 0xA58, f32) = 0.0f;
    PF(p, 0xA5C, s32) = 0;
    PF(p, 0x95E, s16) = 0;
    PF(p, 0x960, s16) = 0;
    PF(p, 0x962, s16) = 0;
    PF(p, 0x964, s16) = 0;
    PF(p, 0xA1C, s16) = 0;
    PF(p, 0xA1E, s16) = 0;
    PF(p, 0xA20, s16) = 0;
    PF(p, 0x924, f32) = 0.0f;
    PF(p, 0x92C, f32) = 0.75f;
    PF(p, 0x930, s32) = 0;
    for (j = 0; j < 5; j++) {
        PF(p, 0xA34 + j * 4, s32) = -1;
    }
    PF(p, 0x11C, s32) = 0;
    PF(p, 0x120, u32) = 0;
    PF(p, 0x124, u32) = 0;
    PF(p, 0x128, s32) = 0;
    PF(p, 0x12C, s32) = 0;
    for (j = 0; j < 11; j++) {
        if (PUP_TYPE(p, j) == 9 && (PUP_SPECIALFLAGS(p, j) & 8)) {
            PUP_TIMELEFT(p, j) = 0.0f;
        }
    }
    PF(p, 0x8C4, s32) = 0;
    if ((lbl_802575BC & 1) == 0 || gGameMode != 0x400B) {
        setup_player_display(i);
    }
    if (lbl_80344DA4 != 0) {
        get_player_pos();
        CreateYPRMatrix(m, (f32*)((u8*)p + 0xC4), NULL);
        CopyMat3(m, p->mat);
        PF(p, 0x894, f32) = PF(p, 0xC8, f32);
        PF(p, 0x8B4, f32) = p->pos[1];
        PF(p, 0x87C, f32) = p->pos[0];
        PF(p, 0x880, f32) = p->pos[1];
        PF(p, 0x884, f32) = p->pos[2];
        if ((p->hud_flags & 0x20) == 0) {
            UpdateObjWorldMat(p->mat);
            fn_8005A404(p->mat, p->anchor_fwd, p->anchor_pos);
        }
        *(f32*)(PF(p, 0x6C8, u8*) + 0x30) = *(f32*)(p->node + 0x30);
        *(f32*)(PF(p, 0x6C8, u8*) + 0x34) = *(f32*)(p->node + 0x34);
        *(f32*)(PF(p, 0x6C8, u8*) + 0x38) = *(f32*)(p->node + 0x38);
    }
}

/* ------------------------------------------------------------------ */
/* save image / per-character stats                                    */
/* ------------------------------------------------------------------ */

typedef struct PlayerSaveImage {
    u8 bytes[0x1434];
} PlayerSaveImage;

typedef struct PlayerMemcardView {
    u8 _pad0000[0xA80];
    PlayerSaveImage image;
    u8 _pad1EB4[0x18];
    PlayerSaveImage backup;
    u8 _pad3300[0x4C];
    s32 cardFile;
    s32 cardDirectory;
    u8 _pad3354[4];
    s32 cardSlot;
} PlayerMemcardView;

/* Memcard read into the save image, then unpack (msg on failure).     */
s32 PlayerLoadSaveFile(s32 i, s32 slot) {
    s32 player = i;
    s32 size[2];
    s32 ok;
    s32 j;
    PlayerMemcardView* p = &((PlayerMemcardView*)gPlayers)[player];

    p->cardSlot = slot;
    size[0] = sizeof(p->image);
    do {
        ok = saveLoad(p->cardFile, p->cardDirectory, p->cardSlot,
                      &p->image, size);
        if (ok == 0 && memCardErrorPrompt("Game load failed...") == 0) {
            break;
        }
    } while (ok == 0);
    if (ok != 0) {
        j = InitPreferences(p->cardFile, p->cardDirectory);
        if (j != 0) {
            OptionsSetup(j);
        }
    }
    player_get_from_save(p, -1);
    p->image.bytes[0xB] = 1;
    /* image -> backup */
    p->backup = p->image;
    for (j = 0; j < 0x100; j++) {
        p->image.bytes[0x1334 + j] &= 0xF0;
    }
    change_player(player, ((Player*)p)->character);
    return ok;
}

/* Pack and memcard-write the save image (msg on failure).             */
s32 PlayerWriteSaveFile(s32 i, s32 slot) {
    PlayerMemcardView* p = &((PlayerMemcardView*)gPlayers)[i];
    s32 ok;

    p->cardSlot = slot;
    player_store_in_save(p);
    do {
        ok = saveSave(p->cardFile, p->cardDirectory, p->cardSlot,
                      &p->image, sizeof(p->image));
        if (ok == 0 && memCardErrorPrompt("Game save failed...") == 0) {
            break;
        }
    } while (ok == 0);
    p->image.bytes[0xB] = 1;
    return ok;
}

/* Revive dying players and restore per-character health.              */
void PlayersRestoreHealth(void) {
    s32 chartype;
    Player* p = P(0);
    f32 cap;
    s32 i;
    u8 unused[8];

    for (i = 0; i < 4; i++, p++) {
        if (p->state == 8) {
            p->state = 1;
        }
        chartype = p->character;
        if (chartype == 2 && HIDDEN_CODE(p) == lbl_80343D6C) {
            cap = 0.5 * (p->level - 1) + 30.0;
            if (cap > 9999.0f) {
                cap = 9999.0f;
            }
            p->health = cap;
        } else {
            p->health = *(f32*)&CHAR_STATS(p, chartype)[1];
        }
    }
}

/* Restore the level-start snapshot (image <- backup, then unpack).    */
void PlayerRestoreState(s32 player) {
    Player* p = P(player);
    f32 cap;

    if (p->character == 2 && HIDDEN_CODE(p) == lbl_80343D6C) {
        cap = 0.5 * (p->level - 1) + 30.0;
        if (cap > 9999.0f) {
            cap = 9999.0f;
        }
        p->health = cap;
    } else {
        *(PlayerSaveImage*)((u8*)p + 0xA80) =
            *(PlayerSaveImage*)((u8*)p + 0x1ECC);
        player_get_from_save(p, -1);
    }
}

/* Snapshot the running state (pack, then backup <- image).            */
void PlayerSaveState(s32 player, s32 full) {
    Player* p = P(player);

    if (p->character == 2 && HIDDEN_CODE(p) == lbl_80343D6C) {
        return;
    }
    player_store_in_save(p);
    if (full != 0 && !(sMusicTrackHi == 5 && sMusicTrackLo == 1) &&
        !(sMusicTrackHi == 6 && sMusicTrackLo == 1)) {
        *(PlayerSaveImage*)((u8*)p + 0x1ECC) =
            *(PlayerSaveImage*)((u8*)p + 0xA80);
    }
    PF(p, 0xA8B, u8) = 0;
}

/* Unpack the per-character slots into the live fields.  type < 0      */
/* re-reads the character stamped in the image.  The active hidden     */
/* character instead gets the fixed lv99 loadout.                      */
void player_get_from_save(void* vp, s32 type) {
    Player* p = vp;
    s32* st;
    u8* it;
    f32 cap;
    s32 t;
    s32 lv;

    if (p->character == 2 && HIDDEN_CODE(p) == lbl_80343D6C) {
        /* hidden character: fixed loadout */
        memcpy((u8*)p + 0xA80, (u8*)p + 0x1ECC, 0x1434);
        p->class_id = 0;
        ATT_FIGHT(p) = 0.9f;
        ATT_ARMOR(p) = 0.9f;
        ATT_MAGIC(p) = 0.9f;
        ATT_SPEED(p) = 0.9f;
        PlayerUpdateAtts(p);
        p->level = 99;
        p->exp = 0x54218;
        cap = 0.5 * (p->level - 1) + 30.0;
        if (cap > 9999.0f) {
            cap = 9999.0f;
        }
        p->health = cap;
        p->gold = 5000;
        PF(p, 0x1EBC, s32) = 9;
        PF(p, 0x1EB8, s32) = 9;
        PF(p, 0x1EC8, u16) = 0x7FE;
        PF(p, 0x1ECA, u16) = 0x1FFF;
        for (t = 0; t < 11; t++) {
            memset((u8*)p + 0x130 + t * 0x10, 0, 0x10);
        }
        PF(p, 0x1EC, s32) = 0;
        PF(p, 0x124, u32) = 0;
        return;
    }

    t = type;
    if (t < 0) {
        t = PF(p, 0xA88, s16);
    }
    p->character = t;
    p->class_id = PF(p, 0xA8A, s8);
    st = CHAR_STATS(p, p->character);
    PlayerUpdateAtts(p);
    p->exp = st[0];
    for (lv = 99; lv > 0; lv--) {
        if (LevelToExp(lv) <= p->exp) {
            break;
        }
    }
    if (lv < 1) {
        lv = 1;
    }
    p->level = lv;
    p->health = *(f32*)&st[1];
    it = CHAR_ITEMS(p, t);
    p->gold = *(s32*)(it + 0x30);
    PF(p, 0x1EBC, s32) = *(s16*)(it + 0x2);
    PF(p, 0x1EB8, s32) = *(s16*)(it + 0x0);
    PF(p, 0x1EC8, u16) = *(u16*)(it + 0x4);
    PF(p, 0x1ECA, u16) = *(u16*)(it + 0x6);
    if (*(f32*)&st[1] == 0.0f) {
        clear_player(p->index, 0);
    }
    p->character = t;
    p->char_type = t;
    if (p->char_type > 7) {
        p->char_type -= 8;
    }
    check_player_atts(p, t, NULL);
    memcpy((u8*)p + 0x130, it + 0x34, 0xB0);
    PF(p, 0x1EC, s32) = *(s16*)(it + 0xA);
    PF(p, 0x11C, s32) = 0;
    PF(p, 0x120, u32) = 0;
    PF(p, 0x124, u32) = 0;
    lbl_80240E5C[p->index * 0xF] = PF(p, 0x1DB0, u8);
    lbl_80240E60[p->index * 0xF] = PF(p, 0x1DB1, u8);
    lbl_80240E68[p->index * 0xF] = PF(p, 0x1DB2, u8);
    lbl_80240E64[p->index * 0xF] = PF(p, 0x1DB3, u8);
}

/* Pack the live fields into the per-character slots + image header.   */
void player_store_in_save(void* vp) {
    Player* p = vp;
    s32* st;
    u8* it;
    s32 total;
    s32 exp;
    s32 lv;
    s32 j;

    if (p->character == 2 && HIDDEN_CODE(p) == lbl_80343D6C) {
        /* hidden char: park it, restore the base character, re-flag */
        memcpy((u8*)p + 0xA80, (u8*)p + 0x1ECC, 0x1434);
        HIDDEN_CODE(p) = NULL;
        player_get_from_save(p, -1);
        HIDDEN_CODE(p) = lbl_80343D6C;
    }
    st = CHAR_STATS(p, p->character);
    it = CHAR_ITEMS(p, p->character);
    st[0] = p->exp;
    *(f32*)&st[1] = p->health;
    *(s32*)(it + 0x30) = p->gold;
    *(s16*)(it + 0x0) = (s16)PF(p, 0x1EB8, s32);
    *(s16*)(it + 0x2) = (s16)PF(p, 0x1EBC, s32);
    *(u16*)(it + 0x4) |= PF(p, 0x1EC8, u16);
    *(u16*)(it + 0x6) |= PF(p, 0x1ECA, u16);
    PF(p, 0xA88, s16) = (s16)p->character;
    PF(p, 0xA8A, s8) = (s8)p->class_id;
    /* total-level checksum across all 16 characters */
    total = 0;
    for (j = 0; j < 16; j++) {
        exp = CHAR_STATS(p, j)[0];
        for (lv = 99; lv > 0; lv--) {
            if (lv < 0x3D) {
                if ((lv - 1) * (lv * 0x1E + 1000) <= exp) {
                    break;
                }
            } else if ((lv - 0x3C) * 0x11F8 + 0x28550 <= exp) {
                break;
            }
        }
        if (lv < 1) {
            lv = 1;
        }
        total += lv;
    }
    PF(p, 0xA8E, s16) = (s16)total;
    memcpy(it + 0x34, (u8*)p + 0x130, 0xB0);
    *(s16*)(it + 0xA) = (s16)PF(p, 0x1EC, s32);
    PF(p, 0x1DB0, u8) = (u8)lbl_80240E5C[p->index * 0xF];
    PF(p, 0x1DB1, u8) = (u8)lbl_80240E60[p->index * 0xF];
    PF(p, 0x1DB2, u8) = (u8)lbl_80240E68[p->index * 0xF];
    PF(p, 0x1DB3, u8) = (u8)lbl_80240E64[p->index * 0xF];
    if (p->character == 2 && HIDDEN_CODE(p) == lbl_80343D6C) {
        player_get_from_save(p, -1);
    }
}

/* Copy the live pad config into both control-save byte sets.          */
void player_save_controls(s32 i) {
    Player* p = P(i);

    PF(p, 0x1DB0, u8) = (u8)lbl_80240E30[i].scheme;
    PF(p, 0x1DB1, u8) = (u8)lbl_80240E30[i].hasActuator;
    PF(p, 0x1DB2, u8) = (u8)lbl_80240E30[i].unk38;
    PF(p, 0x1DB3, u8) = (u8)lbl_80240E30[i].unk34;
    PF(p, 0x31FC, u8) = (u8)lbl_80240E30[i].scheme;
    PF(p, 0x31FD, u8) = (u8)lbl_80240E30[i].hasActuator;
    PF(p, 0x31FE, u8) = (u8)lbl_80240E30[i].unk38;
    PF(p, 0x31FF, u8) = (u8)lbl_80240E30[i].unk34;
}

/* Derive the combat stats from the attribute norms x class ranges.    */
void PlayerUpdateAtts(void* vp) {
    Player* p = vp;
    f32* r = lbl_80343D7C;

    LoadPlyrData(p->index, p->character, NULL);
    if (p->character != 2 || HIDDEN_CODE(p) != lbl_80343D6C) {
        check_player_atts(p, p->character, NULL);
    }
    STAT_DMG(p)   = 0.01 * ATT_FIGHT(p) * (r[1] - r[0]) + r[0];
    STAT_ARMOR(p) = 0.01 * ATT_ARMOR(p) * (r[3] - r[2]) + r[2];
    STAT_MAGIC(p) = 0.01 * ATT_MAGIC(p) * (r[5] - r[4]) + r[4];
    STAT_SPEED(p) = 0.01 * ATT_SPEED(p) * (r[7] - r[6]) + r[6];
    if (p->char_type == 2 || p->char_type == 6) {
        /* magic-missile classes scale missiles on magic */
        STAT_MDMG(p) = 0.01 * ATT_MAGIC(p) * (r[9] - r[8]) + r[8];
        STAT_MSPD(p) = 0.01 * ATT_MAGIC(p) * (r[11] - r[10]) + r[10];
    } else {
        STAT_MDMG(p) = 0.01 * ATT_FIGHT(p) * (r[9] - r[8]) + r[8];
        STAT_MSPD(p) = 0.01 * ATT_FIGHT(p) * (r[11] - r[10]) + r[10];
    }
}

/* Zero the per-character bonus stats for all 16 characters.           */
void set_player_default_atts(void* p) {
    s32 j = 0;
    s32 index = ((Player*)p)->index;
    s32 chartype = ((Player*)p)->character;

    for (; j < 16; j++) {
        LoadPlyrData(index, j, NULL);
        *(f32*)((u8*)p + j * 0x18 + 0xA98) = 0.0f;
        *(f32*)((u8*)p + j * 0x18 + 0xA9C) = 0.0f;
        *(f32*)((u8*)p + j * 0x18 + 0xAA0) = 0.0f;
        *(f32*)((u8*)p + j * 0x18 + 0xAA4) = 0.0f;
    }
    check_player_atts(p, chartype, NULL);
}

/* ------------------------------------------------------------------ */
/* geo / model loading                                                 */
/* ------------------------------------------------------------------ */

/*
 * Build the in-world player: scene node, atree, wrist/head/glow nodes,
 * weapon and shadow.  FatalError if a node already exists.
 */
void load_player_geo(s32 i, void* vp) {
    Player* p = vp;
    char name[20];
    char* c;
    s32* nd;
    s32 pad;
    s32 cls;
    s32 tier;
    s32 n;

    if (p->node != NULL) {
        FatalError("PLAYER.OBJ NODE EXISTS BEFORE LOAD %x", 0x800000);
    }
    if (lbl_80344828 > 0) {
        set_hidden_player(p);
    }
    if (HIDDEN_CODE(p) == NULL) {
        if (p->character == player_multiple_models[i].cur_class &&
            p->class_id == player_multiple_models[i].cur_pad &&
            player_multiple_models[i].cur_override == 0 &&
            p->level / 10 == player_multiple_models[i].cur_tier) {
            PF(p, 0x7F4, void*) = player_multiple_models[i].arena;
        } else {
            PF(p, 0x7F4, u32) = load_player_model(i, p, -1, NULL);
        }
    } else {
        PF(p, 0x7F4, u32) = load_player_model(i, p, i, HIDDEN_CODE(p));
    }
    pad = p->class_id;
    LoadPlyrData(i, p->character, NULL);
    if (sWeaponsBuf != 0) {
        InitPlayerMissiles(p);
    }
    cls = p->character;
    if (HIDDEN_CODE(p) == NULL) {
        strcpy(name, lbl_80120104[pad]);
    } else {
        strcpy(name, HIDDEN_CODE(p));
        for (c = name; *c != 0; c++) {
            *c = (char)toupper(*c);
        }
    }
    sprintf(tbuf, "%s%s%s", lbl_801200B0 + cls * 4, name, "");
    strncpy((char*)p + 0x6C0, tbuf, 8);
    p->node = MBNewNode(lbl_80344B2C, gIdentityMatrix, 1);
    PF(p, 0x78, s32) = 0;
    n = fn_80011BBC(player_multiple_models[i].model_buf, (char*)(lbl_801200B0 + p->char_type * 4),
                    (u8*)p + 0x7C, tbuf, 0x800);
    PF(p, 0x7C, s32) = n;
    if (PF(p, 0x7C, s32) == 0) {
        FatalErrorf("Player Atree %s not found", lbl_801200B0 + p->char_type * 4);
    }
    MBNodeSetParent(*(void**)PF(p, 0x7C, s32*), p->node);
    InitActions((u8*)p + 0x7C, (u8*)p + 0x210, 0x80126C68);
    if (gGameMode != 0x4012 && gGameMode != 0x400D &&
        gGameMode != 0x400F && gGameMode != 0x4016) {
        LoadPlyrData(i, p->character, (void*)1);
    }
    PF(p, 0x744, s32) = 0;
    /* attachment nodes */
    sprintf(tbuf, "%s%s", (char*)p + 0x6C0, lbl_80120184[cls]);
    n = MBOX_ReallyFindObject(tbuf, PF(p, 0x7F4, s32), PF(p, 0x7F4, s32), 1);
    nd = AtreeFindMbidxNode((void*)PF(p, 0x7C, s32), n);
    PF(p, 0x6D0, s32) = (nd == NULL) ? 0 : *nd;
    sprintf(tbuf, "%s%s", (char*)p + 0x6C0, lbl_80120144[cls]);
    n = MBOX_ReallyFindObject(tbuf, PF(p, 0x7F4, s32), PF(p, 0x7F4, s32), 1);
    nd = AtreeFindMbidxNode((void*)PF(p, 0x7C, s32), n);
    PF(p, 0x6CC, s32) = (nd == NULL) ? 0 : *nd;
    sprintf(tbuf, "%sCFGLOW", (char*)p + 0x6C0);
    n = MBOX_ReallyFindObject(tbuf, PF(p, 0x7F4, s32), PF(p, 0x7F4, s32), -1);
    nd = (n < 0) ? NULL : AtreeFindMbidxNode((void*)PF(p, 0x7C, s32), n);
    PF(p, 0x6D8, s32) = (nd == NULL) ? 0 : *nd;
    if (nd != NULL) {
        MBTreeSetFlags((void*)*nd, 0x800810, 0);
    }
    sprintf(tbuf, "%sHEAD", (char*)p + 0x6C0);
    n = MBOX_ReallyFindObject(tbuf, PF(p, 0x7F4, s32), PF(p, 0x7F4, s32), 1);
    nd = AtreeFindMbidxNode((void*)PF(p, 0x7C, s32), n);
    PF(p, 0x6D4, s32) = (nd == NULL) ? 0 : *nd;
    sprintf(tbuf, "%sTORSO", (char*)p + 0x6C0);
    n = MBOX_ReallyFindObject(tbuf, PF(p, 0x7F4, s32), PF(p, 0x7F4, s32), 1);
    nd = AtreeFindMbidxNode((void*)PF(p, 0x7C, s32), n);
    PF(p, 0x6DC, s32) = (nd == NULL) ? 0 : *nd;
    /* weapon */
    if (sWeaponsBuf != 0) {
        tier = (p->level >= 0x32) ? 2 : (p->level >= 10) ? 1 : 0;
        if (lbl_80120598[p->character] == 0 && p->character < 8 && HIDDEN_CODE(p) == NULL) {
            sprintf(tbuf, "WEAP %s HD%d", lbl_80120104[pad], tier + 1);
        } else {
            sprintf(tbuf, "WEAP_HOLD", cls, tier);
        }
        n = MBOX_ReallyFindObject(tbuf, PF(p, 0x7F4, s32), PF(p, 0x7F4, s32), 1);
        PF(p, 0x6E0, void*) = MBNewObject(n, NULL, (void*)PF(p, 0x6D0, s32), 0x810);
        PF(p, 0x7F8, s32) = -1;
        if (p->char_type == 7) {
            *(u32*)(PF(p, 0x6E0, u8*) + 0x60) |= 0x4000000;
            PF(p, 0x7F8, s32) = AddSpecialTexmod(PF(p, 0x7F4, s32), "%s%s", (char*)player_multiple_models[i].sfx_arena,
                                            "", 5, 1);
        }
    }
    /* live combat fields */
    PF(p, 0x72C, s32) = 0;
    PF(p, 0x730, s32) = 0;
    PF(p, 0x734, s32) = 0;
    PF(p, 0x748, s32) = 0;
    PF(p, 0x790, s32) = 0;
    PF(p, 0x6E4, s32) = 0;
    PF(p, 0x73C, s32) = 0;
    PF(p, 0x738, s32) = -1;
    PF(p, 0x740, s32) = 0;
    PF(p, 0x968, s32) = 0;
    PF(p, 0xA20, s16) = 0;
    PF(p, 0xA1C, s16) = 0;
    PF(p, 0xA1E, s16) = 0;
    PF(p, 0x96C, s32) = 0;
    PF(p, 0xA14, s32) = 0;
    /* shadow */
    n = MBOX_ReallyFindObject("SHADOWL1", PF(p, 0x7F4, s32), PF(p, 0x7F4, s32), 1);
    PF(p, 0x6C8, void*) = MBNewObject(n, gIdentityMatrix, NULL, 0x880);
    *(s16*)(PF(p, 0x6C8, u8*) + 0x68) = -0x24;
    PF(p, 0x7FC, f32) = 0.0f;
    if (PF(p, 0x6D0, s32) != 0) {
        MBPsysSetDebugNode(PF(p, 0x6D0, s32), 0);
    } else if (PF(p, 0x6CC, s32) != 0) {
        MBPsysSetDebugNode(PF(p, 0x6CC, s32), 0);
    } else if (PF(p, 0x6D4, s32) != 0) {
        MBPsysSetDebugNode(PF(p, 0x6D4, s32), 0);
    }
    MBTreeSetFlags(p->node, 2, 0);
    if (PF(p, 0x6C8, s32) != 0) {
        if (sMusicTrackHi == 0xC && sMusicTrackLo == 8) {
            MBTreeSetFlags(PF(p, 0x6C8, void*), 2, 1);
        } else {
            MBTreeSetFlags(PF(p, 0x6C8, void*), 2, 0);
        }
    }
}

/*
 * Hidden-character/cheat name hook, called from load_player_geo when
 * secret characters are enabled.  Compares p->name against the cheat
 * strings ("Access?", "Unlimited?", "NoDamage?", "Shards?", "Runes?",
 * "Cheats?", "Select a character?", "Worlds?"), the 27-entry hidden
 * character table ("ICE600".."Rand??") and the 27-entry powerup-cheat
 * table ("INVULN"..); sets p->hidden_code + class/char on a match.
 */
s32 set_hidden_player(void* vp) {
    Player* p = vp;
    char* nm = (char*)p + 0xA80;
    s32 pick = -1;
    u32 pups = 0;
    s32 match = 0;
    s32 j;

    if (strncmp(nm, "ACCESS", 6) == 0) {
        pick = 0x10;
        match = 1;
    }
    /* the interactive cheat menu (start+trigger names) */
    if ((strncmp(nm, "CHEATS", 6) == 0 || strncmp(nm, "SELECT", 6) == 0 ||
         strncmp(nm, "WORLDS", 6) == 0) &&
        any_level(0x100000) != 0 && any_level(0x400000) != 0) {
        if (saveMenuPrompt("Access?", NULL, 2) == 0) {
            if (saveMenuPrompt("Levels?", NULL, 2) == 0) {
                lbl_802575A8 = 1;
            }
            if (saveMenuPrompt("Unlimited?", NULL, 2) == 0) {
                lbl_80257594 = 3;
            }
            if (saveMenuPrompt("NoDamage?", NULL, 2) == 0) {
                gGameOptions = 1;
            }
            if (saveMenuPrompt("Shards?", NULL, 2) == 0) {
                PF(p, 0x1EC8, u16) = 0xFFFF;
            }
            if (saveMenuPrompt("Runes?", NULL, 2) == 0) {
                PF(p, 0x1ECA, u16) = 0xFFFF;
            }
            if (saveMenuPrompt("Cheats?", NULL, 2) == 0) {
                pups = 0xFFFFFFFF;
            }
            if (saveMenuPrompt("Select a character?", NULL, 2) == 0) {
                /* d-pad browse through the hidden character list */
                while (any(0x80000000) == 0) {
                    if (any(0x40000000) != 0) {
                        pick++;
                        if (pick >= 0 && pick < 27) {
                            saveMenuPrompt(Hidden[pick].name, NULL, 1);
                        }
                        if (pick >= 27) {
                            pick = (rand() & 0xFF) % 27;
                            saveMenuPrompt("Rand??", NULL, 1);
                            break;
                        }
                    }
                    ControlsUpdate(0, NULL, 0);
                    ReadControls();
                }
            }
            if (saveMenuPrompt("Worlds?", NULL, 2) == 0) {
                for (j = 0; j < 16; j++) {
                    memset((u8*)p + 0x1CD0 + p->character * 0xE, 0xFF, 0xE);
                    memset(CHAR_ITEMS(p, j) + 0x1E, 0xFF, 0x20);
                    *(u16*)(CHAR_ITEMS(p, p->character) + 0xC) = 0xFFFF;
                    *(u16*)(CHAR_ITEMS(p, p->character) + 0x18) = 0xFFFF;
                }
            }
            saveMenuPrompt("Mike and Bob say, Thank You for Playing!", NULL, 1);
        }
    }
    /* one-shot cheat names */
    if (any_level(0x100000) != 0 && any_level(0x400000) != 0) {
        if (strncmp(nm, "RANDOM", 6) == 0) {
            match = 1;
            pick = (rand() & 0xFF) % 27;
            pups = rand();
        }
        if (strncmp(nm, "SECRET", 6) == 0) {
            match = 1;
            pick = 5;
            pups = rand();
        }
        if (strncmp(nm, "ALLFUL", 6) == 0) {
            match = 1;
            pick = 1;
            pups = rand();
            /* plus the full unlock block (same as Worlds?) */
            lbl_802575A8 = 1;
            lbl_80257594 = 3;
            gGameOptions = 1;
            PF(p, 0x1EC8, u16) = 0xFFFF;
            PF(p, 0x1ECA, u16) = 0xFFFF;
            for (j = 0; j < 16; j++) {
                memset((u8*)p + 0x1CD0 + p->character * 0xE, 0xFF, 0xE);
                memset(CHAR_ITEMS(p, j) + 0x1E, 0xFF, 0x20);
                *(u16*)(CHAR_ITEMS(p, p->character) + 0xC) = 0xFFFF;
                *(u16*)(CHAR_ITEMS(p, p->character) + 0x18) = 0xFFFF;
            }
        }
        if (strncmp(nm, "TOWER?", 6) == 0) {
            match = 1;
            pick = 0x17;
            pups = rand();
        }
    }
    if (HIDDEN_CODE(p) == NULL) {
        for (j = 0; j < 27; j++) {
            if ((strncmp(nm, Hidden[j].name, 6) == 0 &&
                 (Hidden[j].unlocked == 0 || lbl_80344828 > 1)) ||
                (match && pick == j)) {
                p->class_id = Hidden[j].class_id;
                p->character = Hidden[j].char_type;
                HIDDEN_CODE(p) = Hidden[j].code;
                return 1;
            }
        }
        for (j = 0; j < 27; j++) {
            if (strncmp(nm, Cheats[j].name, 6) == 0 || (pups & (1 << j))) {
                switch (Cheats[j].type) {
                case 1:
                    p->gold = (s32)Cheats[j].value;
                    break;
                case 2:
                    PF(p, 0x1EB8, s32) = (s32)Cheats[j].value;
                    break;
                case 4:
                    PF(p, 0x1EBC, s32) = (s32)Cheats[j].value;
                    break;
                default:
                    PlayerAddPowerup(Cheats[j].value, 1.0f, p,
                                     Cheats[j].type, Cheats[j].mask);
                    if (Cheats[j].type == 9) {
                        PF(p, 0x124, u32) |= Cheats[j].mask;
                    }
                    break;
                }
            }
        }
        return 0;
    }
    if (HIDDEN_CODE(p) == lbl_80343D6C) {
        p->char_type = 2;
        p->character = 2;
        return 0;
    }
    return 1;
}

/* Load the class model + sfx model set into player slot i.            */
s32 load_player_model(s32 i, void* vp, s32 alt, char* name) {
    Player* p = vp;
    u8* pot = (u8*) potionicon_tab;
    s32 prod;
    u8* q;
    s32 cls;
    s32 ret;
    s32 t;
    s32 raw;

    prod = i * 76;
    q = pot + i * 13148;
    cls = *(s32*) (q + 3140);
    ret = load_player_model_sub(i, vp, cls, name, pot + prod + 1300);
    raw = *(s32*) ((u8*) p + 12);
    t = raw;
    if (raw >= 8) {
        t -= 8;
    }
    if (cls < 0 || cls >= 4) {
        cls = i;
    }
    sprintf((char*) pot + 1268, lbl_80114098,
            (char*) lbl_8012006C + t * 4, lbl_801200F4[cls]);
    q = pot + prod;
    cls = MBOX_LoadModelFixed((char*) pot + 1268, *(u32*) (q + 1360), 0, NULL,
                              *(u32*) (q + 1352));
    *(s32*) (q + 1352) = cls;
    MLMReadFile((char*) pot + 1268, lbl_80347A38, *(u32*) (q + 1364),
                *(char**) (q + 1372));
    *(s32*) (q + 1356) =
        fn_8001267C(*(u16**) (q + 1372), cls, *(s32*) (q + 1356));
    InitTexMods(*(void**) (q + 1336), *(u32*) (q + 1316));
    InitTexMods(*(void**) (q + 1348), *(u32*) (q + 1316));
    InitTexMods(*(void**) (q + 1372), *(u32*) (q + 1352));
    return ret;
}

/* Load one class model + anim set into a model slot.                  */
s32 load_player_model_sub(s32 i, void* vp, s32 cls_in, char* name, void* vslot) {
    PlayerModelSlot* slot = vslot;
    u8* pot = (u8*) potionicon_tab;
    u8* tab = (u8*) lbl_8011FC48;
    u8* fmt = (u8*) lbl_80113AE0;
    u8* q;
    s32 tier;
    s32 ct;
    s32 ct8;
    s32 cls;
    u32 arena;

    q = (u8*) vp;
    tier = ((Player*) vp)->level / 10;
    ct = *(s32*) (q + 12);
    q = pot + i * 13148;
    cls = *(s32*) (q + 3140);
    ct8 = ct;
    if (ct >= 8) {
        ct8 -= 8;
    }
    if (name != NULL) {
        sprintf((char*) pot + 1268, (char*) fmt + 1484, tab + ct * 4 + 1060, name);
    } else if (*(s32*) (tab + ct * 4 + 2384) != 0) {
        sprintf((char*) pot + 1268, (char*) fmt + 1500, tab + ct * 4 + 1060,
                *(char**) (tab + cls * 4 + 1196), tier);
    } else {
        sprintf((char*) pot + 1268, (char*) fmt + 1484, tab + ct * 4 + 1060,
                *(char**) (tab + cls * 4 + 1196));
    }
    arena = MBOX_LoadModelFixed((char*) pot + 1268, slot->model_max, 0, NULL,
                                (u32) slot->arena);
    if ((s32) slot->anim_max > 0) {
        MLMReadFile((char*) pot + 1268, lbl_80347A38, slot->anim_max, slot->model_buf);
    } else {
        slot->model_buf = AllocFile((char*) pot + 1268, lbl_80347A38, slot->anim_max);
    }
    slot->anim_remap2 = fn_8001267C((u16*) slot->model_buf, arena, slot->anim_remap2);
    slot->arena = (void*) arena;
    slot->cur_class = ct;
    slot->cur_pad = cls;
    slot->cur_tier = tier;
    slot->cur_override = (s32) name;
    sprintf((char*) pot + 1268, (char*) fmt + 1520, tab + ct8 * 4 + 1060);
    if ((s32) slot->model_buf_max > 0) {
        MLMReadFile((char*) pot + 1268, lbl_80347A38, slot->model_buf_max,
                    slot->anim_buf);
    } else {
        slot->anim_buf = AllocFile((char*) pot + 1268, lbl_80347A38,
                                   slot->model_buf_max);
    }
    slot->anim_remap = fn_8001267C((u16*) slot->anim_buf, arena, slot->anim_remap);
    return arena;
}

/* Re-register the model texmods after a video mode change.            */
void SetupPlayerTexMods(s32 i) {
    if (player_multiple_models[i].cur_class >= 0) {
        DoTexMods(player_multiple_models[i].model_buf);
        DoTexMods(player_multiple_models[i].anim_buf);
        DoTexMods(player_multiple_models[i].sfx_buf);
    }
}

/* Apply the per-slot model texmods (items.c per-frame).               */
void DoPlayerTexMods(s32 i) {
    if (player_multiple_models[i].cur_class >= 0) {
        InitTexMods(player_multiple_models[i].model_buf, (u32)player_multiple_models[i].arena);
        InitTexMods(player_multiple_models[i].anim_buf, (u32)player_multiple_models[i].arena);
        InitTexMods(player_multiple_models[i].sfx_buf, (u32)player_multiple_models[i].sfx_arena);
    }
}

/* ------------------------------------------------------------------ */
/* init / HUD blit creation                                            */
/* ------------------------------------------------------------------ */

/* One-time init: per-player HUD blits, got-it table, icon textures.   */
void init_players(void) {
    s32 i;
    u32 tex;

    lbl_80344B24 = -1;
    it_blit = NULL;
    for (i = 0; i < 4; i++) {
        create_player_blits(i);
    }
    for (i = 0; i < 4; i++) {
        lbl_80257630[i] = 0;
    }
    for (i = 0; i < 2; i++) {
        lbl_803447A8[i] = 0;
    }
    lbl_80344B2C = MBNewNode(gSceneRoot, gIdentityMatrix, 1);
    for (i = 0; i < 24; i++) {
        got_it[i].state = 0;
        if (got_it[i].blit2 != NULL) {
            MBRemoveBlit(got_it[i].blit2);
            got_it[i].blit2 = NULL;
        }
        if (got_it[i].blit1 != NULL) {
            MBRemoveBlit(got_it[i].blit1);
            got_it[i].blit1 = NULL;
        }
    }
    welcome_timer = 0;
    alpha = 0;
    key_blit_idx = (s32)MBOX_FindTexture("KEY_ICON", NULL);
    for (i = 0; i < 5; i++) {
        tex = (u32)MBOX_FindTexture(lbl_8011FCD4[i], NULL);
        potionicon_tab[i] = (void*)tex;
    }
}

/* Create every per-player HUD blit set (portrait frames, runes,       */
/* crystals, keys, power meter, rune13, HOD, quest, tb_info).          */
static void create_player_blits(s32 i) {
    u16 lx = lbl_80120238[i];
    u16 rx;
    u32 tex;
    void* b;
    s32 j;

    frame_blit[i][0] = MBCreateBlit(0, 0, lx, 0x130, 0x80, -1);
    frame_blit[i][1] = MBCreateBlit(0, 0, lx, 0x140, 0x80, -1);
    frame_blit[i][2] = MBCreateBlit(0, 0, lx, 0x140, 0x80, -1);
    frame_blit[i][3] = MBCreateBlit(0, 0, lx, 0x158, 0x94, -1);
    frame_blit[i][4] = MBCreateBlit(0, 0, lx + 8, 0x148, 0x14, 0x14);
    frame_blit[i][5] = MBCreateBlit(0, 0, lx, 0x148, 0x14, 0x14);
    for (j = 0; j < 6; j++) {
        mbBlitInit3414(frame_blit[i][j], 1);
        mbBlitCvtCoord(frame_blit[i][j], 0.1f);
    }
    for (j = 0; j < 12; j++) {
        sprintf(tbuf, "SM_RUNE_%s_%02d", lbl_801205D8[j / 3], j % 3 + 1);
        tex = (u32)MBOX_FindTexture_Err(tbuf, NULL, 1);
        rune_blit[i][j] = MBCreateBlit(0, tex, lx + j * 8 + j / 3 + 0xF, 0x132, -1, -1);
        mbBlitInit3414(rune_blit[i][j], 1);
        mbBlitCvtCoord(rune_blit[i][j], 0.1f);
    }
    for (j = 0; j < 8; j++) {
        sprintf(tbuf, "SM_KEY_%s", lbl_801205F8[j]);
        tex = (u32)MBOX_FindTexture_Err(tbuf, NULL, 1);
        crystal_blit[i][j] = MBCreateBlit(0, tex, lx + j * 12 + 0xC, 300, -1, -1);
        mbBlitInit3414(crystal_blit[i][j], 1);
        mbBlitCvtCoord(crystal_blit[i][j], 0.1f);
    }
    for (j = 0; j < 4; j++) {
        key_blit[i][j] = MBCreateBlit(0, 0, lx + 0x1A, j * 3 + 0x142, -1, -1);
        mbBlitInit3414(key_blit[i][j], 1);
        mbBlitCvtCoord(key_blit[i][j], 0.1f);
    }
    for (j = 0; j < 7; j++) {
        b = MBNewBlit((char*)lbl_8011FC48[j * 5 + 1], i * 0x80 + lbl_8011FC48[j * 5 + 2],
                      (u32)lbl_8011FC48[j * 5 + 3]);
        pm_blit[i][j] = b;
        lbl_8011FC48[j * 5] = MBBlitGetTex(b);
        mbBlitInit3414(b, 1);
        mbBlitCvtCoord(b, (f32)-lbl_8011FC48[j * 5 + 4]);
    }
    PF(P(i), 0x3340, s32) = 0;
    rx = lbl_80120240[i];
    tb_info[i].sel = -1;
    tb_info[i].y_top = -1;
    tb_info[i].state = 0;
    tb_info[i].x_right = rx - 0x34;
    tb_info[i].y_top = 0x14F;
    tb_info[i].x_left = rx - 0x40;
    tb_info[i].y_box = 0x143;
    tb_info[i].tex1 = 0xF9F1;
    tb_info[i].tex2 = 0xF9F2;
    tb_info[i].label = NULL;
    rune13_blit[i] = MBCreateBlit(0, 0, rx - 0xE, -0x143, -1, -1);
    tex = (u32)MBOX_FindTexture_Err("BTMBK_LEVL", NULL, 1);
    mbInitBlitEntry(rune13_blit[i], tex, 0);
    mbBlitInit3414(rune13_blit[i], 1);
    mbBlitCvtCoord(rune13_blit[i], 0.1f);
    if (lbl_803447C0 != 0) {
        mbBlitUpdateEntry(rune13_blit[i], -1, 0x100);
    }
    PF(P(i), 0x954, s16) = 0;
    hod_blit[i] = MBCreateBlit(0, 0, lx + 8, 0x154, 0x10, 0x10);
    tex = (u32)MBOX_FindTexture_Err("HODICON", NULL, 1);
    mbInitBlitEntry(hod_blit[i], tex, 0);
    mbBlitInit3414(hod_blit[i], 1);
    mbBlitCvtCoord(hod_blit[i], 0.1f);
    quest_blit[i] = MBCreateBlit(0, 0, lx + 0x68, 0x152, 0x10, 0x10);
    mbBlitInit3414(quest_blit[i], 1);
    mbBlitCvtCoord(quest_blit[i], 0.1f);
    P(i)->node = NULL;
    P(i)->index = i;
}

/* Wipe all four records (keeps index + controller binding).           */
void reset_players(void) {
    Player* p;
    s32 i;

    for (i = 0; i < 4; i++) {
        p = P(i);
        memset(p, 0, 0x335C);
        p->class_id = i;
        p->index = i;
    }
}

/* Allocate the per-player model arenas and file buffers.              */
void setup_player_models(void) {
    PlayerModelSlot* s;
    s32 i;
    s32 free0;

    GetMaxPlayerModelSize();
    for (i = 0; i < 4; i++) {
        s = &player_multiple_models[i];
        free0 = BytesFree();
        bulletproof_printf("Player %d -- MEM %d\n", i, free0);
        sprintf(tbuf, "PLAYER %d", i);
        s->arena = (void*)MBOX_LoadModelFixed(tbuf, s->model_max, (s32)s->arena_max,
                                              tbuf, 0);
        s->cur_class = -1;
        s->cur_override = 0;
        s->model_buf = AllocMem(s->model_buf_max, (s32)s->arena_max, tbuf);
        s->anim_remap = -1;
        s->anim_buf = AllocMem(s->anim_max, (s32)s->arena_max, tbuf);
        s->anim_remap2 = -1;
        s->sfx_arena = (void*)MBOX_LoadModelFixed(tbuf, s->sfx_max, (s32)s->sfx_arena_max,
                                                  tbuf, 0);
        s->sfx_buf = AllocMem(s->sfx_buf_max, (s32)s->sfx_arena_max, tbuf);
        s->sfx_remap = -1;
    }
}

/* Fill/level the per-slot arena budgets (once).                       */
static void GetMaxPlayerModelSize(void) {
    PlayerModelSlot* s;
    s32 i;
    u8 unused[16];

    if (got_max_player_sizes != 0) {
        return;
    }
    for (i = 0; i < 4; i++) {
        s = &player_multiple_models[i];
        s->model_max = 0xB800;
        s->model_buf_max = 0x4D800;
        s->arena_max = 0xB000;
        s->anim_max = 0x1000;
        s->sfx_max = 0x2BC00;
        s->sfx_buf_max = 0xC000;
        s->sfx_arena_max = 0x18000;
    }
    for (i = 1; i < 4; i++) {
        s = &player_multiple_models[i];
        player_multiple_models[0].model_max = player_multiple_models[0].model_max;
        if ((s32)s->model_max > (s32)player_multiple_models[0].model_max) {
            player_multiple_models[0].model_max = s->model_max;
        }
        if ((s32)s->model_buf_max > (s32)player_multiple_models[0].model_buf_max) {
            player_multiple_models[0].model_buf_max = s->model_buf_max;
        }
        if ((s32)s->arena_max > (s32)player_multiple_models[0].arena_max) {
            player_multiple_models[0].arena_max = s->arena_max;
        }
        if ((s32)s->anim_max > (s32)player_multiple_models[0].anim_max) {
            player_multiple_models[0].anim_max = s->anim_max;
        }
        if ((s32)s->sfx_max > (s32)player_multiple_models[0].sfx_max) {
            player_multiple_models[0].sfx_max = s->sfx_max;
        }
        if ((s32)s->sfx_buf_max > (s32)player_multiple_models[0].sfx_buf_max) {
            player_multiple_models[0].sfx_buf_max = s->sfx_buf_max;
        }
        if ((s32)s->sfx_arena_max > (s32)player_multiple_models[0].sfx_arena_max) {
            player_multiple_models[0].sfx_arena_max = s->sfx_arena_max;
        }
    }
    for (i = 1; i < 4; i++) {
        s = &player_multiple_models[i];
        s->model_max = player_multiple_models[0].model_max;
        s->model_buf_max = player_multiple_models[0].model_buf_max;
        s->arena_max = player_multiple_models[0].arena_max;
        s->anim_max = player_multiple_models[0].anim_max;
        s->sfx_max = player_multiple_models[0].sfx_max;
        s->sfx_buf_max = player_multiple_models[0].sfx_buf_max;
        s->sfx_arena_max = player_multiple_models[0].sfx_arena_max;
    }
    got_max_player_sizes = 1;
}

/* Player slot i's texture arena handle.                               */
void* PlayerModel(s32 i) {
    return player_multiple_models[i].arena;
}

/* ------------------------------------------------------------------ */
/* powerups                                                            */
/* ------------------------------------------------------------------ */

/*
 * GIANT (0x18F8) -- documented skeleton.  Per-frame powerup master:
 * walks all 11 slots ticking timers, runs the per-type processors --
 * levitation/anti-death node juggling (AtreeInit overlays via
 * lbl_80282930 class colors), reflect-shot, x-ray (do_see_thru),
 * growth/shrink, invisibility/invuln node alpha (MBTreeSetScale family),
 * mikey (PlayerProcessMikeyPUP), skin FX (PlayerProcessSkinFX), timed
 * expiry sounds (fn_8009D4F0/fn_8009D560/fn_8009D5A0) and the HUD
 * mini-inventory dirty flags (PUP_DIRTY).  On Xbox this decomposes
 * into PlayerProcessPowerups + PlayerProcessFamiliar + PlayerUsePowerup
 * + DropMikey + player_find_powerup_from_typemask (all inlined here).
 * Real body next session -- transcribe from Ghidra 0x8007CC48.
 */
void PlayerProcessPowerups(void* vp, s32 state, s32* c) {
    (void)vp;
    (void)state;
    (void)c;
}

/* Struct view over the familiar/halo atree state at Player+0x748.  A    */
/* typed member (displacement) read keeps &atree out of an address-CSE.  */
typedef struct PlayerSkinView {
    u8 pad0[0x748];
    /* 0x748 */ void* atree;   /* level-tier halo/familiar atree handle */
    u8 pad1[4];
    /* 0x750 */ u32 src_id;    /* source id, compared with atree word[1] */
} PlayerSkinView;

/* Rebuild a level-tier halo atree if its source changed; inlined per   */
/* tier by PlayerProcessSkinFX.  Returns 1 when a fresh tree was built.  */
static int PlayerSetupSkinTree(PlayerSkinView* p, void* atree, void* parent) {
    if (p->atree != NULL) {
        if (atree == NULL || p->src_id != ((u32*)atree)[1]) {
            AtreeDelete(&p->atree);
        }
    }
    if (p->atree == NULL && atree != NULL) {
        p->atree = (void*)AtreeInit(atree, &p->atree, 0, 0x800);
        MBTreeSetFlags(*(void**)p->atree, 0x10, 0);
        MBNodeSetParent(*(void**)p->atree, parent);
        MBTreeSetAlpha(*(void**)p->atree, 0, 1);
        return 1;
    }
    return 0;
}

/* Level-tier halo/glow overlay (30+/80+ levels, brighter at 99).       */
static void PlayerProcessSkinFX(void* vp) {
    Player* p = vp;
    PlayerSkinView* ps = vp;
    s32 fresh = 0;

    if (p->level >= 0x50) {
        fresh = PlayerSetupSkinTree(ps, FamiliarTree[p->index][1], p->node);
    } else if (p->level >= 0x1E) {
        fresh = PlayerSetupSkinTree(ps, FamiliarTree[p->index][0], p->node);
    } else {
        if (ps->atree != NULL) {
            AtreeDelete(&ps->atree);
        }
    }
    if (fresh != 0 && ps->atree != NULL) {
        if (p->level >= 99) {
            *(f32*)((u8*)ps->atree + 0x10) = 1.5 * *(f32*)(lbl_80282930[p->index] + 0x164);
            *(f32*)((u8*)ps->atree + 0x14) = 1.5 * *(f32*)(lbl_80282930[p->index] + 0x168);
            *(f32*)((u8*)ps->atree + 0x18) = 1.5 * *(f32*)(lbl_80282930[p->index] + 0x16C);
            *(f32*)(*(u8**)ps->atree + 0x30) = 1.5 * *(f32*)(lbl_80282930[p->index] + 0x164);
            *(f32*)(*(u8**)ps->atree + 0x34) = 1.5 * *(f32*)(lbl_80282930[p->index] + 0x168);
            *(f32*)(*(u8**)ps->atree + 0x38) = 1.5 * *(f32*)(lbl_80282930[p->index] + 0x16C);
        } else {
            *(f32*)((u8*)ps->atree + 0x10) = *(f32*)(lbl_80282930[p->index] + 0x164);
            *(f32*)((u8*)ps->atree + 0x14) = *(f32*)(lbl_80282930[p->index] + 0x168);
            *(f32*)((u8*)ps->atree + 0x18) = *(f32*)(lbl_80282930[p->index] + 0x16C);
            *(f32*)(*(u8**)ps->atree + 0x30) = *(f32*)(lbl_80282930[p->index] + 0x164);
            *(f32*)(*(u8**)ps->atree + 0x34) = *(f32*)(lbl_80282930[p->index] + 0x168);
            *(f32*)(*(u8**)ps->atree + 0x38) = *(f32*)(lbl_80282930[p->index] + 0x16C);
        }
    }
    if (ps->atree != NULL) {
        void* h = ps->atree;
        s32 a2 = 0;
        s32 a3 = 0;
        if (gGameMode == 0x4010 && (PF(p, 0x124, u32) & 0x80)) {
            MBTreeSetFlags(*(void**)h, 2, 0);
        } else {
            MBTreeClearFlags(*(void**)h, 2, 0);
            if (PF(p, 0x900, u32) & 0x10000000) {
                a2 = 1;
                a3 = 2;
            }
            AnimateATree(&ps->atree, a2, a3);
        }
    }
}

/* Mikey powerup: hatch/despawn state machine + orbit anim.            */
void PlayerProcessMikeyPUP(void* vp) {
    Player* p = vp;
    s16 t = PF(p, 0xA1C, s16);
    void* atree;
    s32 slot;
    s32 j;

    if (t != 2) {
        if (t < 2) {
            if (t == 0) {
                return;
            }
            if (t < 0) {
                return;
            }
            /* t == 1: hatch */
            slot = -1;
            for (j = 0; j < 11; j++) {
                if (PUP_TYPE(p, j) == 9 && PUP_SPECIALFLAGS(p, j) == 0x100000) {
                    slot = j;
                    break;
                }
            }
            if (slot < 0) {
                return;
            }
            if (PUP_DIRTY(p, slot) != 2) {
                return;
            }
            atree = AtreeMatch(sPowerupsBuf, "MIKEYPUP_ON", 1);
            PF(p, 0x96C, s32) = AtreeInit(atree, (u8*)p + 0x96C, 0, 0);
            PF(p, 0x9A4, s16) = 1;
            PF(p, 0xA14, void*) = MBNewNode(lbl_80344BD4, gIdentityMatrix, 1);
            PF(p, 0xA18, s32) = 0;
            MBNodeSetParent(*(void**)PF(p, 0x96C, s32*), PF(p, 0xA14, void*));
            *(f32*)(PF(p, 0xA14, u8*) + 0x30) = p->col_pos[0];
            *(f32*)(PF(p, 0xA14, u8*) + 0x34) = p->col_pos[1];
            *(f32*)(PF(p, 0xA14, u8*) + 0x38) = p->col_pos[2];
            CopyMat4((f32*)PF(p, 0xA14, void*), (f32*)((u8*)p + 0x9B4));
            PF(p, 0x9F4, s32) = *(s32*)(PF(p, 0xA14, u8*) + 0x30);
            PF(p, 0x9F8, s32) = *(s32*)(PF(p, 0xA14, u8*) + 0x34);
            PF(p, 0x9FC, s32) = *(s32*)(PF(p, 0xA14, u8*) + 0x38);
            PF(p, 0xA04, s32) = PF(p, 0x9F4, s32);
            PF(p, 0xA08, s32) = PF(p, 0x9F8, s32);
            PF(p, 0xA0C, s32) = PF(p, 0x9FC, s32);
            PF(p, 0xA1C, s16) = 2;
            PUP_DIRTY(p, slot) = 1;
            return;
        }
        if (t == 300) {
            /* despawn */
            slot = -1;
            for (j = 0; j < 11; j++) {
                if (PUP_TYPE(p, j) == 9 && PUP_SPECIALFLAGS(p, j) == 0x100000) {
                    slot = j;
                    break;
                }
            }
            AtreeDelete((void**)((u8*)p + 0x96C));
            MBRemoveNode(PF(p, 0xA14, void*), 1);
            PF(p, 0xA14, s32) = 0;
            PF(p, 0xA1C, s16) = 0;
            if (slot >= 0) {
                PUP_DIRTY(p, slot) = 1;
            }
            return;
        }
    }
    /* live: tick anim + sparkles */
    MBTreeClearFlags(*(void**)PF(p, 0x96C, s32*), 2, 0);
    AnimateATree((void**)((u8*)p + 0x96C), 0, 0);
    t = PF(p, 0xA1C, s16);
    if (t < 0x3C && t == (t / 10) * 10) {
        StartGemFX((f32*)((u8*)p + 0xA04), rand() % 4 + 1);
    }
    PF(p, 0xA1C, s16) = t + 1;
    slot = -1;
    for (j = 0; j < 11; j++) {
        if (PUP_TYPE(p, j) == 9 && PUP_SPECIALFLAGS(p, j) == 0x100000) {
            slot = j;
            break;
        }
    }
    if (slot >= 0 && PUP_DIRTY(p, slot) == 2) {
        PF(p, 0xA1C, s16) = 300;
    }
}

/* Drop the big-ape unlock powerups into the level.                    */
void AppendBigapePowerupsToScene(void) {
    u8 unused[16];
    s32 i;

    for (i = 0; i < lbl_80343DAC; i++) {
        if (InLevel((s32*)&lbl_8012028C[i]) != 0) {
            BigapePowerupInfo* info =
                &lbl_80120274[lbl_8012028C[i].type];
            AppendItemToLevel(lbl_8012028C[i].x, lbl_8012028C[i].y,
                              lbl_8012028C[i].z,
                              info->name, info->flags);
        }
    }
}

/* Instantiate a named pickup item at x/y/z via the item template.     */
void AppendItemToLevel(f32 x, f32 y, f32 z, char* name, u32 flags) {
    u8 unused[24];
    char* itemName = appended_item_template.name;
    s32* item;

    appended_item_template.type = 1;
    appended_item_template.count = 9;
    appended_item_template.active = 1;
    appended_item_template.pad0A = 0;
    appended_item_template.scale = 0.6f;
    appended_item_template.radius = 0.5f;
    appended_item_template.pos[0] = 0.0f;
    appended_item_template.pos[1] = 0.0f;
    appended_item_template.rot[1] = 0.0f;
    appended_item_template.rot[0] = 0.0f;
    appended_item_template.pos[2] = 0.0f;
    sprintf(itemName, name);
    *(s32*)&appended_item_template.name[16] = 0;
    appended_item_template.flags = flags;
    appended_item_template.field40 = 0;
    appended_item_template.field42 = -1;
    appended_item_template.field44 = 0;
    appended_item_template.field46 = 0x10;
    appended_item_template.field48 = 0;
    appended_item_template.field4A = 0x1E;
    appended_item_template.atree = AtreeMatch(sPowerupsBuf, itemName, 0);
    item = AddItem((s32*)&appended_item_template, NULL);
    *((u8*)item + 0xCD) = 0;
    MBTreeClearFlags((void*)item[0x19], 2, 0);
    if (*(s32*)item[0] == 1) {
        *(s16*)&item[0x3B] = 0x3C;
    }
    *(f32*)&item[0xD] = x;
    *(f32*)&item[0xE] = y;
    *(f32*)&item[0xF] = z;
    AddItemSub(item);
}

/*
 * X-ray/see-thru: pick the tree containing the closest visible chest
 * and swap the player onto the transparent variant; end_see_thru is
 * the state == -1 restore block (inlined, Xbox local).
 */
static s32 do_see_thru(void* vp) {
    Player* p = vp;
    s32 i = p->index;
    u8* chest = NULL;
    void* tree;
    s32* fl;
    s32 floor_id;
    s32 j;

    if (sItems == NULL) {
        return 0;
    }
    j = (s32)ClosestChest(p);
    if (j >= 0) {
        chest = sItems + j * 0xF0;
        if (!PointVisible(0.5f * *(f32*)(*(u8**)chest + 0xC), (s32*)(chest + 0x44))) {
            chest = NULL;
        }
    }
    if (chest == NULL) {
        lbl_8025EC78[i] = -1;
    } else {
        floor_id = *(s16*)(chest + 0xDC);
        fl = (s32*)(lbl_8028CAF4 + floor_id * 0x50);
        while (*fl == -1) {
            floor_id = *(s16*)((u8*)fl + RandItemIdx(j, fl[1], 0) * 2 + 8);
            fl = (s32*)(lbl_8028CAF4 + floor_id * 0x50);
        }
        tree = sDeathIconAtree;
        if (*fl != 4 && (*fl != 1 || fl[1] != 2 || *(s16*)(chest + 0xEC) < 2)) {
            tree = (void*)fl[0x13];
        } else if (*fl == 1) {
            tree = sKeyringAtree;
        }
        for (j = 0; j < 4; j++) {
            if (j != i && lbl_8025EC88[j] == chest) {
                tree = NULL;
            }
        }
        if (tree == NULL) {
            lbl_8025EC78[i] = -1;
        } else {
            s32 fresh = 0;
            if (lbl_8025EC88[i] != chest) {
                u8* old = lbl_8025EC88[i];
                if (old != NULL && *(s32*)(old + 100) != 0) {
                    MBNodeSetParent(*(void**)(old + 100), (void*)lbl_8025EC98[i]);
                    MBTreeSetAlpha(*(void**)(old + 100), 0, 1);
                    CopyMat3((f32*)lbl_8025ECA8[i], *(f32**)(old + 100));
                    *(s32*)(*(u8**)(old + 100) + 0x30) = *(s32*)(lbl_8025ECA8[i] + 0x30);
                    *(s32*)(*(u8**)(old + 100) + 0x34) = *(s32*)(lbl_8025ECA8[i] + 0x34);
                    *(s32*)(*(u8**)(old + 100) + 0x38) = *(s32*)(lbl_8025ECA8[i] + 0x38);
                }
                MBTreeSetAlpha(*(void**)(chest + 100), 0xC0, 1);
                lbl_8025EC98[i] = *(s32*)(*(u8**)(chest + 100) + 0x74);
                MBNodeSetParent(lbl_8025ECA8[i], (void*)lbl_8025EC98[i]);
                CopyMat3(*(f32**)(chest + 100), (f32*)lbl_8025ECA8[i]);
                *(s32*)(lbl_8025ECA8[i] + 0x30) = *(s32*)(*(u8**)(chest + 100) + 0x30);
                *(s32*)(lbl_8025ECA8[i] + 0x34) = *(s32*)(*(u8**)(chest + 100) + 0x34);
                *(s32*)(lbl_8025ECA8[i] + 0x38) = *(s32*)(*(u8**)(chest + 100) + 0x38);
                CopyMat4(gIdentityMatrix, *(f32**)(chest + 100));
                lbl_8025EC88[i] = chest;
                fresh = 1;
            }
            if ((fresh && lbl_8025ECB8[i][0] == NULL) || floor_id != lbl_8025EC78[i]) {
                lbl_8025EC78[i] = floor_id;
                if (lbl_8025ECB8[i][0] != NULL) {
                    AtreeDelete(&lbl_8025ECB8[i][0]);
                }
                lbl_8025ECB8[i][0] = (void*)AtreeInit(tree, &lbl_8025ECB8[i][0], 0, 0x80);
                MBTreeSetFlags(*(void**)lbl_8025ECB8[i][0], 8, 0);
                *(f32*)((u8*)*(void**)lbl_8025ECB8[i][0] + 0x40) = 1.001f;
                *(f32*)((u8*)*(void**)lbl_8025ECB8[i][0] + 0x44) = 1.001f;
                *(f32*)((u8*)*(void**)lbl_8025ECB8[i][0] + 0x48) = 1.001f;
                fresh = 1;
            }
            if (fresh) {
                MBNodeSetParent((void*)lbl_8025EC68[i], NULL);
                MBNodeSetParent(*(void**)(chest + 100), NULL);
                MBNodeSetParent(*(void**)lbl_8025ECB8[i][0], lbl_8025ECA8[i]);
                MBNodeSetParent((void*)lbl_8025EC68[i], lbl_8025ECA8[i]);
                MBNodeSetParent(*(void**)(chest + 100), lbl_8025ECA8[i]);
                AudioPlayerXray(i);
            }
            MBTreeClearFlags((void*)lbl_8025EC68[i], 1, 0);
        }
    }
    if (lbl_8025EC78[i] == -1) {
        /* end_see_thru (inlined) */
        if (lbl_8025ECB8[i][0] != NULL) {
            AtreeDelete(&lbl_8025ECB8[i][0]);
        }
        MBTreeSetFlags((void*)lbl_8025EC68[i], 1, 0);
        if (lbl_8025EC88[i] != NULL) {
            u8* old = lbl_8025EC88[i];
            if (*(s32*)(old + 100) != 0) {
                MBNodeSetParent(*(void**)(old + 100), (void*)lbl_8025EC98[i]);
                MBTreeSetAlpha(*(void**)(old + 100), 0, 1);
                CopyMat3((f32*)lbl_8025ECA8[i], *(f32**)(old + 100));
                *(s32*)(*(u8**)(old + 100) + 0x30) = *(s32*)(lbl_8025ECA8[i] + 0x30);
                *(s32*)(*(u8**)(old + 100) + 0x34) = *(s32*)(lbl_8025ECA8[i] + 0x34);
                *(s32*)(*(u8**)(old + 100) + 0x38) = *(s32*)(lbl_8025ECA8[i] + 0x38);
            }
        }
        lbl_8025EC88[i] = NULL;
    }
    return 0;
}

/* Index of the closest openable chest to p (squared/NR distance).     */
static f32 ClosestChest(void* vp) {
    Player* p = vp;
    u8* it;
    s32 j;
    s32 st;
    f32 dx, dy, dz;
    f32 d;
    f32 best = 250000.0f;

    StartEnemyGrid(p->pos, 60.0f);
    while ((j = NextGridEnemy()) >= 0) {
        it = sItems + j * 0xF0;
        if (*(s16*)(it + 0xC4) == -1) {
            continue;
        }
        if (*(s32*)*(u8**)it != 2) {
            continue;
        }
        if (*(u8*)(it + 0xCD) != 0 || *(u8*)(it + 0xC8) != 0) {
            continue;
        }
        st = *(s32*)(lbl_8028CAF4 + *(s16*)(it + 0xDC) * 0x50);
        if (st != -1 && st != 4 && st != 1) {
            continue;
        }
        dx = *(f32*)(it + 0x34) - p->pos[0];
        dy = *(f32*)(it + 0x38) - p->pos[1];
        dz = *(f32*)(it + 0x3C) - p->pos[2];
        d = dz * dz + dx * dx + dy * dy;
        if (d < best) {
            best = d;
        }
    }
    return best;
}

/* Query (and tick) the slot holding powerup type/mask.  Returns the   */
/* remaining state: 0 gone, 1 permanent, else the ticking timer.       */
typedef struct PlayerPowerupState {
    f32 timeleft;
    s32 type;
    f32 attributeadd;
    u32 specialflags;
} PlayerPowerupState;

typedef struct PlayerPowerupOverlay {
    u8 pad[0x130];
    PlayerPowerupState powerups[11];
} PlayerPowerupOverlay;

s32 player_get_powerup_state(f32 dt, void* vp, s32 type, u32 mask) {
    register Player* p = (Player*)vp;
    s32 j;
    s32 r = 0;

    for (j = 0; j < 11; j++) {
        u8* entry = (u8*)p + j * 0x10;

        if (*(f32*)(entry + 0x130) != 0.0) {
            if (*(s32*)(entry + 0x134) == type) {
                if (*(u32*)(entry + 0x13C) & mask) {
                    break;
                }
            }
        }
    }
    if (j < 11) {
        PlayerPowerupOverlay* overlay = (PlayerPowerupOverlay*)p;

        r = (s32)overlay->powerups[j].attributeadd;
        if ((s32)overlay->powerups[j].attributeadd < 0) {
            r = 1;
        } else if (sMusicTrackHi != 0xD) {
            if (dt < 0.0f) {
                overlay->powerups[j].attributeadd = 0.0f;
            } else {
                overlay->powerups[j].attributeadd -= dt;
            }
            if (overlay->powerups[j].attributeadd <= 0.0f) {
                overlay->powerups[j].timeleft = 0.0f;
            }
        }
    }
    return r;
}

/* Add/extend a powerup slot.  mask & 8 also drives the x-ray range.   */
void PlayerAddPowerup(f32 duration, f32 strength, void* vp, s32 type, u32 mask) {
    Player* p = vp;
    f32 best = 1000000.0f;
    f32 str;
    f32 w;
    s32 j;
    s32 pick = 0;

    str = strength * PF(lbl_80282930[p->index], 0x58, f32);
    for (j = 0; j < 11; j++) {
        if (PUP_TYPE(p, j) == type && (s32)PUP_SPECIALFLAGS(p, j) == (s32)mask) {
            if (duration > 0.0) {
                PUP_ATTRIBUTEADD(p, j) += duration;
            }
            if (PUP_TIMELEFT(p, j) >= 0.0f && str > 0.0) {
                PUP_TIMELEFT(p, j) += 0.25 * str;
            } else if (str < 0.0) {
                PUP_TIMELEFT(p, j) = str;
            }
            if (mask & 8) {
                lbl_80344B20 = PUP_TIMELEFT(p, j);
            }
            return;
        }
    }
    /* find the weakest/free slot */
    for (j = 0; j < 11; j++) {
        w = PUP_TIMELEFT(p, j);
        if (w < 0.0f) {
            w = (PUP_TYPE(p, j) == type) ? 999999.0f : 1000000.0f;
        }
        if (best == 2000000.0 || w == 0.0 || (w >= 0.0 && w < best)) {
            pick = j;
            best = w;
        }
        if (best == 0.0) {
            break;
        }
    }
    PUP_TIMELEFT(p, pick) = str;
    PUP_TYPE(p, pick) = type;
    PUP_ATTRIBUTEADD(p, pick) = duration;
    PUP_SPECIALFLAGS(p, pick) = mask;
    if (mask & 8) {
        lbl_80344B20 = str;
    }
    PUP_DIRTY(p, pick) = 1;
}

/* Attribute bump helpers (per-character bonus + norm recompute).      */
typedef struct PlayerAttributeBonus {
    f32 _pad0[2];
    f32 fight;
    f32 armor;
    f32 magic;
    f32 speed;
} PlayerAttributeBonus;

typedef struct PlayerAttributeOverlay {
    s32 index;
    s32 class_id;
    s32 char_type;
    s32 character;
    u8 _pad10[0xA90 - 0x10];
    PlayerAttributeBonus bonuses[16];
} PlayerAttributeOverlay;

void PlayerIncSpeed(void* vp, u32 amount) {
    PlayerAttributeOverlay* p = vp;

    p->bonuses[p->character].speed += (f32)(s32)amount;
    check_player_atts(p, p->character, NULL);
}

void PlayerIncMagic(void* vp, u32 amount) {
    PlayerAttributeOverlay* p = vp;

    p->bonuses[p->character].magic += (f32)(s32)amount;
    check_player_atts(p, p->character, NULL);
}

void PlayerIncArmor(void* vp, u32 amount) {
    PlayerAttributeOverlay* p = vp;

    p->bonuses[p->character].armor += (f32)(s32)amount;
    check_player_atts(p, p->character, NULL);
}

void PlayerIncFight(void* vp, u32 amount) {
    PlayerAttributeOverlay* p = vp;

    p->bonuses[p->character].fight += (f32)(s32)amount;
    check_player_atts(p, p->character, NULL);
}

/*
 * Recompute the four attribute norms: class base + 5/level (capped by
 * the class max) + per-character bonus, clamped to 999.  The repeated
 * min(cap, base + (level-1)*5) is the auto-inlined player_max_att.
 */
void check_player_atts(void* vp, s32 chartype, s32* stats) {
    Player* p = vp;
    u8* cls;
    f32 cap;
    f32 v;

    if (p->character == 2 && HIDDEN_CODE(p) == lbl_80343D6C) {
        ATT_FIGHT(p) = 0.9f;
        ATT_ARMOR(p) = 0.9f;
        ATT_MAGIC(p) = 0.9f;
        ATT_SPEED(p) = 0.9f;
        return;
    }
    if (stats == NULL) {
        stats = CHAR_STATS(p, chartype);
    }
    LoadPlyrData(p->index, chartype, NULL);
    cls = lbl_80282930[p->index];

    cap = PF(cls, 0x2C, f32);
    v = PF(cls, 0x28, f32) + (f32)((p->level - 1) * 5);
    if (v < cap) {
        cap = v;
    }
    ATT_FIGHT(p) = (cap + stats[2] < 999.0) ? cap + stats[2] : 999.0;

    cap = PF(cls, 0x3C, f32);
    v = PF(cls, 0x38, f32) + (f32)((p->level - 1) * 5);
    if (v < cap) {
        cap = v;
    }
    ATT_ARMOR(p) = (cap + stats[3] < 999.0) ? cap + stats[3] : 999.0;

    cap = PF(cls, 0x44, f32);
    v = PF(cls, 0x40, f32) + (f32)((p->level - 1) * 5);
    if (v < cap) {
        cap = v;
    }
    ATT_MAGIC(p) = (cap + stats[4] < 999.0) ? cap + stats[4] : 999.0;

    cap = PF(cls, 0x34, f32);
    v = PF(cls, 0x30, f32) + (f32)((p->level - 1) * 5);
    if (v < cap) {
        cap = v;
    }
    ATT_SPEED(p) = (cap + stats[5] < 999.0) ? cap + stats[5] : 999.0;
}

/* Show (on != 0) or hide every per-player HUD blit.                   */
void SetPlayerWindows(s32 on) {
    s32 i;
    s32 j;
    s32 blit_alpha = (on != 0) ? 0xFF : 0;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 6; j++) {
            if (frame_blit[i][j] != NULL) {
                MBBlitSetAlpha(frame_blit[i][j], blit_alpha);
            }
        }
        for (j = 0; j < 12; j++) {
            if (rune_blit[i][j] != NULL) {
                MBBlitSetAlpha(rune_blit[i][j], blit_alpha);
            }
        }
        for (j = 0; j < 8; j++) {
            if (crystal_blit[i][j] != NULL) {
                MBBlitSetAlpha(crystal_blit[i][j], blit_alpha);
            }
        }
        for (j = 0; j < 4; j++) {
            if (key_blit[i][j] != NULL) {
                MBBlitSetAlpha(key_blit[i][j], blit_alpha);
            }
        }
        if (rune13_blit[i] != NULL) {
            MBBlitSetAlpha(rune13_blit[i], blit_alpha);
        }
        if (hod_blit[i] != NULL) {
            MBBlitSetAlpha(hod_blit[i], blit_alpha);
        }
        if (quest_blit[i] != NULL) {
            MBBlitSetAlpha(quest_blit[i], blit_alpha);
        }
        for (j = 0; j < 7; j++) {
            MBBlitSetAlpha(pm_blit[i][j], blit_alpha);
        }
    }
    alpha = on;
}

/* ------------------------------------------------------------------ */
/* got-it ticker                                                       */
/* ------------------------------------------------------------------ */

/* Per-frame got-it slide/hold/fade state machine + blit creation.     */
static void do_got_it_8007FC80(void) {
    char buf[36];
    GotIt* g;
    s32 y[3];
    u32 x;
    s32 i;

    for (i = 0, g = got_it; i < 24; i++, g++) {
        if (g->state == 2) {
            /* sliding up into place */
            if (g->blit1 != NULL) {
                mbBlitCalcRect(g->blit1, NULL, &y[0], NULL);
                y[0] -= gFrameTicks;
                if (y[0] < 0x131) {
                    y[0] = 0x130;
                    g->state++;
                    g->timer = 0x5A;
                }
                mbBlitCalcY(g->blit1, y[0]);
                mbBlitCalcY(g->blit2, y[0] + 0x10);
            }
        } else if (g->state == 4) {
            /* sliding down out */
            if (g->blit1 != NULL) {
                mbBlitCalcRect(g->blit1, NULL, &y[0], NULL);
                y[0] += gFrameTicks;
                if (y[0] > 399) {
                    g->state = -1;
                }
                mbBlitCalcY(g->blit1, y[0]);
                mbBlitCalcY(g->blit2, y[0] + 0x10);
            }
        } else if (g->state == 3) {
            /* holding */
            g->timer -= gFrameTicks;
            if (g->timer < 1) {
                g->state++;
            }
        } else if (g->state < 0) {
            if (g->state > -2) {
                g->state = 0;
                if (g->blit1 != NULL) {
                    MBRemoveBlit(g->blit1);
                    g->blit1 = NULL;
                    MBRemoveBlit(g->blit2);
                    g->blit2 = NULL;
                }
            }
        } else if (g->state == 1) {
            /* create the pair */
            x = lbl_80120238[g->player];
            sprintf(buf, "%d", g->count);
            switch (g->type) {
            case 1:
                g->blit1 = MBNewBlit(buf, x, 0);
                if (sMusicTrackHi == 0xC) {
                    g->blit2 = MBNewBlit("COINHUD", x, 0);
                } else if (g->count < 0xB) {
                    g->blit2 = MBNewBlit("KEY", x, 0);
                } else {
                    g->blit2 = MBNewBlit("KEYS", x, 0);
                }
                break;
            case 2:
                g->blit1 = MBNewBlit(buf, x, 0);
                g->blit2 = MBNewBlit((g->count < 2) ? "KEY" : "KEY_RING", x, 0);
                break;
            case 3:
                g->blit1 = MBNewBlit(buf, x, 0);
                if (g->count >= 100) {
                    g->blit2 = MBNewBlit("MEAT", x, 0);
                } else if (g->count < -99) {
                    g->blit2 = MBNewBlit("BADMEAT", x, 0);
                } else if (g->count < 0) {
                    g->blit2 = MBNewBlit("BADFRUIT", x, 0);
                } else {
                    g->blit2 = MBNewBlit("FRUIT", x, 0);
                }
                break;
            case 4:
                g->blit1 = MBNewBlit(buf, x, 0);
                g->blit2 = MBNewBlit("MAGIC", x, 0);
                break;
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                g->blit1 = MBNewBlit(buf, x, 0);
                g->blit2 = MBNewBlit("SPECIALS", x, 0);
                break;
            case 10:
                g->blit1 = MBNewBlit(buf, x, 0);
                g->blit2 = MBNewBlit("RUNESTONE", x, 0);
                break;
            case 0xD:
                g->blit1 = MBNewBlit(buf, x, 0);
                g->blit2 = MBNewBlit("LEGEND", x, 0);
                break;
            case 0xF:
                g->blit1 = MBNewBlit(buf, x, 0);
                g->blit2 = MBNewBlit("CRYSTAL", x, 0);
                break;
            case 0x10:
                g->blit1 = MBNewBlit(buf, x, 0);
                g->blit2 = MBNewBlit("GOLDNICON", x, 0);
                break;
            default:
                g->state = 0;
                return;
            }
            if (g->blit1 != NULL) {
                mbBlitProject(g->blit1, 0x80, 0);
                mbBlitCalcWidth(g->blit1, x, 0x180, 0.15f);
            }
            if (g->blit2 != NULL) {
                mbBlitProject(g->blit2, 0x80, 0);
                mbBlitCalcWidth(g->blit2, x, 400, 0.16f);
            }
            g->state++;
        }
    }
}

/* Free every got-it entry belonging to player i.                      */
void kill_got_it(s32 i) {
    s32 j;

    for (j = 0; j < 24; j++) {
        if (i == got_it[j].player) {
            got_it[j].state = 0;
            if (got_it[j].blit1 != NULL) {
                MBRemoveBlit(got_it[j].blit1);
                got_it[j].blit1 = NULL;
            }
            if (got_it[j].blit2 != NULL) {
                MBRemoveBlit(got_it[j].blit2);
                got_it[j].blit2 = NULL;
            }
        }
    }
}

/* Queue a got-it ticker entry (accepted types only).                  */
void add_got_it(s32 player, s32 type, s32 count) {
    GotIt* g = got_it;
    s32 j;

    for (j = 0; j < 24; j++, g++) {
        if (g->state == 0) {
            break;
        }
    }
    if (j >= 24) {
        return;
    }
    if (type != 0xD) {
        if (type < 0xD) {
            if (type >= 0xB) {
                return;
            }
            if (type < 1) {
                return;
            }
        } else {
            if (type >= 0x11) {
                return;
            }
            if (type < 0xF) {
                return;
            }
        }
    }
    g->state = 1;
    g->player = player;
    g->type = type;
    g->count = count;
    g->timer = 0;
}

/* Reset the got-it table (level load).                                */
void init_got_it(void) {
    GotIt* g = got_it;
    s32 j;

    for (j = 0; j < 24; j++, g++) {
        g->state = 0;
        if (g->blit2 != NULL) {
            MBRemoveBlit(g->blit2);
            g->blit2 = NULL;
        }
        if (g->blit1 != NULL) {
            MBRemoveBlit(g->blit1);
            g->blit1 = NULL;
        }
    }
}

/* ------------------------------------------------------------------ */
/* world matrix / mini-inventory                                       */
/* ------------------------------------------------------------------ */

/* Recompute the world matrix unless attached; anchor when asked.      */
void UpdatePlayerWorldMat(void* vp, s32 anchor) {
    Player* p = vp;

    if ((p->hud_flags & 0x20) == 0) {
        UpdateObjWorldMat(p->mat);
        if (anchor != 0) {
            fn_8005A404(p->mat, p->anchor_fwd, p->anchor_pos);
        }
    }
}

/* Pad-driven mini-inventory: cycle selection, use, open/close.        */
void mini_inventory_update(s32 i) {
    Player* p = P(i);
    TbInfo* tb = &tb_info[i];
    u32 held;
    s32 moved = 0;
    s32 sel;
    s32 j;

    if (gGameMode != 0x4010 || gGameBusy != 0) {
        return;
    }
    if (tb->state == 1) {
        if (PUP_TIMELEFT(p, tb->sel) == 0.0 ||
            (lbl_80240E38[i * 0xF] & 0x10000000)) {
            AudioCursorH();
            moved = 1;
            sel = mini_inventory_find_previous_selectable_item(i);
            if (sel == -1) {
                tb->state = 3;
                tb->slide = 0;
            } else {
                tb->sel = sel;
            }
        }
        held = lbl_80240E38[i * 0xF];
        if (held & 0x20000000) {
            AudioCursorH();
            moved = 1;
            sel = mini_inventory_find_next_selectable_item(i);
            if (sel == -1) {
                tb->sel = -1;
                tb->state = 3;
                tb->slide = 0;
            } else {
                tb->sel = sel;
            }
        }
        if (held & 0x40000000) {
            AudioCursorSelect();
            if (PUP_DIRTY(p, tb->sel) == 2) {
                PUP_DIRTY(p, tb->sel) = 3;
            } else {
                PUP_DIRTY(p, tb->sel) = 2;
            }
        }
        if ((held & 0x80000000) && tb->state == 1) {
            AudioCursorV();
            tb->state = 3;
            moved = 1;
            tb->slide = 0;
        }
    } else if ((lbl_80240E38[i * 0xF] & 0x40000000) != 0) {
        AudioCursorV();
        if (tb->state == 0) {
            if (tb->sel == -1 && (sel = mini_inventory_find_previous_selectable_item(i)) >= 0) {
                tb->sel = sel;
                for (j = 0; j < lbl_80343D68; j++) {
                    tb->label = (char*)lbl_8011FCE8[j * 3 + 2];
                    if (lbl_8011FCE8[j * 3 + 1] ==
                            (lbl_8011FCE8[j * 3 + 1] & (s32)PUP_SPECIALFLAGS(p, tb->sel)) &&
                        PUP_TYPE(p, tb->sel) == lbl_8011FCE8[j * 3]) {
                        break;
                    }
                }
                if (j >= lbl_80343D68) {
                    tb->label = (char*)lbl_8011FCE8[(lbl_80343D68 - 1) * 3 + 2];
                }
            }
            if (tb->sel >= 0) {
                tb->state = 2;
                moved = 1;
                tb->slide = 0;
            }
        }
    }
    if (tb->state == 2) {
        if (tb->slide < 0x80) {
            tb->slide += gFrameTicks * 4;
            if (tb->slide > 0x80) {
                tb->slide = 0x80;
            }
        } else {
            tb->state = 1;
        }
    } else if (tb->state == 1) {
        mini_inventory_draw_label(i);
        if (moved) {
            for (j = 0; j < lbl_80343D68; j++) {
                tb->label = (char*)lbl_8011FCE8[j * 3 + 2];
                if (lbl_8011FCE8[j * 3 + 1] ==
                        (lbl_8011FCE8[j * 3 + 1] & (s32)PUP_SPECIALFLAGS(p, tb->sel)) &&
                    PUP_TYPE(p, tb->sel) == lbl_8011FCE8[j * 3]) {
                    break;
                }
            }
            if (j >= lbl_80343D68) {
                tb->label = (char*)lbl_8011FCE8[(lbl_80343D68 - 1) * 3 + 2];
            }
        }
    } else if (tb->state == 3) {
        if (tb->slide < 0x80) {
            tb->slide += gFrameTicks * 4;
            if (tb->slide > 0x80) {
                tb->slide = 0x80;
            }
        } else {
            tb->state = 0;
        }
    }
}

/* Draw the selected-powerup label + remaining-time bar.               */
void mini_inventory_draw_label(s32 i) {
    char* label;
    TbInfo* tb;
    u8 st;
    s32 x;
    s32 y;

    tb = (TbInfo*) ((u8*) potionicon_tab + i * 40 + 2368);
    label = *(char**) ((u8*) potionicon_tab + i * 40 + 2404);
    if (label == NULL) {
        return;
    }
    st = *(u8*) ((u8*) potionicon_tab + i * 13148 + tb->sel + 3616);
    y = *(s32*) ((u8*) potionicon_tab + i * 40 + 2388) - 25;
    y += 128 - tb->slide;
    x = *(s32*) ((u8*) potionicon_tab + i * 40 + 2380) + 12;
    switch (st) {
    case 1:
    case 3:
        DrawTextKeepScale(0.09f, x, y, 6, 0xFFFFFF, label);
        break;
    case 2:
        DrawGlowText(0.09f, x, y, label);
        break;
    }
}

/* Previous slot (wrapping) holding a live powerup, or -1.             */
s32 mini_inventory_find_previous_selectable_item(s32 i) {
    s32 current;
    s32 sel;
    Player* p;
    s32 n;

    current = tb_info[i].sel;
    p = P(i);
    sel = current;
    n = 0;
    if (current < 0) {
        goto wrap_initial;
    }
    sel--;
wrap_initial:
    if (sel == -1) {
        sel = 9;
    }
    goto check_slot;

previous_slot:
    sel--;
    n++;
    if (sel == -1) {
        sel = 9;
    }
    if (n >= 10) {
        return -1;
    }
check_slot:
    if ((f64)p->powerup[sel].timeleft == 0.0) {
        goto previous_slot;
    }
    return sel;
}

/* Next slot (wrapping) holding a live powerup, or -1.                 */
s32 mini_inventory_find_next_selectable_item(s32 i) {
    Player* p;
    s32 n;
    s32 sel;

    sel = tb_info[i].sel;
    p = P(i);
    sel++;
    n = 0;
    if (sel < 10) {
        goto check_slot;
    }
    sel = 0;
    goto check_slot;

next_slot:
    sel++;
    n++;
    if (sel < 10) {
        goto count_check;
    }
    sel = 0;
count_check:
    if (n >= 10) {
        return -1;
    }
check_slot:
    if ((f64)p->powerup[sel].timeleft == 0.0) {
        goto next_slot;
    }
    return sel;
}

/* Load the INVENTORY overlay file once.                               */
void mini_inventory_setup(void) {
    s32 f;

    lbl_80344B28 = 0;
    f = (s32)MBOX_LoadModelFixed("INVENTORY", 0, 0, NULL, 0xFFFFFFFF);
    lbl_80344B28 = f;
    ShopLoadData(f);
}
