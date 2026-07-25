#include "types.h"

typedef void (*__signal_func_ptr)(int);

void exit(int code);

#define SIG_IGN ((__signal_func_ptr) 1)

static __signal_func_ptr sig_funcs[6];

int raise(int signal)
{
    __signal_func_ptr f;

    if (signal < 1 || signal > 6) {
        return -1;
    }
    f = sig_funcs[signal - 1];
    if (f != SIG_IGN) {
        sig_funcs[signal - 1] = 0;
    }
    if (f == SIG_IGN || (f == 0 && signal == 1)) {
        return 0;
    }
    if (f == 0) {
        exit(0);
    }
    f(signal);
    return 0;
}
