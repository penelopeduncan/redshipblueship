/**
 * @file mm_hint_options_1754_test.cpp
 * ROM-free, display-free lock for the Song of Soaring (2Ship e3bfae008 + 74e519def)
 * and Bank Reward (2Ship 2056ebc7a) hint options ported for #1754.
 * CTest label "redship", row MMHintOptions1754 in CMake/SingleExecutable.cmake
 * (beside MMUpstreamFixes1754), dispatch "mm-hint-options-1754" in
 * src/common/test_runner.cpp.
 *
 * LEGS:
 *   1. Descriptors: both rows are LIVE checkboxes under Hints, default 0, labelled
 *      "Song of Soaring Hint" and "Bank Reward Hint". The labels and tooltips follow
 *      the hosted Hints page's "<X> Hint" pattern, not upstream's ("Song of Soaring",
 *      "Bank Reward"), which 2Ship's own Rando/Menu.cpp keeps.
 *   2. Rando::HintSources::Listed over an authored rando image: false at 0, true at 1.
 *   3. Soaring resolver: RI_SONG_SOARING on a chosen MM check resolves to FIXED with
 *      that check and its Combo_Area_OfCheck, text starting "A note is carved into the stone..."
 *      and naming that area (Combo_Area_Name, the name ReplaceItemLocation inserts).
 *   4. Bank resolver: bank check shuffled holding RI_HOOKSHOT; text contains
 *      "Deposit %r5000 Rupees" with LowerBankRewardThresholds at 0 and
 *      "Deposit %r1000 Rupees" at 1,
 *      targeting that check and item; TestLegacyFixedText equals TestTextboxText for both.
 *   5. Hooks through MM_GameHooks_ExecuteOnOpenText over a calloc'd PlayState with a
 *      three-row messageTableNES (0x1C14, 0x0C02, 0xFFFF):
 *      (a) 0x0C02 with Soaring option off: check eligible, load stays true.
 *      (b) 0x0C02 with Soaring option on: load false, source marked read.
 *      (c) 0x0C02 with Soaring option on and MM_gPlayState NULL: still eligible, load true.
 *      (d) 0x1C14 with bank check shuffled and unobtained: load false, source marked read.
 *      (e) 0x1C14 with bank check obtained: load true, bit stays clear.
 *      (f) Bank option set to 0 and BankSignHint() re-run: 0x1C14 leaves load true.
 *
 * ENTRY. No paired foreign placement may be active (Combo_ForeignPairingActive):
 * the bank text and the Soaring location consult the crossing store, so a pairing
 * left armed by an earlier row would change what legs 3 and 4 read.
 *
 * CLEANUP. Restores SaveContext, MM_gPlayState, the bank threshold key, both option
 * CVars, and both read bits, then re-runs InitEnTimeTagBehavior() and BankSignHint()
 * under the restored save.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
extern "C" int MM_HintOptions1754_RunHeadless(void) {
    printf("[TEST] mm-hint-options-1754: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/ActorBehavior/ActorBehavior.h"
#include "2s2h/Rando/HintSources.h"
#include "2s2h/Rando/MiscBehavior/MiscBehavior.h"
#include "2s2h/Rando/Rando.h"

#include "combo_area.h"
#include "combo_hint_view.h"
#include "combo_mm_options_view.h"
#include "context.h"
#include "foreign_items.h"
#include "game.h"

extern "C" {
#include "variables.h"
#include "message_data_static.h"
int MM_HintSources_TestLegacyFixedText(uint16_t source, char* out, int cap);
int MM_HintSources_TestTextboxText(uint16_t source, char* out, int cap);
}

namespace {

#define HO1754_ASSERT(cond, code, msg)                                                                        \
    do {                                                                                                      \
        if (!(cond)) {                                                                                        \
            printf("[TEST] FAIL(%d): mm-hint-options-1754: %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                                                    \
        }                                                                                                     \
    } while (0)

constexpr const char* kBankThresholdCVar = "gEnhancements.DifficultyOptions.LowerBankRewardThresholds";

struct Ho1754SavedKey {
    const char* key;
    bool existed;
    int32_t value;

    explicit Ho1754SavedKey(const char* k) : key(k), existed(Combo_CVarIsExplicitInt(k)), value(CVarGetInteger(k, 0)) {
    }
    void Restore() const {
        if (existed) {
            CVarSetInteger(key, value);
        } else {
            CVarClear(key);
        }
    }
};

bool Ho1754GetReadBit(uint16_t source) {
    return ((gComboCtx.mmHintsRead[source >> 3] >> (source & 7u)) & 1u) != 0;
}

void Ho1754SetReadBit(uint16_t source, bool on) {
    const uint8_t mask = (uint8_t)(1u << (source & 7u));
    if (on) {
        gComboCtx.mmHintsRead[source >> 3] |= mask;
    } else {
        gComboCtx.mmHintsRead[source >> 3] &= (uint8_t)~mask;
    }
}

void Ho1754InitAuthoredSave(SaveContext* save) {
    std::memset(save, 0, sizeof(SaveContext));
    static const char kNewf[6] = { 'Z', 'E', 'L', 'D', 'A', '3' };
    std::memcpy(save->save.saveInfo.playerData.newf, kNewf, sizeof(kNewf));
    save->save.shipSaveInfo.saveType = SAVETYPE_RANDO;
}

// MESSAGE_HEADER_SIZE (11, 2s2h/CustomMessage/CustomMessage.h) header bytes + 0xBF terminator.
const char kEmptyVanillaMsg[MESSAGE_HEADER_SIZE + 1] = {
    0,          0,          (char)0xFE, (char)0xFF, (char)0xFF, (char)0xFF,
    (char)0xFF, (char)0xFF, (char)0xFF, (char)0xFF, (char)0xFF, (char)0xBF,
};

MessageTableEntry kTestMessageTable[3] = {
    { 0x1C14, 0, kEmptyVanillaMsg, (u32)sizeof(kEmptyVanillaMsg) },
    { 0x0C02, 0, kEmptyVanillaMsg, (u32)sizeof(kEmptyVanillaMsg) },
    { 0xFFFF, 0, nullptr, 0 },
};

int Ho1754RunDescriptorLeg() {
    if (Combo_MMOptionCount() == 0) {
        MM_RandoOptionsUi_Register();
    }

    const ComboMMOptionDesc* soaring = Combo_MMOptionById((uint16_t)RO_HINTS_SONG_OF_SOARING);
    HO1754_ASSERT(soaring != nullptr, 10, "RO_HINTS_SONG_OF_SOARING has no UI descriptor");
    HO1754_ASSERT(soaring->group == COMBO_MM_GROUP_HINTS, 11, "RO_HINTS_SONG_OF_SOARING is not under Hints");
    HO1754_ASSERT(soaring->widget == COMBO_MM_WIDGET_CHECKBOX, 12, "RO_HINTS_SONG_OF_SOARING is not a checkbox");
    HO1754_ASSERT(soaring->liveness == COMBO_MM_LIVENESS_LIVE, 13, "RO_HINTS_SONG_OF_SOARING is not LIVE");
    HO1754_ASSERT(soaring->defaultValue == 0, 14, "RO_HINTS_SONG_OF_SOARING default is not 0");
    // The hosted page's "<X> Hint" pattern, not upstream's label (LEGS, 1).
    HO1754_ASSERT(soaring->label != nullptr && std::strcmp(soaring->label, "Song of Soaring Hint") == 0, 15,
                  "RO_HINTS_SONG_OF_SOARING label is not 'Song of Soaring Hint'");

    const ComboMMOptionDesc* bank = Combo_MMOptionById((uint16_t)RO_HINTS_BANK_SIGN);
    HO1754_ASSERT(bank != nullptr, 16, "RO_HINTS_BANK_SIGN has no UI descriptor");
    HO1754_ASSERT(bank->group == COMBO_MM_GROUP_HINTS, 17, "RO_HINTS_BANK_SIGN is not under Hints");
    HO1754_ASSERT(bank->widget == COMBO_MM_WIDGET_CHECKBOX, 18, "RO_HINTS_BANK_SIGN is not a checkbox");
    HO1754_ASSERT(bank->liveness == COMBO_MM_LIVENESS_LIVE, 19, "RO_HINTS_BANK_SIGN is not LIVE");
    HO1754_ASSERT(bank->defaultValue == 0, 20, "RO_HINTS_BANK_SIGN default is not 0");
    HO1754_ASSERT(bank->label != nullptr && std::strcmp(bank->label, "Bank Reward Hint") == 0, 21,
                  "RO_HINTS_BANK_SIGN label is not 'Bank Reward Hint'");
    return 0;
}

int Ho1754RunListedAndResolverLegs() {
    auto save = std::make_unique<SaveContext>();
    Ho1754InitAuthoredSave(save.get());

    // Leg 2: Listed follows each option (0 -> false, 1 -> true).
    save->save.shipSaveInfo.rando.randoSaveOptions[RO_HINTS_SONG_OF_SOARING] = 0;
    HO1754_ASSERT(!Rando::HintSources::Listed(save.get(), RSBS_MM_HINT_SRC_SONG_OF_SOARING), 25,
                  "Song of Soaring hint source is listed with option at 0");
    save->save.shipSaveInfo.rando.randoSaveOptions[RO_HINTS_SONG_OF_SOARING] = 1;
    HO1754_ASSERT(Rando::HintSources::Listed(save.get(), RSBS_MM_HINT_SRC_SONG_OF_SOARING), 26,
                  "Song of Soaring hint source is not listed with option at 1");

    save->save.shipSaveInfo.rando.randoSaveOptions[RO_HINTS_BANK_SIGN] = 0;
    HO1754_ASSERT(!Rando::HintSources::Listed(save.get(), RSBS_MM_HINT_SRC_BANK_REWARD), 27,
                  "Bank Reward hint source is listed with option at 0");
    save->save.shipSaveInfo.rando.randoSaveOptions[RO_HINTS_BANK_SIGN] = 1;
    HO1754_ASSERT(Rando::HintSources::Listed(save.get(), RSBS_MM_HINT_SRC_BANK_REWARD), 28,
                  "Bank Reward hint source is not listed with option at 1");

    // Leg 3: Soaring resolver.
    const RandoCheckId kSoaringCheck = RC_WOODFALL_GREAT_FAIRY;
    save->save.shipSaveInfo.rando.randoSaveChecks[kSoaringCheck].randoItemId = RI_SONG_SOARING;
    save->save.shipSaveInfo.rando.randoSaveChecks[kSoaringCheck].shuffled = true;

    ComboHintResolution soaringRes{};
    HO1754_ASSERT(Rando::HintSources::Resolve(save.get(), RSBS_MM_HINT_SRC_SONG_OF_SOARING, &soaringRes), 30,
                  "Soaring hint source did not resolve");
    HO1754_ASSERT(soaringRes.type == (uint8_t)COMBO_HINT_TYPE_FIXED, 31, "Soaring hint type is not FIXED");
    HO1754_ASSERT(soaringRes.targetCount == 1, 32, "Soaring hint does not have exactly 1 target");
    HO1754_ASSERT(soaringRes.targets[0].checkGame == (uint8_t)GAME_MM &&
                      soaringRes.targets[0].check == (uint16_t)kSoaringCheck,
                  33, "Soaring hint target does not name the placed MM check");
    const ComboArea wantArea = Combo_Area_OfCheck((uint8_t)GAME_MM, (uint16_t)kSoaringCheck);
    HO1754_ASSERT(wantArea.area != COMBO_AREA_NONE && soaringRes.targets[0].area.game == wantArea.game &&
                      soaringRes.targets[0].area.area == wantArea.area,
                  34, "Soaring hint target area does not match Combo_Area_OfCheck");
    static const char kSoaringPrefix[] = "A note is carved into the stone...";
    HO1754_ASSERT(std::strncmp(soaringRes.text, kSoaringPrefix, sizeof(kSoaringPrefix) - 1) == 0, 35,
                  "Soaring hint text does not start with upstream engraving text");
    // ReplaceItemLocation fills "%y{{location}}%w" with Combo_Area_Name of the holding check's area
    // (HintAreaSingleExe.cpp ReplaceAreaText; an MM area's name is inserted unchanged).
    const char* const wantAreaName = Combo_Area_Name(wantArea);
    HO1754_ASSERT(wantAreaName != nullptr && wantAreaName[0] != '\0', 36, "the Soaring check's area has no name");
    const std::string wantAreaText = std::string("%y") + wantAreaName + "%w";
    HO1754_ASSERT(std::strstr(soaringRes.text, wantAreaText.c_str()) != nullptr, 36,
                  "Soaring hint text does not name the area of the check holding the Song of Soaring");

    // Leg 4: Bank resolver and Legacy/Textbox parity.
    const RandoCheckId kBankCheck = RC_CLOCK_TOWN_WEST_BANK_PIECE_OF_HEART;
    save->save.shipSaveInfo.rando.randoSaveChecks[kBankCheck].shuffled = true;
    save->save.shipSaveInfo.rando.randoSaveChecks[kBankCheck].obtained = false;
    save->save.shipSaveInfo.rando.randoSaveChecks[kBankCheck].randoItemId = RI_HOOKSHOT;

    CVarSetInteger(kBankThresholdCVar, 0);
    ComboHintResolution bankRes5000{};
    HO1754_ASSERT(Rando::HintSources::Resolve(save.get(), RSBS_MM_HINT_SRC_BANK_REWARD, &bankRes5000), 40,
                  "Bank Reward hint source did not resolve with threshold key at 0");
    HO1754_ASSERT(bankRes5000.type == (uint8_t)COMBO_HINT_TYPE_FIXED, 41, "Bank Reward hint type is not FIXED");
    HO1754_ASSERT(bankRes5000.targetCount == 1 && bankRes5000.targets[0].checkGame == (uint8_t)GAME_MM &&
                      bankRes5000.targets[0].check == (uint16_t)kBankCheck &&
                      bankRes5000.targets[0].itemGame == (uint8_t)GAME_MM &&
                      bankRes5000.targets[0].item == (uint16_t)RI_HOOKSHOT,
                  42, "Bank Reward hint target does not name the bank check holding RI_HOOKSHOT");
    HO1754_ASSERT(std::strstr(bankRes5000.text, "Deposit %r5000 Rupees") != nullptr, 43,
                  "Bank Reward hint text does not say 'Deposit %r5000 Rupees' with LowerBankRewardThresholds at 0");

    CVarSetInteger(kBankThresholdCVar, 1);
    ComboHintResolution bankRes1000{};
    HO1754_ASSERT(Rando::HintSources::Resolve(save.get(), RSBS_MM_HINT_SRC_BANK_REWARD, &bankRes1000), 44,
                  "Bank Reward hint source did not resolve with threshold key at 1");
    HO1754_ASSERT(std::strstr(bankRes1000.text, "Deposit %r1000 Rupees") != nullptr, 45,
                  "Bank Reward hint text does not say 'Deposit %r1000 Rupees' with LowerBankRewardThresholds at 1");

    // Legacy vs Textbox parity on live gSaveContext.
    std::memcpy(&gSaveContext, save.get(), sizeof(SaveContext));
    char legacy[RSBS_HINT_TEXT_CAP];
    char textbox[RSBS_HINT_TEXT_CAP];
    const uint16_t kSources[2] = { RSBS_MM_HINT_SRC_SONG_OF_SOARING, RSBS_MM_HINT_SRC_BANK_REWARD };
    for (const uint16_t src : kSources) {
        std::memset(legacy, 0, sizeof(legacy));
        std::memset(textbox, 0, sizeof(textbox));
        const int legLen = MM_HintSources_TestLegacyFixedText(src, legacy, (int)sizeof(legacy));
        const int boxLen = MM_HintSources_TestTextboxText(src, textbox, (int)sizeof(textbox));
        HO1754_ASSERT(legLen > 0 && boxLen > 0 && std::strcmp(legacy, textbox) == 0, 46,
                      "MM_HintSources_TestLegacyFixedText differs from MM_HintSources_TestTextboxText");
    }
    return 0;
}

int Ho1754RunHookLegs(PlayState* play) {
    play->msgCtx.messageTableNES = kTestMessageTable;
    MM_gPlayState = play;

    Ho1754InitAuthoredSave(&gSaveContext);
    RANDO_SAVE_CHECKS[RC_WOODFALL_GREAT_FAIRY].randoItemId = RI_SONG_SOARING;
    RANDO_SAVE_CHECKS[RC_WOODFALL_GREAT_FAIRY].shuffled = true;
    RANDO_SAVE_CHECKS[RC_SOUTHERN_SWAMP_SONG_OF_SOARING].cycleObtained = false;
    RANDO_SAVE_CHECKS[RC_SOUTHERN_SWAMP_SONG_OF_SOARING].eligible = false;
    RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_WEST_BANK_PIECE_OF_HEART].shuffled = true;
    RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_WEST_BANK_PIECE_OF_HEART].obtained = false;
    RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_WEST_BANK_PIECE_OF_HEART].randoItemId = RI_HOOKSHOT;
    RANDO_SAVE_OPTIONS[RO_HINTS_SONG_OF_SOARING] = 0;
    RANDO_SAVE_OPTIONS[RO_HINTS_BANK_SIGN] = 1;

    Rando::ActorBehavior::InitEnTimeTagBehavior();
    Rando::MiscBehavior::BankSignHint();

    // (a) 0x0C02 with Soaring option off: check becomes eligible, load stays true.
    Ho1754SetReadBit(RSBS_MM_HINT_SRC_SONG_OF_SOARING, false);
    u16 textId = 0x0C02;
    bool load = true;
    MM_GameHooks_ExecuteOnOpenText(&textId, &load);
    HO1754_ASSERT(RANDO_SAVE_CHECKS[RC_SOUTHERN_SWAMP_SONG_OF_SOARING].eligible, 50,
                  "(a) 0x0C02 with Soaring option off did not mark RC_SOUTHERN_SWAMP_SONG_OF_SOARING eligible");
    HO1754_ASSERT(load, 51, "(a) 0x0C02 with Soaring option off cleared loadFromMessageTable");
    HO1754_ASSERT(!Combo_HintIsRead((uint8_t)GAME_MM, RSBS_MM_HINT_SRC_SONG_OF_SOARING), 52,
                  "(a) 0x0C02 with Soaring option off marked the hint read");

    // (b) 0x0C02 with Soaring option on: load is false and hint is marked read.
    RANDO_SAVE_OPTIONS[RO_HINTS_SONG_OF_SOARING] = 1;
    Ho1754SetReadBit(RSBS_MM_HINT_SRC_SONG_OF_SOARING, false);
    textId = 0x0C02;
    load = true;
    MM_GameHooks_ExecuteOnOpenText(&textId, &load);
    HO1754_ASSERT(!load, 53, "(b) 0x0C02 with Soaring option on did not clear loadFromMessageTable");
    HO1754_ASSERT(Combo_HintIsRead((uint8_t)GAME_MM, RSBS_MM_HINT_SRC_SONG_OF_SOARING), 54,
                  "(b) 0x0C02 with Soaring option on did not mark RSBS_MM_HINT_SRC_SONG_OF_SOARING read");

    // (c) Option on with MM_gPlayState NULL: still eligible, load stays true.
    RANDO_SAVE_CHECKS[RC_SOUTHERN_SWAMP_SONG_OF_SOARING].eligible = false;
    MM_gPlayState = nullptr;
    textId = 0x0C02;
    load = true;
    MM_GameHooks_ExecuteOnOpenText(&textId, &load);
    MM_gPlayState = play;
    HO1754_ASSERT(RANDO_SAVE_CHECKS[RC_SOUTHERN_SWAMP_SONG_OF_SOARING].eligible, 55,
                  "(c) 0x0C02 with MM_gPlayState NULL did not mark RC_SOUTHERN_SWAMP_SONG_OF_SOARING eligible");
    HO1754_ASSERT(load, 56, "(c) 0x0C02 with MM_gPlayState NULL cleared loadFromMessageTable");

    // (d) 0x1C14 with bank check shuffled and not obtained: load false and bit read.
    Ho1754SetReadBit(RSBS_MM_HINT_SRC_BANK_REWARD, false);
    textId = 0x1C14;
    load = true;
    MM_GameHooks_ExecuteOnOpenText(&textId, &load);
    HO1754_ASSERT(!load, 57, "(d) 0x1C14 with bank check shuffled and unobtained did not clear loadFromMessageTable");
    HO1754_ASSERT(Combo_HintIsRead((uint8_t)GAME_MM, RSBS_MM_HINT_SRC_BANK_REWARD), 58,
                  "(d) 0x1C14 did not mark RSBS_MM_HINT_SRC_BANK_REWARD read");

    // (e) Bank check obtained: load stays true and bit is not set.
    RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_WEST_BANK_PIECE_OF_HEART].obtained = true;
    Ho1754SetReadBit(RSBS_MM_HINT_SRC_BANK_REWARD, false);
    textId = 0x1C14;
    load = true;
    MM_GameHooks_ExecuteOnOpenText(&textId, &load);
    HO1754_ASSERT(load, 59, "(e) 0x1C14 with bank check obtained cleared loadFromMessageTable");
    HO1754_ASSERT(!Combo_HintIsRead((uint8_t)GAME_MM, RSBS_MM_HINT_SRC_BANK_REWARD), 60,
                  "(e) 0x1C14 with bank check obtained marked the hint read");

    // (f) Bank option off: re-run BankSignHint() so registration reflects option 0; 0x1C14 leaves load true.
    RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_WEST_BANK_PIECE_OF_HEART].obtained = false;
    RANDO_SAVE_OPTIONS[RO_HINTS_BANK_SIGN] = 0;
    Rando::MiscBehavior::BankSignHint();
    Ho1754SetReadBit(RSBS_MM_HINT_SRC_BANK_REWARD, false);
    textId = 0x1C14;
    load = true;
    MM_GameHooks_ExecuteOnOpenText(&textId, &load);
    HO1754_ASSERT(load, 61, "(f) 0x1C14 with bank option off cleared loadFromMessageTable");
    HO1754_ASSERT(!Combo_HintIsRead((uint8_t)GAME_MM, RSBS_MM_HINT_SRC_BANK_REWARD), 62,
                  "(f) 0x1C14 with bank option off marked the hint read");
    return 0;
}

} // namespace

extern "C" int MM_HintOptions1754_RunHeadless(void) {
    printf("[TEST] mm-hint-options-1754: Song of Soaring and Bank Reward hint options (#1754)\n");

    auto ctx = Ship::Context::GetInstance();
    HO1754_ASSERT(ctx != nullptr, 1, "Ship::Context singleton missing - run the shared bring-up first");
    HO1754_ASSERT(ctx->GetConsoleVariables() != nullptr, 1, "ConsoleVariables missing - the legs read CVars");
    HO1754_ASSERT(!Combo_ForeignPairingActive(), 2,
                  "a paired foreign placement is active at entry - an earlier row left a pairing armed, and the "
                  "bank and Soaring legs would read its crossings");

    auto saved = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    std::memcpy(saved.get(), &gSaveContext, sizeof(gSaveContext));
    PlayState* const savedPlay = MM_gPlayState;
    const bool savedSoaringRead = Ho1754GetReadBit(RSBS_MM_HINT_SRC_SONG_OF_SOARING);
    const bool savedBankRead = Ho1754GetReadBit(RSBS_MM_HINT_SRC_BANK_REWARD);
    const Ho1754SavedKey savedKeys[] = {
        Ho1754SavedKey(kBankThresholdCVar),
        Ho1754SavedKey(Rando::StaticData::Options[RO_HINTS_SONG_OF_SOARING].cvar),
        Ho1754SavedKey(Rando::StaticData::Options[RO_HINTS_BANK_SIGN].cvar),
    };

    PlayState* play = static_cast<PlayState*>(calloc(1, sizeof(PlayState)));
    int rc = 1;
    if (play != nullptr) {
        rc = Ho1754RunDescriptorLeg();
        if (rc == 0) {
            rc = Ho1754RunListedAndResolverLegs();
        }
        if (rc == 0) {
            rc = Ho1754RunHookLegs(play);
        }
    } else {
        printf("[TEST] FAIL(1): mm-hint-options-1754: could not allocate stand-in PlayState\n");
    }

    // ---- Cleanup (runs on pass and on fail) --------------------------------
    std::memcpy(&gSaveContext, saved.get(), sizeof(gSaveContext));
    MM_gPlayState = savedPlay;
    for (const Ho1754SavedKey& key : savedKeys) {
        key.Restore();
    }
    Ho1754SetReadBit(RSBS_MM_HINT_SRC_SONG_OF_SOARING, savedSoaringRead);
    Ho1754SetReadBit(RSBS_MM_HINT_SRC_BANK_REWARD, savedBankRead);
    Rando::ActorBehavior::InitEnTimeTagBehavior();
    Rando::MiscBehavior::BankSignHint();
    free(play);

    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-hint-options-1754: PASS (descriptors LIVE under Hints, Listed/Resolve/Legacy parity hold, "
           "EnTimeTag and BankSignHint OnOpenText hooks pass all six legs)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
