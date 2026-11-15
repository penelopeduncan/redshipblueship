/**
 * @file oot_has_item_fold_test.cpp
 * ROM-free, display-free lock for backport bundle O6: SoH c5896c7ef (#6795) with
 * c238f0745 (#6798), which fold HasBossSoul and CanOpenOverworldDoor into
 * Logic::HasItem, and SoH 3cfe4ddff (#7183), which keeps a shuffled enemy drop
 * from despawning. CTest row OoTHasItemFold in CMake/SingleExecutable.cmake,
 * dispatch "oot-has-item-fold" in src/common/test_runner.cpp.
 *
 * THE LEGS. A real Rando::Context and a real Rando::Logic over a private zeroed
 * save, with only the flag a leg names set.
 *   S  BOSS SOULS. Shuffle Boss Souls Off: every boss soul, Ganon's included, is
 *      held with no flag (the removed HasBossSoul answered true; HasItem read the
 *      bare flag and answered false). On: a dungeon boss soul is held only with
 *      its flag, Ganon's soul with no flag. On + Ganon: Ganon's soul only with its
 *      flag.
 *   D  OVERWORLD DOORS. Lock Overworld Doors off: a door key is held with no flag.
 *      On: held only with its own flag or the Skeleton Key.
 *   T  THE DESPAWN EXEMPTION. The randomizer's real vanilla-behaviour handler,
 *      registered on the GameInteractor as RandomizerRegisterHooks registers it,
 *      reached through the real GameInteractor_Should. VB_ITEM00_TIMER_TICK stops
 *      the timer of an ITEM00_SOH_DUMMY whose original type is a small key, a
 *      Piece of Heart or a Heart Container, and leaves it running for a dummy that
 *      was a rupee and for an unconverted key (EnItem00's own exemption covers
 *      that one). The control runs without the handler: the dispatch answers the
 *      vanilla result, so the leg's "false" is the handler's.
 *
 * NOT COVERED: that EnItem00_Update dispatches VB_ITEM00_TIMER_TICK at its timer
 * tick (z_en_item00.c). Driving the real update needs a play state this row does
 * not stand up.
 *
 * Every option this row changes is put back, and the world is released.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>

#include "context.h"
#include "game.h"

#include "soh/Enhancements/randomizer/randomizerTypes.h"
#include "soh/Enhancements/randomizer/SeedContext.h"
#include "soh/Enhancements/randomizer/logic.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>

// hook_handlers.cpp: the handler RandomizerRegisterHooks registers on a
// randomizer file load.
void RandomizerOnVanillaBehaviorHandler(GIVanillaBehavior id, bool* should, va_list originalArgs);

namespace {

int sFailures = 0;

void HfFail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] oot-has-item-fold: FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    sFailures++;
}

#define HF_CHECK(cond, ...)      \
    do {                         \
        if (!(cond)) {           \
            HfFail(__VA_ARGS__); \
        }                        \
    } while (0)

/** HasItem(@p rg) over a zeroed save with only @p flag set (none for RAND_INF_MAX). */
bool HeldWith(const std::shared_ptr<Rando::Context>& world, RandomizerGet rg, RandomizerInf flag) {
    auto save = std::make_unique<SaveContext>();
    std::memset(save.get(), 0, sizeof(SaveContext));
    Rando::Logic probe;
    probe.SetContext(world);
    probe.SetSaveContext(save.get());
    if (flag != RAND_INF_MAX) {
        probe.SetRandoInf((uint32_t)flag, true);
    }
    const bool held = probe.HasItem(rg);
    probe.SetContext(nullptr);
    return held;
}

struct Soul {
    RandomizerGet rg;
    RandomizerInf flag;
    const char* name;
};
const Soul kDungeonSouls[] = {
    { RG_GOHMA_SOUL, RAND_INF_GOHMA_SOUL, "Gohma" },
    { RG_KING_DODONGO_SOUL, RAND_INF_KING_DODONGO_SOUL, "King Dodongo" },
    { RG_BARINADE_SOUL, RAND_INF_BARINADE_SOUL, "Barinade" },
    { RG_PHANTOM_GANON_SOUL, RAND_INF_PHANTOM_GANON_SOUL, "Phantom Ganon" },
    { RG_VOLVAGIA_SOUL, RAND_INF_VOLVAGIA_SOUL, "Volvagia" },
    { RG_MORPHA_SOUL, RAND_INF_MORPHA_SOUL, "Morpha" },
    { RG_BONGO_BONGO_SOUL, RAND_INF_BONGO_BONGO_SOUL, "Bongo Bongo" },
    { RG_TWINROVA_SOUL, RAND_INF_TWINROVA_SOUL, "Twinrova" },
};

void CheckSouls(const std::shared_ptr<Rando::Context>& world) {
    Rando::OptionValue& option = world->GetOption(RSK_SHUFFLE_BOSS_SOULS);
    const uint8_t prior = option.Get();

    option.Set(RO_BOSS_SOULS_OFF);
    for (const Soul& s : kDungeonSouls) {
        HF_CHECK(HeldWith(world, s.rg, RAND_INF_MAX), "S: souls Off, %s's soul is not held with no flag", s.name);
    }
    HF_CHECK(HeldWith(world, RG_GANON_SOUL, RAND_INF_MAX), "S: souls Off, Ganon's soul is not held with no flag");

    option.Set(RO_BOSS_SOULS_ON);
    for (const Soul& s : kDungeonSouls) {
        HF_CHECK(!HeldWith(world, s.rg, RAND_INF_MAX), "S: souls On, %s's soul is held with no flag", s.name);
        HF_CHECK(HeldWith(world, s.rg, s.flag), "S: souls On, %s's soul is not held with its flag", s.name);
    }
    HF_CHECK(HeldWith(world, RG_GANON_SOUL, RAND_INF_MAX), "S: souls On, Ganon's soul is not held with no flag");

    option.Set(RO_BOSS_SOULS_ON_PLUS_GANON);
    HF_CHECK(!HeldWith(world, RG_GANON_SOUL, RAND_INF_MAX), "S: souls On + Ganon, Ganon's soul is held with no flag");
    HF_CHECK(HeldWith(world, RG_GANON_SOUL, RAND_INF_GANON_SOUL),
             "S: souls On + Ganon, Ganon's soul is not held with its flag");
    HF_CHECK(HeldWith(world, RG_GOHMA_SOUL, RAND_INF_GOHMA_SOUL),
             "S: souls On + Ganon, Gohma's soul is not held with its flag");

    option.Set(prior);
}

void CheckDoors(const std::shared_ptr<Rando::Context>& world) {
    Rando::OptionValue& option = world->GetOption(RSK_LOCK_OVERWORLD_DOORS);
    const uint8_t prior = option.Get();

    option.Set(0);
    HF_CHECK(HeldWith(world, RG_MASK_SHOP_KEY, RAND_INF_MAX), "D: doors unlocked, the Mask Shop key is not held");
    HF_CHECK(HeldWith(world, RG_FISHING_HOLE_KEY, RAND_INF_MAX), "D: doors unlocked, the Fishing Hole key is not held");

    option.Set(1);
    HF_CHECK(!HeldWith(world, RG_MASK_SHOP_KEY, RAND_INF_MAX),
             "D: doors locked, the Mask Shop key is held with no flag");
    HF_CHECK(HeldWith(world, RG_MASK_SHOP_KEY, RAND_INF_MASK_SHOP_KEY_OBTAINED),
             "D: doors locked, the Mask Shop key is not held with its flag");
    HF_CHECK(HeldWith(world, RG_MASK_SHOP_KEY, RAND_INF_HAS_SKELETON_KEY),
             "D: doors locked, the Mask Shop key is not held with the Skeleton Key");
    HF_CHECK(!HeldWith(world, RG_FISHING_HOLE_KEY, RAND_INF_MASK_SHOP_KEY_OBTAINED),
             "D: doors locked, the Fishing Hole key is held with the Mask Shop key's flag");

    option.Set(prior);
}

/** What GameInteractor_Should answers for the timer tick of a drop. */
bool TimerTicks(int16_t params, int16_t ogParams) {
    EnItem00 item00;
    std::memset(&item00, 0, sizeof(item00));
    item00.actor.params = params;
    item00.ogParams = ogParams;
    item00.unk_15A = 220;
    return GameInteractor_Should(VB_ITEM00_TIMER_TICK, item00.unk_15A > 0, &item00);
}

void CheckTimerTick(void) {
    // A drop's params carry its collectible flag above the low byte.
    const int16_t flagBits = 0x0100;

    HF_CHECK(TimerTicks(ITEM00_SOH_DUMMY, ITEM00_SMALL_KEY | flagBits),
             "T: control, with no handler the dispatch stopped a dummy's timer");

    const HOOK_ID vbHook = GameInteractor::Instance->RegisterGameHook<GameInteractor::OnVanillaBehavior>(
        RandomizerOnVanillaBehaviorHandler);
    HF_CHECK(!TimerTicks(ITEM00_SOH_DUMMY, ITEM00_SMALL_KEY | flagBits), "T: a shuffled small key drop's timer runs");
    HF_CHECK(!TimerTicks(ITEM00_SOH_DUMMY, ITEM00_HEART_PIECE | flagBits), "T: a shuffled Piece of Heart's timer runs");
    HF_CHECK(!TimerTicks(ITEM00_SOH_DUMMY, ITEM00_HEART_CONTAINER), "T: a shuffled Heart Container's timer runs");
    HF_CHECK(TimerTicks(ITEM00_SOH_DUMMY, ITEM00_RUPEE_GREEN), "T: a dummy that was a rupee has its timer stopped");
    HF_CHECK(TimerTicks(ITEM00_SMALL_KEY, ITEM00_SMALL_KEY | flagBits),
             "T: an unconverted small key has its timer stopped by the handler");
    GameInteractor::Instance->UnregisterGameHook<GameInteractor::OnVanillaBehavior>(vbHook);
}

} // namespace

/**
 * @return 0 when every leg holds, 1 otherwise.
 */
extern "C" int OoT_HasItemFold_RunHeadless(void) {
    printf("[TEST] oot-has-item-fold: HasItem answers boss souls and overworld door keys under their options, and a "
           "shuffled enemy drop's timer is stopped (SoH #6795, #6798, #7183)\n");
    sFailures = 0;
    const GameId prevGame = Context_GetCurrentGame();
    // The GameInteractor dispatch is gated off only under GAME_MM.
    Context_SetCurrentGame(GAME_OOT);
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }

    std::shared_ptr<Rando::Context> world = Rando::Context::CreateInstance();
    if (world == nullptr) {
        HfFail("could not create a Rando::Context");
    } else {
        CheckSouls(world);
        CheckDoors(world);
        CheckTimerTick();
        // Context and its Logic hold each other by shared_ptr; cut the back edge
        // first (oot_check_state_load_test.cpp).
        world->GetLogic()->SetContext(nullptr);
        world.reset();
    }

    Context_SetCurrentGame(prevGame);
    if (sFailures != 0) {
        printf("[TEST] FAIL (oot-has-item-fold): %d failure(s)\n", sFailures);
        return 1;
    }
    printf("[TEST] PASS: oot-has-item-fold\n");
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
