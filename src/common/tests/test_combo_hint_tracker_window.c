/**
 * @file test_combo_hint_tracker_window.c
 * @brief ROM-free lock for the combined Hint Tracker (#1475, lane H2).
 *
 * The MM side's model has its own lock (test_mm_hint_sources.c). This one holds
 * the window that draws both games and what H2 added beside it:
 *
 * 1. REGISTRATION. The window lands on a bare Ship::Gui under SoH's own window
 *    name ("Hint Tracker", which SoH's Toggle Hint Tracker row looks up), is the
 *    combined class, a second registration is a no-op, and stand-ins already
 *    holding the retired SoH window's name and its settings window's are
 *    undisturbed.
 *
 * 2. INERTNESS. No ImGui context exists here, so a Draw() that reached
 *    ImGui::Begin would abort the process: with the visibility key cleared,
 *    Draw() and Update() run under GAME_OOT, GAME_MM and GAME_NONE, and the
 *    model reads the draw makes (both games' rows through the production
 *    adapters) run under each, answering no rows unless a pane is READY.
 *
 * 3. SoH's GROUPING (DrawHintList), over synthetic rows: the Locations view's
 *    groups in key order with "(read/total)" headers and every row; the
 *    Journal's read rows only, by type group, found entries last and Hide Found
 *    dropping them from the list but not the counts, "(read)" headers; the
 *    search matching an entry's name or its group's and dropping the groups it
 *    empties; the pane totals counting every row in both views.
 *
 * 4. SoH's ITEM SLOT (ItemHintSlot) and MM's Journal decisions
 *    (ComboHintMMPaneRow): Way of the Hero / Foolish compact and coloured by the
 *    areas they name, the fixed area hints under "Area", MM item hints ranked
 *    through MM's real descriptor (the Lens of Truth is major), an OoT item
 *    ungraded, an area-less source under "Other Hints", found once every named
 *    check is collected in the MM half.
 *
 * 5. CROSS-GAME SEEN. Combo_HintMarkHostSeen reaches the OoT ops' writer for an
 *    OoT host only; Combo_HintReadHostChecks names each OoT check the READ MM
 *    hints name once (never an MM one), only over an ARMED MM half, and counts
 *    past its cap. Source scans hold the two call sites (MM's read hook, OoT's
 *    load seam) and that SoH's toggle row names this window.
 *
 * Appearance is the UiSnapshot rows' (window/Hint Tracker), not this.
 *
 * Linkage note: #included into test_runner.cpp at FILE SCOPE (compiled as C++),
 * for the C++-linkage ComboGui functions.
 */

#include "../ComboHintTrackerWindow.h"
#include "../combo_area.h"
#include "../combo_hint_view.h"
#include "../combo_tracker_view.h"
#include "../context.h"
#include "../game.h"
#include "../test_runner.h"
#include "test_named_items.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <libultraship/bridge/consolevariablebridge.h>

// games/mm/2s2h/Rando/HintSourcesSingleExe.cpp's test bridge (also declared by test_mm_hint_sources.c).
extern "C" void MM_HintSources_TestAuthorSave(void* image, uint32_t finalSeed, int hintsOn);

#define HTW_CHECK(cond)                                                    \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("[TEST] FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            return false;                                                  \
        }                                                                  \
    } while (0)

namespace {

class HintStandinWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void InitElement() override {
    }
    void DrawElement() override {
    }
    void UpdateElement() override {
    }
};

std::string HtwSource(const char* relPath) {
    std::ifstream in(std::string(RSBS_SOURCE_DIR) + "/" + relPath, std::ios::binary);
    std::string text;
    for (std::istreambuf_iterator<char> it(in), end; it != end; ++it) {
        if (*it != '\r') { // a CRLF checkout reads as the LF one
            text += *it;
        }
    }
    return text;
}

// ---- 1 and 2. Registration and inertness ------------------------------------

bool HtwRegistration() {
    auto gui = std::make_shared<Ship::Gui>();
    const char* const kNeighbours[] = { "SoH Hint Tracker", "Hint Tracker Settings", "Check Tracker" };
    std::vector<std::shared_ptr<Ship::GuiWindow>> neighbours;
    for (const char* name : kNeighbours) {
        neighbours.push_back(std::make_shared<HintStandinWindow>("", name));
        gui->AddGuiWindow(neighbours.back());
    }
    CVarClear(ComboGui::kComboHintTrackerVisibilityCVar);

    ComboGui::RegisterComboHintTrackerWindow(gui);
    auto window = gui->GetGuiWindow(ComboGui::kComboHintTrackerWindowName);
    HTW_CHECK(window != nullptr);
    HTW_CHECK(std::dynamic_pointer_cast<ComboGui::ComboHintTrackerWindow>(window) != nullptr);
    ComboGui::RegisterComboHintTrackerWindow(gui);
    HTW_CHECK(gui->GetGuiWindow(ComboGui::kComboHintTrackerWindowName) == window);
    for (size_t i = 0; i < neighbours.size(); i++) {
        HTW_CHECK(gui->GetGuiWindow(kNeighbours[i]) == neighbours[i]);
    }
    ComboGui::RegisterComboHintTrackerWindow(nullptr); // a null Gui is a no-op

    const GameId prevGame = Context_GetCurrentGame();
    for (const GameId game : { GAME_OOT, GAME_MM, GAME_NONE }) {
        Context_SetCurrentGame(game);
        window->Update();
        window->Draw(); // the cleared key: an ImGui call here aborts the process
        for (const uint8_t pane : { (uint8_t)GAME_OOT, (uint8_t)GAME_MM }) {
            std::vector<ComboGui::ComboHintRowData> rows;
            const uint8_t state = ComboGui::ComboCollectHintRows(pane, rows);
            printf("[TEST] combo-hint-tracker-window: %s running, the %s pane is state %u with %zu row(s)\n",
                   game == GAME_OOT ? "OoT" : (game == GAME_MM ? "MM" : "no game"),
                   pane == (uint8_t)GAME_OOT ? "OoT" : "MM", (unsigned)state, rows.size());
            HTW_CHECK(state <= (uint8_t)COMBO_HINT_PANE_READY);
            HTW_CHECK(state == (uint8_t)COMBO_HINT_PANE_READY || rows.empty());
        }
    }
    Context_SetCurrentGame(prevGame);
    return true;
}

// ---- 3. SoH's grouping ---------------------------------------------------------

ComboGui::ComboHintRowData HtwRow(uint16_t source, const char* name, bool read, uint32_t locKey, const char* locName,
                                  uint32_t journalKey, const char* journalName) {
    ComboGui::ComboHintRowData r;
    r.source = source;
    r.name = name;
    r.read = read;
    r.hasText = read;
    r.text = read ? std::string("text of ") + name : "";
    r.locationKey = locKey;
    r.locationName = locName;
    r.journalKey = journalKey;
    r.journalName = journalName;
    return r;
}

std::string HtwNames(const ComboGui::ComboHintGroup& g) {
    std::string s;
    for (const auto& e : g.entries) {
        s += (s.empty() ? "" : ",") + e.name;
    }
    return s;
}

bool HtwGrouping() {
    using namespace ComboGui;
    std::vector<ComboHintRowData> rows;
    // A read Way of the Hero stone, an unread item stone, both in key 5; a read
    // item stone in key 2; a read, found "Other Hints" item hint.
    rows.push_back(HtwRow(1, "Woth Stone", true, 5, "Kokiri Forest", kComboHintJournalWoth, "Way of the Hero"));
    rows.back().journalLabel = "Deku Tree";
    rows.back().journalCompact = true;
    rows.back().journalColour = COMBO_HINT_COLOUR_WOTH;
    rows.push_back(HtwRow(2, "Item Stone", false, 5, "Kokiri Forest", kComboHintJournalItem, "Major Items"));
    rows.push_back(HtwRow(3, "Zebra Stone", true, 2, "Lost Woods", kComboHintJournalItem, "Major Items"));
    rows.push_back(
        HtwRow(4, "Aardvark", true, RSBS_HINT_GROUP_OTHER, "Other Hints", kComboHintJournalItem, "Major Items"));
    rows.back().found = true;

    ComboHintPaneGroups pane;
    ComboGroupHintRows(rows, false, "", false, pane);
    HTW_CHECK(pane.total == 4 && pane.read == 3);
    HTW_CHECK(pane.groups.size() == 3);
    HTW_CHECK(pane.groups[0].key == 2 && pane.groups[1].key == 5 && pane.groups[2].key == RSBS_HINT_GROUP_OTHER);
    HTW_CHECK(pane.groups[1].read == 1 && pane.groups[1].total == 2);
    HTW_CHECK(HtwNames(pane.groups[1]) == "Item Stone,Woth Stone"); // by name; the unread one is listed, masked
    HTW_CHECK(!pane.groups[1].entries[0].read && !pane.groups[1].entries[1].compact);
    HTW_CHECK(!pane.groups[2].entries[0].found); // found is a Journal state
    HTW_CHECK(ComboHintGroupLabel(pane.groups[1], false) == "Kokiri Forest (1/2)###HintTrackerKokiri Forest");

    ComboGroupHintRows(rows, true, "", false, pane);
    HTW_CHECK(pane.total == 4 && pane.read == 3); // the totals count every row in both views
    HTW_CHECK(pane.groups.size() == 2);
    HTW_CHECK(pane.groups[0].key == kComboHintJournalWoth && pane.groups[0].entries.size() == 1);
    HTW_CHECK(pane.groups[0].entries[0].name == "Deku Tree" && pane.groups[0].entries[0].compact &&
              pane.groups[0].entries[0].colour == COMBO_HINT_COLOUR_WOTH);
    HTW_CHECK(pane.groups[1].read == 2 && pane.groups[1].total == 2); // the unread item stone is not in the Journal
    HTW_CHECK(HtwNames(pane.groups[1]) == "Zebra Stone,Aardvark");    // found last, whatever its name
    HTW_CHECK(ComboHintGroupLabel(pane.groups[1], true) == "Major Items (2)###HintTrackerMajor Items");

    ComboGroupHintRows(rows, true, "", true, pane); // Hide Found
    HTW_CHECK(pane.groups.size() == 2 && HtwNames(pane.groups[1]) == "Zebra Stone" && pane.groups[1].total == 2);

    ComboGroupHintRows(rows, true, "deku", false, pane); // an entry's (compact) name
    HTW_CHECK(pane.groups.size() == 1 && pane.groups[0].key == kComboHintJournalWoth && pane.total == 4);
    ComboGroupHintRows(rows, true, "major", false, pane); // a group's name keeps all its entries
    HTW_CHECK(pane.groups.size() == 1 && pane.groups[0].entries.size() == 2);
    ComboGroupHintRows(rows, false, "zzz", false, pane); // nothing matches: no group, the totals stand
    HTW_CHECK(pane.groups.empty() && pane.total == 4 && pane.read == 3);
    return true;
}

// ---- 4. The item slot and MM's Journal decisions -------------------------------

bool HtwItemSlot() {
    using namespace ComboGui;
    int sortRank = -1;
    HTW_CHECK(ComboHintItemSlot({}, sortRank) == COMBO_HINT_RANK_JUNK && sortRank == COMBO_HINT_RANK_JUNK + 1);
    HTW_CHECK(ComboHintItemSlot({ COMBO_HINT_RANK_UNGRADED }, sortRank) == COMBO_HINT_RANK_UNGRADED);
    HTW_CHECK(ComboHintItemSlot({ COMBO_HINT_RANK_LESSER, COMBO_HINT_RANK_UNGRADED }, sortRank) ==
                  COMBO_HINT_RANK_LESSER &&
              sortRank == COMBO_HINT_RANK_LESSER);
    HTW_CHECK(ComboHintItemSlot({ COMBO_HINT_RANK_JUNK, COMBO_HINT_RANK_MAJOR }, sortRank) == COMBO_HINT_RANK_MAJOR);
    HTW_CHECK(ComboHintItemSlot({ COMBO_HINT_RANK_JUNK, COMBO_HINT_RANK_UNGRADED }, sortRank) == COMBO_HINT_RANK_JUNK);
    HTW_CHECK(std::string(ComboHintRankGroupName(COMBO_HINT_RANK_MAJOR)) == "Major Items" &&
              std::string(ComboHintRankGroupName(COMBO_HINT_RANK_UNGRADED)) == "Items");
    return true;
}

ComboHintRow HtwMMRow(uint16_t source, uint8_t type, ComboArea area) {
    ComboHintRow row;
    std::memset(&row, 0, sizeof(row));
    row.info.source = source;
    row.info.name = "MM Source";
    row.info.area = area;
    row.areaName = Combo_Area_Name(area);
    row.read = true;
    row.known = true;
    row.hint.type = type;
    snprintf(row.display, sizeof(row.display), "%s", "plain text");
    return row;
}

bool HtwMMJournal() {
    using namespace ComboGui;
    const ComboArea zora = { (uint8_t)GAME_MM, (uint8_t)COMBO_AREA_MM_ZORA_HALL };
    const ComboArea village = { (uint8_t)GAME_MM, (uint8_t)COMBO_AREA_MM_MOUNTAIN_VILLAGE };
    HTW_CHECK(Combo_Area_Name(zora) != nullptr && Combo_Area_Name(village) != nullptr);

    ComboHintRow woth = HtwMMRow(0, COMBO_HINT_TYPE_WOTH, zora);
    (void)Combo_HintResolutionAddArea(&woth.hint, zora);
    (void)Combo_HintResolutionAddArea(&woth.hint, village);
    ComboHintRowData d = ComboHintMMPaneRow(woth);
    HTW_CHECK(d.journalKey == kComboHintJournalWoth && d.journalCompact && d.journalColour == COMBO_HINT_COLOUR_WOTH);
    HTW_CHECK(d.journalLabel == std::string(Combo_Area_Name(zora)) + ", " + Combo_Area_Name(village));
    HTW_CHECK(d.locationKey == zora.area && d.locationName == Combo_Area_Name(zora) && d.text == "plain text");

    ComboHintRow foolish = HtwMMRow(1, COMBO_HINT_TYPE_FOOLISH, zora);
    d = ComboHintMMPaneRow(foolish); // no area named: not compact, so no colour (SoH's rule)
    HTW_CHECK(d.journalKey == kComboHintJournalFoolish && !d.journalCompact &&
              d.journalColour == COMBO_HINT_COLOUR_NONE);

    d = ComboHintMMPaneRow(HtwMMRow(RSBS_MM_HINT_SRC_OATH, COMBO_HINT_TYPE_FIXED, village));
    HTW_CHECK(d.journalKey == kComboHintJournalArea && d.journalName == "Area");
    HTW_CHECK(ComboHintMMFixedNamesArea(RSBS_MM_HINT_SRC_GOLD_DUST) &&
              !ComboHintMMFixedNamesArea(RSBS_MM_HINT_SRC_GOLD_DUST_PRIZE));

    // MM's real descriptor grades MM's items (MM_HintAdapter_Register, through Combo_TrackerWindow_Init).
    SharedItem lens;
    HTW_CHECK(TestNamedItem((uint8_t)GAME_MM, "Lens of Truth", &lens));
    const ComboMMHintDesc* desc = Combo_Hint_GetMMDesc();
    HTW_CHECK(desc != nullptr && desc->itemRank != nullptr);
    HTW_CHECK(desc->itemRank((uint8_t)GAME_MM, lens.id) == COMBO_HINT_RANK_MAJOR);
    HTW_CHECK(desc->itemRank((uint8_t)GAME_OOT, lens.id) == COMBO_HINT_RANK_UNGRADED);

    ComboHintRow item = HtwMMRow(RSBS_MM_HINT_SRC_SPIDER_HOUSE_SWAMP, COMBO_HINT_TYPE_FIXED, village);
    ComboHintTarget t = {};
    t.checkGame = (uint8_t)GAME_NONE;
    t.itemGame = (uint8_t)GAME_MM;
    t.item = lens.id;
    (void)Combo_HintResolutionAddTarget(&item.hint, &t);
    d = ComboHintMMPaneRow(item);
    HTW_CHECK(d.journalKey == kComboHintJournalItem + COMBO_HINT_RANK_MAJOR && d.journalName == "Major Items");
    HTW_CHECK(d.sortRank == COMBO_HINT_RANK_MAJOR && !d.found); // a check named nowhere is never found
    item.hint.targets[0].itemGame = (uint8_t)GAME_OOT;
    d = ComboHintMMPaneRow(item);
    HTW_CHECK(d.journalKey == kComboHintJournalItem + COMBO_HINT_RANK_UNGRADED && d.journalName == "Items");

    ComboHintRow nowhere = HtwMMRow(2, COMBO_HINT_TYPE_ITEM, ComboArea{ (uint8_t)GAME_MM, (uint8_t)COMBO_AREA_NONE });
    nowhere.read = false;
    nowhere.known = false;
    d = ComboHintMMPaneRow(nowhere);
    HTW_CHECK(d.locationKey == RSBS_HINT_GROUP_OTHER && d.locationName == "Other Hints" && !d.hasText);
    return true;
}

// ---- 5. Cross-game Seen --------------------------------------------------------

std::vector<uint16_t> gHtwMarked;

bool HtwFakeMark(uint16_t check) {
    gHtwMarked.push_back(check);
    return true;
}
uint8_t HtwFakeCollect(int* outCount) {
    *outCount = 2;
    return COMBO_HINT_PANE_READY;
}
bool HtwFakeRowAt(int index, ComboHintPaneRow* out) {
    if (index < 0 || index > 1) {
        return false;
    }
    out->source = (uint16_t)(100 + index);
    out->name = index == 0 ? "Stone A" : "Stone B";
    out->read = index == 0;
    out->text = index == 0 ? "says A" : nullptr;
    out->locationKey = 7;
    out->locationName = "Area Seven";
    return true;
}
uint8_t HtwFakeCollectNotRando(int* outCount) {
    *outCount = 5; // a pane that is not READY lists nothing, whatever the count says
    return COMBO_HINT_PANE_NOT_RANDO;
}

bool HtwFakeInfo(uint16_t source, ComboHintSourceInfo* out) {
    if (source >= RSBS_MM_HINT_SRC_COUNT) {
        return false;
    }
    std::memset(out, 0, sizeof(*out));
    out->source = source;
    out->kind = COMBO_HINT_SOURCE_GOSSIP_STONE;
    out->name = "Fake Stone";
    return true;
}
bool HtwFakeListed(const void*, uint16_t) {
    return true;
}
// Source 3 names OoT 0x0101 and MM 0x0005; source 4 names OoT 0x0101 again and OoT 0x0202.
bool HtwFakeResolve(const void*, uint16_t source, ComboHintResolution* out) {
    Combo_HintResolutionClear(out);
    if (source != 3 && source != 4) {
        return false;
    }
    out->type = COMBO_HINT_TYPE_ITEM;
    ComboHintTarget a = {};
    a.checkGame = (uint8_t)GAME_OOT;
    a.check = 0x0101;
    ComboHintTarget b = {};
    b.checkGame = source == 3 ? (uint8_t)GAME_MM : (uint8_t)GAME_OOT;
    b.check = source == 3 ? 0x0005 : 0x0202;
    (void)Combo_HintResolutionAddTarget(out, &a);
    (void)Combo_HintResolutionAddTarget(out, &b);
    return true;
}

bool HtwCrossGameLegs() {
    using namespace ComboGui;
    // a. The OoT writer, reached for an OoT host only.
    static const ComboOoTHintOps kFake = { HtwFakeCollect, HtwFakeRowAt, HtwFakeMark };
    static const ComboOoTHintOps kNoWriter = { HtwFakeCollect, HtwFakeRowAt, nullptr };
    static const ComboOoTHintOps kNotRando = { HtwFakeCollectNotRando, HtwFakeRowAt, nullptr };
    gHtwMarked.clear();
    Combo_Hint_RegisterOoT(&kFake);
    HTW_CHECK(Combo_Hint_GetOoTOps() == &kFake);
    HTW_CHECK(Combo_HintMarkHostSeen((uint8_t)GAME_OOT, 0x0123) && gHtwMarked.size() == 1 && gHtwMarked[0] == 0x0123);
    HTW_CHECK(!Combo_HintMarkHostSeen((uint8_t)GAME_MM, 0x0123) && gHtwMarked.size() == 1);

    // The OoT pane copies the adapter's rows, a NULL text as none.
    std::vector<ComboHintRowData> rows;
    HTW_CHECK(ComboCollectHintRows((uint8_t)GAME_OOT, rows) == COMBO_HINT_PANE_READY && rows.size() == 2);
    HTW_CHECK(rows[0].hasText && rows[0].text == "says A" && !rows[1].hasText && rows[1].name == "Stone B");
    Combo_Hint_RegisterOoT(&kNotRando);
    HTW_CHECK(ComboCollectHintRows((uint8_t)GAME_OOT, rows) == COMBO_HINT_PANE_NOT_RANDO && rows.empty());
    Combo_Hint_RegisterOoT(&kNoWriter);
    HTW_CHECK(!Combo_HintMarkHostSeen((uint8_t)GAME_OOT, 0x0123) && gHtwMarked.size() == 1);
    Combo_Hint_RegisterOoT(nullptr);
    HTW_CHECK(!Combo_HintMarkHostSeen((uint8_t)GAME_OOT, 0x0123));

    // b. The hosts the read MM hints name, over an authored MM half.
    ComboMMHintDesc fake = {};
    fake.sourceCount = (uint16_t)RSBS_MM_HINT_SRC_COUNT;
    fake.sourceInfo = HtwFakeInfo;
    fake.listed = HtwFakeListed;
    fake.resolve = HtwFakeResolve;
    Combo_Hint_RegisterMM(&fake);
    const ComboMMTrackerDesc* tdesc = Combo_Tracker_GetMMDesc();
    HTW_CHECK(tdesc != nullptr);
    Context_InitFrozenStates();
    Context_ClearFrozenState(GAME_MM); // not armed
    std::vector<uint8_t> half((size_t)MM_SAVE_CONTEXT_SIZE, 0);
    MM_HintSources_TestAuthorSave(half.data(), 0x14751475u, 1);
    Context_UpdateShadowCopy(GAME_MM, half.data(), half.size());
    Context_SetCurrentGame(GAME_OOT);
    ComboContext_Init();
    uint16_t hosts[RSBS_HINT_HOST_CAP];
    (void)Combo_HintMarkRead((uint8_t)GAME_MM, 3);
    HTW_CHECK(Combo_HintReadHostChecks((uint8_t)GAME_OOT, hosts, (int)RSBS_HINT_HOST_CAP) == 0); // not armed
    HTW_CHECK(Context_ArmShadowAsFrozen(GAME_MM, 0) != 0);
    HTW_CHECK(Combo_HintReadHostChecks((uint8_t)GAME_OOT, hosts, (int)RSBS_HINT_HOST_CAP) == 1 && hosts[0] == 0x0101);
    (void)Combo_HintMarkRead((uint8_t)GAME_MM, 4);
    HTW_CHECK(Combo_HintReadHostChecks((uint8_t)GAME_OOT, hosts, (int)RSBS_HINT_HOST_CAP) == 2 && hosts[0] == 0x0101 &&
              hosts[1] == 0x0202);
    hosts[1] = 0xFFFF;
    HTW_CHECK(Combo_HintReadHostChecks((uint8_t)GAME_OOT, hosts, 1) == 2 && hosts[1] == 0xFFFF); // capped writes
    HTW_CHECK(Combo_HintReadHostChecks((uint8_t)GAME_MM, hosts, (int)RSBS_HINT_HOST_CAP) == 0);
    ComboContext_Init(); // no source read: no host
    HTW_CHECK(Combo_HintReadHostChecks((uint8_t)GAME_OOT, hosts, (int)RSBS_HINT_HOST_CAP) == 0);

    // c. An MM item hint is found once its MM check is collected in that half.
    MM_HintAdapter_Register(); // MM's real descriptor again: its itemRank
    SharedItem lens;
    HTW_CHECK(TestNamedItem((uint8_t)GAME_MM, "Lens of Truth", &lens));
    const uint16_t check = 5;
    HTW_CHECK(check < tdesc->checkCount);
    ComboHintRow row = HtwMMRow(RSBS_MM_HINT_SRC_SPIDER_HOUSE_SWAMP, COMBO_HINT_TYPE_FIXED,
                                ComboArea{ (uint8_t)GAME_MM, (uint8_t)COMBO_AREA_MM_ZORA_HALL });
    ComboHintTarget t = {};
    t.checkGame = (uint8_t)GAME_MM;
    t.check = check;
    t.itemGame = (uint8_t)GAME_MM;
    t.item = lens.id;
    (void)Combo_HintResolutionAddTarget(&row.hint, &t);
    HTW_CHECK(!ComboHintMMPaneRow(row).found);
    half[tdesc->checkTableOffset + (size_t)check * tdesc->checkStride + tdesc->obtainedOffset] = 1;
    Context_UpdateShadowCopy(GAME_MM, half.data(), half.size());
    HTW_CHECK(ComboHintMMPaneRow(row).found);
    return true;
}

bool HtwSourceScans() {
    const std::string mm = HtwSource("games/mm/2s2h/Rando/HintSourcesSingleExe.cpp");
    const size_t markRead = mm.find("void Rando::HintSources::MarkRead(uint16_t source) {");
    HTW_CHECK(markRead != std::string::npos);
    const size_t markEnd = mm.find("\n}\n", markRead);
    HTW_CHECK(markEnd != std::string::npos);
    HTW_CHECK(mm.substr(markRead, markEnd - markRead).find("Combo_HintMarkHostSeen((uint8_t)GAME_OOT, t.check)") !=
              std::string::npos);
    HTW_CHECK(mm.find("desc.itemRank = MMHintItemRank;") != std::string::npos);

    const std::string save = HtwSource("games/oot/soh/SaveManager.cpp");
    const size_t replay = save.find("        OoT_TrackerAdapter_ReplaySkipJournal();");
    HTW_CHECK(replay != std::string::npos);
    const size_t apply = save.find("        OoT_TrackerAdapter_ApplyMMHintsSeen();", replay);
    HTW_CHECK(apply != std::string::npos && save.find("\n    }\n", replay) > apply);

    // SoH's own window is retired, never unregistered: its InitElement feeds the OoT pane.
    std::string gui;
    for (const char c : HtwSource("games/oot/soh/SohGui/SohGui.cpp")) {
        if (c != ' ' && c != '\n' && c != '\t') {
            gui += c;
        }
    }
    HTW_CHECK(gui.find("std::make_shared<OoTActiveGated<RetiredInSingleExe<HintTracker::HintTrackerWindow>>>(SOH_"
                       "RETIRED_HINT_TRACKER_KEY,kSohRetiredHintTrackerWindowName,ImVec2(500,600));") !=
              std::string::npos);
    HTW_CHECK(gui.find("gui->AddGuiWindow(mHintTrackerWindow);") != std::string::npos);

    // SoH's Toggle Hint Tracker row opens its window by name and labels itself from the key this window reads.
    const std::string menu = HtwSource("games/oot/soh/SohGui/SohMenuRandomizer.cpp");
    const size_t toggle = menu.find("\"Toggle Hint Tracker\", WIDGET_WINDOW_BUTTON)");
    HTW_CHECK(toggle != std::string::npos);
    const std::string row = menu.substr(toggle, 400);
    HTW_CHECK(row.find(std::string(".WindowName(\"") + ComboGui::kComboHintTrackerWindowName + "\")") !=
              std::string::npos);
    HTW_CHECK(row.find(".CVar(CVAR_WINDOW(\"HintTracker\"))") != std::string::npos &&
              std::string(ComboGui::kComboHintTrackerVisibilityCVar) == "gOpenWindows.HintTracker");
    return true;
}

} // namespace

extern "C" int Combo_HintTrackerWindow_RunHeadless(void) {
    printf("[TEST] combo-hint-tracker-window: the combined Hint Tracker registers under SoH's name, is inert under "
           "every game, groups as SoH does, and marks cross-game hosts Seen (#1475)\n");
    // Production bring-up, headless-safe: every adapter, the hint ones included, and no Gui.
    Combo_TrackerWindow_Init();

    const GameId prevGame = Context_GetCurrentGame();
    std::vector<uint8_t> priorShadow((size_t)MM_SAVE_CONTEXT_SIZE);
    Context_InitFrozenStates();
    std::memcpy(priorShadow.data(), Context_GetMMSaveContext(), priorShadow.size());
    const bool priorArmed = Context_HasFrozenState(GAME_MM) != 0;
    const uint16_t priorEntrance = Context_GetFrozenReturnEntrance(GAME_MM);

    bool ok = HtwRegistration() && HtwGrouping() && HtwItemSlot() && HtwMMJournal();
    ok = ok && HtwCrossGameLegs();
    ok = ok && HtwSourceScans();

    // Leave the process as it was: the production adapters, the MM half, no read record.
    OoT_HintAdapter_Register();
    MM_HintAdapter_Register();
    Context_ClearFrozenState(GAME_MM);
    Context_UpdateShadowCopy(GAME_MM, priorShadow.data(), priorShadow.size());
    if (priorArmed) {
        (void)Context_ArmShadowAsFrozen(GAME_MM, priorEntrance);
    }
    ComboContext_Init();
    Context_SetCurrentGame(prevGame);
    printf("[TEST] combo-hint-tracker-window: %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
