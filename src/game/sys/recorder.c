/*
 * recorder.c -- demo-stage snapshot storage.
 *
 * .text       0x8008BF88..0x8008C52C
 * .bss        0x80282940..0x80284878
 * .sbss       0x80344B50..0x80344B90
 * extab       0x80006C60..0x80006C80
 * extabindex  0x8000AAE0..0x8000AB10
 */

#include "types.h"
#include "game/item.h"
#include "game/player.h"
#include "game/camera.h"

static u8 lbl_80282940[0x18C];
static f32 sCameraPos[3];
static f32 sReticlePos[7][3];
static f32 sReticleDepth[11];
static u8 sPlayerByte17[152];
static u8 sPlayerByte16[152];
static s32 sItemType[1024];
static s16 sItemActive[1520];
static f32 sSavedPlayerPos[4];

extern Player gPlayers[4];
extern void* sItemWobjTargets[];
extern f32 lbl_8023F83C[3];
extern f32 lbl_8023F848[7];
extern f32 lbl_8023F864[7][3];

extern s32 sNumItems;
extern s32 sNumItemWobjs;
extern s32 sLastWorldLevel;

s32 lbl_80344B50 = 0;
s32 lbl_80344B54 = 0;
s32 lbl_80344B58 = 0;
f32 lbl_80344B5C = 0.0f;
f32 lbl_80344B60 = 0.0f;
s32 lbl_80344B64 = 0;
f32 lbl_80344B68 = 0.0f;
f32 lbl_80344B6C = 0.0f;
s32 lbl_80344B70 = 0;
s32 lbl_80344B74 = 0;
s32 lbl_80344B78 = 0;
s32 lbl_80344B7C = 0;
s32 lbl_80344B80 = 0;
s32 lbl_80344B84 = 0;
s32 lbl_80344B88[2];

extern s32 lbl_8034454C;
extern s32 lbl_80344510;
extern s32 lbl_8034450C;
extern s32 lbl_80344508;
extern f32 lbl_80344534;
extern f32 lbl_80344530;
extern s32 lbl_803444DC;
extern f32 lbl_803444D8;
extern f32 lbl_803444D4;
extern s32 lbl_803447D0;

extern void ResolveWorldData(s32 level);
extern void MBTreeSetFlags(void* node, s32 mask, s32 value);
extern void get_player_pos(s32 player, s32 mode);
extern void UpdatePlayerWorldMat(Player* player, s32 force);
extern void camera_mode_level(s32 mode);
extern void fn_80051C78(void);
extern void fn_8002E328(void* source, void* destination);

void LoadStage(void);
void SaveStage(void);

void LoadAllRecords(void) {
    Item* item;
    s32 i;
    s32 activeOffset;
    s32 typeOffset;

    ResolveWorldData(lbl_80344B84);
    LoadStage();

    item = sItems;
    i = 0;
    activeOffset = 0;
    typeOffset = 0;
    while (i < sNumItems) {
        if (item->info == 0 ||
            item->info->type != *(s32*)((u8*)sItemType + typeOffset)) {
            item->active = -1;
            if (item->objgrp.node != 0) {
                MBTreeSetFlags(item->objgrp.node, 2, 0);
            }
        } else {
            item->active = *(s16*)((u8*)sItemActive + activeOffset);
            if (item->active == -1 && item->objgrp.node != 0) {
                MBTreeSetFlags(item->objgrp.node, 2, 0);
            }
            if (item->info->type == 3 && (item->active & 1) == 0) {
                item->armor = -1;
                item->data[6] = 0;
                item->active = -1;
                if (item->objgrp.node != 0) {
                    MBTreeSetFlags(item->objgrp.node, 2, 0);
                }
            }
        }
        i++;
        activeOffset += 2;
        typeOffset += 4;
        item++;
    }

    {
        s32 playerIndex;

        for (playerIndex = 0;
             playerIndex < sNumItemWobjs;
             playerIndex++) {
            ((u8*)sItemWobjTargets[playerIndex])[0x16] =
                sPlayerByte16[playerIndex];
            ((u8*)sItemWobjTargets[playerIndex])[0x17] =
                sPlayerByte17[playerIndex];
        }
    }
}

void SaveAllRecords_8008C0F4(s32 excludedItem, s32 recordArg, f32* playerPos) {
    s32 count;
    Item* item;
    u8* player;
    s32 i;

    lbl_80344B88[0] = recordArg;
    sSavedPlayerPos[0] = playerPos[0];
    sSavedPlayerPos[1] = playerPos[1];
    sSavedPlayerPos[2] = playerPos[2];
    lbl_80344B84 = sLastWorldLevel;
    SaveStage();

    count = sNumItems;
    item = sItems;
    if (count < 0) {
        count = 0;
    } else if (count > 1024) {
        count = 1024;
    }

    for (i = 0; i < count; i++, item++) {
        if (i == excludedItem || item->info == 0) {
            sItemActive[i] = -1;
        } else if (item->info->type == 2 && item->action > 0) {
            sItemActive[i] = -1;
        } else {
            sItemActive[i] = item->active;
            sItemType[i] = item->info->type;
        }
    }

    for (; i < 1024; i++) {
        sItemActive[i] = -1;
        sItemType[i] = 1;
    }
    lbl_80344B78 = count;

    count = sNumItemWobjs;
    if (count < 0) {
        count = 0;
    } else if (count > 150) {
        count = 150;
    }
    {
        s32 playerIndex;

        for (playerIndex = 0;
             playerIndex < count;
             playerIndex++) {
            player = (u8*)sItemWobjTargets[playerIndex];
            sPlayerByte16[playerIndex] = player[0x16];
            sPlayerByte17[playerIndex] = player[0x17];
        }
    }
    lbl_80344B74 = count;
}

void LoadStage(void) {
    u8 unused[16];
    s32 i;
    s32 first = 1;

    for (i = 0; i < 4; i++) {
        if (gPlayers[i].state == 1) {
            if (first != 0) {
                gPlayers[i].pos[0] = sSavedPlayerPos[0];
                first = 0;
                gPlayers[i].pos[1] = sSavedPlayerPos[1];
                gPlayers[i].pos[2] = sSavedPlayerPos[2];
            } else {
                get_player_pos(i, 2);
            }
        }
        UpdatePlayerWorldMat(&gPlayers[i], 1);
    }

    camera_mode_level(1);
    lbl_803447D0 = 0;
    fn_80051C78();
    fn_8002E328(lbl_80282940, gCameras);

    for (i = 0; i < 7; i++) {
        lbl_8023F864[i][0] = sReticlePos[i][0];
        lbl_8023F864[i][1] = sReticlePos[i][1];
        lbl_8023F864[i][2] = sReticlePos[i][2];
        lbl_8023F848[i] = sReticleDepth[i];
    }

    lbl_8034454C = lbl_80344B70;
    lbl_80344510 = lbl_80344B50;
    lbl_8034450C = lbl_80344B54;
    lbl_80344508 = lbl_80344B58;
    lbl_80344534 = lbl_80344B5C;
    lbl_80344530 = lbl_80344B60;
    lbl_8023F83C[0] = sCameraPos[0];
    lbl_8023F83C[1] = sCameraPos[1];
    lbl_8023F83C[2] = sCameraPos[2];
    lbl_803444DC = lbl_80344B64;
    lbl_803444D8 = lbl_80344B68;
    lbl_803444D4 = lbl_80344B6C;
}

void SaveStage(void) {
    s32 i;

    fn_8002E328(gCameras, lbl_80282940);
    for (i = 0; i < 7; i++) {
        sReticlePos[i][0] = lbl_8023F864[i][0];
        sReticlePos[i][1] = lbl_8023F864[i][1];
        sReticlePos[i][2] = lbl_8023F864[i][2];
        sReticleDepth[i] = lbl_8023F848[i];
    }

    lbl_80344B70 = lbl_8034454C;
    lbl_80344B50 = lbl_80344510;
    lbl_80344B54 = lbl_8034450C;
    lbl_80344B58 = lbl_80344508;
    lbl_80344B5C = lbl_80344534;
    lbl_80344B60 = lbl_80344530;
    sCameraPos[0] = lbl_8023F83C[0];
    sCameraPos[1] = lbl_8023F83C[1];
    sCameraPos[2] = lbl_8023F83C[2];
    lbl_80344B64 = lbl_803444DC;
    lbl_80344B68 = lbl_803444D8;
    lbl_80344B6C = lbl_803444D4;
}
