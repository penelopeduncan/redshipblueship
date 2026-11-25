/**
 * @file mm_int_shuffle_ocarina_drive.cpp
 * @brief Majora's Mask's half of the integration row IntMMShuffleOcarina
 *        (int-mm-shuffle-ocarina, #1671 lane B2): the Shuffle Ocarina option on
 *        a live paired file. OoT's half (the world: the option on and Moon Crash
 *        Behavior on Last Save, both through their keys, in memory) is
 *        SoArmWorld in games/oot/soh/GameExports_SingleExe.cpp; the paired first
 *        crossing (generation, creation, load, the Happy Mask Shop door and the
 *        arrival's own checks) is the int-paired-first-crossing drive this row
 *        rides.
 *
 * What it proves, and the lane that made each fact true:
 *   - B1: the paired start drops RI_OCARINA, so MM arrives with the Song of
 *     Time and an empty SLOT_OCARINA (the start-list leg is in
 *     MM_PairedFirstCrossingCheckStart, GameExports_SingleExe.cpp).
 *   - B0: the first cycle's 5x clock does not run for the ocarina-less half
 *     (MM_Scene_FirstCycleTimeSpeedApplies dispatches VB_FASTER_FIRST_CYCLE),
 *     Termina Field is not emptied, and Day 3 midnight outside South Clock Town
 *     does not pull the player into the festival
 *     (VB_CLOCK_TOWER_OPENING_CONSIDER_THIS_FIRST_CYCLE).
 *   - B1: the frozen option forces New Cycle (MM_Rando_FrozenMoonCrash) over a
 *     frozen Last Save byte, so the moon falling at 6:00 on Day 3 starts a new
 *     cycle that keeps the file's progress.
 *
 * Nothing here calls the gates or the reset. The drive moves the clock and the
 * scene the way a debug warp does, and the three-day clock actor (EnTest4), the
 * real Interface_StartMoonCrash, Play_Init's New Cycle cutscene skip (#1789,
 * SkipMoonCrash.cpp from z_demo.c's func_800EDDB0) and
 * Sram_ResetSaveFromMoonCrash do the rest; the drive reads the state they leave.
 *
 * The steps, run from MM_IntegrationGameplayFrameTick's GP_PHASE_MM_PLAY branch
 * once the paired drive's own frames are done (live frames only). Each has a
 * budget, and each failure prints one line with the values it read and the ones
 * it expected, plus the whole state (SodFail):
 *   (arrival) at the first South Clock Town arrival, ten stable frames in:
 *             SLOT_OCARINA empty, the Song of Time held, R_TIME_SPEED and the
 *             scene's time speed not 5, the Termina Field gate false, the world
 *             frozen with the option, MM_Rando_FrozenMoonCrash New Cycle over a
 *             Last Save byte, and no free starting check holding the ocarina.
 *   PAYOUTS   20 quiet frames in a row (no get-item event, no textbox, no
 *             transition), pressing A past a finished textbox after 15 frames
 *             and closing one nobody pressed past after 90. 900 frames.
 *   PREPARE   once: the ocarina still not held, the Story skip forced off in
 *             memory, the three-day clock actor present; the file goes to Day 3
 *             23:54 at night and South Clock Town's clock actor is killed in the
 *             same frame (it would read the jump as a half-day change and play
 *             the night title card), then North Clock Town is entered.
 *   ARRIVE    North Clock Town live before midnight, Day 3, its clock actor
 *             present, R_TIME_SPEED not 5. 600 frames.
 *   MIDNIGHT  every frame: no transition, no festival cutscene index (0xFFF1),
 *             still North Clock Town; done 20 frames after the clock passes
 *             0:02 with the clock actor still alive. 1200 frames.
 *   CRASH     the fixture items (the Bunny Hood, a bottle, the Song of Healing)
 *             and a Deku Playground record are written, the reset count and
 *             the two New Cycle line counts recorded, and the clock set to
 *             5:58; the clock actor must drop the moon at 6:00 (day 4, during
 *             the white fade). 600 frames.
 *   CRASHING  (#1789) no frame in Termina Field (the moonfall) or SCENE_SPOT00
 *             (the Fire Wall): either is a FAIL; the first live scene after the
 *             fade is the landing; 30 live frames in it. 900 frames.
 *   VERDICT   one more three-day reset, the record cleared, day 0 or 1, the New
 *             Cycle reset line and the #1789 skip line each logged once more,
 *             the landing South Clock Town at 0xD800 (a paired file), the
 *             fixture items and the Song of Time kept, the frozen byte still
 *             Last Save, still New Cycle, the ocarina still not held and the
 *             clock not on 5. A pass erases file 3 and ends the run in MM.
 */

#include "global.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <variant>

#if defined(RSBS_SINGLE_EXECUTABLE)

#include <libultraship/bridge/consolevariablebridge.h>

#include "combo_mm_options_view.h"
#include "context.h"
#include "entrance.h"
#include "integration_test_hooks.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/Foreign.h"
#include "2s2h/Rando/Rando.h"

extern "C" {
// games/mm/src/code/z_play.c: the Termina Field gate MM_Play_Init asks
// (mm_combo_first_cycle_test.cpp declares it the same way).
bool MM_Play_ShouldEmptyFirstCycleTerminaField(s32 scene, u16 entrance);
// games/oot/soh/GameExports_SingleExe.cpp: a pass erases the paired file 3, as
// the Soaring row's MM-side pass does.
void OoT_SoaringTest_ErasePairedFile(void);
}

namespace {

enum class SodStep { Payouts, Prepare, Arrive, Midnight, Crash, Crashing, Done, Failed };

const int kSodPayoutBudget = 900;
const int kSodQuietNeeded = 20;
const int kSodTextPressFrames = 15;
const int kSodTextCloseFrames = 90;
const int kSodArriveBudget = 600;
const int kSodMidnightBudget = 1200;
const int kSodMidnightHold = 20;
const int kSodCrashBudget = 600;
// Under the MM phase watchdog (framesPerPhase * 4 + 3600 = 4800 frames at
// RSBS_GP_FRAMES=300, counted from the start of the gameplay window), so this
// step's own failure line fires before the generic one. Since #1789 no crash
// cutscene plays (int-mm-moon-crash-cycle measured the crash and the Fire Wall
// at about 1200 frames): the 60-frame white fade, then 30 live frames in the
// landing (the Day Telop is not a drive frame).
const int kSodCrashingBudget = 900;
const int kSodFramesAfterCrash = 30;
const u16 kSodFestivalCutscene = 0xFFF1; // z_en_test4.c's Day 3 midnight cutscene index
const u32 kSodRecordSentinel = 16712u;   // #1671 B2
// The midnight leg's window: 23:54 leaves the scene load time to settle before
// the 0:00 bell, and the bell is read as passed once the clock shows 0:02.
const u16 kSodBeforeMidnight = CLOCK_TIME(23, 54);
const u16 kSodPastMidnight = CLOCK_TIME(0, 2);
const u16 kSodBeforeDawn = CLOCK_TIME(5, 58);
// Sram_ResetSaveFromMoonCrash's line for the New Cycle branch (z_sram_NES.c).
// Counted, never printed here (a print would be counted too).
const char* const kSodNewCycleNeedle = "[MM] moon crash: New Cycle (#1069)";
// #1789: MM_Play_SkipMoonCrashUnderNewCycle's line (SkipMoonCrash.cpp), kept by
// the tee's "moon crash: New Cycle" tag. Counted, never printed here.
const char* const kSodSkipNeedle = "[MM] moon crash: New Cycle skips the Termina Field and Fire Wall cutscenes (#1789)";
// Where a paired file lands after the moon crash (#1278): South Clock Town spawn 0.
const u16 kSodLandingEntrance = 0xD800;

struct SodState {
    SodStep step = SodStep::Payouts;
    int stepFrames = 0;
    int driveFrames = 0;
    u16 held = 0;
    int heldFrames = 0;
    int quietFrames = 0;
    int textFrames = 0;
    u16 textId = 0xFFFF;
    int pastMidnightFrames = 0;
    int afterCrashFrames = 0;
    u8 arrivalByte = 0xFF;
    u16 resetsBefore = 0;
    int newCycleLinesBefore = -1;
    int skipLinesBefore = -1; // #1789
    int landingScene = -1;    // #1789: the first live scene after the crash
    u16 landingEntrance = 0;  // #1789: the save's entrance there
};
SodState sSod;

const char* SodStepName(SodStep step) {
    switch (step) {
        case SodStep::Payouts:
            return "payouts";
        case SodStep::Prepare:
            return "prepare";
        case SodStep::Arrive:
            return "North Clock Town";
        case SodStep::Midnight:
            return "Day 3 midnight";
        case SodStep::Crash:
            return "the moon falls";
        case SodStep::Crashing:
            return "the crash";
        case SodStep::Done:
            return "done";
        case SodStep::Failed:
            return "failed";
    }
    return "?";
}

bool SodEventNone() {
    return std::holds_alternative<GIEventNone>(MM_GameEvents_Current());
}

/** The three-day clock actor (EnTest4, ACTORCAT_SWITCH), or NULL. */
Actor* SodFindClockActor(PlayState* play) {
    if (play == nullptr) {
        return nullptr;
    }
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_SWITCH].first; a != nullptr; a = a->next) {
        if (a->id == ACTOR_EN_TEST4 && a->update != nullptr) {
            return a;
        }
    }
    return nullptr;
}

bool SodOcarinaHeld() {
    return gSaveContext.save.saveInfo.inventory.items[SLOT_OCARINA] != ITEM_NONE;
}

/** Every value the drive's steps are judged by, on one line. */
std::string SodDump(PlayState* play) {
    char buf[900];
    const SaveInfo& info = gSaveContext.save.saveInfo;
    snprintf(buf, sizeof(buf),
             "scene %d entrance 0x%04X transition %d/%d nextEntrance 0x%04X nextCutsceneIndex 0x%04X cutsceneIndex "
             "0x%04X; day %d eventDayCount %d time 0x%04X isNight %d; R_TIME_SPEED %d sceneTimeSpeed %d; clock actor "
             "%s; SLOT_OCARINA 0x%02X songOfTime %d; frozen moon crash %d byte %u (at arrival %u); option %d; "
             "threeDayResetCount %u (before the crash %u); Deku record %u; New Cycle lines %d (before %d); queued "
             "%d current event %d msgMode %d; drive: step '%s' frame %d (drive frame %d)",
             play != nullptr ? (int)play->sceneId : -1, (unsigned)(u16)gSaveContext.save.entrance,
             play != nullptr ? (int)play->transitionTrigger : -1, play != nullptr ? (int)play->transitionMode : -1,
             play != nullptr ? (unsigned)(u16)play->nextEntrance : 0u, (unsigned)gSaveContext.nextCutsceneIndex,
             (unsigned)(u16)gSaveContext.save.cutsceneIndex, (int)gSaveContext.save.day,
             (int)gSaveContext.save.eventDayCount, (unsigned)gSaveContext.save.time, (int)gSaveContext.save.isNight,
             (int)R_TIME_SPEED, play != nullptr ? (int)play->envCtx.sceneTimeSpeed : -1,
             SodFindClockActor(play) != nullptr ? "present" : "absent", (unsigned)info.inventory.items[SLOT_OCARINA],
             CHECK_QUEST_ITEM(QUEST_SONG_TIME) ? 1 : 0, MM_Rando_FrozenMoonCrash(),
             (unsigned)gSaveContext.save.shipSaveInfo.rando.randoSaveMoonCrash, (unsigned)sSod.arrivalByte,
             Rando::Foreign::ShuffleOcarinaForWorld(RANDO_SAVE_OPTIONS) ? 1 : 0,
             (unsigned)info.playerData.threeDayResetCount, (unsigned)sSod.resetsBefore,
             (unsigned)info.dekuPlaygroundHighScores[0], IntegrationTest_StderrCaptureCount(kSodNewCycleNeedle),
             sSod.newCycleLinesBefore, (int)MM_GameEvents_Queue().size(), (int)MM_GameEvents_Current().index(),
             play != nullptr ? (int)play->msgCtx.msgMode : -1, SodStepName(sSod.step), sSod.stepFrames,
             sSod.driveFrames);
    return buf;
}

void SodFail(PlayState* play, const char* why) {
    if (sSod.step == SodStep::Failed) {
        return;
    }
    const std::string state = SodDump(play);
    const SodStep at = sSod.step;
    sSod.step = SodStep::Failed;
    sSod.held = 0;
    sSod.heldFrames = 0;
    MM_PadMgr_SetTestButtons(0);
    std::string msg = "int-mm-shuffle-ocarina, step '";
    msg += SodStepName(at);
    msg += "': ";
    msg += why;
    msg += " [";
    msg += state;
    msg += "]";
    IntegrationTest_GameplayFail(msg.c_str());
}

void SodNext(SodStep step) {
    fprintf(stderr, "[PFC] mm shuffle ocarina: step '%s' done after %d frame(s) (drive frame %d)\n",
            SodStepName(sSod.step), sSod.stepFrames, sSod.driveFrames);
    fflush(stderr);
    sSod.step = step;
    sSod.stepFrames = 0;
    sSod.textFrames = 0;
    sSod.textId = 0xFFFF;
}

/** Once per drive frame: the latch is consumed by the next pad update, so a
 *  held button is set again on every frame of its hold (as the Minuet drive). */
void SodPump() {
    if (sSod.heldFrames > 0) {
        MM_PadMgr_SetTestButtons(sSod.held);
        sSod.heldFrames--;
    } else {
        sSod.held = 0;
    }
}

/** A finished payout textbox: A after 15 frames, closed after 90 (fixture
 *  work, as the Minuet drive's MwdrStrayTextbox). */
void SodStrayTextbox(PlayState* play) {
    MessageContext* m = &play->msgCtx;
    const u8 state = MM_Message_GetState(m);
    if (m->msgMode != MSGMODE_TEXT_DONE || (state != TEXT_STATE_DONE && state != TEXT_STATE_EVENT)) {
        sSod.textFrames = 0;
        sSod.textId = 0xFFFF;
        return;
    }
    if (m->currentTextId != sSod.textId) {
        sSod.textId = m->currentTextId;
        sSod.textFrames = 0;
    }
    sSod.textFrames++;
    if (sSod.textFrames >= kSodTextCloseFrames) {
        fprintf(stderr, "[PFC] mm shuffle ocarina: closed a payout textbox nobody pressed past (text 0x%04X)\n",
                (unsigned)m->currentTextId);
        fflush(stderr);
        MM_Message_CloseTextbox(play);
        sSod.textFrames = 0;
    } else if (sSod.textFrames % kSodTextPressFrames == 0 && sSod.heldFrames == 0) {
        sSod.held = BTN_A;
        sSod.heldFrames = 1;
    }
}

bool SodQuiet(PlayState* play, Player* player) {
    return MM_GameEvents_Queue().empty() && SodEventNone() && play->msgCtx.msgMode == MSGMODE_NONE &&
           !(player->stateFlags1 & PLAYER_STATE1_400) && !MM_Player_InBlockingCsMode(play, player) &&
           play->transitionTrigger == TRANS_TRIGGER_OFF;
}

void SodStepPayouts(PlayState* play, Player* player) {
    SodStrayTextbox(play);
    sSod.quietFrames = SodQuiet(play, player) ? sSod.quietFrames + 1 : 0;
    if (sSod.quietFrames >= kSodQuietNeeded) {
        SodNext(SodStep::Prepare);
        return;
    }
    if (sSod.stepFrames >= kSodPayoutBudget) {
        SodFail(play, "the arrival's starting-check payouts never drained (expected 20 quiet frames in a row)");
    }
}

void SodStepPrepare(PlayState* play) {
    static const char* const kForeignDrives[] = { "RSBS_PFC_SONG_HOST", "RSBS_GP_MM_FOREIGN_MODEL", "RSBS_GP_MM_SHOP" };
    for (const char* env : kForeignDrives) {
        if (std::getenv(env) != nullptr) {
            std::string why = "another paired drive is armed in this run (";
            why += env;
            why += "): it gives and moves the player on its own frames";
            SodFail(play, why.c_str());
            return;
        }
    }
    if (SodOcarinaHeld()) {
        char why[256];
        snprintf(why, sizeof(why),
                 "after the payouts SLOT_OCARINA holds 0x%02X, expected ITEM_NONE (0x%02X): something gave the "
                 "ocarina between the arrival and the payouts' end (a payout or a crossing of the first OoT leg)",
                 (unsigned)gSaveContext.save.saveInfo.inventory.items[SLOT_OCARINA], (unsigned)ITEM_NONE);
        SodFail(play, why);
        return;
    }
    if (SodFindClockActor(play) == nullptr) {
        SodFail(play, "South Clock Town has no live three-day clock actor (EnTest4), so the clock jump below "
                      "would not be the one a player meets");
        return;
    }
    // A staged config must not change the path under test (in memory only, put
    // back at the verdict by IntegrationTest_SoaringCleanup): the Story skip
    // answers the festival's VB on its own and resets a crash from Play_Init.
    IntegrationTest_SoaringSetCVarInt("gEnhancements.TimeSavers.SkipCutscene.Story", 0);
    // The jump and the scene change in one frame: South Clock Town's clock actor
    // is killed first, or it would read 23:54 as a half-day change and play the
    // night title card before the fade.
    MM_Actor_Kill(SodFindClockActor(play));
    gSaveContext.save.day = 3;
    gSaveContext.save.eventDayCount = 3;
    gSaveContext.save.time = kSodBeforeMidnight;
    gSaveContext.save.isNight = true;
    fprintf(stderr,
            "[PFC] mm shuffle ocarina: the file goes to Day 3 0x%04X (23:54) at night and enters North Clock Town "
            "(entrance 0x%04X) from scene %d\n",
            (unsigned)gSaveContext.save.time, (unsigned)(u16)ENTRANCE(NORTH_CLOCK_TOWN, 0), (int)play->sceneId);
    fflush(stderr);
    play->nextEntrance = ENTRANCE(NORTH_CLOCK_TOWN, 0);
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
    SodNext(SodStep::Arrive);
}

void SodStepArrive(PlayState* play) {
    if (play->sceneId != SCENE_BACKTOWN || play->transitionTrigger != TRANS_TRIGGER_OFF ||
        play->transitionMode != TRANS_MODE_OFF) {
        if (sSod.stepFrames >= kSodArriveBudget) {
            char why[160];
            snprintf(why, sizeof(why), "North Clock Town (scene %d) never became live; expected it within %d frames",
                     (int)SCENE_BACKTOWN, kSodArriveBudget);
            SodFail(play, why);
        }
        return;
    }
    char why[320];
    const u16 now = (u16)gSaveContext.save.time;
    if (gSaveContext.save.day != 3 || now < CLOCK_TIME(18, 0)) {
        snprintf(why, sizeof(why),
                 "North Clock Town became live on day %d at 0x%04X; expected Day 3 before midnight (at or after "
                 "0x%04X): the midnight leg would be vacuous",
                 (int)gSaveContext.save.day, (unsigned)now, (unsigned)CLOCK_TIME(18, 0));
        SodFail(play, why);
        return;
    }
    if (SodFindClockActor(play) == nullptr) {
        SodFail(play, "North Clock Town has no live three-day clock actor (EnTest4): nothing would ring midnight, "
                      "so the no-festival leg would be vacuous");
        return;
    }
    if (R_TIME_SPEED == 5 || play->envCtx.sceneTimeSpeed == 5) {
        snprintf(why, sizeof(why),
                 "North Clock Town runs the first cycle's fast clock: R_TIME_SPEED %d, sceneTimeSpeed %d, expected "
                 "neither to be 5 (VB_FASTER_FIRST_CYCLE for a combo half)",
                 (int)R_TIME_SPEED, (int)play->envCtx.sceneTimeSpeed);
        SodFail(play, why);
        return;
    }
    fprintf(stderr,
            "[PFC] mm shuffle ocarina: North Clock Town live on Day 3 at 0x%04X, R_TIME_SPEED %d, clock actor "
            "present; waiting for midnight\n",
            (unsigned)now, (int)R_TIME_SPEED);
    fflush(stderr);
    SodNext(SodStep::Midnight);
}

void SodStepMidnight(PlayState* play) {
    char why[384];
    if (gSaveContext.nextCutsceneIndex == kSodFestivalCutscene || play->transitionTrigger != TRANS_TRIGGER_OFF ||
        play->sceneId != SCENE_BACKTOWN) {
        snprintf(why, sizeof(why),
                 "the festival warp fired at Day 3 midnight outside South Clock Town: nextCutsceneIndex 0x%04X "
                 "(festival 0x%04X), transitionTrigger %d (expected %d), nextEntrance 0x%04X, scene %d (expected "
                 "North Clock Town, %d)",
                 (unsigned)gSaveContext.nextCutsceneIndex, (unsigned)kSodFestivalCutscene, (int)play->transitionTrigger,
                 (int)TRANS_TRIGGER_OFF, (unsigned)(u16)play->nextEntrance, (int)play->sceneId, (int)SCENE_BACKTOWN);
        SodFail(play, why);
        return;
    }
    const u16 now = (u16)gSaveContext.save.time;
    const bool past = now >= kSodPastMidnight && now < CLOCK_TIME(6, 0);
    sSod.pastMidnightFrames = past ? sSod.pastMidnightFrames + 1 : 0;
    if (sSod.pastMidnightFrames >= kSodMidnightHold) {
        if (SodFindClockActor(play) == nullptr) {
            SodFail(play, "the clock actor is gone after midnight (the festival branch kills it): expected it alive");
            return;
        }
        fprintf(stderr,
                "[PFC] mm shuffle ocarina leg 'Day 3 midnight' PASS: the clock passed midnight in North Clock Town "
                "(now 0x%04X, day %d) with no festival warp, the clock actor alive\n",
                (unsigned)now, (int)gSaveContext.save.day);
        fflush(stderr);
        SodNext(SodStep::Crash);
        return;
    }
    if (sSod.stepFrames >= kSodMidnightBudget) {
        snprintf(why, sizeof(why),
                 "the clock never passed 0x%04X (0:02) within %d frames: it reads 0x%04X at R_TIME_SPEED %d",
                 (unsigned)kSodPastMidnight, kSodMidnightBudget, (unsigned)now, (int)R_TIME_SPEED);
        SodFail(play, why);
    }
}

void SodStepCrash(PlayState* play) {
    if (sSod.stepFrames == 1) {
        // What a new cycle keeps and what only a new cycle clears (as
        // int-mm-moon-crash-cycle's fixture), then the clock two minutes before
        // dawn: the clock actor drops the moon itself at 6:00.
        gSaveContext.save.saveInfo.inventory.items[SLOT_MASK_BUNNY] = ITEM_MASK_BUNNY;
        gSaveContext.save.saveInfo.inventory.items[SLOT_BOTTLE_1] = ITEM_BOTTLE;
        gSaveContext.save.saveInfo.inventory.questItems |= (1u << QUEST_SONG_HEALING);
        gSaveContext.save.saveInfo.dekuPlaygroundHighScores[0] = kSodRecordSentinel;
        sSod.resetsBefore = gSaveContext.save.saveInfo.playerData.threeDayResetCount;
        sSod.newCycleLinesBefore = IntegrationTest_StderrCaptureCount(kSodNewCycleNeedle);
        sSod.skipLinesBefore = IntegrationTest_StderrCaptureCount(kSodSkipNeedle);
        gSaveContext.save.time = kSodBeforeDawn;
        fprintf(stderr,
                "[PFC] mm shuffle ocarina: the clock is set to 0x%04X (5:58) on Day 3; the file plays under %d "
                "(expected %d, New Cycle) over byte %u; threeDayResetCount %u\n",
                (unsigned)gSaveContext.save.time, MM_Rando_FrozenMoonCrash(), RSBS_MM_MOON_CRASH_CYCLE,
                (unsigned)gSaveContext.save.shipSaveInfo.rando.randoSaveMoonCrash, (unsigned)sSod.resetsBefore);
        fflush(stderr);
        return;
    }
    // #1789: the moon's fall is read during the white fade (day 4), before
    // Play_Init skips Termina Field and the Fire Wall.
    if (gSaveContext.save.day == 4) {
        SodNext(SodStep::Crashing);
        return;
    }
    if (sSod.stepFrames >= kSodCrashBudget) {
        char why[256];
        snprintf(why, sizeof(why),
                 "the moon never fell: %d frames after the clock was set to 0x%04X the file is on day %d at 0x%04X "
                 "(expected day 4, Interface_StartMoonCrash's, once the clock actor passes 0x%04X)",
                 kSodCrashBudget, (unsigned)kSodBeforeDawn, (int)gSaveContext.save.day,
                 (unsigned)gSaveContext.save.time, (unsigned)CLOCK_TIME(6, 0));
        SodFail(play, why);
    }
}

void SodVerdict(PlayState* play) {
    char why[512];
    const SaveInfo& info = gSaveContext.save.saveInfo;
    const int newCycleLines = IntegrationTest_StderrCaptureCount(kSodNewCycleNeedle);
    const int skipLines = IntegrationTest_StderrCaptureCount(kSodSkipNeedle);
    const u8 byte = gSaveContext.save.shipSaveInfo.rando.randoSaveMoonCrash;
    why[0] = '\0';
    if (info.playerData.threeDayResetCount != (u16)(sSod.resetsBefore + 1) ||
        info.dekuPlaygroundHighScores[0] == kSodRecordSentinel) {
        snprintf(why, sizeof(why),
                 "the crash did not start a new cycle: threeDayResetCount %u, expected %u; Deku record %u, expected "
                 "it cleared from %u (the Last Save path ran)",
                 (unsigned)info.playerData.threeDayResetCount, (unsigned)(sSod.resetsBefore + 1),
                 (unsigned)info.dekuPlaygroundHighScores[0], (unsigned)kSodRecordSentinel);
    } else if (newCycleLines != sSod.newCycleLinesBefore + 1) {
        // The needle is not printed here: a printed copy would be counted too.
        snprintf(why, sizeof(why),
                 "the reset logged z_sram_NES.c's New Cycle line %d time(s) since the clock was set, expected exactly "
                 "once",
                 newCycleLines - sSod.newCycleLinesBefore);
    } else if (skipLines != sSod.skipLinesBefore + 1) {
        snprintf(why, sizeof(why),
                 "the crash logged SkipMoonCrash.cpp's New Cycle cutscene-skip line %d time(s) since the clock was "
                 "set, expected exactly once (#1789)",
                 skipLines - sSod.skipLinesBefore);
    } else if (sSod.landingScene != (int)SCENE_CLOCKTOWER || sSod.landingEntrance != kSodLandingEntrance) {
        snprintf(why, sizeof(why),
                 "after the crash the first live scene is %d at entrance 0x%04X, expected South Clock Town (%d) at "
                 "0x%04X, a paired file's landing (#1278, #1789)",
                 sSod.landingScene, (unsigned)sSod.landingEntrance, (int)SCENE_CLOCKTOWER,
                 (unsigned)kSodLandingEntrance);
    } else if (gSaveContext.save.day > 1 || gSaveContext.save.eventDayCount > 1) {
        snprintf(why, sizeof(why),
                 "after the crash the file is on day %d (eventDayCount %d), expected the Dawn of the First Day (day 0 "
                 "or 1)",
                 (int)gSaveContext.save.day, (int)gSaveContext.save.eventDayCount);
    } else if (info.inventory.items[SLOT_MASK_BUNNY] != ITEM_MASK_BUNNY ||
               info.inventory.items[SLOT_BOTTLE_1] != ITEM_BOTTLE ||
               (info.inventory.questItems & (1u << QUEST_SONG_HEALING)) == 0u || !CHECK_QUEST_ITEM(QUEST_SONG_TIME)) {
        snprintf(why, sizeof(why),
                 "the new cycle lost the file's progress: Bunny Hood slot 0x%02X (expected 0x%02X), bottle 0x%02X "
                 "(expected 0x%02X), quest items %08X (expected the Song of Healing and the Song of Time)",
                 (unsigned)info.inventory.items[SLOT_MASK_BUNNY], (unsigned)ITEM_MASK_BUNNY,
                 (unsigned)info.inventory.items[SLOT_BOTTLE_1], (unsigned)ITEM_BOTTLE,
                 (unsigned)info.inventory.questItems);
    } else if (byte != sSod.arrivalByte || byte != RSBS_MM_MOON_CRASH_RESET ||
               MM_Rando_FrozenMoonCrash() != RSBS_MM_MOON_CRASH_CYCLE) {
        snprintf(why, sizeof(why),
                 "after the crash the frozen byte is %u (at arrival %u, expected Last Save %d) and the file plays "
                 "under %d (expected New Cycle %d)",
                 (unsigned)byte, (unsigned)sSod.arrivalByte, RSBS_MM_MOON_CRASH_RESET, MM_Rando_FrozenMoonCrash(),
                 RSBS_MM_MOON_CRASH_CYCLE);
    } else if (SodOcarinaHeld()) {
        snprintf(why, sizeof(why), "after the crash SLOT_OCARINA holds 0x%02X, expected ITEM_NONE (0x%02X)",
                 (unsigned)info.inventory.items[SLOT_OCARINA], (unsigned)ITEM_NONE);
    } else if (R_TIME_SPEED == 5 || play->envCtx.sceneTimeSpeed == 5) {
        snprintf(why, sizeof(why),
                 "the new cycle runs the first cycle's fast clock: R_TIME_SPEED %d, sceneTimeSpeed %d, expected "
                 "neither to be 5",
                 (int)R_TIME_SPEED, (int)play->envCtx.sceneTimeSpeed);
    }
    if (why[0] != '\0') {
        SodFail(play, why);
        return;
    }
    fprintf(stderr,
            "[PFC] mm shuffle ocarina leg 'the moon crash' PASS: a new cycle in scene %d on day %d at 0x%04X "
            "(threeDayResetCount %u, was %u), progress kept, byte still Last Save (%u), no ocarina, R_TIME_SPEED %d\n",
            (int)play->sceneId, (int)gSaveContext.save.day, (unsigned)gSaveContext.save.time,
            (unsigned)info.playerData.threeDayResetCount, (unsigned)sSod.resetsBefore, (unsigned)byte,
            (int)R_TIME_SPEED);
    fprintf(stderr, "[PFC] PASS: int-mm-shuffle-ocarina (arrival, Day 3 midnight, the moon crash)\n");
    fflush(stderr);
    sSod.step = SodStep::Done;
    IntegrationTest_SetGameplayPhase(GP_PHASE_DONE);
    // A pass leaves no file behind (a failure keeps it for forensics); before
    // the shutdown, while OoT's SaveManager is still up.
    OoT_SoaringTest_ErasePairedFile();
    IntegrationTest_SignalBootComplete(GAME_MM, "Shuffle Ocarina: no festival warp, the moon crash began a new cycle");
}

void SodStepCrashing(PlayState* play) {
    // #1789: under New Cycle the moon's fall never loads Termina Field (the
    // moonfall) nor SCENE_SPOT00 (the Fire Wall): a frame in either is the
    // cutscene playing. Day 4 is the white fade in the scene the moon fell in.
    if (play->sceneId == SCENE_00KEIKOKU || play->sceneId == SCENE_SPOT00) {
        char why[160];
        snprintf(why, sizeof(why), "the moon-crash cutscene played under New Cycle: a frame in scene %d (#1789)",
                 (int)play->sceneId);
        SodFail(play, why);
        return;
    }
    if (gSaveContext.save.day == 4) {
        sSod.afterCrashFrames = 0;
    } else if (play->transitionTrigger == TRANS_TRIGGER_OFF && play->transitionMode == TRANS_MODE_OFF &&
               gSaveContext.gameMode == GAMEMODE_NORMAL) {
        if (sSod.landingScene < 0) {
            sSod.landingScene = (int)play->sceneId;
            sSod.landingEntrance = (u16)gSaveContext.save.entrance;
            fprintf(stderr, "[PFC] mm shuffle ocarina: first live scene after the crash: %d (entrance 0x%04X)\n",
                    sSod.landingScene, (unsigned)sSod.landingEntrance);
            fflush(stderr);
        }
        if (++sSod.afterCrashFrames >= kSodFramesAfterCrash) {
            SodVerdict(play);
            return;
        }
    }
    if (sSod.stepFrames >= kSodCrashingBudget) {
        char why[160];
        snprintf(why, sizeof(why), "the moon crash did not hand over to a new cycle within %d frames",
                 kSodCrashingBudget);
        SodFail(play, why);
    }
}

} // namespace

extern "C" bool MM_IntSoDrive_CheckArrival(PlayState* play) {
    char why[512];
    why[0] = '\0';
    sSod.arrivalByte = gSaveContext.save.shipSaveInfo.rando.randoSaveMoonCrash;
    RandoCheckId ocarinaOnFreeCheck = RC_UNKNOWN;
    for (RandoCheckId rc : { RC_STARTING_ITEM_DEKU_MASK, RC_STARTING_ITEM_SONG_OF_HEALING }) {
        if (RANDO_SAVE_CHECKS[rc].randoItemId == RI_OCARINA) {
            ocarinaOnFreeCheck = rc;
        }
    }
    int mmHosts = 0;
    for (int i = 0; i < (int)RC_MAX; i++) {
        mmHosts += RANDO_SAVE_CHECKS[i].randoItemId == RI_OCARINA ? 1 : 0;
    }
    const bool fieldEmpty =
        MM_Play_ShouldEmptyFirstCycleTerminaField(ENTR_SCENE_TERMINA_FIELD, (u16)ENTRANCE(TERMINA_FIELD, 0));
    if (play == nullptr) {
        snprintf(why, sizeof(why), "no MM play state at the arrival");
    } else if (!Rando::Foreign::ShuffleOcarinaForWorld(RANDO_SAVE_OPTIONS)) {
        snprintf(why, sizeof(why), "the arriving half's frozen options have RO_SHUFFLE_OCARINA %u, expected on",
                 (unsigned)RANDO_SAVE_OPTIONS[RO_SHUFFLE_OCARINA]);
    } else if (ocarinaOnFreeCheck != RC_UNKNOWN) {
        snprintf(why, sizeof(why),
                 "fixture: the pinned world puts RI_OCARINA on the free starting check %d, which the arrival pays "
                 "out, so the no-ocarina legs cannot be read on this seed",
                 (int)ocarinaOnFreeCheck);
    } else if (SodOcarinaHeld()) {
        snprintf(why, sizeof(why),
                 "SLOT_OCARINA holds 0x%02X at the first arrival, expected ITEM_NONE (0x%02X) (the first OoT leg's "
                 "chest skips the Ocarina; if another OoT check it collected held it, this is the pinned seed, not "
                 "the option)",
                 (unsigned)gSaveContext.save.saveInfo.inventory.items[SLOT_OCARINA], (unsigned)ITEM_NONE);
    } else if (!CHECK_QUEST_ITEM(QUEST_SONG_TIME)) {
        snprintf(why, sizeof(why), "the Song of Time is not held at the first arrival (quest items %08X), expected it",
                 (unsigned)gSaveContext.save.saveInfo.inventory.questItems);
    } else if (R_TIME_SPEED == 5 || play->envCtx.sceneTimeSpeed == 5) {
        snprintf(why, sizeof(why),
                 "South Clock Town runs the first cycle's fast clock: R_TIME_SPEED %d, sceneTimeSpeed %d, expected "
                 "neither to be 5 (B0's VB_FASTER_FIRST_CYCLE dispatch for a combo half without its ocarina)",
                 (int)R_TIME_SPEED, (int)play->envCtx.sceneTimeSpeed);
    } else if (fieldEmpty) {
        snprintf(why, sizeof(why),
                 "the Termina Field gate answers 1 (empty the field) for the ocarina-less half, expected 0 "
                 "(VB_TERMINA_FIELD_BE_EMPTY for a combo half)");
    } else if (MM_Rando_FrozenMoonCrash() != RSBS_MM_MOON_CRASH_CYCLE || sSod.arrivalByte != RSBS_MM_MOON_CRASH_RESET) {
        snprintf(why, sizeof(why),
                 "the frozen moon crash is %d over byte %u; expected New Cycle (%d) forced over the Last Save byte "
                 "(%d) the row's key froze",
                 MM_Rando_FrozenMoonCrash(), (unsigned)sSod.arrivalByte, RSBS_MM_MOON_CRASH_CYCLE,
                 RSBS_MM_MOON_CRASH_RESET);
    }
    if (why[0] != '\0') {
        SodFail(play, why);
        return false;
    }
    fprintf(stderr,
            "[PFC] mm shuffle ocarina leg 'arrival' PASS: no ocarina, the Song of Time held, R_TIME_SPEED %d "
            "(sceneTimeSpeed %d), Termina Field gate 0, New Cycle forced over byte %u; RI_OCARINA on %d MM check(s) "
            "(0 means it is on an Ocarina of Time check)\n",
            (int)R_TIME_SPEED, (int)play->envCtx.sceneTimeSpeed, (unsigned)sSod.arrivalByte, mmHosts);
    fflush(stderr);
    return true;
}

extern "C" void MM_IntSoDrive_Frame(PlayState* play) {
    if (play == nullptr || sSod.step == SodStep::Failed || sSod.step == SodStep::Done) {
        return;
    }
    // A new cycle passes through the Day Telop game state: only a frame of the
    // play state itself is a drive frame (int-mm-moon-crash-cycle's gate).
    if (MM_gGameState != &play->state || gSaveContext.gameMode != GAMEMODE_NORMAL) {
        return;
    }
    Player* player = GET_PLAYER(play);
    if (player == nullptr) {
        SodFail(play, "no Player in the MM play state");
        return;
    }
    sSod.driveFrames++;
    sSod.stepFrames++;
    switch (sSod.step) {
        case SodStep::Payouts:
            SodStepPayouts(play, player);
            break;
        case SodStep::Prepare:
            SodStepPrepare(play);
            break;
        case SodStep::Arrive:
            SodStepArrive(play);
            break;
        case SodStep::Midnight:
            SodStepMidnight(play);
            break;
        case SodStep::Crash:
            SodStepCrash(play);
            break;
        case SodStep::Crashing:
            SodStepCrashing(play);
            break;
        case SodStep::Done:
        case SodStep::Failed:
            break;
    }
    if (sSod.step != SodStep::Failed && sSod.step != SodStep::Done) {
        SodPump();
    }
}

#else

extern "C" bool MM_IntSoDrive_CheckArrival(PlayState* play) {
    (void)play;
    return true;
}

extern "C" void MM_IntSoDrive_Frame(PlayState* play) {
    (void)play;
}

#endif // RSBS_SINGLE_EXECUTABLE
