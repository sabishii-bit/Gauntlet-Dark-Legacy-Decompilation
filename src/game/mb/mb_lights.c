/*
 * mb_lights.c - MB scene lighting (mb_lights.obj).
 *
 * A tiny TU sitting just below mb_main in the MB graphics library: it owns the
 * per-scene light list and ambient/background colour held in the g3d render
 * context (gWinGlobals->lights / gWinGlobals->screen). MBInitLights clears the
 * light count each frame; MBAddLight installs up to three directional lights
 * (colour scaled by intensity, direction normalised); MBSetAmbient/MBSetBGColor
 * set the scene ambient RGB and clear colour.
 *
 * Address range 0x800B6CDC..0x800B6ED8 (4 functions, reverse-Xbox order:
 * MBSetAmbient, MBSetBGColor, MBInitLights, MBAddLight). Names come from the
 * Xbox shell3D PDB (mb_lights.obj); MBAddLight's "Too many lights" assert and
 * the light_default_dir {-0.3,-1.4,1.0} constant (@0x80115D70) pin the TU.
 *
 * cflags_demo (-O4 no-peephole, -Cpp_exceptions on, -str reuse,readonly).
 */
#include "types.h"

/* g3d scene / render context reached via gWinGlobals (window/model-mgr ctx). */
typedef struct MBLight {   /* 0x20 */
    /* 0x00 */ f32 color[3];   /* colour * intensity */
    /* 0x0C */ f32 unk0C;
    /* 0x10 */ f32 dir[3];     /* normalised direction */
    /* 0x1C */ f32 unk1C;
} MBLight;

typedef struct MBScene {
    /* 0x00 */ u8 _pad0[0x9C];
    /* 0x9C */ s32 lightCount;
    /* 0xA0 */ f32 _padA0;
    /* 0xA4 */ f32 ambient[3];
    /* 0xB0 */ u8 _padB0[0xC];
    /* 0xBC */ MBLight lights[3];
} MBScene;

typedef struct MBScreen {
    /* 0x00 */ u8 _pad0[0x1C];
    /* 0x1C */ u32 bgColor;
    /* 0x20 */ u8 _pad20[0x20];
    /* 0x40 */ s32 dirty;
} MBScreen;

typedef struct MBWinGlobals {
    /* 0x00 */ u8 _pad00[0x10];
    /* 0x10 */ MBScreen* screen;
    /* 0x14 */ u8 _pad14[0x08];
    /* 0x1C */ MBScene* lights;
} MBWinGlobals;

extern MBWinGlobals* gWinGlobals;    /* 0x80344FC0 */

extern void vec3Normalize(f32* v);   /* 0x800BDA98 */
extern void ErrorPrintf(const char* fmt, ...);

/* light_default_dir @0x80115D70 : {-0.3f, -1.4f, 1.0f} */
static const f32 light_default_dir[3] = {-0.3f, -1.4f, 1.0f};

/* 0x800B6CDC - MBSetAmbient */
void MBSetAmbient(f32* rgb, f32 intensity)
{
    MBWinGlobals* wg = gWinGlobals;

    if (rgb) {
        wg->lights->ambient[0] = rgb[0] * intensity;
        wg->lights->ambient[1] = rgb[1] * intensity;
        wg->lights->ambient[2] = rgb[2] * intensity;
    } else {
        wg->lights->ambient[0] = intensity;
        wg->lights->ambient[1] = intensity;
        wg->lights->ambient[2] = intensity;
    }
}

/* 0x800B6D38 - MBSetBGColor */
void MBSetBGColor(int r, int g, int b)
{
    MBWinGlobals* wg = gWinGlobals;

    wg->screen->bgColor = (r << 16) | (g << 8) | b;
    wg->screen->dirty = 1;
}

/* 0x800B6D64 - MBInitLights */
void MBInitLights(void)
{
    gWinGlobals->lights->lightCount = 0;
}

/* 0x800B6D78 - MBAddLight */
int MBAddLight(f32* dir, f32* color, f32 intensity)
{
    MBWinGlobals* wg = gWinGlobals;
    MBScene* scene = wg->lights;
    int count = scene->lightCount;
    int idx;

    if (count >= 3) {
        ErrorPrintf("Too many lights");
        return -1;
    }
    scene->lightCount = count + 1;
    idx = count;

    if (color) {
        wg->lights->lights[idx].color[0] = color[0] * intensity;
        wg->lights->lights[idx].color[1] = color[1] * intensity;
        wg->lights->lights[idx].color[2] = color[2] * intensity;
    } else {
        wg->lights->lights[idx].color[0] = intensity;
        wg->lights->lights[idx].color[1] = intensity;
        wg->lights->lights[idx].color[2] = intensity;
    }

    if (dir) {
        wg->lights->lights[idx].dir[0] = dir[0];
        wg->lights->lights[idx].dir[1] = dir[1];
        wg->lights->lights[idx].dir[2] = dir[2];
    } else {
        wg->lights->lights[idx].dir[0] = light_default_dir[0];
        wg->lights->lights[idx].dir[1] = light_default_dir[1];
        wg->lights->lights[idx].dir[2] = light_default_dir[2];
    }

    vec3Normalize(wg->lights->lights[idx].dir);
    return idx;
}
