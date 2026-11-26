#include "MiscBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/Rando/HintArea.h"
#ifdef RSBS_SINGLE_EXECUTABLE
#include "2s2h/Rando/HintSources.h"
#endif

#ifdef RSBS_SINGLE_EXECUTABLE
static std::string BankSignHintText() {
    Rando::HintSources::MarkRead(RSBS_MM_HINT_SRC_BANK_REWARD);
    const Rando::HintSources::JunkName junk = Rando::HintSources::JunkName::Textbox;
    return Rando::HintSources::FixedText(RSBS_MM_HINT_SRC_BANK_REWARD, RANDO_SAVE_CHECKS, nullptr, junk);
}
#else
static std::string BankSignHintText() {
    bool useCustomThresholds = CVarGetInteger("gEnhancements.DifficultyOptions.LowerBankRewardThresholds", 0);
    std::string threshold = useCustomThresholds ? "1000" : "5000";

    std::string msg = "%gSpecial Promotion!%w\n"
                      "Deposit %r" +
                      threshold +
                      " Rupees%w in your account and "
                      "receive %g{{item}}%w!\n"
                      "-Clock Town Bank";

    CustomMessage::Replace(
        &msg, "{{item}}",
        Rando::StaticData::GetItemName(RANDO_SAVE_CHECKS[RC_CLOCK_TOWN_WEST_BANK_PIECE_OF_HEART].randoItemId, true));
    return msg;
}
#endif

#define BANK_SIGN_MESSAGE_ID 0x1C14

void Rando::MiscBehavior::BankSignHint() {
    bool shouldRegister = IS_RANDO && RANDO_SAVE_OPTIONS[RO_HINTS_BANK_SIGN];

    COND_ID_HOOK(OnOpenText, BANK_SIGN_MESSAGE_ID, shouldRegister, [](u16* textId, bool* loadFromMessageTable) {
        RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != nullptr);
        auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
        RandoCheckId randoCheckId = RC_CLOCK_TOWN_WEST_BANK_PIECE_OF_HEART;
        auto& saveCheck = RANDO_SAVE_CHECKS[randoCheckId];

        if (!saveCheck.shuffled || saveCheck.obtained) {
            return; // Allow vanilla behavior to handle the text natively
        }

        entry.msg = BankSignHintText();

        CustomMessage::LoadCustomMessageIntoFont(entry);
        *loadFromMessageTable = false;
    });
}
