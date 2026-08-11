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
 * Player-data model (lbl_80282930[4], one header per player):
 *   header: s16 count @0, SfxRecord* records @4, s32 slot @0x24.
 *   SfxRecord: 0x50 bytes; u32 flags @0, s32 handle @8 (word[2]).
 */

#include "types.h"
#include "game/effect.h"

extern u8* lbl_80282930[4];
extern void ClearCustomEffect(s32 index);
extern void* player_multiple_models[];
typedef struct PsfxPdataBuf {
    char  name[0x20];  /* 0x00 : sprintf'd "<class>.wad" filename */
    s32   cur[4];      /* 0x20 : per-player currently-loaded class */
    void* bufs[4];     /* 0x30 : per-player load buffers */
    s32   sizes[16];   /* 0x40 : per-class pdata file sizes */
    s32   flags[4];    /* 0x80 */
} PsfxPdataBuf;
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
extern u8 gIdentityMatrix[];
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
extern void fn_80093D98(s32 effect, s32 texture, u32 color, s32 alpha,
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
    s32 maximumAlpha = 0;
    s32 maximumIndex = 0;
    s32 minimumAlpha = 255;
    s32 minimumIndex = 0;
    s32 lastIndex = player->lastTrail;
    s32 index;
    s32 offset;
    s32 alpha;
    s32 chosenIndex;
    void** slot;
    void* node;
    s32 object;
    f32 dx;
    f32 dy;
    f32 dz;

    if (player->activeNode == NULL) {
        goto done;
    }
    if (gFrameTicks == 0) {
        asm { b done }
    }

ticks_active:
    if (lastIndex >= 0 || player->trailActive != 0) {
                index = 0;
                offset = 0;
                while (index <= lastIndex) {
                    slot = (void**)((u8*)player + offset + 0x808);
                    node = *slot;
                    if (node != NULL) {
                        if ((*(u32*)((u8*)node + 0x60) & 0x200) != 0) {
                            alpha = 255 - *(u8*)((u8*)node + 0x53);
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
                       *(void**)((u8*)player + offset + 0x808) == NULL) {
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
                            dx = *(f32*)((u8*)node + 0x30) - matrix[12];
                            dy = *(f32*)((u8*)node + 0x34) - matrix[13];
                            dz = *(f32*)((u8*)node + 0x38) - matrix[14];
                            if ((f64)(dx * dx + dy * dy + dz * dz) < 0.01) {
                                return;
                            }
                        }
                    }

                    node = player->worldNode;
                    slot = (void**)((u8*)player + chosenIndex * 4);
                    object = (*(u32*)((u8*)node + 0x6C) & 0xFFFF) |
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
STUB(0x80089350, fn_80089350)
STUB(0x800898DC, fn_800898DC)

typedef struct PlayerSfxRecord {
    u32 flags;          /* 0x00 */
    u32 _04;
    s32 texture;        /* 0x08 */
    s32 parent;         /* 0x0C: parent selector / positional sound */
    u8 _10[0x20];
    s16 skinLoops;      /* 0x30 */
    s16 speed;          /* 0x32 */
    f32 position[3];    /* 0x34 */
    f32 duration;       /* 0x40 */
    f32 rate;           /* 0x44 */
    f32 parameterScale; /* 0x48 */
    u32 color;          /* 0x4C */
} PlayerSfxRecord;

s32 DoPlyrSfxSub(u8* player, s32 recordIndex, f32* offset,
                 s32 absolute, s32 effectIndex);
s32 DoPlyrSfx(u8* player, PlayerSfxRecord* record, f32* position,
              s32 absolute, u32 flags, s32 effectIndex);

/* Build and configure the particle node for one player-SFX record.
 * Xbox PDB: PSFX.OBJ local PsfxDoParticle. */
void PsfxDoParticle(u8* player, PlayerSfxRecord* record, s32 effectIndex)
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
    rate = (f32)(lbl_80347E28 * (f64)record->rate);
    texture = record->texture;
    parentHandle = record->parent;
    duration = record->duration;
    parameterScale = record->parameterScale;
    speed = (f32)(lbl_80347D98 * (f64)record->speed);

    if ((flags & 0x40000) != 0 && effectIndex >= 0) {
        parent = Effects[effectIndex].node;
    } else if ((flags & 0x2000) != 0 && lbl_80344B40 != NULL) {
        parent = *(void**)(lbl_80344B40 + 0x74);
    } else if (parentHandle == -1) {
        parent = *(void**)(player + 0x74);
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
        *(f32*)((u8*)psys + 0x30) = record->position[0];
        *(f32*)((u8*)psys + 0x34) = record->position[1];
        *(f32*)((u8*)psys + 0x38) = record->position[2];
        MBPsysSetPSpeed(speed, psys);
        MBPsysSetPTex(psys, texture);
    }
}

/* Resolve one player-SFX record, execute its selected effect path, then
 * recurse through the record's linked successor.  Xbox PDB: DoPlyrSfxSub. */
s32 DoPlyrSfxSub(u8* player, s32 recordIndex, f32* offset,
                 s32 absolute, s32 effectIndex)
{
    PlayerSfxRecord* record;
    u32 flags;
    s32 result = -1;
    f32 finalPosition[3];
    f32 localPosition[3];
    s32 playerIndex = *(s32*)player;

    if (recordIndex < 0) {
        return -1;
    }

    record = (PlayerSfxRecord*)(*(u8**)(lbl_80282930[playerIndex] + 4) +
                                      recordIndex * sizeof(PlayerSfxRecord));
    flags = record->flags;

    if ((flags & 0x400) != 0) {
        MBTreeSetFlags(*(void**)(player + 0x74), 1, 0);
        MBTreeSetFlags(*(void**)(*(u8**)(player + 0x74) + 0x78), 2, 2);
    }

    if ((flags & 0x100) != 0) {
        SetSkinFX(player + 0x7DC, record->texture, (s32)record->duration,
                  record->skinLoops, record->rate);
    } else if ((flags & 0x0F000000) != 0) {
        PsfxDoParticle(player, record, effectIndex);
    } else if ((flags & 0x200) == 0) {
        if (absolute == 0) {
            MulVecMat3(record->position, localPosition,
                       (f32*)(player + 0x14));
        } else {
            localPosition[0] = record->position[0];
            localPosition[1] = record->position[1];
            localPosition[2] = record->position[2];
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

    if (record->parent >= 0 && (flags & 0x0F000000) == 0) {
        AudioPlay3DSel(record->parent, 0xE0, (f32*)(player + 0x44), 1);
    }
    if ((flags & 2) != 0) {
        ShakeCamera(0, 0, 90, lbl_80347E24, 100);
    }
    if ((flags & 0x20) != 0) {
        SafeRockSetup();
    }
    if ((s32)record->_04 >= 0) {
        DoPlyrSfxSub(player, (s32)record->_04, offset, absolute, result);
    }
    return result;
}

s32 DoPlyrSfx(u8* player, PlayerSfxRecord* record, f32* position,
              s32 absolute, u32 flags, s32 effectIndex)
{
    u32 effectFlags;
    u32 spawnFlags;
    s32 type;
    s32 effect;
    void* parent;
    u8* effectData;
    void* rootNode;

    type = record->texture;
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

    effect = StartFXSub(type, position, spawnFlags, effectFlags, record->duration);
    if (effect < 0) {
        return effect;
    }

    if ((flags & 0x80) != 0) {
        SfxSetMat(effect, (f32*)(player + 0x14),
                  (f32*)(*(u8**)(player + 0x74) + 0x30));
    } else if ((flags & 0x40) != 0) {
        if ((flags & 0x2000) != 0 && lbl_80344B40 != NULL) {
            player = lbl_80344B40;
        }
        if (absolute != 0) {
            MulVecMat4(position, position, (f32*)(player + 0x14));
        }
        SfxSetMat(effect, (f32*)(player + 0x14), position);
    } else if (absolute != 0) {
        if ((flags & 0x2000) != 0 && lbl_80344B40 != NULL) {
            player = lbl_80344B40;
        }
        if ((flags & 0x40000) != 0 && effectIndex >= 0) {
            parent = Effects[effectIndex].node;
        } else if ((flags & 0x800) != 0) {
            parent = *(void**)(*(u8**)(player + 0x74) + 0x74);
        } else if ((flags & 1) != 0) {
            parent = *(void**)(player + 0x74);
        } else {
            parent = *(void**)(*(u8**)(player + 0x74) + 0x78);
        }
        SfxSetParent(effect, parent);
    } else if ((flags & 0x40000) != 0 && effectIndex >= 0) {
        SfxSetParent(effect, Effects[effectIndex].node);
    }

    if ((flags & 0x20000) != 0) {
        fn_80093D98(effect, WeaponStreakTex, lbl_8011A178[*(s32*)(player + 4)],
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
                                           *(s16*)((u8*)rec + 0x30),
                                           *(s16*)((u8*)rec + 0x32));
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
            PlayerSfxClearData(*(u32**)(hdr + 4), *(s16*)hdr);
            *(s32*)(lbl_80282930[i] + 0x24) = 0;
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

    ClearPlyrRecords(*(u32**)(hdr + 4), *(s16*)hdr);
    *(s32*)(lbl_80282930[player] + 0x24) = 0;
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

/* player-data header (first chunk of the pdata wad) */
typedef struct PsfxHeader {
    /* 0x00 */ s16 count;    /* number of SfxRecords            */
    /* 0x02 */ s16 _02;
    /* 0x04 */ u8* records;  /* SfxRecord[count], 0x50 each     */
    /* 0x08 */ u8* moves;    /* third chunk rows, 0x58 each     */
    /* 0x0C */ u8 _0c[0x18];
    /* 0x24 */ s32 resolved; /* handles resolved this level     */
} PsfxHeader;

extern char lbl_80347E54[]; /* header-chunk wad tag  */
extern char lbl_80347E5C[]; /* record-chunk wad tag  */
extern char lbl_80347E64[]; /* move-chunk wad tag    */
extern u32 gControllerButtons;
extern u32 sFlags;
extern s32 fn_80055F68(s32 a, s32 b);
extern u8 MBSetupWad(void* wad, void* data);
extern void* MBGetFromWad(void* wad, s32 tag, s32* count);
extern void* memcpy(void* dst, const void* src, u32 n);
extern u8 gPlayers[];
extern void PlayerSfxInitData(s32* player, u32* records, s32 count, void* param4);

/* LoadPlyrData @0x8008A928 -- ensure player plr has class cls's pdata wad
 * loaded and parsed: reload the per-class file if needed (debug flag 0x10
 * forces a disk re-read), copy it into the player's load buffer, pull the
 * three chunks out of the wad, byte-swap every record when the archive is
 * foreign-endian, then resolve the effect/sound handles. */
void LoadPlyrData(s32 plr, s32 cls, s32 resolve)
{
    s32 wad[4];
    s32 n1;
    s32 n2;
    s32 n3;
    PsfxHeader* hdr;
    u8* p;
    s32 mode;
    s32 i;
    s32 j;
    s32 k;
    s32 off;
    u8 swapped;

    mode = 0;
    if (plr < 0) {
        return;
    }
    if (cls < 0) {
        return;
    }

    if (cls != *(s32*)((u8*)&lbl_802828B0 + 0x20 + plr * 4) ||
        (*(s32*)(gPlayers + plr * 0x335C + 0xE8) != 0 && resolve != 0)) {
        if ((sFlags & 0x10) == 0 || fn_80055F68(0, -1) == 0) {
            mode = 1;
        } else {
            mode = 2;
        }
    }

    if (mode == 0) {
        /* already loaded: only (re)resolve the handles */
        if (resolve != 0) {
            hdr = (PsfxHeader*)lbl_80282930[plr];
            if (hdr->resolved == 0) {
                PlayerSfxInitData((s32*)(gPlayers + plr * 0x335C),
                                  (u32*)hdr->records, hdr->count,
                                  ((void**)player_multiple_models)[plr * 0x13 + 18]);
                hdr->resolved = 1;
            }
        }
        return;
    }

    if (mode == 2) {
        /* forced re-read of the class file from disk */
        sprintf((char*)&lbl_802828B0, "%s.wad", (char*)lbl_8012006C + cls * 4);
        if (FileExists("pdata", (char*)&lbl_802828B0)) {
            if (lbl_80120E00[cls] == 0) {
                lbl_80120E00[cls] = AllocFile("pdata", (char*)&lbl_802828B0);
                *(s32*)((u8*)&lbl_802828B0 + 0x40 + cls * 4) = mlmLastFileSize;
            } else if (!MLMReadFile("pdata", (char*)&lbl_802828B0,
                                    *(s32*)((u8*)&lbl_802828B0 + 0x40 + cls * 4),
                                    lbl_80120E00[cls])) {
                FatalErrorf("pdata file %s: file on disk go too large for buffer",
                            (char*)&lbl_802828B0);
            }
        } else {
            ErrorPrintf("No player data file: %s", (char*)&lbl_802828B0);
            lbl_80120E00[cls] = 0;
            *(s32*)((u8*)&lbl_802828B0 + 0x40 + cls * 4) = 0;
        }
    }

    if (lbl_80120E00[cls] == 0) {
        FatalErrorf("No player data file: %s", (char*)&lbl_802828B0);
        return;
    }

    memcpy(*(void**)((u8*)&lbl_802828B0 + 0x30 + plr * 4), lbl_80120E00[cls],
           *(s32*)((u8*)&lbl_802828B0 + 0x40 + cls * 4));
    swapped = MBSetupWad(wad, *(void**)((u8*)&lbl_802828B0 + 0x30 + plr * 4));

    hdr = (PsfxHeader*)MBGetFromWad(wad, WADTAG(lbl_80347E54), &n1);
    lbl_80282930[plr] = (u8*)hdr;
    hdr = (PsfxHeader*)lbl_80282930[plr];
    hdr->records = MBGetFromWad(wad, WADTAG(lbl_80347E5C), &n2);
    hdr = (PsfxHeader*)lbl_80282930[plr];
    hdr->moves = MBGetFromWad(wad, WADTAG(lbl_80347E64), &n3);
    hdr = (PsfxHeader*)lbl_80282930[plr];
    hdr->resolved = 0;
    *(s32*)((u8*)&lbl_802828B0 + 0x20 + plr * 4) = cls;

    if (swapped) {
        /* header rows: 0x180 bytes each */
        off = 0;
        for (i = 0; i < n1; i++, off += 0x180) {
            p = (u8*)lbl_80282930[plr] + off;
            SWAP16(*(u16*)(p + 0x00));
            SWAP16(*(u16*)(p + 0x02));
            SWAP16(*(u16*)(p + 0x0C));
            SWAP16(*(u16*)(p + 0x0E));
            SWAP16(*(u16*)(p + 0x10));
            SWAP16(*(u16*)(p + 0x12));
            SWAP16(*(u16*)(p + 0x14));
            SWAP16(*(u16*)(p + 0x16));
            SWAP16(*(u16*)(p + 0x18));
            SWAP16(*(u16*)(p + 0x1A));
            SWAP16(*(u16*)(p + 0x1C));
            SWAP16(*(u16*)(p + 0x1E));
            SWAP16(*(u16*)(p + 0x20));
            SWAP16(*(u16*)(p + 0x22));
            SWAP32(*(u32*)(p + 0x24));
            SWAPF(*(f32*)(p + 0x28));
            SWAPF(*(f32*)(p + 0x2C));
            SWAPF(*(f32*)(p + 0x30));
            SWAPF(*(f32*)(p + 0x34));
            SWAPF(*(f32*)(p + 0x38));
            SWAPF(*(f32*)(p + 0x3C));
            SWAPF(*(f32*)(p + 0x40));
            SWAPF(*(f32*)(p + 0x44));
            SWAPF(*(f32*)(p + 0x48));
            SWAPF(*(f32*)(p + 0x4C));
            SWAPF(*(f32*)(p + 0x50));
            SWAPF(*(f32*)(p + 0x54));
            SWAPF(*(f32*)(p + 0x58));
            SWAPF(*(f32*)(p + 0x17C));
            for (k = 0; k < 3; k++) {
                SWAPF(*(f32*)(p + 0x5C + k * 4));
                SWAPF(*(f32*)(p + 0x158 + k * 4));
                SWAPF(*(f32*)(p + 0x164 + k * 4));
                SWAPF(*(f32*)(p + 0x170 + k * 4));
            }
            for (j = 0; j < 10; j++) {
                for (k = 0; k < 3; k++) {
                    SWAPF(*(f32*)(p + 0x68 + j * 0xC + k * 4));
                    SWAPF(*(f32*)(p + 0xE0 + j * 0xC + k * 4));
                }
            }
        }

        /* sfx records: 0x50 bytes each */
        off = 0;
        for (i = 0; i < n2; i++, off += 0x50) {
            p = ((PsfxHeader*)lbl_80282930[plr])->records + off;
            SWAP16(*(u16*)(p + 0x30));
            SWAP16(*(u16*)(p + 0x32));
            SWAP32(*(u32*)(p + 0x00));
            SWAP32(*(u32*)(p + 0x04));
            SWAP32(*(u32*)(p + 0x08));
            SWAP32(*(u32*)(p + 0x0C));
            SWAPF(*(f32*)(p + 0x40));
            SWAPF(*(f32*)(p + 0x44));
            SWAPF(*(f32*)(p + 0x48));
            SWAP32(*(u32*)(p + 0x4C));
            for (k = 0; k < 3; k++) {
                SWAPF(*(f32*)(p + 0x34 + k * 4));
            }
        }

        /* move rows: 0x58 bytes each */
        off = 0;
        for (i = 0; i < n3; i++, off += 0x58) {
            p = ((PsfxHeader*)lbl_80282930[plr])->moves + off;
            SWAP16(*(u16*)(p + 0x00));
            SWAP16(*(u16*)(p + 0x02));
            SWAP16(*(u16*)(p + 0x48));
            SWAP16(*(u16*)(p + 0x4A));
            SWAP16(*(u16*)(p + 0x4C));
            SWAP16(*(u16*)(p + 0x4E));
            SWAP16(*(u16*)(p + 0x50));
            SWAP16(*(u16*)(p + 0x52));
            SWAP16(*(u16*)(p + 0x54));
            SWAPF(*(f32*)(p + 0x08));
            SWAPF(*(f32*)(p + 0x0C));
            SWAPF(*(f32*)(p + 0x10));
            SWAPF(*(f32*)(p + 0x14));
            SWAPF(*(f32*)(p + 0x18));
            SWAPF(*(f32*)(p + 0x1C));
            SWAPF(*(f32*)(p + 0x20));
            SWAPF(*(f32*)(p + 0x24));
            SWAPF(*(f32*)(p + 0x28));
            SWAPF(*(f32*)(p + 0x38));
            SWAPF(*(f32*)(p + 0x3C));
            SWAPF(*(f32*)(p + 0x40));
            SWAPF(*(f32*)(p + 0x44));
            SWAP32(*(u32*)(p + 0x04));
            for (k = 0; k < 3; k++) {
                SWAPF(*(f32*)(p + 0x2C + k * 4));
            }
        }
    }

    if (resolve != 0) {
        hdr = (PsfxHeader*)lbl_80282930[plr];
        PlayerSfxInitData((s32*)(gPlayers + plr * 0x335C), (u32*)hdr->records,
                          hdr->count,
                          ((void**)player_multiple_models)[plr * 0x13 + 18]);
        hdr->resolved = 1;
    }
}
/* LoadPdataFile @0x8008BAF0 -- preflight all 16 class pdata files, retain
 * their largest size, then allocate four reusable player load buffers. */
void LoadPdataFile(void)
{
    u8 unused[8];
    u8* temp = (u8*)&lbl_802828B0;
    u8* sizePtr;
    s32 maxSize;
    s32 offset;
    s32 index;
    s32 zero2;

    maxSize = 0;
    index = 0;
    offset = 0;
    do {
        void** slot = (void**)((u8*)lbl_80120E00 + offset);
        void* zed = NULL;
        *slot = zed;
        sprintf(temp, lbl_80347E44, lbl_8012006C + offset);
        if (FileExists(lbl_80347E4C, temp)) {
            if (*slot == NULL) {
                *slot = AllocFile(lbl_80347E4C, temp);
                sizePtr = temp + offset;
                *(u32*)(sizePtr + 0x40) = mlmLastFileSize;
            } else if (!MLMReadFile(lbl_80347E4C, temp,
                                    *(u32*)((sizePtr = temp + offset) + 0x40),
                                    *slot)) {
                FatalErrorf(lbl_801142A0, temp);
            }
        } else {
            ErrorPrintf(lbl_801142D4, temp);
            *slot = zed;
            sizePtr = temp + offset;
            *(u32*)(sizePtr + 0x40) = (u32)zed;
        }
        sizePtr = temp + offset;
        if (*(s32*)(sizePtr + 0x40) > maxSize) {
            maxSize = *(s32*)(sizePtr + 0x40);
        }
        index++;
        offset += 4;
    } while (index < 16);

    index = 0;
    zero2 = index;
    offset = 0;
    do {
        sizePtr = temp + offset;
        *(s32*)(sizePtr + 0x20) = -1;
        *(void**)(sizePtr + 0x30) = AllocMem(maxSize);
        *(s32*)(sizePtr + 0x80) = zero2;
        index++;
        offset += 4;
    } while (index < 4);
}

#undef STUB
