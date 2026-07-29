/*
 * options.c -- GCN OPTIONS.OBJ (shell3D.pdb module .\Release\OPTIONS.OBJ).
 *
 * The in-game options / pause-menu system: the stacked option-menu engine
 * (start_optmenu / do_optmenu / show_optmenu / end_optmenu with a 4-deep
 * options_stack), the top-level per-frame driver DoOptions() (called from
 * game/sys/main.c), the title-screen menu wrappers used by the attract
 * sequencer (TitleMenuInit/TitleMenu/TitleMenuEnd), the audio/controls/screen
 * sub-menus, Sumner's hint menus (HintMenu + the next_*_hint pickers), and the
 * player preference block (init_prefs / OptionsSetup).
 *
 * .text       0x80070A60..0x800745D0  (27 GC functions)
 * extab       0x80006840..0x80006900  (24 entries x 8; 3 leaves have none)
 * extabindex  0x8000A4B0..0x8000A5D0  (24 entries x 12)
 * .data       ~0x8011DDB0..~0x80120050 (rgb trios, menu defs + item tables,
 *              hint tables, controller-diagram tables; NOT claimed yet)
 * .sdata      0x80343D00..0x80343D68  (OPTMENU_* tunables; NOT claimed yet)
 * .sdata2     0x80347594..0x80347608  (float pool + "slider"/" ~ "/prompt
 *              strings; PLAYER.OBJ pool starts at 0x80347608 "XRay")
 * .sbss       0x80344A98..0x80344AF8+ (menu state, hint counters)
 * .bss        0x80274E00..0x80274EA0  (optglobals.tbuf, options_stack, sliders, prefs)
 *
 * BOUNDARY EVIDENCE (OPTIONS | PLAYER at 0x800745D0):
 *  - extabindex run 0x8000A4B0..0x8000A5D0 covers exactly fns 0x80070A60..
 *    0x80074548(OptionsSetup); init_prefs (0x80074598, leaf) is the last
 *    OPTIONS fn; PlayerAttacking onward are Player-struct parent/grab ops called
 *    from critter.c (0x8003BAFC/0x8003CA98) and PlayerMotion (0x80081504).
 *  - sdata2 seam: options pool ends 0x80347600 (int-magic double); 0x80347608
 *    is "XRay" (PLAYER powerup name).
 *  - sdata seam: options tunables end 0x80343D68 (OPTCTL_DY=2 at 0x80343D64);
 *    0x80343D68 (=75) belongs to the next TU.
 *  - string ownership: "A Hint for You"/"OPTONS MAXLEVEL EXCEEDED"/"end_optmenu
 *    called with bad opti" all referenced only from this range.
 *
 * GC function order does NOT match the Xbox PDB listing (the Xbox linker
 * reorders /Gy COMDATs); names below are anchored by behavior + strings:
 *
 *   0x80070A60 HintMenu              (G)  Sumner hint menu entry [tower, attract]
 *   0x80070B00 TitleMenuEnd          (G)  end title menu, clear abortall [attract]
 *   0x80070B3C TitleMenu             (G)  poll title menu; 0=open 1=chose 2=abort
 *   0x80070BA4 TitleMenuInit         (G)  open the title menu [attract]
 *   0x80070BD8 DeleteOptionBlits     (G)  empty on GCN
 *   0x80070BDC OptionsDone           (G)  "menus all closed?" query [game_main]
 *   0x80070C24 DoOptions             (G)  per-frame master driver [main]
 *   0x80071E1C OptionsStart          (G)  open the state-appropriate top menu
 *   0x80071F20 do_screenmenu         (L)  screen-position menu blits (per-frame)
 *   0x800721C8 do_controlsmenu       (L)  controller-diagram + label draw
 *   0x8007234C do_audiomenu          (L)  slider positioning + stereo text
 *   0x80072470 position_audioslider  (L)  place the 5 slider blits
 *   0x800725D8 start_audioslider     (L)  create the 5 slider blits
 *   0x800726A8 do_optmenu            (G)  menu input/navigation, returns code
 *   0x80072AA4 show_optmenu          (G)  menu draw (text, fade, icon, prompts)
 *   0x80073718 end_optmenu           (L)  pop/close menu level(s)
 *   0x80073998 finish_optmenu        (L)  fade-out timer, then remove
 *   0x80073AD8 remove_optmenu        (G)  free menu message/blits
 *   0x80073B54 end_all_optmenus      (G)  close every level
 *   0x80073B8C start_optmenu         (L)  push menu on options_stack
 *   0x80073C78 start_optmenu_nostack (G)  build menu display (font/icon/blits)
 *   0x80073FE4 next_rune_hint        (L)  pick next rune hint (13 runes)
 *   0x80074164 next_legend_hint      (L)  pick next legend-item hint
 *   0x800742F4 next_boss_hint        (L)  pick next boss hint
 *   0x80074484 next_general_hint     (L)  pick next general hint
 *   0x80074548 OptionsSetup          (G)  apply prefs (audio vol/mode, screen)
 *   0x80074598 init_prefs            (G)  default prefs [gamemain]
 *
 * Xbox fns NOT emitted on GC (inlined/dropped, 35 -> 27): hide_optmenu,
 * update_options_time, start_audiomenu, end_audioslider, end_audiomenu,
 * start_controlsmenu, end_controlsmenu, NewTempBlitZ.
 *
 * DATA MAP (flip-time TODO -- .data tables are referenced extern for now):
 *   0x8011DDB0 titlemenu_rgb_off[3], 0x8011DDBC _on, 0x8011DDC8 _hi
 *   0x8011DDD4 optmenu_rgb_off[3],   0x8011DDE0 _on,  0x8011DDEC _hi
 *   0x8011DDF8 optmenu_rgb_text[3],  0x8011DE04 optmenu_rgb_ctls[3]
 *   0x8011DEBC titlemenu (OPTMENU, 0xE8) + item tables/menus through 0x8011FA38
 *   0x8011FA38 ctl_label_pos[16] {mode,dx,dy}, 0x8011FAF8 ctl_blits[3]
 *   0x8011F9F8 rune_idx_table[13], 0x8011F910 hint display menu
 *   .sdata 0x80343D00: OPTMENU_FADE=40, FADE_HOLD=5, FONT=6, MARGIN_X=64,
 *     MARGIN_Y=64, MARGIN_TITLE=58, MARGIN_PLAYER=34, SCROLLZ=255600.0f,
 *     ICON_TIME=15, {general,boss,legend,rune}_hint_index=-1, options_level=-1,
 *     OPTMENU_FINISH_FRAMES=30, optmenu_choice_twice=1, optmenu_icon_z=1.1f,
 *     msg_scale=0.667f, optplyr_scale=0.667f, slider_dxl=-52, slider_dxr=-24,
 *     OPTAUDIO_VOL_WIDTH=264, OPTAUDIO_VOL_HEIGHT=64, OPTCTL_FONT=-1,
 *     OPTCTL_SCALE=1.0f, OPTCTL_DY=2
 */

#include "types.h"

/* ------------------------------------------------------------------ */
/* menu structures                                                     */
/* ------------------------------------------------------------------ */

typedef struct OPTITEM {
    char* text;   /* 0x00 item label */
    s32 code;     /* 0x04 selection code returned by do_optmenu */
    s32 dy;       /* 0x08 extra line spacing */
    char* text2;  /* 0x0C second label part (drawn after " ~ ") */
    u32 rgb;      /* 0x10 per-item color (flags & 0x200) */
    s32 draw_y;   /* 0x14 last drawn y */
    s32 draw_xend;/* 0x18 last drawn end x */
    s32 on;       /* 0x1C radio/checked state */
    s32 value;    /* 0x20 item value; < 0 = disabled (greyed) */
} OPTITEM; /* 0x24 */

typedef struct OPTMENU {
    s32 state;       /* 0x00 menu state id (copied to options_state) */
    f32 title_scale; /* 0x04 title text scale */
    char* title;     /* 0x08 title string (NULL = passive menu) */
    s32 x;           /* 0x0C text x (<0: centered variants) */
    s32 y;           /* 0x10 text y (-1: center on 0xC0) */
    s32 w;           /* 0x14 measured max item width */
    s32 h;           /* 0x18 measured item-column height */
    OPTITEM* items;  /* 0x1C item array, text==NULL terminated */
    u32 flags;       /* 0x20 1=Back 2=Select/Accept 4=Change 8=Center
                             0x10=player tag 0x20=fade 0x40=parchment font
                             0x80=garamond font 0x100=Accept wording
                             0x200=per-item color */
    s32 prompt_y;    /* 0x24 prompt row y */
    s32* rgb_off;    /* 0x28 normal color trio */
    s32* rgb_on;     /* 0x2C highlight color trio */
    s32* rgb_hi;     /* 0x30 flash color trio */
    f32 scale;       /* 0x34 text scale */
    s32 icon;        /* 0x38 nonzero: selection arrow icon */
    u32 icon_rgb;    /* 0x3C icon color */
    s32 icon_dx;     /* 0x40 icon x offset */
    char* blit_name; /* 0x44 backdrop blit texture name */
    s32 bx, by, bw, bh; /* 0x48..0x54 backdrop rect (-1 = auto) */
    char* burn_name; /* 0x58 burn/frame blit texture name */
    s32 ux, uy, uw, uh; /* 0x5C..0x68 burn rect */
    s32 active;      /* 0x6C -1 while shown, 0 when done */
    s32 finish_timer;/* 0x70 fade-out frame counter */
    s32 sel;         /* 0x74 current selection index */
    s32 player;      /* 0x78 owning player (-1 = any) */
    s32 time;        /* 0x7C lifetime frame counter */
    s32 font;        /* 0x80 override font handle */
    s32 num_items;   /* 0x84 counted lazily */
    void* icon_node; /* 0x88 selection-icon scene node */
    u8 msg[0x48];    /* 0x8C message/anim block (AtreeDelete/fn_80012F78) */
    s32 icon_y;      /* 0xD4 icon y position latch */
    s32 icon_t;      /* 0xD8 icon glide timer */
    void* title_blit;/* 0xDC backdrop blit */
    void* burn_blit; /* 0xE0 burn blit */
    s32 burn_frames; /* 0xE4 burn blit frame count */
} OPTMENU; /* 0xE8 */

typedef struct AUDIOSLIDER {
    s32 val;     /* 0x00 0..255 */
    void* ml;    /* 0x04 marker left */
    void* empty; /* 0x08 empty bar */
    void* pink;  /* 0x0C pink (filled) bar */
    void* slid;  /* 0x10 slider knob */
    void* mr;    /* 0x14 marker right */
} AUDIOSLIDER; /* 0x18 */

typedef struct CTLBLIT {
    char* name; /* 0x00 "CONTROLER 1" etc */
    s32 x;      /* 0x04 */
    s32 y;      /* 0x08 */
    s32 w;      /* 0x0C */
    s32 h;      /* 0x10 */
    void* blit; /* 0x14 live blit ptr (written at runtime) */
} CTLBLIT; /* 0x18 */

typedef struct CTLLABEL {
    s32 mode; /* 0x00 1=left justify 2=center else right */
    s32 dx;   /* 0x04 */
    s32 dy;   /* 0x08 */
} CTLLABEL; /* 0x0C */

/* ------------------------------------------------------------------ */
/* extern data (other TUs / unclaimed .data of this TU)                */
/* ------------------------------------------------------------------ */

/* this TU's .data (referenced extern until the data split is claimed) */
extern OPTMENU titlemenu_menu;       /* 0x8011DEBC */
extern OPTMENU optmenu_game;         /* 0x8011E058 in-game top menu */
extern OPTMENU optmenu_tower;        /* 0x8011E218 tower (no hints) */
extern OPTMENU optmenu_yesno;        /* 0x8011E390 quit-confirm */
extern OPTMENU optmenu_tower_hints;  /* 0x8011E4E4 tower (Sumner hints) */
extern OPTMENU optmenu_options;      /* 0x8011E638 options submenu */
extern OPTMENU optmenu_audio_mode;   /* 0x8011E78C */
extern OPTMENU optmenu_yesno2;       /* 0x8011E8E0 */
extern OPTMENU optmenu_audio;        /* 0x8011E9C8 audio sliders menu */
extern OPTMENU optmenu_yesno3;       /* 0x8011EB1C */
extern OPTMENU optmenu_pref_b;       /* 0x8011EC94 */
extern OPTMENU optmenu_pref_c;       /* 0x8011EE0C */
extern OPTMENU optmenu_vibration;    /* 0x8011EF60 */
extern OPTMENU optmenu_screen_sub;   /* 0x8011F13C */
extern OPTMENU optmenu_controls;     /* 0x8011F26C controls diagram menu */
extern OPTMENU optmenu_screen;       /* 0x8011F3C0 screen-position menu */
extern OPTMENU optmenu_sub2;         /* 0x8011F514 */
extern OPTMENU optmenu_sub3;         /* 0x8011F668 */
extern OPTMENU hintmenu_menu;        /* 0x8011F804 hint category menu */
extern OPTMENU hintmenu_display;     /* 0x8011F910 hint display menu */

extern s32 optmenu_rgb_text[3];      /* 0x8011DDF8 */
extern s32 optmenu_rgb_ctls[3];      /* 0x8011DE04 */
extern char opt_style_names[][16];   /* 0x8011F048 "Default"... */
extern char opt_controls_desc[][16]; /* 0x8011DD20 per-style desc */
extern s32 rune_idx_table[13];       /* 0x8011F9F8 */
extern CTLLABEL ctl_label_pos[16];   /* 0x8011FA38 */
extern CTLBLIT ctl_blits[3];         /* 0x8011FAF8 */

extern s32 crystal_order[];           /* boss/legend-item id table (hint order) */

/* player records (shared, world/save system) */
extern u8 gPlayers[];            /* gPlayerRecords[4], stride 0x335C */
#define PREC_STRIDE 0x335C
#define PREC(i, off, T) (*(T*)(gPlayers + (i) * PREC_STRIDE + (off)))

/* pad state (controls TU) */
extern u32 lbl_80240E34[];  /* per-player, stride 0xF words */
extern u32 lbl_80240E38[];
extern s32 lbl_80240E5C[];  /* per-player control style */
extern s32 lbl_80240E60[];  /* per-pad setting (radio menu 0xF) */
extern u32 lbl_80240E64[];  /* per-pad boolean (radio menu 0x10) */
extern u32 lbl_80240E68[];  /* per-pad boolean (radio menu 0x11) */
extern u32 lbl_80240FB0;    /* global buttons held */
extern u32 lbl_80240FC0;    /* global buttons pressed */
extern u32 lbl_8034461C;    /* any-pad pressed mask */
extern u32 lbl_80344620;    /* any-pad held mask */

/* gamemain / world state */
extern s32 gGameMode;    /* game state (0x8009 in-game, 0x4010 tower) */
extern s32 lbl_803447B8;
extern u32 sFlags;    /* pause/movie flags */
extern s32 lbl_803445D8;
extern s32 lbl_80344578;    /* vb_elapsed */
extern s32 sLastWorldLevel;
extern s32 lbl_80344824;    /* active-player mask */
extern s32 sWorldDataConst;
extern s32 sMusicTrackHi;
extern s32 lbl_80344A44;    /* fullscreen-inventory mode */
extern u32 sPowerupsBuf;    /* icon tint */
extern s32 gDrawTextY;    /* hint text line dy (auxscreen) */
extern s32 gLineSpacing;    /* auxscreen text dy override */
extern s32 lbl_803443E4;    /* auxscreen font override */
extern f32 lbl_80343BC8;    /* highlight pulse amplitude */
extern void* gWinGlobals;  /* current window/camera set */

/* HUD arrow/button blit ids (shared) */
extern s32 lbl_80344E2C;
extern s32 lbl_80344E30;
extern s32 lbl_80344E34;
extern s32 lbl_80344E38;
extern s32 lbl_80344E3C;
extern s32 lbl_80344E44;
extern s32 lbl_80344E48;

/* ------------------------------------------------------------------ */
/* extern functions                                                    */
/* ------------------------------------------------------------------ */

/* mb blit/text library */
extern void* MBOX_FindTexture(char* name, s32* out);
extern void* MBNewTempBlit(s32 tex, s32 x, s32 y, s32 w, s32 h);
extern void* MBNewBlit(char* name, s32 x, s32 y);
extern void* mbNewBlitSized(char* name, s32 x, s32 y, s32 w, s32 h);
extern void* MBRemoveBlit(void* blit);
extern void mbBlitInit3414(void* blit, s32 hide);
extern void mbBlitCalcWidth(void* blit, s32 x, s32 y, f32 z);
extern void mbBlitProject(void* blit, s32 w, s32 h);
extern void mbBlitCvtCoord(void* blit, f32 z);
extern void MBBlitSetAlpha(void* blit, s32 alpha);
extern s32 MBBlitGetTex(void* blit);
extern void mbInitBlitEntry(void* blit, s32 frames, s32 frame);
extern s32 MBFontStringWidth(char* text);
extern void MBSetFont(s32 font);
extern void MBSetFontColor(u32 rgb);
extern s32 MBSetFontFlags(s32 flags);
extern void MBSetFontAlpha(s32 alpha);
extern void MBSetFontScale(f32 sx, f32 sy);
extern void MBSetFontScaleSpace(f32 sx, f32 sy);
extern void* MBDrawText(s32 x, s32 y, char* text);
extern void MBWorldToScreen3D(f32* pos, f32* out);

/* scene node helpers */
extern void* MBNewNode(f32 a, f32* b, s32 c);
extern void MBTreeClearFlags(void* node, s32 a, s32 b);
extern void* MBTreeSetFlags(void* node, s32 a, s32 b);
extern void* MBRemoveNode(void* node, s32 a);
extern void MBNodeSetParent(void* node, void* parent);
extern void* AtreeMatch(void* tree, u32 rgb, char* name, s32 d);
extern void* fn_80012F78(void* tree, void* node, void* msg, s32 a, s32 b);
extern void AtreeDelete(void* msg);
extern void fn_80011104(void* msg, s32 a, s32 b, s32 x, s32 y);
extern void CopyMat3(f32* dst, f32* src);
extern void* PitchMat3(void);
extern s32 StartFireScroll(char* name, s32 a, s32 x, s32 y, s32 w, s32 h, s32 e, f32 z);
extern void MBBlitOrder(s32 scroll, void* blit);

/* text (auxscreen/btext) */
extern s32 FontHeight(f32 scale, s32 font);
extern void* DrawNormalText(f32 scale, s32 x, s32 y, s32 font, u32 rgb, char* text);
extern s32 DrawTextKeepScale(f32 scale, char* text, s32 font);
extern void SetDrawStringScale(f32 scale);
extern void RestoreDrawStringScale(void);
extern void DrawStringText(s32 x, s32 y, s32 font, u32 rgb, s32 msg, s32 line);
extern void DrawScrollListText(s32 a, u32 always, s32 x, s32 y, s32 font, u32 rgb, s32 sub, s32 msg, s32 line);
extern char* GetScrollText(s32 a, s32 list, s32 msg, void* d);
extern s32 GetStringListMsg(s32 list, s32 msg);
extern s32 GetScrollListMsg(s32 a, s32 list, s32 msg);
extern void ScrollTextListNum(s32 a, s32 list);
extern s32 StringTextWidth(f32 scale, s32 msg, s32 line);
extern s32 StringTextHeight(f32 scale, s32 msg, s32 line, s32 b);

/* audio */
extern void AudioSetVolMusic(s32 vol);
extern void AudioSetVolSfx(s32 vol);
extern void AudioSetEnabled(s32 mode);
extern void AudioClampMusicVol(f32 vol, f32 b);
extern void AudioSetEvt1(s32 vol);
extern void AudioSelectReset(void);
extern void OSSetSoundMode(s32 stereo);

/* menu sounds (sounds_evt) */
extern void fn_8009D350(void);
extern void AudioMenuExit(void);
extern void AudioCursorSelect(void);
extern void AudioCursorH(void);
extern void AudioCursorV(void);
extern void AudioBuzzer(void);
extern void fn_8009EE2C(s32 which);

/* misc game systems */
extern void FatalError(char* text, s32 code);
extern int sprintf(char* buf, const char* fmt, ...);
extern void fn_800C25F0(s32 dx, s32 dy);       /* pb screen offset */
extern s32 ServeFireScroll(void);                  /* screensaver poll */
extern s32 FireScrollActive(void);                  /* screensaver active */
extern void ticks_for_firescroll(void);                 /* screensaver kick */
extern void draw_panels(void);                 /* fullscreen inventory draw */
extern void draw_fullscreen_inventory(void);
extern void controls_remove_active_player(s32 player);
extern void ClearAllPlayerControls(s32 a);
extern void player_save_controls(s32 player);           /* player vibe test */
extern void init_player_select(s32 mode);
extern void init_shop(s32 mode);
extern s32 WorldOpen(s32 world);
extern s32 towerAllPlayersMetLevelReq(s32 lvl);
extern s32 towerGetRuneNearStat(s32 player, s32 world);
extern s32 PlayerHasRune(s32 player, s32 rune);
extern s32 PlayerHasShard(s32 player, s32 shard);
extern void SumnerHintsActivate(void);

/* ------------------------------------------------------------------ */
/* this TU's .sdata (values from DOL 0x80343D00; unclaimed until flip)  */
/* ------------------------------------------------------------------ */

extern s32 OPTMENU_FADE;           /* 0x80343D00 = 40 */
extern s32 OPTMENU_FADE_HOLD;      /* 0x80343D04 = 5 */
extern s32 OPTMENU_FONT;           /* 0x80343D08 = 6 */
extern s32 OPTMENU_MARGIN_X;       /* 0x80343D0C = 64 */
extern s32 OPTMENU_MARGIN_Y;       /* 0x80343D10 = 64 */
extern s32 OPTMENU_MARGIN_TITLE;   /* 0x80343D14 = 58 */
extern s32 OPTMENU_MARGIN_PLAYER;  /* 0x80343D18 = 34 */
extern f32 OPTMENU_SCROLLZ;        /* 0x80343D1C = 255600.0f */
extern s32 OPTMENU_ICON_TIME;      /* 0x80343D20 = 15 */
extern s32 general_hint_index;     /* 0x80343D24 = -1 */
extern s32 boss_hint_index;        /* 0x80343D28 = -1 */
extern s32 legend_hint_index;      /* 0x80343D2C = -1 */
extern s32 rune_hint_index;        /* 0x80343D30 = -1 */
extern s32 options_level;          /* 0x80343D34 = -1 */
extern s32 OPTMENU_FINISH_FRAMES;  /* 0x80343D38 = 30 */
extern s32 optmenu_choice_twice;   /* 0x80343D3C = 1 */
extern f32 optmenu_icon_z;         /* 0x80343D40 = 1.1f */
extern f32 msg_scale;              /* 0x80343D44 = 0.667f */
extern f32 optplyr_scale;          /* 0x80343D48 = 0.667f */
extern s32 slider_dxl;             /* 0x80343D4C = -52 */
extern s32 slider_dxr;             /* 0x80343D50 = -24 */
extern s32 OPTAUDIO_VOL_WIDTH;     /* 0x80343D54 = 264 */
extern s32 OPTAUDIO_VOL_HEIGHT;    /* 0x80343D58 = 64 */
extern s32 OPTCTL_FONT;            /* 0x80343D5C = -1 */
extern f32 OPTCTL_SCALE;           /* 0x80343D60 = 1.0f */
extern s32 OPTCTL_DY;              /* 0x80343D64 = 2 */

/* ------------------------------------------------------------------ */
/* this TU's .sbss 0x80344A98.. (extern until claimed)                  */
/* ------------------------------------------------------------------ */

extern s32 options_state;          /* 0x80344A98 current menu state (0=closed,
                                      1=finishing, else cur menu->state) */
extern s32 options_lockout;        /* 0x80344A9C blocks auto-open */
extern s32 optmenu_nochoice;       /* 0x80344AA0 input blocked latch */
extern s32 vb_elapsed_menu;        /* 0x80344AA4 */
extern s32 screen_dx;              /* 0x80344AA8 live screen offset */
extern s32 screen_dy;              /* 0x80344AAC */
extern s32 control_style;          /* 0x80344AB0 style being edited */
extern s32 OPTMENU_SHADOW;         /* 0x80344AB4 */
extern s32 OPTMSG_SHADOW;          /* 0x80344AB8 */
extern s32 hint_submenu;           /* 0x80344ABC current hint code 0x27..0x2A */
extern s32 general_hint_num;       /* 0x80344AC0 */
extern s32 boss_hint_pass;         /* 0x80344AC4 */
extern s32 boss_hint_num;          /* 0x80344AC8 */
extern s32 legend_hint_pass;       /* 0x80344ACC */
extern s32 legend_hint_num;        /* 0x80344AD0 */
extern s32 rune_hint_pass;         /* 0x80344AD4 */
extern s32 rune_hint_num;          /* 0x80344AD8 */
extern s32 sfx_sound_count;        /* 0x80344ADC slider test-sound timer */
extern char optionsStringPool[];   /* 0x80113830 OPTIONS rodata anchor */
extern s32 OPTMENU_VOL_DY;         /* 0x80344AE0 */
extern s32 optmenu_abortall;       /* 0x80344AE4 */
extern s32 title_choice;           /* 0x80344AE8 title selection latch */
extern s32 hints_inited;           /* 0x80344AEC */
extern s32 opt_restart_request;    /* 0x80344AF0 (menu state 0x14 exit) */
extern s32 opt_quit_request;       /* 0x80344AF4 (menu state 0x13 exit) */
extern s32 opt_force_player;       /* 0x80344AF8 */

/* ------------------------------------------------------------------ */
/* this TU's .bss 0x80274E00..0x80274EA0: ONE aggregate global.        */
/* PROOF: init_prefs materializes 0x80274E00 once (lis/addi) and       */
/* stores the prefs at +128..+156 -- separate globals (commons or      */
/* statics) each get their own HA/LO reloc pair under every flag       */
/* variant tested, so the original must be a single struct.            */
/* ------------------------------------------------------------------ */

typedef struct OPTGLOBALS {
    char tbuf[0x40];        /* 0x00 "Player %d" scratch */
    OPTMENU* stack[4];      /* 0x40 options_stack */
    AUDIOSLIDER music;      /* 0x50 music slider (live) */
    AUDIOSLIDER sfx;        /* 0x68 sfx slider (live) */
    s32 sfx_vol;            /* 0x80 pref: sfx volume */
    s32 music_vol;          /* 0x84 pref: music volume */
    s32 sound_mode;         /* 0x88 pref: 1 = stereo */
    s32 screen_dx;          /* 0x8C pref: saved screen offset */
    s32 screen_dy;          /* 0x90 */
    s32 style;              /* 0x94 pref */
    s32 vibration;          /* 0x98 pref */
    s32 subtitles;          /* 0x9C pref */
} OPTGLOBALS;

OPTGLOBALS optglobals;      /* 0x80274E00 */

/* ------------------------------------------------------------------ */
/* forward decls (source order = GC emission order)                    */
/* ------------------------------------------------------------------ */

s32 OptionsStart(s32 player, s32 b);
static void do_screenmenu(void);
static void do_controlsmenu(OPTMENU* m, s32 player);
static void do_audiomenu(OPTMENU* m);
static void position_audioslider(AUDIOSLIDER* s, s32 x, s32 y, s32 w, s32 h, s32 active);
static void start_audioslider(AUDIOSLIDER* s);
s32 do_optmenu(OPTMENU* m, s32 allowNav);
void show_optmenu(OPTMENU* m);
static void end_optmenu(s32 dir, s32 mode);
static s32 finish_optmenu(OPTMENU* m, s32 force);
void remove_optmenu(OPTMENU* m);
void end_all_optmenus(void);
static void start_optmenu(OPTMENU* m, s32 sel);
void start_optmenu_nostack(OPTMENU* m, s32 sel);
static void next_rune_hint(s32 advance);
static void next_legend_hint(s32 advance);
static void next_boss_hint(s32 advance);
static void next_general_hint(s32 advance);

/* the remove_optmenu blit/message cleanup, written out inline in
 * end_optmenu/finish_optmenu in the original (remove_optmenu is defined
 * AFTER them, so this cannot be compiler inlining) */
#define REMOVE_OPTMENU(m)                                  \
    do {                                                   \
        AtreeDelete((m)->msg);                             \
        if ((m)->icon_node != NULL) {                      \
            (m)->icon_node = MBRemoveNode((m)->icon_node, 1); \
        }                                                  \
        if ((m)->title_blit != NULL) {                     \
            (m)->title_blit = MBRemoveBlit((m)->title_blit); \
        }                                                  \
        if ((m)->burn_blit != NULL) {                      \
            (m)->burn_blit = MBRemoveBlit((m)->burn_blit); \
        }                                                  \
        (m)->active = 0;                                   \
        (m)->finish_timer = 0;                             \
    } while (0)

/* ================================================================== */
/* 0x80070A60 HintMenu                                                 */
/* ================================================================== */

s32 HintMenu(s32 type)
{
    if (type < 0) {
        hints_inited = 0;
        return 0;
    }
    if (options_state != 0) {
        return 0;
    }
    if (hints_inited == 0) {
        hints_inited = 1;
        next_general_hint(0);
    }
    next_boss_hint(0);
    next_legend_hint(0);
    next_rune_hint(0);
    optmenu_nochoice = 0;
    start_optmenu(&hintmenu_menu, type);
    return 1;
}

/* ================================================================== */
/* 0x80070B00 TitleMenuEnd                                             */
/* ================================================================== */

void TitleMenuEnd(void)
{
    if (options_state != 0) {
        end_optmenu(-1, 1);
    }
    optmenu_abortall = 0;
}

/* ================================================================== */
/* 0x80070B3C TitleMenu                                                */
/* ================================================================== */

s32 TitleMenu(s32 y)
{
    s32 ret = 0;

    titlemenu_menu.y = y;
    if (optmenu_abortall != 0 && options_state == 0) {
        ret = 2;
    } else if (options_state == 2) {
        if (title_choice != 0) {
            title_choice = 0;
            ret = 1;
        }
    } else if (options_state != 0) {
        ret = 1;
    }
    return ret;
}

/* ================================================================== */
/* 0x80070BA4 TitleMenuInit                                            */
/* ================================================================== */

void TitleMenuInit(s32 sel)
{
    optmenu_nochoice = 0;
    start_optmenu(&titlemenu_menu, sel);
}

/* ================================================================== */
/* 0x80070BD8 DeleteOptionBlits                                        */
/* ================================================================== */

void DeleteOptionBlits(void)
{
}

/* ================================================================== */
/* 0x80070BDC OptionsDone                                              */
/* ================================================================== */

s32 OptionsDone(void)
{
    if (FireScrollActive() != 0) {
        return 0;
    }
    if (options_state != 0) {
        return 0;
    }
    return 1;
}

/* ================================================================== */
/* 0x80070C24 DoOptions -- per-frame master driver                     */
/* ================================================================== */

int DoOptions(void)
{
    OPTMENU* m;
    OPTITEM* item;
    s32 choice;
    s32 player;
    s32 i;
    s32 saver;
    s32 skipBackSound;

    choice = 0;
    skipBackSound = 0;

    /* menu clock: full speed, or (paused w/ button held) 2, else 0 */
    if ((sFlags & 8) == 0) {
        vb_elapsed_menu = lbl_80344578;
    } else if ((lbl_80240FB0 & 0x2000000) == 0 && (lbl_80240FC0 & 0x1000000) == 0) {
        vb_elapsed_menu = 0;
    } else {
        vb_elapsed_menu = 2;
    }

    saver = ServeFireScroll();

    /* auto-open: any active player pressing START */
    if (options_state == 0 && options_lockout == 0 && opt_force_player < 0 &&
        (gGameMode != 0x4010 || lbl_803447B8 == 0)) {
        for (i = 0; i < 4; i++) {
            if (PREC(i, 0xE8, s32) == 1 && (lbl_80240E38[i * 0xF] & 0x40000) != 0 &&
                OptionsStart(i, 0) != 0) {
                break;
            }
        }
    }

    if (lbl_803445D8 != 0 &&
        ((gGameMode != 0x4010 && gGameMode != 0x400C) || options_state != 0)) {
        ticks_for_firescroll();
    }

    if (options_state == 0) {
        return saver;
    }
    if (options_state == 1) {
        return finish_optmenu(optglobals.stack[0], 0);
    }

    /* let covered levels finish fading */
    if (options_level > 0 && optglobals.stack[options_level - 1] != NULL) {
        finish_optmenu(optglobals.stack[options_level - 1], 0);
    }
    if (options_level < 3 && optglobals.stack[options_level + 1] != NULL) {
        finish_optmenu(optglobals.stack[options_level + 1], 0);
    }

    m = optglobals.stack[options_level];
    player = m->player;

    /* fullscreen inventory view */
    if (lbl_80344A44 != 0) {
        draw_panels();
        choice = do_optmenu(m, 0);
        if (m->icon_node != NULL) {
            MBTreeSetFlags(m->icon_node, 2, 0);
        }
        if (m->burn_blit != NULL) {
            mbBlitInit3414(m->burn_blit, 1);
        }
        if (m->title_blit != NULL) {
            mbBlitInit3414(m->title_blit, 1);
        }
        if ((u32)(choice + 2) < 2 || choice == 0xF) {
            draw_fullscreen_inventory();
        }
        return 1;
    }

    if (options_state != 2 || saver == 0) {
        show_optmenu(m);
        choice = do_optmenu(m, 1);
    }

    item = &m->items[m->sel];
    AudioClampMusicVol(0.1f, -1.0f);

    switch (options_state) {
    case 2: /* title menu */
        if ((sFlags & 4) != 0) {
            choice = 0xB;
        }
        if (choice == 1) {
            title_choice = 1;
        } else if (choice < 1) {
            if (choice < 0 && choice > -3) {
                start_optmenu(NULL, player);
                controls_remove_active_player(player);
                return 1;
            }
        } else if (choice == 0xC) {
            AudioCursorSelect();
            start_optmenu(&optmenu_game, player);
        } else if (choice < 0xC && choice > 0xA) {
            optmenu_abortall = 1;
            end_optmenu(-1, 0);
        }
        break;

    case 3: /* in-game top menu */
        if (choice == 0xF) {
            AudioCursorSelect();
            while (options_level >= 0) {
                end_optmenu(-1, 1);
            }
            init_shop(2);
        } else if (choice < 0xF) {
            if (choice == 0xD) {
                while (options_level >= 0) {
                    end_optmenu(-1, 1);
                }
                init_player_select(1);
                if (player >= 0) {
                    PREC(player, 0xE8, s32) = 2;
                    PREC(player, 0x3338, s32) = 1;
                }
            } else if (choice < 0xD) {
                if (choice > 0xB) {
                    start_optmenu(&optmenu_yesno, player);
                }
            } else {
                while (options_level >= 0) {
                    end_optmenu(-1, 1);
                }
                AudioCursorSelect();
                init_shop(1);
            }
        } else if (choice == 0x25) {
            start_optmenu(&optmenu_yesno3, player);
            optmenu_yesno3.sel = 0;
        }
        break;

    case 4: /* quit-confirm */
        if (choice == 0x26) {
            start_optmenu(&optmenu_yesno2, player);
            optmenu_yesno2.sel = 0;
        } else if (choice < 0x26 && choice == 0xC) {
            start_optmenu(&optmenu_options, player);
        }
        break;

    case 5:
    case 6:
    case 7: /* options submenu */
        if (choice == 0x14) {
            start_optmenu(&optmenu_vibration, player);
            optmenu_vibration.sel = optglobals.vibration;
        } else if (choice < 0x14) {
            if (choice == 0x11) {
                start_optmenu(&optmenu_audio, player);
                optglobals.music.val = optglobals.music_vol;
                optglobals.sfx.val = optglobals.sfx_vol;
                start_audioslider(&optglobals.music);
                start_audioslider(&optglobals.sfx);
                sfx_sound_count = 0;
            } else if (choice < 0x11 && choice > 0xF) {
                start_optmenu(&optmenu_audio_mode, player);
            }
        } else if (choice < 0x16) {
            start_optmenu(&optmenu_screen_sub, player);
        }
        break;

    case 8: { /* audio sliders */
        s32 step = 0;
        AudioClampMusicVol(0.0f, 1.0f);
        do_audiomenu(m);

        switch (choice) {
        default:
            if (item->code == 0x18 && sfx_sound_count > 0x3C) {
                fn_8009EE2C(0);
                sfx_sound_count = 0;
            }
            break;
        case 3:
        case 5:
            step = -2;
            /* fallthrough */
        case 4:
        case 6:
            if (item->code == 0x18) {
                optglobals.sfx.val += (step + 1) * vb_elapsed_menu;
                if (optglobals.sfx.val < 0) {
                    optglobals.sfx.val = 0;
                } else if (optglobals.sfx.val > 0xFF) {
                    optglobals.sfx.val = 0xFF;
                }
                if (optglobals.sfx_vol == optglobals.sfx.val) {
                    if (sfx_sound_count > 0xF) {
                        fn_8009EE2C(1);
                        sfx_sound_count = 0;
                    }
                } else {
                    AudioSetVolSfx(optglobals.sfx.val);
                    optglobals.sfx_vol = optglobals.sfx.val;
                }
            } else if (item->code < 0x18) {
                if (item->code > 0x16) {
                    optglobals.music.val += (step + 1) * vb_elapsed_menu;
                    if (optglobals.music.val < 0) {
                        optglobals.music.val = 0;
                    } else if (optglobals.music.val > 0xFF) {
                        optglobals.music.val = 0xFF;
                    }
                    AudioSetVolMusic(optglobals.music.val);
                    AudioSetEvt1(0x80);
                    optglobals.music_vol = optglobals.music.val;
                }
            } else if (item->code < 0x1A && (u32)(choice - 3) < 2) {
                AudioCursorH();
                item->value ^= 1;
                optglobals.sound_mode = item->value;
                OSSetSoundMode(optglobals.sound_mode == 1);
                AudioSetEnabled(optglobals.sound_mode);
            }
            break;
        case 0x17:
        case 0x18:
        case 0x19:
            break;
        case -2:
        case -1:
            optglobals.music.ml = MBRemoveBlit(optglobals.music.ml);
            optglobals.music.pink = MBRemoveBlit(optglobals.music.pink);
            optglobals.music.empty = MBRemoveBlit(optglobals.music.empty);
            optglobals.music.mr = MBRemoveBlit(optglobals.music.mr);
            optglobals.music.slid = MBRemoveBlit(optglobals.music.slid);
            optglobals.sfx.empty = MBRemoveBlit(optglobals.sfx.empty);
            optglobals.sfx.pink = MBRemoveBlit(optglobals.sfx.pink);
            optglobals.sfx.ml = MBRemoveBlit(optglobals.sfx.ml);
            optglobals.sfx.mr = MBRemoveBlit(optglobals.sfx.mr);
            optglobals.sfx.slid = MBRemoveBlit(optglobals.sfx.slid);
            fn_8009D350();
            skipBackSound = 1;
            break;
        }
        break;
    }

    case 9: /* more prefs */
        if (choice == 0x13) {
            start_optmenu(&optmenu_pref_c, player);
            optmenu_pref_c.sel = optglobals.subtitles;
        } else if (choice < 0x13 && choice > 0x11) {
            start_optmenu(&optmenu_pref_b, player);
            optmenu_pref_b.sel = optglobals.style;
        }
        break;

    case 10: /* radio menu on optglobals.style */
        for (i = 0; i < m->num_items; i++) {
            m->items[i].on = (i == optglobals.style) ? 1 : 0;
        }
        if (choice > 0x1B && choice < 0x1F) {
            optglobals.style = m->sel;
            fn_8009D350();
        }
        break;

    case 0xB: /* radio menu on optglobals.subtitles */
        for (i = 0; i < m->num_items; i++) {
            m->items[i].on = (i == optglobals.subtitles) ? 1 : 0;
        }
        if (choice > 0x1B && choice < 0x1F) {
            optglobals.subtitles = m->sel;
            fn_8009D350();
        }
        break;

    case 0xC: /* vibration on/off */
        for (i = 0; i < m->num_items; i++) {
            m->items[i].on = (i == optglobals.vibration) ? 1 : 0;
        }
        if (choice == 0x1F) {
            optglobals.vibration = 1;
            fn_8009D350();
        } else if (choice > 0x1E && choice < 0x21) {
            optglobals.vibration = 0;
            fn_8009D350();
        }
        break;

    case 0xD: /* controls hub */
        if (choice == 0x22) {
            start_optmenu(&optmenu_screen, player);
        } else if (choice < 0x22) {
            if (choice == -1) {
                player_save_controls(player);
            } else if (choice > -2 && choice > 0x20) {
                control_style = lbl_80240E5C[player * 0xF];
                start_optmenu(&optmenu_controls, player);
                for (i = 0; i < 3; i++) {
                    ctl_blits[i].blit = MBNewBlit(ctl_blits[i].name, ctl_blits[i].x, ctl_blits[i].y);
                    mbBlitProject(ctl_blits[i].blit, ctl_blits[i].w, ctl_blits[i].h);
                }
                fn_8009D350();
            }
        } else if (choice == 0x24) {
            start_optmenu(&optmenu_sub3, player);
        } else if (choice < 0x24) {
            start_optmenu(&optmenu_sub2, player);
        }
        break;

    case 0xE: { /* controller diagram / style picker */
        s32 add = 0;
        if (player >= 0) {
            m->items[0].text = opt_style_names[control_style];
            do_controlsmenu(m, player);
        }
        if (choice == 3) {
            add = -2;
            goto style_step;
        } else if (choice < 3) {
            if (choice == -1) {
                choice = 0;
                break;
            }
            if (choice != -2) {
                break;
            }
            goto style_accept;
        } else if (choice != 0x21) {
            if (choice > 0x20 || choice > 4) {
                break;
            }
        style_step:
            if (item->code == 0x21 && player >= 0) {
                add += control_style;
                control_style = add + 1;
                if (control_style < 0) {
                    control_style = add + 4;
                }
                control_style = control_style % 3;
                AudioCursorH();
            }
            break;
        }
    style_accept:
        lbl_80240E5C[player * 0xF] = control_style;
        for (i = 0; i < 3; i++) {
            if (ctl_blits[i].blit != NULL) {
                ctl_blits[i].blit = MBRemoveBlit(ctl_blits[i].blit);
            }
        }
        fn_8009D350();
        start_optmenu(NULL, player);
        break;
    }

    case 0xF: /* per-pad setting radio (pad + 0x2C) */
        for (i = 0; i < m->num_items; i++) {
            m->items[i].on = (i == lbl_80240E60[player * 0xF]) ? 1 : 0;
        }
        if (choice != 0x1B) {
            if (choice < 0x1B) {
                if (choice < 0x1A) {
                    break;
                }
            } else if (choice > 0x1E) {
                break;
            }
            lbl_80240E60[player * 0xF] = m->sel;
            fn_8009D350();
        }
        break;

    case 0x10: /* per-pad boolean radio (pad + 0x30; 0 = first item) */
        for (i = 0; i < m->num_items; i++) {
            if (i == (lbl_80240E64[player * 0xF] == 0)) {
                m->items[i].on = 1;
            } else {
                m->items[i].on = 0;
            }
        }
        if (choice < 0x1C && choice > 0x19) {
            lbl_80240E64[player * 0xF] = (m->sel == 0);
            fn_8009D350();
        }
        break;

    case 0x11: /* per-pad boolean radio (pad + 0x34) */
        for (i = 0; i < m->num_items; i++) {
            if (i == (lbl_80240E68[player * 0xF] == 0)) {
                m->items[i].on = 1;
            } else {
                m->items[i].on = 0;
            }
        }
        if (choice < 0x1C && choice > 0x19) {
            lbl_80240E68[player * 0xF] = (m->sel == 0);
            fn_8009D350();
        }
        break;

    case 0x12: { /* screen position */
        s32 step2 = 0;
        do_screenmenu();
        switch (choice) {
        case 2:
            screen_dx = 0;
            screen_dy = 0;
            fn_800C25F0(0, 0);
            break;
        case 3:
        case 5:
            step2 = -2;
            /* fallthrough */
        case 4:
        case 6:
            screen_dx += (step2 + 1) * 4;
            if (screen_dx < -0x80) {
                screen_dx = -0x80;
            } else if (screen_dx > 0x80) {
                screen_dx = 0x80;
            }
            fn_800C25F0(screen_dx, screen_dy);
            break;
        case 7:
        case 9:
            step2 = -2;
            /* fallthrough */
        case 8:
        case 10:
            screen_dy += step2 + 1;
            if (screen_dy < -0x28) {
                screen_dy = -0x28;
            } else if (screen_dy > 0x28) {
                screen_dy = 0x28;
            }
            fn_800C25F0(screen_dx, screen_dy);
            break;
        case -2:
        case -1:
            fn_800C25F0(optglobals.screen_dx, optglobals.screen_dy);
            break;
        }
        break;
    }

    case 0x13:
        if (choice == 0x25) {
            opt_quit_request = 1;
            end_optmenu(-1, -1);
        }
        break;

    case 0x14:
        if (choice == 0x26) {
            opt_restart_request = 1;
            end_optmenu(-1, -1);
        }
        break;

    case 0x16: /* hint category select */
        hint_submenu = choice;
        if (choice == 0x28) {
            next_boss_hint(1);
            hintmenu_display.title = GetScrollText(1, 0xC, boss_hint_index, NULL);
            ScrollTextListNum(1, 0);
            start_optmenu(&hintmenu_display, player);
        } else if (choice < 0x28) {
            if (choice < 0) {
                if (choice > -3) {
                    SumnerHintsActivate();
                }
            } else if (choice > 0x26) {
                next_general_hint(1);
                hintmenu_display.title = "A Hint for You";
                ScrollTextListNum(1, 3);
                start_optmenu(&hintmenu_display, player);
            }
        } else if (choice == 0x2A) {
            next_rune_hint(1);
            hintmenu_display.title = GetScrollText(1, 0x1B, rune_idx_table[rune_hint_index] - 1, NULL);
            ScrollTextListNum(1, 1);
            start_optmenu(&hintmenu_display, player);
        } else if (choice < 0x2A) {
            next_legend_hint(1);
            hintmenu_display.title = GetScrollText(1, 0x28, legend_hint_index, NULL);
            ScrollTextListNum(1, 2);
            start_optmenu(&hintmenu_display, player);
        }
        break;

    case 0x17: { /* hint display */
        u32 rgb = (m->rgb_off[2] & 0xFF) | ((m->rgb_off[0] & 0xFF) << 16) | ((m->rgb_off[1] & 0xFF) << 8);
        s32 y;
        SetDrawStringScale(m->scale);
        if (hint_submenu == 0x29) {
            y = 0x70;
            for (i = 0; i < legend_hint_pass; i++) {
                DrawScrollListText(1, 0xFFFFFF00, y, 0, 6, rgb, 2, legend_hint_index, i);
                y = gDrawTextY + 4;
            }
        } else if (hint_submenu < 0x29) {
            if (hint_submenu == 0x27) {
                DrawScrollListText(1, 0xFFFFFF00, -hintmenu_display.y, 0, 6, rgb, 3,
                                   general_hint_index, general_hint_num);
            } else if (hint_submenu > 0x26) {
                y = 0x70;
                for (i = 0; i < boss_hint_pass; i++) {
                    DrawScrollListText(1, 0xFFFFFF00, y, 0, 6, rgb, 0, boss_hint_index, i);
                    y = gDrawTextY + 4;
                }
            }
        } else if (hint_submenu < 0x2B) {
            s32 rune = rune_idx_table[rune_hint_index];
            y = 0x70;
            for (i = 0; i < rune_hint_pass; i++) {
                DrawScrollListText(1, 0xFFFFFF00, y, 0, 6, rgb, 1, rune - 1, i);
                y = gDrawTextY;
            }
        }
        RestoreDrawStringScale();
        break;
    }
    }

    /* back out */
    if ((u32)(choice + 2) < 2) {
        start_optmenu(NULL, player);
        if (skipBackSound == 0) {
            AudioMenuExit();
        }
    }
    return 1;
}

/* ================================================================== */
/* 0x80071E1C OptionsStart                                             */
/* ================================================================== */

s32 OptionsStart(s32 player, s32 b)
{
    s32 i;
    OPTITEM* it;

    if (options_state != 0) {
        return options_state;
    }
    optmenu_nochoice = 0;
    switch (gGameMode) {
    case 0x8009:
        start_optmenu(&optmenu_game, player);
        break;
    case 0x4010:
        if (sLastWorldLevel == sWorldDataConst) {
            start_optmenu(&optmenu_tower, player);
        } else {
            start_optmenu(&optmenu_tower_hints, player);
            for (i = 0;; i++) {
                it = &optmenu_tower_hints.items[i];
                if (it->text == NULL) {
                    break;
                }
                if (it->code == 0x26) {
                    if (sMusicTrackHi == 0xC) {
                        it->value = -1;
                    } else {
                        it->value = 0;
                    }
                }
            }
        }
        AudioSelectReset();
        break;
    }
    if (options_state != 0) {
        AudioCursorSelect();
    }
    return options_state;
}

/* ================================================================== */
/* 0x80071F20 do_screenmenu                                            */
/* ================================================================== */

#pragma push
#pragma opt_propagation off
static void do_screenmenu(void)
{
    s32 tex;
    void* blit;
    s32 w;
    s32 h;
    s32 sz;
    s32 y1;
    s32 x1;
    s32 y2;
    s32 x2;
    s32 w2;
    s32 hw;
    f32 one;
    u8 unused[64];

    /* #pragma opt_propagation off reproduces the target's runtime
     * 511-w / 383-h / sz/2 arithmetic (subfic/srawi) and register-cached
     * blit z (melee precedent for shipped scoped pragmas). */
    w = 0x10;
    h = 2;
    one = 1.0f;
    tex = (s32)MBOX_FindTexture("AAAWHITE", NULL);

    blit = MBNewTempBlit(tex, 1, 1, w, h);
    if (blit != NULL) {
        mbBlitCvtCoord(blit, one);
    }
    blit = MBNewTempBlit(tex, 1, 1, h, w);
    if (blit != NULL) {
        mbBlitCvtCoord(blit, one);
    }
    x1 = 511 - w;
    blit = MBNewTempBlit(tex, x1, 1, w, h);
    if (blit != NULL) {
        mbBlitCvtCoord(blit, one);
    }
    x2 = 511 - h;
    blit = MBNewTempBlit(tex, x2, 1, h, w);
    if (blit != NULL) {
        mbBlitCvtCoord(blit, one);
    }
    y1 = 383 - h;
    blit = MBNewTempBlit(tex, 1, y1, w, h);
    if (blit != NULL) {
        mbBlitCvtCoord(blit, one);
    }
    y2 = 383 - w;
    blit = MBNewTempBlit(tex, 1, y2, h, w);
    if (blit != NULL) {
        mbBlitCvtCoord(blit, one);
    }
    blit = MBNewTempBlit(tex, x1, y1, w, h);
    if (blit != NULL) {
        mbBlitCvtCoord(blit, one);
    }
    blit = MBNewTempBlit(tex, x2, y2, h, w);
    if (blit != NULL) {
        mbBlitCvtCoord(blit, one);
    }

    /* center cross */
    hw = h / 2;
    w2 = w * 2;
    blit = MBNewTempBlit(tex, 256 - w, 192 - hw, w2, h);
    if (blit != NULL) {
        mbBlitCvtCoord(blit, one);
    }
    blit = MBNewTempBlit(tex, 256 - hw, 192 - w, h, w2);
    if (blit != NULL) {
        mbBlitCvtCoord(blit, one);
    }

    /* d-pad arrows */
    sz = 0x20;
    w2 = sz / 2;
    hw = 256 - w2;
    blit = MBNewTempBlit(lbl_80344E30, hw, 0x90, sz, sz);
    if (blit != NULL) {
        mbBlitCvtCoord(blit, one);
    }
    blit = MBNewTempBlit(lbl_80344E38, (256 - sz) - w2, 0xB0, sz, sz);
    if (blit != NULL) {
        mbBlitCvtCoord(blit, one);
    }
    blit = MBNewTempBlit(lbl_80344E34, 288 - w2, 0xB0, sz, sz);
    if (blit != NULL) {
        mbBlitCvtCoord(blit, one);
    }
    blit = MBNewTempBlit(lbl_80344E2C, hw, 0xD0, sz, sz);
    if (blit != NULL) {
        mbBlitCvtCoord(blit, one);
    }
}
#pragma pop

/* ================================================================== */
/* 0x800721C8 do_controlsmenu                                          */
/* ================================================================== */

static void do_controlsmenu(OPTMENU* m, s32 player)
{
    s32 style;
    s32 i;
    s32 w;
    s32 h;
    s32 x;

    style = control_style;
    if (player >= 0) {
        for (i = 0; i < 3; i++) {
            mbBlitCalcWidth(ctl_blits[i].blit, ctl_blits[i].x, ctl_blits[i].y, -1.0f);
            mbBlitProject(ctl_blits[i].blit, ctl_blits[i].w, ctl_blits[i].h);
        }
        SetDrawStringScale(OPTCTL_SCALE);
        gLineSpacing = OPTCTL_DY;
        for (i = 0; i < 16; i++) {
            s32 msg = GetStringListMsg(3, style);
            w = StringTextWidth(OPTCTL_SCALE, msg, i);
            h = StringTextHeight(OPTCTL_SCALE, msg, i, -1);
            if (ctl_label_pos[i].mode == 1) {
                x = -(ctl_label_pos[i].dx + 0x100);
            } else if (ctl_label_pos[i].mode == 2) {
                x = -((ctl_label_pos[i].dx + 0x100) - w / 2);
            } else {
                x = -(ctl_label_pos[i].dx + w / 2 + 0x100);
            }
            DrawStringText(x, ctl_label_pos[i].dy - h / 2, OPTCTL_FONT,
                           (optmenu_rgb_ctls[2] & 0xFF) |
                           ((optmenu_rgb_ctls[0] & 0xFF) << 16) |
                           ((optmenu_rgb_ctls[1] & 0xFF) << 8),
                           msg, i);
        }
        RestoreDrawStringScale();
        gLineSpacing = 0;
    }
}

/* ================================================================== */
/* 0x8007234C do_audiomenu                                             */
/* ================================================================== */

static void do_audiomenu(OPTMENU* m)
{
    s32 i;
    s32 off;
    OPTITEM* it;
    s32 fh;
    s32* sm;
    s32 act;

    i = 0;
    off = 0;
    fh = FontHeight(m->scale, OPTMENU_FONT);
    sfx_sound_count += vb_elapsed_menu;
    sm = &optglobals.sound_mode;
    for (;; i++, off += 0x24) {
        it = (OPTITEM*)((u8*)m->items + off);
        if (it->text == NULL) {
            break;
        }
        act = (i == m->sel) ? 0 : 1;
        switch (it->code) {
        case 0x17:
            position_audioslider(&optglobals.music, m->x,
                                 it->draw_y + (fh + OPTMENU_VOL_DY),
                                 OPTAUDIO_VOL_WIDTH, OPTAUDIO_VOL_HEIGHT, act);
            break;
        case 0x18:
            position_audioslider(&optglobals.sfx, m->x,
                                 it->draw_y + (fh + OPTMENU_VOL_DY),
                                 OPTAUDIO_VOL_WIDTH, OPTAUDIO_VOL_HEIGHT, act);
            break;
        case 0x19:
            it->on = *sm + 1;
            it->value = *sm;
            break;
        }
    }
}

/* ================================================================== */
/* 0x80072470 position_audioslider                                     */
/* ================================================================== */

static void position_audioslider(AUDIOSLIDER* s, s32 x, s32 y, s32 w, s32 h, s32 active)
{
    s32 t;
    s32 fill;
    s32 alpha;
    u8 unused[8];

    t = (w * s->val) / 0xFF;
    fill = t;
    if (t < 1) {
        fill = 1;
    }
    mbBlitInit3414(s->ml, 0);
    mbBlitInit3414(s->mr, 0);
    mbBlitInit3414(s->empty, 0);
    mbBlitInit3414(s->pink, 0);
    mbBlitInit3414(s->slid, 0);

    mbBlitCalcWidth(s->ml, x + slider_dxl, y, -1.0f);
    mbBlitCalcWidth(s->mr, x + w + slider_dxr, y, -1.0f);
    mbBlitCalcWidth(s->empty, x, y + 0xB, -1.0f);
    mbBlitProject(s->empty, w, 0x20);
    mbBlitCalcWidth(s->pink, x, y + 0xF, -1.0f);
    mbBlitProject(s->pink, fill, 0x20);
    mbBlitCalcWidth(s->slid, x + fill - 0x14, y + 2, -1.0f);

    alpha = active * 100;
    MBBlitSetAlpha(s->ml, alpha);
    MBBlitSetAlpha(s->mr, alpha);
    MBBlitSetAlpha(s->empty, active * 0x96);
    MBBlitSetAlpha(s->pink, active * 0xA0);
    MBBlitSetAlpha(s->slid, alpha);
}

/* ================================================================== */
/* 0x800725D8 start_audioslider                                        */
/* ================================================================== */

static void start_audioslider(AUDIOSLIDER* s)
{
    char* strings = optionsStringPool;
    s->empty = MBNewBlit(strings + 520, 0, 0);
    s->pink = MBNewBlit(strings + 532, 0, 0);
    s->ml = MBNewBlit(strings + 544, 0, 0);
    s->mr = MBNewBlit(strings + 556, 0, 0);
    s->slid = MBNewBlit("slider", 0, 0);
    mbBlitInit3414(s->ml, 1);
    mbBlitInit3414(s->mr, 1);
    mbBlitInit3414(s->empty, 1);
    mbBlitInit3414(s->pink, 1);
    mbBlitInit3414(s->slid, 1);
}

/* ================================================================== */
/* 0x800726A8 do_optmenu                                               */
/* ================================================================== */

s32 do_optmenu(OPTMENU* m, s32 allowNav)
{
    s32 i;
    u32 pressed;
    u32 held;
    s32 code;

    /* menu clock (same policy as DoOptions) */
    if ((sFlags & 8) == 0) {
        vb_elapsed_menu = lbl_80344578;
    } else if ((lbl_80240FB0 & 0x2000000) == 0 && (lbl_80240FC0 & 0x1000000) == 0) {
        vb_elapsed_menu = 0;
    } else {
        vb_elapsed_menu = 2;
    }
    m->time += vb_elapsed_menu;

    pressed = lbl_8034461C;
    held = lbl_80344620;
    if (m->player >= 0) {
        pressed = lbl_80240E38[m->player * 0xF];
        held = lbl_80240E34[m->player * 0xF];
    }

    if (m->num_items == 0) {
        i = 0;
        for (code = 0; m->items[code].text != NULL; code++) {
            i++;
        }
        m->num_items = i;
    }
    while (m->items[m->sel].value < 0) {
        m->sel++;
    }
    if (m->sel >= m->num_items) {
        m->sel = 0;
    }

    if (m->player < 0 || (lbl_80344824 & (1 << m->player)) != 0) {
        if (m->state == 2) {
            if ((pressed & 0x40000) != 0) {
                pressed |= 0x2000000;
            }
            optmenu_nochoice = 0;
        } else if (m->title == NULL) {
            optmenu_nochoice = 0;
        } else if ((pressed & 0x40000) != 0) {
            optmenu_nochoice = 1;
        }
    } else {
        optmenu_nochoice = 1;
    }

    if (optmenu_nochoice != 0) {
        return -2;
    }

    if ((pressed & 0x2000000) != 0) {
        /* A / select */
        if (m->items[m->sel].value < 0) {
            AudioBuzzer();
            return 0;
        }
        if ((m->flags & 0x400) != 0) {
            AudioCursorSelect();
        }
        return m->items[m->sel].code;
    }
    if ((pressed & 0x8000000) != 0) {
        /* B / back */
        if ((m->flags & 0x800) != 0) {
            AudioMenuExit();
        }
        return -1;
    }
    if ((pressed & 0x1000000) != 0) {
        return 2;
    }
    if ((pressed & 0x10000003) != 0) {
        return 3;
    }
    if ((pressed & 0x2000000C) != 0) {
        return 4;
    }

    if (allowNav != 0 && m->items[0].text != NULL) {
        if ((pressed & 0x800000C0) != 0) {
            /* down */
            i = m->sel;
            do {
                m->sel++;
                if (m->sel >= m->num_items) {
                    m->sel = 0;
                }
            } while (m->sel != i && m->items[m->sel].value < 0);
            AudioCursorV();
            return 1;
        }
        if ((pressed & 0x40000030) != 0) {
            /* up */
            i = m->sel;
            do {
                m->sel--;
                if (m->sel < 0) {
                    m->sel = m->num_items - 1;
                }
            } while (m->sel != i && m->items[m->sel].value < 0);
            AudioCursorV();
            return 1;
        }
    } else {
        if ((pressed & 0x40000030) != 0) {
            return 7;
        }
        if ((pressed & 0x800000C0) != 0) {
            return 8;
        }
    }

    if ((held & 0x10000003) != 0) {
        return 5;
    }
    if ((held & 0x2000000C) != 0) {
        return 6;
    }
    if ((held & 0x40000030) != 0) {
        return 9;
    }
    if ((held & 0x800000C0) != 0) {
        return 10;
    }
    return 0;
}

/* ================================================================== */
/* 0x80072AA4 show_optmenu                                             */
/* ================================================================== */

void show_optmenu(OPTMENU* m)
{
    void* winset = gWinGlobals;
    s32 idx;
    s32 itofs;
    s32 font2;
    s32 part;
    u32 fade;
    s32 x;
    s32 y;
    s32 lh;
    s32 savedFlags;
    s32 sel;
    OPTITEM* it;
    s32 rgb[3];
    u32 color;
    s32 hifont;
    s32 hi;
    void* txt;
    char* text;
    s32 w;
    f32 fx;
    f32 fy;
    f32 fz;

    idx = 0;
    itofs = 0;
    font2 = 0;
    part = 0;
    fade = 0;

    if (m->icon_node != NULL) {
        MBTreeClearFlags(m->icon_node, 2, 0);
    }
    if (m->burn_blit != NULL) {
        mbBlitInit3414(m->burn_blit, 0);
    }
    if (m->title_blit != NULL) {
        mbBlitInit3414(m->title_blit, 0);
    }
    if ((m->flags & 0x40) != 0) {
        font2 = (s32)MBOX_FindTexture("FONT32_PARCH", NULL);
    }

    /* fade in / fade out alpha */
    if ((m->flags & 0x20) == 0) {
        if (m->finish_timer != 0) {
            return;
        }
    } else if (m->finish_timer == 0) {
        if (m->time < OPTMENU_FINISH_FRAMES) {
            fade = ((OPTMENU_FINISH_FRAMES - m->time) * 0xFF) / OPTMENU_FINISH_FRAMES;
        }
    } else {
        fade = (m->finish_timer * 0xFF) / OPTMENU_FINISH_FRAMES;
    }

    /* garamond font intro frames */
    hifont = 0;
    if (m->font != 0) {
        s32 fr;
        if (m->finish_timer == 0) {
            fr = (m->time - 10) / 2;
        } else {
            fr = 6 - (m->finish_timer * 6) / OPTMENU_FINISH_FRAMES;
        }
        hifont = m->font;
        if (fr >= 0) {
            hifont = 0;
            if (fr < 6) {
                hifont = m->font + fr;
            }
        }
    }

    /* title */
    if (m->title != NULL && m->title_blit != NULL) {
        x = -(m->bx + m->bw / 2);
        y = m->by + OPTMENU_MARGIN_TITLE;
        if (OPTMENU_SHADOW != 0) {
            DrawNormalText(m->title_scale, x + OPTMENU_SHADOW, y + OPTMENU_SHADOW, OPTMENU_FONT, 0,
                           m->title);
        }
        txt = DrawNormalText(m->title_scale, x, y, OPTMENU_FONT,
                             (optmenu_rgb_text[2] & 0xFF) |
                             ((optmenu_rgb_text[0] & 0xFF) << 16) |
                             ((optmenu_rgb_text[1] & 0xFF) << 8),
                             m->title);
        if (font2 != 0) {
            *(s16*)((u8*)txt + 0x26) = font2;
        }
    }
    if ((m->flags & 0x200) != 0) {
        font2 = 0;
    }

    savedFlags = MBSetFontFlags(0);
    y = m->y;
    lh = FontHeight(m->scale, OPTMENU_FONT);
    MBSetFont(OPTMENU_FONT);
    sel = m->sel;

    for (;;) {
        u32 flags = m->flags;
        f32 scale;
        u32 alpha;
        s32 hi2;

        it = (OPTITEM*)((u8*)m->items + itofs);
        if (it->text == NULL) {
            break;
        }
        scale = m->scale;
        hi = 0;
        alpha = fade;
        if (it->value < 0 && (s32)fade < 0x80) {
            alpha = 0x80;
        }
        MBSetFontFlags(0);
        hi2 = hifont;

        if (idx == sel && it->value >= 0) {
            /* selected item: pulse or flat highlight */
            s32 t2 = OPTMENU_FADE * 2;
            u32 ph = m->time - (m->time / (OPTMENU_FADE_HOLD + t2)) * (OPTMENU_FADE_HOLD + t2);
            hi = 1;
            if ((s32)ph > t2) {
                ph = 0;
            } else if ((s32)ph > OPTMENU_FADE) {
                ph = t2 - ph;
            }
            if (optmenu_choice_twice == 0) {
                s32 k;
                for (k = 0; k < 3; k++) {
                    s32 d = m->rgb_hi[k] - m->rgb_on[k];
                    s32 v = m->rgb_on[k] + (OPTMENU_FADE + (s32)ph * d - 1) / OPTMENU_FADE;
                    if (v < 0) {
                        v = 0;
                    } else if (v > 0xFF) {
                        v = 0xFF;
                    }
                    rgb[k] = v;
                }
                scale = (f32)(scale * m->scale *
                              (1.0 + (f32)(lbl_80343BC8 * ((f32)(s32)ph / (f32)OPTMENU_FADE))));
            } else {
                s32 v = (OPTMENU_FADE + 0xFF * (s32)ph - 1) / OPTMENU_FADE;
                alpha = alpha + (0x7F - v / 2);
                if ((s32)alpha > 0xFF) {
                    alpha = 0xFF;
                }
                if (lbl_803443E4 > 0) {
                    hi2 = lbl_803443E4;
                }
                {
                    s32 k;
                    for (k = 0; k < 3; k++) {
                        rgb[k] = m->rgb_hi[k];
                    }
                }
                MBSetFontFlags(0x4000);
            }
            ph = 0;
            font2 = 0;
        } else if (-1 - idx == sel && part == it->value) {
            s32 k;
            for (k = 0; k < 3; k++) {
                rgb[k] = m->rgb_hi[k];
            }
            font2 = 0;
        } else {
            s32 k;
            for (k = 0; k < 3; k++) {
                if (font2 == 0 || hifont != 0) {
                    rgb[k] = m->rgb_off[k];
                } else {
                    rgb[k] = 0xFF;
                }
            }
        }

        if ((font2 == 0 || hi2 != 0) && (flags & 0x200) != 0) {
            color = it->rgb;
        } else {
            color = (rgb[2] & 0xFF) | ((rgb[0] & 0xFF) << 16) | ((rgb[1] & 0xFF) << 8);
        }
        MBSetFontAlpha(alpha);
        MBSetFontColor(color);
        MBSetFontScaleSpace(scale, scale);
        MBSetFontScale(m->scale, m->scale);

        if (part == 0 && (hi == 0 || it->value < 1)) {
            x = m->x;
            text = it->text;
        } else {
            s32 w1 = DrawTextKeepScale(m->scale, it->text, OPTMENU_FONT);
            s32 w2 = DrawTextKeepScale(m->scale, " ~ ", OPTMENU_FONT);
            text = it->text2;
            x = m->x + w1 + w2;
        }
        txt = MBDrawText(x, y, text);
        if (hi2 == 0) {
            if (font2 != 0) {
                *(s16*)((u8*)txt + 0x26) = font2;
            }
        } else {
            *(s16*)((u8*)txt + 0x26) = hi2;
        }
        w = DrawTextKeepScale(m->scale, text, OPTMENU_FONT);
        x = x + w;

        if (it->on > 0 && it->on == part + 1) {
            txt = MBDrawText(x, y, " ~");
            w = DrawTextKeepScale(m->scale, " ~", OPTMENU_FONT);
            x = x + w;
            if (hi2 == 0) {
                if (font2 != 0) {
                    *(s16*)((u8*)txt + 0x26) = font2;
                }
            } else {
                *(s16*)((u8*)txt + 0x26) = hi2;
            }
        }
        it->draw_y = y;
        it->draw_xend = x;
        MBSetFontAlpha(0);

        if (idx == sel && optmenu_choice_twice != 0) {
            sel = -1 - sel;
        } else if (it->text2 == NULL || part != 0) {
            if (txt != NULL) {
                y = y + lh + it->dy;
            }
            part = 0;
            idx++;
            itofs += 0x24;
        } else {
            part = 1;
        }
    }

    MBSetFontScaleSpace(1.0f, 1.0f);
    MBSetFontFlags(savedFlags);

    /* "Player N" tag */
    if ((m->flags & 0x10) != 0 && m->player >= 0) {
        s32 p = m->player;
        y = m->by + OPTMENU_MARGIN_PLAYER;
        sprintf(optglobals.tbuf, "Player %d", p + 1);
        txt = DrawNormalText(optplyr_scale, -(p * 100 + 0x6A), y, OPTMENU_FONT,
                             (optmenu_rgb_text[2] & 0xFF) |
                             ((optmenu_rgb_text[0] & 0xFF) << 16) |
                             ((optmenu_rgb_text[1] & 0xFF) << 8),
                             optglobals.tbuf);
        if (font2 != 0) {
            *(s16*)((u8*)txt + 0x26) = (s16)font2;
        }
    }

    /* selection icon glide */
    if (m->icon_node != NULL) {
        s32 tx = m->x;
        s32 t;
        u32 ty;
        if (tx < 0) {
            tx = -(tx + m->w / 2);
        }
        t = m->icon_t;
        ty = lh / 2 + m->items[m->sel].draw_y;
        if (t == 0) {
            if (ty != (u32)m->icon_y) {
                m->icon_t = 1;
                ty = m->icon_y;
            }
        } else if (t < OPTMENU_ICON_TIME) {
            m->icon_t = t + vb_elapsed_menu;
            ty = m->icon_y + ((ty - m->icon_y) * m->icon_t) / OPTMENU_ICON_TIME;
        } else {
            m->icon_y = ty;
            m->icon_t = 0;
        }
        {
            f32 v[3];
            v[0] = (f32)(s32)(tx + m->icon_dx);
            v[1] = (f32)(s32)ty;
            v[2] = optmenu_icon_z;
            MBWorldToScreen3D((f32*)((u8*)m->icon_node + 0x30), v);
        }
        *(u32*)((u8*)m->icon_node + 0x40) = m->icon_rgb;
        *(u32*)((u8*)m->icon_node + 0x44) = m->icon_rgb;
        *(u32*)((u8*)m->icon_node + 0x48) = m->icon_rgb;
        CopyMat3((f32*)(*((u8**)winset + 1) + 0x240), (f32*)m->icon_node);
        PitchMat3();
        fn_80011104(m->msg, 0, 0, tx, ty);
    }

    /* burn blit animation */
    if (m->burn_blit != NULL) {
        mbInitBlitEntry(m->burn_blit, m->burn_frames, (m->time >> 3) % 5);
    }

    /* button prompts */
    if ((m->flags & 0xF) != 0 && m->finish_timer == 0) {
        s32 n = 0;
        s32 px;
        s32 sz;
        u32 py;
        u32 rgbp;
        s32 pw;
        char* pc;

        for (idx = 0; idx < 4; idx++) {
            if ((m->flags & (1 << idx)) != 0) {
                n++;
            }
        }
        px = 0x200 / (n + 1);
        sz = (s32)(32.0f * msg_scale);
        py = m->prompt_y;
        rgbp = (optmenu_rgb_text[2] & 0xFF) | ((optmenu_rgb_text[0] & 0xFF) << 16) |
               ((optmenu_rgb_text[1] & 0xFF) << 8);
        idx = 0;
        if ((m->flags & 1) != 0) {
            if (OPTMSG_SHADOW != 0) {
                DrawNormalText(msg_scale, -(px + OPTMSG_SHADOW), py + OPTMSG_SHADOW,
                               OPTMENU_FONT, 0, "Back");
            }
            txt = DrawNormalText(msg_scale, -px, py, OPTMENU_FONT, rgbp, "Back");
            if (font2 != 0) {
                *(s16*)((u8*)txt + 0x26) = (s16)font2;
            }
            pw = DrawTextKeepScale(msg_scale, "Back", OPTMENU_FONT);
            MBNewTempBlit(lbl_80344E44, ((px - pw / 2) - sz) - 4, py, sz, sz);
            idx = px;
        }
        if ((m->flags & 4) != 0) {
            idx = idx + px;
            if (OPTMSG_SHADOW != 0) {
                DrawNormalText(msg_scale, -(idx + OPTMSG_SHADOW), py + OPTMSG_SHADOW,
                               OPTMENU_FONT, 0, "Change");
            }
            txt = DrawNormalText(msg_scale, -idx, py, OPTMENU_FONT, rgbp, "Change");
            if (font2 != 0) {
                *(s16*)((u8*)txt + 0x26) = (s16)font2;
            }
            pw = DrawTextKeepScale(msg_scale, "Change", OPTMENU_FONT);
            px = ((idx - pw / 2) - sz) - 4;
            MBNewTempBlit(lbl_80344E38, px - sz, py, sz, sz);
            MBNewTempBlit(lbl_80344E34, px, py, sz, sz);
        }
        if ((m->flags & 2) != 0) {
            if ((m->flags & 0x102) == 0x102) {
                pc = "Accept";
            } else {
                pc = "Select";
            }
            idx = idx + px;
            if (OPTMSG_SHADOW != 0) {
                DrawNormalText(msg_scale, -(idx + OPTMSG_SHADOW), py + OPTMSG_SHADOW,
                               OPTMENU_FONT, 0, pc);
            }
            txt = DrawNormalText(msg_scale, -idx, py, OPTMENU_FONT, rgbp, pc);
            if (font2 != 0) {
                *(s16*)((u8*)txt + 0x26) = (s16)font2;
            }
            pw = DrawTextKeepScale(msg_scale, pc, OPTMENU_FONT);
            MBNewTempBlit(lbl_80344E48, ((idx - pw / 2) - sz) - 4, py, sz, sz);
        }
        if ((m->flags & 8) != 0) {
            idx = idx + px;
            if (OPTMSG_SHADOW != 0) {
                DrawNormalText(msg_scale, -(idx + OPTMSG_SHADOW), py + OPTMSG_SHADOW,
                               OPTMENU_FONT, 0, "Center");
            }
            txt = DrawNormalText(msg_scale, -idx, py, OPTMENU_FONT, rgbp, "Center");
            if (font2 != 0) {
                *(s16*)((u8*)txt + 0x26) = (s16)font2;
            }
            pw = DrawTextKeepScale(msg_scale, "Center", OPTMENU_FONT);
            MBNewTempBlit(lbl_80344E3C, ((idx - pw / 2) - sz) - 4, py, sz, sz);
        }
    }
}

/* ================================================================== */
/* 0x80073718 end_optmenu                                              */
/* ================================================================== */

static void end_optmenu(s32 dir, s32 mode)
{
    OPTGLOBALS* og = (OPTGLOBALS*)&optglobals;
    s32 keepBlit = 1;
    OPTMENU* m;
    s32 off;
    s32 i;

    if (options_level < 0 || og->stack[options_level] == NULL) {
        FatalError("end_optmenu called with bad options_level", 0x800000);
    }

    if (mode < 0) {
        /* collapse: kill every level below, move top to level 0 */
        for (i = 0, off = 0; i < options_level; i++) {
            m = *(OPTMENU**)((u8*)og->stack + off);
            if (m != NULL) {
                REMOVE_OPTMENU(m);
            }
            off += 4;
        }
        og->stack[0] = og->stack[options_level];
        options_level = 0;
    }

    m = og->stack[options_level];
    AtreeDelete(m->msg);
    if (m->icon_node != NULL) {
        m->icon_node = MBRemoveNode(m->icon_node, 1);
    }
    if (m->burn_blit != NULL) {
        m->burn_blit = MBRemoveBlit(m->burn_blit);
    }
    if (mode <= 0 && (m->flags & 0x20) != 0) {
        m->finish_timer = 1;
    } else {
        REMOVE_OPTMENU(m);
    }

    if (dir < 0) {
        OPTMENU* m2;
        options_level--;
        m2 = og->stack[options_level];
        if (options_level >= 0) {
            options_state = m2->state;
        } else {
            options_state = 1;
        }
        /* hand the backdrop blit off to a self-fading scroll when the new
         * top menu has no backdrop of its own */
        if (mode <= 0 && m->title_blit != NULL &&
            (mode < 0 || options_level < 0 || m2->blit_name == NULL)) {
            s32 scroll = StartFireScroll(m->blit_name, -1, m->bx, m->by, m->bw, m->bh, 0, 0.0f);
            MBBlitOrder(scroll, m->title_blit);
            keepBlit = 0;
        }
    }
    if (keepBlit && m->title_blit != NULL) {
        m->title_blit = MBRemoveBlit(m->title_blit);
    }
}

/* ================================================================== */
/* 0x80073998 finish_optmenu                                           */
/* ================================================================== */

static s32 finish_optmenu(OPTMENU* m, s32 force)
{
    s32 i;
    u8 unused[8];

    if (m->active == 0 || m->finish_timer == 0) {
        if (options_state == 1) {
            options_state = 0;
            for (i = 0; i < 4; i++) {
                optglobals.stack[i] = NULL;
            }
        }
        return 0;
    }

    m->finish_timer += vb_elapsed_menu;
    if (m->finish_timer > OPTMENU_FINISH_FRAMES || force != 0) {
        REMOVE_OPTMENU(m);
        if (options_state == 1) {
            options_state = 0;
            for (i = 0; i < 4; i++) {
                optglobals.stack[i] = NULL;
            }
        }
        return 0;
    }
    show_optmenu(m);
    return 1;
}

/* ================================================================== */
/* 0x80073AD8 remove_optmenu                                           */
/* ================================================================== */

void remove_optmenu(OPTMENU* m)
{
    AtreeDelete(m->msg);
    if (m->icon_node != NULL) {
        m->icon_node = MBRemoveNode(m->icon_node, 1);
    }
    if (m->title_blit != NULL) {
        m->title_blit = MBRemoveBlit(m->title_blit);
    }
    if (m->burn_blit != NULL) {
        m->burn_blit = MBRemoveBlit(m->burn_blit);
    }
    m->active = 0;
    m->finish_timer = 0;
}

/* ================================================================== */
/* 0x80073B54 end_all_optmenus                                         */
/* ================================================================== */

void end_all_optmenus(void)
{
    while (options_level >= 0) {
        end_optmenu(-1, 1);
    }
}

/* ================================================================== */
/* 0x80073B8C start_optmenu                                            */
/* ================================================================== */

static void start_optmenu(OPTMENU* m, s32 sel)
{
    ClearAllPlayerControls(2);
    if (m != NULL) {
        if (options_level >= 0) {
            end_optmenu(1, 0);
        }
        options_level++;
    } else {
        /* pop */
        end_optmenu(-1, 0);
        if (options_level >= 0) {
            m = optglobals.stack[options_level];
        } else {
            return;
        }
    }
    if (options_level >= 4) {
        FatalError("OPTONS_MAXLEVEL EXCEEDED MAX", 0x800000);
    }
    optglobals.stack[options_level] = m;
    options_state = m->state;
    start_optmenu_nostack(m, sel);
    m->active = options_level + 1;
}

/* ================================================================== */
/* 0x80073C78 start_optmenu_nostack                                    */
/* ================================================================== */

void start_optmenu_nostack(OPTMENU* m, s32 sel)
{
    s32 h;
    s32 i;
    s32 n;
    s32 fh;
    s32 w;
    OPTITEM* it;
    void* node;
    void* match;

    h = 0;
    if (sel > -2) {
        m->player = sel;
    }

    if ((m->flags & 0x80) == 0) {
        m->font = 0;
    } else {
        m->font = (s32)MBOX_FindTexture("FONT32GAR0", NULL);
    }

    if (m->num_items == 0) {
        n = 0;
        for (it = m->items; it->text != NULL; it++) {
            n++;
        }
        m->num_items = n;
    }

    if (m->icon == 0) {
        m->icon_node = NULL;
    } else {
        m->icon_node = MBNewNode(0.0f, NULL, 0);
        node = MBTreeSetFlags(m->icon_node, 8, 0);
        match = AtreeMatch(node, sPowerupsBuf, "ICON_ARROW", 0);
        m->icon_node = fn_80012F78(node, match, m->msg, 0, 0);
        if (m->icon_node != NULL && *(void**)m->icon_node != NULL) {
            MBNodeSetParent(*(void**)m->icon_node, m->icon_node);
        }
    }
    if (m->icon_node != NULL) {
        *(f32*)((u8*)m->icon_node + 0x40) = 1.0f;
        *(f32*)((u8*)m->icon_node + 0x44) = 1.0f;
        *(f32*)((u8*)m->icon_node + 0x48) = 1.0f;
    }

    m->icon_y = 0;
    m->icon_t = OPTMENU_ICON_TIME;
    m->active = -1;
    m->finish_timer = 0;
    m->time = 0;

    if (m->h < 1) {
        fh = FontHeight(m->scale, OPTMENU_FONT);
        for (i = 0; i < m->num_items; i++) {
            h += fh + m->items[i].dy;
        }
        m->h = h;
    }

    if (m->w < 1) {
        w = 0;
        MBSetFont(OPTMENU_FONT);
        MBSetFontScale(m->scale, m->scale);
        for (i = 0; i < m->num_items; i++) {
            s32 tw = MBFontStringWidth(m->items[i].text);
            if (w < tw) {
                w = tw;
            }
        }
        MBSetFontScale(1.0f, 1.0f);
        m->w = w;
    }

    if (m->y == -1) {
        m->y = 0xC0 - m->h / 2;
    } else if (m->y < 0) {
        m->y = -(m->y + m->h / 2);
    }

    if (m->blit_name == NULL) {
        m->title_blit = NULL;
    } else {
        if (m->bw < 0) {
            m->bw = m->w + OPTMENU_MARGIN_X * 2;
        }
        if (m->bh < 0) {
            m->bh = m->h + OPTMENU_MARGIN_Y * 2;
        }
        if (m->bx < 0) {
            m->bx = 0x100 - m->bw / 2;
        }
        if (m->by < 0) {
            m->by = 0xC0 - m->bh / 2;
        }
        m->title_blit = mbNewBlitSized(m->blit_name, m->bx, m->by, m->bw, m->bh);
        mbBlitCvtCoord(m->title_blit, OPTMENU_SCROLLZ);
    }

    if (m->burn_name == NULL) {
        m->burn_blit = NULL;
    } else {
        m->burn_blit = mbNewBlitSized(m->burn_name, m->ux, m->uy, m->uw, m->uh);
        m->burn_frames = MBBlitGetTex(m->burn_blit);
        mbBlitCvtCoord(m->title_blit, (f32)(OPTMENU_SCROLLZ - 1.0));
    }
}

/* ================================================================== */
/* 0x80073FE4 next_rune_hint                                           */
/* ================================================================== */

static void next_rune_hint(s32 advance)
{
    s32 pass;
    s32 n;
    s32 i;
    s32 off;
    s32 p;
    u32 bit;

    pass = 0;
    if (advance == 0) {
        rune_hint_index = -1;
        rune_hint_num = 0;
    } else {
        n = rune_hint_index + 1;
        for (;;) {
            if (n >= 0xD) {
                rune_hint_num = 1;
                n = n % 0xD;
            }
            i = n;
            off = n << 2;
            for (; i < 0xD; i++, off += 4) {
                if (rune_hint_num != 0) {
                    break;
                }
                if (PlayerHasShard(-1, *(s32*)((u8*)rune_idx_table + off) - 1) == 0) {
                    break;
                }
            }
            if (i != 0xD) {
                break;
            }
            if (rune_hint_num > 1) {
                rune_hint_num = 1;
                rune_hint_index = 0;
                i = rune_hint_index;
                break;
            }
            rune_hint_num++;
            n = 1;
        }
        rune_hint_index = i;

        /* per-player: mask lives in the current-level record (stride 0xF0,
         * level index at record+0xC) */
        bit = 1 << (rune_idx_table[rune_hint_index] - 1);
        for (p = 0; p < 4; p++) {
            u8* rec = &gPlayers[p * PREC_STRIDE];
            if (pass == 0 && (*(u16*)(rec + *(s32*)(rec + 0xC) * 0xF0 + 0xDDC) & bit) != 0) {
                pass = 1;
            }
            if ((*(u16*)(rec + *(s32*)(rec + 0xC) * 0xF0 + 0xDDE) & bit) != 0) {
                pass = 2;
            }
        }
        rune_hint_pass = pass + 1;
    }
}

/* ================================================================== */
/* 0x80074164 next_legend_hint                                         */
/* ================================================================== */

static void next_legend_hint(s32 advance)
{
    s32 pass;
    s32 n;
    s32 off;
    s32 i;
    s32 id;
    s32 p;
    u32 bit;

    pass = 0;
    if (advance == 0) {
        legend_hint_index = 0;
        legend_hint_num = 0;
        return;
    }
    n = legend_hint_index + 1;
    for (;;) {
        if (n >= 10) {
            legend_hint_num = 1;
            n = n % 10;
        }
        if (n == 0) {
            n = 1;
        }
        i = n;
        off = n << 2;
        for (; i < 10; i++, off += 4) {
            id = *(s32*)((u8*)crystal_order + off);
            if (WorldOpen(id) != 0 &&
                (legend_hint_num != 0 || towerGetRuneNearStat(-1, id) == 0)) {
                break;
            }
        }
        if (i != 10) {
            break;
        }
        if (legend_hint_num > 1) {
            legend_hint_num = 1;
            legend_hint_index = 1;
            goto have_index;
        }
        legend_hint_num++;
        n = 1;
    }
    legend_hint_index = i;
have_index:
    bit = 1 << crystal_order[legend_hint_index];
    for (p = 0; p < 4; p++) {
        u8* rec = &gPlayers[p * PREC_STRIDE];
        if (pass == 0 && (*(u16*)(rec + *(s32*)(rec + 0xC) * 0xF0 + 0xDE0) & bit) != 0) {
            pass = 1;
        }
        if ((*(u16*)(rec + *(s32*)(rec + 0xC) * 0xF0 + 0xDE2) & bit) != 0) {
            pass = 2;
        }
    }
    legend_hint_pass = pass + 1;
}

/* ================================================================== */
/* 0x800742F4 next_boss_hint                                           */
/* ================================================================== */

static void next_boss_hint(s32 advance)
{
    s32 pass;
    s32 n;
    s32 off;
    s32 i;
    s32 id;
    s32 p;
    u32 bit;

    pass = 0;
    if (advance == 0) {
        boss_hint_index = 0;
        boss_hint_num = 0;
        return;
    }
    n = boss_hint_index + 1;
    for (;;) {
        if (n >= 0xB) {
            boss_hint_num = 1;
            n = n % 0xB;
        }
        if (n == 0) {
            n = 1;
        }
        i = n;
        off = n << 2;
        for (; i < 0xB; i++, off += 4) {
            id = *(s32*)((u8*)crystal_order + off);
            if (WorldOpen(id) != 0 &&
                (boss_hint_num != 0 || PlayerHasRune(-1, id) == 0)) {
                break;
            }
        }
        if (i != 0xB) {
            break;
        }
        if (boss_hint_num > 1) {
            boss_hint_num = 1;
            boss_hint_index = 1;
            goto have_index;
        }
        boss_hint_num++;
        n = 1;
    }
    boss_hint_index = i;
have_index:
    bit = 1 << crystal_order[boss_hint_index];
    for (p = 0; p < 4; p++) {
        u8* rec = &gPlayers[p * PREC_STRIDE];
        if (pass == 0 && (*(u16*)(rec + *(s32*)(rec + 0xC) * 0xF0 + 0xDE4) & bit) != 0) {
            pass = 1;
        }
        if ((*(u16*)(rec + *(s32*)(rec + 0xC) * 0xF0 + 0xDE6) & bit) != 0) {
            pass = 2;
        }
    }
    boss_hint_pass = pass + 1;
}

/* ================================================================== */
/* 0x80074484 next_general_hint                                        */
/* ================================================================== */

static void next_general_hint(s32 advance)
{
    s32 n;

    if (advance == 0) {
        general_hint_index = -1;
        general_hint_num = -1;
    } else {
        for (;;) {
            if (general_hint_index < 0) {
                general_hint_index = 3;
            }
            while (general_hint_index > 0 &&
                   towerAllPlayersMetLevelReq(general_hint_index - 1) == 0) {
                general_hint_num = -1;
                general_hint_index--;
            }
            general_hint_num++;
            n = GetScrollListMsg(1, 3, general_hint_index);
            if (general_hint_num < n) {
                break;
            }
            general_hint_num = -1;
            general_hint_index--;
        }
    }
}

/* ================================================================== */
/* 0x80074548 OptionsSetup                                             */
/* ================================================================== */

void OptionsSetup(void)
{
    AudioSetVolMusic(optglobals.music_vol);
    AudioSetVolSfx(optglobals.sfx_vol);
    AudioSetEnabled(optglobals.sound_mode);
    fn_800C25F0(optglobals.screen_dx, optglobals.screen_dy);
}

/* ================================================================== */
/* 0x80074598 init_prefs                                               */
/* ================================================================== */

void init_prefs(void)
{
    optglobals.sound_mode = 1;
    optglobals.sfx_vol = 0x80;
    optglobals.music_vol = 0x80;
    optglobals.screen_dx = 0;
    optglobals.screen_dy = 0;
    optglobals.style = 1;
    optglobals.vibration = 0;
    optglobals.subtitles = 0;
}
