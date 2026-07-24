#include "types.h"

/* Midway "bulletproof" utilities (Xbox counterpart: PB_UTILS.OBJ - rand,
 * srand, pbRand, pbGetTime, pbSetTime, pbGetCPUTime, pbPulseTime; stricmp is
 * GameCube-only here, the Xbox build used the CRT's _stricmp). */

typedef s64 OSTime;
OSTime OSGetTime(void);
long time(long* t);

extern u32 pbLoad;

static u8 sNeedSeed = 1;

u8 pbMeasureLoad = 0;
static long sCPUBase;
static long sTime;
static long sCPUTime;
static long sLoadBase;
static u32 sSeed;
static long sSeconds;

int rand(void)
{
    long t;

    if (sNeedSeed) {
        sSeed = time(&t);
        sNeedSeed = 0;
    }
    sSeed = sSeed * 0x343FD + 0x269EC3;
    return sSeed & 0x7fff;
}

void srand(u32 seed)
{
    long t;

    if (sNeedSeed) {
        time(&t);
        sNeedSeed = 0;
    }
    sSeed = seed;
}

int stricmp(const char* a, const char* b)
{
    int ca;
    int cb;

    do {
        ca = *(u8*) a;
        a++;
        if (ca >= 'A' && ca <= 'Z') {
            ca += 0x20;
        }
        cb = *(u8*) b;
        b++;
        if (cb >= 'A' && cb <= 'Z') {
            cb += 0x20;
        }
    } while (ca != 0 && ca == cb);
    return ca - cb;
}

int pbRand(void)
{
    u8 unused1[4];
    long t;
    u8 unused2[4];

    if (sNeedSeed) {
        sSeed = time(&t);
        sNeedSeed = 0;
    }
    sSeed = sSeed * 0x343FD + 0x269EC3;
    return sSeed & 0x7fff;
}

long pbGetTime(void)
{
    return sTime;
}

void pbSetTime(long t)
{
    sCPUBase = sCPUTime - t;
}

long pbGetCPUTime(void)
{
    return sCPUTime - sCPUBase;
}

u32 pbPulseTime(void)
{
    u32 ret;

    sSeconds = OSGetTime() / (s64) (*(u32*) 0x800000F8 / 4 / 1000);
    sTime = sSeconds * 300000;
    sCPUTime = ((u64) (u32) sSeconds * 15720 + 500) / 1000;
    if (pbMeasureLoad) {
        if (sLoadBase == 0) {
            sLoadBase = sSeconds;
        }
        /* NOTE: ret is intentionally left uninitialized when load
         * measurement is off; the original returns whatever the division
         * left in r3 (shape-forced, matches retail). */
        ret = (sSeconds - sLoadBase) * 3;
        pbLoad = ret / 0x32;
    }
    return ret;
}
