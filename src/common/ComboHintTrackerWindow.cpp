/**
 * @file ComboHintTrackerWindow.cpp
 * @brief Renders both games' hints in one window (#1475, lane H2).
 *
 * See ComboHintTrackerWindow.h for the contract. Each pane is SoH's Hint
 * Tracker body (randomizer_hint_tracker.cpp, DrawHintList), statement for
 * statement where the data allows: the same header controls behind the same
 * keys, the same group headers and entry lines in the same colours, the same
 * notes. What differs is where the rows come from (each game's adapter, already
 * grouped the way SoH groups them) and that the list is drawn inline rather than
 * in a child, so the window scrolls as a whole across both panes, as the
 * combined Check Tracker does. Nothing is cached: the rows are read again every
 * frame. The file's only state is each pane's search box and its scroll to the
 * current area (with, for MM's pane, the last scene it mapped to an area), and
 * the tab bar's last seed.
 *
 * src/common cannot include UIWidgets, so the themed buttons and the panes'
 * collapsing headers go through the combo_ui seam (Button: SoH's
 * ButtonOptions().Color(THEME_COLOR) at Sizes::Inline when given width 0;
 * PushTheme: UIWidgets::PushStyleCombobox(THEME_COLOR), which SoH also wraps
 * its filter box in).
 */

#include "ComboHintTrackerWindow.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/IconsFontAwesome4.h>
#include <libultraship/bridge/consolevariablebridge.h>

#include "combo_area.h"      // Combo_Area_Name
#include "combo_item_view.h" // Combo_ItemActiveGamePaused (the floating overlay's Only Enable While Paused)
#include "combo_tracker_view.h"
#include "combo_ui.h"
#include "context.h" // Context_GetCurrentGame

namespace ComboGui {

namespace {

const ComboUiTable& Ui() {
    return *ComboUi_Get();
}

// SoH's Hint Tracker colours and their defaults (randomizer_hint_tracker.cpp):
// the read text, the "???" of an unread hint, the Way of the Hero and Foolish
// names, and the dimmed found entries; and SoH's tracker background.
constexpr Color_RGBA8 kReadTextDefault = { 179, 179, 179, 255 };
constexpr Color_RGBA8 kUnreadDefault = { 128, 128, 128, 255 };
constexpr Color_RGBA8 kWothDefault = { 100, 180, 255, 255 };
constexpr Color_RGBA8 kFoolishDefault = { 255, 150, 180, 255 };
constexpr Color_RGBA8 kFoundDefault = { 110, 110, 110, 255 };
constexpr Color_RGBA8 kBgDefault = { 0, 0, 0, 255 }; // Color_Bg_Default

// SoH's notes (randomizer_hint_tracker.cpp), in its words.
constexpr const char* kWaitingNote = "Waiting for file load...";
constexpr const char* kRandoOnlyNote = "Hint Tracker is only available on randomizer saves.";
constexpr const char* kNoHintsNote = "No hints are available for this seed.";
constexpr const char* kJournalEmptyNote = "Hints you read will appear here.";

ImVec4 ToVec4(Color_RGBA8 c) {
    return ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
}

struct HintColours {
    ImVec4 read;
    ImVec4 unread;
    ImVec4 woth;
    ImVec4 foolish;
    ImVec4 found;
};

/** SoH's colour keys (DrawHintList: CVAR_TRACKER_HINT("<Name>Color.Value")), read once per frame. */
HintColours ReadHintColours() {
    HintColours c;
    c.read = ToVec4(CVarGetColor("gTrackers.HintTracker.ReadTextColor.Value", kReadTextDefault));
    c.unread = ToVec4(CVarGetColor("gTrackers.HintTracker.UnreadColor.Value", kUnreadDefault));
    c.woth = ToVec4(CVarGetColor("gTrackers.HintTracker.WothColor.Value", kWothDefault));
    c.foolish = ToVec4(CVarGetColor("gTrackers.HintTracker.FoolishColor.Value", kFoolishDefault));
    c.found = ToVec4(CVarGetColor("gTrackers.HintTracker.FoundColor.Value", kFoundDefault));
    return c;
}

/** One pane's own state: SoH's hintSearch (kept across frames) and its scroll to the current area. */
struct HintPaneState {
    ImGuiTextFilter search;
    ComboAreaScroll scroll;
    // MM's pane: the last scene key mapped to a hint area (ComboHintMMAreaOfScene).
    bool sceneCached = false;
    uint16_t mappedScene = 0;
    bool sceneMapped = false;
    uint16_t sceneArea = 0;
};

HintPaneState& Pane(uint8_t game) {
    static HintPaneState sPanes[3];
    return sPanes[game < 3 ? game : 0];
}

void SaveKeysNextFrame() {
    auto ctx = Ship::Context::GetInstance();
    if (ctx == nullptr || ctx->GetWindow() == nullptr || ctx->GetWindow()->GetGui() == nullptr) {
        return;
    }
    ctx->GetWindow()->GetGui()->SaveConsoleVariablesNextFrame();
}

/**
 * SoH's DrawViewTabs: the Locations / Journal tab bar, its choice kept in SoH's
 * view key. The key's value is forced onto the tab bar whenever it differs from
 * the one the bar last followed (SoH forces it once per session; here a key a
 * settings row or a preset changes reaches the bar too), and a tab the player
 * picks is written back, as SoH writes it. Returns the view to draw.
 */
int DrawViewTabs(int& seeded) {
    const int key = CVarGetInteger(kComboHintTrackerViewModeKey, kComboHintViewLocations);
    const bool force = seeded != key;
    struct Tab {
        const char* label;
        int mode;
        const char* tooltip;
    };
    static const Tab kTabs[] = {
        { "Locations###HintTrackerLocationsTab", kComboHintViewLocations,
          "Every hint location grouped by area, with unread hints masked as \"???\"" },
        { "Journal###HintTrackerJournalTab", kComboHintViewJournal,
          "Hints you have already read, grouped by hint type with the most valuable first" },
    };
    int shown = -1;
    if (ImGui::BeginTabBar("##HintTrackerViewTabs")) {
        for (const Tab& tab : kTabs) {
            const ImGuiTabItemFlags flags = (force && key == tab.mode) ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(tab.label, nullptr, flags)) {
                shown = tab.mode;
                ImGui::EndTabItem();
            }
            Ui().Tooltip(tab.tooltip);
        }
        ImGui::EndTabBar();
    }
    seeded = key;
    if (force || shown < 0) {
        return key; // the bar takes the key's tab from the next frame
    }
    if (shown != key) {
        CVarSetInteger(kComboHintTrackerViewModeKey, shown);
        SaveKeysNextFrame();
        seeded = shown;
    }
    return shown;
}

/** SoH's DrawHintEntry. */
void DrawHintEntry(const ComboHintEntry& entry, const HintColours& colours) {
    // A collected hint is dimmed and prefixed with a tick; the dim colour wins
    // over the Way of the Hero / Foolish colour.
    const ImVec4* nameColour = nullptr;
    if (entry.found) {
        nameColour = &colours.found;
    } else if (entry.colour == (uint8_t)COMBO_HINT_COLOUR_WOTH) {
        nameColour = &colours.woth;
    } else if (entry.colour == (uint8_t)COMBO_HINT_COLOUR_FOOLISH) {
        nameColour = &colours.foolish;
    }
    const std::string name = entry.found ? (std::string(ICON_FA_CHECK " ") + entry.name) : entry.name;
    if (nameColour != nullptr) {
        ImGui::PushStyleColor(ImGuiCol_Text, *nameColour);
        ImGui::TextUnformatted(name.c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::TextUnformatted(name.c_str());
    }
    if (entry.compact) {
        return;
    }
    Ui().Indent();
    if (entry.read) {
        ImGui::PushStyleColor(ImGuiCol_Text, entry.found ? colours.found : colours.read);
        ImGui::TextWrapped("%s", entry.text.c_str());
        ImGui::PopStyleColor();
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, colours.unread);
        ImGui::TextUnformatted(RSBS_HINT_MASKED_TEXT);
        ImGui::PopStyleColor();
    }
    Ui().Unindent();
    Ui().Spacer(0.0f);
}

/** SoH's DrawGroupHeader: open on first sight, then Expand All / Collapse All (`nextTreeState` 2 / 1). */
bool DrawGroupHeader(const ComboHintGroup& group, bool journal, uint8_t nextTreeState) {
    if (nextTreeState != 0) {
        ImGui::SetNextItemOpen(nextTreeState == 2, ImGuiCond_Always);
    } else {
        ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    }
    return ImGui::TreeNodeEx(ComboHintGroupLabel(group, journal).c_str());
}

/**
 * One game's pane body: SoH's DrawHintList, from its header to its list. SoH's
 * scroll to the area the player is in (its OnTransitionEnd sets doAreaScroll)
 * is paid at that area's group header, on the active game's pane only and in
 * the Locations view, once per area change (ComboAreaScrollNote).
 */
void DrawHintPane(uint8_t game, bool journal, const HintColours& colours) {
    std::vector<ComboHintRowData> rows;
    const uint8_t state = ComboCollectHintRows(game, rows);
    if (state == (uint8_t)COMBO_HINT_PANE_WAITING) {
        ImGui::TextUnformatted(kWaitingNote);
        return;
    }
    if (state == (uint8_t)COMBO_HINT_PANE_NOT_RANDO) {
        ImGui::TextWrapped("%s", kRandoOnlyNote);
        return;
    }

    HintPaneState& pane = Pane(game);
    uint8_t nextTreeState = 0;
    const bool showExpandCollapse = CVarGetInteger("gTrackers.HintTracker.ExpandCollapseButtonsVisible", 1) != 0;
    const bool showSearch = CVarGetInteger("gTrackers.HintTracker.SearchInputVisible", 1) != 0;
    const bool showTotals = CVarGetInteger("gTrackers.HintTracker.HintTotalsVisible", 1) != 0;
    if (showExpandCollapse) {
        const ComboUiWidgetOpts collapseTip = { "Collapse all areas", false, nullptr };
        if (Ui().Button("Collapse All", 0.0f, &collapseTip)) {
            nextTreeState = 1;
        }
        ImGui::SameLine();
        const ComboUiWidgetOpts expandTip = { "Expand all areas", false, nullptr };
        if (Ui().Button("Expand All", 0.0f, &expandTip)) {
            nextTreeState = 2;
        }
    }
    if (showSearch) {
        if (showExpandCollapse) {
            ImGui::SameLine();
        }
        const ComboUiWidgetOpts clearTip = { "Clear the search field", false, nullptr };
        if (Ui().Button("Clear", 0.0f, &clearTip)) {
            pane.search.Clear();
        }
        Ui().PushTheme();
        if (pane.search.Draw()) {
            nextTreeState = 2;
        }
        Ui().PopTheme();
    } else if (pane.search.IsActive()) {
        // SoH: the list is never left filtered by a box that is no longer shown.
        pane.search.Clear();
    }

    ComboHintPaneGroups groups;
    ComboGroupHintRows(rows, journal, pane.search.InputBuf, CVarGetInteger("gTrackers.HintTracker.HideFound", 0) != 0,
                       groups);
    if (showTotals) {
        ImGui::Text("%zu / %zu %s", groups.read, groups.total, "Hints Read");
    }
    if (groups.total == 0) {
        ImGui::TextWrapped("%s", kNoHintsNote);
        return;
    }
    if (journal && groups.read == 0) {
        ImGui::TextWrapped("%s", kJournalEmptyNote);
        return;
    }

    uint16_t area = 0;
    bool areaKnown = Combo_TrackerCurrentArea(game, &area);
    if (areaKnown && game == (uint8_t)GAME_MM) {
        // MM's current area is a scene key; its hint groups are shared-vocabulary
        // areas. The scene's area is worked out once per scene, not per frame.
        if (!pane.sceneCached || pane.mappedScene != area) {
            pane.sceneMapped = ComboHintMMAreaOfScene(area, &pane.sceneArea);
            pane.mappedScene = area;
            pane.sceneCached = true;
        }
        areaKnown = pane.sceneMapped;
        area = pane.sceneArea;
    }
    const bool activePane = game == (uint8_t)Context_GetCurrentGame();
    (void)ComboAreaScrollNote(pane.scroll, activePane, true, areaKnown, area);
    for (const ComboHintGroup& group : groups.groups) {
        const bool open = DrawGroupHeader(group, journal, nextTreeState);
        if (!journal && group.isArea && group.key <= 0xFFFFu && ComboAreaScrollTake(pane.scroll, (uint16_t)group.key)) {
            ImGui::SetScrollHereY(0.0f);
        }
        if (open) {
            for (const ComboHintEntry& entry : group.entries) {
                DrawHintEntry(entry, colours);
            }
            ImGui::TreePop();
        }
    }
}

/** One game's pane: its collapsing header (themed, as the Check Tracker's), then SoH's body. */
void DrawGamePane(uint8_t game, const char* title, bool journal, const HintColours& colours) {
    Ui().PushTheme();
    const bool open = ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen);
    Ui().PopTheme();
    if (!open) {
        return;
    }
    ImGui::PushID((int)game);
    DrawHintPane(game, journal, colours);
    ImGui::PopID();
}

/** Trackers::BeginFloatWindows' style, as the Check Tracker draws it (ComboTrackerWindow.cpp). */
void BeginChromeStyle(const ComboTrackerChromeKeys& keys) {
    ImVec4 bg(keys.bg[0] / 255.0f, keys.bg[1] / 255.0f, keys.bg[2] / 255.0f, keys.bg[3] / 255.0f);
    ImGuiWindow* window = ImGui::FindWindowByName(kComboHintTrackerWindowName);
    if (window != nullptr && window->DockTabIsVisible && window->ParentWindow != nullptr &&
        strncmp(window->ParentWindow->Name, "Main - Deck", strlen("Main - Deck")) == 0) {
        bg.w = 1.0f;
    }
    ImVec4 noBorder = ImGui::GetStyleColorVec4(ImGuiCol_Border);
    noBorder.w = 0.0f;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, bg);
    ImGui::PushStyleColor(ImGuiCol_Border, noBorder);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
}

void EndChromeStyle() {
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
}

/** Whether the check a hint names is collected in its own game's save (the pane's source). */
bool TargetObtained(const ComboHintTarget& t) {
    if (t.checkGame != (uint8_t)GAME_OOT && t.checkGame != (uint8_t)GAME_MM) {
        return false;
    }
    // OoT's own adapter indexes rows by check id; any other registrant (the UI
    // snapshot's synthetic world) is searched, as combo_tracker_view.c's
    // HostCheckFound does, so the answer never rests on that coincidence.
    ComboTrackerCheckRow row;
    if (Combo_TrackerCheckAt(t.checkGame, (int)t.check, &row) && row.checkId == t.check) {
        return row.obtained;
    }
    const int count = Combo_TrackerCheckCount(t.checkGame);
    for (int i = 0; i < count; i++) {
        if (Combo_TrackerCheckAt(t.checkGame, i, &row) && row.checkId == t.check) {
            return row.obtained;
        }
    }
    return false;
}

} // namespace

ComboHintRowData ComboHintRowCopy(const ComboHintPaneRow& row) {
    ComboHintRowData d;
    d.source = row.source;
    d.read = row.read;
    d.name = row.name != nullptr ? row.name : "";
    d.hasText = row.text != nullptr;
    d.text = row.text != nullptr ? row.text : "";
    d.locationKey = row.locationKey;
    d.locationName = row.locationName != nullptr ? row.locationName : "";
    d.locationIsArea = row.locationIsArea;
    d.journalKey = row.journalKey;
    d.journalName = row.journalName != nullptr ? row.journalName : "";
    d.journalLabel = row.journalLabel != nullptr ? row.journalLabel : "";
    d.journalCompact = row.journalCompact;
    d.journalColour = row.journalColour;
    d.sortRank = row.sortRank;
    d.found = row.found;
    return d;
}

const char* ComboHintRankGroupName(int rank) {
    // SoH's rankLabels (ItemCategoryRankName), then its ungraded "Items" group.
    static const char* const kNames[] = {
        "Major Items", "Boss Keys", "Small Keys", "Skulltula Tokens", "Hearts", "Lesser Items", "Junk Items", "Items",
    };
    return rank >= 0 && rank <= COMBO_HINT_RANK_UNGRADED ? kNames[rank] : kNames[COMBO_HINT_RANK_UNGRADED];
}

int ComboHintItemSlot(const std::vector<int>& ranks, int& sortRank) {
    // SoH's ItemHintSlot, clause for clause.
    sortRank = COMBO_HINT_RANK_JUNK + 1;
    bool ungraded = false;
    for (const int rank : ranks) {
        if (rank >= COMBO_HINT_RANK_MAJOR && rank <= COMBO_HINT_RANK_JUNK) {
            sortRank = std::min(sortRank, rank);
        } else {
            ungraded = true;
        }
    }
    if (ungraded && sortRank > COMBO_HINT_RANK_JUNK) {
        return COMBO_HINT_RANK_UNGRADED;
    }
    return std::min(sortRank, (int)COMBO_HINT_RANK_JUNK);
}

bool ComboHintMMFixedNamesArea(uint16_t source) {
    switch (source) {
        case RSBS_MM_HINT_SRC_REMAINS_ODOLWA:
        case RSBS_MM_HINT_SRC_REMAINS_GOHT:
        case RSBS_MM_HINT_SRC_REMAINS_GYORG:
        case RSBS_MM_HINT_SRC_REMAINS_TWINMOLD:
        case RSBS_MM_HINT_SRC_OATH:
        case RSBS_MM_HINT_SRC_HOOKSHOT:
        case RSBS_MM_HINT_SRC_GOLD_DUST:
            return true;
        default:
            return false;
    }
}

ComboHintRowData ComboHintMMPaneRow(const ComboHintRow& row) {
    ComboHintRowData d;
    d.source = row.info.source;
    d.read = row.read;
    d.name = row.info.name != nullptr ? row.info.name : "";
    // A read row prints the resolver's plain text ("???" when its world cannot be read).
    d.hasText = row.read;
    d.text = row.read ? row.display : "";
    if (row.areaName != nullptr && row.info.area.area != (uint8_t)COMBO_AREA_NONE) {
        d.locationKey = row.info.area.area;
        d.locationName = row.areaName;
    } else {
        d.locationKey = RSBS_HINT_GROUP_OTHER;
        d.locationName = "Other Hints";
    }
    // MM's areas are the shared vocabulary's, not the check tracker's MM area
    // keys (scenes): the pane maps MM's current scene into this space
    // (ComboHintMMAreaOfScene) to scroll to it, as SoH's Locations view does.
    d.locationIsArea = d.locationKey != RSBS_HINT_GROUP_OTHER;

    const ComboHintResolution& hint = row.hint;
    switch (hint.type) {
        case COMBO_HINT_TYPE_WOTH:
        case COMBO_HINT_TYPE_FOOLISH: {
            const bool woth = hint.type == (uint8_t)COMBO_HINT_TYPE_WOTH;
            d.journalKey = woth ? kComboHintJournalWoth : kComboHintJournalFoolish;
            d.journalName = woth ? "Way of the Hero" : "Foolish";
            // SoH: an area statement's areas say everything the hint did.
            std::string areas;
            for (int i = 0; i < (int)hint.areaCount && i < RSBS_HINT_TARGET_CAP; i++) {
                const char* name = Combo_Area_Name(hint.areas[i]);
                if (name == nullptr || name[0] == '\0') {
                    continue;
                }
                if (!areas.empty()) {
                    areas += ", ";
                }
                areas += name;
            }
            d.journalLabel = areas;
            d.journalCompact = !areas.empty();
            if (d.journalCompact) {
                d.journalColour = woth ? (uint8_t)COMBO_HINT_COLOUR_WOTH : (uint8_t)COMBO_HINT_COLOUR_FOOLISH;
            }
            break;
        }
        case COMBO_HINT_TYPE_ITEM:
        case COMBO_HINT_TYPE_FIXED: {
            if (hint.type == (uint8_t)COMBO_HINT_TYPE_FIXED && ComboHintMMFixedNamesArea(d.source)) {
                d.journalKey = kComboHintJournalArea;
                d.journalName = "Area";
                break;
            }
            // SoH's item ranking: the best item the hint names, found once every check it names is collected (in
            // whichever game holds it).
            const ComboMMHintDesc* desc = Combo_Hint_GetMMDesc();
            std::vector<int> ranks;
            bool found = hint.targetCount > 0;
            for (int i = 0; i < (int)hint.targetCount && i < RSBS_HINT_TARGET_CAP; i++) {
                const ComboHintTarget& t = hint.targets[i];
                ranks.push_back(desc != nullptr && desc->itemRank != nullptr ? desc->itemRank(t.itemGame, t.item)
                                                                             : (int)COMBO_HINT_RANK_UNGRADED);
                found = found && TargetObtained(t);
            }
            int sortRank = 0;
            const int slot = ComboHintItemSlot(ranks, sortRank);
            d.journalKey = kComboHintJournalItem + (uint32_t)slot;
            d.journalName = ComboHintRankGroupName(slot);
            d.sortRank = sortRank;
            d.found = found;
            break;
        }
        default:
            d.journalKey = kComboHintJournalOther;
            d.journalName = "Other Hints";
            break;
    }
    return d;
}

bool ComboHintMMAreaOfScene(uint16_t sceneKey, uint16_t* outArea) {
    const int count = Combo_TrackerCheckCount((uint8_t)GAME_MM);
    for (int i = 0; i < count; i++) {
        ComboTrackerCheckRow row;
        if (!Combo_TrackerCheckAt((uint8_t)GAME_MM, i, &row) || row.areaName == nullptr || row.areaKey != sceneKey) {
            continue;
        }
        const ComboArea area = Combo_Area_OfCheck((uint8_t)GAME_MM, row.checkId);
        if (area.game != (uint8_t)GAME_MM || area.area == (uint8_t)COMBO_AREA_NONE ||
            area.area == (uint8_t)COMBO_AREA_POCKET) {
            continue;
        }
        if (outArea != nullptr) {
            *outArea = area.area;
        }
        return true;
    }
    return false;
}

uint8_t ComboCollectHintRows(uint8_t game, std::vector<ComboHintRowData>& out) {
    out.clear();
    if (game == (uint8_t)GAME_OOT) {
        const ComboOoTHintOps* ops = Combo_Hint_GetOoTOps();
        if (ops == nullptr) {
            return (uint8_t)COMBO_HINT_PANE_WAITING;
        }
        int count = 0;
        const uint8_t state = ops->collect(&count);
        if (state != (uint8_t)COMBO_HINT_PANE_READY) {
            return state;
        }
        for (int i = 0; i < count; i++) {
            ComboHintPaneRow row;
            std::memset(&row, 0, sizeof(row));
            if (ops->rowAt(i, &row)) {
                out.push_back(ComboHintRowCopy(row));
            }
        }
        return (uint8_t)COMBO_HINT_PANE_READY;
    }
    if (game == (uint8_t)GAME_MM) {
        ComboHintGameSummary summary;
        Combo_HintGameSummary((uint8_t)GAME_MM, &summary);
        if (summary.freshness == (uint8_t)COMBO_TRACKER_FRESH_UNAVAILABLE) {
            return (uint8_t)COMBO_HINT_PANE_WAITING;
        }
        if (!summary.hasWorld) {
            return (uint8_t)COMBO_HINT_PANE_NOT_RANDO;
        }
        // Drawn every frame: one buffer, filled in one pass (Combo_HintRowsAll).
        static std::vector<ComboHintRow> sRows((size_t)RSBS_MM_HINT_READ_CAP);
        const int count = Combo_HintRowsAll((uint8_t)GAME_MM, sRows.data(), (int)sRows.size());
        for (int i = 0; i < count && i < (int)sRows.size(); i++) {
            out.push_back(ComboHintMMPaneRow(sRows[(size_t)i]));
        }
        return (uint8_t)COMBO_HINT_PANE_READY;
    }
    return (uint8_t)COMBO_HINT_PANE_WAITING;
}

void ComboGroupHintRows(const std::vector<ComboHintRowData>& rows, bool journal, const char* search, bool hideFound,
                        ComboHintPaneGroups& out) {
    out = ComboHintPaneGroups();
    const ImGuiTextFilter filter(search != nullptr ? search : "");
    std::map<uint32_t, ComboHintGroup> groups; // SoH's std::map: groups in key order
    for (const ComboHintRowData& row : rows) {
        out.total++;
        if (row.read) {
            out.read++;
        }
        // SoH: the Journal lists only hints already read, so it never says which
        // sources hold an unread Way of the Hero or Foolish hint.
        if (journal && !row.read) {
            continue;
        }
        const uint32_t key = journal ? row.journalKey : row.locationKey;
        const std::string& groupName = journal ? row.journalName : row.locationName;
        ComboHintGroup& group = groups[key];
        if (group.name.empty()) {
            group.key = key;
            group.name = groupName;
            group.isArea = !journal && row.locationIsArea;
        }
        group.total++;
        if (row.read) {
            group.read++;
        }
        // "Found" is a Journal state only; Hide Found drops it from the list, not from the counts.
        const bool found = journal && row.found;
        if (found && hideFound) {
            continue;
        }
        const std::string& entryName = (journal && !row.journalLabel.empty()) ? row.journalLabel : row.name;
        if (!filter.PassFilter(entryName.c_str()) && !filter.PassFilter(groupName.c_str())) {
            continue;
        }
        ComboHintEntry entry;
        entry.source = row.source;
        entry.name = entryName;
        entry.read = row.read;
        entry.hasText = row.hasText;
        entry.text = row.text;
        entry.compact = journal && row.journalCompact;
        entry.sortRank = journal ? row.sortRank : 0;
        entry.colour = journal ? row.journalColour : (uint8_t)COMBO_HINT_COLOUR_NONE;
        entry.found = found;
        group.entries.push_back(std::move(entry));
    }
    const bool searching = filter.IsActive();
    for (auto& [key, group] : groups) {
        (void)key;
        if (group.entries.empty() && searching) {
            continue; // SoH skips a group the search left empty
        }
        std::stable_sort(group.entries.begin(), group.entries.end(),
                         [](const ComboHintEntry& left, const ComboHintEntry& right) {
                             // Collected hints sink to the bottom of their group.
                             if (left.found != right.found) {
                                 return !left.found;
                             }
                             if (left.sortRank != right.sortRank) {
                                 return left.sortRank < right.sortRank;
                             }
                             return left.name < right.name;
                         });
        out.groups.push_back(std::move(group));
    }
}

std::string ComboHintGroupLabel(const ComboHintGroup& group, bool journal) {
    // SoH's DrawGroupHeader: the Journal shows no total, which would spoil how
    // many hints of a kind exist.
    std::string label = group.name + " (" + std::to_string(group.read);
    if (!journal) {
        label += "/" + std::to_string(group.total);
    }
    return label + ")###HintTracker" + group.name;
}

ComboTrackerChromeKeys ComboReadHintTrackerChromeKeys() {
    // SoH's keys and defaults (randomizer_hint_tracker.cpp, HintTrackerWindow::DrawElement).
    ComboTrackerChromeKeys keys;
    keys.windowType = CVarGetInteger("gTrackers.HintTracker.WindowType", kComboTrackerWindowWindow);
    keys.draggable = CVarGetInteger("gTrackers.HintTracker.Draggable", 1) != 0;
    keys.showOnlyPaused = CVarGetInteger("gTrackers.HintTracker.ShowOnlyPaused", 0) != 0;
    keys.displayType = CVarGetInteger("gTrackers.HintTracker.DisplayType", kComboTrackerDisplayAlways);
    keys.comboButton1 = CVarGetInteger("gTrackers.HintTracker.ComboButton1", kComboTrackerComboButtonL);
    keys.comboButton2 = CVarGetInteger("gTrackers.HintTracker.ComboButton2", kComboTrackerComboButtonR);
    const Color_RGBA8 bg = CVarGetColor("gTrackers.HintTracker.BgColor.Value", kBgDefault);
    keys.bg[0] = bg.r;
    keys.bg[1] = bg.g;
    keys.bg[2] = bg.b;
    keys.bg[3] = bg.a;
    keys.fontSize = CVarGetFloat("gTrackers.HintTracker.FontSize", 1.0f);
    return keys;
}

void ComboHintTrackerWindow::Draw() {
    // The active game's area, asked every frame, open or not (as the Check Tracker asks it).
    (void)Combo_TrackerCurrentArea((uint8_t)Context_GetCurrentGame(), nullptr);

    // The visibility CVar, read live (#489 cause 1; the Check Tracker's pattern).
    if (!CVarGetInteger(kComboHintTrackerVisibilityCVar, 0)) {
        return;
    }

    // SoH's Hint Tracker chrome: its keys, its floating gates on the active game's
    // pause and the first pad's held buttons, and BeginFloatWindows' flags.
    const ComboTrackerChromeKeys keys = ComboReadHintTrackerChromeKeys();
    const bool floating = keys.windowType == kComboTrackerWindowFloating;
    const bool paused = floating && keys.showOnlyPaused && Combo_ItemActiveGamePaused();
    const uint32_t pad =
        floating && keys.displayType == kComboTrackerDisplayComboButton ? ComboTrackerHeldPadButtons() : 0;
    const ComboTrackerChrome chrome = ComboTrackerChromeFor(keys, paused, pad);
    if (!chrome.draws) {
        return;
    }
    if (chrome.mainViewport) {
        ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    }

    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(500, 600), ImGuiCond_FirstUseEver); // SoH's first-use size
    BeginChromeStyle(keys);
    if (ImGui::Begin(kComboHintTrackerWindowName, chrome.closeButton ? &open : nullptr,
                     (ImGuiWindowFlags)chrome.flags)) {
        ImGui::SetWindowFontScale(keys.fontSize);
        DrawElement();
    }
    EndChromeStyle();
    ImGui::End();
    if (!open) {
        SetVisibility(false);
    }
}

void ComboHintTrackerWindow::DrawElement() {
    // SoH's tab bar first, one for the window: both panes show the view it picks.
    const bool journal = DrawViewTabs(mSeededView) == kComboHintViewJournal;
    const HintColours colours = ReadHintColours();
    DrawGamePane((uint8_t)GAME_OOT, "Ocarina of Time", journal, colours);
    DrawGamePane((uint8_t)GAME_MM, "Majora's Mask", journal, colours);
}

void RegisterComboHintTrackerWindow(std::shared_ptr<Ship::Gui> gui) {
    if (gui == nullptr) {
        return;
    }
    // Idempotence, the #457 guard: AddGuiWindow rejects duplicates silently.
    if (gui->GetGuiWindow(kComboHintTrackerWindowName) != nullptr) {
        return;
    }
    gui->AddGuiWindow(
        std::make_shared<ComboHintTrackerWindow>(kComboHintTrackerVisibilityCVar, kComboHintTrackerWindowName));
}

} // namespace ComboGui
