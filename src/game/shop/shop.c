/* shop.c -- GCN SHOP.OBJ (shell3D.pdb module .\Release\SHOP.OBJ), NonMatching.
 *
 * The between-level shop screen: gold/item piles, the scrolling buy menu,
 * per-player shopping state, the level-intro ("shop_show_lv") and final-stats
 * panels, and the shop.wad asset loader.
 *
 * TU boundary evidence:
 *   .text      0x800988A4..0x8009C228 (sfx.c ends at 0x800988A4;
 *              shopquery.c -- ItemDefValid/PlayerItemState, already Matching --
 *              owns 0x8009C228..0x8009C2CC, then sounds_evt.c)
 *   extab      0x80006EB0..0x80006F18 (flush against sounds_evt.c's claim)
 *   extabindex 0x8000AE58..0x8000AEF4 (contiguous 13 entries 0x988A4..0x9C0F0)
 *   sdata2     pool 0x80348328..~0x80348450 (disjoint from the sfx pool)
 *   .data      jumptable_8012318C (do_shopping), jumptable_8012322C
 *              (calculate_player_shopping_parameters), pile coordinate tables
 *              0x80122ED0/0x80122F30/0x80122F40/0x80122F50
 *   strings    "Loading..." / "Final Stats" 0x8011495C (do_shop),
 *              "SHP_GOLD"/"SHP_BONES"/"SHOP_SCR" 0x80114918 textures,
 *              "shop.wad" 0x80114A3C (ShopLoadData)
 *
 * GC has 13 functions where Xbox SHOP.OBJ has 29: the small player_*
 * query helpers, pile init/hide, draw_shop_screen, make_skiplist,
 * recalc_last_available_idx, remove_shop_player_blits, shop_ready and
 * FAKEdo_shop were inlined into the survivors by MWCC -O4 (or live in
 * shopquery.c). Names mapped behaviorally against the Xbox roster:
 *
 *   0x800988A4 do_shop            (G) 0x72C master state machine; draws
 *                                     "Loading..." while SelectLoadDone pends,
 *                                     drives every screen below; gamemain caller [high]
 *   0x80098FD0 shop_show_final_stats (L) 0x440 4-player column stats panel
 *                                     (x tables 0x80/0x100/0x180, sprintf)       [med-high]
 *   0x80099410 shop_show_lv       (L) 0x8B4 level-intro panel: GetStringText/
 *                                     GetStringListText/DrawStringText           [high]
 *   0x80099CC4 show_piles         (L) 0x198 pile blit verts (no text)            [med-high]
 *   0x80099E5C show_gold          (L) 0x250 pile blits + sprintf'd gold amount   [med-high]
 *   0x8009A0AC fn_8009A0AC            0x268 world-space setup (ResolveWorldData,
 *                                     gCurLevel) -- likely init_piles            [med, kept fn_]
 *   0x8009A314 shop_setup         (L) 0x5F4 build screen: LoadTowerAndSelect,
 *                                     MBNewBlit/mbNewBlitSized, per-player
 *                                     display, ItemDefValid/PlayerItemState      [high]
 *   0x8009A908 init_shop          (G) 0x140 music + audio reset + SelectLoadStart;
 *                                     DoOptions/gamemain callers                 [high]
 *   0x8009AA48 do_shopping        (L) 0xD98 buy/sell driver: jumptable, RandInt
 *                                     buy voices, ItemDefValid/PlayerItemState   [high]
 *   0x8009B7E0 write_shop_menu    (L) 0x644 menu text: DrawGlowTextMLines,
 *                                     MBFontMsgSetAlpha, calls calc_shop_ypos    [high]
 *   0x8009BE24 calc_shop_ypos     (L) 0x130 y layout via TextHeightMLines        [high]
 *   0x8009BF54 ShopLoadData       (G) 0x19C AllocFile("shop.wad") + MBSetupWad
 *                                     + MBGetFromWad                             [high, pre-named]
 *   0x8009C0F0 calculate_player_shopping_parameters (L) 0x138 per-player
 *                                     (mulli 0x335C = gPlayerRecords stride),
 *                                     jumptable_8012322C                         [high]
 *
 * Data (Xbox SHOP.OBJ data roster: shop_lx/cx/rx, pcol, buyvoice, cursor_y,
 * top_y, scroll_flag, curfreeze, last_available, pile_* , shop_minprice,
 * shop_dim_dy, shop_topy, shop_boty, shop_did_setup, shop_from_menu,
 * PILE_TOPY/PILE_BOTY/PILE_MINY, timer): the GC equivalents live in the
 * lbl_80344C00..lbl_80344C18 sbss cluster + lbl_803448xx and the coordinate
 * tables listed above; left under lbl_ names until pinned individually.
 *
 * gPlayerRecords = gPlayers (shared [4][0x335C] progress array) is
 * referenced throughout; per project policy it stays lbl_ until the
 * coordinated rename.
 *
 * All bodies are doc-only this pass (the final DOL links the original bytes
 * for this range via the splits.txt claim; this file exists to own the claim
 * and carry the recovered map).
 */

#include "types.h"

extern u8 gPlayers[];
extern s32 lbl_8028A520[];
extern void* lbl_8028B120[];
extern s32 lbl_803448A0;
extern s32 lbl_803448A4;
extern s32 lbl_80344C04;
extern s32 lbl_80344C08;
extern s32 lbl_80344C0C;
extern s32 lbl_80344C10;
extern u8* lbl_80344C14;
extern s32 lbl_80344C18;
extern s32 lbl_80344808;
extern s32 lbl_8034481C;
extern s32 gGameMode;
extern s32 gGameBusy;
extern s32 good_wiz_exit_timer;
extern u8 lbl_802897D0[];

extern void* AllocFile(char* wad, char* name);
extern s32 MBSetupWad(s32* wad, s32 base);
extern s32 MBGetFromWad(s32* wad, s32 key, s32* sizeOut);
extern f32 player_max_health(void* player);
extern s32 MBBlitGetTex(void* blit);
extern s32 TextHeightMLines(f32 scale, s32 font, char* str);
extern void towerUpdateCurWorldObj(void);
extern void PlayersRestoreHealth(void);
extern void AudioStopSelect(void);
extern void AudioSelectReset(void);
extern void AudioReset(s32 force);
extern void AudioEmptyCb2(void);
extern void ShopMusicStart(void);
extern void fn_800BC418(s32 startLine, s32 count);
extern void fn_80053D08(s32 a, s32 b, s32 c);
extern void TransitionBlitShow(s32 arg);
extern void setup_player_display(s32 player);
extern void SelectLoadStart(void);
extern void fn_80053C70(void);
extern u32 gFrameTicks;
extern s32 lbl_80343E0C;    /* pile meter top    */
extern s32 lbl_80343E10;    /* pile meter bottom */
extern s32 lbl_80343E14;    /* pile meter base   */
extern f64 lbl_803483B0;    /* pile height -> vert scale */
extern f32 lbl_8034832C;    /* pile vert coord A */
extern f32 lbl_80348328;    /* pile vert coord B */
extern void mbBlitInit3414(void* blit, s32 hide);
extern void mbBlitProject(void* blit, s32 w, s32 h);
extern void mbBlitCalcY(void* blit, s32 y);
extern void mbBlitSetupVerts(void* blit, f32 a, f32 b, f32 c, f32 d);

extern s32 SelectLoadDone(void);
extern void WritePlayerInfo(s32 player);
extern void LoadPlyrData(s32 player, s32 pad, s32 mode);
extern void TransitionBlitHide(void);
extern s32 ShowLoading(void);
extern void fn_8009FCA8(s32 flag);
extern s32 sndFxUpdate(s32 mode);
extern void AudioStopMusicA(void);
extern void init_panel_blits(s32 player);
extern void fn_8009C460(s32 mode);
extern s32 draw_inventory_panel(s32 player);
extern void init_inventory_panel(s32 player);
extern void end_inventory_panel(s32 player);
extern void MBRemoveBlit(void* blit);
extern s32 lbl_80344A28;
extern char lbl_8011495C[];    /* "Loading..." */
extern f32 lbl_80348328;
extern f32 lbl_8034832C;
extern s32 lbl_80344C0C;
extern s32 lbl_80343E04;
extern void DrawGlowText(f32 scale, s32 y, s32 x, char* txt);
extern struct PadStateView lbl_80240E30[];
extern void AudioCursorSelect(void);
extern s32 lbl_803448C4;
extern s32 lbl_803448C8;
s32 show_gold(s32 col);
s32 show_piles(s32 col);
void fn_8009A0AC(s32 col);
static s32 shop_show_final_stats(u8* pl);
static s32 shop_show_lv(u8* pl, s32 mode);
static s32 do_shopping_8009AA48(s32 player);
static void shop_setup(void);

/* Master shop state machine: runs each active player's shop flow and
 * returns nonzero when everyone is done. */
s32 do_shop(void)
{
    u8* page = lbl_802897D0;
    s32 result = 1;
    s32 statsFlag = 0;
    s32 loaded;

    loaded = SelectLoadDone();
    if (loaded != 0 && lbl_80344C08 == 0) {
        shop_setup();
    }
    WritePlayerInfo(-1);
    if (loaded == 0) {
        TransitionBlitShow(0);
        DrawGlowText(lbl_80348328, 340, 260, lbl_8011495C);
        return 0;
    }
    if (lbl_80344C18 == 0) {
        TransitionBlitHide();
        if (gGameBusy != 0 || lbl_80344A28 != 0) {
            result = 0;
        } else {
            u8* pads = (u8*)lbl_80240E30;
            u8* pl = gPlayers;
            f32 kHalf = lbl_8034832C;
            s32 i;
            s32 o60 = 0;
            s32 o256 = 0;
            s32 o12 = 0;
            s32 o13148 = 0;
            s32 o24 = 0;
            s32 o4 = 0;
            for (i = 0; i < 4; i++, o60 += 60, o256 += 256, o12 += 12,
                o13148 += 13148, o24 += 24, o4 += 4, pl += 13148) {
                volatile s32 leave;
                s32 j;
                if (*(s32*)(pl + 232) != 1 && *(s32*)(pl + 232) != 5) {
                    continue;
                }
                leave = 0;
                LoadPlyrData(i, *(s32*)(pl + 12), 0);
                switch (*(u32*)(pl + 2660)) {
                case 0:
                    *(s32*)(page + o4 + 16) = 0;
                    *(s32*)(page + o4) = 0;
                    *(s32*)(page + o4 + 32) = 0;
                    fn_8009A0AC(i);
                    *(s32*)(pl + 2668) = 0;
                    *(f32*)(pl + 2672) = kHalf;
                    *(f32*)(pl + 2676) = kHalf;
                    *(f32*)(pl + 2680) = kHalf;
                    *(f32*)(pl + 2684) = kHalf;
                    if (lbl_80344C0C != 0) {
                        void** q = (void**)(page + o24);
                        s32 cnt;
                        s32 koff;
                        u8* slot;
                        if (*(q += 1876) != 0) {
                            mbBlitInit3414(q[0], 1);
                        }
                        if (q[1] != 0) {
                            mbBlitInit3414(q[1], 1);
                        }
                        if (q[2] != 0) {
                            mbBlitInit3414(q[2], 1);
                        }
                        cnt = 0;
                        koff = 0;
                        for (j = 3; j != 0; j--) {
                            if (*(s32*)(page + o12 + 224 + koff) == 0) {
                                break;
                            }
                            cnt++;
                            koff += 4;
                        }
                        slot = page + o12 + cnt * 4;
                        *(s32*)(slot + 128) = *(s32*)(slot + 80);
                        *(s32*)(slot + 80) =
                            *(s32*)(gPlayers + o13148 + 7876);
                        *(s32*)(pl + 2660) = 6;
                    } else {
                        *(s32*)(pl + 2660) += 1;
                    }
                    break;
                case 1:
                case 2:
                case 3:
                    if (show_gold(i) == 0) {
                        if (*(u32*)(pads + *(s32*)pl * 60 + 8) &
                            0x2000000) {
                            void** q;
                            AudioCursorSelect();
                            q = (void**)(page + o24);
                            if (*(q += 1876) != 0) {
                                mbBlitInit3414(q[0], 1);
                            }
                            if (q[1] != 0) {
                                mbBlitInit3414(q[1], 1);
                            }
                            if (q[2] != 0) {
                                mbBlitInit3414(q[2], 1);
                            }
                            *(s32*)(pl + 2660) = 4;
                            *(s32*)(pl + 2668) = 0;
                        }
                    } else {
                        statsFlag = 1;
                    }
                    break;
                case 4:
                    if (shop_show_lv(pl, 0) != 0) {
                        if (lbl_803448C8 == 8 && lbl_803448C4 == 3) {
                            *(s32*)(pl + 2660) = 20;
                        } else {
                            *(s32*)(pl + 2660) += 1;
                        }
                    }
                    break;
                case 5: {
                    s32 cnt = 0;
                    s32 koff = 0;
                    u8* slot;
                    for (j = 3; j != 0; j--) {
                        if (*(s32*)(page + o12 + 224 + koff) == 0) {
                            break;
                        }
                        cnt++;
                        koff += 4;
                    }
                    slot = page + o12 + cnt * 4;
                    *(s32*)(slot + 128) = *(s32*)(slot + 80);
                    *(s32*)(slot + 80) = *(s32*)(gPlayers + o13148 + 7876);
                    *(s32*)(pl + 2660) += 1;
                    break;
                }
                case 6:
                    if (lbl_80344C0C == 2) {
                        {
                            u32 k;
                            s32 off = 0;
                            u8* blitBase = page + o24 + 7504;
                            for (k = 0; k < 6; k++, off += 4) {
                                void** q = (void**)(blitBase + off);
                                if (*q != 0) {
                                    MBRemoveBlit(*q);
                                    *q = 0;
                                }
                            }
                        }
                        for (j = 0; j < lbl_80344C10; j++) {
                            void** q =
                                (void**)(page + o256 + 6480 + j * 4);
                            if (*q != 0) {
                                MBRemoveBlit(*q);
                                *q = 0;
                            }
                        }
                        *(s32*)(pl + 2660) = 9;
                        break;
                    }
                    if (*(s32*)(pl + 232) == 1 || *(s32*)(pl + 232) == 5) {
                        {
                            u8* playerBlits =
                                page + *(s32*)pl * 24;
                            mbBlitInit3414(
                                *(void**)(playerBlits + 7516), 0);
                        }
                        mbBlitInit3414(
                            *(void**)(page + *(s32*)pl * 24 + 7520), 0);
                        {
                            void* b;
                            for (j = 0; j < lbl_80344C10; j++) {
                                if ((b = *(void**)(page + (*(s32*)pl << 8) +
                                                   6480 + j * 4)) != 0) {
                                    mbBlitInit3414(b, 1);
                                }
                            }
                        }
                    }
                    *(s32*)(pl + 2660) += 1;
                    /* fall through */
                case 7:
                    show_piles(i);
                    if (do_shopping_8009AA48(i) != 0) {
                        {
                            u32 k;
                            for (k = 0; k < 6; k++) {
                                void** q =
                                    (void**)(page + o24 + 7504 + k * 4);
                                if (*q != 0) {
                                    MBRemoveBlit(*q);
                                    *q = 0;
                                }
                            }
                        }
                        for (j = 0; j < lbl_80344C10; j++) {
                            void** q =
                                (void**)(page + o256 + 6480 + j * 4);
                            if (*q != 0) {
                                MBRemoveBlit(*q);
                                *q = 0;
                            }
                        }
                        *(s32*)(pl + 2660) += 1;
                    }
                    break;
                case 8:
                    if (shop_show_lv(pl, 1) != 0) {
                        *(s32*)(pl + 2660) += 1;
                    }
                    break;
                case 9:
                    if (lbl_80344C0C == 1) {
                        *(s32*)(pl + 2660) = 100;
                    } else {
                        init_panel_blits(i);
                        fn_8009C460(2);
                        *(s32*)(pl + 2660) += 1;
                    }
                    break;
                case 10:
                    draw_inventory_panel(i);
                    {
                        u8* playerPad = pads + o60;
                        if (*(u32*)(playerPad + 8) & 0x2000000) {
                            AudioCursorSelect();
                            init_inventory_panel(i);
                            *(s32*)(pl + 2660) += 1;
                        }
                    }
                    break;
                case 11:
                    if (draw_inventory_panel(i) != 0) {
                        end_inventory_panel(i);
                        *(s32*)(pl + 2660) += 1;
                    }
                    break;
                case 12:
                    *(s32*)(pl + 2660) = 100;
                    break;
                case 20:
                    if (shop_show_final_stats(pl) != 0) {
                        *(s32*)(pl + 2660) += 1;
                    }
                    break;
                default:
                    *(s32*)(pl + 2660) = 100;
                    /* fall through */
                case 100:
                    leave = 1;
                    break;
                }
                if (leave != 0) {
                    *(s32*)(pl + 232) = 5;
                } else {
                    result = 0;
                }
            }
            fn_8009FCA8(statsFlag);
        }
    }
    if (ShowLoading() == 0) {
        if (result != 0) {
            DrawGlowText(lbl_80348328, 340, 260, lbl_8011495C);
        }
        result = 0;
    }
    if (sndFxUpdate(1) != 0) {
        result = 0;
    }
    if (result != 0) {
        AudioStopMusicA();
        if (lbl_80344C0C == 0) {
            result = 2;
        }
    }
    return result;
}

/* Animate one player gold-pile column toward its target height; returns 0
 * while the pile is still sinking (drives the count-down loop). */
s32 show_piles(s32 col)
{
    u8* tbl = lbl_802897D0;
    u8* counts = tbl + col * 12 + 224;
    s32 result = 1;
    u8* pl = gPlayers + col * 13148;
    s32 adj = gFrameTicks + (gFrameTicks >> 1);
    s32 range = lbl_80343E10 - lbl_80343E0C - lbl_80343E14;
    s32 n = 3;
    s32 count = 0;
    s32 off = 0;
    u8* blit;
    u8* slot;
    s32 gold;
    s32 cur;
    s32 tgt;
    u8* curp;
    f32 height;

    for (; n != 0; n--) {
        if (*(s32*)(counts + count * 4) == 0) {
            break;
        }
        count++;
    }

    blit = *(u8**)(tbl + col * 24 + 7504);
    if (blit == 0) {
        return 1;
    }
    gold = *(s32*)(pl + 7876);
    slot = tbl + col * 12 + count * 4;
    curp = slot + 128;
    cur = *(s32*)(slot + 128);
    tgt = lbl_80343E14 + gold * range / (*(s32*)(slot + 80) + 1);
    if (gold == 0) {
        mbBlitInit3414(blit, 1);
    } else {
        mbBlitInit3414(blit, 0);
        if (tgt < cur) {
            cur = cur - adj;
            result = 0;
            if (cur < tgt) {
                cur = tgt;
            }
        } else {
            cur = tgt;
        }
        height = (f32)((f32)cur * lbl_803483B0);
        mbBlitCalcY(blit, lbl_80343E10 - cur);
        mbBlitProject(blit, 0, cur);
        mbBlitSetupVerts(blit, lbl_8034832C, lbl_80348328, lbl_8034832C,
                         height);
        *(s32*)curp = cur;
    }
    return result;
}

extern u8 lbl_80122ED0[];   /* shop draw-layout page (.data)          */
extern char lbl_803483B8[7]; /* "%s: %d" gold-line fmt (sdata2)        */
extern char lbl_801149F4[]; /* "Continue" label                        */
extern char lbl_80348368[8]; /* "Stats" label (sdata, color-code +2)    */
extern f32 lbl_80348360;    /* gold-line text scale                    */
extern f32 lbl_80348364;    /* stats text scale                        */
extern void* lbl_80344E48;  /* continue-arrow texture                  */
extern int sprintf(char* buf, const char* fmt, ...);
extern void DrawGlowText(f32 scale, s32 y, s32 x, char* txt);
extern s32 DrawTextKeepScale(f32 scale, s32 y, s32 x, s32 font, s32 color,
                             char* txt);
extern void* MBNewTempBlit(void* tex, int x, int y, int w, int h);

/* Draw the three per-pile gold totals (rising-pile animation + glow text),
 * the Continue prompt once everything settled, and the Stats footer.
 * Returns 1 + the index of the pile still animating, 0 when done. */
s32 show_gold(s32 col)
{
    u8* dpage = lbl_80122ED0;
    u8* tbl = lbl_802897D0;
    s32 adj = gFrameTicks + (gFrameTicks >> 1);
    f64 heightScale = lbl_803483B0;
    u8* blits = tbl + col * 24 + 7504;
    u8* counts = tbl + col * 12 + 224;
    u8* shownT = tbl + col * 12 + 128;
    u8* targT = tbl + col * 12 + 80;
    u8* valT = tbl + col * 12 + 176;
    s32 ypos = *(s32*)(dpage + col * 4 + 112);
    s32 xbase = *(s32*)(dpage + col * 4 + 96);
    s32 done = 0;
    s32 k = 0;
    s32 fontY;
    u8* blit;
    s32 tgt;
    s32 shown;
    s32 grew;
    u8* item;
    s32 drawX;
    f32 height;
    char buf[32];

    for (; k < 3; k++) {
        tgt = *(s32*)(targT + k * 4);
        shown = *(s32*)(shownT + k * 4);
        blit = *(u8**)(blits + *(s32*)(counts + k * 4) * 4);
        grew = 0;
        if (done != 0) {
            mbBlitInit3414(blit, 1);
        } else {
            mbBlitInit3414(blit, 0);
            if (shown < tgt) {
                shown += adj;
                if (shown > tgt) {
                    shown = tgt;
                }
                done = k + 1;
                grew = 1;
            } else {
                shown = tgt;
            }
            height = (f32)((f32)shown * heightScale);
            mbBlitCalcY(blit, lbl_80343E10 - shown);
            mbBlitProject(blit, 0, shown);
            mbBlitSetupVerts(blit, lbl_8034832C, lbl_80348328, lbl_8034832C,
                             height);
            *(s32*)(shownT + k * 4) = shown;
        }
        item = dpage + *(s32*)(counts + k * 4) * 4;
        drawX = *(s32*)(item + 172);
        sprintf(buf, lbl_803483B8, *(u32*)(item + 160), *(s32*)(valT + k * 4));
        if (grew != 0) {
            DrawGlowText(lbl_80348360, xbase + 16, drawX, buf);
        } else {
            DrawTextKeepScale(lbl_80348360, xbase + 16, drawX, 6,
                              0xFFFFFF, buf);
        }
    }
    fontY = *(s32*)(dpage + 184);
    if (done == 0) {
        DrawGlowText(lbl_80348360, xbase + 32, fontY, lbl_801149F4);
        MBNewTempBlit(lbl_80344E48, xbase + 16, fontY - 3, 20, 20);
    }
    DrawTextKeepScale(lbl_80348364, -ypos, 8, 6, 0, lbl_80348368);
    return done;
}

extern u8* gCurLevel;
extern char lbl_80114918[];  /* final-stats string pool                */
extern s32 lbl_80122F30[];   /* per-player stats x column              */
extern s32 lbl_80122F40[];   /* per-player stats y column              */
extern f32 lbl_80348330;     /* stats line text scale                  */
extern f32 lbl_80348334;     /* stats title text scale                 */
extern char lbl_80348338[8];  /* "%d" fmt (sdata)                       */
extern f64 lbl_80348340;     /* seconds per day                        */
extern f64 lbl_80348348;     /* seconds per hour                       */
extern f64 lbl_80348350;     /* seconds per minute                     */
extern char lbl_80348358[8]; /* "%d Days" fmt (sdata2)                 */
typedef struct PadStateView { u8 _0[8]; u32 buttons; u8 _c[48]; } PadStateView;
extern PadStateView lbl_80240E30[]; /* pad states, stride 60, buttons @+8 (CTL in controls.c) */
extern void AudioCursorSelect(void);

/* Staged end-of-game Final Stats screen: reveals one glowing line per
 * 60-tick window, then the playtime breakdown and the Continue prompt.
 * Returns 1 once the player confirms with Start. */
#pragma opt_lifetimes off
static s32 shop_show_final_stats(u8* pl)
{
    s32 y2;
    s32 hours;
    char* pool;
    s32 t;
    s32 nx;
    s32 x8;
    s32 done;
    u8* stats;
    s32 timer;
    s32 xbase;
    s32 ypos;
    s32 days;
    s32 mins;
    f32 scale;
    f32 secs;
    u8 _pad8[8];
    char buf[36];

    pool = lbl_80114918;
    done = 0;
    stats = pl + *(s32*)(pl + 12) * 28 + 3088;
    timer = *(s32*)(pl + 2668);
    scale = lbl_80348330;
    xbase = lbl_80122F30[*(s32*)pl];
    ypos = lbl_80122F40[*(s32*)pl];
    x8 = xbase + 8;
    t = timer & 0xFFFF;

    if (t < 0xF000) {
        *(s32*)(pl + 2668) = timer + gFrameTicks;
    }
    nx = -ypos;
    DrawGlowText(lbl_80348334, nx, 32, pool + 80);
    if (t > 90 && t < 150) {
        DrawGlowText(scale, nx, 60, pool + 92);
    } else {
        DrawTextKeepScale(scale, nx, 60, 6, 0xFFFFFF, pool + 92);
    }
    if (t > 90) {
        sprintf(buf, lbl_80348338, *(s32*)stats);
        DrawGlowText(scale, nx, 78, buf);
    }
    if (t > 150 && t < 210) {
        DrawGlowText(scale, nx, 98, pool + 108);
        y2 = 116;
        DrawGlowText(scale, nx, 116, pool + 120);
    } else {
        DrawTextKeepScale(scale, nx, 98, 6, 0xFFFFFF, pool + 108);
        y2 = 116;
        DrawTextKeepScale(scale, nx, 116, 6, 0xFFFFFF, pool + 120);
    }
    if (t > 150) {
        sprintf(buf, lbl_80348338, *(s32*)(stats + 16));
        DrawGlowText(scale, nx, y2 + 18, buf);
    }
    if (t > 210 && t < 270) {
        DrawGlowText(scale, nx, y2 + 38, pool + 132);
    } else {
        DrawTextKeepScale(scale, nx, y2 + 38, 6, 0xFFFFFF, pool + 132);
    }
    if (t > 210) {
        sprintf(buf, lbl_80348338, *(s32*)(stats + 20));
        DrawGlowText(scale, nx, y2 + 56, buf);
    }
    if (t > 270 && t < 330) {
        DrawGlowText(scale, nx, y2 + 76, pool + 144);
    } else {
        DrawTextKeepScale(scale, nx, y2 + 76, 6, 0xFFFFFF, pool + 144);
    }
    secs = *(f32*)(stats + 24);
    days = (s32)(secs / lbl_80348340);
    secs = (f32)-(lbl_80348340 * (f32)(s32)(secs / lbl_80348340) - secs);
    hours = (s32)(secs / lbl_80348348);
    secs = (f32)-(lbl_80348348 * (f32)(s32)(secs / lbl_80348348) - secs);
    mins = (s32)(secs / lbl_80348350);
    if (t > 270) {
        sprintf(buf, lbl_80348358, days);
        DrawGlowText(scale, nx, y2 + 94, buf);
        sprintf(buf, pool + 160, hours);
        DrawGlowText(scale, nx, y2 + 112, buf);
        sprintf(buf, pool + 172, mins);
        DrawGlowText(scale, nx, y2 + 130, buf);
    }
    if (t >= 330) {
        done = 1;
    }
    if (done != 0) {
        if ((lbl_80240E30[*(s32*)pl].buttons & 0x2000000) != 0) {
            AudioCursorSelect();
            *(s32*)(pl + 2668) = 0;
            return 1;
        }
        MBNewTempBlit(lbl_80344E48, x8 + 8, 280, 16, 16);
        DrawGlowText(lbl_80348360, x8 + 32, 280, pool + 184);
    }
    DrawTextKeepScale(lbl_80348364, nx, 8, 6, 0, lbl_80348368);
    return 0;
}
#pragma opt_lifetimes reset

extern s32 lbl_803448C4;    /* current world number  */
extern s32 lbl_803448C8;    /* current level number  */
extern s32 sWorldDataConst; /* shop world-data key   */
extern void ResolveWorldData(s32 key);

extern s32 ExpToLevel(s32 exp);
extern void AudioExp(s32 pad, s32 mode);
extern void check_player_atts(u8* pl, s32 cls, u8* expslot);
extern char* GetStringText(s32 id, s32 sub, s32 mode);
extern char* GetStringListText(s32 id, s32 sub, s32 line, s32 mode);
extern void DrawStringText(s32 x, s32 y, s32 font, u32 rgb, s32 msg, ...);
extern f32 lbl_80348330;    /* stat row text scale   */
extern f32 lbl_80348378;    /* level number scale    */
extern f32 lbl_8034837C;    /* level name scale      */
extern char lbl_80348380[8]; /* stat row 2 label      */
extern char lbl_80348388[8]; /* stat row 3 label      */
extern char lbl_80348390[8]; /* stat row 4 label      */
extern char lbl_80348398[4]; /* health label line 1   */
extern char lbl_8034839C[8]; /* health label line 2   */
extern f64 lbl_803483A8;    /* health-per-level      */
extern f64 lbl_80348370;    /* int-conv magic        */
extern s32 lbl_80122F30[];  /* per-player panel base x */

/* Level-up / level-intro panel for one shop player; returns 1 when the
 * player confirms past it. */
static s32 shop_show_lv(u8* pl, s32 final)
{
    char* fmts;
    u8* exps;
    s32 xcol;
    s32 anim;
    s32 x1;
    s32 x2;
    s32 rowgate;
    s32 done;
    s32 lvl;
    s32 tick;
    s32 hl;
    s32 chg;
    f32 kScale;
    u8 _spare[24];
    f32 d1;
    f32 d2;
    f32 d3;
    f32 d4;
    char buf[20];

    kScale = lbl_80348330;
    exps = pl + *(s32*)(pl + 12) * 24 + 7900;
    fmts = lbl_80114918;
    x1 = *(s32*)((u8*)lbl_80122F30 + (*(s32*)pl << 2)) + 8;
    x2 = *(s32*)((u8*)lbl_80122F30 + (*(s32*)pl << 2)) + 88;
    xcol = lbl_80122F40[*(s32*)pl];
    tick = *(s32*)(pl + 2668) & 0xFFFF;
    done = 0;
    lvl = ExpToLevel(*(s32*)exps);
    if (final == 0) {
        if (lvl == *(s32*)(pl + 13092)) {
            return 1;
        }
        anim = 1;
        rowgate = 90;
    } else {
        lvl = *(s32*)(pl + 13092);
        anim = 0;
        rowgate = 30;
    }
    if (*(s32*)(pl + 2668) == 0) {
        if (final == 0) {
            AudioExp(*(s32*)pl, 1);
        }
        *(s32*)(pl + 2668) += 1;
    }
    if (tick < 0xF000) {
        *(s32*)(pl + 2668) += gFrameTicks;
    }
    sprintf(buf, fmts + 196, *(s32*)(pl + 13092));
    if (anim != 0) {
        DrawGlowText(lbl_80348378, -xcol, 32, buf);
    } else {
        DrawTextKeepScale(lbl_80348378, -xcol, 32, 6, 0xFFFFFF, buf);
    }
    {
        s32 lv = *(s32*)(pl + 13092);
        s32 tens = lv / 10;
        char* name;
        if (lv == 99) {
            name = GetStringText(21, 0, 0);
        } else if (lv < 10) {
            name = GetStringText(23, *(s32*)(pl + 12), 0);
        } else {
            name = GetStringListText(0, *(s32*)(pl + 12), tens >> 1, 0);
        }
        xcol = -xcol;
        DrawTextKeepScale(lbl_8034837C, xcol, 64, 6, 0xFFFFFF, name);
    }
    {
        s32 old = *(s32*)(pl + 13092);
        *(s32*)(pl + 13092) = lvl;
        check_player_atts(pl, *(s32*)(pl + 12), exps);
        d1 = *(f32*)(pl + 244) - *(f32*)(pl + 2672);
        d2 = *(f32*)(pl + 248) - *(f32*)(pl + 2676);
        d3 = *(f32*)(pl + 252) - *(f32*)(pl + 2680);
        d4 = *(f32*)(pl + 256) - *(f32*)(pl + 2684);
        *(s32*)(pl + 13092) = old;
        check_player_atts(pl, *(s32*)(pl + 12), 0);
    }
    chg = (d1 != *(f32*)(pl + 244));
    if (d1 != *(f32*)(pl + 244) && tick > rowgate && tick < rowgate + 60) {
        DrawGlowText(kScale, x1, 96, fmts + 208);
    } else {
        DrawTextKeepScale(kScale, x1, 96, 6, 0xFFFFFF, fmts + 208);
    }
    if (chg != 0 && tick > rowgate) {
        sprintf(buf, lbl_80348338, (s32)*(f32*)(pl + 244));
        DrawGlowText(kScale, x2, 96, buf);
    } else {
        sprintf(buf, lbl_80348338, (s32)d1);
        DrawTextKeepScale(kScale, x2, 96, 6, 0xFFFFFF, buf);
    }
    if (chg != 0) {
        rowgate += 60;
    }
    chg = (d2 != *(f32*)(pl + 248));
    if (d2 != *(f32*)(pl + 248) && tick > rowgate && tick < rowgate + 60) {
        DrawGlowText(kScale, x1, 116, lbl_80348380);
    } else {
        DrawTextKeepScale(kScale, x1, 116, 6, 0xFFFFFF, lbl_80348380);
    }
    if (chg != 0 && tick > rowgate) {
        sprintf(buf, lbl_80348338, (s32)*(f32*)(pl + 248));
        DrawGlowText(kScale, x2, 116, buf);
    } else {
        sprintf(buf, lbl_80348338, (s32)d2);
        DrawTextKeepScale(kScale, x2, 116, 6, 0xFFFFFF, buf);
    }
    if (chg != 0) {
        rowgate += 60;
    }
    chg = (d3 != *(f32*)(pl + 252));
    if (d3 != *(f32*)(pl + 252) && tick > rowgate && tick < rowgate + 60) {
        DrawGlowText(kScale, x1, 136, lbl_80348388);
    } else {
        DrawTextKeepScale(kScale, x1, 136, 6, 0xFFFFFF, lbl_80348388);
    }
    if (chg != 0 && tick > rowgate) {
        sprintf(buf, lbl_80348338, (s32)*(f32*)(pl + 252));
        DrawGlowText(kScale, x2, 136, buf);
    } else {
        sprintf(buf, lbl_80348338, (s32)d3);
        DrawTextKeepScale(kScale, x2, 136, 6, 0xFFFFFF, buf);
    }
    if (chg != 0) {
        rowgate += 60;
    }
    chg = (d4 != *(f32*)(pl + 256));
    if (d4 != *(f32*)(pl + 256) && tick > rowgate && tick < rowgate + 60) {
        DrawGlowText(kScale, x1, 156, lbl_80348390);
    } else {
        DrawTextKeepScale(kScale, x1, 156, 6, 0xFFFFFF, lbl_80348390);
    }
    if (chg != 0 && tick > rowgate) {
        sprintf(buf, lbl_80348338, (s32)*(f32*)(pl + 256));
        DrawGlowText(kScale, x2, 156, buf);
    } else {
        sprintf(buf, lbl_80348338, (s32)d4);
        DrawTextKeepScale(kScale, x2, 156, 6, 0xFFFFFF, buf);
    }
    if (chg != 0) {
        rowgate += 60;
    }
    if (final != 0) {
        exps = (u8*)0;
    } else {
        exps = (u8*)1;
    }
    if ((s32)exps != 0 && tick > rowgate && tick < rowgate + 60) {
        DrawGlowText(kScale, x1, 188, lbl_80348398);
        DrawGlowText(kScale, x1, 204, lbl_8034839C);
    } else {
        DrawTextKeepScale(kScale, x1, 188, 6, 0xFFFFFF, lbl_80348398);
        DrawTextKeepScale(kScale, x1, 204, 6, 0xFFFFFF, lbl_8034839C);
    }
    if ((s32)exps != 0 && tick > rowgate) {
        sprintf(buf, lbl_80348338, (s32)player_max_health(pl));
        DrawGlowText(kScale, x2, 196, buf);
        *(u32*)(pl + 2668) |= 0x10000;
    } else {
        s32 v = (s32)player_max_health(pl);
        if (final == 0) {
            v = (s32)(v - lbl_803483A8 * (*(s32*)(pl + 13092) - lvl));
        }
        sprintf(buf, lbl_80348338, v);
        DrawTextKeepScale(kScale, x2, 196, 6, 0xFFFFFF, buf);
    }
    if ((s32)exps != 0) {
        rowgate += 60;
    }
    if (final == 0) {
        if (*(s32*)(pl + 13092) == 25) {
            DrawStringText(xcol, 224, 6, 0xFF80C0, 184, *(s32*)(pl + 8));
        }
        if (*(s32*)(pl + 13092) == 50) {
            DrawStringText(xcol, 224, 6, 0xFF80C0, 185, *(s32*)(pl + 8));
        }
    }
    if (tick >= rowgate) {
        done = 1;
    }
    if (done != 0) {
        if (lbl_80240E30[*(s32*)pl].buttons & 0x2000000) {
            AudioCursorSelect();
            *(s32*)(pl + 2668) = 0;
            return 1;
        }
        MBNewTempBlit(lbl_80344E48, x1 + 8, 280, 16, 16);
        DrawGlowText(lbl_80348360, x1 + 32, 280, fmts + 184);
    }
    DrawTextKeepScale(lbl_80348364, xcol, 8, 6, 0, lbl_80348368);
    return 0;
}

/* Compute the three end-of-level pile stats for one player (gold earned,
 * kills, second currency), rank them, and seed the pile animation tables. */
void fn_8009A0AC(s32 col)
{
    u8* tbl = lbl_802897D0;
    u8* pl = gPlayers + col * 13148;
    s32 range = lbl_80343E10 - lbl_80343E0C;
    u8* lvl;
    s32 cls;
    s32 goldRaw;
    s32 raw2;
    s32 raw3;
    s32 statG;
    s32 stat2;
    s32 stat3;
    s32 t;
    s32 t2;
    s32 t3;
    s32 ra;
    s32 rb;
    s32 rc;
    s32 off;
    s32 i;
    u8* b240;
    u8* b24;
    u8* b28;
    u8* p;

    ResolveWorldData((lbl_803448C8 << 8) | (u8)lbl_803448C4);
    cls = *(s32*)(pl + 12);
    lvl = gCurLevel;
    goldRaw = *(s32*)(pl + 7876) -
              *(s32*)((b240 = pl + cls * 240) + 8780);
    t = goldRaw * range / (*(s32*)(lvl + 224) + 1);
    b28 = pl + cls * 28;
    raw2 = (*(s32*)(b28 + 3088) + *(s32*)(b28 + 3104)) -
           (*(s32*)(b28 + 8284) + *(s32*)(b28 + 8300));
    raw3 = *(s32*)(pl + 7872) -
           *(s32*)((b24 = pl + cls * 24) + 7900);
    if (t < 64) {
        statG = 64;
    } else if (t > range) {
        statG = range;
    } else {
        statG = t;
    }
    t2 = raw2 * range / (*(s32*)(lvl + 228) + 1);
    if (t2 < 64) {
        stat2 = 64;
    } else if (t2 > range) {
        stat2 = range;
    } else {
        stat2 = t2;
    }
    t3 = raw3 * range / (*(s32*)(lvl + 232) + 1);
    if (t3 < 64) {
        stat3 = 64;
    } else if (t3 > range) {
        stat3 = range;
    } else {
        stat3 = t3;
    }

    if (statG >= stat3 && statG >= stat2) {
        if (stat3 >= stat2) {
            ra = 0;
            rb = 1;
            rc = 2;
        } else {
            ra = 0;
            rc = 1;
            rb = 2;
        }
    } else if (stat3 >= statG && stat3 >= stat2) {
        if (statG >= stat2) {
            rb = 0;
            ra = 1;
            rc = 2;
        } else {
            rb = 0;
            rc = 1;
            ra = 2;
        }
    } else if (statG >= stat3) {
        rc = 0;
        ra = 1;
        rb = 2;
    } else {
        rc = 0;
        rb = 1;
        ra = 2;
    }

    off = col * 12;
    p = tbl + off + 224;
    *(s32*)(p + ra * 4) = (i = 0);
    *(s32*)(p + rb * 4) = 2;
    *(s32*)(p + rc * 4) = 1;
    p = tbl + off;
    for (t = 3; t != 0; t--) {
        *(s32*)(p + 128 + i) = lbl_80343E14;
        i += 4;
    }
    p = tbl + off + 80;
    *(s32*)(p + ra * 4) = statG;
    *(s32*)(p + rb * 4) = stat3;
    *(s32*)(p + rc * 4) = stat2;
    p = tbl + off + 176;
    *(s32*)(p + ra * 4) = goldRaw;
    *(s32*)(p + rb * 4) = raw3;
    *(s32*)(p + rc * 4) = raw2;
    ResolveWorldData(sWorldDataConst);
}

/* Enter the between-level shop and launch its asynchronous front-end load. */
extern void LoadTowerAndSelect(void);
extern void* mbNewBlitSized(char* name, s32 tex, s32 x, s32 w, s32 h);
extern void* MBNewBlit(void* def, s32 tex, s32 mode);
extern void mbBlitCvtCoord(void* blit, f32 c);
extern void mbBlitUpdateEntry(void* blit, s32 a, s32 b);
extern void MBBlitSetColor4(void* blit, u32 a, u32 b, u32 c, u32 d);
extern void mbBlitCalcWidth(void* blit, s32 x, s32 w, f32 h);
extern s32 ItemDefValid(u8* entry);
extern s32 PlayerItemState(s32 player, u8* entry);
extern s32 AudioFindPlayerSlot(s32 player, s32 a, s32 b);
extern void WritePlayerInfo(s32 player);
extern s32 lbl_80344C00;
extern f64 lbl_80348370;
extern f32 lbl_803483C0;
extern f32 lbl_803483C4;
extern f32 lbl_803483C8;
static s32 calculate_player_shopping_parameters_8009C0F0(s32 player,
                                                         u8* entry);

/* Build the whole shop screen: per-player name/border/top blits, gold
 * piles, item-name blits, and the per-item owned/affordable flags. */
static void shop_setup(void)
{
    u8* tbl = lbl_80122ED0;
    char* fmts = lbl_80114918;
    u8* page = lbl_802897D0;
    s32 i;
    s32 j;
    char buf[16];
    u8 _spare[28];

    LoadTowerAndSelect();
    if (lbl_80344C18 != 0) {
        TransitionBlitShow(0);
    }
    if (lbl_80344C18 == 0) {
        s32 boff = 0;
        s32 toff = 0;
        s32 poff = 0;
        for (i = 0; i < 4; i++, boff += 20, toff += 4, poff += 13148) {
            u8* pl = gPlayers + poff;
            s32 cls = *(s32*)(pl + 4);
            s32* texp = (s32*)(tbl + toff);
            void** b = (void**)(page + boff);
            void** b1;
            void** b2;
            void** b3;
            void** b4;
            u8* clsBase;
            sprintf(buf, fmts + 232, i + 1);
            *(b += 1900) = mbNewBlitSized(buf, *(texp += 24), 0, 128, -1);
            sprintf(buf, fmts + 244, i + 1);
            *(b1 = b + 1) = mbNewBlitSized(buf, *texp, 256, 128, -1);
            sprintf(buf, fmts + 256);
            *(b2 = b + 2) = mbNewBlitSized(buf, *texp, 0, 128, -1);
            sprintf(buf, fmts + 56);
            *(b3 = b + 3) = mbNewBlitSized(buf, *texp, 256, 128, -1);
            clsBase = tbl + cls * 4;
            sprintf(buf, fmts + 268, *(char**)(clsBase + 144));
            *(b4 = b + 4) = MBNewBlit(buf, *texp + 32, 0);
            if (*(s32*)(pl + 232) == 0) {
                mbBlitInit3414(*b4, 1);
            }
            mbBlitCvtCoord(*b, lbl_803483C0);
            mbBlitCvtCoord(*b1, lbl_803483C0);
            mbBlitCvtCoord(*b2, lbl_803483C4);
            mbBlitCvtCoord(*b3, lbl_803483C4);
            mbBlitCvtCoord(*b4, lbl_803483C8);
            mbBlitUpdateEntry(*b2, -1, 0x4000);
            MBBlitSetColor4(*b2, 0x80808080, 0x80808080, 0x80808080,
                            0x80808080);
            mbBlitUpdateEntry(*b3, -1, 0x4000);
            MBBlitSetColor4(*b3, 0x80808080, 0x80808080, 0x80808080,
                            0x80808080);
        }
    }
    {
        u8* pl = gPlayers;
        s32 o24 = 0;
        s32 o4 = 0;
        s32 o256 = 0;
        s32 o768 = 0;
        for (i = 0; i < 4;
             i++, o768 += 768, o4 += 4, o256 += 256, o24 += 24, pl += 13148) {
            s32* clearBlits;
            s32* itemBlits;
            s32* available;
            s32* playerMap;
            *(s32*)(pl + 2664) = 0;
            *(s32*)(pl + 2660) = 0;
            *(s32*)(pl + 2668) = 0;
            setup_player_display(i);
            if (lbl_80344C18 != 0) {
                continue;
            }
            clearBlits = (s32*)(page + o24 + 7504);
            {
                for (j = 0; j < 6; j++) {
                    clearBlits[j] = 0;
                }
            }
            {
                s32 n = lbl_80344C10;
                itemBlits = (s32*)(page + o256 + 6480);
                available = (s32*)(page + o256 + 4432);
                playerMap = (s32*)(page + o256 + 5456);
                for (j = 0; j < n; j++) {
                    itemBlits[j] = 0;
                    available[j] = 0;
                    playerMap[j] = 0;
                }
            }
            if (*(s32*)(pl + 232) == 1 || *(s32*)(pl + 232) == 5) {
                s32* texp = (s32*)(tbl + o4);
                s32 name20 = *(texp += 24) + 20;
                u8* e = tbl;
                for (j = 0; j < 6; j++, e += 16) {
                    void* blit = mbNewBlitSized(*(char**)e, *texp,
                                                *(s32*)(e + 8), -1, -1);
                    ((void**)clearBlits)[j] = blit;
                    mbBlitCalcWidth(blit, *(s32*)(e + 4) + *texp,
                                    *(s32*)(e + 8),
                                    (f32)(*(s32*)(e + 12) + 64000));
                    mbBlitInit3414(blit, 1);
                }
                {
                    s32* count = (s32*)(page + o4);
                    *(count += 16) = 0;
                    {
                        u8* item = lbl_80344C14;
                        for (j = 0; j < lbl_80344C10; j++, item += 80) {
                            if (*(s32*)(pl + 7876) >= *(s32*)(item + 72)) {
                                *count += 1;
                            }
                            ((void**)itemBlits)[j] = MBNewBlit(item, name20, 0);
                            mbBlitInit3414(((void**)itemBlits)[j], 1);
                        }
                    }
                }
                *(s32*)(page + o256 + 3408) = -1;
                {
                    u8* item = lbl_80344C14 + 80;
                    s32* flags = (s32*)(page + o768 + 336);
                    for (j = 1; j < lbl_80344C10; j++, item += 80) {
                        s32* fli = &flags[j];
                        s32 r;
                        s32 t;
                        *fli = 0;
                        if (ItemDefValid(item) != 0) {
                            r = AudioFindPlayerSlot(
                                i,
                                *(s32*)(tbl + *(s32*)(item + 68) * 8 + 188),
                                *(s32*)(tbl + *(s32*)(item + 68) * 8 + 192));
                        } else {
                            r = -1;
                        }
                        if (r >= 0) {
                            t = 1;
                        } else if (PlayerItemState(i, item) >= 0) {
                            t = 1;
                        } else {
                            t = 0;
                        }
                        if (t != 0) {
                            *fli |= 4;
                        }
                        if (calculate_player_shopping_parameters_8009C0F0(
                                i, item) != 0) {
                            *fli |= 2;
                        }
                    }
                }
                {
                    s32 ioff = 80;
                    for (j = 1; j < lbl_80344C10; j++, ioff += 80) {
                        s32 r;
                        s32 t;
                        if (calculate_player_shopping_parameters_8009C0F0(
                                i, lbl_80344C14 + ioff) != 0) {
                            goto notavail;
                        }
                        {
                            u8* item = lbl_80344C14 + ioff;
                            if (ItemDefValid(item) != 0) {
                                r = AudioFindPlayerSlot(
                                    i,
                                    *(s32*)(tbl +
                                            *(s32*)(item + 68) * 8 + 188),
                                    *(s32*)(tbl +
                                            *(s32*)(item + 68) * 8 + 192));
                            } else {
                                r = -1;
                            }
                            if (r >= 0) {
                                t = 1;
                            } else if (PlayerItemState(i, item) >= 0) {
                                t = 1;
                            } else {
                                t = 0;
                            }
                            if (t != 0) {
                                goto notavail;
                            }
                            available[j] = 1;
                            continue;
                        }
notavail:
                        available[j] = 0;
                    }
                }
            }
        }
    }
    WritePlayerInfo(-1);
    lbl_80344C00 = 0;
    lbl_80344C08 = 1;
}

void init_shop(s32 fromMenu)
{
    u8* player;
    s32 i;

    lbl_80344C0C = fromMenu;
    gGameMode = 0x4012;
    gGameBusy = 0;
    good_wiz_exit_timer = 0;
    lbl_80344808 = 0;
    lbl_80344C18 = 0;
    lbl_80344C08 = 0;
    if (fromMenu != 0) {
        towerUpdateCurWorldObj();
        PlayersRestoreHealth();
    }
    if (lbl_8034481C >= 2 && lbl_8034481C != 12) {
        lbl_80344C18 = 1;
        return;
    }

    player = gPlayers;
    for (i = 0; i < 4; i++, player += 13148) {
        if (*(s32*)(player + 232) == 5 || *(s32*)(player + 232) == 1) {
            break;
        }
    }
    if (i == 4) {
        lbl_80344C18 = 1;
    }
    lbl_8034481C = 0;
    AudioStopSelect();
    AudioSelectReset();
    AudioReset(0);
    AudioEmptyCb2();
    if (lbl_80344C18 == 0) {
        ShopMusicStart();
    }
    fn_800BC418(2, -1);
    fn_80053D08(-1, 0, -1);
    TransitionBlitShow(0);
    for (i = 0; i < 4; i++) {
        setup_player_display(i);
    }
    SelectLoadStart();
    if (lbl_80344C18 != 0) {
        TransitionBlitShow(0);
    }
    fn_80053C70();
}

extern s32 new_left(s32 player);
extern s32 new_right(s32 player);
extern s32 new_ctrl(u32 mask, s32 player);
extern s32 new_menu_back(s32 player);
extern void AudioClick(s32 player, s32 dir);
extern void AudioBuzzer(void);
extern s32 RandInt(s32 range);
extern void fn_8009D038(s32 player);
extern void PlayerProcessPowerups(u8* pl);
extern void PlayerAddPowerup(u8* pl, s32 kind, u32 mask, f32 dur, f32 pw);
extern void PlayerIncFight(u8* pl, s32 amt);
extern void PlayerIncSpeed(u8* pl, s32 amt);
extern void PlayerIncArmor(u8* pl, s32 amt);
extern void PlayerIncMagic(u8* pl, s32 amt);
extern void heal_player(u8* pl, f32 amount);
extern s32 lbl_803448A0;
extern s32 lbl_803448A4;
extern f64 lbl_803483D8;
extern f32 lbl_803483CC;
extern f32 lbl_803483D0;
extern f32 lbl_803483E0;
extern f32 lbl_803483E4;
extern f32 lbl_803483E8;
extern f32 lbl_803483EC;
extern f32 lbl_803483F0;
extern f32 lbl_803483F4;
extern f32 lbl_803483F8;
extern f32 lbl_803483FC;
extern f32 lbl_80348400;
extern f32 lbl_80348404;
extern f32 lbl_80348408;
extern char lbl_8034840C[4];
extern f32 lbl_80348364;
static s32 write_shop_menu(s32 player, s32 scroll);

/* Buy/sell driver for one shop player: cursor movement over available
 * items, sell-back, and the per-item purchase effects. */
typedef struct { u8 _pad[72]; s32 price; u8 _pad2[4]; } DSItem;
typedef struct { u8 _pad[336]; u32 v; } DSFlag4;
typedef struct { u8 _pad[13056]; s32 v; } DSPot4;
typedef struct { u8 _pad[5456]; s32 v; } DSTim4;

static s32 do_shopping_8009AA48(s32 player)
{
    u8* pl = gPlayers + player * 13148;
    u8* page = lbl_802897D0;
    u8* tbl = lbl_80122ED0;
    volatile s32 exit = 0;
    volatile s32 speed = 1;
    s32 moved = 0;
    s32 click = 0;
    s32 bought = 0;
    s32 paid = 0;
    s32* scrollp = (s32*)(page + (player << 2) + 32);
    s32* topp = (s32*)(page + (player << 2) + 16);
    s32* dimp = (s32*)(page + (player << 2));
    s32* cntp = dimp + 16;
    u8* avail = page + (player << 8) + 4432;
    s32 price;
    s32 sell;
    s32 buy;
    s32 j;
    u8 _spare[72];

    if (*scrollp != 0) {
        speed = *scrollp;
    }
    *dimp = *(s32*)(pl + 2664) - *topp;
    do {
        if (new_left(player) != 0) {
            if (click == 0) {
                AudioClick(player, 0);
                click = 1;
            }
            if (*scrollp != 0) {
                speed = *scrollp + 1;
            }
            {
                s32 c = *(s32*)(pl + 2664) + 1;
                *(s32*)(pl + 2664) = c;
                if (c >= *cntp) {
                    *(s32*)(pl + 2664) = 0;
                    moved = 1;
                    *dimp = 0;
                    *topp = 0;
                }
            }
        }
        if (new_right(player) != 0) {
            if (click == 0) {
                AudioClick(player, 1);
                click = 1;
            }
            if (*scrollp != 0) {
                speed = *scrollp + 1;
            }
            {
            s32 c = *(s32*)(pl + 2664) - 1;
            *(s32*)(pl + 2664) = c;
            if (c < 0) {
                u8* item;
                *cntp = 0;
                item = lbl_80344C14;
                for (j = 0; j < lbl_80344C10; j++, item += 80) {
                    if (*(s32*)(pl + 7876) >= *(s32*)(item + 72)) {
                        goto set_back_count;
                    } else {
                        s32 r;
                        s32 t;
                        if (ItemDefValid(item) != 0) {
                            r = AudioFindPlayerSlot(
                                player,
                                *(s32*)(tbl +
                                        *(s32*)(item + 68) * 8 + 188),
                                *(s32*)(tbl +
                                        *(s32*)(item + 68) * 8 + 192));
                        } else {
                            r = -1;
                        }
                        if (r >= 0) {
                            t = 1;
                        } else if (PlayerItemState(player, item) >= 0) {
                            t = 1;
                        } else {
                            t = 0;
                        }
                        if (t == 0) {
                            goto next_back_item;
                        }
                    }
set_back_count:
                    *cntp = j;
next_back_item:
                    ;
                }
                *cntp += 1;
                *(s32*)(pl + 2664) = *cntp - 1;
                *dimp = *(s32*)(pl + 2664);
                *topp = *dimp - 6;
                if (*topp < 0) {
                    *dimp = *dimp - *topp;
                    *topp = 0;
                }
                moved = 1;
            }
            }
        }
        price = *(s32*)(lbl_80344C14 + *(s32*)(pl + 2664) * 80 + 72);
    } while (*(s32*)(avail + *(s32*)(pl + 2664) * 4) != 0);
    if (*scrollp == 0) {
        sell = new_ctrl(0x2000000, player);
        buy = new_ctrl(0x1000000, player);
    } else {
        sell = 0;
        buy = 0;
    }
    if (buy != 0 &&
        (((DSFlag4*)(page + player * 768 + *(s32*)(pl + 2664) * 4))->v & 4)) {
        u8* item =
            lbl_80344C14 + *(s32*)(pl + 2664) * 80;
        switch (*(s32*)(item + 68)) {
        case 1:
            if (*(s32*)(pl + 7864) > 0) {
                *(s32*)(pl + 7864) -= 1;
                bought = 1;
            }
            break;
        case 3:
            if (*(s32*)(pl + 7868) > 0) {
                *(s32*)(pl + 7868) -= 1;
                bought = 1;
            }
            break;
        default: {
            s32 have;
            s32 r;
            if (*(s32*)(tbl + *(s32*)(item + 68) * 8 + 188) != 0 &&
                *(s32*)(tbl + *(s32*)(item + 68) * 8 + 192) != 0) {
                have = 1;
            } else {
                have = 0;
            }
            if (have != 0) {
                s32 t;
                if (*(s32*)(tbl + *(s32*)(item + 68) * 8 + 188) != 0 &&
                    *(s32*)(tbl + *(s32*)(item + 68) * 8 + 192) != 0) {
                    t = 1;
                } else {
                    t = 0;
                }
                if (t != 0) {
                    r = AudioFindPlayerSlot(
                        player,
                        *(s32*)(tbl + *(s32*)(item + 68) * 8 + 188),
                        *(s32*)(tbl + *(s32*)(item + 68) * 8 + 192));
                } else {
                    r = -1;
                }
                *(f32*)(pl + r * 16 + 304) = lbl_8034832C;
                *(f32*)(pl + r * 16 + 312) =
                    *(s32*)(pl + r * 16 + 316) =
                        *(s32*)(pl + r * 16 + 308) = 0;
                bought = 1;
            }
            break;
        }
        }
        if (bought != 0) {
            {
                *(s32*)(pl + 7876) +=
                    ((DSItem*)lbl_80344C14)[*(s32*)(pl + 2664)].price * 3 / 4;
            }
            fn_8009D038(player);
            PlayerProcessPowerups(pl);
        } else {
            AudioBuzzer();
        }
    }
    if (sell != 0) {
        if (*(s32*)(pl + 7876) >=
            *(s32*)(lbl_80344C14 + *(s32*)(pl + 2664) * 80 + 72)) {
            u8* item = lbl_80344C14 + *(s32*)(pl + 2664) * 80;
            paid = 1;
            switch (*(s32*)(item + 68)) {
            default:
                exit = 1;
                break;
            case 1:
                if (*(s32*)(pl + 7864) < lbl_803448A4) {
                    *(s32*)(pl + 7864) += 1;
                } else {
                    paid = 0;
                }
                break;
            case 2:
                PlayerAddPowerup(pl, 5, 0x200000, lbl_8034832C,
                                 lbl_803483CC);
                break;
            case 3:
                if (*(s32*)(pl + 7868) < lbl_803448A0) {
                    s32 r = RandInt(4);
                    s32 old = *(s32*)(pl + 7868);
                    *(s32*)(pl + 7868) = old + 1;
                    ((DSPot4*)(pl + old * 4))->v = r + 1;
                } else {
                    paid = 0;
                }
                break;
            case 4:
                PlayerAddPowerup(pl, 9, 0x100, lbl_8034832C, lbl_803483D0);
                break;
            case 5:
                PlayerIncFight(pl, 10);
                *(f32*)(pl + 2672) =
                    (f32)(*(f32*)(pl + 2672) + lbl_803483D8);
                break;
            case 6:
                PlayerIncSpeed(pl, 10);
                *(f32*)(pl + 2684) =
                    (f32)(*(f32*)(pl + 2684) + lbl_803483D8);
                break;
            case 7:
                PlayerIncArmor(pl, 10);
                *(f32*)(pl + 2676) =
                    (f32)(*(f32*)(pl + 2676) + lbl_803483D8);
                break;
            case 8:
                PlayerIncMagic(pl, 10);
                *(f32*)(pl + 2680) =
                    (f32)(*(f32*)(pl + 2680) + lbl_803483D8);
                break;
            case 9:
                PlayerAddPowerup(pl, 6, 0x20000, lbl_8034832C,
                                 lbl_803483D0);
                break;
            case 10:
                PlayerAddPowerup(pl, 9, 0x80, lbl_8034832C, lbl_803483E0);
                break;
            case 11:
                PlayerAddPowerup(pl, 5, 0x20000000, lbl_8034832C,
                                 lbl_803483D0);
                break;
            case 12:
                PlayerAddPowerup(pl, 5, 0x80000, lbl_8034832C,
                                 lbl_803483E0);
                break;
            case 13:
                PlayerAddPowerup(pl, 5, 0x10000000, lbl_803483E4,
                                 lbl_803483E8);
                break;
            case 14:
                PlayerAddPowerup(pl, 6, 0x400000, lbl_8034832C,
                                 lbl_803483EC);
                break;
            case 15:
                PlayerAddPowerup(pl, 6, 0x200000, lbl_8034832C,
                                 lbl_803483EC);
                break;
            case 16:
                PlayerAddPowerup(pl, 6, 0x110000, lbl_8034832C,
                                 lbl_803483F0);
                break;
            case 17:
                heal_player(pl, (f32)*(s32*)(item + 76));
                break;
            case 18:
                PlayerAddPowerup(pl, 9, 1, lbl_8034832C, lbl_803483F4);
                break;
            case 19:
                PlayerAddPowerup(pl, 9, 0x100, lbl_8034832C, lbl_803483D0);
                break;
            case 20:
                PlayerAddPowerup(pl, 5, 1, lbl_8034832C, lbl_803483CC);
                break;
            case 21:
                PlayerAddPowerup(pl, 5, 2, lbl_8034832C, lbl_803483CC);
                break;
            case 22:
                PlayerAddPowerup(pl, 5, 3, lbl_8034832C, lbl_803483CC);
                break;
            case 23:
                PlayerAddPowerup(pl, 5, 4, lbl_8034832C, lbl_803483CC);
                break;
            case 24:
                PlayerAddPowerup(pl, 5, 0x100000, lbl_803483F8,
                                 lbl_803483E8);
                break;
            case 25:
                PlayerAddPowerup(pl, 9, 0x10, lbl_803483F8, lbl_803483E8);
                break;
            case 26:
                PlayerAddPowerup(pl, 9, 0x40, lbl_803483F8, lbl_803483E8);
                break;
            case 27:
                PlayerAddPowerup(pl, 9, 0x20, lbl_803483F8, lbl_803483E8);
                break;
            case 28:
                PlayerAddPowerup(pl, 9, 0x10000, lbl_803483FC,
                                 lbl_80348400);
                break;
            case 29:
                PlayerAddPowerup(pl, 9, 0x200, lbl_8034832C, lbl_803483EC);
                break;
            case 30:
                PlayerAddPowerup(pl, 9, 4, lbl_8034832C, lbl_803483EC);
                break;
            case 31:
                PlayerAddPowerup(pl, 6, 0x10000, lbl_8034832C,
                                 lbl_803483D0);
                break;
            case 32:
                PlayerAddPowerup(pl, 9, 2, lbl_8034832C, lbl_80348404);
                break;
            case 33:
                PlayerAddPowerup(pl, 6, 0x2008, lbl_8034832C,
                                 lbl_803483EC);
                break;
            case 34:
                PlayerAddPowerup(pl, 5, 0x400000, lbl_8034832C,
                                 lbl_803483E0);
                break;
            case 35:
                PlayerAddPowerup(pl, 6, 0x80000, lbl_8034832C,
                                 lbl_80348404);
                break;
            case 36:
                PlayerAddPowerup(pl, 9, 0x200000, lbl_8034832C,
                                 lbl_80348404);
                break;
            case 37:
                PlayerAddPowerup(pl, 9, 0x400000, lbl_8034832C,
                                 lbl_80348404);
                break;
            case 38:
                PlayerAddPowerup(pl, 9, 0x100000, lbl_8034832C,
                                 lbl_80348404);
                break;
            case 39:
                PlayerAddPowerup(pl, 9, 8, lbl_8034832C, lbl_80348408);
                break;
            }
            if (paid != 0) {
                *(s32*)(pl + 7876) -= price;
                if (*(s32*)(pl + 2664) != 0) {
                    fn_8009D038(player);
                    PlayerProcessPowerups(pl);
                }
            } else {
                AudioBuzzer();
            }
        } else {
            AudioBuzzer();
        }
    } else if (new_menu_back(player) != 0) {
        if (*(s32*)(pl + 2664) != 0) {
            *(s32*)(pl + 2664) = 0;
            moved = 1;
        }
    }
    if (paid != 0 || bought != 0) {
        u8* item = lbl_80344C14 + 80;
        s32 joff = 4;
        for (j = 1; j < lbl_80344C10; j++, joff += 4, item += 80) {
            s32* fli = (s32*)(page + player * 768 + 336 + joff);
            s32 r;
            s32 t;
            *fli = 0;
            if (ItemDefValid(item) != 0) {
                r = AudioFindPlayerSlot(
                    player, *(s32*)(tbl + *(s32*)(item + 68) * 8 + 188),
                    *(s32*)(tbl + *(s32*)(item + 68) * 8 + 192));
            } else {
                r = -1;
            }
            if (r >= 0) {
                t = 1;
            } else if (PlayerItemState(player, item) >= 0) {
                t = 1;
            } else {
                t = 0;
            }
            if (t != 0) {
                *fli |= 4;
            }
            if (calculate_player_shopping_parameters_8009C0F0(player, item)
                != 0) {
                *fli |= 2;
            }
        }
        {
            s32 ioff = 80;
            joff = 4;
            for (j = 1; j < lbl_80344C10; j++, joff += 4, ioff += 80) {
                s32 r;
                s32 t;
                if (calculate_player_shopping_parameters_8009C0F0(
                        player, lbl_80344C14 + ioff) != 0) {
                    goto notavail;
                }
                {
                    u8* it2 = lbl_80344C14 + ioff;
                    if (ItemDefValid(it2) != 0) {
                        r = AudioFindPlayerSlot(
                            player,
                            *(s32*)(tbl + *(s32*)(it2 + 68) * 8 + 188),
                            *(s32*)(tbl + *(s32*)(it2 + 68) * 8 + 192));
                    } else {
                        r = -1;
                    }
                    if (r >= 0) {
                        t = 1;
                    } else if (PlayerItemState(player, it2) >= 0) {
                        t = 1;
                    } else {
                        t = 0;
                    }
                    if (t == 0) {
                        goto available;
                    }
                }
notavail:
                *(s32*)(avail + joff) = 0;
                continue;
available:
                *(s32*)(avail + joff) = 1;
            }
        }
        {
            ((DSTim4*)(page + (player << 8) + *(s32*)(pl + 2664) * 4))->v =
                30;
        }
        while (*(s32*)(pl + 7876) <
                   *(s32*)(lbl_80344C14 + *(s32*)(pl + 2664) * 80 + 72) ||
               *(s32*)(avail + *(s32*)(pl + 2664) * 4) != 0) {
            *(s32*)(pl + 2664) -= 1;
        }
        if (*(s32*)(pl + 2664) < *topp) {
            u8* item;
            *topp = *(s32*)(pl + 2664);
            *cntp = 0;
            item = lbl_80344C14;
            for (j = 0; j < lbl_80344C10; j++, item += 80) {
                if (*(s32*)(pl + 7876) >= *(s32*)(item + 72)) {
                    goto set_forward_count;
                } else {
                    s32 r;
                    s32 t;
                    if (ItemDefValid(item) != 0) {
                        r = AudioFindPlayerSlot(
                            player,
                            *(s32*)(tbl + *(s32*)(item + 68) * 8 + 188),
                            *(s32*)(tbl + *(s32*)(item + 68) * 8 + 192));
                    } else {
                        r = -1;
                    }
                    if (r >= 0) {
                        t = 1;
                    } else if (PlayerItemState(player, item) >= 0) {
                        t = 1;
                    } else {
                        t = 0;
                    }
                    if (t == 0) {
                        goto next_forward_item;
                    }
                }
set_forward_count:
                *cntp = j;
next_forward_item:
                ;
            }
            *cntp += 1;
            moved = 1;
        }
        {
            s32 n = lbl_80344C10;
            for (j = *(s32*)(pl + 2664) + 1; j < n; j++) {
                if (*(u32*)(page + (player << 8) + 6480 + j * 4) == 0) {
                    break;
                }
            }
        }
    }
    if (moved == 0) {
        s32 top = *topp;
        s32 d = *(s32*)(pl + 2664) - top;
        if (d > 6) {
            *topp = d + top - 6;
        } else if (d < 0) {
            *topp = top + d;
        } else {
            s32 v0 = *dimp;
            if (v0 < d) {
                if (v0 >= 3 && (*cntp - 1) - (top + 6) > 0) {
                    *topp = top + 1;
                }
            } else if (d < v0 && v0 <= 3 && top > 0) {
                *topp = top - 1;
            }
        }
    }
    if (moved != 0) {
        speed = -1;
    }
    *scrollp = write_shop_menu(player, speed);
    DrawTextKeepScale(lbl_80348364,
                      -*(s32*)(tbl + (player << 2) + 112), 8, 6, 0,
                      lbl_8034840C);
    return exit;
}

extern s32 lbl_80343E00;    /* menu fade band height */
extern s32 lbl_80343E04;    /* menu top y            */
extern s32 lbl_80343E08;    /* menu bottom y         */
extern f64 lbl_80348428;    /* mlines scale factor   */
extern char lbl_80348414[5]; /* "%d" price fmt        */
extern char lbl_8034841C[5]; /* "%d" sell fmt         */
extern char lbl_80348430[8]; /* up-arrow texture name */
extern char lbl_80114A30[]; /* down-arrow texture    */
extern s32 lbl_80122F50[];  /* per-player price x column */
extern void mbBlitCalcRect(void* blit, s32 a, s32* rect, s32 b);
extern void MBBlitSetAlpha(void* blit, s32 alpha);
extern s32 MBSetFontAlpha(s32 alpha);
extern void MBFontMsgSetAlpha(s32 msg, s32 alpha);
extern s32 DrawTextMLines(f32 scale, s32 x, s32 y, s32 font, s32 color,
                          char* str);
extern s32 DrawGlowTextMLines(f32 scale, s32 x, s32 y, char* str);
extern void* MBOX_FindTexture(char* name, s32 mode);
void calc_shop_ypos(s32 player);

/* Draw one player's scrolling shop menu column; returns the scroll speed
 * while the list is still moving. */
static s32 write_shop_menu(s32 player, s32 scroll)
{
    u8* page = lbl_802897D0;
    u8* pl = gPlayers + player * 13148;
    s32 didScroll = 0;
    s32 needUp = 0;
    s32 needDn = 0;
    s32 mv = scroll;
    s32* cursp;
    s32* scrollflag;
    s32 ybase;
    s32 j;
    s32 joff;
    volatile s32 itemoff;
    s32* colp;
    s32* xcol;
    u8* blits;
    u8* timers;
    u8* avail;
    u8* flags;
    s32 tex;
    s32* fli;
    f64 kGold;
    u8 _spare[12];
    char buf[20];
    s32 y;
    u8 _ypad[8];

    if (mv > 0 && mv < 2) {
        mv = 2;
    }
    cursp = (s32*)(page + (player << 8) + 3408);
    if (*cursp == -1) {
        mv = -1;
    }
    calc_shop_ypos(player);
    scrollflag = (s32*)(page + (player << 2));
    *(scrollflag += 12) = 0;
    {
        s32 top = lbl_80343E04;
        s32 bot = lbl_80343E08;
        s32 cy = *cursp;
        s32 last;
        ybase = top + (bot - top) / 2;
        if (ybase + cy > top) {
            ybase = top - cy;
        }
        last = *(s32*)(page + (player << 8) + lbl_80344C10 * 4 + 3404);
        if (ybase + last < bot) {
            ybase = bot - last;
        }
    }
    kGold = lbl_80348428;
    colp = (s32*)((u8*)lbl_80122F50 + (player << 2));
    xcol = (s32*)((u8*)lbl_80122F40 + (player << 2));
    itemoff = 0;
    blits = page + (player << 8) + 6480;
    timers = page + (player << 8) + 5456;
    avail = page + (player << 8) + 4432;
    flags = page + player * 768 + 336;
    joff = 0;
    for (j = 0; j < lbl_80344C10; j++, joff += 4, itemoff += 80) {
        s32* timp = (s32*)(timers + joff);
        s32 yt = ybase + *(s32*)((u8*)cursp + joff);
        u8* item = lbl_80344C14 + itemoff;
        void* blit = *(void**)(blits + joff);
        s32 hl;
        s32 sel;
        s32 a;
        s32 price;
        if (*timp > 0) {
            s32 t = *timp - gFrameTicks;
            *timp = t;
            if (t < 0) {
                *timp = 0;
            }
            hl = 0xFF0000;
        } else {
            hl = 0;
        }
        if (j == *(s32*)(pl + 2664)) {
            sel = 1;
        } else {
            sel = 0;
        }
        mbBlitCalcRect(blit, 0, &y, 0);
        if (y - yt < -1 || y - yt > 1) {
            if (mv < 0) {
                y = yt;
            } else {
                *scrollflag = 1;
                if (y > yt) {
                    y = y - gFrameTicks * mv;
                    if (y < yt) {
                        y = yt;
                    }
                    didScroll = 1;
                } else if (y < yt) {
                    y = y + gFrameTicks * mv;
                    if (y > yt) {
                        y = yt;
                    }
                    didScroll = 1;
                }
            }
            mbBlitCalcY(blit, y);
        }
        {
            s32 dim = lbl_80343E00;
            s32 top = lbl_80343E04;
            if (y < top - dim) {
                a = 256;
                if (*(s32*)(avail + joff) == 0) {
                    needUp = 1;
                }
            } else if (y < top) {
                a = (top - y) * 510 / dim;
                if (a > 255) {
                    a = 255;
                }
                if (*(s32*)(avail + joff) == 0) {
                    needUp = 1;
                }
            } else if (y > lbl_80343E08 + dim) {
                a = 256;
                if (*(s32*)(avail + joff) == 0) {
                    needDn = 1;
                }
            } else if (y > lbl_80343E08) {
                a = (y - lbl_80343E08) * 510 / dim;
                if (a > 255) {
                    a = 255;
                }
                if (*(s32*)(avail + joff) == 0) {
                    needDn = 1;
                }
            } else {
                a = 0;
            }
        }
        if (a >= 256) {
            mbBlitInit3414(blit, 1);
            continue;
        }
        if (a == 0 && *(s32*)(avail + joff) != 0) {
            a = 160;
        }
        tex = MBBlitGetTex(blit);
        if (tex == 0) {
            mbBlitInit3414(blit, 1);
        } else {
            mbBlitInit3414(blit, 0);
            MBBlitSetAlpha(blit, a);
        }
        {
            s32 rawPrice = *(s32*)(item + 72);
            if (rawPrice > 0) {
                s32 x;
                s32 ytxt;
                u32 msg;
                price = rawPrice;
                fli = (s32*)(flags + joff);
                x = *colp - 64;
                if (*fli & 8) {
                    ytxt = y - 6;
                } else {
                    ytxt = y + 12;
                }
                sprintf(buf, lbl_80348414, price);
                if (sel != 0) {
                    DrawGlowText(lbl_80348360, x, ytxt, buf);
                } else {
                    msg = DrawTextKeepScale(lbl_80348360, x, ytxt, 6, hl, buf);
                    if (msg != 0) {
                        MBFontMsgSetAlpha(msg, a);
                    }
                }
                if (*fli & 8) {
                    ytxt = y + 12;
                    sprintf(buf, lbl_8034841C, price * 3 / 4);
                    if (sel != 0) {
                        DrawGlowText(lbl_80348360, x, ytxt, buf);
                    } else {
                        msg = DrawTextKeepScale(lbl_80348360, x, ytxt, 6, hl,
                                                buf);
                        if (msg != 0) {
                            MBFontMsgSetAlpha(msg, a);
                        }
                    }
                }
            }
        }
        if (tex > 0) {
            y = y + 32;
        } else {
            y = y + 12;
        }
        if (*(s8*)(item + 32) != 0) {
            s32 x2 = -*xcol;
            s32 h;
            if (sel != 0) {
                h = DrawGlowTextMLines((f32)(kGold * *(f32*)(item + 64)),
                                       x2, y, (char*)(item + 32));
            } else {
                s32 old = MBSetFontAlpha(a);
                h = DrawTextMLines((f32)(kGold * *(f32*)(item + 64)), x2, y,
                                   6, hl, (char*)(item + 32));
                MBSetFontAlpha(old);
            }
            y = y + h;
        }
    }
    if (needUp != 0) {
        MBNewTempBlit(MBOX_FindTexture(lbl_80348430, 0), *xcol - 32,
                      lbl_80343E04 - 40, -1, -1);
    }
    if (needDn != 0) {
        MBNewTempBlit(MBOX_FindTexture(lbl_80114A30, 0), *xcol - 32,
                      lbl_80343E08 + 56, -1, -1);
    }
    if (mv > 0 && didScroll != 0) {
        return mv;
    }
    return 0;
}

/* Build and center the per-entry vertical offsets for one shop player. */
void calc_shop_ypos(s32 player)
{
    void** blits;
    s32* ypos;
    u8* p;
    u8* entry;
    s32 i;
    s32 y;
    f64 scale = lbl_80348428;

    p = &gPlayers[player * 13148];
    ypos = &lbl_8028A520[player << 6];
    blits = &lbl_8028B120[player << 6];
    y = 0;
    for (i = 0; i < lbl_80344C10; i++) {
        ypos[i] = y;
        entry = lbl_80344C14 + i * 80;
        if (MBBlitGetTex(blits[i]) > 0) {
            y += 24;
        }
        if ((s8)entry[32] != 0) {
            y = y + TextHeightMLines((f32)(scale *
                                            (f64)*(f32*)(entry + 64)),
                                     6, (char*)entry + 32);
            y = y + 16;
        }
    }

    y = ypos[*(s32*)(p + 2664)];
    {
        s32 count;

        i = 0;
        for (count = lbl_80344C10; count > 0; count--, i++) {
            ypos[i] -= y;
            if (ypos[i] > 1024) {
                ypos[i] = 1024;
            }
            if (ypos[i] < -1024) {
                ypos[i] = -1024;
            }
        }
    }
}

static char* shopMoreUpPoolSeed(void)
{
    return "MORE_UP";
}

static inline u32 shopSwapU32(u32 value)
{
    u32 result;
    u8* src = (u8*)&value;
    u8* dst = (u8*)&result;

    dst[0] = src[3];
    dst[1] = src[2];
    dst[2] = src[1];
    dst[3] = src[0];
    return result;
}

static inline f32 shopSwapF32(f32 value)
{
    f32 result;
    u32 bits;

    bits = *(u32*)&value;
    *(u32*)&result = shopSwapU32(bits);
    return result;
}

/* Load and endian-fix the shop entry table. */
#pragma opt_common_subs off
void ShopLoadData(void)
{
    u8 wad[12];
    char* file;
    s32 swapped;
    s32 i;
    s32 offset;

    file = AllocFile("shpdata", "shop.wad");
    swapped = MBSetupWad((s32*)(wad - 4), (s32)file);
    lbl_80344C14 = (u8*)MBGetFromWad(
        (s32*)(wad - 4),
        ((s32)(s8)"ITEM"[0] << 24) | ((s32)(s8)"ITEM"[1] << 16) |
        ((s32)(s8)"ITEM"[2] << 8) | (s32)(s8)"ITEM"[3],
        &lbl_80344C10);
    if ((swapped & 0xFF) != 0) {
        i = 0;
        offset = 0;
        while (i < lbl_80344C10) {
            u8* entry = lbl_80344C14 + offset;

            *(f32*)(entry + 64) = shopSwapF32(*(f32*)(entry + 64));
            *(u32*)(entry + 68) = shopSwapU32(*(u32*)(entry + 68));
            *(u32*)(entry + 72) = shopSwapU32(*(u32*)(entry + 72));
            *(u32*)(entry + 76) = shopSwapU32(*(u32*)(entry + 76));
            i++;
            offset += 80;
        }
    }
    lbl_80344C04 = *(s32*)(lbl_80344C14 + 152);
}
#pragma opt_common_subs reset

/* Return whether a player can buy the selected shop entry. */
static s32 calculate_player_shopping_parameters_8009C0F0(s32 player, u8* entry)
{
    u8* p = &gPlayers[player * 13148];

    if (*(s32*)(p + 7876) < *(s32*)(entry + 72)) {
        return 0;
    }
    switch (*(u32*)(entry + 68)) {
    case 1:
        if (*(s32*)(p + 7864) >= lbl_803448A4) {
            return 0;
        }
        break;
    case 3:
        if (*(s32*)(p + 7868) >= lbl_803448A0) {
            return 0;
        }
        break;
    case 5:
        if ((f64)*(f32*)(p + 244) >= 999.0) {
            return 0;
        }
        break;
    case 6:
        if ((f64)*(f32*)(p + 256) >= 999.0) {
            return 0;
        }
        break;
    case 7:
        if ((f64)*(f32*)(p + 248) >= 999.0) {
            return 0;
        }
        break;
    case 8:
        if ((f64)*(f32*)(p + 252) >= 999.0) {
            return 0;
        }
        break;
    case 17:
        if ((f64)*(f32*)(p + 7860) >= (f64)player_max_health(p)) {
            return 0;
        }
        break;
    case 38:
        break;
    }
    return 1;
}
