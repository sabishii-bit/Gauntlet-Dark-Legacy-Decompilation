#ifndef GAME_ML_FMATH_H
#define GAME_ML_FMATH_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

f32 smallsqrt(f32 value);
f32 fqdist(f32 x, f32 y);
u32 RandInt(u32 limit);
f32 Random(f32 scale);
void Randomize(u32 seed);
f32 AddAngle(f32 angle, f32 amount);
f32 SubAngle(f32 angle, f32 amount);
f32 FixAngle(f32 angle);
void GetYawPitch(const f32* vector, f32* yaw, f32* pitch);
void CreateYPRMatrix(f32* matrix, const f32* angles);
void CreateRYPMatrix(f32* matrix, const f32* angles);
void CreatePYRMatrix(f32* matrix, const f32* angles);

void ReflectVector2D(const f32* vector, const f32* normal, f32* out);
void ReflectVector(const f32* vector, const f32* normal, f32* out);
f32 SlowNormalVector2D(f32* vector);
f32 NormalVector2D(f32* vector);
f32 SlowNormalVector(f32* vector);
f32 NormalVector(f32* vector);

void MulBodyVecMat4(const f32* vector, f32* out, const f32* matrix);
void MulVec4Mat3(const f32* vector, f32* out, const f32* matrix);
void MulVecMat3(const f32* vector, f32* out, const f32* matrix);
void MulVec4Mat4(const f32* vector, f32* out, const f32* matrix);
void MulVecMat4(const f32* vector, f32* out, const f32* matrix);
void PitchVec3(const f32* vector, f32* out, f32 angle);
void YawVec3(const f32* vector, f32* out, f32 angle);
void WorldVector(const f32* vector, f32* out, const f32* matrix);
void BodyVector(const f32* vector, f32* out, const f32* matrix);

void MulMat3(f32* lhs, f32* rhs, f32* out);
void ExtractScaleMat4(const f32* matrix, f32* out);
void MulMat4Scale(f32* lhs, f32* rhs, f32* out, f32* scale);
void MulMat4(f32* lhs, f32* rhs, f32* out);
void RollMat3(f32* matrix, f32 angle);
void PitchMat3(f32* matrix, f32 angle);
void YawMat3(f32* matrix, f32 angle);
void WRollMat3(f32* matrix, f32 angle);
void WPitchMat3(f32* matrix, f32 angle);
void WYawMat3(f32* matrix, f32 angle);
void ScaleMat3Vec3(const f32* matrix, f32* out, const f32* scale);
void InvertMat4(const f32* matrix, f32* out);
void CopyMat3(const f32* src, f32* dst);
void CopyMat4(const f32* src, f32* dst);
s32 Round(f32 value);

#ifdef __cplusplus
}
#endif

#endif
