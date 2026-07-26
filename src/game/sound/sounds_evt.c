#include "types.h"

/* ------------------------------------------------------------------------
 * Front slice of the SOUNDS audio module (Xbox SOUNDS.OBJ), covering the
 * game-event sound-trigger helpers in 0x8009C2CC-0x800A00A0 (~130 fns).
 * The tail name/speech/music slice lives in game/sound/sounds.c (0x800A00A0+);
 * together they are the single SOUNDS TU (shared .sdata2 float-literal pool
 * lbl_80348480..lbl_80348508 = -1,0.5,10,3,4,1,5,... ; shared sndFx callees).
 *
 * This is a SCOUT scaffold: the TU is wired NonMatching (dtk supplies the
 * original bytes) so the build stays green; bodies are not yet reconstructed.
 *
 * Each function is a thin wrapper that fires one/few sound events through the
 * sndFx* engine (game/audio/sndfx.c) or queues announcer voice via
 * sndFxQueAddEx (gated on good_wiz_state<=2).  Precise per-event Xbox PDB
 * names (AudioRotator/AudioElevator/AudioEnemyDies/...) could NOT be pinned
 * 1:1: no sound-id->name table exists and GC function order != Xbox order.
 * They are therefore left as fn_ pending an id table; the caller-domain and
 * play-primitive of every function are recorded in the scout report.
 *
 * Confidently mapped (behavioral, in symbols.txt):
 *   AudioFindPlayerSlot   0x8009C2CC  search player[r3] sub-slots (stride 16
 *                                     from +304) for class(+4)==r4 & type
 *                                     (+12)==r5, skipping empty(+304==0); ret idx
 *   AudioPlay3DSel        0x8009C32C  play sound on track 15, sndFxPlay3DAtten
 *                                     when arg4!=0 else sndFxPlay3D
 *   AudioSetupBossStreams 0x8009E108  builds boss/wizard music+speech stream
 *                                     names ("wizmus.s","GOL%c","DIE","KILL",
 *                                     "%s1"/"%s2") into sMusicSlot0..2 by gBossType
 *   AudioClearActiveTracks 0x8009EDF0 sActiveTrackId[0..44]=-1; sMusicSlot0..2=-1
 *                                     (note: audio.c already owns AudioClearTracks)
 * Data: sVoiceRotIdxA/B (0x80344C38/34) 0..3 announcer-voice rotation counters.
 * ---------------------------------------------------------------------- */
