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
extern void fn_80053A68(s32 arg);
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

/* Animate one player gold-pile column toward its target height; returns 0
 * while the pile is still sinking (drives the count-down loop). */
s32 show_piles(s32 col)
{
    u8* tbl = lbl_802897D0;
    u8* counts = tbl + col * 12 + 224;
    u8* pl = gPlayers + col * 13148;
    s32 result = 1;
    s32 range = lbl_80343E10 - lbl_80343E0C - lbl_80343E14;
    s32 adj = gFrameTicks + (gFrameTicks >> 1);
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
        if (*(s32*)(counts + off) == 0) {
            break;
        }
        count++;
        off += 4;
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
extern char lbl_803483B8[]; /* "%s: %d" gold-line fmt (sdata)         */
extern char lbl_801149F4[]; /* "Continue" label                        */
extern char lbl_80348368[]; /* "Stats" label (sdata, color-code +2)    */
extern f32 lbl_80348360;    /* gold-line text scale                    */
extern f32 lbl_80348364;    /* stats text scale                        */
extern void* lbl_80344E48;  /* continue-arrow texture                  */
extern int sprintf(char* buf, const char* fmt, ...);
extern void DrawGlowText(f32 scale, s32 y, s32 x, char* txt);
extern void DrawTextKeepScale(f32 scale, s32 y, s32 x, s32 font, s32 color,
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
    u8* blits = tbl + col * 24 + 7504;
    u8* counts = tbl + col * 12 + 224;
    u8* shownT = tbl + col * 12 + 128;
    u8* targT = tbl + col * 12 + 80;
    u8* valT = tbl + col * 12 + 176;
    s32 colorMask = 0x1000000;
    s32 ypos = *(s32*)(dpage + col * 4 + 112);
    s32 xbase = *(s32*)(dpage + col * 4 + 96);
    s32 done = 0;
    s32 k = 0;
    s32 off = 0;
    s32 fontY;
    u8* blit;
    s32 tgt;
    s32 shown;
    s32 grew;
    u8* item;
    s32 drawX;
    f32 height;
    char buf[32];

    for (; k < 3; k++, off += 4) {
        tgt = *(s32*)(targT + off);
        shown = *(s32*)(shownT + off);
        blit = *(u8**)(blits + *(s32*)(counts + off) * 4);
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
            height = (f32)((f32)shown * lbl_803483B0);
            mbBlitCalcY(blit, lbl_80343E10 - shown);
            mbBlitProject(blit, 0, shown);
            mbBlitSetupVerts(blit, lbl_8034832C, lbl_80348328, lbl_8034832C,
                             height);
            *(s32*)(shownT + off) = shown;
        }
        item = dpage + *(s32*)(counts + off) * 4;
        drawX = *(s32*)(item + 172);
        sprintf(buf, lbl_803483B8, *(u32*)(item + 160), *(s32*)(valT + off));
        if (grew != 0) {
            DrawGlowText(lbl_80348360, xbase + 16, drawX, buf);
        } else {
            DrawTextKeepScale(lbl_80348360, xbase + 16, drawX, 6,
                              colorMask - 1, buf);
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
extern char lbl_80348338[];  /* "%d" fmt (sdata)                       */
extern f64 lbl_80348340;     /* seconds per day                        */
extern f64 lbl_80348348;     /* seconds per hour                       */
extern f64 lbl_80348350;     /* seconds per minute                     */
extern char lbl_80348358[];  /* "%d Days" fmt (sdata)                  */
extern u8 lbl_80240E30[];    /* pad states, stride 60, buttons @+8     */
extern void AudioCursorSelect(void);

/* Staged end-of-game Final Stats screen: reveals one glowing line per
 * 60-tick window, then the playtime breakdown and the Continue prompt.
 * Returns 1 once the player confirms with Start. */
static s32 shop_show_final_stats(u8* pl)
{
    char* pool = lbl_80114918;
    s32 done = 0;
    s32 cls = *(s32*)(pl + 12);
    s32 idx = *(s32*)(pl + 0);
    s32 timer = *(s32*)(pl + 2668);
    f32 scale = lbl_80348330;
    u8* stats = pl + (cls * 28 + 3088);
    s32 xbase = *(s32*)((u8*)lbl_80122F30 + idx * 4);
    s32 ypos = *(s32*)((u8*)lbl_80122F40 + idx * 4);
    s32 x8 = xbase + 8;
    s32 colorMask = 0x1000000;
    s32 t = timer & 0xFFFF;
    s32 nx;
    s32 y2;
    s32 days;
    s32 hours;
    s32 mins;
    f32 secs;
    char buf[36];

    if (t < 0xF000) {
        *(s32*)(pl + 2668) = timer + gFrameTicks;
    }
    nx = -ypos;
    DrawGlowText(lbl_80348334, nx, 32, pool + 80);
    if (t > 90 && t < 150) {
        DrawGlowText(scale, nx, 60, pool + 92);
    } else {
        DrawTextKeepScale(scale, nx, 60, 6, colorMask - 1, pool + 92);
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
        DrawTextKeepScale(scale, nx, 98, 6, colorMask - 1, pool + 108);
        y2 = 116;
        DrawTextKeepScale(scale, nx, 116, 6, colorMask - 1, pool + 120);
    }
    if (t > 150) {
        sprintf(buf, lbl_80348338, *(s32*)(stats + 16));
        DrawGlowText(scale, nx, y2 + 18, buf);
    }
    if (t > 210 && t < 270) {
        DrawGlowText(scale, nx, y2 + 38, pool + 132);
    } else {
        DrawTextKeepScale(scale, nx, y2 + 38, 6, colorMask - 1, pool + 132);
    }
    if (t > 210) {
        sprintf(buf, lbl_80348338, *(s32*)(stats + 20));
        DrawGlowText(scale, nx, y2 + 56, buf);
    }
    if (t > 270 && t < 330) {
        DrawGlowText(scale, nx, y2 + 76, pool + 144);
    } else {
        DrawTextKeepScale(scale, nx, y2 + 76, 6, colorMask - 1, pool + 144);
    }
    secs = *(f32*)(stats + 24);
    days = (s32)(secs / lbl_80348340);
    secs = (f32)-(lbl_80348340 * (f32)days - secs);
    hours = (s32)(secs / lbl_80348348);
    secs = (f32)-(lbl_80348348 * (f32)hours - secs);
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
        if ((*(u32*)(lbl_80240E30 + *(s32*)pl * 60 + 8) & 0x2000000) != 0) {
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

extern s32 lbl_803448C4;    /* current world number  */
extern s32 lbl_803448C8;    /* current level number  */
extern s32 sWorldDataConst; /* shop world-data key   */
extern void ResolveWorldData(s32 key);

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
    s32 tG;
    s32 ra;
    s32 rb;
    s32 rc;
    s32 off;
    s32 i;
    u8* base;
    u8* b28;

    ResolveWorldData((lbl_803448C8 << 8) | (u8)lbl_803448C4);
    cls = *(s32*)(pl + 12);
    lvl = gCurLevel;
    base = pl + cls * 240;
    goldRaw = *(s32*)(pl + 7876) - *(s32*)(base + 8780);
    t = goldRaw * range / (*(s32*)(lvl + 224) + 1);
    b28 = pl + cls * 28;
    base = pl + cls * 24;
    raw2 = (*(s32*)(b28 + 3088) + *(s32*)(b28 + 3104)) -
           (*(s32*)(b28 + 8284) + *(s32*)(b28 + 8300));
    raw3 = *(s32*)(pl + 7872) - *(s32*)(base + 7900);
    tG = t;
    if (t < 64) {
        statG = 64;
    } else if (tG > range) {
        statG = range;
    } else {
        statG = tG;
    }
    t2 = raw2 * range / (*(s32*)(lvl + 228) + 1);
    tG = t2;
    if (t2 < 64) {
        stat2 = 64;
    } else if (tG > range) {
        stat2 = range;
    } else {
        stat2 = tG;
    }
    t3 = raw3 * range / (*(s32*)(lvl + 232) + 1);
    tG = t3;
    if (t3 < 64) {
        stat3 = 64;
    } else if (tG > range) {
        stat3 = range;
    } else {
        stat3 = tG;
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
    i = 0;
    base = tbl + off;
    *(s32*)(base + 224 + ra * 4) = i;
    *(s32*)(base + 224 + rb * 4) = 2;
    *(s32*)(base + 224 + rc * 4) = 1;
    base = tbl + off;
    for (t = 3; t != 0; t--) {
        *(s32*)(base + 128 + i) = lbl_80343E14;
        i += 4;
    }
    base = tbl + off;
    *(s32*)(base + 80 + ra * 4) = statG;
    *(s32*)(base + 80 + rb * 4) = stat3;
    *(s32*)(base + 80 + rc * 4) = stat2;
    base = tbl + off;
    *(s32*)(base + 176 + ra * 4) = goldRaw;
    *(s32*)(base + 176 + rb * 4) = raw3;
    *(s32*)(base + 176 + rc * 4) = raw2;
    ResolveWorldData(sWorldDataConst);
}

/* Enter the between-level shop and launch its asynchronous front-end load. */
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
    fn_80053A68(0);
    for (i = 0; i < 4; i++) {
        setup_player_display(i);
    }
    SelectLoadStart();
    if (lbl_80344C18 != 0) {
        fn_80053A68(0);
    }
    fn_80053C70();
}

/* Build and center the per-entry vertical offsets for one shop player. */
void calc_shop_ypos(s32 player)
{
    s32* ypos = &lbl_8028A520[player << 6];
    void** blits = &lbl_8028B120[player << 6];
    u8* p = &gPlayers[player * 13148];
    u8* entry;
    s32 i;
    s32 y;

    y = 0;
    for (i = 0; i < lbl_80344C10; i++) {
        ypos[i] = y;
        entry = lbl_80344C14 + i * 80;
        if (MBBlitGetTex(blits[i]) > 0) {
            y += 24;
        }
        if ((s8)entry[32] != 0) {
            y += TextHeightMLines((f32)(0.5 * (f64)*(f32*)(entry + 64)),
                                  6, (char*)entry + 32) + 16;
        }
    }

    y = ypos[*(s32*)(p + 2664)];
    {
        s32 offset = 0;
        s32 count;

        for (count = lbl_80344C10; count > 0; count--) {
            s32* cur = (s32*)((u8*)ypos + offset);

            *cur -= y;
            if (*cur > 1024) {
                *cur = 1024;
            }
            if (*cur < -1024) {
                *cur = -1024;
            }
            offset += 4;
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
s32 calculate_player_shopping_parameters(s32 player, u8* entry)
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
