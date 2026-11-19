/**
 * @file oot_ocarina_note_mapping_cleanup_test.cpp
 * ROM-free, display-free lock for #1741. CTest label "redship", row
 * OoTOcarinaNoteMappingCleanup in CMake/SingleExecutable.cmake, dispatch
 * "oot-ocarina-note-mapping-cleanup" in src/common/test_runner.cpp. The fix is
 * games/oot/soh/Enhancements/controls/OcarinaNoteMappingCleanup.cpp, called
 * once per process from OoT_InitSharedContextSubsystems (OTRGlobals.cpp).
 *
 * WHY. Until #1740 the Cosmetics Editor copied port 0's D-pad mappings onto the
 * four custom ocarina-note buttons; a saved note list kept naming the D-pad
 * mapping's id, and removing that copy erased the D-pad's own record.
 *
 * THE LEGS.
 *   (p) the pure helper on its own: a prefix-sharing bitmask is foreign, an id
 *       with no "-" after its B field is kept, empty tokens are left out.
 *   (h) a second bring-up on the live context leaves the stray state alone.
 *   (a)-(f) one clean-up run over a stray state built in the CVar store.
 *   (g) a second run changes nothing and reports nothing.
 * Every key the row touches is recorded first and put back afterwards (set when
 * it was present, cleared when it was not). Nothing is saved to disk: the row
 * calls the clean-up directly, never the bring-up hook, and never
 * ReloadAllMappingsFromConfig (ButtonMappingFactory can Save()).
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include <libultraship/libultraship.h>
#include <ship/Context.h>
#include <ship/config/ConsoleVariable.h>

#include "soh/Enhancements/controls/OcarinaNoteMappingCleanup.h"

#include <cstdio>
#include <string>
#include <vector>

extern "C" int OoT_InitSharedContextSubsystems(void);

namespace {

// Literal keys, so a production key that drifts from libultraship's
// ("gSettings.Controllers.Port<n>.Buttons.<decimal bitmask>ButtonMappingIds")
// fails the row.
const char* const kOnmcDpadList = "gSettings.Controllers.Port1.Buttons.1024ButtonMappingIds";
const char* const kOnmcF4List = "gSettings.Controllers.Port1.Buttons.131072ButtonMappingIds";
const char* const kOnmcA4List = "gSettings.Controllers.Port1.Buttons.262144ButtonMappingIds";
const char* const kOnmcPort4F4List = "gSettings.Controllers.Port4.Buttons.131072ButtonMappingIds";
const char* const kOnmcRecordBitmask = "gSettings.Controllers.ButtonMappings.P0-B1024-KB4093.Bitmask";
const char* const kOnmcRecordClass = "gSettings.Controllers.ButtonMappings.P0-B1024-KB4093.ButtonMappingClass";
const char* const kOnmcRecordScancode = "gSettings.Controllers.ButtonMappings.P0-B1024-KB4093.KeyboardScancode";

const char* const kOnmcDpadValue = "P0-B1024-KB4093,P0-B131072-KB4095,";
const char* const kOnmcF4Value = "P0-B1024-KB4093,P0-B131072-KB4094,";
const char* const kOnmcA4Value = "P0-B256-SDLB14,not-an-id,";
const char* const kOnmcPort4F4Value = "P3-B1024-KB4093,";

struct OnmcSavedKey {
    std::string name;
    bool present;
    bool isString;
    std::string stringValue;
    int32_t intValue;
};

std::vector<OnmcSavedKey> OnmcAllTouchedKeys() {
    std::vector<OnmcSavedKey> keys;
    // Every list the clean-up can rewrite (four notes on every port), plus the
    // D-pad list and the D-pad mapping's record.
    static const char* const kNotes[] = { "131072", "262144", "524288", "1048576" };
    for (int port = 1; port <= MAXCONTROLLERS; port++) {
        for (const char* note : kNotes) {
            keys.push_back(
                { "gSettings.Controllers.Port" + std::to_string(port) + ".Buttons." + note + "ButtonMappingIds", false,
                  true, "", 0 });
        }
    }
    keys.push_back({ kOnmcDpadList, false, true, "", 0 });
    keys.push_back({ kOnmcRecordClass, false, true, "", 0 });
    keys.push_back({ kOnmcRecordBitmask, false, false, "", 0 });
    keys.push_back({ kOnmcRecordScancode, false, false, "", 0 });
    return keys;
}

std::string OnmcGet(const char* key) {
    return CVarGetString(key, "");
}

int OnmcCheck(bool ok, const char* what) {
    if (!ok) {
        printf("[TEST] FAIL: %s\n", what);
        return 1;
    }
    printf("[TEST] ok: %s\n", what);
    return 0;
}

int OnmcPureLeg() {
    int fails = 0;
    size_t dropped = 99;
    const std::string kept =
        SohInput_StripForeignBitmaskMappingIds(",P0-B1310720-KB1,,P0-B1024,P0-B131072-SDLA1-ADP,", 131072, &dropped);
    fails += OnmcCheck(kept == "P0-B1024,P0-B131072-SDLA1-ADP,",
                       "(p) the pure helper drops a prefix-sharing foreign bitmask, keeps a foreign id whose B field "
                       "has no \"-\" after it (not an id shape), keeps the note's own id, and leaves out empty tokens");
    fails += OnmcCheck(dropped == 1, "(p) the pure helper counts one dropped id");
    return fails;
}

int OnmcRunLegs(Ship::ConsoleVariable& cvars) {
    int fails = OnmcPureLeg();

    // The stray state: the D-pad's own record and list (plus a foreign id on
    // the D-pad list, which must stay because the D-pad is not a note), a note
    // list holding the D-pad id and one real id, an A4 list holding a foreign
    // id and an unparseable one, and a stray on port 4.
    for (const OnmcSavedKey& key : OnmcAllTouchedKeys()) {
        cvars.ClearVariable(key.name.c_str());
    }
    cvars.SetInteger(kOnmcRecordBitmask, 1024);
    cvars.SetString(kOnmcRecordClass, "KeyboardKeyToButtonMapping");
    cvars.SetInteger(kOnmcRecordScancode, 4093);
    cvars.SetString(kOnmcDpadList, kOnmcDpadValue);
    cvars.SetString(kOnmcF4List, kOnmcF4Value);
    cvars.SetString(kOnmcA4List, kOnmcA4Value);
    cvars.SetString(kOnmcPort4F4List, kOnmcPort4F4Value);

    // (h) The row's own bring-up already ran the once-per-process clean-up, so
    // a second bring-up on the live context must leave the stray state alone.
    fails += OnmcCheck(OoT_InitSharedContextSubsystems() == 0, "(h) a second bring-up succeeds");
    fails += OnmcCheck(OnmcGet(kOnmcF4List) == kOnmcF4Value,
                       "(h) a second bring-up on a live context does not run the clean-up again");

    const bool firstChanged = SohInput_DropForeignOcarinaNoteMappingIds();
    fails += OnmcCheck(firstChanged, "the clean-up reports that it rewrote a list");
    fails += OnmcCheck(OnmcGet(kOnmcF4List).find("P0-B1024-KB4093") == std::string::npos,
                       "(a) the D-pad id is gone from the F4 list");
    fails += OnmcCheck(OnmcGet(kOnmcF4List) == "P0-B131072-KB4094,", "(b) the F4 list keeps its own real id");
    fails += OnmcCheck(OnmcGet(kOnmcDpadList) == kOnmcDpadValue, "(c) the D-pad list is untouched");
    fails += OnmcCheck(CVarGetInteger(kOnmcRecordBitmask, 0) == 1024 &&
                           OnmcGet(kOnmcRecordClass) == "KeyboardKeyToButtonMapping" &&
                           CVarGetInteger(kOnmcRecordScancode, 0) == 4093,
                       "(d) the D-pad mapping's settings record is untouched");
    fails += OnmcCheck(OnmcGet(kOnmcA4List) == "not-an-id,", "(e) the A4 list keeps an id it cannot parse");
    fails += OnmcCheck(!CVarExists(kOnmcPort4F4List), "(f) port 4's emptied F4 list is cleared (every port scanned)");

    const std::string f4After = OnmcGet(kOnmcF4List);
    const std::string a4After = OnmcGet(kOnmcA4List);
    const bool secondChanged = SohInput_DropForeignOcarinaNoteMappingIds();
    fails += OnmcCheck(!secondChanged && OnmcGet(kOnmcF4List) == f4After && OnmcGet(kOnmcA4List) == a4After &&
                           OnmcGet(kOnmcDpadList) == kOnmcDpadValue && !CVarExists(kOnmcPort4F4List),
                       "(g) a second run reports nothing and changes nothing");
    return fails;
}

} // namespace

extern "C" int OoT_OcarinaNoteMappingCleanup_RunHeadless(void) {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetConsoleVariables() == nullptr || ctx->GetControlDeck() == nullptr) {
        printf("[TEST] FAIL: no console variables or control deck\n");
        return 1;
    }
    Ship::ConsoleVariable& cvars = *ctx->GetConsoleVariables();

    std::vector<OnmcSavedKey> saved = OnmcAllTouchedKeys();
    for (OnmcSavedKey& key : saved) {
        key.present = cvars.Get(key.name.c_str()) != nullptr;
        if (key.isString) {
            key.stringValue = cvars.GetString(key.name.c_str(), "");
        } else {
            key.intValue = cvars.GetInteger(key.name.c_str(), 0);
        }
    }

    const int fails = OnmcRunLegs(cvars);

    // Put every touched key back, on pass or fail: set when it was present,
    // cleared (never stored) when it was not.
    for (const OnmcSavedKey& key : saved) {
        if (!key.present) {
            cvars.ClearVariable(key.name.c_str());
        } else if (key.isString) {
            cvars.SetString(key.name.c_str(), key.stringValue.c_str());
        } else {
            cvars.SetInteger(key.name.c_str(), key.intValue);
        }
    }

    printf("[TEST] %s: ocarina note mapping clean-up (#1741), %d failure(s)\n", fails == 0 ? "PASS" : "FAIL", fails);
    return fails == 0 ? 0 : 1;
}

#endif // RSBS_SINGLE_EXECUTABLE
