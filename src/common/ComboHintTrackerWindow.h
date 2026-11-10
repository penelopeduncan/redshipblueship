/**
 * @file ComboHintTrackerWindow.h
 * @brief The combined Hint Tracker: both games' hints in one window (#1475,
 *        lane H2).
 *
 * Ship of Harkinian's Hint Tracker (randomizer_hint_tracker.cpp, #1468) drawn
 * once for each game, the way the combined Check Tracker draws SoH's Check
 * Tracker once per game (ComboTrackerWindow.h): SoH's Locations / Journal tab
 * bar on top, then two collapsing panes, "Ocarina of Time" and "Majora's Mask",
 * each fed only by its own game's adapter (combo_hint_view.h: OoT's ops over
 * SoH's hint data, MM's resolver over the MM save the check tracker's MM pane
 * reads), each with SoH's header (Collapse All, Expand All, Clear and the
 * filter, the "N / M Hints Read" line) and SoH's grouped list, under SoH's Hint
 * Tracker Settings keys (gTrackers.HintTracker.*), the window chrome included.
 *
 * It is SoH's window in the single executable: registered under SoH's window
 * name and opened by SoH's window key, so SoH's Randomizer > Hint Tracker >
 * Toggle Hint Tracker opens it. SoH's own window stays registered, retired, as
 * "SoH Hint Tracker" (games/oot/soh/SohGui/SohRetiredWindow.h): its InitElement
 * keeps registering the read hook and the hintTrackerData save section the OoT
 * pane reads. Common-owned (ADR 0008): it reads the adapters and gComboCtx and
 * nothing else, so it draws under GAME_OOT, GAME_MM and GAME_NONE alike.
 *
 * Locked ROM-free by combo-hint-tracker-window (registration, the draw path's
 * inertness with no ImGui context, the grouping SoH's DrawHintList does, MM's
 * Journal decisions, and the cross-game Seen plumbing). Its appearance is the UI
 * snapshot row's (window/Hint Tracker).
 */

#ifndef RSBS_COMMON_COMBO_HINT_TRACKER_WINDOW_H
#define RSBS_COMMON_COMBO_HINT_TRACKER_WINDOW_H

#ifdef __cplusplus

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <ship/window/gui/GuiWindow.h>

#include "ComboTrackerWindow.h" // ComboTrackerChromeKeys, ComboTrackerChromeFor, ComboAreaScroll
#include "combo_hint_view.h"

namespace Ship {
class Gui;
}

namespace ComboGui {

// SoH's own window name and key (SohGui.cpp, SetupGuiElements; CVAR_WINDOW("HintTracker")):
// SoH's toggle row looks its window up by name and labels itself from the key.
inline constexpr const char* kComboHintTrackerWindowName = "Hint Tracker";
inline constexpr const char* kComboHintTrackerVisibilityCVar = "gOpenWindows.HintTracker";
// SoH's view key (CVAR_TRACKER_HINT("ViewMode")) and its two values (HintTrackerViewMode).
inline constexpr const char* kComboHintTrackerViewModeKey = "gTrackers.HintTracker.ViewMode";
inline constexpr int kComboHintViewLocations = 0; // HINT_TRACKER_VIEW_LOCATIONS
inline constexpr int kComboHintViewJournal = 1;   // HINT_TRACKER_VIEW_JOURNAL

class ComboHintTrackerWindow final : public Ship::GuiWindow {
  public:
    using Ship::GuiWindow::GuiWindow;

    void Draw() override;

  protected:
    void DrawElement() override;
    void InitElement() override {
    }
    void UpdateElement() override {
    }

  private:
    int mSeededView = -1; // the view key's value the tab bar last followed
};

/** Register the window on `gui` under kComboHintTrackerWindowName. Idempotent; a null `gui` is a no-op. */
void RegisterComboHintTrackerWindow(std::shared_ptr<Ship::Gui> gui);

// ============================================================================
// The pane's decisions, as pure functions the draw code calls (and the lock holds)
// ============================================================================

/** A ComboHintPaneRow with its strings owned. */
struct ComboHintRowData {
    uint16_t source = 0;
    bool read = false;
    std::string name;
    bool hasText = false;
    std::string text;
    uint32_t locationKey = RSBS_HINT_GROUP_OTHER;
    std::string locationName;
    bool locationIsArea = false;
    uint32_t journalKey = 0;
    std::string journalName;
    std::string journalLabel; // "" : name
    bool journalCompact = false;
    uint8_t journalColour = COMBO_HINT_COLOUR_NONE;
    int sortRank = 0;
    bool found = false;
};

/** `row` with its strings copied (NULL strings read as ""). */
ComboHintRowData ComboHintRowCopy(const ComboHintPaneRow& row);

/** SoH's Journal group names (randomizer_hint_tracker.cpp): an item rank's group, COMBO_HINT_RANK_UNGRADED "Items". */
const char* ComboHintRankGroupName(int rank);

/**
 * SoH's Journal group key for a hint type, by its priority list (Way of the
 * Hero, Foolish, Item, Area, ...), scaled by its eight item slots: an item hint
 * adds its slot (ComboHintItemSlot) to the Item key.
 */
inline constexpr uint32_t kComboHintJournalSlots = 8;
inline constexpr uint32_t kComboHintJournalWoth = 0 * kComboHintJournalSlots;
inline constexpr uint32_t kComboHintJournalFoolish = 1 * kComboHintJournalSlots;
inline constexpr uint32_t kComboHintJournalItem = 2 * kComboHintJournalSlots;
inline constexpr uint32_t kComboHintJournalArea = 3 * kComboHintJournalSlots;
inline constexpr uint32_t kComboHintJournalOther = 10 * kComboHintJournalSlots;

/**
 * SoH's ItemHintSlot over ranks: `ranks` are each named check's item rank
 * (COMBO_HINT_RANK_UNGRADED for one the game cannot grade). Returns the slot
 * (COMBO_HINT_RANK_MAJOR .. COMBO_HINT_RANK_JUNK, or COMBO_HINT_RANK_UNGRADED
 * when some check is ungraded and none ranks above junk) and sets `sortRank`:
 * the best rank, or one past junk with none.
 */
int ComboHintItemSlot(const std::vector<int>& ranks, int& sortRank);

/**
 * Whether MM fixed source `source` is SoH's "Area" kind (it says where an item
 * is: the remains posters, the Oath, the Hookshot, the gold dust) rather than an
 * item hint (it names what a check holds: the gold dust prize, the spider
 * houses, the Great Fairies, the Bomb Shop's next stock).
 */
bool ComboHintMMFixedNamesArea(uint16_t source);

/**
 * MM's row as the pane lists it: grouped by its area in the Locations view, and
 * in the Journal under SoH's group for its kind (Way of the Hero and Foolish
 * by the areas they name, coloured; item hints by their items' ranks through
 * MM's descriptor, found once every check they name is collected in either
 * game; the fixed area hints under "Area").
 */
ComboHintRowData ComboHintMMPaneRow(const ComboHintRow& row);

/**
 * The MM pane's group key for MM's current area (#1475): MM's check tracker keys
 * the player's area by scene (Combo_TrackerCurrentArea(GAME_MM)), its hint rows
 * by the shared vocabulary's MM area. This is the area (Combo_Area_OfCheck) of the
 * first MM check its tracker lists under scene key `sceneKey`, in `*outArea`; false
 * when no such check has an area (no MM save to read, a scene with no checks).
 */
bool ComboHintMMAreaOfScene(uint16_t sceneKey, uint16_t* outArea);

/** `game`'s rows into `out` and what the pane has to list (a ComboHintPaneState). */
uint8_t ComboCollectHintRows(uint8_t game, std::vector<ComboHintRowData>& out);

/** One group of a pane's list: SoH's HintGroup. */
struct ComboHintEntry {
    uint16_t source = 0;
    std::string name;
    bool read = false;
    bool hasText = false;
    std::string text;
    bool compact = false;
    int sortRank = 0;
    uint8_t colour = COMBO_HINT_COLOUR_NONE;
    bool found = false;
};

struct ComboHintGroup {
    uint32_t key = 0;
    std::string name;
    std::size_t read = 0;
    std::size_t total = 0;
    bool isArea = false;
    std::vector<ComboHintEntry> entries; // SoH's order: found last, then sortRank, then name
};

struct ComboHintPaneGroups {
    std::size_t total = 0; // every hint the pane lists, either view
    std::size_t read = 0;
    std::vector<ComboHintGroup> groups; // ascending key
};

/**
 * SoH's DrawHintList grouping over `rows`: in the Locations view every row by
 * its location group; in the Journal only read rows, by their journal group,
 * with Hide Found (`hideFound`) dropping the found ones. Group counts count
 * every row the view files there; entries are the rows `search` (ImGui's filter
 * syntax, SoH's box) passes on the entry's name or its group's name. A group the
 * search empties is dropped while the search is active.
 */
void ComboGroupHintRows(const std::vector<ComboHintRowData>& rows, bool journal, const char* search, bool hideFound,
                        ComboHintPaneGroups& out);

/** SoH's group header label: "<name> (read/total)", or "<name> (read)" in the Journal, with a stable id. */
std::string ComboHintGroupLabel(const ComboHintGroup& group, bool journal);

/**
 * SoH's Hint Tracker chrome keys (gTrackers.HintTracker.*: Window Type, Enable
 * Dragging, Only Enable While Paused, Display Mode and its combo buttons,
 * Background Color, Font Size), SoH's defaults for any key not set, in the
 * Check Tracker's struct, so ComboTrackerChromeFor decides the frame.
 */
ComboTrackerChromeKeys ComboReadHintTrackerChromeKeys();

} // namespace ComboGui

#endif // __cplusplus

#endif // RSBS_COMMON_COMBO_HINT_TRACKER_WINDOW_H
