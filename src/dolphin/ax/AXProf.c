#include "types.h"
#include "dolphin/ax.h"

#include "__ax.h"

typedef struct _AXPROFILE {
    /* 0x00 */ u64 axFrameStart;
    /* 0x08 */ u64 auxProcessingStart;
    /* 0x10 */ u64 auxProcessingEnd;
    /* 0x18 */ u64 userCallbackStart;
    /* 0x20 */ u64 userCallbackEnd;
    /* 0x28 */ u64 axFrameEnd;
    /* 0x30 */ u32 axNumVoices;
} AXPROFILE;

static AXPROFILE* __AXProfile;
static u32 __AXMaxProfiles;
static u32 __AXCurrentProfile;
static u32 __AXProfileInitialized;

AXPROFILE* __AXGetCurrentProfile(void)
{
    AXPROFILE* profile;

    if (__AXProfileInitialized != 0U) {
        profile = &__AXProfile[__AXCurrentProfile];
        __AXCurrentProfile += 1;
        __AXCurrentProfile %= __AXMaxProfiles;
        return profile;
    }
    return 0;
}
