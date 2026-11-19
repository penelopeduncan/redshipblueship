#ifndef SOULS_H
#define SOULS_H

#include "Rando/Rando.h"

#define SOUL_RI_TO_RANDO_INF(randoItemId) ((randoItemId - RI_SOUL_BOSS_GOHT) + RANDO_INF_OBTAINED_SOUL_OF_BOSS_GOHT)

extern bool HaveEnemySoul(ActorId enemyId);
// 2Ship #1927: true when this actor's enemy soul is shuffled and not yet found; of the ACTOR_EN_INVADEPOH
// types only the aliens have a soul.
extern bool IsEnemySoulMissing(Actor* actor);

#endif // SOULS_H