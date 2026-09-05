#include "types.h"
#include "game/leveldata.h"
#include "game/player.h"

/* Initialized SOUNDS tables, recovered from GameCube data. Xbox PDB array
 * dimensions corroborate the independent tables (including MovieBanks,
 * MovieMusic, food and turbo IDs); GC consumers and bytes decide order.
 * Existing exported labels are retained for cross-TU references. Original
 * file/function scope is not claimed: the Xbox procedures differ in order.
 * MWCC naturally pools these arrays, including unused movie-bank entries.
 * Do not replace them with a padded struct or cross-array pointer walk. */
long lbl_801232C8[5] = {127, 127, 127, 127, 127};
char lbl_801232DC[6][8] = {"MET", "ROPE", "CHAIN", "ICE", "STONE", "*ROCK"};
char lbl_8012330C[16] = "abcdefghijk";
static char* MovieBanks[32] = {"title.s", "story.s", "wizmus.s", "valmus.s", "warmus.s", "arcmus.s", "grunt_a.s", "grunt_b.s", "grunt_c.s", "grunt_d.s", "demon_a", "demon_b.s", "demon_c.s", "demon_d.s", "key.s", "atari.s", "3dfxsplash.s", "kata-sor.s", "kata-kni.s", "kata-dwf.s", "kata-jes.s", "grunt_g.s", "demon_k.s", "grunt_i.s", "demon_i.s", "grunt_j.s", "grunt_h.s", "attr-newgame.s", "attr-playme.s", "gamegiveaway.s", "skorne2garm.s", "completion.s"};
static int MovieMusic[32] = {1, 1, -1, -1, -1, -1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
long lbl_8012341C[14] = {-1, 0xf0009, 0x100009, 0x110008, 0x120008, 0x130005, 0x140005, 0x150008, 0x160007, 0x170008, 0x180009, 0x190008, -1, 0x0};
long lbl_80123454[14] = {-1, 0x1a0001, 0x1b0001, 0x1c0001, 0x1d0001, 0x1e0001, 0x1f0001, 0x200001, 0x210001, 0x220001, 0x230001, 0x240001, -1, 0x0};
long lbl_8012348C[11] = {0x10032, 0x10033, 0x10034, 0x10035, 0x10036, 0x10037, 0x10038, 0x10039, 0x1003a, 0x1003b, 0x1003c};
long lbl_801234B8[11] = {0x2000b, 0x2000a, 0x20009, 0x20008, 0x20007, 0x20006, 0x20005, 0x20004, 0x20003, 0x20002, 0x20001};
long lbl_801234E4[8][4] = {
    {0x40000, 0x40001, 0x40002, 0x40003},
    {0x50000, 0x50001, 0x50002, 0x50003},
    {0x60000, 0x60001, 0x60002, 0x60003},
    {0x70000, 0x70001, 0x70002, 0x70003},
    {0x80000, 0x80001, 0x80002, 0x80003},
    {0x90000, 0x90001, 0x90002, 0x90003},
    {0xa0000, 0xa0001, 0xa0002, 0xa0003},
    {0xb0000, 0xb0001, 0xb0002, 0xb0003}
};
long lbl_80123564[8] = {0x40004, 0x50004, 0x60004, 0x70004, 0x80004, 0x90004, 0xa0004, 0xb0004};
static int snd_eat[8][4] = {
    {0x40008, 0x40008, 0x40008, 0x40008},
    {0x50008, 0x50008, 0x50008, 0x50008},
    {0x60008, 0x60008, 0x60008, 0x60008},
    {0x70008, 0x70009, 0x7000a, 0x7000b},
    {0x80008, 0x80008, 0x80008, 0x80008},
    {0x90008, 0x90008, 0x90008, 0x90008},
    {0xa0008, 0xa0008, 0xa0008, 0xa0008},
    {0xb0008, 0xb0008, 0xb0008, 0xb0008}
};
static int snd_eat_default[8] = {0x40006, 0x50006, 0x60006, 0x70006, 0x80006, 0x90006, 0xa0006, 0xb0006};
long lbl_80123624[8] = {0x40007, 0x50007, 0x60007, 0x70007, 0x80007, 0x90007, 0xa0007, 0xb0007};
long lbl_80123644[8] = {0x40009, 0x50009, 0x60009, 0x7000c, 0x80009, 0x90009, 0xa0009, 0xb0009};
long lbl_80123664[8] = {0x4000a, 0x5000a, 0x6000a, 0x7000d, 0x8000a, 0x9000a, 0xa000a, 0xb000a};
long lbl_80123684[8] = {0x4000b, 0x5000b, 0x6000b, 0x7000e, 0x8000b, 0x9000b, 0xa000b, 0xb000b};
long lbl_801236A4[8] = {0x4000c, 0x5000c, 0x6000c, 0x7000f, 0x8000c, 0x9000c, 0xa000c, 0xb000c};
static int snd_turboA[8][4] = {
    {0x4000e, 0x4000f, 0x40010, 0x4000d},
    {0x5000e, 0x5000f, 0x50010, 0x5000d},
    {0x6000e, 0x6000f, 0x60010, 0x6000d},
    {0x70011, 0x70012, 0x70013, 0x70010},
    {0x8000e, 0x8000f, 0x80010, 0x8000d},
    {0x9000e, 0x9000f, 0x90010, 0x9000d},
    {0xa000e, 0xa000f, 0xa0010, 0xa000d},
    {0xb0010, 0xb0011, 0xb0012, 0xb000d}
};
static int snd_turboB[8] = {0x40011, 0x50011, 0x60011, 0x70014, 0x80011, 0x90011, 0xa0012, 0xb0013};
static int snd_turboC[8] = {0x40012, 0x50012, 0x60012, 0x70015, 0x80012, 0x90012, 0xa0013, 0xb0014};
long lbl_80123784[14] = {-1, 0x2f001b, 0x2e000e, 0x300018, 0x310016, -1, -1, 0x34001c, -1, 0x320015, 0x350013, 0x330015, -1, 0x0};
long lbl_801237BC[5][4] = {
    {0x2f, 0x2f, 0x2f, 0x2f},
    {0x31, 0x31, 0x31, 0x31},
    {0x33, 0x33, 0x33, 0x33},
    {0x35, 0x35, 0x35, 0x35},
    {0x36, 0x36, 0x36, 0x36}
};
long lbl_8012380C[8] = {0x40005, 0x50005, 0x60005, 0x70005, 0x80005, 0x90005, 0xa0005, 0xb0005};
long lbl_8012382C[14][7] = {
    {-1, -1, -1, -1, -1, -1, -1},
    {0x25000f, -1, 0x25000d, 0x250000, 0x250002, 0x250002, -1},
    {-1, 0x260027, -1, -1, -1, -1, -1},
    {0x270031, 0x270032, 0x270030, -1, -1, -1, 0x270033},
    {0x28002d, 0x280000, -1, 0x28002c, 0x28002e, 0x28002e, -1},
    {-1, -1, -1, -1, -1, -1, -1},
    {-1, 0x38001f, -1, -1, -1, -1, -1},
    {-1, 0x290028, -1, -1, -1, -1, -1},
    {-1, 0x2a001c, -1, -1, -1, -1, -1},
    {-1, 0x2b0028, -1, -1, -1, -1, -1},
    {-1, 0x2c002a, -1, -1, -1, -1, -1},
    {-1, 0x2d0028, -1, -1, -1, -1, -1},
    {-1, -1, -1, -1, -1, -1, -1},
    {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0}
};
long lbl_801239B4[2][5] = {
    {0x7, 0x7, 0x6, 0x8, 0x9},
    {0x4d, 0x4d, 0x4c, 0x4e, 0x4f}
};
long lbl_801239DC[2][5] = {
    {0x1c, 0x20, 0x1e, 0x1a, 0x22},
    {0x1d, 0x21, 0x1f, 0x1b, 0x23}
};
long lbl_80123A04[14] = {-1, 0x250007, 0x260024, 0x270020, 0x280003, -1, -1, 0x290025, 0x2a0019, 0x2b0025, 0x2c0027, 0x2d0025, -1, 0x0};
long lbl_80123A3C[14] = {-1, 0x250008, 0x260025, 0x270021, 0x280004, -1, -1, 0x290026, 0x2a001a, 0x2b0026, 0x2c0028, 0x2d0026, -1, 0x0};
long lbl_80123A74[14] = {-1, 0x250009, 0x260026, 0x270022, 0x280005, -1, -1, 0x290027, 0x2a001b, 0x2b0027, 0x2c0029, 0x2d0027, -1, 0x0};
long lbl_80123AAC[14] = {-1, 0x250041, 0x260001, 0x270027, 0x280001, 0x370000, 0x38001e, 0x290034, 0x2a002c, 0x2b0049, -1, -1, -1, 0x0};
long lbl_80123AE4[14] = {-1, -1, -1, -1, 0x280002, -1, -1, -1, -1, 0x2b004b, -1, -1, -1, 0x0};
long lbl_80123B1C[14] = {-1, 0x260002, -1, -1, -1, -1, -1, -1, -1, 0x2b004c, -1, -1, -1, 0x0};
long lbl_80123B54[14] = {-1, 0x260003, -1, -1, -1, -1, -1, -1, -1, 0x2b004d, -1, -1, -1, 0x0};
long lbl_80123B8C[2][14] = {
    {-1, 0x25000a, -1, 0x270023, 0x280006, 0x36001a, 0x38001d, 0x29002a, -1, 0x2b004e, 0x2c002c, 0x2d002a, -1, 0x0},
    {-1, -1, -1, 0x27002c, -1, -1, -1, -1, -1, -1, -1, 0x2d0033, -1, 0x0}
};
long lbl_80123BFC[14] = {-1, 0x25000b, -1, 0x270024, 0x280007, -1, -1, -1, 0x2a0027, 0x2b004f, -1, -1, -1, 0x0};
long lbl_80123C34[14] = {-1, 0x25000c, -1, 0x270025, 0x280008, -1, -1, -1, 0x2a0028, 0x2b0050, -1, -1, -1, 0x0};
long lbl_80123C6C[14][4] = {
    {-1, -1, -1, -1},
    {0x250006, 0x250004, 0x250005, 0x250003},
    {0x260023, 0x260023, 0x260023, 0x260023},
    {0x27001d, 0x27001e, 0x27001f, 0x27001d},
    {0x28002b, 0x28002b, 0x28002b, 0x28002b},
    {-1, -1, -1, -1},
    {-1, -1, -1, -1},
    {0x290024, 0x290024, 0x290024, 0x290041},
    {0x2a0018, 0x2a0018, 0x2a0018, 0x2a0018},
    {0x2b0024, 0x2b0024, 0x2b0024, 0x2b0024},
    {0x2c0026, 0x2c0026, 0x2c0026, 0x2c004a},
    {0x2d0024, 0x2d0024, 0x2d0024, 0x2d0024},
    {-1, -1, -1, -1},
    {0x0, 0x0, 0x0, 0x0}
};
static int bronze[4] = {0x3b0000, 0x3b0003, 0x3b0006, 0x3b0009};
static int silver[4] = {0x3b0001, 0x3b0004, 0x3b0007, 0x3b000a};
static int gold[4] = {0x3b0002, 0x3b0005, 0x3b0008, 0x3b000b};
long lbl_80123D7C[16][9] = {
    {0xe0048, 0xe0049, 0xe004a, 0xe004b, 0xe004c, 0xe004d, 0xe004e, 0xe004f, 0xe0050},
    {0xe0024, 0xe0025, 0xe0026, 0xe0027, 0xe0028, 0xe0029, 0xe002a, 0xe002b, 0xe002c},
    {0xe006c, 0xe006d, 0xe006e, 0xe006f, 0xe0070, 0xe0071, 0xe0072, 0xe0073, 0xe0074},
    {0xe0000, 0xe0001, 0xe0002, 0xe0003, 0xe0004, 0xe0005, 0xe0006, 0xe0007, 0xe0008},
    {0xe005a, 0xe005b, 0xe005c, 0xe005d, 0xe005e, 0xe005f, 0xe0060, 0xe0061, 0xe0062},
    {0xe0036, 0xe0037, 0xe0038, 0xe0039, 0xe003a, 0xe003b, 0xe003c, 0xe003d, 0xe003e},
    {0xe007e, 0xe007f, 0xe0080, 0xe0081, 0xe0082, 0xe0083, 0xe0084, 0xe0085, 0xe0086},
    {0xe0012, 0xe0013, 0xe0014, 0xe0015, 0xe0016, 0xe0017, 0xe0018, 0xe0019, 0xe001a},
    {0xe0051, 0xe0052, 0xe0053, 0xe0054, 0xe0055, 0xe0056, 0xe0057, 0xe0058, 0xe0059},
    {0xe002d, 0xe002e, 0xe002f, 0xe0030, 0xe0031, 0xe0032, 0xe0033, 0xe0034, 0xe0035},
    {0xe0075, 0xe0076, 0xe0077, 0xe0078, 0xe0079, 0xe007a, 0xe007b, 0xe007c, 0xe007d},
    {0xe0009, 0xe000a, 0xe000b, 0xe000c, 0xe000d, 0xe000e, 0xe000f, 0xe0010, 0xe0011},
    {0xe0063, 0xe0064, 0xe0065, 0xe0066, 0xe0067, 0xe0068, 0xe0069, 0xe006a, 0xe006b},
    {0xe003f, 0xe0040, 0xe0041, 0xe0042, 0xe0043, 0xe0044, 0xe0045, 0xe0046, 0xe0047},
    {0xe0087, 0xe0088, 0xe0089, 0xe008a, 0xe008b, 0xe008c, 0xe008d, 0xe008e, 0xe008f},
    {0xe001b, 0xe001c, 0xe001d, 0xe001e, 0xe001f, 0xe0020, 0xe0021, 0xe0022, 0xe0023}
};
static int legend_snd1[11] = {0x2f001e, 0x2e000f, 0x300019, 0x310017, 0x370017, -1, 0x340021, -1, 0x320016, 0x350014, 0x330016};
static int legend_snd2[11] = {0x2f001f, 0x2e0010, 0x30001a, -1, 0x370019, -1, -1, -1, 0x320018, 0x350016, 0x330017};
static int legend_snd3[11] = {0x2f0020, 0x2e0011, 0x30001b, 0x310019, 0x37001a, -1, 0x340024, -1, 0x320019, 0x350017, 0x330018};
static int legend_snd4[11] = {0x2f0021, 0x2e0012, 0x30001c, 0x31001a, 0x370018, -1, 0x340022, -1, 0x320017, 0x350015, 0x330019};
long lbl_8012406C[14] = {-1, 0x25000e, -1, 0x270026, -1, -1, -1, 0x290029, 0x2a002b, 0x2b0029, 0x2c002b, 0x2d0029, 0x3b001a, 0x0};
long lbl_801240A4[14] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, 0x2b0047, -1, -1, -1, 0x0};
long lbl_801240DC[14] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, 0x2b0048, -1, -1, -1, 0x0};
long lbl_80124114[13] = {-1, 0x250034, 0x260030, 0x27003c, 0x280038, 0x360019, 0x38001c, 0x290023, 0x2a0017, 0x2b0023, 0x2c0023, 0x2d0023, -1};
long lbl_80124148[13] = {-1, 0x250033, 0x26002f, 0x27003b, 0x280037, 0x360018, 0x38001b, 0x290022, 0x2a0016, 0x2b0022, 0x2c0022, 0x2d0022, -1};
long lbl_8012417C[11][8] = {
    {0x2e000b, 0x2e000c, 0x2e000d, 0x2e000d, 0x2e000d, -1, -1, -1},
    {0x2f0018, 0x2f0019, 0x2f001a, 0x2f001a, 0x2f001a, -1, -1, -1},
    {0x300015, 0x300016, 0x300017, 0x300017, 0x300017, -1, -1, -1},
    {0x310013, 0x310014, 0x310015, 0x310015, 0x310015, -1, -1, -1},
    {0x330011, 0x330012, 0x330013, 0x330014, 0x330014, -1, -1, -1},
    {0x320011, 0x320012, 0x320013, 0x320014, 0x320014, -1, -1, -1},
    {0x35000f, 0x350010, 0x350011, 0x350012, 0x350012, -1, -1, -1},
    {0x340018, 0x340019, 0x34001a, 0x34001b, 0x34001b, -1, -1, -1},
    {0x370015, 0x370016, -1, -1, -1, -1, -1, -1},
    {0x390014, 0x390014, -1, -1, -1, -1, -1, -1},
    {0x3a0014, 0x3a0014, -1, -1, -1, -1, -1, -1}
};
long lbl_801242DC[9] = {-1, 0xe0093, 0xe0094, 0xe0095, 0xe0096, 0xe0097, 0xe0092, 0xe0090, 0xe0091};
long lbl_80124300[9] = {-1, 0xe009b, 0xe009c, 0xe009d, 0xe009e, 0xe009f, 0xe009a, 0xe0098, 0xe0099};
long lbl_80124324[3] = {0xe00a0, 0xe00a1, 0xe00a2};
long lbl_80124330[4] = {0x10057, 0x10045, 0x10058, 0x10047};
long lbl_80124340[4] = {0x10055, 0x10046, 0x10056, 0x10048};

#define offsetof(type, member) ((u32)&(((type*)0)->member))

/* struct audio_data -- the per-level audio descriptor level_data.audio points
 * at.  leveldata.h only forward-declares it, so the body is completed here as
 * a file-local view rather than in the shared header.
 * Layout from the Xbox PDB (audio_data); the three fields this TU actually
 * touches are corroborated on GC by BOTH their access width and their use
 * site: entersnd@0x10 is the s16 read by AudioEnterNextStage, hitsnd@0x12 the
 * s16 read by AudioExplodeWall, namesnd@0x14 the s32 feeding
 * AudioEnterNextStage's announcer queue -- each matching the PDB field's
 * declared size (2/2/4).  The remaining fields carry PDB names unverified on
 * GC; verify a displacement against target asm before relying on one. */
struct audio_data {
    /* 0x00 */ char bank[16];
    /* 0x10 */ s16  entersnd;
    /* 0x12 */ s16  hitsnd;
    /* 0x14 */ s32  namesnd;
    /* 0x18 */ char stream[16];
    /* 0x28 */ s16  nareas;
    /* 0x2A */ s16  stereo;
    /* 0x2C */ s16  nparts[8];
};

/* struct sound_data -- the 24-byte per-event sound record this TU indexes as
 * `*(u8**)(gWorldData + 44) + idx * 24`.  Four independent confirmations:
 * the Xbox PDB record `sound_data` (audio.h Id=3271) is exactly 0x18, its
 * idx/vol/pri offsets and widths (4/2/2 @0x10/0x14/0x16) match every access
 * here, the owning world record declares `struct sound_data* sounds` at
 * Offset=0x2c -- i.e. the 44 in the base expression -- and this file's own
 * reconstruction had already named the two s16s `atten` and `priority`.
 * Accesses stay on the raw pointer with offsetof-spelled displacements: `e`
 * is an index-computed base touching three nearby fields, the exact shape
 * claim.law.multifield-alias-defeats-indexed-addressing warns a typed alias
 * regresses. */
struct sound_data {
    /* 0x00 */ char desc[16];
    /* 0x10 */ s32  idx;
    /* 0x14 */ s16  vol;
    /* 0x16 */ s16  pri;
};

/* ------------------------------------------------------------------------
 * Front slice of the SOUNDS audio module (Xbox SOUNDS.OBJ), covering the
 * game-event sound-trigger helpers in 0x8009C2CC-0x800A00A0 (~118 fns).
 * The tail name/speech/music slice lives in game/sound/sounds.c (0x800A00A0+);
 * they share data/pool context, but their original GameCube TU boundary is
 * unresolved. The Xbox SOUNDS.OBJ grouping alone does not establish it.
 *
 * NonMatching: dtk supplies the original bytes so the DOL stays byte-exact;
 * the object is compiled only for per-function objdiff comparison.
 *
 * Each function is a thin wrapper that fires one/few sound events through the
 * sndFx* engine (game/audio/sndfx.c) or queues announcer voice via
 * sndFxQueAddEx (gated on good_wiz_state<=2).  Precise per-event Xbox PDB
 * names (AudioRotator/AudioElevator/AudioEnemyDies/...) could NOT be pinned
 * 1:1: no sound-id->name table exists and GC function order != Xbox order.
 * They are therefore left as fn_ pending an id table; the caller-domain and
 * play-primitive of every function are recorded in the scout report.
 *
 * Food/turbo accesses use the recovered independent arrays above. The
 * original compiler pools their addressing naturally; the former padded
 * views are not needed. Boss-stream formats are ordinary string literals.
 * AudioSetupBossStreams still has one extra instruction, and its jump-table
 * destinations therefore remain four bytes later than retail. Consult the
 * memory graph and fresh reports for current function-level status.
 * ---------------------------------------------------------------------- */

/* --- sound-engine callees (game/audio/sndfx.c + audio.c, 0x8001xxxx) --- */
extern f32 sndFxQueAddEx(int mode, int soundId, f32 vol, f32 param, int pri, int track, int flags);
extern f32 sndFxQueAdd(int soundId, f32 vol, f32 param, int pri, int track, int flags);
extern void sndFxPlay3D(int soundId, int pos, int p2, int flags);
extern void sndFxPlay3DAtten(int soundId, int pos, int p2, int flags);
extern void sndFxPlay3DTracked(int soundId, int pos, int p2, int flags);
extern int sndFxPlayEx(int soundId, int p1, int pan, int flags);
extern int sndFxPlayHandle(int a, int b, int c);
extern int AudioWithName(int id, int pidx, f32 vol, int s4, int s5);
extern void AudioKillBySound(int soundId);
extern f32 AudioGetSoundVol(int a);
extern int AudioSoundExists(int a);
extern int AudioMaskByEvent(int a);
extern void AudioKillMask(void);
extern int AudioAng(int a);
extern void AudioSetTrackPan(int a, int b);
extern int RandInt(int a);

/* --- module data --- */
extern s32 lbl_803448B4;   /* SDA index into lbl_80123454 */
extern u8 sSpeechNameBuf[]; /* speech scratch buffer; aliases id tables at offsets */
extern s32 lbl_8028B610[][2]; /* [idx] -> {id1, id2} event-follow pairs */
extern Player gPlayers[4]; /* 0x80275AE0 player records, stride 0x335C */
extern s32 sVoiceRotIdxA;  /* 0..3 announcer-voice rotation counter */
extern s32 sVoiceRotIdxB;  /* 0..3 announcer-voice rotation counter */
extern s32 good_wiz_state; /* <=2 => attract/menu path (announcer allowed) */
extern s32 lbl_803447B4;   /* gate flag */
extern s32 lbl_803447B8;   /* gate flag */
extern s32 lbl_8034476C;   /* alternate player-audio dispatch mode */
extern s32 sMusicTrackHi;
extern s32 sMusicTrackLo;
extern s32 sMusicSlot0;
extern s32 sMusicSlot1;
extern s32 sMusicSlot2;
extern s32 sActiveTrackId[]; /* active-track id array (45 entries) */
extern s32 lbl_80343E24; /* SDA sound-id table base indexed by sel */
extern s32 lbl_80343E2C; /* SDA sound-id table base indexed by sel */
extern s32 lbl_80343E34; /* SDA sound-id table base indexed by rotation counter */
extern s32 sAudioOverride; /* audio override flag (saved/restored) */
extern s32 lbl_80344C30;   /* 0..1 rotation counter */
extern f32 sMusicFadeBase;
extern f32 sMusicFadeCur;
extern f32 sMusicVolPrev;
extern f32 lbl_8034832C; /* player-slot empty threshold */
extern f32 sMusicVolScale;
extern s32 gBossType;
extern level_data* gCurLevel;
extern u8* gWorldData;
extern f32 lbl_80348480;
extern f32 lbl_80348484;
extern f32 lbl_80348490;
extern f32 lbl_80348494;
extern f32 lbl_80348498;

/* ----------------------------------------------------------------- */

#pragma opt_propagation off
int AudioFindPlayerSlot(int pidx, int class_, int type)
{
    u8* slot = (u8*)gPlayers[pidx].powerup;
    f32 value;
    f32 threshold;
    int i;

    for (i = 0; i < 11; i++) {
        value = *(f32*)slot;
        threshold = lbl_8034832C;
        if (value <= threshold) {
            continue;
        }
        if (*(int*)(slot + 12) == type && *(int*)(slot + 4) == class_) {
            return i;
        }
        slot += 16;
    }
    return -1;
}
#pragma opt_propagation reset

void AudioPlay3DSel(int soundId, int p2, int pos, int sel)
{
    if (sel != 0) {
        sndFxPlay3DAtten(soundId, pos, p2, 15);
    } else {
        sndFxPlay3D(soundId, pos, p2, 15);
    }
}

void fn_8009C378(void)
{
    int i = sVoiceRotIdxA;
    int id = lbl_80124340[i];

    i++;
    sVoiceRotIdxA = i;
    if (i >= 4) {
        sVoiceRotIdxA = 0;
    }
    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, id, -1.0f, 0.5f, 224, 127, 2);
    }
}

void fn_8009C3EC(void)
{
    int i = sVoiceRotIdxB;
    int id = lbl_80124330[i];

    i++;
    sVoiceRotIdxB = i;
    if (i >= 4) {
        sVoiceRotIdxB = 0;
    }
    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, id, -1.0f, 0.5f, 224, 127, 2);
    }
}

void AudioTowerFX(int sel)
{
    int id = -1;

    switch (sel) {
    case 1:  id = 0xE00A3; break;
    case 2:  id = 22; break;
    case 10: id = 0xE00AA; break;
    case 14: id = 0xE00AB; break;
    case 21: id = 0xE00A9; break;
    }
    if (id >= 0) {
        sndFxPlay3D(id, 0, 255, 10);
    }
}

int AudioRuneSpeech(int sel)
{
    int id = -1;

    switch (sel) {
    case 0:  id = 0xE00A4; break;
    case 13: id = 0xE00A8; break;
    case 36: id = 0xE00A7; break;
    case 25: id = 0xE00A5; break;
    case 26: id = 0xE00A6; break;
    }
    if (id >= 0) {
        sndFxQueAddEx(1, id, -1.0f, 10.0f, 224, 127, 2);
    }
    return id;
}

int fn_8009C5B8(int idx)
{
    int id = lbl_80124324[idx];

    if (id >= 0) {
        sndFxQueAddEx(1, id, -1.0f, 10.0f, 224, 127, 2);
    }
    return id;
}

int fn_8009C620(int idx)
{
    int id = lbl_80124300[idx];

    if (id >= 0) {
        sndFxQueAddEx(1, id, -1.0f, 10.0f, 224, 127, 2);
    }
    return id;
}

void AudioShardSpeech(int idx)
{
    int id = -1;

    if (idx < 9) {
        id = lbl_801242DC[idx];
    } else if (idx == 15) {
        id = 0xE00AC;
    } else if (idx == 16) {
        id = 0x3000B;
    }
    if (id >= 0) {
        sndFxQueAddEx(1, id, -1.0f, 10.0f, 224, 127, 2);
    }
}

void AudioGoodWizard(int a, int b)
{
    int row = a - 34;
    int id = lbl_8012417C[row][b];

    if (id >= 0) {
        sndFxQueAddEx(1, id, -1.0f, 10.0f, 224, 127, 2);
    }
}

void AudioGeneratorDamaged(int pos, int sel)
{
    int mt = sMusicTrackHi;
    int id = lbl_80124148[mt];

    if (mt == 10 && sel == 24) {
        id = 0x2C0024;
    }
    sndFxPlay3DAtten(id, pos, 180, 91);
}

void AudioGeneratorDies(int pos, int sel)
{
    int mt = sMusicTrackHi;
    int id = lbl_80124114[mt];

    if (mt == 10 && sel == 24) {
        id = 0x2C0025;
    }
    if (gBossType < 0 && id >= 0) {
        sndFxPlay3DAtten(id, pos, 127, 81);
    }
}

void AudioWorldHitPlyr(int pos)
{
    int id = lbl_801240DC[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 127, 100);
    }
}

void AudioWorldExplosion(int pos)
{
    int id = lbl_801240A4[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 127, 101);
    }
}

void AudioExplodeWall(int pos, int flag)
{
    if (flag > 0) {
        int idx = gCurLevel->audio->hitsnd;
        if (idx >= 0) {
            u8* e = *(u8**)(gWorldData + 44) + idx * 24;
            if (*(s32*)(e + offsetof(struct sound_data, idx)) >= 0) {
                int atten = *(s16*)(e + offsetof(struct sound_data, vol)) != 0
                                ? *(s16*)(e + offsetof(struct sound_data, vol))
                                : 224;
                int priority = *(s16*)(e + offsetof(struct sound_data, pri)) != 0
                                ? *(s16*)(e + offsetof(struct sound_data, pri))
                                : 126;

                sndFxPlay3DAtten(*(s32*)(e + offsetof(struct sound_data, idx)),
                                 pos, atten, priority);
            }
        }
    } else {
        sndFxPlay3DAtten(46, pos, 127, 24);
    }
}

void fn_8009C98C(int pos)
{
    int id = lbl_8012406C[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 127, 68);
    }
}

void fn_8009C9DC(int sel, int pos)
{
    s32* t = lbl_801232C8;
    int k = sMusicTrackHi - 1;

    switch (sel) {
    case 0:
        sndFxPlay3D(100, pos, 224, 10);
        break;
    case 1:
        sndFxPlay3D(t[k + 829], pos, 224, 10);
        break;
    case 2:
        sndFxPlay3D(t[k + 840], pos, 224, 10);
        break;
    case 3:
        AudioKillBySound(t[k + 840]);
        sndFxPlay3D(t[k + 851], pos, 224, 10);
        break;
    case 4:
        sndFxPlay3D(t[k + 862], pos, 224, 10);
        break;
    }
}

/* Dispatch player event sounds between named playback and the announcer
 * queue.  The accepted event ranges differ in the alternate audio mode. */
#pragma opt_propagation off
void fn_8009CB44(s32 pidx, u32 sound, u32 extra)
{
    s32 track = lbl_801232C8[pidx];
    u32 event = sound;
    u32 tail = extra;
    f32 volume;

    if (lbl_8034476C <= 1) {
        switch (event) {
        case 0x1003D:
        case 0x2002C:
            volume = lbl_80348490;
            if (event == 0x1003D) {
                volume = lbl_80348494;
            }
            AudioWithName(-1, pidx, volume, 0x20010, event);
            return;
        case 0x2000F:
            AudioWithName(-1, pidx, lbl_80348498, event, tail);
            return;
        case 0x1002C:
            AudioWithName(-1, pidx, lbl_80348498, event, tail);
            return;
        }
        if (good_wiz_state <= 2) {
            sndFxQueAddEx(1, event, lbl_80348480, lbl_80348484, 224,
                          track, 2);
        }
        return;
    }

    switch (event) {
    case 0x1003D:
    case 0x20011:
    case 0x20012:
    case 0x20013:
    case 0x20014:
    case 0x20015:
    case 0x20016:
    case 0x20017:
    case 0x20018:
    case 0x20019:
    case 0x2001A:
    case 0x2001B:
    case 0x2001C:
    case 0x2001D:
    case 0x2001E:
    case 0x2001F:
    case 0x20020:
    case 0x20021:
    case 0x20022:
    case 0x20023:
    case 0x20024:
    case 0x20025:
    case 0x20026:
    case 0x20027:
    case 0x20028:
    case 0x2002B:
    case 0x2002C:
    case 0x2002D:
    case 0x2002E:
    case 0x2002F:
    case 0x20030:
    case 0x20031:
    case 0x20035:
    case 0x20036:
    case 0x20037:
    case 0x20038:
    case 0x20039:
    case 0x2003A:
    case 0x2003B:
    case 0x2003C:
    case 0x2003D:
        volume = lbl_80348490;
        if (event == 0x1003D) {
            volume = lbl_80348494;
        }
        AudioWithName(-1, pidx, volume, 0x20010, event);
        return;
    case 0x2000F:
        AudioWithName(-1, pidx, lbl_80348498, event, tail);
        return;
    case 0x1002C:
        AudioWithName(-1, pidx, lbl_80348498, event, tail);
        return;
    }
    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, event, lbl_80348480, lbl_80348484, 224, track, 2);
    }
}
#pragma opt_propagation reset

void fn_8009D078(int pos)
{
    sndFxPlay3DAtten(44, pos, 127, 64);
}

void fn_8009D0A8(int pos, int col)
{
    int id = lbl_80123C6C[sMusicTrackHi][col];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 127, 64);
    }
}

void fn_8009D258(int pos)
{
    sndFxPlay3D(4, pos, 127, 9);
}

void fn_8009D288(void)
{
    sndFxPlayHandle(2, 224, 9);
}

void fn_8009D2B4(void)
{
    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, 0x2000D, -1.0f, 10.0f, 224, 127, 2);
    }
}

void fn_8009D300(void)
{
    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, 0x10006, -1.0f, 10.0f, 224, 127, 2);
    }
}

void fn_8009D34C(void)
{
}

void fn_8009D350(s32 player)
{
    (void)player;
    sndFxPlayHandle(12, 224, 4);
}

void fn_8009D37C(void)
{
    sndFxPlayHandle(19, 224, 3);
}

void AudioMenuExit(void)
{
    sndFxPlayHandle(16, 127, 3);
}

void AudioCursorSelect(void)
{
    sndFxPlayHandle(17, 127, 3);
}

void AudioCursorChar(void)
{
    sndFxPlayHandle(15, 127, 3);
}

void AudioCursorH(void)
{
    sndFxPlayHandle(14, 127, 3);
}

void AudioCursorV(void)
{
    sndFxPlayHandle(13, 127, 3);
}

void AudioBuzzer(void)
{
    sndFxPlayHandle(10, 127, 48);
}

void fn_8009D4B0(int pidx)
{
    sndFxPlay3D(80, (int)gPlayers[pidx].col_pos, 224, 40);
}

void fn_8009D4F0(int pidx)
{
    sndFxPlay3D(89, (int)gPlayers[pidx].col_pos, 224, 40);
}

void fn_8009D530(void)
{
    sndFxPlayEx(85, 127, 224, 40);
}

void fn_8009D560(int pidx)
{
    sndFxPlay3D(87, (int)gPlayers[pidx].col_pos, 224, 40);
}

void fn_8009D5A0(int pidx)
{
    sndFxPlay3D(99, (int)gPlayers[pidx].col_pos, 224, 40);
}

void fn_8009D5E0(int pos)
{
    sndFxPlay3DAtten(36, pos, 180, 120);
}

void fn_8009D610(int flag, int pos)
{
    int h;

    if (flag == 0) {
        if (AudioSoundExists(3) == 0) {
            sndFxPlay3DTracked(3, pos, 224, 118);
        }
        h = AudioMaskByEvent(118);
        if (h != 0) {
            AudioSetTrackPan(h, AudioAng(pos));
        }
    } else {
        AudioKillBySound(3);
    }
}

int fn_8009D694(int a, int pos, int idx)
{
    int id1;
    int ret = 0;
    int id2;

    if (a < 0) {
        if (AudioMaskByEvent(115) != 0) {
            AudioKillMask();
        }
        return 0;
    }
    if (idx < 0 || (u32)idx >= 6) {
        return 0;
    }
    id1 = lbl_8028B610[idx][0];
    id2 = lbl_8028B610[idx][1];
    if (id1 < 0 || id2 < 0) {
        return 0;
    }
    if (lbl_803447B8 != 0 || lbl_803447B4 != 0) {
        a = 0;
    }
    if (a == 0) {
        if (AudioSoundExists(id1) != 0) {
            ret = 1;
        } else {
            sndFxPlay3DTracked(id1, pos, 224, 115);
            ret = 2;
        }
        id1 = AudioMaskByEvent(115);
        if (id1 != 0) {
            AudioSetTrackPan(id1, AudioAng(pos));
        }
    } else {
        if (AudioSoundExists(id1) != 0) {
            AudioKillBySound(id1);
            if (a >= 2) {
                sndFxPlay3D(id2, pos, 224, 68);
            }
        }
    }
    return ret;
}

void fn_8009D8CC(int pos)
{
    int id = lbl_80123AE4[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 224, 18);
    }
}

void fn_8009D91C(int pos)
{
    int mt = sMusicTrackHi;
    int id = lbl_80123AAC[mt];

    if (mt == 6 && sMusicTrackLo == 1) {
        id = 0x390000;
    } else if (mt == 9 && sMusicTrackLo == 4) {
        id = 0x32000D;
    }
    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 224, 18);
    }
}

void fn_8009D9A4(int pos)
{
    sndFxPlay3DAtten(0x250001, pos, 224, 50);
}

void fn_8009D9D8(int pos)
{
    int id = lbl_80123A74[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 224, 85);
    }
}

void fn_8009DA28(int pos)
{
    int id = lbl_80123A3C[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 224, 85);
    }
}

void fn_8009DA78(int pos)
{
    int id = lbl_80123A04[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3DAtten(id, pos, 224, 85);
    }
}

void fn_8009DAC8(int pos)
{
    sndFxPlay3DAtten(56, pos, 224, 50);
}

void fn_8009DAF8(void)
{
    sndFxPlayHandle(45, 224, 20);
}

void fn_8009DCB4(int pos)
{
    sndFxPlay3D(58, pos, 180, 20);
}

void fn_8009DCE4(int pos)
{
    int mt = sMusicTrackHi;
    int id = 0x260027;

    if (mt == 3) {
        id = 0x270032;
    }
    if (mt == 4) {
        id = 0x280000;
    }
    if (mt == 6) {
        id = 0x38001F;
    }
    sndFxPlay3DAtten(id, pos, 127, 50);
}

void fn_8009DD48(void)
{
    AudioKillBySound(55);
}

void fn_8009DD6C(int pos)
{
    sndFxPlay3DAtten(55, pos, 224, 52);
}

void fn_8009DD9C(int pos)
{
    sndFxPlay3DAtten(sMusicSlot2, pos, 224, 12);
}

void fn_8009DDCC(int pos)
{
    sndFxPlay3D(sMusicSlot1, pos, 224, 13);
}

void fn_8009DDFC(int pos)
{
    sndFxPlay3DAtten(50, pos, 127, 126);
}

void fn_8009DE2C(int pos)
{
    sndFxPlay3DAtten(48, pos, 127, 126);
}

void fn_8009DE5C(int a, int pos)
{
    sndFxPlay3DAtten(60, pos, 127, 120);
}

void fn_8009E03C(int pos)
{
    switch (*(int*)pos) {
    case 29:
    case 32:
        sndFxPlay3DAtten(sMusicSlot0, pos + 52, 127, 28);
        break;
    }
}

static inline void sndFxPlay3DAttenOrdered(int soundId, int pos, int flags,
                                           int pan)
{
    sndFxPlay3DAtten(soundId, pos, pan, flags);
}

void fn_8009DB24(int sel, int arg)
{
    s32* t = lbl_801232C8;
    int soundId;
    int pan;
    int flags;

    pan = 224;
    flags = 126;
    soundId = -1;
    switch (sel) {
    case 1:
        soundId = 0x30000B;
        flags = 14;
        break;
    case 2:
        soundId = 57;
        flags = 54;
        break;
    case 3:
        soundId = 0x30000D;
        flags = 14;
        break;
    case 5: {
        int idx = gCurLevel->audio->hitsnd;

        if (idx >= 0) {
            u8* e = *(u8**)(gWorldData + 44) + idx * 24;

            if (*(s32*)(e + offsetof(struct sound_data, idx)) >= 0) {
                sndFxPlay3DAttenOrdered(
                    *(s32*)(e + offsetof(struct sound_data, idx)), arg,
                    *(s16*)(e + offsetof(struct sound_data, pri)) != 0
                        ? *(s16*)(e + offsetof(struct sound_data, pri)) : 126,
                    *(s16*)(e + offsetof(struct sound_data, vol)) != 0
                        ? *(s16*)(e + offsetof(struct sound_data, vol)) : 224);
            }
        }
        break;
    }
    case 6:
        soundId = 63;
        pan = 127;
        flags = 40;
        break;
    case 7:
        soundId = 64;
        pan = 127;
        flags = 40;
        break;
    case 4:
        soundId = 56;
        flags = 50;
        break;
    case 8:
        soundId = t[444];
        pan = 127;
        flags = 15;
        break;
    case 9:
        soundId = t[445];
        pan = 127;
        flags = 15;
        break;
    case 10:
        soundId = t[446];
        pan = 127;
        flags = 15;
        break;
    case 11:
        soundId = t[447];
        pan = 127;
        flags = 15;
        break;
    case 12:
        soundId = 0x310011;
        pan = 127;
        flags = 14;
        break;
    case 0:
    default:
        soundId = sel;
        break;
    }
    if (soundId >= 0) {
        sndFxPlay3DAtten(soundId, arg, pan, flags);
    }
}

void fn_8009DE88(int p, int mode)
{
    s32* T = (s32*)sSpeechNameBuf;
    int id0 = T[*(int*)p + 300];
    int id;

    if (id0 >= 0) {
        if (mode == 1) {
            if (*(s16*)(p + 518) >= 2) {
                if (*(s16*)(p + 516) <= 1) {
                    id = T[id0 + 393];
                } else {
                    id = T[id0 + 401];
                }
            } else {
                id = T[id0 + 377];
            }
            sndFxPlay3DAtten(id, p + 52, 224, 35);
        } else {
            if (*(s16*)(p + 518) >= 2) {
                if (*(s16*)(p + 516) <= 1) {
                    id = T[id0 + 409];
                } else {
                    id = T[id0 + 417];
                }
            } else {
                id = T[id0 + 385];
            }
            sndFxPlay3DAtten(id, p + 52, 224, 95);
        }
    }
}

void fn_8009DF7C(int p, int mode)
{
    s32* T = (s32*)sSpeechNameBuf;
    int id0 = T[*(int*)p + 300];

    if (id0 >= 0) {
        if (mode == 1) {
            int id = *(s16*)(p + 518) >= 2 ? T[id0 + 353] : T[id0 + 345];

            sndFxPlay3DAtten(id, p + 52, 224, 25);
        } else {
            int id;

            if (*(s16*)(p + 518) >= 2) {
                id = T[id0 + 369];
            } else {
                id = T[id0 + 361];
            }
            sndFxPlay3DAtten(id, p + 52, 224, 85);
        }
    }
}

void fn_8009E08C(int p)
{
    s32* T = (s32*)sSpeechNameBuf;
    int id0 = T[*(int*)p + 300];

    if (id0 >= 0) {
        int id = *(s16*)(p + 518) >= 2 ? T[id0 + 433] : T[id0 + 425];

        sndFxPlay3DAtten(id, p + 52, 180, 75);
    }
}

/* --- AudioSetupBossStreams support --------------------------------- */
extern char* strcpy(char* dst, const char* src);
extern char* strcat(char* dst, const char* src);
extern int sprintf(char* buf, const char* fmt, ...);
extern int AudioFindSound(char* a, int b, int c);
extern int LevelLetter(int a);
extern s32 lbl_802577CC[]; /* level -> boss-stream select code (0..29) */
extern s32 lbl_8025778C[]; /* level -> boss rank/tier */

/* gBossType 36/37/41 use a shared sample set: truncate the speech name
 * at 14 chars and append the variant letter before the lookup. */
#define BossNameFixup(buffer)                                               \
    if (gBossType > 0) {                                                    \
        switch (gBossType) {                                                \
        case 41:                                                            \
            (buffer)[14] = 0;                                               \
            strcat((buffer), "B");                                         \
            break;                                                          \
        case 37:                                                            \
            (buffer)[14] = 0;                                               \
            strcat((buffer), "D");                                         \
            break;                                                          \
        case 36:                                                            \
            (buffer)[14] = 0;                                               \
            strcat((buffer), "C");                                         \
            break;                                                          \
        }                                                                   \
    }

#pragma opt_propagation off
/* One .bss object, split into two symbols by the extractor: its head is
 * named sSpeechNameBuf (.bss 0x8028B5D0, size 0x40 -- the sprintf scratch
 * buffer) and the id table 0x4B0 bytes later is named sActiveTrackId (.bss
 * 0x8028BA80, size 0x238).  0x8028B5D0 + 0x4B0 is exactly sActiveTrackId's
 * base, and the TARGET reaches every id store through the HEAD symbol with
 * the table offset FOLDED INTO THE STORE DISPLACEMENT (r30 = @sSpeechNameBuf
 * for the whole function, `stw r3,1412(r4)`), which a compiler can only do
 * when the two live in one declared object.  So the block is declared as one
 * record here, per claim.law.walked-base-symbol-identity keeping
 * sSpeechNameBuf as the relocated base symbol.
 *
 * Layout implied by the accesses: 45 active-track ids at 0x4B0 (matching
 * this file's own "45 entries" note), then a [12][8] boss-speech id table at
 * 0x564 with 0x20-byte rows, indexed [row][idx]. */
typedef struct {
    char name[0x40];     /* 0x000 sprintf scratch buffer (sSpeechNameBuf) */
    u8 pad_40[0x470];    /* 0x040 fields not reached by this TU */
    s32 activeTrack[45]; /* 0x4B0 sActiveTrackId */
    s32 boss[12][8];     /* 0x564 boss-speech ids, [row][idx] */
} SpeechBlock;

void AudioSetupBossStreams(register int idx, register char* name)
{
    char bufA[32]; /* close-variant stream name */
    char bufB[32]; /* far-variant stream name */
    register int mode;
    register char* suffix = "DIE";
    register int sel;
    register SpeechBlock* speech = (SpeechBlock*)sSpeechNameBuf;
    int nvar;

    sel = lbl_802577CC[idx];
    if (sel < 0) {
        return;
    }
    if (name == NULL || *name == 0) {
        idx = -1;
    }
    speech->activeTrack[sel] = idx;
    mode = 0;

    switch (sel) {
    case 29: /* golem/wizard */
        sprintf(bufA, "GOL%c", (signed char)LevelLetter(0));
        sprintf(bufB, "GOL%c", (signed char)LevelLetter(0));
        nvar = 0;
        sprintf(speech->name, "S_GOL%cSTOMP", (signed char)LevelLetter(0));
        sMusicSlot0 = AudioFindSound(speech->name, -1, 1);
        sprintf(speech->name, "S_GOL%cBORN", (signed char)LevelLetter(0));
        sMusicSlot1 = AudioFindSound(speech->name, -1, 1);
        sprintf(speech->name, "S_GOL%cSWING", (signed char)LevelLetter(0));
        sMusicSlot2 = AudioFindSound(speech->name, -1, 1);
        suffix = "KILL";
        break;
    default:
        strcpy(bufA, name);
        strcpy(bufB, name);
        nvar = 0;
        if (sel != 29) {
            mode = 1;
        }
        break;
    case 1:
    case 2:
    case 4:
    case 5:
    case 7:
    case 8:
    case 10:
    case 11:
    case 13:
    case 14:
    case 16:
    case 17:
    case 19:
    case 20:
    case 23:
    case 24:
    case 25:
    case 26: /* boss levels */
        if (lbl_8025778C[idx] >= 10 || gBossType >= 0) {
            sprintf(bufA, "%s2", name);
            sprintf(bufB, "%s2", name);
            nvar = 2;
        } else {
            sprintf(bufA, "%s1", name);
            sprintf(bufB, "%s2", name);
            nvar = 1;
        }
        if (sel == 2 || sel == 8 || sel == 19 || sel == 17 || sel == 24 || sel == 25) {
            mode = 1;
        } else if (sel == 11) {
            mode = 2;
        }
        break;
    case 27:
        sprintf(bufA, "%s1", name);
        sprintf(bufB, "%s1", name);
        nvar = 0;
        break;
    }

    sprintf(speech->name, "S_%s%sCLOSE", bufA, suffix);
    BossNameFixup(speech->name);
    speech->boss[0][idx] = AudioFindSound(speech->name, -1, 1);

    sprintf(speech->name, "S_%s%sCLOSE", bufB, suffix);
    BossNameFixup(speech->name);
    speech->boss[1][idx] = AudioFindSound(speech->name, -1, 1);

    sprintf(speech->name, "S_%s%sFAR", bufA, suffix);
    BossNameFixup(speech->name);
    speech->boss[2][idx] = AudioFindSound(speech->name, -1, 1);

    sprintf(speech->name, "S_%s%sFAR", bufB, suffix);
    BossNameFixup(speech->name);
    speech->boss[3][idx] = AudioFindSound(speech->name, -1, 1);

    if (nvar < 2) {
        sprintf(speech->name, "S_%sHITCLOSE", bufA);
        BossNameFixup(speech->name);
        speech->boss[4][idx] = AudioFindSound(speech->name, -1, 1);

        sprintf(speech->name, "S_%sHITFAR", bufA);
        BossNameFixup(speech->name);
        speech->boss[5][idx] = AudioFindSound(speech->name, -1, 1);
    }

    if (nvar != 0) {
        sprintf(speech->name, "S_%sHIT1CLOSE", bufB);
        BossNameFixup(speech->name);
        speech->boss[6][idx] = AudioFindSound(speech->name, -1, 1);

        sprintf(speech->name, "S_%sHIT2CLOSE", bufB);
        BossNameFixup(speech->name);
        speech->boss[7][idx] = AudioFindSound(speech->name, -1, 1);

        sprintf(speech->name, "S_%sHIT1FAR", bufB);
        BossNameFixup(speech->name);
        speech->boss[8][idx] = AudioFindSound(speech->name, -1, 1);

        sprintf(speech->name, "S_%sHIT2FAR", bufB);
        BossNameFixup(speech->name);
        speech->boss[9][idx] = AudioFindSound(speech->name, -1, 1);
    } else {
        sprintf(speech->name, "S_%sHITCLOSE", bufB);
        BossNameFixup(speech->name);
        speech->boss[6][idx] = AudioFindSound(speech->name, -1, 1);
        speech->boss[7][idx] = speech->boss[6][idx];

        sprintf(speech->name, "S_%sHITFAR", bufB);
        BossNameFixup(speech->name);
        speech->boss[8][idx] = AudioFindSound(speech->name, -1, 1);
        speech->boss[9][idx] = speech->boss[8][idx];
    }

    if (mode == 2) {
        sprintf(speech->name, "S_%sSTRIKE", bufA);
        BossNameFixup(speech->name);
        speech->boss[10][idx] = AudioFindSound(speech->name, -1, 1);

        sprintf(speech->name, "S_%sSTRIKE", bufB);
        BossNameFixup(speech->name);
        speech->boss[11][idx] = AudioFindSound(speech->name, -1, 1);
    } else if (mode == 1) {
        sprintf(speech->name, "S_%sBITE", bufA);
        BossNameFixup(speech->name);
        speech->boss[10][idx] = AudioFindSound(speech->name, -1, 1);

        sprintf(speech->name, "S_%sBITE", bufB);
        BossNameFixup(speech->name);
        speech->boss[11][idx] = AudioFindSound(speech->name, -1, 1);
    }
}
#pragma opt_propagation reset

void fn_8009FCA8(int flag)
{
    int id = lbl_80123454[lbl_803448B4];

    if (id >= 0) {
        if (flag != 0) {
            if (AudioSoundExists(id) == 0) {
                sndFxPlay3DTracked(id, 0, 224, 123);
            }
        } else {
            if (AudioSoundExists(id) != 0) {
                AudioKillBySound(id);
            }
        }
    }
}

void AudioClearActiveTracks(void)
{
    int i;

    for (i = 0; i < 45; i++) {
        sActiveTrackId[i] = -1;
    }
    sMusicSlot0 = -1;
    sMusicSlot1 = -1;
    sMusicSlot2 = -1;
}

void fn_8009EE2C(int flag)
{
    int save = sAudioOverride;

    sAudioOverride = 1;
    if (flag != 0) {
        sndFxPlayEx(20, 127, 127, 1);
    } else {
        u32 c;

        sndFxPlayEx((&lbl_80343E34)[lbl_80344C30], 127, 127, 1);
        c = lbl_80344C30 + 1;
        lbl_80344C30 = c;
        if (c >= 2) {
            lbl_80344C30 = 0;
        }
    }
    sAudioOverride = save;
}

void AudioClick(int pidx, int sel)
{
    int track = lbl_801232C8[pidx];
    int id = (&lbl_80343E2C)[sel];

    sndFxPlayEx(id, track, 127, 125);
}

void AudioClick2(int pidx, int sel)
{
    int track = lbl_801232C8[pidx];
    int id = (&lbl_80343E24)[sel];

    sndFxPlayEx(id, track, 127, 125);
}

void fn_8009EF4C(int pos)
{
    sndFxPlay3DAtten(60, pos, 127, 120);
}

void AudioPlayerXray(int pidx)
{
    sndFxPlay3D(81, (int)gPlayers[pidx].col_pos, 127, 60);
}

void fn_8009F158(int pidx)
{
    sndFxPlay3D(82, (int)gPlayers[pidx].col_pos, 224, 60);
}

void fn_8009F390(int pidx)
{
    sndFxPlay3D(5, (int)gPlayers[pidx].effectpos, 127, 19);
}

void fn_8009F3D0(int pidx)
{
    sndFxPlay3D(93, (int)gPlayers[pidx].col_pos, 224, 40);
}

void fn_8009F410(int pidx)
{
    sndFxPlay3D(92, (int)gPlayers[pidx].col_pos, 224, 40);
}

void fn_8009F450(int pidx)
{
    sndFxPlay3D(91, (int)gPlayers[pidx].col_pos, 224, 40);
}

void fn_8009F490(int pidx)
{
    sndFxPlay3D(90, (int)gPlayers[pidx].col_pos, 224, 40);
}

void fn_8009EF7C(int a, int pos)
{
    if (sMusicFadeBase >= 1.0 + sMusicVolPrev) {
        sndFxPlay3DAtten(65, pos, 127, 40);
        sMusicVolPrev = sMusicFadeBase;
    }
}

void fn_8009EFCC(int pidx, int a, int b)
{
    int id = lbl_801239DC[a][b];

    sndFxPlay3DAtten(id, (int)gPlayers[pidx].pos, 127, 125);
}

void AudioPotion(int a, int pos, int c)
{
    sndFxPlay3D(lbl_801239B4[c][a], pos, 127, 15);
}

void fn_8009F340(int pos)
{
    int id = lbl_80123784[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlay3D(id, pos, 224, 6);
    }
}

void AudioPlayerDies(int pidx)
{
    Player* player = &gPlayers[pidx];

    if (player->state == 1) {
        int id;

        sndFxPlay3D(1, (int)player->pos, 127, 8);
        id = lbl_8012380C[player->char_type];
        if (player->flags & 0x400) {
            id = 98;
        }
        sndFxPlay3D(id, (int)player->pos, 224, 7);
    }
}

void AudioPlayerHit(int pidx, int a)
{
    Player* player = &gPlayers[pidx];

    if (player->state == 1) {
        sndFxPlay3D(lbl_801237BC[a][pidx], (int)player->pos, 127, 74);
    }
}

void fn_8009FA84(void)
{
    sndFxPlayHandle(0xC0085, 224, 20);
}

void fn_8009FAB4(void)
{
    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, 0x10029, -1.0f, 1.0f, 224, 127, 2);
    }
}

void fn_8009FB00(void)
{
    sndFxPlayHandle(0x1005A, 224, 20);
}

int fn_8009FB30(void)
{
    int id = 0x3B0025;

    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, id, -1.0f, 1.0f, 224, 127, 2);
    }
    return id;
}

void DoAudioTallySFX(int sel_)
{
    int sel;

    if ((sel = sel_) < 0 || sel > 10) {
        return;
    }
    sndFxPlayHandle(lbl_801234B8[sel], 224, sel + 21);
}

void AudioMapDot(void)
{
    int id = lbl_8012341C[sMusicTrackHi];

    if (id >= 0) {
        sndFxPlayHandle(id, 224, 30);
    }
}

/* sSoundDataEntry: the sound record for an event index, or NULL when the
 * index or the record's own sound id is unset. */
static inline u8* sSoundDataEntry(int idx)
{
    if (idx >= 0) {
        u8* e = *(u8**)(gWorldData + 44) + idx * 24;

        if (*(s32*)(e + offsetof(struct sound_data, idx)) >= 0) {
            return e;
        }
    }
    return 0;
}

void AudioEnterNextStage(void)
{
    struct audio_data* level = gCurLevel->audio;
    u8* entry = sSoundDataEntry(level->entersnd);

    if (entry != 0) {
        if (level->namesnd >= 0) {
            int sound_id = *(int*)(entry + offsetof(struct sound_data, idx));

            if (good_wiz_state <= 2) {
                sndFxQueAddEx(1, sound_id, lbl_80348480, lbl_80348480, 224,
                              127, 2);
            }
            {
                int next_id = gCurLevel->audio->namesnd;

                if (good_wiz_state <= 2) {
                    sndFxQueAddEx(1, next_id, lbl_80348480, lbl_80348480, 224,
                                  127, 2);
                }
            }
        }
    }
}

void AudioPlayerBreath(int pidx)
{
    int sound = sMusicTrackHi == 13 ? 0x30014 : 0x2000C;

    AudioWithName(-1, pidx, 5.0f, sound, -1);
}

void fn_8009FEA0(int pidx)
{
    int track = lbl_801232C8[pidx];

    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, 0x10001, -1.0f, 3.0f, 224, track, 2);
    }
}

void fn_8009FEFC(int pidx)
{
    int track = lbl_801232C8[pidx];

    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, 0x10000, -1.0f, 3.0f, 224, track, 2);
    }
}

void fn_8009FF54(int pos)
{
    int track = AudioAng(pos);

    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, 0x20033, -1.0f, 3.0f, 224, track, 2);
    }
}

void fn_8009FFA4(int pos)
{
    int track = AudioAng(pos);

    if (good_wiz_state <= 2) {
        sndFxQueAddEx(1, 0x20034, -1.0f, 3.0f, 224, track, 2);
    }
}

void AudioDamageTile(int pos, int idx)
{
    if (idx < 7) {
        int id = lbl_8012382C[sMusicTrackHi][idx];

        if (id == 0x260027 && gBossType == 34) {
            id = 0x2E0009;
        }
        if (id >= 0) {
            if (id == 0x250002 || id == 0x28002E) {
                sndFxPlay3D(id, pos, 180, 50);
            } else {
                sndFxPlay3D(id, pos, 127, 50);
            }
        }
    }
}

#pragma opt_common_subs off
void AudioPlayerTurbo(int pidx, int sel, int arg3)
{
    int f8;
    int slot;
    int flags;

    /* Three nearby fields off one index-computed base: a typed
     * `gPlayers[pidx].field` form regressed this function (real 0 -> 14,
     * schedule-class) per claim.law.multifield-alias-defeats-indexed-
     * addressing.  The law's verified counter-form is kept here -- the raw
     * single additive expression with offsetof()-spelled displacements. */
    flags = *(int*)((u8*)gPlayers + pidx * 13148 + offsetof(Player, flags));
    f8 = *(int*)((u8*)gPlayers + pidx * 13148 + offsetof(Player, char_type));
    slot = (int)((u8*)gPlayers + pidx * 13148 + offsetof(Player, pos));

    if (flags & 0x400) {
        sndFxPlay3D(95, slot, 224, 16);
    } else {
        switch (sel) {
        case 0:
            sndFxPlay3D(snd_turboA[f8][arg3], slot, 224, 19);
            break;
        case 1:
            sndFxPlay3D(snd_turboB[f8], slot, 224, 17);
            break;
        case 2:
            sndFxPlay3D(snd_turboC[f8], slot, 224, 16);
            break;
        }
    }
}
#pragma opt_common_subs reset

void AudioPlayerEatFood(int pidx, int foodType)
{
    Player* player = &gPlayers[pidx];
    Player* p = player;

    if (RandInt(4) == 0) {
        if (!(p->flags & 0x400)) {
            if (snd_eat[p->char_type][foodType] >= 0) {
                int pan = AudioAng((int)p->pos);

                sndFxQueAdd(snd_eat[p->char_type][foodType],
                            -1.0f, 1.0f, 192, pan, 66);
            }
        }
    } else {
        int id;

        if ((id = snd_eat_default[p->char_type]) >= 0) {
            int pan = AudioAng((int)p->pos);

            if (p->flags & 0x400) {
                id = 97;
            }
            sndFxQueAdd(id, -1.0f, 1.0f, 192, pan, 66);
        }
    }
}

void AudioPlayerEatSFX(int pidx)
{
    /* NOTE: this function's compiled body is pinned by a WebFrank rule
     * (config/GUNE5D/webfrank.json), so its source SHAPE must not change --
     * the byte-offset walk below is deliberately left in its original form
     * rather than converted to Player member access.  Only the base
     * derivation is respelled for the retyped gPlayers declaration;
     * `(u8*)gPlayers + playerOffset` is the same arithmetic the previous
     * `&gPlayers[playerOffset]` performed when gPlayers was `u8[]`. */
    int playerOffset = pidx * 13148;
    int f284;
    u8* player;

    player = (u8*)gPlayers + playerOffset;
    f284 = *(int*)(player + 284);

    if (f284 & 0x580000) {
        sndFxPlay3DAtten(66, (int)(player + 68), 127, 40);
    } else {
        switch (f284 & 0xF) {
        case 1:
            sndFxPlay3DAtten(68, (int)(player + 68), 127, 40);
            break;
        case 2:
            sndFxPlay3DAtten(70, (int)(player + 68), 127, 40);
            break;
        case 3:
            sndFxPlay3DAtten(69, (int)(player + 68), 127, 40);
            break;
        case 4:
            sndFxPlay3DAtten(67, (int)(player + 68), 127, 40);
            break;
        default:
            player = (u8*)gPlayers + playerOffset;
            sndFxPlay3DAtten(lbl_801236A4[*(int*)(player + 8)],
                            (int)(player + 68), 127, 42);
            break;
        }
    }
}

#ifdef __MWERKS__
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif

void fn_8009F748(int pidx, int sourcePlayer)
{
    Player* player = &gPlayers[pidx];

    if (!(player->flags & 0x400)) {
        if (lbl_80123644[player->char_type] >= 0) {
            int pan = AudioAng((int)player->pos);

            sndFxQueAdd(lbl_80123644[player->char_type], -1.0f, 1.0f, 192, pan, 110);
        }
    }
}

void AudioPlayerSeverePain(int pidx)
{
    Player* player = &gPlayers[pidx];
    int id;

    if ((id = lbl_80123624[player->char_type]) >= 0) {
        int pan = AudioAng((int)player->pos);

        if (player->flags & 0x400) {
            id = 98;
        }
        sndFxQueAdd(id, -1.0f, 1.0f, 192, pan, 66);
    }
}

void AudioPlayerPoison(int pidx)
{
    Player* player = &gPlayers[pidx];

    if (player->state == 1) {
        if (!(player->shield_flags & 0x10000)) {
            int id = lbl_80123564[player->char_type];

            if (id >= 0) {
                if (player->flags & 0x400) {
                    id = 96;
                }
                sndFxPlay3D(id, (int)player->pos, 224, 62);
            }
        }
    }
}

typedef struct AudioPlayerRecord {
    u8 data[13148];
} AudioPlayerRecord;

void AudioHeartBeat(int pidx)
{
    Player* player = gPlayers;
    f32 v;
    int track = lbl_801232C8[pidx];

    player += pidx;
    v = player->health;

    if (v <= 10.0) {
        sndFxPlayEx(0, track, 202, 9);
    } else if (v < 25.0) {
        sndFxPlayEx(0, track, 177, 9);
    } else if (v < 100.0) {
        sndFxPlayEx(0, track, 152, 9);
    } else {
        sndFxPlayEx(0, track, 127, 9);
    }
}
#pragma opt_propagation off
void AudioPlayerPain(int pidx)
{
    Player* player = &gPlayers[pidx];

    if (player->state == 1) {
        int pan = AudioAng((int)player->pos);
        int r = RandInt(4);
        u32 randomOffset = (u32)r << 2;
        int id = *(int*)((u8*)lbl_801234E4 +
                         (player->char_type << 4) + randomOffset);

        if (player->flags & 0x400) {
            id = 96;
        }
        sndFxQueAdd(id, -1.0f, 1.0f, 224, pan, 100);
    }
}
#pragma opt_propagation reset

void AudioNumRunesFound(int runeCount)
{
    if (runeCount <= 0) {
        return;
    }
    if (runeCount == 1) {
        if (good_wiz_state <= 2) {
            sndFxQueAddEx(1, 0x10030, -1.0f, -1.0f, 224, 127, 2);
        }
    } else if (runeCount <= 12) {
        int id = lbl_8012348C[runeCount - 2];

        if (good_wiz_state <= 2) {
            sndFxQueAddEx(1, id, -1.0f, -1.0f, 224, 127, 2);
        }
        if (good_wiz_state <= 2) {
            sndFxQueAddEx(1, 0x10031, -1.0f, -1.0f, 224, 127, 2);
        }
    }
}

void fn_8009FFF4(int sel, int pidx)
{
    switch (sel) {
    case 0:
    default:
        AudioWithName(-1, pidx, 2.0f, 0x10004, -1);
        break;
    case 1:
        AudioWithName(-1, pidx, 1.0f, 0x10005, -1);
        break;
    case 2:
        AudioWithName(-1, pidx, 1.0f, 0x10003, -1);
        break;
    case 3:
        AudioWithName(-1, pidx, 0.5f, 0x20000, -1);
        break;
    }
}

void AudioTurboDefense(int pidx)
{
    int f292 = gPlayers[pidx].flags;
    int id;
    int pos;

    if (f292 & 0x10) {
        id = 71;
    } else if (f292 & 0x20) {
        id = 72;
    } else if (f292 & 0x40) {
        id = 73;
    } else {
        return;
    }
    pos = (u32)gPlayers[pidx].col_pos;
    sndFxPlay3D(id, pos, 224, 40);
}

void fn_8009D7E4(int a, int pos)
{
    int id1 = lbl_80123B1C[sMusicTrackHi];
    int id2 = lbl_80123B54[sMusicTrackHi];

    if (lbl_803447B8 != 0 || lbl_803447B4 != 0) {
        a = 0;
    }
    if (a == 0) {
        if (AudioSoundExists(id1) == 0) {
            sndFxPlay3DTracked(id1, pos, 224, 114);
        }
        id1 = AudioMaskByEvent(114);
        if (id1 != 0) {
            AudioSetTrackPan(id1, AudioAng(pos));
        }
    } else {
        AudioKillBySound(id1);
        if (a >= 2) {
            sndFxPlay3D(id2, pos, 224, 0);
        }
    }
}

void AudioExp(int pidx, int flag)
{
    if (flag > 0) {
        AudioWithName(-1, pidx, 3.0f, 0x20010, 0x1003D);
    } else if (flag < 0) {
        AudioWithName(-1, pidx, 3.0f, 0x1003E, -1);
    }
}

int fn_8009CD80(int a, int b, int val)
{
    int id;

    if (val >= 99) {
        id = 0x1003F;
    } else {
        id = lbl_80123D7C[b][val / 10 - 1];
    }
    AudioWithName(-1, a, 5.0f, id, -1);
    return id;
}

void fn_8009CDF8(int pidx)
{
    sndFxPlayEx(37, lbl_801232C8[pidx], 127, 66);
}

void fn_8009CE38(int pidx)
{
    f32 val;
    f32 delta;

    sndFxPlayEx(40, lbl_801232C8[pidx], 127, 40);
    val = 0.5f;
    delta = (f32)(AudioGetSoundVol(40) - 1.0);
    sMusicFadeCur = sMusicFadeBase + delta;
    if (val < 0.0) {
        val = 0.0f;
    } else if (val < 0.2) {
        val = 0.2f;
    } else if (val > 1.0) {
        val = 1.0f;
    }
    sMusicVolScale = val;
}

void fn_8009D038(int pidx)
{
    sndFxPlayEx(38, lbl_801232C8[pidx], 127, 66);
}

void fn_8009CEE0(int pidx, int sel, int flags)
{
    int p1 = lbl_801232C8[pidx];
    int soundId = 39;
    int pan = 127;

    switch (sel) {
    case 15:
        soundId = 38;
        break;
    case 6:
        if (flags & 0x200000) {
            soundId = 41;
        }
        break;
    case 9:
        if (flags & 1) {
            soundId = 88;
        } else if (flags & 0x100) {
            soundId = 86;
            pan = 180;
        } else if (flags & 0x200) {
            soundId = 84;
            pan = 180;
        } else if (flags & 0x400) {
            soundId = 94;
        }
        break;
    }
    sndFxPlayEx(soundId, p1, pan, 66);
}

#pragma opt_propagation off
void fn_8009CFA8(int pidx, int sel)
{
    s32* t = lbl_801232C8;
    int p1 = t[pidx];
    int soundId = 38;

    if (sMusicTrackHi == 12) {
        switch (sel) {
        case 50:  soundId = t[pidx + 673]; break;
        case 100: soundId = t[pidx + 677]; break;
        case 500:
        default:  soundId = t[pidx + 681]; break;
        }
    }
    sndFxPlayEx(soundId, p1, 127, 66);
}
#pragma opt_propagation reset

void AudioBridgeOpen(int pos)
{
    int id = lbl_80123C34[sMusicTrackHi];

    if (lbl_803447B8 == 0) {
        switch (lbl_803447B4) {
        case 0:
            if (id >= 0) {
                sndFxPlay3D(id, pos, 224, 68);
            }
            break;
        }
    }
}

void AudioBridgeClose(int pos)
{
    int id = lbl_80123BFC[sMusicTrackHi];

    if (lbl_803447B8 == 0) {
        switch (lbl_803447B4) {
        case 0:
            if (id >= 0) {
                sndFxPlay3D(id, pos, 224, 30);
            }
            break;
        }
    }
}

void AudioWorldObjectMotion(int pos, int sel)
{
    int id = lbl_80123B8C[sel][sMusicTrackHi];

    if (lbl_803447B8 == 0) {
        switch (lbl_803447B4) {
        case 0:
            if (gBossType < 0 && id >= 0) {
                sndFxPlay3D(id, pos, 224, 30);
            }
            break;
        }
    }
}
