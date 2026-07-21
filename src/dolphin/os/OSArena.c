#include "dolphin/types.h"

static void* __OSArenaHi;
static void* __OSArenaLo = (void*)-1;

void* OSGetArenaHi(void) {
    return __OSArenaHi;
}

void* OSGetArenaLo(void) {
    return __OSArenaLo;
}

void OSSetArenaHi(void* newHi) {
    __OSArenaHi = newHi;
}

void OSSetArenaLo(void* newLo) {
    __OSArenaLo = newLo;
}
