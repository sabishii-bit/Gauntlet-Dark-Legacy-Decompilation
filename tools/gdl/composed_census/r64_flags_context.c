/* Synthetic context/pragma calibration, not proposed game source. */
typedef unsigned int u32;
#if STORAGE == 1
extern u32 gSysFlags;
#elif STORAGE == 2
static volatile u32 gSysFlags;
#else
static u32 gSysFlags;
#endif
#if CONTEXT
u32 before(u32 x, u32 y) { gSysFlags = x + y; return gSysFlags * x; }
#endif
#if OVERRIDE
#pragma optimization_level 4
#pragma peephole on
#pragma scheduling on
#endif
void sysClearFlags(u32 mask
#if ARITY
, u32 unused
#endif
) {
    gSysFlags &= ~mask;
}
void sysSetFlags(u32 mask) { gSysFlags |= mask; }
u32 optControl(u32 x, u32 y) {
    u32 a = x * 8;
    u32 b = x * 8;
    if (y) a = a + b;
    return a + (x * 8);
}
