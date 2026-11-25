#include "ActorBehavior.h"
#include <libultraship/bridge/consolevariablebridge.h>
#include "2s2h/Rando/Logic/Logic.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/Rando/HintArea.h"
#ifdef RSBS_SINGLE_EXECUTABLE
#include "2s2h/Rando/HintSources.h"
#endif

extern "C" {
#include "variables.h"
#include "src/overlays/actors/ovl_En_Toto/z_en_toto.h"
extern void func_80BA36C0(EnToto* enToto, PlayState* play, s32 index);
}

#ifdef RSBS_SINGLE_EXECUTABLE
static std::string SoaringHintText() {
    Rando::HintSources::MarkRead(RSBS_MM_HINT_SRC_SONG_OF_SOARING);
    return Rando::HintSources::FixedText(RSBS_MM_HINT_SRC_SONG_OF_SOARING, RANDO_SAVE_CHECKS, nullptr);
}
#else
static std::string SoaringHintText() {
    std::string msg =
        "A note is carved into the stone...\x10What you seek lies " RANDO_HINT_PREPOS("in") " %y{{location}}%w.";
    Rando::HintArea::ReplaceItemLocation(&msg, "{{location}}", RI_SONG_SOARING);
    return msg;
}
#endif

// The cutscene skip is handled elsewhere, and forced on for rando.
// Upon interacting with the engraving, this text ID is opened.
#define ENGRAVING_TEXT_ID 0xC02

void Rando::ActorBehavior::InitEnTimeTagBehavior() {
    COND_ID_HOOK(OnOpenText, ENGRAVING_TEXT_ID, IS_RANDO, [](u16* textId, bool* loadFromMessageTable) {
        if (!RANDO_SAVE_CHECKS[RC_SOUTHERN_SWAMP_SONG_OF_SOARING].cycleObtained) {
            RANDO_SAVE_CHECKS[RC_SOUTHERN_SWAMP_SONG_OF_SOARING].eligible = true;
        }

        if (RANDO_SAVE_OPTIONS[RO_HINTS_SONG_OF_SOARING]) {
            RSBS_SINGLE_EXE_REQUIRE(MM_gPlayState != nullptr);
            auto entry = CustomMessage::LoadVanillaMessageTableEntry(*textId);
            entry.msg = SoaringHintText();
            CustomMessage::LoadCustomMessageIntoFont(entry);
            *loadFromMessageTable = false;
        }
    });
}
