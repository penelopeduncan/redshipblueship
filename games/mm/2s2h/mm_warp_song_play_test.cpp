/**
 * @file mm_warp_song_play_test.cpp
 * Majora's Mask's half of two ROM-free rows of #1169 lane XW7 (Ocarina of Time's
 * warp songs played in Termina; the design is src/common/mm_warp_song_play.h, the
 * rows' dispatch src/common/test_runner.cpp, their common half
 * src/common/tests/test_mm_warp_song_play.c):
 *
 *   mm-warp-song-matcher (CTest MMWarpSongMatcher, redship tier; the runner brings
 *   up the display-free shared context first: the ocarina path reads the
 *   CustomOcarina CVars). MM's REAL free-play checker, frame by frame through the
 *   real AudioOcarina_Update, the pad fed through the fenced input seam
 *   (MM_AudioOcarina_SetInputOverrideForTest), as mm-ocarina-dpad-notes does:
 *     M1 rules off (owned, paired, free play): no song matches (state 0xFE);
 *     M2 rules on but nothing owned: no match;
 *     M3 unpaired: no match;
 *     M4 not free play (OCARINA_ACTION_CHECK_NOTIME), and no play state: no match;
 *     M5 everything on: each song answers 0x80 + song exactly on its last note
 *        and 0xFE on every note before;
 *     M6 with every song usable each vanilla song 0..13, alone in the mask,
 *        answers what the pure matcher predicts: its own index unless a warp
 *        song completes strictly before its last note (a recorded prefix steal),
 *        and its own index on a same-note tie (vanilla wins);
 *     M7 the recorded steal: a stray A then the Inverted Song of Time answers the
 *        Requiem of Spirit on the Inverted song's fifth note;
 *     M8 an 8-note scarecrow spawn song whose last six notes are Minuet's answers
 *        the scarecrow (22) on its 8th note and nothing on the 6th;
 *     M9 the Termina wall: with the rules on, 200 generated walls are all random
 *        notes (never an OoT song) and none lets a usable song complete before
 *        its last note; with the rules off the generator still produces OoT-song
 *        walls (the alternation is untouched).
 *
 *   mm-warp-song-departure (CTest MMWarpSongDeparture, redship tier, same bring-up)
 *   over a private PlayState and a zeroed En_Test7 driven through the REAL
 *   EnTest7_WarpCsWarp:
 *     D1 a departure with nothing taken arms nothing and writes nothing;
 *     D2 the first call after TakePending arms (MM, the current entrance, OoT,
 *        Minuet's pad 0x0600, COMBO_SWITCH_KIND_WARP_SONG), starts the fade to
 *        the marker and moves En_Test7 off EnTest7_WarpCsWarp;
 *     D3 a second EnTest7_WarpCsWarp, and the hold action, write no field and
 *        leave the arm as it was;
 *     D4 the transition's check consumes the target (OoT, the pad, the kind),
 *        and a third call after that changes nothing;
 *     D5 every refusal (OoT adult, OoT unknown, no record, unpaired, restricted,
 *        OoT unplayable) arms nothing and reloads the current entrance;
 *     D6 Player's fallback (Depart straight after TakePending) arms once per
 *        TakePending;
 *     D6b the prompt's decision MM_WarpSongMM_OnSongPlayed acts on
 *        (MM_WarpSongMM_PromptActionFor): every gate holding offers the prompt
 *        (0x1B93's box, a question, Minuet pending where Player's warp arm
 *        looks); OoT frozen as an adult and a Soaring-restricted scene refuse
 *        with 0x1B95 and leave nothing pending, a stale pending song included;
 *     D7 En_Test7's predicate takes exactly 0x8000..0x8005, never
 *        ENTEST7_ARRIVE or an owl mode.
 *
 * Everything touched is restored: the ocarina globals, the wall, MM_gPlayState,
 * MM's entrance, the combo record, the frozen states, the switch status override
 * and the warp target. CVars are restored clear-never-store.
 */

#include "global.h"

#include <cstdio>

#if !defined(RSBS_SINGLE_EXECUTABLE)
// The rows are single-exe only; test_runner.cpp declares these unconditionally.
extern "C" int MM_WarpSongPlay_MatcherRunHeadless(void) {
    printf("[TEST] mm-warp-song-matcher: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
extern "C" int MM_WarpSongPlay_DepartureRunHeadless(void) {
    printf("[TEST] mm-warp-song-departure: PASS (not applicable outside RSBS_SINGLE_EXECUTABLE)\n");
    return 0;
}
#else

#include <climits>
#include <cstring>
#include <memory>

#include <libultraship/bridge/consolevariablebridge.h>

#include "context.h"
#include "entrance.h"
#include "foreign_items.h"
#include "mm_warp_song_play.h"
#include "warp_songs_mm.h"
#include "2s2h/Rando/WarpSongsMM.h"

extern "C" {
#include "overlays/actors/ovl_En_Test7/z_en_test7.h"
// src/audio/code_8019AF00.c, file scope with external linkage (redship.map names).
extern u32 sOcarinaInputButtonCur;
extern u32 sOcarinaInputButtonStart;
extern u32 sOcarinaInputButtonPrev;
extern s32 sOcarinaInputButtonPress;
extern u8 sCurOcarinaPitch;
extern u8 sPrevOcarinaPitch;
extern u8 sCurOcarinaButtonIndex;
extern u32 sOcarinaFlags;
extern u8 sIsOcarinaInputEnabled;
extern s8 sOcarinaInstrumentId;
extern u8 sOcarinaStaffPlayingPos;
extern u8 MM_sOcarinaDropInputTimer;
extern OcarinaStaff MM_sPlayingStaff;
extern OcarinaSongButtons gOcarinaSongButtons[OCARINA_SONG_MAX];
extern OcarinaNote sOcarinaSongNotes[OCARINA_SONG_MAX][20];
extern u32 sOcarinaWallCounter;
extern u8 sButtonToPitchMap[5];
// The playback MM_WarpSongMM_SetupSongPlayback sets (D8 restores it).
extern OcarinaNote* MM_sPlaybackSong;
extern u8 MM_sPlaybackState;
extern u8 sPlaybackStaffStopPos;
extern u8 sPlaybackStaffStartPos;
extern s32 sPlaybackNoteTimer;
extern u16 MM_sPlaybackNotePos;
extern u16 sPlaybackStaffPos;
extern u8 sPlaybackPitch;
void AudioOcarina_Update(void);
void AudioOcarina_StartDefault(u32 ocarinaFlags);
void AudioOcarina_SetInstrument(u8 ocarinaInstrumentId);
void AudioOcarina_TerminaWallGenerateNotes(void);
void MM_AudioOcarina_SetInputOverrideForTest(const Input* input);
// ovl_En_Test7/z_en_test7.c
void EnTest7_WarpCsWarp(EnTest7* thisx, PlayState* play);
// games/oot/soh/SaveManager.cpp: OoT's descriptors, the frozen age's among them.
void OoT_SlotMeta_Register(void);
// games/oot/soh/Enhancements/randomizer/WarpSongRecordOoT.cpp
void OoT_WarpSongRecord_TestFreezeOoTHalf(int linkAge, int started, int live);
}

namespace {

constexpr u32 kWspFreePlay = 0xC0000000u;
constexpr u8 kWspNoSong = 0xFE;
constexpr int kWspHoldFrames = 3;
constexpr int kWspReleaseFrames = 3;
constexpr uint32_t kWspSeed = 0x1169B7B7u;
constexpr uint32_t kWspSettingsHash = 0x1169C7C7u;
// South Clock Town spawn 1, ENTRANCE(SOUTH_CLOCK_TOWN, 1): no door link claims it.
constexpr u16 kWspMarker = 0xD810;

Input sWspInput;
int sWspFailures = 0;

#define WSP_CHECK(tag, cond, ...)                                  \
    do {                                                           \
        if (!(cond)) {                                             \
            printf("[TEST] FAIL (%s): ", (tag));                   \
            printf(__VA_ARGS__);                                   \
            printf("  [%s] (%s:%d)\n", #cond, __FILE__, __LINE__); \
            sWspFailures++;                                        \
        }                                                          \
    } while (0)

void WspFrame(u16 buttons) {
    memset(&sWspInput, 0, sizeof(sWspInput));
    sWspInput.cur.button = buttons;
    AudioOcarina_Update();
}

/** Holds @p buttons and releases; returns the staff state on the press frame. */
u8 WspPress(u16 buttons) {
    WspFrame(buttons);
    const u8 state = MM_sPlayingStaff.state;
    for (int i = 1; i < kWspHoldFrames; i++) {
        WspFrame(buttons);
    }
    for (int i = 0; i < kWspReleaseFrames; i++) {
        WspFrame(0);
    }
    return state;
}

void WspStartFreePlay(u32 songMask) {
    MM_sOcarinaDropInputTimer = 0;
    AudioOcarina_StartDefault(songMask | kWspFreePlay);
    for (int i = 0; i < kWspReleaseFrames; i++) {
        WspFrame(0);
    }
}

u16 WspPad(u8 button) {
    switch (button) {
        case OCARINA_BTN_A:
            return BTN_A;
        case OCARINA_BTN_C_DOWN:
            return BTN_CDOWN;
        case OCARINA_BTN_C_RIGHT:
            return BTN_CRIGHT;
        case OCARINA_BTN_C_LEFT:
            return BTN_CLEFT;
        case OCARINA_BTN_C_UP:
            return BTN_CUP;
        default:
            return 0;
    }
}

/** Plays @p n staff buttons; *outBefore gets the first non-0xFE state before the
 *  last note (0xFE when there was none); returns the last note's state. */
u8 WspPlay(const u8* buttons, int n, u8* outBefore) {
    u8 before = kWspNoSong;
    u8 state = kWspNoSong;
    for (int i = 0; i < n; i++) {
        state = WspPress(WspPad(buttons[i]));
        if (i < n - 1 && state != kWspNoSong && before == kWspNoSong) {
            before = state;
        }
    }
    if (outBefore != nullptr) {
        *outBefore = before;
    }
    return state;
}

/** A paired world whose frozen second rules word is @p rules2, every song owned
 *  as @p owned (XW6's Xw6FreezeWorld shape). */
void WspFreezeWorld(uint32_t rules2, uint8_t owned, bool paired) {
    ComboContext_Init();
    gComboCtx.sourceIsRando = paired;
    gComboCtx.sharedRandoSeed = kWspSeed;
    gComboCtx.sharedRandoSettingsHash = paired ? kWspSettingsHash : 0u;
    ComboSettingsRecord rec;
    Combo_ComboSettingsDefaults(&rec);
    Combo_FreezeComboSettingsEx2(&rec, NULL, rules2);
    Combo_MMWarpSongsSetOwned(owned);
}

struct WspOcarinaSnapshot {
    u32 cur, start, prev;
    s32 press;
    u8 curPitch, prevPitch, curButton;
    u32 flags;
    u8 inputEnabled;
    s8 instrument;
    u8 staffPos;
    u8 dropTimer;
    OcarinaStaff playingStaff;
    OcarinaSongButtons scarecrow;
    OcarinaSongButtons wall;
    OcarinaNote wallNotes[20];
    u32 wallCounter;
};

WspOcarinaSnapshot WspTakeSnapshot() {
    WspOcarinaSnapshot s;
    s.cur = sOcarinaInputButtonCur;
    s.start = sOcarinaInputButtonStart;
    s.prev = sOcarinaInputButtonPrev;
    s.press = sOcarinaInputButtonPress;
    s.curPitch = sCurOcarinaPitch;
    s.prevPitch = sPrevOcarinaPitch;
    s.curButton = sCurOcarinaButtonIndex;
    s.flags = sOcarinaFlags;
    s.inputEnabled = sIsOcarinaInputEnabled;
    s.instrument = sOcarinaInstrumentId;
    s.staffPos = sOcarinaStaffPlayingPos;
    s.dropTimer = MM_sOcarinaDropInputTimer;
    s.playingStaff = MM_sPlayingStaff;
    s.scarecrow = gOcarinaSongButtons[OCARINA_SONG_SCARECROW_SPAWN];
    s.wall = gOcarinaSongButtons[OCARINA_SONG_TERMINA_WALL];
    for (int i = 0; i < 20; i++) {
        s.wallNotes[i] = sOcarinaSongNotes[OCARINA_SONG_TERMINA_WALL][i];
    }
    s.wallCounter = sOcarinaWallCounter;
    return s;
}

void WspRestoreSnapshot(const WspOcarinaSnapshot& s) {
    sOcarinaInputButtonCur = s.cur;
    sOcarinaInputButtonStart = s.start;
    sOcarinaInputButtonPrev = s.prev;
    sOcarinaInputButtonPress = s.press;
    sCurOcarinaPitch = s.curPitch;
    sPrevOcarinaPitch = s.prevPitch;
    sCurOcarinaButtonIndex = s.curButton;
    sOcarinaFlags = s.flags;
    sIsOcarinaInputEnabled = s.inputEnabled;
    sOcarinaInstrumentId = s.instrument;
    sOcarinaStaffPlayingPos = s.staffPos;
    MM_sOcarinaDropInputTimer = s.dropTimer;
    MM_sPlayingStaff = s.playingStaff;
    gOcarinaSongButtons[OCARINA_SONG_SCARECROW_SPAWN] = s.scarecrow;
    gOcarinaSongButtons[OCARINA_SONG_TERMINA_WALL] = s.wall;
    for (int i = 0; i < 20; i++) {
        sOcarinaSongNotes[OCARINA_SONG_TERMINA_WALL][i] = s.wallNotes[i];
    }
    sOcarinaWallCounter = s.wallCounter;
}

/** Each song's last-note state and earliest earlier state under the current
 *  gates; returns how many songs answered @p want on the last note with nothing
 *  before (want 0xFE: "no song matched"). */
int WspSongsAnswer(const char* tag, bool expectMatch) {
    int ok = 0;
    for (int song = 0; song < RSBS_MMWP_SONG_COUNT; song++) {
        uint8_t n = 0;
        const uint8_t* buttons = Combo_MMWarpPlay_Buttons(song, &n);
        WspStartFreePlay(0);
        u8 before = 0;
        const u8 last = WspPlay(buttons, n, &before);
        const u8 want = expectMatch ? Combo_MMWarpPlay_StaffState(song) : kWspNoSong;
        printf("[TEST] %s: %s -> last note 0x%02X, before 0x%02X (want 0x%02X)\n", tag, Combo_MMWarpPlay_SongName(song),
               last, before, want);
        if (last == want && before == kWspNoSong) {
            ok++;
        }
    }
    return ok;
}

void WspMatcherLegs(PlayState* play) {
    const char* tag = "mm-warp-song-matcher";
    const uint32_t allSix = RSBS_MMWP_RULE_MASK;

    // Control: MM's own pitch map is the one the pure matcher's table assumes.
    WSP_CHECK(tag,
              sButtonToPitchMap[OCARINA_BTN_A] == OCARINA_PITCH_D4 &&
                  sButtonToPitchMap[OCARINA_BTN_C_UP] == OCARINA_PITCH_D5,
              "MM's sButtonToPitchMap changed under the row\n");

    // M1. Rules off, everything else on.
    WspFreezeWorld(0u, RSBS_MM_WARP_SONG_OWNED_ALL, true);
    play->msgCtx.ocarinaAction = OCARINA_ACTION_FREE_PLAY;
    WSP_CHECK(tag, WspSongsAnswer("M1 rules off", false) == RSBS_MMWP_SONG_COUNT,
              "M1: a warp song matched with every rule off\n");

    // M2. Rules on, nothing owned.
    WspFreezeWorld(allSix, 0u, true);
    WSP_CHECK(tag, WspSongsAnswer("M2 unowned", false) == RSBS_MMWP_SONG_COUNT,
              "M2: a warp song matched without being owned\n");

    // M3. Unpaired.
    WspFreezeWorld(allSix, RSBS_MM_WARP_SONG_OWNED_ALL, false);
    WSP_CHECK(tag, !Combo_ForeignPairingActive(), "M3 setup: the world must read unpaired\n");
    WSP_CHECK(tag, WspSongsAnswer("M3 unpaired", false) == RSBS_MMWP_SONG_COUNT,
              "M3: a warp song matched in an unpaired world\n");

    // M4. Not free play; then no play state.
    WspFreezeWorld(allSix, RSBS_MM_WARP_SONG_OWNED_ALL, true);
    play->msgCtx.ocarinaAction = OCARINA_ACTION_CHECK_NOTIME;
    WSP_CHECK(tag, WspSongsAnswer("M4 check-notime", false) == RSBS_MMWP_SONG_COUNT,
              "M4: a warp song matched outside free play\n");
    play->msgCtx.ocarinaAction = OCARINA_ACTION_FREE_PLAY;
    MM_gPlayState = nullptr;
    WSP_CHECK(tag, WspSongsAnswer("M4 no play state", false) == RSBS_MMWP_SONG_COUNT,
              "M4: a warp song matched with no play state\n");
    MM_gPlayState = play;

    // M5. Everything on.
    WSP_CHECK(tag, MM_WarpSongMM_UsableMask() == 0x3Fu, "M5 setup: all six songs must be usable\n");
    WSP_CHECK(tag, WspSongsAnswer("M5 all on", true) == RSBS_MMWP_SONG_COUNT,
              "M5: a warp song did not answer 0x80 + song exactly on its last note\n");

    // M6. Vanilla songs, each alone in the mask, with every warp song usable.
    int ties = 0;
    int steals = 0;
    for (int v = OCARINA_SONG_SONATA; v <= OCARINA_SONG_DOUBLE_TIME; v++) {
        const OcarinaSongButtons song = gOcarinaSongButtons[v];
        // The pure matcher's prediction: a warp song completing before the last
        // note steals; one completing ON the last note ties and loses.
        int stealer = -1;
        static const uint8_t kIdentity[5] = { 0, 1, 2, 3, 4 };
        for (uint8_t end = 1; end < song.numButtons && stealer < 0; end++) {
            stealer = Combo_MMWarpPlay_MatchSong(song.buttonIndex, end, kIdentity, 0x3Fu);
        }
        const bool tie = Combo_MMWarpPlay_MatchSong(song.buttonIndex, song.numButtons, kIdentity, 0x3Fu) >= 0;
        WspStartFreePlay(1u << v);
        u8 before = 0;
        const u8 last = WspPlay(song.buttonIndex, song.numButtons, &before);
        const u8 want = stealer >= 0 ? Combo_MMWarpPlay_StaffState(stealer) : (u8)v;
        const u8 got = stealer >= 0 ? before : last;
        printf("[TEST] %s M6: vanilla song %d -> 0x%02X (want 0x%02X%s%s)\n", tag, v, got, want,
               stealer >= 0 ? ", a recorded prefix steal" : "", tie ? ", a same-note tie" : "");
        WSP_CHECK(tag, got == want, "M6: vanilla song %d answered 0x%02X; want 0x%02X\n", v, got, want);
        ties += tie && stealer < 0 ? 1 : 0;
        steals += stealer >= 0 ? 1 : 0;
    }
    printf("[TEST] %s M6: %d same-note tie(s) (vanilla won), %d prefix steal(s) recorded\n", tag, ties, steals);

    // M7. The recorded steal: a stray A, then the Inverted Song of Time.
    {
        const OcarinaSongButtons inv = gOcarinaSongButtons[OCARINA_SONG_INVERTED_TIME];
        u8 notes[9];
        notes[0] = OCARINA_BTN_A;
        for (int i = 0; i < inv.numButtons && i < 8; i++) {
            notes[i + 1] = inv.buttonIndex[i];
        }
        WspStartFreePlay(1u << OCARINA_SONG_INVERTED_TIME);
        u8 state = kWspNoSong;
        int at = -1;
        for (int i = 0; i < inv.numButtons + 1; i++) {
            state = WspPress(WspPad(notes[i]));
            if (state != kWspNoSong) {
                at = i;
                break;
            }
        }
        printf("[TEST] %s M7: a stray A then the Inverted Song of Time -> 0x%02X at note %d\n", tag, state, at);
        WSP_CHECK(tag, state == Combo_MMWarpPlay_StaffState(3) && at == 5,
                  "M7: the Requiem of Spirit's recorded prefix steal of the Inverted Song of Time changed (0x%02X at "
                  "%d)\n",
                  state, at);
    }

    // M8. An 8-note scarecrow spawn song whose last six notes are Minuet's.
    {
        const u8 spawn[8] = { OCARINA_BTN_C_DOWN, OCARINA_BTN_C_DOWN,  OCARINA_BTN_A,      OCARINA_BTN_C_UP,
                              OCARINA_BTN_C_LEFT, OCARINA_BTN_C_RIGHT, OCARINA_BTN_C_LEFT, OCARINA_BTN_C_RIGHT };
        WspStartFreePlay(1u << OCARINA_SONG_SCARECROW_SPAWN);
        gOcarinaSongButtons[OCARINA_SONG_SCARECROW_SPAWN].numButtons = 8;
        for (int i = 0; i < 8; i++) {
            gOcarinaSongButtons[OCARINA_SONG_SCARECROW_SPAWN].buttonIndex[i] = spawn[i];
        }
        u8 sixth = 0;
        u8 last = 0;
        for (int i = 0; i < 8; i++) {
            const u8 s = WspPress(WspPad(spawn[i]));
            if (i == 5) {
                sixth = s;
            }
            last = s;
        }
        printf("[TEST] %s M8: the scarecrow's Minuet-tailed spawn song -> 6th 0x%02X, 8th 0x%02X\n", tag, sixth, last);
        WSP_CHECK(tag, sixth == kWspNoSong, "M8: the 6th note of the scarecrow's song answered 0x%02X\n", sixth);
        WSP_CHECK(tag, last == OCARINA_SONG_SCARECROW_SPAWN,
                  "M8: the scarecrow's song lost its 8th-note tie to a warp song (0x%02X)\n", last);
    }

    // M9. The Termina wall.
    {
        int oot = 0;
        int stolen = 0;
        for (int i = 0; i < 200; i++) {
            AudioOcarina_TerminaWallGenerateNotes();
            const OcarinaNote* w = sOcarinaSongNotes[OCARINA_SONG_TERMINA_WALL];
            const bool random = w[1].pitch == OCARINA_PITCH_NONE && w[1].length == 3 && w[0].length == 19;
            oot += random ? 0 : 1;
            const OcarinaSongButtons& b = gOcarinaSongButtons[OCARINA_SONG_TERMINA_WALL];
            stolen += Combo_MMWarpPlay_WallStealsBeforeEnd(b.buttonIndex, b.numButtons, 0x3Fu) ? 1 : 0;
        }
        printf("[TEST] %s M9: rules on, 200 walls: %d OoT-song wall(s), %d stealable\n", tag, oot, stolen);
        WSP_CHECK(tag, oot == 0, "M9: with a warp-song rule on the wall still showed an OoT song (%d)\n", oot);
        WSP_CHECK(tag, stolen == 0, "M9: a wall let a usable warp song complete before its last note (%d)\n", stolen);

        WspFreezeWorld(0u, RSBS_MM_WARP_SONG_OWNED_ALL, true);
        int ootOff = 0;
        for (int i = 0; i < 20; i++) {
            AudioOcarina_TerminaWallGenerateNotes();
            const OcarinaNote* w = sOcarinaSongNotes[OCARINA_SONG_TERMINA_WALL];
            ootOff += (w[1].pitch == OCARINA_PITCH_NONE && w[1].length == 3 && w[0].length == 19) ? 0 : 1;
        }
        printf("[TEST] %s M9: rules off, 20 walls: %d OoT-song wall(s)\n", tag, ootOff);
        WSP_CHECK(tag, ootOff > 0,
                  "M9: with the rules off the wall never showed an OoT song (the alternation moved)\n");
    }
}

/** Shared arrangement for the departure legs: a paired world with every rule on
 *  and Minuet owned, a PRESENT unshuffled record, a child OoT half, OoT playable,
 *  nothing pending. */
void WspArrangeDeparture(PlayState* play) {
    WspFreezeWorld(RSBS_MMWP_RULE_MASK, RSBS_MM_WARP_SONG_OWNED_ALL, true);
    (void)Combo_OoTWarpRecord_Build(&gComboCtx.comboOoTWarps, false, false, NULL, NULL);
    Context_ClearAllFrozenStates();
    OoT_WarpSongRecord_TestFreezeOoTHalf(1, 1, 1);
    Switch_SetTargetStatusForTest(SWITCH_TARGET_PLAYABLE);
    ComboEntrance_Init();
    Combo_MMWarpPlay_ResetLatches();
    memset(play, 0, sizeof(PlayState));
    play->msgCtx.lastPlayedSong = Combo_MMWarpPlay_StaffState(0);
    gSaveContext.save.entrance = kWspMarker;
}

bool WspArmedMinuet() {
    ComboWarpTarget t;
    return ComboWarp_Peek(&t) && t.armed && t.sourceGame == GAME_MM && t.marker == kWspMarker &&
           t.targetGame == GAME_OOT && t.targetEntrance == 0x0600u && t.kind == COMBO_SWITCH_KIND_WARP_SONG;
}

void WspDepartureLegs(PlayState* play) {
    const char* tag = "mm-warp-song-departure";
    std::unique_ptr<EnTest7> t7 = std::make_unique<EnTest7>();

    // D1. Nothing taken: a departure arms nothing and writes nothing.
    WspArrangeDeparture(play);
    play->nextEntrance = 0x1234;
    MM_WarpSongMM_Depart(play, 0);
    WSP_CHECK(tag, !ComboWarp_IsArmed() && play->nextEntrance == 0x1234 && play->transitionTrigger == 0,
              "D1: a departure with no prompt taken armed or wrote something\n");

    // D2. The first call through the real En_Test7 action departs.
    WspArrangeDeparture(play);
    Combo_MMWarpPlay_SetPending(0);
    u8 song = 0xFF;
    WSP_CHECK(tag, MM_WarpSongMM_TakePending(play, &song) && song == 0, "D2 setup: Minuet's prompt was not taken\n");
    memset(t7.get(), 0, sizeof(EnTest7));
    t7->actor.params = (s16)MM_WarpSongMM_EnTest7Params(0);
    t7->actionFunc = EnTest7_WarpCsWarp;
    EnTest7_WarpCsWarp(t7.get(), play);
    printf("[TEST] %s D2: armed %d, nextEntrance 0x%04X, trigger %d, type %d, action moved %d\n", tag,
           WspArmedMinuet() ? 1 : 0, (unsigned)play->nextEntrance, (int)play->transitionTrigger,
           (int)play->transitionType, t7->actionFunc != EnTest7_WarpCsWarp ? 1 : 0);
    WSP_CHECK(tag, WspArmedMinuet(), "D2: the departure did not arm MM -> OoT Minuet's pad with the warp kind\n");
    WSP_CHECK(tag,
              play->nextEntrance == kWspMarker && play->transitionTrigger == TRANS_TRIGGER_START &&
                  play->transitionType == TRANS_TYPE_FADE_BLACK,
              "D2: the departure did not start the fade to the marker\n");
    WSP_CHECK(tag, t7->actionFunc != EnTest7_WarpCsWarp && t7->actionFunc != nullptr,
              "D2: En_Test7 stayed on its final warp action\n");

    // D3. Every later call is a silent no-op.
    play->nextEntrance = 0x1234;
    play->transitionTrigger = 0;
    EnTest7_WarpCsWarp(t7.get(), play);
    t7->actionFunc(t7.get(), play);
    WSP_CHECK(tag, play->nextEntrance == 0x1234 && play->transitionTrigger == 0 && WspArmedMinuet(),
              "D3: a second En_Test7 call wrote a field or moved the arm\n");

    // D4. The transition's check consumes the target; a third call changes nothing.
    const uint16_t back = Combo_CheckCrossGameEntrance("mm", kWspMarker);
    printf("[TEST] %s D4: the check returned 0x%04X, switch %d to 0x%04X kind %d, still armed %d\n", tag,
           (unsigned)back, Combo_IsCrossGameSwitch() ? 1 : 0, (unsigned)Combo_GetSwitchTargetEntrance(),
           (int)Entrance_GetSwitchKind(), ComboWarp_IsArmed() ? 1 : 0);
    WSP_CHECK(tag,
              back == kWspMarker && Combo_IsCrossGameSwitch() && Combo_GetSwitchTargetEntrance() == 0x0600u &&
                  Entrance_GetSwitchKind() == COMBO_SWITCH_KIND_WARP_SONG && !ComboWarp_IsArmed(),
              "D4: the transition's check did not consume the warp into Minuet's pad\n");
    EnTest7_WarpCsWarp(t7.get(), play);
    WSP_CHECK(tag, play->nextEntrance == 0x1234 && play->transitionTrigger == 0 && !ComboWarp_IsArmed(),
              "D4: a call after the consumption wrote a field or re-armed\n");
    Combo_ClearPendingSwitch();

    // D5. Every refusal arms nothing and reloads the current entrance.
    struct Refusal {
        const char* name;
        int mode;
    };
    const Refusal kRefusals[] = { { "OoT adult", 0 }, { "OoT unknown", 1 }, { "no record", 2 },
                                  { "unpaired", 3 },  { "restricted", 4 },  { "OoT unplayable", 5 } };
    for (const Refusal& r : kRefusals) {
        WspArrangeDeparture(play);
        switch (r.mode) {
            case 0:
                Context_ClearAllFrozenStates();
                OoT_WarpSongRecord_TestFreezeOoTHalf(0, 1, 1);
                break;
            case 1:
                Context_ClearAllFrozenStates();
                break;
            case 2:
                Combo_OoTWarpRecord_Clear(&gComboCtx.comboOoTWarps);
                break;
            case 3:
                gComboCtx.sharedRandoSettingsHash = 0u;
                break;
            case 4:
                play->interfaceCtx.restrictions.songOfSoaring = 1;
                break;
            default:
                Switch_SetTargetStatusForTest(SWITCH_TARGET_NO_GAME_ARCHIVE);
                break;
        }
        Combo_MMWarpPlay_SetPending(0);
        (void)MM_WarpSongMM_TakePending(play, &song);
        MM_WarpSongMM_Depart(play, 0);
        printf("[TEST] %s D5: %s -> armed %d, nextEntrance 0x%04X\n", tag, r.name, ComboWarp_IsArmed() ? 1 : 0,
               (unsigned)play->nextEntrance);
        WSP_CHECK(tag, !ComboWarp_IsArmed() && play->nextEntrance == kWspMarker,
                  "D5: the %s refusal armed the warp or did not reload in place\n", r.name);
    }

    // D6. Player's fallback: once per TakePending.
    WspArrangeDeparture(play);
    Combo_MMWarpPlay_SetPending(0);
    (void)MM_WarpSongMM_TakePending(play, &song);
    MM_WarpSongMM_Depart(play, 0);
    const bool first = WspArmedMinuet();
    ComboEntrance_Init();
    MM_WarpSongMM_Depart(play, 0);
    WSP_CHECK(tag, first && !ComboWarp_IsArmed(), "D6: Player's fallback departed %s\n",
              first ? "twice for one prompt" : "not at all");

    // D6b. The prompt's decision, as MM_WarpSongMM_OnSongPlayed acts on it: every
    // gate holding offers the prompt and leaves Minuet pending for Player's warp
    // arm; OoT frozen as an adult, or the scene restricting the Song of Soaring,
    // refuses with MM's own text and leaves nothing pending. A stale pending song
    // is set first, so a decision that does not clear the latch shows.
    struct PromptCase {
        const char* name;
        int mode;
        ComboMMWarpPlayGate gate;
    };
    const PromptCase kPromptCases[] = { { "every gate holds", 0, COMBO_MMWP_OFFER },
                                        { "OoT adult", 1, COMBO_MMWP_NOT_CHILD },
                                        { "Soaring restricted", 2, COMBO_MMWP_RESTRICTED } };
    for (const PromptCase& c : kPromptCases) {
        WspArrangeDeparture(play);
        if (c.mode == 1) {
            Context_ClearAllFrozenStates();
            OoT_WarpSongRecord_TestFreezeOoTHalf(0, 1, 1);
        } else if (c.mode == 2) {
            play->interfaceCtx.restrictions.songOfSoaring = 1;
        }
        Combo_MMWarpPlay_SetPending(3);
        ComboMMWarpPlayPromptAction action;
        memset(&action, 0xA5, sizeof(action));
        MM_WarpSongMM_PromptActionFor(play, 0, &action);
        const int pendingAfter = Combo_MMWarpPlay_PendingSong();
        const bool hasPending = MM_WarpSongMM_HasPending(play);
        printf("[TEST] %s D6b: %s -> gate %d, prompt %d, text 0x%04X, pending %d (latch %d, Player sees %d)\n", tag,
               c.name, (int)action.gate, (int)action.prompt, (unsigned)action.textId, (int)action.pendingSong,
               pendingAfter, hasPending ? 1 : 0);
        WSP_CHECK(tag, action.gate == (uint8_t)c.gate, "D6b: %s decided gate %d, want %d\n", c.name, (int)action.gate,
                  (int)c.gate);
        if (c.mode == 0) {
            WSP_CHECK(tag,
                      action.prompt == 1u && action.textId == (uint16_t)RSBS_MMWP_TEXT_PROMPT_BOX &&
                          action.question[0] != '\0' && action.pendingSong == 0 && pendingAfter == 0 && hasPending,
                      "D6b: with every gate holding the prompt was not offered with Minuet pending\n");
        } else {
            WSP_CHECK(tag,
                      action.prompt == 0u && action.textId == (uint16_t)RSBS_MMWP_TEXT_RESTRICTED &&
                          action.pendingSong == -1 && pendingAfter == -1 && !hasPending,
                      "D6b: %s did not refuse with 0x1B95 and nothing pending\n", c.name);
        }
    }
    Combo_MMWarpPlay_ResetLatches();

    // D7. En_Test7's predicate.
    int accepted = 0;
    for (int p = -32768; p <= 32767; p++) {
        accepted += MM_WarpSongMM_IsEnTest7Params((int16_t)p) ? 1 : 0;
    }
    bool owlsVanilla = !MM_WarpSongMM_IsEnTest7Params((int16_t)ENTEST7_ARRIVE);
    for (int m = OCARINA_MODE_WARP_TO_GREAT_BAY_COAST; m <= OCARINA_MODE_WARP_TO_ENTRANCE; m++) {
        owlsVanilla = owlsVanilla && !MM_WarpSongMM_IsEnTest7Params((int16_t)m);
    }
    WSP_CHECK(tag,
              accepted == RSBS_MMWP_SONG_COUNT && MM_WarpSongMM_IsEnTest7Params((int16_t)(uint16_t)0x8000u) &&
                  MM_WarpSongMM_IsEnTest7Params((int16_t)(uint16_t)0x8005u) && owlsVanilla,
              "D7: En_Test7's predicate takes %d params (want 6: 0x8000..0x8005) or an owl mode\n", accepted);

    // D8. E2's playback (z_message.c SETUP_DISPLAY_SONG_PLAYED calls this helper
    // and runs vanilla's playback and fanfare only when it answers false): every
    // vanilla state and every non-song state leaves the playback alone; each warp
    // state points it at OoT's own notes, never a vanilla table (whose index 0x80+
    // would read past sOcarinaSongNotes' end).
    OcarinaNote* const priorPlayback = MM_sPlaybackSong;
    const u8 priorPlaybackState = MM_sPlaybackState;
    const u8 priorStopPos = sPlaybackStaffStopPos;
    const u8 priorStartPos = sPlaybackStaffStartPos;
    const s32 priorNoteTimer = sPlaybackNoteTimer;
    const u16 priorNotePos = MM_sPlaybackNotePos;
    const u16 priorStaffPos = sPlaybackStaffPos;
    const u8 priorPitch = sPlaybackPitch;
    int wrongVanilla = -1;
    for (unsigned s = 0; s <= 0xFFu; s++) {
        if (s >= RSBS_MMWP_STATE_FIRST && s <= RSBS_MMWP_STATE_LAST) {
            continue;
        }
        MM_sPlaybackSong = sOcarinaSongNotes[0];
        if (MM_WarpSongMM_SetupSongPlayback(s) || MM_sPlaybackSong != sOcarinaSongNotes[0]) {
            wrongVanilla = (int)s;
            break;
        }
    }
    WSP_CHECK(tag, wrongVanilla < 0, "D8: state 0x%02X took the warp song's playback\n", (unsigned)wrongVanilla);
    OcarinaNote* warpTables[RSBS_MMWP_SONG_COUNT] = {};
    bool warpOk = true;
    for (int i = 0; i < RSBS_MMWP_SONG_COUNT; i++) {
        MM_sPlaybackSong = sOcarinaSongNotes[0];
        const bool took = MM_WarpSongMM_SetupSongPlayback(Combo_MMWarpPlay_StaffState(i));
        warpTables[i] = MM_sPlaybackSong;
        bool vanillaTable = false;
        for (int k = 0; k < OCARINA_SONG_MAX; k++) {
            vanillaTable = vanillaTable || (MM_sPlaybackSong == sOcarinaSongNotes[k]);
        }
        warpOk = warpOk && took && !vanillaTable;
        for (int j = 0; j < i; j++) {
            warpOk = warpOk && warpTables[j] != warpTables[i];
        }
    }
    // Minuet of Forest, OoT's notes: D4 D5 B4 A4 B4 A4, then the end.
    MM_sPlaybackSong = sOcarinaSongNotes[0];
    (void)MM_WarpSongMM_SetupSongPlayback(Combo_MMWarpPlay_StaffState(0));
    const u8 kMinuetPitches[7] = { OCARINA_PITCH_D4, OCARINA_PITCH_D5, OCARINA_PITCH_B4,  OCARINA_PITCH_A4,
                                   OCARINA_PITCH_B4, OCARINA_PITCH_A4, OCARINA_PITCH_NONE };
    bool minuetOk = true;
    for (int n = 0; n < 7; n++) {
        minuetOk = minuetOk && MM_sPlaybackSong[n].pitch == kMinuetPitches[n];
    }
    printf("[TEST] %s D8: vanilla states untouched %d, six warp tables distinct and OoT's %d, Minuet's notes %d\n", tag,
           wrongVanilla < 0 ? 1 : 0, warpOk ? 1 : 0, minuetOk ? 1 : 0);
    WSP_CHECK(tag, warpOk, "D8: a warp state did not point the playback at its own OoT table\n");
    WSP_CHECK(tag, minuetOk, "D8: Minuet's playback is not OoT's D4 D5 B4 A4 B4 A4\n");
    MM_sPlaybackSong = priorPlayback;
    MM_sPlaybackState = priorPlaybackState;
    sPlaybackStaffStopPos = priorStopPos;
    sPlaybackStaffStartPos = priorStartPos;
    sPlaybackNoteTimer = priorNoteTimer;
    MM_sPlaybackNotePos = priorNotePos;
    sPlaybackStaffPos = priorStaffPos;
    sPlaybackPitch = priorPitch;
}

struct WspSavedCVar {
    const char* key;
    int value; // INT_MIN: absent
};

/** The combo and switch state both rows change, put back afterwards. */
struct WspComboSnapshot {
    std::unique_ptr<ComboContext> combo;
    PlayState* play;
    decltype(gSaveContext.save.entrance) entrance;
};

WspComboSnapshot WspTakeComboSnapshot() {
    WspComboSnapshot s;
    s.combo = std::make_unique<ComboContext>();
    memcpy(s.combo.get(), &gComboCtx, sizeof(ComboContext));
    s.play = MM_gPlayState;
    s.entrance = gSaveContext.save.entrance;
    return s;
}

void WspRestoreComboSnapshot(const WspComboSnapshot& s) {
    memcpy(&gComboCtx, s.combo.get(), sizeof(ComboContext));
    MM_gPlayState = s.play;
    gSaveContext.save.entrance = s.entrance;
    Switch_SetTargetStatusForTest(-1);
    ComboEntrance_Init();
    Context_ClearAllFrozenStates();
    Combo_MMWarpPlay_ResetLatches();
}

} // namespace

extern "C" int MM_WarpSongPlay_MatcherRunHeadless(void) {
    const char* tag = "mm-warp-song-matcher";
    WspSavedCVar saved[] = { { "gSettings.CustomOcarina.Dpad", 0 },
                             { "gSettings.CustomOcarina.RightStick", 0 },
                             { "gEnhancements.DpadNoDropOcarinaInput", 0 },
                             { "gEnhancements.Songs.EnableSunsSong", 0 } };
    for (auto& s : saved) {
        s.value = CVarGetInteger(s.key, INT_MIN);
        CVarClear(s.key);
    }
    const WspOcarinaSnapshot ocarina = WspTakeSnapshot();
    WspComboSnapshot combo = WspTakeComboSnapshot();
    std::unique_ptr<PlayState> play = std::make_unique<PlayState>();
    memset(play.get(), 0, sizeof(PlayState));
    MM_gPlayState = play.get();
    sWspFailures = 0;

    // As mm-ocarina-dpad-notes: a ROM-free process never booted MM's audio.
    bool sfxListsUnset = false;
    for (int bank = 0; bank < (int)ARRAY_COUNT(gSfxBanks); bank++) {
        sfxListsUnset = sfxListsUnset || (gSfxBanks[bank][0].next == 0);
    }
    if (sfxListsUnset) {
        AudioSfx_Reset();
    }
    memset(&sWspInput, 0, sizeof(sWspInput));
    MM_AudioOcarina_SetInputOverrideForTest(&sWspInput);
    AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_DEFAULT);
    MM_sOcarinaDropInputTimer = 0;
    if (sOcarinaInputButtonStart != 0 || sOcarinaInstrumentId == OCARINA_INSTRUMENT_OFF) {
        printf("[TEST] FAIL (%s): taking out the ocarina with nothing held did not leave the reader armed\n", tag);
        sWspFailures++;
    } else {
        WspMatcherLegs(play.get());
    }

    AudioOcarina_SetInstrument(OCARINA_INSTRUMENT_OFF);
    MM_AudioOcarina_SetInputOverrideForTest(nullptr);
    WspRestoreSnapshot(ocarina);
    WspRestoreComboSnapshot(combo);
    for (const auto& s : saved) {
        if (s.value == INT_MIN) {
            CVarClear(s.key);
        } else {
            CVarSetInteger(s.key, s.value);
        }
    }
    if (sWspFailures != 0) {
        printf("[TEST] FAIL (%s): %d failure(s)\n", tag, sWspFailures);
        return 1;
    }
    printf("[TEST] PASS: %s\n", tag);
    return 0;
}

extern "C" int MM_WarpSongPlay_DepartureRunHeadless(void) {
    const char* tag = "mm-warp-song-departure";
    WspComboSnapshot combo = WspTakeComboSnapshot();
    std::unique_ptr<PlayState> play = std::make_unique<PlayState>();
    memset(play.get(), 0, sizeof(PlayState));
    sWspFailures = 0;
    // OoT's descriptors: the frozen age reads OoT's real layout.
    OoT_SlotMeta_Register();
    WspDepartureLegs(play.get());
    WspRestoreComboSnapshot(combo);
    if (sWspFailures != 0) {
        printf("[TEST] FAIL (%s): %d failure(s)\n", tag, sWspFailures);
        return 1;
    }
    printf("[TEST] PASS: %s\n", tag);
    return 0;
}

#endif /* RSBS_SINGLE_EXECUTABLE */
