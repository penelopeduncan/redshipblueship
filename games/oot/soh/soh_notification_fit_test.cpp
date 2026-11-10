/**
 * @file soh_notification_fit_test.cpp
 * @brief Display-free lock: every cross-game refusal toast keeps the overlay's
 *        margin on both sides of libultraship's default 640 x 480 window and of
 *        the smallest window the ui tier renders, drawn by SoH's OWN notification
 *        overlay in the real menu font, at the default Notifications.Size (1.8)
 *        and at 1.0; and at the X-Large menu scale in a 4K window.
 *
 * CTest label "redship", row PairingRefusalToastFit in CMake/SingleExecutable.cmake,
 * dispatch "pairing-refusal-toast-fit" in src/common/test_runner.cpp.
 *
 * WHY. The "Cross-game pairing REFUSED:" toasts ran off the screen (the #749
 * playtest notes): SoH's overlay (Notification::Window::Draw,
 * soh/Notification/Notification.cpp) draws the prefix and the message on ONE line
 * and never wraps, and the MM-options refusal's sentence pair drew a toast about
 * 2,480 px wide. The copy is now one short line (src/common/pairing_refusal_toast.h);
 * this row is what keeps it one.
 *
 * HOW THE WIDTH IS DERIVED, not guessed: nothing here restates a pixel of the
 * overlay's layout. The row builds a private ImGui context whose default font is
 * the one SoH makes default (OTRGlobals.cpp: fontStandardLarger =
 * Montserrat-Regular.ttf at 20 px, loaded from the same file soh.o2r packs), sets
 * the display to a profile, emits each toast through its PRODUCTION emitter, and
 * calls SoH's Notification::Window::Draw itself for a few frames. The toast
 * window's position and size are then read back exactly as the ui tier's TOAST
 * oracle reads them (every active "notification#" window). Draw right-aligns a
 * toast a fixed margin in from the right edge and grows it leftwards, so the
 * right margin is MEASURED (display width less the window's right edge), and the
 * toast must keep the same margin on the left: a toast that does not crowds the
 * left edge, which no SoH toast does ("Game autosaved"), and one wider still runs
 * off it, which is the defect.
 *
 * PROFILES:
 *   - 640 x 480 at FontGlobalScale 1.0 (#906): the window a config with no size
 *     opens (libultraship's default), the real minimum. At size 1.8 the overlay
 *     draws a toast whose copy is wider than the window at the largest size, no
 *     smaller than 1.0, at which it fits; the row requires the margin on both
 *     sides and prints how many it drew smaller;
 *   - 832 x 600 at FontGlobalScale 1.0: the ui tier's smallest profile
 *     (soh_ui_snapshot.cpp, "min-832x600") at SoH's default menu scale, the copy
 *     contract: here and wider every toast is drawn at the player's own size;
 *   - 3840 x 2160 at FontGlobalScale 2.0: a 4K window at SoH's X-Large menu
 *     scale, applied the way OTRGlobals::ScaleImGui applies it (style sizes
 *     scaled, FontGlobalScale set), which multiplies every toast's font.
 * Each at Notifications.Size 1.8 (the default and the playtest scale) and 1.0.
 *
 * WHAT IT COVERS, in every profile and size:
 *   - the arrival's and the MM spoiler's refusals through
 *     MM_Rando_EmitPairingRefusalToast: the RULES refusal over EVERY non-empty
 *     combination of the divergence bits (their count is DERIVED from
 *     Combo_ComboSettingsDivergenceFieldName, so a new bit is covered the day it
 *     is named), and the SPOILER refusal over every route the MM loader reports
 *     plus an unknown one;
 *   - the refused file's toasts (#836): the file select's refusal through
 *     RsbsSave_EmitFileSelectRefusalToast over every entry of the words table
 *     (Combo_RefusalWords), and the load's refusals through RsbsSave_EmitLoadToast;
 *   - that every one of those refusals opens with the class's ONE prefix,
 *     RSBS_REFUSAL_TOAST_PREFIX, and that its message is capitalized and has no
 *     trailing period (#836, operator ruling 2026-10-01);
 *   - OoT's paired-spoiler refusal through OoT_EmitPairedSpoilerRefusalToast
 *     (outside the class: "Spoiler not loaded:", width only);
 *   - #564 V18: MM's save-failure toast through MM_Combo_EmitSaveFailureToast,
 *     every cause (outside the class: "Save failed:", width only);
 *   - that a one-field RULES refusal names its field, and that the two-field
 *     refusal mm-combo-settings-gate leg 4 depends on names both;
 *   - #865: the pickup toast OoT shows for an MM item found at an OoT check, through
 *     its production emitter (OoT_Rando_Foreign_EmitPickupToast), for EVERY MM
 *     item that may cross: it reads "You found <article><name> (MM)", and every
 *     one of them fits like a refusal toast (#906: not only the names OoT also
 *     uses);
 *   - the creation toasts the ui tier draws (its toast/creation-* pages), each
 *     through its production emitter: the failure ("Not created:", PR #749, also
 *     printed as the calibration: #749 measured it at 800 px in the 832-px
 *     window), the shortfall ("Fewer cross-game items:") and the three goal
 *     warnings ("Not proven:"); and Generate's OoT Logic pin ("No Logic:",
 *     #1350), which has no ui page yet. Width and the copy contract, like any
 *     toast;
 *   - #881: the paired-file load's "Restored from file:" and "Restored for
 *     Majora's Mask:" toasts through RsbsSave_EmitLoadToast, at 640 x 480 (the
 *     window a config with no size opens: libultraship's default) and 832 x 600,
 *     both at menu scale 1.0, over every rule and MM option or trick label a real
 *     load names, alone and all at once: each keeps the margin on both sides, and
 *     the #881 case ("Starting Hearts +1") is not cut where it fits whole. #906:
 *     each is drawn at the player's own size in BOTH windows, because #904's
 *     name-dropping fit runs first; only its floor, a bare count ("37 settings",
 *     every settable MM trick at 640 px and 1.8), may be drawn smaller there.
 *     #1479: a Cross-Game Rule's name is never cut. Every "Restored from file:"
 *     message is whole labels from its input, in order, then " +N" for the rest
 *     of the true count (never "...", never a bare count), over inputs that
 *     include what the load's RSBS_LOAD_RULES_NAMES_LEN buffer carries for a
 *     long restore (the fifteen mask rows, every rule) with the restore's true
 *     count; that floor, one whole name and its "+N", may be drawn smaller in the
 *     640-px window, never below 1.0.
 * That the production refusal SITES call these emitters is locked where each
 * site is driven: mm-combo-settings-gate (MM options, rules, missing half),
 * mm-spoiler-identity (spoiler) and combo-creation-event (OoT spoiler) compare
 * the toast each site queued with the emitter's copy, exactly.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "soh/Notification/Notification.h"
#include "soh/OTRGlobals.h"
#include "soh/cvar_prefixes.h"
#include <libultraship/libultraship.h>

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "combo_logic.h"           // RSBS_COMBO_HALF_* (the creation goal-warning toasts, #906)
#include "combo_mm_options_view.h" // Combo_MMOptionAt: the labels the load's MM toast names (#881)
#include "combo_mm_tricks_view.h"
#include "combo_settings_view.h" // Combo_ComboSettingLabel: the labels the load's rules toast names
#include "combo_single_bag.h"    // #1483: the last report the file select's home-capacity toast reads
#include "foreign_items.h"       // Combo_ComboSettingsDivergenceDescribe / FieldName, RSBS_COMBO_DIVERGE_*
#include "notification_bridge.h"
#include "pairing_refusal_toast.h"
#include "save.h"         // RsbsSave_EmitLoadToast, RsbsSave_EmitFileSelectRefusalToast
#include "shared_items.h" // ComboItemClassRow, RSBS_FILL_CLASS_PROGRESSION (#865's pickup toasts)

#include <cctype>
#include <utility>

extern "C" void OoT_Creation_ReportFailureAtFileSelect(int slot, int reason);
extern "C" void OoT_Creation_EmitShortfallToast(int placed, int requested);
extern "C" void OoT_Creation_EmitGoalWarningToast(uint32_t unprovedHalves);
// #1350: Generate's toast for OoT's No Logic generated Glitchless (ForeignItemsSingleExe.cpp).
extern "C" void OoT_Creation_EmitLogicPinToast(void);
// #1483: Generate's home-capacity refusal and the goal refusal (ForeignItemsSingleExe.cpp).
extern "C" int OoT_Creation_EmitHomeCapacityToast(uint32_t shortGames, unsigned piecesOoT, unsigned piecesMM);
extern "C" void OoT_Creation_EmitGoalUnreachableToast(unsigned goal, int goalOoT, int goalMM);
// #865: the pickup toast for an MM item found at an OoT check (hook_handlers.cpp),
// and the MM items that may cross (MM's classification and its id space).
extern "C" void OoT_Rando_Foreign_EmitPickupToast(SharedItem item);
extern "C" int MM_ComboLogic_ClassifyItem(uint16_t id, ComboItemClassRow* out);
extern "C" int MM_ForeignItem_TestItemIdMax(void);
// OoT's item table, for the names OoT also uses (ComboLogicEngineOoT.cpp): this
// row never runs the OTR bring-up that fills it, and the transient form leaves no
// Rando::Context behind for the rows after it under `--test all`.
extern "C" int OoT_ComboLogic_TestEnsureItemTableTransient(void);

namespace {

// SoH's default font (OTRGlobals.cpp: fontStandardLarger, ImGui::GetIO().FontDefault).
constexpr float kDefaultFontPixels = 20.0f;

struct Profile {
    const char* name;
    float width;
    float height;
    // OTRGlobals.cpp imguiScaleOptionToValue: 1.0 is the default, 2.0 X-Large.
    float menuScale;
};

constexpr Profile kProfiles[] = {
    // #906: the window a config with no size opens (libultraship's default,
    // gfx_sdl2.cpp Window.Width 640), the real minimum.
    { "640x480 at menu scale 1.0", 640.0f, 480.0f, 1.0f },
    // The ui tier's smallest profile (soh_ui_snapshot.cpp, ChooseProfile: "min-832x600").
    { "832x600 at menu scale 1.0", 832.0f, 600.0f, 1.0f },
    // A 4K window at SoH's X-Large menu scale.
    { "3840x2160 at menu scale 2.0", 3840.0f, 2160.0f, 2.0f },
};

int Fail(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[TEST] FAIL: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
    return 1;
}

struct Measured {
    int windows = 0;
    float x0 = 0.0f;
    float x1 = 0.0f;
    // #906: the font scale the overlay drew the toast at (its window's FontWindowScale).
    float fontScale = 0.0f;
    std::string prefix;
    std::string message;
    std::string suffix;
};

/** Draw the queued toasts through SoH's own overlay and read the windows back. */
Measured DrawAndMeasure(Notification::Window& overlay) {
    // Three frames: an AlwaysAutoResize window measures its content on the frame
    // it first appears and Draw positions it from GetWindowSize(), so the settled
    // rectangle is the one after the first frame (the ui tier's Settle does the same).
    for (int frame = 0; frame < 3; frame++) {
        ImGui::NewFrame();
        overlay.Draw();
        ImGui::EndFrame();
    }
    Measured m;
    m.x0 = 1e9f;
    m.x1 = -1e9f;
    for (ImGuiWindow* w : GImGui->Windows) {
        if (w == nullptr || !w->Active || std::string(w->Name).rfind("notification#", 0) != 0) {
            continue;
        }
        m.windows++;
        m.x0 = std::min(m.x0, w->Pos.x);
        m.x1 = std::max(m.x1, w->Pos.x + w->Size.x);
        m.fontScale = m.windows == 1 ? w->FontWindowScale : std::min(m.fontScale, w->FontWindowScale);
    }
    ComboNotification last;
    memset(&last, 0, sizeof(last));
    if (OoT_Notification_PeekLastForTest(&last) == 1) {
        m.prefix = last.prefix != nullptr ? last.prefix : "";
        m.message = last.message != nullptr ? last.message : "";
        m.suffix = last.suffix != nullptr ? last.suffix : "";
    }
    return m;
}

struct FitRun {
    Notification::Window* overlay = nullptr;
    const Profile* profile = nullptr;
    int checked = 0;
    float widest = 0.0f;
    std::string widestText;
    // #906: the toasts the overlay drew below the player's size to fit the window.
    int shrunk = 0;
    float smallest = 0.0f;
    std::string smallestText;
    // #906: the load's "Restored ..." toasts carry their own fit (#904 drops
    // names until the toast fits the window at the player's size), so the
    // overlay may draw one smaller only once that fit has reached its floor, the
    // bare count ("37 settings"), which has nothing left to drop, or (#1479) the
    // rules toast's floor, one whole rule name and its "+N".
    bool fullSizeOnly = false;
    int floorShrunk = 0;
};

/**
 * #906: whether a "Restored ...: <message>" toast's message is #904's floor, a
 * bare count with no name ("2 settings", "37 settings", "1 rule"): FitNames'
 * last step, which FitLoadToast returns when no name fits the window.
 */
bool IsBareCount(const std::string& text) {
    const std::size_t colon = text.rfind(": ");
    const std::string message = colon == std::string::npos ? text : text.substr(colon + 2);
    std::size_t digits = 0;
    while (digits < message.size() && std::isdigit(static_cast<unsigned char>(message[digits]))) {
        digits++;
    }
    if (digits == 0) {
        return false;
    }
    const std::string word = message.substr(digits);
    return word == " rule" || word == " rules" || word == " setting" || word == " settings";
}

// The rules toast's prefix (save.cpp, RsbsSave_EmitLoadToast).
const char* const kRulesRestoredPrefix = "Restored from file:";

/**
 * #1479: whether a toast is the rules toast's floor, ONE whole rule name and its
 * "+N" ("Restored from file: Shared Nocturne of Shadow +1"): a rule name is never
 * cut, so this has nothing left to drop and the overlay may draw it smaller.
 * That the name is whole and the count true is CheckRulesMessage's.
 */
bool IsRulesNameFloor(const std::string& text) {
    const std::string prefix = std::string(kRulesRestoredPrefix) + " ";
    if (text.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }
    const std::string message = text.substr(prefix.size());
    return !message.empty() && message.find(", ") == std::string::npos && message.find("...") == std::string::npos;
}

// #906: the copy contract. In the ui tier's smallest window and wider, every
// toast's copy fits at the player's own size, so the overlay's fit never draws
// one smaller there. Only a narrower window (libultraship's 640-px default) at a
// size above 1.0 may draw a toast smaller, and never below 1.0 (the menus' own
// text size).
constexpr float kCopyContractWidth = 832.0f;

/** #906: the toast was drawn at full size where the copy contract holds, and otherwise no smaller than 1.0. */
int CheckFontScale(FitRun& run, const char* what, float scale, float fontScale, const std::string& text) {
    const Profile& p = *run.profile;
    if (fontScale >= scale - 0.001f) {
        return 0;
    }
    // Counted before any verdict, so the row's "drawn below the player's size"
    // line names every such toast, the failing ones too.
    run.shrunk++;
    if (run.smallest == 0.0f || fontScale < run.smallest) {
        run.smallest = fontScale;
        run.smallestText = text;
    }
    if (run.fullSizeOnly && !IsBareCount(text) && !IsRulesNameFloor(text)) {
        return Fail("%s at %.1fx, %s: the overlay drew it at %.2fx to fit; the load toast's own fit (#904) must "
                    "drop names until it fits at the player's size: '%s'",
                    what, scale, p.name, fontScale, text.c_str());
    }
    if (run.fullSizeOnly) {
        run.floorShrunk++;
    }
    const bool mayShrink = p.width < kCopyContractWidth && scale > 1.0f;
    if (!mayShrink) {
        return Fail("%s at %.1fx, %s: the overlay drew it at %.2fx to fit; its copy must fit here at the player's "
                    "size: '%s'",
                    what, scale, p.name, fontScale, text.c_str());
    }
    if (fontScale < 1.0f - 0.001f) {
        return Fail("%s at %.1fx, %s: the overlay drew it at %.2fx, below the menus' own 1.0: '%s'", what, scale,
                    p.name, fontScale, text.c_str());
    }
    return 0;
}

/** Whether a toast belongs to the refusal class, whose prefix and copy shape are checked too. */
enum class Class { REFUSAL, OTHER };

/** One toast, already emitted, at the current Notifications.Size. */
int CheckOne(FitRun& run, const char* what, float scale, Class cls = Class::REFUSAL) {
    const Measured m = DrawAndMeasure(*run.overlay);
    OoT_Notification_ClearForTest();
    const Profile& p = *run.profile;
    if (m.windows != 1) {
        return Fail("%s at %.1fx, %s: %d toast window(s) drawn, expected exactly one", what, scale, p.name, m.windows);
    }
    if (cls == Class::REFUSAL) {
        // #836 (operator ruling 2026-10-01): ONE prefix for the whole class, and
        // a message that is capitalized and has no trailing period, so the Combo
        // > Save Files status cell is the toast's own line.
        if (m.prefix != RSBS_REFUSAL_TOAST_PREFIX) {
            return Fail("%s: the toast's prefix is '%s', not the refusal class's one prefix '%s' (message '%s')", what,
                        m.prefix.c_str(), RSBS_REFUSAL_TOAST_PREFIX, m.message.c_str());
        }
        if (m.message.empty() || std::islower(static_cast<unsigned char>(m.message.front())) ||
            m.message.back() == '.') {
            return Fail("%s: the message '%s' is not capitalized without a trailing period", what, m.message.c_str());
        }
    }
    if (m.message.find('\n') != std::string::npos || m.prefix.find('\n') != std::string::npos) {
        return Fail("%s: the toast carries a line break ('%s %s'); SoH's toasts are one line", what, m.prefix.c_str(),
                    m.message.c_str());
    }
    const float width = m.x1 - m.x0;
    // The overlay's right margin, measured: Draw anchors the window's right edge
    // there. A toast must keep the same margin on the left.
    const float margin = p.width - m.x1;
    if (margin <= 0.0f) {
        return Fail("%s at %.1fx, %s: the toast's right edge is at %.0f, not inside the %.0f-px window", what, scale,
                    p.name, m.x1, p.width);
    }
    if (m.x0 < margin) {
        return Fail("%s at %.1fx, %s: the toast leaves %.0f px on the left against the overlay's %.0f-px right "
                    "margin (x %.0f to %.0f, %.0f px wide, at most %.0f): '%s %s'",
                    what, scale, p.name, m.x0, margin, m.x0, m.x1, width, p.width - 2.0f * margin, m.prefix.c_str(),
                    m.message.c_str());
    }
    if (CheckFontScale(run, what, scale, m.fontScale, m.prefix + " " + m.message) != 0) {
        return 1;
    }
    run.checked++;
    if (scale > 1.5f && width > run.widest) {
        run.widest = width;
        run.widestText = m.prefix + " " + m.message;
    }
    return 0;
}

/**
 * #865: the pickup toast OoT shows for an MM item found at an OoT check, drawn
 * through its production emitter for EVERY MM item that may cross (MM's
 * PROGRESSION class): its words name the item marked " (MM)", and the toast of
 * every name both games use (the Hookshot, the Song of Time, ...), which is what
 * the marker is for, keeps the overlay's margin on both sides.
 */
int CheckPickupToasts(FitRun& run, float scale) {
    const Profile& p = *run.profile;
    const int idMax = MM_ForeignItem_TestItemIdMax();
    int failures = 0;
    if (OoT_ComboLogic_TestEnsureItemTableTransient() != 0) {
        return Fail("OoT's item table could not be brought up, so no name can be compared with OoT's");
    }
    int swept = 0;
    int shared = 0;
    int otherOverflows = 0;
    float widestShared = 0.0f;
    std::string widestSharedText;
    float widest = 0.0f;
    std::string widestText;
    for (int id = 1; id < idMax; id++) {
        ComboItemClassRow row;
        if (MM_ComboLogic_ClassifyItem((uint16_t)id, &row) != 1 || row.fillClass != RSBS_FILL_CLASS_PROGRESSION) {
            continue;
        }
        SharedItem item;
        item.originGame = (uint8_t)GAME_MM;
        item.flags = 0;
        item.id = (uint16_t)id;
        const char* name = Combo_GetForeignItemName(item);
        const char* article = Combo_GetForeignItemArticle(item);
        if (name == nullptr || article == nullptr) {
            failures += Fail("MM item %d has no name or article for its pickup toast", id);
            continue;
        }
        OoT_Rando_Foreign_EmitPickupToast(item);
        const Measured m = DrawAndMeasure(*run.overlay);
        OoT_Notification_ClearForTest();
        const std::string text = m.prefix + m.message + m.suffix;
        const std::string want = std::string("You found ") + article + name + " (MM)";
        if (text != want) {
            failures += Fail("the pickup toast for MM item %d reads '%s', want '%s'", id, text.c_str(), want.c_str());
            continue;
        }
        if (m.windows != 1) {
            failures += Fail("the pickup toast '%s' drew %d windows, expected exactly one", text.c_str(), m.windows);
            continue;
        }
        swept++;
        const float width = m.x1 - m.x0;
        const float margin = p.width - m.x1;
        const bool fits = margin > 0.0f && m.x0 >= margin;
        const bool isShared = Combo_GetForeignItemByNameFor((uint8_t)GAME_OOT, name, nullptr);
        if (width > widest) {
            widest = width;
            widestText = text;
        }
        if (isShared) {
            shared++;
            if (width > widestShared) {
                widestShared = width;
                widestSharedText = text;
            }
        }
        // #906: every pickup toast fits, not only those of the names OoT also uses.
        if (!fits) {
            failures += Fail("the pickup toast '%s' at %.1fx, %s: x %.0f to %.0f (%.0f px) against the overlay's "
                             "%.0f-px margin%s",
                             text.c_str(), scale, p.name, m.x0, m.x1, width, margin,
                             isShared ? "" : " (no OoT item shares this name)");
            otherOverflows += isShared ? 0 : 1;
            continue;
        }
        if (CheckFontScale(run, "the pickup toast", scale, m.fontScale, text) != 0) {
            failures++;
            continue;
        }
        run.checked++;
    }
    if (shared == 0) {
        failures += Fail("no MM item that may cross shares a name with an OoT item; the shared-name sweep is vacuous");
    }
    printf("[TEST] %s, %.1fx: %d MM pickup toasts read 'You found <item> (MM)'; of the %d names OoT also uses, the "
           "widest '%s' at %.0f px; widest of all '%s' at %.0f px; %d other(s) past the margin\n",
           p.name, scale, swept, shared, widestSharedText.c_str(), widestShared, widestText.c_str(), widest,
           otherOverflows);
    return failures;
}

void SetScale(float scale) {
    CVarSetFloat(CVAR_SETTING("Notifications.Size"), scale);
}

// #881: the profiles the paired-file load's "Restored ..." toasts are checked
// at. A config with no window size opens libultraship's default 640 x 480
// (gfx_sdl2.cpp, Window.Width 640), which nothing in rsbs/ or src/ overrides; the
// ui tier's smallest profile is 832 x 600.
constexpr Profile kLoadProfiles[] = {
    { "640x480 at menu scale 1.0", 640.0f, 480.0f, 1.0f },
    { "832x600 at menu scale 1.0", 832.0f, 600.0f, 1.0f },
};

/** One "Restored ..." toast: its kind, the names a real load hands it, the true count (0: the list is complete). */
struct LoadInput {
    int kind;
    std::string names;
    int count;
    std::string what;
};

/** A ", "-joined list's names, in order. */
std::vector<std::string> SplitNames(const std::string& list) {
    std::vector<std::string> names;
    std::size_t start = 0;
    while (!list.empty()) {
        const std::size_t comma = list.find(", ", start);
        names.push_back(list.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 2;
    }
    return names;
}

/** #1479: @p labels as the load's buffer carries them, the fill of
 *  Combo_ComboSettingsRestoreLiveEx3: as many leading WHOLE labels as fit
 *  RSBS_LOAD_RULES_NAMES_LEN with its terminator. */
std::string ThroughLoadBuffer(const std::vector<std::string>& labels) {
    std::string fitted;
    for (const std::string& label : labels) {
        const std::string candidate = fitted.empty() ? label : fitted + ", " + label;
        if (candidate.size() >= (std::size_t)RSBS_LOAD_RULES_NAMES_LEN) {
            break;
        }
        fitted = candidate;
    }
    return fitted;
}

/**
 * #1479: a "Restored from file:" message names whole rule labels only, the
 * input's leading ones in order (at least one), followed by " +N" where N is the
 * true count less the names shown: never a name cut with "...", never a bare
 * count, never a "+N" short of the true count.
 */
int CheckRulesMessage(const std::string& names, int count, const std::string& message, const char* what, float scale,
                      const Profile& p) {
    const std::vector<std::string> labels = SplitNames(names);
    const std::size_t total = std::max<std::size_t>(labels.size(), count > 0 ? (std::size_t)count : 0u);
    std::string shown;
    for (std::size_t k = 1; k <= labels.size(); k++) {
        shown += (k == 1 ? "" : ", ") + labels[k - 1];
        const std::string want = shown + (total > k ? " +" + std::to_string(total - k) : std::string());
        if (message == want) {
            return 0;
        }
    }
    return Fail("%s at %.1fx, %s: the rules toast reads '%s %s'; it must name whole rule labels from the list, in "
                "order, then ' +N' for the rest of the %zu restored (never a name cut with '...', never a bare count)",
                what, scale, p.name, kRulesRestoredPrefix, message.c_str(), total);
}

/**
 * #881: the load's "Restored from file:" and "Restored for Majora's Mask:" toasts,
 * through RsbsSave_EmitLoadToast, with the inputs a real load hands it: every
 * Cross-Game Rule alone and all of them at once; the playtest's MM case (one
 * option named, a second counted: "Starting Hearts +1", the #881 frame); every
 * MM option and settable trick label alone; and all of them at once.
 */
std::vector<LoadInput> LoadInputs() {
    std::vector<LoadInput> inputs;
    std::string rules;
    for (int i = 0; i < COMBO_SETTING_COUNT; i++) {
        const char* label = Combo_ComboSettingLabel(static_cast<ComboSettingId>(i));
        if (label == nullptr || label[0] == '\0') {
            continue;
        }
        inputs.push_back({ RSBS_LOAD_TOAST_RULES_RESTORED, label, 0, std::string("rules restored (") + label + ")" });
        rules += (rules.empty() ? "" : ", ") + std::string(label);
    }
    inputs.push_back({ RSBS_LOAD_TOAST_RULES_RESTORED, rules, 0, "rules restored (every rule)" });
    // #1479: what the load hands the emitter for a long restore, its buffer of
    // whole labels and the restore's true count (shared-masks-rule locks that the
    // restore fills the buffer this way): the fifteen mask rows, and every rule.
    std::vector<std::string> masks;
    for (ComboSettingId id :
         { COMBO_SETTING_SHARED_MASK_GORON, COMBO_SETTING_SHARED_MASK_ZORA, COMBO_SETTING_SHARED_MASK_BUNNY,
           COMBO_SETTING_SHARED_MASK_KEATON, COMBO_SETTING_SHARED_MASK_TRUTH, COMBO_SETTING_BLAST_MASK_OOT,
           COMBO_SETTING_STONE_MASK_OOT, COMBO_SETTING_GERUDO_MASK_MM, COMBO_SETTING_SKULL_MASK_MM,
           COMBO_SETTING_SPOOKY_MASK_MM, COMBO_SETTING_SHARED_MASK_BLAST, COMBO_SETTING_SHARED_MASK_STONE,
           COMBO_SETTING_SHARED_MASK_GERUDO, COMBO_SETTING_SHARED_MASK_SKULL, COMBO_SETTING_SHARED_MASK_SPOOKY }) {
        const char* label = Combo_ComboSettingLabel(id);
        masks.push_back(label != nullptr ? label : "");
    }
    inputs.push_back({ RSBS_LOAD_TOAST_RULES_RESTORED, ThroughLoadBuffer(masks), (int)masks.size(),
                       "rules restored (the fifteen mask rows, through the load's buffer)" });
    // Every restorable row, in the order the restore writes and names them
    // (Combo_ComboSettingsRestorableAt: "Goal, Logic, ..."), not the page's.
    std::vector<std::string> restorable;
    for (int i = 0; i < Combo_ComboSettingsRestorableCount(); i++) {
        const char* label = Combo_ComboSettingLabel(Combo_ComboSettingsRestorableAt(i));
        if (label != nullptr && label[0] != '\0') {
            restorable.push_back(label);
        }
    }
    inputs.push_back({ RSBS_LOAD_TOAST_RULES_RESTORED, ThroughLoadBuffer(restorable), (int)restorable.size(),
                       "rules restored (every restorable rule, through the load's buffer)" });
    inputs.push_back(
        { RSBS_LOAD_TOAST_MM_RESTORED, "Starting Hearts", 2, "MM restored (#881's 'Starting Hearts +1')" });
    // The descriptor tables the MM options and tricks pages publish (both
    // registrars are idempotent; GameExports_SingleExe.cpp brings them up the same way).
    if (Combo_MMOptionCount() == 0) {
        MM_RandoOptionsUi_Register();
    }
    if (Combo_MMTrickCount() == 0) {
        MM_RandoTricksUi_Register();
    }
    // #906: every settable MM trick at once, the input the ui tier's
    // toast/load-mm-restored-many page hands the emitter: in the 640-px window at
    // 1.8 its fit reaches the bare count.
    std::string tricks;
    int trickCount = 0;
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
        if (d != nullptr && d->bound && !d->reserved && d->label != nullptr && d->label[0] != '\0') {
            tricks += (tricks.empty() ? "" : ", ") + std::string(d->label);
            trickCount++;
        }
    }
    if (trickCount > 1) {
        inputs.push_back({ RSBS_LOAD_TOAST_MM_RESTORED, tricks, trickCount, "MM restored (every trick)" });
    }
    std::string all;
    int count = 0;
    auto add = [&](const char* label) {
        if (label == nullptr || label[0] == '\0') {
            return;
        }
        inputs.push_back({ RSBS_LOAD_TOAST_MM_RESTORED, label, 1, std::string("MM restored (") + label + ")" });
        all += (all.empty() ? "" : ", ") + std::string(label);
        count++;
    };
    for (int i = 0; i < Combo_MMOptionCount(); i++) {
        const ComboMMOptionDesc* d = Combo_MMOptionAt(i);
        add(d != nullptr ? d->label : nullptr);
    }
    for (int i = 0; i < Combo_MMTrickCount(); i++) {
        const ComboMMTrickDesc* d = Combo_MMTrickAt(i);
        add(d != nullptr && d->bound && !d->reserved ? d->label : nullptr);
    }
    if (count > 1) {
        inputs.push_back({ RSBS_LOAD_TOAST_MM_RESTORED, all, count, "MM restored (every option and trick)" });
    }
    return inputs;
}

/**
 * #881: every "Restored ..." toast keeps the overlay's margin on both sides of a
 * 640-px and an 832-px window, at Notifications.Size 1.8 and 1.0. And where the
 * whole #881 toast fits (the 832-px window, and the 640-px one at size 1.0), it
 * is not cut: the fit shortens a toast only for a window it does not fit.
 */
int CheckLoadToasts(Notification::Window& overlay, const ImGuiStyle& baseStyle, int& checked) {
    ImGuiIO& io = ImGui::GetIO();
    const std::vector<LoadInput> inputs = LoadInputs();
    int failures = 0;
    for (const Profile& profile : kLoadProfiles) {
        ImGui::GetStyle() = baseStyle;
        ImGui::GetStyle().ScaleAllSizes(profile.menuScale);
        io.FontGlobalScale = profile.menuScale;
        io.DisplaySize = ImVec2(profile.width, profile.height);
        FitRun run;
        run.overlay = &overlay;
        run.profile = &profile;
        run.fullSizeOnly = true; // #906: CheckOne fails a load toast drawn smaller unless a bare count
        for (float scale : { 1.8f, 1.0f }) {
            SetScale(scale);
            std::string issueText;
            for (const LoadInput& in : inputs) {
                RsbsSave_EmitLoadToast(in.kind, in.names.c_str(), in.count);
                ComboNotification last;
                memset(&last, 0, sizeof(last));
                std::string text;
                if (OoT_Notification_PeekLastForTest(&last) == 1) {
                    text = std::string(last.prefix != nullptr ? last.prefix : "") + " " +
                           (last.message != nullptr ? last.message : "");
                }
                if (in.what.find("#881") != std::string::npos) {
                    issueText = text;
                }
                if (in.kind == RSBS_LOAD_TOAST_RULES_RESTORED) {
                    failures += CheckRulesMessage(in.names, in.count, last.message != nullptr ? last.message : "",
                                                  in.what.c_str(), scale, profile);
                }
                failures += CheckOne(run, in.what.c_str(), scale, Class::OTHER);
            }
            printf("[TEST] %s, %.1fx: the #881 toast reads '%s'\n", profile.name, scale, issueText.c_str());
            // Where the whole #881 toast fits it must read whole: the 832-px
            // window at both sizes, and the 640-px window at size 1.0. Only
            // 640 px at 1.8 may shorten it.
            const bool fitsWhole = profile.width >= 832.0f || scale == 1.0f;
            if (fitsWhole && issueText != "Restored for Majora's Mask: Starting Hearts +1") {
                failures += Fail("%s at %.1fx: the #881 toast reads '%s'; it fits whole here, so it must not be cut",
                                 profile.name, scale, issueText.c_str());
            }
        }
        checked += run.checked;
        printf("[TEST] %s: %zu load toast inputs checked; widest at 1.8x %.0f px ('%s'); %d drawn below the "
               "player's size, %d of them a fit's floor (a bare count, #904, or one whole rule name, #1479)\n",
               profile.name, inputs.size(), run.widest, run.widestText.c_str(), run.shrunk, run.floorShrunk);
    }
    return failures;
}

/**
 * How many RSBS_COMBO_DIVERGE_* bits exist, DERIVED from the describer's own
 * name table: the bits are allocated from bit 0 upwards, so the count is the
 * first bit Combo_ComboSettingsDivergenceFieldName does not name. -1 when a named
 * bit sits above an unnamed one (a hole the loop below would skip).
 */
int CountDivergenceBits() {
    int count = 0;
    while (count < 32 && strcmp(Combo_ComboSettingsDivergenceFieldName(1u << count), "(unknown)") != 0) {
        count++;
    }
    for (int b = count; b < 32; b++) {
        if (strcmp(Combo_ComboSettingsDivergenceFieldName(1u << b), "(unknown)") != 0) {
            return -1;
        }
    }
    return count;
}

} // namespace

extern "C" int OoT_NotificationFit_RunHeadless(const char* fontPath) {
    printf("[TEST] pairing-refusal-toast-fit: every cross-game refusal toast keeps the overlay's margin on both "
           "sides, drawn by SoH's own overlay in its default font, at Notifications.Size 1.8 and 1.0, in a 640-px "
           "window, an 832-px window and a 4K window at the X-Large menu scale\n");

    if (fontPath == nullptr || fontPath[0] == '\0') {
        return Fail("no font path: the runner resolves games/oot/assets/custom/fonts/Montserrat-Regular.ttf (the "
                    "file soh.o2r packs as SoH's default font) under RSBS_SOURCE_DIR");
    }
    const int divergenceBits = CountDivergenceBits();
    if (divergenceBits <= 0 || divergenceBits > 30) {
        return Fail("the divergence field names are not a contiguous run from bit 0 (count %d); the RULES sweep "
                    "cannot know which bits to cover",
                    divergenceBits);
    }
    const uint32_t combinations = (1u << divergenceBits) - 1u;

    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGuiContext* context = ImGui::CreateContext();
    ImGui::SetCurrentContext(context);
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.DeltaTime = 1.0f / 60.0f;
    ImFont* font = io.Fonts->AddFontFromFileTTF(fontPath, kDefaultFontPixels);
    int rc = 0;
    if (font == nullptr) {
        rc = Fail("could not load the default font from '%s'", fontPath);
    } else {
        io.FontDefault = font;
        unsigned char* pixels = nullptr;
        int texW = 0;
        int texH = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &texW, &texH); // builds the atlas; no renderer needs it
    }
    const ImGuiStyle baseStyle = ImGui::GetStyle();

    // The overlay's own position and scale settings, pinned for the run and
    // cleared after it: bottom right (its default, 3) is where a toast grows
    // leftwards off the window.
    CVarSetInteger(CVAR_SETTING("Notifications.Position"), 3);
    Notification::Window overlay("", true, "Notifications Window");
    OoT_Notification_ClearForTest();

    // Every toast is checked even after a failure, so a red run lists all of
    // them (at most kMaxRulesFailures of the rules combinations per pass).
    constexpr int kMaxRulesFailures = 3;
    int failures = 0;
    int checked = 0;
    for (const Profile& profile : kProfiles) {
        if (rc != 0) {
            break;
        }
        // OTRGlobals::ScaleImGui: style sizes scaled by the menu scale, and the
        // scale set as FontGlobalScale (which the overlay's SetWindowFontScale
        // multiplies).
        ImGui::GetStyle() = baseStyle;
        ImGui::GetStyle().ScaleAllSizes(profile.menuScale);
        io.FontGlobalScale = profile.menuScale;
        io.DisplaySize = ImVec2(profile.width, profile.height);
        FitRun run;
        run.overlay = &overlay;
        run.profile = &profile;
        const int failuresBefore = failures;

        for (float scale : { 1.8f, 1.0f }) {
            SetScale(scale);

            // Calibration: PR #749's creation-failure toast, measured at 800 px in
            // the ui tier's 832-px window.
            OoT_Creation_ReportFailureAtFileSelect(0, 0);
            {
                const Measured m = DrawAndMeasure(overlay);
                OoT_Notification_ClearForTest();
                printf("[TEST] calibration, %s, %.1fx: PR #749's creation-failure toast draws %.0f px wide (x %.0f "
                       "to %.0f) at %.2fx\n",
                       profile.name, scale, m.x1 - m.x0, m.x0, m.x1, m.fontScale);
            }
            // #906: the creation toasts the ui tier draws (toast/creation-*), under
            // the same width and copy contract as every other toast. Outside the
            // refusal class: their own prefixes.
            OoT_Creation_ReportFailureAtFileSelect(0, 0);
            failures += CheckOne(run, "creation-failure toast", scale, Class::OTHER);
            OoT_Creation_EmitShortfallToast(2, 4); // the numbers the toast/creation-shortfall page draws
            failures += CheckOne(run, "creation-shortfall toast", scale, Class::OTHER);
            for (uint32_t halves : { (uint32_t)RSBS_COMBO_HALF_OOT, (uint32_t)RSBS_COMBO_HALF_MM,
                                     (uint32_t)(RSBS_COMBO_HALF_OOT | RSBS_COMBO_HALF_MM) }) {
                OoT_Creation_EmitGoalWarningToast(halves);
                failures += CheckOne(run, "creation goal-warning toast", scale, Class::OTHER);
            }
            // #1350: Generate's note that OoT's No Logic was generated Glitchless.
            OoT_Creation_EmitLogicPinToast();
            failures += CheckOne(run, "OoT No Logic pin toast", scale, Class::OTHER);
            // #1483: Generate's home-capacity refusal, every line, read back word
            // for word (a branch that picks the wrong line, or a line that is not
            // drawn, fails here), then under the width and copy contract; and the
            // file select's "Not created:" copy of each, drawn the same way.
            struct HomeCapacityCase {
                uint32_t shortGames;
                unsigned piecesOoT;
                unsigned piecesMM;
                const char* line;
            };
            for (const HomeCapacityCase& c : {
                     HomeCapacityCase{ RSBS_COMBO_HALF_MM, 100, 155, "too many Majora's Mask pieces." },
                     HomeCapacityCase{ RSBS_COMBO_HALF_OOT, 100, 155, "too many Ocarina of Time pieces." },
                     HomeCapacityCase{ RSBS_COMBO_HALF_OOT | RSBS_COMBO_HALF_MM, 100, 155,
                                       "too many pieces for either game." },
                     HomeCapacityCase{ RSBS_COMBO_HALF_MM, 0, 0, "too few checks in Majora's Mask." },
                     HomeCapacityCase{ RSBS_COMBO_HALF_OOT, 0, 0, "too few checks in Ocarina of Time." },
                     HomeCapacityCase{ RSBS_COMBO_HALF_OOT | RSBS_COMBO_HALF_MM, 30, 0,
                                       "too few checks in either game." },
                 }) {
                if (OoT_Creation_EmitHomeCapacityToast(c.shortGames, c.piecesOoT, c.piecesMM) != 1) {
                    failures += Fail("home-capacity toast (%s): the emitter raised nothing", c.line);
                    continue;
                }
                ComboNotification shown;
                memset(&shown, 0, sizeof(shown));
                const std::string text = OoT_Notification_PeekLastForTest(&shown) == 1
                                             ? std::string(shown.prefix != nullptr ? shown.prefix : "") + " " +
                                                   (shown.message != nullptr ? shown.message : "")
                                             : std::string();
                if (text != std::string("Not generated: ") + c.line) {
                    failures +=
                        Fail("home-capacity toast: reads '%s', expected 'Not generated: %s'", text.c_str(), c.line);
                }
                failures += CheckOne(run, "home-capacity toast", scale, Class::OTHER);
                // The file select's copy ("Not created:"), whose words come from
                // the last report: one that names exactly c.shortGames (10 rows on
                // 9 checks in each short game, 1 on 9 in the other) and the pieces.
                ComboSingleBagReport last;
                memset(&last, 0, sizeof(last));
                last.status = RSBS_COMBO_LOGIC_ERR_NO_CANDIDATE;
                last.hostsOoT = 9;
                last.hostsMM = 9;
                last.homeRequiredOoT = (c.shortGames & RSBS_COMBO_HALF_OOT) != 0u ? 10 : 1;
                last.homeRequiredMM = (c.shortGames & RSBS_COMBO_HALF_MM) != 0u ? 10 : 1;
                last.triforcePiecesOoT = (uint16_t)c.piecesOoT;
                last.triforcePiecesMM = (uint16_t)c.piecesMM;
                Combo_SingleBag_TestSetLastReport(&last);
                OoT_Creation_ReportFailureAtFileSelect(0, RSBS_SINGLE_BAG_FAILURE_HOME_CAPACITY);
                Combo_SingleBag_TestSetLastReport(nullptr);
                memset(&shown, 0, sizeof(shown));
                const std::string created = OoT_Notification_PeekLastForTest(&shown) == 1
                                                ? std::string(shown.prefix != nullptr ? shown.prefix : "") + " " +
                                                      (shown.message != nullptr ? shown.message : "")
                                                : std::string();
                if (created != std::string("Not created: ") + c.line) {
                    failures += Fail("file-select home-capacity toast: reads '%s', expected 'Not created: %s'",
                                     created.c_str(), c.line);
                }
                failures += CheckOne(run, "file-select home-capacity toast", scale, Class::OTHER);
            }
            if (OoT_Creation_EmitHomeCapacityToast(0u, 100, 155) != 0) {
                OoT_Notification_ClearForTest();
                failures +=
                    Fail("home-capacity toast: raised with no short game; Generate would lose its generic line");
            }
            // #1483: the goal refusal under the hunt names the Triforce Goal row.
            OoT_Creation_EmitGoalUnreachableToast(RSBS_COMBO_GOAL_TRIFORCE_HUNT, 0, 0);
            {
                ComboNotification shown;
                memset(&shown, 0, sizeof(shown));
                if (OoT_Notification_PeekLastForTest(&shown) != 1 || shown.message == nullptr ||
                    strcmp(shown.message, "the Triforce Goal can never be met.") != 0) {
                    failures += Fail("hunt goal-unreachable toast: reads '%s'",
                                     shown.message != nullptr ? shown.message : "(nothing)");
                }
            }
            failures += CheckOne(run, "hunt goal-unreachable toast", scale, Class::OTHER);

            MM_Rando_EmitPairingRefusalToast(RSBS_PAIRING_REFUSAL_MM_OPTIONS, nullptr);
            failures += CheckOne(run, "MM-options refusal", scale);
            MM_Rando_EmitPairingRefusalToast(RSBS_PAIRING_REFUSAL_MISSING_HALF, nullptr);
            failures += CheckOne(run, "missing-half refusal", scale);
            for (const char* route : { RSBS_SPOILER_REFUSAL_NOT_PAIRED, RSBS_SPOILER_REFUSAL_SESSION_UNSETTLED,
                                       RSBS_SPOILER_REFUSAL_NO_IDENTITY, RSBS_SPOILER_REFUSAL_IDENTITY_INCOMPLETE,
                                       RSBS_SPOILER_REFUSAL_OTHER_SEED, RSBS_SPOILER_REFUSAL_OTHER_SETTINGS,
                                       RSBS_SPOILER_REFUSAL_OTHER_MM_OPTIONS, "anUnknownRouteTheCopyHasNoWordsFor" }) {
                MM_Rando_EmitPairingRefusalToast(RSBS_PAIRING_REFUSAL_SPOILER, route);
                const std::string what = std::string("spoiler refusal (") + route + ")";
                failures += CheckOne(run, what.c_str(), scale);
            }
            // Outside the refusal class (it refuses a document and latches
            // nothing): width only, under its own "Spoiler not loaded:".
            OoT_EmitPairedSpoilerRefusalToast(/*mute=*/1);
            failures += CheckOne(run, "OoT paired-spoiler refusal", scale, Class::OTHER);
            // #564 V18: an MM save that did not reach the file, every cause.
            // Outside the refusal class: its own "Save failed:".
            for (int cause = 0; cause < RSBS_SAVE_FAILURE_COUNT; cause++) {
                MM_Combo_EmitSaveFailureToast(cause);
                const std::string what = std::string("save failure (") + Combo_SaveFailureWords(cause) + ")";
                failures += CheckOne(run, what.c_str(), scale, Class::OTHER);
            }

            // The refused FILE (#836): the file select's toast over every words
            // entry, and the load's own refusal toasts, each through its
            // production emitter.
            for (int words = 0; words < RSBS_REFUSAL_WORDS_COUNT; words++) {
                RsbsSave_EmitFileSelectRefusalToast(Combo_RefusalWords(words));
                const std::string what = std::string("file-select refusal (") + Combo_RefusalWords(words) + ")";
                failures += CheckOne(run, what.c_str(), scale);
            }
            for (const auto& [kind, name] :
                 { std::pair<int, const char*>{ RSBS_LOAD_TOAST_REFUSED_RULES, "load refusal (rules)" },
                   { RSBS_LOAD_TOAST_REFUSED_OTHER_BUILD, "load refusal (other build)" },
                   { RSBS_LOAD_TOAST_REFUSED_DAMAGED, "load refusal (damaged)" } }) {
                RsbsSave_EmitLoadToast(kind, nullptr, 0);
                failures += CheckOne(run, name, scale);
            }

            // RULES: every non-empty combination of the divergence bits, drawn once
            // per distinct message.
            std::set<std::string> drawn;
            int rulesFailures = 0;
            for (uint32_t bits = 1; bits <= combinations && rulesFailures < kMaxRulesFailures; bits++) {
                char fields[256];
                Combo_ComboSettingsDivergenceDescribe(bits, fields, sizeof(fields));
                char message[512];
                const int named =
                    Combo_PairingRefusalToastMessage(RSBS_PAIRING_REFUSAL_RULES, fields, message, sizeof(message));
                if ((bits & (bits - 1)) == 0 && named != 1) {
                    rulesFailures +=
                        Fail("the one-field rules refusal for '%s' does not name it: '%s'", fields, message);
                    continue;
                }
                if (!drawn.insert(message).second) {
                    continue;
                }
                MM_Rando_EmitPairingRefusalToast(RSBS_PAIRING_REFUSAL_RULES, fields);
                const std::string what = std::string("rules refusal (") + fields + ")";
                rulesFailures += CheckOne(run, what.c_str(), scale);
            }
            failures += rulesFailures;
            if (rulesFailures == 0) {
                printf("[TEST] %s, %.1fx: %zu distinct rules messages over %u field combinations (%d bits) fit\n",
                       profile.name, scale, drawn.size(), combinations, divergenceBits);
            }

            // #865: the pickup toast of an MM item found at an OoT check.
            failures += CheckPickupToasts(run, scale);
        }
        checked += run.checked;
        if (failures == failuresBefore) {
            printf("[TEST] %s: widest refusal toast at 1.8x is %.0f px ('%s')\n", profile.name, run.widest,
                   run.widestText.c_str());
        }
        // #906: how many toasts the overlay drew smaller to fit this window.
        if (run.shrunk > 0) {
            printf("[TEST] %s: %d toast(s) drawn below the player's size to fit, the smallest at %.2fx ('%s')\n",
                   profile.name, run.shrunk, run.smallest, run.smallestText.c_str());
        } else {
            printf("[TEST] %s: no toast drawn below the player's size\n", profile.name);
        }
    }
    if (rc == 0) {
        // #881: the load's "Restored ..." toasts, at the default 640-px window too.
        failures += CheckLoadToasts(overlay, baseStyle, checked);
    }
    ImGui::GetStyle() = baseStyle;
    io.FontGlobalScale = 1.0f;
    if (rc == 0 && failures != 0) {
        rc = Fail("%d toast check(s) failed (listed above)", failures);
    }

    if (rc == 0) {
        // mm-combo-settings-gate leg 4 tells a moved triforce half from a matching
        // one by whether the toast names "triforceHunt" beside "goal".
        char fields[256];
        Combo_ComboSettingsDivergenceDescribe(RSBS_COMBO_DIVERGE_GOAL | RSBS_COMBO_DIVERGE_TRIFORCE, fields,
                                              sizeof(fields));
        char message[128];
        if (Combo_PairingRefusalToastMessage(RSBS_PAIRING_REFUSAL_RULES, fields, message, sizeof(message)) != 2) {
            rc = Fail("the goal + triforceHunt refusal does not name both fields: '%s'", message);
        }
    }

    CVarClear(CVAR_SETTING("Notifications.Size"));
    CVarClear(CVAR_SETTING("Notifications.Position"));
    OoT_Notification_ClearForTest();
    ImGui::DestroyContext(context);
    ImGui::SetCurrentContext(previous);

    if (rc != 0) {
        return rc;
    }
    printf("[TEST] PASS: %d toasts drawn by SoH's overlay, every one keeping the overlay's margin on both "
           "sides\n",
           checked);
    return 0;
}

#endif // RSBS_SINGLE_EXECUTABLE
