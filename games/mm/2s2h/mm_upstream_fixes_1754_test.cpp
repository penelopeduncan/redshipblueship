/**
 * @file mm_upstream_fixes_1754_test.cpp
 * ROM-free, display-free lock for the eight 2Ship fixes ported from #1754 and #1755. CTest label "redship", row
 * MMUpstreamFixes1754 in CMake/SingleExecutable.cmake (beside MMRuntimeParity), dispatch "mm-upstream-fixes-1754" in
 * src/common/test_runner.cpp.
 *
 * LEGS (each names the production line whose removal turns it red):
 *   1. Pause Owl Warp (2Ship afd85345d). PauseOwlWarp_CanPlaySoaring: on a vanilla file, Soaring and the Ocarina pass,
 *      either alone fails; on a rando file, Soaring, the Ocarina and C-Down, C-Left and C-Up pass, any one of the three
 *      buttons missing fails, no Ocarina fails (Shuffle Ocarina), and a world that shuffles no buttons (all five flags
 *      held from creation) passes. Then PauseOwlWarp_IsOwlWarpEnabled over a zeroed stand-in play state, with the
 *      option on and an owl active: false without the Ocarina, true with it. Shared buttons are covered by
 *      MMSharedOcarinaButtons: their give (GameExports_SingleExe.cpp, MM_SharedButtonGive) writes the same randoInf bit
 *      this helper reads.
 *   2. Saria's Song text (2Ship f443698ea). With RO_SHUFFLE_SONG_SARIA on a rando file and SariasSongHint() armed, a
 *      0x1B95 open while Saria's Song was not played keeps loadFromMessageTable and writes no custom message. Control:
 *      after VB_SONG_AVAILABLE_TO_PLAY for Saria's Song with the song owned, the next 0x1B95 open loads the custom
 *      prompt (load false, a message written), so the hook is armed and the first assertion is not vacuous.
 *   3. Soulless aliens (2Ship #1927). IsEnemySoulMissing: an alien without its soul is missing, with it is not; the
 *      invasion handler and Romani on night 1 (both ACTOR_EN_INVADEPOH, no soul of their own) are not; a Wolfos
 *      without its soul is.
 *   4. Telescope Invert Y (2Ship #1927). GameInteractor_InvertControl(GI_INVERT_TELESCOPE_Y) follows
 *      gEnhancements.Camera.FirstPerson.InvertY (-1 on its default, 1 when 0); GI_INVERT_TELESCOPE_X follows InvertX.
 *   5. En_Rail_Skb (2Ship #1702). Its profile carries a reset function, which MM_Actor_FreeOverlay calls.
 *   6. Ammo Buyback (2Ship a5cbfdf1c). AmmoBuyback_SalePriceFor (header-only, AmmoBuybackPrice.h): Half Price
 *      halves bombs, never arrows.
 *   7. Auto Bank Deposit (2Ship 24df3b56c). AutoBankDeposit_ReachesNotificationMilestone (header-only,
 *      AutoBankDepositMilestones.h) at each milestone's edges. Both decisions are inline so this row names nothing
 *      AmmoBuyback.cpp or AutoBankDeposit.cpp defines and cannot un-elide either registrar TU.
 *   8. Fix Deku Butler Shock Animation (2Ship f7ccda83a). With the key off, VB_DEKU_BUTLER_CHANGE_SHOCK_ANIMATION
 * leaves changeAnim alone (control); on, it clears changeAnim while the butler is in SHOCK_LOOP (18) and leaves it in
 * any other animation. The TU is reached only through ShipInit (nothing it defines is named here, so this reference
 *      cannot un-elide it; mm-registrar-coverage's W6-FIXES probe holds the link).
 * The Swamp archery gate (2Ship b41f365aa) is held by mm-registrar-coverage's Skip Misc Interactions probe.
 *
 * CLEANUP. The SaveContext image, MM_gPlayState, sOcarinaSongFanfares[OCARINA_SONG_SARIAS] and every key this row
 * touches are put back; SariasSongHint() is re-run under the restored save, so the row is order-free. It registers
 * hooks while it runs, so it sits after mm-registrar-coverage in the table.
 *
 * WHAT THIS DOES NOT COVER. The draw-time grayscale cue, the telescope's pitch in z_player.c, the sound-effect path in
 * sfx.c, the toast's look and the butler's animation on screen: see the pull request.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
/** The S2H registries and the ShipInit split this lock drives exist only in the single-exe build. */
extern "C" int MM_UpstreamFixes1754_RunHeadless(void) {
    printf("[TEST] mm-upstream-fixes-1754: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <cstdlib>
#include <cstring>
#include <memory>

#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/Context.h>

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp"
#include "2s2h/Enhancements/Enhancements.h"
#include "2s2h/Enhancements/Items/AmmoBuybackPrice.h"
#include "2s2h/Enhancements/Songs/Songs.h"
#include "2s2h/Enhancements/Timesavers/AutoBankDepositMilestones.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/ActorBehavior/Souls.h"
#include "2s2h/Rando/MiscBehavior/MiscBehavior.h"

extern "C" {
#include "variables.h"
#include "functions.h"
#include <z64ocarina.h>
#include "overlays/actors/ovl_En_Invadepoh/z_en_invadepoh.h"
#include "overlays/actors/ovl_En_Dno/z_en_dno.h"
extern s16 sOcarinaSongFanfares[17];
}

namespace {

#define UF1754_ASSERT(cond, code, msg)                                                                          \
    do {                                                                                                        \
        if (!(cond)) {                                                                                          \
            printf("[TEST] FAIL(%d): mm-upstream-fixes-1754: %s (%s:%d)\n", (code), (msg), __FILE__, __LINE__); \
            return (code);                                                                                      \
        }                                                                                                       \
    } while (0)

constexpr const char* kUf1754PauseOwlWarpCVar = "gEnhancements.Songs.PauseOwlWarp";
constexpr const char* kUf1754InvertXCVar = "gEnhancements.Camera.FirstPerson.InvertX";
constexpr const char* kUf1754InvertYCVar = "gEnhancements.Camera.FirstPerson.InvertY";
constexpr const char* kUf1754MirroredWorldCVar = "gModes.MirroredWorld.State";
constexpr const char* kUf1754DekuButlerCVar = "gEnhancements.Fixes.DekuButlerFixShockLoopAnimation";

/** One key's state on entry, put back on the way out (re-driven through ShipInit only when the row drove it). */
struct Uf1754SavedKey {
    const char* key;
    bool reinit;
    bool existed;
    int32_t value;

    Uf1754SavedKey(const char* k, bool ri) : key(k), reinit(ri), existed(CVarExists(k)), value(CVarGetInteger(k, 0)) {
    }
    void Restore() const {
        if (existed) {
            CVarSetInteger(key, value);
        } else {
            CVarClear(key);
        }
        if (reinit) {
            ShipInit::Init(key);
        }
    }
};

/** randoInf written directly: Flags_SetRandoInf would also dispatch OnFlagSet hooks. */
void Uf1754SetRandoInf(s32 flag, bool on) {
    u16& word = gSaveContext.save.shipSaveInfo.rando.randoInf[flag >> 4];
    if (on) {
        word |= (u16)(1 << (flag & 0xF));
    } else {
        word &= (u16) ~(1 << (flag & 0xF));
    }
}

void Uf1754SetButtons(bool a, bool cDown, bool cRight, bool cLeft, bool cUp) {
    Uf1754SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_A, a);
    Uf1754SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_C_DOWN, cDown);
    Uf1754SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_C_RIGHT, cRight);
    Uf1754SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_C_LEFT, cLeft);
    Uf1754SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_C_UP, cUp);
}

void Uf1754SetSoaringAndOcarina(bool soaring, bool ocarina) {
    if (soaring) {
        SET_QUEST_ITEM(QUEST_SONG_SOARING);
    } else {
        REMOVE_QUEST_ITEM(QUEST_SONG_SOARING);
    }
    INV_CONTENT(ITEM_OCARINA_OF_TIME) = ocarina ? ITEM_OCARINA_OF_TIME : ITEM_NONE;
}

// ---- Leg 1: Pause Owl Warp ---------------------------------------------------
int Uf1754RunPauseOwlWarpLegs() {
    // Vanilla file: no button term.
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_VANILLA;
    Uf1754SetButtons(false, false, false, false, false);
    Uf1754SetSoaringAndOcarina(true, true);
    UF1754_ASSERT(PauseOwlWarp_CanPlaySoaring(), 10, "vanilla file: Soaring and the Ocarina do not allow the warp");
    Uf1754SetSoaringAndOcarina(true, false);
    UF1754_ASSERT(!PauseOwlWarp_CanPlaySoaring(), 11,
                  "vanilla file: Soaring without the Ocarina allows the warp (2Ship afd85345d's inventory check)");
    Uf1754SetSoaringAndOcarina(false, true);
    UF1754_ASSERT(!PauseOwlWarp_CanPlaySoaring(), 12, "vanilla file: the Ocarina without Soaring allows the warp");

    // Rando file, buttons shuffled.
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_OCARINA_BUTTONS] = 1;
    Uf1754SetSoaringAndOcarina(true, true);
    Uf1754SetButtons(false, true, false, true, true);
    UF1754_ASSERT(PauseOwlWarp_CanPlaySoaring(), 13,
                  "rando file: Soaring, the Ocarina, C-Down, C-Left and C-Up do not allow the warp");
    const struct {
        s32 flag;
        const char* name;
    } soaringButtons[] = {
        { RANDO_INF_OBTAINED_OCARINA_BUTTON_C_DOWN, "C-Down" },
        { RANDO_INF_OBTAINED_OCARINA_BUTTON_C_LEFT, "C-Left" },
        { RANDO_INF_OBTAINED_OCARINA_BUTTON_C_UP, "C-Up" },
    };
    for (const auto& button : soaringButtons) {
        Uf1754SetRandoInf(button.flag, false);
        if (PauseOwlWarp_CanPlaySoaring()) {
            printf("[TEST] FAIL(14): mm-upstream-fixes-1754: rando file without %s still allows the warp - the gate "
                   "does not ask canPlaySong(OCARINA_SONG_SOARING) (2Ship afd85345d)\n",
                   button.name);
            return 14;
        }
        Uf1754SetRandoInf(button.flag, true);
    }
    Uf1754SetSoaringAndOcarina(true, false);
    UF1754_ASSERT(!PauseOwlWarp_CanPlaySoaring(), 15,
                  "rando file: Soaring and its buttons without the Ocarina (Shuffle Ocarina) allow the warp");

    // Rando file, buttons not shuffled: every flag is held from file creation (StartingItems).
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_OCARINA_BUTTONS] = 0;
    Uf1754SetButtons(true, true, true, true, true);
    Uf1754SetSoaringAndOcarina(true, true);
    UF1754_ASSERT(PauseOwlWarp_CanPlaySoaring(), 16,
                  "rando file without the button shuffle: Soaring and the Ocarina do not allow the warp");

    // The gate itself, over a zeroed stand-in play state.
    CVarSetInteger(kUf1754PauseOwlWarpCVar, 1);
    gSaveContext.save.saveInfo.playerData.owlActivationFlags = 1;
    Uf1754SetSoaringAndOcarina(true, false);
    UF1754_ASSERT(!PauseOwlWarp_IsOwlWarpEnabled(), 17,
                  "PauseOwlWarp_IsOwlWarpEnabled allows the warp without the Ocarina - it does not call "
                  "PauseOwlWarp_CanPlaySoaring");
    Uf1754SetSoaringAndOcarina(true, true);
    UF1754_ASSERT(PauseOwlWarp_IsOwlWarpEnabled(), 18,
                  "PauseOwlWarp_IsOwlWarpEnabled refuses the warp with the option on, an owl, Soaring, the Ocarina and "
                  "its buttons");
    return 0;
}

// ---- Leg 2: Saria's Song restricted-song text -----------------------------------
int Uf1754RunSariasSongLegs(PlayState* play) {
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONG_SARIA] = 1;
    SET_QUEST_ITEM(QUEST_SONG_SARIA);
    Rando::MiscBehavior::SariasSongHint(); // arms the hooks; state 0

    play->msgCtx.msgLength = 0;
    u16 textId = 0x1B95;
    bool load = true;
    MM_GameHooks_ExecuteOnOpenText(&textId, &load);
    UF1754_ASSERT(load && play->msgCtx.msgLength == 0, 20,
                  "a restricted song opened 0x1B95 while Saria's Song was not played, and the hook replaced the "
                  "vanilla text with an empty custom message (2Ship f443698ea)");

    // Control: playing Saria's Song (owned) moves the hook to state 1, and the next open loads its prompt.
    uint8_t song = OCARINA_SONG_SARIAS;
    (void)MM_GameHooks_ExecuteVBShould(VB_SONG_AVAILABLE_TO_PLAY, false, &song);
    textId = 0x1B95;
    load = true;
    MM_GameHooks_ExecuteOnOpenText(&textId, &load);
    UF1754_ASSERT(!load && play->msgCtx.msgLength != 0, 21,
                  "after Saria's Song was played the 0x1B95 hook did not load its prompt - it is not armed, so leg 20 "
                  "would be vacuous");
    return 0;
}

// ---- Leg 3: soulless aliens -------------------------------------------------------
int Uf1754RunSoulLegs() {
    const s32 alienSoul = SOUL_RI_TO_RANDO_INF(RI_SOUL_ENEMY_ALIEN);
    const s32 wolfosSoul = SOUL_RI_TO_RANDO_INF(RI_SOUL_ENEMY_WOLFOS);
    Uf1754SetRandoInf(alienSoul, false);
    Uf1754SetRandoInf(wolfosSoul, false);

    Actor actor{};
    actor.id = ACTOR_EN_INVADEPOH;
    actor.params = EN_INVADEPOH_PARAMS(0, EN_INVADEPOH_TYPE_ALIEN, 0);
    UF1754_ASSERT(IsEnemySoulMissing(&actor), 30,
                  "an alien without its soul is not reported soulless - the gray cue skips every ACTOR_EN_INVADEPOH "
                  "(2Ship #1927)");
    actor.params = EN_INVADEPOH_PARAMS(0, EN_INVADEPOH_TYPE_INVASION_HANDLER, 0);
    UF1754_ASSERT(!IsEnemySoulMissing(&actor), 31, "the invasion handler (ACTOR_EN_INVADEPOH) is reported soulless");
    actor.params = EN_INVADEPOH_PARAMS(0, EN_INVADEPOH_TYPE_ROMANI_NIGHT_1, 0);
    UF1754_ASSERT(!IsEnemySoulMissing(&actor), 32, "Romani, night 1 (ACTOR_EN_INVADEPOH) is reported soulless");
    Uf1754SetRandoInf(alienSoul, true);
    actor.params = EN_INVADEPOH_PARAMS(0, EN_INVADEPOH_TYPE_ALIEN, 0);
    UF1754_ASSERT(!IsEnemySoulMissing(&actor), 33, "an alien with its soul found is reported soulless");

    Actor wolfos{};
    wolfos.id = ACTOR_EN_WF;
    UF1754_ASSERT(IsEnemySoulMissing(&wolfos), 34, "a Wolfos without its soul is not reported soulless");
    return 0;
}

// ---- Leg 4: telescope inversion -----------------------------------------------------
int Uf1754RunTelescopeLegs() {
    CVarSetInteger(kUf1754MirroredWorldCVar, 0);
    CVarClear(kUf1754InvertYCVar); // default 1: inverted
    UF1754_ASSERT(GameInteractor_InvertControl(GI_INVERT_TELESCOPE_Y) == -1, 40,
                  "GI_INVERT_TELESCOPE_Y does not follow Invert Aiming Y's default (2Ship #1927)");
    CVarSetInteger(kUf1754InvertYCVar, 0);
    UF1754_ASSERT(GameInteractor_InvertControl(GI_INVERT_TELESCOPE_Y) == 1, 41,
                  "GI_INVERT_TELESCOPE_Y is inverted with Invert Aiming Y off");
    CVarSetInteger(kUf1754InvertXCVar, 1);
    UF1754_ASSERT(GameInteractor_InvertControl(GI_INVERT_TELESCOPE_X) == -1, 42,
                  "GI_INVERT_TELESCOPE_X does not follow Invert Aiming X (2Ship #1927)");
    CVarSetInteger(kUf1754InvertXCVar, 0);
    UF1754_ASSERT(GameInteractor_InvertControl(GI_INVERT_TELESCOPE_X) == 1, 43,
                  "GI_INVERT_TELESCOPE_X is inverted with Invert Aiming X off");
    return 0;
}

// ---- Leg 5: En_Rail_Skb reset ----------------------------------------------------------
int Uf1754RunRailSkbLeg() {
    const ActorProfile* profile = gActorOverlayTable[ACTOR_EN_RAIL_SKB].profile;
    UF1754_ASSERT(profile != nullptr, 50, "gActorOverlayTable has no profile for ACTOR_EN_RAIL_SKB");
    UF1754_ASSERT(profile->reset != nullptr, 51,
                  "En_Rail_Skb_Profile has no reset function, so its spawn counter survives an overlay free (2Ship "
                  "#1702)");
    profile->reset(); // the counter is 0 between spawns, so this changes nothing
    return 0;
}

// ---- Legs 6-7: Ammo Buyback and Auto Bank Deposit ----------------------------------------
int Uf1754RunPriceAndBankLegs() {
    UF1754_ASSERT(AmmoBuyback_SalePriceFor(AMMO_BUYBACK_HALF_PRICE, ITEM_BOW, 30, 1) == 30, 60,
                  "Half Price halves 30 arrows - arrows always sell at full price (2Ship a5cbfdf1c)");
    UF1754_ASSERT(AmmoBuyback_SalePriceFor(AMMO_BUYBACK_HALF_PRICE, ITEM_BOMB, 5, 3) == 8, 61,
                  "Half Price does not halve 5 bombs (15 rupees) to 8, rounded up");
    UF1754_ASSERT(AmmoBuyback_SalePriceFor(AMMO_BUYBACK_FULL_PRICE, ITEM_BOW, 30, 1) == 30, 62,
                  "Full Price does not sell 30 arrows for 30 rupees");

    const struct {
        s16 before;
        s16 after;
        bool notify;
    } deposits[] = {
        { 0, 99, false },    { 99, 100, true },    { 100, 150, false },   { 150, 200, true },
        { 450, 1200, true }, { 4999, 5000, true }, { 5000, 5000, false },
    };
    for (const auto& d : deposits) {
        if (AutoBankDeposit_ReachesNotificationMilestone(d.before, d.after) != d.notify) {
            printf("[TEST] FAIL(70): mm-upstream-fixes-1754: a deposit from %d to %d %s - Auto Bank Deposit notifies "
                   "only when the balance reaches 100, 200, 500, 1000 or 5000 (2Ship 24df3b56c)\n",
                   d.before, d.after, d.notify ? "raised no notification" : "raised a notification");
            return 70;
        }
    }
    return 0;
}

// ---- Leg 8: Fix Deku Butler Shock Animation ----------------------------------------------
bool Uf1754ButlerChangesAnim(EnDno* dno, s32 animIndex) {
    dno->animIndex = animIndex;
    u8 changeAnim = true;
    (void)GameInteractor_Should(VB_DEKU_BUTLER_CHANGE_SHOCK_ANIMATION, true, dno, &changeAnim);
    return changeAnim != 0;
}

int Uf1754RunDekuButlerLegs(EnDno* dno) {
    const s32 kShockLoop = 18; // EN_DNO_ANIM_SHOCK_LOOP (z_en_dno.c)
    CVarClear(kUf1754DekuButlerCVar);
    ShipInit::Init(kUf1754DekuButlerCVar);
    UF1754_ASSERT(Uf1754ButlerChangesAnim(dno, kShockLoop), 80,
                  "with Fix Deku Butler Shock Animation off the shock-loop animation is kept - the leg below would be "
                  "vacuous");
    CVarSetInteger(kUf1754DekuButlerCVar, 1);
    ShipInit::Init(kUf1754DekuButlerCVar);
    UF1754_ASSERT(!Uf1754ButlerChangesAnim(dno, kShockLoop), 81,
                  "with Fix Deku Butler Shock Animation on, cue 2 still restarts the shock animation while the butler "
                  "is in SHOCK_LOOP (2Ship f7ccda83a)");
    UF1754_ASSERT(Uf1754ButlerChangesAnim(dno, 1), 82,
                  "with Fix Deku Butler Shock Animation on, an animation other than SHOCK_LOOP no longer changes");
    CVarClear(kUf1754DekuButlerCVar);
    ShipInit::Init(kUf1754DekuButlerCVar);
    return 0;
}

} // namespace

extern "C" int MM_UpstreamFixes1754_RunHeadless(void) {
    printf("[TEST] mm-upstream-fixes-1754: the 2Ship fixes ported from #1754 and #1755\n");

    auto ctx = Ship::Context::GetInstance();
    UF1754_ASSERT(ctx != nullptr, 1, "Ship::Context singleton missing - run the shared bring-up first");
    UF1754_ASSERT(ctx->GetConsoleVariables() != nullptr, 1, "ConsoleVariables missing - the legs read CVars");

    auto saved = std::make_unique<uint8_t[]>(sizeof(gSaveContext));
    memcpy(saved.get(), &gSaveContext, sizeof(gSaveContext));
    PlayState* const savedPlay = MM_gPlayState;
    const s16 savedSariaFanfare = sOcarinaSongFanfares[OCARINA_SONG_SARIAS];
    const Uf1754SavedKey savedKeys[] = {
        Uf1754SavedKey(kUf1754PauseOwlWarpCVar, false), Uf1754SavedKey(kUf1754InvertXCVar, false),
        Uf1754SavedKey(kUf1754InvertYCVar, false),      Uf1754SavedKey(kUf1754MirroredWorldCVar, false),
        Uf1754SavedKey(kUf1754DekuButlerCVar, true),
    };

    PlayState* play = static_cast<PlayState*>(calloc(1, sizeof(PlayState)));
    EnDno* dno = static_cast<EnDno*>(calloc(1, sizeof(EnDno)));
    int rc = 1;
    if (play != nullptr && dno != nullptr) {
        MM_gPlayState = play;
        rc = Uf1754RunPauseOwlWarpLegs();
        if (rc == 0) {
            rc = Uf1754RunSariasSongLegs(play);
        }
        if (rc == 0) {
            rc = Uf1754RunSoulLegs();
        }
        if (rc == 0) {
            rc = Uf1754RunTelescopeLegs();
        }
        if (rc == 0) {
            rc = Uf1754RunRailSkbLeg();
        }
        if (rc == 0) {
            rc = Uf1754RunPriceAndBankLegs();
        }
        if (rc == 0) {
            rc = Uf1754RunDekuButlerLegs(dno);
        }
    } else {
        printf("[TEST] FAIL(1): mm-upstream-fixes-1754: could not allocate the stand-in play state and Deku Butler\n");
    }

    // ---- Cleanup (runs on pass and on fail) ----------------------------------
    memcpy(&gSaveContext, saved.get(), sizeof(gSaveContext));
    Rando::MiscBehavior::SariasSongHint(); // re-armed (or disarmed) under the restored save
    sOcarinaSongFanfares[OCARINA_SONG_SARIAS] = savedSariaFanfare;
    MM_gPlayState = savedPlay;
    for (const Uf1754SavedKey& key : savedKeys) {
        key.Restore();
    }
    free(dno);
    free(play);

    if (rc != 0) {
        return rc;
    }
    printf("[TEST] mm-upstream-fixes-1754: PASS (Pause Owl Warp asks for Soaring, the Ocarina and its buttons; a "
           "restricted song keeps its text; soulless aliens alone are cued; the telescope follows both inversions; "
           "En_Rail_Skb resets; arrows sell at full price; bank toasts at milestones; the Deku Butler keeps its "
           "shock loop)\n");
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
