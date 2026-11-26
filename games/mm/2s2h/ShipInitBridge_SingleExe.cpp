/**
 * @file ShipInitBridge_SingleExe.cpp
 * @brief #539 — the CVar-change driver for MM's `S2H::ShipInit` registrar map.
 *
 * THE BUG THIS CLOSES
 * ===================
 * `2s2h/ShipInit.hpp` puts MM's registrar map in `namespace S2H` (the #375
 * COMDAT-fold fix), so MM owns a map that is disjoint from OoT's. Upstream MM
 * re-runs entries out of that map whenever a CVar changes, from its own widget
 * layer (`2s2h/BenGui/UIWidgets.cpp`) — but in single-exe that layer is drawn
 * only by the tracker windows and only ever passes tracker CVar paths, so no
 * converged enhancement key ever reaches it. The one live menu is OoT's
 * `SohMenu`, whose widgets call OoT's `ShipInit::Init(cvar)`.
 *
 * Result before this TU existed: MM's map was driven EXACTLY ONCE per process,
 * by `S2H::ShipInit::InitAll()` in `2s2h/GameExports_SingleExe.cpp`. A shared
 * key like `gEnhancements.RememberSaveLocation` re-armed OoT's registrar the
 * instant the player clicked, and left MM's latched at whatever the value had
 * been at MM's first boot. Under the one-game ruling that divergence is
 * corruption: one combo, two halves silently disagreeing about the same
 * setting.
 *
 * THE CHOKE POINT
 * ===============
 * OoT's `ShipInit::Init(path)` (`games/oot/soh/ShipInit.hpp`) — every one of
 * the menu's ~13 widget call sites funnels through it, so wiring it here is one
 * edit instead of thirteen, and a widget added tomorrow is driven for free.
 * That header calls the entry point below; the policy lives in this TU, next to
 * MM's map, rather than in OoT's header.
 *
 * WHAT IS DELIBERATELY *NOT* FORWARDED
 * ====================================
 * The map is keyed by path string, and two of those keys are pseudo-paths with
 * game-specific timing rather than CVar names:
 *
 *  - `"*"` — "every registrar". OoT drives it from boot (`OTRGlobals.cpp`) and
 *    from preset loads (`Enhancements/Presets/Presets.cpp`). Forwarding it
 *    would run MM's whole registrar set from OoT's boot, BEFORE MM's own
 *    `InitAll()` has ever run, and again on every OoT preset load. MM drives
 *    its own wildcard once, at MM bring-up (`MM_Rando_InitCore`); upstream MM
 *    also drives it from `PresetManager.cpp`, which the single exe does not
 *    build. A BULK write of CVars on OoT's side (an Enhancements preset, a
 *    dropped config file) re-drives MM's registrars keyed on the CVars it
 *    CHANGED instead of `"*"`: see BULK CVAR WRITES below.
 *  - `"IS_RANDO"` — "the rando arm-state may have changed". OoT drives it when
 *    an OoT rando save loads (`Enhancements/randomizer/hook_handlers.cpp`);
 *    MM drives its own from `Rando/Rando.cpp` on MM's save-load chain, which is
 *    what the #439 arm-state locks assert. Forwarding OoT's would arm MM's
 *    rando hooks against MM's *previous* save state on an OoT-only file load.
 *
 * Both exclusions are asserted by the `mm-shipinit-driver` lock, so "simplify
 * the filter" is a red test rather than a silent behavior change.
 *
 * BLAST RADIUS
 * ============
 * Bounded by construction: the forward is keyed on the exact CVar name OoT's
 * menu just wrote, and only fires when MM's map already holds a registrar under
 * that name. A key MM does not register on is a lookup miss and nothing runs,
 * so the set of MM registrars this can reach is exactly "MM registrars keyed on
 * a CVar the unified menu writes" — i.e. the converged shared keys, which is
 * precisely the set #539 is about. The `find()` guard also keeps the forward
 * from growing MM's map: `ShipInit::Init` indexes with `operator[]`, which
 * would insert an empty vector for every OoT-only key the player ever touched.
 *
 * BULK CVAR WRITES (#1811)
 * ========================
 * Most MM enhancement registrars decide whether their hook is registered when
 * the registrar runs (COND_HOOK, COND_ID_HOOK and COND_VB_SHOULD read the
 * option once), so a write that changed many CVars and then called only OoT's
 * `ShipInit::InitAll()` left every MM hook as it was: the row showed the new
 * value and the game kept the old one until a click or a restart. OoT's bulk
 * writers (`applyPreset` in Enhancements/Presets/Presets.cpp and
 * `SoH_HandleConfigDrop` in OTRGlobals.cpp) now open a scope with
 * `MM_ShipInit_BeginBulkCVarWrite` before their first write and close it with
 * `MM_ShipInit_EndBulkCVarWrite` (the `ShipInitMmBulkCVarWrite` guard in OoT's
 * ShipInit.hpp). Begin copies the value of every CVar key MM's map holds a
 * registrar under; End runs `ShipInit::Init(key)` for each key whose value
 * differs, which is what one click per changed row runs. Unchanged keys, `"*"`
 * and `"IS_RANDO"` are never driven, so the blast radius stays the click
 * path's. End reads CVars and never writes one, and it collects the changed
 * keys before it drives any, so a registrar that writes a CVar (PersistentMasks'
 * state key) cannot feed the comparison.
 *
 * The four gfx-patch keys go through `MM_GfxPatches_Reapply`, never
 * `ShipInit::Init`, because their patches must land under MM's resource cache
 * owner (#1000). Fix Out of Bounds Textures, Fix Scene Geometry Seams and
 * Disable Scene Geometry Distance Check have no registrar (SoH's rows call the
 * reapply from their callbacks); Fix Circle Shadow Streaks has a registrar
 * whose body is not owner-gated and no OoT row, so driving it from OoT's menu
 * would patch OoT's copy of gameplay_keep. The reapply runs at most once per
 * bulk write: at once while MM runs, at MM's next resume otherwise, and not at
 * all before MM's asset phase (which reads the keys itself).
 */

#ifdef RSBS_SINGLE_EXECUTABLE

#include "2s2h/ShipInit.hpp"

#include <libultraship/bridge/consolevariablebridge.h>

#include "cvar_shared_keys.h"   // src/common: the two scene-geometry patch keys
#include "mm_graphics_bridge.h" // src/common: MM_GfxPatches_Reapply

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace {

// The two pseudo-paths MM owns the timing of. See the header comment.
bool MmShipInitBridge_IsPseudoPath(const char* path) {
    return std::strcmp(path, "*") == 0 || std::strcmp(path, "IS_RANDO") == 0;
}

// #1811: the keys that choose MM's keyed gfx patches. See BULK CVAR WRITES in the header comment.
const char* const kMmShipInitBridgeGfxPatchKeys[] = {
    "gEnhancements.FixTexturesOOB",
    RSBS_CVAR_MM_GRAPHICS_FIX_GEOMETRY_SEAMS,
    RSBS_CVAR_MM_GRAPHICS_NO_GEOMETRY_DISTANCE_CHECK,
    "gFixes.FixCircleShadowStreaks",
};

bool MmShipInitBridge_IsGfxPatchKey(const std::string& key) {
    for (const char* gfxKey : kMmShipInitBridgeGfxPatchKeys) {
        if (key == gfxKey) {
            return true;
        }
    }
    return false;
}

// A copy of a CVar's stored type and value, so a later read can tell whether it changed. Never keeps the
// shared_ptr<CVar>: CVarSet* mutates that object in place (a kept pointer would never differ) and a config Load
// replaces it.
std::string MmShipInitBridge_Fingerprint(const char* key) {
    const std::shared_ptr<Ship::CVar> cvar = CVarGet(key);
    if (cvar == nullptr) {
        return "-";
    }
    switch (cvar->Type) {
        case Ship::ConsoleVariableType::Integer:
            return "i" + std::to_string(cvar->Integer);
        case Ship::ConsoleVariableType::Float: {
            uint32_t bits = 0;
            static_assert(sizeof(bits) == sizeof(cvar->Float), "float is not 32 bits");
            std::memcpy(&bits, &cvar->Float, sizeof(bits));
            return "f" + std::to_string(bits);
        }
        case Ship::ConsoleVariableType::String:
            return std::string("s") + (cvar->String != nullptr ? cvar->String : "");
        case Ship::ConsoleVariableType::Color:
            return "c" + std::to_string(cvar->Color.r) + "," + std::to_string(cvar->Color.g) + "," +
                   std::to_string(cvar->Color.b) + "," + std::to_string(cvar->Color.a);
        case Ship::ConsoleVariableType::Color24:
            return "r" + std::to_string(cvar->Color24.r) + "," + std::to_string(cvar->Color24.g) + "," +
                   std::to_string(cvar->Color24.b);
    }
    return "?";
}

// The open bulk write's snapshot: (key, fingerprint) for every registrar key, and separately for the gfx-patch keys.
bool sMmShipInitBulkOpen = false;
std::vector<std::pair<std::string, std::string>> sMmShipInitBulkKeys;
std::vector<std::pair<std::string, std::string>> sMmShipInitBulkGfxKeys;

} // namespace

extern "C" {

/**
 * Run MM's registrars keyed on `path`, if any. Called from OoT's
 * `ShipInit::Init` — i.e. from the unified menu's widget layer — after OoT's
 * own registrars for the same path have run.
 */
void MM_ShipInit_OnCVarChanged(const char* path) {
    if (path == nullptr || path[0] == '\0') {
        return;
    }

    // Pseudo-paths: MM owns their timing. See the header comment.
    if (MmShipInitBridge_IsPseudoPath(path)) {
        return;
    }

    auto& shipInitFuncs = S2H::ShipInit::GetAll();
    const auto it = shipInitFuncs.find(path);
    if (it == shipInitFuncs.end() || it->second.empty()) {
        // MM registers nothing on this key — the overwhelmingly common case,
        // since most menu keys are OoT-only. Returning here (rather than
        // calling Init) is what keeps operator[] from inserting an empty
        // vector per key touched.
        return;
    }

    S2H::ShipInit::Init(path);
}

/**
 * How many MM registrars are keyed on `path`. Non-inserting, unlike
 * `ShipInit::Init`'s `operator[]`. Exists so the `mm-shipinit-driver` lock can
 * assert the converged keys really do have MM registrars behind them — the
 * non-vacuity half of the fix, which is otherwise unobservable from outside
 * MM's headers.
 */
int MM_ShipInit_RegistrarCountForPath(const char* path) {
    if (path == nullptr) {
        return 0;
    }
    auto& shipInitFuncs = S2H::ShipInit::GetAll();
    const auto it = shipInitFuncs.find(path);
    return it == shipInitFuncs.end() ? 0 : static_cast<int>(it->second.size());
}

/**
 * #1811: open a bulk CVar write. Copies the value of every CVar key MM's map holds a registrar under (the
 * pseudo-paths and the gfx-patch keys aside), and of the four gfx-patch keys. A second Begin before End replaces the
 * snapshot. Reads CVars only.
 */
void MM_ShipInit_BeginBulkCVarWrite(void) {
    sMmShipInitBulkKeys.clear();
    sMmShipInitBulkGfxKeys.clear();
    for (const auto& entry : S2H::ShipInit::GetAll()) {
        const std::string& path = entry.first;
        if (entry.second.empty() || MmShipInitBridge_IsPseudoPath(path.c_str()) ||
            MmShipInitBridge_IsGfxPatchKey(path)) {
            continue;
        }
        sMmShipInitBulkKeys.emplace_back(path, MmShipInitBridge_Fingerprint(path.c_str()));
    }
    for (const char* gfxKey : kMmShipInitBridgeGfxPatchKeys) {
        sMmShipInitBulkGfxKeys.emplace_back(gfxKey, MmShipInitBridge_Fingerprint(gfxKey));
    }
    sMmShipInitBulkOpen = true;
}

/**
 * #1811: close the bulk CVar write Begin opened. Runs `ShipInit::Init(key)` once for each snapshotted key whose value
 * changed (what one click on its row runs), then, if any gfx-patch key changed, `MM_GfxPatches_Reapply()` once.
 * Returns the number of keys driven, plus 1 when the gfx reapply was requested; 0 when no write is open. Reads CVars
 * only.
 */
int MM_ShipInit_EndBulkCVarWrite(void) {
    if (!sMmShipInitBulkOpen) {
        return 0;
    }
    sMmShipInitBulkOpen = false;

    // Collect every change before driving any: a registrar that writes a CVar must not feed the comparison.
    std::vector<std::string> changed;
    for (const auto& snap : sMmShipInitBulkKeys) {
        if (MmShipInitBridge_Fingerprint(snap.first.c_str()) != snap.second) {
            changed.push_back(snap.first);
        }
    }
    bool gfxChanged = false;
    for (const auto& snap : sMmShipInitBulkGfxKeys) {
        if (MmShipInitBridge_Fingerprint(snap.first.c_str()) != snap.second) {
            gfxChanged = true;
        }
    }
    sMmShipInitBulkKeys.clear();
    sMmShipInitBulkGfxKeys.clear();

    // Iterate the collected keys, never the map: every one came from GetAll(), so Init's operator[] inserts nothing,
    // and a registrar that registers another ShipInit entry cannot invalidate this loop.
    for (const std::string& key : changed) {
        S2H::ShipInit::Init(key);
    }
    if (gfxChanged) {
        (void)MM_GfxPatches_Reapply();
    }
    return static_cast<int>(changed.size()) + (gfxChanged ? 1 : 0);
}

} // extern "C"

#endif // RSBS_SINGLE_EXECUTABLE
