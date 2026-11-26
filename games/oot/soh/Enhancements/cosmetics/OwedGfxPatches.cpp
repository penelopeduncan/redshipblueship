#include "OwedGfxPatches.h"

#ifdef RSBS_SINGLE_EXECUTABLE
#include <libultraship/bridge.h>

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "context.h" // src/common: Context_GetCurrentGame

struct PlayState;
extern "C" PlayState* OoT_gPlayState;

// RSBS (#1829): see OwedGfxPatches.h. The debt lives in memory only: at shutdown it is dropped, and the next boot's
// InitAll (run before any game is current) owes every pass again.
struct OoTOwedGfxEntry {
    const char* id;
    OoTGfxPatchPass pass;
    bool needsPlay;
};

static std::vector<OoTOwedGfxEntry> sOoTOwedGfx; // in the order the passes were first asked for

// The ROM-free MMShipInitDriver row's seams (games/oot/soh/soh_shipinit_driver_test.cpp): a pass count per id, and a
// body skip, because that row mounts no game archive and has no OTRGlobals::Instance.
static std::unordered_map<std::string, int> sOoTOwedGfxPasses;
static bool sOoTOwedGfxBodySkipped = false;

static const OoTOwedGfxEntry* OoTOwedGfx_Find(const char* id) {
    for (const OoTOwedGfxEntry& e : sOoTOwedGfx) {
        if (strcmp(e.id, id) == 0) {
            return &e;
        }
    }
    return nullptr;
}

static void OoTOwedGfx_Run(const char* id, OoTGfxPatchPass pass, bool owed) {
    sOoTOwedGfxPasses[id]++;
    if (!sOoTOwedGfxBodySkipped) {
        pass(owed);
    }
}

void OoT_GfxPatches_RunOrOwe(const char* id, OoTGfxPatchPass pass, bool needsPlay) {
    if (Context_GetCurrentGame() != GAME_OOT) {
        if (OoTOwedGfx_Find(id) != nullptr) {
            return; // owed once; it keeps its first place and reads its CVar when paid
        }
        sOoTOwedGfx.push_back({ id, pass, needsPlay });
        return;
    }
    OoTOwedGfx_Run(id, pass, false);
}

extern "C" int OoT_GfxPatches_ApplyOwed(void) {
    if (sOoTOwedGfx.empty() || Context_GetCurrentGame() != GAME_OOT) {
        return 0;
    }
    std::vector<OoTOwedGfxEntry> owed;
    owed.swap(sOoTOwedGfx);
    int paid = 0;
    for (const OoTOwedGfxEntry& e : owed) {
        // OoT's first frame after a return from MM is the Opening gamestate, which has just written a debug save over
        // gSaveContext; OoT's own save is back once Play_Init has run, and the play state is set after that.
        if (e.needsPlay && OoT_gPlayState == NULL) {
            sOoTOwedGfx.push_back(e);
            continue;
        }
        OoTOwedGfx_Run(e.id, e.pass, true);
        paid++;
    }
    return paid;
}

extern "C" int OoT_GfxPatchesTest_Passes(const char* id) {
    auto it = sOoTOwedGfxPasses.find(id);
    return it == sOoTOwedGfxPasses.end() ? 0 : it->second;
}

extern "C" int OoT_GfxPatchesTest_Owed(const char* id) {
    return OoTOwedGfx_Find(id) != nullptr ? 1 : 0;
}

extern "C" int OoT_GfxPatchesTest_OwedCount(void) {
    return (int)sOoTOwedGfx.size();
}

extern "C" void OoT_GfxPatchesTest_SetBodySkipped(int skip) {
    sOoTOwedGfxBodySkipped = skip != 0;
}

// A probe pass for the row: records the value of a key when it is called, so the row can tell a pass that reads its
// CVar at payment time from one that captured it when it was asked for.
static const char* sOoTOwedGfxProbeKey = "";
static int sOoTOwedGfxProbeSeen = -1;

static void OoTOwedGfx_ProbePass(bool) {
    sOoTOwedGfxProbeSeen = CVarGetInteger(sOoTOwedGfxProbeKey, -1);
}

extern "C" void OoT_GfxPatchesTest_RunOrOweProbe(const char* key) {
    sOoTOwedGfxProbeKey = key;
    sOoTOwedGfxProbeSeen = -1;
    OoT_GfxPatches_RunOrOwe("OoTGfxPatchesTestProbe", OoTOwedGfx_ProbePass, false);
}

extern "C" int OoT_GfxPatchesTest_ProbeSeen(void) {
    return sOoTOwedGfxProbeSeen;
}
#endif
