#include "types.h"
#include "game/gamemode.h"

/* GDL in-game message / notification queue (GCN MESSAGE.OBJ region,
 * 0x800A4870-0x800A573C). Names are provisional (no clean PDB anchor on GCN).
 * The system holds up to 4 active message boxes, posts messages by priority
 * from a 256-entry descriptor table, and renders localized messages with the
 * game's text library. */

typedef struct MsgDesc {
    /* 0x00 */ int f0;
    /* 0x04 */ int f4;
    /* 0x08 */ int priority;
    /* 0x0C */ int category;
    /* 0x10 */ int type;
    /* 0x14 */ int param;
    /* 0x18 */ int flags;
} MsgDesc;

typedef struct MsgData {
    int levels[6];
    char* fonts[5];
    u32 cfg[5];
    MsgDesc desc[256];
} MsgData;

/* Header block of MsgData: the message-delay ladder, the blit font names and
 * the per-font colour words live in one 0x40-byte read/write object, ahead of
 * the descriptor table. */
typedef struct MsgCfgBlock {
    int levels[6];
    char* fonts[5];
    u32 cfg[5];
} MsgCfgBlock;

typedef struct World {
    /* 0x0000 */ char _pad0[4];
    /* 0x0004 */ int class_id;
    /* 0x0008 */ char _pad8[4];
    /* 0x000C */ int character;
    /* 0x0010 */ char _pad10[0xD8];
    /* 0x00E8 */ int state;
    /* 0x00EC */ char _padEC[0x38];
    /* 0x0124 */ int flags;
    /* 0x0128 */ char _pad128[0x1C8C];
    /* 0x1DB4 */ u8 items[0x15A8];
} World;

/* --- shared externs (owned by other TUs) --- */
extern World gPlayers[];                /* stride 0x335C */
extern void* gMsgBoxes[4];
extern short lbl_80120240[];          /* lbl_80120240 */

/* small-data (sda/sbss) globals */
extern int gCurWorld;
extern int gLanguageId;
extern s32 gGameMode;   /* game mode; see enum e_mode */
extern int gGameBusy;
extern int gFrameTicks;
extern int gGameplayPauseTimer;
extern int gMsgIndex;
extern int gTriggerCameraState;
extern int lbl_803447C0;                        /* lbl_803447C0 */
extern int gModalRenderDepth;
extern int options_state;
extern int gMessageState;
extern int gMessageDelayIndex;
extern int gMessageDelay;
extern int gMessageValue;
extern int gMessageTextArg;
extern int gMessageFontFlags;
extern int gMessageCenterY;
extern int gMessageCenterX;
extern int gCurrentMessage;
extern int gMessageActive;
extern int gMsgDescCount;
extern int gMessageTimer;
extern int lbl_80344298;
extern u64 gControllerButtons;
extern int sFlags;

/* --- text library --- */
int  StringTextNum(int text);
int  StringTextHeight(float scale, int text, int param, int lines);
int  StringTextWidth(float scale, int text, int param);
void DrawStringTextMLines(int x, int y, int flags, int color, int lines,
                          int text, ...);
int  DrawStringTextMulti(int x, int y, int spacing, int font, int color,
                         int text);
int  DrawStringText(int x, int y, u32 flags, u32 color, int text, int param,
                    ...);
float RestoreDrawStringScale(void);
float SetDrawStringScale(float scale);
char* GetStringListText(int a, int b, int c, u32* d);
int  GetStringListMsg(int a, int b);
char* GetStringText(int a, int b, u32* c);
u32  MBSetFontFlags(u32 a);
void mbBlitInit3414(void* box, int flag);
void MBRemoveBlit(void* box);
void* MBNewBlit(char* name, int x, int y);
void mbBlitProject(void* box, int width, int height);
void MBBlitSetAlpha(void* box, int alpha);
void get_screen_pos(int camera, int* x, int* y, void* position);
void fn_8009CD80(int player, int value, int count);
void fn_8009CB44(int player, u32 flags, int arg);

/* --- forward decls (address order) --- */
void msgUpdate(void);
int  msgPost(int idx, int param, char* str);
void msgDraw(void);
void msgInit(void);
int  msgWidth(int p0, int idx);
int  msgWorldFlags(int who, int worldMask);
int  fn_800A5734(void);

/* --- data --- */
static MsgCfgBlock lbl_80124E58 = {
    {0, 0x78, 0xF0, 0x1A4, 0x258, -1},
    {"SCROLL_A", "SCROLL_A", "SCROLL_A", "SCROLL_A", "SCROLL_A"},
    {0x001F1F00, 0x0000001F, 0x001F0000, 0x00001F00, 0x00160C03},
};
static MsgDesc gMsgDescTable[256] = {
    {0, 0, 0x32, 3, 0x22, -1, 0x10007},
    {0, 0, 0x32, 3, 0x23, -1, 0x1000F},
    {0, 0, 0x32, 3, 0x24, -1, 0x10010},
    {0, 0, 0x32, 3, 0x25, -1, 0x10013},
    {0, 0, 0x32, 3, 0x26, -1, 0x10014},
    {1, 0, 0x32, 3, 0x44, -1, 0x10015},
    {1, 0, 0x32, 3, 0x28, -1, 0x10009},
    {1, 0, 0x32, 3, 0x29, -1, 0x10008},
    {1, 0, 0x32, 3, 0x2A, -1, 0x1000E},
    {1, 0, 0x32, 3, 0x2B, -1, 0x10018},
    {1, 0, 0x32, 3, -1, -1, 0},
    {0, 0, 0x32, 3, 0x2C, -1, 0x10019},
    {0, 0, 0x32, 3, -1, -1, -1},
    {1, 0, 0x32, 3, -1, -1, -1},
    {1, 0, 0x32, 3, 0x2F, -1, 0x1000C},
    {0, 0, 0x32, 3, 0x8D, -1, 0x1001A},
    {0, 0, 0x32, 3, 0x8E, -1, 0x1001B},
    {0, 0, 0x32, 3, 0x5B, -1, 0x10021},
    {1, 0, 0x32, 3, 0x30, -1, 0x1000D},
    {1, 0, 0x32, 3, 0x31, -1, 0x10016},
    {1, 0, 0x32, 3, 0x32, -1, 0x10017},
    {1, 0, 0x32, 3, 0x33, -1, 0x10022},
    {0, 0, 0x32, 3, 0x34, -1, 0x10023},
    {1, 0, 0x32, 3, 0x35, -1, 0x10024},
    {0, 0, 0x32, 3, -1, -1, -1},
    {0, 0, 0x32, 3, -1, -1, -1},
    {0, 0, 0x32, 3, -1, -1, 0},
    {1, 0, 0x32, 3, 0x36, -1, 0x10025},
    {0, 0, 0x32, 3, 0x5C, -1, 0x1001C},
    {0, 0, 0x32, 0, 0x37, -1, 0x10028},
    {0, 0, 0x32, 3, -1, -1, 0x20011},
    {0, 0, 0x32, 3, -1, -1, 0x20012},
    {0, 0, 0x32, 3, 0x5F, -1, 0x20013},
    {0, 0, 0x32, 3, 0x60, -1, 0x20014},
    {0, 0, 0x32, 0, 0x61, -1, 0x1003D},
    {0, 0, 0x32, 3, 0x63, -1, 0x20016},
    {0, 0, 0x32, 3, 0x62, -1, 0x20015},
    {0, 0, 0x32, 3, 0x64, -1, 0x20019},
    {0, 0, 0x32, 3, 0x65, -1, 0x2001B},
    {0, 0, 0x32, 3, 0x66, -1, 0x2001D},
    {0, 0, 0x32, 3, 0x67, -1, 0x20020},
    {0, 0, 0x32, 3, 0x68, -1, 0x20021},
    {0, 0, 0x32, 3, 0x69, -1, 0x20022},
    {0, 0, 0x32, 3, 0x6A, -1, 0x20023},
    {1, 0, 0x32, 3, 0x38, -1, 0x10026},
    {1, 0, 0x32, 3, 0x39, -1, 0x10027},
    {0, 0, 0x32, 2, 0x3A, -1, -1},
    {0, 0, 0x32, 3, 0x6B, -1, 0x2001A},
    {0, 0, 0x32, 3, 0x6C, -1, 0x2001C},
    {0, 0, 0x32, 3, 0x6D, -1, 0x2001F},
    {0, 0, 0x28, 0, 0x6E, -1, 0x2000F},
    {0, 0, 0x32, 3, 0x6F, -1, 0x2001E},
    {0, 0, 0x32, 3, 0x70, -1, 0x20028},
    {0, 0, 0x32, 3, 0x71, -1, 0x20017},
    {0, 0, 0x32, 3, 0x63, -1, 0x20016},
    {0, 0, 0x32, 3, -1, -1, -1},
    {0, 0, 0x32, 2, -1, 0, -1},
    {0, 0, 0x3C, 1, 0x19, 1, 0x40025},
    {0, 0, 0x46, 1, 0x19, 2, 0x40026},
    {0, 0, 0x32, 2, -1, 0, -1},
    {0, 0, 0x3C, 1, 0x1A, 1, 0x50024},
    {0, 0, 0x46, 1, 0x1A, 2, 0x50025},
    {0, 0, 0x32, 2, -1, 0, -1},
    {0, 0, 0x3C, 1, 0x1B, 1, 0x60024},
    {0, 0, 0x46, 1, 0x1B, 2, 0x60025},
    {0, 0, 0x32, 2, -1, 0, -1},
    {0, 0, 0x3C, 1, 0x1C, 1, 0x70027},
    {0, 0, 0x46, 1, 0x1C, 2, 0x70028},
    {0, 0, 0x32, 2, -1, 0, -1},
    {0, 0, 0x3C, 1, 0x1D, 1, 0x80024},
    {0, 0, 0x46, 1, 0x1D, 2, 0x80025},
    {0, 0, 0x32, 2, -1, 0, -1},
    {0, 0, 0x3C, 1, 0x1E, 1, 0x90024},
    {0, 0, 0x46, 1, 0x1E, 2, 0x90025},
    {0, 0, 0x32, 2, -1, 0, -1},
    {0, 0, 0x3C, 1, 0x1F, 1, 0xA0025},
    {0, 0, 0x46, 1, 0x1F, 2, 0xA0026},
    {0, 0, 0x32, 2, -1, 0, -1},
    {0, 0, 0x3C, 1, 0x20, 1, 0xB0027},
    {0, 0, 0x46, 1, 0x20, 2, 0xB0028},
    {0, 0, 0x5A, 3, -1, -1, 0},
    {0, 0, 0x32, 3, 0x72, -1, 0x20024},
    {0, 0, 0x32, 3, 0x73, -1, 0x20027},
    {0, 0, 0x32, 3, 0x74, -1, 0x20025},
    {0, 0, 0x32, 3, 0x75, -1, 0x2002B},
    {0, 0, 0x32, 3, 0x76, -1, 0x20026},
    {0, 0, 0x32, 3, 0x77, -1, 0x2002D},
    {0, 0, 0x32, 3, 0x78, -1, 0x20031},
    {0, 0, 0x32, 3, 0x79, -1, 0x20018},
    {0, 0, 0x32, 3, 0x7A, -1, 0x2002C},
    {0, 0, 0x32, 0, 0x3B, -1, 0x10041},
    {0, 0, 0x32, 3, 0x7B, -1, 0x20029},
    {0, 0, 0x32, 3, 0x7C, -1, 0x2002A},
    {0, 0, 0x32, 0, 0x7D, -1, 0x1002C},
    {0, 0, 0x32, 3, 0x3D, -1, 0x1000A},
    {0, 0, 0x32, 3, 0x3E, -1, 0x1000B},
    {0, 0, 0x5A, 0, -1, -1, -1},
    {0, 0, 0x32, 3, 0x40, -1, -1},
    {0, 0, 0x32, 3, 0x7E, -1, 0x2002E},
    {0, 0, 0x32, 3, 0x7F, -1, 0x2002F},
    {0, 0, 0x32, 3, 0x80, -1, 0x20030},
    {0, 0, 0x32, 0, 0x18, -1, 0x1003D},
    {0, 0, 0x46, 2, 0x21, 0, 0x40027},
    {0, 0, 0x46, 2, 0x21, 1, 0x50026},
    {0, 0, 0x46, 2, 0x21, 2, 0x60026},
    {0, 0, 0x46, 2, 0x21, 3, 0x70029},
    {0, 0, 0x46, 2, 0x21, 4, 0x80026},
    {0, 0, 0x46, 2, 0x21, 5, 0x90026},
    {0, 0, 0x46, 2, 0x21, 6, 0xA0027},
    {0, 0, 0x46, 2, 0x21, 7, 0xB0029},
    {0, 0, 0x32, 3, 0x41, -1, 0x10011},
    {0, 0, 0x32, 3, 0x42, -1, 0x10012},
    {0, 0, 0x32, 1, -1, -1, -1},
    {0, 0, 0x32, 3, 0x81, -1, 0x10002},
    {0, 0, 0x32, 3, 0x82, -1, 0x20039},
    {0, 0, 0x32, 3, 0x83, -1, 0x20035},
    {0, 0, 0x32, 3, 0x84, -1, 0x2003A},
    {0, 0, 0x32, 3, 0x85, -1, 0x20037},
    {0, 0, 0x32, 3, 0x86, -1, 0x20036},
    {0, 0, 0x32, 3, 0x87, -1, 0x20036},
    {0, 0, 0x32, 3, 0x88, -1, 0x2003C},
    {0, 0, 0x32, 3, 0x89, -1, 0x20036},
    {0, 0, 0x32, 3, 0x8A, -1, 0x2003D},
    {0, 0, 0x32, 3, 0x8B, -1, 0x20038},
    {0, 0, 0x32, 3, 0x8C, -1, 0x2003B},
    {0, 0, 0x32, 2, 0x45, -1, 0x1002E},
    {0, 0, 0x32, 1, 0x46, -1, 0x1002F},
    {0, 0, 0x32, 1, 0x47, -1, 0x10040},
    {0, 0, 0x32, 3, 0x48, -1, 0x10020},
    {0, 0, 0x32, 3, 0x49, -1, 0x1001F},
    {0, 0, 0x32, 3, 0x4A, -1, 0x1001E},
    {0, 0, 0x32, 3, 0x4B, -1, 0x1001D},
    {0, 0, 0x32, 3, 0x8F, -1, 0x10042},
    {0, 0, 0x32, 2, 0x4C, -1, 0x10043},
    {0, 0, 0x32, 3, 0x4D, -1, 0x10044},
    {0, 0, 0x32, 3, 0x4E, -1, 0x10052},
    {0, 0, 0x32, 3, 0x4F, -1, 0x10053},
    {0, 0, 0x32, 3, 0x50, -1, 0x10054},
    {0, 0, 0x32, 3, 0x51, -1, 0x10059},
    {0, 0, 0x32, 2, 0x52, -1, 0x1004B},
    {0, 0, 0x32, 2, 0x53, -1, 0x10050},
    {0, 0, 0x32, 2, 0x54, -1, 0x1004E},
    {0, 0, 0x32, 2, 0x55, -1, 0x1004D},
    {0, 0, 0x32, 2, 0x56, -1, 0x1004A},
    {0, 0, 0x32, 2, 0x57, -1, 0x1004F},
    {0, 0, 0x32, 2, 0x58, -1, 0x1004C},
    {0, 0, 0x32, 2, 0x59, -1, 0x10051},
    {0, 0, 0x32, 2, 0x5A, -1, 0x10049},
    {0, 0, 0x32, 3, 0xCA, -1, 0x2A},
    {0, 0, 0x32, 3, 0xCB, -1, 0x2A},
    {0, 0, 0x32, 3, 0xCC, -1, 0x2A},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},
};

/* msgUpdate */
void msgUpdate(void)
{
    void** boxes = gMsgBoxes;
    int b568 = gGameBusy;
    int i;
    int t;
    int cnt;
    void* box;

    if (b568 == 0) {
        t = gMessageDelay - gFrameTicks;
        gMessageDelay = t;
        if (t < 0) {
            gMessageDelay = 0;
        }
    }
    cnt = 0;
    for (i = 0; i < 4; i++) {
        int st = gPlayers[i].state;
        if (st == 1 || (u32)(st - 2) <= 1 || st == 5) {
            break;
        }
        cnt++;
    }
    if (cnt == 4) {
        gMessageTimer = 0;
    }
    if (gMessageActive == 0) {
        return;
    }
    if (options_state != 0 || gModalRenderDepth > 0 || gTriggerCameraState != 0) {
        if ((box = boxes[gMsgIndex]) != 0) {
            mbBlitInit3414(box, 1);
        }
        return;
    }
    if (b568 != 0) {
        if (gMessageTimer > 0) {
            msgDraw();
            box = boxes[gMsgIndex];
            if (box != 0) {
                mbBlitInit3414(box, 0);
            }
        }
        return;
    }
    t = gGameplayPauseTimer - gFrameTicks;
    gGameplayPauseTimer = t;
    if (t < 0) {
        gGameplayPauseTimer = 0;
    }
    t = gMessageTimer - gFrameTicks;
    gMessageTimer = t;
    if (t > 0) {
        msgDraw();
        box = boxes[gMsgIndex];
        if (box != 0) {
            mbBlitInit3414(box, 0);
        }
        return;
    }
    for (i = 0; i < 3; i++) {
        if (boxes[i] != 0) {
            MBRemoveBlit(boxes[i]);
            boxes[i] = 0;
        }
    }
    gMessageActive = 0;
    gGameplayPauseTimer = 0;
    gMessageState = 0;
}

/* Validate, place, and activate one descriptor-table message. */
int msgPost(int idx, int param, char* position)
{
    MsgDesc* desc;
    MsgData* msgData;
    void** boxes;
    int fontIndex;
    int initialLineCount;
    int lineCount;
    int textType;
    int textParam;
    int left;
    int top;
    int width;
    int height;
    u8 framePad[8];
    int centerX;
    int centerY;
    int worldFlags;
    int category;
    int last;
    int first;
    int count;
    int i;
    int descOffset;
    int playerOffset;
    u8 unused[4];

    msgData = (MsgData*)&lbl_80124E58;
    descOffset = idx * sizeof(MsgDesc);
    desc = (MsgDesc*)((u8*)&msgData->desc[0] + descOffset);
    boxes = gMsgBoxes;

    if (idx < 0 || idx >= gMsgDescCount) {
        return 0;
    }
    if (desc->type < 0) {
        return 0;
    }
    if (gTriggerCameraState != 0) {
        return 0;
    }

    if (lbl_803447C0 != 0) {
        initialLineCount = 12;
    } else {
        initialLineCount = 8;
    }
    lineCount = initialLineCount;
    if (gLanguageId == 1) {
        lineCount = 14;
    }

    category = desc->category;
    switch (category) {
    case 0:
        break;
    case 1:
        if ((msgWorldFlags(idx, -1) & 0xF) != 0) {
            return 0;
        }
        break;
    case 2:
        if (msgWorldFlags(idx, param) != 0) {
            return 0;
        }
        break;
    case 3:
    default:
        if ((msgWorldFlags(idx, -1) & 0x100) != 0) {
            return 0;
        }
        break;
    }

    if (gMessageActive != 0) {
        u8* priorityBase = (u8*)&msgData->desc[0].priority;

        if (*(int*)(priorityBase + gCurrentMessage * sizeof(MsgDesc)) >=
            *(int*)(priorityBase + descOffset)) {
            return 0;
        }
    }
    if ((idx <= 0x1C || (idx >= 0x2C && idx <= 0x2D) ||
         idx == 0x37 || idx == 0x50) &&
        gGameMode != MA_DEMO && gGameMode != MA_INSTRUCT && gMessageDelay > 0) {
        return 0;
    }

    if (param >= 0) {
        fontIndex = param;
    } else {
        fontIndex = 4;
    }
    if (param < 0) {
        param = 0;
    }
    gMsgIndex = 0;
    if (position != 0) {
        get_screen_pos(0, &centerX, &centerY, position);
        centerY -= 0x3E;
    } else {
        if (param >= 0) {
            centerX = (u16)lbl_80120240[param];
            centerY = 0xFA;
        } else {
            centerX = 0x100;
            centerY = 0xC0;
        }
    }

    if (desc->param >= 0) {
        descOffset = 1;
    } else {
        descOffset = StringTextNum(desc->type);
    }
    textType = desc->type;
    textParam = desc->param;
    height = StringTextHeight(1.0f, textType, textParam, lineCount) + 0x10;
    top = centerY - (height >> 1);
    if (top < 2) {
        centerY += 2 - top;
        top = 2;
    } else if (top + height > 0x130) {
        centerY += 0x130 - (top + height);
        top = 0x130 - height;
    }
    gMessageCenterY = centerY;

    width = msgWidth(param, idx) + 0x40;
    left = centerX - (width >> 1);
    if (left < 0) {
        centerX -= left;
        left = 0;
    } else if (left + width > 0x1FF) {
        centerX -= left + width - 0x1FF;
        left = 0x1FF - width;
    }
    gMessageCenterX = centerX;

    if (boxes[gMsgIndex] != 0) {
        MBRemoveBlit(boxes[gMsgIndex]);
        boxes[gMsgIndex] = 0;
    }
    {
        void* newBox = MBNewBlit(msgData->fonts[fontIndex], left, top);
        void** boxSlot = &boxes[gMsgIndex];

        *boxSlot = newBox;
        mbBlitProject(*boxSlot, width, height);
    }
    MBBlitSetAlpha(boxes[gMsgIndex], 0x40);
    mbBlitInit3414(boxes[gMsgIndex], 0);

    gMessageTextArg = 1;
    gMessageFontFlags = msgData->cfg[fontIndex];
    gCurWorld = param;
    gCurrentMessage = idx;
    gMessageValue = *(int*)((u8*)&gPlayers[param] + 0x3324);
    gMessageActive = 1;
    msgDraw();
    gMessageTimer = descOffset * 0x3C + 0x1E;

    category = desc->category;
    switch (category) {
    case 0:
        break;
    case 2:
        if (param < 0) {
            first = 0;
            last = 3;
        } else {
            first = param;
            last = param;
        }
        playerOffset = first * 0x335C;
        for (i = first; i <= last; i++, playerOffset += 0x335C) {
            World* world = (World*)((u8*)gPlayers + playerOffset);
            if (world->state != 0) {
                world->items[idx] |= 0x11;
            }
        }
        break;
    case 1:
    case 3:
    default:
        playerOffset = 0;
        for (i = 0; i < 4; i++, playerOffset += 0x335C) {
            World* world = (World*)((u8*)gPlayers + playerOffset);
            if (world->state != 0) {
                world->items[idx] |= 0x11;
            }
        }
        break;
    }

    category = desc->f4;
    if (gGameMode == MA_INSTRUCT || gMessageState != 0) {
        category = -1;
    }
    switch (category) {
    default:
        gGameplayPauseTimer = 0;
        break;
    case 1:
        gGameplayPauseTimer = 0x1E;
        break;
    case -1:
        gGameplayPauseTimer = 0x3C;
        break;
    }

    if (desc->flags != 0) {
        if (idx == 0x65 && gMessageValue >= 10) {
            if (gMessageValue >= 99) {
                fn_8009CD80(param, 0, 99);
            } else {
                fn_8009CD80(param, gPlayers[param].character, gMessageValue);
            }
        } else {
            fn_8009CB44(param, desc->flags, -1);
        }
        if ((gControllerButtons & 0x10) == 0) {
            if (gGameMode == MA_INSTRUCT || gGameMode == MA_DEMO) {
                gMessageDelay = 0x3C;
            } else {
                int delayIndex = gMessageDelayIndex++;
                gMessageDelay = msgData->levels[delayIndex];
                if (msgData->levels[gMessageDelayIndex] < 0) {
                    gMessageDelayIndex--;
                }
            }
        } else {
            gMessageDelay = 0;
        }
    }
    return -1;
}

/* Render the active message, including the localized player/world variants. */
#pragma opt_propagation off
void msgDraw(void)
{
    MsgDesc* desc;
    int tens;
    int centerY;
    int lineHeight;
    int playerClass;
    int playerWorld;
    u32 oldFlags;
    char* text;
    char* classText;
    char* worldText;
    char* specialWorldText;
    int worldWidth;
    int classWidth;
    int labelWidth;
    int numberWidth;
    int x;
    int y;
    int textMsg;
    int scratch[6];
    u32 color;
    volatile u32 stackPad;

    desc = &gMsgDescTable[gCurrentMessage];
    centerY = gMessageCenterY | 0x1000;
    playerClass = gPlayers[gCurWorld].character;
    playerWorld = gPlayers[gCurWorld].class_id;

    if (((gGameMode & MODE_GROUP_ATTRACT) == 0 || lbl_80344298 == 0) && desc->type >= 0) {
        oldFlags = MBSetFontFlags(0x02000000);
        if (gCurrentMessage == 0x65 && gMessageValue >= 10) {
        tens = gMessageValue / 10;
        if (gMessageValue == 99) {
            text = GetStringText(0x15, 0, &color);
        } else {
            text = GetStringListText(0, playerClass, tens >> 1, &color);
        }
        worldText = GetStringText(2, playerWorld, (u32*)scratch);
        classText = GetStringText(3, playerClass, 0);
        lineHeight = StringTextHeight(1.0f, 0x18, 0, 2) + 2;

        if (gLanguageId == 1) {
            worldWidth = StringTextWidth(1.0f, 2, playerWorld);
            classWidth = StringTextWidth(1.0f, 3, playerClass);
            labelWidth = StringTextWidth(1.0f, 0x18, 0);
            y = worldWidth + classWidth;
            labelWidth = y + labelWidth;
            x = gMessageCenterX - (labelWidth + 0x14) / 2;
            labelWidth = centerY - lineHeight;
            DrawStringText(x, labelWidth, -1, gMessageFontFlags, 2, playerWorld);
            worldWidth = x + worldWidth;
            DrawStringText(worldWidth + 10, labelWidth, -1, gMessageFontFlags, 3, playerClass);
            y = worldWidth + classWidth;
            DrawStringText(y + 0x14, labelWidth, -1, gMessageFontFlags,
                           0x18, 0);
            DrawStringText(-gMessageCenterX, centerY, -1, gMessageFontFlags,
                           0x18, 1, gMessageValue);

            if (gMessageValue == 99) {
                textMsg = 0x15;
                classWidth = 0;
            } else {
                textMsg = GetStringListMsg(0, playerClass);
                classWidth = tens >> 1;
            }
            numberWidth = StringTextWidth(1.25f, textMsg, classWidth);
            labelWidth = StringTextWidth(1.0f, 0x18, 2);
            labelWidth = numberWidth + labelWidth;
            worldWidth = gMessageCenterX - (labelWidth + 0x10) / 2;
            SetDrawStringScale(1.25f);
            x = centerY + lineHeight;
            DrawStringText(worldWidth, x + 2, color,
                           gMessageFontFlags, textMsg, classWidth);
            RestoreDrawStringScale();
            DrawStringText(worldWidth + numberWidth + 0x10, x, -1,
                           gMessageFontFlags, 0x18, 2);
        } else {
            DrawStringText(-gMessageCenterX, centerY - lineHeight, -1,
                           gMessageFontFlags, 0x18, 0, worldText, classText);
            DrawStringText(-gMessageCenterX, centerY, -1, gMessageFontFlags,
                           0x18, 1);
            DrawStringText(-gMessageCenterX, centerY + lineHeight, -1,
                           gMessageFontFlags, 0x18, 2, gMessageValue, text);
        }
        } else if (gCurrentMessage == 0x22) {
            DrawStringTextMLines(-gMessageCenterX, centerY, 2, -1,
                                 gMessageFontFlags, desc->type, gMessageValue);
        } else if (gCurrentMessage == 0x32 || gCurrentMessage == 0x59 ||
                   gCurrentMessage == 0x5D) {
            specialWorldText = GetStringText(2, playerWorld, 0);
            text = GetStringText(3, playerClass, 0);
            lineHeight = StringTextHeight(1.0f, desc->type, 0, 2) + 2;
            lineHeight >>= 1;
            if ((gPlayers[gCurWorld].flags & 0x400) != 0 &&
                gCurrentMessage != 0x5D) {
                DrawStringText(-gMessageCenterX, centerY - lineHeight, -1,
                               gMessageFontFlags, 4, 0);
            } else {
                DrawStringText(-gMessageCenterX, centerY - lineHeight,
                               gMessageTextArg, gMessageFontFlags, desc->type, 0,
                               specialWorldText, text);
            }
            DrawStringText(-gMessageCenterX, centerY + lineHeight,
                           gMessageTextArg, gMessageFontFlags, desc->type, 1);
        } else if (desc->param >= 0) {
            DrawStringText(-gMessageCenterX, centerY, -1, gMessageFontFlags,
                           desc->type, desc->param);
        } else {
            DrawStringTextMulti(-gMessageCenterX, centerY, 2, -1,
                                gMessageFontFlags, desc->type);
        }
        MBSetFontFlags(oldFlags);
    }
}
#pragma opt_propagation reset

/* msgInit */
void msgInit(void)
{
    int i;

    gMessageState = 0;
    for (i = 0; i < 4; i++) {
        gMsgBoxes[i] = 0;
    }
    gGameplayPauseTimer = 0;
    gMessageActive = 0;
    gMessageDelay = 0;
    gMessageDelayIndex = 0;
    gMsgIndex = 0;
    gMsgDescCount = 256;
}

#pragma opt_propagation off
#pragma opt_lifetimes off
/* msgWidth */
int msgWidth(int p0, int idx)
{
    int a, b, c;
    int fc;
    int w;

    w = StringTextWidth(1.0f, gMsgDescTable[idx].type, gMsgDescTable[idx].param);
    if (idx == 50 || idx == 89 || idx == 93) {
        a = StringTextWidth(1.0f, 3, gPlayers[gCurWorld].character);
        p0 = gCurWorld;
        b = StringTextWidth(1.0f, 2, p0);
        c = a + 12;
        c = b + c;
        if (gLanguageId == 1) {
            c += 20;
        }
        if (c > w) {
            w = c;
        }
    } else if (idx == 101) {
        if (gLanguageId == 1) {
            s32 sum;
            s32 branchWidth;

            fc = gPlayers[gCurWorld].character;
            a = StringTextWidth(1.0f, 2, gCurWorld);
            b = StringTextWidth(1.0f, 3, fc);
            branchWidth = StringTextWidth(1.0f, 24, 1);
            sum = a + b;
            branchWidth = sum + branchWidth;
            branchWidth += 20;
            if (branchWidth > w) {
                w = branchWidth;
            }
        }
    }
    return w;
}
#pragma opt_lifetimes reset
#pragma opt_propagation reset

/* msgWorldFlags */
int msgWorldFlags(int who, int worldMask)
{
    int i;
    int last;
    int acc;
    u8 b;

    acc = 256;
    if (worldMask < 0) {
        worldMask = 0;
        last = 3;
    } else {
        last = worldMask;
    }
    for (i = worldMask; i <= last; i++) {
        World* w = &gPlayers[i];
        if (w->state != 0) {
            b = w->items[who];
            if (b != 0) {
                acc |= b;
            } else {
                acc &= 0xFF;
            }
        }
    }
    return acc;
}

/* fn_800A5734 */
int fn_800A5734(void)
{
    return 2;
}
