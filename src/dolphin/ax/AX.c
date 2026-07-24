#include "types.h"
#include "dolphin/ax.h"

#include "__ax.h"

void AXInit(void)
{
    __AXAllocInit();
    __AXVPBInit();
    __AXSPBInit();
    __AXAuxInit();
    __AXClInit();
    __AXOutInit();
}
