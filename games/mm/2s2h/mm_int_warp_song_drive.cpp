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
 *             Button Shuffle buttons when that shuffle is on, the Minuet given
 *             (in int-mm-warp-song-minuet-shared, #1169 XW14, NOT given: it must
 *             already be in MM's arrival apply's give record, the family word
 *             and MM's ownership byte),
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
 *
 * int-mm-shared-sun-saria (#1587, the in-game rows of stages T and A) rides the
 * same steps with a song cursor: PREPARE also checks the shared Sun's Song and
 * Saria's Song facts (the apply's give record, the family word, the quest bits,
 * MM's forced rows, the fanfare fix, the playable mask, the scene's time speed and
 * the day window), then the cursor plays the Sun's Song, Saria's Song and the
 * Minuet in that order, each through TAKE_OUT, SETTLE, NOTES and MATCH:
 *   SUN_TEXT   the played text: 0x004B in 0x1B78's box with stage T's bytes, never
 *              0x1B7D; ends at the text's close (msgMode NONE). 900 frames.
 *   SUN_EFFECT after the close: SPEED_TIME seen, then INACTIVE, the clock past
 *              18:00 and R_TIME_SPEED back to its PREPARE value. 900 frames.
 *   SARIA_END  no 0x1B95, no restricted mode, no custom text, nothing pressed;
 *              ends with Link free for 20 frames, the song still owned. 900 frames.
 * A background sampler (MwdrSunSample) runs from the Sun's MATCH through
 * SUN_EFFECT: START is never visible from this tick (OceffSpot_End sets it and
 * Interface_Update turns it into SPEED_TIME in the same Play_Update), so the
 * first SPEED_TIME frame is what it records. Question 3 is recorded only
 * (MwdrQ3Sample, a same-scene re-arm count from SUN_EFFECT's end to DEPART).
 * The old variants keep the Minuet cursor, so their steps, budgets and presses
 * are unchanged; they gain only frame-free rules-off reads at PREPARE (C1-C4).
 */

#include "global.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <variant>

#if defined(RSBS_SINGLE_EXECUTABLE)

#include <libultraship/bridge/consolevariablebridge.h>

#include "combo_song_pool.h" // #1587: Combo_SharedSongFrozenShared, SariasSongHint's gate
#include "context.h"
#include "entrance.h"
#include "foreign_items.h"
#include "integration_test_hooks.h"
#include "mm_warp_song_drive.h"
#include "mm_warp_song_play.h"
#include "shared_resources.h" // #1169 XW14: the apply's give record, the family word
#include "warp_songs_mm.h"
#include "2s2h/CustomMessage/CustomMessage.h"
#include "2s2h/GameInteractor/GameInteractor.h"
#include "2s2h/ShipInit.hpp" // #1587: re-run FasterSongPlayback's registrar after its in-memory 0
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
// #1587: src/code/z_message.c, as SariasSongHint.cpp and
// mm_crossing_song_playable_test.cpp declare them.
extern s16 sOcarinaSongFanfares[17];
u32 MM_Message_BuildOcarinaAvailableSongs(void);
#include "message_data_static.h" // MessageTableEntry (the vanilla headers of 0x1B78 / 0x1B7D)
}

namespace {

enum class MwdrStep {
    Payouts,
    Prepare,
    TakeOut,
    Settle,
    Notes,
    Match,
    SunText,
    SunEffect,
    SariaEnd,
    Played,
    Prompt,
    Depart,
    Handed,
    Failed
};

// #1587: the song the cursor plays next. The old variants hold Minuet throughout.
enum class MwdrSong { Sun, Saria, Minuet };

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
// #1587 (int-mm-shared-sun-saria only).
const int kMwdrSunTextBudget = 900;   // PLAYED's: the playback, then "You played"
const int kMwdrSunEffectBudget = 900; // about 100 frames from 06:00-07:00 to 18:00 at 400 a frame
const int kMwdrSariaEndBudget = 900;  // PLAYED's: the playback, the text and its close
const int kMwdrSariaFreeNeeded = 20;  // Link free this many frames in a row: the song is over
const int kMwdrTimeSpeedSun = 400;    // z_parameter.c's R_TIME_SPEED while SUNSSONG_SPEED_TIME
const u16 kMwdrSunTextVanilla = (u16)(RSBS_MMSUN_TEXT_VANILLA_BASE + RSBS_MMSUN_SONG_STATE); // 0x1B7D
const u16 kMwdrSariaTextVanilla = (u16)(RSBS_MMSUN_TEXT_VANILLA_BASE + OCARINA_SONG_SARIAS); // 0x1B77
// Stage T's bytes after the 11-byte header (mm_suns_song_played_text_test.cpp, S3):
// "You played the " yellow "Sun's Song" white "!" and the end code.
const char kMwdrSunTextBody[] = "You played the \x04Sun's Song\x00!\xBF";
const size_t kMwdrSunTextBodyLen = sizeof(kMwdrSunTextBody) - 1; // 29
const size_t kMwdrMsgHeaderLen = 11;

struct MwdrSunWatch {
    bool armed = false;       // from the Sun's MATCH through SUN_EFFECT
    int matchFrame = -1;      // drive frame of the Sun's MATCH
    int oceffLastSeen = -1;   // last drive frame an ACTOR_OCEFF_SPOT was in ITEMACTION
    int firstSpeedFrame = -1; // first drive frame with SUNSSONG_SPEED_TIME
    int firstSpeedMsgMode = -1;
    int firstSpeedTextId = -1;
    int firstSpeedOcarinaMode = -1;
    int speedFrames = 0;      // SPEED_TIME frames
    int speedFramesMsgUp = 0; // of which with msgMode != NONE (the clock cannot move)
    int startFrames = 0;      // SUNSSONG_START seen (never expected: see the file comment)
    int msgNoneFrame = -1;    // first drive frame msgMode was NONE after the match
    s16 prePlaySpeed = 0;     // R_TIME_SPEED at PREPARE
    u16 sceneTimeSpeed = 0;   // play->envCtx.sceneTimeSpeed at PREPARE
    u16 timeAtFirstSpeed = 0;
    // SUN_TEXT
    int customTextFrames = 0; // frames with 0x004B in MSGMODE_16..18
    bool textChecked = false;
    int textCloseFrame = -1;
    // Question 3: the same-scene re-arm window, SUN_EFFECT's end to DEPART.
    bool q3Armed = false;
    int q3Frames = 0;
    int q3RearmFrames = 0;
    int q3OceffSightings = 0;
    int q3BlockSunsSong = 0;
};

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
    // #1587: int-mm-shared-sun-saria's cursor and records.
    MwdrSong song = MwdrSong::Minuet;
    MwdrSunWatch sun;
    int sariaFreeFrames = 0;
    int sariaPlayedTextId = -1;
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
        case MwdrStep::SunText:
            return "sun's song played text";
        case MwdrStep::SunEffect:
            return "sun's song effect";
        case MwdrStep::SariaEnd:
            return "saria's song end";
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

const char* MwdrSongName(MwdrSong song) {
    switch (song) {
        case MwdrSong::Sun:
            return "the Sun's Song";
        case MwdrSong::Saria:
            return "Saria's Song";
        case MwdrSong::Minuet:
            return "the Minuet of Forest";
    }
    return "?";
}

/** The row the failure lines name: the old variants keep their own name. */
const char* MwdrRowName() {
    return IntegrationTest_MMSharedSunSaria() ? "int-mm-shared-sun-saria" : "int-mm-warp-song-minuet";
}

/** MM's own song index of the Sun's Song or Saria's Song (z64ocarina.h); -1 for the
 *  Minuet, which is OoT's and played from OoTMM's buttons (Combo_MMWarpDrive_*). */
int MwdrMMSongIndex(MwdrSong song) {
    return song == MwdrSong::Sun ? (int)OCARINA_SONG_SUNS : song == MwdrSong::Saria ? (int)OCARINA_SONG_SARIAS : -1;
}

/** How many notes @p song has: the Minuet's from OoTMM's buttons, the Sun's Song
 *  and Saria's Song from MM's own table, the one MM's matcher reads
 *  (gOcarinaSongButtons, code_8019AF00.c), so the drive cannot play notes the
 *  matcher does not know. */
int MwdrNoteCount(MwdrSong song) {
    const int index = MwdrMMSongIndex(song);
    if (index < 0) {
        return Combo_MMWarpDrive_NoteCount(kMwdrSong);
    }
    const int n = (int)gOcarinaSongButtons[index].numButtons;
    const int cap =
        (int)(sizeof(gOcarinaSongButtons[index].buttonIndex) / sizeof(gOcarinaSongButtons[index].buttonIndex[0]));
    return n < cap ? n : cap;
}

/** The pad bit of note @p note of @p song (0 out of range). */
u16 MwdrNotePad(MwdrSong song, int note) {
    const int index = MwdrMMSongIndex(song);
    if (index < 0) {
        return Combo_MMWarpDrive_NotePad(kMwdrSong, note);
    }
    if (note < 0 || note >= MwdrNoteCount(song)) {
        return 0;
    }
    return Combo_MMWarpDrive_PadButton(gOcarinaSongButtons[index].buttonIndex[note]);
}

/** What msgCtx.lastPlayedSong reads once MM's matcher took @p song: the warp
 *  song's staff state (0x80) for the Minuet, MM's own index for the others
 *  (z_message.c, msgCtx->lastPlayedSong = msgCtx->ocarinaStaff->state). */
u16 MwdrWant(MwdrSong song) {
    const int index = MwdrMMSongIndex(song);
    return index < 0 ? (u16)Combo_MMWarpPlay_StaffState(kMwdrSong) : (u16)index;
}

/** Is a Sun's Song effect actor (ACTOR_OCEFF_SPOT, ACTORCAT_ITEMACTION) in the
 *  actor list? */
bool MwdrOceffSpotPresent(PlayState* play) {
    for (Actor* a = play->actorCtx.actorLists[ACTORCAT_ITEMACTION].first; a != nullptr; a = a->next) {
        if (a->id == ACTOR_OCEFF_SPOT) {
            return true;
        }
    }
    return false;
}

/** The first five header bytes (box, position, icon, next id hi/lo) of the vanilla
 *  message @p textId in MM's table, walked as CustomMessage::LoadVanillaMessageTableEntry
 *  walks it; false when the id is absent or its segment short (that function would
 *  read the terminator instead). */
bool MwdrVanillaHeader(PlayState* play, u16 textId, u8 out[5]) {
    const MessageTableEntry* e = play->msgCtx.messageTableNES;
    if (e == nullptr) {
        return false;
    }
    for (; e->textId != 0xFFFF; e++) {
        if (e->textId != textId) {
            continue;
        }
        if (e->segment == nullptr || e->msgSize < 5u) {
            return false;
        }
        for (int i = 0; i < 5; i++) {
            out[i] = (u8)e->segment[i];
        }
        return true;
    }
    return false;
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

/** The departure path: the song-warp cutscene id, En_Test7, the skip enhancement.
 *  The label reads both facts, as Player's choice does (z_player.c, the
 *  RSBS_SINGLE_EXECUTABLE warp-song branch): En_Test7 is spawned only with a
 *  song-warp cutscene id, and a NULL spawn takes the plain fade too. */
std::string MwdrDeparturePath(PlayState* play) {
    char buf[256];
    EnTest7* t7 = MwdrFindEnTest7(play);
    const bool csId = play != nullptr && play->playerCsIds[PLAYER_CS_ID_SONG_WARP] != CS_ID_NONE;
    const char* label = !csId           ? "Player's plain fade (no song-warp cutscene)"
                        : t7 != nullptr ? "En_Test7 path"
                                        : "Player's plain fade (En_Test7 spawn failed)";
    snprintf(buf, sizeof(buf), "playerCsIds[SONG_WARP] %d (%s), En_Test7 %s (timer %d, action %p), skip cutscene %d",
             play != nullptr ? (int)play->playerCsIds[PLAYER_CS_ID_SONG_WARP] : -2, label,
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
             "world: usable 0x%02X owned 0x%02X (family owned2 0x%08X, apply give record 0x%08X) rule2 0x%08X record "
             "status %d flags 0x%02X dest 0x%04X",
             Combo_MMWarpPlay_GateName(gate), (unsigned)in.paired, (unsigned)in.ootPlayable, (unsigned)in.switchPending,
             (unsigned)in.soaringRestricted, (int)in.ootAge, (unsigned)in.recordStatus, (unsigned)in.destResolvable,
             (unsigned)MM_WarpSongMM_UsableMask(), (unsigned)Combo_MMWarpSongsOwned(),
             (unsigned)Combo_SharedFamilies2Owned(), (unsigned)Combo_SharedFamilies2GivenAtLastApply(GAME_MM),
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

    if (IntegrationTest_MMSharedSunSaria()) {
        const MwdrSunWatch& s = sMwdr.sun;
        const u32 avail = MM_Message_BuildOcarinaAvailableSongs();
        char sunBuf[1536];
        snprintf(sunBuf, sizeof(sunBuf),
                 "; sun/saria: song %s, sunsSongState %d R_TIME_SPEED %d (prePlay %d, sceneTimeSpeed now %u at PREPARE "
                 "%u) time 0x%04X day %d isNight %d, blockSunsSong %d, OceffSpot %s (last seen %d), match %d first "
                 "SPEED_TIME %d (msgMode %d textId 0x%04X ocarinaMode %d, time 0x%04X), SPEED_TIME frames %d (%d with "
                 "a message up), START seen %d, msg NONE %d, custom text frames %d, text close %d; available 0x%08X; "
                 "quest Sun %d Saria %d; rows Sun %d Saria %d; fanfare[SARIAS] 0x%04X; EnableSunsSong %d "
                 "FasterSongPlayback %d; frozen shared Sun %d Saria %d; saria free %d played text 0x%04X; q3 armed %d "
                 "frames %d re-arm %d OceffSpot %d",
                 MwdrSongName(sMwdr.song), (int)gSaveContext.sunsSongState, (int)R_TIME_SPEED, (int)s.prePlaySpeed,
                 (unsigned)play->envCtx.sceneTimeSpeed, (unsigned)s.sceneTimeSpeed, (unsigned)CURRENT_TIME,
                 (int)gSaveContext.save.day, (int)gSaveContext.save.isNight, (int)m->blockSunsSong,
                 MwdrOceffSpotPresent(play) ? "present" : "absent", s.oceffLastSeen, s.matchFrame, s.firstSpeedFrame,
                 s.firstSpeedMsgMode, (unsigned)(s.firstSpeedTextId & 0xFFFF), s.firstSpeedOcarinaMode,
                 (unsigned)s.timeAtFirstSpeed, s.speedFrames, s.speedFramesMsgUp, s.startFrames, s.msgNoneFrame,
                 s.customTextFrames, s.textCloseFrame, (unsigned)avail, CHECK_QUEST_ITEM(QUEST_SONG_SUN) ? 1 : 0,
                 CHECK_QUEST_ITEM(QUEST_SONG_SARIA) ? 1 : 0, (int)RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONG_SUN],
                 (int)RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONG_SARIA],
                 (unsigned)(u16)sOcarinaSongFanfares[OCARINA_SONG_SARIAS],
                 (int)CVarGetInteger("gEnhancements.Songs.EnableSunsSong", 0),
                 (int)CVarGetInteger("gEnhancements.Songs.FasterSongPlayback", 0),
                 Combo_SharedSongFrozenShared(RSBS_XITEM2_SHARED_SONG_SUN) ? 1 : 0,
                 Combo_SharedSongFrozenShared(RSBS_XITEM2_SHARED_SONG_SARIAS) ? 1 : 0, sMwdr.sariaFreeFrames,
                 (unsigned)(sMwdr.sariaPlayedTextId & 0xFFFF), s.q3Armed ? 1 : 0, s.q3Frames, s.q3RearmFrames,
                 s.q3OceffSightings);
        out += sunBuf;
    }
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
    std::string msg = MwdrRowName();
    msg += ", step '";
    msg += MwdrStepName(at);
    msg += "': ";
    if (IntegrationTest_MMSharedSunSaria()) {
        msg += "(playing ";
        msg += MwdrSongName(sMwdr.song);
        msg += ") ";
    }
    msg += why;
    msg += " [";
    msg += state;
    msg += "]";
    IntegrationTest_GameplayFail(msg.c_str());
}

/** Moves to @p step, logging the step it leaves as @p outcome ("done", or
 *  "abandoned" when the take-out falls back to the payout wait). */
void MwdrNext(MwdrStep step, const char* outcome = "done") {
    if (IntegrationTest_MMSharedSunSaria()) {
        fprintf(stderr, "[PFC] mm warp song: step '%s' (%s) %s after %d frame(s) (drive frame %d)\n",
                MwdrStepName(sMwdr.step), MwdrSongName(sMwdr.song), outcome, sMwdr.stepFrames, sMwdr.driveFrames);
    } else {
        fprintf(stderr, "[PFC] mm warp song: step '%s' %s after %d frame(s) (drive frame %d)\n",
                MwdrStepName(sMwdr.step), outcome, sMwdr.stepFrames, sMwdr.driveFrames);
    }
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

/**
 * int-mm-shared-sun-saria's PREPARE (#1587): everything the two plays are judged
 * against, read before a note is played, each failure naming its fact.
 *  - The family carried both songs: MM's ARRIVAL APPLY gave them (its give record,
 *    which no starting-check payout writes), the family word holds them, and MM's
 *    quest bits are set (MM_SharedSunSariaGive sets only the quest bit).
 *  - Foreign.cpp step 2d reached the file: MM's own Shuffle Sun's Song and Shuffle
 *    Saria's Song rows are on; SariasSongHint registered with its row on (it puts
 *    NA_BGM_SARIAS_SONG in the fanfare table only then, SariasSongHint.cpp); the
 *    gate its hook bodies read says shared.
 *  - The quest bits make both playable (MM_Message_BuildOcarinaAvailableSongs).
 *    EnableSunsSong is logged: under this row its hook returns early (the rando
 *    row is on), so it cannot be what makes the Sun's Song playable.
 *  - The Sun's effect can run and end by day: R_TIME_SPEED is not already 400
 *    (OceffSpot_End refuses it), the scene has a time speed (else SPECIAL), and the
 *    clock is in 06:00-16:00 (a night play would run to 06:00 and the day change).
 *  - FasterSongPlayback forced to 0 in memory: z_parameter.c reads it on the
 *    START pass (put back at the verdict by IntegrationTest_SoaringCleanup).
 * Then the cursor goes to the Sun's Song and the question 3 baseline is printed
 * (a baseline, not evidence: MM's Play init resets it).
 */
bool MwdrPrepareSunSaria(PlayState* play) {
    struct SongFact {
        uint32_t bit;
        int quest;
        int row;
        int index;
        const char* name;
    };
    static const SongFact kSongs[] = {
        { RSBS_XITEM2_SHARED_SONG_SUN, (int)QUEST_SONG_SUN, (int)RO_SHUFFLE_SONG_SUN, (int)OCARINA_SONG_SUNS,
          "Sun's Song" },
        { RSBS_XITEM2_SHARED_SONG_SARIAS, (int)QUEST_SONG_SARIA, (int)RO_SHUFFLE_SONG_SARIA, (int)OCARINA_SONG_SARIAS,
          "Saria's Song" },
    };
    const uint32_t given = Combo_SharedFamilies2GivenAtLastApply(GAME_MM);
    const uint32_t owned2 = Combo_SharedFamilies2Owned();
    const u32 avail = MM_Message_BuildOcarinaAvailableSongs();
    char why[512];
    for (const SongFact& f : kSongs) {
        const bool quest = CHECK_QUEST_ITEM(f.quest) != 0;
        const u32 row = RANDO_SAVE_OPTIONS[f.row];
        if ((given & f.bit) == 0u) {
            snprintf(
                why, sizeof(why),
                "the family did not give Majora's Mask the %s at arrival (OoT's harvest at suspend or MM's apply): "
                "apply give record 0x%08X, family owned2 0x%08X, quest bit %d",
                f.name, (unsigned)given, (unsigned)owned2, quest ? 1 : 0);
            MwdrFail(play, why);
            return false;
        }
        if ((owned2 & f.bit) == 0u || !quest) {
            snprintf(why, sizeof(why),
                     "the apply gave the %s but the family word or MM's quest bit does not hold it: family owned2 "
                     "0x%08X, quest bit %d (MM_SharedSunSariaGive)",
                     f.name, (unsigned)owned2, quest ? 1 : 0);
            MwdrFail(play, why);
            return false;
        }
        if (row != (u32)RO_GENERIC_YES) {
            snprintf(why, sizeof(why),
                     "MM's own Shuffle %s row is %u, not on: Foreign.cpp step 2d (a shared song is MM's too) did not "
                     "reach the file",
                     f.name, (unsigned)row);
            MwdrFail(play, why);
            return false;
        }
        if ((avail & (1u << f.index)) == 0u) {
            snprintf(why, sizeof(why),
                     "MM's playable-song mask 0x%08X lacks the %s (bit %d) with its quest bit set "
                     "(MM_Message_BuildOcarinaAvailableSongs)",
                     (unsigned)avail, f.name, f.index);
            MwdrFail(play, why);
            return false;
        }
        if (!Combo_SharedSongFrozenShared(f.bit)) {
            snprintf(why, sizeof(why),
                     "Combo_SharedSongFrozenShared says the %s is not shared in the frozen world (rule2 0x%08X)",
                     f.name, (unsigned)Combo_ComboCrossItemFlags2());
            MwdrFail(play, why);
            return false;
        }
    }
    if (sOcarinaSongFanfares[OCARINA_SONG_SARIAS] != NA_BGM_SARIAS_SONG) {
        snprintf(why, sizeof(why),
                 "the fanfare of Saria's Song is 0x%04X, not NA_BGM_SARIAS_SONG: SariasSongHint did not register with "
                 "MM's Saria's Song row on (SariasSongHint.cpp, shouldRegister)",
                 (unsigned)(u16)sOcarinaSongFanfares[OCARINA_SONG_SARIAS]);
        MwdrFail(play, why);
        return false;
    }
    MwdrSunWatch& s = sMwdr.sun;
    s.prePlaySpeed = (s16)R_TIME_SPEED;
    s.sceneTimeSpeed = play->envCtx.sceneTimeSpeed;
    if (s.prePlaySpeed == kMwdrTimeSpeedSun) {
        MwdrFail(play, "R_TIME_SPEED is already 400 before the Sun's Song: OceffSpot_End would refuse the effect");
        return false;
    }
    if (s.sceneTimeSpeed == 0) {
        MwdrFail(play,
                 "scene has no time speed: Sun would go SPECIAL (z_parameter.c), so its speed-up cannot be shown");
        return false;
    }
    const u16 now = (u16)CURRENT_TIME;
    if ((s32)now < CLOCK_TIME(6, 0) || (s32)now >= CLOCK_TIME(16, 0)) {
        snprintf(
            why, sizeof(why),
            "the clock is 0x%04X (day %d, isNight %d), outside 06:00-16:00: a night play runs to 06:00 and the day "
            "change, and a late one leaves no room before 18:00 (a 12:00 fixture, as the shuffle-ocarina drive "
            "sets its clock, is the fix)",
            (unsigned)now, (int)gSaveContext.save.day, (int)gSaveContext.save.isNight);
        MwdrFail(play, why);
        return false;
    }
    // The CVar alone is not enough: FasterSongPlayback.cpp's hooks are armed from it
    // by its ShipInit registrar (COND_ID_HOOK on ACTOR_PLAYER), which ran at MM's
    // InitAll. Re-running the registrar on the key unregisters them with the key 0.
    const int fasterBefore = (int)CVarGetInteger("gEnhancements.Songs.FasterSongPlayback", 0);
    IntegrationTest_SoaringSetCVarInt("gEnhancements.Songs.FasterSongPlayback", 0);
    ShipInit::Init("gEnhancements.Songs.FasterSongPlayback");
    fprintf(stderr,
            "[PFC] shared sun/saria: FasterSongPlayback was %d, set to 0 in memory and its ShipInit registrar re-run "
            "(its OnActorUpdate hook on Player is now unregistered)\n",
            fasterBefore);
    fprintf(stderr,
            "[PFC] shared sun/saria: MM holds both through the family (apply give record 0x%08X, owned2 0x%08X), "
            "quest bits Sun %d Saria %d, rows Sun %u Saria %u, fanfare[SARIAS] 0x%04X (NA_BGM_SARIAS_SONG), playable "
            "mask 0x%08X, EnableSunsSong %d (its hook returns early under the row); R_TIME_SPEED %d, sceneTimeSpeed "
            "%u, clock 0x%04X day %d isNight %d; the drive gives neither\n",
            (unsigned)given, (unsigned)owned2, CHECK_QUEST_ITEM(QUEST_SONG_SUN) ? 1 : 0,
            CHECK_QUEST_ITEM(QUEST_SONG_SARIA) ? 1 : 0, (unsigned)RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONG_SUN],
            (unsigned)RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONG_SARIA],
            (unsigned)(u16)sOcarinaSongFanfares[OCARINA_SONG_SARIAS], (unsigned)avail,
            (int)CVarGetInteger("gEnhancements.Songs.EnableSunsSong", 0), (int)s.prePlaySpeed,
            (unsigned)s.sceneTimeSpeed, (unsigned)now, (int)gSaveContext.save.day, (int)gSaveContext.save.isNight);
    fprintf(stderr,
            "[PFC] shared sun/saria q3 pre-play baseline, not evidence (MM Play init resets it, z_parameter.c:10083): "
            "sunsSongState %d, R_TIME_SPEED %d, clock 0x%04X, day %d, isNight %d, sLastPlayedSong 0x%02X, "
            "lastPlayedSong 0x%02X, blockSunsSong %d\n",
            (int)gSaveContext.sunsSongState, (int)R_TIME_SPEED, (unsigned)now, (int)gSaveContext.save.day,
            (int)gSaveContext.save.isNight, (unsigned)(u16)MM_sLastPlayedSong, (unsigned)play->msgCtx.lastPlayedSong,
            (int)play->msgCtx.blockSunsSong);
    fflush(stderr);
    sMwdr.song = MwdrSong::Sun;
    return true;
}

/**
 * The control on int-mm-warp-song-minuet and int-mm-warp-song-minuet-shared
 * (#1587): frame-free reads, no input, that main's rules-off state did not move.
 *  C1 neither shared rule is frozen in the created world;
 *  C2 MM's own Shuffle Sun's Song and Shuffle Saria's Song rows are off (the
 *     paired profile resets the PARTIAL rows to their default, Foreign.cpp 1a);
 *  C3 SariasSongHint is unregistered (the fanfare is vanilla NA_BGM_MAJORAS_LAIR);
 *  C4 neither song is playable on the rules-off rows: no quest bits, and the
 *     EnableSunsSong VB path (EnableSunsSong.cpp) is off. Read only, never set:
 *     if that shipped default ever turns on, this goes red by name.
 */
bool MwdrRulesOffControl(PlayState* play) {
    const uint32_t rule2 = Combo_ComboCrossItemFlags2();
    const uint32_t bits = RSBS_XITEM2_SHARED_SONG_SUN | RSBS_XITEM2_SHARED_SONG_SARIAS;
    const u32 sunRow = RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONG_SUN];
    const u32 sariaRow = RANDO_SAVE_OPTIONS[RO_SHUFFLE_SONG_SARIA];
    const u16 fanfare = (u16)sOcarinaSongFanfares[OCARINA_SONG_SARIAS];
    const u32 avail = MM_Message_BuildOcarinaAvailableSongs();
    const u32 songBits = (1u << OCARINA_SONG_SARIAS) | (1u << OCARINA_SONG_SUNS);
    const int enableSuns = (int)CVarGetInteger("gEnhancements.Songs.EnableSunsSong", 0);
    const char* why = nullptr;
    if ((rule2 & bits) != 0u) {
        why = "C1: the created world froze a shared Sun's Song or Saria's Song rule on a rules-off row";
    } else if (sunRow != 0u || sariaRow != 0u) {
        why = "C2: MM's own Shuffle Sun's Song or Shuffle Saria's Song row is on with the shared rules off";
    } else if (fanfare != (u16)NA_BGM_MAJORAS_LAIR) {
        why = "C3: SariasSongHint registered (the Saria's Song fanfare fix) with MM's Saria's Song row off";
    } else if ((avail & songBits) != 0u || enableSuns != 0) {
        why = "C4: a song is playable on a rules-off row (want neither playable: no Sun's or Saria's Song quest bit, "
              "and the EnableSunsSong VB path, EnableSunsSong.cpp:13-23, off)";
    }
    char line[640];
    snprintf(line, sizeof(line),
             "rule2 0x%08X, rows Sun %u Saria %u, fanfare[SARIAS] 0x%04X, playable mask 0x%08X (Saria bit %d, Sun bit "
             "%d), EnableSunsSong %d",
             (unsigned)rule2, (unsigned)sunRow, (unsigned)sariaRow, (unsigned)fanfare, (unsigned)avail,
             (avail & (1u << OCARINA_SONG_SARIAS)) != 0u ? 1 : 0, (avail & (1u << OCARINA_SONG_SUNS)) != 0u ? 1 : 0,
             enableSuns);
    if (why != nullptr) {
        std::string msg = why;
        msg += " [";
        msg += line;
        msg += "]";
        MwdrFail(play, msg.c_str());
        return false;
    }
    fprintf(stderr, "[PFC] mm warp song: the rules-off control C1-C4 holds (%s)\n", line);
    fflush(stderr);
    return true;
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
    if (IntegrationTest_MMWarpSongMinuetShared()) {
        // #1169 XW14: nothing is given here. OoT learnt its own Minuet before the
        // Happy Mask Shop door; MM must hold it through the shared family. The
        // apply's give record says MM's ARRIVAL APPLY gave it: a starting-check
        // payout (after the apply, through MM_WarpSongMM_Give) never writes it, so
        // this cannot pass on a pinned seed that puts the pair's MM half on a
        // starting check while the apply is broken.
        const uint32_t given = Combo_SharedFamilies2GivenAtLastApply(GAME_MM);
        const uint32_t owned2 = Combo_SharedFamilies2Owned();
        const uint8_t owned = Combo_MMWarpSongsOwned();
        if ((given & RSBS_XITEM2_SHARED_SONG_MINUET) == 0u) {
            char why[256];
            snprintf(why, sizeof(why),
                     "the family did not give Majora's Mask its Minuet at arrival (OoT's harvest at suspend or MM's "
                     "apply): apply give record 0x%08X, family owned2 0x%08X, ownership byte 0x%02X",
                     (unsigned)given, (unsigned)owned2, (unsigned)owned);
            MwdrFail(play, why);
            return;
        }
        if ((owned2 & RSBS_XITEM2_SHARED_SONG_MINUET) == 0u || (owned & RSBS_MM_WARP_SONG_OWNED_MINUET) == 0u) {
            char why[256];
            snprintf(why, sizeof(why),
                     "the apply gave the Minuet but the family word or MM's ownership byte does not hold it: family "
                     "owned2 0x%08X, ownership byte 0x%02X",
                     (unsigned)owned2, (unsigned)owned);
            MwdrFail(play, why);
            return;
        }
        fprintf(stderr,
                "[PFC] mm warp song (shared): MM holds the Minuet through the family (apply give record 0x%08X, "
                "owned2 0x%08X, ownership byte 0x%02X); the drive gives nothing\n",
                (unsigned)given, (unsigned)owned2, (unsigned)owned);
        fflush(stderr);
    } else {
        MM_WarpSongMM_Give(RI_SONG_MINUET);
    }
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
    // #1587: the shared Sun's and Saria's songs' facts on their own row; on the
    // two Minuet rows, frame-free reads that the rules-off state did not move.
    if (IntegrationTest_MMSharedSunSaria() ? !MwdrPrepareSunSaria(play) : !MwdrRulesOffControl(play)) {
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
        fflush(stderr);
        MwdrNext(MwdrStep::Payouts, "abandoned");
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
    const int count = MwdrNoteCount(sMwdr.song);
    if (sMwdr.noteFrame == 0) {
        if (play->msgCtx.msgMode != MSGMODE_OCARINA_PLAYING) {
            char why[128];
            snprintf(why, sizeof(why), "the ocarina left free play before note %d of %d", sMwdr.note + 1, count);
            MwdrFail(play, why);
            return;
        }
        MwdrHold(MwdrNotePad(sMwdr.song, sMwdr.note), RSBS_MWD_NOTE_HOLD);
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

bool MwdrRefused(PlayState* play) {
    return play->msgCtx.ocarinaMode == OCARINA_MODE_PROCESS_RESTRICTED_SONG ||
           play->msgCtx.currentTextId == RSBS_MMWP_TEXT_RESTRICTED;
}

void MwdrStepMatch(PlayState* play) {
    const u16 want = MwdrWant(sMwdr.song);
    if (play->msgCtx.lastPlayedSong == want && play->msgCtx.msgMode != MSGMODE_OCARINA_PLAYING) {
        switch (sMwdr.song) {
            case MwdrSong::Minuet:
                MwdrNext(MwdrStep::Played);
                break;
            case MwdrSong::Sun:
                // #1587: the Sun watch starts here, not at the text's close: the
                // OceffSpot is spawned as playback begins and the effect can start
                // while the playback staff or the text is still up.
                sMwdr.sun.armed = true;
                sMwdr.sun.matchFrame = sMwdr.driveFrames;
                fprintf(stderr,
                        "[PFC] shared sun/saria: matched the Sun's Song (lastPlayedSong %u) at drive frame %d; "
                        "the Sun watch is armed\n",
                        (unsigned)play->msgCtx.lastPlayedSong, sMwdr.driveFrames);
                fflush(stderr);
                MwdrNext(MwdrStep::SunText);
                break;
            case MwdrSong::Saria:
                fprintf(stderr, "[PFC] shared sun/saria: matched Saria's Song (lastPlayedSong %u) at drive frame %d\n",
                        (unsigned)play->msgCtx.lastPlayedSong, sMwdr.driveFrames);
                fflush(stderr);
                sMwdr.sariaFreeFrames = 0;
                sMwdr.sariaPlayedTextId = -1;
                MwdrNext(MwdrStep::SariaEnd);
                break;
        }
        return;
    }
    if (sMwdr.stepFrames >= kMwdrMatchBudget) {
        if (sMwdr.song == MwdrSong::Minuet) {
            MwdrFail(play, "the six notes were played but MM's matcher never answered the Minuet (0x80)");
        } else {
            char why[160];
            snprintf(why, sizeof(why), "the six notes of %s were played but MM's matcher never answered it (want %u)",
                     MwdrSongName(sMwdr.song), (unsigned)want);
            MwdrFail(play, why);
        }
    }
}

/**
 * #1587, the Sun watch: every drive tick from the Sun's MATCH through SUN_EFFECT,
 * before the step. It records the last frame an OceffSpot was seen (not asserted:
 * a killed actor can stay listed for one more update), the first SPEED_TIME frame
 * with its msgMode, text and ocarinaMode (z_parameter.c writes OCARINA_MODE_END on
 * the START pass, the same Play_Update as the SPEED_TIME entry), the SPEED_TIME
 * frames and those with a message up, and the first frame the message is gone.
 * It fails at once on SUNSSONG_SPECIAL (persistent), on a SPEED_TIME frame with
 * R_TIME_SPEED other than 400, and on INACTIVE after SPEED_TIME before 18:00.
 * START is not observable from this tick (set by OceffSpot_End inside
 * Actor_UpdateAll and consumed by Interface_Update later in the same
 * Play_Update); it is counted only in case that ever changes.
 */
void MwdrSunSample(PlayState* play) {
    MwdrSunWatch& s = sMwdr.sun;
    const MessageContext* m = &play->msgCtx;
    const s16 state = gSaveContext.sunsSongState;
    const u16 now = (u16)CURRENT_TIME;
    if (MwdrOceffSpotPresent(play)) {
        s.oceffLastSeen = sMwdr.driveFrames;
    }
    if (s.msgNoneFrame < 0 && m->msgMode == MSGMODE_NONE) {
        s.msgNoneFrame = sMwdr.driveFrames;
    }
    if (state == SUNSSONG_START) {
        s.startFrames++;
    }
    if (state == SUNSSONG_SPECIAL) {
        MwdrFail(play, "sunsSongState is SUNSSONG_SPECIAL: the scene's time speed is 0 here (z_parameter.c), so the "
                       "Sun's Song cannot speed the clock");
        return;
    }
    if (state == SUNSSONG_SPEED_TIME) {
        if (s.firstSpeedFrame < 0) {
            s.firstSpeedFrame = sMwdr.driveFrames;
            s.firstSpeedMsgMode = (int)m->msgMode;
            s.firstSpeedTextId = (int)m->currentTextId;
            s.firstSpeedOcarinaMode = (int)m->ocarinaMode;
            s.timeAtFirstSpeed = now;
            fprintf(stderr,
                    "[PFC] shared sun/saria: first SPEED_TIME at drive frame %d (%d after the match; OceffSpot last "
                    "seen %d) with msgMode %d, textId 0x%04X, ocarinaMode %d (OCARINA_MODE_END expected, from "
                    "TEXT_CLOSING's free-play write if the text closed first, z_message.c, or from the START pass, "
                    "z_parameter.c), R_TIME_SPEED %d, clock 0x%04X; START not observable from the tick (set and "
                    "consumed in one Play_Update)\n",
                    s.firstSpeedFrame, s.firstSpeedFrame - s.matchFrame, s.oceffLastSeen, s.firstSpeedMsgMode,
                    (unsigned)(s.firstSpeedTextId & 0xFFFF), s.firstSpeedOcarinaMode, (int)R_TIME_SPEED, (unsigned)now);
            fflush(stderr);
        }
        s.speedFrames++;
        if (m->msgMode != MSGMODE_NONE) {
            s.speedFramesMsgUp++;
        }
        if ((int)R_TIME_SPEED != kMwdrTimeSpeedSun) {
            char why[160];
            snprintf(why, sizeof(why), "a SPEED_TIME frame with R_TIME_SPEED %d, not 400 (z_parameter.c's speed-up)",
                     (int)R_TIME_SPEED);
            MwdrFail(play, why);
        }
        return;
    }
    if (state == SUNSSONG_INACTIVE && s.firstSpeedFrame >= 0 && (s32)now <= CLOCK_TIME(18, 0)) {
        char why[256];
        snprintf(why, sizeof(why),
                 "sunsSongState went back to INACTIVE after SPEED_TIME with the clock at 0x%04X, not past 18:00: "
                 "En_Rd/ClockShuffle/z_map_exp reset?",
                 (unsigned)now);
        MwdrFail(play, why);
    }
}

/** #1587, question 3, recorded only: from SUN_EFFECT's end to DEPART (through the
 *  Saria and Minuet plays, with no Sun's Song played by the drive), count the
 *  frames the Sun's effect is live again (sunsSongState not INACTIVE or
 *  R_TIME_SPEED 400) and the OceffSpot sightings. No crossing happens inside it. */
void MwdrQ3Sample(PlayState* play) {
    MwdrSunWatch& s = sMwdr.sun;
    s.q3Frames++;
    if (gSaveContext.sunsSongState != SUNSSONG_INACTIVE || (int)R_TIME_SPEED == kMwdrTimeSpeedSun) {
        s.q3RearmFrames++;
    }
    if (MwdrOceffSpotPresent(play)) {
        s.q3OceffSightings++;
    }
    s.q3BlockSunsSong = (int)play->msgCtx.blockSunsSong;
}

/** The custom text's checks, on its first frame in MSGMODE_16..18: 0x1B78's
 *  header (read live from MM's table, both vanilla headers printed) and stage T's
 *  body bytes in the font buffer; the box and position the decoder took and a
 *  log-only look for "Sun's Song" in the decoded text. False on a failed check. */
bool MwdrSunTextCheck(PlayState* play) {
    MessageContext* m = &play->msgCtx;
    const unsigned char* buf = m->font.msgBuf.schar;
    u8 box[5] = { 0 };
    u8 vanilla[5] = { 0 };
    const bool haveBox = MwdrVanillaHeader(play, (u16)RSBS_MMWP_TEXT_PLAYED_BOX, box);
    const bool haveVanilla = MwdrVanillaHeader(play, kMwdrSunTextVanilla, vanilla);
    bool sameHeaders = haveBox && haveVanilla;
    for (int i = 0; i < 5 && sameHeaders; i++) {
        sameHeaders = box[i] == vanilla[i];
    }
    fprintf(stderr,
            "[PFC] shared sun/saria: the played text 0x%04X (msgLength %d): font header %02X %02X %02X %02X %02X; "
            "0x1B78's %s %02X %02X %02X %02X %02X; 0x1B7D's %s %02X %02X %02X %02X %02X%s; decoded box %u position "
            "%u\n",
            (unsigned)m->currentTextId, (int)m->msgLength, buf[0], buf[1], buf[2], buf[3], buf[4],
            haveBox ? "header" : "(absent)", box[0], box[1], box[2], box[3], box[4],
            haveVanilla ? "header" : "(absent)", vanilla[0], vanilla[1], vanilla[2], vanilla[3], vanilla[4],
            sameHeaders ? " (the two vanilla headers are equal: the header check is vacuous)" : "",
            (unsigned)m->textBoxType, (unsigned)m->textBoxPos);
    if (!haveBox) {
        MwdrFail(play, "0x1B78 is not in MM's message table, so the played text's box cannot be checked");
        return false;
    }
    for (int i = 0; i < 5; i++) {
        if (buf[i] != box[i]) {
            MwdrFail(play, "the Sun's played text is not in 0x1B78's box (the font header differs from 0x1B78's)");
            return false;
        }
    }
    if (std::memcmp(buf + kMwdrMsgHeaderLen, kMwdrSunTextBody, kMwdrSunTextBodyLen) != 0) {
        std::string got;
        char hex[4];
        for (size_t i = 0; i < kMwdrSunTextBodyLen; i++) {
            snprintf(hex, sizeof(hex), "%02X", (unsigned)buf[kMwdrMsgHeaderLen + i]);
            got += hex;
        }
        std::string why = "the Sun's played text is not stage T's 'You played the ' yellow 'Sun's Song' white '!' "
                          "(font bytes after the header: ";
        why += got;
        why += ")";
        MwdrFail(play, why.c_str());
        return false;
    }
    // Log only: the decoder's byte form is unverified.
    const size_t decodedCap = sizeof(m->decodedBuffer.schar);
    const size_t decodedLen = (size_t)m->decodedTextLen < decodedCap ? (size_t)m->decodedTextLen : decodedCap;
    const std::string decoded(m->decodedBuffer.schar, decodedLen);
    fprintf(stderr,
            "[PFC] shared sun/saria: the Sun's played text is stage T's (0x1B78's box, body bytes equal); decoded text "
            "%s \"Sun's Song\" (log only, %u bytes)\n",
            decoded.find("Sun's Song") != std::string::npos ? "contains" : "does not contain", (unsigned)decodedLen);
    fflush(stderr);
    return true;
}

/** #1587, SUN_TEXT: from the Sun's MATCH to the played text's close. */
void MwdrStepSunText(PlayState* play) {
    MessageContext* m = &play->msgCtx;
    MwdrSunWatch& s = sMwdr.sun;
    if (MwdrRefused(play)) {
        MwdrFail(play, "the Sun's Song was refused (0x1B95 / the restricted-song mode)");
        return;
    }
    if (m->currentTextId == kMwdrSunTextVanilla) {
        MwdrFail(play, "MM's own 0x1B7D opened for the Sun's Song: stage T's branch in "
                       "MSGMODE_DISPLAY_SONG_PLAYED_TEXT_BEGIN did not run");
        return;
    }
    const bool textMode = m->msgMode == MSGMODE_16 || m->msgMode == MSGMODE_17 || m->msgMode == MSGMODE_18;
    if (m->currentTextId == CUSTOM_MESSAGE_ID && textMode) {
        s.customTextFrames++;
        if (!s.textChecked) {
            s.textChecked = true;
            if (!MwdrSunTextCheck(play)) {
                return;
            }
        }
    }
    if (m->msgMode == MSGMODE_NONE) {
        if (s.customTextFrames == 0) {
            MwdrFail(play, "the message closed before the Sun's played text opened (did the START pass's "
                           "ocarinaMode END, z_parameter.c, close the staff before TEXT_BEGIN?)");
            return;
        }
        s.textCloseFrame = sMwdr.driveFrames;
        // Recorded only: nothing in the message flow waits for the OceffSpot (about
        // 100 frames from its spawn to START, z_oceff_spot.c), so SPEED_TIME can
        // begin on either side of the close. SUN_EFFECT requires it, by name.
        if (s.firstSpeedFrame < 0) {
            fprintf(stderr,
                    "[PFC] shared sun/saria: the Sun's played text was up %d frame(s) and closed at drive frame %d "
                    "(%d after the match); SPEED_TIME not seen yet (OceffSpot last seen %d): it is required at "
                    "SUN_EFFECT\n",
                    s.customTextFrames, s.textCloseFrame, s.textCloseFrame - s.matchFrame, s.oceffLastSeen);
        } else {
            fprintf(stderr,
                    "[PFC] shared sun/saria: the Sun's played text was up %d frame(s) and closed at drive frame %d "
                    "(%d after the match); first SPEED_TIME at %d, before the close (%d frame(s) with a message up)\n",
                    s.customTextFrames, s.textCloseFrame, s.textCloseFrame - s.matchFrame, s.firstSpeedFrame,
                    s.speedFramesMsgUp);
        }
        fflush(stderr);
        MwdrNext(MwdrStep::SunEffect);
        return;
    }
    if (sMwdr.stepFrames >= kMwdrSunTextBudget) {
        MwdrFail(play, s.customTextFrames == 0 ? "the Sun's played text never opened (see msgMode and ocarinaMode; the "
                                                 "START pass's early ocarinaMode END is the first suspect)"
                                               : "the Sun's played text never closed");
    }
}

/** #1587, SUN_EFFECT: from the text's close until the speed-up has run its course.
 *  The clock checks are made with msgMode NONE only (the clock cannot move while a
 *  message is up, z_kankyo.c). */
void MwdrStepSunEffect(PlayState* play) {
    MwdrSunWatch& s = sMwdr.sun;
    const u16 now = (u16)CURRENT_TIME;
    const bool done = play->msgCtx.msgMode == MSGMODE_NONE && s.firstSpeedFrame >= 0 &&
                      gSaveContext.sunsSongState == SUNSSONG_INACTIVE && (s32)now > CLOCK_TIME(18, 0) &&
                      (s16)R_TIME_SPEED == s.prePlaySpeed;
    if (done) {
        fprintf(stderr,
                "[PFC] shared sun/saria: the Sun's effect ran its course: SPEED_TIME %d frame(s) (%d with a message "
                "up, R_TIME_SPEED 400 on each), first at drive frame %d with msgMode %d and ocarinaMode %d, from clock "
                "0x%04X to 0x%04X (past 18:00), R_TIME_SPEED back to %d; OceffSpot last seen at %d; START seen %d\n",
                s.speedFrames, s.speedFramesMsgUp, s.firstSpeedFrame, s.firstSpeedMsgMode, s.firstSpeedOcarinaMode,
                (unsigned)s.timeAtFirstSpeed, (unsigned)now, (int)s.prePlaySpeed, s.oceffLastSeen, s.startFrames);
        fflush(stderr);
        s.armed = false;
        s.q3Armed = true;
        sMwdr.song = MwdrSong::Saria;
        MwdrNext(MwdrStep::TakeOut);
        return;
    }
    if (sMwdr.stepFrames >= kMwdrSunEffectBudget) {
        char why[256];
        snprintf(why, sizeof(why),
                 "the Sun's effect did not end past 18:00 with the time speed put back within %d frames of the text's "
                 "close (sunsSongState %d, clock 0x%04X, R_TIME_SPEED %d, want %d)",
                 kMwdrSunEffectBudget, (int)gSaveContext.sunsSongState, (unsigned)now, (int)R_TIME_SPEED,
                 (int)s.prePlaySpeed);
        MwdrFail(play, why);
    }
}

/**
 * #1587, SARIA_END: from Saria's MATCH, nothing pressed. The claim is "the shared
 * Saria's Song redirect does not open 0x1B95 or enter restricted mode
 * (SariasSongHint.cpp's VB_MSG_CAPTURE_MSGMODE_TEXT_CLOSING_OCARINA_ACTION body),
 * and the song stays owned"; the TEXT_DONE and OnOpenText early returns are the
 * ROM-free MMSharedSariaHooks row's (each is reached only after 0x1B95 opens).
 */
void MwdrStepSariaEnd(PlayState* play, Player* player) {
    MessageContext* m = &play->msgCtx;
    if (m->currentTextId == RSBS_MMWP_TEXT_RESTRICTED) {
        MwdrFail(play, "0x1B95 opened after Saria's Song: SariasSongHint's redirect ran under the shared rule");
        return;
    }
    if (m->ocarinaMode == OCARINA_MODE_PROCESS_RESTRICTED_SONG) {
        MwdrFail(play, "ocarinaMode is PROCESS_RESTRICTED_SONG after Saria's Song: SariasSongHint's redirect ran under "
                       "the shared rule");
        return;
    }
    if (m->currentTextId == CUSTOM_MESSAGE_ID) {
        MwdrFail(play, "a custom text opened after Saria's Song (2Ship's one-use prompt or hint)");
        return;
    }
    if (sMwdr.sariaPlayedTextId < 0 &&
        (m->msgMode == MSGMODE_16 || m->msgMode == MSGMODE_17 || m->msgMode == MSGMODE_18)) {
        sMwdr.sariaPlayedTextId = (int)m->currentTextId;
    }
    sMwdr.sariaFreeFrames = MwdrLinkFree(play, player) ? sMwdr.sariaFreeFrames + 1 : 0;
    if (sMwdr.sariaFreeFrames >= kMwdrSariaFreeNeeded) {
        const uint32_t owned2 = Combo_SharedFamilies2Owned();
        const bool quest = CHECK_QUEST_ITEM(QUEST_SONG_SARIA) != 0;
        if (!quest || (owned2 & RSBS_XITEM2_SHARED_SONG_SARIAS) == 0u) {
            char why[256];
            snprintf(why, sizeof(why),
                     "Saria's Song is gone after it was played (quest bit %d, family owned2 0x%08X): a shared Saria's "
                     "Song is permanent (Rando::RemoveItem in SariasSongHint.cpp?)",
                     quest ? 1 : 0, (unsigned)owned2);
            MwdrFail(play, why);
            return;
        }
        fprintf(stderr,
                "[PFC] shared sun/saria: Saria's Song ended with no 0x1B95, no restricted mode and no custom text; "
                "still owned (quest bit 1, owned2 0x%08X); played text 0x%04X (0x%04X expected, recorded only), "
                "fanfare 0x%04X, ocarinaMode after the end %d, playedSariasSongState n/a (file-static in "
                "SariasSongHint.cpp)\n",
                (unsigned)owned2, (unsigned)(sMwdr.sariaPlayedTextId & 0xFFFF), (unsigned)kMwdrSariaTextVanilla,
                (unsigned)(u16)sOcarinaSongFanfares[OCARINA_SONG_SARIAS], (int)m->ocarinaMode);
        fflush(stderr);
        sMwdr.song = MwdrSong::Minuet;
        MwdrNext(MwdrStep::TakeOut);
        return;
    }
    if (sMwdr.stepFrames >= kMwdrSariaEndBudget) {
        MwdrFail(play, "Saria's Song never ended with Link free (see msgMode, ocarinaMode and the player flags)");
    }
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
        if (IntegrationTest_MMSharedSunSaria()) {
            const MwdrSunWatch& s = sMwdr.sun;
            fprintf(stderr,
                    "[MWDR] q3 same-scene re-arm window: %d frames, %d OceffSpot sightings, blockSunsSong=%d (no "
                    "crossing inside this window; %d frame(s) watched from the Sun's effect's end to the departure, "
                    "no Sun's Song played by the drive since; recorded only, question 3 stays open)\n",
                    s.q3RearmFrames, s.q3OceffSightings, s.q3BlockSunsSong, s.q3Frames);
        }
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
    // #1587: the background samplers, int-mm-shared-sun-saria only (their flags
    // are never set on the other variants), before the step.
    if (sMwdr.sun.armed) {
        MwdrSunSample(play);
        if (sMwdr.step == MwdrStep::Failed) {
            return;
        }
    }
    if (sMwdr.sun.q3Armed) {
        MwdrQ3Sample(play);
    }
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
        case MwdrStep::SunText:
            MwdrStepSunText(play);
            break;
        case MwdrStep::SunEffect:
            MwdrStepSunEffect(play);
            break;
        case MwdrStep::SariaEnd:
            MwdrStepSariaEnd(play, player);
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
    std::string msg = MwdrRowName();
    msg += ", step 'handed to OoT': MM still running 600 frames after the warp armed: the fade never crossed [";
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
