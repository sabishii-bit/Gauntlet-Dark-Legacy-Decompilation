#include "types.h"

/* GDL diagnostic / debug HUD overlay (GCN PB_DIAG.OBJ region,
 * 0x800A573C-0x800A87C8). The Xbox shell3D PDB stubs this module out
 * (pbDiagMenu/pbDiagMenuDraw are 1-byte no-ops there), so most GCN names
 * are provisional/descriptive. This module draws the audio/frame/object/
 * texture diagnostic screens and a debug menu, driven by pad input via the
 * `buttons` control-state block. pbDiagCtrlInt / pbDiagCtrlFloat adjust a
 * menu value from D-pad/stick input with key-repeat + wrap.
 *
 * Status: NonMatching. 6/15 functions reconstructed:
 *   pbDiagCtrlInt   - MATCHING (byte-exact)
 *   pbDiagCtrlFloat - MATCHING (byte-exact)
 *   pbResetDiag     - equivalent, insn-count exact; volatile-reg allocation
 *                     (buttons r5-vs-r6) differs; parked (regalloc-only).
 *   pbInitDiag      - equivalent, insn-count exact; gDiagData hoist +
 *                     sdata2 pool ordering differ; parked (regalloc/pool).
 *   pbDiagDrawMenuA - equivalent, insn-count exact; saved-reg numbering
 *                     (line/off/colorbase permute) differs; parked.
 *   pbDiagDrawMenuB - equivalent, insn-count exact; same regalloc residual.
 * The 9 larger draw functions (audio/soundrow/info/texture/texlabel/object/
 * colorbars/strrow/menu) are not yet reconstructed. */

typedef struct WinGlobals {
    u8 _pad0[0x30];
    u32* f30;
} WinGlobals;

typedef struct DiagList {
    /* 0x00 */ char* strs;      /* base of fixed-stride string rows */
    /* 0x04 */ u8 _pad4[0x10];
    /* 0x14 */ s32 count;       /* number of entries */
} DiagList;

typedef struct DiagMenu {
    /* 0x00 */ s16 count;
    /* 0x02 */ u8 _pad2[2];
    /* 0x04 */ char* strs;      /* base of 36-byte string rows */
} DiagMenu;

/* pad / control state block (PB_DIAG `buttons`, 0x8028C388) */
extern u32 buttons[];

/* window globals pointer (owned by pb_window, gWinGlobals) */
extern WinGlobals* gWinGlobals;

/* diag config block (this TU's .data, gDiagData, 0x170 bytes) */
extern f32 gDiagData[];

/* menu-B highlight table + current index */
extern s32 gDiagMenuList[];     /* gDiagMenuList */
extern s32 gDiagMenuIdx;        /* gDiagMenuIdx */

/* diag state (sdata/sbss, owned by this TU) */
extern s32 gDiagTurbo;          /* gDiagTurbo */
extern void* gDiagWhiteObj;     /* gDiagWhiteObj */
extern s32 gDiag_D4;            /* gDiag_D4 */
extern s32 gDiag_D8;            /* gDiag_D8 */
extern s32 gDiag_DC;            /* gDiag_DC */
extern s32 gDiagListSel;        /* gDiagListSel (menu-A highlight index) */
extern s32 gDiag_E0;            /* gDiag_E0 */
extern s32 gDiag_E4;            /* gDiag_E4 */
extern s32 gDiagRepeatDelay;    /* gDiagRepeatDelay */
extern s32 gDiagRepeatRate;     /* gDiagRepeatRate */
extern s32 gDiag_F0;            /* gDiag_F0 */
extern s32 gDiag_F4;            /* gDiag_F4 */
extern u32 lbl_80240FC0[];      /* pb frame/screen state block */
extern f32 gDiag_E68;           /* color-bar animation speed scale */
extern f64 lbl_803486D0;        /* E68 ramp step */
extern f32 lbl_803486B4;        /* E68 reset value */
extern f32 lbl_80348670;        /* zero */
extern f64 lbl_80348710;        /* hue speed factor */
extern f64 lbl_80348718;        /* hue speed cap test */
extern f32 lbl_80348720;        /* hue speed cap */
extern f32 lbl_80348724;        /* hue ctrl min */
extern f32 lbl_80348728;        /* hue ctrl max */
extern f64 lbl_80348730;        /* wrap high bound */
extern f64 lbl_80348738;        /* wrap full circle */
extern f64 lbl_80348740;        /* wrap low bound */
extern f64 lbl_80348748;        /* sat speed factor */
extern f32 lbl_80348750;        /* sat/val ctrl min */
extern f32 lbl_80348754;        /* sat/val ctrl max */
extern f64 lbl_80348688;        /* val speed factor */
f32 pbDiagCtrlFloat(s32 axis, s32 pad, f32 val, f32 inc, f32 min, f32 max);

/* string-row list: count at +0x4C, row base at +0x5C, 24-byte rows */
typedef struct DiagStrRows {
    u8 _pad00[76];
    u32 count;              /* 0x4C */
    u8 _pad50[12];
    char* strs;             /* 0x5C */
} DiagStrRows;

typedef struct DiagRow {       /* 24-byte string row */
    char name[22];
    s16  val;
} DiagRow;

extern u32 gDiag_FC;
extern s32 gDiag_D0C;
extern s32 gDiag_F0;
extern s32 gDiag_DEC;
extern s32 gDiag_E6C;
extern s32 gDiag_E70;
extern u64 gControllerButtons;  /* low half = sFlags@803445CC */
extern f32 gIdentityMatrix[];
extern f32 lbl_803486B0;
extern f32 lbl_803486B8;
extern char lbl_80348700[8];
extern char lbl_80348708[8];
extern char lbl_803486F0[8];
extern void MBSetAmbient(int idx, f32 v);
extern void MBAddLight(int a, int b, f32 v);
extern void MBWindowViewport(f32 a, f32 b, f32 c, f32 d);
extern s32 MBOX_NewObject(char* name, f32* mtx, int a, int b);
extern void* MBOX_FindTexture_Err(char* name, int a, int b);
extern s32 MBCreateBlit(s32 a, void* tex, int b, int c, int d, int e);
extern void mbBlitCvtCoord(s32 blit, f32 v);
extern void MBBlitSetColor(s32 blit, s32 color);
extern void MBSetObject(s32 obj, void* data);
extern void MBTreeSetAltTex(s32 obj, int idx, void* tex, int a);
extern void CreatePYRMatrix(s32 obj, void* pyr);
extern u32 fn_800C02F4(u32 color);
extern void fn_800C01C0(int x, int y, const char* fmt, ...);
extern u8* MBOX_ReallyFindObject(void* entry, int a, int b, int c);
void pbDiagDrawColorBars(void);
void pbDiagDrawStrRow(DiagStrRows* p);
s32 pbDiagCtrlInt(s32 axis, s32 pad, s32 val, s32 inc, s32 min, s32 max);

/* object record view (also the DiagStrRows passed to pbDiagDrawStrRow) */
typedef struct DiagObjView {
    u8   _pad00[32];
    char name[44];          /* 0x20 */
    s32  count;             /* 0x4C */
    u8   _pad50[12];
    char* rows;             /* 0x5C: 24-byte rows */
} DiagObjView;


extern u32 gDiag_FC;
extern s32 gDiag_D0C;            /* gDiag_FC */
extern u32 gDiag_D00;           /* gDiag_D00 */
extern u32 gDiag_D04;           /* gDiag_D04 */
extern s32 gDiag_D08;           /* gDiag_D08 */

/* --- text / draw primitives + subsystem init (other TUs) --- */
extern void fn_800C008C(u32 rgba, int x, int y, const char* fmt, ...);
extern void AudioStopSelect(void);
extern void AudioSelectReset(void);
extern void fn_800C0310(void);
extern void MBTreeInit(void);
extern void DebugCamInit(void); /* newcam.c: init the pb-diag debug camera */
extern void* MBOX_FindTexture(const char* name, int arg);
extern int strlen(const char* s);

void pbResetDiag(void);

/* audio bank table (owned by the sound driver): counts + column arrays */
typedef struct AudioBankTable {
    s32 count;              /* 0x00: number of banks */
    u8  _pad04[8];
    u8* banks;              /* 0x0C: bank records, stride 9364 */
    u8* subs;               /* 0x10: sub-sound records, stride 44 */
    u8* voices;             /* 0x14: voice records, stride 28 */
} AudioBankTable;

extern char lbl_80114E90[];     /* diag format strings (.rodata) */
extern AudioBankTable* sAudioBankTable;
extern s32 gDiag_D28;           /* selected bank */
extern s32 gDiag_D2C;           /* selected sound */
extern s32 gDiag_D30;           /* selected voice row */
extern s32 gDiag_D34;           /* selected sub-sound */
extern s32 gDiag_D38;           /* focused column (0/1/2) */
extern char lbl_803486A0[8];    /* "%s"-style row format (sdata2) */
extern char lbl_803486A8[8];    /* sub-row format (sdata2) */
extern int sprintf(char* dst, const char* fmt, ...);
extern int printf(const char* fmt, ...);

extern s32 gDiag_D3C;           /* audio column selector */
extern s32 gDiag_D40;           /* volume A */
extern s32 gDiag_D44;           /* volume B */
extern s32 gDiag_D48;           /* master volume */
extern s32 gDiag_D4C;           /* voice slot (-1 = auto) */
extern s32 gDiag_D50;           /* pan */
extern s32 gDiag_D54;           /* last started voice mask */
extern s32 gDiag_D58;           /* autoplay mode (0/1/2/3/4/16/64) */
extern f32 gDiag_D5C;           /* next-advance time */
extern u16 gDiag_D60;           /* last queue/start status */
extern s32 sAudioQueBusy;
extern s32 sAudioMute;
extern f32 sMusicFadeBase;
extern u32 lbl_80126C10[];      /* voice-slot name table, stride 8 */
extern f64 lbl_80348678;        /* min audible duration */
extern f64 lbl_80348680;        /* max hold time */
extern f64 lbl_80348688;        /* mode-4 hold */
extern f64 lbl_80348690;        /* random hold base */
extern f32 lbl_80348698;        /* random hold range */
extern f32 lbl_8034869C;        /* short retrigger delay */
extern void audio_init(void);
extern void AudioEmptyCb2(void);
extern s32 RandInt(void);
extern f32 Random(f32 range);
extern void AudioClearTracks(void);
extern s32 AudioBankQueueName(u8* snd, u8* name, int a);
extern s32 sndFxStartVoice(s32 slot, u32 id, s32 volA, s32 a, s32 volB, s32 pan);
extern s32 AudioSetMode(u8* bank);
extern void AudioKillMask(u32 mask);
extern void sndFxResetVoices(void);
void pbDiagDrawSoundRow(void);

/* audio browser: bank/sound/voice playback console over the sound driver */
typedef struct SndSlots {
    u8  _pad0[28];
    s32 slots[64];  /* 0x1C */
} SndSlots;

typedef struct VoiceRec {
    u8  _pad0[20];
    f32 dur;        /* 0x14 */
    u8  _pad18[4];
} VoiceRec;         /* 28-byte voice record */

s32 pbDiagDrawAudio(void)
{
    char* strs = lbl_80114E90;
    u32* b = buttons;
    u8* bank;
    u8* snd;
    u8* sub;
    u8* voice;
    s32 v;
    u32 id;
    u32 w;
    u32* bp;
    f64 hold;
    f32 dur;
    u8* q;
    char buf[76];
    u8 _spare[84];

    if (gDiag_E4 != gDiag_E0) {
        v = 0;
        gDiag_D34 = v;
        gDiag_D30 = v;
        gDiag_D2C = v;
        gDiag_D28 = v;
        gDiag_D38 = 2;
        gDiag_D3C = v;
        gDiag_D40 = 127;
        gDiag_D44 = 127;
        gDiag_D48 = -1;
        gDiag_D4C = -1;
        gDiag_D50 = v;
        gDiag_D54 = v;
        gDiag_D58 = v;
        audio_init();
        AudioEmptyCb2();
        gDiag_D60 = v;
        gDiag_D5C = lbl_80348670;
    }
    fn_800C008C(0x0000FF00, -1, 1, strs + 12);
    fn_800C008C(0x00FF0000, 2, 4, strs + 24);
    pbDiagDrawSoundRow();
    bank = sAudioBankTable->banks + gDiag_D28 * 9364;
    snd = bank + gDiag_D2C * 292 + 20;
    sub = sAudioBankTable->subs + ((SndSlots*)snd)->slots[gDiag_D34] * 44;
    voice = sAudioBankTable->voices + *(s16*)(sub + 38) * 28;
    if (gDiag_D58 != 0 && sMusicFadeBase >= gDiag_D5C) {
        if (gDiag_D58 >= 2 && gDiag_D58 <= 4) {
            gDiag_D30 = gDiag_D30 + 1;
        } else if (gDiag_D58 > 4) {
            v = RandInt() + gDiag_D30;
            gDiag_D30 = v + 1;
        }
        while (gDiag_D30 >= *(s16*)(sub + 36)) {
            gDiag_D30 = gDiag_D30 - *(s16*)(sub + 36);
            if (gDiag_D34 < *(s32*)(snd + 24) - 1) {
                gDiag_D34 = gDiag_D34 + 1;
            } else {
                gDiag_D34 = 0;
                gDiag_D2C = (gDiag_D2C + 1) % *(s32*)(bank + 16);
            }
            snd = bank + gDiag_D2C * 292 + 20;
            q = bank + gDiag_D2C * 292;
            sub = sAudioBankTable->subs + *(s32*)(q + gDiag_D34 * 4 + 48) * 44;
        }
        voice = sAudioBankTable->voices + *(s16*)(sub + 38) * 28;
        if (((VoiceRec*)voice)[gDiag_D30].dur > lbl_80348678) {
            if (*(s32*)(snd + 284) != gDiag_D34) {
                AudioClearTracks();
                gDiag_D60 = AudioBankQueueName(snd, sub + 16, 0);
            }
            q = sAudioBankTable->banks + gDiag_D28 * 9364 + gDiag_D2C * 292 +
                gDiag_D34 * 4;
            id = gDiag_D30 | (*(s32*)(q + 48) << 16);
            v = sndFxStartVoice(gDiag_D4C, id, gDiag_D40, 0, gDiag_D44, gDiag_D50);
            gDiag_D60 = v;
            gDiag_D54 = (v >> 8) & 0xFF;
            if (gDiag_D58 <= 2) {
                dur = ((VoiceRec*)voice)[gDiag_D30].dur;
                hold = lbl_80348680;
                hold = (hold < dur) ? hold : dur;
                gDiag_D5C = (f32)(sMusicFadeBase + hold);
            } else if (gDiag_D58 == 4) {
                gDiag_D5C = (f32)(lbl_80348688 + sMusicFadeBase);
            } else {
                gDiag_D5C = (f32)(lbl_80348690 + sMusicFadeBase + Random(lbl_80348698));
            }
        }
    }
    sprintf(buf, strs + 84, gDiag_D40);
    fn_800C008C((gDiag_D3C == 0) ? 0x0000FF00 : 0x00FFFFFF, 2, 37, buf);
    sprintf(buf, strs + 96, gDiag_D44);
    fn_800C008C((gDiag_D3C == 1) ? 0x0000FF00 : 0x00FFFFFF, 16, 37, buf);
    sprintf(buf, strs + 108, gDiag_D48);
    fn_800C008C((gDiag_D3C == 2) ? 0x0000FF00 : 0x00FFFFFF, 26, 37, buf);
    sprintf(buf, strs + 124, gDiag_D58);
    fn_800C008C(0x00FF0000, 45, 37, buf);
    sprintf(buf, strs + 140, lbl_80126C10[gDiag_D4C * 2 + 1]);
    fn_800C008C((gDiag_D3C == 3) ? 0x0000FF00 : 0x00FFFFFF, 2, 38, buf);
    sprintf(buf, strs + 152, gDiag_D50);
    fn_800C008C((gDiag_D3C == 4) ? 0x0000FF00 : 0x00FFFFFF, 16, 38, buf);
    gDiag_D38 = pbDiagCtrlInt(0, 0, gDiag_D38, 1, 0, 3);
    switch (gDiag_D38) {
    case 0:
        v = pbDiagCtrlInt(1, 0, gDiag_D28, 1, 0, sAudioBankTable->count);
        if (v != gDiag_D28) {
            gDiag_D28 = v;
            gDiag_D30 = 0;
            gDiag_D34 = 0;
            gDiag_D2C = 0;
            gDiag_D60 = AudioSetMode(sAudioBankTable->banks + v * 9364);
        }
        break;
    case 1:
        if (gDiagRepeatDelay != 0) {
            break;
        }
        w = b[0];
        if (w & 0xC0) {
            gDiagRepeatDelay = gDiagRepeatRate;
            if (gDiag_D34 < *(s32*)(snd + 24) - 1) {
                gDiag_D34 = gDiag_D34 + 1;
                sub = sAudioBankTable->subs +
                      ((SndSlots*)snd)->slots[gDiag_D34] * 44;
            } else {
                gDiag_D34 = 0;
                gDiag_D2C = (gDiag_D2C + 1) % *(s32*)(bank + 16);
                snd = bank + gDiag_D2C * 292 + 20;
                sub = sAudioBankTable->subs + *(s32*)(snd + 28) * 44;
            }
        } else if (w & 0x30) {
            gDiagRepeatDelay = gDiagRepeatRate;
            if (gDiag_D34 > 0) {
                gDiag_D34 = gDiag_D34 - 1;
                sub = sAudioBankTable->subs +
                      ((SndSlots*)snd)->slots[gDiag_D34] * 44;
            } else {
                gDiag_D2C = gDiag_D2C - 1;
                if (gDiag_D2C < 0) {
                    gDiag_D2C = *(s32*)(bank + 16) - 1;
                }
                snd = bank + gDiag_D2C * 292 + 20;
                gDiag_D34 = *(s32*)(snd + 24) - 1;
                sub = sAudioBankTable->subs +
                      ((SndSlots*)snd)->slots[gDiag_D34] * 44;
            }
        } else {
            break;
        }
        gDiag_D30 = 0;
        if (*(s32*)(snd + 284) != gDiag_D34) {
            AudioClearTracks();
            gDiag_D60 = AudioBankQueueName(snd, sub + 16, 0);
        }
        break;
    case 2:
        v = pbDiagCtrlInt(1, 0, gDiag_D30, 1, 0, *(s32*)(snd + 20));
        if (v != gDiag_D30) {
            gDiag_D30 = v;
            if (b[0] & 0x02000000) {
                id = gDiag_D30 |
                     (*(s32*)(sAudioBankTable->banks + gDiag_D28 * 9364 +
                              gDiag_D2C * 292 + gDiag_D34 * 4 + 48) << 16);
                v = sndFxStartVoice(gDiag_D4C, id, gDiag_D40, 0, gDiag_D44, gDiag_D50);
                gDiag_D60 = v;
                gDiag_D54 = (v >> 8) & 0xFF;
                dur = ((VoiceRec*)voice)[gDiag_D30].dur;
                if (dur > lbl_80348678) {
                    hold = lbl_80348680;
                    hold = (hold < dur) ? hold : dur;
                    gDiag_D5C = (f32)(sMusicFadeBase + hold);
                } else {
                    gDiag_D5C = lbl_8034869C + sMusicFadeBase;
                }
            }
        }
        break;
    }
    if ((b[4] & 0x02000000) ||
        ((b[0] & 0x02000000) && sMusicFadeBase >= gDiag_D5C)) {
        id = gDiag_D30 |
             (*(s32*)(sAudioBankTable->banks + gDiag_D28 * 9364 +
                      gDiag_D2C * 292 + gDiag_D34 * 4 + 48) << 16);
        v = sndFxStartVoice(gDiag_D4C, id, gDiag_D40, 0, gDiag_D44, gDiag_D50);
        gDiag_D60 = v;
        gDiag_D54 = (v >> 8) & 0xFF;
        dur = ((VoiceRec*)voice)[gDiag_D30].dur;
        if (dur > lbl_80348678) {
            hold = lbl_80348680;
            if (hold < dur) {
            } else {
                hold = dur;
            }
            gDiag_D5C = (f32)(sMusicFadeBase + hold);
        } else {
            gDiag_D5C = lbl_8034869C + sMusicFadeBase;
        }
    }
    if (b[4] & 0x04000000) {
        if (gDiag_D4C < 0) {
            AudioKillMask(gDiag_D54);
        } else {
            AudioKillMask(1 << gDiag_D4C);
        }
        sAudioQueBusy = 0;
        gDiag_D5C = lbl_80348670;
    }
    if (b[4] & 0x01000000) {
        sndFxResetVoices();
        gDiag_D58 = 0;
    }
    gDiag_D3C = pbDiagCtrlInt(0, 1, gDiag_D3C, 1, 0, 5);
    switch (gDiag_D3C) {
    case 0:
        gDiag_D40 = pbDiagCtrlInt(1, 1, gDiag_D40, 1, 0, 255);
        break;
    case 1:
        gDiag_D44 = pbDiagCtrlInt(1, 1, gDiag_D44, 1, 0, 255);
        break;
    case 2:
        v = pbDiagCtrlInt(1, 1, gDiag_D48, 1, 0, 255);
        if (v != gDiag_D48 && sAudioMute == 0) {
            gDiag_D48 = v;
            AudioEmptyCb2();
            sAudioQueBusy = 0;
        }
        break;
    case 3:
        gDiag_D4C = pbDiagCtrlInt(1, 1, gDiag_D4C, 1, -1, 5);
        break;
    case 4:
        gDiag_D50 = pbDiagCtrlInt(1, 1, gDiag_D50, 1, 0, 127);
        break;
    }
    bp = &b[5];
    if (b[5] & 0x04000000) {
        switch (gDiag_D3C) {
        case 0:
            gDiag_D40 = 127;
            break;
        case 1:
            gDiag_D44 = 127;
            break;
        case 2:
            if (gDiag_D48 != 127 && sAudioMute == 0) {
                gDiag_D48 = 127;
                AudioEmptyCb2();
                sAudioQueBusy = 0;
            }
            break;
        case 3:
            gDiag_D4C = -1;
            break;
        case 4:
            gDiag_D50 = 0;
            break;
        }
    }
    if ((*bp & 0x02000000) ||
        ((b[1] & 0x02000000) && sMusicFadeBase >= gDiag_D5C)) {
        id = gDiag_D30 |
             (*(s32*)(sAudioBankTable->banks + gDiag_D28 * 9364 +
                      gDiag_D2C * 292 + gDiag_D34 * 4 + 48) << 16);
        v = sndFxStartVoice(gDiag_D4C, id, gDiag_D40, 0, gDiag_D44, gDiag_D50);
        gDiag_D60 = v;
        gDiag_D54 = (v >> 8) & 0xFF;
        dur = ((VoiceRec*)voice)[gDiag_D30].dur;
        if (dur > lbl_80348678) {
            hold = lbl_80348680;
            if (hold < dur) {
            } else {
                hold = dur;
            }
            gDiag_D5C = (f32)(sMusicFadeBase + hold);
        } else {
            gDiag_D5C = lbl_8034869C + sMusicFadeBase;
        }
    }
    if (*bp & 0x01000000) {
        switch (gDiag_D58) {
        case 0:
            gDiag_D58 = 1;
            break;
        case 1:
            gDiag_D58 = 2;
            break;
        case 2:
            gDiag_D58 = 3;
            break;
        case 3:
            gDiag_D58 = 4;
            break;
        case 4:
            gDiag_D58 = 16;
            break;
        case 16:
            gDiag_D58 = 64;
            break;
        case 64:
        default:
            gDiag_D58 = 0;
            break;
        }
        gDiag_D5C = lbl_80348670;
    }
    sprintf(buf, strs + 164, gDiag_D60);
    fn_800C008C(0x00FF0000, 2, 39, buf);
    if (b[0] & 0x08000000) {
        sndFxResetVoices();
        return 1;
    }
    return 0;
}

/* sound-browser columns: banks / sounds+subsounds / voices */
void pbDiagDrawSoundRow(void)
{
    char* strs = lbl_80114E90;
    u8* bank;
    u8* snd;
    u8* snd2;
    u8* sub2;
    u8* voice;
    u32 col1;
    u32 col2;
    u32 col3;
    int i;
    int off;
    int s;
    int soff;
    int k;
    int koff;
    int row;
    int sel;
    int flat;
    int t;
    int top;
    int bot;
    int flag;
    int id;
    u8* sub;
    s32* q32;
    u32 val;
    char buf[128];

    col1 = (gDiag_D38 == 0) ? 0x00FFFF00 : 0x0000FF00;
    i = 0;
    off = 0;
    while (i < sAudioBankTable->count) {
        sprintf(buf, lbl_803486A0, sAudioBankTable->banks + off);
        fn_800C008C((i == gDiag_D28) ? col1 : 0x00FFFFFF, 2, i + 5, buf);
        i++;
        off += 9364;
    }
    bank = sAudioBankTable->banks + gDiag_D28 * 9364;
    if (gDiag_D38 == 1) {
        col2 = 0x00FFFF00;
    } else {
        col2 = 0x0000FF00;
    }
    sel = -1;
    flat = 0;
    for (s = 0; s < *(s32*)(bank + 16); s++) {
        int n = *(s32*)(bank + s * 292 + 44);
        for (k = 0; k < n; k++) {
            if (s == gDiag_D2C && k == gDiag_D34) {
                sel = flat;
            }
            flat++;
        }
        flat++;
    }
    t = (sel - 15 > 0) ? (sel - 15) : 0;
    bot = t + 29;
    top = t;
    if (bot >= flat) {
        bot = flat - 1;
        top = (flat - 30 > 0) ? (bot - 29) : 0;
    }
    row = 0;
    s = row;
    soff = 0;
    while (s < *(s32*)(bank + 16)) {
        snd = bank + soff + 20;
        if (row >= top && row <= bot) {
            sprintf(buf, lbl_803486A0, snd);
            fn_800C008C((s == gDiag_D2C) ? col2 : 0x00FFFFFF, 10, row + 5 - top, buf);
        }
        k = 0;
        koff = 0;
        row++;
        while (k < *(s32*)(snd + 24)) {
            id = *(s32*)(snd + koff + 28);
            sub = sAudioBankTable->subs + id * 44;
            if (row >= top && row <= bot) {
                sprintf(buf, lbl_803486A8, sub + 16);
                flag = 0;
                if (s == gDiag_D2C && k == gDiag_D34) {
                    flag = 1;
                }
                fn_800C008C(flag ? col2 : 0x00FFFFFF, 10, row + 5 - top, buf);
            }
            row++;
            k++;
            koff += 4;
        }
        s++;
        soff += 292;
    }
    while (row < 30) {
        fn_800C008C(0x00FFFFFF, 10, row + 5, strs + 180);
        row++;
    }
    snd2 = sAudioBankTable->banks + (gDiag_D28 * 9364 + gDiag_D2C * 292 + 20);
    q32 = (s32*)(snd2 + gDiag_D34 * 4);
    id = q32[7];
    sub2 = sAudioBankTable->subs + id * 44;
    voice = sAudioBankTable->voices + *(s16*)(sub2 + 38) * 28;
    if (gDiag_D38 == 2) {
        col3 = 0x00FFFF00;
    } else {
        col3 = 0x0000FF00;
    }
    top = gDiag_D30 - 15;
    if (top > 0) {
    } else {
        top = 0;
    }
    bot = top + 29;
    if (bot >= *(s16*)(sub2 + 36)) {
        bot = *(s16*)(sub2 + 36) - 1;
        top = (*(s16*)(sub2 + 36) - 30 > 0) ? (bot - 29) : 0;
    }
    row = 0;
    i = 0;
    soff = 0;
    while (i < *(s16*)(sub2 + 36)) {
        if (row >= top && row <= bot) {
            u8* e = voice + soff;
            val = ((*(s32*)(snd2 + gDiag_D34 * 4 + 28) & 0x7FFF) << 16) | i;
            sprintf(buf, strs + 204, *(f32*)(e + 20), val);
            fn_800C008C((i == gDiag_D30) ? col3 : 0x00FFFFFF, 25, row + 5 - top, buf);
        }
        row++;
        i++;
        soff += 28;
    }
    while (row < 30) {
        fn_800C008C(0x00FFFFFF, 25, row + 5, strs + 228);
        row++;
    }
}

extern f32 gDiag_D20;           /* info-screen ambient ramp */
extern f32 gDiag_D24;           /* info-screen anim clock */
extern void* gDiag_D1C;         /* last attached list */
extern s32 gDiag_F00;
extern s32 gGameBusy;
extern f32 gClockFrameStep;
extern s32 lbl_80344CF8;        /* menu count (latched from lbl_803441E8) */
extern s32 lbl_803441E8;
extern DiagMenu* lbl_8023D180[]; /* per-menu DiagMenu ptr table */
extern s32 lbl_8023CFA0[];      /* per-menu texmod arg table */
extern u8 lbl_8023D000[];       /* per-menu atree params, stride 16 */
extern f64 lbl_803486E0;        /* s32->f32 conversion bias */
extern f64 lbl_803486C0;        /* anim rate constant */
extern f64 lbl_803486C8;        /* ambient scale */
extern f64 lbl_80348678;        /* anim clock threshold */
extern char lbl_803486D8[8];    /* info column format (sdata2) */
extern void InitTexMods(DiagMenu* menu, s32 arg);
extern void DoTexMods(DiagMenu* menu);
extern void AtreeDelete(void* slot);
extern s32 AtreeInit(DiagList* list, void* slot, void* params, int a);
extern void MBNodeSetParent(u32 node, s32 parent);
extern s32 DoAnimateTreeFrame(void* slot, s32 sel, s32 frame, int mode);
extern void MBTreeSetAmbientAdd(u32 node, s32 amount, int a);
void pbDiagDrawMenuA(DiagList* list);
void pbDiagDrawMenuB(DiagMenu* menu);

/* info/animation browser: menu columns, atree preview + anim clock */
s32 pbDiagDrawInfo(void)
{
    char* strs = lbl_80114E90;
    u32* b = buttons;
    f32* gd = gDiagData;
    int x;
    DiagMenu* menu;
    DiagList* entry;
    s32 old;
    s32 v;
    int i;
    s32 stepped;
    u32 w;
    u32 w2;
    u32 saved;
    void* tex;
    f32* px;
    f32* py;
    DiagObjView* obj;
    char buf16[84];
    char buf[68];

    x = 0;
    if (gDiag_FC == 0) {
        MBSetAmbient(0, lbl_803486B0);
        MBAddLight(0, 0, lbl_803486B4);
        gDiag_D20 = lbl_80348670;
        gDiag_FC = MBOX_NewObject(strs + 268, gIdentityMatrix, 0, 0);
        gDiag_D8 = 1;
        lbl_80344CF8 = lbl_803441E8;
        gDiagMenuIdx = 1;
        gDiagListSel = 0;
        gDiag_D1C = 0;
        b[114] = 0;
        *(f32*)((u8*)b + 1552) = gd[36];
        *(f32*)((u8*)b + 1556) = gd[37];
        *(f32*)((u8*)b + 1560) = gd[38];
        *(f32*)((u8*)b + 380) = *(f32*)((u8*)b + 1552);
        *(f32*)((u8*)b + 384) = *(f32*)((u8*)b + 1556);
        *(f32*)((u8*)b + 388) = *(f32*)((u8*)b + 1560);
        gDiag_D24 = lbl_803486B8;
        for (i = 0; i < 256; i++) {
            *(u32*)((u8*)b + i * 4 + 528) = 0;
        }
    }
    if (gDiag_D00 == 0) {
        tex = MBOX_FindTexture_Err(strs + 280, 0, 1);
        gDiag_D00 = MBCreateBlit(gDiag_DEC, tex, 0, 0, 512, 384);
        mbBlitCvtCoord(gDiag_D00, lbl_803486B8);
    }
    if (gDiag_D00 != 0) {
        MBBlitSetColor(gDiag_D00, (&((s32*)gDiagData)[gDiag_D8])[27]);
    }
    MBSetBGColor(*(s32*)((u32)gd + gDiag_D8 * 12), *(s32*)((u8*)gd + gDiag_D8 * 12 + 4),
                 *(s32*)((u8*)gd + gDiag_D8 * 12 + 8));
    old = gDiagMenuIdx;
    v = pbDiagCtrlInt(0, 0, old, 1, 0, lbl_80344CF8);
    gDiagMenuIdx = v;
    menu = lbl_8023D180[v];
    if (v != old && menu != 0) {
        gDiagListSel = (&b[v])[68];
        if (lbl_8023CFA0[v] >= 0 && menu->count != 0) {
            InitTexMods(menu, lbl_8023CFA0[gDiagMenuIdx]);
        }
        gDiag_D24 = lbl_803486B8;
        printf(strs + 292, menu->strs + (&b[gDiagMenuIdx])[44] * 36);
        gDiag_D0C = 0;
    }
    if (menu == 0 || menu->count == 0) {
        (&b[gDiagMenuIdx])[44] = 0;
        entry = 0;
    } else {
        if (menu->strs != 0) {
            entry = (DiagList*)((u8*)menu +
                *(s32*)(menu->strs + (&b[gDiagMenuIdx])[44] * 36 + 32));
            gDiagListSel = pbDiagCtrlInt(1, 0, gDiagListSel, 1, 0, entry->count);
        } else {
            gDiagListSel = 0;
            entry = 0;
        }
        v = gDiagListSel;
        if (v != (s32)(&b[gDiagMenuIdx])[68]) {
            (&b[gDiagMenuIdx])[68] = v;
            gDiag_D24 = (f32)(s32)(&b[v])[132];
        }
    }
    if (menu != 0 && entry != 0) {
        stepped = 0;
        w = b[8];
        if (w & 0x01000000) {
            v = (&b[gDiagMenuIdx])[44] + 1;
            (&b[gDiagMenuIdx])[44] = v;
            if (v >= menu->count) {
                (&b[gDiagMenuIdx])[44] = 0;
            }
            stepped = 1;
        }
        if (w & 0x04000000) {
            v = (&b[gDiagMenuIdx])[44] - 1;
            (&b[gDiagMenuIdx])[44] = v;
            if (v < 0) {
                (&b[gDiagMenuIdx])[44] = menu->count - 1;
            }
            stepped = 1;
        }
        if (stepped != 0) {
            entry = (DiagList*)((u8*)menu +
                *(s32*)(menu->strs + (&b[gDiagMenuIdx])[44] * 36 + 32));
            if (gDiagListSel >= entry->count) {
                gDiagListSel = 0;
                (&b[gDiagMenuIdx])[68] = 0;
                gDiag_D24 = (f32)(s32)(&b[gDiagListSel])[132];
            }
        }
    }
    if (!(b[114] != 0 && (void*)entry == gDiag_D1C)) {
        if (b[114] != 0) {
            AtreeDelete((u8*)((u32)b + 456));
        }
        if (entry != 0) {
            s32 kept = gDiag_F00;
            if (gControllerButtons & 1) {
                b[114] = AtreeInit(entry, (u8*)((u32)b + 456), 0, 0);
            } else {
                b[114] =
                    AtreeInit(entry, (u8*)((u32)b + 456), lbl_8023D000 + gDiagMenuIdx * 16, 0);
            }
            if (gDiag_F00 != 0) {
                gDiag_F00 = kept;
            }
            MBNodeSetParent(b[114], gDiag_FC);
        } else {
            b[114] = 0;
        }
        gDiag_D1C = entry;
        gDiag_D24 = lbl_803486B8;
    }
    if (entry != 0) {
        if (gGameBusy == 0) {
            s16 spd = *(s16*)(*(u8**)((u8*)b + 460) + gDiagListSel * 48 + 34);
            if (spd > 0) {
                gDiag_D24 = (f32)(lbl_803486C0 / (f64)spd *
                                  (lbl_803486C0 * gClockFrameStep) + gDiag_D24);
            } else {
                gDiag_D24 = (f32)(gDiag_D24 + lbl_803486C0 * gClockFrameStep);
            }
            if (gDiag_D24 < lbl_80348678) {
                gDiag_D24 = lbl_80348670;
            }
        }
        *(u16*)((u8*)b + 512) = 1;
        if (gDiag_D24 >= (f32)(s32)*(s16*)((u8*)b + 476)) {
            gDiag_D24 = lbl_80348670;
        }
        if (gDiag_D24 >= lbl_80348678) {
            DoAnimateTreeFrame((u8*)((u32)b + 456), gDiagListSel, (s32)gDiag_D24, 2);
        }
        MBTreeSetAmbientAdd(*(u32*)b[114],
                            (s32)(lbl_803486C8 * gDiag_D20), 1);
        if (menu != 0) {
            DoTexMods(menu);
        }
    }
    v = gDiagListSel;
    (&b[v])[132] = (s32)gDiag_D24;
    pbDiagDrawColorBars();
    CreatePYRMatrix(gDiag_FC, (u8*)b + 368);
    px = (f32*)((u8*)b + 384);
    py = (f32*)((u8*)b + 388);
    *(f32*)((u8*)gDiag_FC + 48) = *(f32*)((u8*)b + 380);
    *(f32*)((u8*)gDiag_FC + 52) = *(f32*)((u8*)b + 384);
    *(f32*)((u8*)gDiag_FC + 56) = *(f32*)((u8*)b + 388);
    w2 = b[8];
    if (w2 & 0x00400000) {
        gDiag_D20 = (f32)(gDiag_D20 + lbl_803486D0);
    }
    if (w2 & 0x00800000) {
        gDiag_D20 = (f32)(gDiag_D20 - lbl_803486D0);
    }
    if (gControllerButtons & 1) {
        MBTreeSetAltTex(gDiag_FC, -2, gDiagWhiteObj, 1);
        gDiag_D4 = 1;
    } else if (gDiag_D4 != 0) {
        MBTreeSetAltTex(gDiag_FC, -1, 0, 1);
        gDiag_D4 = 0;
    }
    for (i = 0; i < lbl_80344CF8; i++) {
        sprintf(buf, lbl_803486D8, i);
        fn_800C008C((i == gDiagMenuIdx) ? 0x00FFFF00 : 0x00FFFFFF, x, 2, buf);
        x += 3;
    }
    saved = fn_800C02F4(0x00FFFFFF);
    if (entry != 0) {
        obj = *(DiagObjView**)((u8*)gWinGlobals->f30 + gDiag_F4 * 16 + 4);
        if (*(s8*)obj->name != 0) {
            fn_800C008C(0x00FFFF00, 61 - strlen(obj->name), 3, obj->name);
        }
    }
    sprintf(buf16, strs + 304, *(f32*)((u8*)b + 484),
            *(s16*)((u8*)b + 476) - 1,
            (entry != 0) ? *(s16*)(*(u8**)entry + gDiagListSel * 48 + 34) : 0);
    fn_800C01C0(2, 43, buf16);
    sprintf(buf16, strs + 344, *(f32*)((u8*)b + 380), *px, *py,
            *(f32*)((u8*)b + 368), *(f32*)((u8*)b + 372), *(f32*)((u8*)b + 376));
    fn_800C01C0(2, 44, buf16);
    fn_800C02F4(saved);
    if (menu != 0) {
        pbDiagDrawMenuB(menu);
    }
    if (entry != 0) {
        pbDiagDrawMenuA(entry);
    }
    if (b[0] & 0x08000000) {
        MBTreeInit();
        gDiag_FC = 0;
        gDiag_D00 = 0;
        return 1;
    }
    return 0;
}

void pbDiagDrawMenuA(DiagList* list) {
    int off;
    int colorBase;
    int i;
    int line;
    int start;
    int end;
    int count = list->count;

    line = 3;
    if (count < 38) {
        end = count;
        start = 0;
    } else {
        end = gDiagListSel + 19;
        if (end < 38) {
            end = 38;
        }
        if (end >= count) {
            end = count;
        }
        start = end - 38;
    }
    off = (i = start) * 48;
    colorBase = 0x01000000;
    for (; i < end;) {
        if (i == gDiagListSel) {
            fn_800C008C(colorBase - 0x100, 18, line, list->strs + off);
        } else {
            fn_800C008C(colorBase - 1, 18, line, list->strs + off);
        }
        line++;
        i++;
        off += 48;
    }
}

void pbDiagDrawMenuB(DiagMenu* menu) {
    int line;
    int i;
    int off;
    int end;
    u8 _spare[8];
    int count = menu->count;

    line = 3;
    {
        int start;
        if (count < 38) {
            end = count;
            start = 0;
        } else {
            end = gDiagMenuList[gDiagMenuIdx] + 19;
            if (end < 38) {
                end = 38;
            }
            if (end >= count) {
                end = count;
            }
            start = end - 38;
        }
        if (menu == 0) {
            return;
        }
        if (menu->strs == 0) {
            return;
        }
        i = start;
    }
    off = i * 36;
    while (i < end) {
        if (i == gDiagMenuList[gDiagMenuIdx]) {
            fn_800C008C(0x00FFFF00, 1, line, menu->strs + off);
            strlen(menu->strs + off);
        } else {
            fn_800C008C(0x00FFFFFF, 1, line, menu->strs + off);
        }
        line++;
        i++;
        off += 36;
    }
}

/* buttons-block view for pbDiagDrawTexture: cursor table at +112, tile blits at +392 */
typedef struct BtnView {
    u8  _pad0[112];
    s32 cursors[70];        /* 0x70: per-bank texture cursor (u32[.. ]) */
    u32 blits[8];           /* 0x188 (392): 6 tile blit handles */
    u32 f424;               /* 0x1A8 */
} BtnView;

/* wg->f30 object-view entry: 16-byte stride, view ptr at +4 */
typedef struct ObjEnt {
    s32 a;
    struct DiagObjView* obj;
    s32 c;
    s32 d;
} ObjEnt;

/* wg->f30 texture-bank entry: 16-byte stride, bank ptr at +4, lock flag read at +16 */
typedef struct TexBankEnt {
    s32 a;                  /* 0x0 */
    struct DiagTexBank* bank; /* 0x4 */
    s32 c;
    s32 d;
} TexBankEnt;

/* texture bank view: name at +0x20, slot count at +0x48, defs at +0x58 */
typedef struct DiagTexBank {
    u8   _pad00[32];
    char name[40];          /* 0x20 */
    u32  nslots;            /* 0x48 */
    u8   _pad4C[12];
    u8*  defs;              /* 0x58: 16-byte texdef records */
} DiagTexBank;

extern s32 gDiag_F4;            /* declared above */
extern s32 gDiagBtns_F8[];      /* per-screen cursor table */
extern char lbl_803486F8[8];    /* "%3d %s"-style row format (sdata2) */
extern void* MBOX_GetTexDef(u32 id);

extern s32 gDiag_D10;           /* texture-screen mode (0..3) */
extern u8* gDiag_D14;           /* highlighted texdef ptr */
extern s32 gDiag_DE8;
extern char lbl_80114FA8[];     /* backdrop texture name (.rodata) */
extern char lbl_8011501C[];     /* "LOCKED"-style banner (.rodata) */
extern char lbl_803486E8[8];    /* texdef printf format (sdata2) */
extern void fn_800C7864(int a);
extern void mbBlitUpdateEntry(u32 blit, int idx, u32 flags);
extern void MBRemoveBlit(u32 blit);
extern void mbInitBlitEntry(u32 blit, u32 id, int a);
extern void mbBlitCalcRect(u32 blit, s32* x, s32* y, int a);
extern void mbBlitCalcWidth(u32 blit, s32 x, s32 y, f32 v);
extern u32 mbBlitStub343C(void);
void pbDiagDrawTexLabel();

/* texture-browser screen: backdrop blit, 6-tile grid or single zoom blit,
 * per-bank texture cursor, tile refresh + highlight toggling */
s32 pbDiagDrawTexture(void)
{
    char buf[60];
    register s32* gdi = (s32*)gDiagData;
    u32* b = buttons;
    register BtnView* bv = (BtnView*)buttons;
    register u8* texdef;
    register WinGlobals* wg;
    register int x;
    register DiagTexBank* tb;
    register f32* gd;
    register int i;
    register s32 old;
    register s32 old2;
    register s32 v;
    register u32 saved;
    register void* tex;
    register u32* bp;
    register u32 w;
    u8 _spare[52];
    s32 rectX;
    s32 rectY;
    u8 _spare2[8];
    register s16 tw;
    register s16 th;
    register u32 blit;
    register u32* cp;

    gd = (f32*)gdi;
    wg = gWinGlobals;
    x = 0;
    texdef = 0;
    if (gDiag_D00 == 0) {
        tex = MBOX_FindTexture_Err(lbl_80114FA8, 0, 1);
        gDiag_D00 = MBCreateBlit(gDiag_DEC, tex, 0, 0, 512, 384);
        mbBlitCvtCoord(gDiag_D00, lbl_803486B8);
    }
    if (gDiag_D00 != 0) {
        MBBlitSetColor(gDiag_D00, (&gdi[gDiag_D8])[27]);
    }
    if (gDiag_D14 != 0) {
        *(u16*)(gDiag_D14 + 2) &= ~2;
    }
    if (gDiag_D10 >= 2) {
        if ((u32)(&b[0])[98] == 0) {
            fn_800C7864(0);
            for (i = 0; i < 6; i++) {
                tw = ((s16*)&gd[i])[164];
                th = ((s16*)&gd[i])[165];
                blit = MBCreateBlit(gDiag_DE8, 0,
                                           ((s16*)&gd[i * 4])[108],
                                           ((s16*)&gd[i * 4])[109],
                                           tw, th);
                bp = &b[i];
                bp[98] = blit;
                mbBlitUpdateEntry(bp[98], -1, 0x01000200);
            }
        }
        if (gDiag_D04 != 0) {
            MBRemoveBlit(gDiag_D04);
            gDiag_D04 = 0;
        }
    } else {
        if (gDiag_D04 == 0) {
            gDiag_D04 = MBCreateBlit(gDiag_DE8, 0, 256, 64, -2, -2);
            mbBlitUpdateEntry(gDiag_D04, -1, 0x01000000);
            gDiag_D8 = 0;
        }
        if ((u32)(&b[0])[98] != 0) {
            for (i = 0; i < 6; i++) {
                bp = &b[i];
                MBRemoveBlit(bp[98]);
            }
            (&b[0])[98] = 0;
        }
    }
    MBSetBGColor(*(s32*)((u32)gd + gDiag_D8 * 12), *(s32*)((u8*)gd + gDiag_D8 * 12 + 4),
                 *(s32*)((u8*)gd + gDiag_D8 * 12 + 8));
    old2 = gDiag_F0;
    v = pbDiagCtrlInt(0, 0, gDiag_F4, 1, 0, old2);
    gDiag_F4 = v;
    old = (s32)(&b[v])[28];
    tb = ((DiagTexBank**)&((s32*)wg->f30)[v * 4])[1];
    if ((&((s32*)wg->f30)[v * 4])[4] == 0) {
        old2 = pbDiagCtrlInt(1, 0, old, 1, 0, tb->nslots);
        v = gDiag_F4;
        (&b[v])[28] = old2;
        old2 = pbDiagCtrlInt(4, 0, (&b[v])[28], 10, 0, tb->nslots);
        v = gDiag_F4;
        (&b[v])[28] = old2;
        if (old != (s32)(&b[v])[28]) {
            printf(lbl_803486E8, MBOX_GetTexDef((u16)(&b[v])[28] | (v << 16)));
            gDiag_D0C = 0;
        }
        if (gDiag_D10 >= 2) {
            for (i = 0; i < 6; i++) {
                bp = &b[i];
                cp = &b[gDiag_F4];
                mbInitBlitEntry(bp[98],
                                (u16)cp[28] | (gDiag_F4 << 16), 0);
            }
        } else if (gDiag_D04 != 0) {
            mbInitBlitEntry(gDiag_D04,
                            (u16)(&b[gDiag_F4])[28] | (gDiag_F4 << 16), 0);
        }
    }
    for (i = 0; i < gDiag_F0; i++) {
        sprintf(buf, lbl_803486F0, i);
        fn_800C008C((i == gDiag_F4) ? 0x00FFFF00 : 0x00FFFFFF, x, 2, buf);
        x += 4;
    }
    if (*(s8*)tb->name != 0) {
        fn_800C008C(0x00FFFF00, 61 - strlen(tb->name), 3, tb->name);
    }
    cp = (u32*)((u8*)wg->f30 + 16);
    if (((TexBankEnt*)cp)[gDiag_F4].a == 0) {
        pbDiagDrawTexLabel(tb);
        saved = fn_800C02F4(0x00FFFFFF);
        if (tb->nslots != 0) {
            texdef = tb->defs + (&b[gDiag_F4])[28] * 16;
        } else {
            texdef = 0;
        }
        if (texdef != 0 && gDiag_D14 != 0) {
            fn_800C02F4(0x00FF0000);
            fn_800C01C0(30, 43, lbl_8011501C);
        }
        fn_800C02F4(saved);
    }
    if (gDiag_D04 != 0) {
        mbBlitCalcRect(gDiag_D04, &rectX, &rectY, 0);
        w = b[1];
        if (w & 3) {
            rectX = rectX - 1;
        }
        if (w & 0xC) {
            rectX = rectX + 1;
        }
        if (w & 0x30) {
            rectY = rectY + 1;
        }
        if (w & 0xC0) {
            rectY = rectY - 1;
        }
        mbBlitCalcWidth(gDiag_D04, rectX, rectY, lbl_803486B8);
    }
    if (b[0] & 0x01000000) {
        if (gDiag_D10 == 1) {
            gDiag_D10 = 2;
        } else if (gDiag_D10 == 3) {
            gDiag_D10 = 0;
        }
    } else {
        if (gDiag_D10 == 0) {
            gDiag_D10 = 1;
        } else if (gDiag_D10 == 2) {
            gDiag_D10 = 3;
        }
    }
    bp = &b[5];
    if (b[5] & 0x04000000) {
        if (gDiag_D04 != 0) {
            mbBlitUpdateEntry(gDiag_D04, -1, mbBlitStub343C() ^ 256);
        }
    }
    if (*bp & 0x02000000) {
        if (gDiag_D14 != 0) {
            gDiag_D14 = 0;
        } else {
            gDiag_D14 = texdef;
        }
    }
    if (b[0] & 0x08000000) {
        MBTreeInit();
        gDiag_D04 = 0;
        gDiag_D00 = 0;
        (&b[0])[106] = 0;
        (&b[0])[98] = 0;
        gDiag_D14 = 0;
        return 1;
    }
    if (gDiag_D14 != 0) {
        gDiag_D14 = texdef;
        *(u16*)(texdef + 2) |= 2;
    }
    return 0;
}

/* one label row per texture slot of bank `bank`, windowed like MenuA */
void pbDiagDrawTexLabel(tb, bank)
DiagTexBank* tb;
int bank;
{
    int line;
    u32 color;
    u32 hi;
    int i;
    int start;
    int end;
    register u32 count;
    u32 id;
    void* def;
    u8 _spare[8];


    line = 3;
    if ((count = tb->nslots) < 38) {
        end = count;
        start = 0;
    } else {
        end = gDiagBtns_F8[gDiag_F4] + 19;
        if (end < 38) {
            end = 38;
        }
        if ((u32)end >= count) {
            end = count;
        }
        start = end - 38;
    }
    hi = bank << 16;
    for (i = start; i < (s32)end; i++) {
        id = (u16)i | hi;
        if (i == gDiagBtns_F8[gDiag_F4]) {
            color = 0x00FFFF00;
        } else {
            color = 0x00FFFFFF;
        }
        def = MBOX_GetTexDef(id);
        fn_800C008C(color, 1, line, lbl_803486F8, i, def);
        line++;
    }
}



/* object-browser screen: spawn/manage the preview object, columns, HSV */
s32 pbDiagDrawObject(void)
{
    char buf[68];
    f32* gd = gDiagData;
    s32* gdi = (s32*)gDiagData;
    s32* b = (s32*)buttons;
    char* strs = lbl_80114E90;
    WinGlobals* wg;
    DiagObjView* obj;
    int x;
    int i;
    s32 old;
    s32 ret;
    u8* row;
    u32 saved;
    f32* px;
    f32* py;
    s32 v;
    s32 idx;
    u8* t10;
    u8* fnd;
    void* tex;

    x = 0;
    wg = gWinGlobals;
    if (gDiag_FC == 0) {
        f32 z;
        MBSetAmbient(0, lbl_803486B0);
        MBAddLight(0, 0, lbl_803486B4);
        z = lbl_80348670;
        MBWindowViewport(z, z, z, z);
        gDiag_FC = MBOX_NewObject(strs + 268, gIdentityMatrix, 0, 0);
        gDiag_D8 = 1;
        gDiag_D0C = 0;
        tex = MBOX_FindTexture_Err(strs + 280, 0, 1);
        gDiag_D00 = MBCreateBlit(gDiag_DEC, tex, 0, 0, 512, 384);
        mbBlitCvtCoord(gDiag_D00, lbl_803486B8);
    }
    MBSetBGColor(*(s32*)((u32)gd + gDiag_D8 * 12), *(s32*)((u8*)gd + gDiag_D8 * 12 + 4),
                 *(s32*)((u8*)gd + gDiag_D8 * 12 + 8));
    if (gDiag_D00 != 0) {
        MBBlitSetColor(gDiag_D00, (&gdi[gDiag_D8])[27]);
    }
    old = gDiag_F0;
    v = pbDiagCtrlInt(0, 0, gDiag_F4, 1, 0, old);
    gDiag_F4 = v;
    obj = ((ObjEnt*)wg->f30)[v].obj;
    old = (&b[v])[12];
    ret = pbDiagCtrlInt(1, 0, (&b[gDiag_F4])[12], 1, 0, obj->count);
    v = gDiag_F4;
    (&b[v])[12] = ret;
    ret = pbDiagCtrlInt(4, 0, (&b[v])[12], 10, 0, obj->count);
    v = gDiag_F4;
    (&b[v])[12] = ret;
    if (old != (&b[v])[12]) {
        printf(lbl_80348700, obj->rows + (&b[v])[12] * 24);
        gDiag_D0C = 0;
    }
    v = gDiag_F4;
    fnd = MBOX_ReallyFindObject(obj->rows + (&b[v])[12] * 24, v, v, 1) + gDiag_D0C;
    if (gDiag_F4 != gDiag_E6C || gDiag_E70 != (&b[gDiag_F4])[12]) {
        gDiag_E6C = gDiag_F4;
        gDiag_E70 = (&b[gDiag_F4])[12];
    }
    MBSetObject(gDiag_FC, fnd);
    if (gControllerButtons & 1) {
        MBTreeSetAltTex(gDiag_FC, -2, gDiagWhiteObj, 1);
        gDiag_D4 = 1;
    } else if (gDiag_D4 != 0) {
        MBTreeSetAltTex(gDiag_FC, -1, 0, 1);
        gDiag_D4 = 0;
    }
    if (b[8] & 0x01000000) {
        v = gDiag_D0C + 1;
        gDiag_D0C = v;
        row = (u8*)obj->rows + (&b[gDiag_F4])[12] * 24;
        if (v > *(s16*)(row + 22)) {
            gDiag_D0C = 0;
        }
    }
    pbDiagDrawColorBars();
    CreatePYRMatrix(gDiag_FC, (u8*)((u32)b + 368));
    px = (f32*)((u8*)b + 384);
    py = (f32*)((u8*)b + 388);
    *(f32*)((u8*)gDiag_FC + 48) = *(f32*)((u8*)b + 380);
    *(f32*)((u8*)gDiag_FC + 52) = *(f32*)((u8*)b + 384);
    *(f32*)((u8*)gDiag_FC + 56) = *(f32*)((u8*)b + 388);
    for (i = 0; i < gDiag_F0; i++) {
        sprintf(buf, lbl_803486F0, i);
        fn_800C008C((i == gDiag_F4) ? 0x00FFFF00 : 0x00FFFFFF, x, 2, buf);
        x += 4;
    }
    saved = fn_800C02F4(0x00FFFFFF);
    if (*(s8*)obj->name != 0) {
        fn_800C008C(0x00FFFF00, 57 - strlen(obj->name), 3, lbl_80348708, obj->name, gDiag_F4);
    }
    t10 = *(u8**)((u8*)gDiag_FC + 112);
    if (*(u8**)(t10 + 44) != 0) {
        idx = (s32)(*(u8**)(t10 + 44) - (u8*)obj->rows) / 24;
    } else {
        idx = -1;
    }
    fn_800C01C0(2, 43, strs + 656, *(f32*)(t10 + 4), *(s32*)(t10 + 32), *(s32*)(t10 + 36),
                (u16)*(u32*)((u8*)gDiag_FC + 108), idx, *(s32*)(t10 + 40));
    fn_800C01C0(2, 44, strs + 716, *(f32*)((u32)b + 380), *px, *py,
                *(f32*)((u8*)b + 368), *(f32*)((u8*)b + 372), *(f32*)((u8*)b + 376));
    fn_800C02F4(saved);
    pbDiagDrawStrRow((DiagStrRows*)obj);
    if (b[0] & 0x08000000) {
        MBTreeInit();
        gDiag_FC = 0;
        gDiag_D00 = 0;
        return 1;
    }
    return 0;
}


/* buttons block view: color-bar HSV/RGB state at +368 */
typedef struct DiagPadView {
    u32 words[92];          /* 0x000: raw pad words (word 5 = held buttons) */
    f32 f368;               /* 0x170: hue A */
    f32 f372;               /* 0x174: hue B */
    f32 f376;               /* 0x178 */
    f32 f380;               /* 0x17C: sat */
    f32 f384;               /* 0x180: val A */
    f32 f388;               /* 0x184: val B */
} DiagPadView;

/* animate/adjust the diag color-bar HSV values from pad input */
#pragma opt_propagation off
void pbDiagDrawColorBars(void)
{
    f32* gd = gDiagData;
    DiagPadView* b = (DiagPadView*)buttons;
    f32* dst;
    f32 spd;
    f64 v;

    if (lbl_80240FC0[1] != 0) {
        gDiag_E68 = (f32)(gDiag_E68 + lbl_803486D0);
    } else {
        gDiag_E68 = lbl_803486B4;
    }
    if (b->words[5] & 0x40000) {
        f32 z = lbl_80348670;
        b->f368 = z;
        b->f372 = z;
        b->f376 = z;
        b->f380 = gd[36];
        b->f384 = gd[37];
        b->f388 = gd[38];
    } else {
        v = lbl_80348710 * gDiag_E68;
        spd = (f32)v;
        if (spd > lbl_80348718) {
            spd = lbl_80348720;
        }
        b->f368 = pbDiagCtrlFloat(1, 1, b->f368, -spd, lbl_80348724, lbl_80348728);
        v = b->f368;
        if (v > lbl_80348730) {
            v = v - lbl_80348738;
        } else if (v <= lbl_80348740) {
            v = lbl_80348738 + v;
        }
        b->f368 = (f32)v;
        dst = &b->f372;
        *dst = pbDiagCtrlFloat(0, 1, b->f372, spd, lbl_80348724, lbl_80348728);
        v = *dst;
        if (v > lbl_80348730) {
            v = v - lbl_80348738;
        } else if (v <= lbl_80348740) {
            v = lbl_80348738 + v;
        }
        *dst = (f32)v;
        b->f380 = pbDiagCtrlFloat(3, 1, b->f380, (f32)(lbl_80348748 * gDiag_E68),
                                  lbl_80348750, lbl_80348754);
        dst = &b->f384;
        *dst = pbDiagCtrlFloat(4, 1, b->f384, (f32)(lbl_80348688 * gDiag_E68),
                               lbl_80348750, lbl_80348754);
        dst = &b->f388;
        *dst = pbDiagCtrlFloat(2, 1, b->f388, (f32)(lbl_80348688 * gDiag_E68),
                               lbl_80348750, lbl_80348754);
    }
}
#pragma opt_propagation reset



extern s32 gDiagBtns_B8[];      /* per-screen cursor table (B8 block) */
extern s32 gDiag_D0C;
extern char lbl_80348758[8];    /* value-suffix row format (sdata2) */

/* windowed 24-byte string rows; selected row also prints its s16 value */
void pbDiagDrawStrRow(DiagStrRows* p)
{
    int i;
    int line;
    int end;
    int start;
    u32 count;
    int len;

    line = 3;
    if ((count = p->count) < 38) {
        end = count;
        start = 0;
    } else {
        end = gDiagBtns_B8[gDiag_F4] + 19;
        if (end < 38) {
            end = 38;
        }
        if ((u32)end >= count) {
            end = count;
        }
        start = end - 38;
    }
    for (i = start; i < end; i++) {
        if (i == gDiagBtns_B8[gDiag_F4]) {
            fn_800C008C(0x00FFFF00, 1, line, ((DiagRow*)p->strs)[i].name);
            len = strlen(((DiagRow*)p->strs)[i].name) + 3;
            if (((DiagRow*)p->strs)[i].val > 0) {
                fn_800C008C(0x00FFFF00, len, line, lbl_80348758, gDiag_D0C);
            }
        } else {
            fn_800C008C(0x00FFFFFF, 1, line, ((DiagRow*)p->strs)[i].name);
        }
        line++;
    }
}



extern u32 lbl_80240FB0[4];
extern u32 lbl_80240FA0[4];

/* 12-byte top-menu entry rows at gDiagData+156 */
typedef struct DiagMenuEntry {
    char name[8];
    s32 (*fn)(void);
} DiagMenuEntry;

typedef struct DiagDataView {
    u8 _pad00[156];
    DiagMenuEntry entries[5];
} DiagDataView;

/* top-level diag menu: latch pads, draw/step the 5 entries, dispatch the
 * active entry's handler */
s32 pbDiagDrawMenu(void)
{
    f32* gd = gDiagData;
    u32* b = buttons;
    char* strs = lbl_80114E90;
    char* row;
    int x;
    int i;
    u32 color;
    s32 old;
    s32 ret;
    u8 _spare[8];

    fn_800C0310();
    if (gDiagRepeatDelay > 0) {
        gDiagRepeatDelay = gDiagRepeatDelay - 1;
        gDiagRepeatRate = 2;
    } else {
        gDiagRepeatDelay = 0;
        gDiagRepeatRate = 8;
    }
    for (i = 0; i < 4; i++) {
        b[i] = lbl_80240FC0[i];
        *(u32*)((u8*)b + i * 4 + 16) = *(u32*)((u8*)lbl_80240FB0 + i * 4);
        *(u32*)((u8*)b + i * 4 + 32) = *(u32*)((u8*)lbl_80240FA0 + i * 4);
    }
    if (gDiag_E0 < 0) {
        MBSetBGColor(0, 0, 0);
        old = gDiag_DC;
        ret = pbDiagCtrlInt(0, 0, old, 1, 0, 5);
        gDiag_DC = ret;
        if (ret != old) {
            row = (char*)gd + gDiag_DC * 12;
            printf(strs + 764, row + 156);
        }
        x = 0;
        for (i = x; (u32)i < 5; i++) {
            if (i == gDiag_DC) {
                color = 0x00FFFF00;
            } else {
                color = 0x00FFFFFF;
            }
            fn_800C008C(color, x, 2, (char*)&gd[i * 3] + 156);
            x += 8;
        }
        if (gDiagRepeatDelay == 0 && (b[4] & 0x02000000)) {
            gDiag_E0 = gDiag_DC;
            row = (char*)gd + gDiag_E0 * 12;
            printf(strs + 784, row + 156);
        }
    } else {
        row = (char*)gd + gDiag_E0 * 12;
        ret = (*(s32 (**)(void))(row + 164))();
        if (ret == 1) {
            gDiag_E0 = -1;
            printf(strs + 804);
        } else if (ret == 2) {
            return 2;
        }
        gDiag_E4 = gDiag_E0;
        if (b[4] & 0x00100000) {
            gDiag_D8 = gDiag_D8 + 1;
            if ((u32)gDiag_D8 >= 9) {
                gDiag_D8 = 0;
            }
        }
    }
    return 0;
}

#pragma opt_propagation off
void pbInitDiag(int mode) {
    f32* dp = gDiagData;
    f32* fp = (f32*)buttons;

    AudioStopSelect();
    AudioSelectReset();
    fn_800C0310();
    MBTreeInit();
    DebugCamInit();
    fp[92] = 0.0f;
    fp[93] = 0.0f;
    fp[94] = 0.0f;
    fp[95] = dp[36];
    fp[96] = dp[37];
    fp[97] = dp[38];
    gDiag_DC = 0;
    gDiag_E0 = mode;
    gDiag_E4 = -2;
    gDiagWhiteObj = MBOX_FindTexture("aaawhite", 0);
    gDiag_D4 = 0;
    pbResetDiag();
}
#pragma opt_propagation reset

void pbResetDiag(void) {
    int i;
    u32* p = buttons;

    gDiag_F0 = *gWinGlobals->f30;
    gDiag_F4 = 0;
    for (i = 0; i < 16; i++) {
        p[i + 12] = 0;
        p[i + 28] = 0;
        p[i + 44] = 0;
    }
    for (i = 0; i < 24; i++) {
        p[i + 44] = 0;
        p[i + 68] = 0;
    }
    gDiag_FC = 0;
    gDiag_D08 = 0;
    gDiagRepeatDelay = 15;
    gDiagRepeatRate = 8;
    gDiag_D04 = 0;
    gDiag_D00 = 0;
    p[98] = 0;
    p[106] = 0;
    gDiag_D8 = 0;
}

f32 pbDiagCtrlFloat(s32 axis, s32 pad, f32 val, f32 inc, f32 min, f32 max) {
    u32 up;
    u32 down;

    if (gDiagRepeatDelay != 0) {
        return val;
    }
    if (gDiagTurbo) {
        if (buttons[pad] & 0x00100000) {
            inc *= 5.0f;
        }
    }
    switch (axis) {
    case 0:
    default:
        up = 3;
        down = 12;
        break;
    case 1:
        up = 0x30;
        down = 0xC0;
        break;
    case 2:
        up = 0x08000000;
        down = 0x02000000;
        break;
    case 3:
        up = 0x04000000;
        down = 0x01000000;
        break;
    case 4:
        up = 0x00400000;
        down = 0x00800000;
        break;
    }
    if (buttons[pad] & up) {
        val += inc;
        gDiagRepeatDelay = gDiagRepeatRate;
        if (val > max) {
            val = min;
        }
    }
    if (buttons[pad] & down) {
        val -= inc;
        gDiagRepeatDelay = gDiagRepeatRate;
        if (val < min) {
            val = max;
        }
    }
    return val;
}

s32 pbDiagCtrlInt(s32 axis, s32 pad, s32 val, s32 inc, s32 min, s32 max) {
    u32 up;
    u32 down;

    if (gDiagRepeatDelay != 0) {
        return val;
    }
    if (gDiagTurbo) {
        if (buttons[pad] & 0x00100000) {
            inc *= 5;
        }
    }
    switch (axis) {
    case 0:
    default:
        up = 3;
        down = 12;
        break;
    case 1:
        up = 0x30;
        down = 0xC0;
        break;
    case 2:
        up = 0x08000000;
        down = 0x02000000;
        break;
    case 3:
        up = 0x04000000;
        down = 0x01000000;
        break;
    case 4:
        up = 0x00400000;
        down = 0x00800000;
        break;
    }
    if (buttons[pad] & down) {
        val += inc;
        gDiagRepeatDelay = gDiagRepeatRate;
        if (val >= max) {
            val = min;
        }
    }
    if (buttons[pad] & up) {
        val -= inc;
        gDiagRepeatDelay = gDiagRepeatRate;
        if (val < min) {
            val = max - 1;
        }
    }
    return val;
}
