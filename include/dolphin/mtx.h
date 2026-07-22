#ifndef DOLPHIN_MTX_H
#define DOLPHIN_MTX_H

#include "types.h"

typedef struct Vec {
    f32 x, y, z;
} Vec, *VecPtr;

typedef f32 Mtx[3][4];
typedef f32 (*MtxPtr)[4];
typedef f32 Mtx44[4][4];
typedef f32 (*Mtx44Ptr)[4];

void PSMTXIdentity(Mtx m);
void PSMTXCopy(Mtx src, Mtx dst);
void PSMTXConcat(Mtx a, Mtx b, Mtx ab);
void PSMTXTranspose(Mtx src, Mtx xPose);
void PSMTXMultVec(Mtx m, Vec* src, Vec* dst);
void C_MTXOrtho(Mtx44 m, f32 t, f32 b, f32 l, f32 r, f32 n, f32 f);
void PSVECNormalize(Vec* src, Vec* unit);
void PSVECCrossProduct(Vec* a, Vec* b, Vec* axb);

#endif
