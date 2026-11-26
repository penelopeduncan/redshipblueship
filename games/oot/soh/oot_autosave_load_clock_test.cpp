/**
 * @file oot_autosave_load_clock_test.cpp
 * ROM-free, display-free lock for #1735 (SoH d46bfd971, upstream PR 6298,
 * partial port: the interval autosave clock resets on OnLoadGame). CTest label
 * "redship", row OoTAutosaveLoadClock in CMake/SingleExecutable.cmake, dispatch
 * "oot-autosave-load-clock" in src/common/test_runner.cpp. The fix is
 * games/oot/soh/Enhancements/QoL/Autosave.cpp.
 *
 * WHY. Before d46bfd971, Autosave.cpp initialised lastSaveTimestamp once at
 * program start and registered no OnLoadGame hook, so loading a file more than
 * three minutes after launch could trigger an interval autosave as soon as the
 * 60-frame guard cleared. Upstream added a COND_HOOK(GameInteractor::OnLoadGame,
 * CVAR_AUTOSAVE_VALUE, ...) handler in RegisterAutosave to reset the clock on
 * file load.
 *
 * WHAT IT DRIVES. Autosave.cpp's own ShipInit registrar under
 * CVAR_ENHANCEMENT("Autosave") (found by registration site, so a linker-elided
 * registrar fails the row), then invokes ONLY the OnLoadGame handler that
 * Autosave registered (isolated by HookIds / NewIds so none of the other twenty
 * OnLoadGame registrants run inside --test all).
 *
 * THE LEGS:
 *   (0) exactly one ShipInit entry from QoL/Autosave.cpp under the key;
 *   (1) option off: clearing the key and running the entry registers no new
 *       OnLoadGame hook;
 *   (2) option on: setting the key to 1 and running the entry registers exactly
 *       one new OnLoadGame hook; setting the clock to 1 via the bridge and
 *       invoking that handler stamps lastSaveTimestamp within [t0, t1];
 *   (3) teardown: restores the key (clearing it if it was unset), re-runs the
 *       entry, restores the clock through the bridge, and restores the active
 *       game.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>

#include <libultraship/bridge.h>
#include "context.h"
#include "soh/OTRGlobals.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

#include <algorithm>
#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

extern "C" {
uint64_t OoT_Autosave_TestGetLastSaveTimestamp(void);
void OoT_Autosave_TestSetLastSaveTimestamp(uint64_t timestamp);
}

namespace {

#define ALC_ASSERT(cond, ...)                         \
    do {                                              \
        if (!(cond)) {                                \
            printf("[TEST] FAIL: ");                  \
            printf(__VA_ARGS__);                      \
            printf(" (%s:%d)\n", __FILE__, __LINE__); \
            return 1;                                 \
        }                                             \
    } while (0)

const char* const kAutosaveKey = CVAR_ENHANCEMENT("Autosave");

bool IsAutosaveSite(const ShipInitEntry& entry) {
    std::string file = entry.registeredAt.file_name();
    std::replace(file.begin(), file.end(), '\\', '/');
    const std::string want = "QoL/Autosave.cpp";
    if (file.size() < want.size() || file.compare(file.size() - want.size(), want.size(), want) != 0) {
        return false;
    }
    return file.size() == want.size() || file[file.size() - want.size() - 1] == '/';
}

int CountAutosaveRegistrars(const char* key) {
    int found = 0;
    for (const auto& entry : ShipInit::GetAll()[key]) {
        if (IsAutosaveSite(entry)) {
            found++;
        }
    }
    return found;
}

void RunAutosaveRegistrar(const char* key) {
    for (const auto& entry : ShipInit::GetAll()[key]) {
        if (IsAutosaveSite(entry)) {
            entry.initFunc();
        }
    }
}

template <typename Hook> std::vector<HOOK_ID> HookIds() {
    std::vector<HOOK_ID> ids;
    for (auto& kv : GameInteractor::RegisteredGameHooks<Hook>::functions) {
        ids.push_back(kv.first);
    }
    return ids;
}

template <typename Hook> std::vector<HOOK_ID> NewIds(const std::vector<HOOK_ID>& before) {
    std::vector<HOOK_ID> out;
    for (HOOK_ID id : HookIds<Hook>()) {
        if (std::find(before.begin(), before.end(), id) == before.end()) {
            out.push_back(id);
        }
    }
    return out;
}

int RunLegs(void) {
    // (0) Exactly one ShipInit entry from QoL/Autosave.cpp under the key.
    const int regCount = CountAutosaveRegistrars(kAutosaveKey);
    printf("[TEST] (0) QoL/Autosave.cpp ShipInit entries under %s: %d\n", kAutosaveKey, regCount);
    ALC_ASSERT(regCount == 1, "(0): expected 1 QoL/Autosave.cpp ShipInit entry under %s, got %d", kAutosaveKey,
               regCount);

    // (1) Option off: clearing the key registers no new OnLoadGame hook.
    CVarClear(kAutosaveKey);
    const auto beforeOff = HookIds<GameInteractor::OnLoadGame>();
    RunAutosaveRegistrar(kAutosaveKey);
    const auto addedOff = NewIds<GameInteractor::OnLoadGame>(beforeOff);
    printf("[TEST] (1) option off: new OnLoadGame hook ids = %zu\n", addedOff.size());
    ALC_ASSERT(addedOff.empty(), "(1): Autosave registered %zu OnLoadGame hook(s) while %s was off", addedOff.size(),
               kAutosaveKey);

    // (2) Option on: setting the key to 1 registers exactly one new OnLoadGame
    // hook, and invoking that handler updates lastSaveTimestamp into [t0, t1].
    CVarSetInteger(kAutosaveKey, 1);
    const auto beforeOn = HookIds<GameInteractor::OnLoadGame>();
    RunAutosaveRegistrar(kAutosaveKey);
    const auto addedOn = NewIds<GameInteractor::OnLoadGame>(beforeOn);
    printf("[TEST] (2) option on: new OnLoadGame hook ids = %zu\n", addedOn.size());
    ALC_ASSERT(addedOn.size() == 1, "(2): expected 1 new OnLoadGame hook id with %s on, got %zu", kAutosaveKey,
               addedOn.size());

    const HOOK_ID hookId = addedOn[0];
    OoT_Autosave_TestSetLastSaveTimestamp(1);
    const uint64_t t0 = GetUnixTimestamp();
    GameInteractor::RegisteredGameHooks<GameInteractor::OnLoadGame>::functions[hookId](0);
    const uint64_t t1 = GetUnixTimestamp();
    const uint64_t clock = OoT_Autosave_TestGetLastSaveTimestamp();
    printf("[TEST] (2) t0=%" PRIu64 " clock=%" PRIu64 " t1=%" PRIu64 "\n", t0, clock, t1);
    ALC_ASSERT(t0 <= clock && clock <= t1, "(2): clock %" PRIu64 " not in [%" PRIu64 ", %" PRIu64 "]", clock, t0, t1);

    return 0;
}

} // namespace

extern "C" int OoT_AutosaveLoadClock_RunHeadless(void) {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    const GameId prevGame = Context_GetCurrentGame();
    Context_SetCurrentGame(GAME_OOT);
    const uint64_t prevClock = OoT_Autosave_TestGetLastSaveTimestamp();
    const bool hadKey = CVarGet(kAutosaveKey) != nullptr;
    const int savedValue = hadKey ? CVarGetInteger(kAutosaveKey, 0) : 0;

    const int rc = RunLegs();

    // (3) Teardown: leave global state as it was, on pass or fail.
    if (hadKey) {
        CVarSetInteger(kAutosaveKey, savedValue);
    } else {
        CVarClear(kAutosaveKey);
    }
    RunAutosaveRegistrar(kAutosaveKey);
    OoT_Autosave_TestSetLastSaveTimestamp(prevClock);
    Context_SetCurrentGame(prevGame);

    if (rc == 0) {
        printf("[TEST] oot-autosave-load-clock: PASS\n");
    }
    return rc;
}

#endif // RSBS_SINGLE_EXECUTABLE
