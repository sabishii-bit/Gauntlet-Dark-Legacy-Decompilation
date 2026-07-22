#include "types.h"

#pragma peephole off

typedef void (*voidfunctionptr)(void);
extern voidfunctionptr _ctors[];

void PPCHalt(void);

void __init_user(void);
static void __init_cpp(void);
void _ExitProcess(void);

void __init_user(void) {
    __init_cpp();
}

static void __init_cpp(void) {
    voidfunctionptr* constructor;

    for (constructor = _ctors; *constructor; constructor++) {
        (*constructor)();
    }
}

void _ExitProcess(void) {
    PPCHalt();
}
