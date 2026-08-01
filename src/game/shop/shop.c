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
