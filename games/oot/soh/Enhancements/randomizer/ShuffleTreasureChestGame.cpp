#include <soh/OTRGlobals.h>
#include "soh/Enhancements/randomizer/randomizer.h"

extern "C" {
#include "overlays/actors/ovl_Door_Shutter/z_door_shutter.h"
#include "overlays/actors/ovl_En_Changer/z_en_changer.h"
#include "overlays/actors/ovl_En_Takara_Man/z_en_takara_man.h"
extern PlayState* OoT_gPlayState;
}

static bool GetChestGameRandInf(Actor* chest, RandomizerInf* randInf) {
    if (OoT_gPlayState->sceneNum != SCENE_TREASURE_BOX_SHOP) {
        return false;
    }

    s32 treasureFlag = chest->params & 0x1F;
    if (treasureFlag > 9) {
        return false;
    }

    *randInf = static_cast<RandomizerInf>(
        (treasureFlag & 1 ? RAND_INF_MARKET_TREASURE_CHEST_GAME_KEY_1 : RAND_INF_MARKET_TREASURE_CHEST_GAME_ITEM_1) +
        treasureFlag / 2);
    return true;
}

void DoorShutter_RandomizerInit(void* actorRef) {
    DoorShutter* door = static_cast<DoorShutter*>(actorRef);

    if (OoT_gPlayState->sceneNum != SCENE_TREASURE_BOX_SHOP || door->doorType != SHUTTER_KEY_LOCKED) {
        return;
    }

    door->dyna.actor.params &= ~0x20;

    if (OoT_Flags_GetSwitch(OoT_gPlayState, door->dyna.actor.params & 0x3F)) {
        door->unlockTimer = 0;
    }
}

void ItemEtcetera_RandomizerInit(void* actorRef) {
    Actor* actor = static_cast<Actor*>(actorRef);

    if (OoT_gPlayState->sceneNum != SCENE_TREASURE_BOX_SHOP || ((actor->params >> 8) & 0x1F) > 9) {
        return;
    }

    // these only ever show the vanilla contents through the lens, so drop them
    OoT_Actor_Kill(actor);
}

extern "C" void EnChanger_RandomizerWait(EnChanger* thisx, PlayState* play) {
}

void EnChanger_RandomizerInit(void* actorRef) {
    if (OoT_gPlayState->sceneNum != SCENE_TREASURE_BOX_SHOP || OoT_gPlayState->roomCtx.curRoom.num >= 6) {
        return;
    }

    EnChanger* changer = static_cast<EnChanger*>(actorRef);

    // stop opening one chest from sealing the other and playing the reveal
    changer->actionFunc = EnChanger_RandomizerWait;
}

void EnTakaraMan_RandomizerInit(void* actorRef) {
    if (Flags_GetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER)) {
        EnTakaraMan* takaraMan = static_cast<EnTakaraMan*>(actorRef);
        // acting as if the game was already paid for stops him offering it again
        takaraMan->unk_214 = 1;
    }
}

void EnBox_RandomizerInit(void* actorRef) {
    Actor* chest = static_cast<Actor*>(actorRef);
    RandomizerInf randInf;

    if (GetChestGameRandInf(chest, &randInf)) {
        // the chests are see-through until the lens reveals them, but there is nothing left to reveal
        chest->flags &= ~ACTOR_FLAG_REACT_TO_LENS;
    }
}

void RegisterShuffleTreasureChestGame() {
    bool shouldRegister = IS_RANDO && RAND_GET_OPTION(RSK_SHUFFLE_CHEST_MINIGAME);

    // prevent wiping chest flags and key count on every visit
    COND_VB_SHOULD(VB_TAKARA_MAN_RESET_CHESTS_AND_KEYS, shouldRegister, { *should = false; });

    COND_VB_SHOULD(VB_TAKARA_MAN_OFFER_GET_ITEM, shouldRegister, {
        if (!Flags_GetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER)) {
            Actor* takaraMan = va_arg(args, Actor*);
            GetItemEntry getItemEntry = Rando::Context::GetInstance()->GetFinalGIEntry(
                RC_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER, true, GI_DOOR_KEY);
            if (GiveItemEntryFromActor(takaraMan, OoT_gPlayState, getItemEntry, 2000.0f, 1000.0f)) {
                Flags_SetRandomizerInf(RAND_INF_MARKET_TREASURE_CHEST_GAME_SHOPKEEPER);
            }
        }
        *should = false;
    });

    COND_VB_SHOULD(VB_EN_CHANGER_SWAP_CHESTS, shouldRegister, { *should = false; });

    COND_VB_SHOULD(VB_CHEST_SET_TREASURE_FLAG, shouldRegister, {
        Actor* chest = va_arg(args, Actor*);
        RandomizerInf randInf;
        if (GetChestGameRandInf(chest, &randInf)) {
            Flags_SetRandomizerInf(randInf);
            *should = false;
        }
    });

    COND_VB_SHOULD(VB_CHEST_CONSIDER_CHEST_OPEN, shouldRegister, {
        Actor* chest = va_arg(args, Actor*);
        RandomizerInf randInf;
        if (GetChestGameRandInf(chest, &randInf)) {
            *should = Flags_GetRandomizerInf(randInf);
        }
    });

    COND_ID_HOOK(OnActorInit, ACTOR_DOOR_SHUTTER, shouldRegister, DoorShutter_RandomizerInit);
    COND_ID_HOOK(OnActorInit, ACTOR_ITEM_ETCETERA, shouldRegister, ItemEtcetera_RandomizerInit);
    COND_ID_HOOK(OnActorInit, ACTOR_EN_CHANGER, shouldRegister, EnChanger_RandomizerInit);
    COND_ID_HOOK(OnActorInit, ACTOR_EN_BOX, shouldRegister, EnBox_RandomizerInit);
    COND_ID_HOOK(OnActorInit, ACTOR_EN_TAKARA_MAN, shouldRegister, EnTakaraMan_RandomizerInit);
}

static RegisterShipInitFunc registerShuffleTreasureChestGame(RegisterShuffleTreasureChestGame, { "IS_RANDO" });
