/*
 * controls.c - player controls / PS2 scePad+sceMtap shim layer (CONTROLS.OBJ).
 *
 * .text 0x8003101C-0x80034CFC, extab 0x80005C10-0x80005CE0,
 * extabindex 0x80009268-0x800093A0.
 *
 * TU identity/boundary evidence:
 *  - Xbox PDB CONTROLS.OBJ carries the full roster (ReadControls,
 *    PlayerControls, joyReadPad/joyGetStatus, serve_mtap, MtapOpenPort,
 *    the new_/any_ edge-query family, aiPad* data).  Every function in
 *    0x8003101C-0x80034CFC shares the same statics (lbl_802407xx pad
 *    arrays, lbl_80240E30 per-player structs, lbl_803445xx flag cluster,
 *    ctrls_initialized) - one TU.
 *  - fn_80030C84 (ends exactly 0x8003101C) references sWeaponsBuf/
 *    sPowerupsBuf (weapons-fx TU); fn_80034CFC references gEnemies/
 *    gCurLevel (combat-side TU) - clean seams both sides.
 *  - extabindex rows 0x80009268..0x800093A0 are exactly this TU's
 *    LR-saving fns (first row fn=0x8003101C, last row fn=0x80034B3C,
 *    bracketed by 0x80030C84 / 0x80034CFC rows).
 *
 * Names from the Xbox PDB (research/xbox_symbols/functions_by_module.txt).
 * NOTE two symbols are pinned by Matching main.c and keep their old names:
 *  - "PlayerControls" @0x80032B84 is really PlayerControls (per-frame
 *    master: ReadControls -> PlayerControls -> vibes -> CheckSpecials ->
 *    serve_memcard; Xbox PlayerControls).
 *  - ControlsUpdate @0x800330D4 is really PlayerControls (edge/repeat-edge
 *    computation + serve_mtap; size 0x98C vs Xbox PlayerControls 0x907).
 *
 * GC-only helper fn_80034C88 (frsqrte Newton sqrt, same idiom as
 * g3dpad.c g3dSqrt) has no Xbox counterpart (x86 fsqrt) - keeps fn_ name.
 *
 * Xbox helpers absent on GC (inlined/stripped): padIsAnalog, padHasStick,
 * joy_open, mtap_open, get_dir, get_analog_dir, dir_edge,
 * calc_analog_stick, ctl_read_plyr, lf_memcpy, InitSpecialMoves,
 * print_edges, ResetPlayerControls, setDigitalButton, setMissingButton,
 * setAnalogButton, joyBeginShake, joyEndShake, vibe_off, serve_vibes,
 * my_ctrl_scheme, controls_set_active_players, all_plyr_new_*, start,
 * new_menu_back's siblings, controls_monkey, test_controls.
 *
 * Status: NonMatching.  20 of 38 fns byte-exact/matched (nuke_ctrls,
 * active_player_edge, new_menu_back/accept, controls_first_active_player,
 * start_no_assignment, new_start, new_up/down/left/right, any_level, any,
 * new_ctrl, and_edges*, assigned_controller, ClearControls*,
 * ClearPlayerControl*, controls_remove_active_player*, serve_mtap,
 * get_button, fn_80034C88; * = bss/data reloc-name cosmetic only).
 * Parked residual classes in PARKED.txt.  ControlsUpdate body is complete
 * but spill-pattern heavy (target saves r14-r31 with several loop webs
 * spilled; structure aligned).  Still skeletons: joyGetStatus (pad
 * connection state machine, phases 0/0x28-0x2A/0x46-0x4D/99 over
 * scePad shim stubs), joyReadPad (scePadRead report parser -> per-pad
 * levels + button records; get_dir/calc_analog_stick/set*Button
 * inlined), ReadControls (per-pad joyGetStatus pump + analog assembly
 * via JoyAng/JoyMag/fn_80034C88/atan2 into the staged F20..FC0 arrays).
 */
#include "types.h"

/* ------------------------------------------------------------------ */
/* external code                                                       */
/* ------------------------------------------------------------------ */
extern void* memset(void* p, int c, size_t n);
extern void bulletproof_printf(const char* fmt, ...);
extern void ErrorPrintf(const char* fmt, ...);
extern void FatalErrorf(const char* fmt, ...);
extern void serve_memcard(void);
extern void init_all_dir_info(void);
extern f32 atan2(f32 y, f32 x); /* PS2-shim float-returning decl (see matching-recipes) */
extern f64 __frsqrte(f64 x);

/* dolphin PAD (real GC motor control lives alongside the PS2 shim) */
extern void PADControlMotor(s32 chan, u32 cmd);
extern void PADControlAllMotors(const u32* cmds);

/* PS2 scePad/sceMtap shim layer (game/ps2/fakelib TU @0x800AF000+) */
extern s32 scePadGetState(s32 port, s32 slot);
extern s32 scePadInfoMode(void);       /* stub */
extern s32 scePadRead(void);           /* used by joyReadPad */
extern void scePadSetActDirect(s32 port, s32 slot, u8* data);
extern s32 scePadPortOpen(s32 port, s32 slot, void* data);
extern s32 scePadPortClose(s32 port);  /* stub */
extern s32 sceMtapPortOpen(s32 port);
extern s32 sceMtapGetConnection(s32 port);
extern s32 fn_800AF1F0(void);          /* scePad info/mode stub family */
extern s32 fn_800AF1F8(void);
extern s32 fn_800AF200(void);
extern s32 fn_800AF208(void);
extern s32 scePadSetActAlign(void);
extern s32 fn_800AF2D4(void);
extern s32 sceMtapInit(void);
extern void sysResetService(void);     /* fn_800DD180 */

/* ------------------------------------------------------------------ */
/* data                                                                */
/* ------------------------------------------------------------------ */

/* per-player assembled control state, stride 0x3C (lbl_80240E30) */
typedef struct CTL {
    /* 0x00 */ u32 ctl;          /* clear/hold-off countdown           */
    /* 0x04 */ u32 levels;       /* held buttons                       */
    /* 0x08 */ u32 edges;        /* new presses (new_* family reads)   */
    /* 0x0C */ u32 repedges;     /* auto-repeat edges                  */
    /* 0x10 */ s32 spTimer;   /* special-move cooldown              */
    /* 0x14 */ s32 spResult;  /* CheckSpecials result this frame    */
    /* 0x18 */ s32 spLast;    /* last special id                    */
    /* 0x1C */ f32 lx;           /* analog stick state                 */
    /* 0x20 */ f32 ly;
    /* 0x24 */ f32 rx;
    /* 0x28 */ f32 ry;
    /* 0x2C */ s32 scheme;       /* control scheme (SpecialData row)   */
    /* 0x30 */ s32 hasActuator;  /* abHasActuator                      */
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 unk38;
} CTL;

/* special-move combo program (0x8011A2A8, 0xC8 per record) */
typedef struct SMOVE {
    s32 id;
    s32 byteMode; /* nonzero: match low byte only */
    struct {
        u32 mask;
        s32 min;
        s32 hold;
    } st[16];
} SMOVE;

/* special-move level table + combo codes + names (0x8011AE10) */
typedef struct SMTAB {
    f32 lv[5];
    s32 dir[16]; /* direction-nibble -> 7x7 grid index */
    char* names[10];
} SMTAB;

/* ------------------------------------------------------------------ */
/* TU-owned data.  MWCC anchors own-layout statics/globals with        */
/* base+displacement merging, so these must be DEFINED here (not       */
/* extern) to reproduce the target codegen.  Declaration order =       */
/* address order.  .data [0x8011A220,0x8011AED4) / .sdata 0x80343BE0 / */
/* .bss [0x80240798,0x80240FD0) / .sbss [0x803445D8,0x80344628).       */
/* ------------------------------------------------------------------ */

/* --- .data --- */

/* 0x8011A220  rep_speed: auto-repeat delay ladder (frames) */
static s32 lbl_8011A220[14] = { 30, 20, 10, 6, 3, 3, 3, 3, 2, 2, 2, 2, 1, -1 };

/* 0x8011A258  player -> pad assignment */
static s32 lbl_8011A258[4] = { -1, -1, -1, -1 };

/* 0x8011A268  pad -> player assignment */
static s32 lbl_8011A268[8] = { -1, -1, -1, -1, 0, 0, 0, 0 };

/* 0x8011A288  scePadPortOpen done flags */
static s32 lbl_8011A288[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

/* 0x8011A2A8  SpecialMoves[13]: combo recognizer programs.  Each stage
 * matches a right-stick/button level word against mask (0x8000 = any-of,
 * 0x4000 = none-of, 0xFFF = accept), holding min..hold frames. */
static SMOVE lbl_8011A2A8[13] = {
    { 1, 1,
      {
          { 0x0000, 1, 0 },
          { 0x8030, 1, 0 },
          { 0x0000, 1, 0 },
          { 0x0030, 1, -1 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
      } },
    { 2, 1,
      {
          { 0x0000, 1, 0 },
          { 0x80C0, 1, 0 },
          { 0x0000, 1, 0 },
          { 0x00C0, 1, -1 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
      } },
    { 3, 1,
      {
          { 0x0000, 1, 0 },
          { 0x8003, 1, 0 },
          { 0x0000, 1, 0 },
          { 0x0003, 1, -1 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
      } },
    { 4, 1,
      {
          { 0x0000, 1, 0 },
          { 0x800C, 1, 0 },
          { 0x0000, 1, 0 },
          { 0x000C, 1, -1 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
      } },
    { 5, 1,
      {
          { 0x0000, 1, 0 },
          { 0x8033, 1, 0 },
          { 0x0000, 1, 0 },
          { 0x0033, 1, -1 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
      } },
    { 6, 1,
      {
          { 0x0000, 1, 0 },
          { 0x803C, 1, 0 },
          { 0x0000, 1, 0 },
          { 0x003C, 1, -1 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
      } },
    { 7, 1,
      {
          { 0x0000, 1, 0 },
          { 0x80C3, 1, 0 },
          { 0x0000, 1, 0 },
          { 0x00C3, 1, -1 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
      } },
    { 8, 1,
      {
          { 0x0000, 1, 0 },
          { 0x80CC, 1, 0 },
          { 0x0000, 1, 0 },
          { 0x00CC, 1, -1 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
      } },
    { 9, 1,
      {
          { 0x400F, 1, 0 },
          { 0x8003, 1, 0 },
          { 0x400F, 0, 0 },
          { 0x800C, 1, 0 },
          { 0x400F, 0, 0 },
          { 0x8003, 1, -1 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
      } },
    { 9, 1,
      {
          { 0x400F, 1, 0 },
          { 0x800C, 1, 0 },
          { 0x400F, 0, 0 },
          { 0x8003, 1, 0 },
          { 0x400F, 0, 0 },
          { 0x800C, 1, -1 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
      } },
    { 10, 1,
      {
          { 0x40F0, 1, 0 },
          { 0x8030, 1, 0 },
          { 0x40F0, 0, 0 },
          { 0x80C0, 1, 0 },
          { 0x40F0, 0, 0 },
          { 0x8030, 1, -1 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
      } },
    { 10, 1,
      {
          { 0x40F0, 1, 0 },
          { 0x80C0, 1, 0 },
          { 0x40F0, 0, 0 },
          { 0x8030, 1, 0 },
          { 0x40F0, 0, 0 },
          { 0x80C0, 1, -1 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
      } },
    { 11, 0,
      {
          { 0x0300, 1, -1 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
          { 0x0000, 0, 0 },
      } },
};

/* 0x8011ACD0  SpecialData[10]: per-scheme turbo masks {lo,hi} x4 */
static u32 lbl_8011ACD0[10][8] = {
    { 0x01000000, 0, 0x01000000, 0, 0x08000000, 0, 0x08000000, 0 },
    { 0x02000000, 0, 0x02000000, 0, 0x02000000, 0, 0x02000000, 0 },
    { 0x04000000, 0, 0x08000000, 0, 0x00100000, 0, 0x00100000, 0 },
    { 0x08000000, 0, 0x04000000, 0, 0x00400000, 0, 0x00400000, 0 },
    { 0x08000000, 0, 0x04000000, 0, 0x00400000, 0, 0x00400000, 0 },
    { 0x00100000, 0, 0x00100000, 0, 0x04000000, 0x01000000, 0, 0 },
    { 0x00400000, 0, 0x00400000, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0x00800000, 0, 0x00800000, 0, 0x00800000, 0, 0x00800000, 0 },
};

/* 0x8011AE10  special-move level table + combo codes + names */
static SMTAB lbl_8011AE10 = {
    { 0.0f, 0.33f, 0.66f, 1.0f, 1.0f },
    { 3, 2, 1, 0, 4, 0, 0, 0, 5, 0, 0, 0, 6, 0, 0, 0 },
    { "S_MAGIC", "S_ATK_QUICK", "S_ATK_SLOW", "S_TURBO", "S_DEFEND",
      "S_CHARGE", "S_STRAFE", "S_MAGIC_SHIELD", "S_THROW_MAGIC",
      "S_COMBO_MOVE" },
};

/* 0x8011AE8C  vibe_inten */
static f32 lbl_8011AE8C[5] = { 0.2f, 0.4f, 0.6f, 0.8f, 1.0f };

/* 0x8011AEA0  monkey/shadow playback names (ReadControls) */
static char* lbl_8011AEA0[3] = { "SHADOW1L1", "SHADOW2L1", "SHADOW3L1" };

/* 0x8011AEAC / 0x8011AEC0  stick scale ladders */
static f32 lbl_8011AEAC[5] = { 1.0f, 1.0f, 1.5f, 2.0f, 2.0f };
static f32 lbl_8011AEC0[5] = { 1.0f, 1.0f, 0.5f, 0.3f, 0.2f };

/* --- .sdata --- */

/* 0x80343BE0  MtapOpenPort: sceMtapGetConnection-done per port */
static s32 lbl_80343BE0[2] = { 0, 0 };

/* --- .bss --- */

extern u32 lbl_80240798[8]; /* 0x80240798 latched menu edges - owned by an earlier TU (target bss starts at 0x802407B8) */
static u32 lbl_802407B8[4];      /* 0x802407B8 per-pad levels                  */
static u32 lbl_802407C8[4];      /* 0x802407C8 per-pad old levels              */
static u32 lbl_802407D8[4];      /* 0x802407D8 per-pad levels word 2           */
static u32 lbl_802407E8[4];      /* 0x802407E8 per-pad edges                   */
static u32 lbl_802407F8[4];      /* 0x802407F8 per-pad repeat edges            */
static u32 lbl_80240808[4];      /* 0x80240808 sw_cnt repeat accumulators      */
static s32 lbl_80240818[4];      /* 0x80240818 sw_idx repeat speed index       */
static u32 lbl_80240828[4][12];  /* 0x80240828 right-stick repeat counters     */
static s32 lbl_802408E8[4][12];  /* 0x802408E8 right-stick repeat indexes      */
static u8 lbl_802409A8[0x100];   /* 0x802409A8 aallPadDmaBuffer (8 x 32)       */
static s32 lbl_80240AA8[4];      /* 0x80240AA8 aiPadPhase                      */
static s32 lbl_80240AB8[4];      /* 0x80240AB8 aiPadType                       */
static s32 lbl_80240AC8[4];      /* 0x80240AC8 aiPadLastState                  */
static s32 lbl_80240AD8[4];      /* 0x80240AD8 aiPadTermID                     */
static u8 lbl_80240AE8[4][0x68]; /* 0x80240AE8 per-player button records       */
static f32 lbl_80240C88[7][7];   /* 0x80240C88 JoyAng (7x7 stick angle LUT)    */
static f32 lbl_80240D4C[7][7];   /* 0x80240D4C JoyMag (7x7 stick magnitude)    */
static s32 lbl_80240E10[8];      /* 0x80240E10 vibe {active,timer} x 4         */
static CTL lbl_80240E30[4];      /* 0x80240E30 per-player control structs      */
static f32 lbl_80240F20[4];      /* 0x80240F20 right-stick angle (staged)      */
static f32 lbl_80240F30[4];      /* 0x80240F30 right-stick magnitude (staged)  */
static f32 lbl_80240F40[4];      /* 0x80240F40 left-stick angle (staged)       */
static f32 lbl_80240F50[4];      /* 0x80240F50 left-stick magnitude (staged)   */
static u32 lbl_80240F60[4];      /* 0x80240F60 staged levels word 2            */
static u32 lbl_80240F70[4];      /* 0x80240F70 staged levels                   */
static u32 lbl_80240F80[4];      /* 0x80240F80                                 */
static u32 lbl_80240F90[4];      /* 0x80240F90 right-stick old levels          */
static u32 lbl_80240FA0[4];      /* 0x80240FA0 right-stick repeat edges        */
static u32 lbl_80240FB0[4];      /* 0x80240FB0 right-stick edges               */
static u32 lbl_80240FC0[4];      /* 0x80240FC0 right-stick levels              */

/* --- .sbss --- */

static s32 lbl_803445D8;         /* 0x803445D8 assignment-disable bits         */
static s32 lbl_803445DC;         /* 0x803445DC mtap serve selector             */
static volatile u32 lbl_803445E0; /* 0x803445E0 pad access busy flag (spin-waited) */
static s32 lbl_803445E4;         /* 0x803445E4 pad progress/watchdog code      */
static s32 lbl_803445E8;         /* 0x803445E8 per-port flag                   */
static s32 lbl_803445EC;         /* 0x803445EC mtap driver present             */
s32 ctrls_initialized;    /* 0x803445F0                                 */
static s32 lbl_803445F4;         /* 0x803445F4 in-controls-update flag         */
static s32 lbl_803445F8;         /* 0x803445F8 "updated" staged-data flag      */
static s32 lbl_803445FC;         /* 0x803445FC disable_player_controls         */
static s32 lbl_80344600;         /* 0x80344600 aux_sel_active                  */
static s32 lbl_80344604;         /* 0x80344604                                 */
static s32 lbl_80344608[2];      /* 0x80344608 mtap slot status (port+2)       */
static s32 lbl_80344610[2];      /* 0x80344610 mtap port status                */
u32 lbl_80344618;         /* 0x80344618 all-player repeat edges         */
u32 lbl_8034461C;         /* 0x8034461C all-player edges                */
u32 lbl_80344620;         /* 0x80344620 all-player levels               */
static u32 lbl_80344624;         /* 0x80344624 (pad)                           */

/* --- data owned by other TUs --- */
extern u32 lbl_80344824;  /* active player mask (later TU sbss)         */
extern s32 lbl_80344578;  /* steptime (clock TU)                        */

/* byte copy helper (Xbox PDB: lf_memcpy; inlined on GC) */
static void lf_memcpy(u8* d, u8* src, s32 n)
{
    while (n-- != 0) {
        *d++ = *src++;
    }
}

/* ------------------------------------------------------------------ */
/* prototypes (nearly every intra-TU call is a forward reference -     */
/* callees sit AFTER callers in the emission order)                    */
/* ------------------------------------------------------------------ */
void nuke_ctrls(void);
s32 active_player_edge(u32 mask);
s32 new_menu_back(s32 plyr);
void controls_remove_active_player(s32 plyr);
s32 controls_first_active_player(void);
s32 new_menu_accept(s32 plyr, s32 allow_start);
s32 start_no_assignment(void);
s32 new_start(s32 plyr);
s32 new_up(s32 plyr);
s32 new_down(s32 plyr);
s32 new_left(s32 plyr);
s32 new_right(s32 plyr);
s32 any_level(u32 mask);
s32 any(u32 mask);
s32 new_ctrl(u32 mask, s32 plyr);
s32 and_edges(u32 mask);
s32 assigned_controller(s32 pad);
s32 assign_controller(s32 pad);
void vibrators_off(void);
void do_vibe(s32 plyr, s32 inten, s32 time);
s32 joyGetStatus(s32 pad, u8* buf);
s32 joyReadPad(s32 pad, u8* buf, s32 state);
void EnablePlayerControls(void);
void DisablePlayerControls(void);
void ClearControls(void);
void InitPlayerControls(void);
void ClearAllPlayerControls(s32 code);
void ClearPlayerControl(s32 plyr, s32 code);
void PlayerControls(void);     /* really PlayerControls - see header */
void InitJoyAng(void);
void ControlsUpdate(void);        /* really PlayerControls - see header */
s32 CheckSpecials(s32 plyr, u32 lev);
void ReadControls(void);
void InitControls(void);
void serve_mtap(s32 which);
s32 get_button(u8* rec, s32 no_analog);
void init_controls(void);
void MtapOpenPort(s32 port, s32 flag);
f32 fn_80034C88(f32 x);

/* ================================================================== */

/* 0x8003101C  clear the latched edge block so menus don't double-fire */
void nuke_ctrls(void)
{
    memset(lbl_80240798, 0, 0x20);
}

/* 0x8003104C  does any ACTIVE player (gPlayers stride 0x335C) have a
 * new edge under mask?  (auxscreen: accept-button query) */
extern u8 gPlayers[4][0x335C]; /* gPlayers */
s32 active_player_edge(u32 mask)
{
    int i;

    for (i = 0; i < 4; i++) {
        if (*(s32*)(gPlayers[i] + 0xE8) != 0 && (mask & lbl_80240E30[i].edges) != 0) {
            return 1;
        }
    }
    return 0;
}

/* 0x800310A8  new press of the menu-back button (0x8000000) */
s32 new_menu_back(s32 plyr)
{
    s32 ret;

    if (plyr == -1) {
        ret = and_edges(0x8000000);
    } else if (lbl_80240E30[plyr].edges & 0x8000000) {
        ret = plyr + 1;
    } else {
        ret = 0;
    }
    switch (ret) {
    case 0:
        ret = 0;
        break;
    }
    return ret;
}

/* 0x80031110  drop one player's controller assignment (-1 = all) and
 * rebuild the active-player mask */
void controls_remove_active_player(s32 plyr)
{
    int i;

    if (plyr == -1) {
        for (i = 0; i < 4; i++) {
            lbl_8011A258[i] = -1;
        }
        for (i = 0; i < 4; i++) {
            lbl_802407E8[i] = 0;
            lbl_802407F8[i] = 0;
            lbl_8011A268[i] = -1;
        }
        lbl_803445D8 = 0;
    } else {
        s32 pad = lbl_8011A258[plyr];
        if (pad >= 0) {
            lbl_802407E8[pad] = 0;
            lbl_802407F8[pad] = 0;
            lbl_8011A268[pad] = -1;
        }
        lbl_8011A258[plyr] = -1;
    }

    lbl_80344824 = 0;
    for (i = 0; i < 4; i++) {
        if (lbl_8011A258[i] != -1) {
            lbl_80344824 |= 1 << i;
        }
    }
}

/* 0x80031208  first player slot with a controller assigned, or -1 */
s32 controls_first_active_player(void)
{
    int i;

    for (i = 0; i < 4; i++) {
        if (lbl_8011A258[i] != -1) {
            return i;
        }
    }
    return -1;
}

/* 0x80031244  new press of the menu-accept button (0x2000000), START
 * optionally accepted too; opens the controller-assignment window */
s32 new_menu_accept(s32 plyr, s32 allow_start)
{
    s32 ret;
    u32 mask;

    lbl_80344600 = 1;
    if (allow_start != 0) {
        mask = 0x2040000;
    } else {
        mask = 0x2000000;
    }
    if (plyr == -1) {
        ret = and_edges(mask);
    } else if (mask & lbl_80240E30[plyr].edges) {
        ret = plyr + 1;
    } else {
        ret = 0;
    }
    lbl_80344600 = 0;
    switch (ret) {
    case 0:
        ret = 0;
        break;
    }
    return ret;
}

/* 0x800312D0  is START held on ANY raw pad (no controller assignment
 * side effects - attract mode "press start") */
s32 start_no_assignment(void)
{
    int i;

    for (i = 0; i < 4; i++) {
        if ((lbl_802407B8[i] & 0x40000) != 0) {
            return 1;
        }
    }
    return 0;
}

/* 0x8003130C  new press of START (assignment window open) */
s32 new_start(s32 plyr)
{
    s32 ret;

    lbl_80344600 = 1;
    if (plyr == -1) {
        ret = and_edges(0x40000);
    } else if (lbl_80240E30[plyr].edges & 0x40000) {
        ret = plyr + 1;
    } else {
        ret = 0;
    }
    lbl_80344600 = 0;
    return ret;
}

/* 0x80031374  new press of up (dpad | stick) */
s32 new_up(s32 plyr)
{
    s32 ret;

    if (plyr == -1) {
        ret = and_edges(0x2000000C);
    } else if (lbl_80240E30[plyr].edges & 0x2000000C) {
        ret = plyr + 1;
    } else {
        ret = 0;
    }
    return ret;
}

/* 0x800313D8  new press of down */
s32 new_down(s32 plyr)
{
    s32 ret;

    if (plyr == -1) {
        ret = and_edges(0x10000003);
    } else if (lbl_80240E30[plyr].edges & 0x10000003) {
        ret = plyr + 1;
    } else {
        ret = 0;
    }
    return ret;
}

/* 0x8003143C  new press of left */
s32 new_left(s32 plyr)
{
    s32 ret;

    if (plyr == -1) {
        ret = and_edges(0x800000C0);
    } else if (lbl_80240E30[plyr].edges & 0x800000C0) {
        ret = plyr + 1;
    } else {
        ret = 0;
    }
    return ret;
}

/* 0x800314A0  new press of right */
s32 new_right(s32 plyr)
{
    s32 ret;

    if (plyr == -1) {
        ret = and_edges(0x40000030);
    } else if (lbl_80240E30[plyr].edges & 0x40000030) {
        ret = plyr + 1;
    } else {
        ret = 0;
    }
    return ret;
}

/* 0x80031504  is mask held on any raw pad */
s32 any_level(u32 mask)
{
    int i;

    for (i = 0; i < 4; i++) {
        if ((mask & lbl_802407B8[i]) != 0) {
            return 1;
        }
    }
    return 0;
}

/* 0x80031540  is mask newly pressed on any raw pad */
s32 any(u32 mask)
{
    int i;

    for (i = 0; i < 4; i++) {
        if ((mask & lbl_802407B8[i]) != 0 && (mask & lbl_802407C8[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

/* 0x80031594  new press of an arbitrary mask for one player (-1 = any) */
s32 new_ctrl(u32 mask, s32 plyr)
{
    s32 ret;

    if (plyr == -1) {
        ret = and_edges(mask);
    } else if (mask & lbl_80240E30[plyr].edges) {
        ret = plyr + 1;
    } else {
        ret = 0;
    }
    return ret;
}

/* 0x800315E8  scan raw pads for a new press of mask; unassigned pads
 * only match START (or, while the assignment window is open, a fresh
 * level press) and get a controller assigned on the spot.
 * Returns plyr+1, 10 for an unassignable hit, or 0. */
s32 and_edges(u32 mask)
{
    int pad;

    for (pad = 0; pad < 4; pad++) {
        s32 hit;
        s32 assigned;

        hit = 0;
        if (lbl_8011A268[pad] >= 0) {
            assigned = 1;
        } else {
            assigned = 0;
        }
        if (!assigned) {
            mask = 0x40000;
        }
        if (lbl_80344600 != 0 && !assigned && (mask & lbl_802407B8[pad]) != 0 &&
            (mask & lbl_802407C8[pad]) == 0) {
            hit = 1;
        }
        if ((mask & lbl_802407E8[pad]) != 0) {
            hit = 1;
        }
        if (hit) {
            s32 plyr;

            if (lbl_80344600 != 0) {
                plyr = assign_controller(pad);
                if (plyr != -1) {
                    return plyr + 1;
                }
            } else {
                return 10;
            }
        }
    }
    return 0;
}

/* 0x800316E0  does this pad have a player assigned */
s32 assigned_controller(s32 pad)
{
    if (lbl_8011A268[pad] >= 0) {
        return 1;
    }
    return 0;
}

/* 0x8003170C  assign a pad to a player slot: prefer slot = mtap port
 * when no mtap is present, then slot = mtap slot, then first free. */
s32 assign_controller(s32 pad)
{
    s32* q;
    s32* p;
    s32 plyr;
    int i;

    if (lbl_803445D8 != 0) {
        return -1;
    }
    p = &lbl_8011A268[pad];
    if (*p >= 0) {
        return *p;
    }
    if (lbl_80344610[0] != 2) {
        if (lbl_80344610[1] != 2 && pad % 4 == 0) {
            plyr = pad / 4;
            if (lbl_8011A258[plyr] < 0) {
                lbl_8011A258[plyr] = pad;
                *p = plyr;
                lbl_80344824 |= 1 << plyr;
                return plyr;
            }
        }
    }
    plyr = pad % 4;
    q = &lbl_8011A258[plyr];
    if (*q == -1) {
        *q = pad;
        *p = plyr;
        lbl_80344824 |= 1 << plyr;
        return plyr;
    }
    for (i = 0; i < 4; i++) {
        if (lbl_8011A258[i] == -1) {
            lbl_8011A258[i] = pad;
            *p = i;
            lbl_80344824 |= 1 << i;
            return i;
        }
    }
    return -1;
}

/* 0x80031854  stop every rumble motor (PS2 act direct + GC motors) */
void vibrators_off(void)
{
    s32 pad;
    int i;
    u32 cmds[4] = { 2, 2, 2, 2 };
    u8 act[8];

    for (i = 0; i < 4; i++) {
        if (lbl_80240E10[i * 2] != 0) {
            lbl_80240E10[i * 2 + 1] = 0;
            lbl_80240E10[i * 2] = 0;
            pad = lbl_8011A258[i];
            if (lbl_803445E0 == 0) {
                memset(act, 0, 6);
                scePadSetActDirect(pad / 4, pad & 3, act);
            }
            PADControlMotor(lbl_8011A258[i], 0);
        }
    }
    PADControlAllMotors(cmds);
}

/* 0x80031938  start a rumble at vibe_inten[inten] for time frames */
void do_vibe(s32 plyr, s32 inten, s32 time)
{
    if (lbl_80240E30[plyr].hasActuator != 0 && inten >= 0) {
        f32 v;
        s32* pp;
        u8 act[8];

        if (inten < 0) {
            inten = 0;
        } else if (inten > 4) {
            inten = 4;
        }
        pp = &lbl_8011A258[plyr];
        v = lbl_8011AE8C[inten];
        if (lbl_803445E0 == 0) {
            s32 pad = *pp;

            memset(act, 0, 6);
            act[0] = 0;
            act[1] = (u8)(s32)(v * 255.0f);
            scePadSetActDirect(pad / 4, pad & 3, act);
        }
        PADControlMotor(*pp, 1);
        lbl_80240E10[plyr * 2] = 1;
        lbl_80240E10[plyr * 2 + 1] = time;
    }
}

/* 0x80031A40  pump one pad's connection state machine (scePadGetState/
 * scePadInfoMode/press-mode negotiation, phases 0/0x28-0x2A/0x46-0x4D/99)
 * and joyReadPad it once stable.  SKELETON - full transcription pending. */
#pragma dont_inline on
s32 joyGetStatus(s32 pad, u8* buf)
{
    (void)pad;
    (void)buf;
    return 0;
}
#pragma dont_inline off

/* 0x80031E74  read one pad via scePadRead and translate the PS2-format
 * button/pressure report into the per-pad level words + per-player
 * button records (get_dir/calc_analog_stick/set*Button inlined).
 * SKELETON - full transcription pending. */
#pragma dont_inline on
s32 joyReadPad(s32 pad, u8* buf, s32 state)
{
    (void)pad;
    (void)buf;
    (void)state;
    return 0;
}
#pragma dont_inline off

/* 0x80032778  re-enable player controls (clears everything first) */
void EnablePlayerControls(void)
{
    int i, j;

    lbl_803445FC = 0;
    for (i = 0; i < 4; i++) {
        ClearPlayerControl(i, 0);
        for (j = 0; j < 12; j++) {
            lbl_80240828[i][j] = 0;
            lbl_802408E8[i][j] = 0;
        }
        lbl_80240F90[i] = 0;
    }
    ClearAllPlayerControls(4);
}

/* 0x80032814  disable player controls (clears everything, then flags) */
void DisablePlayerControls(void)
{
    int i, j;

    lbl_803445FC = 0;
    for (i = 0; i < 4; i++) {
        ClearPlayerControl(i, 0);
        for (j = 0; j < 12; j++) {
            lbl_80240828[i][j] = 0;
            lbl_802408E8[i][j] = 0;
        }
        lbl_80240F90[i] = 0;
    }
    ClearAllPlayerControls(4);
    lbl_803445FC = 1;
}

/* 0x800328B8  clear all per-player control state (no assignment reset) */
void ClearControls(void)
{
    int i, j;

    lbl_803445FC = 0;
    for (i = 0; i < 4; i++) {
        lbl_80240E30[i].ctl = 0;
        lbl_80240E30[i].levels = 0;
        lbl_80240E30[i].edges = 0;
        lbl_80240E30[i].repedges = 0;
        lbl_80240E30[i].spResult = 0;
        lbl_80240E30[i].spLast = 0;
        lbl_80240E30[i].spTimer = 0;
        lbl_80240E30[i].lx = 0.0f;
        lbl_80240E30[i].ly = 0.0f;
        lbl_80240E30[i].rx = 0.0f;
        lbl_80240E30[i].ry = 0.0f;
        for (j = 0; j < 12; j++) {
            lbl_80240828[i][j] = 0;
            lbl_802408E8[i][j] = 0;
        }
        lbl_80240F90[i] = 0;
    }
}

/* 0x80032964  full per-player init: button records, control structs,
 * repeat state, schemes/actuator flags, then the joystick angle table */
void InitPlayerControls(void)
{
    int i, j;

    lbl_803445FC = 0;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 13; j++) {
            *(u32*)(lbl_80240AE8[i] + j * 8) = 0;
            *(u32*)(lbl_80240AE8[i] + j * 8 + 4) = 0;
        }
        lbl_80240E30[i].ctl = 0;
        lbl_80240E30[i].levels = 0;
        lbl_80240E30[i].edges = 0;
        lbl_80240E30[i].repedges = 0;
        lbl_80240E30[i].spResult = 0;
        lbl_80240E30[i].spLast = 0;
        lbl_80240E30[i].spTimer = 0;
        lbl_80240E30[i].lx = 0.0f;
        lbl_80240E30[i].ly = 0.0f;
        lbl_80240E30[i].rx = 0.0f;
        lbl_80240E30[i].ry = 0.0f;
        for (j = 0; j < 12; j++) {
            lbl_80240828[i][j] = 0;
            lbl_802408E8[i][j] = 0;
        }
        lbl_80240F90[i] = 0;
        lbl_80240E30[i].scheme = 0;
        lbl_80240E30[i].hasActuator = 1;
        lbl_80240E30[i].unk34 = 1;
        lbl_80240E30[i].unk38 = 1;
    }
    InitJoyAng();
}

/* 0x80032A80  reset every player's control struct (negative code also
 * wipes the raw pad arrays first) */
void ClearAllPlayerControls(s32 code)
{
    int i;

    if (code < 0) {
        for (i = 0; i < 4; i++) {
            lbl_802407C8[i] = lbl_802407D8[i] = lbl_802407B8[i] = lbl_802407E8[i] = lbl_802407F8[i] = 0;
        }
        code = -code;
    }
    for (i = 0; i < 4; i++) {
        lbl_80240E30[i].ctl = code;
        lbl_80240E30[i].levels = 0;
        lbl_80240E30[i].edges = 0;
        lbl_80240E30[i].repedges = 0;
        lbl_80240E30[i].spResult = 0;
        lbl_80240E30[i].spLast = 0;
        lbl_80240E30[i].spTimer = 0;
        lbl_80240E30[i].lx = 0.0f;
        lbl_80240E30[i].ly = 0.0f;
        lbl_80240E30[i].rx = 0.0f;
        lbl_80240E30[i].ry = 0.0f;
        lbl_80240FC0[i] = 0;
        lbl_80240FB0[i] = 0;
        lbl_80240FA0[i] = 0;
        lbl_80240F80[i] = 0;
    }
    lbl_80344620 = 0;
    lbl_8034461C = 0;
    lbl_80344618 = 0;
}

/* 0x80032B3C  reset one player's control struct */
void ClearPlayerControl(s32 plyr, s32 code)
{
    lbl_80240E30[plyr].ctl = code;
    lbl_80240E30[plyr].levels = 0;
    lbl_80240E30[plyr].edges = 0;
    lbl_80240E30[plyr].repedges = 0;
    lbl_80240E30[plyr].spResult = 0;
    lbl_80240E30[plyr].spLast = 0;
    lbl_80240E30[plyr].spTimer = 0;
    lbl_80240E30[plyr].lx = 0.0f;
    lbl_80240E30[plyr].ly = 0.0f;
    lbl_80240E30[plyr].rx = 0.0f;
    lbl_80240E30[plyr].ry = 0.0f;
}

/* 0x80032B84  per-frame controls master (Xbox name: PlayerControls):
 * ReadControls -> ControlsUpdate (edge update) -> vibe timers -> per-player
 * CTL assembly (stick angle/mag from the JoyAng/JoyMag LUT or the staged
 * analog values) -> CheckSpecials -> edge debug print -> serve_memcard. */
void PlayerControls(void)
{
    int i;
    u8 unused[8];

    ReadControls();
    lbl_80344604 = 0;
    if (lbl_803445FC != 0) {
        for (i = 0; i < 4; i++) {
            ClearPlayerControl(i, 0);
        }
    } else {
        lbl_803445E4 = 400;
        ControlsUpdate();
        for (i = 0; i < 4; i++) {
            if (lbl_80240E10[i * 2] != 0) {
                s32 t = lbl_80240E10[i * 2 + 1] - 1;

                lbl_80240E10[i * 2 + 1] = t;
                if (t < 0) {
                    s32 pad;
                    u8 act[8];

                    lbl_80240E10[i * 2 + 1] = 0;
                    lbl_80240E10[i * 2] = 0;
                    pad = lbl_8011A258[i];
                    if (lbl_803445E0 == 0) {
                        memset(act, 0, 6);
                        scePadSetActDirect(pad / 4, pad & 3, act);
                    }
                    PADControlMotor(lbl_8011A258[i], 0);
                }
            }
        }
        for (i = 0; i < 4; i++) {
            s32 pad = lbl_8011A258[i];

            if (pad != -1 && lbl_80240E30[i].ctl == 0) {
                u32 lev = lbl_802407B8[pad];
                s32 dy, dx;

                lbl_80240E30[i].levels = lev;
                lbl_80240E30[i].edges = lbl_802407E8[pad];
                lbl_80240E30[i].repedges = lbl_802407F8[pad];
                dy = lbl_8011AE10.dir[(lev >> 4) & 0xF];
                dx = lbl_8011AE10.dir[lev & 0xF];
                if (lbl_80240F50[pad] > 0.33) {
                    lbl_80240E30[i].lx = lbl_80240F40[pad];
                    lbl_80240E30[i].ly = lbl_80240F50[pad];
                } else {
                    lbl_80240E30[i].lx = lbl_80240C88[dy][dx];
                    lbl_80240E30[i].ly = lbl_80240D4C[dy][dx];
                }
                if (lbl_80240E30[i].scheme == 2) {
                    if (lbl_80240F30[pad] > 0.5) {
                        lbl_80240E30[i].rx = lbl_80240F20[pad];
                        lbl_80240E30[i].ry = lbl_80240F30[pad];
                    } else {
                        u32 lev2 = lbl_802407D8[pad];

                        if (lev2 != 0) {
                            dy = lbl_8011AE10.dir[(lev2 >> 4) & 0xF];
                            dx = lbl_8011AE10.dir[lev2 & 0xF];
                            lbl_80240E30[i].rx = lbl_80240C88[dy][dx];
                            lbl_80240E30[i].ry = lbl_80240D4C[dy][dx];
                        } else {
                            lbl_80240E30[i].ry = 0.0f;
                        }
                    }
                }
            } else {
                if (lbl_80240E30[i].ctl != 0) {
                    lbl_80240E30[i].ctl--;
                }
                lbl_80240E30[i].levels = 0;
                lbl_80240E30[i].edges = 0;
                lbl_80240E30[i].repedges = 0;
                lbl_80240E30[i].lx = 0.0f;
                lbl_80240E30[i].ly = 0.0f;
                lbl_80240E30[i].rx = 0.0f;
                lbl_80240E30[i].ry = 0.0f;
            }
            if (lbl_80240E30[i].spTimer > 0) {
                lbl_80240E30[i].spResult = 0;
                lbl_80240E30[i].spTimer--;
            } else {
                lbl_80240E30[i].spResult = CheckSpecials(i, lbl_80240FC0[i]);
            }
            if (lbl_80240E30[i].spResult != 0) {
                lbl_80240E30[i].spTimer = 10;
                lbl_80240E30[i].spLast = lbl_80240E30[i].spResult;
            }
        }
        for (i = 0; i < 4; i++) {
            if ((s32)lbl_80240E30[i].edges != 0) {
                int b;

                for (b = 8; b <= 17; b++) {
                    if ((1 << b) & lbl_80240E30[i].edges) {
                        bulletproof_printf("Control %d has %s pressed.\n", i,
                                           lbl_8011AE10.names[b - 8]);
                    }
                }
            }
        }
        serve_memcard();
        lbl_803445E4 = 499;
    }
}

/* 0x80032F30  build the 7x7 joystick angle/magnitude lookup tables:
 * JoyAng = atan2 over the stick grid (axis cells pinned to 0/+-pi/2/pi),
 * JoyMag = special-move level for the cell's magnitude. */
void InitJoyAng(void)
{
    int i, j;
    f32 m, xx, x, y;

    x = -3.0f;
    for (i = 0; i < 7; i++) {
        xx = x * x;
        y = 3.0f;
        for (j = 0; j < 7; j++) {
            if (x == 0.0f && y == 0.0f) {
                lbl_80240C88[i][j] = 0.0f;
            } else if (x == 0.0f) {
                lbl_80240C88[i][j] = (y > 0.0f) ? 1.570796327 : -1.570796327;
            } else if (y == 0.0f) {
                lbl_80240C88[i][j] = (x > 0.0f) ? 0.0 : 3.141592654;
            } else {
                lbl_80240C88[i][j] = atan2(y, x);
            }
            m = y * y + xx;
            if (m > 0.0f) {
                volatile f32 res;
                f64 g = __frsqrte(m);

                g = 0.5 * g * (3.0 - g * g * m);
                g = 0.5 * g * (3.0 - g * g * m);
                g = 0.5 * g * (3.0 - g * g * m);
                res = (f32)(m * (0.5 * g * (3.0 - g * g * m)));
                m = res;
            }
            lbl_80240D4C[i][j] = lbl_8011AE10.lv[(s32)m];
            y -= 1.0;
        }
        x += 1.0;
    }
}

/* one per-bit button edge: e = level-bit, kept only on a fresh press */
#define BTN_EDGE(var, bit)                            \
    var = lev & (bit);                                \
    hit = 0;                                          \
    if (var != 0 && (old & (bit)) == 0) {             \
        hit = 1;                                      \
    }                                                 \
    if (!hit) {                                       \
        var = 0;                                      \
    }

/* 0x800330D4  edge/repeat-edge computation (Xbox name: PlayerControls):
 * consume ReadControls' staged levels ("updated" flag), map scheme turbo
 * buttons in (SpecialData rows), derive per-pad edges (per-direction
 * nibbles + 24 button bits) + auto-repeat edges (rep_speed ladder),
 * mirror the right-stick words, aggregate the all-player level/edge
 * words, and pump serve_mtap. */
void ControlsUpdate(void)
{
    u32 oldlev[4];
    s32 step;
    int i, plyr, t;

    lf_memcpy((u8*)oldlev, (u8*)lbl_802407B8, 16);
    lf_memcpy((u8*)lbl_802407C8, (u8*)lbl_802407B8, 16);
    if (ctrls_initialized != 0) {
        if (lbl_803445F8 == 0) {
            return;
        }
        lbl_803445F8 = 0;
        for (i = 0; i < 4; i++) {
            lbl_802407B8[i] = lbl_80240F70[i];
            lbl_802407D8[i] = lbl_80240F60[i];
        }
    } else {
        for (i = 0; i < 4; i++) {
            lbl_802407D8[i] = 0;
            lbl_802407B8[i] = 0;
        }
    }
    lbl_803445E4 = 300;
    lbl_803445F4 = 1;
    if (lbl_803445EC != 0) {
        if (lbl_803445DC == 0) {
            serve_mtap(0);
        } else {
            serve_mtap(1);
        }
    }
    step = lbl_80344578;
    lbl_80344618 = 0;
    lbl_80344620 = 0;
    lbl_8034461C = 0;
    for (plyr = 0; plyr < 4; plyr++) {
        s32 pad = lbl_8011A258[plyr];
        u32 lev, old, lev8, old8;
        u32 eD, eC, eA, eB;
        u32 e40000, e80000, e200, e400, e100, e8000, e10000, e800;
        u32 e1000, e4000, e2000, e20000, e200000, e100000, e800000;
        u32 e400000, e8000000, e1000000, e2000000, e4000000;
        u32 e20000000, e10000000, e80000000, e40000000;
        s32 hit;

        if (pad == -1) {
            pad = plyr;
        }
        old = oldlev[pad];
        lev = lbl_802407B8[pad];
        old8 = old & 0xFF;
        lev8 = lev & 0xFF;
        if (lev != 0) {
            for (t = 0; t < 10; t++) {
                if ((lbl_8011ACD0[t][lbl_80240E30[plyr].scheme * 2] & lev) != 0 ||
                    (lbl_8011ACD0[t][lbl_80240E30[plyr].scheme * 2 + 1] & lev) != 0) {
                    lev = lev | 1 << (t + 8);
                }
            }
            lbl_802407B8[pad] = lev;
            if (lev8 >> 6 == 0 || (old8 & 0xC0) != 0) {
                eD = 0;
            } else {
                eD = (lev8 >> 6) << 6;
            }
            eC = lev8 >> 4 & 3;
            if (eC == 0 || (old8 & 0x30) != 0) {
                eC = 0;
            } else {
                eC = eC << 4;
            }
            eA = lev & 3;
            if (eA == 0 || (old8 & 3) != 0) {
                eA = 0;
            }
            eB = lev8 >> 2 & 3;
            if (eB == 0 || (old8 & 0xC) != 0) {
                eB = 0;
            } else {
                eB = eB << 2;
            }
            BTN_EDGE(e40000, 0x40000)
            BTN_EDGE(e80000, 0x80000)
            BTN_EDGE(e200, 0x200)
            BTN_EDGE(e400, 0x400)
            BTN_EDGE(e100, 0x100)
            BTN_EDGE(e8000, 0x8000)
            BTN_EDGE(e10000, 0x10000)
            BTN_EDGE(e800, 0x800)
            BTN_EDGE(e1000, 0x1000)
            BTN_EDGE(e4000, 0x4000)
            BTN_EDGE(e2000, 0x2000)
            BTN_EDGE(e20000, 0x20000)
            BTN_EDGE(e200000, 0x200000)
            BTN_EDGE(e100000, 0x100000)
            BTN_EDGE(e800000, 0x800000)
            BTN_EDGE(e400000, 0x400000)
            BTN_EDGE(e8000000, 0x8000000)
            BTN_EDGE(e1000000, 0x1000000)
            BTN_EDGE(e2000000, 0x2000000)
            BTN_EDGE(e4000000, 0x4000000)
            BTN_EDGE(e20000000, 0x20000000)
            BTN_EDGE(e10000000, 0x10000000)
            BTN_EDGE(e80000000, 0x80000000)
            BTN_EDGE(e40000000, 0x40000000)
            lbl_802407E8[pad] = e40000000 | e80000000 | e10000000 | e20000000 | e4000000 |
                                e2000000 | e1000000 | e8000000 | e400000 | e800000 | e100000 |
                                e200000 | e20000 | e2000 | e4000 | e1000 | e800 | e10000 | e8000 |
                                e100 | e400 | e200 | e80000 | e40000 | eD | eC | eA | eB;
            lbl_802407F8[pad] = lbl_802407E8[pad];
            lev = lbl_802407B8[pad];
            if (lev == 0 || old != lev) {
                lbl_80240808[pad] = 0;
                lbl_80240818[pad] = 0;
            } else {
                lbl_80240808[pad] += step;
                if ((u32)lbl_8011A220[lbl_80240818[pad]] <= lbl_80240808[pad]) {
                    lbl_80240808[pad] -= lbl_8011A220[lbl_80240818[pad]];
                    lbl_80240818[pad]++;
                    if (lbl_8011A220[lbl_80240818[pad]] < 1) {
                        lbl_80240818[pad]--;
                    }
                    lbl_802407F8[pad] |= lev;
                }
            }
        } else {
            lbl_802407E8[pad] = 0;
            lbl_802407F8[pad] = 0;
        }
    }
    for (i = 0; i < 2; i++) {
        u32 lev, edges;

        lbl_80240FC0[i] = lbl_802407B8[i];
        lev = lbl_80240FC0[i];
        edges = lev & (lbl_80240F90[i] ^ lev);
        lbl_80240FB0[i] = edges;
        lbl_80240FA0[i] = lbl_80240FB0[i];
        if (lev == 0 || lev != lbl_80240F90[i]) {
            lbl_80240828[i][0] = 0;
            lbl_802408E8[i][0] = 0;
        } else {
            lbl_80240828[i][0] += step;
            if ((s32)lbl_80240828[i][0] >= lbl_8011A220[lbl_802408E8[i][0]]) {
                lbl_80240828[i][0] -= lbl_8011A220[lbl_802408E8[i][0]];
                lbl_802408E8[i][0]++;
                if (lbl_8011A220[lbl_802408E8[i][0]] < 1) {
                    lbl_802408E8[i][0]--;
                }
                lbl_80240FA0[i] |= lev;
            }
        }
        lbl_8034461C |= edges;
        lbl_80344620 |= lev;
        lbl_80344618 |= lbl_80240FA0[i];
        lbl_80240F90[i] = lev;
    }
    for (i = 0; i < 4; i++) {
        lbl_80240F60[i] = 0;
        lbl_80240F70[i] = 0;
    }
    lbl_803445F4 = 0;
    lbl_803445E4 = 399;
}

/* 0x80033A60  special-move (combo) recognizer: run each SpecialMoves
 * program over this player's level word; per-move progress lives in the
 * 13 {stage,count} pairs of lbl_80240AE8[plyr]. */
s32 CheckSpecials(s32 plyr, u32 lev)
{
    s32 found;
    u32 i;
    u8 unused[16];

    found = 0;
    for (i = 0; i < 13; i++) {
        SMOVE* mv = &lbl_8011A2A8[i];
        s32* rec;
        u32 l;

        if (mv->byteMode != 0) {
            l = lev & 0xFF;
        } else {
            l = lev;
        }
        rec = (s32*)(lbl_80240AE8[plyr] + i * 8);
        for (;;) {
            s32 stage = rec[0];
            u32 mask = mv->st[stage].mask;
            u32 m;
            s32 b8000, b4000;

            if ((mask & 0x8000) != 0) {
                b8000 = 1;
            } else {
                b8000 = 0;
            }
            if ((mask & 0x4000) != 0) {
                b4000 = 1;
            } else {
                b4000 = 0;
            }
            m = mask & 0xFFFF7FFF;
            if (stage >= 16) {
                FatalErrorf("Error! Special Move %d stage = %d > %d", i, stage, 16);
            }
            if (m == 0xFFF) {
                found = mv->id;
                goto next_move;
            }
            if (l == m || (b8000 != 0 && (l & m) != 0) || (b4000 != 0 && (l & m) == 0)) {
                s32* ph;
                s32 hv;

                ph = &mv->st[stage].hold;
                hv = *ph;
                if (hv == -1) {
                    found = mv->id;
                } else {
                    s32 h;

                    rec[1]++;
                    h = *ph;
                    switch (h) {
                    case 0:
                        h = 6;
                        break;
                    }
                    if (rec[1] > h) {
                        rec[0] = 0;
                        rec[1] = 0;
                    }
                }
                goto next_move;
            }
            if (rec[1] < mv->st[stage].min) {
                break;
            }
            rec[0]++;
            rec[1] = 0;
        }
        rec[0] = 0;
        rec[1] = 0;
    next_move:
        if (found != 0) {
            break;
        }
    }
    if (found != 0) {
        for (i = 0; i < 13; i++) {
            s32* rec = (s32*)(lbl_80240AE8[plyr] + i * 8);

            rec[0] = 0;
            rec[1] = 0;
        }
    }
    return found;
}

/* 0x80033C5C  top-level pad read: joyGetStatus every pad, assemble raw
 * levels + analog sticks (JoyAng/JoyMag lookup, fn_80034C88 sqrt,
 * atan2) into the staged arrays for ControlsUpdate.
 * SKELETON - full transcription pending. */
void ReadControls(void)
{
}

/* 0x800347A0  one-time controls init */
void InitControls(void)
{
    int i;

    init_controls();
    for (i = 0; i < 4; i++) {
        lbl_802407C8[i] = lbl_802407D8[i] = lbl_802407B8[i] = lbl_802407E8[i] = lbl_802407F8[i] = 0;
    }
    init_all_dir_info();
    ctrls_initialized = 1;
}

/* 0x8003480C  poll both multitap ports; latch new connections */
void serve_mtap(s32 which)
{
    int port;
    u8 unused[8];

    lbl_803445E4 = 100;
    for (port = 0; port < 2; port++) {
        while (lbl_803445E0 != 0) {
        }
        lbl_803445E0 = 1;
        if (sceMtapPortOpen(port) != 0 && lbl_80344610[port] != 2) {
            lbl_80344610[port] = 2;
            lbl_803445D8 &= ~0xF00;
            bulletproof_printf("MTAP %d CONNECTED\n", port);
            if (sceMtapGetConnection(port + 2) != 0 && sceMtapPortOpen(port + 2) != 0) {
                lbl_80344608[port] = 2;
            }
        }
        lbl_803445E0 = 0;
    }
    lbl_803445E4 = 199;
}

/* 0x800348EC  press level (0-3) for one button record: bit0 = present,
 * bit1 = analog-capable, byte1 = pressure */
s32 get_button(u8* rec, s32 no_analog)
{
    if ((rec[0] & 1) != 0) {
        if ((rec[0] & 2) != 0) {
            if (no_analog != 0) {
                if (rec[1] > 0x20) {
                    return 3;
                }
            } else {
                if (rec[1] > 0x80) {
                    return 3;
                }
                if (rec[1] > 0x70) {
                    return 2;
                }
                if (rec[1] > 0x60) {
                    return 1;
                }
            }
        } else {
            if (rec[1] != 0) {
                return 3;
            }
            return 0;
        }
    }
    return 0;
}

/* 0x80034974  open the pad/mtap driver layer and every pad port */
void init_controls(void)
{
    int i;
    s32 pad;

    lbl_803445D8 = 0;
    lbl_803445DC = 0;
    for (i = 0; i < 2; i++) {
        lbl_80344610[i] = 3;
        lbl_80344608[i] = 3;
    }
    scePadPortClose(0);
    if (lbl_803445EC < 0) {
        lbl_803445EC = 0;
        ErrorPrintf("Disabling MTAP (load IRX failed)\n");
    } else {
        lbl_803445EC = sceMtapInit();
    }
    if (lbl_803445EC == 0) {
        lbl_80344610[0] = 1;
        lbl_80344610[1] = 1;
        lbl_80344608[0] = 1;
        lbl_80344608[1] = 1;
    } else {
        for (i = 0; i < 2; i++) {
            MtapOpenPort(i, 1);
        }
    }
    for (pad = 0; pad < 4; pad++) {
        s32 port = pad / 4;
        s32 opened;

        if (lbl_8011A288[port * 4 + (pad & 3)] == 0) {
            opened = scePadPortOpen(port, pad & 3, &lbl_802409A8[(port * 4 + (pad & 3)) * 64]);
            if (opened) {
                lbl_8011A288[port * 4 + (pad & 3)] = 1;
            }
        } else {
            opened = 1;
        }
        if (!opened) {
            bulletproof_printf("Unable to open pad port %d %d\n", port, pad & 3);
        }
    }
    memset(lbl_80240AA8, 0, 0x10);
    memset(lbl_80240AB8, 0, 0x10);
    memset(lbl_80240AC8, 0, 0x10);
    memset(lbl_80240AD8, 0, 0x10);
    memset(&lbl_803445E8, 0, 4);
}

/* 0x80034B3C  bring one multitap port up (joyGetStatus pump + open) */
void MtapOpenPort(s32 port, s32 flag)
{
    if (lbl_803445EC != 0) {
        if (lbl_80344610[port] != 1) {
            s32 ok;
            u8 unused[16];

            while (lbl_803445E0 != 0) {
            }
            joyGetStatus(port * 4, NULL);
            if (lbl_80344610[port] != 2 && lbl_80240AC8[port * 4] == 0) {
                lbl_803445E0 = 1;
                lbl_803445E4 = 0x3E9;
                if (lbl_80343BE0[port] == 0) {
                    ok = sceMtapGetConnection(port);
                    lbl_80343BE0[port] = 1;
                } else {
                    ok = 1;
                }
                lbl_803445E4 = 0x3EA;
                if (ok != 0) {
                    ok = sceMtapPortOpen(port);
                    lbl_803445E4 = 0x3EB;
                    if (ok != 0) {
                        lbl_80344610[port] = 2;
                        lbl_803445D8 &= ~0xF00;
                        bulletproof_printf("MTAP %d OPEN\n", port);
                        lbl_803445E4 = 0x3F5;
                        sceMtapGetConnection(port + 2);
                        ok = sceMtapPortOpen(port + 2);
                        if (ok != 0) {
                            lbl_80344608[port] = 2;
                        }
                    }
                }
                lbl_803445E4 = 0x44B;
                lbl_803445E0 = 0;
            }
        }
    }
    (void)flag;
}

/* 0x80034C88  float sqrt via frsqrte + 4 Newton refinements (same idiom
 * as g3dpad.c g3dSqrt; GC-only, no Xbox counterpart) */
f32 fn_80034C88(f32 x)
{
    volatile f32 result;

    if (x > 0.0f) {
        f64 y = __frsqrte(x);

        y = 0.5 * y * (3.0 - y * y * x);
        y = 0.5 * y * (3.0 - y * y * x);
        y = 0.5 * y * (3.0 - y * y * x);
        result = (f32)(x * (0.5 * y * (3.0 - y * y * x)));
        return result;
    }
    return x;
}
