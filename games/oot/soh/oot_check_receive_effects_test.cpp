/**
 * @file oot_check_receive_effects_test.cpp
 * ROM-free, display-free behaviour legs of the row OoTCheckReceiveEffects
 * (#1602, #1600 step 2); the source legs are in
 * src/common/tests/test_oot_check_receive_effects.c, which calls this.
 *
 * WHAT WAS BROKEN. RandomizerOnItemReceiveHandler (hook_handlers.cpp) ran the
 * Spirit Temple's story step when the check RC_SPIRIT_TEMPLE_SILVER_GAUNTLETS_CHEST
 * gave its item, but only a NATIVE item reaches that handler. When the chest
 * hosts a Majora's Mask item, OoT's get-item cutscene shows it and its give
 * point gives nothing (ForeignModelHostOoT.cpp): it releases the RC queue's slot
 * through Randomizer_ReleaseQueuedShowOnly instead, so Nabooru was never marked
 * captured. That chest is always shown in a cutscene (its native give never skips
 * its animation either), so the release IS its receive.
 *
 * THE LEGS (the slot is occupied with OoT_Rando_TestQueueGive, as the drain's
 * show-only queueing occupies it):
 *   1. Skip Story Cutscenes on: the release of a slot held by the Silver
 *      Gauntlets chest sets EVENTCHKINF_NABOORU_CAPTURED_BY_TWINROVA and arms no
 *      Colossus transition. Red before #1602.
 *   2. Once: the slot is free, so a second release (the flag cleared first) sets
 *      nothing.
 *   3. Skip Story Cutscenes off: the release arms the Colossus transition (an
 *      OnPlayerUpdate hook that waits for the cutscene to end) and sets no flag.
 *      Red before #1602. The hook is disarmed before the row ends.
 *   4. Control: the release of a slot held by any other check (a business scrub,
 *      a chest) does neither.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstring>

// At file scope, not inside an extern "C" block (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>
#include "variables.h"
#include "macros.h"
#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/cvar_prefixes.h"

#include <libultraship/bridge/consolevariablebridge.h>

#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <memory>

extern "C" {
// Enhancements/randomizer/hook_handlers.cpp: occupy the give slot, and the
// Colossus transition's arm.
void OoT_Rando_TestQueueGive(uint16_t rc, const GetItemEntry* entry);
int OoT_Rando_TestColossusWarpArmed(int disarm);
}

// Enhancements/randomizer/hook_handlers.cpp: the show-only give's slot release
// (its give point, ForeignModelHostOoT.cpp's take).
void Randomizer_ReleaseQueuedShowOnly(uint16_t rc);

namespace {

int sFailures = 0;

#define OCR_CHECK(cond, ...)                                            \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("[TEST] FAIL (oot-check-receive-effects): ");   \
            std::printf(__VA_ARGS__);                                   \
            std::printf("  [%s] (%s:%d)\n", #cond, __FILE__, __LINE__); \
            sFailures++;                                                \
        }                                                               \
    } while (0)

/** A hook type's registry set aside for a scope: OoT_Flags_SetEventChkInf
 *  dispatches OnFlagSet, and the bring-up can leave hooks registered there that
 *  read a live play state this row does not have. */
template <typename H> struct HooksAside {
    decltype(GameInteractor::RegisteredGameHooks<H>::functions) functions;
    decltype(GameInteractor::RegisteredGameHooks<H>::functionsForFilter) filters;
    HooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
    ~HooksAside() {
        functions.swap(GameInteractor::RegisteredGameHooks<H>::functions);
        filters.swap(GameInteractor::RegisteredGameHooks<H>::functionsForFilter);
    }
};

const char* const kSkipStoryCVar = CVAR_ENHANCEMENT("TimeSavers.SkipCutscene.Story");
constexpr uint16_t kGauntletsChest = RC_SPIRIT_TEMPLE_SILVER_GAUNTLETS_CHEST;

bool NabooruCaptured() {
    return GET_EVENTCHKINF(EVENTCHKINF_NABOORU_CAPTURED_BY_TWINROVA) != 0;
}

/** Occupy the slot for `rc` with an arbitrary entry, clear the flag, and release
 *  it as the give point does. */
void QueueAndRelease(uint16_t rc) {
    GetItemEntry entry;
    std::memset(&entry, 0, sizeof(entry));
    entry.modIndex = MOD_RANDOMIZER;
    entry.itemId = RG_BLUE_RUPEE;
    entry.getItemId = RG_BLUE_RUPEE;
    OoT_Rando_TestQueueGive(rc, &entry);
    CLEAR_EVENTCHKINF(EVENTCHKINF_NABOORU_CAPTURED_BY_TWINROVA);
    Randomizer_ReleaseQueuedShowOnly(rc);
}

} // namespace

extern "C" int OoT_CheckReceiveEffects_RunHeadless(void) {
    sFailures = 0;
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    auto saveBefore = std::make_unique<SaveContext>();
    std::memcpy(saveBefore.get(), &gSaveContext, sizeof(SaveContext));
    // Set aside and put back; never stored (a stored key would reach the config).
    const int32_t kUnset = -12345;
    const int32_t storyBefore = CVarGetInteger(kSkipStoryCVar, kUnset);
    OoT_Rando_TestColossusWarpArmed(1);
    {
        HooksAside<GameInteractor::OnFlagSet> flagSet;
        HooksAside<GameInteractor::OnPlayerUpdate> playerUpdate;

        // Leg 1: Skip Story Cutscenes on.
        CVarSetInteger(kSkipStoryCVar, 1);
        QueueAndRelease(kGauntletsChest);
        std::printf("[TEST] leg 1: story skipped, the Silver Gauntlets chest's show-only release: Nabooru captured %d, "
                    "Colossus transition armed %d\n",
                    NabooruCaptured() ? 1 : 0, OoT_Rando_TestColossusWarpArmed(0));
        OCR_CHECK(NabooruCaptured(), "leg 1: an MM item given by the Silver Gauntlets chest must mark Nabooru captured "
                                     "by Twinrova, as a native item does\n");
        OCR_CHECK(OoT_Rando_TestColossusWarpArmed(0) == 0,
                  "leg 1: with the story skipped no Colossus transition may be armed\n");

        // Leg 2: once. The slot is free now.
        CLEAR_EVENTCHKINF(EVENTCHKINF_NABOORU_CAPTURED_BY_TWINROVA);
        Randomizer_ReleaseQueuedShowOnly(kGauntletsChest);
        OCR_CHECK(!NabooruCaptured(), "leg 2: a second release of a freed slot ran the check's receive again\n");

        // Leg 3: Skip Story Cutscenes off.
        CVarSetInteger(kSkipStoryCVar, 0);
        QueueAndRelease(kGauntletsChest);
        const int armed = OoT_Rando_TestColossusWarpArmed(1);
        std::printf("[TEST] leg 3: story shown, the same release: Colossus transition armed %d, Nabooru captured %d\n",
                    armed, NabooruCaptured() ? 1 : 0);
        OCR_CHECK(armed == 1, "leg 3: with the story shown the release must arm the Colossus transition\n");
        OCR_CHECK(!NabooruCaptured(), "leg 3: with the story shown the flag is the cutscene's to set, not the "
                                      "release's\n");

        // Leg 4: control, other checks.
        const uint16_t others[] = { RC_GANONS_CASTLE_DEKU_SCRUB_RIGHT, RC_SPIRIT_TEMPLE_COMPASS_CHEST };
        for (const uint16_t rc : others) {
            for (const int32_t story : { 0, 1 }) {
                CVarSetInteger(kSkipStoryCVar, story);
                QueueAndRelease(rc);
                const int otherArmed = OoT_Rando_TestColossusWarpArmed(1);
                OCR_CHECK(!NabooruCaptured() && otherArmed == 0,
                          "leg 4: the release of check %u (story skipped %d) ran the Silver Gauntlets chest's "
                          "receive\n",
                          (unsigned)rc, (int)story);
            }
        }
        std::printf("[TEST] leg 4: other checks' releases ran nothing\n");

        OoT_Rando_TestQueueGive(0, nullptr);
        OoT_Rando_TestColossusWarpArmed(1);
    }
    if (storyBefore == kUnset) {
        CVarClear(kSkipStoryCVar);
    } else {
        CVarSetInteger(kSkipStoryCVar, storyBefore);
    }
    std::memcpy(&gSaveContext, saveBefore.get(), sizeof(SaveContext));
    return sFailures;
}

#endif // RSBS_SINGLE_EXECUTABLE
