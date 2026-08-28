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
#include "game/effect.h"

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
extern Effect Effects[];
extern const f64 lbl_80347880;

/* enemies/world debug */
extern s32 lbl_802575B0;  /* debug HUD mode (1 = pos, 2 = XP) */
extern s32 lbl_802575BC;

/* controls (pad state, stride 0xF words per player) */
extern u32 lbl_80240E34[];
extern u32 lbl_80240E38[];
extern f32 lbl_80240E50[];

/* game/world state (.sbss/.sdata) */
extern s32 gGameMode;   /* game state */
extern s32 gGameOptions[];
extern s32 options_state;
extern s32 lbl_80344298;
extern s32 lbl_80344824;   /* active-player mask */
extern s32 lbl_80344760;
extern u32 sFlags;   /* sFlags: pause/movie */
extern u64 gControllerButtons;
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
extern s32 lbl_80344808;
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
extern s32 gBossActive;
extern s32 gBossDead;
extern s32 lbl_803444E4;
extern s32 lbl_803444F4;
extern s32 lbl_803444F8;
extern s32 lbl_803444FC;
extern s32 lbl_80344500;
extern f32 lbl_80344504;
extern s32 lbl_80344508;
extern s32 lbl_8034450C;
extern s32 lbl_80344510;
extern s32 dbgTextFlagA;
extern s32 lbl_80344E44;   /* HUD button texture ids */
extern s32 lbl_80344E48;
extern f32 lbl_80343D70;
extern f32 lbl_80343D74;
extern f32 lbl_80343D78;
extern f32 lbl_801201F8[];

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
extern s32 fn_800C0ADC(f32* pos, f32* color, f32 radius, f32 intensity);

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
extern void AudioPotion(u32 color, f32* pos, s32 heal);
extern void update_player_milestone(Player* player);
extern void fn_8005ACE0(f32* pos);
extern void fn_8005DE50(void* p, s32* req);
extern void update_class_spec(s32 player);

/* sfx */
extern s32 StartShieldFX(f32* pos, s32 type, s32 player, f32 dmg, f32 size);
extern s32 StartMagicFX(f32* pos, s32 type, s32 player, f32 power, f32 scale);
extern s32 StartThrowMagicFX(f32* pos, f32* vel, s32 type, s32 player, s32 snd,
                            f32 weight, f32 dmg, f32 size);
extern s32 StartLevelUpFX(s32 arg0, s32 classId);
extern s32 StartMagicHealFX(f32* pos, f32 scale);
extern s32 PlaceEffectOnFloor(s32 fx, f32* pos);
extern void SfxSetParent(s32 fx, void* node);

/* this TU, back slice (0x80077BF0..0x8008091C), forward decls */
void PlayerProcessScale(void* p);
static void do_exit(void* p, s32 dest);
static s32 do_weakening(Player* p, s32 active);
static s32 all_players_go_to_same_level(void);
void inactivate_player(s32 player);
void abort_player(s32 player);
s32 activate_player(s32 player);
void PlayerProcessPowerups(void* p);
static void PlayerProcessSkinFX(void* p);
void check_player_atts(void* p, s32 chartype, f32* stats);
static void do_got_it_8007FC80(void);
void mini_inventory_update(s32 player);
s32 heal_player(Player* p, f32 amount);
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
static void do_see_thru(void* p);
static s32 ClosestChest(void* p);
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
s32 damage_player(s32 i, f32 dmg, s32 mode, u32 flags, f32* dir);
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
extern void PlayerMotion(void* p);
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
    s32 i;
    s32 cnt;
    s32 total;
    s32 type;
    void* tex;

    if (welcome_timer > 0) {
        return;
    }
    if (options_state != 0) {
        return;
    }
    switch (gGameMode) {
    case 0x4010:
        break;
    default:
        return;
    }

    if ((f64)p->got_timer > 0.0) {
        i = p->index;
        p->got_timer = p->got_timer - gClockFrameStep;
        type = p->got_type;
        if (type >= 0x200) {
            cnt = p->got_count;
            total = sVisibleSumCoinCount;
            if (type - 0x200 < 0x10) {
                sprintf(tbuf, "16_%sCOIN", &lbl_801200B0[(type - 0x200) * 4]);
            } else {
                sprintf(tbuf, "16_SUM");
            }
        } else if (type >= 0x100) {
            cnt = towerGetLevelRecord(i, type -= 0x100);
            total = lbl_80124CDC[type];
            sprintf(tbuf, "SM_%s", &lbl_80124CE8[type * 0xE]);
        } else {
            cnt = towerBossStatus(i, type);
            total = lbl_80124C70[type];
            sprintf(tbuf, "SM_CRYSTAL_%s", &lbl_80124C94[type * 8]);
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
#pragma opt_propagation off
void ShowRuneStones(void) {
    s32 i;
    s32 j;
    void* blit;
    u8* p;
    s32 state;
    s32 hide;
    s32 result;
    s32 result2;
    u8 _spare[8];

    if (welcome_timer > 0 && options_state == 0) {
        if ((welcome_timer -= gFrameTicks) < 0) {
            welcome_timer = 0;
        }
        for (i = 0; i < 4; i++) {
            s16 hud_flags2;

            p = (u8*)potionicon_tab + i * PREC_STRIDE;
            hud_flags2 = *(s16*)(p + 5542);
            state = *(s32*)(p + 3368);
            p += 3136;
            if (!(hud_flags2 & 2)) {
                ((Player*)p)->hud_flags2 = hud_flags2 | 2;
                if ((u32)(state - 1) <= 1 || (u32)(state - 4) <= 1) {
                    for (j = 0; j < 8; j++) {
                        if ((blit = crystal_blit[i][j]) != NULL) {
                            if ((((Player*)p)->char_save[((Player*)p)->character].rune_stones &
                                 (1 << j)) != 0) {
                                hide = 0;
                            } else {
                                hide = 1;
                            }
                            result2 = hide;
                            mbBlitInit3414(blit, result2);
                        }
                    }
                } else {
                    for (j = 0; j < 8; j++) {
                        if ((blit = crystal_blit[i][j]) != NULL) {
                            mbBlitInit3414(blit, 1);
                        }
                    }
                }
                if (!(gGameMode & 0x8000) && (state == 1 || state == 5)) {
                    for (j = 0; j < 12; j++) {
                        if ((blit = rune_blit[i][j]) != NULL) {
                            if ((((Player*)p)->shards & (1 << j)) != 0) {
                                hide = 0;
                            } else {
                                hide = 1;
                            }
                            result = hide;
                            mbBlitInit3414(blit, result);
                        }
                    }
                }
            }
        }
    } else {
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 8; j++) {
                if ((blit = crystal_blit[i][j]) != NULL) {
                    mbBlitInit3414(blit, 1);
                }
            }
        }
    }
}
#pragma opt_propagation reset

/* Name/level/health/keys/potions writer for one player's HUD row. */
static void write_health_and_items(s32 i) {
    Player* p = P(i);
    u16* left = lbl_80120238;
    u16* right = lbl_80120240;
    u32* colors = lbl_801201C8;
    s32 hidden;
    s32 show_gold;
    s32 j;
    u32 rgb;
    f32 oldz;
    char buf2[44];
    char buf[16];
    u8 unused[32];
    void* blit;

    hidden = 0;
    show_gold = 1;
    rgb = colors[p->class_id];
    mini_inventory_update(i);
    oldz = MBSetFontZ(63990.0f);
    if (lbl_80344A28 != 0 || gGameOptions[8] != 0) {
        hidden = 1;
    }
    if (gGameMode == 0x4010 && lbl_80344760 > 0 && p->state == 0xB &&
        p->motion_state == 1) {
        u32 x = left[i] + 6;

        hidden = 1;
        MBNewTempBlit((void*)lbl_80344E48, x, 0x14C, 0xE, 0xE);
        MBNewTempBlit((void*)lbl_80344E44, x, 0x160, 0xE, 0xE);
        DrawTextKeepScale(1.2f, x + 0xE, 0x150, 1, 0xFFFFFF, "Wait In Tower");
        DrawTextKeepScale(1.2f, x + 0xE, 0x164, 1, 0xFFFFFF, "Quit Game");
        show_gold = 0;
    }
    if ((p->state == 5 || p->state == 0xB) && gGameMode != 0x400D &&
        gGameMode != 0x4013 && gGameMode != 0x4017 && !hidden) {
        hidden = 1;
        setup_player_display(i);
        if (p->state == 0xB) {
            DrawTextKeepScale(1.2f, -right[i], 0x154, 1, rgb, "IN TOWER");
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
            DrawText((left[i] + 0x74) - w, 0x167, 4, colors[p->class_id], buf);
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
    switch (p->display_mode) {
    case 6:
        if (!hidden) {
            DrawTextKeepScale(0.667f, -right[i], 0x153, 7, rgb, p->name);
        }
        break;
    case 1:
    case 2:
        if (gGameOptions[8] != 0 && p->state == 1) {
            hidden = 1;
            if (gGameOptions[8] == 1) {
                debug_player_pos(i);
            } else {
                DrawText(left[i] + 8, 0x154, 1, 0xFFFFFF, "XP: %d",
                         p->exp);
            }
        }
        /* fall through */
    case 5:
        if (!hidden) {
            DrawTextKeepScale(0.667f, -right[i], 0x153, 7, rgb, p->name);
        }
        if (lbl_80344A28 != 0 || !hidden) {
            sprintf(buf2, "LV %d", p->level);
            DrawText(-right[i], 0x146, 1, 0xFFFFFF, buf2);
        }
        break;
    case 10:
        break;
    }
    if (p->state != 2 && p->display_mode != 0 && alpha == 0) {
        if (p->item_body_lo > 0) {
            blit = MBNewTempBlit((void*)key_blit_idx, left[i] + 8, 0x143, -1, -1);
            mbBlitCvtCoord(blit, 64000.0f);
            sprintf(buf2, "%d", p->item_body_lo);
            DrawTextKeepScale(0.8f, left[i] + 0x1A, 0x147, 4, rgb, buf2);
        }
        if (p->item_body_hi > 0) {
            blit = MBNewTempBlit(potionicon_tab[PF(p, 0x32FC + p->item_body_hi * 4, s32)],
                                 left[i] + 0x66, 0x143, -1, -1);
            mbBlitCvtCoord(blit, 64000.0f);
            sprintf(buf2, "%d", p->item_body_hi);
            DrawTextKeepScale(0.8f, left[i] + 0x5C, 0x147, 4, rgb, buf2);
        }
    }
    if (p->health > 0.0f && lbl_80344A44 == 0) {
        if (gGameMode == 0x4010 && p->quest_state != 0 && sMusicTrackHi != 0xD) {
            mbBlitInit3414(quest_blit[i], 0);
        } else {
            mbBlitInit3414(quest_blit[i], 1);
        }
        if ((u32)(gGameMode - 0x400F) <= 1 && (p->shards & 0x1000)) {
            mbBlitInit3414(hod_blit[i], 0);
        } else {
            mbBlitInit3414(hod_blit[i], 1);
        }
        if (gGameMode == 0x4010) {
            draw_power_meter(i);
        }
    } else {
        for (j = 0; j < 7; j++) {
            mbBlitInit3414(pm_blit[i][j], 1);
        }
    }
    MBSetFontZ(oldz);
}

/* Debug HUD: floor name, position, facing. */
extern u8 lbl_80113AE0[];
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
#pragma opt_common_subs off
#pragma opt_lifetimes off
static void debug_player_pos(s32 i) {
    Player* p;
    struct {
        u8 head[4];
        f32 screen[2];
        u8 gap[8];
        volatile f32 y;
        f32 actualX;
        u8 tail[16];
    } work;
    u16* x;
    char* fmt;
    u8* base;
    char* name;
    char* floor;
    f32 magnitude;
    s32 oldflags;

    fmt = (char*)lbl_80113AE0;
    base = (u8*)potionicon_tab;
    p = (Player*)((u8*)(p = (Player*)(base + i * 0x335C)) + 0xC40);
    name = fmt + 908;
    if (gGameMode == 0x4010) {
        fn_800C02F4(0x80FF80);
        get_actual_screen_pos(0, &work.actualX, (f32*)&work.y, p->col_pos);
        dbgTextFlagA = 1;
        floor = p->floor_name;
        if (floor != NULL &&
            (magnitude = lbl_80240E30[i].leftMagnitude)) {
            name = floor;
        } else if (p->floor_name2 != NULL) {
            name = p->floor_name2;
        }
        oldflags = MBSetFontFlags(0x40000);
        work.y = 330.0f;
        x = &lbl_80120238[i];
        DrawText(*x + 8, (s32)work.y, 1, 0xFFFFFF, name);
        work.y += 10.0f;
        sprintf((char*)base + 0x4F4, fmt + 920,
                p->pos[0], p->pos[1], p->pos[2]);
        DrawText(*x + 8, (s32)work.y, 1, 0xFFFFFF, (char*)base + 0x4F4);
        work.y += 10.0f;
        MBWorldToScreen(work.screen, p->pos);
        sprintf((char*)base + 0x4F4, fmt + 940,
                work.screen[0], work.screen[1]);
        DrawText(*x + 8, (s32)work.y, 1, 0xFFFFFF, (char*)base + 0x4F4);
        MBSetFontFlags(oldflags);
    }
}
#pragma opt_lifetimes reset
#pragma opt_common_subs reset

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
    s32 zone0;
    s32 zone;
    f32 pw;
    f32 frac;
    s32 state;
    u32 rgb;
    u32 rgb2;
    u16* tex;
    s32 w;
    u8 unused[0x30];

    for (j = 0; j < 7; j++) {
        mbBlitInit3414(pm_blit[i][j], 1);
    }
    pw = (f32)(0.01 * p->power_level);
    if (pw < 0.4) {
        zone0 = 1;
    } else if (pw < 0.99) {
        zone0 = 2;
    } else {
        zone0 = 3;
    }
    if ((f64)p->power_target > 100.0) {
        p->power_target = 100.0f;
    }
    if (p->power_level < p->power_target) {
        p->power_level = p->power_level + (f32)(u32)gFrameTicks;
        if (p->power_level > p->power_target) {
            p->power_level = p->power_target;
        }
    } else {
        p->power_level = p->power_level - (f32)((u32)gFrameTicks << 1);
        if (p->power_level < p->power_target) {
            p->power_level = p->power_target;
        }
    }
    frac = (f32)(0.01 * p->power_level);
    if (frac < 0.4) {
        zone = 1;
        frac = frac / 0.4f;
    } else if (frac < 0.99) {
        zone = 2;
        frac = (frac - 0.4f) / 0.6f;
    } else {
        zone = 3;
        frac = 1.0f;
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
    switch (state) {
    case 0: {
        s32 a = (s32)(127.0 * frac + 128.0);

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
        w = (s32)((f32)(s32)tex[5] * frac) >> 1;
        if (w < 1) {
            w = 1;
        }
        mbBlitCalcWidth(pm_blit[i][0],
                        (pm_bar_x + i * 0x80 + ((s32)tex[5] / 2)) - w, pm_bar_y,
                        (f32)pm_bar_z);
        mbBlitProject(pm_blit[i][0], w << 1, 0);
        mbBlitCalcWidth(pm_blit[i][1], pm_frame_x + i * 0x80, pm_frame_y,
                        (f32)pm_frame_z);
        MBBlitSetColor(pm_blit[i][0], rgb);
        MBBlitSetColor(pm_blit[i][1], rgb2);
        break;
    }
    case 1:
        j = p->meter_timer >> 2;
        if (j >= 5 && j < 10) {
            j = 4 - (j - 5);
        }
        if (j < 5) {
            mbBlitInit3414(pm_blit[i][6], 0);
            mbInitBlitEntry(pm_blit[i][6], pm_frames, j);
        } else {
            p->meter_flash = 0;
        }
        break;
    case 2:
        j = (p->meter_timer << 9) / 0x78;
        if (j > 0xFF && j <= 0x1FF) {
            j = 0x1FF - j;
        }
        if (j <= 0xFF) {
            mbBlitInit3414(pm_blit[i][3], 0);
            MBBlitSetAlpha(pm_blit[i][3], 0xFF - j);
        } else {
            p->meter_timer = 0;
        }
        MBBlitSetColor(pm_blit[i][0], 0xFF0000);
        MBBlitSetColor(pm_blit[i][1], 0xFF0000);
        break;
    }
    p->meter_timer = p->meter_timer + gFrameTicks;
}

/* Rebuild the 6 portrait-frame blits for a player's display mode. */
void setup_player_display(s32 i) {
    Player* players = gPlayerRecords;
    Player* p = &players[i];
    u16 x = lbl_80120238[i];
    s32 mode;
    s32 cls;
    s32 chr;
    u32 frames;
    char buf[40];
    u8 unused[8];

    mode = get_display_mode(i);
    cls = p->class_id;
    del_player_blits(i);
    chr = p->character;
    p->display_mode = mode;
    frames = (u32)MBOX_FindTexture_Err("S3", NULL, 1);
    mbInitBlitEntry(frame_blit[i][0], frames, 0);
    mbBlitInit3414(frame_blit[i][0], 0);
    frames = (u32)MBOX_FindTexture_Err("S4", NULL, 1);
    mbInitBlitEntry(frame_blit[i][1], frames, 0);
    mbBlitInit3414(frame_blit[i][1], 0);
    if (p->state == 0) {
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
    switch (mode) {
    case 0:
    case 3:
        mbBlitInit3414(frame_blit[i][4], 1);
        mbBlitInit3414(frame_blit[i][5], 1);
        goto done;
    case 6:
        chr = p->respawn_char;
        /* fallthrough */
    case 1:
    case 2:
    case 5:
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
                s32 hide;

                if (p->shards & (1 << j)) {
                    hide = 0;
                } else {
                    hide = 1;
                }
                mbBlitInit3414(rune_blit[i][j], hide);
            }
        }
        if (lbl_80344824 & (1 << i)) {
            mbBlitInit3414(rune13_blit[i], 0);
        }
        frames = (u32)MBOX_FindTexture_Err("QUEST_ICON", NULL, 0);
        mbInitBlitEntry(quest_blit[i], frames, 0);
        break;
    case 4:
    default:
        goto done;
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

static inline s32 CalcLevelExp(s32 lv) {
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

/* Give exp; mode -2 scales by level bracket, mode 1 charges the power
 * meter, mode >= 0 forwards into an attached familiar. */
s32 AddExp(s32 pnum, s32 amount, s32 mode) {
    Player* p = P(pnum);
    s32 res;

    if (mode == -2) {
        s32 lv;
        s32 delta;

        if (amount < 0 && p->level == 99) {
            return 0;
        }
        lv = p->level;
        if (lv <= 60) {
            delta = (lv - 1) * 0x3C + 1000;
        } else {
            delta = 0x11F8;
        }
        amount = amount * (s32)(0.01 * (f32)delta);
    } else {
        f32 dist = PF(gCurLevel, 0x9C, f32);
        f32 fac = PF(gCurLevel, 0xA0, f32);

        if (dist > 0.0f) {
            if ((f32)p->level > dist) {
                fac = fac * (1.0 / (0.1 * ((f32)p->level - dist) + 1.0));
            }
        }
        amount = (s32)((f32)amount * fac);
        if (gControllerButtons & 0x10) {
            f64 five = 5.0;
            amount = (s32)((f64)amount * five);
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
            s32 fx;

            msgPost(0x22, pnum, (u32)p->col_pos);
            fx = StartLevelUpFX(0, p->class_id);
            SfxSetParent(fx, p->node);
            PF(p, 0x1EB4, f32) += 100.0;
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
    u8 unused[8];

    p->exp = p->exp + delta;
    if (p->exp < 0) {
        p->exp = 0;
    }
    if (delta > 0) {
        need = CalcLevelExp(p->level + 1);
        while (p->exp >= need && p->level < 99) {
            res = 1;
            p->level = p->level + 1;
            check_player_atts(p, p->character, NULL);
            need = CalcLevelExp(p->level + 1);
        }
    } else if (delta < 0) {
        while ((need = p->level) > 1) {
            need = CalcLevelExp(need);
            if (p->exp >= need) {
                break;
            }
            res = -1;
            p->level = *(volatile s32*)&p->level - 1;
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
void start_magic(s32 pnum, f32* pos, u32 flags, s32 mode, f32 power_scale) {
    Player* p;
    f32 scale;
    f32 power;
    u32 color;
    s32 fx;
    f32 vpos[3];
    f32 vel[3];
    f32 pw;
    f64 tmp;

    scale = 1.0f;
    p = NULL;
    if (pnum >= 0) {
        p = P(pnum);
        power = p->magic_power * power_scale;
        if (pnum == DamageColor(flags)) {
            power *= 1.1;
            scale += 0.1;
        }
        if (p->level >= 0x19) {
            flags |= 0x800000;
        }
    } else {
        scale = 0.8f;
        power = (f32)(20.0 * power_scale);
    }
    color = flags & 0xF;
    if (color == 0) {
        color = (randpottype++ % 4) + 1;
        flags |= color;
    }
    flags |= 0x200;
    if (pnum < 0) {
        mode = 0;
    }
    if (mode >= 2) {
        pw = (f32)(-1.5 * (f64)p->throw_str + 5.0);

        vel[0] = p->mat[8];
        vel[1] = p->mat[9];
        vel[2] = p->mat[10];
        vpos[0] = pos[0];
        vpos[1] = pos[1];
        vpos[2] = pos[2];
        vpos[0] = (f32)(2.0 * vel[0] + vpos[0]);
        vpos[1] = (f32)(2.0 * vel[1] + vpos[1]);
        vpos[2] = (f32)(2.0 * vel[2] + vpos[2]);
        vpos[1] += 4.0;
        tmp = *(volatile f32*)&vel[0];
        vel[0] = (f32)(tmp * lbl_80347880);
        vel[1] = 0.707f;
        vel[2] = (f32)((f64)vel[2] * lbl_80347880);
        vel[0] *= pw;
        vel[1] *= pw;
        vel[2] *= pw;
        StartThrowMagicFX(vpos, vel, flags, pnum, color + 7, 100.0f,
                          (f32)(40.0 * scale), (f32)(0.75 * power));
    } else if (mode == 1) {
        fx = StartShieldFX(NULL, flags, pnum, (f32)(25.0 * scale),
                           (f32)(0.25 * power));
        MBNodeSetParent(Effects[fx].node, p->node);
        if (pnum >= 0) {
            AudioPotion(color, pos, 1);
        }
    } else {
        fx = StartMagicFX(pos, flags, pnum, (f32)(40.0 * scale), power);
        PlaceEffectOnFloor(fx, NULL);
        if (pnum >= 0) {
            AudioPotion(color, pos, 0);
        }
    }
    if (p != NULL && p->level >= 0x4B && mode != 1) {
        fx = StartMagicHealFX(NULL, power);
        SfxSetParent(fx, p->node);
    }
}

/* ------------------------------------------------------------------ */
/* master driver                                                       */
/* ------------------------------------------------------------------ */

/* Per-frame driver for all four players: welcome/demo speech, beacon
 * light decay, state machine per player (select/dead/ghost/tower),
 * action + motion, "IT" search, speech round-robin, ambient audio. */
s32 do_players(void) {
    s32 i;
    s32 j;
    s32 it;
    f32 best;
    Player* p;
    s32 loaded;
    s32 exit_level;
    s32 state;

    exit_level = 0;
    if ((u32)lbl_80344824 != 0) {
        loaded = 1;
    } else {
        loaded = 0;
    }
    it = -1;
    best = 9999.0f;
    if (gControllerButtons & 0x10) {
        opt_force_player = 0xFFFFFFFF;
    }
    if (gGameMode == 0x4010 && (s32)opt_force_player >= 0) {
        if (gScriptedCameraState != 0 || lbl_803447B8 != 0) {
            lbl_80344B10 = 0;
        } else {
            lbl_80344B10 += gFrameTicks;
            if (lbl_80344B10 >= 1) {
                if (gDemoMode != 0) {
                    if (sMusicTrackHi == 0xD) {
                        if (lbl_80344C4C == 1 && !(opt_force_player & 1)) {
                            ControllerMessageBox(-1, FindStringMessageListSub(0, "DemoWelcome"), 0, -1);
                            opt_force_player |= 1;
                        }
                    } else if (sMusicTrackHi != 0xC && (opt_force_player & 2)) {
                        ControllerMessageBox(-1, FindStringMessageListSub(0, "DemoLevel"), 0, -1);
                        opt_force_player &= ~2;
                    }
                } else {
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
                }
            }
        }
    }
    if (gGameMode != 0x4012 && gGameMode != 0x400D && gGameMode != 0x400F &&
        gGameMode != 0x4016) {
        msgUpdate();
        for (i = 0, p = PT(0); i < 4; i++, p++) {
            if (p->state != 0) {
                s32 sel;

                if (p->state == 2 || p->state == 3) {
                    sel = 1;
                } else {
                    sel = 0;
                }

                if (!sel) {
                    fn_8005A338(p->mat, p->anchor_fwd, p->anchor_pos);
                    if (p->platform != NULL && *p->platform != 0) {
                        WorldVector((f32*)((u8*)*p->platform + 0x30), p->beacon_pos,
                                    p->mat);
                        p->beacon_pos[1] = 0.0f;
                        p->beacon_pos[0] = p->pos[0] + p->beacon_pos[0];
                        p->beacon_pos[1] = p->pos[1] + p->beacon_pos[1];
                        p->beacon_pos[2] = p->pos[2] + p->beacon_pos[2];
                    }
                }
            }
        }
        if (gGameBusy | gGameplayPauseTimer) {
            WritePlayerInfo(-1);
            return 0;
        }
        if (gTriggerCameraState == 1) {
            WritePlayerInfo(-1);
            for (i = 0, p = PT(0); i < 4; i++, p++) {
                p->select_timer = p->select_timer + gClockFrameStep;
            }
            return 0;
        }
        if (gGameMode == 0x4010) {
            TowerCheckMessages(0);
        }
        if (gScriptedCameraState != 0) {
            if (lbl_803447B8 != 0) {
                for (i = 0, p = PT(0); i < 4; i++, p++) {
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
                    if (!(p->hud_flags & 0x20)) {
                        UpdateObjWorldMat(p->mat);
                    }
                }
            }
            WritePlayerInfo(-1);
        }
        if (lbl_8034481C == 0 && opt_restart_request == 0) {
            loaded = 0;
        }
        if (lbl_803447B8 == 0 || lbl_8034481C != 0 || opt_restart_request != 0) {
            lbl_80344804 = 0;
            for (i = 0, p = PT(0); i < 4; i++, p++) {
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
                if (p->hud_flags & 4) {
                    p->hud_flags |= 8;
                    p->hud_flags &= ~4;
                } else {
                    p->hud_flags &= ~8;
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
            exit_level = all_players_go_to_same_level();
            if (!(gControllerButtons & 4)) {
                dbgTextPrintfCol(2, 2, "TRANSMITTER: CT=%02X, C1=%02X, C2=%02X, RATIO=%4.2f",
                                 lbl_80344508, lbl_80344510, lbl_8034450C, lbl_80344504);
            }
        }
    }
    for (i = 0, p = PT(0); i < 4; i++, p++) {
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
    for (i = 0, p = PT(0); i < 4; i++, p++) {
        s32 selected;

        if (gGameMode == 0x400B || gGameMode == 0x400F) {
            continue;
        }
        state = p->state;
        if (state == 2 || state == 3) {
            selected = 1;
        } else {
            selected = 0;
        }
        if (selected == 0) {
            if (gGameMode == 0x4012 || gGameMode == 0x400D || gGameMode == 0x400F ||
                gGameMode == 0x4016) {
                continue;
            }
            if (lbl_803447B8 != 0 && lbl_8034481C == 0 && opt_restart_request == 0) {
                if (state == 1) {
                    PlayerProcessPowerups(p);
                    PlayerMotion_SetAnimState(p);
                    PlayerProcessScale(p);
                    PlayerDoWeapTrail(p);
                    if (!(p->hud_flags & 0x20)) {
                        UpdateObjWorldMat(p->mat);
                    }
                    PF(PF(p, 0x6C8, u8*), 0x30, f32) = p->pos[0];
                    PF(PF(p, 0x6C8, u8*), 0x34, f32) = p->pos[1];
                    PF(PF(p, 0x6C8, u8*), 0x38, f32) = p->pos[2];
                }
                continue;
            }
        }
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
            {
                s32 timer = p->respawn_timer;

                if (timer > 0) {
                    timer -= gFrameTicks;
                    p->respawn_timer = timer;
                    if ((s16)timer <= 0) {
                        setup_player_display(i);
                    }
                }
                break;
            }
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
            case 4:
                if (lbl_80344808 == 0) {
                    loaded = 0;
                }
                if (p->prev_state != 4) {
                    MBTreeClearFlags(p->node, 2, 0);
                    if (PF(p, 0x6C8, void*) != NULL) {
                        MBTreeSetFlags(PF(p, 0x6C8, void*), 2, 0);
                    }
                }
                if (exit_level != 0 || lbl_803447B4 != 0) {
                    s32 any = 0;

                    if (lbl_803447B4 == 0) {
                        for (j = 0; j < 4; j++) {
                            s32 st = PT(j)->state;

                            if (st == 1 || st == 8) {
                                any = 1;
                                break;
                            }
                        }
                    }
                    if (!any) {
                        PlayerProcessScale(p);
                        PlayerDoWeapTrail(p);
                        do_exit(p, exit_level);
                        break;
                    }
                }
                if (p->idle_timer >= 600) {
                    p->idle_timer = p->idle_timer - 540;
                    AudioPlayerBreath(i);
                }
                /* fallthrough */
            case 1:
                if (gGameMode == 0x4010) {
                    f32 light_pos[3];

                    p->intower = 1;
                    PF(p, 0xC28 + p->character * 0x1C, f32) =
                        PF(p, 0xC28 + p->character * 0x1C, f32) + (f32)gFrameTicks;
                    if (PF(gCurLevel, 0, u32) & 8) {
                        light_pos[0] = p->col_pos[0];
                        light_pos[1] = p->col_pos[1];
                        light_pos[2] = p->col_pos[2];
                        light_pos[1] += lbl_80343D78;
                        fn_800C0ADC(light_pos, &lbl_801201F8[p->class_id * 4],
                                   lbl_80343D74, lbl_80343D70);
                    }
                }
                if ((u32)(lbl_8034489C - 2) <= 1) {
                    if (p->quest_state != 0) {
                        if (p->quest_state == 1) {
                            p->quest_state = 2;
                            towerClearRuneNear(i, sMusicTrackHi);
                        }
                        p->pulse_7FC = 2.0f;
                    } else {
                        p->pulse_7FC = 0.8f;
                    }
                }
                add_target(p->mat);
                {
                    s32 name_timer = p->name_timer;

                    if ((sMusicTrackHi != 0xD || sumnerSpeechActive() == 0) &&
                        gTriggerCameraState == 0 && gModalRenderDepth == 0 &&
                        gMessageActive == 0 && name_timer > 0 &&
                        !(gGameBusy | gGameplayPauseTimer)) {
                        char name[88];
                        f32 spos[2];

                        name_timer -= gFrameTicks;
                        p->name_timer = name_timer;
                        if ((s16)name_timer <= 0) {
                            p->name_timer = 0;
                        }
                        for (j = 0; j < 8; j++) {
                            name[j] = p->name[j];
                            if (name[j] == '_') {
                                name[j] = ' ';
                            }
                        }
                        name[6] = 0;
                        MBWorldToScreen(spos, p->col_pos);
                        DrawTextKeepScale(0.5f, -(s32)spos[0], (s32)spos[1], 7,
                                          0xFFFFFF, name);
                    }
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
                PlayerProcessPowerups(p);
                if ((gControllerButtons & 0x10) && gGameOptions[8] != 0 &&
                    i == 0 && gBossType < 0) {
                    fn_8005ACE0(p->pos);
                }
                PlayerMotion(p);
                if (p->fall_time > 0.0 && p->fall_time + 2.0 < sMusicFadeBase) {
                    if (p->fall_frames >= 0x2D) {
                        fn_8009FEFC(i);
                    } else if (p->fall_frames >= 0x1E) {
                        fn_8009FEA0(i);
                    }
                    p->fall_time = 0.0f;
                    p->fall_frames = 0;
                }
                PlayerProcessScale(p);
                PlayerDoWeapTrail(p);
                do_weakening(p, it == i);
                if (lbl_80344808 == 0) {
                    loaded = 0;
                }
                break;
            case 5:
                if (gGameMode == 0x4013 || gGameMode == 0x4017) {
                    if (p->node != NULL) {
                        MBTreeClearFlags(p->node, 2, 0);
                        DoPlayerAction(p);
                        PF(PF(p, 0x6C8, u8*), 0x30, f32) = p->pos[0];
                        PF(PF(p, 0x6C8, u8*), 0x34, f32) = p->pos[1];
                        PF(PF(p, 0x6C8, u8*), 0x38, f32) = p->pos[2];
                        PlayerProcessSkinFX(p);
                        if (p->character == 0xC) {
                            MBTreeSetScale(1.6f, 1.6f, 1.6f, p->node);
                        } else if (p->level >= 99) {
                            MBTreeSetScale(1.2f, 1.2f, 1.2f, p->node);
                        } else {
                            MBTreeClearFlags(p->node, 8, 0);
                            *(f32*)(p->node + 0x40) = 1.0f;
                            *(f32*)(p->node + 0x44) = 1.0f;
                            *(f32*)(p->node + 0x48) = 1.0f;
                        }
                    }
                    loaded = 0;
                } else {
                    if (p->node != NULL) {
                        MBTreeSetFlags(p->node, 2, 0);
                    }
                    if (lbl_803447B4 < 2 && lbl_8034481C == 0 &&
                        opt_restart_request == 0) {
                        loaded = 0;
                    }
                }
                break;
            case 8:
                PlayerProcessPowerups(p);
                PlayerCheckMovingFloor_80088688(p);
                if (p->count_920 > 0) {
                    p->anim_20C = 0x7E;
                    if (p->anim_208 == 0x7E) {
                        p->count_920 = p->count_920 - 1;
                    }
                }
                DoPlayerAction(p);
                if (p->count_920 <= 0 && p->anim_208 == 0x7E) {
                    p->anim_20C = 0;
                }
                PF(PF(p, 0x6C8, u8*), 0x30, f32) = p->beacon_pos[0];
                PF(PF(p, 0x6C8, u8*), 0x34, f32) = p->beacon_pos[1];
                PF(PF(p, 0x6C8, u8*), 0x38, f32) = p->beacon_pos[2];
                PF(PF(p, 0x6C8, u8*), 0x34, f32) = p->pos[1];
                PlayerProcessScale(p);
                PlayerDoWeapTrail(p);
                if (p->count_920 <= 0 && p->anim_208 != 0x7E) {
                    inactivate_player(i);
                }
                loaded = 0;
                break;
            case 2:
            {
                s32 peer_found;

                PlayerCheckMovingFloor_80088688(p);
                for (j = 0; j < 4; j++) {
                    s32 st;

                    if (j != i && (st = PT(j)->state) != 0 && st != 2 && st != 3) {
                        break;
                    }
                }
                if (j >= 4) {
                    peer_found = 0;
                } else {
                    peer_found = -1;
                }
                if (peer_found == 0) {
                    loaded = 0;
                }
                break;
            }
            case 3:
            {
                s32 peer_found;

                PlayerCheckMovingFloor_80088688(p);
                if (MBBackgroundLoading() == 0 && gGameMode != 0x4013 &&
                    gGameMode != 0x4017) {
                    activate_player(i);
                } else if (gGameOptions[11] == 0 ||
                           (gGameMode != 0x400B && gGameMode != 0x400D &&
                            gGameMode != 0x400F && gGameMode != 0x4016)) {
                    update_class_spec(i);
                    WritePlayerInfo(i);
                }
                for (j = 0; j < 4; j++) {
                    s32 st;

                    if (j != i && (st = PT(j)->state) != 0 && st != 2 && st != 3) {
                        break;
                    }
                }
                if (j >= 4) {
                    peer_found = 0;
                } else {
                    peer_found = -1;
                }
                if (peer_found == 0) {
                    loaded = 0;
                }
                break;
            }
            }
            p->prev_state = state;
    }
    if (gGameMode != 0x400B && gGameMode != 0x400D && gGameMode != 0x4012 &&
        gGameMode != 0x400F && gGameMode != 0x4016) {
        for (i = 0, p = PT(0); i < 4; i++, p++) {
            if (p->state != 0 && (p->hud_flags & 1) &&
                !(p->hud_flags & 0x20)) {
                UpdateObjWorldMat(p->mat);
            }
        }
    }
    j = firstgetidx % 4;
    firstgetidx++;
    for (i = 0; i < 4; i++) {
        s32 k = (j + i) % 4;
        Player* speaker = PT(k);

        if (speaker->speech_req != NULL) {
            if (speaker->state == 1 && gGameMode == 0x4010) {
                fn_8005DE50(speaker, speaker->speech_req);
                for (j = 0; j < 4; j++) {
                    if (j != k && PT(j)->speech_req == speaker->speech_req) {
                        PT(j)->speech_req = NULL;
                    }
                }
            }
            speaker->speech_req = NULL;
        }
    }
    for (i = 0, p = PT(0); i < 4; i++, p++) {
        if ((p->state == 1 || p->state == 4) && p->idle_timer != 0) {
            break;
        }
    }
    if (i < 4 && gGameMode != 0x4012) {
        fn_8009D610(0, PT(i)->col_pos);
    } else {
        fn_8009D610(2, NULL);
    }
    if (gGameMode != 0x400D && gGameMode != 0x4012 && gGameMode != 0x4016 &&
        lbl_803447B8 == 0) {
        if (lbl_803444FC == 0 && lbl_803444F8 <= 0) {
            lbl_80344500 = 0;
        }
        WritePlayerInfo(-1);
        do_got_it_8007FC80();
        AudioAmbientUpdate();
    }
    return loaded;
}

/* ================================================================== */
/* BACK SLICE (0x80077BF0..0x8008091C) -- wired 2026-07-27.            */
/* Faithful Ghidra transcriptions pending match passes; giants noted.  */
/* ================================================================== */

/* extern data (back slice) */
extern char* lbl_80343D6C;    /* active hidden-character code ptr (set_hidden_player) */
extern s32 lbl_80343D68;      /* mini-inventory label table count */
extern s32 lbl_80343DAC;      /* bigape powerup table count */
extern f32 lbl_80343D7C[2];   /* damage range */
extern f32 lbl_80343D84[2];   /* armor range */
extern f32 lbl_80343D8C[2];   /* magic range */
extern f32 lbl_80343D94[2];   /* speed range */
extern f32 lbl_803477AC;
extern f32 lbl_803477B4;
extern f64 lbl_803477D0;
extern f32 lbl_803477D8;
extern f64 lbl_803478B0;
extern f64 lbl_80347A40;
extern f64 lbl_80347838;
extern f64 lbl_803478F8;
extern f32 lbl_80347920;
extern f64 lbl_80347930;
extern f32 lbl_80347A50;
extern f32 lbl_80347A54;
extern f64 lbl_80347A58;
extern f64 lbl_80347A60;
extern f32 lbl_80347A88;
extern f32 lbl_803478E4;
extern f32 lbl_80347778;
extern f32 lbl_80347770;
extern f32 lbl_80347790;
extern f64 __frsqrte(f64 value);
extern f64 __sin(f64 value);
extern f32 lbl_80343D9C[2];   /* missile damage range */
extern f32 lbl_80343DA4[2];   /* missile speed range */
extern s32 lbl_80257594;      /* Unlimited? cheat (3 = unlimited turbo) */
extern s32 lbl_802575A8;      /* Access? cheat (levels open) */
extern s32 lbl_80257630[4];   /* per-player targeting state cleared at init */
extern s32 good_wiz_state;      /* cutscene/no-damage global */
extern s32 gBoss398;      /* boss floor index */
typedef struct PlayerEnemyView {
    u8 pad0[0x32C];
    s32 skip_itemcol;
    u8 pad330[0x64];
} PlayerEnemyView;
extern PlayerEnemyView gEnemies[25]; /* Enemy[25], stride 0x394 */
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
extern u8 gWorldInfo[];
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
extern void* WeapHoldFxTree[4][5];
extern void* PojoTree;
extern void* FireShieldTree;
extern void* lbl_803445B0;
extern void* BreatheFireTree;
extern void* BreatheAcidTree;
extern void* BreatheElecTree;
extern void* WingsTree;
extern PlayerControlState lbl_80240E30[4];
extern u32 lbl_80240E5C[];    /* pad config words, stride 0xF */
extern u32 lbl_80240E60[];
extern u32 lbl_80240E64[];
extern u32 lbl_80240E68[];
extern s32 lbl_80344828;      /* hidden-characters-allowed count */
extern void* sWeaponsBuf;    /* weapons-in-hand enabled */
extern void* gSceneRoot;    /* HUD camera parent */
extern s32 got_max_player_sizes;      /* got_max_player_sizes once-flag */
extern s32 lbl_80344B28;      /* INVENTORY file handle */
extern void* sPowerupsBuf;    /* global atree bank */
extern void* lbl_80344BD4;    /* mikey camera parent */
extern s32 lbl_80344BEC;      /* exit FX color */
extern s32 lbl_80344BF0;
extern s32 lbl_80344BF4;
extern s32 sLastWorldLevel;      /* secret-exit destination */
extern s32 sWorldDataConst;      /* town level id */
extern s32 lbl_80344B84;      /* battle-tower level id */
extern s32 gClockFrameNumber; /* frame parity counter */
extern u32 lbl_80344DA4;      /* world loaded */
extern s32 lbl_803447D0;      /* level-clear/exit progress state */
extern s32 lbl_803447A8[];   /* cleared at init_players (2 elems; unsized = absolute addr) */
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
extern char lbl_80347A38[3];  /* "rb" (sdata2) */
extern char* lbl_80347734;
extern char* lbl_80347738;
extern char* lbl_80347740;
DECL_SECT(".sdata2") extern char lbl_803479C8[];
DECL_SECT(".sdata2") extern char lbl_803479D0[];
DECL_SECT(".sdata2") extern char lbl_803479D8[];
DECL_SECT(".sdata2") extern char lbl_803479E0[];
DECL_SECT(".sdata2") extern char lbl_803479E8[];
DECL_SECT(".sdata2") extern char lbl_803479F0[];
DECL_SECT(".sdata2") extern char lbl_803479F8[];
DECL_SECT(".sdata2") extern char lbl_80347A00[];
DECL_SECT(".sdata2") extern char lbl_80347A08[];
DECL_SECT(".sdata2") extern char lbl_80347A10[];
DECL_SECT(".sdata2") extern char lbl_80347A18[];
DECL_SECT(".sdata2") extern char lbl_80347A20[];
DECL_SECT(".sdata2") extern char lbl_80347A28[];
DECL_SECT(".sdata2") extern char lbl_80347A30[];
extern char lbl_80347A68;
extern char lbl_80347A70;
extern char lbl_80347A78;
extern char lbl_80347A80;
extern char lbl_801205D8[][4];  /* rune world tags (4) */
extern char lbl_801205F8[][4];  /* crystal color tags (8) */
extern char* lbl_8011FCD4[];  /* POTION_ICON_* names (5) */

/* extern functions (back slice) */
extern int rand(void);
extern s32 strncmp(const char* a, const char* b, u32 n);
extern void* memset(void* p, int c, u32 n);
extern void* memcpy(void* d, const void* s, u32 n);
extern f64 __fabs(f64 x);
extern void FatalError(const char* text, s32 errorCode);
extern void FatalErrorf(const char* fmt, ...);
extern int bulletproof_printf(const char* fmt, ...);
extern s32 BytesFree(void);
extern void* AllocMem(u32 size);
extern char* AllocFile(char* name, char* mode, u32 sizehint);
extern void MLMReadFile(char* name, char* mode, u32 size, char* buf);
extern void* AtreeMatch(void* bank, char* name, s32 a);
extern u32 MBOX_LoadModelFixed(char* name, u32 arena, s32 a, char* b, u32 c);
extern void* MBOX_AllocModelMem(s32 objectSize, s32 textureSize,
                               const char* name);
extern s32 MBOX_FindObject(const char* name);
extern void* MBOX_NewObject(const char* name, f32* mat, void* parent, u32 flags);
extern void MBSetObject(void* node, s32 object);
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
extern f32 atan2(f32 x, f32 z);
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
extern s32 StartDeathFX(void* node, s32 kind, u32 flags);
extern s32 DeleteEffect(s32 effect, s32 mode);
extern void AudioPlayEvt102Follow(f32* pos, s32 player);
extern void fn_8009D4F0(s32 player);
extern void fn_8009D560(s32 player);
extern void fn_8009D5A0(s32 player);
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
extern void* fn_80011BBC(void* modelbuf, char* name, void* atree_out, char* tmp, s32 size);
extern void InitActions(void* atree, void* animctx, s32 bank);
extern s32 MBOX_ReallyFindObject(char* name, s32 a, s32 b, s32 dir);
extern s32* AtreeFindMbidxNode(void* atree, s32 idx);
extern void* MBNewObject(s32 node, f32* mat, void* c, u32 flags);
extern void MBPsysSetDebugNode(void* node, s32 a);
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
extern s32 saveMenuPrompt(const char* prompt, char** options, s32 n);
extern s32 OptionsSetup(s32 a);
extern void ControlsUpdate(void);
extern void ReadControls(void);
extern s32 any_level(u32 button);
extern s32 any(u32 button);
extern s32 InitTexMods(void* modelbuf, u32 arena);
extern void DoTexMods(void* modelbuf);
extern s32 fn_8001267C(u16* anim, u32 arena, s32 old);
extern void fn_8005A404(f32* mat, f32* fwd, f32* pos);
extern void ShopLoadData(s32 file);
extern void AudioPlayerXray(s32 player);
extern void get_player_pos(s32 player, s32 mode);
extern void CreateYPRMatrix(f32* out, const f32* angles);
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

typedef struct PlayerExitTimerView {
    u8 _pad000[0x1F2];
    s16 exit_timer;
} PlayerExitTimerView;

/* Sink-and-spin exit sequence; dest chooses the next level.           */
static void do_exit(void* vp, s32 dest) {
    Player* p = vp;
    PlayerExitTimerView* exitPlayer = vp;
    s16 t;

    if (FireScrollActive() != 0) {
        return;
    }
    if (exitPlayer->exit_timer == 0 && dest != 0) {
        exitPlayer->exit_timer = (lbl_8034481C != 0) ? 0 : 0x32;
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
        } else if (lbl_8034481C >= 0xC) {
            PF(p, 0x830, s32) = sWorldDataConst;
        } else if (lbl_8034481C >= 3) {
            PF(p, 0x830, s32) = ((lbl_8034481C - 3) & 0xFF) | 0xC00;
        } else if (lbl_8034481C == 2) {
            if (dest < 0) {
                PF(p, 0x830, s32) = sLastWorldLevel;
            } else {
                PF(p, 0x830, s32) = dest;
            }
        } else if (lbl_8034481C == 1) {
            if (dest < 0) {
                PF(p, 0x830, s32) = (s32)NextWorldLevel(1);
            } else {
                PF(p, 0x830, s32) = dest;
            }
        } else if (lbl_8034481C == -1) {
            if (dest < 0) {
                PF(p, 0x830, s32) = (s32)PrevWorldLevel(1);
            } else {
                PF(p, 0x830, s32) = dest;
            }
        } else {
            PF(p, 0x830, s32) = dest;
        }
        {
            s32 skin = lbl_80344BEC;
            f32* fx = (f32*)((u8*)p + 0x7DC);
            SetSkinFX(1.5f, fx, skin, 10, 1);
        }
    }
    t = exitPlayer->exit_timer - gFrameTicks;
    exitPlayer->exit_timer = t;
    if (t <= 0) {
        exitPlayer->exit_timer = 0;
        if (p->state != 5) {
            p->state = 5;
            del_target(p->mat);
        }
    } else if ((2.0 * PF(p, 0x854, f32) + p->pos[1]) + 1.0 > PF(p, 0x8B8, f32)) {
        /* still above the hole floor: sink and spin */
        f32 move_x = 0.0f;
        f32 move_y = -0.12f;
        f32 move_z = 0.0f;
        u32 ticks = gFrameTicks;
        u8 unused[8];
        move_x *= (f32)ticks;
        p->pos[0] += move_x;
        move_y *= (f32)ticks;
        p->pos[1] += move_y;
        move_z *= (f32)ticks;
        p->pos[2] += move_z;
        YawMat3(p->mat, (f32)(9.424777962 * gClockFrameStep));
        p->hud_flags |= 1;
    }
}

/* Health-drain warning beeps; returns -1 once health is gone.         */
static s32 do_weakening(Player* p, s32 active) {
    s32 player = p->index;
    s16 t;

    if (p->weakening_period != 0) {
        if (p->state == 1 && (gControllerButtons & 4) == 0 && sMusicTrackHi != 0xC) {
            s32 elapsed = p->weakening_elapsed + gFrameTicks;
            p->weakening_elapsed = elapsed;
            if (elapsed >= p->weakening_period) {
                p->weakening_elapsed -= p->weakening_period;
            }
        }
    }
    if (p->health < 1.0) {
        return -1;
    }
    if (active != 0 && p->health <= 150.0f) {
        t = p->heartbeat_timer - gFrameTicks;
        p->heartbeat_timer = t;
        if (t <= 0) {
            if (sMusicTrackHi != 0xD && (p->shield_flags & 0x110000) == 0) {
                AudioHeartBeat(player);
            }
            if (p->health >= 100.0f) {
                p->heartbeat_timer = 0x78;
            } else if (p->health >= 50.0f) {
                p->heartbeat_timer = 0x3C;
            } else {
                p->heartbeat_timer = 0x1E;
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
    f32 give;
    f32 cap;
    f32 giveq;
    f32 d;
    s32 i;
    s32 typ;
    u8 unused[16];

    typ = -1;
    if (p->level >= 75) {
        /* heal caster (heal_player inlined, return discarded) */
        give = (f32)(0.016 * (p->level - 75) + 0.1);
        cap = 100.0 * (p->level - 1) + 500.0;
        if (cap > 9999.0f) {
            cap = 9999.0f;
        }
        amount *= give;
        if (!(amount > 0.0f && p->health >= cap)) {
            p->health += amount;
            if (p->health > cap) {
                p->health = cap;
            }
        }
        giveq = 0.5 * amount;
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
s32 heal_player(Player* p, f32 amount) {
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

extern u8 lbl_801201C4[];     /* weakening default period */

/*
 * Apply damage to player i.  flags carry the damage-kind mask (0x600 =
 * directional/back-stab family, 0x10160 = heavy, 0x8000 = scripted,
 * 0x200 = front-arc-checked); dir is the attacker facing for the
 * front-arc test.  Armor absorbs via ModifyDamage, grunts/gore route
 * through the sfx TU, and the death path posts msg 0xD, drops the meter
 * and got-it entries, and parks state 8 (dying).
 */
s32 damage_player(s32 i, f32 dmg_in, s32 mode, u32 flags, f32* dir) {
    f32 dmg = dmg_in;
    f32 reduced_dmg;
    u32 fl = flags;
    Player* p = P(i);
    s32 result = 0;
    s32 invuln;
    s32 hp_old;
    s32 hp_new;
    f32 hp;
    f32 red;
    s16 hf;

    if (p->state != 1) {
        return 0;
    }
    if (gTriggerCameraState != 0) {
        return 0;
    }
    if ((fl & 0x200) != 0) {
        return 0;
    }
    if (player_can_be_damaged(p) == 0) {
        return 0;
    }

    invuln = gGameOptions[0];
    if (invuln < 2) {
        if (invuln >= 1 && dmg != 999999.0f) {
            invuln = 1;
        } else {
            invuln = 0;
        }
    }
    if ((fl & 0x8000) == 0) {
        if (invuln > 2 || p->state == 4) {
            return 0;
        }
        if (good_wiz_state != 0) {
            return 0;
        }
        if (gBossType >= 0 && gBoss398 >= 0 &&
            *(s32*)((u8*)gEnemies + gBoss398 * 0x394 + 0xB4) != 1) {
            return 0;
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
        if ((hf & 0x600) != 0) {
            /* shielded/back arc */
            if (hf & 0x200) {
                if (dir == NULL) {
                    reduced_dmg = 0.0f;
                } else {
                    red = atan2(dir[0], dir[2]) - PF(p, 0x894, f32);
                    if (red > 3.141592653589793) {
                        red = red - 6.283185307179586;
                    } else if (red <= -3.141592653589793) {
                        red = red + 6.283185307179586;
                    }
                    if (red > -1.5707963267948966 && red < 1.5707963267948966) {
                        reduced_dmg = 0.0f;
                    } else {
                        reduced_dmg = dmg * 0.25;
                    }
                }
            } else {
                reduced_dmg = dmg * 0.25;
            }
            if (dmg - reduced_dmg > 0.5 && p->timer_1FE <= 0) {
                f32 clank = (f32)(0.75 * reduced_dmg);
                if (clank < 0.1) {
                    clank = 0.1;
                } else if (clank > 1.0) {
                    clank = 1.0;
                }
                StartBlockFX(clank, p->index);
                p->timer_1FE = (s16)(s32)(5.0 * clank);
                p->hud_flags |= 0x2000;
            }
            if (fl & 0x10160) {
                fl = (fl & 0xFFFEFE9F) | 0x10;
            } else {
                fl &= ~0x10;
            }
            dmg = reduced_dmg;
        } else {
            /* front hit: "ouch" speech occasionally */
            if (dmg > 40.0f && (fl & 0x10160) && (hf & 0x2000) == 0 &&
                sMusicFadeBase > 5.0) {
                msgPost(0x7D, p->index, (u32)p->col_pos);
            }
        }
    }

    hp = p->health;
    if (dmg < 0.0) {
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
    } else {
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
                    PF(p, 0x898, f32) = 1.0 + sMusicFadeBase;
                }
                if (fl & 0x1000) {
                    PF(p, 0x898, f32) = 4.0 + sMusicFadeBase;
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
    }

    if (p->health < 1.0) {
        /* death */
        AudioPlayerDies(i);
        p->state = 8;
        PF(p, 0x920, s32) = 4;
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
        result = 1;
    } else {
        /* grunt tiers on crossing 150/50 hp; big-hit speech (msg 0xD) */
        hp_old = (s32)(0.25 + hp);
        hp_new = (s32)(0.25 + p->health);
        if (dmg > 0.0f) {
            PF(p, 0x924, f32) += dmg;
        }
        if (hp_old > 150 && hp_new <= 150) {
            if (msgPost(0xD, i, (u32)p->col_pos) == 0) {
                fn_8009FFF4(1, i);
            }
        } else if (hp_old > 50 && hp_new <= 50) {
            if (gClockFrameNumber % 2 == 0) {
                fn_8009FFF4(2, i);
            } else {
                fn_8009FFF4(3, i);
            }
        } else {
            if (mode == 2) {
                if (dmg > 0.0f) {
                    AudioPlayerPain(i);
                }
                PF(p, 0x924, f32) = 0.0f;
            } else if (mode == 3) {
                AudioPlayerPoison(i);
                PF(p, 0x924, f32) = 0.0f;
            } else if (mode != 0) {
                if (hp_old - hp_new > 60) {
                    if (dmg > 0.0f) {
                        AudioPlayerPain(i);
                    }
                    PF(p, 0x924, f32) = 0.0f;
                    mode = 0;
                } else if (PF(p, 0x924, f32) >= 45.0) {
                    PF(p, 0x924, f32) -= 45.0;
                    if (dmg > 0.0f) {
                        AudioPlayerPain(i);
                    }
                    mode = 0;
                }
            }
        }
        if (mode == 1) {
            if (fl & 0x800) {
                if (dmg > 0.0f) {
                    AudioPlayerSeverePain(i);
                }
            } else if (p->timer_1F0 <= 0) {
                s32 kind2 = 0;
                if (fl & 0x20000) {
                    kind2 = 1;
                } else if (fl & 0x40000) {
                    kind2 = 2;
                }
                if (dmg > 0.0f) {
                    AudioPlayerHit(dmg, i, kind2);
                }
                p->timer_1F0 = 0x1E;
            }
        }
    }
    return result;
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
    if (p->item_body_lo > 0 && sMusicTrackHi != 0xD) {
        CopyMat4(gIdentityMatrix, m);
        m[12] = death_pos[0];
        m[13] = death_pos[1];
        m[14] = death_pos[2];
        CopyMat4(p->mat, m);
        if (gBossType < 0) {
            chest = PlaceItem(1, 2, (p->item_body_lo == 1) ? "KEY" : "KEYRING", m);
            if (chest != NULL) {
                chest[0x38] = p->item_body_lo;
            }
        }
    }
    p->item_body_lo = 0;
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

static inline void restore_inactive_player(s32 i) {
    typedef struct InactiveSaveImage {
        u8 bytes[0x1434];
    } InactiveSaveImage;
    Player* p = PT(i);
    f32 cap;

    if (p->character == 2 && HIDDEN_CODE(p) == lbl_80343D6C) {
        cap = 0.5 * (p->level - 1) + 30.0;
        if (cap > 9999.0f) {
            cap = 9999.0f;
        }
        p->health = cap;
    } else {
        *(InactiveSaveImage*)((u8*)p + 0xA80) =
            *(InactiveSaveImage*)((u8*)p + 0x1ECC);
        player_get_from_save(p, -1);
    }
}

/* Park player i (level change / joined-late slot). In the tower the   */
/* slot just goes back to selecting with saved-health restore.         */
void inactivate_player(s32 i) {
    Player* p = PT(i);
    u8 unused[8];

    if (sMusicTrackHi == 0xD) {
        p->state = 1;
        restore_inactive_player(i);
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
#pragma opt_common_subs off
void remove_player_geo(s32 i) {
    Player* p = P(i);
    u8* kid;

    if (PF(p, 0x6D0, u32) != 0) {
        MBPsysSetDebugNode(PF(p, 0x6D0, void*), 1);
    }
    if (PF(p, 0x6CC, u32) != 0) {
        MBPsysSetDebugNode(PF(p, 0x6CC, void*), 1);
    }
    if (PF(p, 0x6D4, u32) != 0) {
        MBPsysSetDebugNode(PF(p, 0x6D4, void*), 1);
    }
    if (PF(p, 0x7F8, s32) >= 0) {
        DelSpecialTexmod(PF(p, 0x7F8, s32));
    }
    if (PF(p, 0x6E0, void*) != NULL) {
        MBRemoveNode(PF(p, 0x6E0, void*), 0);
        PF(p, 0x6E0, void*) = NULL;
    }
    if (PF(p, 0x748, u32) != 0) {
        AtreeDelete((void**)((u8*)p + 0x748));
    }
    if (PF(p, 0x790, u32) != 0) {
        AtreeDelete((void**)((u8*)p + 0x790));
    }
    if (PF(p, 0x6E4, u32) != 0) {
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
    if (PF(p, 0x96C, u32) != 0) {
        AtreeDelete((void**)((u8*)p + 0x96C));
    }
    PF(p, 0xA1C, s16) = 0;
    if (PF(p, 0x96C, u32) != 0) {
        AtreeDelete((void**)((u8*)p + 0x96C));
    }
    if (PF(p, 0xA14, void*) != NULL) {
        MBRemoveNode(PF(p, 0xA14, void*), 1);
        PF(p, 0xA14, void*) = NULL;
    }
    if (PF(p, 0xA14, u8*) != NULL && *(u32*)(PF(p, 0xA14, u8*) + 0x78) != 0) {
        ErrorPrintf("mikey objgrp OBJ NODE HAS KIDS AFTER ALL REMOVED\n");
    }
    /* orphan any remaining children back onto the world */
    if (p->node != NULL && *(u32*)(p->node + 0x78) != 0) {
        u8* node;
        while ((node = p->node,
                kid = *(u8**)(*(u8**)(node + 0x78) + 0x7C)) != NULL) {
            MBNodeSetParent(kid, *(void**)(node + 0x74));
        }
    }
    SfxDeleteParented(p->node, 1, i);
    AtreeDelete((void**)((u8*)p + 0x7C));
    if (p->node != NULL) {
        if (p->node != NULL && *(u32*)(p->node + 0x78) != 0) {
            ErrorPrintf("PLAYER OBJ NODE HAS KIDS AFTER ALL REMOVED\n");
        }
        MBRemoveNode(p->node, 1);
        p->node = NULL;
    }
    MBRemoveNode(PF(p, 0x6C8, void*), 0);
    PF(p, 0x6C8, void*) = NULL;
    ClearPlyrData(i);
}
#pragma opt_common_subs reset

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
    s32 player_index;
    s32 cls;
    s32 j;
    u8 unused[8];

    PF(p, 0x1EBC, s32) = 0;
    for (j = 0; j < 9; j++) {
        ((s32*)p)[0xCC0 + j] = j & 3;
    }
    cls = j;
    if (gDemoMode != 0) {
        p->gold = 0x9C4;
    } else {
        p->gold = 0;
    }
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
    switch (cls) {
    case 0:
    default:
        cls = 6;
        break;
    case 1:
        cls = 5;
        break;
    case 2:
        cls = 4;
        break;
    case 3:
        cls = 7;
        break;
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
    player_index = p->index;
    {
        s32 load_class = 0;
        s32 character = p->character;
        s32 stat_offset = 0;

        do {
            LoadPlyrData(player_index, load_class, NULL);
            PF(p, 0xA98 + stat_offset, f32) = 0.0f;
            PF(p, 0xA9C + stat_offset, f32) = 0.0f;
            PF(p, 0xAA0 + stat_offset, f32) = 0.0f;
            PF(p, 0xAA4 + stat_offset, f32) = 0.0f;
            load_class++;
            stat_offset += 0x18;
        } while (load_class < 16);
        check_player_atts(p, character, NULL);
    }
}

/* Take player i live into the world (post-select).                    */
s32 activate_player(s32 i) {
    Player* players = gPlayerRecords;
    Player* p;
    s32 j;

    players[i].state = 1;
    p = &players[i];
    PF(p, 0x830, s32) = other_players_next_level(i);
    del_player_blits(i);
    LoadPlyrData(i, p->character, (void*)1);
    if (gGameMode != 0x4010) {
        return 1;
    }
    load_player(i);
    if (lbl_803447B8 == 0) {
        PlayerAddPowerup(0.0f, 5.0f, p, 9, 4);
    }
    for (j = 0; j < gNumEnemies; j++) {
        gEnemies[j].skip_itemcol = 0;
    }
    if (lbl_803447B4 != 0 || lbl_803447D0 >= 10 || gGameMode == 0x4016) {
        for (j = 0; j < 4; j++) {
            Player* other = &players[j];
            s32 state = other->state;

            if (lbl_803447B4 != 0 || state == 5) {
                if (state - 4U <= 1) {
                    PF(p, 0x830, s32) = PF(other, 0x830, s32);
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
    lbl_80240E30[i].edges &= ~0x40000;
    return 1;
}

/*
 * Rebuild the player world state after select/level load: cheat-level
 * bump (AllChars), geo load, and the big live-gameplay block reset.
 * Xbox analogue: load_player.
 */
void load_player(s32 i) {
    Player* p = P(i);
    s32 lvl;
    s32 exp;
    s32 product;
    s32 j;
    s32 zero;
    struct {
        f32 pad[2];
        f32 matrix[16];
    } scratch;

    if (gDemoMode != 0 && sMusicTrackHi != 0xD) {
        /* cheat build: force the level stamped on the current level */
        if ((f32)p->level != PF(gCurLevel, 0x9C, f32)) {
            opt_force_player |= 2;
        }
        lvl = (s32)PF(gCurLevel, 0x9C, f32);
        if ((s32)PF(gCurLevel, 0x9C, f32) <= 60) {
            exp = (lvl - 1) * (lvl * 30 + 1000);
        } else {
            product = (lvl - 60) * 4600;
            exp = 0x28550;
            exp += product;
        }
        p->exp = exp;
        p->level = lvl;
        set_player_default_atts(p);
        check_player_atts(p, p->character, NULL);
        p->health = 0.5 * (lvl - 1) + 30.0;
    }
    zero = 0;
    p->node = NULL;
    PF(p, 0x78, s32) = zero;
    load_player_geo(i, p);
    /* Reset the live-gameplay block in the target's store order. */
    PF(p, 0x800, s32) = 0;
    PF(p, 0x804, s32) = 0;
    for (j = 0; j < 8; j++) {
        ((s32*)((u8*)p + 0x808))[j] = zero;
    }
    PF(p, 0x208, s32) = 0;
    PF(p, 0x20C, s32) = 0;
    PF(p, 0x204, s32) = 0;
    PF(p, 0x910, f32) = 0.0f;
    PF(p, 0x6B8, s32) = 0;
    PF(p, 0x6BC, s32) = 0;
    PF(p, 0x838, f32) = 0.0f;
    PF(p, 0x83C, f32) = PF(lbl_80282930[i], 0x50, f32);
    PF(p, 0x840, f32) = 0.0f;
    PF(p, 0x844, f32) = 0.0f;
    PF(p, 0x848, f32) = PF(lbl_80282930[i], 0x54, f32);
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
    PF(p, 0x850, f32) = PF(lbl_80282930[i], 0x4C, f32);
    PF(p, 0x854, f32) = PF(lbl_80282930[i], 0x48, f32) * 0.01;
    PF(p, 0x1F0, s16) = 0;
    PF(p, 0x1FA, s16) = 0;
    PF(p, 0x1FC, s16) = 0;
    PF(p, 0x1FE, s16) = 0;
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
    PF(p, 0xA30, u32) = lbl_801201C4[0];
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
        ((s32*)((u8*)p + 0xA34))[j] = -1;
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
    if ((gGameOptions[11] & 1) == 0 || gGameMode != 0x400B) {
        setup_player_display(i);
    }
    if (lbl_80344DA4 != 0) {
        get_player_pos(i, 1);
        CreateYPRMatrix(scratch.matrix, (f32*)((u8*)p + 0xC4));
        CopyMat3((f32*)((u8*)&scratch + sizeof(scratch.pad)), p->mat);
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
#pragma dont_inline on
void player_get_from_save(void* vp, s32 type) {
    s32 player;
    Player* p;
    s32 character;
    s32 offset;
    s32 exp;
    f32 cap;
    s32 t;
    s32 lv;
    u8 unused[16];

    p = vp;
    player = p->index;

    if (p->character == 2 && HIDDEN_CODE(p) == lbl_80343D6C) {
        /* hidden character: fixed loadout */
        *(PlayerSaveImage*)((u8*)p + 0x1ECC) =
            *(PlayerSaveImage*)((u8*)p + 0xA80);
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

    if (type < 0) {
        type = PF(p, 0xA88, s16);
    }
    p->character = type;
    p->class_id = PF(p, 0xA8A, s8);
    character = p->character;
    PlayerUpdateAtts(p);
    offset = character * 0x18;
    p->exp = PF(p, offset + 0xA90, s32);
    exp = p->exp;
    lv = 99;
    for (;;) {
        if (exp >= LevelToExp(lv)) {
            break;
        }
        if (--lv <= 0) {
            lv = 1;
            break;
        }
    }
    p->level = lv;
    p->health = PF(p, offset + 0xA94, f32);
    offset = type * 0xF0;
    p->gold = PF(p, offset + 0xE00, s32);
    PF(p, 0x1EBC, s32) = PF(p, offset + 0xDD0, s16);
    PF(p, 0x1EB8, s32) = PF(p, offset + 0xDD2, s16);
    PF(p, 0x1EC8, u16) = PF(p, offset + 0xDD4, u16);
    PF(p, 0x1ECA, u16) = PF(p, offset + 0xDD6, u16);
    if (*(f32*)&CHAR_STATS(p, type)[1] == 0.0f) {
        clear_player(p->index, 0);
    }
    p->character = type;
    p->char_type = type;
    if (p->char_type >= 8) {
        p->char_type -= 8;
    }
    check_player_atts(p, type, NULL);
    memcpy((u8*)p + 0x130, (u8*)p + offset + 0xE04, 0xB0);
    PF(p, 0x1EC, s32) = PF(p, offset + 0xDDA, s16);
    PF(p, 0x11C, s32) = 0;
    PF(p, 0x120, u32) = 0;
    PF(p, 0x124, u32) = 0;
    lbl_80240E30[player].scheme = PF(p, 0x1DB0, u8);
    lbl_80240E30[player].hasActuator = PF(p, 0x1DB1, u8);
    lbl_80240E30[player].unk38 = PF(p, 0x1DB2, u8);
    lbl_80240E30[player].unk34 = PF(p, 0x1DB3, u8);
}
#pragma dont_inline off

/* Pack the live fields into the per-character slots + image header.   */
#pragma opt_common_subs off
void player_store_in_save(void* vp) {
    s32 chartype;
    s32 total;
    s32 player;
    Player* p;
    s32* st;
    s32 j;

    p = vp;
    total = 0;
    chartype = p->character;
    player = p->index;

    if (chartype == 2 && HIDDEN_CODE(p) == lbl_80343D6C) {
        /* hidden char: park it, restore the base character, re-flag */
        *(PlayerSaveImage*)((u8*)p + 0xA80) =
            *(PlayerSaveImage*)((u8*)p + 0x1ECC);
        HIDDEN_CODE(p) = NULL;
        player_get_from_save(p, -1);
        HIDDEN_CODE(p) = lbl_80343D6C;
    }
    chartype *= 0xF0;
    st = (s32*)((u8*)p + p->character * 0x18);
    st[0xA90 / 4] = p->exp;
    *(f32*)&st[0xA94 / 4] = p->health;
    {
        u8* item = (u8*)p + chartype;
        *(s32*)(item + 0xE00) = p->gold;
        *(s16*)(item + 0xDD0) = (s16)PF(p, 0x1EBC, s32);
        *(s16*)(item + 0xDD2) = (s16)PF(p, 0x1EB8, s32);
        *(u16*)(item + 0xDD4) |= PF(p, 0x1EC8, u16);
        *(u16*)(item + 0xDD6) |= PF(p, 0x1ECA, u16);
    }
    PF(p, 0xA88, s16) = (s16)p->character;
    PF(p, 0xA8A, s8) = (s8)p->class_id;
    /* total-level checksum across all 16 characters */
    for (j = 0; j < 16; j++) {
        total += ExpToLevel(CHAR_STATS(p, j)[0]);
    }
    PF(p, 0xA8E, u16) = total;
    memcpy((u8*)p + chartype + 0xE04, (u8*)p + 0x130, 0xB0);
    {
        u8* item = (u8*)p + chartype;
        *(s16*)(item + 0xDDA) = (s16)PF(p, 0x1EC, s32);
    }
    PF(p, 0x1DB0, u8) = (u8)lbl_80240E30[player].scheme;
    PF(p, 0x1DB1, u8) = (u8)lbl_80240E30[player].hasActuator;
    PF(p, 0x1DB2, u8) = (u8)lbl_80240E30[player].unk38;
    PF(p, 0x1DB3, u8) = (u8)lbl_80240E30[player].unk34;
    if (p->character == 2 && HIDDEN_CODE(p) == lbl_80343D6C) {
        player_get_from_save(p, -1);
    }
}
#pragma opt_common_subs reset

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
static inline f32 player_scale_att(f32* att, f32* range)
{
    return 0.01 * *att * (range[1] - range[0]) + range[0];
}

#pragma opt_propagation off
void PlayerUpdateAtts(void* vp) {
    Player* p = vp;
    u8 unused[96];
    s32 character;

    LoadPlyrData(p->index, p->character, NULL);
    character = p->character;
    if (character != 2 || HIDDEN_CODE(p) != lbl_80343D6C) {
        check_player_atts(p, character, NULL);
    }
    p->stat_damage = player_scale_att(&p->att_fight, lbl_80343D7C);
    p->stat_armor = player_scale_att(&p->att_armor, lbl_80343D84);
    p->magic_power = player_scale_att(&p->att_magic, lbl_80343D8C);
    p->light_range = player_scale_att(&p->att_speed, lbl_80343D94);
    if (p->char_type == 2 || p->char_type == 6) {
        /* magic-missile classes scale missiles on magic */
        p->stat_missile_dmg = player_scale_att(&p->att_magic, lbl_80343D9C);
        p->stat_missile_spd = player_scale_att(&p->att_magic, lbl_80343DA4);
    } else {
        p->stat_missile_dmg = player_scale_att(&p->att_fight, lbl_80343D9C);
        p->stat_missile_spd = player_scale_att(&p->att_fight, lbl_80343DA4);
    }
}
#pragma opt_propagation reset

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
    u8 unused[8];
    char* c;
    s32* nd;
    s32 pad;
    s32 cls;
    s32 tier;
    s32 n;

    if (p->node != NULL) {
        FatalError("PLAYER OBJ NODE EXISTS BEFORE LOAD_PLAYER", 0x800000);
    }
    if (lbl_80344828 > 0) {
        set_hidden_player(p);
    }
    {
        if (p->hidden_code != NULL) {
            PF(p, 0x7F4, u32) = load_player_model(i, p, i, p->hidden_code);
            goto model_ready;
        }
        if (p->character == player_multiple_models[i].cur_class &&
            p->class_id == player_multiple_models[i].cur_pad &&
            (void*)player_multiple_models[i].cur_override == NULL &&
            p->level / 10 == player_multiple_models[i].cur_tier) {
            goto reuse_model;
        }
        PF(p, 0x7F4, u32) = load_player_model(i, p, -1, NULL);
        goto model_ready;
reuse_model:
        PF(p, 0x7F4, void*) = player_multiple_models[i].arena;
    }
model_ready:
    pad = p->class_id;
    LoadPlyrData(i, p->character, NULL);
    if (sWeaponsBuf != 0) {
        InitPlayerMissiles(p);
    }
    cls = p->character;
    c = p->hidden_code;
    if (c != NULL) {
        strcpy(name, c);
        for (c = name; *c != 0; c++) {
            *c = (char)toupper(*c);
        }
    } else {
        strcpy(name, lbl_80120104[pad]);
    }
    sprintf(tbuf, "%s%s%s", &lbl_801200B0[cls * 4], name, "");
    strncpy((char*)&p->pad_0210[0x4B0], tbuf, 8);
    p->node = MBNewNode(lbl_80344B2C, gIdentityMatrix, 1);
    PF(p, 0x78, s32) = 0;
    p->platform = fn_80011BBC(player_multiple_models[i].model_buf,
                             &lbl_801200B0[p->char_type * 4],
                             &p->platform, tbuf, 0x800);
    if (p->platform == NULL) {
        FatalErrorf("Player Atree %s not found", &lbl_801200B0[p->char_type * 4]);
    }
    MBNodeSetParent(*p->platform, p->node);
    InitActions(&p->platform, (u8*)p + 0x210, 0x80126C68);
    if (gGameMode != 0x4012 && gGameMode != 0x400D &&
        gGameMode != 0x400F && gGameMode != 0x4016) {
        LoadPlyrData(i, p->character, (void*)1);
    }
    PF(p, 0x744, s32) = 0;
    /* attachment nodes */
    sprintf(tbuf, "%s%s", (char*)&p->pad_0210[0x4B0],
            lbl_80120184[cls]);
    n = MBOX_ReallyFindObject(tbuf, PF(p, 0x7F4, s32), PF(p, 0x7F4, s32), 1);
    nd = AtreeFindMbidxNode(p->platform, n);
    if (nd != NULL) {
        PF(p, 0x6D0, s32) = *nd;
    } else {
        PF(p, 0x6D0, s32) = 0;
    }
    sprintf(tbuf, "%s%s", (char*)&p->pad_0210[0x4B0],
            lbl_80120144[cls]);
    n = MBOX_ReallyFindObject(tbuf, PF(p, 0x7F4, s32), PF(p, 0x7F4, s32), 1);
    nd = AtreeFindMbidxNode(p->platform, n);
    if (nd != NULL) {
        PF(p, 0x6CC, s32) = *nd;
    } else {
        PF(p, 0x6CC, s32) = 0;
    }
    sprintf(tbuf, "%sCFGLOW", (char*)&p->pad_0210[0x4B0]);
    n = MBOX_ReallyFindObject(tbuf, PF(p, 0x7F4, s32), PF(p, 0x7F4, s32), -1);
    if (n < 0) {
        nd = NULL;
    } else {
        nd = AtreeFindMbidxNode(p->platform, n);
    }
    if (nd != NULL) {
        PF(p, 0x6D8, s32) = *nd;
        MBTreeSetFlags((void*)*nd, 0x800810, 0);
    } else {
        PF(p, 0x6D8, s32) = 0;
    }
    sprintf(tbuf, "%sHEAD", (char*)&p->pad_0210[0x4B0]);
    n = MBOX_ReallyFindObject(tbuf, PF(p, 0x7F4, s32), PF(p, 0x7F4, s32), 1);
    nd = AtreeFindMbidxNode(p->platform, n);
    if (nd != NULL) {
        PF(p, 0x6D4, s32) = *nd;
    } else {
        PF(p, 0x6D4, s32) = 0;
    }
    sprintf(tbuf, "%sTORSO", (char*)&p->pad_0210[0x4B0]);
    n = MBOX_ReallyFindObject(tbuf, PF(p, 0x7F4, s32), PF(p, 0x7F4, s32), 1);
    nd = AtreeFindMbidxNode(p->platform, n);
    if (nd != NULL) {
        PF(p, 0x6DC, s32) = *nd;
    } else {
        PF(p, 0x6DC, s32) = 0;
    }
    /* weapon */
    if (sWeaponsBuf != 0) {
        tier = (p->level >= 0x32) ? 2 : (p->level >= 10) ? 1 : 0;
        if (lbl_80120598[p->character] != 0 ||
            p->character >= 8 || p->hidden_code != NULL) {
            sprintf(tbuf, "WEAP_HOLD");
        } else {
            sprintf(tbuf, "WEAP_%s_HD%d", lbl_80120104[pad], tier + 1);
        }
        n = MBOX_ReallyFindObject(tbuf, PF(p, 0x7F4, s32), PF(p, 0x7F4, s32), 1);
        PF(p, 0x6E0, void*) = MBNewObject(n, NULL, (void*)PF(p, 0x6D0, s32), 0x810);
        PF(p, 0x7F8, s32) = -1;
        if (p->char_type == 7) {
            *(u32*)(PF(p, 0x6E0, u8*) + 0x60) |= 0x4000000;
            PF(p, 0x7F8, s32) = AddSpecialTexmod(PF(p, 0x7F4, s32), "%s%s",
                                            (char*)player_multiple_models[i].sfx_arena,
                                            "", 5, 1);
        }
    }
    /* live combat fields */
    PF(p, 0x730, s32) = 0;
    PF(p, 0x72C, s32) = 0;
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
    nd = PF(p, 0x6D0, s32*);
    if (nd != NULL) {
        MBPsysSetDebugNode(nd, 0);
    } else {
        nd = PF(p, 0x6CC, s32*);
        if (nd != NULL) {
            MBPsysSetDebugNode(nd, 0);
        } else {
            nd = PF(p, 0x6D4, s32*);
            if (nd != NULL) {
                MBPsysSetDebugNode(nd, 0);
            }
        }
    }
    MBTreeSetFlags(p->node, 2, 0);
    nd = PF(p, 0x6C8, s32*);
    if (nd != NULL) {
        if (sMusicTrackHi == 0xC && sMusicTrackLo == 8) {
            MBTreeSetFlags(nd, 2, 1);
        } else {
            MBTreeSetFlags(nd, 2, 0);
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
    u8* rodata = lbl_80113AE0;
    u8* data = (u8*)lbl_8011FC48;
    char* access_options[2];
    char* access_one[1];
    char* fly_options[2];
    char* unlimited_options[2];
    char* nodamage_options[2];
    char* shards_options[2];
    char* runes_options[2];
    char* cheats_options[2];
    char* select_options[2];
    char* worlds_options[2];
    char* all_fly_options[2];
    char* all_unlimited_options[2];
    char* all_nodamage_options[2];
    char* all_shards_options[2];
    char* all_runes_options[2];
    char* all_cheats_options[2];
    s32 pick = -1;
    u32 pups = 0;
    s32 match = 0;
    s32 prompt_ok;
    s32 j;
    s32 k;

    if (strncmp(p->name, lbl_803479E0, 6) == 0) {
        pick = 0x10;
        match = 1;
    }
    /* the interactive cheat menu (start+trigger names) */
    if ((strncmp(p->name, lbl_803479C8, 6) == 0 ||
         strncmp(p->name, lbl_803479D0, 6) == 0 ||
         strncmp(p->name, lbl_803479D8, 6) == 0) &&
        any_level(0x100000) != 0 && any_level(0x400000) != 0) {
        access_options[0] = lbl_80347734;
        access_one[0] = lbl_80347740;
        access_options[1] = lbl_80347738;
        if (saveMenuPrompt(lbl_803479E8, access_options, 2) == 0) {
            prompt_ok = 1;
        } else {
            prompt_ok = 0;
        }
        if (prompt_ok != 0) {
            match = 1;
        } else {
            match = 0;
        }
        if (match != 0) {
            fly_options[0] = lbl_80347734;
            fly_options[1] = lbl_80347738;
            if (saveMenuPrompt(lbl_803479F0, fly_options, 2) == 0) {
                prompt_ok = 1;
            } else {
                prompt_ok = 0;
            }
            if (prompt_ok != 0) {
                gGameOptions[6] = 1;
            }
            unlimited_options[0] = lbl_80347734;
            unlimited_options[1] = lbl_80347738;
            if (saveMenuPrompt((char*)rodata + 1360,
                               unlimited_options, 2) == 0) {
                prompt_ok = 1;
            } else {
                prompt_ok = 0;
            }
            if (prompt_ok != 0) {
                gGameOptions[1] = 3;
            }
            nodamage_options[0] = lbl_80347734;
            nodamage_options[1] = lbl_80347738;
            if (saveMenuPrompt((char*)rodata + 1372,
                               nodamage_options, 2) == 0) {
                prompt_ok = 1;
            } else {
                prompt_ok = 0;
            }
            if (prompt_ok != 0) {
                gGameOptions[0] = 1;
            }
            shards_options[0] = lbl_80347734;
            shards_options[1] = lbl_80347738;
            if (saveMenuPrompt(lbl_803479F8, shards_options, 2) == 0) {
                prompt_ok = 1;
            } else {
                prompt_ok = 0;
            }
            if (prompt_ok != 0) {
                PF(p, 0x1EC8, u16) = 0xFFFF;
            }
            runes_options[0] = lbl_80347734;
            runes_options[1] = lbl_80347738;
            if (saveMenuPrompt(lbl_80347A00, runes_options, 2) == 0) {
                prompt_ok = 1;
            } else {
                prompt_ok = 0;
            }
            if (prompt_ok != 0) {
                PF(p, 0x1ECA, u16) = 0xFFFF;
            }
            cheats_options[0] = lbl_80347734;
            cheats_options[1] = lbl_80347738;
            if (saveMenuPrompt(lbl_80347A08, cheats_options, 2) == 0) {
                prompt_ok = 1;
            } else {
                prompt_ok = 0;
            }
            if (prompt_ok != 0) {
                pups = 0xFFFFFFFF;
            }
            select_options[0] = lbl_80347734;
            select_options[1] = lbl_80347738;
            if (saveMenuPrompt((char*)rodata + 1384,
                               select_options, 2) == 0) {
                prompt_ok = 1;
            } else {
                prompt_ok = 0;
            }
            if (prompt_ok != 0) {
                while (any(0x80000000) == 0) {
                    if (any(0x40000000) != 0) {
                        pick++;
                        if (pick >= 0 && (u32)pick < 27) {
                            saveMenuPrompt(((HiddenChar*)(data + 2512))[pick].name,
                                           access_one, 1);
                        }
                        if ((u32)pick >= 27) {
                            pick = (u8)rand() % 27U;
                            saveMenuPrompt(lbl_80347A10,
                                           access_one, 1);
                            break;
                        }
                    }
                    ControlsUpdate();
                    ReadControls();
                }
            }
            worlds_options[0] = lbl_80347734;
            worlds_options[1] = lbl_80347738;
            if (saveMenuPrompt((char*)rodata + 1408,
                               worlds_options, 2) == 0) {
                prompt_ok = 1;
            } else {
                prompt_ok = 0;
            }
            if (prompt_ok != 0) {
                for (j = 0; j < 16; j++) {
                    for (k = 0; k < 14; k++) {
                        *((u8*)p + 0x1CD0 + p->character * 14 + k) = 0xFF;
                    }
                    for (k = 0; k < 16; k++) {
                        *(s16*)((u8*)p + j * 240 + 3566 + k * 2) = -1;
                    }
                    *(u16*)((u8*)p + p->character * 240 + 3544) = 0xFFFF;
                    for (k = 0; k < 3; k++) {
                        *(s16*)((u8*)p + p->character * 240 + 3560 + k * 2) = -1;
                    }
                }
            }
            saveMenuPrompt((char*)rodata + 1420,
                           access_one, 1);
        }
    }
    /* one-shot cheat names */
    if (any_level(0x100000) != 0 && any_level(0x400000) != 0) {
        if (strncmp(p->name, lbl_80347A18, 6) == 0) {
            match = 1;
            pick = (rand() & 0xFF) % 27U;
            pups = rand();
        }
        if (strncmp(p->name, lbl_80347A20, 6) == 0) {
            match = 1;
            pick = 5;
            pups = rand();
        }
        if (strncmp(p->name, lbl_80347A28, 6) == 0) {
            match = 1;
            pick = 1;
            pups = rand();
            all_fly_options[0] = lbl_80347734;
            all_fly_options[1] = lbl_80347738;
            if (saveMenuPrompt(lbl_803479F0, all_fly_options, 2) == 0) {
                prompt_ok = 1;
            } else {
                prompt_ok = 0;
            }
            if (prompt_ok != 0) {
                gGameOptions[6] = 1;
            }
            all_unlimited_options[0] = lbl_80347734;
            all_unlimited_options[1] = lbl_80347738;
            if (saveMenuPrompt((char*)rodata + 1360,
                               all_unlimited_options, 2) == 0) {
                prompt_ok = 1;
            } else {
                prompt_ok = 0;
            }
            if (prompt_ok != 0) {
                gGameOptions[1] = 3;
            }
            all_nodamage_options[0] = lbl_80347734;
            all_nodamage_options[1] = lbl_80347738;
            if (saveMenuPrompt((char*)rodata + 1372,
                               all_nodamage_options, 2) == 0) {
                prompt_ok = 1;
            } else {
                prompt_ok = 0;
            }
            if (prompt_ok != 0) {
                gGameOptions[0] = 1;
            }
            all_shards_options[0] = lbl_80347734;
            all_shards_options[1] = lbl_80347738;
            if (saveMenuPrompt(lbl_803479F8, all_shards_options, 2) == 0) {
                prompt_ok = 1;
            } else {
                prompt_ok = 0;
            }
            if (prompt_ok != 0) {
                PF(p, 0x1EC8, u16) = 0xFFFF;
            }
            all_runes_options[0] = lbl_80347734;
            all_runes_options[1] = lbl_80347738;
            if (saveMenuPrompt(lbl_80347A00, all_runes_options, 2) == 0) {
                prompt_ok = 1;
            } else {
                prompt_ok = 0;
            }
            if (prompt_ok != 0) {
                PF(p, 0x1ECA, u16) = 0xFFFF;
            }
            all_cheats_options[0] = lbl_80347734;
            all_cheats_options[1] = lbl_80347738;
            if (saveMenuPrompt(lbl_80347A08, all_cheats_options, 2) == 0) {
                prompt_ok = 1;
            } else {
                prompt_ok = 0;
            }
            if (prompt_ok != 0) {
                pups = 0xFFFFFFFF;
            }
            for (j = 0; j < 16; j++) {
                for (k = 0; k < 14; k++) {
                    *((u8*)p + 0x1CD0 + p->character * 14 + k) = 0xFF;
                }
                for (k = 0; k < 16; k++) {
                    *(s16*)((u8*)p + j * 240 + 3566 + k * 2) = -1;
                }
                for (k = 0; k < 3; k++) {
                    *(s16*)((u8*)p + p->character * 240 + 3560 + k * 2) = -1;
                }
                *(u16*)((u8*)p + p->character * 240 + 3544) = 0xFFFF;
            }
        }
        if (strncmp(p->name, lbl_80347A30, 6) == 0) {
            match = 1;
            pick = 0x17;
            pups = rand();
        }
    }
    if (p->hidden_code != NULL) {
        if (p->hidden_code == lbl_80343D6C) {
            p->char_type = 2;
            p->character = 2;
            return 0;
        }
        return 1;
    }
    for (j = 0; (u32)j < 27; j++) {
        HiddenChar* hidden = (HiddenChar*)(data + 2512) + j;
        if ((strncmp(p->name, hidden->name, 6) == 0 &&
             (hidden->unlocked == 0 || lbl_80344828 > 1)) ||
            (match && pick == j)) {
            p->class_id = hidden->class_id;
            p->character = hidden->char_type;
            p->hidden_code = hidden->code;
            return 1;
        }
    }
    for (j = 0; (u32)j < 27; j++) {
        PupCheat* cheat = (PupCheat*)(data + 3484) + j;
        if (strncmp(p->name, cheat->name, 6) == 0 ||
            (pups & (1 << j))) {
            switch (cheat->type) {
            case 1:
                p->gold = (s32)cheat->value;
                break;
            case 2:
                PF(p, 0x1EB8, s32) = (s32)cheat->value;
                break;
            case 4:
                PF(p, 0x1EBC, s32) = (s32)cheat->value;
                break;
            default:
                PlayerAddPowerup(cheat->value, 1.0f, p,
                                 cheat->type, cheat->mask);
                if (cheat->type == 9) {
                    PF(p, 0x124, u32) |= cheat->mask;
                }
                break;
            }
        }
    }
    return 0;
}

/* Load the class model + sfx model set into player slot i.            */
s32 load_player_model(s32 i, void* vp, s32 alt, char* name) {
    Player* p = vp;
    u8* pot = (u8*) potionicon_tab;
    Player* record;
    s32* sfx_arena;
    char** sfx_buf;
    s32* sfx_remap;
    u32* arena;
    s32 prod;
    u8* q;
    s32 cls;
    s32 ret;
    s32 t;
    s32 raw;

    prod = i * 76;
    record = PT(i);
    cls = record->class_id;
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
    sfx_arena = (s32*) (q + 1352);
    cls = MBOX_LoadModelFixed((char*) pot + 1268, *(u32*) (q + 1360), 0, NULL,
                              *sfx_arena);
    *sfx_arena = cls;
    sfx_buf = (char**) (q + 1372);
    MLMReadFile((char*) pot + 1268, lbl_80347A38, *(u32*) (q + 1364), *sfx_buf);
    sfx_remap = (s32*) (q + 1356);
    *sfx_remap = fn_8001267C((u16*) *sfx_buf, cls, *sfx_remap);
    arena = (u32*) (q + 1316);
    InitTexMods(*(void**) (q + 1336), *arena);
    InitTexMods(*(void**) (q + 1348), *arena);
    InitTexMods(*sfx_buf, *sfx_arena);
    return ret;
}

/* Load one class model + anim set into a model slot.                  */
#pragma opt_common_subs off
s32 load_player_model_sub(s32 i, void* vp, s32 cls_in, char* name, void* vslot) {
    u8* fmt = (u8*) lbl_80113AE0;
    u8* tab = (u8*) lbl_8011FC48;
    u8* pot = (u8*) potionicon_tab;
    PlayerModelSlot* slot = vslot;
    u8* q;
    u8* class_entry;
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
        q = tab + ct * 4;
        sprintf((char*) pot + 1268, (char*) fmt + 1484, q + 1060, name);
    } else {
        q = tab + ct * 4;
        if (*(s32*) (q + 2384) != 0) {
            class_entry = tab + cls * 4;
            sprintf((char*) pot + 1268, (char*) fmt + 1500, q + 1060,
                    *(char**) (class_entry + 1196), tier);
        } else {
            class_entry = tab + cls * 4;
            sprintf((char*) pot + 1268, (char*) fmt + 1484, q + 1060,
                    *(char**) (class_entry + 1196));
        }
    }
    arena = MBOX_LoadModelFixed((char*) pot + 1268, slot->model_max, 0, NULL,
                                (u32) slot->arena);
    if ((s32) slot->anim_max > 0) {
        MLMReadFile((char*) pot + 1268, lbl_80347A38, slot->anim_max, slot->anim_buf);
    } else {
        slot->anim_buf = AllocFile((char*) pot + 1268, lbl_80347A38, slot->anim_max);
    }
    slot->anim_remap2 = fn_8001267C((u16*) slot->anim_buf, arena, slot->anim_remap2);
    slot->arena = (void*) arena;
    slot->cur_class = ct;
    slot->cur_pad = cls;
    slot->cur_tier = tier;
    slot->cur_override = (s32) name;
    q = tab + ct8 * 4;
    sprintf((char*) pot + 1268, (char*) fmt + 1520, q + 1060);
    if ((s32) slot->model_buf_max > 0) {
        MLMReadFile((char*) pot + 1268, lbl_80347A38, slot->model_buf_max,
                    slot->model_buf);
    } else {
        slot->model_buf = AllocFile((char*) pot + 1268, lbl_80347A38,
                                    slot->model_buf_max);
    }
    slot->anim_remap = fn_8001267C((u16*) slot->model_buf, arena, slot->anim_remap);
    return arena;
}
#pragma opt_common_subs reset

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
    GotIt* g;
    s32 i;
    s32 j;
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
    for (i = 0, g = got_it; i < 24; i++, g++) {
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
    welcome_timer = 0;
    alpha = 0;
    key_blit_idx = (s32)MBOX_FindTexture("KEY_ICON", NULL);
    for (j = 0; j < 5; j++) {
        tex = (u32)MBOX_FindTexture(lbl_8011FCD4[j], NULL);
        potionicon_tab[j] = (void*)tex;
    }
}

/* Create every per-player HUD blit set (portrait frames, runes,       */
/* crystals, keys, power meter, rune13, HOD, quest, tb_info).          */
static void create_player_blits(s32 i) {
    Player* player;
    u8* tab = (u8*)lbl_8011FC48;
    u16* lx = (u16*)(tab + i * 2);
    u16* rx;
    u32 tex;
    s32 j;

    frame_blit[i][0] = MBCreateBlit(0, 0, *(lx += 760), 0x130, 0x80, -1);
    frame_blit[i][1] = MBCreateBlit(0, 0, *lx, 0x140, 0x80, -1);
    frame_blit[i][2] = MBCreateBlit(0, 0, *lx, 0x140, 0x80, -1);
    frame_blit[i][3] = MBCreateBlit(0, 0, *lx, 0x158, 0x94, -1);
    frame_blit[i][4] = MBCreateBlit(0, 0, *lx + 8, 0x148, 0x14, 0x14);
    frame_blit[i][5] = MBCreateBlit(0, 0, *lx, 0x148, 0x14, 0x14);
    for (j = 0; j < 6; j++) {
        mbBlitInit3414(frame_blit[i][j], 1);
        mbBlitCvtCoord(frame_blit[i][j], 0.1f);
    }
    for (j = 0; j < 12; j++) {
        sprintf(tbuf, "SM_RUNE_%s_%02d", tab + 2448 + (j / 3) * 4,
                j % 3 + 1);
        tex = (u32)MBOX_FindTexture_Err(tbuf, NULL, 1);
        rune_blit[i][j] = MBCreateBlit(0, tex, *lx + j * 8 + j / 3 + 0xF, 0x132, -1, -1);
        mbBlitInit3414(rune_blit[i][j], 1);
        mbBlitCvtCoord(rune_blit[i][j], 0.1f);
    }
    for (j = 0; j < 8; j++) {
        sprintf(tbuf, "SM_KEY_%s", tab + 2480 + j * 4);
        tex = (u32)MBOX_FindTexture_Err(tbuf, NULL, 1);
        crystal_blit[i][j] = MBCreateBlit(0, tex, *lx + j * 12 + 0xC, 300, -1, -1);
        mbBlitInit3414(crystal_blit[i][j], 1);
        mbBlitCvtCoord(crystal_blit[i][j], 0.1f);
    }
    for (j = 0; j < 4; j++) {
        key_blit[i][j] = MBCreateBlit(0, 0, *lx + 0x1A, j * 3 + 0x142, -1, -1);
        mbBlitInit3414(key_blit[i][j], 1);
        mbBlitCvtCoord(key_blit[i][j], 0.1f);
    }
    for (j = 0; j < 7; j++) {
        pm_blit[i][j] = MBNewBlit(*(char**)(tab + j * 20 + 4),
                                  i * 0x80 + *(s32*)(tab + j * 20 + 8),
                                  *(u32*)(tab + j * 20 + 12));
        *(u32*)(tab + j * 20) = MBBlitGetTex(pm_blit[i][j]);
        mbBlitInit3414(pm_blit[i][j], 1);
        mbBlitCvtCoord(pm_blit[i][j],
                       (f32)*(s32*)(tab + j * 20 + 16));
    }
    player = PT(i);
    PF(player, 0x3340, s32) = 0;
    rx = &lbl_80120240[i];
    tb_info[i].sel = -1;
    tb_info[i].slide = -1;
    tb_info[i].state = 0;
    tb_info[i].x_right = *rx - 0x34;
    tb_info[i].y_top = 0x14F;
    tb_info[i].x_left = *rx - 0x40;
    tb_info[i].y_box = 0x143;
    tb_info[i].tex1 = 0xF9F1;
    tb_info[i].tex2 = 0xF9F2;
    tb_info[i].label = NULL;
    rune13_blit[i] = MBCreateBlit(0, 0, *rx - 0xE, -0x143, -1, -1);
    tex = (u32)MBOX_FindTexture_Err("BTMBK_LEVL", NULL, 1);
    mbInitBlitEntry(rune13_blit[i], tex, 0);
    mbBlitInit3414(rune13_blit[i], 1);
    mbBlitCvtCoord(rune13_blit[i], 0.1f);
    if (lbl_803447C0 != 0) {
        mbBlitUpdateEntry(rune13_blit[i], -1, 0x100);
    }
    PF(player, 0x954, s16) = 0;
    hod_blit[i] = MBCreateBlit(0, 0, *lx + 8, 0x154, 0x10, 0x10);
    tex = (u32)MBOX_FindTexture_Err("HODICON", NULL, 1);
    mbInitBlitEntry(hod_blit[i], tex, 0);
    mbBlitInit3414(hod_blit[i], 1);
    mbBlitCvtCoord(hod_blit[i], 0.1f);
    quest_blit[i] = MBCreateBlit(0, 0, *lx + 0x68, 0x152, 0x10, 0x10);
    mbBlitInit3414(quest_blit[i], 1);
    mbBlitCvtCoord(quest_blit[i], 0.1f);
    player->node = NULL;
    player->index = i;
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
    u8 unused[8];

    GetMaxPlayerModelSize();
    for (i = 0; i < 4; i++) {
        void* arena;

        free0 = BytesFree();
        bulletproof_printf("Player %d -- MEM %d\n", i, free0);
        sprintf(tbuf, "PLAYER %d", i);
        s = &player_multiple_models[i];
        arena = MBOX_AllocModelMem(s->model_max, (s32)s->arena_max, tbuf);
        s->cur_class = -1;
        s->cur_override = 0;
        s->arena = arena;
        s->model_buf = AllocMem(s->model_buf_max);
        s->anim_remap = -1;
        s->anim_buf = AllocMem(s->anim_max);
        s->anim_remap2 = -1;
        s->sfx_arena =
            MBOX_AllocModelMem(s->sfx_max, (s32)s->sfx_arena_max, tbuf);
        s->sfx_buf = AllocMem(s->sfx_buf_max);
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
#define PLAYER_SET_FAMILIAR(source_, parent_)                                  \
    do {                                                                       \
        void* familiar_source = (source_);                                     \
        void* familiar_parent = (parent_);                                     \
        if (*handle != NULL &&                                                 \
            (familiar_source == NULL ||                                        \
             PF(p, 0x798, u32) != ((u32*)familiar_source)[1])) {               \
            AtreeDelete(handle);                                               \
        }                                                                      \
        if (*handle == NULL && familiar_source != NULL) {                      \
            *handle = (void*)AtreeInit(familiar_source, handle, 0, 0x800);     \
            MBTreeSetFlags(*(void**)*handle, 0x10, 0);                         \
            MBNodeSetParent(*(void**)*handle, familiar_parent);                \
            MBTreeSetAlpha(*(void**)*handle, 0, 1);                            \
        }                                                                      \
    } while (0)

void PlayerProcessPowerups(void* vp) {
    Player* p = vp;
    u8 unused[64];
    u32 old_flags;
    f32 weapon_time = 0.0f;
    f32 shield_time = lbl_80347920;
    f32 familiar_time = lbl_80347920;
    f32 alpha_time = 0.0f;
    s32 i;
    const char* name_base = (const char*)lbl_80113AE0;

    p->stat_damage = player_scale_att(&p->att_fight, lbl_80343D7C);
    p->stat_armor = player_scale_att(&p->att_armor, lbl_80343D84);
    p->magic_power = player_scale_att(&p->att_magic, lbl_80343D8C);
    p->light_range = player_scale_att(&p->att_speed, lbl_80343D94);
    if (p->char_type == 2 || p->char_type == 6) {
        p->stat_missile_dmg = player_scale_att(&p->att_magic, lbl_80343D9C);
        p->stat_missile_spd = player_scale_att(&p->att_magic, lbl_80343DA4);
    } else {
        p->stat_missile_dmg = player_scale_att(&p->att_fight, lbl_80343D9C);
        p->stat_missile_spd = player_scale_att(&p->att_fight, lbl_80343DA4);
    }

    old_flags = PF(p, 0x124, u32);
    PF(p, 0x11C, u32) = 0;
    PF(p, 0x120, u32) = 0;
    PF(p, 0x124, u32) = 0;

    for (i = 0; i < 11; i++) {
        f32 timeleft = PUP_TIMELEFT(p, i);
        s32 type;
        u32 flags;

        if (timeleft == 0.0f || PUP_DIRTY(p, i) != 2) {
            continue;
        }
        if (timeleft > 0.0f && sMusicTrackHi != 0xD &&
            gTriggerCameraState == 0 && gGameplayPauseTimer == 0) {
            if (gBossType < 0) {
                timeleft -= gClockFrameStep;
            } else if (gBossActive != 0 && gBossDead == 0) {
                timeleft = (f32)((f64)timeleft -
                    lbl_80347A40 * (f64)gClockFrameStep);
            }
            if (timeleft < 0.0f) {
                timeleft = lbl_803477AC;
            }
            PUP_TIMELEFT(p, i) = timeleft;
        }

        type = PUP_TYPE(p, i);
        flags = PUP_SPECIALFLAGS(p, i);
        switch (type) {
        case 5:
            {
                u32 active = PF(p, 0x11C, u32);
                if ((flags & 0xF) != 0) {
                    if (shield_time < 0.0f ||
                        (timeleft > 0.0f && timeleft > shield_time)) {
                        shield_time = timeleft;
                        active &= ~0xF;
                        active |= flags;
                    }
                    active |= flags & ~0xF;
                } else {
                    active |= flags;
                }
                PF(p, 0x11C, u32) = active;
            }
            break;
        case 6:
            PF(p, 0x120, u32) |= flags;
            if (flags & 0x10000) {
                if (timeleft < 0.0f ||
                    (timeleft > 0.0f && timeleft > weapon_time)) {
                    weapon_time = timeleft;
                }
            }
            if ((flags & 0x200000) && timeleft > familiar_time) {
                familiar_time = timeleft;
            }
            break;
        case 7:
            p->light_range += PUP_ATTRIBUTEADD(p, i);
            PF(p, 0x124, u32) |= 0x10000;
            break;
        case 8:
            p->magic_power += PUP_ATTRIBUTEADD(p, i);
            break;
        case 9:
            PF(p, 0x124, u32) |= flags;
            if (flags & 0x80000) {
                PF(p, 0x828, f32) =
                    (f32)((f64)PF(p, 0x828, f32) + lbl_803477D0);
                if ((f64)PF(p, 0x828, f32) > lbl_803477D0) {
                    PF(p, 0x828, f32) = lbl_803477D8;
                }
                if (timeleft < 0.0f) {
                    PF(p, 0x828, f32) = 0.0f;
                }
            }
            if (flags & 4) {
                if (timeleft < 0.0f ||
                    (timeleft > 0.0f && timeleft > alpha_time)) {
                    alpha_time = timeleft;
                }
            }
            if (flags & 0x7004F1) {
                if (timeleft < 0.0f) {
                    familiar_time = lbl_80347A50;
                } else if (timeleft > familiar_time) {
                    familiar_time = timeleft;
                }
            }
            break;
        }
        if (PUP_TIMELEFT(p, i) == 0.0f) {
            PUP_DIRTY(p, i) = 3;
        }
    }

    if (p->node != NULL) {
        if (PF(p, 0x124, u32) & 2) {
            do_see_thru(p);
        } else if (old_flags & 2) {
            s32 player = p->index;
            if (lbl_8025ECB8[player][0] != NULL) {
                AtreeDelete(&lbl_8025ECB8[player][0]);
            }
            MBTreeSetFlags((void*)lbl_8025EC68[player], 1, 0);
            if (lbl_8025EC88[player] != NULL &&
                *(void**)(lbl_8025EC88[player] + 100) != NULL) {
                void* chest_node = *(void**)(lbl_8025EC88[player] + 100);
                MBNodeSetParent(chest_node, (void*)lbl_8025EC98[player]);
                MBTreeSetAlpha(chest_node, 0, 1);
                CopyMat3((f32*)lbl_8025ECA8[player], (f32*)chest_node);
                *(f32*)((u8*)chest_node + 0x30) = *(f32*)(lbl_8025ECA8[player] + 0x30);
                *(f32*)((u8*)chest_node + 0x34) = *(f32*)(lbl_8025ECA8[player] + 0x34);
                *(f32*)((u8*)chest_node + 0x38) = *(f32*)(lbl_8025ECA8[player] + 0x38);
            }
            lbl_8025EC88[player] = NULL;
        }

        if (PF(p, 0x208, s32) == 0x92) {
            MBTreeSetAlpha(p->node, (s32)lbl_80347A54, 1);
        } else if (PF(p, 0x124, u32) & 4) {
            s32 player_alpha;
            if (alpha_time < 0.0f || alpha_time > lbl_80347A40 ||
                (((s32)(alpha_time * lbl_80347A58) & 1) != 0)) {
                player_alpha = 160 +
                    (s32)(lbl_80347A60 * __sin(alpha_time * lbl_80347930));
            } else {
                player_alpha = 0;
            }
            MBTreeSetAlpha(p->node, player_alpha, 1);
        } else {
            MBTreeSetAlpha(p->node, 0, 1);
        }

        if (PF(p, 0x120, u32) & 0x100000) {
            if (weapon_time < 0.0f || weapon_time > lbl_80347A40 ||
                (((s32)(weapon_time * lbl_80347A58) & 1) != 0)) {
                SetSkinFX(lbl_80347790, (f32*)((u8*)p + 0x7DC),
                          lbl_80344BF0, 1, 1);
            }
        }
        if (PF(p, 0x120, u32) & 0x10000) {
            if (weapon_time < 0.0f || weapon_time > lbl_80347A40 ||
                (((s32)(weapon_time * lbl_80347A58) & 1) != 0)) {
                SetSkinFX(lbl_80347790, (f32*)((u8*)p + 0x7DC),
                          lbl_80344BF4, 1, 1);
            }
        }
    }

    {
        s32 had_object = PF(p, 0x72C, void*) != NULL;

        if (PF(p, 0x124, u32) & 0x8000) {
            const char* name = name_base + 1628;
            s32 model = MBOX_FindObject(name);
            if (PF(p, 0x72C, void*) == NULL) {
                PF(p, 0x72C, void*) =
                    MBNewObject(model, NULL, PF(p, 0x6CC, void*), 0x9010);
            } else {
                MBSetObject(PF(p, 0x72C, void*), model);
            }
        } else if (PF(p, 0x120, u32) & 0x20000) {
            s32 model = MBOX_FindObject(&lbl_80347A68);
            if (PF(p, 0x72C, void*) == NULL) {
                PF(p, 0x72C, void*) =
                    MBNewObject(model, NULL, PF(p, 0x6CC, void*), 0x810);
            } else {
                MBSetObject(PF(p, 0x72C, void*), model);
            }
        } else if (PF(p, 0x120, u32) & 0x200000) {
            s32 model = MBOX_FindObject(&lbl_80347A70);
            if (PF(p, 0x72C, void*) == NULL) {
                PF(p, 0x72C, void*) =
                    MBNewObject(model, NULL, PF(p, 0x6CC, void*), 0x810);
            } else {
                MBSetObject(PF(p, 0x72C, void*), model);
            }
        } else if (PF(p, 0x120, u32) & 0x400000) {
            s32 model = MBOX_FindObject(&lbl_80347A78);
            if (PF(p, 0x72C, void*) == NULL) {
                PF(p, 0x72C, void*) =
                    MBNewObject(model, NULL, PF(p, 0x6CC, void*), 0x810);
            } else {
                MBSetObject(PF(p, 0x72C, void*), model);
            }
        } else if (PF(p, 0x72C, void*) != NULL) {
            MBRemoveNode(PF(p, 0x72C, void*), 0);
            PF(p, 0x72C, void*) = NULL;
        }
        if (PF(p, 0x72C, void*) != NULL && !had_object) {
            if (p->state == 1 || p->state == 5) {
                void* node = *(void**)((u8*)PF(p, 0x6CC, void*) + 0x74);
                PF(node, 0x60, u32) |= 1;
            } else if (p->state == 7) {
                PF(PF(p, 0x6CC, void*), 0x60, u32) |= 1;
            }
        } else if (PF(p, 0x72C, void*) == NULL && had_object) {
            if (p->state == 1 || p->state == 5) {
                void* node = *(void**)((u8*)PF(p, 0x6CC, void*) + 0x74);
                PF(node, 0x60, u32) &= ~1;
            } else if (p->state == 7) {
                PF(PF(p, 0x6CC, void*), 0x60, u32) &= ~1;
            }
        }
    }

    if ((PF(p, 0x120, u32) & 0x100000) && PF(p, 0xA1C, s16) == 0) {
        PF(p, 0xA1C, s16) = 1;
    }
    {
        void** object = (void**)((u8*)p + 0x968);
        if (PF(p, 0x124, u32) & 0x200000) {
            if (PF(p, 0xA1E, s16) == 0) {
                const char* name = name_base + 1640;
                s32 model = MBOX_FindObject(name);
                if (*object == NULL) {
                    *object = MBNewObject(model, NULL, p->node, 0x10);
                } else {
                    MBSetObject(*object, model);
                }
                PF(p, 0xA1E, s16) = 1;
                StartGemFX((f32*)((u8*)p + 0x64), 1);
            }
        } else if (PF(p, 0x124, u32) & 0x400000) {
            if (PF(p, 0xA20, s16) == 0) {
                const char* name = name_base + 1660;
                s32 model = MBOX_FindObject(name);
                if (*object == NULL) {
                    *object = MBNewObject(model, NULL, p->node, 0x10);
                } else {
                    MBSetObject(*object, model);
                }
                PF(p, 0xA20, s16) = 1;
                StartGemFX((f32*)((u8*)p + 0x64), 1);
            }
        } else {
            if (*object != NULL) {
                MBRemoveNode(*object, 0);
            }
            PF(p, 0xA20, s16) = 0;
            PF(p, 0xA1E, s16) = 0;
            *object = NULL;
        }
    }

    {
        void** object = (void**)((u8*)p + 0x734);
        void* parent = PF(p, 0x6D4, void*);
        if (PF(p, 0x124, u32) & 0x1000) {
            const char* name = name_base + 1676;
            s32 model = MBOX_FindObject(name);
            if (*object == NULL) {
                *object = MBNewObject(model, NULL, parent, 0x9010);
            } else {
                MBSetObject(*object, model);
            }
        } else if (PF(p, 0x124, u32) & 0x2000) {
            const char* name = name_base + 1688;
            s32 model = MBOX_FindObject(name);
            if (*object == NULL) {
                *object = MBNewObject(model, NULL, parent, 0x9010);
            } else {
                MBSetObject(*object, model);
            }
        } else if (PF(p, 0x120, u32) & 0x80000) {
            const char* name = name_base + 1700;
            s32 model = MBOX_FindObject(name);
            if (*object == NULL) {
                *object = MBNewObject(model, NULL, parent, 0x810);
            } else {
                MBSetObject(*object, model);
            }
        } else if (PF(p, 0x120, u32) & 0x2000) {
            const char* name = name_base + 1712;
            s32 model = MBOX_FindObject(name);
            if (*object == NULL) {
                *object = MBNewObject(model, NULL, parent, 0x810);
            } else {
                MBSetObject(*object, model);
            }
        } else if (PF(p, 0x124, u32) & 2) {
            const char* name = name_base + 1724;
            s32 model = MBOX_FindObject(name);
            if (*object == NULL) {
                *object = MBNewObject(model, NULL, parent, 0x810);
            } else {
                MBSetObject(*object, model);
            }
        } else if (*object != NULL) {
            MBRemoveNode(*object, 0);
            *object = NULL;
        }
    }

    if (lbl_8034489C == 0 || PF(p, 0x834, s32) == 0) {
        void** object = (void**)((u8*)p + 0x730);
        void* parent = PF(p, 0x6D0, void*);
        if (PF(p, 0x124, u32) & 0x4000) {
            const char* name = name_base + 1736;
            s32 model = MBOX_FindObject(name);
            if (*object == NULL) {
                *object = MBNewObject(model, NULL, parent, 0x9010);
            } else {
                MBSetObject(*object, model);
            }
        } else if (PF(p, 0x11C, u32) & 0x100000) {
            const char* name = name_base + 1748;
            s32 model = MBOX_FindObject(name);
            if (*object == NULL) {
                *object = MBNewObject(model, NULL, parent, 0x810);
            } else {
                MBSetObject(*object, model);
            }
        } else if (PF(p, 0x11C, u32) & 0x10000000) {
            const char* name = name_base + 1760;
            s32 model = MBOX_FindObject(name);
            if (*object == NULL) {
                *object = MBNewObject(model, NULL, parent, 0x10);
            } else {
                MBSetObject(*object, model);
            }
        } else if (*object != NULL) {
            MBRemoveNode(*object, 0);
            *object = NULL;
        }
    }

    if (PF(p, 0x730, void*) != NULL) {
        if (PF(p, 0x6E0, void*) != NULL) {
            MBTreeSetFlags(PF(p, 0x6E0, void*), 2, 0);
        }
        AtreeDelete((void**)((u8*)p + 0x6E4));
    } else {
        u32 kind = PF(p, 0x11C, u32) & 0xF;
        void* source = NULL;
        void** handle = (void**)((u8*)p + 0x6E4);
        s32 fresh = 0;

        if (kind != 0) {
            source = WeapHoldFxTree[p->index][kind - 1];
        }
        if (*handle != NULL &&
            (source == NULL || PF(p, 0x6EC, u32) != ((u32*)source)[1])) {
            AtreeDelete(handle);
        }
        if (*handle == NULL && source != NULL) {
            *handle = (void*)AtreeInit(source, handle, 0, 0x81880);
            MBTreeSetFlags(*(void**)*handle, 0x10, 0);
            MBNodeSetParent(*(void**)*handle, PF(p, 0x6D0, void*));
            MBTreeSetAlpha(*(void**)*handle, 0, 1);
            fresh = 1;
        }
        if (fresh != 0 && *handle != NULL) {
            s32 color_offset = (p->level / 10) * 12;
            *(f32*)((u8*)*(void**)*handle + 0x30) =
                ((f32*)(lbl_80282930[p->index] + color_offset))[0x1A];
            *(f32*)((u8*)*(void**)*handle + 0x34) =
                ((f32*)(lbl_80282930[p->index] + color_offset))[0x1B];
            *(f32*)((u8*)*(void**)*handle + 0x38) =
                ((f32*)(lbl_80282930[p->index] + color_offset))[0x1C];
            if (((f32*)(lbl_80282930[p->index] + color_offset))[0x38] != 0.0f) {
                PF(*(void**)*handle, 0x60, u32) |= 8;
                *(f32*)((u8*)*(void**)*handle + 0x40) =
                    ((f32*)(lbl_80282930[p->index] + color_offset))[0x38];
                *(f32*)((u8*)*(void**)*handle + 0x44) =
                    ((f32*)(lbl_80282930[p->index] + color_offset))[0x39];
                *(f32*)((u8*)*(void**)*handle + 0x48) =
                    ((f32*)(lbl_80282930[p->index] + color_offset))[0x3A];
            }
        }
        if (*handle != NULL) {
            AnimateATree(handle, 0, 0);
        }
        MBTreeClearFlags(PF(p, 0x6E0, void*), 2, 0);
    }

    {
        void** handle = (void**)((u8*)p + 0x790);
        s32 anim = 0;
        s32 transition = 0;

        if (PF(p, 0x124, u32) & 0x400) {
            PLAYER_SET_FAMILIAR(PojoTree, p->node);
        } else if ((PF(p, 0x120, u32) & 0x200000) &&
                   PF(p, 0x208, s32) == 22) {
            PLAYER_SET_FAMILIAR(FireShieldTree, p->node);
        } else if (PF(p, 0x124, u32) & 0x80) {
            PLAYER_SET_FAMILIAR(lbl_803445B0, p->node);
        } else if (PF(p, 0x124, u32) & 0x10) {
            PLAYER_SET_FAMILIAR(BreatheFireTree, PF(p, 0x6D4, void*));
        } else if (PF(p, 0x124, u32) & 0x20) {
            PLAYER_SET_FAMILIAR(BreatheAcidTree, PF(p, 0x6D4, void*));
        } else if (PF(p, 0x124, u32) & 0x40) {
            PLAYER_SET_FAMILIAR(BreatheElecTree, PF(p, 0x6D4, void*));
        } else if (PF(p, 0x124, u32) & 1) {
            void* parent = *(void**)((u8*)*(void**)((u8*)p->node + 0x78) + 0x78);
            PLAYER_SET_FAMILIAR(WingsTree, parent);
        } else if (*handle != NULL) {
            AtreeDelete(handle);
        }
        if (*handle != NULL) {
            if (PF(p, 0x124, u32) & 0x400) {
                switch (PF(p, 0x208, s32)) {
                case 8:
                case 17:
                case 18:
                case 19:
                case 20:
                case 22:
                case 25:
                case 26:
                    anim = 1;
                    break;
                case 110:
                    anim = 3;
                    transition = 2;
                    break;
                case 126:
                    anim = 5;
                    transition = 2;
                    break;
                case 27:
                case 127:
                case 128:
                case 129:
                case 130:
                case 131:
                case 133:
                case 135:
                case 148:
                    anim = 4;
                    transition = 2;
                    break;
                }
                if (PF(p, 0x900, u32) & 0x20000000) {
                    anim = 2;
                    transition = 2;
                }
                PF(p, 0x900, u32) &= ~0x20000000;
            } else if ((PF(p, 0x900, u32) & 0x10000000) &&
                       PF(p, 0x7A0, s16) > 1) {
                anim = 1;
                transition = 2;
            }
            if (PF(p, 0x7A2, s16) == 0) {
                transition = 2;
            }
            AnimateATree(handle, anim, transition);
            if (familiar_time >= 0.0f && familiar_time < lbl_80347838) {
                s32 familiar_alpha =
                    (s32)(lbl_803478F8 * (lbl_80347838 - familiar_time));
                MBTreeSetAlpha(*(void**)*handle, familiar_alpha, 1);
            }
        }
    }

    PlayerProcessSkinFX(p);
    PlayerProcessMikeyPUP(p);

    if (PF(p, 0x128, u32) & 1) {
        s32 kind = (PF(p, 0x128, u32) & 2) ? 2 : 1;
        if (PF(p, 0x738, s32) < 0) {
            PF(p, 0x738, s32) = StartDeathFX(p->node, kind, 0x10);
        }
        AudioPlayEvt102Follow((f32*)((u8*)p + 0x44), p->index);
    } else if (PF(p, 0x738, s32) >= 0) {
        PF(p, 0x738, s32) = DeleteEffect(PF(p, 0x738, s32), 0);
        if (PF(p, 0x12C, u32) & 1) {
            AudioPlayEvt102();
        }
    }

    if (PF(p, 0x954, u16) != 0 && (PF(p, 0x964, s16) & 2) == 0) {
        if (PF(p, 0x740, void*) == NULL) {
            PF(p, 0x740, void*) = MBOX_NewObject(&lbl_80347A80, NULL, p->node, 0x10);
        }
    } else if (PF(p, 0x740, void*) != NULL) {
        MBRemoveNode(PF(p, 0x740, void*), 0);
        PF(p, 0x740, void*) = NULL;
    }

    if (PF(p, 0x124, u32) & 0x400) {
        MBTreeSetFlags(*(void**)PF(p, 0x7C, void*), 2, 0);
    } else {
        MBTreeClearFlags(*(void**)PF(p, 0x7C, void*), 2, 0);
        if (old_flags & 0x400) {
            fn_8009D5A0(p->index);
        }
    }

    if (p->character == 12) {
        MBTreeSetScale(lbl_803478E4, lbl_803478E4, lbl_803478E4, p->node);
    } else if (PF(p, 0x124, u32) & 0x100) {
        MBTreeSetScale(lbl_80347A88, lbl_80347A88, lbl_80347A88, p->node);
    } else if (p->level >= 99) {
        MBTreeSetScale(lbl_80347778, lbl_80347778, lbl_80347778, p->node);
    } else {
        MBTreeClearFlags(p->node, 8, 0);
        *(f32*)((u8*)p->node + 0x40) = lbl_80347790;
        *(f32*)((u8*)p->node + 0x44) = lbl_80347790;
        *(f32*)((u8*)p->node + 0x48) = lbl_80347790;
        if (old_flags & 0x100) {
            fn_8009D560(p->index);
        }
    }
    if ((PF(p, 0x124, u32) & 1) == 0 && (old_flags & 1)) {
        fn_8009D4F0(p->index);
    }
    if (p->level >= 99) {
        void* weapon = PF(p, 0x6D4, void*);
        MBTreeSetFlags(weapon, 8, 0);
        *(f32*)((u8*)weapon + 0x40) = lbl_80347770;
        *(f32*)((u8*)weapon + 0x44) = lbl_80347770;
        *(f32*)((u8*)weapon + 0x48) = lbl_80347770;
    }

    PF(p, 0x12C, u32) = PF(p, 0x128, u32);
    PF(p, 0x128, u32) = 0;
    if ((PF(p, 0x120, u32) & 0x80000) == 0) {
        PF(p, 0x95E, s16) = 0;
    }
    if (PF(p, 0x124, u32) & 8) {
        PF(p, 0x960, s16) = 1;
    } else {
        PF(p, 0x960, s16) = 0;
    }

    p->stat_damage = p->stat_damage < lbl_80343D7C[0] ? lbl_80343D7C[0] :
        (p->stat_damage > lbl_80343D7C[1] ? lbl_80343D7C[1] : p->stat_damage);
    p->stat_armor = p->stat_armor < lbl_80343D84[0] ? lbl_80343D84[0] :
        (p->stat_armor > lbl_80343D84[1] ? lbl_80343D84[1] : p->stat_armor);
    p->magic_power = p->magic_power < lbl_80343D8C[0] ? lbl_80343D8C[0] :
        (p->magic_power > lbl_80343D8C[1] ? lbl_80343D8C[1] : p->magic_power);
    p->light_range = p->light_range < lbl_80343D94[0] ? lbl_80343D94[0] :
        (p->light_range > lbl_80343D94[1] ? lbl_80343D94[1] : p->light_range);
    p->stat_missile_dmg = p->stat_missile_dmg < lbl_80343D9C[0] ? lbl_80343D9C[0] :
        (p->stat_missile_dmg > lbl_80343D9C[1] ? lbl_80343D9C[1] : p->stat_missile_dmg);
    p->stat_missile_spd = p->stat_missile_spd < lbl_80343DA4[0] ? lbl_80343DA4[0] :
        (p->stat_missile_spd > lbl_80343DA4[1] ? lbl_80343DA4[1] : p->stat_missile_spd);

    (void)shield_time;
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
typedef struct PlayerMikeyState {
    u8 pad_000[0x96C];
    void* atree;
    u8 pad_970[0x34];
    s16 anim_state;
    u8 pad_9A6[0xE];
    f32 matrix[16];
    f32 saved_pos[3];
    u8 pad_A00[4];
    f32 fx_pos[3];
    u8 pad_A10[4];
    void* node;
    s32 field_A18;
    s16 state;
} PlayerMikeyState;

static inline s32 PlayerFindMikeyPUP(Player* p)
{
    s32 i;

    for (i = 0; i < 11; i++) {
        if (p->powerup[i].type != 9) {
            continue;
        }
        if (p->powerup[i].specialflags != 0x100000) {
            continue;
        }
        return i;
    }
    return -1;
}

void PlayerProcessMikeyPUP(void* vp) {
    Player* p = vp;
    PlayerMikeyState* mp = vp;
    u8 unused[8];
    s32 t = mp->state;
    void* atree;
    s32 slot;
    s32 one;

    if (t == 2) {
        goto live;
    }
    if (t < 2) {
        if (t == 0) {
            return;
        }
        if (t >= 0) {
            goto hatch;
        }
        goto live;
    }
    if (t == 300) {
        goto despawn;
    }
    goto live;

hatch:
    slot = PlayerFindMikeyPUP(p);
    if (slot < 0) {
        return;
    }
    if (p->powerup_state[slot] != 2) {
        return;
    }
    atree = AtreeMatch(sPowerupsBuf, "MIKEYPUP_ON", 1);
    mp->atree = (void*)AtreeInit(atree, &mp->atree, 0, 0);
    one = 1;
    mp->anim_state = one;
    mp->node = MBNewNode(lbl_80344BD4, gIdentityMatrix, 1);
    mp->field_A18 = 0;
    MBNodeSetParent(*(void**)mp->atree, mp->node);
    {
        f32 x = *(f32*)((u8*)p + 0x64);
        f32 y = *(f32*)((u8*)p + 0x68);
        f32 z = *(f32*)((u8*)p + 0x6C);
        *(f32*)((u8*)mp->node + 0x30) = x;
        *(f32*)((u8*)mp->node + 0x34) = y;
        *(f32*)((u8*)mp->node + 0x38) = z;
    }
    CopyMat4((f32*)mp->node, mp->matrix);
    mp->saved_pos[0] = *(f32*)((u8*)mp->node + 0x30);
    mp->saved_pos[1] = *(f32*)((u8*)mp->node + 0x34);
    mp->saved_pos[2] = *(f32*)((u8*)mp->node + 0x38);
    mp->fx_pos[0] = mp->saved_pos[0];
    mp->fx_pos[1] = mp->saved_pos[1];
    mp->fx_pos[2] = mp->saved_pos[2];
    mp->state = 2;
    p->powerup_state[slot] = one;
    return;

despawn:
    slot = PlayerFindMikeyPUP(p);
    AtreeDelete(&mp->atree);
    MBRemoveNode(mp->node, 1);
    mp->node = NULL;
    mp->state = 0;
    p->powerup_state[slot] = 1;
    return;

live:
    /* live: tick anim + sparkles */
    MBTreeClearFlags(*(void**)mp->atree, 2, 0);
    AnimateATree(&mp->atree, 0, 0);
    {
        s32 timer = mp->state;
        if (timer < 0x3C && timer % 10 == 0) {
            StartGemFX(mp->fx_pos, rand() % 4 + 1);
        }
    }
    mp->state++;
    slot = PlayerFindMikeyPUP(p);
    if (slot >= 0 && p->powerup_state[slot] == 2) {
        mp->state = 300;
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
static void do_see_thru(void* vp) {
    Player* p = vp;
    u8 unused[16];
    u8* chest = NULL;
    s32 i = p->index;
    s32* fl;
    s32 closest;
    void* tree;
    s32 floor_id;
    register s32 j;

    if (sItems == NULL) {
        return;
    }
    closest = ClosestChest(p);
    if (closest >= 0) {
        chest = sItems + closest * 0xF0;
        if (!PointVisible(0.5f * *(f32*)(*(u8**)chest + 0xC), (s32*)(chest + 0x44))) {
            chest = NULL;
        }
    }
    if (chest != NULL) {
        floor_id = *(s16*)(chest + 0xDC);
        fl = *(s32**)(gWorldInfo + 0x68) + floor_id * 0x14;
        while (*fl == -1) {
            s16 next_floor = *(s16*)((u8*)fl + RandItemIdx(closest, fl[1], 0) * 2 + 8);
            floor_id = next_floor;
            fl = *(s32**)(gWorldInfo + 0x68) + floor_id * 0x14;
        }
        if (*fl == 4) {
            tree = sDeathIconAtree;
        } else if (*fl == 1 && fl[1] == 2 && *(s16*)(chest + 0xEC) > 1) {
            tree = sKeyringAtree;
        } else {
            tree = (void*)fl[0x13];
        }
        for (j = 0; j < 4; j++) {
            if (j != i && lbl_8025EC88[j] == chest) {
                tree = NULL;
            }
        }
        if (tree != NULL) {
            s32 fresh = 0;
            if (lbl_8025EC88[i] != chest) {
                if (lbl_8025EC88[i] != NULL && *(void**)(lbl_8025EC88[i] + 100) != NULL) {
                    MBNodeSetParent(*(void**)(lbl_8025EC88[i] + 100), (void*)lbl_8025EC98[i]);
                    MBTreeSetAlpha(*(void**)(lbl_8025EC88[i] + 100), 0, 1);
                    CopyMat3((f32*)lbl_8025ECA8[i], *(f32**)(lbl_8025EC88[i] + 100));
                    *(f32*)(*(u8**)(lbl_8025EC88[i] + 100) + 0x30) = *(f32*)(lbl_8025ECA8[i] + 0x30);
                    *(f32*)(*(u8**)(lbl_8025EC88[i] + 100) + 0x34) = *(f32*)(lbl_8025ECA8[i] + 0x34);
                    *(f32*)(*(u8**)(lbl_8025EC88[i] + 100) + 0x38) = *(f32*)(lbl_8025ECA8[i] + 0x38);
                }
                MBTreeSetAlpha(*(void**)(chest + 100), 0xC0, 1);
                lbl_8025EC98[i] = *(s32*)(*(u8**)(chest + 100) + 0x74);
                MBNodeSetParent(lbl_8025ECA8[i], (void*)lbl_8025EC98[i]);
                CopyMat3(*(f32**)(chest + 100), (f32*)lbl_8025ECA8[i]);
                *(f32*)(lbl_8025ECA8[i] + 0x30) = *(f32*)(*(u8**)(chest + 100) + 0x30);
                *(f32*)(lbl_8025ECA8[i] + 0x34) = *(f32*)(*(u8**)(chest + 100) + 0x34);
                *(f32*)(lbl_8025ECA8[i] + 0x38) = *(f32*)(*(u8**)(chest + 100) + 0x38);
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
                MBNodeSetParent(*(void**)(lbl_8025EC88[i] + 100), NULL);
                MBNodeSetParent(*(void**)lbl_8025ECB8[i][0], lbl_8025ECA8[i]);
                MBNodeSetParent((void*)lbl_8025EC68[i], lbl_8025ECA8[i]);
                MBNodeSetParent(*(void**)(lbl_8025EC88[i] + 100), lbl_8025ECA8[i]);
                AudioPlayerXray(i);
            }
            MBTreeClearFlags((void*)lbl_8025EC68[i], 1, 0);
        } else {
            lbl_8025EC78[i] = -1;
        }
    } else {
        lbl_8025EC78[i] = -1;
    }
    if (lbl_8025EC78[i] == -1) {
        /* end_see_thru (inlined) */
        if (lbl_8025ECB8[i][0] != NULL) {
            AtreeDelete(&lbl_8025ECB8[i][0]);
        }
        MBTreeSetFlags((void*)lbl_8025EC68[i], 1, 0);
        if (lbl_8025EC88[i] != NULL) {
            if (*(void**)(lbl_8025EC88[i] + 100) != NULL) {
                MBNodeSetParent(*(void**)(lbl_8025EC88[i] + 100), (void*)lbl_8025EC98[i]);
                MBTreeSetAlpha(*(void**)(lbl_8025EC88[i] + 100), 0, 1);
                CopyMat3((f32*)lbl_8025ECA8[i], *(f32**)(lbl_8025EC88[i] + 100));
                *(f32*)(*(u8**)(lbl_8025EC88[i] + 100) + 0x30) = *(f32*)(lbl_8025ECA8[i] + 0x30);
                *(f32*)(*(u8**)(lbl_8025EC88[i] + 100) + 0x34) = *(f32*)(lbl_8025ECA8[i] + 0x34);
                *(f32*)(*(u8**)(lbl_8025EC88[i] + 100) + 0x38) = *(f32*)(lbl_8025ECA8[i] + 0x38);
            }
        }
        lbl_8025EC88[i] = NULL;
    }
}

/* Index of the closest openable chest to p (Newton-refined distance). */
static s32 ClosestChest(void* vp) {
    Player* p = vp;
    u8* world;
    u8* itemInfo;
    u8* it;
    u8* state;
    s32 j;
    s32 closest = -1;
    s32 st;
    register f32 dx, dy, dz;
    register f32 d;
    f64 half;
    f32 zero;
    f32 best;
    f64 three;
    u8 unused[36];
    volatile f32 root;
    u8 rootPad[4];

    best = lbl_803477B4;
    StartEnemyGrid(p->pos, best);
    zero = lbl_803477AC;
    half = lbl_803478B0;
    world = gWorldInfo;
    three = lbl_80347A40;
    while ((j = NextGridEnemy()) >= 0) {
        it = sItems + j * 0xF0;
        itemInfo = *(u8**)it;
        if (*(s16*)(it + 0xC4) == -1) {
            continue;
        }
        if (*(s32*)itemInfo != 2) {
            continue;
        }
        if (*(s8*)(it + 0xCD) != 0 || *(s8*)(it + 0xC8) != 0) {
            continue;
        }
        state = *(u8**)(world + 0x68);
        state += *(s16*)(it + 0xDC) * 0x50;
        st = *(s32*)state;
        if (st != -1 && st != 4 && st != 1) {
            continue;
        }
        dx = *(volatile f32*)(it + 0x34) - p->pos[0];
        dy = *(volatile f32*)(it + 0x38) - p->pos[1];
        dz = *(volatile f32*)(it + 0x3C) - p->pos[2];
        d = dz * dz + (d = dx * dx + dy * dy);
        if (d > zero) {
            f64 estimate = __frsqrte(d);
            estimate = half * estimate * (three - estimate * estimate * d);
            estimate = half * estimate * (three - estimate * estimate * d);
            estimate = half * estimate * (three - estimate * estimate * d);
            root = (f32)(d * (half * estimate *
                              (three - estimate * estimate * d)));
            d = root;
        }
        if (d < best) {
            best = d;
            closest = j;
        }
    }
    return closest;
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
    u8 dirty[11];
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
    PlayerPowerupOverlay* overlay = vp;
    Player* p = vp;
    f32 best = 1000000.0f;
    f32 str;
    f32 w;
    s32 j;
    s32 pick = 0;

    str = strength * PF(lbl_80282930[p->index], 0x58, f32);
    for (j = 0; j < 11; j++) {
        if (overlay->powerups[j].type == type &&
            (s32)overlay->powerups[j].specialflags == (s32)mask) {
            if (duration > 0.0) {
                overlay->powerups[j].attributeadd += duration;
            }
            if (overlay->powerups[j].timeleft >= 0.0f && str > 0.0) {
                overlay->powerups[j].timeleft += 0.25 * str;
            } else if (str < 0.0) {
                overlay->powerups[j].timeleft = str;
            }
            if (mask & 8) {
                lbl_80344B20 = overlay->powerups[j].timeleft;
            }
            return;
        }
    }
    /* find the weakest/free slot */
    for (j = 0; j < 11; j++) {
        w = overlay->powerups[j].timeleft;
        if (w < 0.0f) {
            w = (overlay->powerups[j].type == type) ? 999999.0f : 1000000.0f;
        }
        if (best == 2000000.0 || w == 0.0 || (w >= 0.0 && w < best)) {
            best = w;
            pick = j;
        }
        if (best == 0.0) {
            break;
        }
    }
    overlay->powerups[pick].timeleft = str;
    overlay->powerups[pick].type = type;
    overlay->powerups[pick].attributeadd = duration;
    overlay->powerups[pick].specialflags = mask;
    if (mask & 8) {
        lbl_80344B20 = str;
    }
    overlay->dirty[pick] = 1;
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
void check_player_atts(void* vp, s32 chartype, f32* stats) {
    Player* p = vp;
    s32 index;
    f32 cap;
    f32 v;

    index = p->index;
    if (p->character == 2 && HIDDEN_CODE(p) == lbl_80343D6C) {
        ATT_FIGHT(p) = 0.9f;
        ATT_ARMOR(p) = 0.9f;
        ATT_MAGIC(p) = 0.9f;
        ATT_SPEED(p) = 0.9f;
        return;
    }
    if (stats == NULL) {
        stats = (f32*)CHAR_STATS(p, chartype);
    }
    LoadPlyrData(index, chartype, NULL);

    v = *(volatile f32*)(lbl_80282930[index] + 0x28) +
        (f32)((p->level - 1) * 5);
    cap = *(volatile f32*)(lbl_80282930[index] + 0x2C);
    if (v < cap) {
        cap = v;
    }
    ATT_FIGHT(p) = (cap + stats[2] < 999.0) ? cap + stats[2] : 999.0;

    v = PF(lbl_80282930[index], 0x38, f32) +
        (f32)((p->level - 1) * 5);
    cap = PF(lbl_80282930[index], 0x3C, f32);
    cap = v < cap ? v : cap;
    ATT_ARMOR(p) = (cap + stats[3] < 999.0) ? cap + stats[3] : 999.0;

    v = PF(lbl_80282930[index], 0x40, f32) +
        (f32)((p->level - 1) * 5);
    cap = PF(lbl_80282930[index], 0x44, f32);
    cap = v < cap ? v : cap;
    ATT_MAGIC(p) = (cap + stats[4] < 999.0) ? cap + stats[4] : 999.0;

    v = PF(lbl_80282930[index], 0x30, f32) +
        (f32)((p->level - 1) * 5);
    cap = PF(lbl_80282930[index], 0x34, f32);
    cap = v < cap ? v : cap;
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
    s32 i;
    u32 x;
    s32 offset;
    s32 player;
    void** blit;

    for (i = 0, offset = 0; i < 24; i++, offset += sizeof(GotIt)) {
        g = (GotIt*)((u8*)got_it + offset);
        player = g->player;
        switch (g->state) {
        case -1:
            g->state = 0;
            blit = &g->blit1;
            if (g->blit1 != NULL) {
                MBRemoveBlit(g->blit1);
                *blit = NULL;
                blit = &g->blit2;
                MBRemoveBlit(g->blit2);
                *blit = NULL;
            }
            break;
        case 4:
            /* sliding down out */
            blit = &g->blit1;
            if (g->blit1 != NULL) {
                mbBlitCalcRect(g->blit1, NULL, &y[0], NULL);
                y[0] += gFrameTicks;
                if (y[0] >= 400) {
                    g->state = -1;
                }
                mbBlitCalcY(*blit, y[0]);
                mbBlitCalcY(g->blit2, y[0] + 0x10);
            }
            break;
        case 3:
            /* holding */
            if ((g->timer -= gFrameTicks) <= 0) {
                g->state++;
            }
            break;
        case 2:
            /* sliding up into place */
            blit = &g->blit1;
            if (g->blit1 != NULL) {
                mbBlitCalcRect(g->blit1, NULL, &y[0], NULL);
                y[0] -= gFrameTicks;
                if (y[0] <= 0x130) {
                    y[0] = 0x130;
                    g->state++;
                    g->timer = 0x5A;
                }
                mbBlitCalcY(*blit, y[0]);
                mbBlitCalcY(g->blit2, y[0] + 0x10);
            }
            break;
        case 1:
            /* create the pair */
            x = lbl_80120238[player];
            sprintf(buf, "%d", g->count);
            switch (g->type) {
            case 2:
                g->blit1 = MBNewBlit(buf, x, 0);
                if (g->count > 1) {
                    g->blit2 = MBNewBlit("KEY_RING", x, 0);
                } else {
                    g->blit2 = MBNewBlit("KEY", x, 0);
                }
                break;
            case 3:
                g->blit1 = MBNewBlit(buf, x, 0);
                if (g->count >= 100) {
                    g->blit2 = MBNewBlit("MEAT", x, 0);
                } else if (g->count <= -100) {
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
            case 1:
                g->blit1 = MBNewBlit(buf, x, 0);
                if (sMusicTrackHi == 0xC) {
                    g->blit2 = MBNewBlit("COINHUD", x, 0);
                } else if (g->count > 10) {
                    g->blit2 = MBNewBlit("KEYS", x, 0);
                } else {
                    g->blit2 = MBNewBlit("KEY", x, 0);
                }
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
            case 0xF:
                g->blit1 = MBNewBlit(buf, x, 0);
                g->blit2 = MBNewBlit("CRYSTAL", x, 0);
                break;
            case 0x10:
                g->blit1 = MBNewBlit(buf, x, 0);
                g->blit2 = MBNewBlit("GOLDNICON", x, 0);
                break;
            case 0xD:
                g->blit1 = MBNewBlit(buf, x, 0);
                g->blit2 = MBNewBlit("LEGEND", x, 0);
                break;
            default:
                g->state = 0;
                return;
            }
            blit = &g->blit1;
            if (g->blit1 != NULL) {
                mbBlitProject(g->blit1, 0x80, 0);
                mbBlitCalcWidth(*blit, x, 0x180, 0.15f);
            }
            blit = &g->blit2;
            if (g->blit2 != NULL) {
                mbBlitProject(g->blit2, 0x80, 0);
                mbBlitCalcWidth(*blit, x, 400, 0.16f);
            }
            g->state++;
            break;
        default:
            break;
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
    s32* label_table;
    u8* base = (u8*)potionicon_tab;
    Player* p = (Player*)(base + i * PREC_STRIDE + 0xC40);
    s32 tb_offset;
    TbInfo* tb;
    u32* held;
    u8 moved;
    u8* selected_pup;
    u8* entry;
    s32 sel;
    s32 j;
    s32 count;
    s32 offset;
    s32 state;
    u8 unused[32];

    label_table = lbl_8011FC48;
    if (gGameMode != 0x4010 || gGameBusy != 0) {
        return;
    }
    tb_offset = i * sizeof(TbInfo);
    tb = (TbInfo*)(base + tb_offset);
    state = *(s32*)((u8*)tb + 0x944);
    moved = 0;
    tb = (TbInfo*)((u8*)tb + 0x940);
    if (state == 1) {
        if (PUP_TIMELEFT(p, tb->sel) == 0.0 ||
            (lbl_80240E30[i].edges & 0x10000000)) {
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
        held = &lbl_80240E30[i].ctl;
        if (*(held += 2) & 0x20000000) {
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
        if (*held & 0x40000000) {
            AudioCursorSelect();
            if (PUP_DIRTY(p, tb->sel) != 2) {
                PUP_DIRTY(p, tb->sel) = 2;
            } else {
                PUP_DIRTY(p, tb->sel) = 3;
            }
        }
        if ((*held & 0x80000000) && tb->state == 1) {
            AudioCursorV();
            tb->state = 3;
            moved = 1;
            tb->slide = 0;
        }
    } else if ((lbl_80240E30[i].edges & 0x40000000) != 0) {
        AudioCursorV();
        if (tb->state == 0) {
            if (tb->sel == -1 && (sel = mini_inventory_find_previous_selectable_item(i)) >= 0) {
                tb->sel = sel;
                j = 0;
                offset = j;
                count = lbl_80343D68;
                selected_pup = (u8*)p + tb->sel * 0x10;
                for (; j < count; j++, offset += 12) {
                    entry = (u8*)label_table + offset;
                    tb->label = *(char**)(entry + 168);
                    entry += 160;
                    if (*(s32*)(entry + 4) ==
                            (*(s32*)(entry + 4) &
                             *(s32*)(selected_pup + 0x13C)) &&
                        *(s32*)(selected_pup + 0x134) == *(s32*)entry) {
                        break;
                    }
                }
                if (j >= lbl_80343D68) {
                    *(char**)(base + tb_offset + 0x964) =
                        (char*)label_table[40 + (lbl_80343D68 - 1) * 3 + 2];
                }
            }
            if (tb->sel >= 0) {
                tb->state = 2;
                moved = 1;
                tb->slide = 0;
            }
        }
    }
    switch (tb->state) {
    case 0:
        break;
    case 2:
        if (tb->slide < 0x80) {
            tb->slide += gFrameTicks * 4;
            if (tb->slide > 0x80) {
                tb->slide = 0x80;
            }
        } else {
            tb->state = 1;
        }
        break;
    case 1:
        mini_inventory_draw_label(i);
        if (moved) {
            j = 0;
            offset = j;
            count = lbl_80343D68;
            selected_pup = (u8*)p + tb->sel * 0x10;
            for (; j < count; j++, offset += 12) {
                entry = (u8*)label_table + offset;
                tb->label = *(char**)(entry + 168);
                entry += 160;
                if (*(s32*)(entry + 4) ==
                        (*(s32*)(entry + 4) &
                         *(s32*)(selected_pup + 0x13C)) &&
                    *(s32*)(selected_pup + 0x134) == *(s32*)entry) {
                    break;
                }
            }
            if (j >= lbl_80343D68) {
                *(char**)(base + tb_offset + 0x964) =
                    (char*)label_table[40 + (lbl_80343D68 - 1) * 3 + 2];
            }
        }
        break;
    case 3:
        if (tb->slide < 0x80) {
            tb->slide += gFrameTicks * 4;
            if (tb->slide > 0x80) {
                tb->slide = 0x80;
            }
        } else {
            tb->state = 0;
        }
        break;
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
    if ((label = *(char**) ((u8*) potionicon_tab + i * 40 + 2404)) == NULL) {
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
