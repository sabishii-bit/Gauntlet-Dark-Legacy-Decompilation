#include "types.h"

typedef unsigned long time_t;

s64 OSGetTime(void);

/* GameCube time base: 50625000 ticks/second; epoch adjusted to 1970. */
time_t __get_time(void)
{
    return (time_t) (OSGetTime() / 50625000) - 0x43E83E00;
}
