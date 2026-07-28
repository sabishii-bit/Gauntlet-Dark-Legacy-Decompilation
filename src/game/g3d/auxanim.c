#include "types.h"

/* Gauntlet Dark Legacy texture-mod / object-animation subsystem.
 * Real function names are from the Xbox build's AUXANIM.OBJ (shell3D.pdb).
 * These animate special (scrolling / cycling) textures and object-attached
 * texture animation lists. */

typedef struct TEXMOD {
    /* 0x00 */ short flag;
    /* 0x02 */ short scrollIdx;
    /* 0x04 */ char name1[0x20];
    /* 0x24 */ char name2[0x20];
    /* 0x44 */ int tex;
    /* 0x48 */ int src;
    /* 0x4C */ short frames;
    /* 0x4E */ short unk4e;
    /* 0x50 */ int rate;
    /* 0x54 */ int counter;
} TEXMOD; /* 0x58 */

typedef struct OANIM {
    /* 0x00 */ char name[0x20];
    /* 0x20 */ int tex;
    /* 0x24 */ short frames;
    /* 0x26 */ short start;
} OANIM; /* 0x28 */

typedef struct OANIMHDR {
    /* 0x00 */ int offset;
    /* 0x04 */ int count;
} OANIMHDR;

typedef struct SCROLL {
    /* 0x00 */ float x0;
    /* 0x04 */ float y0;
    /* 0x08 */ float x1;
    /* 0x0C */ float y1;
} SCROLL; /* 0x10 */

extern TEXMOD special_texmods[5];
extern int special_texmod_num;
extern int texmod_scrollidx;
extern int InfFrame;
extern SCROLL lbl_802C2E28[64];

extern void* MBRomTexPtr(int handle);
extern void MBSetRomTexture(int handle, void* p);
extern int MBOX_FindTexture_Sub(char* name, int* p, int a, int b, int c);
extern int MBOX_FindTexture(char* name, int* p);
extern void MBCopyTexture(int a, int b);
extern void MBClearTexscroll(void);
extern void MBTreeSetFlags(int a, int b, int c);
extern void MBTreeClearFlags(int a, int b, int c);
extern void MBSetObject(void* obj, int objid);
extern void MBTreeSetAlpha(int a, int b, int c);
extern void MBTreeSetAltTex(int a, int b, int c, int d);
extern void MBTreeSetUVScaleAdd(float a, float b, float c, float d, int handle, int flag);
extern int MBOX_ReallyFindObject(OANIM* node, int a, int b, int c);
extern void FatalError(char* msg, int code);
extern void ErrorPrintf(char* fmt, ...);

float CalcTexScroll(float t, float lo, float hi, int frame, float* out);

void DoSpecialTexmods(void)
{
    int i;
    TEXMOD* tm;
    void* p;
    u32 rate;
    int tex;
    int c;

    for (i = 0; i < special_texmod_num; i++) {
        tm = &special_texmods[i];
        rate = tm->rate;
        if ((int)rate <= 0 || InfFrame % rate == 0) {
            tex = tm->tex;
            if (tex >= 0) {
                p = MBRomTexPtr(tm->src + tm->counter);
                MBSetRomTexture(tex, p);
            }
            c = tm->counter + 1;
            tm->counter = c;
            if (c >= tm->frames) {
                tm->counter = 0;
            }
        }
    }
}

void DelSpecialTexmod(int idx)
{
    if (idx < 0) {
        return;
    }
    if (idx >= special_texmod_num) {
        return;
    }
    special_texmods[idx].tex = -1;
}

int AddSpecialTexmod(int texidx1, char* name1, int texidx2, char* name2, int frames, int rate)
{
    TEXMOD* tm;
    int slot;

    for (slot = 0; slot < special_texmod_num; slot++) {
        if (special_texmods[slot].tex < 0) {
            break;
        }
    }
    if (slot >= special_texmod_num) {
        if (special_texmod_num >= 5) {
            ErrorPrintf("> Max %d Special Texmods", special_texmod_num);
            return -1;
        }
        slot = special_texmod_num++;
    }
    tm = &special_texmods[slot];
    tm->tex = MBOX_FindTexture_Sub(name1, 0, texidx1, texidx1, 1);
    tm->src = MBOX_FindTexture_Sub(name2, 0, texidx2, texidx2, 1);
    tm->frames = (short)frames;
    tm->unk4e = 0;
    tm->rate = rate;
    tm->counter = 0;
    return slot;
}

void InitTexMod(TEXMOD* tm, int texidx)
{
    int tex;
    int mode;
    void* p;
    void* t;
    int scrIdx;

    if (tm->src == -4 || tm->src == -5) {
        tm->tex = 0;
    }
    if (tm->tex >= 0) {
        tex = (u32)tm->tex & 0xffff | texidx << 0x10;
    } else {
        tex = MBOX_FindTexture_Sub(tm->name1, 0, texidx, texidx, 1);
    }
    if (tex == 0) {
        ErrorPrintf("TEXMOD with 0 texidx, tex:%s srx:%s", tm->name1, tm->name2);
        tm->tex = -1;
    } else {
        tm->tex = tex;
        mode = tm->src;
        if (mode < 0) {
            if (mode < -3) {
                if (mode == -6) {
                    MBRomTexPtr(tex);
                    MBOX_FindTexture(tm->name2, 0);
                }
            } else if (mode == -1) {
                mode = MBOX_FindTexture(tm->name2, 0);
                MBCopyTexture(mode, tex);
                tm->src = mode;
            } else if (mode < -1 && tm->flag == -1) {
                t = MBRomTexPtr(tex);
                if ((*(u16*)((int)t + 2) & 0x40) != 0) {
                    tm->scrollIdx = *(short*)((int)t + 8);
                } else {
                    scrIdx = texmod_scrollidx;
                    if (scrIdx < 0x40) {
                        texmod_scrollidx = scrIdx + 1;
                        *(u16*)((int)t + 2) |= 0x40;
                        *(short*)((int)t + 8) = (short)scrIdx;
                        lbl_802C2E28[scrIdx].y0 = 0.0f;
                        lbl_802C2E28[scrIdx].y1 = 0.0f;
                        lbl_802C2E28[scrIdx].x0 = 1.0f;
                        lbl_802C2E28[scrIdx].x1 = 1.0f;
                        tm->scrollIdx = (short)scrIdx;
                    } else {
                        ErrorPrintf("> Max %d scrolling textures", scrIdx);
                    }
                }
            }
        } else {
            if ((mode & 0xffff0000) == 0) {
                tm->src = mode & 0xffff | texidx << 0x10;
            }
            p = MBRomTexPtr(tm->src);
            MBSetRomTexture(tex, p);
        }
    }
}

void DoTexModSub(TEXMOD* tm)
{
    short scr;
    int mode;
    u32 rate;
    float scale;
    float sign;
    float v;

    rate = tm->rate;
    if (tm->tex >= 0 && ((int)rate <= 0 || InfFrame % rate == 0)) {
        tm->counter++;
        mode = tm->frames;
        if (mode < 0) {
            mode = -mode;
        }
        if (tm->counter >= mode) {
            tm->counter = 0;
        }
        mode = tm->src;
        if (mode == -3) {
            scr = tm->scrollIdx;
            sign = (float)tm->frames;
            scale = 1.0f;
            if ((float)tm->frames < 0.0f) {
                sign = -sign;
                scale = -1.0f;
            }
            v = CalcTexScroll((float)(tm->counter - tm->unk4e), 0.0f, sign, tm->counter, (float*)0);
            lbl_802C2E28[scr].y1 = scale * v;
        } else {
            if (mode < -3) {
                if (mode == -6) {
                    return;
                }
            } else if (mode < -1) {
                scr = tm->scrollIdx;
                sign = (float)tm->frames;
                scale = 1.0f;
                if ((float)tm->frames < 0.0f) {
                    sign = -sign;
                    scale = -1.0f;
                }
                v = CalcTexScroll((float)(tm->counter - tm->unk4e), 0.0f, sign, tm->counter, (float*)0);
                lbl_802C2E28[scr].y0 = scale * v;
                return;
            }
            if (tm->tex >= 0) {
                void* p = MBRomTexPtr(mode + tm->counter);
                MBSetRomTexture(tm->tex, p);
            }
        }
    }
}

void DoTexModSeqSub(int ctx, TEXMOD* tm, int frame)
{
    int mode;
    int f;
    float lo;
    float out;
    float v;

    if (tm != 0) {
        mode = tm->src;
        if (mode == -4) {
            v = (float)(frame - tm->unk4e);
            lo = (float)(tm->frames);
            if (v <= 0.0f || lo <= 0.0) {
                lo = 0.0f;
            } else if ((float)(frame - tm->unk4e) < lo) {
                lo = (float)(frame - tm->unk4e) / lo;
            } else {
                lo = 1.0f;
            }
            mode = (int)((float)(1.0 - lo) * 255.0);
            MBTreeSetAlpha(ctx, mode, 1);
        } else {
            if (mode < -4) {
                if (mode == -6) {
                    return;
                }
                if (mode > -7) {
                    v = (float)(frame - tm->unk4e);
                    lo = (float)(tm->frames);
                    if (v <= 0.0f || lo <= 0.0) {
                        lo = 0.0f;
                    } else if ((float)(frame - tm->unk4e) < lo) {
                        lo = (float)(frame - tm->unk4e) / lo;
                    } else {
                        lo = 1.0f;
                    }
                    MBTreeSetAlpha(ctx, (int)(lo * 255.0), 1);
                    return;
                }
            } else if (mode == -2) {
                v = CalcTexScroll((float)(frame - tm->unk4e), (float)tm->rate, (float)tm->frames, frame, &out);
                MBTreeSetUVScaleAdd(out, v, 1.0f, 0.0f, ctx, 1);
                return;
            } else if (mode < -2) {
                v = CalcTexScroll((float)(frame - tm->unk4e), (float)tm->rate, (float)tm->frames, frame, &out);
                MBTreeSetUVScaleAdd(1.0f, 0.0f, out, v, ctx, 1);
                return;
            }
            f = frame - tm->unk4e;
            if (f < 0) {
                f = 0;
            } else {
                if (tm->rate > 0) {
                    f = f / tm->rate;
                }
                if (tm->frames <= f) {
                    f = tm->frames - 1;
                }
            }
            MBTreeSetAltTex(ctx, (u32)tm->tex & 0xffff, mode + f, 1);
        }
    }
}

float CalcTexScroll(float t, float lo, float hi, int frame, float* out)
{
    float result;
    int gt;
    float ret;
    float d4;
    float d5;
    double dret;
    int ih;

    gt = 0;
    if (lo > hi) {
        lo = hi;
        gt = 1;
    }
    if (0.0 == lo) {
        ih = (int)hi;
        ret = (float)((float)(frame % ih) / hi);
        result = 0.0f;
    } else {
        if (t <= 0.0) {
            ret = 0.0f;
            d5 = ret;
        } else if (t < lo) {
            d5 = 1.0f;
            d4 = (float)(hi * (float)(1.0 / lo));
            ret = (float)((float)(t * (float)(1.0 / lo)) * ((1.0 - d4) - 2.0 * -d4) + 2.0 * -d4);
        } else if (gt) {
            ret = 0.0f;
            d5 = 1.0f;
        } else if (t < hi) {
            d4 = (float)(t - lo) / (float)(hi - lo);
            d5 = (float)(hi * (float)(1.0 / lo));
            dret = 1.0 - d5;
            d5 = (float)(d4 * (d5 - 1.0) + 1.0);
            ret = (float)(d4 * -dret + dret);
        } else if (t < hi + lo) {
            ret = (float)((float)(1.0 / lo) * (float)(t - hi));
            d5 = (float)(hi * (float)(1.0 / lo));
        } else {
            ret = 1.0f;
            d5 = (float)(hi * (float)(1.0 / lo));
        }
        result = (float)(d5 - ret);
    }
    if (out != 0) {
        *out = result;
    }
    return ret;
}

void ResetTexmods(void)
{
    texmod_scrollidx = 0;
    special_texmod_num = 0;
    MBClearTexscroll();
}

void DoObjAnimation(OANIM* nodes, int ctx, int idx, int frame)
{
    OANIM* node;
    int start;
    int end;
    u32 tex;

    node = &nodes[idx];
    if (node->tex < 0) {
        MBTreeSetFlags(ctx, 1, 0);
    } else {
        MBTreeClearFlags(ctx, 1, 0);
        start = node->start;
        end = start + node->frames - 1;
        if (frame < start || frame > end) {
            if (start == end) {
                tex = node->tex;
            } else {
                tex = 0;
            }
        } else {
            tex = (node->tex + frame) - start;
        }
        MBSetObject((void*)ctx, tex);
    }
}

void InitOAnimList(OANIMHDR* hdr, int arg)
{
    u16 h;
    u32 v;
    u8 r[4];
    u8* s;
    int i;
    int off;
    char* p;

    if (hdr == 0) {
        FatalError("Bad header passed in to InitOAnimList.", 0x804060);
    }
    p = (char*)((int)hdr + hdr->offset);
    off = 0;
    for (i = 0; i < hdr->count; i++) {
        v = *(u32*)(p + off + 0x20);
        s = (u8*)&v;
        r[0] = s[3];
        r[1] = s[2];
        r[2] = s[1];
        r[3] = s[0];
        *(u32*)(p + off + 0x20) = *(u32*)r;
        h = *(u16*)(p + off + 0x24);
        s = (u8*)&h;
        *(u16*)(p + off + 0x24) = (s[1] << 8) | s[0];
        h = *(u16*)(p + off + 0x26);
        s = (u8*)&h;
        *(u16*)(p + off + 0x26) = (s[1] << 8) | s[0];
        off += 0x28;
    }
    i = 0;
    while (i < hdr->count) {
        if (*p != '\0') {
            ((OANIM*)p)->tex = MBOX_ReallyFindObject((OANIM*)p, arg, arg, -1);
            if (((OANIM*)p)->tex == -1) {
                ErrorPrintf("InitOAnimList: Unable to find %s (%d)", p, i);
            }
        } else {
            ((OANIM*)p)->tex = -1;
        }
        i++;
        p += 0x28;
    }
}
