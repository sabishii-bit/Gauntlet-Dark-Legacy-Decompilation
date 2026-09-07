#include "types.h"
#include "game/leveldata.h"

#ifndef offsetof
#define offsetof(type, memb) ((u32)&((type*)0)->memb)
#endif

/* LIGHTS.OBJ: the contiguous lighting text and exception records follow
 * ITEMS.OBJ in the GameCube image. Only the recovered zero literal is owned
 * here; surrounding constants and mutable state remain externally defined. */
extern u8* gCurLevel;
extern f32 sLevelAmbient;
extern f32 sLevelAmbientScale;
extern f32 sLightingScratchY;
extern f32 sLightingScratchZ;
extern f32 sLightingScratchX;
extern f32 AmbientSpecialTime;
extern f32 AmbientSpecialValue;
extern f32 AmbientSpecialCurValue;
extern f32 sOne;
extern f32 sNegativeHalf;
extern volatile f32 sMusicFadeBase;
extern f32 sNegativeOne;
extern f64 sAmbientMinimum;
extern f64 sAmbientDecay;
extern f64 sAmbientBrightenStep;
extern f64 sAmbientDarkenStep;
extern f64 sAmbientMaximum;

extern void MBInitLights(void);
extern int MBAddLight(f32* dir, f32* color, f32 intensity);
extern void MBSetAmbient(f32 val, f32* p);
extern void pbResetWindowPool(void);
extern void pbSetWindowUV1(double a, double b);
extern void pbSetWindowUV0(double a, double b);
void DoLighting(s32 flag);

void InitLighting(s32 flag)
{
    MBInitLights();
    if (flag != 0) {
        sLevelAmbient = *(f32*)(gCurLevel + offsetof(level_data, ambient));
        MBAddLight((f32*)(gCurLevel + offsetof(level_data, lightdir)),
                   (f32*)(gCurLevel + offsetof(level_data, lightcolor_fp)),
                   *(f32*)(gCurLevel + offsetof(level_data, lightinten)));
    } else {
        sLevelAmbient = sOne;
    }
    sLevelAmbientScale = sOne;
    MBSetAmbient(sLevelAmbient, NULL);
    DoLighting(1);
    sLightingScratchY = 0.0f;
    sLightingScratchZ = sNegativeHalf;
    sLightingScratchX = sNegativeHalf;
    AmbientSpecialTime = 0.0f;
    AmbientSpecialValue = 0.0f;
    AmbientSpecialCurValue = 0.0f;
}

void DoLighting(s32 flag)
{
    u8 unused[16];
    f32 a;
    u8 unused2[8];
    f64 step;
    f64 lit;

    pbResetWindowPool();
    if (gCurLevel != NULL && (*(u32*)gCurLevel & 8)) {
        AmbientSpecialCurValue = sNegativeOne;
        AmbientSpecialValue = sNegativeOne;
    } else {
        if (sAmbientMinimum != AmbientSpecialValue && sMusicFadeBase > AmbientSpecialTime) {
            AmbientSpecialValue = (f32)(AmbientSpecialValue * sAmbientDecay);
            a = AmbientSpecialValue;
            *(u32*)&a &= 0x7FFFFFFF;
            if (a < sAmbientBrightenStep) {
                AmbientSpecialValue = 0.0f;
            }
        }
    }
    if (AmbientSpecialValue != AmbientSpecialCurValue) {
        if (AmbientSpecialValue - AmbientSpecialCurValue < sAmbientDarkenStep) {
            step = sAmbientDarkenStep;
        } else if (AmbientSpecialValue - AmbientSpecialCurValue > sAmbientBrightenStep) {
            step = sAmbientBrightenStep;
        } else {
            step = AmbientSpecialValue - AmbientSpecialCurValue;
        }
        AmbientSpecialCurValue = AmbientSpecialCurValue + (f32)step;
    }
    lit = sLevelAmbient * sLevelAmbientScale + AmbientSpecialCurValue <
                  sAmbientMinimum ?
          sAmbientMinimum :
          (sLevelAmbient * sLevelAmbientScale + AmbientSpecialCurValue >
                   sAmbientMaximum ?
           sAmbientMaximum :
           sLevelAmbient * sLevelAmbientScale + AmbientSpecialCurValue);
    MBSetAmbient((f32)lit, NULL);
    pbSetWindowUV1(sOne, AmbientSpecialCurValue);
    pbSetWindowUV0(sOne, AmbientSpecialCurValue);
}
