/*
 * dcsdrv.c - DCS driver front-end (DCS_PS2.OBJ aud_* layer), GameCube port.
 *
 * Text 0x800D4960-0x800D5260.  Owns driver init, the per-frame poll, and the
 * numeric command dispatcher that game code (game/audio/soundmgr.c) posts
 * sound/resource requests through.  Debug: "dcs_driver", "/gauntlet/".
 *
 * NonMatching: reconstruction scaffold.
 */
#include "types.h"

/* 0x800D4960  init driver + block pool (-> pool_init) */
void dcsInit(void) {
}

/* 0x800D49E4  per-frame driver tick ("dcs_driver") */
void dcsMain(void) {
}

/* 0x800D4BF4  dispatch a numeric request (id,in,out) */
void dcsHandleRequest(void) {
}

