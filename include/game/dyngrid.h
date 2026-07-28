#ifndef GAME_DYNGRID_H
#define GAME_DYNGRID_H

#include "types.h"

/*
 * Fixed-grid candidate iterators.  The retail names are historically
 * counterintuitive: the Enemy-named pair returns indices into sItems (0xF0
 * stride), while the Item-named pair returns indices into gEnemies (0x394
 * stride).  Callers must therefore index the pool documented below, not infer
 * a pool from the function name.
 */
void StartEnemyGrid(f32 radius, f32* position); /* iterates sItems */
s32 NextGridEnemy(void);

void StartItemGrid(f32 radius, f32* position);  /* iterates gEnemies */
s32 NextGridItem(void);

void SetupDynGrid(void);
void InitDynGrid(f32 item_pad, f32 enemy_pad);

#endif
