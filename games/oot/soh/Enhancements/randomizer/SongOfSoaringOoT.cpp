/**
 * @file SongOfSoaringOoT.cpp
 * @brief Majora's Mask's Song of Soaring as an Ocarina of Time item (#1169 lane
 *        XW4; OoTMM songSoaringOot, "Song of Soaring (OoT)").
 *
 * THE ITEM. Under the frozen second-word rule RSBS_XITEM2_SONG_SOARING_OOT
 * (src/common/foreign_items.h) OoT's pool gains one RG_SONG_OF_SOARING_OOT
 * (3drando/item_pool.cpp), with the songs' counts. It is a song
 * (ITEMTYPE_SONG), so OoT's restricted song passes count and place it, but it
 * has no quest bit: owning it is RAND_INF_OBTAINED_SONG_OF_SOARING_OOT, which its
 * get-item sets (randomizer.cpp's randomizerGetToRandInf) and OoT's logic reads
 * (logic.cpp: HasItem, and ApplyItemEffect's song branch, which would otherwise
 * set quest bit 0, the Forest Medallion). Under the shared rule
 * RSBS_XITEM2_SHARED_SONG_SOARING the pool gains nothing
 * (Combo_SongSoaringOoTCopies): the shared song is the family lane XW10 builds.
 *
 * NO NEW ItemID. The song has no inventory slot, pause row or C-button use, so
 * it adds nothing past ITEM_MASK_STONE and reaches none of the vanilla-sized
 * ItemID tables (OoT_gItemSlots, gItemAgeReqs, sItemActions; memory note
 * "custom-itemid-table-bounds"). Its GetItemEntry carries its RG id, as every
 * MOD_RANDOMIZER row does, and the give routes on that.
 *
 * WHAT IT DOES. Lane XW6 (below, "PLAYING IT") adds OoT's note matcher, the
 * Soaring gates and the crossing to an owl; lane XW9 adds the logic's travel
 * facts. Here the logic only knows how to PLAY it (CanUse: OoTMM's OoT
 * can_play_soaring, a child with an ocarina and the C-down, C-up and C-left
 * buttons, which Ocarina Button Shuffle decides), and no region uses that.
 *
 * ONE SPOT SHORT. OoT_SongSpots_RefuseOverflow is OoTMM's "Not enough locations"
 * refusal, asked by the restricted song pass (warp_song_items.h says why).
 *
 * INERT. RSBS_XITEM2_HIDDEN covers the bit until lane XW12: Generate refuses it
 * before the freeze (3drando/playthrough.cpp) and a load refuses a world that has
 * it, so the frozen word is 0, the pool never gains the item, CanUse is false and
 * the song pass never sees a thirteenth song.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>

#include <spdlog/spdlog.h>

#include "soh/Enhancements/randomizer/draw.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h" // CustomMessage
#include "soh/Enhancements/custom-message/CustomMessageTypes.h"   // TEXT_CARPET_SALESMAN_ARMS_DEALER
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/ShipInit.hpp"
#include "SongOfSoaringOoT.h"

#include "context.h"           // src/common: SharedItem, GAME_MM, Context_GetCurrentGame, Switch_GetTargetStatus
#include "foreign_items.h"     // src/common: the second rules word, Combo_GetForeignItemByNameFor
#include "warp_song_items.h"   // src/common: the copies rule and the overflow refusal
#include "warp_song_soaring.h" // src/common: the matcher, the gates, the owls (#1169 XW6)

// At file scope first: z64.h includes <memory> under C++, which must not get C
// linkage. Its include guard makes the nested include below a no-op.
#include <z64.h>
extern "C" {
#include "functions.h"
#include "macros.h"
#include "variables.h"
// ForeignModelHostOoT.cpp: an MM item's model, drawn by OoT as the host.
int OoT_ForeignModel_DrawForeignItem(PlayState* play, uint8_t originGame, uint16_t id);
extern PlayState* OoT_gPlayState;
// code_800EC960.c: the notes the ocarina's playback reads (non-static there).
extern OcarinaNote* OoT_sPlaybackSong;
// src/common/entrance.h, declared rather than included: in this directory
// "entrance.h" is SoH's randomizer header (see oot_windows_gate_test.cpp).
bool ComboWarp_Arm(GameId sourceGame, uint16_t marker, GameId targetGame, uint16_t targetEntrance, uint8_t kind);
}

// THE APPEND-ONLY LOCKS. Each id sits right after the Stone Mask's, so no id a
// world or a save already holds moves, and the RAND_INF words OoT saves are still
// the 129 they were: one more flag would make a 130th word and move every field
// after it in the save.
static_assert(RG_SONG_OF_SOARING_OOT == RG_STONE_MASK + 1,
              "RG_SONG_OF_SOARING_OOT is appended right after the masks, before the logic-only ids (#1169 XW4)");
static_assert(RHT_SONG_OF_SOARING_OOT == RHT_STONE_MASK + 1,
              "RHT_SONG_OF_SOARING_OOT is appended after RHT_STONE_MASK (#1169 XW4)");
static_assert(RAND_INF_OBTAINED_SONG_OF_SOARING_OOT == RAND_INF_OBTAINED_STONE_MASK + 1,
              "RAND_INF_OBTAINED_SONG_OF_SOARING_OOT is appended after the Stone Mask's flag (#1169 XW4)");
static_assert((RAND_INF_MAX + 15) / 16 == 129 && sizeof(ShipSaveContextData::randomizerInf) == 129 * sizeof(u16),
              "OoT's RAND_INF flags must still fit the 129 saved u16 words: a 130th word moves the save layout "
              "(#1169 XW4)");
// The refusal names the mode from RSK_SHUFFLE_SONGS's raw value, which fill.cpp
// passes through; warp_song_items.h spells the two restricted values game-free.
static_assert(RSBS_SONG_SPOTS_SONG_LOCATIONS == (unsigned)RO_SONG_SHUFFLE_SONG_LOCATIONS &&
                  RSBS_SONG_SPOTS_DUNGEON_REWARDS == (unsigned)RO_SONG_SHUFFLE_DUNGEON_REWARDS,
              "RSBS_SONG_SPOTS_* must equal OoT's RO_SONG_SHUFFLE_* restricted values (#1169 XW4)");

extern "C" bool OoT_SongOfSoaringOoTOn(void) {
    return Combo_ComboCrossItemRule2(RSBS_XITEM2_SONG_SOARING_OOT);
}

extern "C" int OoT_SongOfSoaringOoTPooledCopies(void) {
    return (int)Combo_SongSoaringOoTCopies(Combo_ComboCrossItemFlags2());
}

extern "C" bool OoT_SoaringOoTOwned(void) {
    return Flags_GetRandomizerInf(RAND_INF_OBTAINED_SONG_OF_SOARING_OOT) != 0;
}

int OoT_SongSpots_RefuseOverflow(const std::vector<RandomizerGet>& songs, size_t spots, uint8_t shuffleSongs) {
    const bool soaringOoT = std::find(songs.begin(), songs.end(), RG_SONG_OF_SOARING_OOT) != songs.end();
    char msg[256];
    if (Combo_SongSpotsOverflow((uint32_t)songs.size(), (uint32_t)spots, (uint32_t)shuffleSongs, soaringOoT, msg,
                                sizeof(msg)) == 0u) {
        return 0;
    }
    SPDLOG_ERROR("{}; refusing to generate (#1169)", msg);
    fprintf(stderr, "[OoT] generation REFUSED: %s (#1169)\n", msg);
    Combo_SongSpotsRecordRefusal(msg);
    return 1;
}

/**
 * RG_SONG_OF_SOARING_OOT's get-item model (a #1185 descriptor): Majora's Mask's
 * own Song of Soaring, drawn by OoT as the host through the co-mounted archives
 * (OoT_ForeignModel_DrawForeignItem: since #1185 MM's songs answer a note and OoT
 * draws its own note in DrawSong's Soaring colour); OoT's generic white note
 * (GID_SONG_GENERIC, OoTMM's draw [NOTE]) when no archive answers.
 */
extern "C" void Randomizer_DrawSongOfSoaringOoT(PlayState* play, GetItemEntry* getItemEntry) {
    (void)getItemEntry;
    static SharedItem sMmSoaring;
    static int sResolved = -1;
    if (play == nullptr) {
        return;
    }
    if (sResolved < 0) {
        sResolved = Combo_GetForeignItemByNameFor((uint8_t)GAME_MM, "Song of Soaring", &sMmSoaring) ? 1 : 0;
    }
    if (sResolved == 1 && OoT_ForeignModel_DrawForeignItem(play, sMmSoaring.originGame, sMmSoaring.id) == 1) {
        return;
    }
    OoT_GetItem_Draw(play, GID_SONG_GENERIC);
}

// ============================================================================
// PLAYING IT (#1169 lane XW6; OoTMM packages/generator/src/oot/ocarina.c)
// ============================================================================
//
// THE MATCH. code_800EC960.c's free-play checker asks OoT_SoaringOoT_NoteMatched
// after its vanilla loop, on a note no vanilla song completed. A match publishes
// RSBS_OOT_SONG_SOARING as the staff state (OoTMM's 0x80 + 1). OoTMM gates the
// match itself on ownership and age (Ocarina_CheckCustomSongs: as an adult the
// notes match nothing and the ocarina stays open until B), and so does this.
// Free play only: OoTMM's check-no-warp arm (the frogs) plays an error and
// restarts, which is what unmatched notes amount to there too (decision 6).
//
// THE MESSAGE BRANCHES. z_message_PAL.c gives the state its own arm in four
// places (OCARINA_PLAYING, SETUP_DISPLAY_SONG_PLAYED, DISPLAY_SONG_PLAYED_TEXT_
// BEGIN, SONG_PLAYED_ACT), so it never indexes a vanilla song table. The last one
// calls OoT_SoaringOoT_OnSongPlayed, which runs the gates (Combo_SoaringDecide):
// any nonzero warp-song restriction or disableWarpSongs, and a Majora's Mask this
// session cannot run, show 0x88C; no active owl in MM's frozen half shows
// OoTMM's no-statues text; every refusal leaves OCARINA_MODE_04, exactly like
// vanilla's warp refusal.
//
// THE INTERIM CHOOSER (lane XW8 replaces OoT_SoaringOoT_BeginChooser with
// OoTMM's Termina owl map in OoT's pause). "Soar to <owl>?" with Soar / Next
// statue / No, opened with OoT_Message_ContinueTextbox from the "You played" box,
// which keeps ocarinaAction at OCARINA_ACTION_FREE_PLAY_DONE, so the vanilla
// close turns OCARINA_MODE_01 into 04 (TEXT_CLOSING). A 3-choice box is never
// converted to ocarina mode 02 (only a 2-choice one is, TEXT_DONE), so Player
// never warps through sWarpSongEntrances. The answer is read on OnPlayerUpdate,
// which runs before Message_Update in the same frame. Only A selects (OoT's
// ShouldAdvance also takes B and C-up, and the cursor starts on Soar): B and C-up
// cancel, as B does in OoTMM's picker. A cancel sets OCARINA_MODE_04 itself, and
// a chooser that closes without a departure gets 04 from the backstop.
//
// THE CROSSING. OoT_SoaringOoT_Depart re-asks the gates, arms the one-shot warp
// target (ComboWarp_Arm: OoT, this scene's entrance as the marker, MM, the owl's
// entrance, COMBO_SWITCH_KIND_SOARING) and starts a black fade to the marker in
// the same frame, with Link frozen (OoTMM HandleSoaring). The fade's end hands
// the marker to Combo_CheckEntranceSwitch (z_play.c), whose consume matches the
// target: from there it is an ordinary game switch, the freeze and the commit
// included, and #954's refusal stays the backstop (Link reloads at the return
// entrance). MM arrives at the owl's entrance; its own Play_Init resolves the
// cleared swamp and the spring mountain (nextCutsceneIndex 0xFFEF).

static_assert(RSBS_OOT_OCARINA_MODE_ACTIVE == OCARINA_MODE_01 && RSBS_OOT_OCARINA_MODE_END == OCARINA_MODE_04,
              "warp_song_soaring.h's ocarina modes must be OoT's OCARINA_MODE_01 and OCARINA_MODE_04 (#1169 XW6)");
static_assert(RSBS_OOT_SONG_SOARING > OCARINA_SONG_MEMORY_GAME && RSBS_OOT_SONG_SOARING < 0xFE,
              "the Soaring staff state must be above every vanilla song and below the playing and failed states "
              "(#1169 XW6)");
static_assert(RSBS_OOT_TEXT_SOARING_PLAYED == TEXT_CARPET_SALESMAN_ARMS_DEALER + 1 &&
                  RSBS_OOT_TEXT_SOARING_NO_STATUES == RSBS_OOT_TEXT_SOARING_PLAYED + 1 &&
                  RSBS_OOT_TEXT_SOARING_CHOOSER == RSBS_OOT_TEXT_SOARING_NO_STATUES + 1,
              "the Soaring texts follow SoH's last custom text id (#1169 XW6)");

namespace {

// MM's Song of Soaring playback (games/mm/src/audio/code_8019AF00.c,
// OCARINA_SONG_SOARING) in OoT's OcarinaNote layout {note, unk_01, length,
// volume, vibrato, tone, semitone}.
OcarinaNote sSoaringOoTPlayback[] = {
    { 5, 0, 18, 84, 0, 0, 0 },  { 11, 0, 18, 80, 0, 0, 0 },  { 14, 0, 36, 94, 0, 0, 0 },  { 5, 0, 18, 73, 0, 0, 0 },
    { 11, 0, 18, 76, 0, 0, 0 }, { 14, 0, 108, 96, 2, 0, 0 }, { 0xFF, 0, 0, 90, 0, 0, 0 },
};

// The interim chooser's RAM-only state, reset whenever it opens and on a load.
uint16_t sSoaringOoTChooserMask = 0;
int sSoaringOoTChooserOwl = -1;
bool sSoaringOoTChooserOpen = false;

ComboSoaringGate SoaringOoTGate(PlayState* play, uint16_t owlMask) {
    return Combo_SoaringDecide(play->msgCtx.disableWarpSongs != 0, play->interfaceCtx.restrictions.warpSongs,
                               Switch_GetTargetStatus(GAME_MM) == SWITCH_TARGET_PLAYABLE, owlMask);
}

void SoaringOoTRefuse(PlayState* play, ComboSoaringGate gate) {
    const uint16_t textId =
        gate == COMBO_SOARING_GATE_NO_STATUES ? (uint16_t)RSBS_OOT_TEXT_SOARING_NO_STATUES : (uint16_t)0x88C;
    OoT_Message_StartTextbox(play, textId, NULL); // 0x88C: OoT's "You can't warp here!"
    play->msgCtx.ocarinaMode = Combo_SoaringOcarinaModeAfterGate(gate);
}

void SoaringOoTBuildPlayed(uint16_t* textId, bool* loadFromMessageTable) {
    (void)textId;
    if (Context_GetCurrentGame() == GAME_MM || OoT_gPlayState == nullptr) {
        return; // shared hook storage guard (#344)
    }
    // OoTMM Ocarina_DisplaySongPlayedTextbox: 0x893's box with its own text.
    const CustomMessage vanilla = CustomMessage::LoadVanillaMessageTableEntry(0x893);
    const std::string text = "You played the %rSong of Soaring%w.&&&";
    CustomMessage msg(text, text, text, vanilla.GetTextBoxType(), vanilla.GetTextBoxPosition());
    msg.Format();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void SoaringOoTBuildNoStatues(uint16_t* textId, bool* loadFromMessageTable) {
    (void)textId;
    if (Context_GetCurrentGame() == GAME_MM || OoT_gPlayState == nullptr) {
        return;
    }
    // OoTMM soaringNoStatuesMessage, English in every language (decision 5).
    const std::string text = "You have yet to leave your %rmark%w&on any of the %rstatues%w you have&come across. "
                             "Hurry up and find&some.";
    CustomMessage msg(text, text, text);
    msg.Format();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

void SoaringOoTBuildChooser(uint16_t* textId, bool* loadFromMessageTable) {
    (void)textId;
    if (Context_GetCurrentGame() == GAME_MM || OoT_gPlayState == nullptr) {
        return;
    }
    const char* name = Combo_SoaringOwlName(sSoaringOoTChooserOwl);
    // OoTMM soaringMessage's "Soar to <owl>?", with a third choice that steps
    // to the next statue (the pause map is lane XW8's). MESSAGE_THREE_CHOICE
    // (0x1C) opens the three choice lines.
    const std::string text = std::string("Soar to %r") + (name != nullptr ? name : "?") + "%w?&" +
                             std::string(1, '\x1C') + "%gSoar&Next statue&No%w";
    CustomMessage msg(text, text, text);
    msg.Format();
    msg.LoadIntoFont();
    *loadFromMessageTable = false;
}

// The chooser's answer, read on OnPlayerUpdate (before Message_Update).
void SoaringOoTPollChooser() {
    if (!sSoaringOoTChooserOpen || Context_GetCurrentGame() == GAME_MM) {
        return; // shared hook storage guard (#344)
    }
    PlayState* play = OoT_gPlayState;
    if (play == nullptr) {
        sSoaringOoTChooserOpen = false;
        return;
    }
    MessageContext* msgCtx = &play->msgCtx;
    if (msgCtx->textId != RSBS_OOT_TEXT_SOARING_CHOOSER) {
        // THE BACKSTOP: the chooser closed, or was replaced, without a
        // departure. Whatever closed it, Link gets control back.
        sSoaringOoTChooserOpen = false;
        if (msgCtx->ocarinaMode == OCARINA_MODE_01) {
            msgCtx->ocarinaMode = OCARINA_MODE_04;
        }
        return;
    }
    if (OoT_Message_GetState(msgCtx) != TEXT_STATE_CHOICE) {
        return;
    }
    const Input* input = &play->state.input[0];
    const bool accept = CHECK_BTN_ALL(input->press.button, BTN_A);
    const bool advance = OoT_Message_ShouldAdvanceSilent(play) != 0;
    const ComboSoaringChoice choice = Combo_SoaringChooserInput(advance, accept, msgCtx->choiceIndex);
    switch (choice) {
        case COMBO_SOARING_CHOICE_WAIT:
            return;
        case COMBO_SOARING_CHOICE_NEXT:
            sSoaringOoTChooserOwl = Combo_SoaringNextOwl(sSoaringOoTChooserMask, sSoaringOoTChooserOwl);
            OoT_Message_ContinueTextbox(play, RSBS_OOT_TEXT_SOARING_CHOOSER);
            msgCtx->ocarinaMode = Combo_SoaringOcarinaModeAfterChoice(choice);
            return;
        case COMBO_SOARING_CHOICE_SOAR:
            sSoaringOoTChooserOpen = false;
            OoT_SoaringOoT_Depart(play, sSoaringOoTChooserOwl);
            return;
        case COMBO_SOARING_CHOICE_CANCEL:
        default:
            // The message system closes the box this frame (TEXT_DONE); END
            // hands Link back, whatever the close does with ocarinaMode.
            sSoaringOoTChooserOpen = false;
            msgCtx->ocarinaMode = Combo_SoaringOcarinaModeAfterChoice(COMBO_SOARING_CHOICE_CANCEL);
            return;
    }
}

// Deliberately NOT behind the #344 shared-hook-storage guard the other hooks
// carry: it touches nothing but this file's chooser state, and clearing that on
// either game's file load is correct (no chooser outlives a load).
void SoaringOoTOnLoadGame(int32_t fileNum) {
    (void)fileNum;
    sSoaringOoTChooserOpen = false;
    sSoaringOoTChooserOwl = -1;
    sSoaringOoTChooserMask = 0;
}

} // namespace

extern "C" int32_t OoT_SoaringOoT_NoteMatched(const uint8_t* song, uint8_t appendPos) {
    // With the hidden rule off (every world a player can make) this is the one
    // read the matcher costs per note, and it writes nothing.
    if (!OoT_SongOfSoaringOoTOn()) {
        return 0;
    }
    const PlayState* play = OoT_gPlayState;
    const bool freePlay = play != nullptr && play->msgCtx.ocarinaAction == OCARINA_ACTION_FREE_PLAY;
    if (!Combo_SoaringMatchArmed(true, Combo_ForeignPairingActive(), OoT_SoaringOoTOwned(), LINK_IS_CHILD, freePlay)) {
        return 0;
    }
    return Combo_SoaringOoTTailMatches(song, appendPos) ? 1 : 0;
}

extern "C" void OoT_SoaringOoT_StartPlayback(void) {
    // OoTMM Ocarina_SetPlaybackSong: start a playback slot, then point it at the
    // custom notes (Audio_OcaSetSongPlayback(1, ...) starts Minuet's slot, whose
    // first note is valid, so the note position stays 0). The audio update runs
    // on the game thread (game.c, func_800F3054), after this frame's messages.
    Audio_OcaSetSongPlayback(1, 1);
    OoT_sPlaybackSong = sSoaringOoTPlayback;
}

extern "C" void OoT_SoaringOoT_OnSongPlayed(PlayState* play) {
    if (play == nullptr) {
        return;
    }
    const uint16_t owlMask = Combo_SoaringOwlMask();
    const ComboSoaringGate gate = SoaringOoTGate(play, owlMask);
    if (gate != COMBO_SOARING_GATE_OFFER) {
        fprintf(stderr, "[OoT] Song of Soaring refused (gate %d, restriction %u, owls 0x%03X) (#1169)\n", (int)gate,
                (unsigned)play->interfaceCtx.restrictions.warpSongs, (unsigned)owlMask);
        SoaringOoTRefuse(play, gate);
        return;
    }
    OoT_SoaringOoT_BeginChooser(play, owlMask);
}

extern "C" void OoT_SoaringOoT_BeginChooser(PlayState* play, uint16_t owlMask) {
    if (play == nullptr) {
        return;
    }
    sSoaringOoTChooserMask = owlMask;
    sSoaringOoTChooserOwl = Combo_SoaringFirstOwl(owlMask);
    if (sSoaringOoTChooserOwl < 0) {
        SoaringOoTRefuse(play, COMBO_SOARING_GATE_NO_STATUES);
        return;
    }
    sSoaringOoTChooserOpen = true;
    // ContinueTextbox, never StartTextbox: StartTextbox sets ocarinaAction to
    // 0xFFFF and ocarinaMode to 00, and the vanilla close would then never hand
    // control back.
    OoT_Message_ContinueTextbox(play, RSBS_OOT_TEXT_SOARING_CHOOSER);
    play->msgCtx.ocarinaMode = Combo_SoaringOcarinaModeAfterGate(COMBO_SOARING_GATE_OFFER);
}

extern "C" void OoT_SoaringOoT_Depart(PlayState* play, int owl) {
    if (play == nullptr) {
        return;
    }
    const uint16_t owlMask = Combo_SoaringOwlMask();
    const ComboSoaringGate gate = SoaringOoTGate(play, owlMask);
    const uint16_t target = Combo_SoaringOwlEntrance(owl);
    if (gate != COMBO_SOARING_GATE_OFFER || target == 0 || ((owlMask >> owl) & 1u) == 0) {
        OoT_Message_CloseTextbox(play);
        play->msgCtx.ocarinaMode = OCARINA_MODE_04;
        return;
    }
    const uint16_t marker = (uint16_t)gSaveContext.entranceIndex;
    if (!ComboWarp_Arm(GAME_OOT, marker, GAME_MM, target, (uint8_t)RSBS_SOARING_SWITCH_KIND)) {
        // ComboWarp_Arm said why on stderr. Link gets control back.
        OoT_Message_CloseTextbox(play);
        play->msgCtx.ocarinaMode = OCARINA_MODE_04;
        return;
    }
    fprintf(stderr, "[OoT] Song of Soaring: crossing to %s (MM entrance 0x%04X) (#1169)\n", Combo_SoaringOwlName(owl),
            (unsigned)target);
    OoT_Message_CloseTextbox(play);
    play->msgCtx.ocarinaMode = OCARINA_MODE_04;
    // OoTMM HandleSoaring: Link frozen, then a black fade (comboTransition). The
    // target is armed in this frame; nothing that would spend it (an exit, a
    // void-out) runs while Link is frozen.
    Player* player = GET_PLAYER(play);
    if (player != nullptr) {
        player->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE;
        player->actor.freezeTimer = 10000;
    }
    play->nextEntranceIndex = marker;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.nextTransitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.seqId = (u8)NA_BGM_DISABLED;
    gSaveContext.natureAmbienceId = NATURE_ID_DISABLED;
}

// Always registered: every handler acts only on the three Soaring text ids or
// on an open chooser, which only a match opens, and a world without the hidden
// rule never matches.
static void RegisterSongOfSoaringOoTPlay() {
    COND_ID_HOOK(OnOpenText, RSBS_OOT_TEXT_SOARING_PLAYED, true, SoaringOoTBuildPlayed);
    COND_ID_HOOK(OnOpenText, RSBS_OOT_TEXT_SOARING_NO_STATUES, true, SoaringOoTBuildNoStatues);
    COND_ID_HOOK(OnOpenText, RSBS_OOT_TEXT_SOARING_CHOOSER, true, SoaringOoTBuildChooser);
    COND_HOOK(OnPlayerUpdate, true, SoaringOoTPollChooser);
    COND_HOOK(OnLoadGame, true, SoaringOoTOnLoadGame);
}

static RegisterShipInitFunc sSoaringOoTPlayInit(RegisterSongOfSoaringOoTPlay);

#endif // RSBS_SINGLE_EXECUTABLE
