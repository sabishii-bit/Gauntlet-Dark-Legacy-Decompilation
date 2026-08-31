/*
 * psfx.c -- GCN PSFX.OBJ scaffold.
 *
 * Player-specific effects, damage, weapon trails, and player-data loading.
 * The last body scans for the player-data files; the following one-instruction
 * body is PSX2.OBJ's LoadVU1GameLogic.
 *
 * .text       0x80089120..0x8008BC50
 * extab       0x80006BE8..0x80006C50
 * extabindex  0x8000AA2C..0x8000AAC8
 *
 * Player-data model (lbl_80282930[4], one header per player). All three
 * record types are the real Midway ones recovered from the Xbox PDB and
 * proved against LoadPlyrData's own byte-swap loops -- see each typedef:
 *   plyr_data   0x180 - the header chunk (numsfx/numdamage + the two chunk
 *                       pointers + the turbo/combo table + tuning floats).
 *   plyr_sfx    0x50  - the "record" chunk rows (effect/sound descriptors
 *                       and their resolved handles).
 *   plyr_damage 0x58  - the "move" chunk rows (per-frame damage windows).
 */

#include "types.h"
#include "game/effect.h"
#include "game/player.h"
#include "game/mbobject.h"

#define offsetof(type, memb) ((u32) & ((type*)0)->memb)

extern u8* lbl_80282930[4];
extern void ClearCustomEffect(s32 index);
extern void* player_multiple_models[];
typedef struct PsfxPdataBuf {
    char  name[0x20];  /* 0x00 : sprintf'd "<class>.wad" filename */
    s32   cur[4];      /* 0x20 : per-player currently-loaded class */
    void* bufs[4];     /* 0x30 : per-player load buffers */
    s32   sizes[16];   /* 0x40 : per-class pdata file sizes */
    u8*   headers[4];  /* 0x80 : parsed per-player headers */
} PsfxPdataBuf;
typedef struct PsfxFileTable {
    u8    pad[0x60];
    void* files[16];
} PsfxFileTable;
/* Player-data header, the first chunk of the pdata wad; stride 0x180.
 *
 * Name + field names are the real Midway ones, from the Xbox PDB `plyr_data`
 * (research/xbox_symbols/misc.h Id=3433, Size=0x180; the neighbouring Ids
 * 3432/3434 are plyr_damage/plyr_sfx, this file's other two record types).
 * The GC layout is proved by LoadPlyrData's own byte-swap loop for the 0x180
 * chunk (claim.law.swap-loop-is-record-layout-ground-truth): it swaps the 14
 * shorts at 0x00/0x02 and 0x0C..0x22, the u32 at 0x24, the 13 floats at
 * 0x28..0x58, the [3] vectors at 0x5C/0x158/0x164/0x170, the two [10][3]
 * float tables at 0x68/0xE0, and the lone float at 0x17C -- and skips
 * exactly 0x04 and 0x08, the two pointer fields that get resolved after the
 * load rather than swapped.
 *
 * sfx and damage keep their reconstructed u8* type (the PDB spells them
 * plyr_sfx and plyr_damage pointers) so the existing byte-stride pointer
 * arithmetic on them is unchanged. */
typedef struct plyr_data {
    /* 0x000 */ s16 numsfx;    /* number of plyr_sfx records     */
    /* 0x002 */ s16 numdamage; /* number of plyr_damage rows     */
    /* 0x004 */ u8* sfx;       /* plyr_sfx[numsfx], 0x50 each    */
    /* 0x008 */ u8* damage;    /* plyr_damage[numdamage], 0x58   */
    /* 0x00C */ s16 turboAclose;
    /* 0x00E */ s16 turboAlow;
    /* 0x010 */ s16 turboAstep;
    /* 0x012 */ s16 turboA360;
    /* 0x014 */ s16 turboAthrow;
    /* 0x016 */ s16 turboB;
    /* 0x018 */ s16 turboC1;
    /* 0x01A */ s16 turboC2;
    /* 0x01C */ s16 combo1;
    /* 0x01E */ s16 combo2;
    /* 0x020 */ s16 combohit;
    /* 0x022 */ s16 victory;
    /* 0x024 */ s32 initflag;  /* handles resolved this level    */
    /* 0x028 */ f32 fight_min;
    /* 0x02C */ f32 fight_max;
    /* 0x030 */ f32 speed_min;
    /* 0x034 */ f32 speed_max;
    /* 0x038 */ f32 armor_min;
    /* 0x03C */ f32 armor_max;
    /* 0x040 */ f32 magic_min;
    /* 0x044 */ f32 magic_max;
    /* 0x048 */ f32 height;
    /* 0x04C */ f32 width;
    /* 0x050 */ f32 attny;
    /* 0x054 */ f32 coly;
    /* 0x058 */ f32 powerup_time;
    /* 0x05C */ f32 weapon_offset[3];
    /* 0x068 */ f32 weapon_fx_offset[10][3];
    /* 0x0E0 */ f32 weapon_fx_scale[10][3];
    /* 0x158 */ f32 turboa_offset[3];
    /* 0x164 */ f32 familiar_offset[3];
    /* 0x170 */ f32 fam_proj_offset[3];
    /* 0x17C */ f32 streakfwdmul;
} plyr_data; /* size 0x180 = 384 */
extern PsfxPdataBuf lbl_802828B0;
extern u8 lbl_8012006C[];
extern void* lbl_80120E00[16];
extern char lbl_801142A0[];
extern char lbl_80114288[];
extern char lbl_801142D4[];
extern const char lbl_80347E44[7];   /* "%s.wad" (sdata2) */
extern const char lbl_80347E4C[6];   /* "pdata"  (sdata2) */
extern s32 mlmLastFileSize;
extern const char lbl_80347E3C[5];  /* "%s%s" (sdata2) */
extern void* InitCustomEffect();
extern s32 MBOX_FindTexture_Sub();
extern s32 AudioFindSound();
extern void* MBOX_ReallyFindObject();
extern s32* AtreeFindMbidxNode();
extern s32 sprintf(void* dst, const char* fmt, ...);
extern s32 FileExists();
extern void* AllocFile();
extern s32 MLMReadFile();
extern void FatalErrorf(const char* fmt, ...);
extern void ErrorPrintf(const char* fmt, ...);
extern void* AllocMem();
extern u32 gFrameTicks;
extern s32 lbl_80343DB0;
extern u8* lbl_80344B40;
extern Effect Effects[64];
extern f32 gIdentityMatrix[];
extern f64 lbl_80347D98;
extern f32 lbl_80347DA0;
extern f64 lbl_80347DE8;
extern f32 lbl_80347E24;
extern f32 lbl_80347E00;
extern f64 lbl_80347E28;
extern f32 lbl_80347E30;
extern f32 lbl_80347E34;
extern f32 lbl_80347E38;
extern f32 lbl_80347DAC;
extern f32 lbl_80347DB8;
extern s32 WeaponStreakTex;
extern s32 lbl_8011A178[];
extern void* MBNewPsysDefault(f32* matrix, void* parent, s32 flags, s32 arena);
extern void MBPsysSetPTex(void* psys, s32 texture);
extern void MBPsysScalePParm(f32 scale, void* psys, s32 parameter);
extern void MBPsysSetPTime(f32 time, void* psys);
extern void MBPsysSetPSpeed(f32 speed, void* psys);
extern void MBPsysSetERate4(f32 a, f32 b, f32 c, f32 d, void* psys);
extern void MBPsysSetEVolume(f32 base, f32 range, void* psys);
extern void MBPsysSetETime(f32 duration, f32 fade, void* psys);
extern void MBTreeSetFlags(void* node, u32 flags, s32 recurse);
extern void SetSkinFX(void* skinFx, s32 base, s32 frames, s32 loops, f32 rate);
extern void MulVecMat3(const f32* vector, f32* out, const f32* matrix);
extern void MulVecMat4(const f32* vector, f32* out, const f32* matrix);
extern s32 StartFXSub(s32 type, f32* position, u32 flagsA, u32 flagsB, f32 time);
extern void SfxSetMat(s32 effect, f32* matrix, f32* position);
extern void SfxSetParent(s32 effect, void* parent);
extern void SfxSetStreak(s32 effect, s32 texture, u32 color, s32 alpha,
                        f32 scale, f32 forwardScale);
extern void MBTreeSetAmbientAdd(void* node, s32 value, s32 recurse);
extern void MBTreeSetColor(void* node, u32 color, s32 recurse);
extern void AudioPlay3DSel(s32 sound, s32 volume, f32* position, s32 selector);
extern void ShakeCamera(s32 type, s32 count, s32 delay, f32 radius,
                        s32 priority);
extern void SafeRockSetup(void);
extern void GetWorldMat(void* node, f32* matrix, void* offset);
extern void* MBRemoveNode(void* node, s32 mode);
extern void MBTreeSetAlpha(void* node, s32 alpha, s32 mode);
extern void* MBNewObject(s32 object, f32* matrix, void* parent, u32 flags);
void fn_8008A678();

#define STUB(address, name) void name(void) {}

typedef struct PlayerTrailState {
    u8 _pad000[0x74];
    void* activeNode;
    u8 _pad078[0x668];
    void* worldNode;
    u8 _pad6E4[0x110];
    s32 objectGroup;
    u8 _pad7F8[8];
    s32 lastTrail;
    s32 trailActive;
    void* trails[8];
} PlayerTrailState;

/* Fade the player's existing weapon-trail nodes, recycle the brightest slot,
 * and keep a fresh trail node at the weapon's current world transform. */
void PlayerDoWeapTrail(PlayerTrailState* player)
{
    f32 matrix[16];
    u8 unused[12];
    s32 minimumIndex = 0;
    s32 maximumIndex = 0;
    s32 minimumAlpha = 255;
    s32 maximumAlpha = 0;
    s32 lastIndex = player->lastTrail;
    void** slot;
    s32 offset;
    s32 index;
    s32 alpha;
    s32 chosenIndex;
    void* node;
    s32 object;
    f32 dx;
    f32 dy;
    f32 dz;

    if (player->activeNode == NULL) {
        goto done;
    }
    if (gFrameTicks == 0) {
        goto done;
    }

ticks_active:
    if (lastIndex >= 0 || player->trailActive != 0) {
                index = 0;
                offset = 0;
                while (index <= lastIndex) {
                    slot = (void**)&player->trails[index];
                    if ((node = *slot) != NULL) {
                        if ((((MBObject*)node)->flags & 0x200) != 0) {
                            alpha = 255 - ((MBObject*)node)->alpha;
                        } else {
                            alpha = 0;
                        }
                        alpha += lbl_80343DB0 * gFrameTicks;
                        if (alpha >= 255) {
                            alpha = 255;
                            *slot = MBRemoveNode(node, 1);
                        }
                        MBTreeSetAlpha(*slot, alpha, 1);
                    } else {
                        alpha = 255;
                    }
                    if (alpha > maximumAlpha) {
                        maximumAlpha = alpha;
                        maximumIndex = index;
                    }
                    if (alpha < minimumAlpha) {
                        minimumAlpha = alpha;
                        minimumIndex = index;
                    }
                    index++;
                    offset += 4;
                }

                offset = lastIndex * 4;
                while (lastIndex >= 0 &&
                       player->trails[lastIndex] == NULL) {
                    lastIndex--;
                    offset -= 4;
                }
                player->lastTrail = lastIndex;

                if (player->trailActive != 0) {
                    if (maximumIndex <= lastIndex && lastIndex < 7) {
                        chosenIndex = lastIndex + 1;
                    } else {
                        chosenIndex = maximumIndex;
                    }
                    GetWorldMat(player->worldNode, matrix, NULL);
                    if (minimumIndex <= lastIndex) {
                        node = player->trails[minimumIndex];
                        if (node != NULL) {
                            dx = ((MBObject*)node)->mat[3][0] - matrix[12];
                            dy = ((MBObject*)node)->mat[3][1] - matrix[13];
                            dz = ((MBObject*)node)->mat[3][2] - matrix[14];
                            if ((f64)(dx * dx + dy * dy + dz * dz) < 0.01) {
                                return;
                            }
                        }
                    }

                    node = player->worldNode;
                    slot = (void**)((u8*)player + chosenIndex * 4);
                    object = (((MBObject*)node)->index & 0xFFFF) |
                             (player->objectGroup << 16);
                    if (*(slot += 0x202) != NULL) {
                        MBRemoveNode(*slot, 1);
                    }
                    *slot = MBNewObject(object, matrix, NULL, 0x800);
                    if (chosenIndex > player->lastTrail) {
                        player->lastTrail = chosenIndex;
                    }
                }
    }
done:
    ;
}
extern f32 lbl_80347DA4;
extern f32 lbl_80347DA8;
extern f32 lbl_80347DAC;
extern f32 lbl_80347DB0;
extern f32 lbl_80347DB4;
extern f32 lbl_80347DB8;
extern f64 lbl_80347DC0;
extern f32 lbl_80347DC8;
extern f64 lbl_80347DD0;
extern f64 lbl_80347DD8;
extern f32 lbl_80347DE0;
extern f64 lbl_80347DE8;
extern s32 pmissile_sfxidx[];
extern void fn_80067AE0(s32 flags, f32 a, f32 b);
extern void msgPost(s32 id, s32 player, void* ptr);
extern void MBTreeClearFlags(void* node, u32 flags, s32 recurse);
extern void YawVec3(f32* src, f32* out, f32 yaw);
extern s32 PlayerStartMissile(u8* p, f32* vec, u32 mask, s32 mode, f32 a,
                              f32 b);
extern void MBRemovePolyInst(void* inst);
extern void player_get_powerup_state(u8* p, s32 kind, u32 mask, f32 dt);
/* One row of the pdata wad's second ("record") chunk, stride 0x50.
 *
 * Name + field names are the real Midway ones, from the Xbox PDB `plyr_sfx`
 * (research/xbox_symbols/misc.h Id=3434, Size=0x50). Confirmed twice over:
 *
 *  1. LoadPlyrData's byte-swap loop for the 0x50 chunk (claim.law.
 *     swap-loop-is-record-layout-ground-truth) swaps the u32s at
 *     0x00/0x04/0x08/0x0C, the shorts at 0x30/0x32, the [3] vector at 0x34
 *     and the floats/u32 at 0x40/0x44/0x48/0x4C, and skips exactly
 *     0x10..0x2F -- the two 16-byte text fields, which never need swapping.
 *  2. fn_8008A678 resolves this record: it reads rec+0x10 and rec+0x20 as
 *     C strings (fxdesc/snddesc) and writes the looked-up handles back into
 *     0x08/0x0C, which PlayerSfxInitData pre-clears to -1 -- i.e. sfxidx and
 *     sndidx, not the "texture"/"parent" this reconstruction had guessed
 *     (0x0C is passed to AudioPlay3DSel as the sound id, 0x08 to StartFXSub/
 *     SetSkinFX as the effect id). nextfxidx@0x04 is the chained record index
 *     DoPlyrSfxSub recurses on.
 *
 * Declared widths are left exactly as the pre-existing reconstruction had
 * them; the PDB spells flags/nextfxidx/sfxidx/sndidx int and color unsigned. */
typedef struct plyr_sfx {
    /* 0x00 */ u32 flags;
    /* 0x04 */ u32 nextfxidx; /* chained record index, -1 = none */
    /* 0x08 */ s32 sfxidx;    /* resolved effect/texture handle  */
    /* 0x0C */ s32 sndidx;    /* resolved sound/mbox handle      */
    /* 0x10 */ char fxdesc[16];
    /* 0x20 */ char snddesc[16];
    /* 0x30 */ s16 zmod;
    /* 0x32 */ s16 alphamod;
    /* 0x34 */ f32 offset[3];
    /* 0x40 */ f32 maxlen;
    /* 0x44 */ f32 radius;
    /* 0x48 */ f32 scale;
    /* 0x4C */ u32 color;
} plyr_sfx; /* size 0x50 = 80 */

/* One row of the pdata wad's third ("move") chunk, stride 0x58.
 *
 * Name + field names are the real Midway ones, from the Xbox PDB
 * `plyr_damage` (research/xbox_symbols/misc.h Id=3432, Size=0x58 -- the
 * neighbouring Ids 3433/3434 are plyr_data/plyr_sfx, i.e. this file's other
 * two record types). The GC layout is confirmed twice over:
 *
 *  1. LoadPlyrData's own byte-swap loop for the 0x58 chunk (claim.law.
 *     swap-loop-is-record-layout-ground-truth) swaps exactly the shorts at
 *     0x00/0x02/0x48/0x4A/0x4C/0x4E/0x50/0x52/0x54, the u32 at 0x04, and the
 *     floats at 0x08..0x44 -- matching this layout field for field, including
 *     dmgtype being the one 4-byte non-float in the float run.
 *  2. Consumer behaviour here and in PlyrSfxDoDamageSub: dmgtype@0x04 ->
 *     Effects[].damagetype, radius@0x0C -> Effects[].damageradius,
 *     delay@0x14 -> Effects[].damagedelay, mintime@0x18 -> minendtime,
 *     angle@0x20 -> YawMat3, pitch@0x28 -> PitchMat3, offset@0x2C -> the
 *     MulVecMat3/4 position vector, next@0x4E -> the PlyrSfxDoDamage chain
 *     argument, startframe/endframe@0x50/0x52 -> the [t0,t1) window test,
 *     helpidx@0x54 -> msgPost.
 *
 * Used only for offsetof() displacements on the raw walked row pointer,
 * never as a typed alias (claim.law.multifield-alias-defeats-indexed-
 * addressing: 3+ nearby fields off one index-computed base regress). */
typedef struct plyr_damage {
    /* 0x00 */ s16 type;
    /* 0x02 */ s16 flags;
    /* 0x04 */ u32 dmgtype;   /* PDB: enum DMG_TYPE */
    /* 0x08 */ f32 hitrad;
    /* 0x0C */ f32 radius;
    /* 0x10 */ f32 minrad;
    /* 0x14 */ f32 delay;
    /* 0x18 */ f32 mintime;
    /* 0x1C */ f32 maxtime;
    /* 0x20 */ f32 angle;
    /* 0x24 */ f32 arc;
    /* 0x28 */ f32 pitch;
    /* 0x2C */ f32 offset[3];
    /* 0x38 */ f32 amount;
    /* 0x3C */ f32 speed_min;
    /* 0x40 */ f32 speed_max;
    /* 0x44 */ f32 weight;
    /* 0x48 */ s16 fxidx;
    /* 0x4A */ s16 hitfxidx;
    /* 0x4C */ s16 loopfxidx;
    /* 0x4E */ s16 next;
    /* 0x50 */ s16 startframe;
    /* 0x52 */ s16 endframe;
    /* 0x54 */ s16 helpidx;
    /* 0x56 */ s16 dummy;
} plyr_damage; /* size 0x58 = 88 */

s32 PlyrSfxDoDamageSub(u8* p, u8* row, s32 mode, u8* other);
void PlyrSfxDoDamage(u8* p, s32 idx, u8* p2, u8* other, f32 t0, f32 t1);

/* 0x80089350 - run one player-sfx sequence row for the [t0,t1) frame window,
 * chaining to the linked row when done. */
void PlyrSfxDoDamage(u8* p, s32 idx, u8* p2, u8* other, f32 t0, f32 t1)
{
    f32 buf[3];
    f32 conv[2];
    u8* row;
    s32 fl;
    s32 n;
    s32 i;
    u32 mask;
    f32 sf;
    f32 ef;
    f32 frac;
    f32 rate;
    f32 scale;
    f32 k;

    if (idx < 0) {
        return;
    }
    row = *(u8**)(lbl_80282930[((Player*)p)->index] + offsetof(plyr_data, damage)) + idx * 88;
    sf = (f32)*(s16*)(row + offsetof(plyr_damage, startframe));
    ef = (f32)*(s16*)(row + offsetof(plyr_damage, endframe));
    if (t1 >= sf) {
        if (ef < lbl_80347DA0 || t0 < ef) {
            fl = *(s16*)(row + offsetof(plyr_damage, flags));
            if (fl & 0x2000) {
                fn_80067AE0(fl, lbl_80347DA4, lbl_80347DA8);
                {
                    f32 v = lbl_80347DAC;
                    ((Player*)p)->pulse_7FC = v;
                    if (p2 != NULL) {
                        ((Player*)p2)->pulse_7FC = v;
                    }
                }
            } else if (fl & 0x20) {
                fn_80067AE0(fl, lbl_80347DA4, lbl_80347DB0);
                {
                    f32 v = lbl_80347DAC;
                    ((Player*)p)->pulse_7FC = v;
                    if (p2 != NULL) {
                        ((Player*)p2)->pulse_7FC = v;
                    }
                }
            } else if (fl & 0x10) {
                fn_80067AE0(fl, lbl_80347DA4, lbl_80347DB4);
                {
                    f32 v = lbl_80347DAC;
                    ((Player*)p)->pulse_7FC = v;
                    if (p2 != NULL) {
                        ((Player*)p2)->pulse_7FC = v;
                    }
                }
            }
        }
    }
    if (ef < lbl_80347DA0) {
        ef = sf;
    } else {
        ef = ef + lbl_80347DB8;
    }
    {
        f32 one = lbl_80347DB8;
        if (t0 < one && t1 >= one && *(s16*)(row + offsetof(plyr_damage, helpidx)) >= 0) {
            msgPost(*(s16*)(row + offsetof(plyr_damage, helpidx)), ((Player*)p)->index, ((Player*)p)->col_pos);
        }
    }
    if (*(s16*)(row + offsetof(plyr_damage, flags)) & 0x400) {
        if (t1 >= sf && t1 < ef) {
            if (((Player*)p)->weaphold_node != NULL) {
                MBTreeSetFlags(((Player*)p)->weaphold_node, 2, 0);
            }
        } else {
            if (((Player*)p)->weaphold_node != NULL) {
                MBTreeClearFlags(((Player*)p)->weaphold_node, 2, 0);
            }
        }
    }
    if (*(s16*)(row + offsetof(plyr_damage, flags)) & 0x80) {
        ef = sf;
    }
    if (t1 >= sf && t0 < ef) {
        f32 zero = lbl_80347DA0;
        if (zero != *(f32*)(row + offsetof(plyr_damage, amount))) {
            ((Player*)p)->power_target = ((Player*)p)->power_target - ((Player*)p)->coll_score;
            ((Player*)p)->coll_score = zero;
        }
        lbl_80344B40 = p2;
        switch (*(s16*)row) {
        case 2:
            PlyrSfxDoDamageSub(p, row, 0, other);
            break;
        case 3:
        case 4:
            PlyrSfxDoDamageSub(p, row, 1, other);
            break;
        case 10:
            n = 0;
            if (t1 == t0) {
                break;
            }
            mask = ((Player*)p)->field_11C & 0xFFB7FFFF;
            if (sf == ef) {
                if (lbl_80347DC0 == (f64)*(f32*)(row + offsetof(plyr_damage, delay))) {
                    YawVec3(&((Player*)p)->mat[8], buf, *(f32*)(row + offsetof(plyr_damage, angle)));
                    n = PlayerStartMissile(p, buf, mask, 0, lbl_80347DC8,
                                           lbl_80347DAC);
                } else {
                    f64 kx;
                    f64 ky;
                    f32 acc;
                    f64 kone;
                    acc = lbl_80347DA0;
                    kx = lbl_80347DD0;
                    ky = lbl_80347DD8;
                    kone = lbl_80347DE8;
                    while (acc < kone) {
                        YawVec3(&((Player*)p)->mat[8], buf,
                                *(f32*)(row + offsetof(plyr_damage, angle)) * acc);
                        rate = lbl_80347DE0;
                        buf[0] = (f32)(buf[0] * kx);
                        scale = lbl_80347DAC;
                        buf[1] = (f32)(buf[1] * ky);
                        buf[2] = (f32)(buf[2] * kx);
                        n += PlayerStartMissile(p, buf, mask, 0,
                                                rate, scale);
                        acc = (f32)(acc + kone / *(f32*)(row + offsetof(plyr_damage, delay)));
                    }
                }
            } else {
                rate = *(f32*)(row + offsetof(plyr_damage, delay));
                if ((f64)rate > lbl_80347DC0) {
                    frac = t1 - sf;
                    if (!(__fabs(rate) > __fabs(frac))) {
                        frac = frac - rate * (f32)(s64)(u64)(frac / rate);
                    }
                    if ((s32)frac != 0) {
                        break;
                    }
                }
                fl = *(s16*)(row + offsetof(plyr_damage, flags));
                scale = *(f32*)(row + offsetof(plyr_damage, angle));
                if (fl & 0x300) {
                    if (ef > sf) {
                        k = (t1 - sf) / (ef - sf);
                    } else {
                        k = lbl_80347DB8;
                    }
                    if (fl & 0x200) {
                        k = (f32)(lbl_80347DE8 - k);
                    }
                    scale = scale * k;
                }
                YawVec3(&((Player*)p)->mat[8], buf, scale);
                buf[1] = lbl_80347DB8;
                buf[0] = buf[0] * *(f32*)(row + offsetof(plyr_damage, offset[0]));
                buf[1] = buf[1] * *(f32*)(row + offsetof(plyr_damage, offset[1]));
                buf[2] = buf[2] * *(f32*)(row + offsetof(plyr_damage, offset[2]));
                n = PlayerStartMissile(p, buf, mask, 0, lbl_80347DC8,
                                       lbl_80347DAC);
                {
                    f32 zero = lbl_80347DA0;
                    for (i = 0; i < n; i++) {
                        s32 ei = pmissile_sfxidx[i];
                        u8* fx;
                        if (ei < 0) {
                            continue;
                        }
                        fx = (u8*)Effects + ei * 240;
                        if (*(f32*)(row + offsetof(plyr_damage, weight)) > zero) {
                            *(f32*)(fx + 160) =
                                *(f32*)(fx + 160) * *(f32*)(row + offsetof(plyr_damage, weight));
                        }
                        if (*(s16*)(row + offsetof(plyr_damage, flags)) & 0x1000) {
                            if (*(void**)(fx + 212) != NULL) {
                                MBRemovePolyInst(*(void**)(fx + 212));
                                *(s32*)(fx + 212) = 0;
                            }
                        }
                    }
                }
                if (t1 > lbl_80347DB8 + sf) {
                    n = 0;
                }
            }
            if (n > 0 && (*(u32*)(p + 284) & 0x00100000)) {
                player_get_powerup_state(p, 5, 0x100000, lbl_80347DB8);
            }
            break;
        case 0:
        case 5:
            break;
        default:
            PlyrSfxDoDamageSub(p, row, 1, other);
            break;
        }
    }
    if (*(s16*)(row + offsetof(plyr_damage, next)) >= 0) {
        PlyrSfxDoDamage(p, *(s16*)(row + offsetof(plyr_damage, next)), p2, other, t0, t1);
    }
}

extern s32 optionsAudioAndPrefs30[];
extern void MulBodyVecMat4(f32* in, f32* out, void* mtx);
extern void PlaceEffectOnFloor(s32 fx, void* node);
extern void YawMat3(void* node, f32 yaw);
extern void PitchMat3(void* node, f32 pitch);
extern void SfxSetHit(s32 fx, u32 a, u32 b, u32 c);
extern void SfxSetMorph(s32 fx, u32 a, s32 b, f32 t);
extern void SfxSetLight(s32 fx, void* color, f32 rad);
extern void PitchVec3(f32* in, f32* out, f32 pitch);
extern f32 Random(f32 range);
extern void SfxSetPhysics(s32 fx, f32* vel, f32* rnd, f32 a, f32 b);
extern void DmgFxAdd(s32 fx);
extern f32 gClockTime;
extern u32 gControllerButtons;
s32 DoPlyrSfxSub(u8* player, s32 recordIndex, f32* offset, s32 absolute,
                 s32 effectIndex);
extern u8 lbl_80120DA0[];
extern f64 lbl_80347DF8;
extern f32 lbl_80347E00;
extern f64 lbl_80347E08;
extern f32 lbl_80347E10;
extern f64 lbl_80347E18;
extern f32 lbl_80347E20;

/* 0x800898DC - start one player effect from a sequence row: spawn through
 * DoPlyrSfxSub, then seed flags, lifetime, hit/morph links and velocity. */
s32 PlyrSfxDoDamageSub(u8* p, u8* row, s32 mode, u8* other)
{
    u8 highPad[8];
    f32 pos[3];
    f32 vel[3];
    f32 rnd[3];
    s32 pidx;
    u8** hdrp;
    u8* seq;
    u8* sub;
    u8* flp;
    u32 fl;
    f32 health;
    u8 unused[20];

    pidx = ((Player*)p)->index;
    if (mode != 0) {
        pos[0] = *(f32*)(row + offsetof(plyr_damage, offset[0]));
        pos[1] = *(f32*)(row + offsetof(plyr_damage, offset[1]));
        pos[2] = *(f32*)(row + offsetof(plyr_damage, offset[2]));
        if (other != NULL) {
            MulBodyVecMat4((f32*)other, (f32*)other, ((Player*)p)->mat);
            pos[0] = *(f32*)other + pos[0];
            pos[1] = *(f32*)(other + 4) + pos[1];
            pos[2] = *(f32*)(other + 8) + pos[2];
        }
    } else {
        if (other != NULL) {
            MulVecMat3((f32*)(row + offsetof(plyr_damage, offset[0])), pos, ((Player*)p)->mat);
            pos[0] = *(f32*)other + pos[0];
            pos[1] = *(f32*)(other + 4) + pos[1];
            pos[2] = *(f32*)(other + 8) + pos[2];
        } else {
            MulVecMat4((f32*)(row + offsetof(plyr_damage, offset[0])), pos, ((Player*)p)->mat);
        }
    }
    mode = DoPlyrSfxSub(p, *(s16*)(row + offsetof(plyr_damage, fxidx)), pos, mode, -1);
    if (mode < 0) {
        goto done;
    }
    hdrp = &lbl_80282930[pidx];
    seq = ((plyr_data*)*hdrp)->sfx + *(s16*)(row + offsetof(plyr_damage, fxidx)) * 80;
    fl = 0;
    switch (*(s16*)row) {
    case 2:
        fl |= 14;
        break;
    case 3:
        fl |= 58;
        break;
    case 4:
    case 6:
    case 7:
        fl |= 42;
        break;
    case 9:
        fl |= 42;
        break;
    }
    fl |= 256;
    if (*(s32*)((u8*)optionsAudioAndPrefs30 + 28) == 2) {
        fl |= 1;
    } else {
        fl |= 512;
    }
    if (*(s16*)(row + offsetof(plyr_damage, flags)) & 0x40) {
        fl &= ~4;
    }
    if (*(u32*)(row + offsetof(plyr_damage, dmgtype)) & 0x00020000) {
        fl |= 0x20000;
    }
    flp = (u8*)&Effects[mode];
    *(u32*)(flp += 100) |= fl;
    if ((*(u32*)seq & 0x10) && *(s16*)(row + offsetof(plyr_damage, hitfxidx)) < 0) {
        PlaceEffectOnFloor(mode, Effects[mode].node);
    }
    if (0.0f != *(f32*)(row + offsetof(plyr_damage, angle))) {
        YawMat3(Effects[mode].node, *(f32*)(row + offsetof(plyr_damage, angle)));
    }
    if (0.0f != *(f32*)(row + offsetof(plyr_damage, pitch))) {
        PitchMat3(Effects[mode].node, *(f32*)(row + offsetof(plyr_damage, pitch)));
    }
    health = *(f32*)(row + offsetof(plyr_damage, amount));
    if (health < 0.0f) {
        health = ((Player*)p)->stat_damage * -health;
    }
    if (health > 0.0f) {
        Effects[mode].damage = health;
        Effects[mode].mindp = *(f32*)(row + offsetof(plyr_damage, arc));
        Effects[mode].damageradius = *(f32*)(row + offsetof(plyr_damage, radius));
        Effects[mode].damagetype = *(u32*)(row + offsetof(plyr_damage, dmgtype));
        Effects[mode].damagedelay = *(f32*)(row + offsetof(plyr_damage, delay));
        if (*(f32*)(row + offsetof(plyr_damage, mintime)) > lbl_80347DC0) {
            Effects[mode].minendtime = gClockTime + *(f32*)(row + offsetof(plyr_damage, mintime));
        }
        Effects[mode].owner = pidx + 1;
        if (*(s16*)(row + offsetof(plyr_damage, hitfxidx)) >= 0) {
            sub = ((plyr_data*)*hdrp)->sfx + *(s16*)(row + offsetof(plyr_damage, hitfxidx)) * 80;
            SfxSetHit(mode, *(u32*)(sub + offsetof(plyr_sfx, sfxidx)), *(u32*)(sub + offsetof(plyr_sfx, sndidx)),
                      *(u32*)(sub + offsetof(plyr_sfx, sndidx)));
            if (*(u32*)sub & 0x10) {
                *(u32*)flp |= 0x200000;
            }
        }
        if (*(s16*)(row + offsetof(plyr_damage, loopfxidx)) >= 0) {
            sub = ((plyr_data*)*hdrp)->sfx + *(s16*)(row + offsetof(plyr_damage, loopfxidx)) * 80;
            SfxSetMorph(mode, *(u32*)(sub + offsetof(plyr_sfx, sfxidx)), 0, *(f32*)(row + offsetof(plyr_damage, maxtime)));
            if (*(s16*)(row + offsetof(plyr_damage, flags)) & 0x800) {
                *(u32*)flp |= 0x8000;
            }
        }
        if (*(f32*)(row + offsetof(plyr_damage, radius)) != 0.0f) {
            SfxSetLight(mode, lbl_80120DA0 + ((Player*)p)->char_type * 12,
                        (f32)(lbl_80347DF8 * *(f32*)(row + offsetof(plyr_damage, radius))));
        } else if (*(f32*)(row + offsetof(plyr_damage, hitrad)) != 0.0f) {
            SfxSetLight(mode, lbl_80120DA0 + ((Player*)p)->char_type * 12,
                        (f32)(lbl_80347DF8 * *(f32*)(row + offsetof(plyr_damage, hitrad))));
        }
        if (*(f32*)(row + offsetof(plyr_damage, speed_min)) > 0.0f) {
            f32 vy;
            f32 spd = lbl_80347E00 * (*(f32*)(row + offsetof(plyr_damage, speed_max)) - *(f32*)(row + offsetof(plyr_damage, speed_min))) +
                      *(f32*)(row + offsetof(plyr_damage, speed_min));
            if (*(s16*)(row + offsetof(plyr_damage, flags)) & 4) {
                vel[0] = ((Player*)p)->mat[8];
                vel[1] = ((Player*)p)->mat[9];
                vel[2] = ((Player*)p)->mat[10];
            } else {
                vy = *(f32*)(p + 2236);
                if (*(u32*)(p + 2240) & 8) {
                    if (vy > lbl_80347E08) {
                        vy = lbl_80347E10;
                    }
                    if (vy > 0.0f && vy < lbl_80347E18) {
                        vy = lbl_80347E00;
                    }
                }
                vel[0] = ((Player*)p)->mat[8] * spd;
                vel[1] = vy * spd;
                vel[2] = ((Player*)p)->mat[10] * spd;
            }
            if (*(s16*)row == 2) {
                if (0.0f != *(f32*)(row + offsetof(plyr_damage, angle))) {
                    YawVec3(vel, vel, *(f32*)(row + offsetof(plyr_damage, angle)));
                }
                if ((*(s16*)(row + offsetof(plyr_damage, flags)) & 8) &&
                    0.0f != *(f32*)(row + offsetof(plyr_damage, pitch))) {
                    PitchVec3(vel, vel, *(f32*)(row + offsetof(plyr_damage, pitch)));
                }
            }
            if (*(u32*)seq & 8) {
                rnd[0] = Random(lbl_80347E20);
                rnd[1] = 0.0f;
                rnd[2] = Random(lbl_80347E20);
                SfxSetPhysics(mode, vel, rnd, *(f32*)(row + offsetof(plyr_damage, weight)),
                            *(f32*)(row + offsetof(plyr_damage, hitrad)));
            } else {
                SfxSetPhysics(mode, vel, 0, *(f32*)(row + offsetof(plyr_damage, weight)), *(f32*)(row + offsetof(plyr_damage, hitrad)));
            }
        } else {
            SfxSetPhysics(mode, 0, 0, *(f32*)(row + offsetof(plyr_damage, weight)), *(f32*)(row + offsetof(plyr_damage, hitrad)));
        }
        if ((*(u64*)&gControllerButtons & 16) != 0 &&
            (*(u64*)&gControllerButtons & 1) != 0) {
            DmgFxAdd(mode);
        }
    } else {
        if (*(s16*)(row + offsetof(plyr_damage, hitfxidx)) >= 0) {
            sub = ((plyr_data*)*hdrp)->sfx + *(s16*)(row + offsetof(plyr_damage, hitfxidx)) * 80;
            SfxSetHit(mode, *(u32*)(sub + offsetof(plyr_sfx, sfxidx)), *(u32*)(sub + offsetof(plyr_sfx, sndidx)),
                      *(u32*)(sub + offsetof(plyr_sfx, sndidx)));
        }
    }
done:
    return mode;
}

s32 DoPlyrSfxSub(u8* player, s32 recordIndex, f32* offset,
                 s32 absolute, s32 effectIndex);
s32 DoPlyrSfx(u8* player, plyr_sfx* record, f32* position,
              s32 absolute, u32 flags, s32 effectIndex);

/* Build and configure the particle node for one player-SFX record.
 * Xbox PDB: PSFX.OBJ local PsfxDoParticle. */
void PsfxDoParticle(u8* player, plyr_sfx* record, s32 effectIndex)
{
    void* psys;
    void* parent;
    u32 flags;
    u32 particleKind;
    s32 parentHandle;
    s32 texture;
    f32 rate;
    f32 speed;
    f32 duration;
    f32 parameterScale;
    u8 unused[8];

    flags = record->flags;
    particleKind = flags & 0x0F000000;
    rate = (f32)(lbl_80347E28 * (f64)record->radius);
    texture = record->sfxidx;
    parentHandle = record->sndidx;
    duration = record->maxlen;
    parameterScale = record->scale;
    speed = (f32)(lbl_80347D98 * (f64)record->alphamod);

    if ((flags & 0x40000) != 0 && effectIndex >= 0) {
        parent = Effects[effectIndex].node;
    } else if ((flags & 0x2000) != 0 && lbl_80344B40 != NULL) {
        parent = ((MBObject*)lbl_80344B40)->parent;
    } else if (parentHandle == -1) {
        parent = ((Player*)player)->node;
    } else {
        parent = (void*)parentHandle;
    }

    switch (particleKind) {
    case 0x02000000:
        psys = MBNewPsysDefault((f32*)gIdentityMatrix, parent, 0, 1);
        if (psys != NULL) {
            MBPsysSetEVolume(lbl_80347E30, lbl_80347DA0, psys);
            if (lbl_80347DE8 != (f64)parameterScale) {
                MBPsysScalePParm(parameterScale, psys, 4);
            }
            MBPsysSetERate4(rate, rate, rate, rate, psys);
            MBPsysSetETime(duration, lbl_80347E34, psys);
        }
        break;
    case 0x01000000:
    default:
        psys = MBNewPsysDefault((f32*)gIdentityMatrix, parent, 0, 1);
        MBPsysSetPTime(lbl_80347E38, psys);
        if (lbl_80347DE8 != (f64)parameterScale) {
            MBPsysScalePParm(parameterScale, psys, 4);
        }
        MBPsysSetERate4(rate, rate, rate, rate, psys);
        MBPsysSetETime(duration, lbl_80347E34, psys);
        MBPsysSetEVolume(lbl_80347E00, lbl_80347E00, psys);
        break;
    }

    if (psys == NULL) {
        ErrorPrintf(lbl_80114288);
    } else {
        ((MBObject*)psys)->mat[3][0] = record->offset[0];
        ((MBObject*)psys)->mat[3][1] = record->offset[1];
        ((MBObject*)psys)->mat[3][2] = record->offset[2];
        MBPsysSetPSpeed(speed, psys);
        MBPsysSetPTex(psys, texture);
    }
}

/* Resolve one player-SFX record, execute its selected effect path, then
 * recurse through the record's linked successor.  Xbox PDB: DoPlyrSfxSub. */
s32 DoPlyrSfxSub(u8* player, s32 recordIndex, f32* offset,
                 s32 absolute, s32 effectIndex)
{
    plyr_sfx* record;
    u32 flags;
    s32 result = -1;
    f32 finalPosition[3];
    f32 localPosition[3];
    s32 playerIndex = ((Player*)player)->index;

    if (recordIndex < 0) {
        return -1;
    }

    record = (plyr_sfx*)(((plyr_data*)lbl_80282930[playerIndex])->sfx +
                                      recordIndex * sizeof(plyr_sfx));
    flags = record->flags;

    if ((flags & 0x400) != 0) {
        MBTreeSetFlags(((Player*)player)->node, 1, 0);
        MBTreeSetFlags(*(void**)(*(u8**)(player + 0x74) + 0x78), 2, 2);
    }

    if ((flags & 0x100) != 0) {
        f32 rate = record->radius;
        s32 loops = record->zmod;

        SetSkinFX(player + 0x7DC, record->sfxidx, (s32)record->maxlen,
                  loops, rate);
    } else if ((flags & 0x0F000000) != 0) {
        PsfxDoParticle(player, record, effectIndex);
    } else if ((flags & 0x200) == 0) {
        if (absolute == 0) {
            MulVecMat3(record->offset, localPosition,
                       ((Player*)player)->mat);
        } else {
            localPosition[0] = record->offset[0];
            localPosition[1] = record->offset[1];
            localPosition[2] = record->offset[2];
        }

        if ((flags & 1) != 0) {
            absolute = 1;
            finalPosition[0] = localPosition[0];
            finalPosition[1] = localPosition[1];
            finalPosition[2] = localPosition[2];
        } else if (offset != NULL) {
            finalPosition[0] = offset[0] + localPosition[0];
            finalPosition[1] = offset[1] + localPosition[1];
            finalPosition[2] = offset[2] + localPosition[2];
        } else {
            finalPosition[0] = localPosition[0];
            finalPosition[1] = localPosition[1];
            finalPosition[2] = localPosition[2];
        }
        result = DoPlyrSfx(player, record, finalPosition, absolute, flags,
                           effectIndex);
    }

    if (record->sndidx >= 0 && (flags & 0x0F000000) == 0) {
        AudioPlay3DSel(record->sndidx, 0xE0, ((Player*)player)->pos, 1);
    }
    if ((flags & 2) != 0) {
        ShakeCamera(0, 0, 90, lbl_80347E24, 100);
    }
    if ((flags & 0x20) != 0) {
        SafeRockSetup();
    }
    if ((s32)record->nextfxidx >= 0) {
        DoPlyrSfxSub(player, (s32)record->nextfxidx, offset, absolute, result);
    }
    return result;
}

s32 DoPlyrSfx(u8* player, plyr_sfx* record, f32* position,
              s32 absolute, u32 flags, s32 effectIndex)
{
    u32 effectFlags;
    u32 spawnFlags;
    s32 type;
    s32 effect;
    void* parent;
    u8* effectData;
    void* rootNode;

    type = record->sfxidx;
    if (type < 0) {
        goto invalid;
    }

    effectFlags = 0x800;
    spawnFlags = 0;
    if ((flags & 4) != 0) {
        effectFlags |= 0x80080;
    }
    if ((flags & 0x1000) != 0) {
        effectFlags |= 0x800000;
    }
    if ((flags & 0x4000) != 0) {
        effectFlags &= ~0x800;
        effectFlags |= 0x2000;
    }
    if ((flags & 0x10) != 0) {
        spawnFlags |= 0x200000;
    }
    if ((flags & 0x8000) != 0) {
        spawnFlags |= 0x10000;
    }
    if ((flags & 0x10000) != 0) {
        spawnFlags |= 0x400000;
    }

    effect = StartFXSub(type, position, spawnFlags, effectFlags, record->maxlen);
    if (effect < 0) {
        return effect;
    }

    if ((flags & 0x80) != 0) {
        SfxSetMat(effect, ((Player*)player)->mat,
                  ((MBObject*)((Player*)player)->node)->mat[3]);
    } else if ((flags & 0x40) != 0) {
        if ((flags & 0x2000) != 0 && lbl_80344B40 != NULL) {
            player = lbl_80344B40;
        }
        if (absolute != 0) {
            MulVecMat4(position, position, ((Player*)player)->mat);
        }
        SfxSetMat(effect, ((Player*)player)->mat, position);
    } else if (absolute != 0) {
        if ((flags & 0x2000) != 0 && lbl_80344B40 != NULL) {
            player = lbl_80344B40;
        }
        if ((flags & 0x40000) != 0 && effectIndex >= 0) {
            parent = Effects[effectIndex].node;
        } else if ((flags & 0x800) != 0) {
            parent = ((MBObject*)((Player*)player)->node)->parent;
        } else if ((flags & 1) != 0) {
            parent = ((Player*)player)->node;
        } else {
            parent = ((MBObject*)((Player*)player)->node)->child;
        }
        SfxSetParent(effect, parent);
    } else if ((flags & 0x40000) != 0 && effectIndex >= 0) {
        SfxSetParent(effect, Effects[effectIndex].node);
    }

    if ((flags & 0x20000) != 0) {
        SfxSetStreak(effect, WeaponStreakTex,
                    lbl_8011A178[((Player*)player)->class_id],
                    0x40, lbl_80347DAC, lbl_80347DB8);
    }

    effectData = (u8*)Effects;
    effectData += effect * sizeof(Effect);
    rootNode = **(void***)(effectData += 0x18);
    if (rootNode != NULL) {
        MBTreeSetAmbientAdd(rootNode, 0x1FF, 1);
    }
    if (record->color != 0xFFFFFFFF) {
        MBTreeSetColor(**(void***)effectData, record->color, 1);
    }
    goto done;

invalid:
    effect = -1;
done:
    return effect;
}

/* PlayerSfxClearData @0x8008A584 -- release the custom-effect handle of every
 * record whose flags lack the 0x0F000100 bits, marking each slot free. */
#pragma dont_inline on
void PlayerSfxClearData(u32* rec, s32 count)
{
    s32 i;
    i = 0;
    while (i < count) {
        if ((rec[0] & 0xF000100) == 0 && (s32)rec[2] >= 0) {
            ClearCustomEffect(rec[2]);
            rec[2] = -1;
        }
        i++;
        rec += 0x14;
    }
}
#pragma dont_inline off

/* PlayerSfxInitData @0x8008A5F4 -- clear every record's two handles to -1,
 * then run fn_8008A678 to resolve the effect/sound for each. */
void PlayerSfxInitData(s32* player, u32* records, s32 count, void* param4)
{
    s32 off;
    u32* rec;
    s32 i;
    s32 secondOff;
    s32 j;

    for (i = 0, off = 0; i < count; i++, off += 0x50) {
        rec = (u32*)((u8*)records + off);
        rec[2] = -1;
        rec[3] = -1;
    }
    j = 0;
    secondOff = 0;
    while (j < count) {
        fn_8008A678(player, (u32*)((u8*)records + secondOff), param4);
        j++;
        secondOff += 0x50;
    }
}

/* fn_8008A678 @0x8008A678 -- resolve one record's custom-effect handle (rec[2])
 * and audio/mbox handle (rec[3]) if not already set. */
void fn_8008A678(s32* player, u32* rec, void* p11)
{
    if ((s32)rec[2] == -1) {
        if ((rec[0] & 0xF000100) != 0) {
            s32 alt = ((s32*)player_multiple_models)[*player * 0x13 + 4];
            rec[2] = MBOX_FindTexture_Sub((char*)(rec + 4), 0, alt, alt, 0);
            alt = ((s32*)player_multiple_models)[*player * 0x13 + 13];
            if ((s32)rec[2] == 0) {
                rec[2] = MBOX_FindTexture_Sub((char*)(rec + 4), 0, alt, alt, 0);
            }
            if ((s32)rec[2] == 0) {
                rec[2] = MBOX_FindTexture_Sub((char*)(rec + 4), 0, -1, -1, 0);
            }
        } else {
            rec[2] = (u32)InitCustomEffect(p11, (char*)(rec + 4),
                                           ((plyr_sfx*)rec)->zmod,
                                           ((plyr_sfx*)rec)->alphamod);
        }
    }
    if ((s32)rec[3] == -1) {
        if ((rec[0] & 0xF000000) != 0) {
            if (*(char*)(rec + 8) != 0) {
                u32* node;
                void* obj;
                sprintf((char*)&lbl_802828B0, lbl_80347E3C,
                        (char*)(player + 0x1B0), (char*)(rec + 8));
                obj = MBOX_ReallyFindObject((char*)&lbl_802828B0, player[0x1FD],
                                            player[0x1FD], 1);
                node = (u32*)AtreeFindMbidxNode((s32*)player[0x1F], obj);
                if (node != 0) {
                    rec[3] = *node;
                } else {
                    rec[3] = 0xFFFFFFFF;
                }
            } else {
                rec[3] = 0xFFFFFFFF;
            }
        } else if (*(char*)(rec + 8) != 0) {
            rec[3] = AudioFindSound((char*)(rec + 8), 0, 1);
        } else {
            rec[3] = 0xFFFFFFFF;
        }
    }
}

/* ClearAllPlyrData @0x8008A82C -- clear every player's sfx records. */
void ClearAllPlyrData(void)
{
    s32 i;
    for (i = 0; i < 4; i++) {
        u8* hdr = lbl_80282930[i];
        if (hdr != 0) {
            PlayerSfxClearData((u32*)((plyr_data*)hdr)->sfx, ((plyr_data*)hdr)->numsfx);
            ((plyr_data*)lbl_80282930[i])->initflag = 0;
        }
    }
}

/* ClearPlyrData @0x8008A898 -- clear one player's sfx records (loop inlined). */
static inline void ClearPlyrRecords(u32* rec, s32 count)
{
    s32 i;

    for (i = 0; i < count; i++, rec += 0x14) {
        if ((rec[0] & 0xF000100) == 0 && (s32)rec[2] >= 0) {
            ClearCustomEffect(rec[2]);
            rec[2] = -1;
        }
    }
}

void ClearPlyrData(s32 player)
{
    u8* hdr = lbl_80282930[player];

    ClearPlyrRecords((u32*)((plyr_data*)hdr)->sfx, ((plyr_data*)hdr)->numsfx);
    ((plyr_data*)lbl_80282930[player])->initflag = 0;
}

/* --- LoadPlyrData support ------------------------------------------------ */

/* pdata wad payload is little-endian (PS2/Xbox heritage); MBSetupWad reports
 * whether the archive needs byte-swapping and these fix each record up. */
#define SWAP16(v)                                                       \
    do {                                                                \
        u16 _t = (v);                                                   \
        (v) = (u16)((((u8*)&_t)[1] << 8) | ((u8*)&_t)[0]);              \
    } while (0)

#define SWAP32(v)                                                       \
    do {                                                                \
        u8 _t[8];                                                       \
        *(u32*)_t = (v);                                                \
        _t[4] = _t[3];                                                  \
        _t[5] = _t[2];                                                  \
        _t[6] = _t[1];                                                  \
        _t[7] = _t[0];                                                  \
        (v) = *(u32*)(_t + 4);                                          \
    } while (0)

#define SWAPF(v)                                                        \
    do {                                                                \
        f32 _in = (v);                                                  \
        f32 _out;                                                       \
        u32 _w = *(u32*)&_in;                                           \
        SWAP32(_w);                                                     \
        *(u32*)&_out = _w;                                              \
        (v) = _out;                                                     \
    } while (0)

/* 4-char wad chunk tags kept as strings in sdata2 (chars are signed) */
#define WADTAG(s) (((s)[0] << 24) | ((s)[1] << 16) | ((s)[2] << 8) | (s)[3])

extern char lbl_80347E54[8]; /* header-chunk wad tag  */
extern char lbl_80347E5C[8]; /* record-chunk wad tag  */
extern char lbl_80347E64[8]; /* move-chunk wad tag    */
extern u32 gControllerButtons;
extern u32 sFlags;
extern s32 fn_80055F68(s32 a, s32 b);
extern u8 MBSetupWad(void* wad, void* data);
extern void* MBGetFromWad(void* wad, s32 tag, s32* count);
extern void* memcpy(void* dst, const void* src, u32 n);
extern Player gPlayers[4]; /* gPlayerRecords[4], stride 0x335C */
extern void PlayerSfxInitData(s32* player, u32* records, s32 count, void* param4);

/* LoadPlyrData @0x8008A928 -- ensure player plr has class cls's pdata wad
 * loaded and parsed: reload the per-class file if needed (debug flag 0x10
 * forces a disk re-read), copy it into the player's load buffer, pull the
 * three chunks out of the wad, byte-swap every record when the archive is
 * foreign-endian, then resolve the effect/sound handles. */
#pragma dont_inline on
void LoadPlyrData(s32 plr, s32 cls, s32 resolve) {
    char* errorStrings = lbl_80114288;
    PsfxPdataBuf* pdata = &lbl_802828B0;
    PsfxFileTable* pdataFiles = (PsfxFileTable*)lbl_80120DA0;
    s32 wad[4];
    s32 n1;
    s32 n2;
    s32 n3;
    plyr_data* hdr;
    u8* p;
    s32 mode = 0;
    s32 i;
    s32 j;
    s32 k;
    s32 off;
    u8 swapped;

    if (plr < 0) {
        return;
    }
    if (cls < 0) {
        return;
    }
    if (cls != pdata->cur[plr] || (gPlayers[plr].state != 0 && resolve != 0)) {
        if ((*(u64*)&gControllerButtons & 0x10) != 0 && fn_80055F68(0, -1) != 0) {
            mode = 2;
        } else {
            mode = 1;
        }
    }

    if (mode != 0) {
        if (mode == 2) {
            /* forced re-read of the class file from disk */
            sprintf(pdata->name, lbl_80347E44, (char*)lbl_8012006C + cls * 4);
            if (FileExists(lbl_80347E4C, pdata->name)) {
                if (pdataFiles->files[cls] == 0) {
                    pdataFiles->files[cls] = AllocFile(lbl_80347E4C, pdata->name);
                    pdata->sizes[cls] = mlmLastFileSize;
                } else if (!MLMReadFile(lbl_80347E4C, pdata->name, pdata->sizes[cls], pdataFiles->files[cls])) {
                    FatalErrorf(errorStrings + 24, pdata->name);
                }
            } else {
                ErrorPrintf(errorStrings + 76, pdata->name);
                pdataFiles->files[cls] = 0;
                pdata->sizes[cls] = 0;
            }
        }

        if (pdataFiles->files[cls] != 0) {
            memcpy(pdata->bufs[plr], pdataFiles->files[cls], pdata->sizes[cls]);
            swapped = MBSetupWad(wad, pdata->bufs[plr]);

            pdata->headers[plr] = MBGetFromWad(wad, WADTAG(lbl_80347E54), &n1);
            ((plyr_data*)pdata->headers[plr])->sfx = MBGetFromWad(wad, WADTAG(lbl_80347E5C), &n2);
            ((plyr_data*)pdata->headers[plr])->damage = MBGetFromWad(wad, WADTAG(lbl_80347E64), &n3);
            ((plyr_data*)pdata->headers[plr])->initflag = 0;
            pdata->cur[plr] = cls;

            if (swapped) {
                /* header rows: 0x180 bytes each */
                i = 0;
                off = 0;
                for (; i < n1; i++, off += 0x180) {
                    p = (u8*)pdata->headers[plr] + off;
                    SWAP16(*(u16*)(p + offsetof(plyr_data, numsfx)));
                    SWAP16(*(u16*)(p + offsetof(plyr_data, numdamage)));
                    SWAP16(*(u16*)(p + offsetof(plyr_data, turboAclose)));
                    SWAP16(*(u16*)(p + offsetof(plyr_data, turboAlow)));
                    SWAP16(*(u16*)(p + offsetof(plyr_data, turboAstep)));
                    SWAP16(*(u16*)(p + offsetof(plyr_data, turboA360)));
                    SWAP16(*(u16*)(p + offsetof(plyr_data, turboAthrow)));
                    SWAP16(*(u16*)(p + offsetof(plyr_data, turboB)));
                    SWAP16(*(u16*)(p + offsetof(plyr_data, turboC1)));
                    SWAP16(*(u16*)(p + offsetof(plyr_data, turboC2)));
                    SWAP16(*(u16*)(p + offsetof(plyr_data, combo1)));
                    SWAP16(*(u16*)(p + offsetof(plyr_data, combo2)));
                    SWAP16(*(u16*)(p + offsetof(plyr_data, combohit)));
                    SWAP16(*(u16*)(p + offsetof(plyr_data, victory)));
                    SWAP32(*(u32*)(p + offsetof(plyr_data, initflag)));
                    SWAPF(*(f32*)(p + offsetof(plyr_data, fight_min)));
                    SWAPF(*(f32*)(p + offsetof(plyr_data, fight_max)));
                    SWAPF(*(f32*)(p + offsetof(plyr_data, speed_min)));
                    SWAPF(*(f32*)(p + offsetof(plyr_data, speed_max)));
                    SWAPF(*(f32*)(p + offsetof(plyr_data, armor_min)));
                    SWAPF(*(f32*)(p + offsetof(plyr_data, armor_max)));
                    SWAPF(*(f32*)(p + offsetof(plyr_data, magic_min)));
                    SWAPF(*(f32*)(p + offsetof(plyr_data, magic_max)));
                    SWAPF(*(f32*)(p + offsetof(plyr_data, height)));
                    k = 0;
                    SWAPF(*(f32*)(p + offsetof(plyr_data, width)));
                    SWAPF(*(f32*)(p + offsetof(plyr_data, attny)));
                    SWAPF(*(f32*)(p + offsetof(plyr_data, coly)));
                    SWAPF(*(f32*)(p + offsetof(plyr_data, powerup_time)));
                    SWAPF(*(f32*)(p + offsetof(plyr_data, streakfwdmul)));
                    for (; k < 3; k++) {
                        SWAPF(*(f32*)(p + offsetof(plyr_data, weapon_offset) + k * 4));
                        SWAPF(*(f32*)(p + offsetof(plyr_data, turboa_offset) + k * 4));
                        SWAPF(*(f32*)(p + offsetof(plyr_data, familiar_offset) + k * 4));
                        SWAPF(*(f32*)(p + offsetof(plyr_data, fam_proj_offset) + k * 4));
                    }
                    for (j = 0; j < 10; j++) {
                        for (k = 0; k < 3; k++) {
                            SWAPF(*(f32*)(p + offsetof(plyr_data, weapon_fx_offset) + j * 0xC + k * 4));
                            SWAPF(*(f32*)(p + offsetof(plyr_data, weapon_fx_scale) + j * 0xC + k * 4));
                        }
                    }
                }

                /* sfx records: 0x50 bytes each */
                {
                    s32 recordIndex = 0;
                    s32 recordOffset = 0;
                    for (; recordIndex < n2; recordIndex++, recordOffset += 0x50) {
                        p = ((plyr_data*)pdata->headers[plr])->sfx + recordOffset;
                        SWAP16(*(u16*)(p + offsetof(plyr_sfx, zmod)));
                        SWAP16(*(u16*)(p + offsetof(plyr_sfx, alphamod)));
                        SWAP32(*(u32*)(p + offsetof(plyr_sfx, flags)));
                        SWAP32(*(u32*)(p + offsetof(plyr_sfx, nextfxidx)));
                        SWAP32(*(u32*)(p + offsetof(plyr_sfx, sfxidx)));
                        SWAP32(*(u32*)(p + offsetof(plyr_sfx, sndidx)));
                        SWAPF(*(f32*)(p + offsetof(plyr_sfx, maxlen)));
                        SWAPF(*(f32*)(p + offsetof(plyr_sfx, radius)));
                        SWAPF(*(f32*)(p + offsetof(plyr_sfx, scale)));
                        SWAP32(*(u32*)(p + offsetof(plyr_sfx, color)));
                        for (k = 0; k < 3; k++) {
                            SWAPF(*(f32*)(p + offsetof(plyr_sfx, offset) + k * 4));
                        }
                    }
                }

                /* move rows: 0x58 bytes each */
                {
                    s32 moveIndex = 0;
                    s32 moveOffset = 0;
                    for (; moveIndex < n3; moveIndex++, moveOffset += 0x58) {
                        p = ((plyr_data*)pdata->headers[plr])->damage + moveOffset;
                        SWAP16(*(u16*)(p + offsetof(plyr_damage, type)));
                        SWAP16(*(u16*)(p + offsetof(plyr_damage, flags)));
                        SWAP16(*(u16*)(p + offsetof(plyr_damage, fxidx)));
                        SWAP16(*(u16*)(p + offsetof(plyr_damage, hitfxidx)));
                        SWAP16(*(u16*)(p + offsetof(plyr_damage, loopfxidx)));
                        SWAP16(*(u16*)(p + offsetof(plyr_damage, next)));
                        SWAP16(*(u16*)(p + offsetof(plyr_damage, startframe)));
                        SWAP16(*(u16*)(p + offsetof(plyr_damage, endframe)));
                        SWAP16(*(u16*)(p + offsetof(plyr_damage, helpidx)));
                        SWAPF(*(f32*)(p + offsetof(plyr_damage, hitrad)));
                        SWAPF(*(f32*)(p + offsetof(plyr_damage, radius)));
                        SWAPF(*(f32*)(p + offsetof(plyr_damage, minrad)));
                        SWAPF(*(f32*)(p + offsetof(plyr_damage, delay)));
                        SWAPF(*(f32*)(p + offsetof(plyr_damage, mintime)));
                        SWAPF(*(f32*)(p + offsetof(plyr_damage, maxtime)));
                        SWAPF(*(f32*)(p + offsetof(plyr_damage, angle)));
                        SWAPF(*(f32*)(p + offsetof(plyr_damage, arc)));
                        SWAPF(*(f32*)(p + offsetof(plyr_damage, pitch)));
                        SWAPF(*(f32*)(p + offsetof(plyr_damage, amount)));
                        SWAPF(*(f32*)(p + offsetof(plyr_damage, speed_min)));
                        SWAPF(*(f32*)(p + offsetof(plyr_damage, speed_max)));
                        SWAPF(*(f32*)(p + offsetof(plyr_damage, weight)));
                        SWAP32(*(u32*)(p + offsetof(plyr_damage, dmgtype)));
                        for (k = 0; k < 3; k++) {
                            SWAPF(*(f32*)(p + offsetof(plyr_damage, offset) + k * 4));
                        }
                    }
                }
                { volatile u8 unused[600]; }
            }

            if (resolve != 0) {
                hdr = (plyr_data*)pdata->headers[plr];
                PlayerSfxInitData((s32*)&gPlayers[plr], (u32*)hdr->sfx, hdr->numsfx,
                                  ((void**)player_multiple_models)[plr * 0x13 + 18]);
                ((plyr_data*)pdata->headers[plr])->initflag = 1;
            }
        } else {
            FatalErrorf(errorStrings + 76, pdata->name);
        }
    } else if (resolve != 0) {
        hdr = (plyr_data*)pdata->headers[plr];
        if (hdr->initflag == 0) {
            PlayerSfxInitData((s32*)&gPlayers[plr], (u32*)hdr->sfx, hdr->numsfx,
                              ((void**)player_multiple_models)[plr * 0x13 + 18]);
            ((plyr_data*)pdata->headers[plr])->initflag = 1;
        }
    }
}
#pragma dont_inline off
/* LoadPdataFile @0x8008BAF0 -- preflight all 16 class pdata files, retain
 * their largest size, then allocate four reusable player load buffers. */
void LoadPdataFile(void)
{
    u8* temp = (u8*)&lbl_802828B0;
    s32 maxSize;
    s32 index;
    s32 zero2;
    u8* record;

    maxSize = 0;
    index = 0;
    do {
        void** slot = &lbl_80120E00[index];
        u32 zero = 0;
        *slot = NULL;
        sprintf(temp, lbl_80347E44, lbl_8012006C + index * 4);
        if (FileExists(lbl_80347E4C, temp)) {
            if (*slot == NULL) {
                *slot = AllocFile(lbl_80347E4C, temp);
                lbl_802828B0.sizes[index] = mlmLastFileSize;
            } else if (!MLMReadFile(lbl_80347E4C, temp,
                                    lbl_802828B0.sizes[index],
                                    *slot)) {
                FatalErrorf(lbl_801142A0, temp);
            }
        } else {
            ErrorPrintf(lbl_801142D4, temp);
            *slot = NULL;
            lbl_802828B0.sizes[index] = zero;
        }
        if (lbl_802828B0.sizes[index] > maxSize) {
            maxSize = lbl_802828B0.sizes[index];
        }
        index++;
    } while (index < 16);

    index = 0;
    zero2 = index;
    do {
        record = temp + index * 4;
        *(s32*)(record + 0x20) = -1;
        *(void**)(record + 0x30) = AllocMem(maxSize);
        *(s32*)(record + 0x80) = zero2;
        index++;
    } while (index < 4);
}

#undef STUB
