#ifndef GAME_TOWER_H
#define GAME_TOWER_H

#include "types.h"

void TowerInit(void);
int WorldOpen(int world);
int towerAwardWorldRunes(void);
int towerGetLevelFlag(u8* record, int level);
void playerGiveGargItem(int player, int item, int count);

int towerAllPlayersMetLevelReq(int level);
int towerGetLevelRecord(int player, int level);
void towerAdvanceLevelRecord(int player, int level);
int towerAllPlayersMetBossReq(int level);
int towerLevelStatus(int player, int level);
int towerBossStatus(int player, int level);
void towerAdvanceBossRecord(int player, int level);

void towerClearRuneNear(int player, int level);
void towerSetRuneNear(int player, int level);
int towerGetRuneNearStat(int player, int level);

void PlayerGiveRune(int player, int rune);
int PlayerHasRune(int player, int rune);
void PlayerGiveShard(int player, int shard);
int PlayerHasShard(int player, int shard);

void TowerNeedGargItemsMsg(int who, int slot);
void TowerNeedCrystalsMsg(int who, int slot);
void TowerCheckMessages(void);
void EnterTower(void);

int GetWorldOrder(int world);
int sumnerSpeechActive(void);
void SumnerHintsActivate(void);
void SumnerEnd(void);
void SumnerInit(void);

#endif /* GAME_TOWER_H */
