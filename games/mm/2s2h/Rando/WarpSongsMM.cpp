/**
 * @file WarpSongsMM.cpp
 * @brief Ocarina of Time's six warp songs as Majora's Mask items (#1169 lane XW5;
 *        OoTMM songMinuetMm, songBoleroMm, songSerenadeMm, songRequiemMm,
 *        songNocturneMm and songPreludeMm). The MM direction of XW4's Song of
 *        Soaring (OoT), on the item pipeline masks P5 built (OoTMasksMM.cpp).
 *
 * THE ITEMS. Under its frozen second-word rule (RSBS_XITEM2_SONG_MINUET_MM ...,
 * hidden until lane XW12) MM's pool gains one copy of that song
 * (Logic/GeneratePools.cpp, MM_WarpSongMM_PooledCopies), ids APPENDED before
 * RI_MAX_TRAP after the masks (Types.h). Each is named as OoTMM names it
 * ("Minuet of Forest (MM)", StaticData/Items.cpp), a RITYPE_MAJOR row, so MM's
 * fill and the bag treat it as progression. Its give sets an OWNED BIT
 * (GiveItem.cpp), and a second copy converts to junk (ConvertItem.cpp). Under the
 * shared rule the MM copy is held back (warp_songs_mm.h): the shared family is
 * lane XW10's.
 *
 * WHERE OWNERSHIP IS STORED. In gComboCtx.mmWarpSongsOwned (context.h, ADR 0009
 * claim 21), one bit per song, not in MM's half: RandoSaveInfo has no padding and
 * randoInf two free bits, so six flags there would shift every later SaveContext
 * field of every older .redsave; the slot state byte P5 used has one free bit. A
 * starting song is the WORLD's (a seed package's world record carries the byte),
 * a pickup is the session's (every commit carries it), and every bracket that
 * restores MM's save after a SIMULATED playthrough restores this byte too
 * (MM_WarpSongsMM_SnapshotOwned / _RestoreOwned): the bag's MM engine, MM's
 * crawl (Logic.cpp), its glitchless fill and the attempt ladder (OnFileCreate.cpp)
 * all give placed items into the live save, and a song given there must not
 * become the player's.
 *
 * WHAT OWNING ONE DOES. Under its frozen rule it can be PLAYED (lane XW7, the
 * second half of this file): MM's note matcher, the warp prompt, the departure
 * through En_Test7 and the crossing to OoT's pad. MM's logic knows
 * only how to PLAY each one (Logic.h, CAN_PLAY_OOT_WARP_SONG: the ocarina, the
 * song under its rule, and OoTMM's can_play_tp_* buttons, which Ocarina Button
 * Shuffle decides), and no region uses that until lane XW9's travel facts. Neither
 * game's pause screen has a row for a foreign song; lane XW11 adds them.
 *
 * ITS MODEL (a #1185 descriptor, the shape P5 used for the masks): Ocarina of
 * Time's own song asked through the co-mounted archives
 * (MM_ForeignModel_DrawForeignItem), which MM's host-native table draws as MM's
 * own note in that song's colour (ForeignModelSingleExe.cpp's kNoteMinuet,
 * kNoteBolero and the GID_04 .. GID_07 rows); MM's generic white note when nothing
 * answers. Hosted by OoT (a crossing), the item answers "a 2S2H custom draw that
 * is not re-expressed" (IsCustomDrawn), the masks' answer.
 */
#ifdef RSBS_SINGLE_EXECUTABLE

#include <cstdint>
#include <cstring>

#include "Rando/Rando.h"
#include "2s2h/Enhancements/FrameInterpolation/FrameInterpolation.h"
#include "Rando/WarpSongsMM.h"

#include <cstdio>
#include <string>

#include "2s2h/CustomMessage/CustomMessage.h" // #1169 XW7: the played text and the warp prompt

#include "context.h"           // src/common: SharedItem, GAME_OOT, Switch_GetTargetStatus
#include "entrance.h"          // src/common: ComboWarp_Arm, the pending-switch questions (#1169 XW7)
#include "foreign_items.h"     // src/common: the second rules word, Combo_GetForeignItemByNameFor
#include "mm_warp_song_play.h" // src/common: the matcher, the gates, the texts, the latches (#1169 XW7)
#include "warp_songs_mm.h"     // src/common: the copies rule, the names, the ownership byte

extern "C" {
#include "variables.h"
#include "functions.h"
#include "objects/object_gi_melody/object_gi_melody.h" // gGiSongNoteDL
// ForeignModelSingleExe.cpp (#980): the other game's item model, MM as the host.
int MM_ForeignModel_DrawForeignItem(PlayState* play, uint8_t originGame, uint16_t id);
// src/audio/code_8019AF00.c: the song MM's playback plays (#1169 XW7).
extern OcarinaNote* MM_sPlaybackSong;
}

// THE APPEND-ONLY LOCKS. The six ids sit right after the masks P5 appended, so no
// id a world, a save or a crossing already holds moves.
static_assert(RI_SONG_MINUET == RI_MASK_SPOOKY + 1 && RI_SONG_PRELUDE == RI_SONG_MINUET + 5,
              "RI_SONG_MINUET .. RI_SONG_PRELUDE are appended right after the masks P5 added (#1169 XW5)");

namespace {

struct WarpSong {
    RandoItemId ri;
    uint32_t ruleBit;
};

// OoTMM's order, which is the order of the rule bits and the ownership bits.
constexpr WarpSong kWarpSongs[] = {
    { RI_SONG_MINUET, RSBS_XITEM2_SONG_MINUET_MM },     { RI_SONG_BOLERO, RSBS_XITEM2_SONG_BOLERO_MM },
    { RI_SONG_SERENADE, RSBS_XITEM2_SONG_SERENADE_MM }, { RI_SONG_REQUIEM, RSBS_XITEM2_SONG_REQUIEM_MM },
    { RI_SONG_NOCTURNE, RSBS_XITEM2_SONG_NOCTURNE_MM }, { RI_SONG_PRELUDE, RSBS_XITEM2_SONG_PRELUDE_MM },
};
static_assert(sizeof(kWarpSongs) / sizeof(kWarpSongs[0]) == RSBS_MM_WARP_SONG_COUNT, "one row per MM warp song");

/** The song's index in kWarpSongs, or -1. */
int IndexOf(uint16_t riId) {
    for (int i = 0; i < (int)RSBS_MM_WARP_SONG_COUNT; i++) {
        if ((uint16_t)kWarpSongs[i].ri == riId) {
            return i;
        }
    }
    return -1;
}

} // namespace

// At file scope, not in the anonymous namespace: OPEN_DISPS / CLOSE_DISPS
// block-declare MM_FrameInterpolation_RecordOpenChild / _RecordCloseChild, and
// inside a namespace that declaration names a C++-linkage function of that
// namespace instead of the extern "C" one FrameInterpolation.h declares, which
// never links (ForeignMaskMM.cpp's DrawOnHead, ForeignModelSingleExe.cpp's
// ForeignModel_DrawLayer).
/** MM's generic note: DrawSong's draw (DrawItem.cpp) in white. */
static void DrawGenericNote() {
    OPEN_DISPS(MM_gPlayState->state.gfxCtx);
    Gfx_SetupDL25_Xlu(MM_gPlayState->state.gfxCtx);
    MATRIX_FINALIZE_AND_LOAD(POLY_XLU_DISP++, MM_gPlayState->state.gfxCtx);
    gDPSetEnvColor(POLY_XLU_DISP++, 255, 255, 255, 255);
    MM_gSPDisplayList(POLY_XLU_DISP++, (Gfx*)&gGiSongNoteDL);
    CLOSE_DISPS(MM_gPlayState->state.gfxCtx);
}

extern "C" uint32_t MM_WarpSongMM_RuleBit(uint16_t riId) {
    const int i = IndexOf(riId);
    return i >= 0 ? kWarpSongs[i].ruleBit : 0u;
}

extern "C" uint16_t MM_WarpSongMM_ItemId(uint32_t ruleBit) {
    for (const WarpSong& song : kWarpSongs) {
        if (song.ruleBit == ruleBit) {
            return (uint16_t)song.ri;
        }
    }
    return (uint16_t)RI_UNKNOWN;
}

extern "C" bool MM_WarpSongMM_RuleOn(uint16_t riId) {
    const uint32_t bit = MM_WarpSongMM_RuleBit(riId);
    return bit != 0u && Combo_ComboCrossItemRule2(bit);
}

extern "C" int MM_WarpSongMM_PooledCopies(uint16_t riId) {
    const uint32_t bit = MM_WarpSongMM_RuleBit(riId);
    return bit != 0u ? (int)Combo_MMWarpSongCopies(Combo_ComboCrossItemFlags2(), bit) : 0;
}

extern "C" bool MM_WarpSongMM_Owned(uint16_t riId) {
    const uint32_t bit = MM_WarpSongMM_RuleBit(riId);
    return bit != 0u && Combo_MMWarpSongOwned(bit);
}

extern "C" void MM_WarpSongMM_Give(uint16_t riId) {
    const uint32_t bit = MM_WarpSongMM_RuleBit(riId);
    if (bit != 0u) {
        Combo_MMWarpSongGive(bit);
    }
}

extern "C" bool MM_WarpSongMM_Usable(uint16_t riId) {
    return MM_WarpSongMM_RuleOn(riId) && MM_WarpSongMM_Owned(riId);
}

extern "C" uint8_t MM_WarpSongsMM_SnapshotOwned(void) {
    return Combo_MMWarpSongsOwned();
}

extern "C" void MM_WarpSongsMM_RestoreOwned(uint8_t owned) {
    Combo_MMWarpSongsSetOwned(owned);
}

extern "C" const char* MM_WarpSongMM_IconPath(uint16_t riId) {
    if (IndexOf(riId) < 0) {
        return nullptr;
    }
    // MM's own song note: every song of MM's quest page draws this texture.
    return (const char*)MM_gItemIcons[ITEM_SONG_SOARING];
}

extern "C" void MM_WarpSongMM_DrawGetItem(uint16_t riId) {
    const int i = IndexOf(riId);
    if (i < 0 || MM_gPlayState == nullptr) {
        return;
    }
    // Ocarina of Time's own song, by the name OoT's item table gives it, through
    // the co-mounted archives. Looked up once per song, a miss included (-1 not
    // yet asked, 0 no such OoT item, 1 found; StoneMaskOoT.cpp's shape): the name
    // registry is static once both games' tables are registered at bring-up, and
    // a miss asked again every frame would walk the whole registry per draw.
    static SharedItem sOoTSong[RSBS_MM_WARP_SONG_COUNT];
    static_assert(RSBS_MM_WARP_SONG_COUNT == 6u, "one 'not yet asked' (-1) per MM warp song below");
    static int8_t sResolved[RSBS_MM_WARP_SONG_COUNT] = { -1, -1, -1, -1, -1, -1 };
    if (sResolved[i] < 0) {
        const char* name = Combo_MMWarpSongOoTName(kWarpSongs[i].ruleBit);
        sResolved[i] = name != nullptr && Combo_GetForeignItemByNameFor((uint8_t)GAME_OOT, name, &sOoTSong[i]) ? 1 : 0;
    }
    if (sResolved[i] == 1 &&
        MM_ForeignModel_DrawForeignItem(MM_gPlayState, sOoTSong[i].originGame, sOoTSong[i].id) == 1) {
        return;
    }
    DrawGenericNote();
}

// ============================================================================
// #1169 lane XW7: PLAYING them (src/common/mm_warp_song_play.h has the design)
// ============================================================================
//
// OoTMM's flow, call site by call site (packages/generator/src/mm/ocarina.c):
// the matcher (Ocarina_CheckCustomSongs) publishes 0x80 + song; the song-played
// box takes it (Ocarina_BeforeSongPlayingProcessed: z_message.c E1); MM plays
// OoT's own notes with no fanfare (Ocarina_SetPlaybackSong: E2; the fanfare is a
// stated divergence, MM's audio has no OoT sequence), spawns OoTMM's song effect
// (Ocarina_SpawnSongEffect) and says "You played the <song>!"
// (Ocarina_DisplaySongPlayedTextbox: E3); at the text's close the prompt or the
// refusal (Ocarina_HandleLastPlayedSong: E4, here MM_WarpSongMM_OnSongPlayed); OK
// is MM's own OCARINA_MODE_WARP, which Player turns into En_Test7 with params
// 0x8000 | song (Ocarina_HandleWarp: z_player.c); En_Test7's final action departs
// (EnTest7_TriggerWarpWrapper, mm/actors/En_Test7.c: here MM_WarpSongMM_Depart).

static_assert(OCARINA_BTN_A == 0 && OCARINA_BTN_C_DOWN == 1 && OCARINA_BTN_C_RIGHT == 2 && OCARINA_BTN_C_LEFT == 3 &&
                  OCARINA_BTN_C_UP == 4,
              "mm_warp_song_play.c spells MM's staff buttons A 0, C-down 1, C-right 2, C-left 3, C-up 4");
static_assert(RSBS_MMWP_STATE_FIRST > OCARINA_SONG_TERMINA_WALL && RSBS_MMWP_STATE_LAST < 0xFE,
              "the warp songs' staff states sit above every vanilla state and below 0xFE / 0xFF");
static_assert((uint16_t)RSBS_MMWP_TEXT_RESTRICTED == 0x1B95u, "MM's restricted-song refusal text");

namespace {

// OoTMM's sOcarinaCustomSongNotes (mm/ocarina.c), OoT's own notes in MM's
// OcarinaNote layout { pitch, length, volume, vibrato, bend, bFlat4Flag }, ended
// as MM's tables end (pitch NONE, length 0).
// Not const: MM_sPlaybackSong is a mutable pointer (playback only reads it).
OcarinaNote sMMWarpPlayNotes[RSBS_MMWP_SONG_COUNT][10] = {
    // Minuet of Forest
    { { OCARINA_PITCH_D4, 18, 86, 0, 0, 0 },
      { OCARINA_PITCH_D5, 18, 92, 0, 0, 0 },
      { OCARINA_PITCH_B4, 72, 86, 0, 0, 0 },
      { OCARINA_PITCH_A4, 18, 80, 0, 0, 0 },
      { OCARINA_PITCH_B4, 18, 88, 0, 0, 0 },
      { OCARINA_PITCH_A4, 144, 86, 0, 0, 0 },
      { OCARINA_PITCH_NONE, 0, 86, 0, 0, 0 } },
    // Bolero of Fire
    { { OCARINA_PITCH_F4, 15, 80, 0, 0, 0 },
      { OCARINA_PITCH_D4, 15, 72, 0, 0, 0 },
      { OCARINA_PITCH_F4, 15, 84, 0, 0, 0 },
      { OCARINA_PITCH_D4, 15, 76, 0, 0, 0 },
      { OCARINA_PITCH_A4, 15, 84, 0, 0, 0 },
      { OCARINA_PITCH_F4, 15, 74, 0, 0, 0 },
      { OCARINA_PITCH_A4, 15, 78, 0, 0, 0 },
      { OCARINA_PITCH_F4, 135, 66, 0, 0, 0 },
      { OCARINA_PITCH_NONE, 0, 66, 0, 0, 0 } },
    // Serenade of Water
    { { OCARINA_PITCH_D4, 36, 60, 0, 0, 0 },
      { OCARINA_PITCH_F4, 36, 78, 0, 0, 0 },
      { OCARINA_PITCH_A4, 33, 82, 0, 0, 0 },
      { OCARINA_PITCH_NONE, 3, 82, 0, 0, 0 },
      { OCARINA_PITCH_A4, 36, 84, 0, 0, 0 },
      { OCARINA_PITCH_B4, 144, 90, 0, 0, 0 },
      { OCARINA_PITCH_NONE, 0, 90, 0, 0, 0 } },
    // Requiem of Spirit
    { { OCARINA_PITCH_D4, 45, 88, 0, 0, 0 },
      { OCARINA_PITCH_F4, 23, 86, 0, 0, 0 },
      { OCARINA_PITCH_D4, 22, 84, 0, 0, 0 },
      { OCARINA_PITCH_A4, 45, 86, 0, 0, 0 },
      { OCARINA_PITCH_F4, 45, 94, 0, 0, 0 },
      { OCARINA_PITCH_D4, 180, 94, 0, 0, 0 },
      { OCARINA_PITCH_NONE, 0, 94, 0, 0, 0 } },
    // Nocturne of Shadow
    { { OCARINA_PITCH_B4, 36, 88, 0, 0, 0 },
      { OCARINA_PITCH_A4, 33, 84, 0, 0, 0 },
      { OCARINA_PITCH_NONE, 3, 84, 0, 0, 0 },
      { OCARINA_PITCH_A4, 18, 82, 0, 0, 0 },
      { OCARINA_PITCH_D4, 18, 60, 0, 0, 0 },
      { OCARINA_PITCH_B4, 18, 90, 0, 0, 0 },
      { OCARINA_PITCH_A4, 18, 88, 0, 0, 0 },
      { OCARINA_PITCH_F4, 144, 96, 0, 0, 0 },
      { OCARINA_PITCH_NONE, 0, 96, 0, 0, 0 } },
    // Prelude of Light
    { { OCARINA_PITCH_D5, 15, 84, 0, 0, 0 },
      { OCARINA_PITCH_A4, 45, 88, 0, 0, 0 },
      { OCARINA_PITCH_D5, 15, 88, 0, 0, 0 },
      { OCARINA_PITCH_A4, 15, 82, 0, 0, 0 },
      { OCARINA_PITCH_B4, 15, 86, 0, 0, 0 },
      { OCARINA_PITCH_D5, 60, 90, 0, 0, 0 },
      { OCARINA_PITCH_NONE, 75, 90, 0, 0, 0 },
      { OCARINA_PITCH_NONE, 0, 90, 0, 0, 0 } },
};

/** MM's teal (message colour 5) for Combo_MMWarpPlay_*Text's "%t", which 2Ship's
 *  CustomMessage has no code for. */
std::string MMWarpPlayMapTeal(const char* text) {
    std::string msg = text != nullptr ? text : "";
    CustomMessage::Replace(&msg, "%t", std::string(1, '\x05'));
    return msg;
}

/** Every input of Combo_MMWarpPlay_Decide, read now; the scene's Soaring
 *  restriction is the one only MM can read (OoTMM: any nonzero value, ruling 5). */
ComboMMWarpPlayInputs MMWarpPlayGather(PlayState* play, int song) {
    ComboMMWarpPlayInputs in;
    Combo_MMWarpPlay_GatherInputs(song, play != nullptr && play->interfaceCtx.restrictions.songOfSoaring != 0, &in);
    return in;
}

const char* MMWarpPlaySongName(int song) {
    const char* name = Combo_MMWarpPlay_SongName(song);
    return name != nullptr ? name : "?";
}

} // namespace

extern "C" uint8_t MM_WarpSongMM_UsableMask(void) {
    if ((Combo_ComboCrossItemFlags2() & RSBS_MMWP_RULE_MASK) == 0u || !Combo_ForeignPairingActive()) {
        return 0;
    }
    uint8_t mask = 0;
    for (int i = 0; i < (int)RSBS_MM_WARP_SONG_COUNT; i++) {
        if (MM_WarpSongMM_Usable((uint16_t)kWarpSongs[i].ri)) {
            mask = (uint8_t)(mask | (1u << i));
        }
    }
    return mask;
}

extern "C" uint8_t MM_WarpSongMM_NoteMatched(const uint8_t* buf, uint8_t appendPos, const uint8_t* btnToPitch) {
    // Gate 1, the only cost with every rule off: one read of the frozen word.
    if ((Combo_ComboCrossItemFlags2() & RSBS_MMWP_RULE_MASK) == 0u) {
        return 0;
    }
    // Free play only (open decision 3): never a teaching, a check or a recording.
    if (MM_gPlayState == nullptr || MM_gPlayState->msgCtx.ocarinaAction != OCARINA_ACTION_FREE_PLAY) {
        return 0;
    }
    const uint8_t usable = MM_WarpSongMM_UsableMask();
    if (usable == 0) {
        return 0;
    }
    const int song = Combo_MMWarpPlay_MatchSong(buf, appendPos, btnToPitch, usable);
    return song >= 0 ? Combo_MMWarpPlay_StaffState(song) : (uint8_t)0;
}

extern "C" bool MM_WarpSongMM_IsStaffState(unsigned state) {
    return Combo_MMWarpPlay_IsStaffState(state);
}

extern "C" bool MM_WarpSongMM_WallRandomOnly(void) {
    return (Combo_ComboCrossItemFlags2() & RSBS_MMWP_RULE_MASK) != 0u && Combo_ForeignPairingActive();
}

extern "C" bool MM_WarpSongMM_WallRejected(const uint8_t* buttons, uint8_t n) {
    if (!MM_WarpSongMM_WallRandomOnly()) {
        return false;
    }
    return Combo_MMWarpPlay_WallStealsBeforeEnd(buttons, n, MM_WarpSongMM_UsableMask());
}

extern "C" void MM_WarpSongMM_PointPlaybackAtSong(unsigned state) {
    const int song = Combo_MMWarpPlay_SongOfStaffState(state);
    if (song >= 0) {
        MM_sPlaybackSong = sMMWarpPlayNotes[song];
    }
}

extern "C" bool MM_WarpSongMM_SpawnSongEffect(PlayState* play, unsigned state) {
    const int song = Combo_MMWarpPlay_SongOfStaffState(state);
    if (song < 0 || play == nullptr) {
        return false;
    }
    Player* player = GET_PLAYER(play);
    if (player == nullptr) {
        return true; // nothing to stand the effect on; never the vanilla tables
    }
    // OoTMM sCustomSongActorEffectIds / Params: OCEFF_WIPE5 with the song's colour,
    // the Prelude OCEFF_WIPE params 1.
    const bool prelude = song == RSBS_MMWP_SONG_COUNT - 1;
    Actor* effect =
        MM_Actor_Spawn(&play->actorCtx, play, prelude ? ACTOR_OCEFF_WIPE : ACTOR_OCEFF_WIPE5, player->actor.world.pos.x,
                       player->actor.world.pos.y, player->actor.world.pos.z, 0, 0, 0, prelude ? 1 : song);
    if (effect != nullptr) {
        play->msgCtx.ocarinaSongEffectActive = true; // the effect's Destroy clears it
    }
    return true;
}

extern "C" uint16_t MM_WarpSongMM_PlayedTextId(unsigned state) {
    char text[96];
    if (Combo_MMWarpPlay_PlayedText(Combo_MMWarpPlay_SongOfStaffState(state), text, sizeof(text)) < 0) {
        return (uint16_t)RSBS_MMWP_TEXT_PLAYED_BOX;
    }
    CustomMessage::Entry entry = CustomMessage::LoadVanillaMessageTableEntry((u16)RSBS_MMWP_TEXT_PLAYED_BOX);
    CustomMessage::SetActiveCustomMessage(MMWarpPlayMapTeal(text), entry);
    return (uint16_t)CUSTOM_MESSAGE_ID;
}

extern "C" void MM_WarpSongMM_OnSongPlayed(PlayState* play, unsigned state) {
    const int song = Combo_MMWarpPlay_SongOfStaffState(state);
    Combo_MMWarpPlay_ClearPending();
    if (play == nullptr) {
        return;
    }
    const ComboMMWarpPlayInputs in = MMWarpPlayGather(play, song);
    const ComboMMWarpPlayGate gate = Combo_MMWarpPlay_Decide(&in);
    char question[128];
    if (gate == COMBO_MMWP_OFFER && Combo_MMWarpPlay_PromptText(song, question, sizeof(question)) >= 0) {
        // OoTMM's layout: the question, a blank line under a one-line question,
        // then the two-choice in green.
        std::string msg = question;
        if (Combo_MMWarpPlay_LineCount(question) < 2) {
            msg += "\n ";
        }
        msg += "\n%g\xC2"
               "     OK\n     No";
        CustomMessage::Entry entry = CustomMessage::LoadVanillaMessageTableEntry((u16)RSBS_MMWP_TEXT_PROMPT_BOX);
        Combo_MMWarpPlay_SetPending(song);
        // As the Soaring branch opens 0x1B93 (no talk actor), with the custom text.
        CustomMessage::SetActiveCustomMessage(MMWarpPlayMapTeal(msg.c_str()), entry);
        MM_Message_StartTextbox(play, (u16)CUSTOM_MESSAGE_ID, NULL);
        play->msgCtx.ocarinaMode = OCARINA_MODE_ACTIVE;
        fprintf(stderr, "[MM] warp song: %s played, the warp prompt is shown (#1169 XW7)\n", MMWarpPlaySongName(song));
        return;
    }
    fprintf(stderr, "[MM] warp song: %s refused: %s (#1169 XW7)\n", MMWarpPlaySongName(song),
            Combo_MMWarpPlay_GateName(gate));
    MM_Message_StartTextbox(play, (u16)RSBS_MMWP_TEXT_RESTRICTED, NULL);
    play->msgCtx.ocarinaMode = OCARINA_MODE_PROCESS_RESTRICTED_SONG;
}

extern "C" bool MM_WarpSongMM_HasPending(PlayState* play) {
    const int song = Combo_MMWarpPlay_PendingSong();
    return song >= 0 && play != nullptr && play->msgCtx.lastPlayedSong == Combo_MMWarpPlay_StaffState(song);
}

extern "C" bool MM_WarpSongMM_TakePending(PlayState* play, uint8_t* outSong) {
    if (!MM_WarpSongMM_HasPending(play)) {
        return false;
    }
    return Combo_MMWarpPlay_TakePending(outSong);
}

extern "C" void MM_WarpSongMM_ClearPending(void) {
    Combo_MMWarpPlay_ClearPending();
}

extern "C" uint16_t MM_WarpSongMM_EnTest7Params(uint8_t song) {
    return RSBS_MMWP_ENTEST7_PARAMS(song);
}

extern "C" bool MM_WarpSongMM_IsEnTest7Params(int16_t params) {
    return Combo_MMWarpPlay_IsEnTest7Params(params);
}

extern "C" void MM_WarpSongMM_Depart(PlayState* play, int song) {
    // One-shot: En_Test7's final action calls this every frame until the scene
    // unloads, and a later call would see its own arm or its own switch.
    if (!Combo_MMWarpPlay_DepartOnce() || play == nullptr) {
        return;
    }
    const ComboMMWarpPlayInputs in = MMWarpPlayGather(play, song);
    const ComboMMWarpPlayGate gate = Combo_MMWarpPlay_Decide(&in);
    const uint16_t marker = (uint16_t)gSaveContext.save.entrance;
    const uint16_t pad = Combo_MMWarpPlay_PadEntrance(song);
    bool armed = false;
    if (gate == COMBO_MMWP_OFFER) {
        armed = ComboWarp_Arm(GAME_MM, marker, GAME_OOT, pad, (uint8_t)COMBO_SWITCH_KIND_WARP_SONG);
    }
    if (armed) {
        fprintf(stderr, "[MM] warp song: %s departs for OoT pad 0x%04X (marker 0x%04X) (#1169 XW7)\n",
                MMWarpPlaySongName(song), (unsigned)pad, (unsigned)marker);
    } else {
        fprintf(stderr, "[MM] warp song: %s departure refused (%s); Link reloads entrance 0x%04X (#1169 XW7)\n",
                MMWarpPlaySongName(song),
                gate == COMBO_MMWP_OFFER ? "the warp target refused to arm" : Combo_MMWarpPlay_GateName(gate),
                (unsigned)marker);
    }
    // En_Test7's own warp lines (EnTest7_WarpCsWarp): the next transition is the
    // marker, which Play_UpdateTransition hands to Combo_CheckEntranceSwitch.
    play->nextEntrance = marker;
    play->transitionTrigger = TRANS_TRIGGER_START;
    play->transitionType = TRANS_TYPE_FADE_BLACK;
    gSaveContext.seqId = (u16)NA_BGM_DISABLED;
    gSaveContext.ambienceId = (u8)AMBIENCE_ID_DISABLED;
}

#endif // RSBS_SINGLE_EXECUTABLE
