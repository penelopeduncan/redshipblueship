#ifndef SOULS_H
#define SOULS_H

#include "Rando/Rando.h"

#define SOUL_RI_TO_RANDO_INF(randoItemId) ((randoItemId - RI_SOUL_BOSS_GOHT) + RANDO_INF_OBTAINED_SOUL_OF_BOSS_GOHT)

extern bool HaveEnemySoul(ActorId enemyId);
// 2Ship #1927: true when this actor has an enemy soul that has not been found (it does not read the soul-shuffle
// option: callers register only when souls are shuffled); of the ACTOR_EN_INVADEPOH types only the aliens have a soul.
extern bool IsEnemySoulMissing(Actor* actor);

#endif // SOULS_H