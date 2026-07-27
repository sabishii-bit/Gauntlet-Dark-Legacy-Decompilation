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
 * gPlayerRecords = lbl_80275AE0 (shared [4][0x335C] progress array) is
 * referenced throughout; per project policy it stays lbl_ until the
 * coordinated rename.
 *
 * All bodies are doc-only this pass (the final DOL links the original bytes
 * for this range via the splits.txt claim; this file exists to own the claim
 * and carry the recovered map).
 */

#include "types.h"
