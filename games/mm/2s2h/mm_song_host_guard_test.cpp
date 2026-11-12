/**
 * @file mm_song_host_guard_test.cpp
 * Majora's Mask's song checks as cross-game hosts: CheckQueue's
 * delivered-song-host guard and the three-day reset (#1542 S0).
 *
 *   mm-song-host-guard (redship tier, ROM-free, generates nothing). Every leg
 *   drives the REAL Rando::MiscBehavior::CheckQueue over the live save's check
 *   table, with one host planted through the pinned placement table
 *   (Combo_SetForeignPlacement), and reads the GIEvent queue it leaves. Nothing
 *   here reads the song-host switch: the guard does not either.
 *     (g1) A planted song host (the New Wave Bossa Nova) that is obtained and
 *          eligible again queues NOTHING, ends !eligible and cycleObtained, and
 *          records nothing. Red without the guard (the foreign branch queues
 *          its event) or with a guard that forgets to write through the array.
 *     (g2) RED HALF: the same host NOT yet obtained queues exactly one event,
 *          the foreign branch's (param = the check, get-item cutscene on). Red on
 *          a guard that eats the first delivery.
 *     (g3) A planted, obtained, eligible Tier A chest still queues its event: the
 *          guard is the song checks' only (red on a guard keyed on anything but
 *          the song-check id).
 *     (g4) A song check holding NO placement, obtained and eligible (a native MM
 *          item delivered on an earlier cycle) is no delivered song host: a
 *          guard that ignored the placement would swallow 2Ship's own re-give.
 *   mm-song-host-guard's reset legs run the Song of Time's reset headless: the
 *   REAL Sram_SaveEndOfCycle over a zeroed stand-in PlayState with the rando's
 *   Before/AfterEndOfCycleSave registered the way a rando load registers them
 *   (Rando::MiscBehavior::OnFileLoad under SAVETYPE_RANDO), as
 *   mm_cycle_keep_items_test.cpp does.
 *     (r1) THE STARTING SONG OF HEALING, planted with the RI_JUNK cover a
 *          crossing host holds, pairing live: the real foreign give
 *          (GiveForeignCheck) records ONE crossing and marks it delivered; after
 *          the reset it is not eligible (IsRenewable(RI_JUNK) is false), a
 *          CheckQueue tick queues nothing, and a second give records nothing
 *          (the once-per-host gate). Red if a reset re-arms Healing's crossing
 *          host or if a second delivery records a second copy.
 *     (r2) THE NEW WAVE BOSSA NOVA, planted, delivered: the reset clears
 *          WEEKEVENTREG_20_40 without the hook; setting it again through MM's
 *          real flag hook (Rando::MiscBehavior::OnFlagSet, the function the
 *          rando's COND_HOOK registers) re-arms the check, and CheckQueue then
 *          consumes it: no event, !eligible, no record. Red without the guard.
 *   Every piece of state touched (the whole MM SaveContext, the combo record,
 *   the placement table, the crossing store and the shared outbox, the event
 *   queue, CheckQueue's latch, the rando registrars) is restored or cleared
 *   before the row returns, on every return path.
 */

#include "global.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <variant>
#include <vector>

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The combo surfaces are single-exe only; test_runner.cpp declares this
// unconditionally, so a standalone 2ship build links.
extern "C" int MM_SongHostGuard_Run(uint8_t, uint16_t) {
    return 1;
}
#else

#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/Foreign.h"
#include "2s2h/Rando/MiscBehavior/MiscBehavior.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/ShipInit.hpp"
#include "context.h"
#include "crossing_store.h"
#include "foreign_items.h"
#include "shared_items.h"

namespace {

const char* const kSongGuardRow = "mm-song-host-guard";

#define SHG_ASSERT(cond, msg)                                                                              \
    do {                                                                                                   \
        if (!(cond)) {                                                                                     \
            printf("[TEST] FAIL (%s): %s  [%s] (%s:%d)\n", kSongGuardRow, msg, #cond, __FILE__, __LINE__); \
            return 1;                                                                                      \
        }                                                                                                  \
    } while (0)

/** Everything a leg touches, put back on every exit. */
struct SongGuardRestore {
    std::unique_ptr<SaveContext> save;
    std::unique_ptr<ComboContext> combo;
    std::vector<GIEvent> queue;
    SongGuardRestore()
        : save(std::make_unique<SaveContext>()), combo(std::make_unique<ComboContext>()), queue(MM_GameEvents_Queue()) {
        memcpy(save.get(), &gSaveContext, sizeof(SaveContext));
        memcpy(combo.get(), &gComboCtx, sizeof(ComboContext));
    }
    ~SongGuardRestore() {
        Rando::MiscBehavior::CheckQueueReset();
        // Disarm the rando registrars a leg armed, under the restored save's type.
        memcpy(&gSaveContext, save.get(), sizeof(SaveContext));
        gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_VANILLA;
        Rando::MiscBehavior::OnFileLoad();
        ShipInit::Init("IS_RANDO");
        memcpy(&gSaveContext, save.get(), sizeof(SaveContext));
        Combo_Crossings_Clear();
        Combo_ClearSharedItemOutbox();
        memcpy(&gComboCtx, combo.get(), sizeof(ComboContext));
        MM_GameEvents_Queue() = queue;
    }
};

/** A live rando save (no flash slot, GAMEMODE_NORMAL, human form, every item
 *  slot empty) with no check eligible, the rando's registrars armed as a rando
 *  load arms them, CheckQueue's latch clear, and no crossing anywhere. */
void SongGuardArmSave() {
    memset(&gSaveContext, 0, sizeof(SaveContext));
    memset(gSaveContext.save.saveInfo.inventory.items, ITEM_NONE, sizeof(gSaveContext.save.saveInfo.inventory.items));
    gSaveContext.fileNum = 0xFF;
    gSaveContext.flashSaveAvailable = true;
    gSaveContext.gameMode = GAMEMODE_NORMAL;
    gSaveContext.save.playerForm = PLAYER_FORM_HUMAN;
    gSaveContext.save.shipSaveInfo.saveType = SAVETYPE_RANDO;
    Rando::MiscBehavior::OnFileLoad();
    ShipInit::Init("IS_RANDO");
    Rando::MiscBehavior::CheckQueueReset();
    MM_GameEvents_Queue().clear();
    ComboContext_Init();
    Combo_ClearForeignPlacements();
    Combo_Crossings_Clear();
    Combo_ClearSharedItemOutbox();
}

/** Plant `item` (an OoT item) on MM check `id` as a crossing host holds it: the
 *  RI_JUNK cover, shuffled, the placement in the table. */
bool SongGuardPlant(RandoCheckId id, SharedItem item) {
    RandoSaveCheck& check = RANDO_SAVE_CHECKS[id];
    check.randoItemId = RI_JUNK;
    check.shuffled = true;
    check.skipped = false;
    return Combo_SetForeignPlacement((uint16_t)id, item) >= 0 && Rando::Foreign::IsForeignCheck(id);
}

void SongGuardSetBits(RandoCheckId id, bool obtained, bool cycleObtained, bool eligible) {
    RANDO_SAVE_CHECKS[id].obtained = obtained;
    RANDO_SAVE_CHECKS[id].cycleObtained = cycleObtained;
    RANDO_SAVE_CHECKS[id].eligible = eligible;
}

/** One CheckQueue frame from a clear latch; the events it queued. */
size_t SongGuardTick() {
    Rando::MiscBehavior::CheckQueueReset();
    MM_GameEvents_Queue().clear();
    Rando::MiscBehavior::CheckQueue();
    return MM_GameEvents_Queue().size();
}

/** The queued event, when it is CheckQueue's give for `id` with the cutscene on. */
bool SongGuardQueuedGiveFor(RandoCheckId id) {
    if (MM_GameEvents_Queue().size() != 1) {
        return false;
    }
    const GIEventGiveItem* give = std::get_if<GIEventGiveItem>(&MM_GameEvents_Queue().back());
    return give != nullptr && give->param == (s16)id;
}

bool SongGuardQueuedForeignGiveFor(RandoCheckId id) {
    if (!SongGuardQueuedGiveFor(id)) {
        return false;
    }
    const GIEventGiveItem* give = std::get_if<GIEventGiveItem>(&MM_GameEvents_Queue().back());
    return give->showGetItemCutscene && give->drawItem != nullptr;
}

/** The Song of Time's reset, headless (mm_cycle_keep_items_test.cpp's shape). */
void SongGuardPlaySongOfTime() {
    static PlayState sStandIn;
    memset(&sStandIn, 0, sizeof(sStandIn));
    PlayState* const previous = MM_gPlayState;
    MM_gPlayState = &sStandIn;
    Sram_SaveEndOfCycle(&sStandIn);
    MM_gPlayState = previous;
}

/** The first Tier A chest row (RCTYPE_CHEST with FLAG_CYCL_SCENE_CHEST). */
RandoCheckId SongGuardFirstChest() {
    for (auto& [id, row] : Rando::StaticData::Checks) {
        if (row.randoCheckType == RCTYPE_CHEST && row.flagType == FLAG_CYCL_SCENE_CHEST) {
            return id;
        }
    }
    return RC_UNKNOWN;
}

int SongGuardQueueLegs(SharedItem item) {
    const RandoCheckId song = RC_GREAT_BAY_COAST_NEW_WAVE_BOSSA_NOVA;
    const RandoCheckId chest = SongGuardFirstChest();
    SHG_ASSERT(chest != RC_UNKNOWN, "MM has no Tier A chest row to compare with");

    // (g1) delivered and armed again: consumed.
    SongGuardArmSave();
    SHG_ASSERT(SongGuardPlant(song, item), "(g1) the song host could not be planted");
    SongGuardSetBits(song, true, false, true);
    SHG_ASSERT(Rando::Foreign::IsDeliveredForeignSongHost(song), "(g1) the planted, obtained song host does not "
                                                                 "read as a delivered song host");
    const int recordsBefore = Combo_CountSharedItems(GAME_OOT, true);
    const size_t g1 = SongGuardTick();
    printf("[TEST] %s: (g1) delivered New Wave host armed again: %zu event(s), eligible %d, cycleObtained %d\n",
           kSongGuardRow, g1, RANDO_SAVE_CHECKS[song].eligible ? 1 : 0, RANDO_SAVE_CHECKS[song].cycleObtained ? 1 : 0);
    SHG_ASSERT(g1 == 0, "(g1) a delivered song host armed again queued a get-item event: the player would be shown "
                        "an item that is not given (the guard is missing)");
    SHG_ASSERT(!RANDO_SAVE_CHECKS[song].eligible && RANDO_SAVE_CHECKS[song].cycleObtained,
               "(g1) the guard did not consume the re-arm through the save's check table");
    SHG_ASSERT(Combo_CountSharedItems(GAME_OOT, true) == recordsBefore, "(g1) the consumed re-arm recorded a crossing");

    // (g2) RED HALF: not yet delivered, so the foreign branch queues its one event.
    SongGuardArmSave();
    SHG_ASSERT(SongGuardPlant(song, item), "(g2) the song host could not be planted");
    SongGuardSetBits(song, false, false, true);
    SHG_ASSERT(!Rando::Foreign::IsDeliveredForeignSongHost(song), "(g2) an undelivered song host reads as delivered");
    const size_t g2 = SongGuardTick();
    printf("[TEST] %s: (g2) undelivered New Wave host: %zu event(s)\n", kSongGuardRow, g2);
    SHG_ASSERT(g2 == 1 && SongGuardQueuedForeignGiveFor(song),
               "(g2) the first delivery of a song host did not queue exactly one foreign-branch event (param the "
               "check, cutscene on): the guard eats a first delivery");
    SHG_ASSERT(RANDO_SAVE_CHECKS[song].eligible, "(g2) the first delivery's host was disarmed before its give ran");

    // (g3) Tier A chest, delivered and armed again: untouched by the guard.
    SongGuardArmSave();
    SHG_ASSERT(SongGuardPlant(chest, item), "(g3) the chest host could not be planted");
    SongGuardSetBits(chest, true, false, true);
    const size_t g3 = SongGuardTick();
    printf("[TEST] %s: (g3) delivered Tier A chest %u armed again: %zu event(s)\n", kSongGuardRow, (unsigned)chest, g3);
    SHG_ASSERT(g3 == 1 && SongGuardQueuedForeignGiveFor(chest),
               "(g3) the guard swallowed a chest's re-arm: it must touch the song checks only");

    // (g4) a song check holding MM's own item, delivered on an earlier cycle: not
    // a delivered SONG HOST, so 2Ship's own re-give runs as before. Asked of the
    // predicate, not through CheckQueue: the native branch converts the cover
    // through Ship_Random's junk pick, which a ROM-free row must not consume.
    SongGuardArmSave();
    RANDO_SAVE_CHECKS[song].randoItemId = RI_JUNK;
    RANDO_SAVE_CHECKS[song].shuffled = true;
    SongGuardSetBits(song, true, false, true);
    SHG_ASSERT(!Rando::Foreign::IsForeignCheck(song), "(g4) the song check holds a placement");
    SHG_ASSERT(!Rando::Foreign::IsDeliveredForeignSongHost(song),
               "(g4) a song check with no placement reads as a delivered song host: the guard would swallow "
               "2Ship's own re-give");
    RANDO_SAVE_CHECKS[song].eligible = false;
    return 0;
}

int SongGuardResetLegs(SharedItem item) {
    const RandoCheckId healing = RC_STARTING_ITEM_SONG_OF_HEALING;
    const RandoCheckId wave = RC_GREAT_BAY_COAST_NEW_WAVE_BOSSA_NOVA;

    // (r1) Healing: one record, then a reset re-arms nothing.
    SongGuardArmSave();
    gComboCtx.sourceIsRando = true;
    gComboCtx.sharedRandoSettingsHash = 0x15420000u; // a live pairing, so the record is authored (#610)
    SHG_ASSERT(SongGuardPlant(healing, item), "(r1) Healing could not be planted");
    // As OnFileCreate arms it at creation.
    SongGuardSetBits(healing, false, false, true);
    const int before = Combo_CountSharedItems(GAME_OOT, true);
    const bool crossed = Rando::Foreign::GiveForeignCheck(healing);
    const int afterGive = Combo_CountSharedItems(GAME_OOT, true);
    printf("[TEST] %s: (r1) Healing's foreign give: crossed %d, records %d -> %d, obtained %d eligible %d\n",
           kSongGuardRow, crossed ? 1 : 0, before, afterGive, RANDO_SAVE_CHECKS[healing].obtained ? 1 : 0,
           RANDO_SAVE_CHECKS[healing].eligible ? 1 : 0);
    SHG_ASSERT(crossed && afterGive == before + 1, "(r1) Healing's foreign give did not record exactly one crossing");
    SHG_ASSERT(RANDO_SAVE_CHECKS[healing].obtained && !RANDO_SAVE_CHECKS[healing].eligible,
               "(r1) Healing's foreign give did not mark the host delivered");
    SongGuardPlaySongOfTime();
    printf("[TEST] %s: (r1) after the reset: Healing eligible %d cycleObtained %d obtained %d item %d\n", kSongGuardRow,
           RANDO_SAVE_CHECKS[healing].eligible ? 1 : 0, RANDO_SAVE_CHECKS[healing].cycleObtained ? 1 : 0,
           RANDO_SAVE_CHECKS[healing].obtained ? 1 : 0, (int)RANDO_SAVE_CHECKS[healing].randoItemId);
    SHG_ASSERT(RANDO_SAVE_CHECKS[healing].obtained && RANDO_SAVE_CHECKS[healing].randoItemId == RI_JUNK,
               "(r1) the reset did not keep Healing's delivered bit and its cover (the stand-in reset is not the "
               "rando's)");
    SHG_ASSERT(!RANDO_SAVE_CHECKS[healing].cycleObtained,
               "(r1) the reset did not clear cycleObtained, so the rando's AfterEndOfCycleSave did not run");
    SHG_ASSERT(!RANDO_SAVE_CHECKS[healing].eligible, "(r1) the reset re-armed Healing's crossing host");
    SHG_ASSERT(SongGuardTick() == 0, "(r1) a CheckQueue tick after the reset queued an event for Healing");
    SHG_ASSERT(!Rando::Foreign::GiveForeignCheck(healing) && Combo_CountSharedItems(GAME_OOT, true) == afterGive,
               "(r1) a second foreign give of Healing recorded a second crossing");

    // (r2) New Wave: delivered, reset, re-armed by the real flag hook, consumed.
    SongGuardArmSave();
    SHG_ASSERT(SongGuardPlant(wave, item), "(r2) the New Wave check could not be planted");
    SongGuardSetBits(wave, true, true, false);
    // Set as the earlier cycle left it, without the hook (it already fired then).
    WEEKEVENTREG(WEEKEVENTREG_20_40 >> 8) |= (u8)(WEEKEVENTREG_20_40 & 0xFF);
    SongGuardPlaySongOfTime();
    printf("[TEST] %s: (r2) after the reset: WEEKEVENTREG_20_40 %d, eligible %d obtained %d\n", kSongGuardRow,
           CHECK_WEEKEVENTREG(WEEKEVENTREG_20_40) ? 1 : 0, RANDO_SAVE_CHECKS[wave].eligible ? 1 : 0,
           RANDO_SAVE_CHECKS[wave].obtained ? 1 : 0);
    SHG_ASSERT(!CHECK_WEEKEVENTREG(WEEKEVENTREG_20_40) && RANDO_SAVE_CHECKS[wave].obtained,
               "(r2) the reset did not clear the New Wave flag while keeping the delivered bit");
    // What MM's flag write does next time the Marine Lab scene plays: the rando's
    // OnFlagSet hook resolves the flag to the check and arms it (it is shuffled).
    WEEKEVENTREG(WEEKEVENTREG_20_40 >> 8) |= (u8)(WEEKEVENTREG_20_40 & 0xFF);
    Rando::MiscBehavior::OnFlagSet(FLAG_WEEK_EVENT_REG, WEEKEVENTREG_20_40);
    SHG_ASSERT(RANDO_SAVE_CHECKS[wave].eligible, "(r2) the flag hook did not re-arm the New Wave check");
    const int recordsBefore = Combo_CountSharedItems(GAME_OOT, true);
    const size_t r2 = SongGuardTick();
    printf("[TEST] %s: (r2) re-armed New Wave host: %zu event(s), eligible %d\n", kSongGuardRow, r2,
           RANDO_SAVE_CHECKS[wave].eligible ? 1 : 0);
    SHG_ASSERT(r2 == 0 && !RANDO_SAVE_CHECKS[wave].eligible,
               "(r2) a delivered song host re-armed on a later cycle queued a get-item event (the guard is missing)");
    SHG_ASSERT(Combo_CountSharedItems(GAME_OOT, true) == recordsBefore, "(r2) the consumed re-arm recorded a "
                                                                        "crossing");
    return 0;
}

} // namespace

/**
 * @param ootOrigin GAME_OOT, as the caller's SharedItem carries it.
 * @param ootItemId an OoT item that CROSSES (not a counted pack or a capacity
 *        tier, which the record credits to the pool instead: #1061, #1120).
 * @return 0 when every leg holds.
 */
extern "C" int MM_SongHostGuard_Run(uint8_t ootOrigin, uint16_t ootItemId) {
    printf("[TEST] %s: a delivered MM song host armed again gives nothing and shows nothing; its first delivery, "
           "a chest's re-arm and a native song check's re-give are untouched; Healing's crossing host is delivered "
           "once across a three-day reset (#1542 S0)\n",
           kSongGuardRow);
    SharedItem item;
    memset(&item, 0, sizeof(item));
    item.originGame = ootOrigin;
    item.id = ootItemId;
    int rc = 0;
    {
        SongGuardRestore restore;
        rc = SongGuardQueueLegs(item);
        if (rc == 0) {
            rc = SongGuardResetLegs(item);
        }
    }
    if (rc != 0) {
        return rc;
    }
    printf("[TEST] PASS: %s\n", kSongGuardRow);
    return 0;
}

#endif
