#include "OcarinaNoteMappingCleanup.h"

#include <libultraship/libultraship.h>
#include <ship/Context.h>
#include <ship/config/ConsoleVariable.h>
#include <ship/controller/controldeck/ControlDeck.h>
#include <ship/utils/StringHelper.h>

#include "soh/OTRGlobals.h"

#include <charconv>
#include <sstream>
#include <system_error>

namespace {

// The four buttons the pre-#1740 Cosmetics Editor loop wrote D-pad copies onto. D4 and the other custom buttons were
// never written, so the rule stays on these four.
const CONTROLLERBUTTONS_T kSohInputOcarinaNoteBitmasks[] = { BTN_CUSTOM_OCARINA_NOTE_F4, BTN_CUSTOM_OCARINA_NOTE_A4,
                                                             BTN_CUSTOM_OCARINA_NOTE_B4, BTN_CUSTOM_OCARINA_NOTE_D5 };

// "P<digits>-B<digits>-..." -> the B field. False when the id is not in that shape.
bool SohInputNoteCleanup_ParseIdBitmask(const std::string& id, uint32_t& out) {
    if (id.empty() || id[0] != 'P') {
        return false;
    }
    const char* const end = id.data() + id.size();
    uint32_t port = 0;
    const std::from_chars_result portField = std::from_chars(id.data() + 1, end, port);
    if (portField.ec != std::errc() || end - portField.ptr < 2 || portField.ptr[0] != '-' || portField.ptr[1] != 'B') {
        return false;
    }
    uint32_t bitmask = 0;
    const std::from_chars_result bitmaskField = std::from_chars(portField.ptr + 2, end, bitmask);
    if (bitmaskField.ec != std::errc() || bitmaskField.ptr == end || *bitmaskField.ptr != '-') {
        return false;
    }
    out = bitmask;
    return true;
}

} // namespace

std::string SohInput_StripForeignBitmaskMappingIds(const std::string& idList, uint32_t ownBitmask, size_t* droppedOut) {
    std::stringstream in(idList);
    std::string id;
    std::string kept;
    size_t dropped = 0;
    while (std::getline(in, id, ',')) {
        if (id.empty()) {
            continue;
        }
        uint32_t idBitmask = 0;
        if (SohInputNoteCleanup_ParseIdBitmask(id, idBitmask) && idBitmask != ownBitmask) {
            dropped++;
            continue;
        }
        kept += id;
        kept += ",";
    }
    if (droppedOut != nullptr) {
        *droppedOut = dropped;
    }
    return kept;
}

bool SohInput_DropForeignOcarinaNoteMappingIds() {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetControlDeck() == nullptr || ctx->GetConsoleVariables() == nullptr) {
        return false;
    }
    auto deck = ctx->GetControlDeck();
    auto cvars = ctx->GetConsoleVariables();
    bool changed = false;
    for (uint8_t port = 0; port < MAXCONTROLLERS; port++) {
        for (CONTROLLERBUTTONS_T bitmask : kSohInputOcarinaNoteBitmasks) {
            // The key ControllerButton::SaveButtonMappingIdsToConfig and ReloadAllMappingsFromConfig build.
            const std::string key = StringHelper::Sprintf(CVAR_PREFIX_CONTROLLERS ".Port%d.Buttons.%sButtonMappingIds",
                                                          port + 1, deck->GetButtonNameForBitmask(bitmask).c_str());
            // A copy: SetString frees the string GetString points at.
            const std::string list = cvars->GetString(key.c_str(), "");
            size_t dropped = 0;
            const std::string kept = SohInput_StripForeignBitmaskMappingIds(list, bitmask, &dropped);
            if (dropped == 0) {
                continue;
            }
            if (kept.empty()) {
                cvars->ClearVariable(key.c_str());
            } else {
                cvars->SetString(key.c_str(), kept.c_str());
            }
            changed = true;
        }
    }
    return changed;
}
