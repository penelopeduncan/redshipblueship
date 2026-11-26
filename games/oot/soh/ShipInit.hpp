#ifndef SHIP_INIT_HPP
#define SHIP_INIT_HPP

#ifdef __cplusplus

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <set>
#include <source_location>
#include <unordered_map>
#include <functional>

struct ShipInitEntry {
    std::function<void()> initFunc;
    std::source_location registeredAt;
};

#ifdef RSBS_SINGLE_EXECUTABLE
/**
 * #539: MM's registrar map is a SEPARATE map (`S2H::ShipInit`, the #375
 * COMDAT-fold split), and in single-exe nothing drives it on a CVar change —
 * MM's own widget layer is not the live menu. `SohMenu`'s widgets all funnel
 * through `ShipInit::Init(cvar)` below, so forwarding from there is the one
 * choke point that re-arms both games from one click.
 *
 * Defined in games/mm/2s2h/ShipInitBridge_SingleExe.cpp, which also owns the
 * policy (which paths forward, and why `"*"` / `"IS_RANDO"` do not).
 */
extern "C" void MM_ShipInit_OnCVarChanged(const char* path);

/**
 * #1811: a bulk CVar write (a preset, a dropped config file) ends in `InitAll()`, whose `"*"` the bridge does not
 * forward, so MM's registrars would keep the values they read last. Begin snapshots the CVars MM's registrars are keyed
 * on; End re-drives the ones that changed, as one click per changed row would. Both only read CVars. Defined in
 * games/mm/2s2h/ShipInitBridge_SingleExe.cpp, beside the forwarding policy.
 */
extern "C" void MM_ShipInit_BeginBulkCVarWrite(void);
extern "C" int MM_ShipInit_EndBulkCVarWrite(void);

/**
 * #1817: OoT's half of the same scope. `InitAll()` runs OoT's registrars, but Fix Out of Bounds Textures has none (its
 * row's callback applies the patches), so Begin reads that key and End reapplies OoT's authentic gfx patches, owner
 * gated, when its effective value changed. Both only read CVars. Defined in
 * games/oot/soh/Enhancements/cosmetics/authenticGfxPatches.cpp.
 */
extern "C" void OoT_ShipInit_BeginBulkCVarWrite(void);
extern "C" int OoT_ShipInit_EndBulkCVarWrite(void);

/**
 * #1811: opens a bulk CVar write for MM on construction and closes it on destruction, so an exception still closes
 * it. Declare it before a bulk writer's first CVar write. #1817: it opens and closes OoT's half too.
 */
struct ShipInitMmBulkCVarWrite {
    ShipInitMmBulkCVarWrite() {
        OoT_ShipInit_BeginBulkCVarWrite();
        MM_ShipInit_BeginBulkCVarWrite();
    }
    ~ShipInitMmBulkCVarWrite() {
        (void)MM_ShipInit_EndBulkCVarWrite();
        (void)OoT_ShipInit_EndBulkCVarWrite();
    }
    ShipInitMmBulkCVarWrite(const ShipInitMmBulkCVarWrite&) = delete;
    ShipInitMmBulkCVarWrite& operator=(const ShipInitMmBulkCVarWrite&) = delete;
};
#endif

struct ShipInit {
    static std::unordered_map<std::string, std::vector<ShipInitEntry>>& GetAll() {
        static std::unordered_map<std::string, std::vector<ShipInitEntry>> shipInitFuncs;
        return shipInitFuncs;
    }

    static void InitAll() {
        ShipInit::Init("*");
    }

    static void Init(const std::string& path) {
        // RSBS_TRACE_SHIP_INIT=1 names each init func's registration site as
        // it starts, so a hang or crash inside one is attributable from the
        // log alone (the #361 rando-gen timeout took a CI cycle to localize
        // without this).
        static const bool trace = []() {
            const char* v = std::getenv("RSBS_TRACE_SHIP_INIT");
            return v != nullptr && v[0] == '1';
        }();
        auto& shipInitFuncs = ShipInit::GetAll();
        for (const auto& entry : shipInitFuncs[path]) {
            if (trace) {
                fprintf(stderr, "[ShipInit] %s:%u\n", entry.registeredAt.file_name(),
                        static_cast<unsigned>(entry.registeredAt.line()));
                fflush(stderr);
            }
            entry.initFunc();
        }

#ifdef RSBS_SINGLE_EXECUTABLE
        // #539: re-arm MM's half of the combo on the same change. Runs AFTER
        // OoT's own registrars so the ordering matches a standalone build of
        // either game (its own registrars first), and unconditionally on every
        // path — the bridge, not this header, decides what is forwarded.
        MM_ShipInit_OnCVarChanged(path.c_str());
#endif
    }
};

/**
 * @brief Register a function to execute on boot and (optionally) in other situations
 *
 * @param initFunc The function to execute
 * @param updatePaths Strings to specify additional situations in which to execute the function
 *
 * ### Examples:
 *
 * #### Execute function `bar` on boot
 *
 * ```cpp
 * static RegisterShipInitFunc foo(bar);
 * ```
 *
 * #### Execute function `bar` on boot and when the CVar `baz` might have changed
 *
 * ```cpp
 * static RegisterShipInitFunc foo(bar, { "baz" });
 * ```
 *
 * #### Execute function `bar` on boot and when `IS_RANDO` might have changed
 *
 * ```cpp
 * static RegisterShipInitFunc foo(bar, { "IS_RANDO" });
 * ```
 *
 * ### Additional Information:
 *
 * To get a better sense of when your function will be executed
 * you can look for `ShipInit::Init` calls throughout the codebase
 */
struct RegisterShipInitFunc {
    RegisterShipInitFunc(std::function<void()> initFunc, const std::set<std::string>& updatePaths = {},
                         const std::source_location loc = std::source_location::current()) {
        auto& shipInitFuncs = ShipInit::GetAll();

        shipInitFuncs["*"].push_back({ initFunc, loc });

        for (const auto& path : updatePaths) {
            shipInitFuncs[path].push_back({ initFunc, loc });
        }
    }
};

#endif // __cplusplus

#endif // SHIP_INIT_HPP
