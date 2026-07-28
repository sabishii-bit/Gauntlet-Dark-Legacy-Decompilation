#include "types.h"

/* GDL in-game message / notification queue (GCN MESSAGE.OBJ region,
 * 0x800A4870-0x800A573C). Names are provisional (no clean PDB anchor on GCN).
 * The system holds up to 4 active message boxes, posts messages by priority
 * from a 256-entry descriptor table, and renders them with the game's text
 * library (fn_8001Fxxx). msgPost/msgDraw are left as stubs (large renderer /
 * priority-insert bodies not yet reconstructed). */

typedef struct MsgDesc {
    /* 0x00 */ int f0;
    /* 0x04 */ int f4;
    /* 0x08 */ int priority;
    /* 0x0C */ int category;
    /* 0x10 */ int type;
    /* 0x14 */ int param;
    /* 0x18 */ int flags;
} MsgDesc;

typedef struct World {
    /* 0x0000 */ char _pad0[4];
    /* 0x0004 */ int f4;
    /* 0x0008 */ char _pad8[4];
    /* 0x000C */ int fC;
    /* 0x0010 */ char _pad10[0xD8];
    /* 0x00E8 */ int state;
    /* 0x00EC */ char _padEC[0x38];
    /* 0x0124 */ int f124;
    /* 0x0128 */ char _pad128[0x1C8C];
    /* 0x1DB4 */ u8 items[0x15A8];
} World;

/* --- shared externs (owned by other TUs) --- */
extern World gWorlds[];                 /* gPlayers, stride 0x335C */
extern void* gMsgBoxes[4];              /* lbl_8028C378 */
extern short gJumpTab120240[];          /* lbl_80120240 */

/* small-data (sda/sbss) globals */
extern int gCurWorld;                   /* lbl_80344CBC */
extern int g3E8;                        /* lbl_803443E8 */
extern int g77C;                        /* gGameMode */
extern int g568;                        /* gGameBusy */
extern int g57C;                        /* gFrameTicks */
extern int g770;                        /* lbl_80344770 */
extern int gMsgIndex;                   /* lbl_80344C98 */
extern int g3B4;                        /* lbl_803443B4 */
extern int g7C0;                        /* lbl_803447C0 */
extern int gA30;                        /* lbl_80344A30 */
extern int gA98;                        /* lbl_80344A98 */
extern int gC9C;                        /* lbl_80344C9C */
extern int gCA0;                        /* lbl_80344CA0 */
extern int gCA4;                        /* lbl_80344CA4 */
extern int gCA8;                        /* lbl_80344CA8 */
extern int gCAC;                        /* lbl_80344CAC */
extern int gCB0;                        /* lbl_80344CB0 */
extern int gCB4;                        /* lbl_80344CB4 */
extern int gCB8;                        /* lbl_80344CB8 */
extern int gCC0;                        /* lbl_80344CC0 */
extern int gCC4;                        /* lbl_80344CC4 */
extern int gCC8;                        /* lbl_80344CC8 */
extern int gCCC;                        /* lbl_80344CCC */
extern int g298;                        /* lbl_80344298 */

/* --- text library --- */
int  fn_8001EDD0(int a, int b, int c);
int  fn_8001F020(int a, int b, float scale);
int  fn_8001F234(int a, int b, int c, int d, int e, int f, int g, int h, int i);
int  fn_8001F48C(int a, int b, int c, int d, int e, int f);
int  fn_8001F93C(int a, int b, int c, int d, int e, int f, int g, int h);
int  fn_8001FBCC(void);
int  fn_8001FBDC(void);
int  fn_8001FD9C(int a, int b, int c, int d);
int  fn_8001FE50(int a, int b);
int  fn_8001FF1C(int a, int b, int* c);
int  fn_800B63B0(int a);
void fn_800B3414(void* box, int flag);
void fn_800B3D6C(void* box);
void get_screen_pos(int a, int* b, int* c, int d);

/* --- forward decls (address order) --- */
void msgUpdate(void);
int  msgPost(int idx, int param, char* str);
void msgDraw(void);
void msgInit(void);
int  msgWidth(int p0, int idx);
int  msgWorldFlags(int who, int worldMask);
int  fn_800A5734(void);

/* --- data --- */
static int gMsgLevels[6] = {0, 0x78, 0xF0, 0x1A4, 0x258, -1};
static char* gMsgFonts[5] = {"SCROLL_A", "SCROLL_A", "SCROLL_A", "SCROLL_A", "SCROLL_A"};
static u32 gMsgCfg[5] = {0x001F1F00, 0x0000001F, 0x001F0000, 0x00001F00, 0x00160C03};
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

/* fn_800A4870 */
void msgUpdate(void)
{
    void** boxes = gMsgBoxes;
    int b568 = g568;
    int i;
    int t;
    int cnt;
    void* box;

    if (b568 == 0) {
        t = gCA4 - g57C;
        gCA4 = t;
        if (t < 0) {
            gCA4 = 0;
        }
    }
    cnt = 0;
    for (i = 0; i < 4; i++) {
        int st = gWorlds[i].state;
        if (st != 1 && (u32)(st - 2) > 1 && st != 5) {
            cnt++;
        }
    }
    if (cnt == 4) {
        gCCC = 0;
    }
    if (gCC4 == 0) {
        return;
    }
    if (gA98 != 0 || gA30 > 0 || g3B4 != 0) {
        box = boxes[gMsgIndex];
        if (box != 0) {
            fn_800B3414(box, 1);
        }
        return;
    }
    if (b568 != 0) {
        if (gCCC > 0) {
            msgDraw();
            box = boxes[gMsgIndex];
            if (box != 0) {
                fn_800B3414(box, 0);
            }
        }
        return;
    }
    t = g770 - g57C;
    g770 = t;
    if (t < 0) {
        g770 = 0;
    }
    t = gCCC - g57C;
    gCCC = t;
    if (t > 0) {
        msgDraw();
        box = boxes[gMsgIndex];
        if (box != 0) {
            fn_800B3414(box, 0);
        }
        return;
    }
    for (i = 0; i < 3; i++) {
        if (boxes[i] != 0) {
            fn_800B3D6C(boxes[i]);
            boxes[i] = 0;
        }
    }
    gCC4 = 0;
    g770 = 0;
    gC9C = 0;
}

/* fn_800A4A38 - TODO: priority-insert / validation body (~387 insns).
 * Validates a message vs its descriptor + world state (via msgWorldFlags),
 * then inserts it into gMsgBoxes[] by priority. Stubbed for now. */
int msgPost(int idx, int param, char* str)
{
    (void)idx; (void)param; (void)str;
    return 0;
}

/* fn_800A5044 - TODO: message renderer (~314 insns), draws the active
 * message box with the fn_8001Fxxx text library. Stubbed for now. */
void msgDraw(void)
{
}

/* fn_800A552C */
void msgInit(void)
{
    int i;

    gC9C = 0;
    for (i = 0; i < 4; i++) {
        gMsgBoxes[i] = 0;
    }
    g770 = 0;
    gCC4 = 0;
    gCA4 = 0;
    gCA0 = 0;
    gMsgIndex = 0;
    gCC8 = 256;
}

/* fn_800A557C */
int msgWidth(int p0, int idx)
{
    int a, b, c;
    int fc;
    int w;

    w = fn_8001F020(gMsgDescTable[idx].type, gMsgDescTable[idx].param, 1.0f);
    if (idx == 50 || idx == 89 || idx == 93) {
        a = fn_8001F020(3, gWorlds[gCurWorld].fC, 1.0f);
        b = fn_8001F020(2, gCurWorld, 1.0f);
        c = a + 12;
        c = c + b;
        if (g3E8 == 1) {
            c += 20;
        }
        if (c > w) {
            w = c;
        }
    } else if (idx == 101) {
        if (g3E8 == 1) {
            fc = gWorlds[gCurWorld].fC;
            a = fn_8001F020(2, gCurWorld, 1.0f);
            b = fn_8001F020(3, fc, 1.0f);
            c = fn_8001F020(24, 1, 1.0f);
            c = a + b + c + 20;
            if (c > w) {
                w = c;
            }
        }
    }
    return w;
}

/* fn_800A56BC */
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
        World* w = &gWorlds[i];
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
