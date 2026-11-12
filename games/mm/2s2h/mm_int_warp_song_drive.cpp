/**
 * @file mm_int_warp_song_drive.cpp
 * @brief Majora's Mask's half of the integration row IntMMWarpSongMinuet
 *        (int-mm-warp-song-minuet, #1169 lane XW13): Ocarina of Time's Minuet of
 *        Forest played on the ocarina in South Clock Town, the warp prompt
 *        answered OK, and the crossing armed. OoT's half (the world, the created
 *        record, the arrival and the verdict) is in
 *        games/oot/soh/GameExports_SingleExe.cpp; the common half
 *        (src/common/mm_warp_song_drive.{h,c}) is locked ROM-free by
 *        MMWarpDriveSeam.
 *
 * Nothing here calls the matcher, the gates, the prompt or the departure. The
 * drive presses buttons through MM's test input seam (MM_PadMgr_SetTestButtons,
 * padmgr.c), which PadMgr_UpdateInputs ORs into controller 1, so Player, the
 * message system and the ocarina reader all see real presses, and it watches
 * the state they leave. Buttons set on frame N are read on frame N+1 (the pad
 * update runs before the tick, games/mm/src/code/graph.c), so a hold is pumped on
 * every drive frame and never skipped mid-hold.
 *
 * The steps, run from MM_IntegrationGameplayFrameTick's GP_PHASE_MM_PLAY branch
 * once the paired drive's own frames are done (games/mm/2s2h/
 * GameExports_SingleExe.cpp). Each has a deadline, and each failure prints one
 * line with the whole state (MwdrFail):
 *   PAYOUTS   the arrival pays out MM's starting checks one at a time; wait for
 *             20 quiet frames in a row (no queued or current get-item event, no
 *             textbox, no get-item action, no blocking cutscene, no transition),
 *             pressing A past a finished textbox after 15 frames and closing one
 *             nobody pressed past after 90 (fixture work). 900 frames.
 *   PREPARE   once: Link is human, MM holds the Ocarina of Time (the MM-start
 *             ruling, asserted, never repaired), it goes on C-down, the Ocarina
 *             Button Shuffle buttons when that shuffle is on, the Minuet given,
 *             two ocarina CVars forced to 0 in memory, the scene unrestricted, and
 *             the prompt's own decision OFFER before a note is played.
 *   TAKE_OUT  C-down while Link is free, retried every 30 frames, until the
 *             staff is open in free play. 300 frames.
 *   SETTLE    8 frames with nothing held.
 *   NOTES     the six notes, RSBS_MWD_NOTE_HOLD frames held and RSBS_MWD_NOTE_GAP
 *             released.
 *   MATCH     MM's matcher answers 0x80 (E1). 90 frames after the last note.
 *   PLAYED    "You played" and its close, then the prompt's box opens; no press.
 *             900 frames.
 *   PROMPT    the two-choice is up on OK with Minuet pending: mark the
 *             departure, press A once. 300 frames.
 *   DEPART    the one-shot target (MM, this entrance, OoT, Minuet's pad, the
 *             warp-song kind), seen armed or already turned into the pending
 *             switch; then the phase goes to OoT. 600 frames from A.
 *   (watch)   600 MM frames after the handover MM must have switched to OoT.
 */

#include "global.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <variant>

#if defined(RSBS_SINGLE_EXECUTABLE)

#include <libultraship/bridge/consolevariablebridge.h>

#include "context.h"
#include "entrance.h"
#include "foreign_items.h"
#include "integration_test_hooks.h"
#include "mm_warp_song_drive.h"
#include "mm_warp_song_play.h"
#include "warp_songs_mm.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/Rando/Foreign.h"
#include "2s2h/Rando/Rando.h"
#include "2s2h/Rando/WarpSongsMM.h"

extern "C" {
#include "overlays/actors/ovl_En_Test7/z_en_test7.h"
// src/audio/code_8019AF00.c and src/code/z_message.c, file scope with external
// linkage (as games/mm/2s2h/mm_warp_song_play_test.cpp declares them).
extern u32 sOcarinaInputButtonCur;
extern u8 sCurOcarinaPitch;
extern u32 sOcarinaFlags;
extern OcarinaStaff MM_sPlayingStaff;
extern u8 MM_sPlaybackState;
extern u16 sPlaybackStaffPos;
extern s16 MM_sLastPlayedSong;
}

namespace {

enum class MwdrStep { Payouts, Prepare, TakeOut, Settle, Notes, Match, Played, Prompt, Depart, Handed, Failed };

const int kMwdrSong = 0; // Minuet of Forest
const int kMwdrPayoutBudget = 900;
const int kMwdrQuietNeeded = 20;
const int kMwdrTextPressFrames = 15;
const int kMwdrTextCloseFrames = 90;
const int kMwdrTakeOutBudget = 300;
const int kMwdrTakeOutRetry = 30;
const int kMwdrSettleFrames = 8;
const int kMwdrMatchBudget = 90;
const int kMwdrPlayedBudget = 900;
const int kMwdrPromptBudget = 300;
const int kMwdrDepartBudget = 600;
const int kMwdrTransitionGrace = 3;
const int kMwdrCrossBudget = 600;

struct MwdrState {
    MwdrStep step = MwdrStep::Payouts;
    int stepFrames = 0;
    int driveFrames = 0;
    u16 held = 0;
    int heldFrames = 0;
    int lastPress = -1000;
    int quietFrames = 0;
    int textFrames = 0;
    u16 textId = 0xFFFF;
    bool relapsed = false;
    int note = 0;
    int noteFrame = 0;
    u16 marker = 0;
    bool sawPlayedText = false;
    int transitionFrames = 0;
    bool departed = false;
    int departedFrames = 0;
    bool watchFailed = false;
};
MwdrState sMwdr;

const char* MwdrStepName(MwdrStep step) {
    switch (step) {
        case MwdrStep::Payouts:
            return "payouts";
        case MwdrStep::Prepare:
            return "prepare";
        case MwdrStep::TakeOut:
            return "take out";
        case MwdrStep::Settle:
            return "settle";
        case MwdrStep::Notes:
            return "notes";
        case MwdrStep::Match:
            return "match";
        case MwdrStep::Played:
            return "you played";
        case MwdrStep::Prompt:
            return "the prompt";
        case MwdrStep::Depart:
            return "departure";
        case MwdrStep::Handed:
            return "handed to OoT";
        case MwdrStep::Failed:
            return "failed";
    }
    return "?";
}

int MwdrEligibleChecks() {
    int n = 0;
    for (int i = 0; i < (int)RC_MAX; i++) {
        n += (RANDO_SAVE_CHECKS[i].eligible && !RANDO_SAVE_CHECKS[i].obtained) ? 1 : 0;
    }
    return n;
}

bool MwdrEventNone() {
    return std::holds_alternative<GIEventNone>(MM_GameEvents_Current());
}

/** The ACTOR_EN_TEST7 the warp prompt's OK spawns (params 0x8000 | song), or NULL. */
EnTest7* MwdrFindEnTest7(PlayState* play) {
    if (play == nullptr) {
        return nullptr;
    }
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first; a != nullptr; a = a->next) {
        if (a->id == ACTOR_EN_TEST7 && (u16)a->params == MM_WarpSongMM_EnTest7Params((uint8_t)kMwdrSong)) {
            return reinterpret_cast<EnTest7*>(a);
        }
    }
    return nullptr;
}

/** The departure path: the song-warp cutscene id, En_Test7, the skip enhancement. */
std::string MwdrDeparturePath(PlayState* play) {
    char buf[256];
    EnTest7* t7 = MwdrFindEnTest7(play);
    snprintf(buf, sizeof(buf), "playerCsIds[SONG_WARP] %d (%s), En_Test7 %s (timer %d, action %p), skip cutscene %d",
             play != nullptr ? (int)play->playerCsIds[PLAYER_CS_ID_SONG_WARP] : -2,
             play != nullptr && play->playerCsIds[PLAYER_CS_ID_SONG_WARP] != CS_ID_NONE ? "En_Test7 path"
                                                                                        : "Player's plain fade",
             t7 != nullptr ? "present" : "absent", t7 != nullptr ? (int)t7->timer : -1,
             t7 != nullptr ? (void*)t7->actionFunc : nullptr,
             (int)CVarGetInteger("gEnhancements.Songs.SkipSoaringCutscene", 0));
    return buf;
}

/** Every value the drive's steps are judged by, on one line. */
std::string MwdrDump(PlayState* play) {
    std::string out;
    char buf[640];
    if (play == nullptr) {
        return "no play state";
    }
    MessageContext* m = &play->msgCtx;
    const OcarinaStaff* playback = AudioOcarina_GetPlaybackStaff();
    snprintf(buf, sizeof(buf),
             "msg: mode %d state %d textId 0x%04X endType 0x%02X choice %d; ocarina: mode %d action %d lastPlayed "
             "0x%02X sLastPlayed 0x%02X songPlayed 0x%02X playing staff %u/%u playback staff %u/%u; audio: playback "
             "state %u staff pos %u flags 0x%08X input 0x%04X pitch %u tasks %d",
             (int)m->msgMode, (int)MM_Message_GetState(m), (unsigned)m->currentTextId, (unsigned)m->textboxEndType,
             (int)m->choiceIndex, (int)m->ocarinaMode, (int)m->ocarinaAction, (unsigned)m->lastPlayedSong,
             (unsigned)(u16)MM_sLastPlayedSong, (unsigned)m->songPlayed, (unsigned)MM_sPlayingStaff.state,
             (unsigned)MM_sPlayingStaff.pos, playback != nullptr ? (unsigned)playback->state : 0u,
             playback != nullptr ? (unsigned)playback->pos : 0u, (unsigned)MM_sPlaybackState,
             (unsigned)sPlaybackStaffPos, (unsigned)sOcarinaFlags, (unsigned)sOcarinaInputButtonCur,
             (unsigned)sCurOcarinaPitch, (int)gAudioCtx.totalTaskCount);
    out += buf;

    ComboWarpTarget t;
    ComboWarp_Peek(&t);
    snprintf(buf, sizeof(buf),
             "; warp: pending song %d, target armed %d (source %d marker 0x%04X target %d 0x%04X kind %u), switch "
             "pending %d requested %d (target %d 0x%04X kind %u)",
             Combo_MMWarpPlay_PendingSong(), t.armed ? 1 : 0, (int)t.sourceGame, (unsigned)t.marker, (int)t.targetGame,
             (unsigned)t.targetEntrance, (unsigned)t.kind, Combo_IsCrossGameSwitch() ? 1 : 0,
             Combo_IsGameSwitchRequested() ? 1 : 0, (int)Entrance_GetSwitchTargetGame(),
             (unsigned)Combo_GetSwitchTargetEntrance(), (unsigned)Entrance_GetSwitchKind());
    out += buf;

    ComboMMWarpPlayInputs in;
    Combo_MMWarpPlay_GatherInputs(kMwdrSong, play->interfaceCtx.restrictions.songOfSoaring != 0, &in);
    const ComboMMWarpPlayGate gate = Combo_MMWarpPlay_Decide(&in);
    snprintf(buf, sizeof(buf),
             "; gate: %s (paired %u ootPlayable %u switchPending %u restricted %u ootAge %d record %u resolvable %u); "
             "world: usable 0x%02X owned 0x%02X rule2 0x%08X record status %d flags 0x%02X dest 0x%04X",
             Combo_MMWarpPlay_GateName(gate), (unsigned)in.paired, (unsigned)in.ootPlayable, (unsigned)in.switchPending,
             (unsigned)in.soaringRestricted, (int)in.ootAge, (unsigned)in.recordStatus, (unsigned)in.destResolvable,
             (unsigned)MM_WarpSongMM_UsableMask(), (unsigned)Combo_MMWarpSongsOwned(),
             (unsigned)Combo_ComboCrossItemFlags2(), Combo_OoTWarpRecord_Status(),
             (unsigned)Combo_OoTWarpRecord_Flags(), (unsigned)Combo_OoTWarpRecord_Dest(kMwdrSong));
    out += buf;

    Player* player = GET_PLAYER(play);
    snprintf(buf, sizeof(buf),
             "; scene %d entrance 0x%04X transition %d/%d cutscene %d; player: form %d flags1 0x%08X flags2 0x%08X "
             "flags3 0x%08X blocking %d, C-down status %u item %u; payouts: queued %d current event %d eligible checks "
             "%d",
             (int)play->sceneId, (unsigned)(u16)gSaveContext.save.entrance, (int)play->transitionTrigger,
             (int)play->transitionMode, (int)play->csCtx.state, player != nullptr ? (int)player->transformation : -1,
             player != nullptr ? (unsigned)player->stateFlags1 : 0u,
             player != nullptr ? (unsigned)player->stateFlags2 : 0u,
             player != nullptr ? (unsigned)player->stateFlags3 : 0u,
             player != nullptr ? (MM_Player_InBlockingCsMode(play, player) ? 1 : 0) : -1,
             (unsigned)gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN], (unsigned)BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_DOWN),
             (int)MM_GameEvents_Queue().size(), (int)MM_GameEvents_Current().index(), MwdrEligibleChecks());
    out += buf;

    snprintf(buf, sizeof(buf),
             "; departure: %s; cvars: DpadNoDropOcarinaInput %d CustomOcarina.Enabled %d; drive: step '%s' frame %d "
             "(drive frame %d), held 0x%04X for %d, latch 0x%04X, note %d",
             MwdrDeparturePath(play).c_str(), (int)CVarGetInteger("gEnhancements.DpadNoDropOcarinaInput", 0),
             (int)CVarGetInteger("gSettings.CustomOcarina.Enabled", 0), MwdrStepName(sMwdr.step), sMwdr.stepFrames,
             sMwdr.driveFrames, (unsigned)sMwdr.held, sMwdr.heldFrames, (unsigned)MM_PadMgr_GetTestButtons(),
             sMwdr.note + 1);
    out += buf;
    return out;
}

void MwdrFail(PlayState* play, const char* why) {
    if (sMwdr.step == MwdrStep::Failed) {
        return;
    }
    const std::string state = MwdrDump(play);
    const MwdrStep at = sMwdr.step;
    sMwdr.step = MwdrStep::Failed;
    sMwdr.held = 0;
    sMwdr.heldFrames = 0;
    MM_PadMgr_SetTestButtons(0);
    std::string msg = "int-mm-warp-song-minuet, step '";
    msg += MwdrStepName(at);
    msg += "': ";
    msg += why;
    msg += " [";
    msg += state;
    msg += "]";
    IntegrationTest_GameplayFail(msg.c_str());
}

void MwdrNext(MwdrStep step) {
    fprintf(stderr, "[PFC] mm warp song: step '%s' done after %d frame(s) (drive frame %d)\n", MwdrStepName(sMwdr.step),
            sMwdr.stepFrames, sMwdr.driveFrames);
    fflush(stderr);
    sMwdr.step = step;
    sMwdr.stepFrames = 0;
    sMwdr.lastPress = -1000;
    sMwdr.textFrames = 0;
    sMwdr.textId = 0xFFFF;
}

void MwdrHold(u16 buttons, int frames) {
    sMwdr.held = buttons;
    sMwdr.heldFrames = frames;
    sMwdr.lastPress = sMwdr.stepFrames;
}

/** Once per drive frame, after the step: the latch is consumed by the next pad
 *  update, so a held button is set again on every frame of its hold. */
void MwdrPump() {
    if (sMwdr.heldFrames > 0) {
        MM_PadMgr_SetTestButtons(sMwdr.held);
        sMwdr.heldFrames--;
    } else {
        sMwdr.held = 0;
    }
}

/** A finished textbox the drive did not open: A after 15 frames, closed after
 *  90 (fixture work, as GpSongHostMMFrame does). True while one is up. */
bool MwdrStrayTextbox(PlayState* play) {
    MessageContext* m = &play->msgCtx;
    const u8 state = MM_Message_GetState(m);
    if (m->msgMode != MSGMODE_TEXT_DONE || (state != TEXT_STATE_DONE && state != TEXT_STATE_EVENT)) {
        sMwdr.textFrames = 0;
        sMwdr.textId = 0xFFFF;
        return m->msgMode != MSGMODE_NONE;
    }
    if (m->currentTextId != sMwdr.textId) {
        sMwdr.textId = m->currentTextId;
        sMwdr.textFrames = 0;
    }
    sMwdr.textFrames++;
    if (sMwdr.textFrames >= kMwdrTextCloseFrames) {
        fprintf(stderr, "[PFC] mm warp song: closed a payout textbox nobody pressed past (text 0x%04X)\n",
                (unsigned)m->currentTextId);
        fflush(stderr);
        MM_Message_CloseTextbox(play);
        sMwdr.textFrames = 0;
    } else if (sMwdr.textFrames % kMwdrTextPressFrames == 0 && sMwdr.heldFrames == 0) {
        MwdrHold(BTN_A, 1);
    }
    return true;
}

bool MwdrQuiet(PlayState* play, Player* player) {
    return MM_GameEvents_Queue().empty() && MwdrEventNone() && play->msgCtx.msgMode == MSGMODE_NONE &&
           !(player->stateFlags1 & PLAYER_STATE1_400) && !MM_Player_InBlockingCsMode(play, player) &&
           play->transitionTrigger == TRANS_TRIGGER_OFF;
}

/** MM_Player_ProcessItemButtons' own early returns, plus no textbox, transition,
 *  cutscene or disabled C-down. */
bool MwdrLinkFree(PlayState* play, Player* player) {
    return !(player->stateFlags1 & (PLAYER_STATE1_CARRYING_ACTOR | PLAYER_STATE1_20000000)) &&
           !(player->stateFlags2 & PLAYER_STATE2_2000000) && !(player->stateFlags3 & PLAYER_STATE3_20000000) &&
           !func_801240DC(player) && play->msgCtx.msgMode == MSGMODE_NONE &&
           play->transitionTrigger == TRANS_TRIGGER_OFF && play->transitionMode == TRANS_MODE_OFF &&
           play->csCtx.state == CS_STATE_IDLE && !MM_Player_InBlockingCsMode(play, player) &&
           gSaveContext.buttonStatus[EQUIP_SLOT_C_DOWN] != BTN_DISABLED;
}

void MwdrStepPayouts(PlayState* play, Player* player) {
    if (sMwdr.stepFrames == 1) {
        fprintf(stderr, "[PFC] mm warp song: waiting for the arrival's payouts (queued %d, eligible checks %d)\n",
                (int)MM_GameEvents_Queue().size(), MwdrEligibleChecks());
        fflush(stderr);
    }
    (void)MwdrStrayTextbox(play);
    sMwdr.quietFrames = MwdrQuiet(play, player) ? sMwdr.quietFrames + 1 : 0;
    if (sMwdr.quietFrames >= kMwdrQuietNeeded) {
        fprintf(stderr, "[PFC] mm warp song: payouts drained (%d quiet frames; eligible checks %d)\n",
                sMwdr.quietFrames, MwdrEligibleChecks());
        MwdrNext(sMwdr.relapsed ? MwdrStep::TakeOut : MwdrStep::Prepare);
        return;
    }
    if (sMwdr.stepFrames >= kMwdrPayoutBudget) {
        MwdrFail(play, "starting-check payouts never drained");
    }
}

void MwdrStepPrepare(PlayState* play, Player* player) {
    static const char* const kForeignDrives[] = { "RSBS_PFC_SONG_HOST", "RSBS_GP_MM_FOREIGN_MODEL", "RSBS_GP_MM_SHOP" };
    for (const char* env : kForeignDrives) {
        if (std::getenv(env) != nullptr) {
            std::string why = "another paired drive is armed in this run (";
            why += env;
            why += "): it presses and gives on its own frames";
            MwdrFail(play, why.c_str());
            return;
        }
    }
    if (player->transformation != PLAYER_FORM_HUMAN) {
        MwdrFail(play, "Link is not in human form, so the ocarina he takes out is not the Ocarina of Time");
        return;
    }
    if (INV_CONTENT(ITEM_OCARINA_OF_TIME) != ITEM_OCARINA_OF_TIME) {
        MwdrFail(play, "the paired MM half has no Ocarina of Time; the MM-start ruling regressed");
        return;
    }
    // C-down: not one of the Minuet's notes, so the press that takes the ocarina
    // out (the reader's start button) never overlaps a note.
    BUTTON_ITEM_EQUIP(0, EQUIP_SLOT_C_DOWN) = ITEM_OCARINA_OF_TIME;
    C_SLOT_EQUIP(0, EQUIP_SLOT_C_DOWN) = SLOT_OCARINA;
    const bool buttonShuffle = Rando::Foreign::ShuffleOcarinaButtonsForWorld(RANDO_SAVE_OPTIONS);
    if (buttonShuffle) {
        Flags_SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_A);
        Flags_SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_C_DOWN);
        Flags_SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_C_RIGHT);
        Flags_SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_C_LEFT);
        Flags_SetRandoInf(RANDO_INF_OBTAINED_OCARINA_BUTTON_C_UP);
    }
    MM_WarpSongMM_Give(RI_SONG_MINUET);
    if ((MM_WarpSongMM_UsableMask() & 0x01u) == 0u) {
        MwdrFail(play, "the Minuet of Forest (MM) is not usable after it was given (rule, pairing or ownership)");
        return;
    }
    // A staged config must not change the path under test (in memory only, put
    // back at the verdict by IntegrationTest_SoaringCleanup).
    IntegrationTest_SoaringSetCVarInt("gSettings.CustomOcarina.Enabled", 0);
    IntegrationTest_SoaringSetCVarInt("gEnhancements.DpadNoDropOcarinaInput", 0);
    if (play->interfaceCtx.restrictions.songOfSoaring != 0) {
        MwdrFail(play, "the scene restricts the Song of Soaring, so every warp song is refused here");
        return;
    }
    ComboMMWarpPlayInputs in;
    Combo_MMWarpPlay_GatherInputs(kMwdrSong, false, &in);
    const ComboMMWarpPlayGate gate = Combo_MMWarpPlay_Decide(&in);
    if (gate != COMBO_MMWP_OFFER) {
        std::string why = "before any note, the prompt's decision is already a refusal: ";
        why += Combo_MMWarpPlay_GateName(gate);
        MwdrFail(play, why.c_str());
        return;
    }
    sMwdr.marker = (u16)gSaveContext.save.entrance;
    fprintf(stderr,
            "[PFC] mm warp song: prepared in scene %d at entrance 0x%04X: Ocarina of Time on C-down, Ocarina Button "
            "Shuffle %d, Minuet owned (usable 0x%02X), skip cutscene %d, prompt decision 'offer'\n",
            (int)play->sceneId, (unsigned)sMwdr.marker, buttonShuffle ? 1 : 0, (unsigned)MM_WarpSongMM_UsableMask(),
            (int)CVarGetInteger("gEnhancements.Songs.SkipSoaringCutscene", 0));
    fflush(stderr);
    MwdrNext(MwdrStep::TakeOut);
}

void MwdrStepTakeOut(PlayState* play, Player* player) {
    if (std::holds_alternative<GIEventGiveItem>(MM_GameEvents_Current())) {
        sMwdr.held = 0;
        sMwdr.heldFrames = 0;
        if (sMwdr.relapsed) {
            MwdrFail(play, "a payout arrived during the take-out");
            return;
        }
        sMwdr.relapsed = true;
        sMwdr.quietFrames = 0;
        fprintf(stderr, "[PFC] mm warp song: a get-item event became current during the take-out; waiting for the "
                        "payouts again (once)\n");
        MwdrNext(MwdrStep::Payouts);
        return;
    }
    if (play->msgCtx.msgMode == MSGMODE_OCARINA_PLAYING) {
        if (play->msgCtx.ocarinaAction != OCARINA_ACTION_FREE_PLAY) {
            MwdrFail(play, "the ocarina opened outside free play (an ocarina actor nearby?); the matcher answers in "
                           "free play only");
            return;
        }
        MwdrNext(MwdrStep::Settle);
        return;
    }
    (void)MwdrStrayTextbox(play);
    if (sMwdr.heldFrames == 0 && sMwdr.stepFrames - sMwdr.lastPress >= kMwdrTakeOutRetry &&
        MM_GameEvents_Queue().empty() && MwdrEventNone() && MwdrLinkFree(play, player)) {
        MwdrHold(BTN_CDOWN, 2);
    }
    if (sMwdr.stepFrames >= kMwdrTakeOutBudget) {
        MwdrFail(play, "Link never took out the ocarina from C-down");
    }
}

void MwdrStepSettle(PlayState* play) {
    if (play->msgCtx.msgMode != MSGMODE_OCARINA_PLAYING) {
        MwdrFail(play, "the ocarina closed before the first note");
        return;
    }
    if (sMwdr.stepFrames >= kMwdrSettleFrames) {
        sMwdr.note = 0;
        sMwdr.noteFrame = 0;
        MwdrNext(MwdrStep::Notes);
    }
}

void MwdrStepNotes(PlayState* play) {
    const int count = Combo_MMWarpDrive_NoteCount(kMwdrSong);
    if (sMwdr.noteFrame == 0) {
        if (play->msgCtx.msgMode != MSGMODE_OCARINA_PLAYING) {
            char why[128];
            snprintf(why, sizeof(why), "the ocarina left free play before note %d of %d", sMwdr.note + 1, count);
            MwdrFail(play, why);
            return;
        }
        MwdrHold(Combo_MMWarpDrive_NotePad(kMwdrSong, sMwdr.note), RSBS_MWD_NOTE_HOLD);
    }
    sMwdr.noteFrame++;
    if (sMwdr.noteFrame >= RSBS_MWD_NOTE_HOLD + RSBS_MWD_NOTE_GAP) {
        sMwdr.noteFrame = 0;
        sMwdr.note++;
        if (sMwdr.note >= count) {
            sMwdr.note = count - 1;
            MwdrNext(MwdrStep::Match);
        }
    }
}

void MwdrStepMatch(PlayState* play) {
    const u16 want = Combo_MMWarpPlay_StaffState(kMwdrSong);
    if (play->msgCtx.lastPlayedSong == want && play->msgCtx.msgMode != MSGMODE_OCARINA_PLAYING) {
        MwdrNext(MwdrStep::Played);
        return;
    }
    if (sMwdr.stepFrames >= kMwdrMatchBudget) {
        MwdrFail(play, "the six notes were played but MM's matcher never answered the Minuet (0x80)");
    }
}

bool MwdrRefused(PlayState* play) {
    return play->msgCtx.ocarinaMode == OCARINA_MODE_PROCESS_RESTRICTED_SONG ||
           play->msgCtx.currentTextId == RSBS_MMWP_TEXT_RESTRICTED;
}

void MwdrStepPlayed(PlayState* play) {
    MessageContext* m = &play->msgCtx;
    if (m->currentTextId == CUSTOM_MESSAGE_ID && m->textboxEndType != TEXTBOX_ENDTYPE_TWO_CHOICE) {
        sMwdr.sawPlayedText = true;
    }
    if (MwdrRefused(play)) {
        MwdrFail(play, "the song was refused (0x1B95 / the restricted-song mode); see the gate");
        return;
    }
    if (m->currentTextId == CUSTOM_MESSAGE_ID && m->textboxEndType == TEXTBOX_ENDTYPE_TWO_CHOICE &&
        m->ocarinaMode == OCARINA_MODE_ACTIVE) {
        MwdrNext(MwdrStep::Prompt);
        return;
    }
    if (sMwdr.stepFrames >= kMwdrPlayedBudget) {
        MwdrFail(play, m->msgMode == MSGMODE_DISPLAY_SONG_PLAYED
                           ? "stalled in DISPLAY_SONG_PLAYED: the playback of OoT's notes never ended (see playback "
                             "state, staff pos and tasks; rerun with RSBS_AUDIO_PROBE=1)"
                           : "the warp prompt never opened after 'You played'");
    }
}

void MwdrStepPrompt(PlayState* play) {
    MessageContext* m = &play->msgCtx;
    if (MwdrRefused(play)) {
        MwdrFail(play, "the song was refused (0x1B95 / the restricted-song mode); see the gate");
        return;
    }
    const bool ready = m->msgMode == MSGMODE_TEXT_DONE && MM_Message_GetState(m) == TEXT_STATE_CHOICE &&
                       m->textboxEndType == TEXTBOX_ENDTYPE_TWO_CHOICE && m->ocarinaMode == OCARINA_MODE_ACTIVE &&
                       Combo_MMWarpPlay_PendingSong() == kMwdrSong && m->choiceIndex == 0;
    if (ready) {
        // The paired return leg's toast check needs the departure mark.
        IntegrationTest_PairedMarkCrossing("Ocarina of Time");
        fprintf(stderr,
                "[PFC] mm warp song: the prompt is up on OK with Minuet pending ('You played' seen %d); "
                "pressing A\n",
                sMwdr.sawPlayedText ? 1 : 0);
        fflush(stderr);
        MwdrHold(BTN_A, 1);
        MwdrNext(MwdrStep::Depart);
        return;
    }
    if (sMwdr.stepFrames >= kMwdrPromptBudget) {
        MwdrFail(play, "the warp prompt never stood on OK with Minuet pending");
    }
}

void MwdrStepDepart(PlayState* play) {
    ComboWarpTarget t;
    ComboWarp_Peek(&t);
    const bool armed = Combo_MMWarpDrive_TargetIsCrossing(t.armed, (int)t.sourceGame, t.marker, (int)t.targetGame,
                                                          t.targetEntrance, t.kind, sMwdr.marker, kMwdrSong);
    const bool switching =
        Combo_MMWarpDrive_SwitchIsCrossing(Entrance_IsCrossGameSwitch(), (int)Entrance_GetSwitchTargetGame(),
                                           Entrance_GetSwitchTargetEntrance(), Entrance_GetSwitchKind(), kMwdrSong);
    if (armed || switching) {
        fprintf(stderr,
                "[PFC] mm warp song 'the crossing': armed {MM 0x%04X -> OoT 0x%04X, warp-song kind} seen %s after %d "
                "frame(s) from A; %s\n",
                (unsigned)sMwdr.marker, (unsigned)Combo_MMWarpPlay_PadEntrance(kMwdrSong),
                armed ? "as the one-shot target" : "as the pending switch (the target already spent)", sMwdr.stepFrames,
                MwdrDeparturePath(play).c_str());
        fflush(stderr);
        sMwdr.held = 0;
        sMwdr.heldFrames = 0;
        MM_PadMgr_SetTestButtons(0);
        sMwdr.departed = true;
        sMwdr.departedFrames = 0;
        IntegrationTest_SetGameplayPhase(GP_PHASE_OOT_RETURN);
        MwdrNext(MwdrStep::Handed);
        return;
    }
    if (t.armed) {
        MwdrFail(play, "a one-shot target is armed, but not the Minuet's crossing from this entrance");
        return;
    }
    if (MwdrRefused(play)) {
        MwdrFail(play, "the song was refused after OK (0x1B95 / the restricted-song mode); see the gate");
        return;
    }
    // Not failed on OCARINA_MODE_END: the closing box may write it after Player
    // took the warp. A "No" leaves nothing armed and no fade, and the budget below
    // names it with ocarinaMode in the dump.
    sMwdr.transitionFrames = play->transitionTrigger != TRANS_TRIGGER_OFF ? sMwdr.transitionFrames + 1 : 0;
    if (sMwdr.transitionFrames > kMwdrTransitionGrace) {
        MwdrFail(play, "the departure's fade started with no armed crossing (the departure was refused: see the "
                       "'[MM] warp song' and '[ENTRANCE] warp target not armed' lines)");
        return;
    }
    if (sMwdr.stepFrames >= kMwdrDepartBudget) {
        MwdrFail(play, "no crossing was armed within 600 frames of OK (ocarinaMode END with nothing armed: the "
                       "prompt answered No, or the press missed the choice)");
    }
}

} // namespace

extern "C" void MM_IntMwDrive_Frame(PlayState* play) {
    if (play == nullptr || sMwdr.step == MwdrStep::Failed || sMwdr.step == MwdrStep::Handed) {
        return;
    }
    Player* player = GET_PLAYER(play);
    if (player == nullptr) {
        MwdrFail(play, "no Player in the MM play state");
        return;
    }
    sMwdr.driveFrames++;
    sMwdr.stepFrames++;
    switch (sMwdr.step) {
        case MwdrStep::Payouts:
            MwdrStepPayouts(play, player);
            break;
        case MwdrStep::Prepare:
            MwdrStepPrepare(play, player);
            break;
        case MwdrStep::TakeOut:
            MwdrStepTakeOut(play, player);
            break;
        case MwdrStep::Settle:
            MwdrStepSettle(play);
            break;
        case MwdrStep::Notes:
            MwdrStepNotes(play);
            break;
        case MwdrStep::Match:
            MwdrStepMatch(play);
            break;
        case MwdrStep::Played:
            MwdrStepPlayed(play);
            break;
        case MwdrStep::Prompt:
            MwdrStepPrompt(play);
            break;
        case MwdrStep::Depart:
            MwdrStepDepart(play);
            break;
        case MwdrStep::Handed:
        case MwdrStep::Failed:
            break;
    }
    if (sMwdr.step != MwdrStep::Failed && sMwdr.step != MwdrStep::Handed) {
        MwdrPump();
    }
}

extern "C" void MM_IntMwDrive_WatchDeparture(void) {
    if (!sMwdr.departed || sMwdr.watchFailed || Context_GetCurrentGame() != GAME_MM) {
        return;
    }
    if (++sMwdr.departedFrames < kMwdrCrossBudget) {
        return;
    }
    sMwdr.watchFailed = true;
    const std::string state = MwdrDump(MM_gPlayState);
    std::string msg = "int-mm-warp-song-minuet, step 'handed to OoT': MM still running 600 frames after the warp "
                      "armed: the fade never crossed [";
    msg += state;
    msg += "]";
    IntegrationTest_GameplayFail(msg.c_str());
}

#else

extern "C" void MM_IntMwDrive_Frame(PlayState* play) {
    (void)play;
}

extern "C" void MM_IntMwDrive_WatchDeparture(void) {
}

#endif // RSBS_SINGLE_EXECUTABLE
