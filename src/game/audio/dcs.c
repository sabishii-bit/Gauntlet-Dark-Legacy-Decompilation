/*
 * dcs.c - Midway DCS (Digital Compression System) audio driver, GameCube port.
 *
 * Text 0x800D1E04-0x800D4308.  The low-level sound driver: loads DCS sound
 * "banks" (VAG/BANK format), decodes/streams ADPCM samples into ARAM via the
 * AR queue (ARQ), and drives the GameCube AX voices through the sndvoice.c
 * layer.  Replaces the PS2/IOP SPU backend (DCS.OBJ/DCS_PS2.OBJ/BANK.OBJ on
 * the arcade/Xbox builds) with an AX/ARQ backend.  Debug strings tag the
 * module: "DCSERROR:", "BankReadHeader", "VagParseHeader NOT A VAG!",
 * "Duck nonzero -- resetting".
 *
 * The ARAM helpers dcsAram* are also called cross-TU by game/sys/memcard.c
 * (save data is parked in the top of ARAM).
 *
 * NonMatching: reconstruction scaffold - names/behaviour identified by
 * scouting; bodies not yet reconstructed.  Extracted bytes are linked from
 * the DOL.
 */
#include "types.h"

/* 0x800D1E04  trigger/refresh a channel; -> dcsVoiceStart */
void dcsChannelPlay(void) {
}

/* 0x800D1ED0  recompute per-channel voice state each tick */
void update_chinfo(void) {
}

/* 0x800D1FFC  push volume/pan to a channel voice */
void dcsChannelSetVolPan(void) {
}

/* 0x800D21B4  volume/pan variant */
void dcsChannelSetVolPan2(void) {
}

/* 0x800D2314  reset the sample allocator (-> pool_new) */
void dcsAllocReset(void) {
}

/* 0x800D2350  look up bank handle/size in dcsBankData */
void dcsBankQuery(void) {
}

/* 0x800D23A0  start playback on a channel */
void dcsVoiceStart(void) {
}

/* 0x800D2534  poll: any bank/stream still loading? */
void AudioStillLoading(void) {
}

/* 0x800D2568  wrapper -> AudioQueUpdate */
void dcsQuePoll(void) {
}

/* 0x800D258C  service the queued sample/duck requests */
void AudioQueUpdate(void) {
}

/* 0x800D285C  open file, read bank header/calls/vags */
void dcsBankLoad(void) {
}

/* 0x800D29D4  set dcsResetPending */
void dcsRequestReset(void) {
}

/* 0x800D29E0  free a loaded bank (-> pool_dispose) */
void dcsBankUnload(void) {
}

/* 0x800D2A68  read VAG sample table, upload to ARAM (readVags) */
void dcsReadVags(void) {
}

/* 0x800D2CEC  read the bank call list (readCalls) */
void dcsReadCalls(void) {
}

/* 0x800D30B4  validate sample indices (callFixup) */
void dcsBankCheckSamples(void) {
}

/* 0x800D3184  service the channel queue */
void dcsServiceQueue(void) {
}

/* 0x800D321C  assign+start a voice for a sample */
void dcsVoicePlay(void) {
}

/* 0x800D33B8  configure AX ADPCM voice for a sample */
void dcsVoiceSetupAdpcm(void) {
}

/* 0x800D3608  begin streaming a sample into ARAM */
void dcsSampleStream(void) {
}

/* 0x800D3674  ARQ post MRAM->ARAM for sample data */
void dcsSampleUpload(void) {
}

/* 0x800D374C  alloc ARAM + ARQ upload */
void dcsSampleAllocUpload(void) {
}

/* 0x800D3874  ARQ read from top of ARAM (memcard uses) */
void dcsAramReadTop(void) {
}

/* 0x800D38F8  ARQ write to top of ARAM (memcard uses) */
void dcsAramWriteTop(void) {
}

/* 0x800D3970  ARQ MRAM->ARAM copy (memcard uses) */
void dcsAramWrite(void) {
}

/* 0x800D39E8  ARQ ARAM->MRAM copy (memcard uses) */
void dcsAramRead(void) {
}

/* 0x800D3A70  ARQ completion cb; clears dcsAramBusy */
void dcsAramCallback(void) {
}

/* 0x800D3A7C  ARQ completion cb; clears dcsSampleBusy */
void dcsSampleCallback(void) {
}

/* 0x800D3A88  start an AX voice (src/state/vol/pan) */
void dcsVoiceStartAx(void) {
}

/* 0x800D3C40  set voice master volume */
void dcsVoiceSetMaster(void) {
}

/* 0x800D3CCC  refresh voice vol/pan/master */
void dcsVoiceUpdate(void) {
}

/* 0x800D3DC4  is this channel/voice active? (voiceInUse) */
void dcsVoiceInUse(void) {
}

/* 0x800D3E24  read+validate a BANK file header */
void BankReadHeader(void) {
}

/* 0x800D4048  parse in-memory BANK header */
void BankParseHeader(void) {
}

/* 0x800D415C  parse a VAG sample header */
void VagParseHeader(void) {
}

/* 0x800D42E4  set stream loop/one-shot flag */
void dcsSetStreamFlag(void) {
}

