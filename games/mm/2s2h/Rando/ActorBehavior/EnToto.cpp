#include "ActorBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/Rando/Logic/Logic.h"

extern "C" {
#include "variables.h"
#include "src/overlays/actors/ovl_En_Toto/z_en_toto.h"
extern void func_80BA36C0(EnToto* enToto, PlayState* play, s32 index);
}

void Rando::ActorBehavior::InitEnTotoBehavior() {
    COND_VB_SHOULD(VB_GIVE_ITEM_FROM_OFFER, IS_RANDO, {
        GetItemId* item = va_arg(args, GetItemId*);
        Actor* refActor = va_arg(args, Actor*);
        Player* player = GET_PLAYER(MM_gPlayState);

        if (refActor->id != ACTOR_EN_TOTO) {
            return;
        }

        RANDO_SAVE_CHECKS[RC_MILK_BAR_CIRCUS_LEADER_MASK].eligible = true;

        *should = false;
        refActor->parent = &player->actor;
        player->talkActor = refActor;
        player->talkActorDistance = refActor->xzDistToPlayer;
        player->exchangeItemAction = PLAYER_IA_MINUS1;
        func_80BA36C0((EnToto*)refActor, MM_gPlayState, 0); // Advance Toto to "Wanna play again?" state
    });

    /*
     * "Please take my mask"
     * Notebook events usually pop by immediately following a textbox. Since the Get Item animation is at the tail end
     * of a cutscene, and the player may have GI animations skipped, there is no subsequent textbox to trigger notebook
     * events. So, we queue the relevant notebook events manually with the final textbox of the cutscene.
     */
    COND_ID_HOOK(OnOpenText, 0x2B3B, IS_RANDO, [](u16* textId, bool* loadFromMessageTable) {
        Message_BombersNotebookQueueEvent(MM_gPlayState, BOMBERS_NOTEBOOK_EVENT_RECEIVED_CIRCUS_LEADERS_MASK);
        Message_BombersNotebookQueueEvent(MM_gPlayState, BOMBERS_NOTEBOOK_EVENT_MET_TOTO);
        Message_BombersNotebookQueueEvent(MM_gPlayState, BOMBERS_NOTEBOOK_EVENT_MET_GORMAN);
    });

    // Upstream 99bb6318c registers this on IS_RANDO alone: the body decides on
    // live state, since canPlaySong now also requires the Ocarina. RedShipBlueShip
    // #1369 (the pair's one button shuffle) is kept by that body rather than by a
    // registration gate: canPlaySong reads the RANDO_INF_OBTAINED_OCARINA_BUTTON_*
    // bits, which a world that shuffles no buttons (by its own option or the
    // pair's, Foreign::ShuffleOcarinaButtonsForWorld) holds from file creation
    // (StartingItems.cpp RsbsWorldShuffles), so the check never fires there.
    COND_VB_SHOULD(VB_TOTO_START_SOUND_CHECK, IS_RANDO, {
        EnToto* totoActor = va_arg(args, EnToto*);
        if (totoActor->text->textId == 0x2B24) {
            if (!(Rando::Logic::canPlaySong(OCARINA_SONG_WIND_FISH_HUMAN) &&
                  Rando::Logic::canPlaySong(OCARINA_SONG_WIND_FISH_DEKU) &&
                  Rando::Logic::canPlaySong(OCARINA_SONG_WIND_FISH_GORON) &&
                  Rando::Logic::canPlaySong(OCARINA_SONG_WIND_FISH_ZORA))) {
                MM_Message_ContinueTextbox(MM_gPlayState, 0x2B25);
                func_80BA36C0(totoActor, MM_gPlayState, 0);
                MM_Flags_UnsetSwitch(MM_gPlayState, ENTOTO_GET_SWITCH_FLAG_1(&totoActor->actor));
                *should = false;
            }
        }
    });
}
