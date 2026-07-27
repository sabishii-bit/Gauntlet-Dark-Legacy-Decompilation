#ifndef GAME_G3DPAD_H
#define GAME_G3DPAD_H

#include "types.h"

void G3DAnalogToStickXY(f32* outX, f32* outY, int rawX, int rawY);
void G3DInitStickCurve(void);
void G3DUpdatePadStatus(void);
void G3DInitPadStatus(u32 mask, s32 recalibrate);

#endif
