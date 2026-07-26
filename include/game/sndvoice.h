#ifndef _GAME_SNDVOICE_H_
#define _GAME_SNDVOICE_H_

#include "types.h"

/*
 * SndVoice - GameCube platform voice-mixer record (Midway), layered over AX.
 *
 * There is no Xbox counterpart module: the Xbox build uses DirectSound, so the
 * authoritative research/xbox_symbols/audio.h has no SNDVOICE/AudioVoice type.
 * This layout is reconstructed from src/game/g3d/sndvoice.c and verified
 * offset-exact against the GameCube target asm (fnasm on sndvoiceInit /
 * SetParams / SetVolume).
 *
 * Backing store: static SndVoice sndVoice[64] @ 0x80341730 (.bss), stride 0x58,
 * indexed by AXVPB.index. sizeof(SndVoice) == 0x58.
 *
 * Offset proofs (GameCube GUNE5D):
 *   sndVoiceInit      : loop unrolled x2, mtctr=32, `addi r7,r7,176` (=2*0x58);
 *                       stw @4=flags(0x50000000), @8=vol, @12=auxA, @16=auxB,
 *                       @20=volIdx(0x40), @24=panIdx(0x7F), @28=master,
 *                       @32=volL, @36=volR, @40=panL, @44=panR;
 *                       sth @48..@84 = mix[0..18]; second voice @0x5C, @0x88.
 *   sndVoiceSetVolume : `mulli r5,r0,88` (index * 0x58), stw @20=volIdx,
 *                       lwz @32=volL.
 *   sndVoiceSetParams : `stw r30,0(r31)` => voice at offset 0x00.
 */
typedef struct SndVoice {
    /* 0x00 */ struct _AXVPB* voice; /* owning AX voice (AXVPB*), NULL if free */
    /* 0x04 */ u32 flags;            /* aux-A/aux-B enable + dirty/start/stop bits */
    /* 0x08 */ s32 vol;              /* per-voice volume (dB, table-indexed)       */
    /* 0x0C */ s32 auxA;             /* aux bus A send level (dB)                  */
    /* 0x10 */ s32 auxB;             /* aux bus B send level (dB)                  */
    /* 0x14 */ s32 volIdx;           /* volume curve index (0..0x7F)               */
    /* 0x18 */ s32 panIdx;           /* pan curve index (0..0x7F)                  */
    /* 0x1C */ s32 master;           /* master level (dB)                          */
    /* 0x20 */ s32 volL;             /* resolved left volume from volIdx           */
    /* 0x24 */ s32 volR;             /* resolved right volume from volIdx          */
    /* 0x28 */ s32 panL;             /* resolved left pan from panIdx              */
    /* 0x2C */ s32 panR;             /* resolved right pan from panIdx             */
    /* 0x30 */ u16 mix[20];          /* 10 (current,target) pairs pushed to AXPB:  */
                                     /*   [0/1] ve.currentVolume  [2/3] mix.vL     */
                                     /*   [4/5] mix.vR   [6/7] mix.vS              */
                                     /*   [8/9] mix.vAuxAL  [10/11] mix.vAuxAR     */
                                     /*   [12/13] mix.vAuxAS [14/15] mix.vAuxBL    */
                                     /*   [16/17] mix.vAuxBR [18/19] mix.vAuxBS    */
} SndVoice; /* sizeof == 0x58 */

#endif /* _GAME_SNDVOICE_H_ */
