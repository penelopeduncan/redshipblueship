/**
 * @file oot_minigame_hookify_test.cpp
 * ROM-free, display-free lock for the #1753 backport (SoH 236c9c81f, #6968;
 * 5c55c01c6, #6969; cca66cdc5, #6972). CTest label "redship", row
 * OoTMinigameHookify in CMake/SingleExecutable.cmake, dispatch
 * "oot-minigame-hookify" in src/common/test_runner.cpp. The hooks are
 * games/oot/soh/Enhancements/Minigames/BombchuBowling.cpp,
 * FrogsOcarinaGame.cpp and ShootingGallery.cpp.
 *
 * WHY. The Bombchu Bowling, frogs' Ocarina game and Shooting Gallery options
 * used to be read inline by four actor files. Upstream moved each into a
 * GameInteractor_Should hook that a ShipInit registrar arms; the actors now
 * only ask GameInteractor_Should(VB_*, vanilla, ...).
 *
 * WHAT IT DRIVES. Each hook file's own registrars, found in ShipInit's map by
 * their registration site under "*" and under each of their CVar keys (so a
 * registrar the linker dropped, or a key left off a list, fails the row), then
 * the REAL GameInteractor_Should dispatch with the arguments the actors pass.
 * Each leg re-arms through the one key it changed, the way a menu change does.
 * The row does not reach the call sites in the four actor files.
 *
 * THE LEGS.
 *   (off) every master option off, every sub-option set: every answer is
 *         vanilla's. Run first and again last.
 *   (1) small cucco: index 0 refused, index 1 kept.
 *   (2) big cucco: index 1 refused, index 0 kept.
 *   (3) bowling ammo: bombchuBowlingStatus set, answer false.
 *   (4) frogs time limit: frogSongTimer == limit * factor (factor 3).
 *   (5) frogs timer tick: answer false.
 *   (6) frogs instant win: textId 0x40AC, msgMode MSGMODE_PAUSED, answer false.
 *   (7) gallery instant win: hitCount 10, signal ENSYATEKI_END, answer false.
 *   (8) constant adult: answer false.
 *   (9) child ammo, (10) adult ammo: only the matching age is changed.
 */

#ifdef RSBS_SINGLE_EXECUTABLE

// At file scope, not inside an extern "C" block: z64.h includes <memory> under
// C++ (see oot_shared_kokiri_sword_test.cpp).
#include <z64.h>

#include "context.h"
#include "soh/ShipInit.hpp"
#include "soh/cvar_prefixes.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/game-interactor/GameInteractor_Hooks.h"

#include <climits>
#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
#include "src/overlays/actors/ovl_En_Fr/z_en_fr.h"
#include "src/overlays/actors/ovl_En_Syateki_Itm/z_en_syateki_itm.h"
extern SaveContext gSaveContext;
extern PlayState* OoT_gPlayState;
}
extern "C" EnFrPointers sEnFrPointers;

namespace {

#define MGH_ASSERT(cond, msg)                                             \
    do {                                                                  \
        if (!(cond)) {                                                    \
            printf("[TEST] FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
            return 1;                                                     \
        }                                                                 \
    } while (0)

const char* const kBowlCustomize = CVAR_ENHANCEMENT("CustomizeBombchuBowling");
const char* const kBowlSmall = CVAR_ENHANCEMENT("BombchuBowlingNoSmallCucco");
const char* const kBowlBig = CVAR_ENHANCEMENT("BombchuBowlingNoBigCucco");
const char* const kBowlAmmo = CVAR_ENHANCEMENT("BombchuBowlingAmmo");
const char* const kFrogCustomize = CVAR_ENHANCEMENT("CustomizeFrogsOcarinaGame");
const char* const kFrogModify = CVAR_ENHANCEMENT("FrogsModifyFailTime");
const char* const kFrogUnlimited = CVAR_ENHANCEMENT("FrogsUnlimitedFailTime");
const char* const kFrogInstant = CVAR_ENHANCEMENT("InstantFrogsGameWin");
const char* const kGalCustomize = CVAR_ENHANCEMENT("CustomizeShootingGallery");
const char* const kGalInstant = CVAR_ENHANCEMENT("InstantShootingGalleryWin");
const char* const kGalConstant = CVAR_ENHANCEMENT("ConstantAdultGallery");
const char* const kGalAmmoChild = CVAR_ENHANCEMENT("ShootingGalleryAmmoChild");
const char* const kGalAmmoAdult = CVAR_ENHANCEMENT("ShootingGalleryAmmoAdult");

const char* const kAllKeys[] = { kBowlCustomize, kBowlSmall,     kBowlBig,       kBowlAmmo,      kFrogCustomize,
                                 kFrogModify,    kFrogUnlimited, kFrogInstant,   kGalCustomize,  kGalInstant,
                                 kGalConstant,   kGalAmmoChild,  kGalAmmoAdult };
constexpr int kAllKeyCount = 13;
constexpr int kUnset = INT_MIN;

// The three hook files this row owns, and how many registrars each registers
// under "*" and under each of its keys.
struct SiteExpect {
    const char* file;
    const char* key;
    int count;
};
const SiteExpect kSites[] = {
    { "BombchuBowling.cpp", "*", 3 },
    { "BombchuBowling.cpp", kBowlCustomize, 3 },
    { "BombchuBowling.cpp", kBowlSmall, 1 },
    { "BombchuBowling.cpp", kBowlBig, 1 },
    { "BombchuBowling.cpp", kBowlAmmo, 1 },
    { "FrogsOcarinaGame.cpp", "*", 3 },
    { "FrogsOcarinaGame.cpp", kFrogCustomize, 3 },
    { "FrogsOcarinaGame.cpp", kFrogModify, 1 },
    { "FrogsOcarinaGame.cpp", kFrogUnlimited, 1 },
    { "FrogsOcarinaGame.cpp", kFrogInstant, 1 },
    { "ShootingGallery.cpp", "*", 4 },
    { "ShootingGallery.cpp", kGalCustomize, 4 },
    { "ShootingGallery.cpp", kGalInstant, 1 },
    { "ShootingGallery.cpp", kGalConstant, 1 },
    { "ShootingGallery.cpp", kGalAmmoChild, 1 },
    { "ShootingGallery.cpp", kGalAmmoAdult, 1 },
};

// Static so every leg starts from zeroed game state.
PlayState sPlay;
EnFr sFrogs[5];
EnSyatekiItm sGallery;

bool EndsWith(const std::string& s, const char* suffix) {
    const size_t n = strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

bool IsSiteOf(const ShipInitEntry& entry, const char* file) {
    return EndsWith(entry.registeredAt.file_name(), file);
}

bool IsRowSite(const ShipInitEntry& entry) {
    return IsSiteOf(entry, "BombchuBowling.cpp") || IsSiteOf(entry, "FrogsOcarinaGame.cpp") ||
           IsSiteOf(entry, "ShootingGallery.cpp");
}

int CountSites(const char* file, const char* key) {
    int found = 0;
    for (const auto& entry : ShipInit::GetAll()[key]) {
        if (IsSiteOf(entry, file)) {
            found++;
        }
    }
    return found;
}

// Re-arms the hooks the way a CVar change does: this row's registrars under
// @p key, and only those (the other registrars under the same keys are not
// this row's business).
void RunSitesUnder(const char* key) {
    for (const auto& entry : ShipInit::GetAll()[key]) {
        if (IsRowSite(entry)) {
            entry.initFunc();
        }
    }
}

void ClearAllKeys() {
    for (int i = 0; i < kAllKeyCount; i++) {
        CVarClear(kAllKeys[i]);
    }
}

void ResetFixtures() {
    memset(&sPlay, 0, sizeof(sPlay));
    memset(sFrogs, 0, sizeof(sFrogs));
    memset(&sGallery, 0, sizeof(sGallery));
    for (int i = 0; i < 5; i++) {
        sEnFrPointers.frogs[i] = &sFrogs[i];
    }
    // Any scene but the bowling alley: the registrars kill cuccos only there.
    sPlay.sceneNum = SCENE_HYRULE_FIELD;
    gSaveContext.linkAge = LINK_AGE_ADULT;
}

// Every hook disarmed, every fixture zeroed.
void BeginLeg() {
    ClearAllKeys();
    RunSitesUnder("*");
    ResetFixtures();
}

// With every master option off, whatever the sub-options hold, every answer
// the actors ask for is vanilla's and no state changes.
int CheckVanilla(const char* leg) {
    ResetFixtures();
    sPlay.bombchuBowlingStatus = 3;
    sFrogs[0].frogSongTimer = 77;
    sFrogs[0].actor.textId = 0x1234;
    sPlay.msgCtx.msgMode = 5;
    sGallery.signal = 1;
    sGallery.hitCount = 4;

    printf("[TEST] %s: every answer vanilla\n", leg);
    MGH_ASSERT(GameInteractor_Should(VB_SPAWN_BOMBCHU_BOWLING_CUCCOS, true, 0), "small cucco refused with options off");
    MGH_ASSERT(GameInteractor_Should(VB_SPAWN_BOMBCHU_BOWLING_CUCCOS, true, 1), "big cucco refused with options off");
    MGH_ASSERT(GameInteractor_Should(VB_SET_BOMBCHU_BOWLING_AMMO, true), "bowling ammo overridden with options off");
    MGH_ASSERT(sPlay.bombchuBowlingStatus == 3, "bowling ammo changed with options off");
    MGH_ASSERT(GameInteractor_Should(VB_SET_FROG_OCARINA_GAME_TIME_LIMIT, true, &sFrogs[0], 40),
               "frogs time limit overridden with options off");
    MGH_ASSERT(sFrogs[0].frogSongTimer == 77, "frogs timer changed with options off");
    MGH_ASSERT(GameInteractor_Should(VB_FROGS_OCARINA_GAME_TIMER_TICK, true), "frogs timer tick stopped with options off");
    MGH_ASSERT(GameInteractor_Should(VB_PLAY_FROG_OCARINA_GAME, true, &sFrogs[0]), "frogs game won with options off");
    MGH_ASSERT(sFrogs[0].actor.textId == 0x1234 && sPlay.msgCtx.msgMode == 5 && sFrogs[0].actionFunc == nullptr,
               "frogs instant win changed state with options off");
    MGH_ASSERT(GameInteractor_Should(VB_PLAY_SHOOTING_GALLERY, true, &sGallery), "gallery won with options off");
    MGH_ASSERT(sGallery.signal == 1 && sGallery.hitCount == 4, "gallery instant win changed state with options off");
    MGH_ASSERT(GameInteractor_Should(VB_SHOOTING_GALLERY_SHUFFLE_ADULT_RUPEES, true),
               "gallery adult rupees fixed with options off");
    MGH_ASSERT(!GameInteractor_Should(VB_SHOOTING_GALLERY_SHUFFLE_ADULT_RUPEES, false),
               "gallery adult rupees forced on with options off");
    const s32 ages[] = { LINK_AGE_CHILD, LINK_AGE_ADULT };
    for (s32 age : ages) {
        gSaveContext.linkAge = age;
        s32 ammo = 15;
        MGH_ASSERT(GameInteractor_Should(VB_SET_SHOOTING_GALLERY_AMMO, true, &ammo) && ammo == 15,
                   "gallery ammo changed with options off");
    }
    return 0;
}

int CheckOffLeg(const char* leg) {
    BeginLeg();
    // Sub-options at non-default values, master options off.
    CVarSetInteger(kBowlSmall, 1);
    CVarSetInteger(kBowlBig, 1);
    CVarSetInteger(kBowlAmmo, 7);
    CVarSetInteger(kFrogModify, 3);
    CVarSetInteger(kFrogUnlimited, 1);
    CVarSetInteger(kFrogInstant, 1);
    CVarSetInteger(kGalInstant, 1);
    CVarSetInteger(kGalConstant, 1);
    CVarSetInteger(kGalAmmoChild, 9);
    CVarSetInteger(kGalAmmoAdult, 20);
    RunSitesUnder("*");
    return CheckVanilla(leg);
}

int RunLegs(void) {
    for (const auto& site : kSites) {
        char msg[160];
        snprintf(msg, sizeof(msg), "%s: %d registrar(s) expected under key \"%s\" in ShipInit's map", site.file,
                 site.count, site.key);
        MGH_ASSERT(CountSites(site.file, site.key) == site.count, msg);
    }
    // Only this row's registrars are in the map: nothing else shares the files.
    int rowSites = 0;
    for (const auto& entry : ShipInit::GetAll()["*"]) {
        rowSites += IsRowSite(entry) ? 1 : 0;
    }
    MGH_ASSERT(rowSites == 10, "the three hook files do not register exactly ten boot registrars");

    // (off, before) every master option off.
    MGH_ASSERT(CheckOffLeg("(off, before)") == 0, "(off, before): an answer is not vanilla");

    // (1) small cucco.
    BeginLeg();
    CVarSetInteger(kBowlCustomize, 1);
    CVarSetInteger(kBowlSmall, 1);
    RunSitesUnder(kBowlSmall);
    MGH_ASSERT(!GameInteractor_Should(VB_SPAWN_BOMBCHU_BOWLING_CUCCOS, true, 0), "(1): small cucco not refused");
    MGH_ASSERT(GameInteractor_Should(VB_SPAWN_BOMBCHU_BOWLING_CUCCOS, true, 1), "(1): big cucco refused");
    printf("[TEST] (1) small cucco: index 0 refused, index 1 kept\n");

    // (2) big cucco.
    BeginLeg();
    CVarSetInteger(kBowlCustomize, 1);
    CVarSetInteger(kBowlBig, 1);
    RunSitesUnder(kBowlBig);
    MGH_ASSERT(!GameInteractor_Should(VB_SPAWN_BOMBCHU_BOWLING_CUCCOS, true, 1), "(2): big cucco not refused");
    MGH_ASSERT(GameInteractor_Should(VB_SPAWN_BOMBCHU_BOWLING_CUCCOS, true, 0), "(2): small cucco refused");
    printf("[TEST] (2) big cucco: index 1 refused, index 0 kept\n");

    // (3) bowling ammo.
    BeginLeg();
    CVarSetInteger(kBowlCustomize, 1);
    CVarSetInteger(kBowlAmmo, 7);
    RunSitesUnder(kBowlAmmo);
    sPlay.bombchuBowlingStatus = 0;
    MGH_ASSERT(!GameInteractor_Should(VB_SET_BOMBCHU_BOWLING_AMMO, true), "(3): vanilla ammo not refused");
    MGH_ASSERT(sPlay.bombchuBowlingStatus == 7, "(3): bombchuBowlingStatus is not the option's value");
    printf("[TEST] (3) bowling ammo: status %d\n", sPlay.bombchuBowlingStatus);

    // (4) frogs time limit.
    BeginLeg();
    CVarSetInteger(kFrogCustomize, 1);
    CVarSetInteger(kFrogModify, 3);
    RunSitesUnder(kFrogModify);
    sFrogs[0].frogSongTimer = 0;
    MGH_ASSERT(!GameInteractor_Should(VB_SET_FROG_OCARINA_GAME_TIME_LIMIT, true, &sFrogs[0], 40),
               "(4): vanilla time limit not refused");
    MGH_ASSERT(sFrogs[0].frogSongTimer == 40 * 3, "(4): frogSongTimer is not limit * factor for limit 40");
    MGH_ASSERT(!GameInteractor_Should(VB_SET_FROG_OCARINA_GAME_TIME_LIMIT, true, &sFrogs[0], 17) &&
                   sFrogs[0].frogSongTimer == 17 * 3,
               "(4): frogSongTimer is not limit * factor for limit 17");
    printf("[TEST] (4) frogs time limit: timer %d\n", sFrogs[0].frogSongTimer);

    // (5) frogs timer tick.
    BeginLeg();
    CVarSetInteger(kFrogCustomize, 1);
    CVarSetInteger(kFrogUnlimited, 1);
    RunSitesUnder(kFrogUnlimited);
    MGH_ASSERT(!GameInteractor_Should(VB_FROGS_OCARINA_GAME_TIMER_TICK, true), "(5): timer tick not refused");
    printf("[TEST] (5) frogs timer tick refused\n");

    // (6) frogs instant win. EnFr_SetupReward walks sEnFrPointers.frogs[0..4]
    // (ResetFixtures points them at the zeroed static frogs).
    BeginLeg();
    CVarSetInteger(kFrogCustomize, 1);
    CVarSetInteger(kFrogInstant, 1);
    RunSitesUnder(kFrogInstant);
    for (int i = 0; i < 5; i++) {
        sFrogs[i].isButterflyDrawn = true;
    }
    sPlay.msgCtx.msgMode = MSGMODE_NONE;
    MGH_ASSERT(!GameInteractor_Should(VB_PLAY_FROG_OCARINA_GAME, true, &sFrogs[0]), "(6): the frogs game was not won");
    MGH_ASSERT(sFrogs[0].actor.textId == 0x40AC, "(6): textId is not 0x40AC");
    MGH_ASSERT(sPlay.msgCtx.msgMode == MSGMODE_PAUSED, "(6): msgMode is not MSGMODE_PAUSED");
    MGH_ASSERT(sFrogs[0].actionFunc != nullptr, "(6): EnFr_SetupReward did not set an action");
    for (int i = 0; i < 5; i++) {
        MGH_ASSERT(!sFrogs[i].isButterflyDrawn, "(6): a butterfly is still drawn after the reward set-up");
    }
    printf("[TEST] (6) frogs instant win: textId 0x%X, msgMode %d\n", sFrogs[0].actor.textId, sPlay.msgCtx.msgMode);

    // (7) gallery instant win.
    BeginLeg();
    CVarSetInteger(kGalCustomize, 1);
    CVarSetInteger(kGalInstant, 1);
    RunSitesUnder(kGalInstant);
    MGH_ASSERT(!GameInteractor_Should(VB_PLAY_SHOOTING_GALLERY, true, &sGallery), "(7): the gallery was not won");
    MGH_ASSERT(sGallery.hitCount == 10, "(7): hitCount is not 10");
    MGH_ASSERT(sGallery.signal == ENSYATEKI_END, "(7): signal is not ENSYATEKI_END");
    printf("[TEST] (7) gallery instant win: hitCount %d, signal %d\n", sGallery.hitCount, sGallery.signal);

    // (8) constant adult.
    BeginLeg();
    CVarSetInteger(kGalCustomize, 1);
    CVarSetInteger(kGalConstant, 1);
    RunSitesUnder(kGalConstant);
    MGH_ASSERT(!GameInteractor_Should(VB_SHOOTING_GALLERY_SHUFFLE_ADULT_RUPEES, true), "(8): adult shuffle not refused");
    printf("[TEST] (8) constant adult: shuffle refused\n");

    // (9) child ammo: only a child's ammo changes.
    BeginLeg();
    CVarSetInteger(kGalCustomize, 1);
    CVarSetInteger(kGalAmmoChild, 9);
    RunSitesUnder(kGalAmmoChild);
    s32 ammo = 15;
    gSaveContext.linkAge = LINK_AGE_CHILD;
    MGH_ASSERT(GameInteractor_Should(VB_SET_SHOOTING_GALLERY_AMMO, true, &ammo) && ammo == 9,
               "(9): a child's ammo is not the option's value");
    ammo = 15;
    gSaveContext.linkAge = LINK_AGE_ADULT;
    MGH_ASSERT(GameInteractor_Should(VB_SET_SHOOTING_GALLERY_AMMO, true, &ammo) && ammo == 15,
               "(9): an adult's ammo changed with only the child option set");
    printf("[TEST] (9) child ammo 9, adult untouched\n");

    // (10) adult ammo: only an adult's ammo changes.
    BeginLeg();
    CVarSetInteger(kGalCustomize, 1);
    CVarSetInteger(kGalAmmoAdult, 20);
    RunSitesUnder(kGalAmmoAdult);
    ammo = 15;
    gSaveContext.linkAge = LINK_AGE_ADULT;
    MGH_ASSERT(GameInteractor_Should(VB_SET_SHOOTING_GALLERY_AMMO, true, &ammo) && ammo == 20,
               "(10): an adult's ammo is not the option's value");
    ammo = 15;
    gSaveContext.linkAge = LINK_AGE_CHILD;
    MGH_ASSERT(GameInteractor_Should(VB_SET_SHOOTING_GALLERY_AMMO, true, &ammo) && ammo == 15,
               "(10): a child's ammo changed with only the adult option set");
    printf("[TEST] (10) adult ammo 20, child untouched\n");

    // (off, after) the hooks go back to vanilla when the options go off.
    MGH_ASSERT(CheckOffLeg("(off, after)") == 0, "(off, after): an answer is not vanilla");
    return 0;
}

} // namespace

extern "C" int OoT_MinigameHookify_RunHeadless(void) {
    if (GameInteractor::Instance == nullptr) {
        GameInteractor::Instance = new GameInteractor();
    }
    const GameId prevGame = Context_GetCurrentGame();
    // The GameInteractor dispatch is gated off only under GAME_MM.
    Context_SetCurrentGame(GAME_OOT);

    int saved[kAllKeyCount];
    for (int i = 0; i < kAllKeyCount; i++) {
        saved[i] = CVarGetInteger(kAllKeys[i], kUnset);
    }
    PlayState* const savedPlayState = OoT_gPlayState;
    const s32 savedLinkAge = gSaveContext.linkAge;
    const EnFrPointers savedFrogs = sEnFrPointers;

    OoT_gPlayState = &sPlay;
    const int rc = RunLegs();

    // Leave global state as it was, on pass or fail: the CVars (cleared, never
    // stored, when they were unset), the play state, the age and the frog
    // pointers, then the hooks to match them.
    for (int i = 0; i < kAllKeyCount; i++) {
        if (saved[i] == kUnset) {
            CVarClear(kAllKeys[i]);
        } else {
            CVarSetInteger(kAllKeys[i], saved[i]);
        }
    }
    OoT_gPlayState = savedPlayState;
    gSaveContext.linkAge = savedLinkAge;
    sEnFrPointers = savedFrogs;
    RunSitesUnder("*");
    Context_SetCurrentGame(prevGame);

    if (rc == 0) {
        printf("[TEST] oot-minigame-hookify: PASS\n");
    }
    return rc;
}

#endif // RSBS_SINGLE_EXECUTABLE
