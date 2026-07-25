#include "abort_exit.h"

void __destroy_global_chain(void);
void __kill_critical_regions(void);
void _ExitProcess(void);
int raise(int sig);

extern void (*_dtors[])(void);

#define SIGABRT 1

static void (*atexit_funcs[64])(void);
static void (*__atexit_funcs[64])(void);

static int __aborting;
static int atexit_curr_func;
static int __atexit_curr_func;
void (*__console_exit)(void);
void (*__stdio_exit)(void);

void __exit(int code)
{
    while (__atexit_curr_func > 0) {
        __atexit_funcs[--__atexit_curr_func]();
    }
    __kill_critical_regions();
    if (__console_exit != NULL) {
        __console_exit();
        __console_exit = NULL;
    }
    _ExitProcess();
}

void exit(int code)
{
    if (__aborting == 0) {
        while (atexit_curr_func > 0) {
            atexit_funcs[--atexit_curr_func]();
        }
        __destroy_global_chain();
        {
            void (**dtor)(void) = _dtors;
            while (*dtor != NULL) {
                (*dtor)();
                dtor++;
            }
        }
        if (__stdio_exit != NULL) {
            __stdio_exit();
            __stdio_exit = NULL;
        }
    }
    __exit(code);
}

#pragma dont_inline on
void abort(void)
{
    raise(SIGABRT);
    __aborting = 1;
    __exit(1);
}
