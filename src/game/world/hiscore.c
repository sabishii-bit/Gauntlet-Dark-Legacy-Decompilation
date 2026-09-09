/* HISCORE.OBJ: the per-player name-entry screen (.text 0x8005A738..0x8005ACE0). */
#include "types.h"
#include "game/controls.h"
#include "game/gamemode.h"
#include "game/options.h"
#include "game/player.h"

extern s32    gGameMode;
extern s64    gControllerButtons;
extern Player gPlayers[4];
extern s32    gFrameTicks;
extern u32    pbLoad;

extern char* strcpy(char* dst, const char* src);
extern s32   RandInt(s32 range);
extern void  DrawTextKeepScale(f32 scale, s32 x, s32 y, u32 font, u32 color, u8* str);
extern s32   new_menu_accept(s32 player, s32 allow_start);
extern s32   new_menu_back(s32 player);
extern s32   new_up(s32 player);
extern s32   new_down(s32 player);
extern void  AudioCursorH(void);
extern void  AudioCursorV(void);
extern void  AudioClick2(s32 player, s32 select);

s32 get_initials(s32 pnum);

/* the twenty random names and, directly after them in .data, the four
 * per-player text colours (get_initials addresses the colours by their own
 * symbol; DoGetName reads them through the name-table pointer, see there) */
static char* sRandomNames[20] = {
    "LARRY",  "PELE",   "CHUCK", "TRENT", "SPENCR", "JOFFRY", "PABLO",
    "JUSTIN", "MAT",    "CHIP ", "FRED",  "SHAWN",  "JAKE",   "CJ",
    "ALEX",   "MARVIN", "WESLEY", "BRAND", "GORDON", "TRAVIS",
};
static u32 sNameColors[4] = { 0xFFFF00, 0x87CEEB, 0xFF0000, 0x00FF00 };
static s16 sInitialsX[4] = { 42, 172, 300, 425 };
static s16 sNameX[4] = { 64, 192, 320, 448 };

/* Drive one player's name entry: -1 = skipped (random name), 1 = done. */
s32 DoGetName(s32 player)
{
    char** names = sRandomNames;
    Player* p = &gPlayers[player];
    s32 ret = 0;
    s16 t;
    u8 _spare[8]; /* 8-byte stack object the retail frame reserves; the Xbox
                   * DoGetName has no stack locals, so it is unrecovered */

    if (gGameOptions.skip & 1) {
        strcpy(p->save.name, names[RandInt(20)]);
        return -1;
    }
    if (p->world_text_active == 0) {
        if ((ret = get_initials(player)) <= 0) {
            goto out;
        }
        p->world_text_active = 1;
        p->name_timer = 60;
        ret = 0;
        if (p->save.name[0] == 0) {
            strcpy(p->save.name, names[RandInt(20)]);
        }
    } else {
        t = p->name_timer - gFrameTicks;
        p->name_timer = t;
        if (t < 0) {
            p->name_timer = 0;
            ret = 1;
        } else if (p->name_timer & 16) {
            /* the retail loads this player's colour as names[player + 20]: the
             * colour table is read through the name-table pointer, 80 bytes past
             * its start, not through its own symbol */
            DrawTextKeepScale(0.75f, -sNameX[player], 340, 7, (u32)names[player + 20],
                              (u8*)p->save.name);
        }
    }
out:
    return ret;
}

s32 get_initials(s32 pnum)
{
    s32 accept;
    s32 skip;
    s32 i;
    u16 x;
    u32 color;
    u32 white;
    char chr[2];
    Player* p;
    u32* rep;

    p = &gPlayers[pnum];
    accept = new_menu_accept(pnum, 0);
    if (accept == 0) {
        /* the retail reaches repedges (+0xC) by bumping a word pointer over
         * the record: the first read is an lwzu, the second reads 0(rep) */
        rep = (u32*)PlayerControl;
        rep += pnum * (sizeof(PLAYERCONTROL) / sizeof(u32));
        if ((*(rep += 3) & 0x40000030) != 0) {
            switch (p->world_name_tail) {
            default:
                p->world_name_tail++;
                break;
            case 'Z':
                p->world_name_tail = '_';
                break;
            case '_':
                p->world_name_tail = '0';
                break;
            case '9':
                p->world_name_tail = '@';
                break;
            case '@':
                p->world_name_tail = 'A';
                break;
            }
            AudioCursorV();
        }
        if ((*rep & 0x800000C0) != 0) {
            switch (p->world_name_tail) {
            default:
                p->world_name_tail--;
                break;
            case 'A':
                p->world_name_tail = '@';
                break;
            case '@':
                p->world_name_tail = '9';
                break;
            case '0':
                p->world_name_tail = '_';
                break;
            case '_':
                p->world_name_tail = 'Z';
                break;
            }
            AudioCursorV();
        }
    }

    if ((new_down(pnum) != 0 || new_menu_back(pnum) != 0) &&
        p->world_name_len > 0) {
        p->world_name_len--;
        p->world_name_tail = (s8)p->save.name[p->world_name_len];
        p->save.name[p->world_name_len] = 0;
        AudioCursorH();
    }

    if (accept != 0 || new_up(pnum) != 0) {
        skip = 0;
        if (p->world_name_tail == '@') {
            if (accept != 0) {
                p->world_name_len = 6;
            } else {
                skip = 1;
            }
        } else if (p->world_name_tail == '<') {
            if (p->world_name_len > 0) {
                p->world_name_len--;
                p->save.name[p->world_name_len] = 0;
            }
        } else {
            if (p->world_name_len + 1 < 7) {
                p->save.name[p->world_name_len] = (s8)p->world_name_tail;
                p->save.name[p->world_name_len + 1] = 0;
            }
            p->world_name_len++;
        }
        if (skip == 0) {
            p->world_name_tail = '@';
            AudioClick2(pnum, 1);
        }
    }

    white = 0xFFFFFF;
    x = sInitialsX[pnum] - 30;
    chr[1] = 0;
    for (i = 0; i < p->world_name_len; i++, x += 18) {
        chr[0] = p->save.name[i];
        DrawTextKeepScale(0.9f, (u16)x - 4, 340, 7, sNameColors[pnum], (u8*)chr);
    }
    if (i < 6) {
        color = (pbLoad & 0x10) ? white : 0x404040;
        chr[0] = (s8)p->world_name_tail;
        DrawTextKeepScale(0.9f, (u16)x - 4, 340, 7, color, (u8*)chr);
        x += 18;
    }
    i++;
    while (i < 6) {
        DrawTextKeepScale(0.9f, (u16)x - 4, 340, 7, white, (u8*)"_");
        i++;
        x += 18;
    }
    if (p->world_name_len >= 6) {
        return 1;
    }
    return 0;
}

void InitGetName(s32 player)
{
    Player* p = &gPlayers[player];
    s32 i;

    p->world_text_active = 0;
    p->world_name_len = 0;
    for (i = 0; i < 7; i++) {
        if (p->save.name[i] != 0) {
            p->world_name_len++;
        }
    }

    if (p->world_name_len >= 5) {
        p->world_name_len = 5;
        p->world_name_tail = (s8)p->save.name[p->world_name_len];
        p->save.name[p->world_name_len] = 0;
    } else {
        p->world_name_tail = 0x40;
    }

    if (gGameMode == MG_PLAYER_SELECT && (gControllerButtons & 4) != 0) {
        p->state = 1;
    }
}
