#pragma once
#ifndef PLANDOMIZER_H
#define PLANDOMIZER_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
}
#endif

#endif // PLANDOMIZER_H

#include <libultraship/libultraship.h>
#include "soh/Enhancements/randomizer/item.h"

#ifdef __cplusplus
#include <string>
#include "nlohmann/json.hpp" // RSBS (#1731): PlandomizerParseSpoilerJson
class PlandomizerWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override;
    void DrawElement() override;
    void UpdateElement() override{};
};

typedef struct {
    std::string checkName;
    Rando::Item checkRewardItem;
    int32_t shopPrice;
    Rando::Item iceTrapModel;
    std::string iceTrapName;
} SpoilerCheckObject;

typedef struct {
    std::string hintName;
    std::string hintType;
    std::string hintText;
} SpoilerHintObject;

// RSBS (#1731): fill the Plandomizer's lists from a spoiler log's json. False, with the lists empty and *error naming
// it, when the log names a location, item or ice-trap model this build does not know.
bool PlandomizerParseSpoilerJson(const nlohmann::json& spoilerLogInput, std::string* error);

typedef enum PlandoTabs {
    TAB_HINTS,
    TAB_LOCATIONS,
} PlandoTabs;

typedef enum PlandoHints {
    HINT_SINGLE,
    HINT_ALL,
} PlandoHints;

#endif
