/*
 * buffile.c - DCS buffered file reader (BUFFILE.OBJ), GameCube port.
 *
 * Text 0x800D4308-0x800D4960.  A small double-buffered reader over the PS2
 * file API (sceOpen/sceRead/sceLseek/sceClose, shimmed by game/ps2/fakelib.c
 * on GameCube).  Used by dcs.c to stream BANK/VAG data off disc.  Debug:
 * "OPEN FILE %s", "FileBuf read %d wanted %d", "No more FILEBUF handles".
 *
 * NonMatching: reconstruction scaffold.
 */
#include "types.h"

/* 0x800D4308  close handle, sceClose */
void FileBufClose(void) {
}

/* 0x800D436C  seek + refill buffer, sceLseek */
void FileBufSeek(void) {
}

/* 0x800D44BC  copy N bytes out, refilling via sceRead */
void FileBufGet(void) {
}

/* 0x800D46E8  rewind/reopen an existing handle */
void FileBufReopen(void) {
}

/* 0x800D4790  open a file ("OPEN FILE %s"), sceOpen */
void FileBufOpen(void) {
}

/* 0x800D4854  alloc handle+buffer and open, sceOpen */
void FileBufStart(void) {
}

