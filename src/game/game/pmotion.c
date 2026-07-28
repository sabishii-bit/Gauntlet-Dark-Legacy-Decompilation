/*
 * pmotion.c -- GCN PMOTION.OBJ scaffold.
 *
 * The object begins at get_player_pos immediately after PLAYER.OBJ.  The
 * boundary at 0x80089120 is the first PSFX.OBJ body; exception records also
 * change ownership there (0x80006BE8 / 0x8000AA2C).
 *
 * .text       0x8008091C..0x80089120
 * extab       0x80006B30..0x80006BE8
 * extabindex  0x8000A918..0x8000AA2C
 */

#define STUB(address, name) void name(void) {}

STUB(0x8008091C, get_player_pos)
STUB(0x80081104, try_location)
STUB(0x80081434, PlayerMotion_SetAnimState)
STUB(0x80081504, PlayerMotion)
STUB(0x80085FA0, ModifyPlayerDpos)
STUB(0x8008619C, PlayerCollideWalls)
STUB(0x800862DC, PlayerMotion_FloorFX)
STUB(0x80086470, PlayerKnockback)
STUB(0x800867F0, PlayerMotion_FindClosestPlayer)
STUB(0x80086924, PlayerMotion_HitTarget)
STUB(0x80086A24, PlayerMotion_DamageTarget)
STUB(0x80086C78, PlayerGetTarget)
STUB(0x80087280, DoTransporter)
STUB(0x80087490, DoExit)
STUB(0x8008760C, PlayerCollideEnemies)
STUB(0x80087830, PlayerCollidePlayers)
STUB(0x80087A20, PlayerCollideItems)
STUB(0x80087E14, PlayerNewFloor)
STUB(0x80087F88, PlayerCheckFloor)
STUB(0x80088068, PlayerCollideFloor)
STUB(0x80088688, PlayerCheckMovingFloor)
STUB(0x80088714, fn_80088714)
STUB(0x80088938, fn_80088938)
STUB(0x80088EF4, fn_80088EF4)

#undef STUB
