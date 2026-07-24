/* pb_window.c -- Midway's window/camera/projection layer (pb_window.obj on
 * Xbox, 26 fns; the GCN build keeps 15). Function names from shell3D.pdb;
 * PBWINDOW layout matches the Xbox PDB struct field-for-field.
 * WIP: debugScissor/pbProjCalc/pbWinSetup/pbCameraUpdate still stubs.
 */

#include "types.h"

#pragma dont_inline on

typedef float f32;

void sceSamp0Normalize(f32* v0, f32* v1);
void sceSamp0RotCameraMatrix(f32* m, f32* p, f32* zd, f32* yd);
void sceSamp0CopyMatrix34(f32* m0, f32* m1);
void mat44LookAt__FR5mat44R4vec4R4vec4R4vec4(void* m, void* p, void* zd, void* yd);
void mat44InvRigid__FR5mat44R5mat44(void* d, void* s);
void mat44Mult__FR5mat44R5mat44R5mat44(void* d, void* a, void* b);
#define mat44LookAt mat44LookAt__FR5mat44R4vec4R4vec4R4vec4
#define mat44InvRigid mat44InvRigid__FR5mat44R5mat44
#define mat44Mult mat44Mult__FR5mat44R5mat44R5mat44
float atan(float x);
float atan2(float y, float x);
float sin(float x);
float cos(float x);
void ErrorPrintf(const char* fmt, ...);
void __as__4vec4FRC4vec4(void* d, const void* s);   /* vec4::operator= */
void __as__4vec3FRC4vec3(void* d, const void* s);   /* vec3::operator= */
void __as__5mat44FRC5mat44(void* d, const void* s); /* mat44::operator= */
void vec3Scale__FR4vec3R4vec3f(void* d, void* v, f32 s);
void identity__5mat44Fv(void* m); /* mat44::identity */
void OSReport(const char* fmt, ...); /* placeholder: real callee fn_800BC2EC */
void pbDebugPrintf(const char* fmt, ...);



typedef struct PBWINDOW {
    u16 flags;                /* 0x000 */
    u8 cam_dirty;             /* 0x002 */
    u8 proj_dirty;            /* 0x003 */
    u32 pad;                  /* 0x004 */
    f32 clip_width;           /* 0x008 */
    f32 clip_height;          /* 0x00C */
    u16 scissor[4];           /* 0x010 (12.4 fixed) */
    f32 cam_pos[4];           /* 0x018 */
    f32 cam_look[4];          /* 0x028 */
    f32 cam_up[4];            /* 0x038 */
    f32 cam_pitch;            /* 0x048 */
    f32 cam_yaw;              /* 0x04C */
    f32 view_angle_horiz;     /* 0x050 */
    f32 aspect;               /* 0x054 */
    f32 near_z;               /* 0x058 */
    f32 far_z;                /* 0x05C */
    f32 hva_sin_x;            /* 0x060 */
    f32 hva_cos_x;            /* 0x064 */
    f32 hva_sin_y;            /* 0x068 */
    f32 hva_cos_y;            /* 0x06C */
    f32 left;                 /* 0x070 */
    f32 right;                /* 0x074 */
    f32 top;                  /* 0x078 */
    f32 bottom;               /* 0x07C */
    f32 projection[4][4];     /* 0x080 */
    f32 viewport[4][4];       /* 0x0C0 */
    f32 view_screen[4][4];    /* 0x100 */
    f32 clipport[4][4];       /* 0x140 */
    f32 view_clip[4][4];      /* 0x180 */
    f32 clip_screen[4][4];    /* 0x1C0 */
    f32 camera[4][4];         /* 0x200 */
    f32 icamera[4][4];        /* 0x240 */
    f32 world_npc[4][4];      /* 0x280 */
    f32 world_screen[4][4];   /* 0x2C0 */
    f32 world_clip[4][4];     /* 0x300 */
    f32 npc2clip[4];          /* 0x340 */
    f32 clip2npc[4];          /* 0x350 */
    f32 npc2screen[2][4];     /* 0x360 */
    f32 clip2screen[2][4];    /* 0x380 */
    f32 screen2clip[2][4];    /* 0x3A0 */
} PBWINDOW; /* 0x3C0 */

/* VU1NEWMTXPACKET heritage: matrix packet as consumed by the renderer */
typedef struct MTXPACKET {
    u32 hdr[4];            /* 0x00 */
    f32 mtx[4][4];         /* 0x10 */
    f32 clip2npc[4];       /* 0x50 */
    f32 npc2screen[2][4];  /* 0x60 */
    f32 clip2screen[2][4]; /* 0x80 */
} MTXPACKET;

/* main window packet variant with a larger header */
typedef struct MTXPACKET2 {
    u32 hdr[8];            /* 0x00 */
    f32 mtx[4][4];         /* 0x20 */
    f32 clip2npc[4];       /* 0x60 */
    f32 npc2screen[2][4];  /* 0x70 */
    f32 clip2screen[2][4]; /* 0x90 */
} MTXPACKET2;

extern f32 gClip2NpcDefault[4]; /* DAT_801284f8 */

/* positional light node */
typedef struct PBLIGHT {
    f32 color[4];          /* 0x00 (scaled by 128 into the packet) */
    f32 pos[4];            /* 0x10 */
    f32 radius;            /* 0x20 */
    f32 intensity;         /* 0x24 */
    struct PBLIGHT* next;  /* 0x28 */
} PBLIGHT;

typedef struct PBLIGHTPKT {
    f32 pos[3];    /* 0x00 */
    f32 invR2;     /* 0x0C */
    f32 color[3];  /* 0x10 */
    f32 intensity; /* 0x1C */
} PBLIGHTPKT; /* 0x20 */

typedef struct PBLIGHTBLOCK {
    u8 unk0[0x98];
    f32 ambient;        /* 0x98 */
    s32 dirCount;       /* 0x9C */
    s32 posCount;       /* 0xA0 */
    f32 ambientRow[4];  /* 0xA4 */
    PBLIGHT* posHead;   /* 0xB4 */
    u8 unkB8[4];        /* 0xB8 */
    u8 dirBase[0x20];   /* 0xBC (dir nodes, stride 0x20, vec at +0x10) */
    PBLIGHTPKT pkt[12]; /* 0xDC */
    f32 radius[12];     /* 0x25C */
} PBLIGHTBLOCK;

extern f32 gVpScaleY;        /* FLOAT_80345160 */
extern f32 gProjD3D[4][4];   /* DAT_802c9bc8 */
extern f32 gScreenAspect;    /* DAT_8025ee70 */
extern int gPbDebugCam;      /* DAT_80345158 */
extern int gPbDebugCamTimer; /* DAT_8034515c */
extern int gWinDebug[];      /* DAT_80343fb8 (PBWINDEBUG image) */
typedef struct PBSCREEN {
    u32 flags;   /* 0x00 */
    u8 pad[0x1C];
    s32 w;       /* 0x20 */
    s32 h;       /* 0x24 */
    u8 pad2[8];
    s32 w2;      /* 0x30 */
    s32 h2;      /* 0x34 */
    f32 xoff;    /* 0x38 */
    f32 yoff;    /* 0x3C */
    s32 dirty;   /* 0x40 */
} PBSCREEN;

typedef struct PBWINLIST {
    PBWINDOW* windows; /* 0x0 */
    int count;         /* 0x4 */
    int unk8;          /* 0x8 */
    int unkC;          /* 0xC */
} PBWINLIST;

typedef struct PBWINGLOBALS {
    void* unk00;        /* 0x00 */
    PBWINDOW* current;  /* 0x04 */
    void* framebuf;     /* 0x08 */
    void* unk0C;        /* 0x0C */
    void* screen;       /* 0x10 */
    void* unk14;        /* 0x14 */
    PBWINLIST* list;    /* 0x18 */
    void* lights;       /* 0x1C */
    u8 unk20[0x24];     /* 0x20 */
    void* unk44;        /* 0x44 */
} PBWINGLOBALS;

extern PBWINGLOBALS* gWinGlobals;   /* DAT_80344fc0 */
extern PBWINDOW** gCurWindowMirror; /* DAT_80343f10 (points at mirror slot) */
extern u32 gWinDefault;             /* DAT_80345154 (SDA-addressed) */
extern PBWINLIST gDefaultWinList;   /* DAT_802c9b78 */
extern PBWINDOW gWindows[];         /* DAT_802c93f8 */
extern f32 gCameraMtx[4][4];        /* DAT_802c9b88 */
extern f32 gUpVector[];             /* DAT_802c9b88's up? placeholder */

void pbCloseWindow(void);
void pbSetDefaultWindow(void);
void pbUpdateMatricies(void);
static void debugScissor(u32* packet);
void pbProjCalc(void);
void pbWinSetup(void);
void pbCameraUpdate(void);
void pbCameraCalc(void);
void pbInitCamera(f32* pos, f32* look);
void MBWindowClip(f32 w, f32 h, f32 nearz, f32 farz);
void MBWindowProjection(f32 angle, f32 aspect);
void MBWindowViewport(f32 l, f32 r, f32 t, f32 b);
void MBSetCurrentWindow(void);
void pbInitWindow(void);

/* 0x800C8294 */
void pbCloseWindow(void)
{
    PBWINGLOBALS* g = gWinGlobals;

    if (g->unk44 != 0) {
        return;
    }
    g->unk44 = &gWinDefault;
}

/* 0x800C82B0 */
void pbSetDefaultWindow(void)
{
    gWinGlobals->unk44 = &gWinDefault;
}

/* 0x800C82C0 */
void pbUpdateMatricies(void)
{
    PBWINGLOBALS* g;
    PBWINDOW* w;

    pbProjCalc();
    g = gWinGlobals;
    /* pbCameraCalc, written out (original pastes the body) */
    sceSamp0Normalize(g->current->cam_look, g->current->cam_look);
    w = g->current;
    mat44LookAt(w->camera, w->cam_pos, w->cam_look, w->cam_up);
    w = g->current;
    sceSamp0RotCameraMatrix((f32*) gCameraMtx, w->cam_pos, w->cam_look, w->cam_up);
    mat44InvRigid(g->current->icamera, g->current->camera);
    g->current->cam_dirty = 0;
    g = gWinGlobals;
    w = g->current;
    mat44Mult(w->world_npc, w->projection, w->camera);
    w = g->current;
    mat44Mult(w->world_screen, w->viewport, w->world_npc);
    w = g->current;
    mat44Mult(w->world_clip, w->clipport, w->world_npc);
}

/* 0x800C838C: debug window scissor override */
static void debugScissor(u32* packet)
{
    u32 x0, x1, y0, y1;
    u32 v;

    if (gWinDebug[0] != 0) {
        if (gWinDebug[2] == 0) {
            f32 scale = *(f32*) &gWinDebug[3];
            if ((double) scale != 0.0f) {
                if (gWinDebug[0] != 0) {
                    f32 t = (f32) (0.5 * (1.0 - (double) scale));
                    if (0.0 != (double) scale) {
                        PBSCREEN* sc = (PBSCREEN*) gWinGlobals->screen;
                        f32 w = (f32) sc->w;
                        f32 h = (f32) sc->h;
                        x0 = (u32) (w * t);
                        x1 = (int) (w * (1.0f - t));
                        y0 = (u32) (h * t);
                        y1 = (int) (h * (1.0f - t));
                    }
                }
            }
        } else {
            x0 = 0;
            y0 = 0;
            x1 = ((PBSCREEN*) gWinGlobals->screen)->w;
            y1 = ((PBSCREEN*) gWinGlobals->screen)->h;
        }
        v = x0 | (x1 << 0x10);
        packet[0x70] = v;
        packet[0x71] = y0 | (y1 << 0x10);
        packet[0x74] = v;
        packet[0x75] = y0 | (y1 << 0x10);
    }
}

/* 0x800C84CC: rebuilds scissor, projection, viewport, clipport,
   clip_screen and the screen-space quads from the window parameters.
   NOTE: writes the matrices through &gWindows[0] directly (the original
   assumes the current window is the first one). */
void pbProjCalc(void)
{
    PBWINGLOBALS* g = gWinGlobals;
    PBWINDOW* w = gWindows;
    volatile f32 sw, sh;
    volatile f32 pl, pr, pt, pb;
    volatile f32 vpcx, vpcy;
    volatile f32 f;
    f32 pf;
    volatile f32 fov, cw, ch;
    volatile f32 zA, zB;
    volatile f32 nz, fz;
    volatile f32 fw, fh;
    volatile f32 ratio;
    volatile f32 at;
    volatile f32 sn, cn, sn2, cn2;
    volatile f32 vpsx, vpsy;
    volatile f32 sx, sy;
    volatile f32 cpx, cpy;
    f32 one;

    sw = (f32) ((PBSCREEN*) g->screen)->w;
    sh = (f32) ((PBSCREEN*) g->screen)->h;

    pf = 0.0f;
    if (g->current->left < 0.0f) {
        pf = sw;
    }
    pl = g->current->left + pf;
    pf = 0.0f;
    if (g->current->right < 0.0f) {
        goto skipR; /* original goto-form; the pair survives no-peephole */
    }
    pf = sw;
skipR:
    pr = pf - g->current->right;
    pf = 0.0f;
    if (g->current->top < 0.0f) {
        goto skipT; /* original bug: label sits before the assignment, so
                       the top edge always wraps by the screen height */
    }
skipT:
    pf = sh;
    pt = g->current->top + pf;
    pf = 0.0f;
    if (g->current->bottom < 0.0f) {
        goto skipB;
    }
    pf = sh;
skipB:
    pb = pf - g->current->bottom;

    fov = g->current->view_angle_horiz;
    cw = g->current->clip_width;
    ch = g->current->clip_height;
    zA = (f32) ((PBSCREEN*) g->screen)->w2;
    zB = (f32) ((PBSCREEN*) g->screen)->h2;
    fz = g->current->far_z;
    nz = g->current->near_z;

    g->current->scissor[0] = ((int) pl << 5) | (g->current->scissor[0] & 0x1f);
    g->current->scissor[1] = ((int) pr << 5) | (g->current->scissor[1] & 0x1f);
    g->current->scissor[2] = ((int) pt << 5) | (g->current->scissor[2] & 0x1f);
    g->current->scissor[3] = ((int) pb << 5) | (g->current->scissor[3] & 0x1f);

    if (gWinDebug[0] != 0) {
        f32 sc1 = (f32) (0.5 * (1.0 - (double) *(f32*) &gWinDebug[3]));
        if (0.0 != (double) *(f32*) &gWinDebug[3]) {
            pl = (f32) ((PBSCREEN*) g->screen)->w * sc1;
            pr = (f32) ((PBSCREEN*) g->screen)->w * (1.0f - sc1);
            pt = (f32) ((PBSCREEN*) g->screen)->h * sc1;
            pb = (f32) ((PBSCREEN*) g->screen)->h * (1.0f - sc1);
        }
        if (0.0 != (double) *(f32*) &gWinDebug[4]) {
            cw = (f32) ((double) (f32) ((PBSCREEN*) g->screen)->w * (double) *(f32*) &gWinDebug[4]);
            ch = (f32) ((PBSCREEN*) g->screen)->h * *(f32*) &gWinDebug[4];
        }
    }

    fw = pr - pl;
    fh = pb - pt;
    vpcx = 0.5f * ((pl + pr) - sw) + ((PBSCREEN*) g->screen)->xoff;
    vpcy = 0.5f * ((pt + pb) - sh) + ((PBSCREEN*) g->screen)->yoff;
    ratio = (g->current->aspect * fw) / fh;
    if ((((PBSCREEN*) g->screen)->flags & 2) != 0) {
        ratio = ratio * 2.0f;
    }

    f = (f32) (0.008726646261111111 * (double) fov);
    sn = sin(f);
    cn = cos(f);
    f = sn / cn;
    ratio = f / ratio;
    at = atan(ratio);
    sn2 = sin(at);
    cn2 = cos(at);
    f = 1.0f / f;
    ratio = 1.0f / ratio;
    w->hva_sin_x = sn;
    w->hva_cos_x = cn;
    w->hva_sin_y = sn2;
    w->hva_cos_y = cn2;
    identity__5mat44Fv(w->projection);
    w->projection[0][0] = f;
    w->projection[1][1] = -ratio;
    w->projection[2][2] = (nz + fz) / (fz - nz);
    w->projection[2][3] = 1.0f;
    w->projection[3][2] = ((-2.0f * nz) * fz) / (fz - nz);
    w->projection[3][3] = 0.0f;

    vpsx = 0.5f * fw;
    vpsy = 0.5f * fh;
    f = 0.5f * (zA - zB);
    zA = 0.5f * (zA + zB);

    sx = vpsx;
    sy = vpsy;
    if (gWinDebug[0] != 0 && gWinDebug[0] == 2) {
        sx = vpsx * *(f32*) &gWinDebug[8];
        sy = vpsy * *(f32*) &gWinDebug[9];
        vpcx = vpsx * -(2.0f * *(f32*) &gWinDebug[10] - (*(f32*) &gWinDebug[8] - 1.0f)) + vpcx;
        vpcy = vpsy * -(2.0f * *(f32*) &gWinDebug[11] - (*(f32*) &gWinDebug[9] - 1.0f)) + vpcy;
    }
    identity__5mat44Fv(w->viewport);
    w->viewport[0][0] = sx;
    w->viewport[1][1] = sy;
    w->viewport[2][2] = f;
    w->viewport[3][0] = vpcx;
    w->viewport[3][1] = vpcy;
    w->viewport[3][2] = zA;
    w->npc2screen[0][0] = sx;
    w->npc2screen[0][1] = sy;
    w->npc2screen[0][2] = f;
    one = 1.0f;
    w->npc2screen[0][3] = one;
    w->npc2screen[1][0] = vpcx;
    w->npc2screen[1][1] = vpcy;
    w->npc2screen[1][2] = zA;
    w->npc2screen[1][3] = 0.0f;

    cpx = fw / cw;
    cpy = fh / ch;
    identity__5mat44Fv(w->clipport);
    w->clipport[0][0] = cpx;
    w->clipport[1][1] = cpy;
    w->clipport[2][2] = one;
    w->npc2clip[0] = cpx;
    w->npc2clip[1] = cpy;
    w->npc2clip[2] = one;
    one = 1.0f;
    w->npc2clip[3] = one;
    w->clip2npc[0] = one / cpx;
    w->clip2npc[1] = one / cpy;
    w->clip2npc[2] = one / w->clipport[2][2];
    w->clip2npc[3] = one;
    identity__5mat44Fv(w->clip_screen);
    w->clip_screen[0][0] = sx / cpx;
    w->clip_screen[1][1] = sy / cpy;
    w->clip_screen[2][2] = f / w->clipport[2][2];
    w->clip_screen[3][0] = vpcx;
    w->clip_screen[3][1] = vpcy;
    w->clip_screen[3][2] = zA;
    w->clip2screen[0][0] = sx / cpx;
    w->clip2screen[0][1] = sy / cpy;
    w->clip2screen[0][2] = f / w->clipport[2][2];
    w->clip2screen[0][3] = 1.0f;
    w->clip2screen[1][0] = vpcx;
    w->clip2screen[1][1] = vpcy;
    w->clip2screen[1][2] = zA;
    w->clip2screen[1][3] = 0.0f;
    mat44Mult(w->view_screen, w->viewport, w->projection);
    gVpScaleY = sy / (((448.0f * gScreenAspect) / gScreenAspect) * 0.5f);

    gProjD3D[3][3] = 0.0f;
    gProjD3D[3][1] = 0.0f;
    gProjD3D[3][0] = 0.0f;
    gProjD3D[2][1] = 0.0f;
    gProjD3D[2][0] = 0.0f;
    gProjD3D[1][3] = 0.0f;
    gProjD3D[1][2] = 0.0f;
    gProjD3D[1][0] = 0.0f;
    gProjD3D[0][3] = 0.0f;
    gProjD3D[0][2] = 0.0f;
    gProjD3D[0][1] = 0.0f;
    gProjD3D[0][0] = w->projection[0][0];
    gProjD3D[1][1] = -w->projection[1][1];
    gProjD3D[2][2] = w->far_z / (w->far_z - w->near_z);
    gProjD3D[3][2] = -1.0f;
    gProjD3D[2][3] = (w->near_z * w->far_z) / (w->far_z - w->near_z);
    w->proj_dirty = 0;
    ((PBSCREEN*) g->screen)->dirty = 0;
}

/* 0x800C8E4C: TODO (per-frame window packet setup) */
void pbWinSetup(void)
{
}

/* 0x800C92B8 handled below as setupMatrices */
static void setupMatrices(MTXPACKET2* p0, MTXPACKET* p1, MTXPACKET* p2);

/* 0x800C9448: positional light packets + camera pitch/yaw */
void pbCameraUpdate(void)
{
    PBWINGLOBALS* g = gWinGlobals;
    int ri;
    int pi;
    PBLIGHT* l;
    PBLIGHTPKT* pk;
    int count;
    f32 one;
    f32 fz, fx;
    u8 pad0[4]; /* unused, matches original frame */
    volatile float y;
    u8 pad1[0xC]; /* unused, matches original frame */

    count = 0;
    ri = 0;
    pi = 0;
    one = 1.0f;
    for (l = ((PBLIGHTBLOCK*) g->lights)->posHead; l != 0; l = l->next) {
        pk = (PBLIGHTPKT*) ((u8*) g->lights + pi + 0xDC);
        __as__4vec3FRC4vec3(pk->pos, l->pos);
        vec3Scale__FR4vec3R4vec3f(pk->color, l->color, 128.0f);
        pk->invR2 = one / (l->radius * l->radius);
        pk->intensity = l->intensity * ((PBLIGHTBLOCK*) g->lights)->ambient;
        *(f32*) ((u8*) g->lights + ri + 0x25C) = l->radius;
        count++;
        ri += 4;
        pi += 0x20;
    }
    ((PBLIGHTBLOCK*) g->lights)->posCount = count;

    fx = g->current->cam_look[0];
    fz = g->current->cam_look[2];
    fx = fx * fx;
    fz = fz * fz;
    one = fx + fz;
    if (one > 0.0f) {
        double gg = __frsqrte((double) one);
        gg = 0.5 * gg * (3.0 - gg * gg * one);
        gg = 0.5 * gg * (3.0 - gg * gg * one);
        gg = 0.5 * gg * (3.0 - gg * gg * one);
        y = (float) (one * (0.5 * gg * (3.0 - gg * gg * one)));
        one = y;
    }
    g->current->cam_pitch = atan(g->current->cam_look[1] / one);
    g->current->cam_yaw = atan2(g->current->cam_look[0], g->current->cam_look[2]);
    g->current->cam_pitch = (f32) (0.31830988614222805 * (double) g->current->cam_pitch);
    g->current->cam_yaw = (f32) (0.15915494307111402 * (double) g->current->cam_yaw);
    if (gPbDebugCam != 0) {
        gPbDebugCamTimer = gPbDebugCamTimer + 1;
        if (gPbDebugCamTimer > 0x14) {
            gPbDebugCamTimer = 0;
            pbDebugPrintf("___ cam pitch yaw ___ %4.2Lf  %4.2Lf\n",
                          (double) g->current->cam_pitch, (double) g->current->cam_yaw);
        }
    }
}

/* 0x800C9638 */
void pbCameraCalc(void)
{
    PBWINGLOBALS* g = gWinGlobals;
    PBWINDOW* w;

    sceSamp0Normalize(g->current->cam_look, g->current->cam_look);
    w = g->current;
    mat44LookAt(w->camera, w->cam_pos, w->cam_look, w->cam_up);
    w = g->current;
    sceSamp0RotCameraMatrix((f32*) gCameraMtx, w->cam_pos, w->cam_look, w->cam_up);
    mat44InvRigid(g->current->icamera, g->current->camera);
    g->current->cam_dirty = 0;
}

/* 0x800C96C0 */
void pbInitCamera(f32* pos, f32* look)
{
    PBWINGLOBALS* g = gWinGlobals;

    g->current->cam_pos[0] = pos[0];
    g->current->cam_pos[1] = pos[1];
    g->current->cam_pos[2] = pos[2];
    g->current->cam_look[0] = look[0];
    g->current->cam_look[1] = look[1];
    g->current->cam_look[2] = look[2];
    g->current->cam_dirty = 1;
}

/* 0x800C971C */
void MBWindowClip(f32 w, f32 h, f32 nearz, f32 farz)
{
    PBWINGLOBALS* g = gWinGlobals;

    if (w == 0.0) {
        w = 4095.0f;
    }
    if (h == 0.0) {
        h = 4095.0f;
    }
    g->current->near_z = nearz;
    g->current->far_z = farz;
    g->current->clip_width = w;
    g->current->clip_height = h;
    g->current->proj_dirty = 1;
}

/* 0x800C9770 */
void MBWindowProjection(f32 angle, f32 aspect)
{
    PBWINGLOBALS* g = gWinGlobals;

    if (aspect <= 0.0f) {
        aspect = 1.0f;
    }
    g->current->aspect = aspect;
    g->current->view_angle_horiz = angle;
    g->current->proj_dirty = 1;
}

/* 0x800C97A8 */
void MBWindowViewport(f32 l, f32 r, f32 t, f32 b)
{
    PBWINGLOBALS* g = gWinGlobals;

    g->current->left = l;
    g->current->right = r;
    g->current->top = t;
    g->current->bottom = b;
    g->current->proj_dirty = 1;
}

/* shared body of MBSetCurrentWindow / pbInitWindow */

/* 0x800C97DC: selects/initializes the current window; the viewport/
   projection/camera resets are written out inline (Midway paste style). */
void MBSetCurrentWindow(void)
{
    PBWINGLOBALS* g = gWinGlobals;

    if (g->list == 0) {
        g->list = &gDefaultWinList;
    }
    g->list->windows = gWindows;
    g->list->count = 1;
    g->list->unk8 = 0;
    g->list->unkC = 0;
    g = gWinGlobals;
    if (g->list->count <= 0) {
        ErrorPrintf("MBSetCurrentWindow: Bad window index %d\n");
    } else {
        g->current = g->list->windows;
        *gCurWindowMirror = g->list->windows;
    }
    g = gWinGlobals;
    if (g->current == 0) {
        g->current = g->list->windows;
    }
    /* MBWindowViewport(0,0,0,0) */
    g = gWinGlobals;
    g->current->left = 0.0f;
    g->current->right = 0.0f;
    g->current->top = 0.0f;
    g->current->bottom = 0.0f;
    g->current->proj_dirty = 1;
    /* MBWindowProjection(90, 1) */
    g = gWinGlobals;
    g->current->aspect = 1.0f;
    g->current->view_angle_horiz = 90.0f;
    g->current->proj_dirty = 1;
    MBWindowClip(2047.0f, 2047.0f, 1.0f, 65536.0f);
    /* camera defaults: pos 0, look -z, up +y */
    g = gWinGlobals;
    g->current->cam_pos[0] = 0.0f;
    g->current->cam_pos[1] = 0.0f;
    g->current->cam_pos[2] = 0.0f;
    g->current->cam_pos[3] = 0.0f;
    g->current->cam_look[0] = 0.0f;
    g->current->cam_look[1] = 0.0f;
    g->current->cam_look[2] = -1.0f;
    g->current->cam_look[3] = 0.0f;
    g->current->cam_up[0] = 0.0f;
    g->current->cam_up[1] = 1.0f;
    g->current->cam_up[2] = 0.0f;
    g->current->cam_up[3] = 1.0f;
}

/* 0x800C9978: unconditional variant of the window init/reset */
void pbInitWindow(void)
{
    PBWINGLOBALS* g = gWinGlobals;

    g->list = &gDefaultWinList;
    g->list->windows = gWindows;
    g->list->count = 1;
    g->list->unk8 = 0;
    g->list->unkC = 0;
    g = gWinGlobals;
    if (g->list->count <= 0) {
        ErrorPrintf("MBSetCurrentWindow: Bad window index %d\n");
    } else {
        g->current = g->list->windows;
        *gCurWindowMirror = g->list->windows;
    }
    g = gWinGlobals;
    if (g->current == 0) {
        g->current = g->list->windows;
    }
    /* MBWindowViewport(0,0,0,0) */
    g = gWinGlobals;
    g->current->left = 0.0f;
    g->current->right = 0.0f;
    g->current->top = 0.0f;
    g->current->bottom = 0.0f;
    g->current->proj_dirty = 1;
    /* MBWindowProjection(90, 1) */
    g = gWinGlobals;
    g->current->aspect = 1.0f;
    g->current->view_angle_horiz = 90.0f;
    g->current->proj_dirty = 1;
    MBWindowClip(2047.0f, 2047.0f, 1.0f, 65536.0f);
    /* camera defaults: pos 0, look -z, up +y */
    g = gWinGlobals;
    g->current->cam_pos[0] = 0.0f;
    g->current->cam_pos[1] = 0.0f;
    g->current->cam_pos[2] = 0.0f;
    g->current->cam_pos[3] = 0.0f;
    g->current->cam_look[0] = 0.0f;
    g->current->cam_look[1] = 0.0f;
    g->current->cam_look[2] = -1.0f;
    g->current->cam_look[3] = 0.0f;
    g->current->cam_up[0] = 0.0f;
    g->current->cam_up[1] = 1.0f;
    g->current->cam_up[2] = 0.0f;
    g->current->cam_up[3] = 1.0f;
}

/* 0x800C92B8 */
static void setupMatrices(MTXPACKET2* p0, MTXPACKET* p1, MTXPACKET* p2)
{
    PBWINGLOBALS* g = gWinGlobals;
    PBWINDOW* w;

    w = g->current;
    mat44Mult(w->world_npc, w->projection, w->camera);
    w = g->current;
    mat44Mult(w->world_screen, w->viewport, w->world_npc);
    w = g->current;
    mat44Mult(w->world_clip, w->clipport, w->world_npc);
    __as__4vec4FRC4vec4(p0->clip2npc, g->current->clip2npc);
    __as__4vec4FRC4vec4(p1->clip2npc, g->current->clip2npc);
    __as__4vec4FRC4vec4(p2->clip2npc, gClip2NpcDefault);
    __as__4vec4FRC4vec4(p0->npc2screen[0], g->current->npc2screen[0]);
    __as__4vec4FRC4vec4(p1->npc2screen[0], g->current->npc2screen[0]);
    __as__4vec4FRC4vec4(p2->npc2screen[0], g->current->npc2screen[0]);
    __as__4vec4FRC4vec4(p0->npc2screen[1], g->current->npc2screen[1]);
    __as__4vec4FRC4vec4(p1->npc2screen[1], g->current->npc2screen[1]);
    __as__4vec4FRC4vec4(p2->npc2screen[1], g->current->npc2screen[1]);
    __as__4vec4FRC4vec4(p0->clip2screen[0], g->current->clip2screen[0]);
    __as__4vec4FRC4vec4(p1->clip2screen[0], g->current->clip2screen[0]);
    __as__4vec4FRC4vec4(p2->clip2screen[0], g->current->npc2screen[0]);
    __as__4vec4FRC4vec4(p0->clip2screen[1], g->current->clip2screen[1]);
    __as__4vec4FRC4vec4(p1->clip2screen[1], g->current->clip2screen[1]);
    __as__4vec4FRC4vec4(p2->clip2screen[1], g->current->npc2screen[1]);
    __as__5mat44FRC5mat44(p0->mtx, g->current->world_clip);
    __as__5mat44FRC5mat44(p1->mtx, g->current->world_clip);
    __as__5mat44FRC5mat44(p2->mtx, g->current->world_npc);
}
