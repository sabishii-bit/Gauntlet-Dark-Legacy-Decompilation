/* pb_window.c -- Midway's window/camera/projection layer (pb_window.obj on
 * Xbox, 26 fns; the GCN build keeps 15). Function names from shell3D.pdb;
 * PBWINDOW layout matches the Xbox PDB struct field-for-field.
 * WIP: debugScissor/pbProjCalc/pbWinSetup/pbCameraUpdate still stubs.
 */

#include "types.h"
#include "game/pbwindow.h"

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
void bulletproof_printf(const char* fmt, ...);



/* PBWINDOW / FIX115 now live in include/game/pbwindow.h */

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

extern f32 gVpScaleY;        /* gVpScaleY */
extern int lbl_80345158;
extern int lbl_8034515C;
typedef struct PBWINDEBUG {
    s32 mode;     /* 0x00 */
    s32 unk04;    /* 0x04 */
    s32 clipOff;  /* 0x08 */
    f32 zoom;     /* 0x0C */
    f32 clipzoom; /* 0x10 */
    u8 unk14[0xC];
    f32 zoomx;    /* 0x20 */
    f32 zoomy;    /* 0x24 */
    f32 zoomcx;   /* 0x28 */
    f32 zoomcy;   /* 0x2C */
} PBWINDEBUG;
extern PBWINDEBUG* gWinDebug; /* gWinDebug (SDA pointer) */
extern f32 gScreenData[];     /* gScreenData ([3] = aspect @8025EE70) */
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

/* PBWINLIST / PBWINGLOBALS / PBWINSTATIC and the gWinGlobals / gCurWindowMirror
 * / gDefaultWinList / gWindows / gCameraMtx externs now live in
 * include/game/pbwindow.h */
extern u32 gWinDefault;             /* DAT_80345154 (SDA-addressed) */
extern f32 gUpVector[];             /* DAT_802c9b88's up? placeholder */

void pbCloseWindow(void);
void pbSetDefaultWindow(void);
void pbUpdateMatricies(void);
static void debugScissor(u32* p);
void pbProjCalc(void);
void pbWinSetup(void);
void pbCameraUpdate();
static void setupMatrices(MTXPACKET2* p0, MTXPACKET* p1, MTXPACKET* p2);
void pbResetDORegs(void);
u32* fn_800C3680(void);
extern int lbl_80345028;     /* use-allocated-packet flag */
extern int lbl_80343F38;     /* cleared at end of pbWinSetup */
extern int lbl_80344DAC;     /* packet buffer end (sbss) */
extern int lbl_80344DB0;     /* packet buffer base (sbss) */
extern f32 lbl_801284D8[];   /* default dir-light node (vec at +0x10) */
void mat44InvBasis__FR5mat44R4vec4R4vec4R4vec4(void* m, void* a, void* b, void* c);
void mat44SetRows__FR5mat44R4vec4R4vec4R4vec4R4vec4(void* m, void* a, void* b, void* c, void* d);
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
static void debugScissor(u32* p)
{
    PBWINGLOBALS* g = gWinGlobals;
    int x0, x1, y0, y1;
    f32 s1, s2;
    f32 fx0, fx1, fy0, fy1;

    if (gWinDebug->mode != 0) {
        if (gWinDebug->clipOff != 0) {
            x0 = 0;
            y0 = 0;
            x1 = ((PBSCREEN*) g->screen)->w;
            y1 = ((PBSCREEN*) g->screen)->h;
        } else {
            if (gWinDebug->zoom) {
                if (gWinDebug->mode != 0) {
                    s1 = (f32) (0.5 * (1.0 - (double) gWinDebug->zoom));
                    if (0.0 != (double) gWinDebug->zoom) {
                        f32 fw1, fw2, fh1, fh2;
                        s2 = 1.0f - s1;
                        fw1 = (f32) ((PBSCREEN*) g->screen)->w;
                        fw2 = (f32) ((PBSCREEN*) g->screen)->w;
                        fh1 = (f32) ((PBSCREEN*) g->screen)->h;
                        fh2 = (f32) ((PBSCREEN*) g->screen)->h;
                        fx0 = fw1 * s1;
                        fx1 = fw2 * s2;
                        fy0 = fh1 * s1;
                        fy1 = fh2 * s2;
                    }
                }
                x0 = (s32) fx0;
                x1 = (s32) fx1;
                y0 = (s32) fy0;
                y1 = (s32) fy1;
            }
        }
        p[0x70] = x0 | (x1 << 16);
        p[0x71] = y0 | (y1 << 16);
        p[0x74] = x0 | (x1 << 16);
        p[0x75] = y0 | (y1 << 16);
    }
}

#pragma dont_inline off
/* debug zoom: shrinks the port rect / clip size (inlined into pbProjCalc) */
static void debugZoomAdjust(volatile f32* l, volatile f32* r, volatile f32* t,
                            volatile f32* b, volatile f32* w, volatile f32* h)
{
    PBWINGLOBALS* g = gWinGlobals;
    PBWINDEBUG* d = gWinDebug;
    if (d->mode != 0) {
        f32 sc = (f32) (0.5 * (1.0 - (double) d->zoom));
        if (0.0 != (double) d->zoom) {
            *l = (f32) ((PBSCREEN*) g->screen)->w * sc;
            *r = (f32) ((PBSCREEN*) g->screen)->w * (1.0f - sc);
            *t = (f32) ((PBSCREEN*) g->screen)->h * sc;
            *b = (f32) ((PBSCREEN*) g->screen)->h * (1.0f - sc);
        }
        {
            f32* pz = &d->clipzoom;
            if (0.0 != (double) *pz) {
                *w = (f32) ((PBSCREEN*) g->screen)->w * *pz;
                *h = (f32) ((PBSCREEN*) g->screen)->h * *pz;
            }
        }
    }
}

/* camera matrices for the current window */
static void calcMatrices(PBWINSTATIC* ws)
{
    PBWINGLOBALS* g = gWinGlobals;

    sceSamp0Normalize(g->current->cam_look, g->current->cam_look);
    mat44LookAt(g->current->camera, g->current->cam_pos, g->current->cam_look, g->current->cam_up);
    sceSamp0RotCameraMatrix((f32*) ws->camera, g->current->cam_pos, g->current->cam_look, g->current->cam_up);
    mat44InvRigid(g->current->icamera, g->current->camera);
    g->current->cam_dirty = 0;
}

/* the two small quad packets + hand-off to setupMatrices */
static inline void setupClipMtxPkt(PBWINSTATIC* ws, u32* p)
{
    ws->hdr0[0] = 0x3000000A;
    ws->hdr0[1] = (u32) ws->quad0;
    ws->hdr0[2] = 0x01000404;
    ws->hdr0[3] = 0x10000000;
    ws->quad0[0] = 0;
    ws->quad0[1] = 0;
    ws->quad0[2] = 0;
    ws->quad0[3] = 0x6C090000;
    ws->hdr1[0] = 0x3000000A;
    ws->hdr1[1] = (u32) ws->quad1;
    ws->hdr1[2] = 0x01000404;
    ws->hdr1[3] = 0x10000000;
    ws->quad1[0] = 0;
    ws->quad1[1] = 0;
    ws->quad1[2] = 0;
    ws->quad1[3] = 0x6C090000;
    setupMatrices((MTXPACKET2*) p, (MTXPACKET*) ws->quad0, (MTXPACKET*) ws->quad1);
}

/* pbProjCalc keeps every float local in one scratch array (PS2-era layout);
   the array is address-taken (debugZoomAdjust), which is what forces the
   reload-per-statement codegen of the original. */
#define fov    vp[0]
#define ang    vp[1]
#define sn     vp[2]
#define cn     vp[3]
#define tanh2  vp[4]
#define invt   vp[5]
#define at     vp[6]
#define sn2    vp[7]
#define cn2    vp[8]
#define ratio2 vp[9]
#define invr2  vp[10]
#define proj00 vp[11]
#define proj11 vp[12]
#define projZB vp[13]
#define projZA vp[14]
#define vpsx   vp[15]
#define vpsy   vp[16]
#define zC     vp[17]
#define vpx    vp[18]
#define vpy    vp[19]
#define zD     vp[20]
#define cs00   vp[21]
#define cs11   vp[22]
#define cs22   vp[23]
#define cs30   vp[24]
#define cs31   vp[25]
#define cs32   vp[26]
#define cpx    vp[27]
#define cpy    vp[28]
#define cpz    vp[29]
#define sw     vp[33]
#define sh     vp[34]
#define pl     vp[35]
#define pr     vp[36]
#define pt     vp[37]
#define pb     vp[38]
#define fw     vp[39]
#define fh     vp[40]
#define cw     vp[41]
#define ch     vp[42]
#define vpcx   vp[43]
#define vpcy   vp[44]
#define ratio  vp[45]
#define zA     vp[46]
#define zB     vp[47]
#define fz     vp[48]
#define nz     vp[49]

/* 0x800C84CC: rebuilds scissor, projection, viewport, clipport,
   clip_screen and the screen-space quads from the window parameters. */
void pbProjCalc(void)
{
    PBWINGLOBALS* g = gWinGlobals;
    PBWINSTATIC* ws = (PBWINSTATIC*) gWindows;
    f32 v[50];
    u8 argpad[12]; /* three more f32 locals in the original (never touched) */
    f32* vp = v;
    f32 f, t;

    sw = (f32) ((PBSCREEN*) g->screen)->w;
    sh = (f32) ((PBSCREEN*) g->screen)->h;

    f = 0.0f;
    if (g->current->left < 0.0f) {
        f = sw;
    }
    pl = g->current->left + f;

    t = g->current->right;
    if (t < 0.0f) {
        f = 0.0f;
    } else {
        f = sw;
    }
    pr = f - t;

    t = g->current->top;
    f = 0.0f;
    if (t < 0.0f) {
        f = sh;
    }
    pt = t + f;

    t = g->current->bottom;
    if (t < 0.0f) {
        f = 0.0f;
    } else {
        f = sh;
    }
    pb = f - t;

    fov = g->current->view_angle_horiz;
    cw = g->current->clip_width;
    ch = g->current->clip_height;
    zA = (f32) ((PBSCREEN*) g->screen)->w2;
    zB = (f32) ((PBSCREEN*) g->screen)->h2;
    fz = g->current->far_z;
    nz = g->current->near_z;

    g->current->scissor[0].i = (s32) pl;
    g->current->scissor[1].i = (s32) pr;
    g->current->scissor[2].i = (s32) pt;
    g->current->scissor[3].i = (s32) pb;

    if (gWinDebug->mode != 0) {
        debugZoomAdjust(&pl, &pr, &pt, &pb, &cw, &ch);
    }

    fw = pr - pl;
    fh = pb - pt;
    vpcx = 0.5f * ((pl + pr) - sw);
    vpcy = 0.5f * ((pt + pb) - sh);
    vpcx = vpcx + ((PBSCREEN*) g->screen)->xoff;
    vpcy = vpcy + ((PBSCREEN*) g->screen)->yoff;
    ratio = (g->current->aspect * fw) / fh;
    if ((((PBSCREEN*) g->screen)->flags & 2) != 0) {
        ratio = ratio * 2.0f;
    }

    ang = (f32) (0.008726646261111111 * (double) fov);
    sn = sin(ang);
    cn = cos(ang);
    tanh2 = sn / cn;
    ratio2 = tanh2 / ratio;
    at = atan(ratio2);
    sn2 = sin(at);
    cn2 = cos(at);
    invt = 1.0f / tanh2;
    invr2 = 1.0f / ratio2;
    proj00 = invt;
    proj11 = invr2;
    f = nz;
    t = fz;
    projZB = ((-2.0f * f) * t) / (t - f);
    t = fz;
    f = nz;
    projZA = (f + t) / (t - f);

    g->current->hva_sin_x = sn;
    g->current->hva_cos_x = cn;
    g->current->hva_sin_y = sn2;
    g->current->hva_cos_y = cn2;
    identity__5mat44Fv(g->current->projection);
    g->current->projection[0][0] = proj00;
    g->current->projection[1][1] = -proj11;
    g->current->projection[2][2] = projZA;
    g->current->projection[2][3] = 1.0f;
    g->current->projection[3][2] = projZB;
    g->current->projection[3][3] = 0.0f;

    vpsx = 0.5f * fw;
    vpsy = 0.5f * fh;
    zC = 0.5f * (zA - zB);
    vpx = vpcx;
    vpy = vpcy;
    zD = 0.5f * (zA + zB);

    if (gWinDebug->mode != 0 && gWinDebug->mode == 2) {
        f32 t1 = vpsx;
        f32 t2 = vpsy;
        vpsx = t1 * gWinDebug->zoomx;
        vpsy = vpsy * gWinDebug->zoomy;
        {
            f32 ex = (gWinDebug->zoomx - 1.0f) - 2.0f * gWinDebug->zoomcx;
            f32 ey = (gWinDebug->zoomy - 1.0f) - 2.0f * gWinDebug->zoomcy;
            vpx = vpx + t1 * ex;
            vpy = vpy + t2 * ey;
        }
    }

    identity__5mat44Fv(g->current->viewport);
    g->current->viewport[0][0] = vpsx;
    g->current->viewport[1][1] = vpsy;
    g->current->viewport[2][2] = zC;
    g->current->viewport[3][0] = vpx;
    g->current->viewport[3][1] = vpy;
    g->current->viewport[3][2] = zD;
    g->current->npc2screen[0][0] = vpsx;
    g->current->npc2screen[0][1] = vpsy;
    g->current->npc2screen[0][2] = zC;
    g->current->npc2screen[0][3] = 1.0f;
    g->current->npc2screen[1][0] = vpx;
    g->current->npc2screen[1][1] = vpy;
    g->current->npc2screen[1][2] = zD;
    g->current->npc2screen[1][3] = 0.0f;

    cpx = fw / cw;
    cpy = fh / ch;
    cpz = 1.0f;
    identity__5mat44Fv(g->current->clipport);
    g->current->clipport[0][0] = cpx;
    g->current->clipport[1][1] = cpy;
    g->current->clipport[2][2] = cpz;
    g->current->npc2clip[0] = cpx;
    g->current->npc2clip[1] = cpy;
    g->current->npc2clip[2] = cpz;
    g->current->npc2clip[3] = 1.0f;
    g->current->clip2npc[0] = 1.0f / cpx;
    g->current->clip2npc[1] = 1.0f / cpy;
    g->current->clip2npc[2] = 1.0f / cpz;
    g->current->clip2npc[3] = 1.0f;

    cs00 = vpsx / cpx;
    cs11 = vpsy / cpy;
    cs22 = zC / cpz;
    cs30 = vpx;
    cs31 = vpy;
    cs32 = zD;
    identity__5mat44Fv(g->current->clip_screen);
    g->current->clip_screen[0][0] = cs00;
    g->current->clip_screen[1][1] = cs11;
    g->current->clip_screen[2][2] = cs22;
    g->current->clip_screen[3][0] = cs30;
    g->current->clip_screen[3][1] = cs31;
    g->current->clip_screen[3][2] = cs32;
    g->current->clip2screen[0][0] = cs00;
    g->current->clip2screen[0][1] = cs11;
    g->current->clip2screen[0][2] = cs22;
    g->current->clip2screen[0][3] = 1.0f;
    g->current->clip2screen[1][0] = cs30;
    g->current->clip2screen[1][1] = cs31;
    g->current->clip2screen[1][2] = cs32;
    g->current->clip2screen[1][3] = 0.0f;

    mat44Mult(g->current->view_screen, g->current->viewport, g->current->projection);

    gVpScaleY = vpsy / (((448.0f * gScreenData[3]) / gScreenData[3]) * 0.5f);

    ws->projD3D[3][3] = 0.0f;
    ws->projD3D[3][1] = 0.0f;
    ws->projD3D[3][0] = 0.0f;
    ws->projD3D[2][1] = 0.0f;
    ws->projD3D[2][0] = 0.0f;
    ws->projD3D[1][3] = 0.0f;
    ws->projD3D[1][2] = 0.0f;
    ws->projD3D[1][0] = 0.0f;
    ws->projD3D[0][3] = 0.0f;
    ws->projD3D[0][2] = 0.0f;
    ws->projD3D[0][1] = 0.0f;
    ws->projD3D[0][0] = g->current->projection[0][0];
    ws->projD3D[1][1] = -g->current->projection[1][1];
    ws->projD3D[2][2] = g->current->far_z / (g->current->far_z - g->current->near_z);
    ws->projD3D[3][2] = -1.0f;
    ws->projD3D[2][3] = (g->current->near_z * g->current->far_z) / (g->current->far_z - g->current->near_z);
    g->current->proj_dirty = 0;
    ((PBSCREEN*) g->screen)->dirty = 0;
}

#undef fov
#undef ang
#undef sn
#undef cn
#undef tanh2
#undef invt
#undef at
#undef sn2
#undef cn2
#undef ratio2
#undef invr2
#undef proj00
#undef proj11
#undef projZB
#undef projZA
#undef vpsx
#undef vpsy
#undef zC
#undef vpx
#undef vpy
#undef zD
#undef cs00
#undef cs11
#undef cs22
#undef cs30
#undef cs31
#undef cs32
#undef cpx
#undef cpy
#undef cpz
#undef sw
#undef sh
#undef pl
#undef pr
#undef pt
#undef pb
#undef fw
#undef fh
#undef cw
#undef ch
#undef vpcx
#undef vpcy
#undef ratio
#undef zA
#undef zB
#undef fz
#undef nz

/* 0x800C8E4C: builds the frame's matrix packet (GIF-tag heritage),
   scissor words, directional light matrices, camera matrices and the two
   small quad packets, then hands everything to setupMatrices. */
void pbWinSetup(void)
{
    PBWINGLOBALS* g = gWinGlobals;
    PBWINSTATIC* ws = (PBWINSTATIC*) gWindows;
    u32* p;
    void* fb = g->framebuf;
    f32* dirs[3];
    int i;
    int n;
    int x0, x1, y0, y1;
    int w, h;
    PBLIGHTBLOCK* lb;

    pbResetDORegs();
    if (lbl_80345028 != 0) {
        p = fn_800C3680();
    } else {
        p = ws->pkt;
    }

    p[0] = 0x7000001D;
    p[1] = 0;
    p[2] = 0x11000000;
    p[3] = 0x03000000;
    p[4] = 0x02000000;
    p[5] = 0x01000404;
    p[6] = 0;
    p[7] = 0x6C110000;

    p[0x4C] = 0x64010000;
    p[0x4D] = ((u32*) fb)[8];
    p[0x4E] = (&lbl_80344DAC - &lbl_80344DB0) >> 3;
    p[0x4F] = 0x17000000;
    p[0x50] = 0;
    p[0x51] = 0;
    p[0x52] = 0;
    p[0x53] = 0x50000009;
    p[0x54] = 0x8008;
    p[0x55] = 0x10000000;
    p[0x57] = 0xE;
    p[0x56] = 0;
    p[0x59] = 0;
    p[0x58] = 0;
    p[0x5D] = 0xFFFFFF;
    p[0x5C] = 0;
    p[0x60] = 0x71603524;
    p[0x61] = 0x60712435;
    p[0x65] = 0;
    p[0x64] = 0;
    p[0x69] = 0;
    p[0x68] = 0;
    p[0x6D] = 0;
    p[0x6C] = 0;
    p[0x5B] = 0x22;
    p[0x5A] = 0;
    p[0x5F] = 0x3D;
    p[0x5E] = 0;
    p[0x63] = 0x44;
    p[0x62] = 0;
    p[0x67] = 0x49;
    p[0x66] = 0;
    p[0x6B] = 0x4A;
    p[0x6A] = 0;
    p[0x6F] = 0x4B;
    p[0x6E] = 0;
    p[0x73] = 0x40;
    p[0x72] = 0;
    p[0x77] = 0x41;
    p[0x76] = 0;

    h = ((PBSCREEN*) g->screen)->h;
    w = ((PBSCREEN*) g->screen)->w;
    x0 = (s32) g->current->left;
    x1 = (s32) ((f32) w - g->current->right);
    y0 = (s32) g->current->top;
    y1 = (s32) ((f32) h - g->current->bottom);
    if ((s32) g->current->left < 0) {
        x0 = 0;
    }
    if (x0 > w) {
        x0 = w;
    }
    if (x1 < x0) {
        x1 = x0;
    }
    if (x1 > w) {
        x1 = w;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (y0 > h) {
        y0 = h;
    }
    if (y1 < y0) {
        y1 = y0;
    }
    if (y1 > h) {
        y1 = h;
    }
    p[0x70] = x0 | (x1 << 16);
    p[0x71] = y0 | (y1 << 16);
    p[0x74] = x0 | (x1 << 16);
    p[0x75] = y0 | (y1 << 16);

    if (gWinDebug->mode != 0) {
        debugScissor(p);
    }

    pbProjCalc();

    g = gWinGlobals;
    lb = (PBLIGHTBLOCK*) g->lights;
    i = 0;
    n = lb->dirCount;
    for (; i < n; i++) {
        dirs[i] = (f32*) ((u8*) lb + 0xBC + i * 0x20);
    }
    for (; i < 3; i++) {
        dirs[i] = lbl_801284D8;
    }
    mat44InvBasis__FR5mat44R4vec4R4vec4R4vec4(p + 0x2C, dirs[0] + 4, dirs[1] + 4, dirs[2] + 4);
    mat44SetRows__FR5mat44R4vec4R4vec4R4vec4R4vec4(p + 0x3C, dirs[0], dirs[1], dirs[2],
                                                   ((PBLIGHTBLOCK*) g->lights)->ambientRow);
    sceSamp0CopyMatrix34((f32*) ws->lightRows, (f32*) (p + 0x2C));
    __as__5mat44FRC5mat44(ws->lightInv, p + 0x3C);

    pbCameraUpdate(p);
    calcMatrices(ws);
    setupClipMtxPkt(ws, p);
    lbl_80343F38 = 0;
}
#pragma dont_inline on


/* 0x800C9448: positional light packets + camera pitch/yaw */
void pbCameraUpdate()
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
    if (lbl_80345158 != 0) {
        lbl_8034515C = lbl_8034515C + 1;
        if (lbl_8034515C > 0x14) {
            lbl_8034515C = 0;
            bulletproof_printf("___ cam pitch yaw ___ %4.2Lf  %4.2Lf\n",
                               (double) g->current->cam_pitch,
                               (double) g->current->cam_yaw);
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
