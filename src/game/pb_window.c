/* pb_window.c -- Midway's window/camera/projection layer (pb_window.obj on
 * Xbox, 26 fns; the GCN build keeps 15). Function names from shell3D.pdb;
 * PBWINDOW layout matches the Xbox PDB struct field-for-field.
 * WIP: debugScissor/pbProjCalc/pbWinSetup/pbCameraUpdate still stubs.
 */

#include "types.h"

typedef float f32;

void sceSamp0Normalize(f32* v0, f32* v1);
void sceSamp0RotCameraMatrix(f32* m, f32* p, f32* zd, f32* yd);
void sceSamp0CopyMatrix34(f32* m0, f32* m1);
void mat44LookAt(void* m, void* p, void* zd, void* yd);
void mat44InvRigid(void* d, void* s);
void mat44Mult(void* d, void* a, void* b);
double atan(double x);
double atan2(double y, double x);
double sin(double x);
double cos(double x);
void ErrorPrintf(const char* fmt, ...);
void __as__4vec4FRC4vec4(void* d, const void* s);   /* vec4::operator= */
void __as__5mat44FRC5mat44(void* d, const void* s); /* mat44::operator= */

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
extern PBWINDOW* gCurWindowMirror;  /* DAT_80343f10 (points at mirror slot) */
extern u8 gWinDefault[];            /* DAT_80345154 */
extern PBWINLIST gDefaultWinList;   /* DAT_802c9b78 */
extern PBWINDOW gWindows[];         /* DAT_802c93f8 */
extern f32 gCameraMtx[4][4];        /* DAT_802c9b88 */
extern f32 gUpVector[];             /* DAT_802c9b88's up? placeholder */

void pbCloseWindow(void);
void pbSetDefaultWindow(void);
void pbUpdateMatricies(void);
void debugScissor(int packet);
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
    if (gWinGlobals->unk44 != 0) {
        return;
    }
    gWinGlobals->unk44 = gWinDefault;
}

/* 0x800C82B0 */
void pbSetDefaultWindow(void)
{
    gWinGlobals->unk44 = gWinDefault;
}

/* 0x800C82C0 */
void pbUpdateMatricies(void)
{
    PBWINDOW* w;

    pbProjCalc();
    /* pbCameraCalc, written out (original pastes the body) */
    sceSamp0Normalize(gWinGlobals->current->cam_look, gWinGlobals->current->cam_look);
    w = gWinGlobals->current;
    mat44LookAt(w->camera, w->cam_pos, w->cam_look, w->cam_up);
    w = gWinGlobals->current;
    sceSamp0RotCameraMatrix((f32*) gCameraMtx, w->cam_pos, w->cam_look, w->cam_up);
    mat44InvRigid(gWinGlobals->current->icamera, gWinGlobals->current->camera);
    gWinGlobals->current->cam_dirty = 0;
    w = gWinGlobals->current;
    mat44Mult(w->world_npc, w->projection, w->camera);
    w = gWinGlobals->current;
    mat44Mult(w->world_screen, w->viewport, w->world_npc);
    w = gWinGlobals->current;
    mat44Mult(w->world_clip, w->clipport, w->world_npc);
}

/* 0x800C838C: TODO (debug scissor packet writer) */
void debugScissor(int packet)
{
}

/* 0x800C84CC: TODO (projection/viewport/clipport calculator) */
void pbProjCalc(void)
{
}

/* 0x800C8E4C: TODO (per-frame window packet setup) */
void pbWinSetup(void)
{
}

/* 0x800C92B8 handled below as setupMatrices */
static void setupMatrices(MTXPACKET2* p0, MTXPACKET* p1, MTXPACKET* p2);

/* 0x800C9448: TODO (pos lights + camera pitch/yaw update) */
void pbCameraUpdate(void)
{
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
    if (gWinGlobals->list->count > 0) {
        gWinGlobals->current = gWinGlobals->list->windows;
        gCurWindowMirror = gWinGlobals->list->windows;
    } else {
        ErrorPrintf("MBSetCurrentWindow: Bad window index %d\n");
    }
    if (gWinGlobals->current == 0) {
        gWinGlobals->current = gWinGlobals->list->windows;
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
    if (gWinGlobals->list->count > 0) {
        gWinGlobals->current = gWinGlobals->list->windows;
        gCurWindowMirror = gWinGlobals->list->windows;
    } else {
        ErrorPrintf("MBSetCurrentWindow: Bad window index %d\n");
    }
    if (gWinGlobals->current == 0) {
        gWinGlobals->current = gWinGlobals->list->windows;
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
